struct PsIn
{
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD0;
};

cbuffer uRootConstants : register(b0)
{
	float4 color;
	float2 scaleBias;
};

Texture2D uTex0 : register(t1);
SamplerState uSampler0 : register(s2);

float4 main(PsIn In) : SV_Target
{
	return float4(1.0, 1.0, 1.0, uTex0.Sample(uSampler0, In.texCoord).r) * color;
}