/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#define _CFX_INTERSECT_HELPERS_CPP
#include "IntersectionHelpers.h"

AABB::AABB()
{
	minBounds = vec3(-0.001f, -0.001f, -0.001f);
	maxBounds = vec3(0.001f, 0.001f, 0.001f);
}

void AABB::Transform(mat4 const& mat)
{	
	minBounds = (mat * vec4(minBounds.getX(), minBounds.getY(), minBounds.getZ(), 1.0f)).getXYZ();
	maxBounds = (mat * vec4(maxBounds.getX(), maxBounds.getY(), maxBounds.getZ(), 1.0f)).getXYZ();
}


void Frustum::InitFrustumVerts(mat4 const& mvp)
{
	mat4 invMvp = inverse(mvp);

	nearTopLeftVert = invMvp * vec4(-1, 1, -1, 1);
	nearTopRightVert = invMvp * vec4(1, 1, -1, 1);
	nearBottomLeftVert = invMvp * vec4(-1, -1, -1, 1);
	nearBottomRightVert = invMvp * vec4(1, -1, -1, 1);
	farTopLeftVert = invMvp * vec4(-1, 1, 1, 1);
	farTopRightVert = invMvp * vec4(1, 1, 1, 1);
	farBottomLeftVert = invMvp * vec4(-1, -1, 1, 1);
	farBottomRightVert = invMvp * vec4(1, -1, 1, 1);

	nearTopLeftVert /= nearTopLeftVert.getW();
	nearTopRightVert /= nearTopRightVert.getW();
	nearBottomLeftVert /= nearBottomLeftVert.getW();
	nearBottomRightVert /= nearBottomRightVert.getW();
	farTopLeftVert /= farTopLeftVert.getW();
	farTopRightVert /= farTopRightVert.getW();
	farBottomLeftVert /= farBottomLeftVert.getW();
	farBottomRightVert /= farBottomRightVert.getW();

	nearTopLeftVert.setW(1.f);
	nearTopRightVert.setW(1.f);
	nearBottomLeftVert.setW(1.f);
	nearBottomRightVert.setW(1.f);
	farTopLeftVert.setW(1.f);
	farTopRightVert.setW(1.f);
	farBottomLeftVert.setW(1.f);
	farBottomRightVert.setW(1.f);
}

bool aabbInsideOrIntersectsFrustum(AABB const& aabb, const Frustum& frustum, bool const& fast)
{
	vec4 frus_planes[6] = {
		frustum.bottomPlane,
		frustum.topPlane,
		frustum.leftPlane,
		frustum.rightPlane,
		frustum.nearPlane,
		frustum.farPlane
	};
	vec4 frus_pnts[8] = {
		frustum.nearBottomLeftVert,
		frustum.nearBottomRightVert,
		frustum.nearTopLeftVert,
		frustum.nearTopRightVert,
		frustum.farBottomLeftVert,
		frustum.farBottomRightVert,
		frustum.farTopLeftVert,
		frustum.farTopRightVert,
	};


	// Fast check (aabb vs frustum)
	for( int i=0; i<6; i++ )
	{
		int out = 0;
		out += ((dot( frus_planes[i], vec4(aabb.minBounds.getX(), aabb.minBounds.getY(), aabb.minBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.maxBounds.getX(), aabb.minBounds.getY(), aabb.minBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.minBounds.getX(), aabb.maxBounds.getY(), aabb.minBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.maxBounds.getX(), aabb.maxBounds.getY(), aabb.minBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.minBounds.getX(), aabb.minBounds.getY(), aabb.maxBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.maxBounds.getX(), aabb.minBounds.getY(), aabb.maxBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.minBounds.getX(), aabb.maxBounds.getY(), aabb.maxBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);
		out += ((dot( frus_planes[i], vec4(aabb.maxBounds.getX(), aabb.maxBounds.getY(), aabb.maxBounds.getZ(), 1.0f) ) < 0.0 ) ? 1 : 0);

		if (out == 8) 
			return false;
	}
	
	// Slow check (frustum vs aabb)
	if (!fast)
	{
		int out;
		
		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getX() > aabb.maxBounds.getX()) ? 1 : 0); 
		if (out == 8) 
			return false;

		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getX() < aabb.minBounds.getX()) ? 1 : 0); 
		if (out == 8) 
			return false;

		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getY() > aabb.maxBounds.getY()) ? 1 : 0); 
		if (out == 8) 
			return false;

		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getY() < aabb.minBounds.getY()) ? 1 : 0); 
		if (out == 8) 
			return false;

		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getZ() > aabb.maxBounds.getZ()) ? 1 : 0); 
		if (out == 8) 
			return false;

		out = 0; 
		for( int i=0; i<8; i++ ) 
			out += ((frus_pnts[i].getZ() < aabb.minBounds.getZ()) ? 1 : 0); 
		if (out == 8) 
			return false;

	}

	return true;
}


