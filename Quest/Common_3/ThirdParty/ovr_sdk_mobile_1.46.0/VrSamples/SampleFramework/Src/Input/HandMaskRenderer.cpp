/************************************************************************************

Filename    :   HandMaskRenderer.cpp
Content     :   A one stop for rendering hand masks
Created     :   03/24/2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "HandMaskRenderer.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

const char* VertexShaderSrc = R"glsl(
  uniform JointMatrices
  {
    highp mat4 Joints[64];
  } jb;

  attribute highp vec4 Position;
  attribute highp vec2 TexCoord;
  varying highp vec2 oTexCoord;

  void main()
  {
    highp vec4 localPos = jb.Joints[ gl_InstanceID ] * Position;
    gl_Position = TransformVertex( localPos );
    oTexCoord = TexCoord;
  }
)glsl";

static const char* FragmentShaderSrc = R"glsl(
    varying highp vec2 oTexCoord;

    uniform float LayerBlend;
    uniform float Falloff;
    uniform float Intensity;
    uniform float FadeIntensity;

    float BorderFade(vec2 uv, float falloff, float intensity)
    {
        uv *= 1.0 - uv.yx;
        float fade = uv.x * uv.y * intensity;
        fade = pow(fade, falloff);
        return clamp(fade, 0.0, 1.0) * FadeIntensity;
    }

    float AlphaGradient(vec2 uv)
    {
        vec2 v = (uv - vec2(0.5)) * vec2(2.0);
        float r = 1.0 - clamp(length(v), 0.0, 1.0);
        return r;
    }

    void main()
    {
#ifdef USE_BORDER_FADE
        float r = BorderFade(oTexCoord, Falloff, Intensity);
#else
        float r = AlphaGradient(oTexCoord);
#endif /// USE_BORDER_FADE

        float c = r * LayerBlend;
        gl_FragColor = vec4(c);
    }
)glsl";

std::vector<OVR::Vector3f> cells = {
    {0.00500, 0.00000, 0.00000},   {0.01740, -0.00030, -0.00030}, {0.00000, 0.00000, 0.00000},
    {0.01370, 0.00000, 0.00000},   {0.00253, 0.00000, 0.00000},   {0.01443, 0.00000, 0.00000},
    {0.02646, 0.00000, 0.00000},   {0.00000, 0.00000, 0.00000},   {0.01530, -0.00040, -0.00030},
    {0.00000, 0.00000, 0.00000},   {0.01470, 0.00000, 0.00000},   {0.00000, 0.00000, 0.00000},
    {0.01570, 0.00000, 0.00000},   {0.02890, 0.00000, 0.00000},   {0.00430, 0.00000, 0.00000},
    {0.01680, 0.00000, 0.00000},   {-0.00170, 0.00000, 0.00000},  {0.01110, 0.00000, 0.00000},
    {0.00100, 0.00000, 0.00000},   {0.01460, 0.00000, 0.00000},   {0.00260, 0.00000, 0.00000},
    {0.01610, 0.00000, -0.00010},  {0.00000, 0.00000, 0.00000},   {0.01400, 0.00000, 0.00000},
    {-0.01100, 0.00000, 0.00000},  {0.00200, 0.00000, 0.00000},   {0.01690, 0.00000, 0.00000},
    {0.00000, 0.00000, 0.00000},   {0.01320, 0.00000, 0.00000},   {0.00000, 0.00000, 0.00000},
    {0.01660, 0.00000, 0.00000},   {0.00000, 0.00000, 0.00000},   {0.01620, 0.00000, 0.00000},
    {0.08120, -0.01130, 0.02410},  {0.03450, -0.01130, 0.01390},  {0.06310, -0.01130, -0.03170},
    {0.03250, -0.01130, -0.01460}, {0.08050, -0.01130, 0.00180},  {0.06470, -0.01130, 0.02340},
    {0.04860, -0.01130, -0.00030}, {0.04480, -0.01130, -0.02370}, {0.07980, -0.01130, -0.01600},
    {0.03250, -0.01130, 0.00060},  {0.06350, -0.01130, 0.01030},  {0.04820, -0.01130, 0.01390},
    {0.04820, -0.01130, 0.02790},  {0.06060, -0.01130, 0.03870},  {0.06350, -0.01130, -0.00600},
    {0.06350, -0.01130, -0.01690}, {0.04860, -0.01130, -0.01290}};

std::vector<ovrHandBone> cellParents = {
    ovrHandBone_Index3,    ovrHandBone_Index3,    ovrHandBone_Index2,    ovrHandBone_Index2,
    ovrHandBone_Index1,    ovrHandBone_Index1,    ovrHandBone_Index1,    ovrHandBone_Middle3,
    ovrHandBone_Middle3,   ovrHandBone_Middle2,   ovrHandBone_Middle2,   ovrHandBone_Middle1,
    ovrHandBone_Middle1,   ovrHandBone_Middle1,   ovrHandBone_Pinky3,    ovrHandBone_Pinky3,
    ovrHandBone_Pinky2,    ovrHandBone_Pinky2,    ovrHandBone_Pinky1,    ovrHandBone_Pinky1,
    ovrHandBone_Ring3,     ovrHandBone_Ring3,     ovrHandBone_Ring2,     ovrHandBone_Ring2,
    ovrHandBone_Ring2,     ovrHandBone_Ring1,     ovrHandBone_Ring1,     ovrHandBone_Thumb3,
    ovrHandBone_Thumb3,    ovrHandBone_Thumb2,    ovrHandBone_Thumb2,    ovrHandBone_Thumb1,
    ovrHandBone_Thumb1,    ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot,
    ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot,
    ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot,
    ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot, ovrHandBone_WristRoot,
    ovrHandBone_WristRoot, ovrHandBone_WristRoot};

void HandMaskRenderer::Init(bool leftHand) {
    /// Shader
    static ovrProgramParm UniformParms[] = {
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
        {"LayerBlend", ovrProgramParmType::FLOAT},
        {"Falloff", ovrProgramParmType::FLOAT},
        {"Intensity", ovrProgramParmType::FLOAT},
        {"FadeIntensity", ovrProgramParmType::FLOAT},
    };

    ProgHandMaskAlphaGradient = GlProgram::Build(
        "",
        VertexShaderSrc,
        "",
        FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));
    ProgHandMaskBorderFade = GlProgram::Build(
        "#define USE_BORDER_FADE 1",
        VertexShaderSrc,
        "#define USE_BORDER_FADE 1",
        FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));

    /// Shader instance buffer
    HandMaskMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    HandMaskUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), HandMaskMatrices.data());

    /// Create surface definition
    HandMaskSurfaceDef.surfaceName = leftHand ? "HandMaskSurfaceL" : "HandMaskSurfaceR";
    HandMaskSurfaceDef.geo = BuildTesselatedQuad(1, 1, false);
    HandMaskSurfaceDef.numInstances = 0;
    /// Build the graphics command
    auto& gc = HandMaskSurfaceDef.graphicsCommand;
    gc.Program = ProgHandMaskBorderFade;
    gc.UniformData[0].Data = &HandMaskUniformBuffer;
    gc.UniformData[1].Data = &LayerBlend;
    gc.UniformData[2].Data = &Falloff;
    gc.UniformData[3].Data = &Intensity;
    gc.UniformData[4].Data = &FadeIntensity;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendMode = GL_FUNC_REVERSE_SUBTRACT;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    gc.GpuState.depthEnable = false;
    gc.GpuState.depthMaskEnable = false;
    /// Add surface
    HandMaskSurface.surface = &(HandMaskSurfaceDef);

    /// Set defaults
    LayerBlend = 1.0f;
    Falloff = 4.0;
    Intensity = 15.0f;
    FadeIntensity = 0.75f;
    UseBorderFade = true;
    BorderFadeSize = 0.03f;
    AlphaMaskSize = 0.0175f;
    RenderInverseSubtract = false;

    /// Set hand
    isLeftHand = leftHand;
}

void HandMaskRenderer::Shutdown() {}

inline Vector3f GetViewMatrixForward(const Matrix4f& m) {
    return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]).Normalized();
}

inline Matrix4f GetViewMatrixFromPose(const OVR::Posef& pose) {
    const Matrix4f transform = Matrix4f(pose);
    return transform.Inverted();
}

void HandMaskRenderer::Update(
    const OVR::Posef& headPose,
    const OVR::Posef& handPose,
    const std::vector<OVR::Matrix4f>& jointTransforms,
    const double displayTimeInSeconds,
    const float handSize) {
    /// get view position
    const Matrix4f centerEyeViewMatrix = GetViewMatrixFromPose(headPose);
    const Matrix4f invViewMatrix = centerEyeViewMatrix.Inverted();
    const Vector3f viewPos = invViewMatrix.GetTranslation();

    /// apply hand transform to the bones
    const Matrix4f matDeviceModel = Matrix4f(handPose);
    const float particleSize = (UseBorderFade ? BorderFadeSize : AlphaMaskSize) * handSize;

    auto& gc = HandMaskSurfaceDef.graphicsCommand;
    gc.Program = UseBorderFade ? ProgHandMaskBorderFade : ProgHandMaskAlphaGradient;
    if (RenderInverseSubtract) {
        gc.GpuState.blendMode = GL_FUNC_REVERSE_SUBTRACT;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    } else {
        gc.GpuState.blendMode = GL_FUNC_ADD;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE;
    }

    for (uint32_t i = 0; i < cells.size(); ++i) {
        const uint32_t parent = cellParents[i];
        Vector3f offset = isLeftHand ? cells[i] * -1.0f : cells[i];
        offset.x *= -1.0f;
        const Matrix4f cellOffset = Matrix4f::Translation(offset);
        const Matrix4f m = (matDeviceModel * (jointTransforms[parent] * cellOffset));
        const Vector3f pos = m.GetTranslation();
        Vector3f normal = (viewPos - pos).Normalized();
        if (normal.LengthSq() < 0.999f) {
            normal = GetViewMatrixForward(centerEyeViewMatrix);
        }
        Matrix4f t = Matrix4f::CreateFromBasisVectors(normal, Vector3f(0.0f, 1.0f, 0.0f));
        t.SetTranslation(pos);
        t = t * Matrix4f::Scaling(particleSize);
        HandMaskMatrices[i] = t.Transposed();
    }
    HandMaskSurface.modelMatrix = Matrix4f();
    HandMaskSurfaceDef.numInstances = cells.size();
    HandMaskUniformBuffer.Update(
        HandMaskMatrices.size() * sizeof(Matrix4f), HandMaskMatrices.data());
}

void HandMaskRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    surfaceList.push_back(HandMaskSurface);
}

} // namespace OVRFW
