struct Vertex_Input
{
	float2 Position : POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct Vertex_Output
{
	float4  Position : SV_POSITION;
	min16float2 TexCoord : TEXCOORD0;
	min10float4 Color : COLOR;
};

cbuffer CB_RootConstant
{	
	float4 uProjScaleShift;
};

Vertex_Output main( Vertex_Input In )
{
	Vertex_Output Out;

	Out.Position =  float4(In.Position*uProjScaleShift.xy + uProjScaleShift.zw, 0.0, 1.0);
	Out.TexCoord = In.TexCoord;
	Out.Color = In.Color;
	
	return Out;
}