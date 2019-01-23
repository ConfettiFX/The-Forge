struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D uTex : register(t1);
SamplerState uSampler : register(s2);

float4 main(PS_INPUT input) : SV_Target
{
	float4 out_col = input.col * uTex.Sample(uSampler, input.uv);
	return out_col;
}