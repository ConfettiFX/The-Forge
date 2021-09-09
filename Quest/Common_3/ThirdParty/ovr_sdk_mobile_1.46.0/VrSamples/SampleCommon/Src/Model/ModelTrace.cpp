/************************************************************************************

Filename    :   ModelTrace.cpp
Content     :   Ray tracer using a KD-Tree.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "ModelTrace.h"

#include <math.h>
#include <algorithm>
#include <vector>
#include <assert.h>

#include "Misc/Log.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

/*
    An Efficient and Robust RayBox Intersection Algorithm
    Amy Williams, Steve Barrus, R. Keith Morley, Peter Shirley
    Journal of Graphics Tools, Issue 10, Pages 49-54, June 2005

    Returns true if the ray intersects the bounds.
    't0' and 't1' are the distances along the ray where the intersections occurs.
    1st intersection = rayStart + t0 * rayDir
    2nd intersection = rayStart + t1 * rayDir
*/
bool Intersect_RayBounds(
    const Vector3f& rayStart,
    const Vector3f& rayDir,
    const Vector3f& mins,
    const Vector3f& maxs,
    float& t0,
    float& t1) {
    const float rcpDirX = (fabsf(rayDir.x) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.x)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpDirY = (fabsf(rayDir.y) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.y)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpDirZ = (fabsf(rayDir.z) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.z)
        : MATH_FLOAT_HUGE_NUMBER;

    const float sX = (mins.x - rayStart.x) * rcpDirX;
    const float sY = (mins.y - rayStart.y) * rcpDirY;
    const float sZ = (mins.z - rayStart.z) * rcpDirZ;

    const float tX = (maxs.x - rayStart.x) * rcpDirX;
    const float tY = (maxs.y - rayStart.y) * rcpDirY;
    const float tZ = (maxs.z - rayStart.z) * rcpDirZ;

    const float minX = std::min(sX, tX);
    const float minY = std::min(sY, tY);
    const float minZ = std::min(sZ, tZ);

    const float maxX = std::max(sX, tX);
    const float maxY = std::max(sY, tY);
    const float maxZ = std::max(sZ, tZ);

    t0 = std::max(minX, std::max(minY, minZ));
    t1 = std::min(maxX, std::min(maxY, maxZ));

    return (t0 <= t1);
}

/*
    Fast, Minimum Storage Ray/Triangle Intersection
    Tomas Möller, Ben Trumbore
    Journal of Graphics Tools, 1997

    Triangles are back-face culled.
    Returns true if the ray intersects the triangle.
    't0' is the distance along the ray where the intersection occurs.
    intersection = rayStart + t0 * rayDir;
    'u' and 'v' are the barycentric coordinates.
    intersection = ( 1 - u - v ) * v0 + u * v1 + v * v2
*/
bool Intersect_RayTriangle(
    const Vector3f& rayStart,
    const Vector3f& rayDir,
    const Vector3f& v0,
    const Vector3f& v1,
    const Vector3f& v2,
    float& t0,
    float& u,
    float& v) {
    assert(rayDir.IsNormalized());

    const Vector3f edge1 = v1 - v0;
    const Vector3f edge2 = v2 - v0;

    const Vector3f tv = rayStart - v0;
    const Vector3f pv = rayDir.Cross(edge2);
    const Vector3f qv = tv.Cross(edge1);
    const float det = edge1.Dot(pv);

    // If the determinant is negative then the triangle is backfacing.
    if (det <= 0.0f) {
        return false;
    }

    // This code has been modified to only perform a floating-point
    // division if the ray actually hits the triangle. If back facing
    // triangles are not culled then the sign of 's' and 't' need to
    // be flipped. This can be accomplished by multiplying the values
    // with the determinant instead of the reciprocal determinant.

    const float s = tv.Dot(pv);
    const float t = rayDir.Dot(qv);

    if (s >= 0.0f && s <= det) {
        if (t >= 0.0f && s + t <= det) {
            // If the determinant is almost zero then the ray lies in the triangle plane.
            // This comparison is done last because it is usually rare for
            // the ray to lay in the triangle plane.
            if (fabsf(det) > MATH_FLOAT_SMALLEST_NON_DENORMAL) {
                const float rcpDet = 1.0f / det;
                t0 = edge2.Dot(qv) * rcpDet;
                u = s * rcpDet;
                v = t * rcpDet;
                return true;
            }
        }
    }

    return false;
}

/*

    Stackless KD-Tree Traversal for High Performance GPU Ray Tracing
    Stefan Popov, Johannes Günther, Hans-Peter Seidel, Philipp Slusallek
    Eurographics, Volume 26, Number 3, 2007

*/

const int RT_KDTREE_MAX_ITERATIONS = 128;

bool ModelTrace::Validate(const bool fullVerify) const {
    bool invalid = false;

    invalid |= header.numVertices != static_cast<int>(vertices.size());
    invalid |= header.numUvs != static_cast<int>(uvs.size());
    invalid |= header.numIndices != static_cast<int>(indices.size());
    invalid |= header.numNodes != static_cast<int>(nodes.size());
    invalid |= header.numLeafs != static_cast<int>(leafs.size());
    invalid |= header.numOverflow != static_cast<int>(overflow.size());
    if (!invalid) {
        ALOG("ModelTrace::Verify - invalid header");
        return false;
    }
    // the number of uvs must be either equal to the number of vertices, or 0
    if (static_cast<int>(uvs.size()) != 0 &&
        static_cast<int>(uvs.size()) != static_cast<int>(vertices.size())) {
        ALOG("ModelTrace::Verify - model must have no uvs, or the same number of uvs as vertices");
        return false;
    }
    if (fullVerify) {
        // verify that all child indices are valid in each node
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            const kdtree_node_t& node = nodes[i];
            const bool isLeaf = (node.data & 1) != 0;
            if (isLeaf) {
                // leaves have no children to verify
                continue;
            }
            int const leftChildIndex = node.data >> 3;
            int const rightChildIndex = leftChildIndex + 1;
            if (leftChildIndex < 0 || leftChildIndex >= static_cast<int>(nodes.size())) {
                ALOG(
                    "ModelTrace::Verify - leftChildIndex of %i for node %i is out of range, max %i",
                    leftChildIndex,
                    i,
                    static_cast<int>(nodes.size()) - 1);
                return false;
            }
            if (rightChildIndex < 0 || rightChildIndex >= static_cast<int>(nodes.size())) {
                ALOG(
                    "ModelTrace::Verify - rightChildIndex of %i for node %i is out of range, max %i",
                    leftChildIndex,
                    i,
                    static_cast<int>(nodes.size()) - 1);
                return false;
            }
        }
        const int numTris = static_cast<int>(indices.size()) / 3;
        if (numTris * 3 != static_cast<int>(indices.size()) * 3) {
            ALOG("ModelTrace::Verify - Orphaned indices");
            return false;
        }
        // verify leaves don't point to any out-of-range triangles
        for (int i = 0; i < static_cast<int>(leafs.size()); ++i) {
            const kdtree_leaf_t& leaf = leafs[i];
            for (int j = 0; j < RT_KDTREE_MAX_LEAF_TRIANGLES; ++j) {
                // if the triangle index is < 1 this is either the end of
                // the triangle list, or an index into the overflow list
                const int triIndex = leaf.triangles[j];
                if (triIndex < 0) {
                    if (triIndex == -1) {
                        break; // no more triangles
                    }
                    // this is an index into the overflow -- verify it is in range
                    const int overflowIndex = triIndex & 0x7FFFFFFF;
                    if (overflowIndex < 0 || overflowIndex >= static_cast<int>(overflow.size())) {
                        ALOG(
                            "ModelTrace::Verify - Leaf %i has an out of range overflow index %i at index %i, max %i",
                            i,
                            overflowIndex,
                            j,
                            numTris - 1);
                        return false;
                    }
                    // we don't verify the overflow indices are in range of the triangle list here
                    // here because we'll do that explicity below to make sure we hit even overflow
                    // indices that aren't referenced by a leaf
                } else if (triIndex >= numTris) {
                    ALOG(
                        "ModelTrace::Verify - Leaf %i has an out of range triangle of index %i at index %i, max %i",
                        i,
                        triIndex,
                        j,
                        numTris - 1);
                    return false;
                }
            }
        }
        // verify overflow list doesn't point to any out-of-range triangles
        for (int i = 0; i < static_cast<int>(overflow.size()); ++i) {
            if (overflow[i] < 0 || overflow[i] >= numTris) {
                ALOG(
                    "ModelTrace::Verify - overflow index %i value %i is out of range, max %i",
                    i,
                    overflow[i],
                    numTris - 1);
                return false;
            }
        }
        // verify indices do not point to any out-of-range vertices
        for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
            if (indices[i] < 0 || indices[i] >= static_cast<int>(vertices.size())) {
                ALOG(
                    "Index %i value %i is out of range, max %i",
                    i,
                    indices[i],
                    static_cast<int>(vertices.size()) - 1);
                return false;
            }
        }
    }
    return true;
}

traceResult_t ModelTrace::Trace(const Vector3f& start, const Vector3f& end) const {
    // in debug, at least warn programmers if they're loading a model
    // that fails simple validation.
    assert(Validate(false));

    traceResult_t result;
    result.triangleIndex = -1;
    result.fraction = 1.0f;
    result.uv = Vector2f(0.0f);
    result.normal = Vector3f(0.0f);

    const Vector3f rayDelta = end - start;
    const float rayLengthSqr = rayDelta.LengthSq();
    const float rayLengthRcp = OVR::RcpSqrt(rayLengthSqr);
    const float rayLength = rayLengthSqr * rayLengthRcp;
    const Vector3f rayDir = rayDelta * rayLengthRcp;

    const float rcpRayDirX = (fabsf(rayDir.x) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.x)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpRayDirY = (fabsf(rayDir.y) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.y)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpRayDirZ = (fabsf(rayDir.z) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.z)
        : MATH_FLOAT_HUGE_NUMBER;

    const float sX = (header.bounds.GetMins()[0] - start.x) * rcpRayDirX;
    const float sY = (header.bounds.GetMins()[1] - start.y) * rcpRayDirY;
    const float sZ = (header.bounds.GetMins()[2] - start.z) * rcpRayDirZ;

    const float tX = (header.bounds.GetMaxs()[0] - start.x) * rcpRayDirX;
    const float tY = (header.bounds.GetMaxs()[1] - start.y) * rcpRayDirY;
    const float tZ = (header.bounds.GetMaxs()[2] - start.z) * rcpRayDirZ;

    const float minX = std::min(sX, tX);
    const float minY = std::min(sY, tY);
    const float minZ = std::min(sZ, tZ);

    const float maxX = std::max(sX, tX);
    const float maxY = std::max(sY, tY);
    const float maxZ = std::max(sZ, tZ);

    const float t0 = std::max(minX, std::max(minY, minZ));
    const float t1 = std::min(maxX, std::min(maxY, maxZ));

    if (t0 >= t1) {
        return result;
    }

    float entryDistance = std::max(t0, 0.0f);
    float bestDistance = std::min(t1 + 0.00001f, rayLength);
    Vector2f uv;

    const kdtree_node_t* currentNode = &nodes[0];

    for (int i = 0; i < RT_KDTREE_MAX_ITERATIONS; i++) {
        const Vector3f rayEntryPoint = start + rayDir * entryDistance;

        // Step down the tree until a leaf node is found.
        while ((currentNode->data & 1) == 0) {
            // Select the child node based on whether the entry point is left or right of the split
            // plane. If the entry point is directly at the split plane then choose the side based
            // on the ray direction.
            const int nodePlane = ((currentNode->data >> 1) & 3);
            int child;
            if (rayEntryPoint[nodePlane] - currentNode->dist < 0.00001f)
                child = 0;
            else if (rayEntryPoint[nodePlane] - currentNode->dist > 0.00001f)
                child = 1;
            else
                child = (rayDelta[nodePlane] > 0.0f);
            currentNode = &nodes[(currentNode->data >> 3) + child];
        }

        // Check for an intersection with a triangle in this leaf.
        const kdtree_leaf_t* currentLeaf = &leafs[(currentNode->data >> 3)];
        const int* leafTriangles = currentLeaf->triangles;
        int leafTriangleCount = RT_KDTREE_MAX_LEAF_TRIANGLES;
        for (int j = 0; j < leafTriangleCount; j++) {
            int currentTriangle = leafTriangles[j];
            if (currentTriangle < 0) {
                if (currentTriangle == -1) {
                    break;
                }

                const int offset = (currentTriangle & 0x7FFFFFFF);
                leafTriangles = &overflow[offset];
                leafTriangleCount = header.numOverflow - offset;
                j = 0;
                currentTriangle = leafTriangles[0];
            }

            float distance;
            float u;
            float v;

            if (Intersect_RayTriangle(
                    start,
                    rayDir,
                    vertices[indices[currentTriangle * 3 + 0]],
                    vertices[indices[currentTriangle * 3 + 1]],
                    vertices[indices[currentTriangle * 3 + 2]],
                    distance,
                    u,
                    v)) {
                if (distance >= 0.0f && distance < bestDistance) {
                    bestDistance = distance;

                    result.triangleIndex = currentTriangle * 3;
                    uv.x = u;
                    uv.y = v;
                }
            }
        }

        // Calculate the distance along the ray where the next leaf is entered.
        const float sXX = (currentLeaf->bounds.GetMins()[0] - start.x) * rcpRayDirX;
        const float sYY = (currentLeaf->bounds.GetMins()[1] - start.y) * rcpRayDirY;
        const float sZZ = (currentLeaf->bounds.GetMins()[2] - start.z) * rcpRayDirZ;

        const float tXX = (currentLeaf->bounds.GetMaxs()[0] - start.x) * rcpRayDirX;
        const float tYY = (currentLeaf->bounds.GetMaxs()[1] - start.y) * rcpRayDirY;
        const float tZZ = (currentLeaf->bounds.GetMaxs()[2] - start.z) * rcpRayDirZ;

        const float maxXX = std::max(sXX, tXX);
        const float maxYY = std::max(sYY, tYY);
        const float maxZZ = std::max(sZZ, tZZ);

        entryDistance = std::min(maxXX, std::min(maxYY, maxZZ));
        if (entryDistance >= bestDistance) {
            break;
        }

        // Calculate the exit plane.
        const int exitX = (0 << 1) | ((sXX < tXX) ? 1 : 0);
        const int exitY = (1 << 1) | ((sYY < tYY) ? 1 : 0);
        const int exitZ = (2 << 1) | ((sZZ < tZZ) ? 1 : 0);
        const int exitPlane =
            (maxXX < maxYY) ? (maxXX < maxZZ ? exitX : exitZ) : (maxYY < maxZZ ? exitY : exitZ);

        // Use a rope to enter the adjacent leaf.
        const int exitNodeIndex = currentLeaf->ropes[exitPlane];
        if (exitNodeIndex == -1) {
            break;
        }

        currentNode = &nodes[exitNodeIndex];
    }

    if (result.triangleIndex != -1) {
        result.fraction = bestDistance * rayLengthRcp;
        // return default uvs if the model has no uvs
        if (static_cast<int>(uvs.size()) == 0) {
            result.uv = Vector2f(0.0f, 0.0f);
        } else {
            result.uv = uvs[indices[result.triangleIndex + 0]] * (1.0f - uv.x - uv.y) +
                uvs[indices[result.triangleIndex + 1]] * uv.x +
                uvs[indices[result.triangleIndex + 2]] * uv.y;
        }
        const Vector3f d1 = vertices[indices[result.triangleIndex + 1]] -
            vertices[indices[result.triangleIndex + 0]];
        const Vector3f d2 = vertices[indices[result.triangleIndex + 2]] -
            vertices[indices[result.triangleIndex + 0]];
        result.normal = d1.Cross(d2).Normalized();
    }

    return result;
}

traceResult_t ModelTrace::Trace_Exhaustive(const Vector3f& start, const Vector3f& end) const {
    // in debug, at least warn programmers if they're loading a model
    // that fails simple validation.
    assert(Validate(false));

    traceResult_t result;
    result.triangleIndex = -1;
    result.fraction = 1.0f;
    result.uv = Vector2f(0.0f);
    result.normal = Vector3f(0.0f);

    const Vector3f rayDelta = end - start;
    const float rayLengthSqr = rayDelta.LengthSq();
    const float rayLengthRcp = OVR::RcpSqrt(rayLengthSqr);
    const float rayLength = rayLengthSqr * rayLengthRcp;
    const Vector3f rayStart = start;
    const Vector3f rayDir = rayDelta * rayLengthRcp;

    float bestDistance = rayLength;
    Vector2f uv;

    for (int i = 0; i < header.numIndices; i += 3) {
        float distance;
        float u;
        float v;

        if (Intersect_RayTriangle(
                rayStart,
                rayDir,
                vertices[indices[i + 0]],
                vertices[indices[i + 1]],
                vertices[indices[i + 2]],
                distance,
                u,
                v)) {
            if (distance >= 0.0f && distance < bestDistance) {
                bestDistance = distance;

                result.triangleIndex = i;
                uv.x = u;
                uv.y = v;
            }
        }
    }

    if (result.triangleIndex != -1) {
        result.fraction = bestDistance * rayLengthRcp;
        // return default uvs if the model has no uvs
        if (static_cast<int>(uvs.size()) == 0) {
            result.uv = Vector2f(0.0f, 0.0f);
        } else {
            result.uv = uvs[indices[result.triangleIndex + 0]] * (1.0f - uv.x - uv.y) +
                uvs[indices[result.triangleIndex + 1]] * uv.x +
                uvs[indices[result.triangleIndex + 2]] * uv.y;
        }
        const Vector3f d1 = vertices[indices[result.triangleIndex + 1]] -
            vertices[indices[result.triangleIndex + 0]];
        const Vector3f d2 = vertices[indices[result.triangleIndex + 2]] -
            vertices[indices[result.triangleIndex + 0]];
        result.normal = d1.Cross(d2).Normalized();
    }

    return result;
}

void ModelTrace::PrintStatsToLog() const {
    ALOG("ModelTrace Stats:");
    ALOG("  Vertices: %i", static_cast<int>(vertices.size()));
    ALOG("  UVs     : %i", static_cast<int>(uvs.size()));
    ALOG("  Indices : %i", static_cast<int>(indices.size()));
    ALOG("  Nodes   : %i", static_cast<int>(nodes.size()));
    ALOG("  Leaves  : %i", static_cast<int>(leafs.size()));
    ALOG("  Overflow: %i", static_cast<int>(overflow.size()));
}

} // namespace OVRFW
