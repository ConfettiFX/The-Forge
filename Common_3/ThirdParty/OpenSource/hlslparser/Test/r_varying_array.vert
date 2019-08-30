struct Vertex_Input
{
	float4 Position : POSITION;
	float4 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float4 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

Vertex_Output main(Vertex_Input In)
{
	Vertex_Output Out;
	Out.Position = In.Position;
	Out.UV = In.UV;
	Out.Color = In.Color;
	return Out;
}