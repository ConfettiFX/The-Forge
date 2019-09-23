/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float2 UV;
        float4 MiscData;
    };
    sampler clampToEdgeNearSampler;
    texture2d<float> DepthPassTexture;
    float main(VSOutput input)
    {
        float tileDepth = (float)(DepthPassTexture.sample(clampToEdgeNearSampler, (input).UV, level(0.0))).r;
        return tileDepth;
    };

    Fragment_Shader(
sampler clampToEdgeNearSampler,texture2d<float> DepthPassTexture) :
clampToEdgeNearSampler(clampToEdgeNearSampler),DepthPassTexture(DepthPassTexture) {}
};

struct FSData {
    sampler clampToEdgeNearSampler      [[id(0)]];
    texture2d<float> DepthPassTexture   [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    constant FSData& fsData         [[buffer(UPDATE_FREQ_NONE)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.UV = input.UV;
    input0.MiscData = input.MiscData;
    Fragment_Shader main(fsData.clampToEdgeNearSampler, fsData.DepthPassTexture);
    return main.main(input0);
}
