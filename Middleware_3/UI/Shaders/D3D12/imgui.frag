struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D uTex : register(t1, space2);
SamplerState uSampler : register(s2, space2);

float4 main(PS_INPUT input) : SV_Target
{
	return float4(input.col.xyz, input.col.a * uTex.Sample(uSampler, input.uv).r);
}
