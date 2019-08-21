#include "Triangle.h"
#include "Helper.h"
#include "Geometry.h"

Triangle::Triangle(const vec3& v0, const vec3& v1, const vec3& v2,
	const vec3& n0, const vec3& n1, const vec3& n2)
	:
	mV0(v0),
	mV1(v1),
	mV2(v2),
	mN0(n0),
	mN1(n1),
	mN2(n2)
{
	mE1 = mV1 - mV0;
	mE2 = mV2 - mV0;
	mNormal = normalize(cross(mE2, mE1));
}

Triangle::~Triangle()
{
}


void Triangle::Init(const vec3& v0, const vec3& v1, const vec3& v2,
	const vec3& n0, const vec3& n1, const vec3& n2,
	const vec2& uv0, const vec2& uv1, const vec2& uv2)
{
	mV0 = v0;
	mV1 = v1;
	mV2 = v2;
	mN0 = n0;
	mN1 = n1;
	mN2 = n2;

	mE1 = mV1 - mV0;
	mE2 = mV2 - mV0;
	mNormal = normalize(cross(mE2, mE1));

	mUV0 = uv0;
	mUV1 = uv1;
	mUV2 = uv2;
}



void Triangle::Intersect(const Ray& ray, Intersection& outIntersection)
{
	vec3 P_V = cross(ray.mDir, mE2);

	float P_dot_E1 = dot(P_V, mE1);

	if (P_dot_E1 == 0.f)
	{
		return;
	}

	vec3 S_V = ray.mStartPos - mV0;

	float u_val = dot(P_V, S_V) / P_dot_E1;

	if (u_val < Epilson || u_val > 1.f)
	{
		return;
	}

	vec3 Q_V = cross(S_V, mE1);

	float v_val = dot(ray.mDir, Q_V) / P_dot_E1;

	if (v_val < Epilson || (v_val + u_val) > 1.f)
	{
		return;
	}

	float t_val = dot(mE2, Q_V) / P_dot_E1;

	if (t_val < Epilson)
	{
		return;
	}

	if (t_val < outIntersection.mIntersection_TVal)
	{
		vec2 barycentricUV = ((1.f - u_val - v_val) * mUV0 + u_val * mUV1 + v_val * mUV2);
		


		if (m_pMeshInstance->mIsAlphaTested)
		{
			CPUImage* texImageRef = m_pMeshInstance->mTextureImgRef;

			ivec4 color = Helper::LoadVec4WithUV(texImageRef, barycentricUV);
			if (color.getW() <= 0)
			{
				return;
			}
		}


		outIntersection.mIsIntersected = true;
		outIntersection.mIntersection_TVal = t_val;
		outIntersection.mHittedPos = ray.Eval(t_val);
		outIntersection.mHittedNormal = normalize
		(  
			( (1.f - u_val - v_val) * mN0 + u_val * mN1 + v_val * mN2 ) 
		);


		outIntersection.mHittedUV = barycentricUV;

		outIntersection.mIntersectedTriangle = this;
	}

}