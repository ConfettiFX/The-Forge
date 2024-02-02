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

#if FT_RAYTRACING && !defined(TARGET_SCARLETT)

// Ray query interface

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

#define RAY_FLAG_SKIP_AABB RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES

#define RayQueryClosestHit(tlas, traversalFlags, rayFlags, ray, mask, hit)   \
RayQuery<rayFlags> hit;                                                      \
hit.TraceRayInline( tlas, 0, mask, ray);                                     \
bool hit##HasHitCandidates = hit.Proceed();                                    

#define RayQueryAnyHit(tlas, traversalFlags, rayFlags, ray, mask, hit)       \
RayQuery<rayFlags | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> hit;           \
hit.TraceRayInline( tlas, 0, mask, ray);                                     \
bool hit##HasHitCandidates = hit.Proceed();                                    

#define RayQueryBeginForEachCandidate(hit) while (hit##HasHitCandidates)
#define RayQueryEndForEachCandidate(hit)
#define RayQueryIsHit(hit) (hit.CommittedStatus() != COMMITTED_NOTHING)
#define RayQueryIsHitNonOpaqueTriangle(hit) (hit.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
#define RayQueryCommitCandidate(hit) hit.CommitNonOpaqueTriangleHit();
#define RayQueryProceed(hit) hit##HasHitCandidates = hit.Proceed();

#define RayQueryBarycentrics(hit) (hit.CommittedTriangleBarycentrics())
#define RayQueryPrimitiveIndex(hit) (hit.CommittedPrimitiveIndex())
#define RayQueryInstanceID(hit) (hit.CommittedInstanceID())
#define RayQueryGeometryIndex(hit) (hit.CommittedGeometryIndex())

#define RayQueryCandidateBarycentrics(hit) (hit.CandidateTriangleBarycentrics())
#define RayQueryCandidatePrimitiveIndex(hit) (hit.CandidatePrimitiveIndex())
#define RayQueryCandidateInstanceID(hit) (hit.CandidateInstanceID())
#define RayQueryCandidateGeometryIndex(hit) (hit.CandidateGeometryIndex())

#endif
