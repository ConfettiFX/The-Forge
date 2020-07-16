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

struct main_input
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

fragment float4 stageMain(
	main_input inputData [[stage_in]],
	texture2d<float,access::sample> ZipTexture  [[texture(6)]],
    sampler uSampler0                           [[sampler(0)]]
)
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.texcoords = inputData.TEXCOORD;
    Fragment_Shader main(
    uSampler0,
    ZipTexture);
    return main.main(input0);
}
