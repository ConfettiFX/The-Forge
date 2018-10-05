struct RayPayload
{
	float3 color;
};

[shader("miss")]
void missShadow(inout RayPayload payload : SV_RayPayload)
{
	payload.color = 0.0f;
}
