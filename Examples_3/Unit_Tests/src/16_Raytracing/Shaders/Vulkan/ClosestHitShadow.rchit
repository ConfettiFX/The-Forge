#version 460
#extension GL_NV_ray_tracing : require

layout(location = 2) rayPayloadInNV vec3 hitValueShadow;

void main()
{
	hitValueShadow = vec3(0.1, 0.1, 0.1);
}
