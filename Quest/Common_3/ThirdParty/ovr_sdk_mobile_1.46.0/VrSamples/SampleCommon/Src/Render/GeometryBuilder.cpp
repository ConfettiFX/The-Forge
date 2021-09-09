/************************************************************************************

Filename    :   GeometryBuilder.cpp
Content     :   OpenGL geometry setup.
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "GeometryBuilder.h"

namespace OVRFW {

int GeometryBuilder::Add(
    const OVRFW::GlGeometry::Descriptor& geometry,
    int parentIndex,
    const OVR::Vector4f& color,
    const OVR::Matrix4f& transform) {
    int parent = parentIndex;
    OVR::Matrix4f t = transform;

    /// Ensure parent is valid
    if (parentIndex >= 0 && parentIndex < int(nodes_.size())) {
        /// Get world-space transform
        t = nodes_[parentIndex].transform * geometry.transform * transform;
    } else {
        /// normalize bad entities to no-parent
        parent = GeometryBuilder::kInvalidIndex;
    }

    /// Add new node
    nodes_.emplace_back(geometry, parent, color, t);

    /// Return the index of the newly added node
    return int(nodes_.size());
}

OVRFW::GlGeometry::Descriptor GeometryBuilder::ToGeometryDescriptor(
    const OVR::Matrix4f& rootTransform) const {
    size_t vertexCount = 0u;
    size_t indexCount = 0u;

    OVRFW::GlGeometry::Descriptor desc;
    VertexAttribs& attribs = desc.attribs;
    std::vector<TriangleIndex>& indices = desc.indices;

    /// Find out the sizes
    for (const auto& node : nodes_) {
        vertexCount += node.geometry.attribs.position.size();
        indexCount += node.geometry.indices.size();
    }

    /// TODO - figure out if we need ALL attributes

    /// TODO - figure out whether we need int32 indices

    /// TODO - add a version where each sub-object has its own bone index

    /// Make room
    attribs.position.resize(vertexCount);
    attribs.normal.resize(vertexCount);
    attribs.tangent.resize(vertexCount);
    attribs.binormal.resize(vertexCount);
    attribs.color.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.uv1.resize(vertexCount);
    attribs.jointIndices.resize(vertexCount);
    attribs.jointWeights.resize(vertexCount);
    indices.resize(indexCount);

    /// Fill it
    size_t currentVertex = 0u;
    size_t currentIndex = 0u;
    for (const auto& node : nodes_) {
        /// Matrices
        const OVR::Matrix4f t = rootTransform * node.transform;
        const OVR::Matrix4f nt = t;

        /// Vertices
        for (size_t i = 0; i < node.geometry.attribs.position.size(); ++i) {
            attribs.position[i + currentVertex] = t.Transform(node.geometry.attribs.position[i]);
        }
        for (size_t i = 0; i < node.geometry.attribs.normal.size(); ++i) {
            attribs.normal[i + currentVertex] =
                nt.Transform(node.geometry.attribs.normal[i]).Normalized();
        }
        for (size_t i = 0; i < node.geometry.attribs.tangent.size(); ++i) {
            attribs.tangent[i + currentVertex] =
                nt.Transform(node.geometry.attribs.tangent[i]).Normalized();
        }
        for (size_t i = 0; i < node.geometry.attribs.binormal.size(); ++i) {
            attribs.binormal[i + currentVertex] =
                nt.Transform(node.geometry.attribs.binormal[i]).Normalized();
        }
        /// uv
        for (size_t i = 0; i < node.geometry.attribs.uv0.size(); ++i) {
            attribs.uv0[i + currentVertex] = node.geometry.attribs.uv0[i];
        }
        for (size_t i = 0; i < node.geometry.attribs.uv1.size(); ++i) {
            attribs.uv1[i + currentVertex] = node.geometry.attribs.uv1[i];
        }

        /// color - from here
        for (size_t i = 0; i < node.geometry.attribs.position.size(); ++i) {
            attribs.color[i + currentVertex] = node.color;
        }

        /// bones
        for (size_t i = 0; i < node.geometry.attribs.jointIndices.size(); ++i) {
            attribs.jointIndices[i + currentVertex] = node.geometry.attribs.jointIndices[i];
        }
        for (size_t i = 0; i < node.geometry.attribs.jointWeights.size(); ++i) {
            attribs.jointWeights[i + currentVertex] = node.geometry.attribs.jointWeights[i];
        }

        /// Indices
        for (size_t i = 0; i < node.geometry.indices.size(); ++i) {
            indices[i + currentIndex] = node.geometry.indices[i] + currentVertex;
        }

        /// Vertices
        currentVertex += node.geometry.attribs.position.size();
        currentIndex += node.geometry.indices.size();
    }

    return desc;
}

} // namespace OVRFW
