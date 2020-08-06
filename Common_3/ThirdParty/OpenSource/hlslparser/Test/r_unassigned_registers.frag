struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float2 UV   : TEXCOORD0;
};

Texture2D Texture00 : register(t0);
Texture2D Texture01[2];
Texture2D Texture10[20] : register(t1, space1);
Texture2D Texture11 : register(t21, space1);
SamplerState Sampler00;
SamplerState Sampler01[2] : register(s3);
SamplerState Sampler10 : register(s5);
SamplerState Sampler11;

cbuffer ContantBuffer
{
	float4 uMult;
}

cbuffer ContantBuffer1 : register(b1)
{
	float4 uMult1;
}

float4 main( Vertex_Output In ) : SV_TARGET
{
	return uMult * uMult1 *
		(Texture00.Sample(Sampler00, In.UV) +
		Texture01[0].Sample(Sampler01[1], In.UV) +
		Texture10[2].Sample(Sampler10, In.UV) +
		Texture11.Sample(Sampler11, In.UV));
}