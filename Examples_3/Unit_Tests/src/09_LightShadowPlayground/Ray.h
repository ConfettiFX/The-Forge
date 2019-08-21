#pragma once
#include "../../../../Common_3/OS/Math/MathTypes.h"


class Ray
{
public:
	Ray(const vec3& startPos,
		const vec3& dir)
		:mStartPos(startPos),
		mDir(dir)
	{
	}
	~Ray()
	{
	}
	
	vec3 Eval(float t) const
	{
		return mStartPos + (mDir * t);
	}

	vec3 GetInvDir() const
	{
		return vec3(1.f / mDir.getX(), 1.f / mDir.getY(), 1.f / mDir.getZ());
	}
	
	vec3 mStartPos;
	vec3 mDir;
};
