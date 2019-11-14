#version 460
#extension GL_NV_ray_tracing : require

struct RayPayload
{
	vec3 radiance;
	uint recursionDepth;
};

layout(location = 0) rayPayloadInNV RayPayload payload;

void main()
{
	payload.radiance = vec3(0.3, 0.6, 1.2);
}
