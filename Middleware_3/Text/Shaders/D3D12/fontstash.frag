struct PsIn
{
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD0;
};

struct Constants
{
	float4 color;
	float2 scaleBias;
};

#ifdef VULKAN_HLSL
[[vk::push_constant]]
#endif
ConstantBuffer<Constants> uRootConstants : register(b0);

Texture2D uTex0 : register(t2);
SamplerState uSampler0 : register(s3);

float4 main(PsIn In) : SV_Target
{
	return float4(1.0, 1.0, 1.0, uTex0.Sample(uSampler0, In.texCoord).r) * uRootConstants.color;
}