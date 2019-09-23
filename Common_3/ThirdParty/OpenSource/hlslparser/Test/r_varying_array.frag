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
	float4 uColor;
	float  uWeights[5];
};

float4 main( Vertex_Output In ) : SV_TARGET
{
	float alpha = 0.0f;
	for(uint i = 0; i < 5; ++i)
	{
		alpha += Texture.Sample(Sampler, In.UV[i]) * uWeights[i];
	}
	return In.Color * uColor * alpha;
}