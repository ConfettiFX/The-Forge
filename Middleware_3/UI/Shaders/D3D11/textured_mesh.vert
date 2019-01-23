struct VsIn
{
	float2 position : Position;
	float2 texcoord : Texcoord;
};

struct VsOut
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

cbuffer uRootConstants : register(b0)
{
	float4 color;
	float2 scaleBias;
};

VsOut main(VsIn input)
{
	VsOut output = (VsOut)0;
	output.position = float4(input.position.xy * scaleBias.xy + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	output.texcoord = input.texcoord;

	return output;
};