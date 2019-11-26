/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position;
        float2 texcoords;
    };
    sampler uSamplerZip;
    texture2d<float> ZipTexture;
    float4 main(VSOutput input)
    {
        return ZipTexture.sample(uSamplerZip, (input).texcoords);
    };

    Fragment_Shader(
sampler uSamplerZip,texture2d<float> ZipTexture) :
uSamplerZip(uSamplerZip),ZipTexture(ZipTexture) {}
};

struct FSData {
    texture2d<float,access::sample> RightText   [[id(0)]];
    texture2d<float,access::sample> LeftText    [[id(1)]];
    texture2d<float,access::sample> TopText     [[id(2)]];
    texture2d<float,access::sample> BotText     [[id(3)]];
    texture2d<float,access::sample> FrontText   [[id(4)]];
    texture2d<float,access::sample> BackText    [[id(5)]];
    texture2d<float,access::sample> ZipTexture  [[id(6)]];
    sampler uSampler0                           [[id(7)]];
};


struct main_input
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};


fragment float4 stageMain(
	main_input inputData [[stage_in]],
    sampler uSamplerZip [[sampler(0)]],
    constant FSData& fsData   [[buffer(UPDATE_FREQ_NONE)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.texcoords = inputData.TEXCOORD;
    Fragment_Shader main(
    fsData.uSampler0,
    fsData.ZipTexture);
    return main.main(input0);
}
