#pragma once
#include "Ray.h"
#include "Intersection.h"

struct SDFMeshInstance;

class Triangle 
{
public:
	Triangle() {};
	Triangle(const vec3& v0, const vec3& v1, const vec3& v2,
		const vec3& n0, const vec3& n1, const vec3& n2);


	void Init(const vec3& v0, const vec3& v1, const vec3& v2,
		const vec3& n0, const vec3& n1, const vec3& n2,
		const vec2& uv0, const vec2& uv1, const vec2& uv2);

	virtual ~Triangle();

	void Intersect(const Ray& ray, Intersection& outIntersection);

	//triangle vertices
	vec3 mV0;
	vec3 mV1;
	vec3 mV2;

	//triangle edges
	vec3 mE1;
	vec3 mE2;
	//triangle normal
	vec3 mNormal;

	//vertices normal
	vec3 mN0;
	vec3 mN1;
	vec3 mN2;

	vec2 mUV0;
	vec2 mUV1;
	vec2 mUV2;


	SDFMeshInstance* m_pMeshInstance;
	   
};

