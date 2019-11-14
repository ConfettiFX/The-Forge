RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct RayPayload
{
	float3 radiance;
	uint recursionDepth;
};

[shader("miss")]
void miss(inout RayPayload payload : SV_RayPayload)
{
	payload.radiance = float3(0.3, 0.6, 1.2);
}
