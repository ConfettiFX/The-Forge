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

	inline ray ToMetal() { return ray(Origin, Direction, TMin, TMax); }
};

#define RaytracingAccelerationStructure instance_acceleration_structure
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

#define RAY_FLAG_NONE                            0x0
#define RAY_FLAG_FORCE_OPAQUE                    0x1
#define RAY_FLAG_FORCE_NON_OPAQUE                0x2
#define RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH 0x4
#define RAY_FLAG_SKIP_CLOSEST_HIT_SHADER         0x8
#define RAY_FLAG_CULL_BACK_FACING_TRIANGLES      0x10
#define RAY_FLAG_CULL_FRONT_FACING_TRIANGLES     0x20
#define RAY_FLAG_CULL_OPAQUE                     0x40
#define RAY_FLAG_CULL_NON_OPAQUE                 0x80
#define RAY_FLAG_SKIP_TRIANGLES                  0x100
#define RAY_FLAG_SKIP_AABB                       0x200

template<uint rayFlags>
inline intersection_params GetMetalIntersectionParams()
{
	intersection_params params;

	// Force opacity
	static_assert(!((rayFlags & RAY_FLAG_FORCE_OPAQUE) && (rayFlags & RAY_FLAG_FORCE_NON_OPAQUE)), "Forcing both Opaque and Non-Opaque not allowed");
	if((rayFlags & RAY_FLAG_FORCE_OPAQUE) != 0)
	{
		params.force_opacity(forced_opacity::opaque);
	}
	if((rayFlags & RAY_FLAG_FORCE_NON_OPAQUE) != 0)
	{
		params.force_opacity(forced_opacity::non_opaque);
	}

	// Accept any hit
	if((rayFlags & RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH) != 0)
	{
		params.accept_any_intersection(true);
	}

	// Cull opacity
	static_assert(!((rayFlags & RAY_FLAG_CULL_OPAQUE) && (rayFlags & RAY_FLAG_CULL_NON_OPAQUE)), "Culling both Opaque and Non-Opaque not allowed");
	if((rayFlags & RAY_FLAG_CULL_OPAQUE) != 0)
	{
		params.set_opacity_cull_mode(opacity_cull_mode::opaque);
		params.force_opacity(forced_opacity::non_opaque);
	}
	if((rayFlags & RAY_FLAG_CULL_NON_OPAQUE) != 0)
	{
		params.set_opacity_cull_mode(opacity_cull_mode::non_opaque);
		params.force_opacity(forced_opacity::opaque);
	}

	// Cull, Force geometry type
	static_assert(!((rayFlags & RAY_FLAG_SKIP_TRIANGLES) && (rayFlags & RAY_FLAG_SKIP_AABB)), "Skipping both Triangles and AABB not allowed");
	if((rayFlags & RAY_FLAG_SKIP_TRIANGLES) != 0)
	{
		params.set_geometry_cull_mode(geometry_cull_mode::triangle);
		params.assume_geometry_type(geometry_type::bounding_box);
	}
	if((rayFlags & RAY_FLAG_SKIP_AABB) != 0)
	{
		params.set_geometry_cull_mode(geometry_cull_mode::bounding_box);
		params.assume_geometry_type(geometry_type::triangle);
	}
	
	return params;
}

#define RayQueryClosestHit(tlas, traversalFlags, rayFlags, ray, mask, hit)        \
intersection_query<triangle_data, instancing> hit;                                \
hit.reset(ray.ToMetal(), tlas, mask, GetMetalIntersectionParams<(rayFlags)>());   \
bool hit##HasHitCandidates = hit.next();

#define RayQueryAnyHit(tlas, traversalFlags, rayFlags, ray, mask, hit)                                                       \
intersection_query<triangle_data, instancing> hit;                                                                           \
hit.reset(ray.ToMetal(), tlas, mask, GetMetalIntersectionParams<(rayFlags  | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH)>());  \
bool hit##HasHitCandidates = hit.next();

#define RayQueryBeginForEachCandidate(hit) while (hit##HasHitCandidates)
#define RayQueryEndForEachCandidate(hit)
#define RayQueryIsHit(hit) (hit.get_committed_intersection_type() != intersection_type::none)
#define RayQueryIsHitNonOpaqueTriangle(hit) (hit.get_candidate_intersection_type() == intersection_type::triangle)
#define RayQueryCommitCandidate(hit) hit.commit_triangle_intersection();
#define RayQueryProceed(hit) hit##HasHitCandidates = hit.next();

#define RayQueryBarycentrics(hit) (hit.get_committed_triangle_barycentric_coord())
#define RayQueryPrimitiveIndex(hit) (hit.get_committed_primitive_id())
#define RayQueryInstanceID(hit) (hit.get_committed_instance_id())
#define RayQueryGeometryIndex(hit) (hit.get_committed_geometry_id())

#define RayQueryCandidateBarycentrics(hit) (hit.get_candidate_triangle_barycentric_coord())
#define RayQueryCandidatePrimitiveIndex(hit) (hit.get_candidate_primitive_id())
#define RayQueryCandidateInstanceID(hit) (hit.get_candidate_instance_id())
#define RayQueryCandidateGeometryIndex(hit) (hit.get_candidate_geometry_id())

#endif
