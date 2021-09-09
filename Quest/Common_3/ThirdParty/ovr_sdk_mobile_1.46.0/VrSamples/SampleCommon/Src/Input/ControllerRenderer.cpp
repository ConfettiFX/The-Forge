/************************************************************************************

Filename    :   ControllerRenderer.cpp
Content     :   A one stop for rendering controllers
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "ControllerRenderer.h"

#include "Render/GeometryBuilder.h"
#include "Render/GlGeometry.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

namespace Controller {

/// clang-format off
static const char* VertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;
attribute lowp vec4 VertexColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp vec4 oColor;

vec3 multiply( mat4 m, vec3 v )
{
  return vec3(
  m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

vec3 transposeMultiply( mat4 m, vec3 v )
{
  return vec3(
  m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
  m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
  m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
  gl_Position = TransformVertex( Position );
  vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
  oEye = eye - vec3( ModelMatrix * Position );

  oNormal = multiply( ModelMatrix, Normal );

  oTexCoord = TexCoord;
  oColor = VertexColor;
}
)glsl";

static const char* FragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp vec4 oColor;

lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
{
  return vec3(
  m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

void main()
{
  lowp vec3 eyeDir = normalize( oEye.xyz );
  lowp vec3 Normal = normalize( oNormal );

  lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;

  lowp vec4 diffuse = oColor;
#ifdef USE_TEXTURE  
  diffuse = texture2D( Texture0, oTexCoord );
#endif

  lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

  lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
  lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

  lowp float specularPower = 1.0f;
  lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
  lowp float nDotH = max( dot( Normal, H ), 0.0 );
  lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
  lowp vec3 specularValue = specularIntensity * SpecularLightColor;

  lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
  gl_FragColor.xyz = controllerColor;
  gl_FragColor.w = 1.0f;
}
)glsl";
/// clang-format on

} // namespace Controller

void ControllerRenderer::LoadModelFromResource(
    OVRFW::ovrFileSys* fileSys,
    const char* controllerModelFile) {
    if (Model) {
        delete Model;
        Model = nullptr;
    }
    if (controllerModelFile && fileSys) {
        ModelGlPrograms programs(&ProgControllerTexture);
        MaterialParms materials;
        Model = LoadModelFile(*fileSys, controllerModelFile, programs, materials);
        if (Model != nullptr) {
            for (auto& model : Model->Models) {
                auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
                gc.UniformData[0].Data = &gc.Textures[0];
                gc.UniformData[1].Data = &SpecularLightDirection;
                gc.UniformData[2].Data = &SpecularLightColor;
                gc.UniformData[3].Data = &AmbientLightColor;
                /// gpu state needs alpha blending
                gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
                gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
                gc.GpuState.blendSrc = GL_SRC_ALPHA;
                gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
                ControllerSurfaceDef = model.surfaces[0].surfaceDef;
            }
            ControllerSurface.surface = &(ControllerSurfaceDef);
        }
    }
}

bool ControllerRenderer::Init(
    bool leftController,
    OVRFW::ovrFileSys* fileSys,
    const char* controllerModelFile) {
    Model = nullptr;

    /// Shader
    ovrProgramParm UniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };
    ProgControllerTexture = GlProgram::Build(
        "#define USE_TEXTURE 1\n",
        Controller::VertexShaderSrc,
        "#define USE_TEXTURE 1\n",
        Controller::FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));

    ProgControllerColor = GlProgram::Build(
        "#define USE_COLOR 1\n",
        Controller::VertexShaderSrc,
        "#define USE_COLOR 1\n",
        Controller::FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));

    /// Create surface definition
    ControllerSurfaceDef.surfaceName =
        leftController ? "ControllerSurfaceL" : "ControllerkSurfaceR";

    /// Atempt to load a resource if passed in
    LoadModelFromResource(fileSys, controllerModelFile);

    /// Build geometry from mesh
    if (Model == nullptr) {
        /// We didn't get a resource, build using gemetry primitives
        OVRFW::GeometryBuilder gb;

        /// common root
        OVRFW::GlGeometry::Descriptor nullDesc;
        nullDesc.transform = Matrix4f::RotationX(OVR::DegreeToRad(30.0f));
        int nullIdx = gb.Add(nullDesc);

        /// long capsure
        const Matrix4f capsuleMatrix = Matrix4f::Translation({0.0f, 0.05f, 0.0f}) *
            Matrix4f::RotationX(OVR::DegreeToRad(90.0f));
        gb.Add(
            OVRFW::BuildTesselatedCapsuleDescriptor(0.02f, 0.08f, 10, 7),
            nullIdx,
            {1.0f, 0.9f, 0.25f, 1.0f},
            capsuleMatrix);

        /// ring
        const Matrix4f ringMatrix = Matrix4f::Translation({0.0f, 0.02f, 0.04f});
        gb.Add(
            OVRFW::BuildTesselatedCylinderDescriptor(0.04f, 0.015f, 24, 2, 1.0f, 1.0f),
            nullIdx,
            {0.6f, 0.8f, 0.25f, 1.0f},
            ringMatrix);

        ControllerSurfaceDef.geo = gb.ToGeometry();

        ovrGraphicsCommand& gc = ControllerSurfaceDef.graphicsCommand;
        gc.GpuState.cullEnable = false; // Double sided
    }

    /// Build the graphics command
    ovrGraphicsCommand& gc = ControllerSurfaceDef.graphicsCommand;
    /// Program
    gc.Program = (Model == nullptr) ? ProgControllerColor : ProgControllerTexture;
    /// Uniforms to match UniformParms abovve
    gc.UniformData[0].Data = &gc.Textures[0];
    gc.UniformData[1].Data = &SpecularLightDirection;
    gc.UniformData[2].Data = &SpecularLightColor;
    gc.UniformData[3].Data = &AmbientLightColor;
    /// gpu state needs alpha blending
    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

    /// Add surface
    ControllerSurface.surface = &(ControllerSurfaceDef);

    /// Set defaults
    SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f);
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;

    /// Set hand
    isLeftController = leftController;

    /// all good
    return true;
}

void ControllerRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgControllerTexture);
    OVRFW::GlProgram::Free(ProgControllerColor);
    ControllerSurfaceDef.geo.Free();
    if (Model != nullptr) {
        delete Model;
        Model = nullptr;
    }
}

void ControllerRenderer::Update(const OVR::Posef& pose) {
    const OVR::Posef controllerPose = pose;
    const OVR::Matrix4f matDeviceModel = OVR::Matrix4f(controllerPose) *
        Matrix4f::RotationY(OVR::DegreeToRad(180.0f)) *
        Matrix4f::RotationX(OVR::DegreeToRad(-90.0f));
    ControllerSurface.modelMatrix = matDeviceModel;
}

void ControllerRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    surfaceList.push_back(ControllerSurface);
}

} // namespace OVRFW
