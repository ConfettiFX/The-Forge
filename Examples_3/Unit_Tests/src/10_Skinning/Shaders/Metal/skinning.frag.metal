/* Write your header comments here */
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

struct Fragment_Shader
{
	texture2d<float> DiffuseTexture;
	sampler DefaultSampler;
	
    struct VSOutput
    {
        float4 Position [[position]];
        float3 Normal;
        float2 UV;
    };

    float4 main(VSOutput input)
    {
        float nDotl = saturate((dot(input.Normal, float3(0, 1, 0)) + 1.0) * 0.5);
        float3 color = DiffuseTexture.sample(DefaultSampler, input.UV).xyz;
        return float4(color * nDotl, 1.0);
    };

	Fragment_Shader(texture2d<float> DiffuseTexture, sampler DefaultSampler) :
	DiffuseTexture(DiffuseTexture), DefaultSampler(DefaultSampler){}
};


fragment float4 stageMain(Fragment_Shader::VSOutput input [[stage_in]],
texture2d<float> DiffuseTexture [[texture(4)]], sampler DefaultSampler [[sampler(5)]]){
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Normal = normalize(input.Normal);
	input0.UV = input.UV;
    Fragment_Shader main(DiffuseTexture, DefaultSampler);
        return main.main(input0);
}
