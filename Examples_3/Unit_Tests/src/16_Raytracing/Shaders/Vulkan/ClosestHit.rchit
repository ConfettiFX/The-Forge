#version 460
#extension GL_NV_ray_tracing : require
//#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInNV vec3 hitValue;

hitAttributeNV vec3 attribs;

void main()
{
	vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

	const vec3 A = vec3(1.0, 0.0, 0.0);
	const vec3 B = vec3(0.0, 1.0, 0.0);
	const vec3 C = vec3(0.0, 0.0, 1.0);

	hitValue = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
}
