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
    sampler LinearSampler;
    texture2d<float> AccumulationTexture;
    texture2d<float> ModulationTexture;
    texture2d<float> BackgroundTexture;
#if PT_USE_REFRACTION!=0
    texture2d<float> RefractionTexture;
#endif

    float MaxComponent(float3 v)
    {
        return max(max(v.x, v.y), v.z);
    };
    float MinComponent(float3 v)
    {
        return min(min(v.x, v.y), v.z);
    };
    float4 main(VSOutput input)
    {
        float4 modulationAndDiffusion = ModulationTexture.sample(PointSampler, input.UV.xy);
        float3 modulation = modulationAndDiffusion.rgb;
        if ((MinComponent(modulation) == 1.0))
        {
            return BackgroundTexture.sample(PointSampler, input.UV.xy);
        }
        float4 accumulation = AccumulationTexture.sample(PointSampler, input.UV.xy);
        if (isinf(accumulation.a))
        {
            (accumulation.a = MaxComponent(accumulation.xyz));
        }
        if (isinf(MaxComponent(accumulation.xyz)))
        {
            (accumulation = (float4)(1.0));
        }
        const float epsilon = 0.0010000000;
        (accumulation.rgb *= ((float3)(0.5) + (max(modulation, epsilon) / (float3)((2.0 * max(epsilon, MaxComponent(modulation)))))));
#if PT_USE_REFRACTION!=0
        float2 delta = ((float2)(3.0) * (float2)(RefractionTexture.sample(PointSampler, input.UV.xy).xy * (float2)((1.0 / 8.0))));
#else
        float2 delta = 0.0;
#endif

        float3 background = 0.0;
#if PT_USE_DIFFUSION!=0
        const float pixelDiffusion2 = 256.0;
        float diffusion2 = (modulationAndDiffusion.a * pixelDiffusion2);
        if ((diffusion2 > (float)(0)))
        {
            int2 backgroundSize;
            backgroundSize.x = BackgroundTexture.get_width();
        backgroundSize.y = BackgroundTexture.get_height();
            float kernelRadius = (min(sqrt(diffusion2), 32.0) * 2.0);
            float mipLevel = max((log(kernelRadius) / log(2.0)), 0.0);
            if ((kernelRadius <= (float)(1)))
            {
                (mipLevel = 0.0);
            }
            float2 offset = ((float2)(pow(2.0, mipLevel)) / (float2)(backgroundSize));
            (background += (float3)(BackgroundTexture.sample(LinearSampler, (input.UV.xy + delta), level(mipLevel)).rgb * (float3)(0.5)));
            (background += (float3)(BackgroundTexture.sample(LinearSampler, ((input.UV.xy + delta) + offset), level(mipLevel)).rgb * (float3)(0.125)));
            (background += (float3)(BackgroundTexture.sample(LinearSampler, ((input.UV.xy + delta) - offset), level(mipLevel)).rgb * (float3)(0.125)));
            (background += (float3)(BackgroundTexture.sample(LinearSampler, ((input.UV.xy + delta) + float2(offset.x, (-offset.y))), level(mipLevel)).rgb * (float3)(0.125)));
            (background += (float3)(BackgroundTexture.sample(LinearSampler, ((input.UV.xy + delta) + float2((-offset.x), offset.y)), level(mipLevel)).rgb * (float3)(0.125)));
        }
        else
        {
#endif

#if PT_USE_REFRACTION!=0
            (background = (float3)(BackgroundTexture.sample(LinearSampler, clamp((delta + input.UV.xy), 0.0010000000, 0.9990000), level(0.0)).rgb));
#else
            (background = (float3)(BackgroundTexture.sample(PointSampler, input.UV.xy, level(0.0)).rgb));
#endif

#if PT_USE_DIFFUSION!=0
        }
#endif

        return float4(((background * modulation) + ((((float3)(1.0) - modulation) * accumulation.rgb) / (float3)(max(accumulation.a, 0.000010000000)))), 1.0);
    };

    Fragment_Shader(
sampler PointSampler,sampler LinearSampler,texture2d<float> AccumulationTexture,texture2d<float> ModulationTexture,texture2d<float> BackgroundTexture
#if PT_USE_REFRACTION!=0
,texture2d<float> RefractionTexture
#endif
) :
PointSampler(PointSampler),LinearSampler(LinearSampler),AccumulationTexture(AccumulationTexture),ModulationTexture(ModulationTexture),BackgroundTexture(BackgroundTexture)
#if PT_USE_REFRACTION!=0
,RefractionTexture(RefractionTexture)
#endif
 {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    sampler PointSampler [[sampler(0)]],
    sampler LinearSampler [[sampler(1)]],
    texture2d<float> AccumulationTexture [[texture(0)]],
    texture2d<float> ModulationTexture [[texture(1)]],
    texture2d<float> BackgroundTexture [[texture(2)]]
#if PT_USE_REFRACTION!=0
    ,texture2d<float> RefractionTexture [[texture(3)]]
#endif
    )
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.UV = input.UV;
    Fragment_Shader main(
    PointSampler,
    LinearSampler,
    AccumulationTexture,
    ModulationTexture,
    BackgroundTexture
#if PT_USE_REFRACTION!=0
    ,RefractionTexture
#endif
    );
    return main.main(input0);
}
