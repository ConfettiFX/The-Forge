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

fragment float4 stageMain(
	main_input inputData [[stage_in]],
	texture2d<float> sceneTexture [[texture(0)]],
    sampler clampMiplessLinearSampler [[sampler(0)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.TexCoord = inputData.TEXCOORD;
    Fragment_Shader main(sceneTexture, clampMiplessLinearSampler);
    return main.main(input0);
}
