/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

struct DerivativesOutput
{
	float3 db_dx;
	float3 db_dy;
};

// 2D interpolation results for texture gradient values
struct GradientInterpolationResults
{
	float2 interp;
	float2 dx;
	float2 dy;
};

// Barycentric coordinates and gradients, struct needed to interpolate values.
struct BarycentricDeriv
{
	float3 m_lambda;
	float3 m_ddx;
	float3 m_ddy;
};

#ifdef USE_RAY_DIFFERENTIALS
// Calculates the local (barycentric coordinates) position of a ray hitting a triangle (Muller-Trumbore algorithm)
// Parameters: p0,p1,p2 -> World space coordinates of triangle
// o -> Origin of ray in world space (Mainly view camera here)
// d-> Unit vector direction of ray from origin
float3 rayTriangleIntersection(float3 p0, float3 p1, float3 p2, float3 o, float3 d)
{
	float3 v0v1 = p1-p0;
	float3 v0v2 = p2-p0;
	float3 pvec = cross(d,v0v2);
	float det = dot(v0v1,pvec);
	float invDet = 1/det;
	float3 tvec = o - p0;
	float u = dot(tvec,pvec) * invDet;
	float3 qvec = cross(tvec,v0v1);
	float v = dot(d,qvec) *invDet;
	float w = 1.0f - v - u;
	return float3(w,u,v);
}

// Calculate ray differentials for a point in world-space
// Parameters: pt0,pt1,pt2 -> world space coordinates of the triangle currently visible on the pixel
// position -> world-space calculated position of the current pixel by reconstructing Z value
// positionDX,positionDY -> world-space positions a pixel footprint right and down of the calculated position w.r.t traingle
BarycentricDeriv CalcRayBary(float3 pt0, float3 pt1, float3 pt2,float3 position,float3 positionDX,float3 positionDY,
								float3 camPos)
{
	BarycentricDeriv ret ;

	// Calculating unit vector directions of all 3 rays
	float3 curRay = position - camPos;
	float3 rayDX = positionDX - camPos;
	float3 rayDY = positionDY - camPos;
	// Calculating barycentric coordinates of each rays hitting the triangle
	float3 H = rayTriangleIntersection(pt0,pt1,pt2,camPos,normalize(curRay));
	float3 Hx = rayTriangleIntersection(pt0,pt1,pt2,camPos,normalize(rayDX));
	float3 Hy = rayTriangleIntersection(pt0,pt1,pt2,camPos,normalize(rayDY));
	ret.m_lambda = H;
	// Ray coordinates differential
	ret.m_ddx = Hx-H;
	ret.m_ddy = Hy-H;
	return ret;
}
#endif

// Computes the partial derivatives of a triangle from the homogeneous clip space vertices
BarycentricDeriv CalcFullBary(float4 pt0, float4 pt1, float4 pt2, float2 pixelNdc, float2 two_over_windowsize)
{
	BarycentricDeriv ret;
	float3 invW =  rcp(float3(pt0.w, pt1.w, pt2.w));
	//Project points on screen to calculate post projection positions in 2D
	float2 ndc0 = pt0.xy * invW.x;
	float2 ndc1 = pt1.xy * invW.y;
	float2 ndc2 = pt2.xy * invW.z;

	// Computing partial derivatives and prospective correct attribute interpolation with barycentric coordinates
	// Equation for calculation taken from Appendix A of DAIS paper:
	// https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf

	// Calculating inverse of determinant(rcp of area of triangle).
	float invDet = rcp(determinant(float2x2(ndc2 - ndc1, ndc0 - ndc1)));

	//determining the partial derivatives
	// ddx[i] = (y[i+1] - y[i-1])/Determinant
	ret.m_ddx = float3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
	ret.m_ddy = float3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;
	// sum of partial derivatives.
	float ddxSum = dot(ret.m_ddx, float3(1,1,1));
	float ddySum = dot(ret.m_ddy, float3(1,1,1));
	
	// Delta vector from pixel's screen position to vertex 0 of the triangle.
	float2 deltaVec = pixelNdc - ndc0;

	// Calculating interpolated W at point.
	float interpInvW = invW.x + deltaVec.x*ddxSum + deltaVec.y*ddySum;
	float interpW = rcp(interpInvW);
	// The barycentric co-ordinate (m_lambda) is determined by perspective-correct interpolation. 
	// Equation taken from DAIS paper.
	ret.m_lambda.x = interpW * (invW[0] + deltaVec.x*ret.m_ddx.x + deltaVec.y*ret.m_ddy.x);
	ret.m_lambda.y = interpW * (0.0f    + deltaVec.x*ret.m_ddx.y + deltaVec.y*ret.m_ddy.y);
	ret.m_lambda.z = interpW * (0.0f    + deltaVec.x*ret.m_ddx.z + deltaVec.y*ret.m_ddy.z);

	//Scaling from NDC to pixel units
	ret.m_ddx *= two_over_windowsize.x;
	ret.m_ddy *= two_over_windowsize.y;
	ddxSum    *= two_over_windowsize.x;
	ddySum    *= two_over_windowsize.y;

	ret.m_ddy *= -1.0f;
	ddySum *= -1.0f;

	// This part fixes the derivatives error happening for the projected triangles.
	// Instead of calculating the derivatives constantly across the 2D triangle we use a projected version
	// of the gradients, this is more accurate and closely matches GPU raster behavior.
	// Final gradient equation: ddx = (((lambda/w) + ddx) / (w+|ddx|)) - lambda

	// Calculating interpW at partial derivatives position sum.
	float interpW_ddx = 1.0f / (interpInvW + ddxSum);
	float interpW_ddy = 1.0f / (interpInvW + ddySum);

	// Calculating perspective projected derivatives.
	ret.m_ddx = interpW_ddx*(ret.m_lambda*interpInvW + ret.m_ddx) - ret.m_lambda;
	ret.m_ddy = interpW_ddy*(ret.m_lambda*interpInvW + ret.m_ddy) - ret.m_lambda;  

	return ret;
}

// Helper functions to interpolate vertex attributes using derivatives.

// Interpolate a float3 vector.
float InterpolateWithDeriv_float3(BarycentricDeriv deriv, float3 v)
{
	return dot(v,deriv.m_lambda);
}
// Interpolate single values over triangle vertices.
float InterpolateWithDeriv(BarycentricDeriv deriv, float v0, float v1, float v2)
{
	float3 mergedV = float3(v0, v1, v2);
	return InterpolateWithDeriv_float3(deriv,mergedV);
}

// Interpolate a float3 attribute for each vertex of the triangle.
// Attribute parameters: a 3x3 matrix (Row denotes attributes per vertex).
float3 InterpolateWithDeriv_float3x3(BarycentricDeriv deriv,f3x3 attributes)
{
	float3 attr0 = getCol0(attributes);
	float3 attr1 = getCol1(attributes);
	float3 attr2 = getCol2(attributes);
	return float3(dot(attr0,deriv.m_lambda),dot(attr1,deriv.m_lambda),dot(attr2,deriv.m_lambda));
}

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
// Attribute paramters: a 3x2 matrix of float2 attributes (Column denotes attribuets per vertex).
GradientInterpolationResults Interpolate2DWithDeriv(BarycentricDeriv deriv,f3x2 attributes)
{
	float3 attr0 = getRow0(attributes);
	float3 attr1 = getRow1(attributes);
	
	GradientInterpolationResults result;
	// independently interpolate x and y attributes.
	result.interp.x = InterpolateWithDeriv_float3(deriv,attr0);
	result.interp.y = InterpolateWithDeriv_float3(deriv,attr1);

	// Calculate attributes' dx and dy (for texture sampling).
	result.dx.x = dot(attr0,deriv.m_ddx);
	result.dx.y = dot(attr1,deriv.m_ddx);
	result.dy.x = dot(attr0,deriv.m_ddy);
	result.dy.y = dot(attr1,deriv.m_ddy);
	return result;
}

// generates dx and dy for for 3x3 attributes
DerivativesOutput Cal3DDeriv(BarycentricDeriv deriv,f3x3 attributes)
{
	DerivativesOutput result;
	result.db_dx = mul(attributes,deriv.m_ddx);
	result.db_dy = mul(attributes,deriv.m_ddy);

	return result;
}

float depthLinearization(float depth, float near, float far)
{
	return (2.0 * near) / (far + near - depth * (far - near));
}

float depthLinearizationRevZ(float depth, float near, float far)
{
	return (far * near) / (depth * (far - near) + near);
}

float3 perturb_normal(float3 normalMap, float3 normalVertex, DerivativesOutput dposition, DerivativesOutput duv)
{
	float3 dPdx = dposition.db_dx;
	float3 dPdy = dposition.db_dy;

	float2 dUVdx = duv.db_dx.xy;
	float2 dUVdy = duv.db_dy.xy;

	float3 crossPdyN = cross(dPdy, normalVertex);
	float3 crossNPdx = cross(normalVertex, dPdx); 

	float3 tangent = crossPdyN * dUVdx.x + crossNPdx * dUVdy.x;
	float3 binormal = crossPdyN * dUVdx.y + crossNPdx * dUVdy.y;

	float invScale = rsqrt(max(dot(tangent, tangent), dot(binormal, binormal)));

	float3x3 TBN = make_f3x3_rows(tangent * invScale, binormal * invScale, normalVertex);
	
	return normalize(mul(normalMap, TBN));
}

uint64_t packDepthVBId(float depth, uint vbID)
{
	uint64_t depthU64 = asuint(depth);
	uint64_t packedU64 = depthU64 << 32 | vbID;
	return packedU64;
}

#define INVALID_VISIBILITY_DATA (~0U)

void unpackDepthVBId(uint64_t packedU64, out(float) depth, out(uint) vbID)
{
#if defined(PROSPERO)
	uint shifted = asuint(packedU64 >> 32);
#else
	uint shifted = uint(packedU64 >> 32);
#endif
	depth = asfloat(shifted);
	vbID = uint(packedU64);
}