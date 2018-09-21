/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

inline void clip(float x) {
    if (x < 0.0) discard_fragment();
}
struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float4 UV;
    };
    sampler PointSampler;
    texture2d<float> AccumulationTexture;
    texture2d<float> RevealageTexture;
    float MaxComponent(    float4 v)
    {
        return max(max(max(v.x, v.y), v.z), v.w);
    };
    float4 main(    VSOutput input)
    {
        float revealage = RevealageTexture.sample(PointSampler, input.UV.xy).r;
        clip(((1.0 - revealage) - 0.000010000000));
        float4 accumulation = AccumulationTexture.sample(PointSampler, input.UV.xy);
        if (isinf(MaxComponent(abs(accumulation))))
        {
            (accumulation.rgb = (float3)(accumulation.a));
        }
        float3 averageColor = (accumulation.rgb / (float3)(max(accumulation.a, 0.000010000000)));
        return float4(averageColor, (1.0 - revealage));
    };

    Fragment_Shader(
sampler PointSampler,texture2d<float> AccumulationTexture,texture2d<float> RevealageTexture) :
PointSampler(PointSampler),AccumulationTexture(AccumulationTexture),RevealageTexture(RevealageTexture) {}
};


fragment float4 stageMain(
Fragment_Shader::VSOutput input [[stage_in]],
    sampler PointSampler [[sampler(0)]],
texture2d<float> AccumulationTexture [[texture(0)]],
texture2d<float> RevealageTexture [[texture(1)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.UV = input.UV;
    Fragment_Shader main(PointSampler, AccumulationTexture, RevealageTexture);
    return main.main(input0);
}
