/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    texture2d<float> VSM;
    sampler VSMSampler;
#if PT_USE_CAUSTICS!=0
    texture2d<float> VSMRed;
    texture2d<float> VSMGreen;
    texture2d<float> VSMBlue;
#endif

    float2 ComputeMoments(float depth)
    {
        float2 moments;
        (moments.x = depth);
        float2 pd = float2(dfdx(depth), dfdy(depth));
        (moments.y = ((depth * depth) + (0.25 * dot(pd, pd))));
        return moments;
    };
    float ChebyshevUpperBound(float2 moments, float t)
    {
        float p = (t <= moments.x);
        float variance = (moments.y - (moments.x * moments.x));
        (variance = max(variance, 0.0010000000));
        float d = (t - moments.x);
        float pMax = (variance / (variance + (d * d)));
        return max(p, pMax);
    };
    float3 ShadowContribution(float2 shadowMapPos, float distanceToLight)
    {
        float2 moments = VSM.sample(VSMSampler, shadowMapPos).xy;
        float3 shadow = ChebyshevUpperBound(moments, distanceToLight);
#if PT_USE_CAUSTICS!=0
        (moments = (float2)(VSMRed.sample(VSMSampler, shadowMapPos).xy));
        (shadow.r *= ChebyshevUpperBound(moments, distanceToLight));
        (moments = (float2)(VSMGreen.sample(VSMSampler, shadowMapPos).xy));
        (shadow.g *= ChebyshevUpperBound(moments, distanceToLight));
        (moments = (float2)(VSMBlue.sample(VSMSampler, shadowMapPos).xy));
        (shadow.b *= ChebyshevUpperBound(moments, distanceToLight));
#endif

        return shadow;
    };
    struct VSOutput
    {
        float4 Position [[position]];
    };
    float2 main(VSOutput input)
    {
        return ComputeMoments(input.Position.z);
    };

    Fragment_Shader(
texture2d<float> VSM,sampler VSMSampler
#if PT_USE_CAUSTICS!=0
,texture2d<float> VSMRed,texture2d<float> VSMGreen,texture2d<float> VSMBlue
#endif
) :
VSM(VSM),VSMSampler(VSMSampler)
#if PT_USE_CAUSTICS!=0
,VSMRed(VSMRed),VSMGreen(VSMGreen),VSMBlue(VSMBlue)
#endif
 {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    texture2d<float> VSM [[texture(0)]],
    sampler VSMSampler [[sampler(0)]]
#if PT_USE_CAUSTICS!=0
    ,texture2d<float> VSMRed [[texture(1)]],
    texture2d<float> VSMGreen [[texture(2)]],
    texture2d<float> VSMBlue [[texture(3)]]
#endif
    )
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    Fragment_Shader main(
    VSM,
    VSMSampler
#if PT_USE_CAUSTICS!=0
    ,VSMRed,
    VSMGreen,
    VSMBlue
#endif
    );
    return float4(main.main(input0), 0.0, 0.0);
}
