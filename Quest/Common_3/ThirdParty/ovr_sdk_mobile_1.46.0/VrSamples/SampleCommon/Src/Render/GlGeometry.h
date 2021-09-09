/************************************************************************************

Filename    :   GlGeometry.h
Content     :   OpenGL geometry setup.
Created     :   October 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include "OVR_Math.h"

#include "GlProgram.h"

namespace OVRFW {

struct VertexAttribs {
    std::vector<OVR::Vector3f> position;
    std::vector<OVR::Vector3f> normal;
    std::vector<OVR::Vector3f> tangent;
    std::vector<OVR::Vector3f> binormal;
    std::vector<OVR::Vector4f> color;
    std::vector<OVR::Vector2f> uv0;
    std::vector<OVR::Vector2f> uv1;
    std::vector<OVR::Vector4i> jointIndices;
    std::vector<OVR::Vector4f> jointWeights;
};

typedef uint16_t TriangleIndex;

class GlGeometry {
   public:
    GlGeometry()
        : vertexBuffer(0),
          indexBuffer(0),
          vertexArrayObject(0),
          primitiveType(0x0004 /* GL_TRIANGLES */),
          vertexCount(0),
          indexCount(0),
          localBounds(OVR::Bounds3f::Init) {}

    GlGeometry(const VertexAttribs& attribs, const std::vector<TriangleIndex>& indices)
        : vertexBuffer(0),
          indexBuffer(0),
          vertexArrayObject(0),
          primitiveType(0x0004 /* GL_TRIANGLES */),
          vertexCount(0),
          indexCount(0),
          localBounds(OVR::Bounds3f::Init) {
        Create(attribs, indices);
    }

    // Create the VAO and vertex and index buffers from arrays of data.
    void Create(const VertexAttribs& attribs, const std::vector<TriangleIndex>& indices);
    void Update(const VertexAttribs& attribs, const bool updateBounds = true);

    // Free the buffers and VAO, assuming that they are strictly for this geometry.
    // We could save some overhead by packing an entire model into a single buffer, but
    // it would add more coupling to the structures.
    // This is not in the destructor to allow objects of this class to be passed by value.
    void Free();

   public:
    static constexpr int32_t MAX_GEOMETRY_VERTICES = 1 << (sizeof(TriangleIndex) * 8);
    static constexpr int32_t MAX_GEOMETRY_INDICES = 1024 * 1024 * 3;

    static constexpr inline int32_t GetMaxGeometryVertices() {
        return MAX_GEOMETRY_VERTICES;
    }
    static constexpr inline int32_t GetMaxGeometryIndices() {
        return MAX_GEOMETRY_INDICES;
    }

    static unsigned IndexType; // GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, etc.

    class TransformScope {
       public:
        TransformScope(const OVR::Matrix4f m, bool enableTransfom = true);
        ~TransformScope();

       private:
        OVR::Matrix4f previousTransform;
        bool wasEnabled;
    };

    struct Descriptor {
        Descriptor(
            const VertexAttribs& a,
            const std::vector<TriangleIndex>& i,
            const OVR::Matrix4f& t)
            : attribs(a), indices(i), transform(t) {}
        Descriptor() : transform(OVR::Matrix4f::Identity()) {}

        VertexAttribs attribs;
        std::vector<TriangleIndex> indices;
        OVR::Matrix4f transform;
    };

   public:
    unsigned vertexBuffer;
    unsigned indexBuffer;
    unsigned vertexArrayObject;
    unsigned primitiveType; // GL_TRIANGLES / GL_LINES / GL_POINTS / etc
    int vertexCount;
    int indexCount;
    OVR::Bounds3f localBounds;
};

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
//
// A horizontal and vertical value of 1 will give a single quad.
//
// Texcoords range from 0 to 1.
//
// Color is 1, fades alpha to 0 along the outer edge.
GlGeometry::Descriptor BuildTesselatedQuadDescriptor(
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    bool twoSided = false);

inline GlGeometry BuildTesselatedQuad(
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    bool twoSided = false) {
    const GlGeometry::Descriptor d = BuildTesselatedQuadDescriptor(horizontal, vertical, twoSided);
    return GlGeometry(d.attribs, d.indices);
}

// 8 quads making a thin border inside the -1 tp 1 square.
// The fractions are the total fraction that will be faded,
// half on one side, half on the other.
GlGeometry::Descriptor BuildVignetteDescriptor(const float xFraction, const float yFraction);

inline GlGeometry BuildVignette(const float xFraction, const float yFraction) {
    const GlGeometry::Descriptor d = BuildVignetteDescriptor(xFraction, yFraction);
    return GlGeometry(d.attribs, d.indices);
}

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
// Fades alpha to 0 along the outer edge.
GlGeometry::Descriptor BuildTesselatedCylinderDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale);
inline GlGeometry BuildTesselatedCylinder(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale) {
    const GlGeometry::Descriptor d =
        BuildTesselatedCylinderDescriptor(radius, height, horizontal, vertical, uScale, vScale);
    return GlGeometry(d.attribs, d.indices);
}

// Build it in a -1 to 1 range, which will be scaled to the appropriate
// aspect ratio for each usage.
// Fades alpha to 0 along the outer edge.
GlGeometry::Descriptor BuildTesselatedConeDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale);
inline GlGeometry BuildTesselatedCone(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale) {
    const GlGeometry::Descriptor d =
        BuildTesselatedConeDescriptor(radius, height, horizontal, vertical, uScale, vScale);
    return GlGeometry(d.attribs, d.indices);
}

GlGeometry::Descriptor BuildTesselatedCapsuleDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical);
inline GlGeometry BuildTesselatedCapsule(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical) {
    const GlGeometry::Descriptor d =
        BuildTesselatedCapsuleDescriptor(radius, height, horizontal, vertical);
    return GlGeometry(d.attribs, d.indices);
}

GlGeometry::Descriptor
BuildDomeDescriptor(const float latRads, const float uScale = 1.0f, const float vScale = 1.0f);
inline GlGeometry
BuildDome(const float latRads, const float uScale = 1.0f, const float vScale = 1.0f) {
    const GlGeometry::Descriptor d = BuildDomeDescriptor(latRads, uScale, vScale);
    return GlGeometry(d.attribs, d.indices);
}

GlGeometry::Descriptor BuildGlobeDescriptor(
    const float uScale = 1.0f,
    const float vScale = 1.0f,
    const float radius = 100.0f);
inline GlGeometry
BuildGlobe(const float uScale = 1.0f, const float vScale = 1.0f, const float radius = 100.0f) {
    const GlGeometry::Descriptor d = BuildGlobeDescriptor(uScale, vScale, radius);
    return GlGeometry(d.attribs, d.indices);
}

// Make a square patch on a sphere that can rotate with the viewer
// so it always covers the screen.
GlGeometry::Descriptor BuildSpherePatchDescriptor(const float fov);
inline GlGeometry BuildSpherePatch(const float fov) {
    const GlGeometry::Descriptor d = BuildSpherePatchDescriptor(fov);
    return GlGeometry(d.attribs, d.indices);
}

// 12 edges of a 0 to 1 unit cube.
GlGeometry::Descriptor BuildUnitCubeLinesDescriptor();
inline GlGeometry BuildUnitCubeLines() {
    const GlGeometry::Descriptor d = BuildUnitCubeLinesDescriptor();
    GlGeometry g(d.attribs, d.indices);
    g.primitiveType = GL_LINES;
    return g;
}

// 1.0 width cube, centered around the 0,0,0 point
GlGeometry::Descriptor BuildUnitCubeDescriptor(float side = 0.5f);
inline GlGeometry BuildUnitCube(float side = 0.5f) {
    const GlGeometry::Descriptor d = BuildUnitCubeDescriptor(side);
    return GlGeometry(d.attribs, d.indices);
}

GlGeometry::Descriptor BuildAxisDescriptor(float sideLength = 0.1f, float sideRatio = 0.25f);
inline GlGeometry BuildAxis(float sideLength = 0.1f, float sideRatio = 0.25f) {
    const GlGeometry::Descriptor d = BuildAxisDescriptor(sideLength, sideRatio);
    return GlGeometry(d.attribs, d.indices);
}

} // namespace OVRFW
