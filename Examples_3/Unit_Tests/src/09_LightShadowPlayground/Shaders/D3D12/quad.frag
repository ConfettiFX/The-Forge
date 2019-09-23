



struct VSOutput
{
	float4 Position  : SV_POSITION;
	float2 Tex_Coord : TEXCOORD0;
};

SamplerState clampNearSampler : register(s0, UPDATE_FREQ_NONE);
Texture2D screenTexture : register(t1);

float LinearizeDepth(float depth)
{
	const float nearPlane = 0.1f;
	const float farPlane = 1000.f;

    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float4 main(VSOutput input) : SV_TARGET
{
	/*float depthValue = screenTexture.Sample(clampNearSampler, input.Tex_Coord).x;

	if(depthValue == 1.0)
	{
		return float4(1.0, 0.0, 0.0, 1.0);
	}

	//depthValue = LinearizeDepth(depthValue) / 1000.f;
	//float4 screen_color = screenTexture.Sample(uSampler0, input.Tex_Coord);
	//return screen_color;
	return float4(depthValue, depthValue, depthValue, 1.0);	*/

	float3 color = screenTexture.Sample(clampNearSampler, input.Tex_Coord).xyz;
	//float rcolor = screenTexture.Sample(clampNearSampler, input.Tex_Coord).x;
	//float3 color = float3(rcolor,rcolor,rcolor);
	return float4(color, 1.0);
}