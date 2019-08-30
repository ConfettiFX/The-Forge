struct Vertex_Input
{
	float4 Position : POSITION;
	float4 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

struct Vertex_Input2
{
	float4 Color  : COLOR1;
};

struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float4 UV[5]   : TEXCOORD0;
	float4 Color  : COLOR;
};

cbuffer CBuffer
{
    float4 p;
};

Vertex_Output main(Vertex_Input In, float s:TEXCOORD5, Vertex_Input2 In2)
{
	Vertex_Output Out;
	Out.Position = In.Position;
	Out.UV = In.UV;
	Out.Color = s * p * (In.Color + In2.Color);
	return Out;
}