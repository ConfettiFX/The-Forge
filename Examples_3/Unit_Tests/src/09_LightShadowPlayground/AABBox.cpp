#include "AABBox.h"
#include "Slab.h"
#include "Interval.h"

void AABBox::Intersect(const Ray& ray, Intersection& outIntersection) const
{
	vec3 diagonalVector = m_max - m_min;

	Slab slab_x(vec3(1.f, 0.f, 0.f), -m_min.getX(),
		-m_min.getX() - diagonalVector.getX());

	Interval interval_x;

	interval_x.Intersect(ray, slab_x);


	Interval interval_y;

	Slab slab_y(vec3(0.f, 1.f, 0.f), -m_min.getY(),
		-m_min.getY() - diagonalVector.getY());

	interval_y.Intersect(ray, slab_y);

	Interval interval_z;

	Slab slab_z(vec3(0.f, 0.f, 1.f), -m_min.getZ(),
		-m_min.getZ() - diagonalVector.getZ());

	interval_z.Intersect(ray, slab_z);

	Interval final_interval;

	final_interval.Intersect(interval_x);
	final_interval.Intersect(interval_y);
	final_interval.Intersect(interval_z);


	if (final_interval.mT0 > final_interval.mT1 ||
		(final_interval.mT0 <= Epilson && final_interval.mT1 <= Epilson))
	{
		return;
	}

	float final_t = 0.f;
	vec3 final_normal;

	if (final_interval.mT0 > Epilson && final_interval.mT0 < final_interval.mT1)
	{
		final_t = final_interval.mT0;
		final_normal = final_interval.mNormal0;
	}
	else
	{
		final_t = final_interval.mT1;
		final_normal = -final_interval.mNormal1;
	}


	if (final_t < outIntersection.mIntersection_TVal)
	{
		outIntersection.mIsIntersected = true;
		outIntersection.mHittedPos = ray.Eval(final_t);
		outIntersection.mHittedNormal = normalize(final_normal);
		outIntersection.mIntersection_TVal = final_t;
	}
}