/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float2 Tex_Coord;
    };
    sampler clampNearSampler;
    texture2d<float> screenTexture;
    float4 main(VSOutput input)
    {
        float rcolor = (float)(screenTexture.sample(clampNearSampler, (input).Tex_Coord).x);
        float3 color = float3(rcolor, rcolor, rcolor);
        return float4(color, 1.0);
    };

    Fragment_Shader(
sampler clampNearSampler,texture2d<float> screenTexture) :
clampNearSampler(clampNearSampler),screenTexture(screenTexture) {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    sampler clampNearSampler [[sampler(0)]],
    texture2d<float> screenTexture [[texture(0)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Tex_Coord = input.Tex_Coord;
    Fragment_Shader main(
    clampNearSampler,
    screenTexture);
    return main.main(input0);
}
