struct VertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV  : TEXCOORD;
	float4 Color : COLOR;
};

float4 SampleTex( Texture2D tex, SamplerState smp, float2 uv )
{
	float4 color = tex.Sample( smp, uv );
	color.rgb *= color.a;
	return color;
}

Texture2D uTexture : register(t0);
Texture2D uTextureArr[2] : register(t1);
SamplerState uSampler : register(s4);

float4 main( VertexOutput In ) : SV_TARGET
{
    float4 tex2 = uTextureArr[0].Sample(uSampler, In.UV);
    float4 tex = SampleTex( uTexture, uSampler, In.UV);
    tex.rgb =  tex.rgb + In.Color.rgb * tex.a;
    tex = tex * In.Color.a;
    return tex+tex2;
}