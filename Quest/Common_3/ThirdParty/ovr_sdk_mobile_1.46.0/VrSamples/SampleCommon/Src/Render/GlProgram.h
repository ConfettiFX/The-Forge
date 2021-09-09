/************************************************************************************

Filename    :   GlProgram.h
Content     :   Shader program compilation.
Created     :   October 11, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include "Egl.h"

#include "GpuState.h"
#include "GlTexture.h"
#include "GlBuffer.h"

#include "OVR_Math.h"

#include <string>

namespace OVRFW {

class GlTexture;
class GlBuffer;

#ifdef OVR_BUILD_DEBUG
#define OVR_USE_UNIFORM_NAMES 1
#else
#define OVR_USE_UNIFORM_NAMES 0
#endif

// STRINGIZE is used so program text strings can include lines like:
// "uniform highp mat4 Joints["MAX_JOINTS_STRING"];\n"

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE(x) STRINGIZE(x)

#define MAX_JOINTS 64
#define MAX_JOINTS_STRING STRINGIZE_VALUE(MAX_JOINTS)

// No attempt is made to support sharing shaders between programs,
// it isn't worth avoiding the duplication.

// Shader uniforms Texture0 - Texture7 are bound to texture units 0 - 7

enum VertexAttributeLocation {
    VERTEX_ATTRIBUTE_LOCATION_POSITION = 0,
    VERTEX_ATTRIBUTE_LOCATION_NORMAL = 1,
    VERTEX_ATTRIBUTE_LOCATION_TANGENT = 2,
    VERTEX_ATTRIBUTE_LOCATION_BINORMAL = 3,
    VERTEX_ATTRIBUTE_LOCATION_COLOR = 4,
    VERTEX_ATTRIBUTE_LOCATION_UV0 = 5,
    VERTEX_ATTRIBUTE_LOCATION_UV1 = 6,
    VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES = 7,
    VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS = 8,
    VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS = 9
};

enum class ovrProgramParmType : char {
    INT, // int
    INT_VECTOR2, // Vector2i
    INT_VECTOR3, // Vector3i
    INT_VECTOR4, // Vector4i
    FLOAT, // float
    FLOAT_VECTOR2, // Vector2f
    FLOAT_VECTOR3, // Vector3f
    FLOAT_VECTOR4, // Vector4f
    FLOAT_MATRIX4, // Matrix4f (always inverted for GL)
    TEXTURE_SAMPLED, // GlTexture
    BUFFER_UNIFORM, // read-only uniform buffer (GLSL: uniform)
    MAX
};

struct ovrProgramParm {
    const char* Name;
    ovrProgramParmType Type;
};

struct ovrUniform {
    // can be made as high as 16
    static const int MAX_UNIFORMS = 16;

    ovrUniform() : Location(-1), Binding(-1), Type(ovrProgramParmType::MAX) {}

    int Location; // the index of the uniform in the render program
    int Binding; // the resource binding (eg. texture image unit or uniform block)
    ovrProgramParmType Type; // the type of the data
};

struct ovrUniformData {
    ovrUniformData() : Data(nullptr), Count(1) {}

    void* Data; // pointer to data
    int Count; // number of items of ovrProgramParmType in the Data buffer
};

//==============================================================
// GlProgram
// Freely copyable. In general, the compilation unit that calls Build() should
// be the compilation unit that calls Free(). Other copies of the object should
// never Free().
struct GlProgram {
    GlProgram()
        : Program(0),
          VertexShader(0),
          FragmentShader(0),
          Uniforms(),
          numTextureBindings(0),
          numUniformBufferBindings(0) {}

    static const int GLSL_PROGRAM_VERSION = 300; // Minimum requirement for multiview support.

    static GlProgram Build(
        const char* vertexSrc,
        const char* fragmentSrc,
        const ovrProgramParm* parms,
        const int numParms,
        const int programVersion = GLSL_PROGRAM_VERSION, // minimum requirement
        bool abortOnError = true);

    // Use this build variant for specifying extensions or other program directives.
    static GlProgram Build(
        const char* vertexDirectives,
        const char* vertexSrc,
        const char* fragmentDirectives,
        const char* fragmentSrc,
        const ovrProgramParm* parms,
        const int numParms,
        const int programVersion = GLSL_PROGRAM_VERSION, // minimum requirement
        bool abortOnError = true);

    static void Free(GlProgram& program);

    static void SetUseMultiview(const bool useMultiview_);

    bool IsValid() const {
        return Program != 0;
    }

    static const int MAX_VIEWS = 2;
    static const int SCENE_MATRICES_UBO_SIZE = 2 * sizeof(OVR::Matrix4f) * MAX_VIEWS;

    unsigned int Program;
    unsigned int VertexShader;
    unsigned int FragmentShader;

    // Globally-defined system level uniforms.
    ovrUniform ViewID; // uniform for ViewID; is -1 if OVR_multiview unavailable or disabled
    ovrUniform ModelMatrix; // uniform for "uniform mat4 ModelMatrix;"
    ovrUniform SceneMatrices; // uniform for "SceneMatrices" ubo :
                              // uniform SceneMatrices {
                              //   mat4 ViewMatrix[NUM_VIEWS];
                              //   mat4 ProjectionMatrix[NUM_VIEWS];
                              // } sm;

    ovrUniform Uniforms[ovrUniform::MAX_UNIFORMS];
    int numTextureBindings;
    int numUniformBufferBindings;
#if OVR_USE_UNIFORM_NAMES
    std::string UniformNames[ovrUniform::MAX_UNIFORMS];
#endif /// OVR_USE_UNIFORM_NAMES

    class MultiViewScope {
       public:
        MultiViewScope(bool enableMultView);
        ~MultiViewScope();

       private:
        bool wasEnabled;
    };
};

struct ovrGraphicsCommand {
    static const int MAX_TEXTURES = 8;

    GlProgram Program;
    ovrGpuState GpuState;
    ovrUniformData
        UniformData[ovrUniform::MAX_UNIFORMS]; // data matching the types in Program.Uniforms[]
    GlTexture Textures[ovrGraphicsCommand::MAX_TEXTURES];

    void BindUniformTextures();
};

} // namespace OVRFW
