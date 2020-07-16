#include <metal_stdlib>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m)
{
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

inline float rcp(float x) {
    return 1.0f / x;
}

constant float NUM_SHADOW_SAMPLES_INV = float(0.03125);
constant array<float, 64> shadowSamples = { {(-0.17466460), (-0.79131840), (-0.129792), (-0.44771160), 0.08863912, (-0.8981690), (-0.58914988), (-0.678163), 0.17484090, (-0.5252063), 0.6483325, (-0.752117), 0.45293192, (-0.384986), 0.09757467, (-0.1166954), 0.3857658, (-0.9096935), 0.56130584, (-0.1283066), 0.768011, (-0.4906538), 0.8499438, (-0.220937), 0.6946555, 0.16058660, 0.9614297, 0.0597522, 0.7986544, 0.53259124, 0.45139648, 0.5592551, 0.2847693, 0.2293397, (-0.2118996), (-0.1609127), (-0.4357893), (-0.3808875), (-0.4662672), (-0.05288446), (-0.139129), 0.23940650, 0.1781853, 0.5254948, 0.4287854, 0.899425, 0.12893490, 0.8724155, (-0.6924323), (-0.2203967), (-0.48997), 0.2795907, (-0.26117242), 0.7359962, (-0.7704172), 0.42331340, (-0.8501040), 0.12639350, (-0.83452672), (-0.499136), (-0.5380967), 0.6264234, (-0.9769312), (-0.15505689)} };

struct Uniforms_cbPerPass
{
    float4x4 projView;
    float4x4 shadowLightViewProj;
    float4 camPos;
    array<float4, 4> lightColor;
    array<float4, 3> lightDirection;
};

struct GLTFTextureProperties
{
    short mTextureIndex;
    short mSamplerIndex;
    int mUVStreamIndex;
    float mRotation;
    float mValueScale;
    float2 mOffset;
    float2 mScale;
};

struct GLTFMaterialData
{
    uint mAlphaMode;
    float mAlphaCutoff;
    float2 mEmissiveGBScale;
    float4 mBaseColorFactor;
    float4 mMetallicRoughnessFactors;
    GLTFTextureProperties mBaseColorProperties;
    GLTFTextureProperties mMetallicRoughnessProperties;
    GLTFTextureProperties mNormalTextureProperties;
    GLTFTextureProperties mOcclusionTextureProperties;
    GLTFTextureProperties mEmissiveTextureProperties;
};


float3 fresnelSchlick(float cosTheta, float3 F0)
{
    float Fc = pow((1.0 - cosTheta), 5.0);
    return (F0 + ((float3(1.0) - F0) * float3(Fc)));
};

float4 sampleTexture(GLTFTextureProperties textureProperties, texture2d<float> tex, sampler s, float4 scaleFactor, float2 uv)
{
    if ((textureProperties.mTextureIndex < 0))
    {
        return scaleFactor;
    }
    float2 texCoord = ((uv * (textureProperties).mScale) + (textureProperties).mOffset);
    if (textureProperties.mRotation)
    {
        float s, c;
        s = sincos((textureProperties).mRotation, c);
        (texCoord = float2(((c * (texCoord).x) - (s * (texCoord).y)), ((s * (texCoord).x) + (c * (texCoord).y))));
    }
    return ((tex.sample(s, texCoord) * float4(textureProperties.mValueScale)) * scaleFactor);
};

float distributionGGX(float3 N, float3 H, float roughness)
{
    float a = (roughness * roughness);
    float a2 = (a * a);
    float NdotH = max((float)dot(N, H),(float)float(0.0));
    float NdotH2 = (NdotH * NdotH);
    float nom = a2;
    float denom = ((NdotH2 * (a2 - float(1.0))) + float(1.0));
    (denom = ((3.14159274 * denom) * denom));
    return (nom / denom);
};

float Vis_SmithJointApprox(float a, float NoV, float NoL)
{
    float Vis_SmithV = (NoL * ((NoV * (1.0 - a)) + a));
    float Vis_SmithL = (NoV * ((NoL * (1.0 - a)) + a));
    return (float(0.5) * rcp(max((float)Vis_SmithV + Vis_SmithL,(float)float(0.0010000000))));
};

float3 reconstructNormal(float4 sampleNormal)
{
    float3 tangentNormal;
    ((tangentNormal).xy = (((sampleNormal).rg * float2(2)) - float2(1)));
    ((tangentNormal).z = sqrt((float(1.0) - saturate(dot((tangentNormal).xy, (tangentNormal).xy)))));
    return normalize(tangentNormal);
};

float3 getNormalFromMap(float3 normal, float3 pos, float2 uv, constant GLTFMaterialData& materialData, texture2d<float> normalMap, sampler normalMapSampler)
{
    if ((materialData.mNormalTextureProperties.mTextureIndex < 0))
    {
        return normalize(normal);
    }
    float3 tangentNormal = reconstructNormal(normalMap.sample(normalMapSampler, uv));
    float3 Q1 = dfdx(pos);
    float3 Q2 = dfdy(pos);
    float2 st1 = dfdx(uv);
    float2 st2 = dfdy(uv);
    float3 N = normalize(normal);
    float3 T = ((Q1 * float3(st2.g)) - (Q2 * float3(st1.g)));
    (T = normalize(T));
    if (isnan((T).x) || isnan((T).y) || isnan((T).z))
    {
        float3 UpVec = (((abs((N).y) < float(0.9990000)))?(float3((float)0.0, (float)1.0, (float)0.0)):(float3((float)0.0, (float)0.0, (float)1.0)));
        (T = normalize(cross(N, UpVec)));
    }
    float3 B = normalize(cross(T, N));
    float3x3 TBN = float3x3(T, B, N);
    float3 res = ((tangentNormal)*(TBN));
    return normalize(res);
};

float3 ComputeLight(float3 albedo, float3 lightColor, float3 metalness, float roughness, float3 N, float3 L, float3 V, float3 H, float NoL, float NoV)
{
    float a = (roughness * roughness);
    float3 F0 = float3(0.040000000, 0.040000000, 0.040000000);
    float3 diffuse = ((float3(1.0) - metalness) * albedo);
    float NDF = distributionGGX(N, H, roughness);
    float G = Vis_SmithJointApprox(a, NoV, NoL);
    float3 F = fresnelSchlick(max((float)dot(N, H),(float)0.0), mix(F0, albedo, metalness));
    float3 specular = (float3(NDF * G) * F);
    float3 F2 = fresnelSchlick(max((float)dot(N, V),(float)0.0), F0);
    (specular += F2);
    float3 irradiance = (float3((lightColor).r, (lightColor).g, (lightColor).b) * float3((float)1.0, (float)1.0, (float)1.0));
    float3 result = (((diffuse + specular) * float3(NoL)) * irradiance);
    return result;
};

float random(float3 seed, float3 freq)
{
    float dt = dot(floor((seed * freq)), float3((float)53.12149811, (float)21.1352005, (float)9.13220024));
    return fract((sin(dt) * float(2105.23535156)));
};

float CalcPCFShadowFactor(float3 worldPos, float4x4 shadowLightViewProj, texture2d<float> ShadowTexture, sampler clampMiplessLinearSampler)
{
    float4 posLS = ((shadowLightViewProj)*(float4((worldPos).xyz, (float)1.0)));
    (posLS /= float4(posLS.w));
    ((posLS).y *= float((-1)));
    ((posLS).xy = (((posLS).xy * float2(0.5)) + float2((float)0.5, (float)0.5)));
    float2 HalfGaps = float2((float)0.00048828124, (float)0.00048828124);
//    float2 Gaps = float2((float)0.0009765625, (float)0.0009765625);
    ((posLS).xy += HalfGaps);
    float shadowFactor = float(1.0);
    float shadowFilterSize = float(0.0016000000);
    float angle = random(worldPos, float3(20.0));
    float s = sin(angle);
    float c = cos(angle);
    for (int i = 0; (i < 32); (i++))
    {
        float2 offset = float2(shadowSamples[(i * 2)], shadowSamples[((i * 2) + 1)]);
        (offset = float2((((offset).x * c) + ((offset).y * s)), (((offset).x * (-s)) + ((offset).y * c))));
        (offset *= float2(shadowFilterSize));
        float shadowMapValue = (ShadowTexture.sample(clampMiplessLinearSampler, ((posLS).xy + offset), level(float(0)))).x;
        (shadowFactor += ((((shadowMapValue - 0.0020000000) > (posLS).z))?(0.0):(1.0)));
    }
    (shadowFactor *= NUM_SHADOW_SAMPLES_INV);
    return shadowFactor;
};

struct main_input
{
    float3 POSITION;
    float3 NORMAL;
    float2 TEXCOORD0;
};

struct Uniforms_cbMaterialData
{
    GLTFMaterialData materialData;
};

fragment float4 stageMain(
	main_input inputData [[stage_in]],
	constant float4x4* modelToWorldMatrices          [[buffer(0)]],
	sampler clampMiplessLinearSampler                [[sampler(0)]],
	texture2d<float> ShadowTexture                   [[texture(0)]],

	constant Uniforms_cbPerPass& cbPerPass           [[buffer(1)]],

    constant Uniforms_cbMaterialData& cbMaterialData [[buffer(2)]],
    texture2d<float> baseColorMap                    [[texture(1)]],
    sampler baseColorSampler                         [[sampler(1)]],
    texture2d<float> normalMap                       [[texture(2)]],
    sampler normalMapSampler                         [[sampler(2)]],
    texture2d<float> metallicRoughnessMap            [[texture(3)]],
    sampler metallicRoughnessSampler                 [[sampler(3)]],
    texture2d<float> occlusionMap                    [[texture(4)]],
    sampler occlusionMapSampler                      [[sampler(4)]],
    texture2d<float> emissiveMap                     [[texture(5)]],
    sampler emissiveMapSampler                       [[sampler(5)]]
)
{
    constant GLTFMaterialData& materialData = cbMaterialData.materialData;

    float4 baseColor = sampleTexture(materialData.mBaseColorProperties, baseColorMap, baseColorSampler, materialData.mBaseColorFactor, inputData.TEXCOORD0);
    float4 metallicRoughness = sampleTexture(materialData.mMetallicRoughnessProperties, metallicRoughnessMap, metallicRoughnessSampler, materialData.mMetallicRoughnessFactors, inputData.TEXCOORD0);
    float ao = (sampleTexture(materialData.mOcclusionTextureProperties, occlusionMap, occlusionMapSampler, float4(1.0), inputData.TEXCOORD0)).x;
    float3 emissive = (sampleTexture(materialData.mEmissiveTextureProperties, emissiveMap, emissiveMapSampler, float4(1.0), inputData.TEXCOORD0)).rgb;

    emissive *= materialData.mEmissiveTextureProperties.mValueScale;
    ((emissive).gb *= materialData.mEmissiveGBScale);
    float3 normal = getNormalFromMap(inputData.NORMAL, inputData.POSITION, inputData.TEXCOORD0, materialData, normalMap, normalMapSampler);
    float3 metalness = float3((metallicRoughness).b, (metallicRoughness).b, (metallicRoughness).b);
    float roughness = (metallicRoughness).g;
    if (materialData.mAlphaMode == 1 && materialData.mAlphaCutoff < 1.0 && ((baseColor).a < materialData.mAlphaCutoff))
    {
        discard_fragment();
    }
    (roughness = clamp(0.020000000, 1.0, roughness));
    float3 N = normal;
    float3 V = normalize(((cbPerPass.camPos).xyz - inputData.POSITION));
    float NoV = max((float)dot(N, V),(float)float(0.0));
    float3 result = float3((float)0.0, (float)0.0, (float)0.0);
    for (uint i = uint(0); (i < uint(1)); (++i))
    {
        float3 L = normalize((cbPerPass.lightDirection[i]).xyz);
        float3 H = normalize((V + L));
        float NoL = max((float)dot(N, L),(float)float(0.0));
        (result += (ComputeLight((baseColor).rgb, (cbPerPass.lightColor[i]).rgb, metalness, roughness, N, L, V, H, NoL, NoV) * float3(cbPerPass.lightColor[i].a)));
    }
    (result *= float3(ao));
    (result *= float3(CalcPCFShadowFactor(inputData.POSITION, cbPerPass.shadowLightViewProj, ShadowTexture, clampMiplessLinearSampler)));
    (result += (((baseColor).rgb * (cbPerPass.lightColor[3]).rgb) * float3(cbPerPass.lightColor[3].a)));
    (result += emissive);
    return float4((result).r, (result).g, (result).b, (baseColor).a);
}
