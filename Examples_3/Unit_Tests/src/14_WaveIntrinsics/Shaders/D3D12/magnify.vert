struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

PSInput main(float3 position : POSITION, float2 uv : TEXCOORD)
{
	PSInput result;

	result.position = float4(position, 1.0f);
	result.uv = uv;

	return result;
}
