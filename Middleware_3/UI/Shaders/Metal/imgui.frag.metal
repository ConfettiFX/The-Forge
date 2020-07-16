#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct Uniforms_uniformBlockVS
    {
        float4x4 ProjectionMatrix;
    };
    constant Uniforms_uniformBlockVS & uniformBlockVS;
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

        return (input).col * ((float4)(uTex.sample(uSampler, (input).uv)));
    };

    Fragment_Shader(
texture2d<float> uTex,sampler uSampler,
constant Uniforms_uniformBlockVS & uniformBlockVS) :
uTex(uTex),uSampler(uSampler),uniformBlockVS(uniformBlockVS) {}
};

fragment float4 stageMain(
    Fragment_Shader::PS_INPUT input              [[stage_in]],
	constant Fragment_Shader::Uniforms_uniformBlockVS& uniformBlockVS [[buffer(0)]],
    sampler uSampler [[sampler(0)]],
    texture2d<float> uTex [[texture(0)]]
)
{
    Fragment_Shader::PS_INPUT input0;
    input0.pos = float4(input.pos.xyz, 1.0 / input.pos.w);
    input0.col = input.col;
    input0.uv = input.uv;
    Fragment_Shader main(uTex, uSampler, uniformBlockVS);
    return main.main(input0);
}
