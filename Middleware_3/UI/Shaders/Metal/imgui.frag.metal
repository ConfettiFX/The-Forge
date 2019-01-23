#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct PS_INPUT
    {
        float4 pos [[position]];
        float4 col;
        float2 uv;
    };
    texture2d<float> uTex;
    sampler uSampler;
    float4 main(PS_INPUT input)
    {
        float4 out_col = ((input).col * (float4)(uTex.sample(uSampler, (input).uv)));
        return out_col;
    };

    Fragment_Shader(
texture2d<float> uTex,sampler uSampler) :
uTex(uTex),uSampler(uSampler) {}
};


fragment float4 stageMain(
    Fragment_Shader::PS_INPUT input [[stage_in]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSampler [[sampler(0)]])
{
    Fragment_Shader::PS_INPUT input0;
    input0.pos = float4(input.pos.xyz, 1.0 / input.pos.w);
    input0.col = input.col;
    input0.uv = input.uv;
    Fragment_Shader main(
    uTex,
    uSampler);
    return main.main(input0);
}
