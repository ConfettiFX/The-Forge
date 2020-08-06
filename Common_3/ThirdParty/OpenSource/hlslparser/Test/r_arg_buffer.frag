struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float2 UV   : TEXCOORD0;
};

Texture2D Texture00 : register(t0);
Texture2D Texture01;
Texture2D Texture10 : register(t1, space1);
Texture2D Texture11 : register(t2, space1);
SamplerState Sampler00 : register(s2, space2);
SamplerState Sampler01 : register(s3, space2);
SamplerState Sampler10 : register(s5);
SamplerState Sampler11 : register(s6, space1);

struct RCB
{
	float4 uMult;
};

ConstantBuffer<RCB> RootConstant;

cbuffer ContantBuffer1 : register(b1, space2)
{
	float4 uMult1;
}

float4 main( Vertex_Output In ) : SV_TARGET
{
	return RootConstant.uMult * uMult1 *
		(Texture00.Sample(Sampler00, In.UV) +
		 Texture01.Sample(Sampler01, In.UV) +
		 Texture10.Sample(Sampler10, In.UV) +
		 Texture11.Sample(Sampler11, In.UV));
}