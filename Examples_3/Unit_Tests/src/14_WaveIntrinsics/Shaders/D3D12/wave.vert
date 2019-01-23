cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 orthProjMatrix;
	float2 mousePosition;
	float2 resolution;
	float time;
	uint renderMode;
	uint laneSize;
	uint padding;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput main(float3 position : POSITION, float4 color : COLOR)
{
	PSInput result;

	result.position = mul(orthProjMatrix, float4(position, 1.0f));
	result.color = color;

	return result;
}
