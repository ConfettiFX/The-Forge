
#define USE_SHADOWS_AA 0

#define kEpsilon 0.00001
#define RAY_OFFSET 0.05

bool RayIntersectsBox(in vec3 origin, in vec3 rayDirInv, in vec3 BboxMin, in vec3 BboxMax)
{
	const vec3 t0 = (BboxMin - origin) * rayDirInv;
	const vec3 t1 = (BboxMax - origin) * rayDirInv;

	const vec3 tmax = max(t0, t1);
	const vec3 tmin = min(t0, t1);

	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
	const float a0 = max(max(tmin.x, tmin.y), max(tmin.z, 0.0f));

	return a1 >= a0;
}

//Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
bool RayTriangleIntersect(
	const vec3 orig,
	const vec3 dir,
	vec3 v0,
	vec3 e0,
	vec3 e1,
	in out float t,
	in out vec2 bCoord)
{
	const vec3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const vec3 d = orig.xyz - v0;
	bCoord.x = dot(d, s1) * invd;
	const vec3 s2 = cross(d, e0);
	bCoord.y = dot(dir.xyz, s2) * invd;
	t = dot(e1, s2) * invd;

	if (
#if BACKFACE_CULLING
		dot(s1, e0) < -kEpsilon ||
#endif
		bCoord.x < 0.0 || bCoord.x > 1.0 || bCoord.y < 0.0 || (bCoord.x + bCoord.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool RayTriangleIntersect(
	const vec3 orig,
	const vec3 dir,
	vec3 v0,
	vec3 e0,
	vec3 e1)
{
	float t = 0;
	vec2 bCoord = vec2(0, 0);
	return RayTriangleIntersect(orig, dir, v0, e0, e1, t, bCoord);
}



