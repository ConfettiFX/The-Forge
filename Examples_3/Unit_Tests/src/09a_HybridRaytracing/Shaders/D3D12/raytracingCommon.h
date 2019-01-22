
#define USE_SHADOWS_AA 0

#define kEpsilon 0.00001
#define RAY_OFFSET 0.05

bool RayIntersectsBox(float3 origin, float3 rayDirInv, float3 BboxMin, float3 BboxMax)
{
	const float3 t0 = (BboxMin - origin) * rayDirInv;
	const float3 t1 = (BboxMax - origin) * rayDirInv;

	const float3 tmax = max(t0, t1);
	const float3 tmin = min(t0, t1);

	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
	const float a0 = max( max(tmin.x,tmin.y), max(tmin.z, 0.0f) );

	return a1 >= a0;
}

//Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
bool RayTriangleIntersect(
	const float3 orig,
	const float3 dir,
	float3 v0,
	float3 e0,
	float3 e1,
	inout float t,
	inout float2 bCoord)
{
	const float3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const float3 d = orig.xyz - v0;
	bCoord.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e0);
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
	const float3 orig,
	const float3 dir,
	float3 v0,
	float3 e0,
	float3 e1)
{
	float t = 0;
	float2 bCoord = 0;
	return RayTriangleIntersect(orig, dir, v0, e0, e1, t, bCoord);
}


