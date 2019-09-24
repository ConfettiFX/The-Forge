Texture2D sceneTexture : register(t6);

cbuffer BufferRootConstant : register(b21) 
{
	float2 ScreenSize;
}

struct PSIn {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

float4 main(PSIn input) : SV_TARGET
{
	int3 px = int3(input.TexCoord * ScreenSize, 0);
	
	float3 result = float3(0.0, 0.0, 0.0);
	result += sceneTexture.Load( px, int2(0, 0) ).rgb;
	result += sceneTexture.Load( px, int2(0, -1) ).rgb;
	result += sceneTexture.Load( px, int2(0, 1) ).rgb;
	result += sceneTexture.Load( px, int2(-1, 0) ).rgb;
	result += sceneTexture.Load( px, int2(1, 0) ).rgb;
	result += sceneTexture.Load( px, int2(-1, -1) ).rgb;
	result += sceneTexture.Load( px, int2(1, 1) ).rgb;
	result += sceneTexture.Load( px, int2(-1, 1) ).rgb;
	result += sceneTexture.Load( px, int2(1, -1) ).rgb;

	result *= 0.1;
	
	return float4(result.r, result.g, result.b, 1.0);
	
}