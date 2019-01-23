RaytracingAccelerationStructure gRtScene : register(t0);

cbuffer gLightDirectionRootConstant : register(b0)
{
	float3 direction;
}

struct RayPayload
{
	float3 color;
};

struct IntersectionAttribs
{
	float2 baryCrd;
};

[shader("closesthit")]
void chsPlane(inout RayPayload payload : SV_RayPayload, IntersectionAttribs attribs : SV_IntersectionAttributes)
{
	float hitT = RayTCurrent();
	float3 rayDirW = WorldRayDirection();
	float3 rayOriginW = WorldRayOrigin();

	// Find the world-space hit position
	float3 posW = rayOriginW + hitT * rayDirW;

	// Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
	RayDesc ray;
	ray.Origin = posW;
	ray.Direction = normalize(direction);
	ray.TMin = 0.01;
	ray.TMax = 100000;
	RayPayload shadowPayload;
	TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);

	float factor = shadowPayload.color.x > 0.0f ? 0.1 : 1.0;
	payload.color = float3(0.8f, 0.9f, 0.9f) * factor;
}
