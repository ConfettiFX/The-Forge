/************************************************************************************

Filename    :   GlGeometry.cpp
Content     :   OpenGL geometry setup.
Created     :   October 8, 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "GlGeometry.h"

#include "OVR_Math.h"
using OVR::Bounds3f;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

//#include "OVR_GlUtils.h"
#include "Misc/Log.h"

/*
 * These are all built inside VertexArrayObjects, so no GL state other
 * than the VAO binding should be disturbed.
 *
 */
namespace OVRFW {

static bool enableGeometryTransfom = false;
static OVR::Matrix4f geometryTransfom = OVR::Matrix4f();
GlGeometry::TransformScope::TransformScope(const OVR::Matrix4f m, bool enableTransfom) {
    // store old
    wasEnabled = enableGeometryTransfom;
    previousTransform = geometryTransfom;
    // set new
    enableGeometryTransfom = enableTransfom;
    geometryTransfom = m;
}
GlGeometry::TransformScope::~TransformScope() {
    // restore
    enableGeometryTransfom = wasEnabled;
    geometryTransfom = previousTransform;
}

unsigned GlGeometry::IndexType = (sizeof(TriangleIndex) == 2) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

template <typename _attrib_type_>
void PackVertexAttribute(
    std::vector<uint8_t>& packed,
    const std::vector<_attrib_type_>& attrib,
    const int glLocation,
    const int glType,
    const int glComponents) {
    if (attrib.size() > 0) {
        const size_t offset = packed.size();
        const size_t size = attrib.size() * sizeof(attrib[0]);

        packed.resize(offset + size);
        memcpy(&packed[offset], attrib.data(), size);

        glEnableVertexAttribArray(glLocation);
        glVertexAttribPointer(
            glLocation, glComponents, glType, false, sizeof(attrib[0]), (void*)(offset));
    } else {
        glDisableVertexAttribArray(glLocation);
    }
}

void GlGeometry::Create(const VertexAttribs& attribs, const std::vector<TriangleIndex>& indices) {
    vertexCount = attribs.position.size();
    indexCount = indices.size();

    const bool t = enableGeometryTransfom;

    std::vector<OVR::Vector3f> position;
    std::vector<OVR::Vector3f> normal;
    std::vector<OVR::Vector3f> tangent;
    std::vector<OVR::Vector3f> binormal;

    /// we asked for incoming transfom
    if (t) {
        position.resize(attribs.position.size());
        normal.resize(attribs.normal.size());
        tangent.resize(attribs.position.size());
        binormal.resize(attribs.binormal.size());

        /// poor man's 3x3
        OVR::Matrix4f nt = geometryTransfom.Transposed();

        /// Positions use 4x4
        for (size_t i = 0; i < attribs.position.size(); ++i) {
            position[i] = geometryTransfom.Transform(attribs.position[i]);
        }
        /// TBN use 3x3
        for (size_t i = 0; i < attribs.normal.size(); ++i) {
            normal[i] = nt.Transform(attribs.normal[i]).Normalized();
        }
        for (size_t i = 0; i < attribs.tangent.size(); ++i) {
            tangent[i] = nt.Transform(attribs.tangent[i]).Normalized();
        }
        for (size_t i = 0; i < attribs.binormal.size(); ++i) {
            binormal[i] = nt.Transform(attribs.binormal[i]).Normalized();
        }
    }

    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    std::vector<uint8_t> packed;
    PackVertexAttribute(
        packed, t ? position : attribs.position, VERTEX_ATTRIBUTE_LOCATION_POSITION, GL_FLOAT, 3);
    PackVertexAttribute(
        packed, t ? normal : attribs.normal, VERTEX_ATTRIBUTE_LOCATION_NORMAL, GL_FLOAT, 3);
    PackVertexAttribute(
        packed, t ? tangent : attribs.tangent, VERTEX_ATTRIBUTE_LOCATION_TANGENT, GL_FLOAT, 3);
    PackVertexAttribute(
        packed, t ? binormal : attribs.binormal, VERTEX_ATTRIBUTE_LOCATION_BINORMAL, GL_FLOAT, 3);
    PackVertexAttribute(packed, attribs.color, VERTEX_ATTRIBUTE_LOCATION_COLOR, GL_FLOAT, 4);
    PackVertexAttribute(packed, attribs.uv0, VERTEX_ATTRIBUTE_LOCATION_UV0, GL_FLOAT, 2);
    PackVertexAttribute(packed, attribs.uv1, VERTEX_ATTRIBUTE_LOCATION_UV1, GL_FLOAT, 2);
    PackVertexAttribute(
        packed, attribs.jointIndices, VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES, GL_INT, 4);
    PackVertexAttribute(
        packed, attribs.jointWeights, VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS, GL_FLOAT, 4);
    // clang-format off

    glBufferData(GL_ARRAY_BUFFER, packed.size() * sizeof(packed[0]), packed.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(indices[0]),
        indices.data(),
        GL_STATIC_DRAW);

    glBindVertexArray(0);

    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_POSITION);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_NORMAL);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_TANGENT);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_BINORMAL);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_COLOR);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_UV0);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_UV1);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES);
    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS);

    localBounds.Clear();
    for (int i = 0; i < vertexCount; i++) {
        localBounds.AddPoint(attribs.position[i]);
    }
}

void GlGeometry::Update(const VertexAttribs& attribs, const bool updateBounds) {
    vertexCount = attribs.position.size();

    glBindVertexArray(vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    std::vector<uint8_t> packed;
    PackVertexAttribute(packed, attribs.position, VERTEX_ATTRIBUTE_LOCATION_POSITION, GL_FLOAT, 3);
    PackVertexAttribute(packed, attribs.normal, VERTEX_ATTRIBUTE_LOCATION_NORMAL, GL_FLOAT, 3);
    PackVertexAttribute(packed, attribs.tangent, VERTEX_ATTRIBUTE_LOCATION_TANGENT, GL_FLOAT, 3);
    PackVertexAttribute(packed, attribs.binormal, VERTEX_ATTRIBUTE_LOCATION_BINORMAL, GL_FLOAT, 3);
    PackVertexAttribute(packed, attribs.color, VERTEX_ATTRIBUTE_LOCATION_COLOR, GL_FLOAT, 4);
    PackVertexAttribute(packed, attribs.uv0, VERTEX_ATTRIBUTE_LOCATION_UV0, GL_FLOAT, 2);
    PackVertexAttribute(packed, attribs.uv1, VERTEX_ATTRIBUTE_LOCATION_UV1, GL_FLOAT, 2);
    PackVertexAttribute(
        packed, attribs.jointIndices, VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES, GL_INT, 4);
    PackVertexAttribute(
        packed, attribs.jointWeights, VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS, GL_FLOAT, 4);

    glBufferData(GL_ARRAY_BUFFER, packed.size() * sizeof(packed[0]), packed.data(), GL_STATIC_DRAW);

    if (updateBounds) {
        localBounds.Clear();
        for (int i = 0; i < vertexCount; i++) {
            localBounds.AddPoint(attribs.position[i]);
        }
    }
}

void GlGeometry::Free() {
    glDeleteVertexArrays(1, &vertexArrayObject);
    glDeleteBuffers(1, &indexBuffer);
    glDeleteBuffers(1, &vertexBuffer);

    indexBuffer = 0;
    vertexBuffer = 0;
    vertexArrayObject = 0;
    vertexCount = 0;
    indexCount = 0;

    localBounds.Clear();
}

GlGeometry::Descriptor BuildTesselatedQuadDescriptor(
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const bool twoSided) {
    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; y++) {
        const float yf = (float)y / (float)vertical;
        for (int x = 0; x <= horizontal; x++) {
            const float xf = (float)x / (float)horizontal;
            const int index = y * (horizontal + 1) + x;
            attribs.position[index].x = -1.0f + xf * 2.0f;
            attribs.position[index].y = -1.0f + yf * 2.0f;
            attribs.position[index].z = 0.0f;
            attribs.uv0[index].x = xf;
            attribs.uv0[index].y = 1.0f - yf;
            for (int i = 0; i < 4; i++) {
                attribs.color[index][i] = 1.0f;
            }
            // fade to transparent on the outside
            if (x == 0 || x == horizontal || y == 0 || y == vertical) {
                attribs.color[index][3] = 0.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6 * (twoSided ? 2 : 1));

    // If this is to be used to draw a linear format texture, like
    // a surface texture, it is better for cache performance that
    // the triangles be drawn to follow the side to side linear order.
    int index = 0;
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
        // fix the quads in the upper right and lower left corners so that the triangles in the
        // quads share the edges going from the center of the tesselated quad to it's corners.
        const int upperLeftIndexStart = 0;
        indices[upperLeftIndexStart + 1] = indices[upperLeftIndexStart + 5];
        indices[upperLeftIndexStart + 3] = indices[upperLeftIndexStart + 0];

        const int lowerRightIndexStart = (horizontal * (vertical - 1) * 6) + (horizontal - 1) * 6;
        indices[lowerRightIndexStart + 1] = indices[lowerRightIndexStart + 5];
        indices[lowerRightIndexStart + 3] = indices[lowerRightIndexStart + 0];
    }
    if (twoSided) {
        for (TriangleIndex y = 0; y < vertical; y++) {
            for (TriangleIndex x = 0; x < horizontal; x++) {
                indices[index + 5] = y * (horizontal + 1) + x;
                indices[index + 4] = y * (horizontal + 1) + x + 1;
                indices[index + 3] = (y + 1) * (horizontal + 1) + x;
                indices[index + 2] = (y + 1) * (horizontal + 1) + x;
                indices[index + 1] = y * (horizontal + 1) + x + 1;
                indices[index + 0] = (y + 1) * (horizontal + 1) + x + 1;
                index += 6;
            }
        }
    }
    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildTesselatedCylinderDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale) {
    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.normal.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; ++y) {
        const float yf = (float)y / (float)vertical;
        for (int x = 0; x <= horizontal; ++x) {
            const float xf = (float)x / (float)horizontal;
            const int index = y * (horizontal + 1) + x;
            attribs.position[index].x = cosf(MATH_FLOAT_PI * 2 * xf) * radius;
            attribs.position[index].y = sinf(MATH_FLOAT_PI * 2 * xf) * radius;
            attribs.position[index].z = -height + yf * 2 * height;
            attribs.normal[index] =
                Vector3f(attribs.position[index].x, attribs.position[index].y, 0).Normalized();
            attribs.uv0[index].x = xf * uScale;
            attribs.uv0[index].y = (1.0f - yf) * vScale;
            for (int i = 0; i < 4; ++i) {
                attribs.color[index][i] = 1.0f;
            }
            // fade to transparent on the outside
            if (y == 0 || y == vertical) {
                attribs.color[index][3] = 0.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    // If this is to be used to draw a linear format texture, like
    // a surface texture, it is better for cache performance that
    // the triangles be drawn to follow the side to side linear order.
    int index = 0;
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildTesselatedConeDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical,
    const float uScale,
    const float vScale) {
    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.normal.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; ++y) {
        const float yf = (float)y / (float)vertical;
        for (int x = 0; x <= horizontal; ++x) {
            const float xf = (float)x / (float)horizontal;
            const int index = y * (horizontal + 1) + x;
            attribs.position[index].x = cosf(MATH_FLOAT_PI * 2 * xf) * radius * yf;
            attribs.position[index].y = sinf(MATH_FLOAT_PI * 2 * xf) * radius * yf;
            attribs.position[index].z = -height + yf * 2 * height;
            attribs.normal[index] =
                Vector3f(attribs.position[index].x, attribs.position[index].y, 0).Normalized();
            attribs.uv0[index].x = xf * uScale;
            attribs.uv0[index].y = (1.0f - yf) * vScale;
            for (int i = 0; i < 4; ++i) {
                attribs.color[index][i] = 1.0f;
            }
            // fade to transparent on the outside
            if (y == 0 || y == vertical) {
                attribs.color[index][3] = 0.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    // If this is to be used to draw a linear format texture, like
    // a surface texture, it is better for cache performance that
    // the triangles be drawn to follow the side to side linear order.
    int index = 0;
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildTesselatedCapsuleDescriptor(
    const float radius,
    const float height,
    const TriangleIndex horizontal,
    const TriangleIndex vertical) {
    const int vertexCount = (horizontal + 1) * (vertical + 1);
    const int indexCount = horizontal * vertical * 6;
    const float latRads = MATH_FLOAT_PI * 0.5;
    const float h = height * 0.5;

    VertexAttribs attribs;
    attribs.position.resize(vertexCount * 3);
    attribs.normal.resize(vertexCount * 3);

    std::vector<TriangleIndex> indices;
    indices.resize(indexCount * 3);

    int vertexIndexOffset = 0;
    int triangleIndexOffset = 0;

    /// Cylinder
    {
        for (int y = 0; y <= vertical; ++y) {
            const float yf = (float)y / (float)vertical;
            for (int x = 0; x <= horizontal; ++x) {
                const float xf = (float)x / (float)horizontal;
                const int index = y * (horizontal + 1) + x + vertexIndexOffset;
                attribs.position[index].x = cosf(MATH_FLOAT_PI * 2 * xf) * radius;
                attribs.position[index].y = sinf(MATH_FLOAT_PI * 2 * xf) * radius;
                attribs.position[index].z = -h + yf * 2 * h;
                attribs.normal[index] =
                    Vector3f(attribs.position[index].x, attribs.position[index].y, 0).Normalized();
            }
        }

        // If this is to be used to draw a linear format texture, like
        // a surface texture, it is better for cache performance that
        // the triangles be drawn to follow the side to side linear order.
        int index = triangleIndexOffset;
        for (TriangleIndex y = 0; y < vertical; y++) {
            for (TriangleIndex x = 0; x < horizontal; x++) {
                indices[index + 0] = y * (horizontal + 1) + x;
                indices[index + 1] = y * (horizontal + 1) + x + 1;
                indices[index + 2] = (y + 1) * (horizontal + 1) + x;
                indices[index + 3] = (y + 1) * (horizontal + 1) + x;
                indices[index + 4] = y * (horizontal + 1) + x + 1;
                indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
                index += 6;
            }
        }

        vertexIndexOffset += vertexCount;
        triangleIndexOffset += indexCount;
    }

    /// Upper Dome
    {
        for (int y = 0; y <= vertical; y++) {
            const float yf = (float)y / (float)vertical;
            const float lat = MATH_FLOAT_PI - yf * latRads - 0.5f * MATH_FLOAT_PI;
            const float cosLat = cosf(lat);
            for (int x = 0; x <= horizontal; x++) {
                const float xf = (float)x / (float)horizontal;
                const float lon = (0.5f + xf) * MATH_FLOAT_PI * 2;
                const int index = y * (horizontal + 1) + x + vertexIndexOffset;
                attribs.position[index].x = radius * cosf(lon) * cosLat;
                attribs.position[index].y = radius * sinf(lon) * cosLat;
                attribs.position[index].z = h + (radius * sinf(lat));
                attribs.normal[index] = Vector3f(
                                            attribs.position[index].x,
                                            attribs.position[index].y,
                                            attribs.position[index].z - h)
                                            .Normalized();
            }
        }

        int index = triangleIndexOffset;
        for (TriangleIndex x = 0; x < horizontal; x++) {
            for (TriangleIndex y = 0; y < vertical; y++) {
                indices[index + 0] = vertexIndexOffset + y * (horizontal + 1) + x;
                indices[index + 2] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
                indices[index + 1] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
                indices[index + 3] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
                indices[index + 5] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
                indices[index + 4] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x + 1;
                index += 6;
            }
        }

        vertexIndexOffset += vertexCount;
        triangleIndexOffset += indexCount;
    }

    /// Lower Dome
    {
        for (int y = 0; y <= vertical; y++) {
            const float yf = (float)y / (float)vertical;
            const float lat = MATH_FLOAT_PI - yf * latRads - 0.5f * MATH_FLOAT_PI;
            const float cosLat = cosf(lat);
            for (int x = 0; x <= horizontal; x++) {
                const float xf = (float)x / (float)horizontal;
                const float lon = (0.5f + xf) * MATH_FLOAT_PI * 2;
                const int index = y * (horizontal + 1) + x + vertexIndexOffset;
                attribs.position[index].x = radius * cosf(lon) * cosLat;
                attribs.position[index].y = radius * sinf(lon) * cosLat;
                attribs.position[index].z = -h + -(radius * sinf(lat));
                attribs.normal[index] = Vector3f(
                                            attribs.position[index].x,
                                            attribs.position[index].y,
                                            attribs.position[index].z + h)
                                            .Normalized();
            }
        }

        int index = triangleIndexOffset;
        for (TriangleIndex x = 0; x < horizontal; x++) {
            for (TriangleIndex y = 0; y < vertical; y++) {
                indices[index + 0] = vertexIndexOffset + y * (horizontal + 1) + x;
                indices[index + 1] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
                indices[index + 2] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
                indices[index + 3] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
                indices[index + 4] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
                indices[index + 5] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x + 1;
                index += 6;
            }
        }

        vertexIndexOffset += vertexCount;
        triangleIndexOffset += indexCount;
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

// To guarantee that the edge pixels are completely black, we need to
// have a band of solid 0.  Just interpolating to 0 at the edges will
// leave some pixels with low color values.  This stuck out as surprisingly
// visible smears from the distorted edges of the eye renderings in
// some cases.
GlGeometry::Descriptor BuildVignetteDescriptor(const float xFraction, const float yFraction) {
    // Leave 25% of the vignette as solid black
    const float posx[] = {-1.001f,
                          -1.0f + xFraction * 0.25f,
                          -1.0f + xFraction,
                          1.0f - xFraction,
                          1.0f - xFraction * 0.25f,
                          1.001f};
    const float posy[] = {-1.001f,
                          -1.0f + yFraction * 0.25f,
                          -1.0f + yFraction,
                          1.0f - yFraction,
                          1.0f - yFraction * 0.25f,
                          1.001f};

    const int vertexCount = 6 * 6;

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 6; x++) {
            const int index = y * 6 + x;
            attribs.position[index].x = posx[x];
            attribs.position[index].y = posy[y];
            attribs.position[index].z = 0.0f;
            attribs.uv0[index].x = 0.0f;
            attribs.uv0[index].y = 0.0f;
            // the outer edges will have 0 color
            const float c = (y <= 1 || y >= 4 || x <= 1 || x >= 4) ? 0.0f : 1.0f;
            for (int i = 0; i < 3; i++) {
                attribs.color[index][i] = c;
            }
            attribs.color[index][3] = 1.0f; // solid alpha
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(24 * 6);

    int index = 0;
    for (TriangleIndex x = 0; x < 5; x++) {
        for (TriangleIndex y = 0; y < 5; y++) {
            if (x == 2 && y == 2) {
                continue; // the middle is open
            }
            // flip triangulation at corners
            if (x == y) {
                indices[index + 0] = y * 6 + x;
                indices[index + 1] = (y + 1) * 6 + x + 1;
                indices[index + 2] = (y + 1) * 6 + x;
                indices[index + 3] = y * 6 + x;
                indices[index + 4] = y * 6 + x + 1;
                indices[index + 5] = (y + 1) * 6 + x + 1;
            } else {
                indices[index + 0] = y * 6 + x;
                indices[index + 1] = y * 6 + x + 1;
                indices[index + 2] = (y + 1) * 6 + x;
                indices[index + 3] = (y + 1) * 6 + x;
                indices[index + 4] = y * 6 + x + 1;
                indices[index + 5] = (y + 1) * 6 + x + 1;
            }
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildDomeDescriptor(const float latRads, const float uScale, const float vScale) {
    const int horizontal = 64;
    const int vertical = 32;
    const float radius = 100.0f;

    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; y++) {
        const float yf = (float)y / (float)vertical;
        const float lat = MATH_FLOAT_PI - yf * latRads - 0.5f * MATH_FLOAT_PI;
        const float cosLat = cosf(lat);
        for (int x = 0; x <= horizontal; x++) {
            const float xf = (float)x / (float)horizontal;
            const float lon = (0.5f + xf) * MATH_FLOAT_PI * 2;
            const int index = y * (horizontal + 1) + x;

            if (x == horizontal) {
                // Make sure that the wrap seam is EXACTLY the same
                // xyz so there is no chance of pixel cracks.
                attribs.position[index] = attribs.position[y * (horizontal + 1) + 0];
            } else {
                attribs.position[index].x = radius * cosf(lon) * cosLat;
                attribs.position[index].y = radius * sinf(lat);
                attribs.position[index].z = radius * sinf(lon) * cosLat;
            }

            attribs.uv0[index].x = xf * uScale;
            attribs.uv0[index].y = (1.0f - yf) * vScale;
            for (int i = 0; i < 4; i++) {
                attribs.color[index][i] = 1.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    int index = 0;
    for (TriangleIndex x = 0; x < horizontal; x++) {
        for (TriangleIndex y = 0; y < vertical; y++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

// Build it with the equirect center down -Z
GlGeometry::Descriptor BuildGlobeDescriptor(
    const float uScale /*= 1.0f*/,
    const float vScale /*= 1.0f*/,
    const float radius /*= 100.0f*/) {
    // Make four rows at the polar caps in the place of one
    // to diminish the degenerate triangle issue.
    const int poleVertical = 3;
    const int uniformVertical = 64;
    const int horizontal = 128;
    const int vertical = uniformVertical + poleVertical * 2;

    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; y++) {
        float yf;
        if (y <= poleVertical) {
            yf = (float)y / (poleVertical + 1) / uniformVertical;
        } else if (y >= vertical - poleVertical) {
            yf =
                (float)(uniformVertical - 1 + ((float)(y - (vertical - poleVertical - 1)) / (poleVertical + 1))) /
                uniformVertical;
        } else {
            yf = (float)(y - poleVertical) / uniformVertical;
        }
        const float lat = (yf - 0.5f) * MATH_FLOAT_PI;
        const float cosLat = cosf(lat);
        for (int x = 0; x <= horizontal; x++) {
            const float xf = (float)x / (float)horizontal;
            const float lon = (0.25f + xf) * MATH_FLOAT_PI * 2;
            const int index = y * (horizontal + 1) + x;

            if (x == horizontal) {
                // Make sure that the wrap seam is EXACTLY the same
                // xyz so there is no chance of pixel cracks.
                attribs.position[index] = attribs.position[y * (horizontal + 1) + 0];
            } else {
                attribs.position[index].x = radius * cosf(lon) * cosLat;
                attribs.position[index].y = radius * sinf(lat);
                attribs.position[index].z = radius * sinf(lon) * cosLat;
            }

            // With a normal mapping, half the triangles degenerate at the poles,
            // which causes seams between every triangle.  It is better to make them
            // a fan, and only get one seam.
            if (y == 0 || y == vertical) {
                attribs.uv0[index].x = 0.5f;
            } else {
                attribs.uv0[index].x = xf * uScale;
            }
            attribs.uv0[index].y = (1.0f - yf) * vScale;
            for (int i = 0; i < 4; i++) {
                attribs.color[index][i] = 1.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    int index = 0;
    for (TriangleIndex x = 0; x < horizontal; x++) {
        for (TriangleIndex y = 0; y < vertical; y++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildSpherePatchDescriptor(const float fov) {
    const int horizontal = 64;
    const int vertical = 64;
    const float radius = 100.0f;

    const int vertexCount = (horizontal + 1) * (vertical + 1);

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y <= vertical; y++) {
        const float yf = (float)y / (float)vertical;
        const float lat = (yf - 0.5f) * fov;
        const float cosLat = cosf(lat);
        for (int x = 0; x <= horizontal; x++) {
            const float xf = (float)x / (float)horizontal;
            const float lon = (xf - 0.5f) * fov;
            const int index = y * (horizontal + 1) + x;

            attribs.position[index].x = radius * cosf(lon) * cosLat;
            attribs.position[index].y = radius * sinf(lat);
            attribs.position[index].z = radius * sinf(lon) * cosLat;

            // center in the middle of the screen for roll rotation
            attribs.uv0[index].x = xf - 0.5f;
            attribs.uv0[index].y = (1.0f - yf) - 0.5f;

            for (int i = 0; i < 4; i++) {
                attribs.color[index][i] = 1.0f;
            }
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    int index = 0;
    for (TriangleIndex x = 0; x < horizontal; x++) {
        for (TriangleIndex y = 0; y < vertical; y++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildUnitCubeLinesDescriptor() {
    VertexAttribs attribs;
    attribs.position.resize(8);

    for (int i = 0; i < 8; i++) {
        attribs.position[i][0] = static_cast<float>(i & 1);
        attribs.position[i][1] = static_cast<float>((i & 2) >> 1);
        attribs.position[i][2] = static_cast<float>((i & 4) >> 2);
    }

    const TriangleIndex staticIndices[24] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7,
                                             7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6};

    std::vector<TriangleIndex> indices;
    indices.resize(24);
    memcpy(&indices[0], staticIndices, 24 * sizeof(indices[0]));

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

// 2*side width cube, centered around the 0,0,0 point
GlGeometry::Descriptor BuildUnitCubeDescriptor(float side) {
    VertexAttribs attribs;

    // positions
    attribs.position = {
        {-side, +side, -side}, {+side, +side, -side},
        {+side, +side, +side}, {-side, +side, +side}, // top
        {-side, -side, -side}, {-side, -side, +side},
        {+side, -side, +side}, {+side, -side, -side}, // bottom
        {+side, -side, -side}, {+side, +side, -side},
        {+side, +side, +side}, {+side, -side, +side}, // right
        {-side, -side, -side}, {-side, -side, +side},
        {-side, +side, +side}, {-side, +side, -side}, // left
        {-side, -side, +side}, {+side, -side, +side},
        {+side, +side, +side}, {-side, +side, +side}, // front
        {-side, -side, -side}, {-side, +side, -side},
        {+side, +side, -side}, {+side, -side, -side}, // back
    };
    attribs.normal = {
        {0.0f, +1.0f, 0.0f}, {0.0f, +1.0f, 0.0f},
        {0.0f, +1.0f, 0.0f}, {0.0f, +1.0f, 0.0f}, // top
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, // bottom
        {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f},
        {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, // right
        {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, // left
        {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f, +1.0f},
        {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, // front
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, // back
    };

    std::vector<TriangleIndex> indices = {
        0,      2,      1,      2,      0,      3, // top
        4,      6,      5,      6,      4,      7, // bottom
        0 + 8,  1 + 8,  2 + 8,  2 + 8,  3 + 8,  0 + 8, // right
        4 + 8,  5 + 8,  6 + 8,  6 + 8,  7 + 8,  4 + 8, // left
        0 + 16, 1 + 16, 2 + 16, 2 + 16, 3 + 16, 0 + 16, // front
        4 + 16, 5 + 16, 6 + 16, 6 + 16, 7 + 16, 4 + 16, // back
    };

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

GlGeometry::Descriptor BuildAxisDescriptor(float sideLength, float sideRatio) {
    VertexAttribs attribs;
    // positions
    attribs.position = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f * sideLength, 0.0f, 0.0f},
        {0.0f, 1.0f * sideLength, 0.0f},
        {0.0f, 0.0f, 1.0f * sideLength},
        {0.0f, sideRatio * sideLength, 0.0f},
        {0.0f, 0.0f, sideRatio * sideLength},
        {sideRatio * sideLength, 0.0f, 0.0f},
    };

    const Vector4f red{1.0f, 0.0f, 0.0f, 1.0f};
    const Vector4f green{0.0f, 1.0f, 0.0f, 1.0f};
    const Vector4f blue{0.0f, 0.0f, 1.0f, 1.0f};
    attribs.color = {
        red,
        green,
        blue,
        red,
        green,
        blue,
        red,
        green,
        blue,
    };

    std::vector<TriangleIndex> indices = {
        0,
        3,
        6,
        1,
        4,
        7,
        2,
        5,
        8,
        0,
        6,
        3,
        1,
        7,
        4,
        2,
        8,
        5,
    };

    return GlGeometry::Descriptor(attribs, indices, geometryTransfom);
}

} // namespace OVRFW
