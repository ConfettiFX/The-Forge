/************************************************************************************

Filename    :   GlProgram.cpp
Content     :   Shader program compilation.
Created     :   October 11, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "GlProgram.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Misc/Log.h"

#include "OVR_Std.h"

#include <string>

namespace OVRFW {
static bool UseMultiview = false;

GlProgram::MultiViewScope::MultiViewScope(bool enableMultView) {
    wasEnabled = UseMultiview;
    GlProgram::SetUseMultiview(enableMultView);
}
GlProgram::MultiViewScope::~MultiViewScope() {
    GlProgram::SetUseMultiview(wasEnabled);
}

// All GlPrograms implicitly get the VertexHeader
static const char* VertexHeader =
    R"glsl(
#ifndef DISABLE_MULTIVIEW
 #define DISABLE_MULTIVIEW 0
#endif
#define NUM_VIEWS 2
#define attribute in
#define varying out
#if defined( GL_OVR_multiview2 ) && ! DISABLE_MULTIVIEW
  #extension GL_OVR_multiview2 : require
  layout(num_views=NUM_VIEWS) in;
  #define VIEW_ID gl_ViewID_OVR
#else
  uniform lowp int ViewID;
  #define VIEW_ID ViewID
#endif

uniform highp mat4 ModelMatrix;

// Use a ubo in v300 path to workaround corruption issue on Adreno 420+v300
// when uniform array of matrices used.
uniform SceneMatrices
{
	highp mat4 ViewMatrix[NUM_VIEWS];
	highp mat4 ProjectionMatrix[NUM_VIEWS];
} sm;
// Use a function macro for TransformVertex to workaround an issue on exynos8890+Android-M driver:
// gl_ViewID_OVR outside of the vertex main block causes VIEW_ID to be 0 for every view.
// NOTE: Driver fix has landed with Android-N.
//highp vec4 TransformVertex( highp vec4 oPos )
//{
//	highp vec4 hPos = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * oPos ) );
//	return hPos;
//}
#define TransformVertex(localPos) (sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * localPos )))
)glsl";

// All GlPrograms implicitly get the FragmentHeader
static const char* FragmentHeader =
    R"glsl(
	#define varying in
	#define gl_FragColor fragColor
	out mediump vec4 fragColor;
	#define texture2D texture
	#define textureCube texture
)glsl";

static const char* FindShaderVersionEnd(const char* src) {
    if (src == nullptr || OVR::OVR_strncmp(src, "#version ", 9) != 0) {
        return src;
    }
    while (*src != 0 && *src != '\n') {
        src++;
    }
    if (*src == '\n') {
        src++;
    }
    return src;
}

static GLuint
CompileShader(GLenum shaderType, const char* directives, const char* src, GLint programVersion) {
    assert(programVersion >= 300);

    const char* postVersion = FindShaderVersionEnd(src);
    if (postVersion != src) {
        ALOGW("GlProgram: #version in source is not supported. Specify at program build time.");
    }

    std::string srcString;
    srcString = std::string("#version ") + std::to_string(programVersion) + std::string(" es\n");

    if (directives != NULL) {
        srcString.append(directives);
    }

    // TODO: If a c string isn't passed here, the previous contents of srcString (ie version info)
    // are corrupted. Determine why.
    srcString += std::string("#define DISABLE_MULTIVIEW ") + std::to_string(UseMultiview ? 0 : 1) +
        std::string("\n");

    if (shaderType == GL_VERTEX_SHADER) {
        srcString.append(VertexHeader);
    } else if (shaderType == GL_FRAGMENT_SHADER) {
        srcString.append(FragmentHeader);
    }

    srcString.append(postVersion);

    src = srcString.c_str();

    GLuint shader = glCreateShader(shaderType);

    const int numSources = 1;
    const char* srcs[1];
    srcs[0] = src;

    glShaderSource(shader, numSources, srcs, 0);
    glCompileShader(shader);

    GLint r;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
    if (r == GL_FALSE) {
        ALOGW(
            "Compiling %s shader: ****** failed ******\n",
            shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment");
        GLchar msg[1024];
        const char* sp = src;
        int charCount = 0;
        int line = 0;
        do {
            if (*sp != '\n') {
                msg[charCount++] = *sp;
                msg[charCount] = 0;
            }
            if (*sp == 0 || *sp == '\n' || charCount == 1023) {
                charCount = 0;
                line++;
                ALOGW("%03d  %s", line, msg);
                msg[0] = 0;
                if (*sp != '\n') {
                    line--;
                }
            }
            sp++;
        } while (*sp != 0);
        if (charCount != 0) {
            line++;
            ALOGW("%03d  %s", line, msg);
        }
        glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
        ALOGW("%s\n", msg);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GlProgram GlProgram::Build(
    const char* vertexSrc,
    const char* fragmentSrc,
    const ovrProgramParm* parms,
    const int numParms,
    const int requestedProgramVersion,
    bool abortOnError) {
    return Build(
        NULL, vertexSrc, NULL, fragmentSrc, parms, numParms, requestedProgramVersion, abortOnError);
}

GlProgram GlProgram::Build(
    const char* vertexDirectives,
    const char* vertexSrc,
    const char* fragmentDirectives,
    const char* fragmentSrc,
    const ovrProgramParm* parms,
    const int numParms,
    const int requestedProgramVersion,
    bool abortOnError) {
    GlProgram p;

    //--------------------------
    // Compile and Create the Program
    //--------------------------

    int programVersion = requestedProgramVersion;
    if (programVersion < GLSL_PROGRAM_VERSION) {
        ALOGW(
            "GlProgram: Program GLSL version requested %d, but does not meet required minimum %d",
            requestedProgramVersion,
            GLSL_PROGRAM_VERSION);
    }

    p.VertexShader = CompileShader(GL_VERTEX_SHADER, vertexDirectives, vertexSrc, programVersion);
    if (p.VertexShader == 0) {
        Free(p);
        ALOG(
            "GlProgram: CompileShader GL_VERTEX_SHADER program failed: \n```%s\n```\n\n",
            vertexSrc);
        if (abortOnError) {
            ALOGE_FAIL("Failed to compile vertex shader");
        }
        return GlProgram();
    }

    p.FragmentShader =
        CompileShader(GL_FRAGMENT_SHADER, fragmentDirectives, fragmentSrc, programVersion);
    if (p.FragmentShader == 0) {
        Free(p);
        ALOG(
            "GlProgram: CompileShader GL_FRAGMENT_SHADER program failed: \n```%s\n```\n\n",
            fragmentSrc);
        if (abortOnError) {
            ALOGE_FAIL("Failed to compile fragment shader");
        }
        return GlProgram();
    }

    p.Program = glCreateProgram();
    glAttachShader(p.Program, p.VertexShader);
    glAttachShader(p.Program, p.FragmentShader);

    //--------------------------
    // Set attributes before linking
    //--------------------------

    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_POSITION, "Position");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_NORMAL, "Normal");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_TANGENT, "Tangent");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_BINORMAL, "Binormal");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_COLOR, "VertexColor");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_UV0, "TexCoord");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_UV1, "TexCoord1");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES, "JointIndices");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS, "JointWeights");
    glBindAttribLocation(p.Program, VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS, "FontParms");

    //--------------------------
    // Link Program
    //--------------------------

    glLinkProgram(p.Program);

    GLint linkStatus;
    glGetProgramiv(p.Program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
        GLchar msg[1024];
        glGetProgramInfoLog(p.Program, sizeof(msg), 0, msg);
        Free(p);
        ALOG("GlProgram: Linking program failed: %s\n", msg);
        if (abortOnError) {
            ALOGE_FAIL("Failed to link program");
        }
        return GlProgram();
    }

    //--------------------------
    // Determine Uniform Parm Location and Binding.
    //--------------------------

    p.numTextureBindings = 0;
    p.numUniformBufferBindings = 0;

    // Globally-defined system level uniforms.
    {
        p.ViewID.Type = ovrProgramParmType::INT;
        p.ViewID.Location = glGetUniformLocation(p.Program, "ViewID");
        p.ViewID.Binding = p.ViewID.Location;

        p.SceneMatrices.Type = ovrProgramParmType::BUFFER_UNIFORM;
        p.SceneMatrices.Location = glGetUniformBlockIndex(p.Program, "SceneMatrices");
        if (p.SceneMatrices.Location >= 0) // this won't be present for v100 shaders.
        {
            p.SceneMatrices.Binding = p.numUniformBufferBindings++;
            glUniformBlockBinding(p.Program, p.SceneMatrices.Location, p.SceneMatrices.Binding);
        }

        p.ModelMatrix.Type = ovrProgramParmType::FLOAT_MATRIX4;
        p.ModelMatrix.Location = glGetUniformLocation(p.Program, "ModelMatrix");
        p.ModelMatrix.Binding = p.ModelMatrix.Location;
    }

    glUseProgram(p.Program);

    for (int i = 0; i < numParms; ++i) {
        OVR_ASSERT(parms[i].Type != ovrProgramParmType::MAX);

        /// Trace the names of the uniform for this progam
#if OVR_USE_UNIFORM_NAMES
        p.UniformNames[i] = std::string(parms[i].Name);
        ALOG(
            "   GlProgram[ %d ]: Uniforms[%d] = `%s` %s",
            p.Program,
            i,
            parms[i].Name,
            parms[i].Type == ovrProgramParmType::TEXTURE_SAMPLED ? "Texture" : "");
#endif /// OVR_USE_UNIFORM_NAMES

        p.Uniforms[i].Type = parms[i].Type;
        if (parms[i].Type == ovrProgramParmType::TEXTURE_SAMPLED) {
            p.Uniforms[i].Location =
                static_cast<int16_t>(glGetUniformLocation(p.Program, parms[i].Name));
            p.Uniforms[i].Binding = p.numTextureBindings++;
            glUniform1i(p.Uniforms[i].Location, p.Uniforms[i].Binding);
        } else if (parms[i].Type == ovrProgramParmType::BUFFER_UNIFORM) {
            p.Uniforms[i].Location = glGetUniformBlockIndex(p.Program, parms[i].Name);
            p.Uniforms[i].Binding = p.numUniformBufferBindings++;
            glUniformBlockBinding(p.Program, p.Uniforms[i].Location, p.Uniforms[i].Binding);
        } else {
            p.Uniforms[i].Location =
                static_cast<int16_t>(glGetUniformLocation(p.Program, parms[i].Name));
            p.Uniforms[i].Binding = p.Uniforms[i].Location;
        }
        if (false == (p.Uniforms[i].Location >= 0 && p.Uniforms[i].Binding >= 0)) {
#if OVR_USE_UNIFORM_NAMES
            p.UniformNames[i] = std::string(parms[i].Name);
            ALOGW(
                "   GlProgram[ %d ]: Uniforms[%d] = `%s` %s NOT BOUND / USED",
                p.Program,
                i,
                parms[i].Name,
                parms[i].Type == ovrProgramParmType::TEXTURE_SAMPLED ? "Texture" : "");
#else
            ALOGW("   GlProgram[ %d ]: Uniforms[%d] NOT BOUND / USED", p.Program, i);
#endif /// OVR_USE_UNIFORM_NAMES
        }

        OVR_ASSERT(p.Uniforms[i].Location >= 0 && p.Uniforms[i].Binding >= 0);
    }

    glUseProgram(0);

    return p;
}

void GlProgram::Free(GlProgram& prog) {
    glUseProgram(0);
    if (prog.Program != 0) {
        glDeleteProgram(prog.Program);
    }
    if (prog.VertexShader != 0) {
        glDeleteShader(prog.VertexShader);
    }
    if (prog.FragmentShader != 0) {
        glDeleteShader(prog.FragmentShader);
    }
    prog.Program = 0;
    prog.VertexShader = 0;
    prog.FragmentShader = 0;
}

void GlProgram::SetUseMultiview(const bool useMultiview_) {
    UseMultiview = useMultiview_;
}

void ovrGraphicsCommand::BindUniformTextures() {
    /// Late bind Textures to the right texture objects
    for (int i = 0; i < ovrUniform::MAX_UNIFORMS; ++i) {
        const ovrUniform& uniform = Program.Uniforms[i];
        if (uniform.Type == ovrProgramParmType::TEXTURE_SAMPLED) {
            UniformData[i].Data = &Textures[uniform.Binding];
        }
    }
}

} // namespace OVRFW
