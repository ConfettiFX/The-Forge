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

#pragma once

#if FT_RAYTRACING

// Ray query interface

struct RayDesc
{
	float3 Origin;
	float3 Direction;
	float TMin;
	float TMax;
};

#define RaytracingAccelerationStructure accelerationStructureEXT
#define RayTraversalFlags uint

#define RAY_TRAVERSAL_FLAG_NONE                       0
#define RAY_TRAVERSAL_FLAG_NO_AABB_GEOMETRY           0
#define RAY_TRAVERSAL_FLAG_NO_AABB_INSTANCE           0
#define RAY_TRAVERSAL_FLAG_USE_SHARED_STACK           0
#define RAY_TRAVERSAL_FLAG_USE_SHARED_STACK_TOP_LEVEL 0
#define RAY_TRAVERSAL_FLAG_IGNORE_INSTANCE_MASKING    0
#define RAY_TRAVERSAL_FLAG_IGNORE_INSTANCE_CULLING    0
#define RAY_TRAVERSAL_FLAG_IGNORE_INSTANCE_OPACITY    0
#define RAY_TRAVERSAL_FLAG_IGNORE_INSTANCE_FLAGS      0
#define RAY_TRAVERSAL_FLAG_AUTO_LDS_SIZE              0
#define RAY_TRAVERSAL_FLAG_DEFAULT                    0

#define RAY_FLAG_NONE                            gl_RayFlagsNoneEXT
#define RAY_FLAG_FORCE_OPAQUE                    gl_RayFlagsOpaqueEXT
#define RAY_FLAG_FORCE_NON_OPAQUE                gl_RayFlagsNoOpaqueEXT
#define RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH gl_RayFlagsTerminateOnFirstHitEXT
#define RAY_FLAG_SKIP_CLOSEST_HIT_SHADER         gl_RayFlagsSkipClosestHitShaderEXT
#define RAY_FLAG_CULL_BACK_FACING_TRIANGLES      gl_RayFlagsCullBackFacingTrianglesEXT
#define RAY_FLAG_CULL_FRONT_FACING_TRIANGLES     gl_RayFlagsCullFrontFacingTrianglesEXT
#define RAY_FLAG_CULL_OPAQUE                     gl_RayFlagsCullOpaqueEXT
#define RAY_FLAG_CULL_NON_OPAQUE                 gl_RayFlagsCullNoOpaqueEXT
#define RAY_FLAG_SKIP_TRIANGLES                  gl_RayFlagsSkipTrianglesEXT
#define RAY_FLAG_SKIP_AABB                       gl_RayFlagsSkipAABBEXT

#define RayQueryClosestHit(tlas, traversalFlags, rayFlags, ray, mask, hit)                       \
rayQueryEXT hit;                                                                                 \
rayQueryInitializeEXT(hit, tlas, rayFlags, mask, ray.Origin, ray.TMin, ray.Direction, ray.TMax); \
bool hit##HasHitCandidates = rayQueryProceedEXT(hit);

#define RayQueryAnyHit(tlas, traversalFlags, rayFlags, ray, mask, hit)                                                                      \
rayQueryEXT hit;                                                                                                                            \
rayQueryInitializeEXT(hit, tlas, rayFlags | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, mask, ray.Origin, ray.TMin, ray.Direction, ray.TMax); \
bool hit##HasHitCandidates = rayQueryProceedEXT(hit);

#define RayQueryBeginForEachCandidate(hit) while (hit##HasHitCandidates)
#define RayQueryEndForEachCandidate(hit)
#define RayQueryIsHit(hit) (rayQueryGetIntersectionTypeEXT(hit, true) != gl_RayQueryCommittedIntersectionNoneEXT)
#define RayQueryIsHitNonOpaqueTriangle(hit) (rayQueryGetIntersectionTypeEXT(hit, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
#define RayQueryCommitCandidate(hit) rayQueryConfirmIntersectionEXT(hit);
#define RayQueryProceed(hit) hit##HasHitCandidates = rayQueryProceedEXT(hit);

#define RayQueryBarycentrics(hit) (rayQueryGetIntersectionBarycentricsEXT(hit, true))
#define RayQueryPrimitiveIndex(hit) (rayQueryGetIntersectionPrimitiveIndexEXT(hit, true))
#define RayQueryInstanceID(hit) (rayQueryGetIntersectionInstanceIdEXT(hit, true))
#define RayQueryGeometryIndex(hit) (rayQueryGetIntersectionGeometryIndexEXT(hit, true))

#define RayQueryCandidateBarycentrics(hit) (rayQueryGetIntersectionBarycentricsEXT(hit, false))
#define RayQueryCandidatePrimitiveIndex(hit) (rayQueryGetIntersectionPrimitiveIndexEXT(hit, false))
#define RayQueryCandidateInstanceID(hit) (rayQueryGetIntersectionInstanceIdEXT(hit, false))
#define RayQueryCandidateGeometryIndex(hit) (rayQueryGetIntersectionGeometryIndexEXT(hit, false))

#endif

#if FT_MULTIVIEW

#if defined(STAGE_VERT)
    #define VR_VIEW_ID (gl_ViewID_OVR)
#else
    #define VR_VIEW_ID(VID) VID
#endif
    #define VR_MULTIVIEW_COUNT 2

#else

    #if defined(STAGE_VERT)
        #define VR_VIEW_ID 0
    #else
        #define VR_VIEW_ID(VID) (0)
    #endif

    #define VR_MULTIVIEW_COUNT 1

#endif