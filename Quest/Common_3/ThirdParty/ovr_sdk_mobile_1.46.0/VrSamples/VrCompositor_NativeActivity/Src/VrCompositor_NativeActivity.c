/************************************************************************************

Filename    :   VrCompositor_NativeActivity.c
Content     :   This sample uses the Android NativeActivity class. This sample does
                not use the application framework.
                This sample only uses the VrApi.
Created     :   March, 2016
Authors     :   J.M.P. van Waveren, Gloria Kennickell

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

/*
This example demonstrates the use of the new compositor layer types: cylinder,
cubemap, and equirect.

Compositor layers provide advantages such as increased resolution and better sampling
qualities over rendering to the eye buffers. Layers are composited strictly in order,
with no depth interaction, so it is the applications responsibility to not place the
layers such that they would be occluded by any world geometry, which can result in
eye straining depth paradoxes.

Layer rendering may break down at extreme grazing angles, and as such, they should be
faded out or converted to normal surfaces which render to the eye buffers if the viewer
can get too close.

Layers are never back-face culled.

All texture levels must have a 0 alpha border to avoid edge smear.
*/

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
#endif

// GL_EXT_texture_cube_map_array
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#endif

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define DEBUG 1
#define OVR_LOG_TAG "VrCompositor"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

typedef struct {
    bool EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool EXT_sRGB_write_control;
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions;

static void EglInitExtensions() {
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != NULL) {
        glExtensions.EXT_texture_border_clamp =
            strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
            strstr(allExtensions, "GL_OES_texture_border_clamp");
        glExtensions.EXT_sRGB_write_control = strstr(allExtensions, "GL_EXT_sRGB_write_control");
    }
}

static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

static const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown";
    }
}

static void GLCheckErrors(int line) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ALOGE("GL error on line %d: %s", line, GlErrorString(error));
    }
}

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

static void ovrEgl_Clear(ovrEgl* egl) {
    egl->MajorVersion = 0;
    egl->MinorVersion = 0;
    egl->Display = 0;
    egl->Config = 0;
    egl->TinySurface = EGL_NO_SURFACE;
    egl->MainSurface = EGL_NO_SURFACE;
    egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
    if (egl->Display != 0) {
        return;
    }

    egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8, // need alpha for the multi-pass timewarp compositor
        EGL_DEPTH_SIZE,
        0,
        EGL_STENCIL_SIZE,
        0,
        EGL_SAMPLES,
        0,
        EGL_NONE};
    egl->Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            egl->Config = configs[i];
            break;
        }
    }
    if (egl->Config == 0) {
        ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    egl->Context = eglCreateContext(
        egl->Display,
        egl->Config,
        (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT,
        contextAttribs);
    if (egl->Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
    if (egl->TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) ==
        EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(egl->Display, egl->TinySurface);
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
}

static void ovrEgl_DestroyContext(ovrEgl* egl) {
    if (egl->Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (egl->Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Context = EGL_NO_CONTEXT;
    }
    if (egl->TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        egl->TinySurface = EGL_NO_SURFACE;
    }
    if (egl->Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(egl->Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Display = 0;
    }
}

/*
================================================================================

ovrGeometry

================================================================================
*/

typedef struct {
    GLint Index;
    GLint Size;
    GLenum Type;
    GLboolean Normalized;
    GLsizei Stride;
    const GLvoid* Pointer;
} ovrVertexAttribPointer;

#define MAX_VERTEX_ATTRIB_POINTERS 3

typedef struct {
    GLuint VertexBuffer;
    GLuint IndexBuffer;
    GLuint VertexArrayObject;
    int VertexCount;
    int IndexCount;
    ovrVertexAttribPointer VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
} ovrGeometry;

enum VertexAttributeLocation {
    VERTEX_ATTRIBUTE_LOCATION_POSITION,
    VERTEX_ATTRIBUTE_LOCATION_COLOR,
    VERTEX_ATTRIBUTE_LOCATION_UV,
};

typedef struct {
    enum VertexAttributeLocation location;
    const char* name;
} ovrVertexAttribute;

static ovrVertexAttribute ProgramVertexAttributes[] = {
    {VERTEX_ATTRIBUTE_LOCATION_POSITION, "vertexPosition"},
    {VERTEX_ATTRIBUTE_LOCATION_COLOR, "vertexColor"},
    {VERTEX_ATTRIBUTE_LOCATION_UV, "vertexUv"},
};

static void ovrGeometry_Clear(ovrGeometry* geometry) {
    geometry->VertexBuffer = 0;
    geometry->IndexBuffer = 0;
    geometry->VertexArrayObject = 0;
    geometry->VertexCount = 0;
    geometry->IndexCount = 0;
    for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++) {
        memset(&geometry->VertexAttribs[i], 0, sizeof(geometry->VertexAttribs[i]));
        geometry->VertexAttribs[i].Index = -1;
    }
}

static void ovrGeometry_CreateGroundPlane(ovrGeometry* geometry) {
    typedef struct {
        float positions[4][4];
        unsigned char colors[4][4];
    } ovrCubeVertices;

    static const ovrCubeVertices cubeVertices = {
        // positions
        {{4.5f, -1.2f, 4.5f, 1.0f},
         {4.5f, -1.2f, -4.5f, 1.0f},
         {-4.5f, -1.2f, -4.5f, 1.0f},
         {-4.5f, -1.2f, 4.5f, 1.0f}},
        // colors
        {
            {255, 0, 0, 255},
            {0, 255, 0, 255},
            {0, 0, 255, 255},
            {255, 255, 0, 255},
        },
    };

    static const unsigned short cubeIndices[6] = {
        0,
        1,
        2,
        0,
        2,
        3,
    };

    geometry->VertexCount = 4;
    geometry->IndexCount = 6;

    geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    geometry->VertexAttribs[0].Size = 4;
    geometry->VertexAttribs[0].Type = GL_FLOAT;
    geometry->VertexAttribs[0].Normalized = false;
    geometry->VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
    geometry->VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, positions);

    geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    geometry->VertexAttribs[1].Size = 4;
    geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    geometry->VertexAttribs[1].Normalized = true;
    geometry->VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
    geometry->VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, colors);

    GL(glGenBuffers(1, &geometry->VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &geometry->IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

static void ovrGeometry_Destroy(ovrGeometry* geometry) {
    GL(glDeleteBuffers(1, &geometry->IndexBuffer));
    GL(glDeleteBuffers(1, &geometry->VertexBuffer));

    ovrGeometry_Clear(geometry);
}

static void ovrGeometry_CreateVAO(ovrGeometry* geometry) {
    GL(glGenVertexArrays(1, &geometry->VertexArrayObject));
    GL(glBindVertexArray(geometry->VertexArrayObject));

    GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));

    for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++) {
        if (geometry->VertexAttribs[i].Index != -1) {
            GL(glEnableVertexAttribArray(geometry->VertexAttribs[i].Index));
            GL(glVertexAttribPointer(
                geometry->VertexAttribs[i].Index,
                geometry->VertexAttribs[i].Size,
                geometry->VertexAttribs[i].Type,
                geometry->VertexAttribs[i].Normalized,
                geometry->VertexAttribs[i].Stride,
                geometry->VertexAttribs[i].Pointer));
        }
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));

    GL(glBindVertexArray(0));
}

static void ovrGeometry_DestroyVAO(ovrGeometry* geometry) {
    GL(glDeleteVertexArrays(1, &geometry->VertexArrayObject));
}

/*
================================================================================

ovrProgram

================================================================================
*/

#define MAX_PROGRAM_UNIFORMS 8
#define MAX_PROGRAM_TEXTURES 8

typedef struct {
    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i
} ovrProgram;

typedef struct {
    enum {
        UNIFORM_VIEW_PROJ_MATRIX,
    } index;
    enum {
        UNIFORM_TYPE_VECTOR4,
        UNIFORM_TYPE_MATRIX4X4,
        UNIFORM_TYPE_INT,
        UNIFORM_TYPE_BUFFER,
    } type;
    const char* name;
} ovrUniform;

static ovrUniform ProgramUniforms[] = {
    {UNIFORM_VIEW_PROJ_MATRIX, UNIFORM_TYPE_MATRIX4X4, "viewProjectionMatrix"},
};

static void ovrProgram_Clear(ovrProgram* program) {
    program->Program = 0;
    program->VertexShader = 0;
    program->FragmentShader = 0;
    memset(program->UniformLocation, 0, sizeof(program->UniformLocation));
    memset(program->UniformBinding, 0, sizeof(program->UniformBinding));
    memset(program->Textures, 0, sizeof(program->Textures));
}

static bool
ovrProgram_Create(ovrProgram* program, const char* vertexSource, const char* fragmentSource) {
    GLint r;

    GL(program->VertexShader = glCreateShader(GL_VERTEX_SHADER));

    GL(glShaderSource(program->VertexShader, 1, &vertexSource, 0));
    GL(glCompileShader(program->VertexShader));
    GL(glGetShaderiv(program->VertexShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->VertexShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", vertexSource, msg);
        return false;
    }

    GL(program->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(program->FragmentShader, 1, &fragmentSource, 0));
    GL(glCompileShader(program->FragmentShader));
    GL(glGetShaderiv(program->FragmentShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->FragmentShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", fragmentSource, msg);
        return false;
    }

    GL(program->Program = glCreateProgram());
    GL(glAttachShader(program->Program, program->VertexShader));
    GL(glAttachShader(program->Program, program->FragmentShader));

    // Bind the vertex attribute locations.
    for (int i = 0; i < sizeof(ProgramVertexAttributes) / sizeof(ProgramVertexAttributes[0]); i++) {
        GL(glBindAttribLocation(
            program->Program,
            ProgramVertexAttributes[i].location,
            ProgramVertexAttributes[i].name));
    }

    GL(glLinkProgram(program->Program));
    GL(glGetProgramiv(program->Program, GL_LINK_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetProgramInfoLog(program->Program, sizeof(msg), 0, msg));
        ALOGE("Linking program failed: %s\n", msg);
        return false;
    }

    int numBufferBindings = 0;

    // Get the uniform locations.
    memset(program->UniformLocation, -1, sizeof(program->UniformLocation));
    for (int i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++) {
        const int uniformIndex = ProgramUniforms[i].index;
        if (ProgramUniforms[i].type == UNIFORM_TYPE_BUFFER) {
            GL(program->UniformLocation[uniformIndex] =
                   glGetUniformBlockIndex(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = numBufferBindings++;
            GL(glUniformBlockBinding(
                program->Program,
                program->UniformLocation[uniformIndex],
                program->UniformBinding[uniformIndex]));
        } else {
            GL(program->UniformLocation[uniformIndex] =
                   glGetUniformLocation(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = program->UniformLocation[uniformIndex];
        }
    }

    GL(glUseProgram(program->Program));

    // Get the texture locations.
    for (int i = 0; i < MAX_PROGRAM_TEXTURES; i++) {
        char name[32];
        sprintf(name, "Texture%i", i);
        program->Textures[i] = glGetUniformLocation(program->Program, name);
        if (program->Textures[i] != -1) {
            GL(glUniform1i(program->Textures[i], i));
        }
    }

    GL(glUseProgram(0));

    return true;
}

static void ovrProgram_Destroy(ovrProgram* program) {
    if (program->Program != 0) {
        GL(glDeleteProgram(program->Program));
        program->Program = 0;
    }
    if (program->VertexShader != 0) {
        GL(glDeleteShader(program->VertexShader));
        program->VertexShader = 0;
    }
    if (program->FragmentShader != 0) {
        GL(glDeleteShader(program->FragmentShader));
        program->FragmentShader = 0;
    }
}

static const char VERTEX_SHADER[] =
    "#version 300 es\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "uniform mat4 viewProjectionMatrix;\n"
    "out vec4 fragmentColor;\n"
    "void main()\n"
    "{\n"
    "	gl_Position = viewProjectionMatrix * vec4( vertexPosition, 1.0 );\n"
    "	fragmentColor = vertexColor;\n"
    "}\n";

static const char FRAGMENT_SHADER[] =
    "#version 300 es\n"
    "in lowp vec4 fragmentColor;\n"
    "out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "	outColor = fragmentColor;\n"
    "}\n";

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    int Multisamples;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    ovrTextureSwapChain* ColorTextureSwapChain;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->Multisamples = 0;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->ColorTextureSwapChain = NULL;
    frameBuffer->DepthBuffers = NULL;
    frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create(
    ovrFramebuffer* frameBuffer,
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples) {
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
        (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
            "glRenderbufferStorageMultisampleEXT");
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
        (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
            "glFramebufferTexture2DMultisampleEXT");

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->Multisamples = multisamples;

    frameBuffer->ColorTextureSwapChain =
        vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, 3);
    frameBuffer->TextureSwapChainLength =
        vrapi_GetTextureSwapChainLength(frameBuffer->ColorTextureSwapChain);
    frameBuffer->DepthBuffers =
        (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
    frameBuffer->FrameBuffers =
        (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the color buffer texture.
        const GLuint colorTexture =
            vrapi_GetTextureSwapChainHandle(frameBuffer->ColorTextureSwapChain, i);
        GLenum colorTextureTarget = GL_TEXTURE_2D;

        GL(glBindTexture(colorTextureTarget, colorTexture));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        GL(glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(colorTextureTarget, 0));

        if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL &&
            glFramebufferTexture2DMultisampleEXT != NULL) {
            // Create multisampled depth buffer.
            GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
            GL(glRenderbufferStorageMultisampleEXT(
                GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            // Create the frame buffer.
            // NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            GL(glFramebufferTexture2DMultisampleEXT(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                colorTexture,
                0,
                multisamples));
            GL(glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER,
                frameBuffer->DepthBuffers[i]));
            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                    "Incomplete frame buffer object: %s",
                    GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        } else {
            // Create depth buffer.
            GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
            GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            // Create the frame buffer.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            GL(glFramebufferRenderbuffer(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER,
                frameBuffer->DepthBuffers[i]));
            GL(glFramebufferTexture2D(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                    "Incomplete frame buffer object: %s",
                    GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        }
    }

    return true;
}

static void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
    GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
    GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
    vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

    free(frameBuffer->DepthBuffers);
    free(frameBuffer->FrameBuffers);

    ovrFramebuffer_Clear(frameBuffer);
}

static void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
    GL(glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]));
}

static void ovrFramebuffer_SetNone() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

static void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

static void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer) {
    // Advance to the next texture from the set.
    frameBuffer->TextureSwapChainIndex =
        (frameBuffer->TextureSwapChainIndex + 1) % frameBuffer->TextureSwapChainLength;
}

/*
================================================================================

ovrTextureSwapChain

================================================================================
*/

static int RoundUpToPow2(int i) {
    i--;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    i++;
    return i;
}

static int IntegerLog2(int i) {
    int r = 0;
    int t;
    t = ((~((i >> 16) + ~0U)) >> 27) & 0x10;
    r |= t;
    i >>= t;
    t = ((~((i >> 8) + ~0U)) >> 28) & 0x08;
    r |= t;
    i >>= t;
    t = ((~((i >> 4) + ~0U)) >> 29) & 0x04;
    r |= t;
    i >>= t;
    t = ((~((i >> 2) + ~0U)) >> 30) & 0x02;
    r |= t;
    i >>= t;
    return (r | (i >> 1));
}

// Set 'numberOfMipmapLevels' to -1 to allocate the full mip chain.
// If 'numberOfMipmapLevels' is -1 and 'data' != NULL, then the full mip chain will be generated
// from the finest data level.
static ovrTextureSwapChain* ovrTextureSwapChain_Create(
    const char* fileName,
    const int width,
    const int height,
    const int depth,
    const int numberOfArrayElements,
    const int numberOfFaces,
    const int numberOfMipmapLevels,
    const GLenum glInternalFormat,
    const void* data,
    const size_t dataSize,
    const bool mipSizeStored) {
    if (width <= 0 || width > 32768 || height <= 0 || height > 32768) {
        ALOGE("%s: Invalid texture size (%dx%d)", fileName, width, height);
        return NULL;
    }

    if (numberOfFaces > 1 && numberOfFaces != 6) {
        ALOGE("%s: Cube maps must have 6 faces (%d)", fileName, numberOfFaces);
        return NULL;
    }

    if (numberOfFaces > 1 && width != height) {
        ALOGE("%s: Cube maps must be square (%dx%d)", fileName, width, height);
        return NULL;
    }

    if (depth > 1 || numberOfArrayElements > 1) {
        ALOGE("%s: 3D and array textures are not supported", fileName);
        return NULL;
    }

    if (glInternalFormat != GL_RGBA8) {
        ALOGE("%s: only RGBA8 is supported", fileName);
        return NULL;
    }

    const GLenum glTarget =
        ((depth > 1) ? GL_TEXTURE_3D
                     : ((numberOfFaces > 1)
                            ? ((numberOfArrayElements > 1) ? GL_TEXTURE_CUBE_MAP_ARRAY
                                                           : GL_TEXTURE_CUBE_MAP)
                            : ((numberOfArrayElements > 1) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D)));
    const int numStorageLevels = (numberOfMipmapLevels >= 1)
        ? numberOfMipmapLevels
        : IntegerLog2(RoundUpToPow2(width > height ? width : height));

    GL(glBindTexture(glTarget, 0));

    ovrTextureSwapChain* chain = NULL;
    if (numberOfFaces <= 1) {
        chain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_2D, GL_SRGB8_ALPHA8, width, height, numStorageLevels, 1);
    } else {
        chain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_CUBE, GL_SRGB8_ALPHA8, width, height, numStorageLevels, 1);
    }

    GLuint id = vrapi_GetTextureSwapChainHandle(chain, 0);

    GL(glBindTexture(glTarget, id));

    if (data != NULL) {
        const int numDataLevels = (numberOfMipmapLevels >= 1) ? numberOfMipmapLevels : 1;
        const unsigned char* levelData = (const unsigned char*)data;
        const unsigned char* endOfBuffer = levelData + dataSize;

        for (int i = 0; i < numDataLevels; i++) {
            const int w = (width >> i) >= 1 ? (width >> i) : 1;
            const int h = (height >> i) >= 1 ? (height >> i) : 1;
            const int d = (depth >> i) >= 1 ? (depth >> i) : 1;

            size_t mipSize = 0;
            bool compressed = false;
            GLenum glFormat = GL_RGBA;
            GLenum glDataType = GL_UNSIGNED_BYTE;
            switch (glInternalFormat) {
                case GL_R8: {
                    mipSize = w * h * d * 1 * sizeof(unsigned char);
                    glFormat = GL_RED;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RG8: {
                    mipSize = w * h * d * 2 * sizeof(unsigned char);
                    glFormat = GL_RG;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RGB8: {
                    mipSize = w * h * d * 3 * sizeof(unsigned char);
                    glFormat = GL_RGB;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
                case GL_RGBA8: {
                    mipSize = w * h * d * 4 * sizeof(unsigned char);
                    glFormat = GL_RGBA;
                    glDataType = GL_UNSIGNED_BYTE;
                    break;
                }
            }
            if (mipSize == 0) {
                ALOGE("%s: Unsupported image format %d", fileName, glInternalFormat);
                GL(glBindTexture(glTarget, 0));
                return chain;
            }

            if (numberOfArrayElements > 1) {
                mipSize = mipSize * numberOfArrayElements * numberOfFaces;
            }
            ALOGV("mipSize%d = %zu (%d)", i, mipSize, numDataLevels);

            if (mipSizeStored) {
                if (levelData + 4 > endOfBuffer) {
                    ALOGE("%s: Image data exceeds buffer size", fileName);
                    GL(glBindTexture(glTarget, 0));
                    return chain;
                }
                const size_t storedMipSize = (size_t) * (const unsigned int*)levelData;
                ALOGV("storedMipSize = %zu", storedMipSize);
                mipSize = storedMipSize;
                levelData += 4;
            }

            if (depth <= 1 && numberOfArrayElements <= 1) {
                for (int face = 0; face < numberOfFaces; face++) {
                    if (mipSize <= 0 || mipSize > (size_t)(endOfBuffer - levelData)) {
                        ALOGE(
                            "%s: Mip %d data exceeds buffer size (%zu > %zu)",
                            fileName,
                            i,
                            mipSize,
                            (endOfBuffer - levelData));
                        GL(glBindTexture(glTarget, 0));
                        return chain;
                    }

                    const GLenum uploadTarget = (glTarget == GL_TEXTURE_CUBE_MAP)
                        ? GL_TEXTURE_CUBE_MAP_POSITIVE_X
                        : GL_TEXTURE_2D;
                    if (compressed) {
                        GL(glCompressedTexSubImage2D(
                            uploadTarget + face,
                            i,
                            0,
                            0,
                            w,
                            h,
                            glInternalFormat,
                            (GLsizei)mipSize,
                            levelData));
                    } else {
#if 0
						if ( w > 32 && h > 32 )
						{
							uint32_t * ld = (uint32_t *)levelData;
							ld += ( h / 4 - 2 ) * w;
							// These lines are drawn at a constant t=0.25, from s=0 to s=0.5...
							// The nature of cube image layout in GL means that:
							//   The left, right, forward, and back image all appear to be flipped in both s and t.
							//   The up and down images do not appear to be flipped in either s or t (assuming you look
							//   at them from a forward facing pose).
							//                 pos x       neg x       pos y       neg y       pos z       neg z
							//                 right       left        up          down        back        fwd
							//                 red         cyan        green       magenta     blue        yellow
							uint32_t c[] = { 0xff0000ff, 0xffffff00, 0xff00ff00, 0xffff00ff, 0xffff0000, 0xff00ffff };
							for ( int j = 0; j < 4; j++ )
							{
								for ( int i = 0; i < w / 2; i++ )
								{
									ld[ i ] = c[ face ];
								}
								ld += w;
							}
						}
#endif
                        GL(glTexSubImage2D(
                            uploadTarget + face, i, 0, 0, w, h, glFormat, glDataType, levelData));
                    }

                    levelData += mipSize;

                    if (mipSizeStored) {
                        levelData += 3 - ((mipSize + 3) % 4);
                        if (levelData > endOfBuffer) {
                            ALOGE("%s: Image data exceeds buffer size", fileName);
                            GL(glBindTexture(glTarget, 0));
                            return chain;
                        }
                    }
                }
            } else {
                if (mipSize <= 0 || mipSize > (size_t)(endOfBuffer - levelData)) {
                    ALOGE(
                        "%s: Mip %d data exceeds buffer size (%zu > %zu)",
                        fileName,
                        i,
                        mipSize,
                        (endOfBuffer - levelData));
                    GL(glBindTexture(glTarget, 0));
                    return chain;
                }

                if (compressed) {
                    GL(glCompressedTexSubImage3D(
                        glTarget,
                        i,
                        0,
                        0,
                        0,
                        w,
                        h,
                        d * numberOfArrayElements,
                        glInternalFormat,
                        (GLsizei)mipSize,
                        levelData));
                } else {
                    GL(glTexSubImage3D(
                        glTarget,
                        i,
                        0,
                        0,
                        0,
                        w,
                        h,
                        d * numberOfArrayElements,
                        glFormat,
                        glDataType,
                        levelData));
                }

                levelData += mipSize;

                if (mipSizeStored) {
                    levelData += 3 - ((mipSize + 3) % 4);
                    if (levelData > endOfBuffer) {
                        ALOGE("%s: Image data exceeds buffer size", fileName);
                        GL(glBindTexture(glTarget, 0));
                        return chain;
                    }
                }
            }
        }

        if (numberOfMipmapLevels <= 0) {
            GL(glGenerateMipmap(glTarget));
        }
    }

    GL(glTexParameteri(
        glTarget,
        GL_TEXTURE_MIN_FILTER,
        (numStorageLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
    GL(glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL(glBindTexture(glTarget, 0));

    return chain;
}

ovrTextureSwapChain* ovrTextureSwapChain_CreateFromKTX(const char* fileName) {
#pragma pack(1)
    typedef struct {
        unsigned char identifier[12];
        unsigned int endianness;
        unsigned int glType;
        unsigned int glTypeSize;
        unsigned int glFormat;
        unsigned int glInternalFormat;
        unsigned int glBaseInternalFormat;
        unsigned int pixelWidth;
        unsigned int pixelHeight;
        unsigned int pixelDepth;
        unsigned int numberOfArrayElements;
        unsigned int numberOfFaces;
        unsigned int numberOfMipmapLevels;
        unsigned int bytesOfKeyValueData;
    } GlHeaderKTX_t;
#pragma pack()

    FILE* fp = fopen(fileName, "rb");
    if (fp == NULL) {
        ALOGE("Failed to open %s", fileName);
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    size_t bufferSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(bufferSize);
    if (fread(buffer, 1, bufferSize, fp) != bufferSize) {
        ALOGE("Failed to read %s", fileName);
        free(buffer);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    if (bufferSize < sizeof(GlHeaderKTX_t)) {
        ALOGE("%s: Invalid KTX file", fileName);
        free(buffer);
        return NULL;
    }

    const unsigned char fileIdentifier[12] = {
        (unsigned char)'\xAB',
        'K',
        'T',
        'X',
        ' ',
        '1',
        '1',
        (unsigned char)'\xBB',
        '\r',
        '\n',
        '\x1A',
        '\n'};

    const GlHeaderKTX_t* header = (GlHeaderKTX_t*)buffer;
    if (memcmp(header->identifier, fileIdentifier, sizeof(fileIdentifier)) != 0) {
        ALOGE("%s: Invalid KTX file", fileName);
        free(buffer);
        return NULL;
    }
    // only support little endian
    if (header->endianness != 0x04030201) {
        ALOGE("%s: KTX file has wrong endianess", fileName);
        free(buffer);
        return NULL;
    }
    // only support unsigned byte
    if (header->glType != GL_UNSIGNED_BYTE) {
        ALOGE("%s: KTX file has unsupported glType %d", fileName, header->glType);
        free(buffer);
        return NULL;
    }
    // skip the key value data
    const size_t startTex = sizeof(GlHeaderKTX_t) + header->bytesOfKeyValueData;
    if ((startTex < sizeof(GlHeaderKTX_t)) || (startTex >= bufferSize)) {
        ALOGE("%s: Invalid KTX header sizes", fileName);
        free(buffer);
        return NULL;
    }

    const int numberOfArrayElements =
        (header->numberOfArrayElements >= 1) ? header->numberOfArrayElements : 1;
    const int numberOfFaces = (header->numberOfFaces >= 1) ? header->numberOfFaces : 1;
    const int numberOfMipmapLevels = header->numberOfMipmapLevels;

    ovrTextureSwapChain* chain = ovrTextureSwapChain_Create(
        fileName,
        header->pixelWidth,
        header->pixelHeight,
        header->pixelDepth,
        numberOfArrayElements,
        numberOfFaces,
        numberOfMipmapLevels,
        header->glInternalFormat,
        buffer + startTex,
        bufferSize - startTex,
        true);

    free(buffer);
    return chain;
}

void ovrTextureSwapChain_Destroy(ovrTextureSwapChain* chain) {
    vrapi_DestroyTextureSwapChain(chain);
}

/*
================================================================================

ovrScene

================================================================================
*/

enum ovrBackgroundType {
    BACKGROUND_NONE,
    BACKGROUND_CUBEMAP,
    BACKGROUND_EQUIRECT,
    BACKGROUND_LOCAL_EQUIRECT,
    MAX_BACKGROUND_TYPES
};

typedef struct {
    bool CreatedScene;
    bool CreatedVAOs;
    ovrProgram Program;
    ovrGeometry GroundPlane;

    ovrTextureSwapChain* CubeMapSwapChain;
    ovrTextureSwapChain* EquirectSwapChain;
    ovrTextureSwapChain* CylinderSwapChain;

    int CylinderWidth;
    int CylinderHeight;

    enum ovrBackgroundType BackGroundType;
} ovrScene;

static void ovrScene_Clear(ovrScene* scene) {
    scene->CreatedScene = false;
    scene->CreatedVAOs = false;
    ovrProgram_Clear(&scene->Program);
    ovrGeometry_Clear(&scene->GroundPlane);

    scene->CubeMapSwapChain = NULL;
    scene->EquirectSwapChain = NULL;
    scene->CylinderSwapChain = NULL;
    scene->CylinderWidth = 0;
    scene->CylinderHeight = 0;

    scene->BackGroundType = BACKGROUND_NONE;
}

static bool ovrScene_IsCreated(ovrScene* scene) {
    return scene->CreatedScene;
}

static void ovrScene_CreateVAOs(ovrScene* scene) {
    if (!scene->CreatedVAOs) {
        ovrGeometry_CreateVAO(&scene->GroundPlane);
        scene->CreatedVAOs = true;
    }
}

static void ovrScene_DestroyVAOs(ovrScene* scene) {
    if (scene->CreatedVAOs) {
        ovrGeometry_DestroyVAO(&scene->GroundPlane);
        scene->CreatedVAOs = false;
    }
}

static void ovrScene_Create(ovrScene* scene, bool permissionsGranted) {
    // Simple ground plane geometry.
    {
        ovrProgram_Create(&scene->Program, VERTEX_SHADER, FRAGMENT_SHADER);
        ovrGeometry_CreateGroundPlane(&scene->GroundPlane);
        ovrScene_CreateVAOs(scene);
    }

    // Simple cubemap loaded from ktx file on the sdcard.
    // Verify sdcard read permission granted before accessing.
    if (permissionsGranted) {
        scene->CubeMapSwapChain =
            ovrTextureSwapChain_CreateFromKTX("/sdcard/oculus/cubemap1536.ktx");
    }

    // Simple checkerboard pattern.
    {
        const int width = 512;
        const int height = 256;
        scene->EquirectSwapChain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_2D, GL_SRGB8_ALPHA8, width, height, 1, 1);

        uint32_t* texData = (uint32_t*)malloc(width * height * sizeof(uint32_t));

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                texData[y * width + x] = (x ^ y) & 16 ? 0xFFFFFFFF : 0xFF000000;
            }
        }

        const int texId = vrapi_GetTextureSwapChainHandle(scene->EquirectSwapChain, 0);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        glBindTexture(GL_TEXTURE_2D, 0);
        free(texData);
    }

    // Simple checkerboard pattern.
    {
        scene->CylinderWidth = 512;
        scene->CylinderHeight = 128;
        scene->CylinderSwapChain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_2D,
            GL_SRGB8_ALPHA8,
            scene->CylinderWidth,
            scene->CylinderHeight,
            1,
            1);

        uint32_t* texData =
            (uint32_t*)malloc(scene->CylinderWidth * scene->CylinderHeight * sizeof(uint32_t));

        for (int y = 0; y < scene->CylinderHeight; y++) {
            for (int x = 0; x < scene->CylinderWidth; x++) {
                texData[y * scene->CylinderWidth + x] = (x ^ y) & 64 ? 0xFF6464F0 : 0xFFF06464;
            }
        }

        const int texId = vrapi_GetTextureSwapChainHandle(scene->CylinderSwapChain, 0);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            scene->CylinderWidth,
            scene->CylinderHeight,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            texData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindTexture(GL_TEXTURE_2D, 0);

        free(texData);
    }

    scene->CreatedScene = true;
}

static void ovrScene_Destroy(ovrScene* scene) {
    ovrScene_DestroyVAOs(scene);
    ovrProgram_Destroy(&scene->Program);
    ovrGeometry_Destroy(&scene->GroundPlane);

    ovrTextureSwapChain_Destroy(scene->CubeMapSwapChain);
    ovrTextureSwapChain_Destroy(scene->EquirectSwapChain);
    ovrTextureSwapChain_Destroy(scene->CylinderSwapChain);

    scene->CreatedScene = false;
}

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct {
    ovrFramebuffer FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrRenderer;

static void ovrRenderer_Clear(ovrRenderer* renderer) {
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
    }
}

static void ovrRenderer_Create(ovrRenderer* renderer, const ovrJava* java) {
    // Create the frame buffers.
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer_Create(
            &renderer->FrameBuffer[eye],
            GL_SRGB8_ALPHA8,
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
            NUM_MULTI_SAMPLES);
    }
}

static void ovrRenderer_Destroy(ovrRenderer* renderer) {
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
    }
}

static ovrLayerProjection2 ovrRenderer_RenderSceneToEyeBuffer(
    ovrRenderer* renderer,
    const ovrJava* java,
    const ovrScene* scene,
    const ovrTracking2* tracking) {
    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = tracking->HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
        layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles =
            ovrMatrix4f_TanAngleMatrixFromProjection(&tracking->Eye[eye].ProjectionMatrix);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

    // Let the background layer show through if one is present.
    float clearAlpha = 1.0f;
    if (scene->BackGroundType != BACKGROUND_NONE) {
        layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
        layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
        clearAlpha = 0.0f;
    }

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
        ovrFramebuffer_SetCurrent(frameBuffer);

        GL(glUseProgram(scene->Program.Program));

        ovrMatrix4f viewProjMatrix = ovrMatrix4f_Multiply(
            &tracking->Eye[eye].ProjectionMatrix, &tracking->Eye[eye].ViewMatrix);
        glUniformMatrix4fv(
            scene->Program.UniformLocation[UNIFORM_VIEW_PROJ_MATRIX],
            1,
            GL_TRUE,
            &viewProjMatrix.M[0][0]);

        GL(glEnable(GL_SCISSOR_TEST));
        GL(glDepthMask(GL_TRUE));
        GL(glEnable(GL_DEPTH_TEST));
        GL(glDepthFunc(GL_LEQUAL));
        GL(glEnable(GL_CULL_FACE));
        GL(glCullFace(GL_BACK));
        GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glClearColor(0.0f, 0.0f, 0.0f, clearAlpha));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        GL(glBindVertexArray(scene->GroundPlane.VertexArrayObject));
        GL(glDrawElements(GL_TRIANGLES, scene->GroundPlane.IndexCount, GL_UNSIGNED_SHORT, NULL));
        GL(glBindVertexArray(0));
        GL(glUseProgram(0));

        ovrFramebuffer_Resolve(frameBuffer);

        ovrFramebuffer_Advance(frameBuffer);
    }

    ovrFramebuffer_SetNone();

    return layer;
}

static ovrLayerCube2 BuildCubeLayer(
    ovrTextureSwapChain* cubeSwapChain,
    const ovrTracking2* tracking) {
    const ovrMatrix4f centerEyeViewMatrix = vrapi_GetViewMatrixFromPose(&tracking->HeadPose.Pose);
    const ovrMatrix4f cubeMatrix = ovrMatrix4f_TanAngleMatrixForCubeMap(&centerEyeViewMatrix);

    ovrLayerCube2 layer = vrapi_DefaultLayerCube2();
    layer.HeadPose = tracking->HeadPose;
    layer.TexCoordsFromTanAngles = cubeMatrix;

    // Apply an optional directional bias [-1.0f,1.0f] to improve
    // quality and sample density in a particular direction.
    layer.Offset.x = 0.0f;
    layer.Offset.y = 0.0f;
    layer.Offset.z = 0.0f;

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        layer.Textures[eye].ColorSwapChain = cubeSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;
    }

    return layer;
}

static ovrLayerEquirect2 BuildEquirectLayer(
    ovrTextureSwapChain* equirectSwapChain,
    const ovrTracking2* tracking) {
    ovrLayerEquirect2 layer = vrapi_DefaultLayerEquirect2();

    layer.HeadPose.Pose.Position.x = 0.0f;
    layer.HeadPose.Pose.Position.y = 0.0f;
    layer.HeadPose.Pose.Position.z = 0.0f;
    layer.HeadPose.Pose.Orientation.x = 0.0f;
    layer.HeadPose.Pose.Orientation.y = 0.0f;
    layer.HeadPose.Pose.Orientation.z = 0.0f;
    layer.HeadPose.Pose.Orientation.w = 1.0f;

    layer.TexCoordsFromTanAngles = ovrMatrix4f_CreateIdentity();
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        layer.Textures[eye].ColorSwapChain = equirectSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;
    }

    return layer;
}

static ovrLayerEquirect3 BuildLocalEquirectLayer(
    ovrTextureSwapChain* equirectSwapChain,
    const ovrTracking2* tracking) {
    ovrLayerEquirect3 layer = vrapi_DefaultLayerEquirect3();

    const float radius = 1.5; // 1.5 m radius

    layer.HeadPose = tracking->HeadPose;
    ovrPosef pose = {};
    pose.Position.x = 0.0f;
    pose.Position.y = 0.0f;
    pose.Position.z = -2.0f;
    pose.Orientation.x = 0.0f;
    pose.Orientation.y = 0.0f;
    pose.Orientation.z = 0.0f;
    pose.Orientation.w = 1.0f;
    const ovrMatrix4f poseM = vrapi_GetTransformFromPose(&pose);

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        const ovrMatrix4f modelViewMatrix =
            ovrMatrix4f_Multiply(&tracking->Eye[eye].ViewMatrix, &poseM);
        ovrMatrix4f tex_coords_matrix = ovrMatrix4f_Inverse(&modelViewMatrix);
        tex_coords_matrix.M[3][3] = radius;
        layer.Textures[eye].TexCoordsFromTanAngles = tex_coords_matrix;
        layer.Textures[eye].ColorSwapChain = equirectSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;
    }

    return layer;
}

// Assumes landscape cylinder shape.
static ovrMatrix4f CylinderModelMatrix(
    const int texWidth,
    const int texHeight,
    const ovrVector3f translation,
    const float rotateYaw,
    const float rotatePitch,
    const float radius,
    const float density) {
    const ovrMatrix4f scaleMatrix =
        ovrMatrix4f_CreateScale(radius, radius * (float)texHeight * VRAPI_PI / density, radius);
    const ovrMatrix4f transMatrix =
        ovrMatrix4f_CreateTranslation(translation.x, translation.y, translation.z);
    const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(rotatePitch, 0.0f, 0.0f);
    const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, rotateYaw, 0.0f);

    const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&transMatrix, &scaleMatrix);
    const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&rotXMatrix, &m0);
    const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&rotYMatrix, &m1);

    return m2;
}

static ovrLayerCylinder2 BuildCylinderLayer(
    ovrTextureSwapChain* cylinderSwapChain,
    const int textureWidth,
    const int textureHeight,
    const ovrTracking2* tracking) {
    ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

    const float fadeLevel = 1.0f;
    layer.Header.ColorScale.x = layer.Header.ColorScale.y = layer.Header.ColorScale.z =
        layer.Header.ColorScale.w = fadeLevel;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

    // layer.Header.Flags = VRAPI_FRAME_LAYER_FLAG_CLIP_TO_TEXTURE_RECT;

    layer.HeadPose = tracking->HeadPose;

    const float density = 4500.0f;
    const float rotateYaw = 0.0f;
    const float rotatePitch = 0.0f;
    const float radius = 3.0f;
    const ovrVector3f translation = {0.0f, 0.0f, 0.0f};

    ovrMatrix4f cylinderTransform = CylinderModelMatrix(
        textureWidth, textureHeight, translation, rotateYaw, rotatePitch, radius, density);

    const float circScale = density * 0.5f / textureWidth;
    const float circBias = -circScale * (0.5f * (1.0f - 1.0f / circScale));

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrMatrix4f modelViewMatrix =
            ovrMatrix4f_Multiply(&tracking->Eye[eye].ViewMatrix, &cylinderTransform);
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
        layer.Textures[eye].ColorSwapChain = cylinderSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;

        // Texcoord scale and bias is just a representation of the aspect ratio. The positioning
        // of the cylinder is handled entirely by the TexCoordsFromTanAngles matrix.

        const float texScaleX = circScale;
        const float texBiasX = circBias;
        const float texScaleY = 0.5f;
        const float texBiasY = -texScaleY * (0.5f * (1.0f - (1.0f / texScaleY)));

        layer.Textures[eye].TextureMatrix.M[0][0] = texScaleX;
        layer.Textures[eye].TextureMatrix.M[0][2] = texBiasX;
        layer.Textures[eye].TextureMatrix.M[1][1] = texScaleY;
        layer.Textures[eye].TextureMatrix.M[1][2] = texBiasY;

        layer.Textures[eye].TextureRect.width = 1.0f;
        layer.Textures[eye].TextureRect.height = 1.0f;
    }

    return layer;
}

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrJava Java;
    ovrEgl Egl;
    ANativeWindow* NativeWindow;
    bool Resumed;
    ovrMobile* Ovr;
    ovrScene Scene;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;
    ovrLayer_Union2 Layers[ovrMaxLayerCount];
    int LayerCount;
    bool TouchPadDownLastFrame;
    ovrRenderer Renderer;
} ovrApp;

static void ovrApp_Clear(ovrApp* app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    memset(app->Layers, 0, sizeof(ovrLayer_Union2) * ovrMaxLayerCount);
    app->LayerCount = 0;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;
    app->TouchPadDownLastFrame = false;

    ovrEgl_Clear(&app->Egl);
    ovrScene_Clear(&app->Scene);
    ovrRenderer_Clear(&app->Renderer);
}

static void ovrApp_RenderLoadingIcon(ovrApp* app) {
    int frameFlags = 0;
    frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

    ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();

    ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();

    const ovrLayerHeader2* layers[] = {
        &blackLayer.Header,
        &iconLayer.Header,
    };

    ovrSubmitFrameDescription2 frameDesc = {0};
    frameDesc.Flags = frameFlags;
    frameDesc.SwapInterval = 1;
    frameDesc.FrameIndex = app->FrameIndex;
    frameDesc.DisplayTime = app->DisplayTime;
    frameDesc.LayerCount = 2;
    frameDesc.Layers = layers;

    vrapi_SubmitFrame2(app->Ovr, &frameDesc);
}

static void ovrApp_HandleVrModeChanges(ovrApp* app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
            // No need to reset the FLAG_FULLSCREEN window flag when using a View
            parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
            parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.Display = (size_t)app->Egl.Display;
            parms.WindowSurface = (size_t)app->NativeWindow;
            parms.ShareContext = (size_t)app->Egl.Context;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            app->Ovr = vrapi_EnterVrMode(&parms);

            ALOGV("        vrapi_EnterVrMode()");

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            // Set app-specific parameters once we have entered VR mode and have a valid ovrMobile.
            if (app->Ovr != NULL) {
                vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

                ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

                vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

                ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

                vrapi_SetPerfThread(
                    app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

                ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);

                vrapi_SetTrackingSpace(app->Ovr, VRAPI_TRACKING_SPACE_LOCAL);

                ALOGV("		vrapi_SetTrackingSpace( LOCAL )");
            }
        }
    } else {
        if (app->Ovr != NULL) {
            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;

            ALOGV("        vrapi_LeaveVrMode()");
            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
        }
    }
}

static void ovrApp_HandleInput(ovrApp* app) {
    bool touchPadDownThisFrame = false;

    for (int i = 0;; i++) {
        ovrInputCapabilityHeader cap;
        ovrResult result = vrapi_EnumerateInputDevices(app->Ovr, i, &cap);
        if (result < 0) {
            break;
        }

        if (cap.Type == ovrControllerType_TrackedRemote) {
            ovrInputStateTrackedRemote trackedRemoteState;
            trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
            result = vrapi_GetCurrentInputState(app->Ovr, cap.DeviceID, &trackedRemoteState.Header);
            if (result == ovrSuccess) {
                touchPadDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Enter;
                touchPadDownThisFrame |= trackedRemoteState.Buttons & ovrButton_A;
                touchPadDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Trigger;
            }
        }
    }

    bool touchPadDownLastFrame = app->TouchPadDownLastFrame;
    app->TouchPadDownLastFrame = touchPadDownThisFrame;

    if (touchPadDownLastFrame && !touchPadDownThisFrame) {
        // Cycle through the background types
        app->Scene.BackGroundType = (app->Scene.BackGroundType + 1) % MAX_BACKGROUND_TYPES;
    }
}

static void ovrApp_HandleVrApiEvents(ovrApp* app) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
                break;
            default:
                ALOGV("vrapi_PollEvent: Unknown event");
                break;
        }
    }
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    ovrApp* appState = (ovrApp*)app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

/// Flag to delay-load file system resources until the Java side grants user permissions
int permissionsGrantedFromJava = 0;

/// This gets called back from Java when the user accepts permissions
void Java_com_oculus_sdk_vrcompositorna_MainActivity_nativeInitWithFileLoadPermissions(
    JNIEnv* jni,
    jclass clazz,
    int permissionsGranted) {
    permissionsGrantedFromJava = permissionsGranted;
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    ovrJava java;
    java.Vm = app->activity->vm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    const ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        // If intialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;

    ovrEgl_CreateContext(&appState.Egl, NULL);

    EglInitExtensions();

    if (glExtensions.EXT_sRGB_write_control) {
        // This app was originally written with the presumption that
        // its swapchains and compositor front buffer were RGB.
        // In order to have the colors the same now that its compositing
        // to an sRGB front buffer, we have to write to an sRGB swapchain
        // but with the linear->sRGB conversion disabled on write.
        GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
    }

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();

    ovrRenderer_Create(&appState.Renderer, &java);

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    while (app->destroyRequested == 0) {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            const int timeoutMilliseconds =
                (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            ovrApp_HandleVrModeChanges(&appState);
        }

        // We must read from the event queue with regular frequency.
        ovrApp_HandleVrApiEvents(&appState);

        if (appState.Ovr == NULL) {
            continue;
        }

        ovrApp_HandleInput(&appState);

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!ovrScene_IsCreated(&appState.Scene)) {
            // Show a loading icon.
            ovrApp_RenderLoadingIcon(&appState);

            // Create the scene.
            ovrScene_Create(&appState.Scene, permissionsGrantedFromJava != 0);
        }

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        const double predictedDisplayTime =
            vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking =
            vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;

        // Set-up the compositor layers for this frame.
        // NOTE: Multiple independent layers are allowed, but they need to be added
        // in a depth consistent order.
        memset(appState.Layers, 0, sizeof(ovrLayer_Union2) * ovrMaxLayerCount);
        appState.LayerCount = 0;

        bool shouldRenderWorldLayer = true;

        // Add a background Layer
        if (appState.Scene.BackGroundType == BACKGROUND_CUBEMAP &&
            appState.Scene.CubeMapSwapChain != NULL /* data loaded from sdcard */) {
            appState.Layers[appState.LayerCount++].Cube =
                BuildCubeLayer(appState.Scene.CubeMapSwapChain, &tracking);

            shouldRenderWorldLayer = false;
        } else if (appState.Scene.BackGroundType == BACKGROUND_EQUIRECT) {
            appState.Layers[appState.LayerCount++].Equirect =
                BuildEquirectLayer(appState.Scene.EquirectSwapChain, &tracking);
        } else if (appState.Scene.BackGroundType == BACKGROUND_LOCAL_EQUIRECT) {
            appState.Layers[appState.LayerCount++].Equirect3 =
                BuildLocalEquirectLayer(appState.Scene.EquirectSwapChain, &tracking);
        }

        // Render the world-view layer (simple ground plane)
        if (shouldRenderWorldLayer) {
            appState.Layers[appState.LayerCount++].Projection = ovrRenderer_RenderSceneToEyeBuffer(
                &appState.Renderer, &appState.Java, &appState.Scene, &tracking);
        }

        // Add a simple cylindrical layer
        appState.Layers[appState.LayerCount++].Cylinder = BuildCylinderLayer(
            appState.Scene.CylinderSwapChain,
            appState.Scene.CylinderWidth,
            appState.Scene.CylinderHeight,
            &tracking);

        // Compose the layers for this frame.
        const ovrLayerHeader2* layerHeaders[ovrMaxLayerCount] = {0};
        for (int i = 0; i < appState.LayerCount; i++) {
            layerHeaders[i] = &appState.Layers[i].Header;
        }

        // Set up the description for this frame.
        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = 0;
        frameDesc.SwapInterval = appState.SwapInterval;
        frameDesc.FrameIndex = appState.FrameIndex;
        frameDesc.DisplayTime = appState.DisplayTime;
        frameDesc.LayerCount = appState.LayerCount;
        frameDesc.Layers = layerHeaders;

        vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
    }

    ovrRenderer_Destroy(&appState.Renderer);

    ovrScene_Destroy(&appState.Scene);
    ovrEgl_DestroyContext(&appState.Egl);

    vrapi_Shutdown();

    (*java.Vm)->DetachCurrentThread(java.Vm);
}
