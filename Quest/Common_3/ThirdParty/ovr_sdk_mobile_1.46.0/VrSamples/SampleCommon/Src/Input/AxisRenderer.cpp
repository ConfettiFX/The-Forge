/************************************************************************************

Filename    :   AxisRenderer.cpp
Content     :   A rendering component for axis
Created     :   September 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "AxisRenderer.h"
#include "Misc/Log.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

static const char* AxisVertexShaderSrc = R"glsl(
    uniform JointMatrices
    {
        highp mat4 Joints[64];
    } jb;

    attribute highp vec4 Position;
    attribute lowp vec4 VertexColor;
    varying lowp vec4 oColor;

    void main()
    {
        highp vec4 localPos = jb.Joints[ gl_InstanceID ] * Position;
        gl_Position = TransformVertex( localPos );
        oColor = VertexColor;
    }
)glsl";

static const char* AxisFragmentShaderSrc = R"glsl(
    varying lowp vec4 oColor;
    void main()
    {
        gl_FragColor = oColor;
    }
)glsl";

namespace OVRFW {

bool ovrAxisRenderer::Init(size_t count) {
    /// Defaults
    Count = count;
    AxisSize = 0.025f;

    /// Create Axis program
    static ovrProgramParm AxisUniformParms[] = {
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
    };
    ProgAxis = GlProgram::Build(
        AxisVertexShaderSrc,
        AxisFragmentShaderSrc,
        AxisUniformParms,
        sizeof(AxisUniformParms) / sizeof(ovrProgramParm));

    TransformMatrices.resize(Count, OVR::Matrix4f::Identity());
    InstancedBoneUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, Count * sizeof(Matrix4f), TransformMatrices.data());

    /// Create Axis surface definition
    AxisSurfaceDef.surfaceName = "AxisSurfaces";
    AxisSurfaceDef.geo = OVRFW::BuildAxis(AxisSize);
    AxisSurfaceDef.numInstances = 0;
    /// Build the graphics command
    auto& gc = AxisSurfaceDef.graphicsCommand;
    gc.Program = ProgAxis;
    gc.UniformData[0].Data = &InstancedBoneUniformBuffer;
    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_DISABLE;
    gc.GpuState.blendSrc = GL_ONE;
    /// Add surface
    AxisSurface.surface = &(AxisSurfaceDef);

    return true;
}

void ovrAxisRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgAxis);
    InstancedBoneUniformBuffer.Destroy();
}

void ovrAxisRenderer::Update(const std::vector<OVR::Posef>& points) {
    Update(points.data(), points.size());
}

void ovrAxisRenderer::Update(const OVR::Posef* points, size_t count) {
    if (count > Count) {
        Count = count;
        TransformMatrices.resize(Count, OVR::Matrix4f::Identity());
        InstancedBoneUniformBuffer.Destroy();
        InstancedBoneUniformBuffer.Create(
            GLBUFFER_TYPE_UNIFORM, Count * sizeof(Matrix4f), TransformMatrices.data());
    }
    for (size_t j = 0; j < count; ++j) {
        /// Compute transform
        OVR::Matrix4f t(points[j]);
        TransformMatrices[j] = t.Transposed();
    }
    InstancedBoneUniformBuffer.Update(
        TransformMatrices.size() * sizeof(Matrix4f), TransformMatrices.data());
}

void ovrAxisRenderer::Render(
    const OVR::Matrix4f& worldMatrix,
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    AxisSurfaceDef.numInstances = Count;
    AxisSurface.modelMatrix = worldMatrix;
    out.Surfaces.push_back(AxisSurface);
}

} // namespace OVRFW
