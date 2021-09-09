/************************************************************************************

Filename    :   HandRenderer.cpp
Content     :   A one stop for rendering hands
Created     :   April 2020
Authors     :   Federico Schliemann

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "HandRenderer.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

namespace Hand {

/// clang-format off
static_assert(MAX_JOINTS == 64, "MAX_JOINTS != 64");
const char* VertexShaderSrc = R"glsl(
  uniform JointMatrices
  {
     highp mat4 Joints[64];
  } jb;
  attribute highp vec4 Position;
  attribute highp vec3 Normal;
  attribute highp vec3 Tangent;
  attribute highp vec3 Binormal;
  attribute highp vec2 TexCoord;
  attribute highp vec4 JointWeights;
  attribute highp vec4 JointIndices;

  varying lowp vec3 oEye;
  varying lowp vec3 oNormal;
  varying lowp vec2 oTexCoord;

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
      highp vec4 localPos1 = jb.Joints[int(JointIndices.x)] * Position;
      highp vec4 localPos2 = jb.Joints[int(JointIndices.y)] * Position;
      highp vec4 localPos3 = jb.Joints[int(JointIndices.z)] * Position;
      highp vec4 localPos4 = jb.Joints[int(JointIndices.w)] * Position;
      highp vec4 localPos = localPos1 * JointWeights.x
                          + localPos2 * JointWeights.y
                          + localPos3 * JointWeights.z
                          + localPos4 * JointWeights.w;
      gl_Position = TransformVertex( localPos );

      vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
      oEye = eye - vec3( ModelMatrix * Position );

      highp vec3 localNormal1 = multiply( jb.Joints[int(JointIndices.x)], Normal);
      highp vec3 localNormal2 = multiply( jb.Joints[int(JointIndices.y)], Normal);
      highp vec3 localNormal3 = multiply( jb.Joints[int(JointIndices.z)], Normal);
      highp vec3 localNormal4 = multiply( jb.Joints[int(JointIndices.w)], Normal);
      highp vec3 localNormal  = localNormal1 * JointWeights.x
                              + localNormal2 * JointWeights.y
                              + localNormal3 * JointWeights.z
                              + localNormal4 * JointWeights.w;
      oNormal = multiply( ModelMatrix, localNormal );

      oTexCoord = TexCoord;
  }
)glsl";

static const char* FragmentShaderSrc = R"glsl(
  uniform sampler2D Texture0;
  uniform lowp vec3 SpecularLightDirection;
  uniform lowp vec3 SpecularLightColor;
  uniform lowp vec3 AmbientLightColor;
  uniform lowp vec3 GlowColor;
  uniform float Confidence;

  varying lowp vec3 oEye;
  varying lowp vec3 oNormal;
  varying lowp vec2 oTexCoord;

  lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
  {
      return vec3(
          m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
          m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
          m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
  }

  lowp float pow5( float x )
  {
      float x2 = x * x;
      return x2 * x2 * x;
  }

  lowp float pow16( float x )
  {
      float x2 = x * x;
      float x4 = x2 * x2;
      float x8 = x4 * x4;
      float x16 = x8 * x8;
      return x16;
  }

  void main()
  {
      lowp vec3 eyeDir = normalize( oEye.xyz );
      lowp vec3 Normal = normalize( oNormal );

      lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
      lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

      lowp float nDotL = max( dot( Normal, SpecularLightDirection ), 0.0 );
      lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

      lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
      lowp float nDotH = max( dot( Normal, H ), 0.0 );

      lowp float specularPower = 1.0f - diffuse.a;
      specularPower = specularPower * specularPower;
      lowp float specularIntensity = pow16( nDotH );
      lowp vec3 specularValue = specularIntensity * SpecularLightColor;

      lowp float vDotN = dot( eyeDir, Normal );
      lowp float fresnel = clamp( pow5( 1.0 - vDotN ), 0.0, 1.0 );
      lowp vec3 fresnelValue = GlowColor * fresnel;
      lowp vec3 controllerColor = diffuseValue
                                + ambientValue
                                + specularValue
                                + fresnelValue
                                ;
      gl_FragColor.xyz = controllerColor;
      gl_FragColor.w = clamp( fresnel, 0.0, 1.0 ) * Confidence;
  }
)glsl";
/// clang-format on

} // namespace Hand

bool HandRenderer::Init(ovrMobile* ovr, bool leftHand) {
    /// Shader
    ovrProgramParm UniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
        {"GlowColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"Confidence", ovrProgramParmType::FLOAT},
    };
    ProgHand = GlProgram::Build(
        "",
        Hand::VertexShaderSrc,
        "",
        Hand::FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));

    /// Mesh/Skeleton
    ovrHandedness handedness = leftHand ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
    ovrHandSkeleton skeleton;
    skeleton.Header.Version = ovrHandVersion_1;
    if (vrapi_GetHandSkeleton(ovr, handedness, &skeleton.Header) != ovrSuccess) {
        ALOG("HandRenderer - failed to get hand skeleton");
        return false;
    }
    std::unique_ptr<ovrHandMesh> mesh = std::make_unique<ovrHandMesh>();
    if (mesh) {
        mesh->Header.Version = ovrHandVersion_1;
        if (vrapi_GetHandMesh(ovr, handedness, &(mesh.get()->Header)) != ovrSuccess) {
            ALOG("HandRenderer - failed to get mesh");
            return false;
        }
    } else {
        ALOG("HandRenderer - failed to get allocalte mesh");
        return false;
    }
    /// Build geometry from mesh
    VertexAttribs attribs;
    std::vector<TriangleIndex> indices;
    attribs.position.resize(mesh->NumVertices);
    memcpy(
        attribs.position.data(),
        &mesh->VertexPositions[0],
        mesh->NumVertices * sizeof(ovrVector3f));
    attribs.normal.resize(mesh->NumVertices);
    memcpy(attribs.normal.data(), &mesh->VertexNormals[0], mesh->NumVertices * sizeof(ovrVector3f));
    attribs.uv0.resize(mesh->NumVertices);
    memcpy(attribs.uv0.data(), &mesh->VertexUV0[0], mesh->NumVertices * sizeof(ovrVector2f));
    attribs.jointIndices.resize(mesh->NumVertices);
    /// We can't do a straight copy heere since the sizes don't match
    for (std::uint32_t i = 0; i < mesh->NumVertices; ++i) {
        const ovrVector4s& blendIndices = mesh->BlendIndices[i];
        attribs.jointIndices[i].x = blendIndices.x;
        attribs.jointIndices[i].y = blendIndices.y;
        attribs.jointIndices[i].z = blendIndices.z;
        attribs.jointIndices[i].w = blendIndices.w;
    }
    attribs.jointWeights.resize(mesh->NumVertices);
    memcpy(
        attribs.jointWeights.data(),
        &mesh->BlendWeights[0],
        mesh->NumVertices * sizeof(ovrVector4f));
    static_assert(
        sizeof(ovrVertexIndex) == sizeof(TriangleIndex),
        "sizeof(ovrVertexIndex) == sizeof(TriangleIndex) don't match!");
    indices.resize(mesh->NumIndices);
    memcpy(indices.data(), mesh->Indices, mesh->NumIndices * sizeof(ovrVertexIndex));

    /// Model
    HandModel.Init(skeleton);
    TransformMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    BindMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    SkinMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    SkinUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), SkinMatrices.data());

    /// Walk the transform hierarchy and store the wolrd space transforms in TransformMatrices
    const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
    for (size_t j = 0; j < poses.size(); ++j) {
        TransformMatrices[j] = Matrix4f(poses[j]);
    }

    for (size_t j = 0; j < BindMatrices.size(); ++j) {
        BindMatrices[j] = TransformMatrices[j].Inverted();
    }

    /// Create surface definition
    HandSurfaceDef.surfaceName = leftHand ? "HandSurfaceL" : "HandkSurfaceR";
    HandSurfaceDef.geo.Create(attribs, indices);
    HandSurfaceDef.numInstances = 0;
    /// Build the graphics command
    ovrGraphicsCommand& gc = HandSurfaceDef.graphicsCommand;
    /// Program
    gc.Program = ProgHand;
    /// Uniforms to match UniformParms abovve
    gc.UniformData[0].Data = &gc.Textures[0];
    gc.UniformData[1].Data = &SpecularLightDirection;
    gc.UniformData[2].Data = &SpecularLightColor;
    gc.UniformData[3].Data = &AmbientLightColor;
    gc.UniformData[4].Count = MAX_JOINTS;
    gc.UniformData[4].Data = &SkinUniformBuffer;
    gc.UniformData[5].Data = &GlowColor;
    gc.UniformData[6].Data = &Confidence;
    /// gpu state needs alpha blending
    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

    /// Add surface
    HandSurface.surface = &(HandSurfaceDef);

    /// Set defaults
    SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;
    GlowColor = OVR::Vector3f(0.75f);
    Confidence = 1.0f;

    /// Set hand
    isLeftHand = leftHand;

    /// all good
    return true;
}

void HandRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgHand);
    HandSurfaceDef.geo.Free();
}

void HandRenderer::Update(const ovrHandPose& handPose) {
    /// update based on hand pose
    HandModel.Update(handPose);

    /// update transforms
    const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
    for (size_t j = 0; j < poses.size(); ++j) {
        /// Compute transform
        TransformMatrices[j] = Matrix4f(poses[j]);
        Matrix4f m = TransformMatrices[j] * BindMatrices[j];
        SkinMatrices[j] = m.Transposed();
    }

    /// Update the shader uniform parameters
    SkinUniformBuffer.Update(SkinMatrices.size() * sizeof(Matrix4f), SkinMatrices.data());

    /// Update the pose
    OVR::Posef rootPose = handPose.RootPose;
    Matrix4f matDeviceModel = Matrix4f(rootPose) * Matrix4f::Scaling(handPose.HandScale);
    HandSurface.modelMatrix = matDeviceModel;
}

void HandRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    surfaceList.push_back(HandSurface);
}

} // namespace OVRFW
