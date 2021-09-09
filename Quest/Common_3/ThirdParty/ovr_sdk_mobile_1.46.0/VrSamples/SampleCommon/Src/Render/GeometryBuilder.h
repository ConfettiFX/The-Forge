/************************************************************************************

Filename    :   GeometryBuilder.h
Content     :   OpenGL geometry setup.
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include <vector>
#include "OVR_Math.h"

#include "GlProgram.h"
#include "GlGeometry.h"

namespace OVRFW {

class GeometryBuilder {
   public:
    GeometryBuilder() = default;
    ~GeometryBuilder() = default;

    static constexpr int kInvalidIndex = -1;

    struct Node {
        Node(
            const OVRFW::GlGeometry::Descriptor& g,
            int parent,
            const OVR::Vector4f& c,
            const OVR::Matrix4f& t)
            : geometry(g), parentIndex(parent), color(c), transform(t) {}

        OVRFW::GlGeometry::Descriptor geometry;
        int parentIndex = -1;
        OVR::Vector4f color = OVR::Vector4f(0.5f, 0.5f, 0.5f, 1.0f);
        OVR::Matrix4f transform = OVR::Matrix4f::Identity();
    };

    int Add(
        const OVRFW::GlGeometry::Descriptor& geometry,
        int parentIndex = kInvalidIndex,
        const OVR::Vector4f& color = OVR::Vector4f(0.5f, 0.5f, 0.5f, 1.0f),
        const OVR::Matrix4f& transform = OVR::Matrix4f::Identity());

    OVRFW::GlGeometry::Descriptor ToGeometryDescriptor(
        const OVR::Matrix4f& rootTransform = OVR::Matrix4f::Identity()) const;

    OVRFW::GlGeometry ToGeometry(
        const OVR::Matrix4f& rootTransform = OVR::Matrix4f::Identity()) const {
        OVRFW::GlGeometry::Descriptor d = ToGeometryDescriptor(rootTransform);
        return OVRFW::GlGeometry(d.attribs, d.indices);
    }

    const std::vector<OVRFW::GeometryBuilder::Node>& Nodes() const {
        return nodes_;
    }

   private:
    std::vector<OVRFW::GeometryBuilder::Node> nodes_;
};

} // namespace OVRFW
