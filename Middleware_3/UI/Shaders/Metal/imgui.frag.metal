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

        return (input).col * ((float4)(uTex.sample(uSampler, (input).uv)));
    };

    Fragment_Shader(
texture2d<float> uTex,sampler uSampler) :
uTex(uTex),uSampler(uSampler) {}
};
  
struct FSDataFreqNone {
    sampler uSampler [[id(1)]];
};

struct FSDataFreqPerBatch {
    texture2d<float> uTex [[id(0)]];
};

fragment float4 stageMain(
    Fragment_Shader::PS_INPUT input              [[stage_in]],
    constant FSDataFreqNone& fsData              [[buffer(UPDATE_FREQ_NONE)]],
    constant FSDataFreqPerBatch& fsDataPerBatch  [[buffer(UPDATE_FREQ_PER_BATCH)]]
)
{
    Fragment_Shader::PS_INPUT input0;
    input0.pos = float4(input.pos.xyz, 1.0 / input.pos.w);
    input0.col = input.col;
    input0.uv = input.uv;
    Fragment_Shader main(fsDataPerBatch.uTex, fsData.uSampler);
    return main.main(input0);
}
