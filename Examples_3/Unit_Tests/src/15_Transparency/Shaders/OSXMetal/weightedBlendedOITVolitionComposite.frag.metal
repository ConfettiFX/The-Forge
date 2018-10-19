/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

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
    float4 main(    VSOutput input)
    {
        float revealage = 1.0;
        float additiveness = 0.0;
        float4 accum = float4(0.0, 0.0, 0.0, 0.0);
        float4 temp = RevealageTexture.sample(PointSampler, input.UV.xy);
        (revealage = temp.r);
        (additiveness = temp.w);
        (accum = (float4)(AccumulationTexture.sample(PointSampler, input.UV.xy)));
        float3 average_color = (accum.rgb / (float3)(max(accum.a, 0.000010000000)));
        float emissive_amplifier = (additiveness * 8.0);
        (emissive_amplifier = mix((emissive_amplifier * (float)(0.25)), emissive_amplifier, revealage));
        (emissive_amplifier += saturate((((float)(1.0) - revealage) * (float)(2.0))));
        (average_color *= (float3)(max(emissive_amplifier, 1.0)));
        if (any(isinf(accum.rgb)))
        {
            (average_color = (float3)(100.0));
        }
        return float4(average_color, ((float)(1.0) - revealage));
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
