/************************************************************************************

Filename    :   CollisionPrimitive.h
Content     :   Generic collision class supporting ray / triangle intersection.
Created     :   September 10, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#pragma once

#include <vector>
#include <cstdlib>

#include "OVR_BitFlags.h"

#include "Render/GlGeometry.h" // For TriangleIndex

namespace OVRFW {

class OvrDebugLines;

enum eContentFlags { CONTENT_NONE = 0, CONTENT_SOLID, CONTENT_ALL = 0x7fffffff };

typedef OVR::BitFlagsT<eContentFlags> ContentFlags_t;

//==============================================================
// OvrCollisionResult
// Structure that holds the result of a collision
class OvrCollisionResult {
   public:
    OvrCollisionResult() : t(FLT_MAX), uv(0.0f), TriIndex(-1) {}

    float t; // fraction along line where the intersection occurred
    OVR::Vector2f uv; // texture coordinate of intersection
    std::int64_t TriIndex; // index of triangle hit (local to collider)
    OVR::Vector2f Barycentric; // barycentric coordinates intersection
};

//==============================================================
// OvrCollisionPrimitive
// Base class for a collision primitive.
class OvrCollisionPrimitive {
   public:
    OvrCollisionPrimitive() {}
    OvrCollisionPrimitive(ContentFlags_t const contents) : Contents(contents) {}
    virtual ~OvrCollisionPrimitive();

    virtual bool IntersectRay(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Posef const& pose,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const = 0;
    // the ray should already be in local space
    virtual bool IntersectRay(
        OVR::Vector3f const& localStart,
        OVR::Vector3f const& localDir,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const = 0;

    // test for ray intersection against only the AAB
    bool IntersectRayBounds(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        float& t0,
        float& t1) const;

    virtual void DebugRender(
        OvrDebugLines& debugLines,
        OVR::Posef& pose,
        OVR::Vector3f const& scale,
        bool const showNormals) const = 0;

    ContentFlags_t GetContents() const {
        return Contents;
    }
    void SetContents(ContentFlags_t const contents) {
        Contents = contents;
    }

    OVR::Bounds3f const& GetBounds() const {
        return Bounds;
    }
    void SetBounds(OVR::Bounds3f const& bounds) {
        Bounds = bounds;
    }

   protected:
    bool IntersectRayBounds(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Vector3f const& scale,
        float& t0,
        float& t1) const;

   private:
    ContentFlags_t Contents; // flags dictating what can hit this primitive
    OVR::Bounds3f Bounds; // Axial-aligned bounds of the primitive
};

//==============================================================
// OvrTriCollisionPrimitive
// Collider that handles collision vs. polygons and stores those polygons itself.
class OvrTriCollisionPrimitive : public OvrCollisionPrimitive {
   public:
    OvrTriCollisionPrimitive();
    OvrTriCollisionPrimitive(
        std::vector<OVR::Vector3f> const& vertices,
        std::vector<TriangleIndex> const& indices,
        std::vector<OVR::Vector2f> const& uvs,
        ContentFlags_t const contents);

    virtual ~OvrTriCollisionPrimitive();

    void Init(
        std::vector<OVR::Vector3f> const& vertices,
        std::vector<TriangleIndex> const& indices,
        std::vector<OVR::Vector2f> const& uvs,
        ContentFlags_t const contents);

    virtual bool IntersectRay(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Posef const& pose,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const override;

    // the ray should already be in local space
    virtual bool IntersectRay(
        OVR::Vector3f const& localStart,
        OVR::Vector3f const& localDir,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const override;

    virtual void DebugRender(
        OvrDebugLines& debugLines,
        OVR::Posef& pose,
        OVR::Vector3f const& scale,
        bool const showNormals) const override;

   private:
    std::vector<OVR::Vector3f> Vertices; // vertices for all triangles
    std::vector<TriangleIndex> Indices; // indices indicating which vertices make up each triangle
    std::vector<OVR::Vector2f> UVs; // uvs for each vertex
};

} // namespace OVRFW
