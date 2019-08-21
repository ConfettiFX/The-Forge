#pragma once
#include  <float.h>

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "Intersection.h"
#include "Ray.h"
#include "Helper.h"

class AABBox
{
public:
	vec3 m_min;
	vec3 m_max;

	inline AABBox() { Reset(); }
	inline AABBox(const vec3& boxMin, const vec3& boxMax) : m_min(boxMin), m_max(boxMax) { }


	void Intersect(const Ray& ray, Intersection& outIntersection) const;

	

	vec3 GetExtent() const
	{
		return 0.5f * (m_max - m_min);
	}

	void Reset()
	{
		m_min = vec3(FLT_MAX);
		m_max = vec3(-FLT_MAX);
	}

	//align bounding box so that its min and max vectors data value
	//is always in the multiple of the alignemnt
	static inline void AlignBBox(AABBox& BBox, float alignment)
	{
		vec3 boxMin = BBox.m_min / alignment; 
		boxMin = vec3(floorf(boxMin.getX()) * alignment, floorf(boxMin.getY()) * alignment, 0.0f);
		vec3 boxMax = BBox.m_max / alignment; 
		boxMax = vec3(ceilf(boxMax.getX()) * alignment, ceilf(boxMax.getY()) * alignment, 0.0f);
		BBox = AABBox(boxMin, boxMax);
	}

	bool IsInvalid() const
	{
		return false;
		//return !(m_min <= m_max);
	}

	void Add(const vec3& v)
	{
		m_min.setX(fmin(v.getX(), m_min.getX()));
		m_min.setY(fmin(v.getY(), m_min.getY()));
		m_min.setZ(fmin(v.getZ(), m_min.getZ()));

		m_max.setX(fmax(v.getX(), m_max.getX()));
		m_max.setY(fmax(v.getY(), m_max.getY()));
		m_max.setZ(fmax(v.getZ(), m_max.getZ()));
	}

	void Add(const AABBox& aabb)
	{
		m_min.setX(fmin(aabb.m_min.getX(), m_min.getX()));
		m_min.setY(fmin(aabb.m_min.getY(), m_min.getY()));
		m_min.setZ(fmin(aabb.m_min.getZ(), m_min.getZ()));

		m_max.setX(fmax(aabb.m_max.getX(), m_max.getX()));
		m_max.setY(fmax(aabb.m_max.getY(), m_max.getY()));
		m_max.setZ(fmin(aabb.m_max.getZ(), m_max.getZ()));
	}

	const vec3 GetSize() const { return m_max - m_min; }
	const vec3 GetCenter() const { return (m_max + m_min) * 0.5f; }

	float GetSizeX() const { return m_max.getX() - m_min.getX(); }
	float GetSizeY() const { return m_max.getY() - m_min.getY(); }
	float GetSizeZ() const { return m_max.getZ() - m_min.getZ(); }

	bool IsIntersectBox(const AABBox& b) const
	{
		if ((m_min.getX() > b.m_max.getX()) || (b.m_min.getX() > m_max.getX())) return false;
		if ((m_min.getY() > b.m_max.getY()) || (b.m_min.getY() > m_max.getY())) return false;
		if ((m_min.getZ() > b.m_max.getZ()) || (b.m_min.getZ() > m_max.getZ())) return false;
		return true;
	}

	bool ContainsPoint(const vec3& point) const
	{


		bool biggerThanMin = point.getX() >= m_min.getX() && 
			point.getY() >= m_min.getY() && point.getZ() >= m_min.getZ();

		bool lesserThanMax = point.getX() <= m_max.getX() && 
			point.getY() <= m_max.getY() && point.getZ() <= m_max.getZ();

		return biggerThanMin && lesserThanMax;
	}

	bool operator != (const AABBox& rhs) const
	{
		return (m_min != rhs.m_min) | (m_max != rhs.m_max);
	}

	bool operator == (const AABBox& rhs) const
	{
		return (m_min == rhs.m_min) & (m_max == rhs.m_max);
	}

	
	void TransformTo(const mat4& tm, AABBox& result) const
	{
		vec3 boxSize = m_max - m_min;

		vec4 vx = Helper::Piecewise_Prod(tm.getCol0() , vec4(boxSize.getX()));
		vec4 vy = Helper::Piecewise_Prod(tm.getCol1(), vec4(boxSize.getY()));
		vec4 vz = Helper::Piecewise_Prod(tm.getCol2(), vec4(boxSize.getZ()));

		//Vec4 vx = tm.getCol0() * Vec4::Swizzle<::x, ::x, ::x, ::x>(boxSize);
		//Vec4 vy = tm.r[1] * Vec4::Swizzle<::y, ::y, ::y, ::y>(boxSize);
		//Vec4 vz = tm.r[2] * Vec4::Swizzle<::z, ::z, ::z, ::z>(boxSize);

		
		vec3 newMin = Helper::Vec4_To_Vec3( tm * vec4(m_min, 1.0) );
		vec3 newMax = newMin;

		//min(Helper::Vec4_To_Vec3(vx), vec3(0.0));

		newMin += Helper::Min_Vec3(Helper::Vec4_To_Vec3(vx), vec3(0.0));
		newMax += Helper::Max_Vec3(Helper::Vec4_To_Vec3(vx), vec3(0.0));

		newMin += Helper::Min_Vec3(Helper::Vec4_To_Vec3(vy), vec3(0.0));
		newMax += Helper::Max_Vec3(Helper::Vec4_To_Vec3(vy), vec3(0.0));

		newMin += Helper::Min_Vec3(Helper::Vec4_To_Vec3(vz), vec3(0.0));
		newMax += Helper::Max_Vec3(Helper::Vec4_To_Vec3(vz), vec3(0.0));

		result = AABBox(newMin, newMax);
	}
};

