#pragma once
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "Ray.h"
#include "Slab.h"


class Interval
{
public:
	Interval();
	Interval(float t0, float t1, const vec3& normal0,
		const vec3& normal1);

	~Interval();

	void Empty();

	void Intersect(const Interval& other_interval);
	void Intersect(const Ray& ray, const Slab& slab);

	vec3 mNormal0;
	float mT0;

	vec3 mNormal1;
	float mT1;
};
