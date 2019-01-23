#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct PsIn
    {
        float4 position [[position]];
        float2 texcoord;
    };
    struct Uniforms_uRootConstants
    {
        float4 color;
        float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    texture2d<float> uTex;
    sampler uSampler;
    float4 main(PsIn input)
    {
        return uTex.sample(uSampler, (input).texcoord) * uRootConstants.color;
    };

    Fragment_Shader(
constant Uniforms_uRootConstants & uRootConstants,texture2d<float> uTex,sampler uSampler) :
uRootConstants(uRootConstants),uTex(uTex),uSampler(uSampler) {}
};


fragment float4 stageMain(
    Fragment_Shader::PsIn input [[stage_in]],
    constant Fragment_Shader::Uniforms_uRootConstants & uRootConstants [[buffer(1)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSampler [[sampler(0)]])
{
    Fragment_Shader::PsIn input0;
    input0.position = float4(input.position.xyz, 1.0 / input.position.w);
    input0.texcoord = input.texcoord;
    Fragment_Shader main(
    uRootConstants,
    uTex,
    uSampler);
    return main.main(input0);
}
