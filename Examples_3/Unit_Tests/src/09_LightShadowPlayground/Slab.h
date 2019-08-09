#pragma once
#include "../../../../Common_3/OS/Math/MathTypes.h"


class Slab {
public:
	Slab(const vec3& normal, float d0, float d1)
		:mNormal(normal),
		mD0(d0),
		mD1(d1)
	{
	}
	~Slab()
	{}

	vec3 mNormal;
	float mD0;
	float mD1;
};
