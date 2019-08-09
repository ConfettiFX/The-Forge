#pragma once
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include <float.h>

class Triangle;

class Intersection
{
public:
	Intersection(const vec3& hitted_pos,
		const vec3& hitted_normal,
		float t_intersection)
		:mIsIntersected(false),
		mHittedPos(hitted_pos),
		mHittedNormal(hitted_normal),
		mIntersection_TVal(t_intersection)
	{

	}
	Intersection()
		:mIsIntersected(false),
		mHittedPos(),
		mHittedNormal(),
		mIntersection_TVal(FLT_MAX),
		mHittedUV()
	{

	}
	~Intersection(){};


	bool mIsIntersected;
	vec3 mHittedPos;
	vec3 mHittedNormal;
	vec2 mHittedUV;
	float mIntersection_TVal;
	Triangle* mIntersectedTriangle;
	
};