struct VsIn
{
	float2 position: Position;
	float2 texCoord: TEXCOORD0;
};

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

cbuffer uniformBlock_rootcbv : register(b1)
{
	float4x4 mvp;
};

PsIn main(VsIn In)
{
	PsIn Out;
	Out.position = mul(mvp , float4(In.position * uRootConstants.scaleBias.xy, 1.0f, 1.0f));
	Out.texCoord = In.texCoord;
	return Out;
}