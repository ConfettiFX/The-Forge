#pragma once
#include  <float.h>
#include "AABBox.h"

#include "Camera.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"


class ConvexHull2D
{
public:
	static const unsigned int MAX_VERTICES = 5;
	vec2 m_vertices[MAX_VERTICES] = {vec2(0.f), vec2(0.f), vec2(0.f), vec2(0.f), vec2(0.f)};
	int m_size;

	ConvexHull2D()
	{
		Reset();
	}

	ConvexHull2D(int numVertices, const vec2* pVertices)
	{
		m_size = FindConvexHull(numVertices, pVertices, m_vertices);
	}

	void Reset()
	{
		m_size = 0;
	}

	const vec2 FindFrustumConvexHull(const Camera& frustum, float frustumZMaxOverride, const mat4& viewProj)
	{
		static const unsigned int numVertices = 5;
		vec3 frustumPos = frustum.GetPosition();
		vec2 vertices[numVertices] = { Helper::Vec3_To_Vec2( Helper::Project(frustum.GetPosition(), 1.f, viewProj) ) };

		mat4 projMat = frustum.GetProjection();

		

		float hz = Helper::Project(vec3(0, 0, frustumZMaxOverride), 1.f, projMat).getZ();

		const vec3 frustumCorners[] =
		{
			vec3(-1.0f, -1.0f, hz),
			vec3(+1.0f, -1.0f, hz),
			vec3(+1.0f, +1.0f, hz),
			vec3(-1.0f, +1.0f, hz),
		};

		
		mat4 tm = viewProj * frustum.GetViewProjectionInverse();
		for (unsigned int i = 1; i < numVertices; ++i)
		{
			vertices[i] = Helper::Vec3_To_Vec2(Helper::Project(frustumCorners[i - 1], 1.f, tm));
		}

		m_size = FindConvexHull(numVertices, vertices, m_vertices);
		

		return vertices[0];
	}

	bool Intersects(const AABBox& BBox) const
	{
		if (m_size == 0)
		{
			return false;
		}

		static const vec2 normals[] =
		{
			vec2(1, 0),
			vec2(0, 1),
			vec2(-1, 0),
			vec2(0,-1),
		};

		vec2 vb1[MAX_VERTICES * 2];
		vec2 vb2[MAX_VERTICES * 2];

		const vec2* v = m_vertices;
		int n = m_size;

		

		int j, index[2];
		float d[2];
		for (int i = 0; i < 4; ++i)
		{
			float pw = -dot(Helper::Vec2_To_Vec3(normals[i]), i < 2 ? BBox.m_min : BBox.m_max);
			index[1] = n - 1;
			d[1] = dot(normals[i], v[index[1]]) + pw;
			for (j = 0; j < n; j++)
			{
				index[0] = index[1];
				index[1] = j;
				d[0] = d[1];
				d[1] = dot(normals[i], v[index[1]]) + pw;
				if (d[1] > 0 && d[0] < 0) break;
			}
			if (j < n)
			{
				int k = 0;
				vec2* tmp = v == vb1 ? vb2 : vb1;
				tmp[k++] = Helper::Vec3_To_Vec2( 
					lerp(
						Helper::Vec2_To_Vec3( v[index[1]] ), 
						Helper::Vec2_To_Vec3( v[index[0]] ), 
						d[1] / (d[1] - d[0])
					) 
				);
				do
				{
					index[0] = index[1];
					index[1] = (index[1] + 1) % n;
					d[0] = d[1];
					d[1] = dot(normals[i], v[index[1]]) + pw;
					tmp[k++] = v[index[0]];
				} while (d[1] > 0);
				tmp[k++] = Helper::Vec3_To_Vec2(
					lerp(
						Helper::Vec2_To_Vec3(v[index[1]]), 
						Helper::Vec2_To_Vec3( v[index[0]] ), 
						d[1] / (d[1] - d[0])
					) 
				);
				n = k;
				v = tmp;
			}
			else
			{
				if (d[1] < 0) return false;
			}
		}
		return n > 0;
	}

	static int FindConvexHull(int numVertices, const vec2* pVertices, vec2* pHull)
	{
		//_ASSERT(numVertices <= MAX_VERTICES);
		const float eps = 1e-5f;
		const float epsSq = eps * eps;
		int leftmostIndex = 0;
		for (int i = 1; i < numVertices; ++i)
		{
			float f = pVertices[leftmostIndex].getX() - pVertices[i].getX();
			if (fabsf(f) < epsSq)
			{
				if (pVertices[leftmostIndex].getY() > pVertices[i].getY())
					leftmostIndex = i;
			}
			else if (f > 0)
			{
				leftmostIndex = i;
			}
		}
		vec2 dir0(0, -1);
		int hullSize = 0;
		int index0 = leftmostIndex;
		do
		{
			float maxCos = -FLT_MAX;
			int index1 = -1;
			vec2 dir1;
			for (int j = 1; j < numVertices; ++j)
			{
				int k = (index0 + j) % numVertices;
				vec2 v = pVertices[k] - pVertices[index0];
			

				float l = lengthSqr(v);
				if (l > epsSq)
				{
					vec2 d = normalize(v);
					float f = dot(d, dir0);
					if (maxCos < f)
					{
						maxCos = f;
						index1 = k;
						dir1 = d;
					}
				}
			}
			if (index1 < 0 || hullSize >= numVertices)
			{
				//_ASSERT(!"epic fail");
				return 0;
			}
			pHull[hullSize++] = pVertices[index1];
			index0 = index1;
			dir0 = dir1;
		} while (lengthSqr(pVertices[index0] - pVertices[leftmostIndex]) > epsSq);
		return hullSize;
	}
};

