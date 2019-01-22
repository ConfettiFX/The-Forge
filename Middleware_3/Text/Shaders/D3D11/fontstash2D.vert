struct VsIn
{
	float2 position: Position;
	float2 texCoord: TEXCOORD0;
};

struct PsIn
{
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD0;
};

cbuffer uRootConstants : register(b0)
{
	float4 color;
	float2 scaleBias;
};

PsIn main(VsIn In)
{
	PsIn Out;
	Out.position = float4 (In.position, 0.0f, 1.0f);
	Out.position.xy = Out.position.xy * scaleBias.xy + float2(-1.0f, 1.0f);
	Out.texCoord = In.texCoord;
	return Out;
};