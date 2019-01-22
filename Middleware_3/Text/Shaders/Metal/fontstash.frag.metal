#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct PsIn
    {
        float4 position [[position]];
        float2 texCoord;
    };
    struct Uniforms_uRootConstants
    {
        float4 color;
        float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    texture2d<float> uTex0;
    sampler uSampler0;
    float4 main(PsIn In)
    {
        return (float4(1.0, 1.0, 1.0, uTex0.sample(uSampler0, (In).texCoord).r) * uRootConstants.color);
    };

    Fragment_Shader(
constant Uniforms_uRootConstants & uRootConstants,texture2d<float> uTex0,sampler uSampler0) :
uRootConstants(uRootConstants),uTex0(uTex0),uSampler0(uSampler0) {}
};


fragment float4 stageMain(
    Fragment_Shader::PsIn In [[stage_in]],
    constant Fragment_Shader::Uniforms_uRootConstants & uRootConstants [[buffer(1)]],
    texture2d<float> uTex0 [[texture(0)]],
    sampler uSampler0 [[sampler(0)]])
{
    Fragment_Shader::PsIn In0;
    In0.position = float4(In.position.xyz, 1.0 / In.position.w);
    In0.texCoord = In.texCoord;
    Fragment_Shader main(
    uRootConstants,
    uTex0,
    uSampler0);
    return main.main(In0);
}
