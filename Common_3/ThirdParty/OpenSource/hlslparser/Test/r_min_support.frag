struct Vertex_Output
{
	float4  Position : SV_POSITION;
	min16float2 TexCoord : TEXCOORD0;
	min16float2 TexCoord2 : TEXCOORD1;
	min10float4 Color : COLOR;
};

Texture2D Texture : register(t0);
Texture2D Masked : register(t1);

SamplerState Sampler : register(s0);

float4 main( Vertex_Output In ) : SV_TARGET
{
	float4 color = Masked.Sample( Sampler, In.TexCoord2) * In.Color;
	float alpha = Texture.Sample( Sampler, In.TexCoord ).x;

	return float4( color.rgb, alpha * color.a );
}