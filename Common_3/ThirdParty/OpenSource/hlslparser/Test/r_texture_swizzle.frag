struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float4 UV   : TEXCOORD0;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

cbuffer CB_RootConstant
{	
	float4   uEdge;
};

float4 main( Vertex_Output In ) : SV_TARGET
{
	return step(uEdge.x, Texture.Sample(Sampler, In.UV).x).xxxx
	       + step(uEdge.xy, Texture.Sample(Sampler, In.UV).xy).xyxx
	       + step(uEdge.xyz, Texture.Sample(Sampler, In.UV).xyz).xyzx
	       + step(uEdge.xyzw, Texture.Sample(Sampler, In.UV).xyzw).xyzw;
}