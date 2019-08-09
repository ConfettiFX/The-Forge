#include "Interval.h"
#include <float.h>
#include "Helper.h"


Interval::Interval()
	:mT0(0),
	mT1(FLT_MAX)
{
}


Interval::Interval(float t0, float t1, const vec3& normal0,
	const vec3& normal1)
	:mT0(t0),
	mT1(t1),
	mNormal0(normal0),
	mNormal1(normal1)
{
}


Interval::~Interval()
{
}

void Interval::Empty()
{
	mT0 = 0.f;
	mT1 = FLT_MAX;
}

void Interval::Intersect(const Interval& other_interval)
{
	if (other_interval.mT0 > mT0)
	{
		mT0 = other_interval.mT0;
		mNormal0 = other_interval.mNormal0;
	}

	if (other_interval.mT1 < mT1)
	{
		mT1 = other_interval.mT1;
		mNormal1 = other_interval.mNormal1;
	}
}

void Interval::Intersect(const Ray& ray, const Slab& slab)
{
	float N_dot_D = dot(ray.mDir, slab.mNormal);
	float N_dot_Q = dot(ray.mStartPos, slab.mNormal);

	if (N_dot_D == 0.f)
	{
		float s0 = N_dot_Q + slab.mD0;
		float s1 = N_dot_Q + slab.mD1;

		if ((s0 > Epilson && s1 > Epilson) || (s0 < Epilson && s1 < Epilson))
		{
			mT0 = 0.f;
			mT1 = FLT_MAX;
			return;
		}

		mT0 = 1.f;
		mT1 = 0.f;
		return;
	}

	float t0 = -(slab.mD0 + N_dot_Q) / N_dot_D;
	float t1 = -(slab.mD1 + N_dot_Q) / N_dot_D;

	if (t1 < t0)
	{
		eastl::swap(t0, t1);
	}

	mT0 = t0;
	mT1 = t1;

	mNormal0 = slab.mNormal;
	mNormal1 = slab.mNormal;
}