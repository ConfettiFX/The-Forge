RaytracingAccelerationStructure		gRtScene	: register(t0);

cbuffer gSettings : register(b1)
{
	uint HitGroupIndex;
	uint MissGroupIndex;
}

cbuffer Settings : register(b10)
{
	float3 CameraPosition;
	float3 LightDirection;
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
	//payload.color = float3(1.0, 1.0, 1.0);
	//return;

	float hitT = RayTCurrent();
	float3 rayDirW = WorldRayDirection();
	float3 rayOriginW = WorldRayOrigin();

	// Find the world-space hit position
	float3 posW = rayOriginW + hitT * rayDirW;

	// Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
	RayDesc ray;
	ray.Origin = posW;
	ray.Direction = normalize(LightDirection);
	ray.TMin = 0.01;
	ray.TMax = 100000;
	RayPayload shadowPayload;
	//TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 3 /* ray index*/, 0, 1, ray, shadowPayload);
	TraceRay(gRtScene, 0, 0xFF, HitGroupIndex, 0, MissGroupIndex, ray, shadowPayload);
	/*
	void TraceRay(RaytracingAccelerationStructure AccelerationStructure,
			  uint RayFlags,
			  uint InstanceInclusionMask,
			  uint RayContributionToHitGroupIndex,
			  uint MultiplierForGeometryContributionToHitGroupIndex,
			  uint MissShaderIndex,
			  RayDesc Ray,
			  inout payload_t Payload);
	*/

	//payload.color = float3(0.8f, 0.9f, 0.9f);
	payload.color = float3(0.8f, 0.9f, 0.9f) * shadowPayload.color;
}
