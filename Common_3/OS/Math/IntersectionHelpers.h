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

#ifndef _CFX_INTERSECT_HELPERS_
#define _CFX_INTERSECT_HELPERS_

#include "mat4.h"
#include "mat3.h"

// Bounding box 
struct AABB
{
	AABB();

	void Transform(mat4 const& mat);

	vec3 minBounds, maxBounds;
};

// Frustum
struct Frustum
{
	vec4 nearPlane, farPlane, topPlane, bottomPlane, leftPlane, rightPlane;
	vec4 nearTopLeftVert, nearTopRightVert, nearBottomLeftVert, nearBottomRightVert;
	vec4 farTopLeftVert, farTopRightVert, farBottomLeftVert, farBottomRightVert;

	// Helper func
	void InitFrustumVerts(mat4 const& mvp);
};

// Frustum to AABB intersection
// false if aabb is completely outside frustum, true otherwise
// Based on Íñigo Quílez' "Correct Frustum Culling" article
// http://www.iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
// If fast is true, function will do extra frustum-in-box checks using the frustum's corner vertices.
bool aabbInsideOrIntersectsFrustum(AABB const& aabb, const Frustum& frustum, bool const& fast = false);

#endif