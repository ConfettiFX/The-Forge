struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float2 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

Texture2D Texture;
SamplerState Sampler;

cbuffer CB_RootConstant
{	
	float4 uColor;
};

float4 main( Vertex_Output In ) : SV_TARGET
{
	float uWeights[5] = {1, 2, 3, 4, 5};
	float alpha = 0.0f;
	for(uint i = 0; i < 5; ++i)
	{
		alpha += Texture.Sample(Sampler, In.UV[i]) * uWeights[i];
	}
	return In.Color * uColor * alpha;
}