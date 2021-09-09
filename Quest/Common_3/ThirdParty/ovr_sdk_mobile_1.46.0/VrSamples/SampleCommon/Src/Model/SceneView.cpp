/************************************************************************************

Filename    :   SceneView.cpp
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SceneView.h"
#include "ModelRender.h"

#include <algorithm>

#include "Misc/Log.h"

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3d;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

/*
    Vertex Color
*/

const char* VertexColorVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute lowp vec4 VertexColor;
varying lowp vec4 oColor;
void main()
{
   gl_Position = TransformVertex( Position );
   oColor = VertexColor;
}
)glsl";

const char* VertexColorSkinned1VertexShaderSrc = R"glsl(
uniform JointMatrices
{
	highp mat4 Joints[64];
} jb;
attribute highp vec4 Position;
attribute lowp vec4 VertexColor;
attribute highp vec4 JointWeights;
attribute highp vec4 JointIndices;
varying lowp vec4 oColor;
void main()
{
   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;
   gl_Position = TransformVertex( localPos );
   oColor = VertexColor;
}
)glsl";

const char* VertexColorFragmentShaderSrc = R"glsl(
varying lowp vec4 oColor;
void main()
{
   gl_FragColor = oColor;
}
)glsl";

/*
    Single Texture
*/

const char* SingleTextureVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
varying highp vec2 oTexCoord;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
}
)glsl";

const char* SingleTextureSkinned1VertexShaderSrc = R"glsl(
uniform JointMatrices
{
	highp mat4 Joints[64];
} jb;
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
attribute highp vec4 JointWeights;
attribute highp vec4 JointIndices;
varying highp vec2 oTexCoord;
void main()
{
   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;
   gl_Position = TransformVertex( localPos );
   oTexCoord = TexCoord;
}
)glsl";

const char* SingleTextureFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
varying highp vec2 oTexCoord;
void main()
{
   gl_FragColor = texture2D( Texture0, oTexCoord );
}
)glsl";

/*
    Light Mapped
*/

const char* LightMappedVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
attribute highp vec2 TexCoord1;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
   oTexCoord1 = TexCoord1;
}
)glsl";

const char* LightMappedSkinned1VertexShaderSrc = R"glsl(
uniform JointMatrices
{
	highp mat4 Joints[64];
} jb;
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
attribute highp vec2 TexCoord1;
attribute highp vec4 JointWeights;
attribute highp vec4 JointIndices;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
void main()
{
   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;
   gl_Position = TransformVertex( localPos );
   oTexCoord = TexCoord;
   oTexCoord1 = TexCoord1;
}
)glsl";

const char* LightMappedFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
void main()
{
   lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
   lowp vec4 emissive = texture2D( Texture1, oTexCoord1 );
   gl_FragColor.xyz = diffuse.xyz * emissive.xyz * 1.5;
   gl_FragColor.w = diffuse.w;
}
)glsl";

/*
    Reflection Mapped
*/

const char* ReflectionMappedVertexShaderSrc = R"glsl(
uniform highp mat4 Modelm;
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;
attribute highp vec2 TexCoord1;
varying highp vec3 oEye;
varying highp vec3 oNormal;
varying highp vec3 oTangent;
varying highp vec3 oBinormal;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
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
   oEye = eye - vec3( Modelm * Position );
   oNormal = multiply( Modelm, Normal );
   oTangent = multiply( Modelm, Tangent );
   oBinormal = multiply( Modelm, Binormal );
   oTexCoord = TexCoord;
   oTexCoord1 = TexCoord1;
}
)glsl";

const char* ReflectionMappedSkinned1VertexShaderSrc = R"glsl(
uniform highp mat4 Modelm;
uniform JointMatrices
{
	highp mat4 Joints[64];
} jb;
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;
attribute highp vec2 TexCoord1;
attribute highp vec4 JointWeights;
attribute highp vec4 JointIndices;
varying highp vec3 oEye;
varying highp vec3 oNormal;
varying highp vec3 oTangent;
varying highp vec3 oBinormal;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
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
   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;
   gl_Position = TransformVertex( localPos );
   vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
   oEye = eye - vec3( Modelm * ( jb.Joints[int(JointIndices.x)] * Position ) );
   oNormal = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Normal ) );
   oTangent = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Tangent ) );
   oBinormal = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Binormal ) );
   oTexCoord = TexCoord;
   oTexCoord1 = TexCoord1;
}
)glsl";

const char* ReflectionMappedFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform sampler2D Texture2;
uniform sampler2D Texture3;
uniform samplerCube Texture4;
varying highp vec3 oEye;
varying highp vec3 oNormal;
varying highp vec3 oTangent;
varying highp vec3 oBinormal;
varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
void main()
{
   mediump vec3 normal = texture2D( Texture2, oTexCoord ).xyz * 2.0 - 1.0;
   mediump vec3 surfaceNormal = normal.x * oTangent + normal.y * oBinormal + normal.z * oNormal;
   mediump vec3 eyeDir = normalize( oEye.xyz );
   mediump vec3 reflectionDir = dot( eyeDir, surfaceNormal ) * 2.0 * surfaceNormal - eyeDir;
   lowp vec3 specular = texture2D( Texture3, oTexCoord ).xyz * textureCube( Texture4, reflectionDir ).xyz;
   lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
   lowp vec4 emissive = texture2D( Texture1, oTexCoord1 );
	gl_FragColor.xyz = diffuse.xyz * emissive.xyz * 1.5 + specular;
   gl_FragColor.w = diffuse.w;
}
)glsl";

/*
PBR
Currently this is flat shaded with emmissive so not really PBR
*/

const char* SimplePBRVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
varying highp vec2 oTexCoord;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
}
)glsl";

const char* SimplePBRSkinned1VertexShaderSrc = R"glsl(
uniform JointMatrices
{
	highp mat4 Joints[64];
} jb;
attribute highp vec4 Position;
attribute highp vec2 TexCoord;
attribute highp vec4 JointWeights;
attribute highp vec4 JointIndices;
varying highp vec2 oTexCoord;
void main()
{
   highp vec4 localPos1 = jb.Joints[int(JointIndices.x)] * Position;
   highp vec4 localPos2 = jb.Joints[int(JointIndices.y)] * Position;
   highp vec4 localPos3 = jb.Joints[int(JointIndices.z)] * Position;
   highp vec4 localPos4 = jb.Joints[int(JointIndices.w)] * Position;
   highp vec4 localPos = localPos1 * JointWeights.x + localPos2 * JointWeights.y + localPos3 * JointWeights.z + localPos4 * JointWeights.w;
   gl_Position = TransformVertex( localPos );
   oTexCoord = TexCoord;
}
)glsl";

const char* SimplePBRFragmentShaderSrc = R"glsl(
uniform lowp vec4 BaseColorFactor;
void main()
{
   gl_FragColor = BaseColorFactor; 
}
)glsl";

const char* BaseColorPBRFragmentShaderSrc = R"glsl(
uniform sampler2D BaseColorTexture;
uniform lowp vec4 BaseColorFactor;
varying highp vec2 oTexCoord;
void main()
{
   lowp vec4 BaseColor = texture2D( BaseColorTexture, oTexCoord );
   gl_FragColor.r = BaseColor.r * BaseColorFactor.r; 
   gl_FragColor.g = BaseColor.g * BaseColorFactor.g; 
   gl_FragColor.b = BaseColor.b * BaseColorFactor.b; 
   gl_FragColor.w = BaseColor.w * BaseColorFactor.w; 
}
)glsl";

const char* BaseColorEmissivePBRFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform lowp vec4 BaseColorFactor;
uniform lowp vec4 EmissiveFactor;
varying highp vec2 oTexCoord;
void main()
{
   lowp vec4 BaseColor = texture2D( Texture0, oTexCoord );
   BaseColor.r = BaseColor.r * BaseColorFactor.r; 
   BaseColor.g = BaseColor.g * BaseColorFactor.g; 
   BaseColor.b = BaseColor.b * BaseColorFactor.b; 
   BaseColor.w = BaseColor.w * BaseColorFactor.w; 
   lowp vec4 EmissiveColor = texture2D( Texture1, oTexCoord );
   EmissiveColor.r = EmissiveColor.r * EmissiveFactor.r; 
   EmissiveColor.g = EmissiveColor.g * EmissiveFactor.g; 
   EmissiveColor.b = EmissiveColor.b * EmissiveFactor.b; 
   EmissiveColor.w = EmissiveColor.w * EmissiveFactor.w; 
   gl_FragColor = BaseColor + EmissiveColor; 
}
)glsl";

void ModelInScene::SetModelFile(const ModelFile* mf) {
    Definition = mf;
    if (mf != NULL) {
        State.GenerateStateFromModelFile(mf);
    }
};

static Vector3f AnimationInterpolateVector3f(
    float* buffer,
    int frame,
    float fraction,
    ModelAnimationInterpolation interpolationType) {
    Vector3f firstElement;
    firstElement.x = buffer[frame * 3 + 0];
    firstElement.y = buffer[frame * 3 + 1];
    firstElement.z = buffer[frame * 3 + 2];
    Vector3f secondElement;
    secondElement.x = buffer[frame * 3 + 3];
    secondElement.y = buffer[frame * 3 + 4];
    secondElement.z = buffer[frame * 3 + 5];

    if (interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR) {
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP) {
        if (fraction >= 1.0f) {
            return secondElement;
        } else {
            return firstElement;
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE) {
        // #TODO implement MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE not implemented");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE) {
        // #TODO implement MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE not implemented");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else {
        ALOGW("inavlid interpolation type on animation");
        return firstElement;
    }
}

static Quatf AnimationInterpolateQuatf(
    float* buffer,
    int frame,
    float fraction,
    ModelAnimationInterpolation interpolationType) {
    Quatf firstElement;
    firstElement.x = buffer[frame * 4 + 0];
    firstElement.y = buffer[frame * 4 + 1];
    firstElement.z = buffer[frame * 4 + 2];
    firstElement.w = buffer[frame * 4 + 3];
    Quatf secondElement;
    secondElement.x = buffer[frame * 4 + 4];
    secondElement.y = buffer[frame * 4 + 5];
    secondElement.z = buffer[frame * 4 + 6];
    secondElement.w = buffer[frame * 4 + 7];

    if (interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR) {
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP) {
        if (fraction >= 1.0f) {
            return secondElement;
        } else {
            return firstElement;
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE) {
        ALOGW(
            "MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE does not make sense for quaternions.");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE) {
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE does not make sense for quaternions.");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else {
        ALOGW("inavlid interpolation type on animation");
        return firstElement;
    }
}

void ModelInScene::AnimateJoints(const double timeInSeconds) {
    // new animation method.
    {
        if (State.animationTimelineStates.size() > 0) {
            State.CalculateAnimationFrameAndFraction(
                MODEL_ANIMATION_TIME_TYPE_LOOP_FORWARD, (float)timeInSeconds);

            for (int i = 0; i < static_cast<int>(State.mf->Animations.size()); i++) {
                const ModelAnimation& animation = State.mf->Animations[i];
                for (int j = 0; j < static_cast<int>(animation.channels.size()); j++) {
                    const ModelAnimationChannel& channel = animation.channels[j];
                    ModelNodeState& nodeState = State.nodeStates[channel.nodeIndex];
                    ModelAnimationTimeLineState& timeLineState =
                        State.animationTimelineStates[channel.sampler->timeLineIndex];

                    float* bufferData = (float*)(channel.sampler->output->BufferData());
                    if (channel.path == MODEL_ANIMATION_PATH_TRANSLATION) {
                        Vector3f translation = AnimationInterpolateVector3f(
                            bufferData,
                            timeLineState.frame,
                            timeLineState.fraction,
                            channel.sampler->interpolation);
                        nodeState.translation = translation;
                    } else if (channel.path == MODEL_ANIMATION_PATH_SCALE) {
                        Vector3f scale = AnimationInterpolateVector3f(
                            bufferData,
                            timeLineState.frame,
                            timeLineState.fraction,
                            channel.sampler->interpolation);
                        nodeState.scale = scale;
                    } else if (channel.path == MODEL_ANIMATION_PATH_ROTATION) {
                        Quatf rotation = AnimationInterpolateQuatf(
                            bufferData,
                            timeLineState.frame,
                            timeLineState.fraction,
                            channel.sampler->interpolation);
                        nodeState.rotation = rotation;
                    } else if (channel.path == MODEL_ANIMATION_PATH_WEIGHTS) {
                        ALOGW(
                            "Weights animation not currently supported on channel %d '%s'",
                            j,
                            animation.name.c_str());
                    } else {
                        ALOGW("Bad animation path on channel %d '%s'", j, animation.name.c_str());
                    }

                    nodeState.CalculateLocalTransform();
                }
            }

            for (int i = 0; i < static_cast<int>(State.nodeStates.size()); i++) {
                State.nodeStates[i].RecalculateMatrix();
            }
        }
    }
}

//-------------------------------------------------------------------------------------

OvrSceneView::OvrSceneView()
    : FreeWorldModelOnChange(false),
      LoadedPrograms(false),
      Paused(false),
      SuppressModelsWithClientId(-1),
      // FIXME: ideally EyeHeight and IPD properties would default initialize to 0.0f, but there are
      // a handful of menus which cache these values before a frame has had a chance to run.
      EyeHeight(1.6750f), // average eye height above the ground when standing
      InterPupillaryDistance(0.0640f), // average interpupillary distance
      Znear(0.1f), // #define VRAPI_ZNEAR 0.1f
      StickYaw(0.0f),
      StickPitch(0.0f),
      SceneYaw(0.0f),
      YawVelocity(0.0f),
      MoveSpeed(3.0f),
      FreeMove(false),
      FootPos(0.0f),
      EyeYaw(0.0f),
      EyePitch(0.0f),
      EyeRoll(0.0f),
      YawMod(-1.0f) {
    CenterEyeTransform = Matrix4f::Identity();
    CenterEyeViewMatrix = Matrix4f::Identity();
}

ModelGlPrograms OvrSceneView::GetDefaultGLPrograms() {
    ModelGlPrograms programs;

    if (!LoadedPrograms) {
        ProgVertexColor = OVRFW::GlProgram::Build(
            VertexColorVertexShaderSrc, VertexColorFragmentShaderSrc, nullptr, 0);

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSingleTexture = OVRFW::GlProgram::Build(
                SingleTextureVertexShaderSrc,
                SingleTextureFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgLightMapped = OVRFW::GlProgram::Build(
                LightMappedVertexShaderSrc,
                LightMappedFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"Modelm", OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture2", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture3", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture4", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgReflectionMapped = OVRFW::GlProgram::Build(
                ReflectionMappedVertexShaderSrc,
                ReflectionMappedFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSimplePBR = OVRFW::GlProgram::Build(
                SimplePBRVertexShaderSrc, SimplePBRFragmentShaderSrc, uniformParms, uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"BaseColorTexture", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgBaseColorPBR = OVRFW::GlProgram::Build(
                SimplePBRVertexShaderSrc,
                BaseColorPBRFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"EmissiveFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgBaseColorEmissivePBR = OVRFW::GlProgram::Build(
                SimplePBRVertexShaderSrc,
                BaseColorEmissivePBRFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedVertexColor = OVRFW::GlProgram::Build(
                VertexColorSkinned1VertexShaderSrc,
                VertexColorFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedSingleTexture = OVRFW::GlProgram::Build(
                SingleTextureSkinned1VertexShaderSrc,
                SingleTextureFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedLightMapped = OVRFW::GlProgram::Build(
                LightMappedSkinned1VertexShaderSrc,
                LightMappedFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                {"Modelm", OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
                /// Fragment
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture2", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture3", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture4", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedReflectionMapped = OVRFW::GlProgram::Build(
                ReflectionMappedSkinned1VertexShaderSrc,
                ReflectionMappedFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedSimplePBR = OVRFW::GlProgram::Build(
                SimplePBRSkinned1VertexShaderSrc,
                SimplePBRFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"BaseColorTexture", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedBaseColorPBR = OVRFW::GlProgram::Build(
                SimplePBRSkinned1VertexShaderSrc,
                BaseColorPBRFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        {
            OVRFW::ovrProgramParm uniformParms[] = {
                /// Vertex
                {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
                /// Fragment
                {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"EmissiveFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
                {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            };
            const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
            ProgSkinnedBaseColorEmissivePBR = OVRFW::GlProgram::Build(
                SimplePBRSkinned1VertexShaderSrc,
                BaseColorEmissivePBRFragmentShaderSrc,
                uniformParms,
                uniformCount);
        }

        LoadedPrograms = true;
    }

    programs.ProgVertexColor = &ProgVertexColor;
    programs.ProgSingleTexture = &ProgSingleTexture;
    programs.ProgLightMapped = &ProgLightMapped;
    programs.ProgReflectionMapped = &ProgReflectionMapped;
    programs.ProgSimplePBR = &ProgSimplePBR;
    programs.ProgBaseColorPBR = &ProgBaseColorPBR;
    programs.ProgBaseColorEmissivePBR = &ProgBaseColorEmissivePBR;
    programs.ProgSkinnedVertexColor = &ProgSkinnedVertexColor;
    programs.ProgSkinnedSingleTexture = &ProgSkinnedSingleTexture;
    programs.ProgSkinnedLightMapped = &ProgSkinnedLightMapped;
    programs.ProgSkinnedReflectionMapped = &ProgSkinnedReflectionMapped;
    programs.ProgSkinnedSimplePBR = &ProgSkinnedSimplePBR;
    programs.ProgSkinnedBaseColorPBR = &ProgSkinnedBaseColorPBR;
    programs.ProgSkinnedBaseColorEmissivePBR = &ProgSkinnedBaseColorEmissivePBR;

    return programs;
}

void OvrSceneView::LoadWorldModel(
    const char* sceneFileName,
    const MaterialParms& materialParms,
    const bool fromApk) {
    ALOG("OvrSceneView::LoadScene( %s )", sceneFileName);

    if (GlPrograms.ProgSingleTexture == NULL) {
        GlPrograms = GetDefaultGLPrograms();
    }

    ModelFile* model = nullptr;
    // Load the scene we are going to draw
    if (fromApk) {
        model = LoadModelFileFromApplicationPackage(sceneFileName, GlPrograms, materialParms);
    } else {
        model = LoadModelFile(sceneFileName, GlPrograms, materialParms);
    }

    if (model == nullptr) {
        ALOGW("OvrSceneView::LoadScene( %s ) failed", sceneFileName);
        return;
    }

    SetWorldModel(*model);

    FreeWorldModelOnChange = true;
}

void OvrSceneView::LoadWorldModel(
    class ovrFileSys& fileSys,
    const char* uri,
    const MaterialParms& materialParms) {
    ALOG("OvrSceneView::LoadScene( %s )", uri);

    if (GlPrograms.ProgSingleTexture == NULL) {
        GlPrograms = GetDefaultGLPrograms();
    }

    ModelFile* model = nullptr;
    // Load the scene we are going to draw
    model = LoadModelFile(fileSys, uri, GlPrograms, materialParms);

    if (model == nullptr) {
        ALOGW("OvrSceneView::LoadScene( %s ) failed", uri);
        return;
    }

    SetWorldModel(*model);

    FreeWorldModelOnChange = true;
}

void OvrSceneView::LoadWorldModelFromApplicationPackage(
    const char* sceneFileName,
    const MaterialParms& materialParms) {
    LoadWorldModel(sceneFileName, materialParms, true);
}

void OvrSceneView::LoadWorldModel(const char* sceneFileName, const MaterialParms& materialParms) {
    LoadWorldModel(sceneFileName, materialParms, false);
}

void OvrSceneView::SetWorldModel(ModelFile& world) {
    ALOG("OvrSceneView::SetWorldModel( %s )", world.FileName.c_str());

    if (FreeWorldModelOnChange && static_cast<int>(Models.size()) > 0) {
        delete WorldModel.Definition;
        FreeWorldModelOnChange = false;
    }
    Models.clear();

    WorldModel.SetModelFile(&world);
    AddModel(&WorldModel);

    // Set the initial player position
    FootPos = Vector3f(0.0f, 0.0f, 0.0f);
    StickYaw = 0.0f;
    StickPitch = 0.0f;
    SceneYaw = 0.0f;
}

void OvrSceneView::ClearStickAngles() {
    StickYaw = 0.0f;
    StickPitch = 0.0f;
}

ovrSurfaceDef* OvrSceneView::FindNamedSurface(const char* name) const {
    return (WorldModel.Definition == NULL) ? NULL : WorldModel.Definition->FindNamedSurface(name);
}

const ModelTexture* OvrSceneView::FindNamedTexture(const char* name) const {
    return (WorldModel.Definition == NULL) ? NULL : WorldModel.Definition->FindNamedTexture(name);
}

const ModelTag* OvrSceneView::FindNamedTag(const char* name) const {
    return (WorldModel.Definition == NULL) ? NULL : WorldModel.Definition->FindNamedTag(name);
}

Bounds3f OvrSceneView::GetBounds() const {
    return (WorldModel.Definition == NULL)
        ? Bounds3f(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.0f))
        : WorldModel.Definition->GetBounds();
}

int OvrSceneView::AddModel(ModelInScene* model) {
    const int modelsSize = static_cast<int>(Models.size());

    // scan for a NULL entry
    for (int i = 0; i < modelsSize; ++i) {
        if (Models[i] == NULL) {
            Models[i] = model;
            return i;
        }
    }

    Models.push_back(model);

    return static_cast<int>(Models.size()) - 1;
}

void OvrSceneView::RemoveModelIndex(int index) {
    Models[index] = NULL;
}

void OvrSceneView::GetFrameMatrices(
    const float fovDegreesX,
    const float fovDegreesY,
    FrameMatrices& frameMatrices) const {
    frameMatrices.CenterView = GetCenterEyeViewMatrix();
    frameMatrices.EyeView[0] = GetEyeViewMatrix(0);
    frameMatrices.EyeView[1] = GetEyeViewMatrix(1);
    frameMatrices.EyeProjection[0] = GetEyeProjectionMatrix(0, fovDegreesX, fovDegreesY);
    frameMatrices.EyeProjection[1] = GetEyeProjectionMatrix(1, fovDegreesX, fovDegreesY);
}

void OvrSceneView::GenerateFrameSurfaceList(
    const FrameMatrices& frameMatrices,
    std::vector<ovrDrawSurface>& surfaceList) const {
    Matrix4f symmetricEyeProjectionMatrix = frameMatrices.EyeProjection[0];
    symmetricEyeProjectionMatrix.M[0][0] = frameMatrices.EyeProjection[0].M[0][0] /
        (fabsf(frameMatrices.EyeProjection[0].M[0][2]) + 1.0f);
    symmetricEyeProjectionMatrix.M[0][2] = 0.0f;

    const float moveBackDistance =
        0.5f * InterPupillaryDistance * symmetricEyeProjectionMatrix.M[0][0];
    Matrix4f centerEyeCullViewMatrix =
        Matrix4f::Translation(0, 0, -moveBackDistance) * frameMatrices.CenterView;

    std::vector<ModelNodeState*> emitNodes;
    for (int i = 0; i < static_cast<int>(Models.size()); i++) {
        if (Models[i] != NULL) {
            ModelState& state = Models[i]->State;
            if (state.DontRenderForClientUid == SuppressModelsWithClientId) {
                continue;
            }
            for (int j = 0; j < static_cast<int>(state.subSceneStates.size()); j++) {
                ModelSubSceneState& subSceneState = state.subSceneStates[j];
                if (subSceneState.visible) {
                    for (int k = 0; k < static_cast<int>(subSceneState.nodeStates.size()); k++) {
                        state.nodeStates[subSceneState.nodeStates[k]].AddNodesToEmitList(emitNodes);
                    }
                }
            }
        }
    }

    BuildModelSurfaceList(
        surfaceList,
        emitNodes,
        EmitSurfaces,
        centerEyeCullViewMatrix,
        symmetricEyeProjectionMatrix);
}

void OvrSceneView::SetFootPos(const Vector3f& pos, bool updateCenterEye /*= true*/) {
    FootPos = pos;
    if (updateCenterEye) {
        UpdateCenterEye();
    }
}

Vector3f OvrSceneView::GetNeutralHeadCenter() const {
    /// This works for VRAPI_TRACKING_SPACE_LOCAL_FLOOR
    return Vector3f(FootPos.x, FootPos.y, FootPos.z);

    /// This works for VRAPI_TRACKING_SPACE_LOCAL
    /// return Vector3f( FootPos.x, FootPos.y + EyeHeight, FootPos.z );
}

Vector3f OvrSceneView::GetCenterEyePosition() const {
    return Vector3f(
        CenterEyeTransform.M[0][3], CenterEyeTransform.M[1][3], CenterEyeTransform.M[2][3]);
}

Vector3f OvrSceneView::GetCenterEyeForward() const {
    return Vector3f(
        -CenterEyeViewMatrix.M[2][0], -CenterEyeViewMatrix.M[2][1], -CenterEyeViewMatrix.M[2][2]);
}

Matrix4f OvrSceneView::GetCenterEyeTransform() const {
    return CenterEyeTransform;
}

Matrix4f OvrSceneView::GetCenterEyeViewMatrix() const {
    return CenterEyeViewMatrix;
}

Matrix4f OvrSceneView::GetEyeViewMatrix(const int eye) const {
    // World space head rotation
    const Matrix4f head_rotation = Matrix4f(CurrentTracking.HeadPose.Rotation);

    // Convert the eye view to world-space and remove translation
    Matrix4f eye_view_rot = CurrentTracking.Eye[eye].ViewMatrix;
    eye_view_rot.M[0][3] = 0;
    eye_view_rot.M[1][3] = 0;
    eye_view_rot.M[2][3] = 0;
    const Matrix4f eye_rotation = eye_view_rot.Inverted();

    // Compute the rotation tranform from head to eye (in case of rotated screens)
    const Matrix4f head_rot_inv = head_rotation.Inverted();
    Matrix4f head_eye_rotation = head_rot_inv * eye_rotation;

    // Add the IPD translation from head to eye
    const float eye_shift = ((eye == 0) ? -0.5f : 0.5f) * InterPupillaryDistance;
    const Matrix4f head_eye_translation = Matrix4f::Translation(eye_shift, 0.0f, 0.0f);

    // The full transform from head to eye in world
    const Matrix4f head_eye_transform = head_eye_translation * head_eye_rotation;

    // Compute the new eye-pose using the input center eye view
    const Matrix4f center_eye_pose_m = CenterEyeViewMatrix.Inverted(); // convert to world
    const Matrix4f eye_pose_m = center_eye_pose_m * head_eye_transform;

    // Convert to view matrix
    Matrix4f eye_view = eye_pose_m.Inverted();
    return eye_view;
}

Matrix4f OvrSceneView::GetEyeProjectionMatrix(
    const int eye,
    const float fovDegreesX,
    const float fovDegreesY) const {
    // OVR_UNUSED( eye );

    // We may want to make per-eye projection matrices if we move away from nearly-centered lenses.
    // Use an infinite projection matrix because, except for things right up against the near plane,
    // it provides better precision:
    //		"Tightening the Precision of Perspective Rendering"
    //		Paul Upchurch, Mathieu Desbrun
    //		Journal of Graphics Tools, Volume 16, Issue 1, 2012
    // return ovrMatrix4f_CreateProjectionFov(fovDegreesX, fovDegreesY, 0.0f, 0.0f, Znear, 0.0f);

    // Use the incoming default projection since our headset are using asymmetric fov
    return CurrentTracking.Eye[eye].ProjectionMatrix;
}

Matrix4f OvrSceneView::GetEyeViewProjectionMatrix(
    const int eye,
    const float fovDegreesX,
    const float fovDegreesY) const {
    return GetEyeProjectionMatrix(eye, fovDegreesX, fovDegreesY) * GetEyeViewMatrix(eye);
}

float OvrSceneView::GetEyeHeight() const {
    return EyeHeight;
}

// This is called by Frame(), but it must be explicitly called when FootPos is
// updated, or calls to GetCenterEyePosition() won't reflect changes until the
// following frame.
void OvrSceneView::UpdateCenterEye() {
    Matrix4f input;
    if (YawMod > 0.0f) {
        input = Matrix4f::Translation(GetNeutralHeadCenter()) *
            Matrix4f::RotationY((StickYaw - fmodf(StickYaw, YawMod)) + SceneYaw) *
            Matrix4f::RotationX(StickPitch);
    } else {
        input = Matrix4f::Translation(GetNeutralHeadCenter()) *
            Matrix4f::RotationY(StickYaw + SceneYaw) * Matrix4f::RotationX(StickPitch);
    }

    const Matrix4f transform(CurrentTracking.HeadPose);
    CenterEyeTransform = input * transform;
    CenterEyeViewMatrix = CenterEyeTransform.Inverted();
}

inline void ApplyDeadZone(Vector2f& v, float deadZoneRadius) {
    const float radiusSquared = deadZoneRadius * deadZoneRadius;
    if (v.LengthSq() < radiusSquared) {
        v.x = 0.0f;
        v.y = 0.0f;
    }
}

void OvrSceneView::Frame(
    const ovrApplFrameIn& vrFrame,
    const long long suppressModelsWithClientId_) {
    SuppressModelsWithClientId = suppressModelsWithClientId_;
    CurrentTracking = vrFrame;
    InterPupillaryDistance = vrFrame.IPD;

    // trim height to 1m at a minimum if the reported height from the API is too low
    static const float minEyeHeight = 1.0f;
    EyeHeight = std::max(vrFrame.EyeHeight, minEyeHeight);

    static double LastPredictedDisplayTime = vrFrame.PredictedDisplayTime;

    // Delta time in seconds since last frame.
    float dt = float(vrFrame.PredictedDisplayTime - LastPredictedDisplayTime);
    const float angleSpeed = 1.5f;

    // Update this
    LastPredictedDisplayTime = vrFrame.PredictedDisplayTime;

    // Controller sticks
    Vector2f LeftStick(0.0f, 0.0f);
    Vector2f RightStick(0.0f, 0.0f);

    /// Flip Y on quest
    if (vrFrame.LeftRemoteTracked) {
        LeftStick.x += vrFrame.LeftRemoteJoystick.x;
        LeftStick.y -= vrFrame.LeftRemoteJoystick.y;
    }
    if (vrFrame.RightRemoteTracked) {
        RightStick.x += vrFrame.RightRemoteJoystick.x;
        RightStick.y -= vrFrame.RightRemoteJoystick.y;
    }

    /// Apply dead zone
    static constexpr float deadZoneRadius = 0.5f;
    ApplyDeadZone(LeftStick, deadZoneRadius);
    ApplyDeadZone(RightStick, deadZoneRadius);

    //
    // Player view angles
    //

    // Turn based on the look stick
    // Because this can be predicted ahead by async TimeWarp, we apply
    // the yaw from the previous frame's controls, trading a frame of
    // latency on stick controls to avoid a bounce-back.
    StickYaw -= YawVelocity * dt;
    if (StickYaw < 0.0f) {
        StickYaw += 2.0f * MATH_FLOAT_PI;
    } else if (StickYaw > 2.0f * MATH_FLOAT_PI) {
        StickYaw -= 2.0f * MATH_FLOAT_PI;
    }
    YawVelocity = angleSpeed * (RightStick.x);

    /*
        // Only if there is no head tracking, allow right stick up/down to adjust pitch,
        // which can be useful for debugging without having to dock the device.
        if ((vrFrame.Tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED) == 0 ||
            (vrFrame.Tracking.Status & VRAPI_TRACKING_STATUS_HMD_CONNECTED) == 0) {
            StickPitch -= angleSpeed * RightStick.y * dt;
        } else {
            StickPitch = 0.0f;
        }
    */

    // We extract Yaw, Pitch, Roll instead of directly using the orientation
    // to allow "additional" yaw manipulation with mouse/controller and scene offsets.
    const Quatf quat = vrFrame.HeadPose.Rotation;

    quat.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&EyeYaw, &EyePitch, &EyeRoll);

    // Yaw is modified by both joystick and application-set scene yaw.
    // Pitch is only modified by joystick when no head tracking sensor is active.
    if (YawMod > 0.0f) {
        EyeYaw += (StickYaw - fmodf(StickYaw, YawMod)) + SceneYaw;
    } else {
        EyeYaw += StickYaw + SceneYaw;
    }
    EyePitch += StickPitch;

    //
    // Player movement
    //

    float allSticksY = LeftStick.y + RightStick.y;
    allSticksY = std::max(-1.0f, std::min(1.0f, allSticksY));

    // Allow up / down movement if there is no floor collision model or in 'free move' mode.
    const bool upDown = (WorldModel.Definition == NULL || FreeMove);
    Vector3f gamepadMove(
        LeftStick.x, upDown ? LeftStick.y : 0.0f, upDown ? RightStick.y : allSticksY);

    // Perform player movement if there is input.
    if (gamepadMove.LengthSq() > 0.0f) {
        const Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw);
        const Vector3f orientationVector = yawRotate.Transform(gamepadMove);

        // Don't let move get too crazy fast
        const float moveDistance = std::min<float>(MoveSpeed * (float)dt, 1.0f);
        if (WorldModel.Definition != NULL && !FreeMove) {
            FootPos = SlideMove(
                FootPos,
                GetEyeHeight(),
                orientationVector,
                moveDistance,
                WorldModel.Definition->Collisions,
                WorldModel.Definition->GroundCollisions);
        } else { // no scene loaded, walk without any collisions
            ModelCollision collisionModel;
            ModelCollision groundCollisionModel;
            FootPos = SlideMove(
                FootPos,
                GetEyeHeight(),
                orientationVector,
                moveDistance,
                collisionModel,
                groundCollisionModel);
        }
    }

    //
    // Center eye transform
    //
    UpdateCenterEye();

    //
    // Model animations
    //

    if (!Paused) {
        for (int i = 0; i < static_cast<int>(Models.size()); i++) {
            if (Models[i] != NULL) {
                Models[i]->AnimateJoints(vrFrame.PredictedDisplayTime);
            }
        }
    }

    // External systems can add surfaces to this list before drawing.
    EmitSurfaces.resize(0);
}

} // namespace OVRFW
