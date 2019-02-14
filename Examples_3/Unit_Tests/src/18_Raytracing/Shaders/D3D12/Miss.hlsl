RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct RayPayload
{
	float3 color;
};

[shader("miss")]
void miss(inout RayPayload payload : SV_RayPayload)
{
	payload.color = float3(0.1, 0.1, 0.1);
}
