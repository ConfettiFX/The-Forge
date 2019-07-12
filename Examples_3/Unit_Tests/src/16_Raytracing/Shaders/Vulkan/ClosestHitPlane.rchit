#version 460
#extension GL_NV_ray_tracing : require

layout(binding = 0, set=0) uniform accelerationStructureNV gRtScene;

layout(binding = 2, set=0, std140) uniform gSettings
{
	vec3 CameraPosition;
	uint _pad0;
	vec3 LightDirection;
	uint _pad1;
	uint HitGroupIndex;
	uint MissGroupIndex;
	uint PlaneHitGroupIndex;
	uint PlaneMissGroupIndex;
};

hitAttributeNV vec3 attribs;
layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 2) rayPayloadNV vec3 hitValueShadow;

void main()
{
	float hitT = gl_HitTNV;
	vec3 rayDirW = gl_WorldRayDirectionNV;
	vec3 rayOriginW = gl_WorldRayOriginNV;

	// Find the world-space hit position
	vec3 posW = rayOriginW + hitT * rayDirW;

	vec4 origin 	= vec4(posW, 1.0);
    vec3 direction 	= normalize(LightDirection);
    uint rayFlags 	= gl_RayFlagsNoneNV;//settings.RayFlags; //gl_RayFlagsOpaqueNV;
    uint cullMask 	= 0xff;
    float tmin = 0.1;
    float tmax = 10000.0;
    traceNV(gRtScene, rayFlags, cullMask, PlaneHitGroupIndex, 0, PlaneMissGroupIndex, origin.xyz, tmin, direction.xyz, tmax, 2);

	hitValue = vec3(0.8f, 0.9f, 0.9f) * hitValueShadow;
}
