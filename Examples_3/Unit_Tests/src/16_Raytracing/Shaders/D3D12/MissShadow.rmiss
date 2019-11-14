struct ShadowRayPayload
{
	bool miss;
};

[shader("miss")]
void missShadow(inout ShadowRayPayload payload : SV_RayPayload)
{
	payload.miss = true;
}
