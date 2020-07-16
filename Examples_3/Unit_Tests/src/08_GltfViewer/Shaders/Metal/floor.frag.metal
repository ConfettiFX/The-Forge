#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    const float NUM_SHADOW_SAMPLES_INV = float(0.03125);
    const array<float, 64> shadowSamples = { {(-0.17466460), (-0.79131840), (-0.129792), (-0.44771160), 0.08863912, (-0.8981690), (-0.58914988), (-0.678163), 0.17484090, (-0.5252063), 0.6483325, (-0.752117), 0.45293192, (-0.384986), 0.09757467, (-0.1166954), 0.3857658, (-0.9096935), 0.56130584, (-0.1283066), 0.768011, (-0.4906538), 0.8499438, (-0.220937), 0.6946555, 0.16058660, 0.9614297, 0.0597522, 0.7986544, 0.53259124, 0.45139648, 0.5592551, 0.2847693, 0.2293397, (-0.2118996), (-0.1609127), (-0.4357893), (-0.3808875), (-0.4662672), (-0.05288446), (-0.139129), 0.23940650, 0.1781853, 0.5254948, 0.4287854, 0.899425, 0.12893490, 0.8724155, (-0.6924323), (-0.2203967), (-0.48997), 0.2795907, (-0.26117242), 0.7359962, (-0.7704172), 0.42331340, (-0.8501040), 0.12639350, (-0.83452672), (-0.499136), (-0.5380967), 0.6264234, (-0.9769312), (-0.15505689)} };
    struct Uniforms_cbPerPass
    {
        float4x4 projView;
        float4x4 shadowLightViewProj;
        float4 camPos;
        array<float4, 4> lightColor;
        array<float4, 3> lightDirection;
    };
    constant Uniforms_cbPerPass& cbPerPass;
    texture2d<float> ShadowTexture;
    sampler clampMiplessLinearSampler;
    struct VSOutput
    {
        float4 Position;
        float3 WorldPos;
        float2 TexCoord;
    };
    float random(float3 seed, float3 freq)
    {
        float dt = dot(floor((seed * freq)), float3((float)53.12149811, (float)21.1352005, (float)9.13220024));
        return fract((sin(dt) * float(2105.23535156)));
    };
    float CalcPCFShadowFactor(float3 worldPos)
    {
        float4 posLS = ((cbPerPass.shadowLightViewProj)*(float4((worldPos).xyz, (float)1.0)));
        (posLS /= float4(posLS.w));
        ((posLS).y *= float((-1)));
        ((posLS).xy = (((posLS).xy * float2(0.5)) + float2((float)0.5, (float)0.5)));
        float2 HalfGaps = float2((float)0.00048828124, (float)0.00048828124);
//        float2 Gaps = float2((float)0.0009765625, (float)0.0009765625);
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
    float4 main(VSOutput input)
    {
        float3 color = float3((float)1.0, (float)1.0, (float)1.0);
        (color *= float3(CalcPCFShadowFactor((input).WorldPos)));
        float i = (float(1.0) - length(abs(((input).TexCoord).xy)));
        (i = pow(i, 1.20000004));
        return float4((color).rgb, i);
    };

    Fragment_Shader(constant Uniforms_cbPerPass& cbPerPass, texture2d<float> ShadowTexture, sampler clampMiplessLinearSampler) :
        cbPerPass(cbPerPass), ShadowTexture(ShadowTexture), clampMiplessLinearSampler(clampMiplessLinearSampler)
    {}
};

struct main_input
{
    float4 SV_POSITION [[position]];
    float3 POSITION;
    float2 TEXCOORD;
};

fragment float4 stageMain(
	main_input inputData [[stage_in]],
	constant float4x4* modelToWorldMatrices          [[buffer(0)]],
	sampler clampMiplessLinearSampler                [[sampler(0)]],
	texture2d<float> ShadowTexture                   [[texture(0)]],

	constant Fragment_Shader::Uniforms_cbPerPass& cbPerPass [[buffer(1)]],

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
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.WorldPos = inputData.POSITION;
    input0.TexCoord = inputData.TEXCOORD;
    Fragment_Shader main(cbPerPass, ShadowTexture, clampMiplessLinearSampler);
    return main.main(input0);
}
