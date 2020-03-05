#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    texture2d<float> sceneTexture;
    sampler clampMiplessLinearSampler;
    struct VSOutput
    {
        float4 Position;
        float2 TexCoord;
    };
    float4 main(VSOutput input)
    {
        return sceneTexture.sample(clampMiplessLinearSampler, (input).TexCoord);
    };

    Fragment_Shader(texture2d<float> sceneTexture, sampler clampMiplessLinearSampler) :
        sceneTexture(sceneTexture), clampMiplessLinearSampler(clampMiplessLinearSampler)
    {}
};

struct main_input
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

struct ArgBuffer0
{
    texture2d<float> sceneTexture [[id(6)]];
    sampler clampMiplessLinearSampler [[id(7)]];
};

fragment float4 stageMain(
	main_input inputData [[stage_in]],
    constant ArgBuffer0& argBuffer0 [[buffer(UPDATE_FREQ_NONE)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.TexCoord = inputData.TEXCOORD;
    Fragment_Shader main(argBuffer0.sceneTexture, argBuffer0.clampMiplessLinearSampler);
    return main.main(input0);
}
