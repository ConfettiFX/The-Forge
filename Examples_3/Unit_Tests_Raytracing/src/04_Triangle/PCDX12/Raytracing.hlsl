RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct RayPayload
{
	float3 color;
};

[shader("raygeneration")]
void rayGen()
{
	uint2 launchIndex = DispatchRaysIndex();
	uint2 launchDim = DispatchRaysDimensions();

	float2 crd = float2(launchIndex);
	float2 dims = float2(launchDim);

	float2 d = ((crd / dims) * 2.f - 1.f);
	float aspectRatio = dims.x / dims.y;

	RayDesc ray;
	ray.Origin = float3(0, 0, -2);
	ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

	ray.TMin = 0.001;
	ray.TMax = 10000;

	RayPayload payload;
	TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
	float3 col = payload.color;
	gOutput[launchIndex.xy] = float4(col, 1);
}

[shader("miss")]
void miss(inout RayPayload payload : SV_RayPayload)
{
	payload.color = float3(0.1, 0.1, 0.1);
}

struct IntersectionAttribs
{
	float2 baryCrd;
};

[shader("closesthit")]
void chs(inout RayPayload payload : SV_RayPayload, IntersectionAttribs attribs : SV_IntersectionAttributes)
{
	float3 barycentrics = float3(1.0 - attribs.baryCrd.x - attribs.baryCrd.y, attribs.baryCrd.x, attribs.baryCrd.y);

	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	payload.color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
}
