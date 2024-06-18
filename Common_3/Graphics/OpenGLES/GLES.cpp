/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "../GraphicsConfig.h"

#if defined(GLES)
#define RENDERER_IMPLEMENTATION

#define GLES_INSTANCE_ID     "gles_InstanceID"

#define VAO_STATE_CACHE_SIZE 64

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IGraphics.h"
#include "../Interfaces/IRay.h"

#include "../../Utilities/Math/AlgorithmsImpl.h"
#include "../../Utilities/RingBuffer.h"

#include "GLESCapsBuilder.h"
#include "GLESContextCreator.h"

// Default GL ES 2.0 support
#include "../ThirdParty/OpenSource/OpenGL/GLES2/gl2.h"
#include "../ThirdParty/OpenSource/OpenGL/GLES2/gl2ext.h"

#include "../../Utilities/Interfaces/IMemory.h"

extern void gles_createShaderReflection(Shader* pProgram, ShaderReflection* pOutReflection, const BinaryShaderDesc* pDesc);

/************************************************************************/
// Descriptor Set Structure
/************************************************************************/

typedef struct DescriptorIndexMap
{
    char*    key;
    uint32_t value;
} DescriptorIndexMap;

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================
struct GLExtensionSupport
{
    uint32_t mHasDebugMarkerEXT : 1;
    uint32_t mHasDebugLabelEXT : 1;
    uint32_t mHasDisjointTimerQueryEXT : 1;

    uint32_t mHasTextureBorderClampOES : 1;
    uint32_t mHasDepthTextureOES : 1;
    uint32_t mHasVertexArrayObjectOES : 1;
    uint32_t mHasMapbufferOES : 1;
};

struct GLRasterizerState
{
    GLenum mCullMode;
    GLenum mFrontFace;
    bool   mScissorTest;
};

struct GLDepthStencilState
{
    bool    mDepthTest;
    bool    mDepthWrite;
    GLenum  mDepthFunc;
    bool    mStencilTest;
    GLenum  mStencilFrontFunc;
    GLenum  mStencilBackFunc;
    uint8_t mStencilReadMask;
    uint8_t mStencilWriteMask;
    GLenum  mStencilFrontFail;
    GLenum  mDepthFrontFail;
    GLenum  mStencilFrontPass;
    GLenum  mStencilBackFail;
    GLenum  mDepthBackFail;
    GLenum  mStencilBackPass;
};

struct GLBlendState
{
    bool   mBlendEnable;
    GLenum mSrcRGBFunc;
    GLenum mDstRGBFunc;
    GLenum mSrcAlphaFunc;
    GLenum mDstAlphaFunc;
    GLenum mModeRGB;
    GLenum mModeAlpha;
};

typedef struct GlVertexAttrib
{
    GLint     mIndex;
    GLint     mSize;
    GLenum    mType;
    GLboolean mNormalized;
    GLsizei   mStride;
    GLuint    mOffset;
} GlVertexAttrib;

typedef struct VertexAttribCache
{
    GLint  mAttachedBuffer;
    GLuint mOffset;
    bool   mIsActive;
} VertexAttribCache;

typedef struct GLVAOState
{
    uint32_t mId;
    uint32_t mVAO;
    int32_t  mAttachedBuffers[MAX_VERTEX_ATTRIBS];
    uint32_t mBufferOffsets[MAX_VERTEX_ATTRIBS];
    uint32_t mActiveIndexBuffer;
    uint32_t mAttachedBufferCount;
    uint32_t mFirstVertex;
} GLVAOState;

struct CmdCache
{
    bool isStarted;

    uint32_t mActiveIndexBuffer;
    uint32_t mIndexBufferOffset;
    Buffer*  pIndexBuffer;

    uint32_t mActiveVertexBuffer;
    uint32_t mVertexBufferOffsets[MAX_VERTEX_ATTRIBS];
    uint32_t mVertexBufferStrides[MAX_VERTEX_ATTRIBS];
    Buffer*  mVertexBuffers[MAX_VERTEX_ATTRIBS];
    uint32_t mVertexBufferCount;

    uint32_t mStencilRefValue;
    uint32_t mRenderTargetHeight;

    vec4 mViewport;
    vec4 mScissor;
    vec2 mDepthRange;

    vec4     mClearColor;
    float    mClearDepth;
    int32_t  mClearStencil;
    uint32_t mFramebuffer;

    // Holds index ordered vertex attribute information
    VertexAttribCache mVertexAttribCache[MAX_VERTEX_ATTRIBS];

    // VAO cache
    uint32_t    mActiveVAO;
    uint16_t*   pActiveVAOStateCount;
    uint16_t*   pActiveVAOStateLoop;
    GLVAOState* pActiveVAOStates;

    // Pipeline state
    uint32_t            mPipeline;
    RootSignature*      pRootSignature;
    GLRasterizerState   mRasterizerState;
    GLDepthStencilState mDepthStencilState;
    GLBlendState        mBlendState;
    GLenum              mGlPrimitiveTopology;
    uint32_t            mVertexLayoutCount;
    GlVertexAttrib      mVertexLayout[MAX_VERTEX_ATTRIBS];
    int32_t             mInstanceLocation;
    // Textures
    Sampler*            pTextureSampler;
    uint32_t            mBoundTexture;
    int32_t             mActiveTexture;
};

/************************************************************************/
// Internal globals
/************************************************************************/
static GLRasterizerState   gDefaultRasterizer = {};
static GLDepthStencilState gDefaultDepthStencilState = {};
static GLBlendState        gDefaultBlendState = {};
static Sampler*            pDefaultSampler = {};
static GLExtensionSupport  gExtensionSupport;

#if defined(RENDERER_IMPLEMENTATION)
/************************************************************************/
// Extension functions
/************************************************************************/
static PFNGLINSERTEVENTMARKEREXTPROC glInsertEventMarkerEXT;
static PFNGLPUSHGROUPMARKEREXTPROC   glPushGroupMarkerEXT;
static PFNGLPOPGROUPMARKEREXTPROC    glPopGroupMarkerEXT;

static PFNGLLABELOBJECTEXTPROC glLabelObjectEXT;

static PFNGLGENQUERIESEXTPROC          glGenQueriesEXT;
static PFNGLDELETEQUERIESEXTPROC       glDeleteQueriesEXT;
static PFNGLBEGINQUERYEXTPROC          glBeginQueryEXT;
static PFNGLENDQUERYEXTPROC            glEndQueryEXT;
static PFNGLQUERYCOUNTEREXTPROC        glQueryCounterEXT;
static PFNGLGETQUERYIVEXTPROC          glGetQueryivEXT;
static PFNGLGETQUERYOBJECTIVEXTPROC    glGetQueryObjectivEXT;
static PFNGLGETQUERYOBJECTUIVEXTPROC   glGetQueryObjectuivEXT;
static PFNGLGETQUERYOBJECTI64VEXTPROC  glGetQueryObjecti64vEXT;
static PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT;
static PFNGLGETINTEGER64VEXTPROC       glGetInteger64vEXT;

static PFNGLBINDVERTEXARRAYOESPROC    glBindVertexArrayOES;
static PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
static PFNGLGENVERTEXARRAYSOESPROC    glGenVertexArraysOES;
static PFNGLISVERTEXARRAYOESPROC      glIsVertexArrayOES;

static PFNGLMAPBUFFEROESPROC         glMapBufferOES;
static PFNGLUNMAPBUFFEROESPROC       glUnmapBufferOES;
static PFNGLGETBUFFERPOINTERVOESPROC glGetBufferPointervOES;

void util_init_extension_support(GLExtensionSupport* pExtensionSupport, const char* availableExtensions)
{
    ASSERT(pExtensionSupport);

    // Extension GL_OES_texture_border_clamp
    pExtensionSupport->mHasTextureBorderClampOES = strstr(availableExtensions, "GL_OES_texture_border_clamp") != nullptr;

    // Extension GL_OES_depth_texture
    pExtensionSupport->mHasDepthTextureOES = strstr(availableExtensions, "GL_OES_depth_texture") != nullptr;

    // Extension GL_EXT_debug_marker
    pExtensionSupport->mHasDebugMarkerEXT = strstr(availableExtensions, "GL_EXT_debug_marker") != nullptr;
    if (pExtensionSupport->mHasDebugMarkerEXT)
    {
        glInsertEventMarkerEXT = (PFNGLINSERTEVENTMARKEREXTPROC)getExtensionsFunction("glInsertEventMarkerEXT");
        glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)getExtensionsFunction("glPushGroupMarkerEXT");
        glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)getExtensionsFunction("glPopGroupMarkerEXT");
    }

    // Extension GL_EXT_debug_label
    pExtensionSupport->mHasDebugLabelEXT = strstr(availableExtensions, "GL_EXT_debug_label") != nullptr;
    glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)getExtensionsFunction("glLabelObjectEXT");

    // Extension GL_EXT_disjoint_timer_query
    pExtensionSupport->mHasDisjointTimerQueryEXT = strstr(availableExtensions, "GL_EXT_disjoint_timer_query") != nullptr;
    if (pExtensionSupport->mHasDisjointTimerQueryEXT)
    {
        glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC)getExtensionsFunction("glGenQueriesEXT");
        glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC)getExtensionsFunction("glDeleteQueriesEXT");
        glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC)getExtensionsFunction("glBeginQueryEXT");
        glEndQueryEXT = (PFNGLENDQUERYEXTPROC)getExtensionsFunction("glEndQueryEXT");
        glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)getExtensionsFunction("glQueryCounterEXT");
        glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC)getExtensionsFunction("glGetQueryivEXT");
        glGetQueryObjectivEXT = (PFNGLGETQUERYOBJECTIVEXTPROC)getExtensionsFunction("glGetQueryObjectivEXT");
        glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC)getExtensionsFunction("glGetQueryObjectuivEXT");
        glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC)getExtensionsFunction("glGetQueryObjecti64vEXT");
        glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)getExtensionsFunction("glGetQueryObjectui64vEXT");
        glGetInteger64vEXT = (PFNGLGETINTEGER64VEXTPROC)getExtensionsFunction("glGetInteger64vEXT");
    }

    // Extension GL_OES_vertex_array_object
    pExtensionSupport->mHasVertexArrayObjectOES = strstr(availableExtensions, "GL_OES_vertex_array_object") != nullptr;
    if (pExtensionSupport->mHasVertexArrayObjectOES)
    {
        glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC)getExtensionsFunction("glBindVertexArrayOES");
        glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC)getExtensionsFunction("glDeleteVertexArraysOES");
        glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC)getExtensionsFunction("glGenVertexArraysOES");
        glIsVertexArrayOES = (PFNGLISVERTEXARRAYOESPROC)getExtensionsFunction("glIsVertexArrayOES");
    }

    // Extension GL_OES_mapbuffer
    pExtensionSupport->mHasMapbufferOES = strstr(availableExtensions, "GL_OES_mapbuffer") != nullptr;
    if (pExtensionSupport->mHasMapbufferOES)
    {
        glMapBufferOES = (PFNGLMAPBUFFEROESPROC)getExtensionsFunction("glMapBufferOES");
        glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC)getExtensionsFunction("glUnmapBufferOES");
        glGetBufferPointervOES = (PFNGLGETBUFFERPOINTERVOESPROC)getExtensionsFunction("glGetBufferPointervOES");
    }
}

/************************************************************************/
// Internal util functions
/************************************************************************/

inline const char* util_get_format_string(GLenum value)
{
    switch (value)
    {
        // ASTC
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_5x4_KHR";
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_6x5_KHR";
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_8x5_KHR";
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_8x6_KHR";
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_10x5_KHR";
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_10x6_KHR";
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_10x8_KHR";
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_12x10_KHR";
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR";

    case GL_COMPRESSED_RGBA_ASTC_3x3x3_OES:
        return "GL_COMPRESSED_RGBA_ASTC_3x3x3_OES";
    case GL_COMPRESSED_RGBA_ASTC_4x3x3_OES:
        return "GL_COMPRESSED_RGBA_ASTC_4x3x3_OES";
    case GL_COMPRESSED_RGBA_ASTC_4x4x3_OES:
        return "GL_COMPRESSED_RGBA_ASTC_4x4x3_OES";
    case GL_COMPRESSED_RGBA_ASTC_4x4x4_OES:
        return "GL_COMPRESSED_RGBA_ASTC_4x4x4_OES";
    case GL_COMPRESSED_RGBA_ASTC_5x4x4_OES:
        return "GL_COMPRESSED_RGBA_ASTC_5x4x4_OES";
    case GL_COMPRESSED_RGBA_ASTC_5x5x4_OES:
        return "GL_COMPRESSED_RGBA_ASTC_5x5x4_OES";
    case GL_COMPRESSED_RGBA_ASTC_5x5x5_OES:
        return "GL_COMPRESSED_RGBA_ASTC_5x5x5_OES";
    case GL_COMPRESSED_RGBA_ASTC_6x5x5_OES:
        return "GL_COMPRESSED_RGBA_ASTC_6x5x5_OES";
    case GL_COMPRESSED_RGBA_ASTC_6x6x5_OES:
        return "GL_COMPRESSED_RGBA_ASTC_6x6x5_OES";
    case GL_COMPRESSED_RGBA_ASTC_6x6x6_OES:
        return "GL_COMPRESSED_RGBA_ASTC_6x6x6_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES";
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES:
        return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES";

    case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
        return "GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE";

    case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
        return "GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT";

    case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
        return "GL_COMPRESSED_RGBA_BPTC_UNORM_EXT";

    default:
        return "GL_UNKNOWN_FORMAT";
    }
}

GLsizei util_get_compressed_texture_size(GLenum format, uint32_t width, uint32_t height)
{
    switch (format)
    {
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return ((uint32_t)ceil(width / 4.0f) * (uint32_t)ceil(height / 4.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return ((uint32_t)ceil(width / 5.0f) * (uint32_t)ceil(height / 4.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return ((uint32_t)ceil(width / 5.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return ((uint32_t)ceil(width / 6.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return ((uint32_t)ceil(width / 6.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 8.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 8.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 10.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return ((uint32_t)ceil(width / 12.0f) * (uint32_t)ceil(height / 10.0f)) << 4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return ((uint32_t)ceil(width / 12.0f) * (uint32_t)ceil(height / 12.0f)) << 4;
    case GL_ETC1_RGB8_OES:
        return ((uint32_t)ceil(width / 4.0f) * (uint32_t)ceil(height / 4.0f)) << 3;
    default:
        LOGF(LogLevel::eERROR, "Unknown compressed GL format!");
        ASSERT(false);
        return ((uint32_t)ceil(width / 4.0f) * (uint32_t)ceil(height / 4.0f)) << 3;
    }
}

inline const char* util_get_enum_string(GLenum value)
{
    switch (value)
    {
        // Errors
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
        // Framebuffer status
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";

    default:
        return "GL_UNKNOWN_ERROR";
    }
}

#define SAFE_FREE(p_var) \
    if (p_var)           \
    {                    \
        tf_free(p_var);  \
        p_var = NULL;    \
    }

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#if defined(ENABLE_GRAPHICS_DEBUG)
#define CHECK_GLRESULT(exp)                                                               \
    {                                                                                     \
        exp;                                                                              \
        GLenum glRes = glGetError();                                                      \
        if (glRes != GL_NO_ERROR)                                                         \
        {                                                                                 \
            LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes)); \
            ASSERT(false);                                                                \
        }                                                                                 \
    }

#define CHECK_GL_RETURN_RESULT(var, exp)                                                  \
    {                                                                                     \
        var = exp;                                                                        \
        GLenum glRes = glGetError();                                                      \
        if (glRes != GL_NO_ERROR)                                                         \
        {                                                                                 \
            LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes)); \
            ASSERT(false);                                                                \
        }                                                                                 \
    }
#else
#define CHECK_GLRESULT(exp) \
    {                       \
        exp;                \
    }

#define CHECK_GL_RETURN_RESULT(var, exp) \
    {                                    \
        var = exp;                       \
    }
#endif

static inline uint32_t gl_type_byte_size(GLenum type)
{
    switch (type)
    {
    case GL_INT:
    case GL_BOOL:
    case GL_FLOAT:
        return 4;
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
    case GL_FLOAT_VEC2:
        return 8;
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
    case GL_FLOAT_VEC3:
        return 16;
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
    case GL_FLOAT_VEC4:
        return 16;
    case GL_FLOAT_MAT2:
        return 32;
    case GL_FLOAT_MAT3:
        return 48;
    case GL_FLOAT_MAT4:
        return 64;
    default:
        ASSERT(false && "Unknown GL type");
        return 0;
    }
}

static inline void util_gl_set_uniform(uint32_t location, uint8_t* data, GLenum type, uint32_t size)
{
    switch (type)
    {
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
        CHECK_GLRESULT(glUniform1iv(location, size, (GLint*)data));
        break;
    case GL_FLOAT:
        CHECK_GLRESULT(glUniform1fv(location, size, (GLfloat*)data));
        break;
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
        CHECK_GLRESULT(glUniform2iv(location, size, (GLint*)data));
        break;
    case GL_FLOAT_VEC2:
        CHECK_GLRESULT(glUniform2fv(location, size, (GLfloat*)data));
        break;
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
        CHECK_GLRESULT(glUniform3iv(location, size, (GLint*)data));
        break;
    case GL_FLOAT_VEC3:
        CHECK_GLRESULT(glUniform3fv(location, size, (GLfloat*)data));
        break;
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
        CHECK_GLRESULT(glUniform4iv(location, size, (GLint*)data));
        break;
    case GL_FLOAT_VEC4:
        CHECK_GLRESULT(glUniform4fv(location, size, (GLfloat*)data));
        break;
    case GL_FLOAT_MAT2:
        CHECK_GLRESULT(glUniformMatrix2fv(location, size, GL_FALSE, (GLfloat*)data));
        break;
    case GL_FLOAT_MAT3:
        CHECK_GLRESULT(glUniformMatrix3fv(location, size, GL_FALSE, (GLfloat*)data));
        break;
    case GL_FLOAT_MAT4:
        CHECK_GLRESULT(glUniformMatrix4fv(location, size, GL_FALSE, (GLfloat*)data));
        break;
    default:
        ASSERT(false && "Unknown GL type");
    }
}

static GLint util_to_gl_usage(ResourceMemoryUsage mem)
{
    switch (mem)
    {
    case RESOURCE_MEMORY_USAGE_GPU_ONLY:
        return GL_STATIC_DRAW;
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
        return GL_NONE;
    case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
        return GL_DYNAMIC_DRAW;
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        return GL_STREAM_DRAW;
    default:
        ASSERT(false && "Invalid Memory Usage");
        return GL_DYNAMIC_DRAW;
    }
}

static GLenum util_to_gl_address_mode(AddressMode mode)
{
    switch (mode)
    {
    case ADDRESS_MODE_MIRROR:
        return GL_MIRRORED_REPEAT;
    case ADDRESS_MODE_REPEAT:
        return GL_REPEAT;
    case ADDRESS_MODE_CLAMP_TO_EDGE:
        return GL_CLAMP_TO_EDGE;
    case ADDRESS_MODE_CLAMP_TO_BORDER:
        return gExtensionSupport.mHasTextureBorderClampOES ? GL_CLAMP_TO_BORDER_OES : GL_CLAMP_TO_EDGE;
    default:
        ASSERT(false && "Invalid AddressMode");
        return GL_REPEAT;
    }
}

static GLenum util_to_gl_blend_const(BlendConstant bconst, bool isSrc)
{
    switch (bconst)
    {
    case BC_ZERO:
        return GL_ZERO;
    case BC_ONE:
        return GL_ONE;
    case BC_SRC_COLOR:
        return GL_SRC_COLOR;
    case BC_ONE_MINUS_SRC_COLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case BC_DST_COLOR:
        return GL_DST_COLOR;
    case BC_ONE_MINUS_DST_COLOR:
        return GL_ONE_MINUS_DST_COLOR;
    case BC_SRC_ALPHA:
        return GL_SRC_ALPHA;
    case BC_ONE_MINUS_SRC_ALPHA:
        return GL_ONE_MINUS_SRC_ALPHA;
    case BC_DST_ALPHA:
        return GL_DST_ALPHA;
    case BC_ONE_MINUS_DST_ALPHA:
        return GL_ONE_MINUS_DST_ALPHA;
    case BC_SRC_ALPHA_SATURATE:
        return GL_SRC_ALPHA_SATURATE;
    case BC_BLEND_FACTOR:
        return GL_CONSTANT_COLOR;
    case BC_ONE_MINUS_BLEND_FACTOR:
        return GL_ONE_MINUS_CONSTANT_COLOR;
    default:
        ASSERT(false && "Invalid BlendConstant");
        return isSrc ? GL_ONE : GL_ZERO;
    }
}

static GLenum util_to_gl_blend_mode(BlendMode mode)
{
    switch (mode)
    {
    case BM_ADD:
        return GL_FUNC_ADD;
    case BM_SUBTRACT:
        return GL_FUNC_ADD;
    case BM_REVERSE_SUBTRACT:
        return GL_FUNC_ADD;
    case BM_MIN:
        return GL_MIN; // GLES 3.2
    case BM_MAX:
        return GL_MAX; // GLES 3.2
    default:
        ASSERT(false && "Invalid BlendMode");
        return GL_FUNC_ADD;
    }
}

static GLenum util_to_gl_stencil_op(StencilOp op)
{
    switch (op)
    {
    case STENCIL_OP_KEEP:
        return GL_KEEP;
    case STENCIL_OP_SET_ZERO:
        return GL_ZERO;
    case STENCIL_OP_REPLACE:
        return GL_REPLACE;
    case STENCIL_OP_INVERT:
        return GL_INVERT;
    case STENCIL_OP_INCR:
        return GL_INCR;
    case STENCIL_OP_DECR:
        return GL_DECR;
    case STENCIL_OP_INCR_SAT:
        return GL_INCR_WRAP;
    case STENCIL_OP_DECR_SAT:
        return GL_DECR_WRAP;
    default:
        ASSERT(false && "Invalid StencilOp");
        return GL_KEEP;
    }
}

static GLenum util_to_gl_compare_mode(CompareMode mode)
{
    switch (mode)
    {
    case CMP_NEVER:
        return GL_NEVER;
    case CMP_LESS:
        return GL_LESS;
    case CMP_EQUAL:
        return GL_EQUAL;
    case CMP_LEQUAL:
        return GL_LEQUAL;
    case CMP_GREATER:
        return GL_GREATER;
    case CMP_NOTEQUAL:
        return GL_NOTEQUAL;
    case CMP_GEQUAL:
        return GL_GEQUAL;
    case CMP_ALWAYS:
        return GL_ALWAYS;
    default:
        ASSERT(false && "Invalid CompareMode");
        return GL_LESS;
    }
}

static GLBlendState util_to_blend_desc(const BlendStateDesc* pDesc)
{
    int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)

    for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
    {
        if (pDesc->mRenderTargetMask & (1 << i))
        {
            ASSERT(pDesc->mSrcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mDstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mBlendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
            ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
        }

        if (pDesc->mIndependentBlend)
            ++blendDescIndex;
    }

    blendDescIndex = 0;
#endif

    ASSERT(!pDesc->mIndependentBlend && "IndependentBlend modes not supported");

    GLBlendState blendState = {};

    blendState.mSrcRGBFunc = util_to_gl_blend_const(pDesc->mSrcFactors[blendDescIndex], true);
    blendState.mDstRGBFunc = util_to_gl_blend_const(pDesc->mDstFactors[blendDescIndex], false);
    blendState.mSrcAlphaFunc = util_to_gl_blend_const(pDesc->mSrcAlphaFactors[blendDescIndex], true);
    blendState.mDstAlphaFunc = util_to_gl_blend_const(pDesc->mDstAlphaFactors[blendDescIndex], false);

    blendState.mModeRGB = util_to_gl_blend_mode(pDesc->mBlendModes[blendDescIndex]);
    blendState.mModeAlpha = util_to_gl_blend_mode(pDesc->mBlendAlphaModes[blendDescIndex]);

    blendState.mBlendEnable = blendState.mSrcRGBFunc != GL_ONE || blendState.mDstRGBFunc != GL_ZERO || blendState.mSrcAlphaFunc != GL_ONE ||
                              blendState.mDstAlphaFunc != GL_ZERO;

    return blendState;

    // Unhandled
    // pDesc->mColorWriteMasks[blendDescIndex]
    // pDesc->mRenderTargetMask
    // pDesc->mAlphaToCoverage
}

static GLDepthStencilState util_to_depth_desc(const DepthStateDesc* pDesc)
{
    ASSERT(pDesc->mDepthFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthFrontFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilFrontPass < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilBackFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthBackFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackPass < StencilOp::MAX_STENCIL_OPS);

    GLDepthStencilState depthState = {};
    depthState.mDepthTest = pDesc->mDepthTest;                          // glEnable / glDisable(GL_DEPTH_TEST)
    depthState.mDepthWrite = pDesc->mDepthWrite;                        // glDepthMask(GL_TRUE / GL_FALSE);
    depthState.mDepthFunc = util_to_gl_compare_mode(pDesc->mDepthFunc); // glDepthFunc(GL_LESS)

    depthState.mStencilTest = pDesc->mStencilTest;           // glEnable / glDisable(GL_STENCIL_TEST)
    depthState.mStencilWriteMask = pDesc->mStencilWriteMask; // glStencilMask(mask)
    depthState.mStencilReadMask = pDesc->mStencilReadMask;

    depthState.mStencilFrontFunc =
        util_to_gl_compare_mode(pDesc->mStencilFrontFunc); // glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, ref_value?, mask (1))
    // glStencilOpSeparate(GLenum face, GLenum GL_KEEP, GLenum GL_KEEP, GLenum GL_KEEP);
    depthState.mStencilFrontFail = util_to_gl_stencil_op(pDesc->mStencilFrontFail);
    depthState.mDepthFrontFail = util_to_gl_stencil_op(pDesc->mDepthFrontFail);
    depthState.mStencilFrontPass = util_to_gl_stencil_op(pDesc->mStencilFrontPass);

    depthState.mStencilBackFunc =
        util_to_gl_compare_mode(pDesc->mStencilBackFunc); // glStencilFuncSeparate(GL_BACK, GL_ALWAYS, ref_value?, mask (1))
    depthState.mStencilBackFail = util_to_gl_stencil_op(pDesc->mStencilBackFail);
    depthState.mDepthBackFail = util_to_gl_stencil_op(pDesc->mDepthBackFail);
    depthState.mStencilBackPass = util_to_gl_stencil_op(pDesc->mStencilBackPass);

    return depthState;
}

static GLRasterizerState util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
{
    ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
    ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
    ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

    GLRasterizerState rasterizationState = {};
    switch (pDesc->mCullMode)
    {
    case CullMode::CULL_MODE_NONE:
        rasterizationState.mCullMode = GL_NONE; // if none glDisable(GL_CULL_FACE);
        break;
    case CullMode::CULL_MODE_BACK:
        rasterizationState.mCullMode = GL_BACK;
        break;
    case CullMode::CULL_MODE_FRONT:
        rasterizationState.mCullMode = GL_FRONT;
        break;
    case CullMode::CULL_MODE_BOTH:
        rasterizationState.mCullMode = GL_FRONT_AND_BACK;
        break;
    default:
        ASSERT(false && "Unknown cull mode");
        break;
    }

    switch (pDesc->mFrontFace)
    {
    case FrontFace::FRONT_FACE_CCW:
        rasterizationState.mFrontFace = GL_CCW;
        break;
    case FrontFace::FRONT_FACE_CW:
        rasterizationState.mFrontFace = GL_CW;
        break;
    default:
        ASSERT(false && "Unknown front face mode");
        break;
    }
    rasterizationState.mScissorTest = pDesc->mScissor;

    // Unhandled
    // pDesc->mDepthBias;
    // pDesc->mSlopeScaledDepthBias;
    // pDesc->mFillMode; // Not supported for GLES
    // pDesc->mMultiSample;
    // pDesc->mDepthClampEnable;

    return rasterizationState;
}

void util_log_program_info(GLuint program)
{
    GLint infoLen = 0;
    CHECK_GLRESULT(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen));
    if (infoLen > 1)
    {
        char* infoLog = nullptr;
        infoLog = (char*)tf_calloc(infoLen + 1, sizeof(char));
        CHECK_GLRESULT(glGetProgramInfoLog(program, infoLen + 1, nullptr, infoLog));
        LOGF(LogLevel::eERROR, "GL shader program error, info log:\n%s\n", infoLog);
        tf_free(infoLog);
    }
    else
    {
        LOGF(LogLevel::eERROR, "GL shader program error: No InfoLog available");
    }
}

bool util_link_and_validate_program(GLuint program)
{
    CHECK_GLRESULT(glLinkProgram(program));
    GLint status = GL_FALSE;
    CHECK_GLRESULT(glGetProgramiv(program, GL_LINK_STATUS, &status));
    if (status)
    {
        CHECK_GLRESULT(glValidateProgram(program));
        CHECK_GLRESULT(glGetProgramiv(program, GL_VALIDATE_STATUS, &status));
        if (!status)
        {
            util_log_program_info(program);
            return false;
        }
    }
    else
    {
        util_log_program_info(program);
        return false;
    }

    return true;
}

/************************************************************************/
// Functions not exposed in IRenderer but still need to be exported in dll
/************************************************************************/
// clang-format off
DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)

/************************************************************************/
// Internal init functions
/************************************************************************/
#if defined(VULKAN) && defined(ANDROID)
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/volk/volk.h"

extern VkAllocationCallbacks* GetAllocationCallbacks(VkObjectType objType);

// temporarily initializes vulkan to verify that gpu is whitelisted
static bool verifyGPU()
{
    if (volkInitialize() != VK_SUCCESS)
        return false;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = NULL;

    VkInstance instance;
    if (vkCreateInstance(&instanceInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_INSTANCE), &instance) != VK_SUCCESS)
        return false;
    volkLoadInstanceOnly(instance);

    // We pick first available device for android
    uint32_t deviceCount = 1;
    VkPhysicalDevice device;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, &device) != VK_SUCCESS || deviceCount != 1)
    {
        vkDestroyInstance(instance, GetAllocationCallbacks(VK_OBJECT_TYPE_INSTANCE));
        return false;
    }

    vkDestroyInstance(instance, GetAllocationCallbacks(VK_OBJECT_TYPE_INSTANCE));
    return true;
}
#endif


static bool addDevice(Renderer* pRenderer, const RendererDesc* pDesc)
{
    const char* glVendor = (const char*)glGetString(GL_VENDOR);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    const char* glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

    // Automated testing - We want to test GLES when specified
#if defined(VULKAN) && defined(ANDROID)
    if (pDesc->mPreferVulkan && verifyGPU())
    {
        LOGF(LogLevel::eINFO, "Device supports Vulkan. switching API...");
        return false;
    }
#endif
    LOGF(LogLevel::eINFO, "GPU detected. Vendor: %s, GPU Name: %s, GL Version: %s, GLSL Version: %s", glVendor, glRenderer, glVersion, glslVersion);

    // Shader caps
    GLboolean isShaderCompilerSupported = GL_FALSE;
    GLint nShaderBinaryFormats = 0;

    GLint maxVertexAttr = 8; // Max number of 4-component generic vertex attributes accessible to a vertex shader
    GLint maxVertexUniformVectors = 128; // Max number of four-element floating-point, integer, or boolean vectors that can be held in uniform variable storage for a vertex shader
    GLint maxFragmentUniformVectors = 16; // Max number of four-element floating-point, integer, or boolean vectors that can be held in uniform variable storage for a fragment shader
    GLint maxVaryingVectors = 8;

    GLint maxTextureImageUnits = 8; // Max supported texture image units for fragment shader using glActiveTexture (initial value GL_TEXTURE0)
    GLint maxVertTextureImageUnits = 0; // Max supported texture image units for fragment shader using glActiveTexture (initial value GL_TEXTURE0)
    GLint maxCombinedTextureImageUnits = 8; // Max supported texture image units vertex/fragment shaders combined

    glGetBooleanv(GL_SHADER_COMPILER, &isShaderCompilerSupported);
    if (!isShaderCompilerSupported)
    {
        LOGF(LogLevel::eERROR, "Unsupported device! No shader compiler support for OpenGL ES 2.0");
        return false;
    }

    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nShaderBinaryFormats);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttr);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniformVectors);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxFragmentUniformVectors);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertTextureImageUnits);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextureImageUnits);

    // Texture alignment
    GLint packAlignment = 4;	// Byte alignment used for reading pixel data from gpu memory
    GLint unpackAlignment = 4;	// Byte alignment used for writing pixel data to gpu memory
    CHECK_GLRESULT(glPixelStorei(GL_PACK_ALIGNMENT, packAlignment));
    CHECK_GLRESULT(glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment));

    // Texture caps
    GLint maxCubeMapTextureSize = 16;
    GLint maxTextureSize = 64;
    GLint maxRenderBufferSize = 1;
    GLint maxViewportDims;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderBufferSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, &maxViewportDims);

    //GL_COMPRESSED_TEXTURE_FORMATS
    GLint nCompressedTextureFormats = 0;
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nCompressedTextureFormats);

    // Set active GPU settings
    static RendererContext context = {};
    context.mGpuCount = 1;
    char* glExtensions = (char*)glGetString(GL_EXTENSIONS);
    LOGF(LogLevel::eINFO, "Extensions: %s", glExtensions);
    util_init_extension_support(&gExtensionSupport, glExtensions);

    GPUSettings& gpuSettings = context.mGpus[0].mSettings;
    setDefaultGPUSettings(&gpuSettings);
    // see extension GL_NVX_gpu_memory_info or WGL_AMD_gpu_association
    gpuSettings.mVRAM = 0;

    glCapsBuilder(&context.mGpus[0], glExtensions);
    //apply rules from gpu.cfg
    applyGPUConfigurationRules(&context.mGpus[0].mSettings, &context.mGpus[0].mCapBits);
    // set hard coded openGL limitation
    gpuSettings.mUniformBufferAlignment = 4;
    gpuSettings.mUploadBufferTextureAlignment = unpackAlignment;
    gpuSettings.mUploadBufferTextureRowAlignment = unpackAlignment;
    gpuSettings.mMaxVertexInputBindings = min((uint32_t)MAX_VERTEX_ATTRIBS, (uint32_t)maxVertexAttr);
    gpuSettings.mMaxBoundTextures = maxCombinedTextureImageUnits;
    gpuSettings.mMultiDrawIndirect = 0;
    gpuSettings.mROVsSupported = 0;
    gpuSettings.mTessellationSupported = 0;
    gpuSettings.mGeometryShaderSupported = 0;
    gpuSettings.mTimestampQueries = gExtensionSupport.mHasDisjointTimerQueryEXT;
    gpuSettings.mOcclusionQueries = false;
    gpuSettings.mPipelineStatsQueries = false;
    gpuSettings.mMaxTotalComputeThreads = 0;
    gpuSettings.mMaxComputeThreads[0] = 0;
    gpuSettings.mMaxComputeThreads[1] = 0;
    gpuSettings.mMaxComputeThreads[2] = 0;

    // Validate requested device extensions
    uint32_t supportedExtensions = 0;
    const char* ptr = strtok(glExtensions, " ");
    while (ptr != nullptr)
    {
        for (uint32_t i = 0; i < pDesc->mGLES.mDeviceExtensionCount; ++i)
        {
            if (strcmp(ptr, pDesc->mGLES.ppDeviceExtensions[i]) == 0)
            {
                ++supportedExtensions;
                break;
            }
        }
        ptr = strtok(nullptr, " ");
    }

    if (supportedExtensions != pDesc->mGLES.mDeviceExtensionCount)
    {
        LOGF(LogLevel::eERROR, "Not all requested device extensions are supported on the device! Device support %u of %u requested.", supportedExtensions, pDesc->mGLES.mDeviceExtensionCount);
        return false;
    }

    GPUVendorPreset& gpuVendorPresets = gpuSettings.mGpuVendorPreset;
    gpuVendorPresets.mVendorId = strtol(glVendor, NULL, 16);
    strncpy(gpuVendorPresets.mVendorName, glVendor, MAX_GPU_VENDOR_STRING_LENGTH);
    strncpy(gpuVendorPresets.mGpuName, glRenderer, MAX_GPU_VENDOR_STRING_LENGTH);
    strncpy(gpuVendorPresets.mGpuDriverVersion, glVersion, MAX_GPU_VENDOR_STRING_LENGTH);
    gpuVendorPresets.mPresetLevel = getGPUPresetLevel(gpuVendorPresets.mVendorId, gpuVendorPresets.mModelId, gpuVendorPresets.mVendorName, gpuVendorPresets.mGpuName);

    pRenderer->pContext = &context;
    pRenderer->pGpu = &context.mGpus[0];
    pRenderer->mLinkedNodeCount = 1;
    pRenderer->mGpuMode = GPU_MODE_SINGLE;

    return true;
}

static void removeDevice(Renderer* pRenderer)
{
    pRenderer->pContext = NULL;
    pRenderer->pGpu = NULL;
}

/************************************************************************/
// Pipeline State Functions
/************************************************************************/
static void add_default_resources(Renderer* pRenderer)
{
    BlendStateDesc blendStateDesc = {};
    blendStateDesc.mSrcFactors[0] = BC_ONE;
    blendStateDesc.mDstFactors[0] = BC_ZERO;
    blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
    blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
    blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blendStateDesc.mIndependentBlend = false;
    gDefaultBlendState = util_to_blend_desc(&blendStateDesc);

    DepthStateDesc depthStateDesc = {};
    depthStateDesc.mDepthFunc = CMP_LEQUAL;
    depthStateDesc.mDepthTest = false;
    depthStateDesc.mDepthWrite = false;
    depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
    depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
    depthStateDesc.mStencilWriteMask = 0xFF;
    depthStateDesc.mStencilReadMask = 0xFF;
    gDefaultDepthStencilState = util_to_depth_desc(&depthStateDesc);

    RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
    gDefaultRasterizer = util_to_rasterizer_desc(&rasterizerStateDesc);

    SamplerDesc samplerDesc = {};
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    addSampler(pRenderer, &samplerDesc, &pDefaultSampler);
}

static void remove_default_resources(Renderer* pRenderer)
{
    removeSampler(pRenderer, pDefaultSampler);
}

/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void gl_initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
    ASSERT(ppRenderer);
    ASSERT(pDesc);

    Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
    ASSERT(pRenderer);

    LOGF(LogLevel::eINFO, "Using OpenGL ES 2.0");
    if(!initGL(&pRenderer->mGLES.pConfig))
    {
        SAFE_FREE(pRenderer);
        return;
    }

    if (!initGLContext(pRenderer->mGLES.pConfig, &pRenderer->mGLES.pContext))
    {
        SAFE_FREE(pRenderer);
        return;
    }

    pRenderer->mRendererApi = RENDERER_API_GLES;
    pRenderer->mGpuMode = pDesc->mGpuMode;
    pRenderer->mShaderTarget = pDesc->mShaderTarget;

    pRenderer->pName = appName;

    if (!addDevice(pRenderer, pDesc))
    {
        removeGLContext(&pRenderer->mGLES.pContext);
        removeGL(&pRenderer->mGLES.pConfig);
        SAFE_FREE(pRenderer);
        pRenderer = nullptr;
        return;
    }

    add_default_resources(pRenderer);

    // Renderer is good!
    *ppRenderer = pRenderer;
}

void gl_exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    remove_default_resources(pRenderer);

    removeDevice(pRenderer);

    removeGLContext(&pRenderer->mGLES.pContext);

    removeGL(&pRenderer->mGLES.pConfig);

    // Free all the renderer components
    SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void gl_addFence(Renderer* pRenderer, Fence** ppFence)
{
    ASSERT(pRenderer);
    ASSERT(ppFence);

    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    // Fences are not available until OpenGL core 3.2
    // using glFinish for Gpu->Cpu sync

    pFence->mGLES.mSubmitted = false;
    *ppFence = pFence;
}

void gl_removeFence(Renderer* pRenderer, Fence* pFence)
{
    ASSERT(pRenderer);
    ASSERT(pFence);

    SAFE_FREE(pFence);
}

void gl_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    ASSERT(pRenderer);
    ASSERT(ppSemaphore);

    Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
    ASSERT(pSemaphore);

    // Semaphores are not available on OpenGL ES 2.0
    // using glFlush for Gpu->Gpu sync

    // Set signal initial state.
    pSemaphore->mGLES.mSignaled = false;

    *ppSemaphore = pSemaphore;
}

void gl_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    ASSERT(pRenderer);
    ASSERT(pSemaphore);

    SAFE_FREE(pSemaphore)
}

void gl_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueue);

    // NOTE: We will still use it to reference the renderer in the queue and to be able to generate
    // a dependency graph to serialize parallel GPU workload.
    Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
    ASSERT(pQueue);

    // Create state cache
    pQueue->mGLES.pCmdCache = (CmdCache*)tf_calloc(1, sizeof(CmdCache));
    ASSERT(pQueue->mGLES.pCmdCache);

    pQueue->mGLES.pCmdCache->isStarted = false;

    pQueue->mGLES.pCmdCache->mActiveIndexBuffer = GL_NONE;
    pQueue->mGLES.pCmdCache->mIndexBufferOffset = 0;

    pQueue->mGLES.pCmdCache->mActiveVertexBuffer = GL_NONE;
    pQueue->mGLES.pCmdCache->mVertexBufferCount = 0;

    //View
    pQueue->mGLES.pCmdCache->mViewport = vec4(0);
    pQueue->mGLES.pCmdCache->mScissor = vec4(0);
    pQueue->mGLES.pCmdCache->mDepthRange = vec2(0, 1);

    // RenderTargets items
    pQueue->mGLES.pCmdCache->mFramebuffer = 0;
    pQueue->mGLES.pCmdCache->mRenderTargetHeight = 0;
    pQueue->mGLES.pCmdCache->mClearColor = vec4(0);
    pQueue->mGLES.pCmdCache->mClearDepth = 1;
    pQueue->mGLES.pCmdCache->mClearStencil = 0;

    pQueue->mGLES.pCmdCache->mStencilRefValue = 0;

    for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        pQueue->mGLES.pCmdCache->mVertexAttribCache[i].mAttachedBuffer = -1;
        pQueue->mGLES.pCmdCache->mVertexAttribCache[i].mOffset = 0;
        pQueue->mGLES.pCmdCache->mVertexAttribCache[i].mIsActive = false;
    }

    // Pipeline state
    pQueue->mGLES.pCmdCache->mDepthStencilState.mDepthWrite = GL_TRUE;
    pQueue->mGLES.pCmdCache->mDepthStencilState.mStencilWriteMask = 0b11111111;
    pQueue->mGLES.pCmdCache->mPipeline = GL_NONE;
    pQueue->mGLES.pCmdCache->pRootSignature = NULL;
    pQueue->mGLES.pCmdCache->mVertexLayoutCount = 0;
    pQueue->mGLES.pCmdCache->mInstanceLocation = -1;
    pQueue->mGLES.pCmdCache->pActiveVAOStates = NULL;

    // VAO
    pQueue->mGLES.pCmdCache->mActiveVAO = GL_NONE;

    // Texture Samplers
    pQueue->mGLES.pCmdCache->pTextureSampler = (Sampler*)tf_calloc(pRenderer->pGpu->mSettings.mMaxBoundTextures, sizeof(Sampler));
    pQueue->mGLES.pCmdCache->mActiveTexture = -1;

    // Provided description for queue creation.
    // Note these don't really mean much w/ GLES but we can use it for debugging
    // what the client is intending to do.
    pQueue->mNodeIndex = pDesc->mNodeIndex;
    pQueue->mType = pDesc->mType;

    *ppQueue = pQueue;
}

void gl_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pRenderer);
    ASSERT(pQueue);

    SAFE_FREE(pQueue->mGLES.pCmdCache->pTextureSampler);
    SAFE_FREE(pQueue->mGLES.pCmdCache);

    SAFE_FREE(pQueue);
}

void gl_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);
    ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

    LOGF(LogLevel::eINFO, "Adding GLES swapchain @ %ux%u", pDesc->mWidth, pDesc->mHeight);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + sizeof(RenderTarget*));
    ASSERT(pSwapChain);

    // Set RT that use GL default framebuffer
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    ASSERT(pSwapChain->ppRenderTargets);
    pSwapChain->ppRenderTargets[0] = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
    RenderTarget* rt = pSwapChain->ppRenderTargets[0];
    ASSERT(rt);
    rt->mGLES.mType = GL_COLOR_BUFFER_BIT;
    rt->mWidth = pDesc->mWidth;
    rt->mHeight = pDesc->mHeight;

    pSwapChain->mEnableVsync = pDesc->mEnableVsync;

    // Create surface
    if (!addGLSurface(pRenderer->mGLES.pContext, pRenderer->mGLES.pConfig, &pDesc->mWindowHandle, &pSwapChain->mGLES.pSurface))
    {
        SAFE_FREE(pSwapChain);
        return;
    }

    // Set swap interval
    setGLSwapInterval(pDesc->mEnableVsync);

    // No multiple swapchains images are supported on OpenGL ES 2.0
    // Rendering is sequential
    pSwapChain->mImageCount = 1;


    *ppSwapChain = pSwapChain;
}

void gl_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[0]);

    removeGLSurface(pRenderer->mGLES.pContext, pRenderer->mGLES.pConfig, &pSwapChain->mGLES.pSurface);

    SAFE_FREE(pSwapChain);
}
/************************************************************************/
// Command Pool Functions
/************************************************************************/
void gl_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    //ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppCmdPool);

    // initialize to zero
    CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
    ASSERT(pCmdPool);

    pCmdPool->pCmdCache = pDesc->pQueue->mGLES.pCmdCache;
    pCmdPool->pQueue = pDesc->pQueue;

    *ppCmdPool = pCmdPool;
}

void gl_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    //check validity of given renderer and command pool
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    SAFE_FREE(pCmdPool);
}

void gl_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    //verify that given pool is valid
    ASSERT(pRenderer);
    ASSERT(pDesc->pPool);
    ASSERT(ppCmd);

    //allocate new command
    Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
    ASSERT(pCmd);

    //set command pool of new command
    pCmd->pRenderer = pRenderer;
    pCmd->pQueue = pDesc->pPool->pQueue;
    pCmd->mGLES.pCmdPool = pDesc->pPool;

    *ppCmd = pCmd;
}

void gl_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
    //verify that given command and pool are valid
    ASSERT(pRenderer);
    ASSERT(pCmd);

    SAFE_FREE(pCmd);
}

void gl_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
    //verify that ***cmd is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(cmdCount);
    ASSERT(pppCmd);

    Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
    ASSERT(ppCmds);

    //add n new cmds to given pool
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        ::addCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void gl_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    //verify that given command list is valid
    ASSERT(ppCmds);

    //remove every given cmd in array
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        removeCmd(pRenderer, ppCmds[i]);
    }

    SAFE_FREE(ppCmds);
}
/************************************************************************/
// All buffer, texture loading handled by resource system -> IResourceLoader.
/************************************************************************/
void gl_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppRenderTarget);

    bool const isDepth = TinyImageFormat_IsDepthAndStencil(pDesc->mFormat) ||
        TinyImageFormat_IsDepthOnly(pDesc->mFormat) || TinyImageFormat_IsStencilOnly(pDesc->mFormat);


    ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

    RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
    ASSERT(pRenderTarget);

    if (isDepth)
    {
        if (TinyImageFormat_IsDepthOnly(pDesc->mFormat))
            pRenderTarget->mGLES.mType = GL_DEPTH_BUFFER_BIT;

        if (TinyImageFormat_IsStencilOnly(pDesc->mFormat))
            pRenderTarget->mGLES.mType = GL_STENCIL_BUFFER_BIT;

        if (TinyImageFormat_IsDepthAndStencil(pDesc->mFormat))
            pRenderTarget->mGLES.mType = (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    else
    {
        pRenderTarget->mGLES.mType = GL_COLOR_BUFFER_BIT;
        // Generate new framebuffer when using a separate renderTarget
        CHECK_GLRESULT(glGenFramebuffers(1, &pRenderTarget->mGLES.mFramebuffer));
    }

    TinyImageFormat targetFormat = pDesc->mFormat;
    if (!(pRenderer->pGpu->mCapBits.mFormatCaps[pDesc->mFormat] & FORMAT_CAP_RENDER_TARGET))
    {
        if (!isDepth)
        {
            switch (TinyImageFormat_ChannelCount(pDesc->mFormat))
            {
            case 1:
                targetFormat = TinyImageFormat_R8_UNORM;
                break;
            case 3:
                targetFormat = TinyImageFormat_R8G8B8_UNORM;
                break;
            default:
                targetFormat = TinyImageFormat_R8G8B8A8_UNORM;
                break;
            }
        }
        else
        {
            targetFormat = TinyImageFormat_IsStencilOnly(pDesc->mFormat) ? TinyImageFormat_S8_UINT : TinyImageFormat_D16_UNORM;
        }

        LOGF(LogLevel::eWARNING, "RenderTarget format \"%s\" is not supported, falling back to \"%s\"", TinyImageFormat_Name(pDesc->mFormat), TinyImageFormat_Name(targetFormat));
    }

    if (gExtensionSupport.mHasDepthTextureOES || !isDepth)
    {
        TextureDesc textureDesc = {};
        textureDesc.mArraySize = pDesc->mArraySize;
        textureDesc.mClearValue = pDesc->mClearValue;
        textureDesc.mDepth = pDesc->mDepth;
        textureDesc.mFlags = pDesc->mFlags;
        textureDesc.mFormat = targetFormat;
        textureDesc.mHeight = pDesc->mHeight;
        textureDesc.mMipLevels = pDesc->mMipLevels;
        textureDesc.mSampleCount = pDesc->mSampleCount;
        textureDesc.mSampleQuality = pDesc->mSampleQuality;
        textureDesc.mWidth = pDesc->mWidth;
        textureDesc.pNativeHandle = pDesc->pNativeHandle;
        textureDesc.mNodeIndex = pDesc->mNodeIndex;
        textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
        textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;
        textureDesc.mStartState = (pDesc->mStartState | RESOURCE_STATE_RENDER_TARGET);
        textureDesc.mDescriptors = pDesc->mDescriptors;

        textureDesc.pName = pDesc->pName;
        addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);
    }
    else
    {
        GLenum internalFormat;
        GLuint typeSize, glType, glFormat;
        TinyImageFormat_ToGL_FORMAT(targetFormat, &glFormat, &glType, &internalFormat, &typeSize);

        if (pRenderTarget->mGLES.mType & GL_DEPTH_BUFFER_BIT)
        {
            CHECK_GLRESULT(glGenRenderbuffers(1, &pRenderTarget->mGLES.mDepthTarget));
            CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, pRenderTarget->mGLES.mDepthTarget));
            CHECK_GLRESULT(glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, pDesc->mWidth, pDesc->mHeight));
            CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }

        if (pRenderTarget->mGLES.mType & GL_STENCIL_BUFFER_BIT)
        {
            CHECK_GLRESULT(glGenRenderbuffers(1, &pRenderTarget->mGLES.mStencilTarget));
            CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, pRenderTarget->mGLES.mStencilTarget));
            CHECK_GLRESULT(glRenderbufferStorage(GL_RENDERBUFFER, targetFormat != TinyImageFormat_D16_UNORM ? internalFormat : GL_STENCIL_INDEX8, pDesc->mWidth, pDesc->mHeight));
            CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }
    }

    pRenderTarget->mWidth = pDesc->mWidth;
    pRenderTarget->mHeight = pDesc->mHeight;
    pRenderTarget->mArraySize = pDesc->mArraySize;
    pRenderTarget->mDepth = pDesc->mDepth;
    pRenderTarget->mMipLevels = pDesc->mMipLevels;
    pRenderTarget->mSampleCount = pDesc->mSampleCount;
    pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
    pRenderTarget->mClearValue = pDesc->mClearValue;
    pRenderTarget->mFormat = targetFormat;

    *ppRenderTarget = pRenderTarget;
}

void gl_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    if(pRenderTarget->pTexture)
        removeTexture(pRenderer, pRenderTarget->pTexture);
    if (pRenderTarget->mGLES.mDepthTarget)
        CHECK_GLRESULT(glDeleteRenderbuffers(1, &pRenderTarget->mGLES.mDepthTarget));
    if (pRenderTarget->mGLES.mStencilTarget)
        CHECK_GLRESULT(glDeleteRenderbuffers(1, &pRenderTarget->mGLES.mStencilTarget));
    if (pRenderTarget->mGLES.mFramebuffer)
        CHECK_GLRESULT(glDeleteFramebuffers(1, &pRenderTarget->mGLES.mFramebuffer));

    SAFE_FREE(pRenderTarget);
}

void gl_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    ASSERT(pRenderer);
    ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
    ASSERT(ppSampler);

    // initialize to zero
    Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
    ASSERT(pSampler);

    if (pDesc->mMinFilter == FILTER_NEAREST)
    {
        pSampler->mGLES.mMinFilter = GL_NEAREST;
        pSampler->mGLES.mMipMapMode = pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    }
    else
    {
        pSampler->mGLES.mMinFilter = GL_LINEAR;
        pSampler->mGLES.mMipMapMode = pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }
    pSampler->mGLES.mMagFilter = pDesc->mMagFilter == FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;

    pSampler->mGLES.mAddressS = util_to_gl_address_mode(pDesc->mAddressU);
    pSampler->mGLES.mAddressT = util_to_gl_address_mode(pDesc->mAddressV);
    pSampler->mGLES.mCompareFunc = util_to_gl_compare_mode(pDesc->mCompareFunc);

    // GL_TEXTURE_MIN_FILTER Options:
    // GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
    // GL_TEXTURE_MAG_FILTER Options:
    // GL_NEAREST, GL_LINEAR
    // GL_TEXTURE_WRAP_S & GL_TEXTURE_WRAP_T Options:
    // GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, GL_REPEAT

    *ppSampler = pSampler;
}

void gl_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pRenderer);
    ASSERT(pSampler);

    SAFE_FREE(pSampler);
}

/************************************************************************/
// Shader Functions
/************************************************************************/
bool gl_compileShader(
    Renderer* pRenderer, ShaderStage stage,
    const char* fileName, uint32_t codeSize, const char* code,
    BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
    ASSERT(code);
    ASSERT(codeSize > 0);
    ASSERT(pOut);

    if (stage != SHADER_STAGE_VERT && stage != SHADER_STAGE_FRAG)
    {
        LOGF(LogLevel::eERROR, "Unsupported shader stage {%u} for OpenGL ES 2.0!", stage);
        return false;
    }

    // Combine shader code
    uint32_t outCodeSize = 0;
    char* outCode = NULL;

    uint32_t codeOffset = 0;
    uint32_t outCodeOffset = 0;

    const char* version = strstr(code, "#version");
    if (version)
    {
        codeOffset = (version - code);
        outCodeSize += codeSize - codeOffset;
    }
    else
    {
        outCodeSize += codeSize;
    }

    outCode = (char*)tf_calloc(1, outCodeSize);

    // Copy shader code remains
    memcpy(outCode + outCodeOffset, (void*)(code + codeOffset), codeSize - codeOffset);


    LOGF(LogLevel::eDEBUG, "Add shader {%s}", fileName);
    GLint compiled = GL_FALSE;
    GLuint shader = GL_NONE;
    CHECK_GL_RETURN_RESULT(shader, glCreateShader(stage == SHADER_STAGE_VERT ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER));

    // Load the shader source
    CHECK_GLRESULT(glShaderSource(shader, 1, (const GLchar*const*)&outCode, (GLint*)&outCodeSize));
    CHECK_GLRESULT(glCompileShader(shader));

    // Check the compile status
    CHECK_GLRESULT(glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled));
    if (!compiled)
    {
        GLint infoLen = 0;
        CHECK_GLRESULT(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen));
        if (infoLen > 1)
        {
            char* infoLog = nullptr;
            infoLog = (char*)tf_calloc(infoLen + 1, sizeof(char));
            CHECK_GLRESULT(glGetShaderInfoLog(shader, infoLen, nullptr, (GLchar*)infoLog));
            LOGF(LogLevel::eERROR, "Error compiling shader:\n%s\n", infoLog);
            tf_free(infoLog);
        }
        else
        {
            LOGF(LogLevel::eERROR, "Error compiling shader: No InfoLog available");
        }

        glDeleteShader(shader);
        ASSERT(false);
        return false;
    }

    pOut->mShader = shader;

    // For now pass on the shader code as "bytecode"
    pOut->pByteCode = outCode;
    pOut->mByteCodeSize = outCodeSize;
    return true;
}

void gl_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mStages);
    ASSERT(ppShaderProgram);
    ASSERT(pDesc->mStages & SHADER_STAGE_VERT && "OpenGL ES 2.0 requires a vertex shader");
    ASSERT(pDesc->mStages & SHADER_STAGE_FRAG && "OpenGL ES 2.0 requires a fragment shader");

    Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
    ASSERT(pShaderProgram);

    // Create GL shader program
    CHECK_GL_RETURN_RESULT(pShaderProgram->mGLES.mProgram, glCreateProgram());

    pShaderProgram->mStages = pDesc->mStages;
    pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage                  stage_mask = (ShaderStage)(1 << i);
        const BinaryShaderStageDesc* pStage = NULL;

        if (stage_mask != (pShaderProgram->mStages & stage_mask))
            continue;

        ASSERT((stage_mask == SHADER_STAGE_VERT || stage_mask == SHADER_STAGE_FRAG) && "Only vertex and fragment shaders are supported for OpenGL ES 2.0");
        if (stage_mask == SHADER_STAGE_VERT)
            pStage = &pDesc->mVert;
        else
            pStage = &pDesc->mFrag;

        CHECK_GLRESULT(glAttachShader(pShaderProgram->mGLES.mProgram, pStage->mShader));
        CHECK_GLRESULT(glDeleteShader(pStage->mShader));
    }

    // Validate GL shader program
    if (!util_link_and_validate_program(pShaderProgram->mGLES.mProgram))
    {
        CHECK_GLRESULT(glDeleteProgram(pShaderProgram->mGLES.mProgram));
        ASSERT(false);
        return;
    }

    // OpenGL ES 2.0 reflection is done over a shader program instead for each individual shader stage
    // Therefore only one reflection exist.
    gles_createShaderReflection(pShaderProgram, pShaderProgram->pReflection->mStageReflections, pDesc);

    createPipelineReflection(pShaderProgram->pReflection->mStageReflections, 1, pShaderProgram->pReflection);

    *ppShaderProgram = pShaderProgram;
}

void gl_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    UNREF_PARAM(pRenderer);

    CHECK_GLRESULT(glDeleteProgram(pShaderProgram->mGLES.mProgram));

    //remove given shader
    destroyPipelineReflection(pShaderProgram->pReflection);

    SAFE_FREE(pShaderProgram);
}

void gl_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pDesc->mSize > 0);
    ASSERT(ppBuffer);

    // initialize to zero
    Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
    ASSERT(pBuffer);

    pBuffer->mGLES.pGLCpuMappedAddress = nullptr;

    // Only options for GLES 2.0
    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER) || (pDesc->mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER))
    {
        pBuffer->mGLES.mTarget = (pDesc->mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER) ? GL_ELEMENT_ARRAY_BUFFER :  GL_ARRAY_BUFFER;
        pBuffer->mMemoryUsage = pDesc->mMemoryUsage;

        GLint usage = util_to_gl_usage(pDesc->mMemoryUsage);
        if (usage != GL_NONE)
        {
            CHECK_GLRESULT(glGenBuffers(1, &pBuffer->mGLES.mBuffer));
            CHECK_GLRESULT(glBindBuffer(pBuffer->mGLES.mTarget, pBuffer->mGLES.mBuffer));
            CHECK_GLRESULT(glBufferData(pBuffer->mGLES.mTarget, pDesc->mSize, NULL, usage));
            CHECK_GLRESULT(glBindBuffer(pBuffer->mGLES.mTarget, GL_NONE));
        }

        if (pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
        {
            pBuffer->mGLES.pGLCpuMappedAddress = tf_malloc(pDesc->mSize);
        }
    }
    else
    {
        uint64_t allocationSize = pDesc->mSize;
        if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            allocationSize = round_up_64(allocationSize, pRenderer->pGpu->mSettings.mUniformBufferAlignment);
        }
        pBuffer->mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
        pBuffer->mGLES.pGLCpuMappedAddress = tf_malloc(allocationSize);
        pBuffer->mGLES.mTarget = GL_NONE;
    }

    if (pDesc->pName)
    {
        setBufferName(pRenderer, pBuffer, pDesc->pName);
    }

    pBuffer->mSize = (uint32_t)pDesc->mSize;
    pBuffer->mNodeIndex = pDesc->mNodeIndex;
    pBuffer->mDescriptors = pDesc->mDescriptors;
    pBuffer->pCpuMappedAddress = nullptr;

    *ppBuffer = pBuffer;
}

void gl_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pBuffer);

    if(pBuffer->mGLES.mBuffer)
    {
        CHECK_GLRESULT(glDeleteBuffers(1, &pBuffer->mGLES.mBuffer));
    }

    SAFE_FREE(pBuffer->mGLES.pGLCpuMappedAddress);

    SAFE_FREE(pBuffer);
}

void gl_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");
    if(pBuffer->mGLES.mBuffer)
    {
        CHECK_GLRESULT(glBindBuffer(pBuffer->mGLES.mTarget, pBuffer->mGLES.mBuffer));
        if (gExtensionSupport.mHasMapbufferOES)
        {
            CHECK_GL_RETURN_RESULT(pBuffer->pCpuMappedAddress, glMapBufferOES(pBuffer->mGLES.mTarget, GL_WRITE_ONLY_OES));
        }
        else
        {
            pBuffer->pCpuMappedAddress = pBuffer->mGLES.pGLCpuMappedAddress;
        }
    }
    else
    {
        pBuffer->pCpuMappedAddress = ((uint8_t*)pBuffer->mGLES.pGLCpuMappedAddress + (pRange ? pRange->mOffset : 0));
    }
}

void gl_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

    pBuffer->pCpuMappedAddress = nullptr;

    if (pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_CPU_ONLY)
    {
        if (gExtensionSupport.mHasMapbufferOES)
        {
            CHECK_GLRESULT(glUnmapBufferOES(pBuffer->mGLES.mTarget));
        }
        else
        {
            CHECK_GLRESULT(glBufferSubData(pBuffer->mGLES.mTarget, 0, pBuffer->mSize, pBuffer->mGLES.pGLCpuMappedAddress));
        }
    }
}

void gl_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
    ASSERT(ppTexture);

    if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
    {
        LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
        ASSERT(false);
        return;
    }

    if (!isPowerOf2(pDesc->mWidth) || !isPowerOf2(pDesc->mHeight))
    {
        LOGF(LogLevel::eWARNING, "Texture \"%s\" dimension is not a power of 2 w: %u, h: %u", pDesc->pName, pDesc->mWidth, pDesc->mHeight);
    }

    TinyImageFormat format = pDesc->mFormat;
    // Only OpenGL ES 2.1 supports SRGB textures, but we use OpenGL ES 2.0
    if (TinyImageFormat_IsSRGB(format))
        format = TinyImageFormat_ToUNORM(format);
    // Check image support
    if (!(pRenderer->pGpu->mCapBits.mFormatCaps[format] & FORMAT_CAP_READ))
    {
        if (TinyImageFormat_IsCompressed(format))
        {
            LOGF(LogLevel::eERROR, "Compressed format \"%s\" is not supported!", TinyImageFormat_Name(format));
            return;
        }

        switch (TinyImageFormat_ChannelCount(pDesc->mFormat))
        {
        case 1:
            format = TinyImageFormat_R8_UNORM;
            break;
        case 3:
            format = TinyImageFormat_R8G8B8_UNORM;
            break;
        default:
            format = TinyImageFormat_R8G8B8A8_UNORM;
            break;
        }

        LOGF(LogLevel::eWARNING, "Texture format \"%s\" is not supported, falling back to \"%s\"", TinyImageFormat_Name(pDesc->mFormat), TinyImageFormat_Name(format));
    }

    // initialize to zero
    Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(Texture));
    ASSERT(pTexture);

    GLuint typeSize;
    TinyImageFormat_ToGL_FORMAT(format, &pTexture->mGLES.mGlFormat, &pTexture->mGLES.mType, &pTexture->mGLES.mInternalFormat, &typeSize);

    CHECK_GLRESULT(glGenTextures(1, &pTexture->mGLES.mTexture));
    if(DESCRIPTOR_TYPE_TEXTURE_CUBE == (pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
        pTexture->mGLES.mTarget = GL_TEXTURE_CUBE_MAP;
    else
        pTexture->mGLES.mTarget = GL_TEXTURE_2D;

    pTexture->mNodeIndex = pDesc->mNodeIndex;
    pTexture->mMipLevels = pDesc->mMipLevels;
    pTexture->mWidth = pDesc->mWidth;
    pTexture->mHeight = pDesc->mHeight;
    pTexture->mDepth = pDesc->mDepth;
    pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;
    pTexture->mFormat = format;
    pTexture->mUav = false;
    pTexture->mOwnsImage = false;
    pTexture->mGLES.mStateModified = false;
    if ((pDesc->mStartState & RESOURCE_STATE_RENDER_TARGET))
    {
        CHECK_GLRESULT(glBindTexture(pTexture->mGLES.mTarget, pTexture->mGLES.mTexture));
        //TODO Set all miplevels
        CHECK_GLRESULT(glTexImage2D(pTexture->mGLES.mTarget, 0, pTexture->mGLES.mGlFormat,
            pTexture->mWidth, pTexture->mHeight, 0, pTexture->mGLES.mGlFormat, pTexture->mGLES.mType, NULL));
        CHECK_GLRESULT(glBindTexture(pTexture->mGLES.mTarget, 0));

        if (pDesc->pName)
        {
            setTextureName(pRenderer, pTexture, pDesc->pName);
        }
    }

    *ppTexture = pTexture;
}

void gl_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pTexture);

    if (pTexture->mGLES.mTexture)
    {
        CHECK_GLRESULT(glDeleteTextures(1, &pTexture->mGLES.mTexture));
    }

    SAFE_FREE(pTexture);
}

/************************************************************************/
// Pipeline Functions
/************************************************************************/
typedef struct GlVariable
{
    uint32_t mIndexInParent;
    uint32_t mSize;
    GLenum	 mType;
    int32_t* pGlLocations;
} GlVariable;

ShaderVariable* util_lookup_shader_variable(const char* name, PipelineReflection* pReflection)
{
    for (uint32_t index = 0; index < pReflection->mVariableCount; ++index)
    {
        ShaderVariable* variable = &pReflection->pVariables[index];
        if (strcmp(name, variable->name) == 0)
        {
            return variable;
        }
    }
    return nullptr;
}

ShaderResource* util_lookup_shader_resource(const char* name, PipelineReflection* pReflection)
{
    for (uint32_t index = 0; index < pReflection->mShaderResourceCount; ++index)
    {
        ShaderResource* resource = &pReflection->pShaderResources[index];
        if (strcmp(name, resource->name) == 0)
        {
            return resource;
        }
    }
    return nullptr;
}

static bool lessShaderVariable(const ShaderVariable* a, const ShaderVariable* b) {
    return a->parent_index <= b->parent_index && a->offset < b->offset;
}

DEFINE_SIMPLE_SORT_FUNCTION(static, simpleSortShaderVariable, ShaderVariable, lessShaderVariable)
DEFINE_INSERTION_SORT_FUNCTION(static, stableSortShaderVariable, ShaderVariable, lessShaderVariable, simpleSortShaderVariable)
DEFINE_PARTITION_IMPL_FUNCTION(static, partitionImplShaderVariable, ShaderVariable, lessShaderVariable)
DEFINE_QUICK_SORT_IMPL_FUNCTION(static, quickSortShaderVariable, ShaderVariable, lessShaderVariable, stableSortShaderVariable, partitionImplShaderVariable)
DEFINE_QUICK_SORT_FUNCTION(static, sortShaderVariable, ShaderVariable, quickSortShaderVariable)

void gl_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
    ASSERT(pRenderer);
    ASSERT(pRootSignatureDesc);
    ASSERT(ppRootSignature);
    ASSERT(pRootSignatureDesc->mShaderCount < 32);
    ShaderResource*		shaderResources = NULL;
    uint32_t*			uboVariableSizes = NULL;
    ShaderVariable*		shaderVariables = NULL;

    DescriptorIndexMap*	indexMap = NULL;
    sh_new_arena(indexMap);

    uint32_t uniqueTextureCount = 0;

    // Collect all unique shader resources in the given shaders
    // Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
    for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
    {
        PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;
        if (pReflection->mShaderStages & SHADER_STAGE_COMP)
        {
            LOGF(LogLevel::eERROR, "Compute shader not supported on OpenGL ES 2.0");
        }

        for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
        {
            ShaderResource const* pRes = &pReflection->pShaderResources[i];

            // Find all unique resources
            DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);
            if (pNode == NULL)
            {
                shput(indexMap, pRes->name, (uint32_t)arrlen(shaderResources));
                arrpush(shaderResources, *pRes);

                uint32_t uboVariableSize = 0;
                switch (pRes->type)
                {
                case DESCRIPTOR_TYPE_BUFFER:
                case DESCRIPTOR_TYPE_RW_BUFFER:
                case DESCRIPTOR_TYPE_BUFFER_RAW:
                case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                    break;
                case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                {
                    for (uint32_t v = 0; v < pReflection->mVariableCount; ++v)
                    {
                        if (pReflection->pVariables[v].parent_index == i)
                        {
                            ++uboVariableSize;
                            arrpush(shaderVariables, pReflection->pVariables[v]);
                        }
                    }
                    break;
                }
                case DESCRIPTOR_TYPE_TEXTURE:
                case DESCRIPTOR_TYPE_RW_TEXTURE:
                case DESCRIPTOR_TYPE_TEXTURE_CUBE:
                {
                    uniqueTextureCount += pRes->size;
                    break;
                }
                default: break;
                }

                arrpush(uboVariableSizes, uboVariableSize);
            }
            else
            {
                // Search for new shader variables within the uniform block
                if (pRes->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    uint32_t uboVariableOffset = 0;
                    ptrdiff_t currentIndex = 0;
                    // Get current offset in shaderVariables list
                    for (; currentIndex < arrlen(shaderResources); ++currentIndex)
                    {
                        if (strcmp(shaderResources[currentIndex].name, pRes->name) == 0)
                        {
                            break;
                        }
                        uboVariableOffset += uboVariableSizes[currentIndex];
                    }

                    for (uint32_t v = 0; v < pReflection->mVariableCount; ++v)
                    {
                        ShaderVariable* curVariable = &pReflection->pVariables[v];
                        if (curVariable->parent_index == i)
                        {
                            uint32_t insertLocation = 0;
                            bool isIncluded = false;
                            // Check if variable is already included in the UBO and match size/type
                            for (uint32_t uboIndex = 0; uboIndex < uboVariableSizes[currentIndex]; ++uboIndex)
                            {
                                ShaderVariable* storedVariable = &shaderVariables[uboVariableOffset + uboIndex];
                                if (strcmp(curVariable->name, storedVariable->name) == 0)
                                {
                                    if (storedVariable->size != curVariable->size || storedVariable->type != curVariable->type)
                                    {
                                        LOGF(LogLevel::eERROR, "Shader variable \"%s\" has unmatching type {%u} != {%u} or size {%u} != {%u}, within the root descriptor!",
                                            storedVariable->name, storedVariable->type, curVariable->type, storedVariable->size, curVariable->size);
                                        ASSERT(false);
                                    }
                                    isIncluded = true;
                                    break;
                                }
                                else if (storedVariable->offset <= curVariable->offset) // Approximate store location, by appending at the end if offset (GlLocation) is higher.
                                {
                                    ++insertLocation;
                                }
                            }

                            if (!isIncluded)
                            {
                                arrins(shaderVariables, uboVariableOffset + insertLocation, pReflection->pVariables[v]);
                                ++uboVariableSizes[currentIndex];
                            }
                        }
                    }
                }
                else
                {
                    if(shaderResources[pNode->value].set != pRes->set)
                    {
                        LOGF(eERROR,
                            "\nFailed to create root signature\n"
                            "Shared shader resource %s has mismatching type. All shader resources "
                            "shared by multiple shaders specified in addRootSignature "
                            "have the same type",
                            pRes->name);
                        return;
                    }
                    // set resource to largest found size
                    shaderResources[pNode->value].size = max(shaderResources[pNode->value].size, pRes->size);
                }
            }
        }
    }

    if (uniqueTextureCount >= pRenderer->pGpu->mSettings.mMaxBoundTextures)
    {
        LOGF(LogLevel::eERROR, "Exceed maximum amount of texture units! required: {%d}, max: {%d}", uniqueTextureCount, pRenderer->pGpu->mSettings.mMaxBoundTextures);
        ASSERT(false);
    }

    uint32_t totalSize = sizeof(RootSignature);

    // Order members by decreasing alignment
    COMPILE_ASSERT(alignof(RootSignature) >= alignof(DescriptorInfo));
    COMPILE_ASSERT(alignof(DescriptorInfo) >= alignof(GlVariable));
    COMPILE_ASSERT(alignof(GlVariable) >= alignof(uint32_t));

    totalSize += arrlen(shaderResources) * sizeof(DescriptorInfo);
    totalSize += arrlen(shaderVariables) * sizeof(GlVariable);

    totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount;
    totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount * arrlen(shaderResources);
    totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount * arrlen(shaderVariables);

    sortShaderVariable(shaderVariables, arrlenu(shaderVariables));

    RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
    ASSERT(pRootSignature);

    pRootSignature->mGLES.mProgramCount = pRootSignatureDesc->mShaderCount;
    pRootSignature->mDescriptorCount = (uint32_t)arrlen(shaderResources);
    pRootSignature->mGLES.mVariableCount = (uint32_t)arrlen(shaderVariables);

    pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);
    ASSERT((uintptr_t)pRootSignature->pDescriptors % alignof(DescriptorInfo) == 0);

    pRootSignature->mGLES.pVariables = (GlVariable*)(pRootSignature->pDescriptors + pRootSignature->mDescriptorCount);
    ASSERT((uintptr_t)pRootSignature->mGLES.pVariables % alignof(GlVariable) == 0);

    pRootSignature->mGLES.pProgramTargets = (uint32_t*)(pRootSignature->mGLES.pVariables + pRootSignature->mGLES.mVariableCount);
    pRootSignature->mGLES.pDescriptorGlLocations = (int32_t*)(pRootSignature->mGLES.pProgramTargets + pRootSignature->mGLES.mProgramCount);
    int32_t* mem = (int32_t*)(pRootSignature->mGLES.pDescriptorGlLocations + pRootSignature->mGLES.mProgramCount * pRootSignature->mDescriptorCount);

    for (uint32_t i = 0; i < pRootSignature->mGLES.mVariableCount; ++i)
    {
        pRootSignature->mGLES.pVariables[i].pGlLocations = mem;
        mem += pRootSignature->mGLES.mProgramCount;
    }

    pRootSignature->pDescriptorNameToIndexMap = indexMap;
    pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;

    // Set shader program targets
    for (uint32_t sh = 0; sh < pRootSignature->mGLES.mProgramCount; ++sh)
    {
        pRootSignature->mGLES.pProgramTargets[sh] = pRootSignatureDesc->ppShaders[sh]->mGLES.mProgram;
    }

    // Set shader variables
    for (uint32_t i = 0; i < pRootSignature->mGLES.mVariableCount; ++i)
    {
        GlVariable* pVar = &pRootSignature->mGLES.pVariables[i];
        ShaderVariable* pRes = &shaderVariables[i];
        pVar->mSize = pRes->size;
        pVar->mType = pRes->type;

        LOGF(LogLevel::eDEBUG, "%u: Shader Variable \"%s\" offset: %u", i, pRes->name, pRes->offset);

        // Get locations per shader program, because this can differ if one of the variables in an uniform block is not used.
        for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
        {
            CHECK_GL_RETURN_RESULT(pVar->pGlLocations[sh], glGetUniformLocation(pRootSignatureDesc->ppShaders[sh]->mGLES.mProgram, pRes->name));
        }
    }

    uint32_t variableIndex = 0;

    // Only one sampler can be applied in a rootSignature for GLES
    if (pRootSignatureDesc->mStaticSamplerCount > 1)
        LOGF(LogLevel::eWARNING, "Only one static sampler can be applied within a rootSignature when using OpenGL ES 2.0. Requested samplers {%u}", pRootSignatureDesc->mStaticSamplerCount);
    pRootSignature->mGLES.pSampler = pRootSignatureDesc->mStaticSamplerCount > 0 ? pRootSignatureDesc->ppStaticSamplers[0] : pDefaultSampler;

    // Fill the descriptor array to be stored in the root signature
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
        ShaderResource* pRes = &shaderResources[i];

        pDesc->mSize = pRes->size;
        pDesc->mType = pRes->type;
        pDesc->mGLES.mGlType = pRes->set; // Not available for GLSL 100 we used it for glType
        pDesc->pName = pRes->name;
        pDesc->mHandleIndex = i;
        pDesc->mRootDescriptor = isDescriptorRootCbv(pRes->name);
        for (uint32_t sh = 0; sh < pRootSignature->mGLES.mProgramCount; ++sh)
        {
            uint32_t index = i + sh * pRootSignature->mDescriptorCount;
            pRootSignature->mGLES.pDescriptorGlLocations[index] = -1;
            ShaderResource* resource = util_lookup_shader_resource(pRes->name, pRootSignatureDesc->ppShaders[sh]->pReflection);
            if (resource)
                pRootSignature->mGLES.pDescriptorGlLocations[index] = resource->reg;
        }
        //pDesc->mIndexInParent;
        //pDesc->mUpdateFrequency = updateFreq; //TODO see if we can use this within GLES

        if(pDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            pDesc->mGLES.mVariableStart = variableIndex; // Set start location of variables in this UBO
            uint32_t uboSize = 0;
            for (uint32_t var = variableIndex; var < variableIndex + uboVariableSizes[i]; ++var)
            {
                pRootSignature->mGLES.pVariables[var].mIndexInParent = i;
                uboSize += pRootSignature->mGLES.pVariables[var].mSize;
            }
            pDesc->mGLES.mUBOSize = uboSize;

            variableIndex += uboVariableSizes[i];
        }
    }

    *ppRootSignature = pRootSignature;
    arrfree(shaderResources);
    arrfree(uboVariableSizes);
    arrfree(shaderVariables);
}

void gl_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
    shfree(pRootSignature->pDescriptorNameToIndexMap);
    SAFE_FREE(pRootSignature);
}

uint32_t gl_getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        if (!strcmp(pName, pRootSignature->pDescriptors[i].pName))
        {
            return i;
        }
    }

    return UINT32_MAX;
}

void addGraphicsPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pDesc);
    ASSERT(pDesc->pShaderProgram);
    ASSERT(pDesc->pRootSignature);

    const Shader*       pShaderProgram = pDesc->pShaderProgram;
    const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

    uint32_t attrib_count = 0;

    // Make sure there's attributes
    if (pVertexLayout != NULL)
    {
        ASSERT(pVertexLayout->mAttribCount < pRenderer->pGpu->mSettings.mMaxVertexInputBindings);
        attrib_count = min(pVertexLayout->mAttribCount, pRenderer->pGpu->mSettings.mMaxVertexInputBindings);
    }

    size_t totalSize = sizeof(Pipeline);
    totalSize += sizeof(GlVertexAttrib) * attrib_count;
    totalSize += (gExtensionSupport.mHasVertexArrayObjectOES ? sizeof(GLVAOState) * VAO_STATE_CACHE_SIZE : 0);
    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), totalSize);
    ASSERT(pPipeline);

    pPipeline->mGLES.mVertexLayoutSize = attrib_count;
    pPipeline->mGLES.pVertexLayout = (GlVertexAttrib*)(pPipeline + 1);
    pPipeline->mGLES.pVAOState = NULL;
    pPipeline->mGLES.mVAOStateCount = 0;
    pPipeline->mGLES.mVAOStateLoop = 0;

    // Generate VAO
    if (gExtensionSupport.mHasVertexArrayObjectOES)
    {
        pPipeline->mGLES.pVAOState = (GLVAOState*)(pPipeline->mGLES.pVertexLayout + attrib_count);

        for (uint32_t stateIndex = 0; stateIndex < VAO_STATE_CACHE_SIZE; ++stateIndex)
        {
            pPipeline->mGLES.pVAOState[stateIndex].mActiveIndexBuffer = GL_NONE;
            for (uint32_t vertexAttribLocation = 0; vertexAttribLocation < MAX_VERTEX_ATTRIBS; ++vertexAttribLocation)
            {
                pPipeline->mGLES.pVAOState[stateIndex].mAttachedBuffers[vertexAttribLocation] = -1;
                pPipeline->mGLES.pVAOState[stateIndex].mBufferOffsets[vertexAttribLocation] = 0;
            }
            pPipeline->mGLES.pVAOState[stateIndex].mAttachedBufferCount = 0;
            pPipeline->mGLES.pVAOState[stateIndex].mVAO = 0;
            pPipeline->mGLES.pVAOState[stateIndex].mId = 0;
        }
    }

    pPipeline->mGLES.mType = PIPELINE_TYPE_GRAPHICS;

    pPipeline->mGLES.pRootSignature = pDesc->pRootSignature;
    for (uint32_t i = 0; i < pDesc->pRootSignature->mGLES.mProgramCount; ++i)
    {
        if (pDesc->pRootSignature->mGLES.pProgramTargets[i] == pDesc->pShaderProgram->mGLES.mProgram)
        {
            pPipeline->mGLES.mRootSignatureIndex = i;
            break;
        }
    }

    attrib_count = 0;

    if (pVertexLayout != NULL)
    {
        for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->mAttribCount; ++attrib_index)
        {
            // Add vertex layouts
            const VertexAttrib* attrib = &(pVertexLayout->mAttribs[attrib_index]);
            const char* semanticName = nullptr;
            if (attrib->mSemanticNameLength > 0)
            {
                semanticName = attrib->mSemanticName;
            }
            else
            {
                switch (attrib->mSemantic)
                {
                case SEMANTIC_POSITION: semanticName = "Position"; break;
                case SEMANTIC_NORMAL: semanticName = "Normal"; break;
                case SEMANTIC_COLOR: semanticName = "Color"; break;
                case SEMANTIC_TANGENT: semanticName = "Tangent"; break;
                case SEMANTIC_BITANGENT: semanticName = "Binormal"; break;
                case SEMANTIC_JOINTS: semanticName = "Joints"; break;
                case SEMANTIC_WEIGHTS: semanticName = "Weights"; break;
                case SEMANTIC_TEXCOORD0: semanticName = "UV"; break;
                case SEMANTIC_TEXCOORD1: semanticName = "UV1"; break;
                case SEMANTIC_TEXCOORD2: semanticName = "UV2"; break;
                case SEMANTIC_TEXCOORD3: semanticName = "UV3"; break;
                case SEMANTIC_TEXCOORD4: semanticName = "UV4"; break;
                case SEMANTIC_TEXCOORD5: semanticName = "UV5"; break;
                case SEMANTIC_TEXCOORD6: semanticName = "UV6"; break;
                case SEMANTIC_TEXCOORD7: semanticName = "UV7"; break;
                case SEMANTIC_TEXCOORD8: semanticName = "UV8"; break;
                case SEMANTIC_TEXCOORD9: semanticName = "UV9"; break;
                default: break;
                }
                ASSERT(semanticName);
            }

            // Find the desired vertex input location in the shader program for given semantic name
            // NOTE: Semantic names must match the attribute name in the .vert shader!
            GLint vertexLocation = -1;
            CHECK_GL_RETURN_RESULT(vertexLocation, glGetAttribLocation(pShaderProgram->mGLES.mProgram, semanticName));
            if (vertexLocation < 0)
            {
                LOGF(LogLevel::eWARNING, "No vertex location found in program {%d} for semantic name \"%s\"", pShaderProgram->mGLES.mProgram, semanticName);
            }

            GlVertexAttrib* vertexAttrib = &pPipeline->mGLES.pVertexLayout[attrib_index];
            uint32_t glFormat, glInternalFormat, typeSize;

            vertexAttrib->mIndex = vertexLocation;
            vertexAttrib->mSize = TinyImageFormat_ChannelCount(attrib->mFormat);
            TinyImageFormat_ToGL_FORMAT(attrib->mFormat, &glFormat, &vertexAttrib->mType, &glInternalFormat, &typeSize);
            vertexAttrib->mStride = TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8;
            vertexAttrib->mNormalized = TinyImageFormat_IsNormalised(attrib->mFormat);
            vertexAttrib->mOffset = attrib->mOffset;
        }
    }

    // Set texture units of current used shader program
    CHECK_GLRESULT(glUseProgram(pShaderProgram->mGLES.mProgram));
    uint32_t uniqueTextureID = 0;
    for (uint32_t i = 0; i < pDesc->pRootSignature->mDescriptorCount; ++i)
    {
        DescriptorInfo* descInfo = &pDesc->pRootSignature->pDescriptors[i];
        if (descInfo->mType == DESCRIPTOR_TYPE_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_RW_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_TEXTURE_CUBE)
        {
            ShaderResource* resource = util_lookup_shader_resource(descInfo->pName, pShaderProgram->pReflection);
            if (resource)
            {
                for (uint32_t arr = 0; arr < resource->size; ++arr)
                {
                    uint32_t textureTarget = uniqueTextureID + arr;
                    util_gl_set_uniform(resource->reg + arr, (uint8_t*)&textureTarget, GL_SAMPLER_2D, 1); // Attach sampler2D uniform to texture unit (only needed once)
                }
            }
            uniqueTextureID += descInfo->mSize;
        }
    }

    pPipeline->mGLES.pRasterizerState = (GLRasterizerState*)tf_calloc(1, sizeof(GLRasterizerState));
    ASSERT(pPipeline->mGLES.pRasterizerState);
    *pPipeline->mGLES.pRasterizerState = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizer;

    pPipeline->mGLES.pDepthStencilState = (GLDepthStencilState*)tf_calloc(1, sizeof(GLDepthStencilState));
    ASSERT(pPipeline->mGLES.pDepthStencilState);
    *pPipeline->mGLES.pDepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthStencilState;

    pPipeline->mGLES.pBlendState = (GLBlendState*)tf_calloc(1, sizeof(GLBlendState));
    ASSERT(pPipeline->mGLES.pBlendState);
    *pPipeline->mGLES.pBlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendState;

    GLenum topology;
    switch (pDesc->mPrimitiveTopo)
    {
        case PRIMITIVE_TOPO_POINT_LIST: topology = GL_POINTS; break;
        case PRIMITIVE_TOPO_LINE_LIST: topology = GL_LINES; break;
        case PRIMITIVE_TOPO_LINE_STRIP: topology = GL_LINE_STRIP; break;
        case PRIMITIVE_TOPO_TRI_LIST: topology = GL_TRIANGLES; break;
        case PRIMITIVE_TOPO_TRI_STRIP: topology = GL_TRIANGLE_STRIP; break;
        case PRIMITIVE_TOPO_PATCH_LIST:
        default: ASSERT(false && "Unsupported primitive topo");  topology = GL_TRIANGLE_STRIP;  break;
    }
    pPipeline->mGLES.mGlPrimitiveTopology = topology;

    *ppPipeline = pPipeline;
}

void gl_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pDesc);

    if (pDesc->mType != PIPELINE_TYPE_GRAPHICS)
    {
        LOGF(LogLevel::eERROR, "Pipeline {%u} not supported on OpenGL ES 2.0", pDesc->mType);
        return;
    }

    addGraphicsPipeline(pRenderer, &pDesc->mGraphicsDesc, ppPipeline);
}

void gl_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);

    if (pPipeline->mGLES.pVAOState)
    {
        for (uint32_t stateIndex = 0; stateIndex < VAO_STATE_CACHE_SIZE; ++stateIndex)
        {
            if (pPipeline->mGLES.pVAOState[stateIndex].mVAO)
            {
                CHECK_GLRESULT(glDeleteVertexArraysOES(1, &pPipeline->mGLES.pVAOState[stateIndex].mVAO));
            }
        }
    }

    SAFE_FREE(pPipeline->mGLES.pBlendState);
    SAFE_FREE(pPipeline->mGLES.pDepthStencilState);
    SAFE_FREE(pPipeline->mGLES.pRasterizerState);
    SAFE_FREE(pPipeline);
}

void gl_addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**)
{
    // Unavailable in OpenGL ES 2.0
}

void gl_removePipelineCache(Renderer*, PipelineCache*)
{
    // Unavailable in OpenGL ES 2.0
}

void gl_getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*)
{
    // Unavailable in OpenGL ES 2.0
}
/************************************************************************/
// Descriptor Set Implementation
/************************************************************************/
const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
    DescriptorIndexMap* it = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pResName);
    if (it != NULL)
    {
        return &pRootSignature->pDescriptors[it->value];
    }
    else
    {
        return NULL;
    }
}

typedef struct BufferDescriptorHandle
{
    void* pBuffer;
    uint32_t mOffset;
} BufferDescriptorHandle;

typedef struct TextureDescriptorHandle
{
    bool hasMips;
    bool mStateModified;
    GLuint mTexture;
} TextureDescriptorHandle;

typedef struct DescriptorHandle
{
    union
    {
        struct BufferDescriptorHandle* pBufferHandles;
        struct TextureDescriptorHandle* pTextures;
    };
} DescriptorHandle;

typedef struct DescriptorDataArray
{
    struct DescriptorHandle*	pData;
} DescriptorDataArray;


void gl_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);

    const RootSignature* pRootSignature = pDesc->pRootSignature;
    size_t totalSize = sizeof(DescriptorSet);
    totalSize += pDesc->mMaxSets * sizeof(DescriptorDataArray);
    totalSize += pDesc->mMaxSets * pRootSignature->mDescriptorCount * sizeof(DescriptorHandle);
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        switch (pRootSignature->pDescriptors[i].mType)
        {
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            totalSize += pDesc->mMaxSets * sizeof(BufferDescriptorHandle);
            break;
        case DESCRIPTOR_TYPE_TEXTURE:
        case DESCRIPTOR_TYPE_RW_TEXTURE:
        case DESCRIPTOR_TYPE_TEXTURE_CUBE:
            totalSize += pDesc->mMaxSets * pRootSignature->pDescriptors[i].mSize * sizeof(TextureDescriptorHandle);
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            totalSize += pDesc->mMaxSets * pRootSignature->pDescriptors[i].mSize * sizeof(BufferDescriptorHandle);
            break;
        }
    }
    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
    ASSERT(pDescriptorSet);
    pDescriptorSet->mGLES.mMaxSets = pDesc->mMaxSets;
    pDescriptorSet->mGLES.pRootSignature = pRootSignature;
    pDescriptorSet->mGLES.mUpdateFrequency = pDesc->mUpdateFrequency;
    pDescriptorSet->mGLES.pHandles = (DescriptorDataArray*)(pDescriptorSet + 1);

    uint8_t* mem = (uint8_t*)(pDescriptorSet->mGLES.pHandles + pDesc->mMaxSets);
    for (uint32_t set = 0; set < pDesc->mMaxSets; ++set)
    {
        pDescriptorSet->mGLES.pHandles[set].pData = (DescriptorHandle*)mem;
        mem += pRootSignature->mDescriptorCount * sizeof(DescriptorHandle);
    }

    for (uint32_t set = 0; set < pDesc->mMaxSets; ++set)
    {
        for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
        {
            switch (pRootSignature->pDescriptors[i].mType)
            {
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                pDescriptorSet->mGLES.pHandles[set].pData[i].pBufferHandles = (BufferDescriptorHandle*)mem;
                mem += sizeof(BufferDescriptorHandle);
                break;
            case DESCRIPTOR_TYPE_TEXTURE:
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            case DESCRIPTOR_TYPE_TEXTURE_CUBE:
                pDescriptorSet->mGLES.pHandles[set].pData[i].pTextures = (TextureDescriptorHandle*)mem;
                mem += pRootSignature->pDescriptors[i].mSize * sizeof(TextureDescriptorHandle);
                break;
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                pDescriptorSet->mGLES.pHandles[set].pData[i].pBufferHandles = (BufferDescriptorHandle*)mem;
                mem += pRootSignature->pDescriptors[i].mSize * sizeof(BufferDescriptorHandle);
                break;
            }
        }
    }

    *ppDescriptorSet = pDescriptorSet;
}

void gl_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    SAFE_FREE(pDescriptorSet);
}

void gl_updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
#if defined(ENABLE_GRAPHICS_DEBUG) || defined(PVS_STUDIO)
#define VALIDATE_DESCRIPTOR(descriptor, msgFmt, ...)                                                    \
    if (!VERIFYMSG((descriptor), "%s : " msgFmt, __FUNCTION__, ##__VA_ARGS__))                          \
    {                                                                                                   \
        continue;                                                                                       \
    }
#else
#define VALIDATE_DESCRIPTOR(descriptor,...)
#endif

    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mGLES.mMaxSets);

    const RootSignature* pRootSignature = pDescriptorSet->mGLES.pRootSignature;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        uint32_t paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;
        VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

        const DescriptorInfo* pDesc = (paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
        if (pDesc == NULL && paramIndex == UINT32_MAX)
        {
            LOGF(LogLevel::eWARNING, "pDesc == NULL. \"%s\" not reflected. Descriptor ignored. Note: GLES does not support non-static separate samplers.", pParam->pName);
            continue;
        }

        paramIndex = pDesc->mHandleIndex;

        if (paramIndex != UINT32_MAX)
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
        }
        else
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
        }

        const DescriptorType type = (DescriptorType)pDesc->mType;
        const uint32_t arrayStart = pParam->mArrayOffset;
        const uint32_t arrayCount = max(1U, pParam->mCount);

        switch (type)
        {
            case DESCRIPTOR_TYPE_TEXTURE:
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            case DESCRIPTOR_TYPE_TEXTURE_CUBE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
                    pDescriptorSet->mGLES.pHandles[index].pData[paramIndex].pTextures[arrayStart + arr] = { pParam->ppTextures[arr]->mMipLevels > 1, pParam->ppTextures[arr]->mGLES.mStateModified, pParam->ppTextures[arr]->mGLES.mTexture };
                }
                break;
            }
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            {
                ASSERT(arrayCount == 1 && "OpenGL ES 2.0 does not support arrays of buffers i.e. uniform float name[][]");

                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);
                pDescriptorSet->mGLES.pHandles[index].pData[paramIndex].pBufferHandles[0] = { pParam->ppBuffers[0]->mGLES.pGLCpuMappedAddress, pParam->pRanges ? pParam->pRanges[0].mOffset : 0 };
                break;
            }
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
                    pDescriptorSet->mGLES.pHandles[index].pData[paramIndex].pBufferHandles[arrayStart + arr] = { pParam->ppBuffers[arr]->mGLES.pGLCpuMappedAddress, pParam->pRanges ? pParam->pRanges[0].mOffset : 0 };
                }
                break;
            }
            default:
                // Unsupported descriptor type for the current OpenGL ES 2.0 renderer
                break;
        }
    }

}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void gl_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    UNREF_PARAM(pRenderer);

    memset(pCmdPool->pCmdCache->mVertexBufferOffsets, 0, MAX_VERTEX_ATTRIBS * sizeof(uint32_t));
    memset(pCmdPool->pCmdCache->mVertexBufferStrides, 0, MAX_VERTEX_ATTRIBS * sizeof(uint32_t));
    memset(pCmdPool->pCmdCache->mVertexBuffers, 0, MAX_VERTEX_ATTRIBS * sizeof(Buffer*));
    pCmdPool->pCmdCache->mVertexBufferCount = 0;
    pCmdPool->pCmdCache->pIndexBuffer = NULL;
    pCmdPool->pCmdCache->mActiveVertexBuffer = GL_NONE;
    pCmdPool->pCmdCache->mActiveIndexBuffer = GL_NONE;
    pCmdPool->pCmdCache->isStarted = false;
}

void gl_beginCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mGLES.pCmdPool);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(!cmdCache->isStarted);

    cmdCache->isStarted = true;
}

void gl_endCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mGLES.pCmdPool);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    //Bind default VAO to avoid recording unrelated api calls
    if (gExtensionSupport.mHasVertexArrayObjectOES && cmdCache->mActiveVAO)
    {
        CHECK_GLRESULT(glBindVertexArrayOES(GL_NONE));
        cmdCache->mActiveVAO = GL_NONE;
    }

    cmdCache->isStarted = false;
}

void gl_cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    if (!pDesc)
    {
        // Reset
        if (cmdCache->mFramebuffer)
        {
            CHECK_GLRESULT(glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE));
            cmdCache->mFramebuffer = GL_NONE;
            getGLSurfaceSize(NULL, &pCmd->mGLES.pCmdPool->pCmdCache->mRenderTargetHeight);
        }
        return;
    }

    uint32_t clearMask = 0;

    for (uint32_t rtIndex = 0; rtIndex < pDesc->mRenderTargetCount; ++rtIndex)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[rtIndex];
        if (rtIndex == 0)
        {
            pCmd->mGLES.pCmdPool->pCmdCache->mRenderTargetHeight = desc->pRenderTarget->mHeight;
        }

        if (desc->pRenderTarget->pTexture)
        {
            // bind new framebuffer for textured rt's
            if (rtIndex == 0 && cmdCache->mFramebuffer != desc->pRenderTarget->mGLES.mFramebuffer)
            {
                CHECK_GLRESULT(glBindFramebuffer(GL_FRAMEBUFFER, desc->pRenderTarget->mGLES.mFramebuffer));
                cmdCache->mFramebuffer = desc->pRenderTarget->mGLES.mFramebuffer;
            }
            CHECK_GLRESULT(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + rtIndex, GL_TEXTURE_2D, desc->pRenderTarget->pTexture->mGLES.mTexture, 0));
        }

        if (desc->mLoadAction == LOAD_ACTION_CLEAR)
        {
            const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pRenderTarget->mClearValue;
            vec4 clearColor = vec4(clearValue->r, clearValue->g, clearValue->b, clearValue->a);
            if (cmdCache->mClearColor != clearColor)
            {
                CHECK_GLRESULT(glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));
                cmdCache->mClearColor = clearColor;
            }
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    const bool hasDeepth = pDesc->mDepthStencil.pDepthStencil;
    if (hasDeepth && pDesc->mDepthStencil.pDepthStencil->mGLES.mType & GL_DEPTH_BUFFER_BIT)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        if (cmdCache->mFramebuffer)
        {
            if (desc->pDepthStencil->pTexture)
            {
                CHECK_GLRESULT(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, desc->pDepthStencil->pTexture->mGLES.mTexture, 0));
            }
            else
            {
                CHECK_GLRESULT(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, desc->pDepthStencil->mGLES.mDepthTarget));
            }
        }

        if (desc->mLoadAction == LOAD_ACTION_CLEAR)
        {
            const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
            if (cmdCache->mClearDepth != clearValue->depth)
            {
                CHECK_GLRESULT(glClearDepthf(clearValue->depth));
                cmdCache->mClearDepth = clearValue->depth;
            }
            if (!cmdCache->mDepthStencilState.mDepthWrite)
            {
                // Mask must be on for clear to have any effect
                CHECK_GLRESULT(glDepthMask(GL_TRUE));
                cmdCache->mDepthStencilState.mDepthWrite = GL_TRUE;
            }
            clearMask |= GL_DEPTH_BUFFER_BIT;
        }
    }

    if (hasDeepth && pDesc->mDepthStencil.pDepthStencil->mGLES.mType & GL_STENCIL_BUFFER_BIT)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        if (cmdCache->mFramebuffer)
        {
            if (desc->pDepthStencil->pTexture)
            {
                CHECK_GLRESULT(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, desc->pDepthStencil->pTexture->mGLES.mTexture, 0));
            }
            else
            {
                CHECK_GLRESULT(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, desc->pDepthStencil->mGLES.mStencilTarget));
            }
        }

        if (desc->mLoadActionStencil == LOAD_ACTION_CLEAR)
        {
            const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
            if (cmdCache->mClearStencil != clearValue->stencil)
            {
                CHECK_GLRESULT(glClearStencil(clearValue->stencil));
                cmdCache->mClearStencil = clearValue->stencil;
            }
            if (cmdCache->mDepthStencilState.mStencilWriteMask != 0b11111111)
            {
                // Mask must be on for clear to have any effect
                CHECK_GLRESULT(glStencilMask(0b11111111));
                cmdCache->mDepthStencilState.mStencilWriteMask = 0b11111111;
            }
            clearMask |= GL_STENCIL_BUFFER_BIT;
        }
    }

#if defined(ENABLE_GRAPHICS_DEBUG)
    GLenum result;
    CHECK_GL_RETURN_RESULT(result, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (result != GL_FRAMEBUFFER_COMPLETE && result != GL_NO_ERROR)
        LOGF(eERROR, "Incomplete framebuffer! %s", util_get_enum_string(result));
#endif

    if (clearMask)
    {
        CHECK_GLRESULT(glClear(clearMask));
    }
}

void gl_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    // Inverse y location. OpenGL screen origin starts in lower left corner
    uint32_t yOffset = pCmd->mGLES.pCmdPool->pCmdCache->mRenderTargetHeight - y - height;
    vec4 viewport = vec4(x, yOffset, width, height);
    if (cmdCache->mViewport != viewport)
    {
        CHECK_GLRESULT(glViewport(x, yOffset, width, height));
        cmdCache->mViewport = viewport;
    }
    vec2 depthRange = vec2(minDepth, maxDepth);
    if (cmdCache->mDepthRange != depthRange)
    {
        CHECK_GLRESULT(glDepthRangef(minDepth, maxDepth));
        cmdCache->mDepthRange = depthRange;
    }
}

void gl_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    // Inverse y location. OpenGL screen origin starts in lower left corner
    uint32_t yOffset = pCmd->mGLES.pCmdPool->pCmdCache->mRenderTargetHeight - y - height;
    vec4 scissor = vec4(x, yOffset, width, height);
    if (cmdCache->mScissor != scissor)
    {
        CHECK_GLRESULT(glScissor(x, yOffset, width, height));
        cmdCache->mScissor = scissor;
    }
}

void gl_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    if (cmdCache->mStencilRefValue != val)
    {
        if (cmdCache->mDepthStencilState.mStencilBackFunc == cmdCache->mDepthStencilState.mStencilFrontFunc)
        {
            CHECK_GLRESULT(glStencilFunc(cmdCache->mDepthStencilState.mStencilFrontFunc, val, cmdCache->mDepthStencilState.mStencilReadMask));
        }
        else
        {
            CHECK_GLRESULT(glStencilFuncSeparate(GL_FRONT, cmdCache->mDepthStencilState.mStencilFrontFunc, val, cmdCache->mDepthStencilState.mStencilReadMask));
            CHECK_GLRESULT(glStencilFuncSeparate(GL_BACK, cmdCache->mDepthStencilState.mStencilBackFunc, val, cmdCache->mDepthStencilState.mStencilReadMask));
        }

        cmdCache->mStencilRefValue = val;
    }
}

void gl_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);
    ASSERT(pPipeline->mGLES.mType == PIPELINE_TYPE_GRAPHICS);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    uint32_t glProgram = pPipeline->mGLES.pRootSignature->mGLES.pProgramTargets[pPipeline->mGLES.mRootSignatureIndex];
    if (cmdCache->mPipeline != glProgram)
    {
        CHECK_GLRESULT(glUseProgram(glProgram));
        cmdCache->mPipeline = glProgram;
        cmdCache->mGlPrimitiveTopology = pPipeline->mGLES.mGlPrimitiveTopology;
        cmdCache->pRootSignature = pPipeline->mGLES.pRootSignature;
        cmdCache->mInstanceLocation = -1;

        if (gExtensionSupport.mHasVertexArrayObjectOES)
        {
            cmdCache->pActiveVAOStates = pPipeline->mGLES.pVAOState;
            cmdCache->pActiveVAOStateLoop = &pPipeline->mGLES.mVAOStateLoop;
            cmdCache->pActiveVAOStateCount = &pPipeline->mGLES.mVAOStateCount;
        }
    }

    // Enable/disable used vertex attribute locations for current program if necessary
    bool shouldEnable[MAX_VERTEX_ATTRIBS] = {};
    for (uint32_t vertexLayoutIndex = 0; vertexLayoutIndex < pPipeline->mGLES.mVertexLayoutSize; ++vertexLayoutIndex)
    {
        cmdCache->mVertexLayout[vertexLayoutIndex] = pPipeline->mGLES.pVertexLayout[vertexLayoutIndex];
        const int32_t vertexAttribIndex = pPipeline->mGLES.pVertexLayout[vertexLayoutIndex].mIndex;
        if (vertexAttribIndex >= 0)
            shouldEnable[vertexAttribIndex] = true;
    }

    for (uint32_t vertexAttribIndex = 0; vertexAttribIndex < MAX_VERTEX_ATTRIBS; ++vertexAttribIndex)
    {
        if (shouldEnable[vertexAttribIndex])
        {
            if (!cmdCache->mVertexAttribCache[vertexAttribIndex].mIsActive)
            {
                if (!gExtensionSupport.mHasVertexArrayObjectOES)
                {
                    CHECK_GLRESULT(glEnableVertexAttribArray(vertexAttribIndex));
                }
                cmdCache->mVertexAttribCache[vertexAttribIndex].mIsActive = true;
            }
        }
        else if (cmdCache->mVertexAttribCache[vertexAttribIndex].mIsActive)
        {
            if (!gExtensionSupport.mHasVertexArrayObjectOES)
            {
                CHECK_GLRESULT(glDisableVertexAttribArray(vertexAttribIndex));
            }
            cmdCache->mVertexAttribCache[vertexAttribIndex].mIsActive = false;
        }
    }
    cmdCache->mVertexLayoutCount = pPipeline->mGLES.mVertexLayoutSize;


    // Set rasterizer state
    if (cmdCache->mRasterizerState.mCullMode != pPipeline->mGLES.pRasterizerState->mCullMode)
    {

        if (pPipeline->mGLES.pRasterizerState->mCullMode)
        {
            CHECK_GLRESULT(glEnable(GL_CULL_FACE));
            CHECK_GLRESULT(glCullFace(pPipeline->mGLES.pRasterizerState->mCullMode));
        }
        else
        {
            CHECK_GLRESULT(glDisable(GL_CULL_FACE));
        }
        cmdCache->mRasterizerState.mCullMode = pPipeline->mGLES.pRasterizerState->mCullMode;
    }
    if (pPipeline->mGLES.pRasterizerState->mCullMode)
    {
        if (cmdCache->mRasterizerState.mFrontFace != pPipeline->mGLES.pRasterizerState->mFrontFace)
        {
            CHECK_GLRESULT(glFrontFace(pPipeline->mGLES.pRasterizerState->mFrontFace));
            cmdCache->mRasterizerState.mFrontFace = pPipeline->mGLES.pRasterizerState->mFrontFace;
        }
    }

    if (cmdCache->mRasterizerState.mScissorTest != pPipeline->mGLES.pRasterizerState->mScissorTest)
    {
        CHECK_GLRESULT(pPipeline->mGLES.pRasterizerState->mScissorTest ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST));
        cmdCache->mRasterizerState.mScissorTest = pPipeline->mGLES.pRasterizerState->mScissorTest;
    }

    // Set depth test, mask and function
    if (cmdCache->mDepthStencilState.mDepthTest != pPipeline->mGLES.pDepthStencilState->mDepthTest)
    {
        CHECK_GLRESULT(pPipeline->mGLES.pDepthStencilState->mDepthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));
        cmdCache->mDepthStencilState.mDepthTest = pPipeline->mGLES.pDepthStencilState->mDepthTest;
    }
    if (pPipeline->mGLES.pDepthStencilState->mDepthTest)
    {
        if (cmdCache->mDepthStencilState.mDepthWrite != pPipeline->mGLES.pDepthStencilState->mDepthWrite)
        {
            CHECK_GLRESULT(glDepthMask(pPipeline->mGLES.pDepthStencilState->mDepthWrite));
            cmdCache->mDepthStencilState.mDepthWrite = pPipeline->mGLES.pDepthStencilState->mDepthWrite;
        }
        if (cmdCache->mDepthStencilState.mDepthFunc != pPipeline->mGLES.pDepthStencilState->mDepthFunc)
        {
            CHECK_GLRESULT(glDepthFunc(pPipeline->mGLES.pDepthStencilState->mDepthFunc));
            cmdCache->mDepthStencilState.mDepthFunc = pPipeline->mGLES.pDepthStencilState->mDepthFunc;
        }
    }

    // Set stencil test, mask and functions
    if (cmdCache->mDepthStencilState.mStencilTest != pPipeline->mGLES.pDepthStencilState->mStencilTest)
    {
        CHECK_GLRESULT(pPipeline->mGLES.pDepthStencilState->mStencilTest ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST));
        cmdCache->mDepthStencilState.mStencilTest = pPipeline->mGLES.pDepthStencilState->mStencilTest;
    }
    if(pPipeline->mGLES.pDepthStencilState->mStencilTest)
    {
        if (cmdCache->mDepthStencilState.mStencilFrontFunc != pPipeline->mGLES.pDepthStencilState->mStencilFrontFunc || cmdCache->mDepthStencilState.mStencilReadMask != pPipeline->mGLES.pDepthStencilState->mStencilReadMask)
        {
            CHECK_GLRESULT(glStencilFuncSeparate(GL_FRONT, pPipeline->mGLES.pDepthStencilState->mStencilFrontFunc, cmdCache->mStencilRefValue, pPipeline->mGLES.pDepthStencilState->mStencilReadMask));
            cmdCache->mDepthStencilState.mStencilFrontFunc = pPipeline->mGLES.pDepthStencilState->mStencilFrontFunc;
        }

        if (cmdCache->mDepthStencilState.mStencilFrontFail != pPipeline->mGLES.pDepthStencilState->mStencilFrontFail ||
            cmdCache->mDepthStencilState.mDepthFrontFail != pPipeline->mGLES.pDepthStencilState->mDepthFrontFail ||
            cmdCache->mDepthStencilState.mStencilFrontPass != pPipeline->mGLES.pDepthStencilState->mStencilFrontPass)
        {
            CHECK_GLRESULT(glStencilOpSeparate(GL_FRONT, pPipeline->mGLES.pDepthStencilState->mStencilFrontFail, pPipeline->mGLES.pDepthStencilState->mDepthFrontFail, pPipeline->mGLES.pDepthStencilState->mStencilFrontPass));
            cmdCache->mDepthStencilState.mStencilFrontFail = pPipeline->mGLES.pDepthStencilState->mStencilFrontFail;
            cmdCache->mDepthStencilState.mDepthFrontFail = pPipeline->mGLES.pDepthStencilState->mDepthFrontFail;
            cmdCache->mDepthStencilState.mStencilFrontPass = pPipeline->mGLES.pDepthStencilState->mStencilFrontPass;
        }

        if (cmdCache->mDepthStencilState.mStencilBackFunc != pPipeline->mGLES.pDepthStencilState->mStencilBackFunc || cmdCache->mDepthStencilState.mStencilReadMask != pPipeline->mGLES.pDepthStencilState->mStencilReadMask)
        {
            CHECK_GLRESULT(glStencilFuncSeparate(GL_BACK, pPipeline->mGLES.pDepthStencilState->mStencilBackFunc, cmdCache->mStencilRefValue, pPipeline->mGLES.pDepthStencilState->mStencilReadMask));
            cmdCache->mDepthStencilState.mStencilBackFunc = pPipeline->mGLES.pDepthStencilState->mStencilBackFunc;
            cmdCache->mDepthStencilState.mStencilReadMask = pPipeline->mGLES.pDepthStencilState->mStencilReadMask;
        }

        if (cmdCache->mDepthStencilState.mStencilBackFail != pPipeline->mGLES.pDepthStencilState->mStencilBackFail ||
            cmdCache->mDepthStencilState.mDepthBackFail != pPipeline->mGLES.pDepthStencilState->mDepthBackFail ||
            cmdCache->mDepthStencilState.mStencilBackPass != pPipeline->mGLES.pDepthStencilState->mStencilBackPass)
        {
            CHECK_GLRESULT(glStencilOpSeparate(GL_BACK, pPipeline->mGLES.pDepthStencilState->mStencilBackFail, pPipeline->mGLES.pDepthStencilState->mDepthBackFail, pPipeline->mGLES.pDepthStencilState->mStencilBackPass));
            cmdCache->mDepthStencilState.mStencilBackFail = pPipeline->mGLES.pDepthStencilState->mStencilBackFail;
            cmdCache->mDepthStencilState.mDepthBackFail = pPipeline->mGLES.pDepthStencilState->mDepthBackFail;
            cmdCache->mDepthStencilState.mStencilBackPass = pPipeline->mGLES.pDepthStencilState->mStencilBackPass;
        }

        if (cmdCache->mDepthStencilState.mStencilWriteMask != pPipeline->mGLES.pDepthStencilState->mStencilWriteMask)
        {
            CHECK_GLRESULT(glStencilMask(pPipeline->mGLES.pDepthStencilState->mStencilWriteMask));
            cmdCache->mDepthStencilState.mStencilWriteMask = pPipeline->mGLES.pDepthStencilState->mStencilWriteMask;
        }
    }


    // Set blend state
    if (cmdCache->mBlendState.mBlendEnable != pPipeline->mGLES.pBlendState->mBlendEnable)
    {
        CHECK_GLRESULT(pPipeline->mGLES.pBlendState->mBlendEnable ? glEnable(GL_BLEND) : glDisable(GL_BLEND));
        cmdCache->mBlendState.mBlendEnable = pPipeline->mGLES.pBlendState->mBlendEnable;
    }
    if (pPipeline->mGLES.pBlendState->mBlendEnable)
    {
        if (cmdCache->mBlendState.mSrcRGBFunc != pPipeline->mGLES.pBlendState->mSrcRGBFunc ||
            cmdCache->mBlendState.mDstRGBFunc != pPipeline->mGLES.pBlendState->mDstRGBFunc ||
            cmdCache->mBlendState.mSrcAlphaFunc != pPipeline->mGLES.pBlendState->mSrcAlphaFunc ||
            cmdCache->mBlendState.mDstAlphaFunc != pPipeline->mGLES.pBlendState->mDstAlphaFunc)
        {
            CHECK_GLRESULT(glBlendFuncSeparate(pPipeline->mGLES.pBlendState->mSrcRGBFunc, pPipeline->mGLES.pBlendState->mDstRGBFunc, pPipeline->mGLES.pBlendState->mSrcAlphaFunc, pPipeline->mGLES.pBlendState->mDstAlphaFunc));
            cmdCache->mBlendState.mSrcRGBFunc = pPipeline->mGLES.pBlendState->mSrcRGBFunc;
            cmdCache->mBlendState.mDstRGBFunc = pPipeline->mGLES.pBlendState->mDstRGBFunc;
            cmdCache->mBlendState.mSrcAlphaFunc = pPipeline->mGLES.pBlendState->mSrcAlphaFunc;
            cmdCache->mBlendState.mDstAlphaFunc = pPipeline->mGLES.pBlendState->mDstAlphaFunc;
        }

        if (cmdCache->mBlendState.mModeRGB != pPipeline->mGLES.pBlendState->mModeRGB ||
            cmdCache->mBlendState.mModeAlpha != pPipeline->mGLES.pBlendState->mModeAlpha)
        {
            CHECK_GLRESULT(glBlendEquationSeparate(pPipeline->mGLES.pBlendState->mModeRGB, pPipeline->mGLES.pBlendState->mModeAlpha));
            cmdCache->mBlendState.mModeRGB = pPipeline->mGLES.pBlendState->mModeRGB;
            cmdCache->mBlendState.mModeAlpha = pPipeline->mGLES.pBlendState->mModeAlpha;
        }
    }
}

void gl_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    const RootSignature* pRootSignature = pDescriptorSet->mGLES.pRootSignature;
    Sampler* pSampler = pRootSignature->mGLES.pSampler;
    ASSERT(pSampler);

    uint32_t currentGlProgram = cmdCache->mPipeline;

    for (uint32_t sh = 0; sh < pRootSignature->mGLES.mProgramCount; ++sh)
    {
        uint32_t glProgram = pRootSignature->mGLES.pProgramTargets[sh];
        if (cmdCache->mPipeline != glProgram)
        {
            CHECK_GLRESULT(glUseProgram(glProgram));
            cmdCache->mPipeline = glProgram;
        }

        GLuint textureIndex = 0;
        for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
        {
            DescriptorInfo* descInfo = &pRootSignature->pDescriptors[i];
            uint32_t locationIndex = i + sh * pRootSignature->mDescriptorCount;
            GLint location = pRootSignature->mGLES.pDescriptorGlLocations[locationIndex];
            if (location < 0)
            {
                if(descInfo->mType == DESCRIPTOR_TYPE_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_RW_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_TEXTURE_CUBE)
                    textureIndex += descInfo->mSize;
                continue;
            }
            if (descInfo->mRootDescriptor)
            {
                continue;
            }

            switch (descInfo->mType)
            {
                case DESCRIPTOR_TYPE_BUFFER:
                case DESCRIPTOR_TYPE_RW_BUFFER:
                case DESCRIPTOR_TYPE_BUFFER_RAW:
                case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                {
                    BufferDescriptorHandle* handle = &pDescriptorSet->mGLES.pHandles[index].pData[i].pBufferHandles[0];
                    if (handle->pBuffer)
                    {
                        util_gl_set_uniform(location, (uint8_t*)handle->pBuffer + handle->mOffset, descInfo->mGLES.mGlType, descInfo->mSize);
                    }
                    break;
                }
                case DESCRIPTOR_TYPE_TEXTURE:
                case DESCRIPTOR_TYPE_RW_TEXTURE:
                case DESCRIPTOR_TYPE_TEXTURE_CUBE:
                {
                    GLenum target = DESCRIPTOR_TYPE_TEXTURE_CUBE == descInfo->mType ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
                    for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
                    {
                        TextureDescriptorHandle* textureHandle = &pDescriptorSet->mGLES.pHandles[index].pData[i].pTextures[arr];
                        if (textureHandle->mTexture != GL_NONE)
                        {
                            if (cmdCache->mActiveTexture != textureIndex)
                            {
                                CHECK_GLRESULT(glActiveTexture(GL_TEXTURE0 + textureIndex));
                                cmdCache->mActiveTexture = textureIndex;
                            }

                    if (textureHandle->mStateModified || textureHandle->mTexture != cmdCache->mBoundTexture )
                    {
                        CHECK_GLRESULT(glBindTexture(target, textureHandle->mTexture));
                        cmdCache->mBoundTexture = textureHandle->mTexture;
                        textureHandle->mStateModified = false;
                    }

                            //if(cmdCache->pTextureSampler[textureIndex].mGLES.mMinFilter != (textureHandle->hasMips ? pSampler->mGLES.mMipMapMode : pSampler->mGLES.mMinFilter))
                            //if (cmdCache->pTextureSampler[textureIndex].mGLES.mMinFilter != pSampler->mGLES.mMinFilter ||
                            //	cmdCache->pTextureSampler[textureIndex].mGLES.mMipMapMode != pSampler->mGLES.mMipMapMode)
                            {
                                CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, textureHandle->hasMips ? pSampler->mGLES.mMipMapMode : pSampler->mGLES.mMinFilter));
                            //	cmdCache->pTextureSampler[textureIndex].mGLES.mMinFilter = (textureHandle->hasMips ? pSampler->mGLES.mMipMapMode : pSampler->mGLES.mMinFilter);
                            //	cmdCache->pTextureSampler[textureIndex].mGLES.mMipMapMode = pSampler->mGLES.mMipMapMode;
                            }

                            if(cmdCache->pTextureSampler[textureIndex].mGLES.mMagFilter != pSampler->mGLES.mMagFilter)
                            {
                                CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, pSampler->mGLES.mMagFilter));
                                cmdCache->pTextureSampler[textureIndex].mGLES.mMagFilter = pSampler->mGLES.mMagFilter;
                            }

                            if (cmdCache->pTextureSampler[textureIndex].mGLES.mAddressS != pSampler->mGLES.mAddressS)
                            {
                                CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_WRAP_S, pSampler->mGLES.mAddressS));
                                cmdCache->pTextureSampler[textureIndex].mGLES.mAddressS = pSampler->mGLES.mAddressS;
                            }

                            if (cmdCache->pTextureSampler[textureIndex].mGLES.mAddressT != pSampler->mGLES.mAddressT)
                            {
                                CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_WRAP_T, pSampler->mGLES.mAddressT));
                                cmdCache->pTextureSampler[textureIndex].mGLES.mAddressT = pSampler->mGLES.mAddressT;
                            }
                        }
                        ++textureIndex;
                    }
                    break;
                }
                case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                {
                    for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
                    {
                        uint32_t uboOffset = descInfo->mGLES.mUBOSize * arr;
                        BufferDescriptorHandle* handle = &pDescriptorSet->mGLES.pHandles[index].pData[i].pBufferHandles[arr];

                        uint8_t* data = (uint8_t*)handle->pBuffer;
                        if (!data)
                            continue;

                        data += handle->mOffset;
                        for (uint32_t varIndex = descInfo->mGLES.mVariableStart; varIndex < pRootSignature->mGLES.mVariableCount; ++varIndex)
                        {
                            GlVariable* variable = &pRootSignature->mGLES.pVariables[varIndex];
                            if (variable->mIndexInParent != i)
                                break;

                            util_gl_set_uniform(variable->pGlLocations[sh] + uboOffset, data, variable->mType, variable->mSize);
                            uint32_t typeSize = gl_type_byte_size(variable->mType);
                            data += typeSize * variable->mSize;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (cmdCache->mPipeline != currentGlProgram)
    {
        CHECK_GLRESULT(glUseProgram(currentGlProgram));
        cmdCache->mPipeline = currentGlProgram;
    }
}

void util_gl_set_constant(const Cmd* pCmd, const RootSignature* pRootSignature, const DescriptorInfo* pDesc, const void* pData)
{
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    uint32_t currentGlProgram = cmdCache->mPipeline;

    for (uint32_t program = 0; program < pRootSignature->mGLES.mProgramCount; ++program)
    {
        uint32_t glProgram = pRootSignature->mGLES.pProgramTargets[program];
        if (cmdCache->mPipeline != glProgram)
        {
            CHECK_GLRESULT(glUseProgram(glProgram));
            cmdCache->mPipeline = glProgram;
        }

        uint32_t locationIndex = pDesc->mHandleIndex + program * pRootSignature->mDescriptorCount;
        GLint location = pRootSignature->mGLES.pDescriptorGlLocations[locationIndex];
        if (location < 0)
            continue;

        uint8_t* cpuBuffer = (uint8_t*)pData;
        switch (pDesc->mType)
        {
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            util_gl_set_uniform(location, cpuBuffer, pDesc->mGLES.mGlType, pDesc->mSize);
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            for (uint32_t varIndex = pDesc->mGLES.mVariableStart; varIndex < pRootSignature->mGLES.mVariableCount; ++varIndex)
            {
                GlVariable* variable = &pRootSignature->mGLES.pVariables[varIndex];
                if (variable->mIndexInParent != pDesc->mHandleIndex)
                    break;
                util_gl_set_uniform(variable->pGlLocations[program], cpuBuffer, variable->mType, variable->mSize);

                uint32_t typeSize = gl_type_byte_size(variable->mType);
                cpuBuffer += typeSize * variable->mSize;
            }
            break;
        }
    }

    if (cmdCache->mPipeline != currentGlProgram)
    {
        CHECK_GLRESULT(glUseProgram(currentGlProgram));
        cmdCache->mPipeline = currentGlProgram;
    }
}

void gl_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
    ASSERT(pCmd);
    ASSERT(pConstants);
    ASSERT(pRootSignature);
    ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);
    ASSERT(pCmd->mGLES.pCmdPool->pCmdCache->isStarted);

    const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
    ASSERT(pDesc);

    util_gl_set_constant(pCmd, pRootSignature, pDesc, pConstants);
}

void gl_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    gl_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

    const RootSignature* pRootSignature = pDescriptorSet->mGLES.pRootSignature;
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);
    uint32_t currentGlProgram = cmdCache->mPipeline;

    for (uint32_t sh = 0; sh < pRootSignature->mGLES.mProgramCount; ++sh)
    {
        uint32_t glProgram = pRootSignature->mGLES.pProgramTargets[sh];

        if (cmdCache->mPipeline != glProgram)
        {
            CHECK_GLRESULT(glUseProgram(glProgram));
            cmdCache->mPipeline = glProgram;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            const DescriptorData* pParam = pParams + i;
            uint32_t paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;
            VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

            const DescriptorInfo* pDesc = (paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
            paramIndex = pDesc->mHandleIndex;

            if (paramIndex != UINT32_MAX)
            {
                VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
            }
            else
            {
                VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
            }

            BufferDescriptorHandle handle = { pParam->ppBuffers[0]->mGLES.pGLCpuMappedAddress, pParam->pRanges[0].mOffset };

            switch (pDesc->mType)
            {
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            {
                uint32_t locationIndex = (uint32_t)(pDesc - pRootSignature->pDescriptors) + sh * pRootSignature->mDescriptorCount;
                GLint location = pRootSignature->mGLES.pDescriptorGlLocations[locationIndex];
                util_gl_set_uniform(location, (uint8_t*)handle.pBuffer + handle.mOffset, pDesc->mGLES.mGlType, pDesc->mSize);
                break;
            }
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                uint8_t* data = (uint8_t*)handle.pBuffer;
                if (!data)
                    continue;

                data += handle.mOffset;
                for (uint32_t varIndex = pDesc->mGLES.mVariableStart; varIndex < pRootSignature->mGLES.mVariableCount; ++varIndex)
                {
                    GlVariable* variable = &pRootSignature->mGLES.pVariables[varIndex];
                    if (variable->mIndexInParent != i)
                        break;

                    util_gl_set_uniform(variable->pGlLocations[sh], data, variable->mType, variable->mSize);
                    uint32_t typeSize = gl_type_byte_size(variable->mType);
                    data += typeSize * variable->mSize;
                }

                break;
            }
            }
        }
    }

    if (cmdCache->mPipeline != currentGlProgram)
    {
        CHECK_GLRESULT(glUseProgram(currentGlProgram));
        cmdCache->mPipeline = currentGlProgram;
    }
}

void gl_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    // GLES 2.0 Only supports GL_UNSIGNED_SHORT
    ASSERT(indexType == INDEX_TYPE_UINT16);

    ASSERT(pBuffer->mGLES.mTarget == GL_ELEMENT_ARRAY_BUFFER);

    if (pBuffer->mGLES.mBuffer)
    {
        if (pBuffer->mGLES.mBuffer != cmdCache->mActiveIndexBuffer)
        {
            if (!gExtensionSupport.mHasVertexArrayObjectOES)
            {
                CHECK_GLRESULT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBuffer->mGLES.mBuffer));
            }
            cmdCache->mActiveIndexBuffer = pBuffer->mGLES.mBuffer;
        }
    }

    cmdCache->mIndexBufferOffset = offset;
    cmdCache->pIndexBuffer = pBuffer;
}

void util_bind_vertex_attribute_buffers(CmdCache* pCmdCache, uint32_t firstVertex, uint16_t vaoIndex)
{
    ASSERT(pCmdCache);
    ASSERT(pCmdCache->isStarted);
    ASSERT(pCmdCache->mVertexBufferCount != 0);

    uint32_t vertexLayoutIndex = 0;
    for (uint32_t index = 0; index < pCmdCache->mVertexBufferCount; ++index)
    {
        bool useGPUResource = pCmdCache->mVertexBuffers[index]->mGLES.mBuffer;
        if (useGPUResource && pCmdCache->mVertexBuffers[index]->mGLES.mBuffer != pCmdCache->mActiveVertexBuffer && !gExtensionSupport.mHasVertexArrayObjectOES)
        {
            CHECK_GLRESULT(glBindBuffer(GL_ARRAY_BUFFER, pCmdCache->mVertexBuffers[index]->mGLES.mBuffer));
            pCmdCache->mActiveVertexBuffer = pCmdCache->mVertexBuffers[index]->mGLES.mBuffer;
        }

        if (gExtensionSupport.mHasVertexArrayObjectOES)
        {
            pCmdCache->pActiveVAOStates[vaoIndex].mBufferOffsets[index] = pCmdCache->mVertexBufferOffsets[index];
            pCmdCache->pActiveVAOStates[vaoIndex].mAttachedBuffers[index] = pCmdCache->mVertexBuffers[index]->mGLES.mBuffer;
        }

        const uint32_t vertexStartOffset = pCmdCache->mVertexLayout[vertexLayoutIndex].mOffset;
        int32_t vertexStrideCount = 0;
        while (vertexStrideCount < pCmdCache->mVertexBufferStrides[index])
        {
            if (vertexLayoutIndex >= pCmdCache->mVertexLayoutCount)
            {
                LOGF(LogLevel::eERROR, "Bound vertex buffer does not match pipeline vertex layout!");
                break;
            }

            GlVertexAttrib* vertexAttrib = &pCmdCache->mVertexLayout[vertexLayoutIndex];
            uint32_t vertexOffset = vertexAttrib->mOffset - vertexStartOffset + pCmdCache->mVertexBufferOffsets[index] + pCmdCache->mVertexBufferStrides[index] * firstVertex;
            if (vertexAttrib->mIndex >= 0)
            {
                if (gExtensionSupport.mHasVertexArrayObjectOES) // Should pass only once per new VAO
                {
                    if (useGPUResource)
                    {
                        CHECK_GLRESULT(glBindBuffer(GL_ARRAY_BUFFER, pCmdCache->mVertexBuffers[index]->mGLES.mBuffer));
                        CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
                            pCmdCache->mVertexBufferStrides[index], (void*)(intptr_t)vertexOffset));
                    }
                    else
                    {
                        CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
                            pCmdCache->mVertexBufferStrides[index], (void*)((uint8_t*)pCmdCache->mVertexBuffers[index]->mGLES.pGLCpuMappedAddress + vertexOffset)));
                    }
                    CHECK_GLRESULT(glEnableVertexAttribArray(vertexAttrib->mIndex));
                }
                //Check if reset is required
                else if (pCmdCache->mVertexAttribCache[vertexAttrib->mIndex].mOffset != vertexOffset ||
                    pCmdCache->mVertexAttribCache[vertexAttrib->mIndex].mAttachedBuffer != pCmdCache->mVertexBuffers[index]->mGLES.mBuffer)
                {
                    if (useGPUResource)
                    {
                        CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
                            pCmdCache->mVertexBufferStrides[index], (void*)(intptr_t)vertexOffset));
                    }
                    else
                    {
                        CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
                            pCmdCache->mVertexBufferStrides[index], (void*)((uint8_t*)pCmdCache->mVertexBuffers[index]->mGLES.pGLCpuMappedAddress + vertexOffset)));
                    }
                    pCmdCache->mVertexAttribCache[vertexAttrib->mIndex].mOffset = vertexOffset;
                    pCmdCache->mVertexAttribCache[vertexAttrib->mIndex].mAttachedBuffer = pCmdCache->mVertexBuffers[index]->mGLES.mBuffer;
                }
            }

            vertexStrideCount += vertexAttrib->mStride;
            ++vertexLayoutIndex;
        }
    }

    if (gExtensionSupport.mHasVertexArrayObjectOES)
        pCmdCache->pActiveVAOStates[vaoIndex].mAttachedBufferCount = pCmdCache->mVertexBufferCount;
}

void gl_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    ASSERT(pCmd);
    ASSERT(bufferCount);
    ASSERT(ppBuffers);
    ASSERT(pStrides);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    cmdCache->mVertexBufferCount = bufferCount;
    for (uint32_t index = 0; index < bufferCount; ++index)
    {
        ASSERT(ppBuffers[index]->mGLES.mTarget == GL_ARRAY_BUFFER);
        uint32_t bufferOffset = pOffsets ? pOffsets[index] : 0;

        cmdCache->mVertexBufferOffsets[index] = bufferOffset;
        cmdCache->mVertexBufferStrides[index] = pStrides[index];
        cmdCache->mVertexBuffers[index] = ppBuffers[index];
    }

    if(!gExtensionSupport.mHasVertexArrayObjectOES)
        util_bind_vertex_attribute_buffers(cmdCache, 0, 0);
}

void util_reset_vao(GLVAOState* pState)
{
    ASSERT((uint32_t)gExtensionSupport.mHasVertexArrayObjectOES);

    CHECK_GLRESULT(glDeleteVertexArraysOES(1, &pState->mVAO));

    pState->mActiveIndexBuffer = GL_NONE;
    for (uint32_t vertexAttribLocation = 0; vertexAttribLocation < MAX_VERTEX_ATTRIBS; ++vertexAttribLocation)
    {
        pState->mAttachedBuffers[vertexAttribLocation] = -1;
        pState->mBufferOffsets[vertexAttribLocation] = 0;
    }
    pState->mAttachedBufferCount = 0;
    pState->mId = 0;
}

void util_bind_vao(CmdCache* pCmdCache, uint32_t firstVertex, bool isIndexed)
{
    ASSERT((uint32_t)gExtensionSupport.mHasVertexArrayObjectOES);

    // Get VAO Id
    static const uint32_t r = 31;
    uint32_t mId = firstVertex;
    mId = mId * r + pCmdCache->mActiveIndexBuffer;
    mId = mId * r + pCmdCache->mVertexBufferCount;
    for (uint32_t i = 0; i < pCmdCache->mVertexBufferCount; ++i)
    {
        mId = mId * r + pCmdCache->mVertexBuffers[i]->mGLES.mBuffer;
        mId = mId * r + pCmdCache->mVertexBufferOffsets[i];
    }

    uint16_t index = 0;
    uint16_t currActiveStateCount = *pCmdCache->pActiveVAOStateCount;
    for(; index < currActiveStateCount; ++index)
    {
        if (pCmdCache->pActiveVAOStates[index].mId == mId)
            break;
    }

    // Create new VAO
    if (index == currActiveStateCount) // New VAO
    {
        if (index == VAO_STATE_CACHE_SIZE)
        {
            index = *pCmdCache->pActiveVAOStateLoop;
            *pCmdCache->pActiveVAOStateLoop = (index + 1) % VAO_STATE_CACHE_SIZE;
            util_reset_vao(&pCmdCache->pActiveVAOStates[index]);
        }
        else
        {
            ++(*pCmdCache->pActiveVAOStateCount);
        }

        CHECK_GLRESULT(glGenVertexArraysOES(1, &pCmdCache->pActiveVAOStates[index].mVAO));
        CHECK_GLRESULT(glBindVertexArrayOES(pCmdCache->pActiveVAOStates[index].mVAO));
        pCmdCache->mActiveVAO = pCmdCache->pActiveVAOStates[index].mVAO;

        pCmdCache->pActiveVAOStates[index].mId = mId;
        pCmdCache->pActiveVAOStates[index].mFirstVertex = firstVertex;

        util_bind_vertex_attribute_buffers(pCmdCache, firstVertex, index);

        if (isIndexed)
        {
            CHECK_GLRESULT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pCmdCache->mActiveIndexBuffer));
        }
        pCmdCache->pActiveVAOStates[index].mActiveIndexBuffer = isIndexed ? pCmdCache->mActiveIndexBuffer : 0;
    }
    else
    {
        if (pCmdCache->mActiveVAO != pCmdCache->pActiveVAOStates[index].mVAO)
        {
            CHECK_GLRESULT(glBindVertexArrayOES(pCmdCache->pActiveVAOStates[index].mVAO));
            pCmdCache->mActiveVAO = pCmdCache->pActiveVAOStates[index].mVAO;
        }
    }
}

void gl_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    if (gExtensionSupport.mHasVertexArrayObjectOES)
    {
        util_bind_vao(cmdCache, 0, false); // firstVertex does not matter for glDrawArrays
    }

    CHECK_GLRESULT(glDrawArrays(cmdCache->mGlPrimitiveTopology, firstVertex, vertexCount));
}

void util_get_instance_location(CmdCache* pCmdCache)
{
    ASSERT(pCmdCache->pRootSignature);

    if (pCmdCache->mInstanceLocation >= 0) // Already found location
        return;

    // Get instance location if exists
    const DescriptorInfo* descInfo = get_descriptor(pCmdCache->pRootSignature, GLES_INSTANCE_ID);
    if (!descInfo)
    {
        pCmdCache->mInstanceLocation = -1;
    }
    else
    {
        for (uint16_t i = 0; pCmdCache->pRootSignature->mGLES.mProgramCount; ++i)
        {
            if (pCmdCache->pRootSignature->mGLES.pProgramTargets[i] == pCmdCache->mPipeline)
            {
                uint32_t locationIndex = descInfo->mHandleIndex + i * pCmdCache->pRootSignature->mGLES.mProgramCount;
                pCmdCache->mInstanceLocation = pCmdCache->pRootSignature->mGLES.pDescriptorGlLocations[locationIndex];
                break;
            }
        }
    }
}

void gl_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);

    if (gExtensionSupport.mHasVertexArrayObjectOES)
    {
        util_bind_vao(cmdCache, 0, false); // firstVertex does not matter for glDrawArrays
    }

    util_get_instance_location(cmdCache);

    // GLES 2.0 with glsl #version 100 does not support instancing
    // Simulate instancing
    if (cmdCache->mInstanceLocation < 0)
    {
        LOGF(LogLevel::eERROR, "No GLES Instance ID exists within the shader program {%d}", cmdCache->mPipeline);
        ASSERT(false);
        return;
    }

    for (uint32_t instanceIndex = firstInstance; instanceIndex < firstInstance + instanceCount; ++instanceIndex)
    {
        util_gl_set_uniform(cmdCache->mInstanceLocation, (uint8_t*)&instanceIndex, GL_INT, 1);
        CHECK_GLRESULT(glDrawArrays(cmdCache->mGlPrimitiveTopology, firstVertex, vertexCount));
    }
}

void gl_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);
    ASSERT(cmdCache->pIndexBuffer);

    if (gExtensionSupport.mHasVertexArrayObjectOES)
    {
        util_bind_vao(cmdCache, firstVertex, true);
    }
    else
    {
        // GLES has no support for vertex offset during glDrawElements
        // Therefore, we reset the vertex attributes based on the last known buffer strides and offsets
        if (firstVertex > 0)
        {
            util_bind_vertex_attribute_buffers(cmdCache, firstVertex, 0);
        }
    }

    uint32_t offset = firstIndex * sizeof(GLushort) + cmdCache->mIndexBufferOffset;

    // Use GPU resource
    if (cmdCache->pIndexBuffer->mGLES.mBuffer)
    {
        CHECK_GLRESULT(glDrawElements(cmdCache->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)(uintptr_t)offset));
    }
    else // Use CPU resource
    {
        CHECK_GLRESULT(glDrawElements(cmdCache->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)((uint8_t*)cmdCache->pIndexBuffer->mGLES.pGLCpuMappedAddress + offset)));
    }
}

void gl_cmdDrawIndexedInstanced(
    Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    ASSERT(pCmd);
    CmdCache* cmdCache = pCmd->mGLES.pCmdPool->pCmdCache;
    ASSERT(cmdCache->isStarted);
    ASSERT(cmdCache->pIndexBuffer);

    if (gExtensionSupport.mHasVertexArrayObjectOES)
    {
        util_bind_vao(cmdCache, firstVertex, true);
    }
    else
    {
        // GLES has no support for vertex offset during glDrawElements
        // Therefore, we reset the vertex attributes based on the last known buffer strides and offsets
        if (firstVertex > 0)
        {
            util_bind_vertex_attribute_buffers(cmdCache, firstVertex, 0);
        }
    }

    util_get_instance_location(cmdCache);

    uint32_t offset = firstIndex * sizeof(GLushort) + cmdCache->mIndexBufferOffset;

    // GLES 2.0 with glsl #version 100 does not support instancing
    // Simulate instancing
    if (cmdCache->mInstanceLocation < 0)
    {
        LOGF(LogLevel::eERROR, "No GLES Instance ID exists within the shader program {%d}", cmdCache->mPipeline);
        ASSERT(false);
        return;
    }

    // Use GPU resource
    if (cmdCache->pIndexBuffer->mGLES.mBuffer)
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < firstInstance + instanceCount; ++instanceIndex)
        {
            util_gl_set_uniform(cmdCache->mInstanceLocation, (uint8_t*)&instanceIndex, GL_INT, 1);
            CHECK_GLRESULT(glDrawElements(cmdCache->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)(uintptr_t)offset));
        }
    }
    else // Use CPU resource
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < firstInstance + instanceCount; ++instanceIndex)
        {
            util_gl_set_uniform(cmdCache->mInstanceLocation, (uint8_t*)&instanceIndex, GL_INT, 1);
            CHECK_GLRESULT(glDrawElements(cmdCache->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)((uint8_t*)cmdCache->pIndexBuffer->mGLES.pGLCpuMappedAddress + offset)));
        }
    }
}

void gl_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mGLES.pCmdPool->pCmdCache->isStarted);

    // Unsupported for GLES 2.0
}

void gl_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    ASSERT(pCmd);
    ASSERT(pSrcBuffer);
    ASSERT(pBuffer);
    ASSERT(pCmd->mGLES.pCmdPool->pCmdCache->isStarted);

    CHECK_GLRESULT(glBindBuffer(pBuffer->mGLES.mTarget, pBuffer->mGLES.mBuffer));
    CHECK_GLRESULT(glBufferSubData(pBuffer->mGLES.mTarget, dstOffset, size, (uint8_t*)pSrcBuffer->pCpuMappedAddress + srcOffset));
    CHECK_GLRESULT(glBindBuffer(pBuffer->mGLES.mTarget, GL_NONE));
}

typedef struct SubresourceDataDesc
{
    uint64_t mSrcOffset;
    uint32_t mMipLevel;
    uint32_t mArrayLayer;
} SubresourceDataDesc;

void gl_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
    ASSERT(pCmd);
    ASSERT(pSubresourceDesc);
    ASSERT(pTexture);
    ASSERT(pCmd->mGLES.pCmdPool->pCmdCache->isStarted);

    uint32_t width = max(1u, (uint32_t)(pTexture->mWidth >> pSubresourceDesc->mMipLevel));
    uint32_t height = max(1u, (uint32_t)(pTexture->mHeight >> pSubresourceDesc->mMipLevel));
    ASSERT(pSrcBuffer->pCpuMappedAddress);

    GLenum target = pTexture->mGLES.mTarget;
    if (pTexture->mGLES.mTarget == GL_TEXTURE_CUBE_MAP)
    {
        switch (pSubresourceDesc->mArrayLayer)
        {
        case 0:	target = GL_TEXTURE_CUBE_MAP_POSITIVE_X; break;
        case 1:	target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X; break;
        case 2:	target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y; break;
        case 3:	target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y; break;
        case 4:	target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z; break;
        case 5:	target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; break;
        default: LOGF(LogLevel::eERROR, "Unable to update cubemap subresource with more than 6 sides!");
        }
    }

    CHECK_GLRESULT(glBindTexture(pTexture->mGLES.mTarget, pTexture->mGLES.mTexture));
    if (pTexture->mGLES.mType == GL_NONE) // Compressed image
    {
        GLsizei imageByteSize = util_get_compressed_texture_size(pTexture->mGLES.mInternalFormat, width, height);
        CHECK_GLRESULT(glCompressedTexImage2D(target, pSubresourceDesc->mMipLevel, pTexture->mGLES.mInternalFormat,
            width, height, 0, imageByteSize, (uint8_t*)pSrcBuffer->pCpuMappedAddress + pSubresourceDesc->mSrcOffset));
    }
    else
    {
        CHECK_GLRESULT(glTexImage2D(target, pSubresourceDesc->mMipLevel, pTexture->mGLES.mGlFormat,
            width, height, 0, pTexture->mGLES.mGlFormat, pTexture->mGLES.mType, (uint8_t*)pSrcBuffer->pCpuMappedAddress + pSubresourceDesc->mSrcOffset));
    }
    pTexture->mGLES.mStateModified = true;
    CHECK_GLRESULT(glBindTexture(pTexture->mGLES.mTarget, GL_NONE));
}

/************************************************************************/
// Transition Commands
/************************************************************************/
void gl_cmdResourceBarrier(Cmd* pCmd,
    uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers,
    uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
    uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void gl_acquireNextImage(
    Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
    ASSERT(pRenderer);
    ASSERT(pSignalSemaphore || pFence);

    *pSwapChainImageIndex = 0;

    if (pFence)
    {
        pFence->mGLES.mSubmitted = true;
    }
    else
    {
        pSignalSemaphore->mGLES.mSignaled = true;
    }
}

static void util_handle_wait_semaphores(Semaphore** ppWaitSemaphores, uint32_t waitSemaphoreCount)
{
    if (waitSemaphoreCount > 0)
    {
        ASSERT(ppWaitSemaphores);
    }

    bool shouldFlushGl = false;

    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        if (ppWaitSemaphores[i]->mGLES.mSignaled)
        {
            ppWaitSemaphores[i]->mGLES.mSignaled = false;
        }
        else
        {
            shouldFlushGl = true;
        }
    }

    if (shouldFlushGl)
    {
        CHECK_GLRESULT(glFlush());
    }
}

void gl_queueSubmit(
    Queue* pQueue, const QueueSubmitDesc* pDesc)
{
    uint32_t cmdCount = pDesc->mCmdCount;
    Cmd** ppCmds = pDesc->ppCmds;
    Fence* pFence = pDesc->pSignalFence;
    uint32_t signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
    Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

    util_handle_wait_semaphores(pDesc->ppWaitSemaphores, pDesc->mWaitSemaphoreCount);

    ASSERT(cmdCount > 0);
    ASSERT(ppCmds);

    if (ppCmds[0]->mGLES.pCmdPool->pCmdCache->mRasterizerState.mScissorTest)
    {
        // Disable scissor test for swap buffers, so we draw full screen and not latest set scissor location
        CHECK_GLRESULT(glDisable(GL_SCISSOR_TEST));
        ppCmds[0]->mGLES.pCmdPool->pCmdCache->mRasterizerState.mScissorTest = GL_FALSE;
    }

    if (signalSemaphoreCount > 0)
    {
        ASSERT(ppSignalSemaphores);
    }

    for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
    {
        if (!ppSignalSemaphores[i]->mGLES.mSignaled)
        {
            ppSignalSemaphores[i]->mGLES.mSignaled = true;
        }
    }

    if (pFence)
    {
        pFence->mGLES.mSubmitted = true;
    }
}

void gl_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    util_handle_wait_semaphores(pDesc->ppWaitSemaphores, pDesc->mWaitSemaphoreCount);

    swapGLBuffers(pDesc->pSwapChain->mGLES.pSurface);
}

void gl_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    UNREF_PARAM(pRenderer);
    if (pFence->mGLES.mSubmitted)
    {
        *pFenceStatus = FENCE_STATUS_COMPLETE;
    }
    else
    {
        *pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
    }
}

void gl_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
    ASSERT(pRenderer);
    ASSERT(fenceCount);
    ASSERT(ppFences);

    bool waitForGL = false;

    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        if (ppFences[i]->mGLES.mSubmitted)
        {
            ppFences[i]->mGLES.mSubmitted = false;
        }
        else
        {
            waitForGL = true;
        }
    }

    if (waitForGL)
    {
        CHECK_GLRESULT(glFinish());
    }
}

void gl_waitQueueIdle(Queue* pQueue)
{
    CHECK_GLRESULT(glFinish());
}

void gl_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
    UNREF_PARAM(pRenderer);
    // Initial vsync value is passed in with the desc when client creates a swapchain.
    ASSERT(*ppSwapChain);
    (*ppSwapChain)->mEnableVsync = !(*ppSwapChain)->mEnableVsync;

    setGLSwapInterval((*ppSwapChain)->mEnableVsync);
}

/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat gl_getSupportedSwapchainFormat(Renderer *pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    return TinyImageFormat_R8G8B8A8_UNORM;
}

uint32_t gl_getRecommendedSwapchainImageCount(Renderer*, const WindowHandle*)
{
    return 1;
}
/************************************************************************/
// Indirect Draw functions
/************************************************************************/
void gl_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
    // Unavailable in OpenGL ES 2.0
}

void gl_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
    // Unavailable in OpenGL ES 2.0
}

void gl_cmdExecuteIndirect(
    Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
    Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    // Unavailable in OpenGL ES 2.0
}

/************************************************************************/
// GPU Query Implementation
/************************************************************************/

uint32_t util_to_gl_query_type(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP: return GL_TIMESTAMP_EXT;
    case QUERY_TYPE_PIPELINE_STATISTICS: return GL_NONE;
    case QUERY_TYPE_OCCLUSION: return GL_NONE;
    default: ASSERT(false && "Invalid query heap type"); return GL_NONE;
    }
}

void gl_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    *pFrequency = 1.0 / 1e-9;
}

void gl_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueryPool);

    if (QUERY_TYPE_TIMESTAMP != pDesc->mType)
    {
        ASSERT(false && "Not supported");
        return;
    }

    // Only available as extension in OpenGL ES 2.0
    if (!pRenderer->pGpu->mSettings.mTimestampQueries)
    {
        ASSERT(false && "Not supported");
        return;
    }

    uint32_t queryCount = pDesc->mQueryCount * 2;

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool) + queryCount * sizeof(uint32_t));
    ASSERT(pQueryPool);
    pQueryPool->mGLES.pQueries = (uint32_t*)(pQueryPool + 1);
    pQueryPool->mCount = queryCount;
    pQueryPool->mGLES.mType = util_to_gl_query_type(pDesc->mType);

    // Only available as extension in OpenGL ES 2.0
    CHECK_GLRESULT(glGenQueriesEXT(queryCount, pQueryPool->mGLES.pQueries));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        CHECK_GLRESULT(glQueryCounterEXT(pQueryPool->mGLES.pQueries[i], pQueryPool->mGLES.mType));
    }

    *ppQueryPool = pQueryPool;
}

void gl_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);

    CHECK_GLRESULT(glDeleteQueriesEXT(pQueryPool->mCount, pQueryPool->mGLES.pQueries));

    SAFE_FREE(pQueryPool);
}

static void QueryCounter(Cmd* pCmd, QueryPool* pQueryPool, uint32_t index)
{
    ASSERT(pCmd);
    ASSERT(pQueryPool);
    ASSERT(pCmd->mGLES.pCmdPool->pCmdCache->isStarted);
    ASSERT(index < pQueryPool->mCount);

    // Only available as extension in OpenGL ES 2.0
    CHECK_GLRESULT(glQueryCounterEXT(pQueryPool->mGLES.pQueries[index], pQueryPool->mGLES.mType));
}

void gl_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    ASSERT(pQuery);
    QueryCounter(pCmd, pQueryPool, pQuery->mIndex * 2);
}

void gl_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    ASSERT(pQuery);
    QueryCounter(pCmd, pQueryPool, pQuery->mIndex * 2 + 1);
}

void gl_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
}

void gl_cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
}

void gl_getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    GLint available = 0;
    CHECK_GLRESULT(glGetQueryObjectivEXT(pQueryPool->mGLES.pQueries[queryIndex * 2 + 1], GL_QUERY_RESULT_AVAILABLE_EXT, &available));

    int32_t disjointOccurred;
    CHECK_GLRESULT(glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjointOccurred));
    pOutData->mValid = available && !disjointOccurred;
    if (pOutData->mValid)
    {
        CHECK_GLRESULT(glGetQueryObjectui64vEXT(pQueryPool->mGLES.pQueries[queryIndex * 2], GL_QUERY_RESULT_EXT, &pOutData->mBeginTimestamp));
        CHECK_GLRESULT(glGetQueryObjectui64vEXT(pQueryPool->mGLES.pQueries[queryIndex * 2 + 1], GL_QUERY_RESULT_EXT, &pOutData->mEndTimestamp));
    }
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void gl_calculateMemoryStats(Renderer* pRenderer, char** stats) {}

void gl_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes) {}

void gl_freeMemoryStats(Renderer* pRenderer, char* stats) {}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void gl_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pCmd);
    ASSERT(pName);

    // Only available as extension in OpenGL ES 2.0
    if (glPushGroupMarkerEXT)
        CHECK_GLRESULT(glPushGroupMarkerEXT(strlen(pName), pName));
#endif
}

void gl_cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pCmd);

    //Only available as extension OpenGL ES 2.0
    if (glPopGroupMarkerEXT)
        CHECK_GLRESULT(glPopGroupMarkerEXT());
#endif
}

void gl_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pCmd);
    ASSERT(pName);

    // Only available as extension OpenGL ES 2.0
    if(glInsertEventMarkerEXT)
        CHECK_GLRESULT(glInsertEventMarkerEXT(strlen(pName), pName));
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void gl_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);

    // Only available as extension OpenGL ES 2.0
    if (glLabelObjectEXT && pBuffer->mGLES.mTarget != GL_NONE)
        CHECK_GLRESULT(glLabelObjectEXT(GL_BUFFER_OBJECT_EXT, pBuffer->mGLES.mBuffer, strlen(pName), pName));
#endif
}

void gl_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);

    // Only available as extension OpenGL ES 2.0
    if (glLabelObjectEXT)
        CHECK_GLRESULT(glLabelObjectEXT(GL_TEXTURE, pTexture->mGLES.mTexture, strlen(pName), pName));
#endif
}

void gl_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pRenderTarget);
    ASSERT(pName);

    // Only available as extension OpenGL ES 2.0
    if (!glLabelObjectEXT)
        return;

    if(pRenderTarget->pTexture)
        setTextureName(pRenderer, pRenderTarget->pTexture, pName);
    else
    {
        if (pRenderTarget->mGLES.mDepthTarget)
            CHECK_GLRESULT(glLabelObjectEXT(GL_RENDERBUFFER, pRenderTarget->mGLES.mDepthTarget, strlen(pName), pName));

        if (pRenderTarget->mGLES.mStencilTarget)
            CHECK_GLRESULT(glLabelObjectEXT(GL_RENDERBUFFER, pRenderTarget->mGLES.mStencilTarget, strlen(pName), pName));
    }

#endif
}

void gl_setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pPipeline);
    ASSERT(pName);

    // Only available as extension OpenGL ES 2.0
    if (glLabelObjectEXT)
        CHECK_GLRESULT(glLabelObjectEXT(GL_PROGRAM_OBJECT_EXT, pPipeline->mGLES.pRootSignature->mGLES.pProgramTargets[pPipeline->mGLES.mRootSignatureIndex], strlen(pName), pName));
#endif
}

#endif

void initGLESRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
    // API functions
    addFence = gl_addFence;
    removeFence = gl_removeFence;
    addSemaphore = gl_addSemaphore;
    removeSemaphore = gl_removeSemaphore;
    addQueue = gl_addQueue;
    removeQueue = gl_removeQueue;
    addSwapChain = gl_addSwapChain;
    removeSwapChain = gl_removeSwapChain;

    // command pool functions
    addCmdPool = gl_addCmdPool;
    removeCmdPool = gl_removeCmdPool;
    addCmd = gl_addCmd;
    removeCmd = gl_removeCmd;
    addCmd_n = gl_addCmd_n;
    removeCmd_n = gl_removeCmd_n;

    addRenderTarget = gl_addRenderTarget;
    removeRenderTarget = gl_removeRenderTarget;
    addSampler = gl_addSampler;
    removeSampler = gl_removeSampler;

    // Resource Load functions
    addBuffer = gl_addBuffer;
    removeBuffer = gl_removeBuffer;
    mapBuffer = gl_mapBuffer;
    unmapBuffer = gl_unmapBuffer;
    cmdUpdateBuffer = gl_cmdUpdateBuffer;
    cmdUpdateSubresource = gl_cmdUpdateSubresource;
    addTexture = gl_addTexture;
    removeTexture = gl_removeTexture;

    // shader functions
    addShaderBinary = gl_addShaderBinary;
    removeShader = gl_removeShader;

    addRootSignature = gl_addRootSignature;
    removeRootSignature = gl_removeRootSignature;
    getDescriptorIndexFromName = gl_getDescriptorIndexFromName;

    // pipeline functions
    addPipeline = gl_addPipeline;
    removePipeline = gl_removePipeline;
    addPipelineCache = gl_addPipelineCache;
    getPipelineCacheData = gl_getPipelineCacheData;
    removePipelineCache = gl_removePipelineCache;
#if defined(SHADER_STATS_AVAILABLE)
    addPipelineStats = NULL;
    removePipelineStats = NULL;
#endif

    // Descriptor Set functions
    addDescriptorSet = gl_addDescriptorSet;
    removeDescriptorSet = gl_removeDescriptorSet;
    updateDescriptorSet = gl_updateDescriptorSet;

    // command buffer functions
    resetCmdPool = gl_resetCmdPool;
    beginCmd = gl_beginCmd;
    endCmd = gl_endCmd;
    cmdBindRenderTargets = gl_cmdBindRenderTargets;
    cmdSetViewport = gl_cmdSetViewport;
    cmdSetScissor = gl_cmdSetScissor;
    cmdSetStencilReferenceValue = gl_cmdSetStencilReferenceValue;
    cmdBindPipeline = gl_cmdBindPipeline;
    cmdBindDescriptorSet = gl_cmdBindDescriptorSet;
    cmdBindPushConstants = gl_cmdBindPushConstants;
    cmdBindDescriptorSetWithRootCbvs = gl_cmdBindDescriptorSetWithRootCbvs;
    cmdBindIndexBuffer = gl_cmdBindIndexBuffer;
    cmdBindVertexBuffer = gl_cmdBindVertexBuffer;
    cmdDraw = gl_cmdDraw;
    cmdDrawInstanced = gl_cmdDrawInstanced;
    cmdDrawIndexed = gl_cmdDrawIndexed;
    cmdDrawIndexedInstanced = gl_cmdDrawIndexedInstanced;
    cmdDispatch = gl_cmdDispatch;

    // Transition Commands
    cmdResourceBarrier = gl_cmdResourceBarrier;

    // queue/fence/swapchain functions
    acquireNextImage = gl_acquireNextImage;
    queueSubmit = gl_queueSubmit;
    queuePresent = gl_queuePresent;
    waitQueueIdle = gl_waitQueueIdle;
    getFenceStatus = gl_getFenceStatus;
    waitForFences = gl_waitForFences;
    toggleVSync = gl_toggleVSync;

    getSupportedSwapchainFormat = gl_getSupportedSwapchainFormat;
    getRecommendedSwapchainImageCount = gl_getRecommendedSwapchainImageCount;

    //indirect Draw functions
    addIndirectCommandSignature = gl_addIndirectCommandSignature;
    removeIndirectCommandSignature = gl_removeIndirectCommandSignature;
    cmdExecuteIndirect = gl_cmdExecuteIndirect;

    /************************************************************************/
    // GPU Query Interface
    /************************************************************************/
    getTimestampFrequency = gl_getTimestampFrequency;
    addQueryPool = gl_addQueryPool;
    removeQueryPool = gl_removeQueryPool;
    cmdBeginQuery = gl_cmdBeginQuery;
    cmdEndQuery = gl_cmdEndQuery;
    cmdResolveQuery = gl_cmdResolveQuery;
    cmdResetQuery = gl_cmdResetQuery;
    getQueryData = gl_getQueryData;
    /************************************************************************/
    // Stats Info Interface
    /************************************************************************/
    calculateMemoryStats = gl_calculateMemoryStats;
    calculateMemoryUse = gl_calculateMemoryUse;
    freeMemoryStats = gl_freeMemoryStats;
    /************************************************************************/
    // Debug Marker Interface
    /************************************************************************/
    cmdBeginDebugMarker = gl_cmdBeginDebugMarker;
    cmdEndDebugMarker = gl_cmdEndDebugMarker;
    cmdAddDebugMarker = gl_cmdAddDebugMarker;
    /************************************************************************/
    // Resource Debug Naming Interface
    /************************************************************************/
    setBufferName = gl_setBufferName;
    setTextureName = gl_setTextureName;
    setRenderTargetName = gl_setRenderTargetName;
    setPipelineName = gl_setPipelineName;

    gl_initRenderer(appName, pSettings, ppRenderer);
}

void exitGLESRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    gl_exitRenderer(pRenderer);
}

#endif
