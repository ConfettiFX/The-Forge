struct RayPayload
{
	float3 color;
};

struct IntersectionAttribs
{
	float2 baryCrd;
};

[shader("closesthit")]
void chsShadow(inout RayPayload payload : SV_RayPayload, IntersectionAttribs attribs : SV_IntersectionAttributes)
{
	payload.color = 0.1f;
}
