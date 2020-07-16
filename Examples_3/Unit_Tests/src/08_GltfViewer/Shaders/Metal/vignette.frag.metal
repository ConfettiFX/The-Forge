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
        float4 src = sceneTexture.sample(clampMiplessLinearSampler, (input).TexCoord);
        uint width, height;
        width = sceneTexture.get_width();
        height = sceneTexture.get_height();
        float2 uv = (input).TexCoord;
        float2 coord = (((float2(2.0) * (uv - float2(0.5))) * float2(float(width))) / float2(float(height)));
        float rf = (sqrt(dot(coord, coord)) * float(0.2));
        float rf2_1 = ((rf * rf) + float(1.0));
        float e = (float(1.0) / (rf2_1 * rf2_1));
        return float4(((src).rgb * float3(e)), 1.0);
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
