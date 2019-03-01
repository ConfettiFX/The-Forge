RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct RayPayload
{
	float3 color;
};

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
