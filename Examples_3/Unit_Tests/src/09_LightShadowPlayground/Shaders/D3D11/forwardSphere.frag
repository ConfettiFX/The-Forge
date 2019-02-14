#define PI 3.1415926289793f
#define PI_2 3.1415926289793f*2.0f

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_ESM 1
#define SHADOW_TYPE_SDF 2

cbuffer ESMInputConstants
{
    float Near : packoffset(c0);
    float Far : packoffset(c0.y);
    float FarNearDiff : packoffset(c0.z);
    float Near_Over_FarNearDiff : packoffset(c0.w);
    float Exponent : packoffset(c1);
};

cbuffer lightUniformBlock
{
    row_major float4x4 lightViewProj : packoffset(c0);
    float4 lightPosition : packoffset(c4);
    float4 lightColor : packoffset(c5);
};

cbuffer renderSettingUniformBlock
{
    float4 WindowDimension : packoffset(c0);
    int ShadowType : packoffset(c1);
};

cbuffer cameraUniformBlock
{
    row_major float4x4 View : packoffset(c0);
    row_major float4x4 Project : packoffset(c4);
    row_major float4x4 ViewProject : packoffset(c8);
    row_major float4x4 InvView : packoffset(c12);
};

Texture2D shadowTexture;
SamplerState clampMiplessSampler;
Texture2D<float4> SphereTex;
SamplerState textureSampler;
Texture2D<float4> PlaneTex;

struct PsIn
{
    float3 WorldPositionIn : POSITION;
    float3 ColorIn : TEXCOORD1;
    float3 NormalIn : NORMAL;
    nointerpolation int IfPlaneIn : TEXCOORD2;
    float4 Position : SV_Position;
};

struct PsOut
{
    float4 FinalColor : SV_Target0;
};

float map_01(float depth)
{
    return (depth - Near) / FarNearDiff;
}

float calcESMShadowFactor(float3 worldPos)
{
    float4 shadowCoord = mul(float4(worldPos, 1.0f), lightViewProj);
    float2 shadowUV = (shadowCoord.xy / (float2(shadowCoord.w, -shadowCoord.w) * 2.0f)) + float2(0.5, 0.5);
    float lightMappedExpDepth = shadowTexture.Sample(clampMiplessSampler, shadowUV).x;

    float pixelMappedDepth = map_01(shadowCoord.w);
    float shadowFactor = lightMappedExpDepth * exp2((-Exponent) * pixelMappedDepth);

    return clamp(shadowFactor, 0.0f, 1.0f);
}

float3 calcSphereTexColor(float3 worldNormal)
{
    float2 uv = 0.0f.xx;
    uv.x = asin(worldNormal.x) / PI + 0.5f;
    uv.y = asin(worldNormal.y) / PI + 0.5f;
    return SphereTex.Sample(textureSampler, uv).xyz;
}

[earlydepthstencil]
PsOut main(PsIn input)
{
    float shadowFactor = 1.0f;
    if (ShadowType == SHADOW_TYPE_ESM)
        shadowFactor = calcESMShadowFactor(input.WorldPositionIn);
    else if (ShadowType == SHADOW_TYPE_SDF)
        shadowFactor = shadowTexture.Load(int3(int2(input.Position.xy), 0)).x;

    // calculate material parameters
    float3 albedo = input.ColorIn;
    float3 normal = normalize(input.NormalIn);
    if (input.IfPlaneIn == 0)
    {
        albedo *= calcSphereTexColor(normal);
    }
    else
    {
        albedo *= PlaneTex.Sample(textureSampler, input.WorldPositionIn.xz).xyz;
    }
    float3 lightVec = normalize(lightPosition.xyz - input.WorldPositionIn);
    float3 viewVec = normalize(input.WorldPositionIn - InvView[3].xyz);
    float dotP = max(dot(normal, lightVec), 0.0f);
    float3 diffuse = albedo * max(dotP, 0.05f);
    const float specularExp = 10.0;
    float specular = pow(max(dot(reflect(lightVec, normal), viewVec), 0.0f) * dotP, specularExp);
    float3 finalColor = clamp(diffuse + specular * 0.5f, 0, 1);
    finalColor *= lightColor.xyz * shadowFactor;
	
    PsOut output;
    output.FinalColor = float4(finalColor, 1.0f);
    return output;
}
