struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float4 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

Texture2D Texture;
SamplerState Sampler;

cbuffer CB_RootConstant
{	
	float4 color;
	float  weights[5];
};

float apply_filter(Texture2D tex, SamplerState smp, float4 uv[5], float color)
{
	float result = 0.0;
	for(uint i = 0; i < 5; ++i)
	{
		result += Texture.Sample(Sampler, uv[i]) * weights[i];
	}
	return color * result;
}

float4 main( Vertex_Output In ) : SV_TARGET
{
	return In.Color * apply_filter(Texture, Sampler, In.UV, color.r);
}
