/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#if defined(GLES) 
#define RENDERER_IMPLEMENTATION

#include "../../ThirdParty/OpenSource/EASTL/functional.h"
#include "../../ThirdParty/OpenSource/EASTL/string_hash_map.h"
#include "../../ThirdParty/OpenSource/EASTL/sort.h"
#include "../../OS/Interfaces/ILog.h"
#include "../IRenderer.h"
#include "../../OS/Core/RingBuffer.h"
#include "../../ThirdParty/OpenSource/EASTL/functional.h"
#include "../../OS/Core/GPUConfig.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "GLESContextCreator.h"
#include "GLESCapsBuilder.h"

// Default GL ES 2.0 support
#include "../../ThirdParty/OpenSource/OpenGL/GLES2/gl2.h"
#include "../../ThirdParty/OpenSource/OpenGL/GLES2/gl2ext.h"

#include "../../OS/Interfaces/IMemory.h"

extern void gles_createShaderReflection(Shader* pProgram, ShaderReflection* pOutReflection, const BinaryShaderDesc* pDesc);

/************************************************************************/
// Descriptor Set Structure
/************************************************************************/
using DescriptorNameToIndexMap = eastl::string_hash_map<uint32_t>;

typedef struct DescriptorIndexMap
{
	eastl::string_hash_map<uint32_t> mMap;
} DescriptorIndexMap;

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================
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

struct CmdCache
{
	bool		isStarted;
	uint32_t	mIndexBufferOffset;
	uint32_t	mVertexBufferOffset;
	uint32_t	mVertexBufferStride;
	Pipeline*	pPipeline;
};

/************************************************************************/
// Internal globals
/************************************************************************/
static GLRasterizerState gDefaultRasterizer = {};
static GLDepthStencilState gDefaultDepthStencilState = {};
static GLBlendState gDefaultBlendState = {};

#if defined(RENDERER_IMPLEMENTATION)

/************************************************************************/
// Internal util functions
/************************************************************************/

inline const char* util_get_format_string(GLenum value)
{
	switch (value)
	{
		// ASTC
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR: return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
	case GL_COMPRESSED_RGBA_ASTC_5x4_KHR: return "GL_COMPRESSED_RGBA_ASTC_5x4_KHR";
	case GL_COMPRESSED_RGBA_ASTC_5x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
	case GL_COMPRESSED_RGBA_ASTC_6x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_6x5_KHR";
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
	case GL_COMPRESSED_RGBA_ASTC_8x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x5_KHR";
	case GL_COMPRESSED_RGBA_ASTC_8x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x6_KHR";
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
	case GL_COMPRESSED_RGBA_ASTC_10x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x5_KHR";
	case GL_COMPRESSED_RGBA_ASTC_10x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x6_KHR";
	case GL_COMPRESSED_RGBA_ASTC_10x8_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x8_KHR";
	case GL_COMPRESSED_RGBA_ASTC_10x10_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
	case GL_COMPRESSED_RGBA_ASTC_12x10_KHR: return "GL_COMPRESSED_RGBA_ASTC_12x10_KHR";
	case GL_COMPRESSED_RGBA_ASTC_12x12_KHR: return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR";

	case GL_COMPRESSED_RGBA_ASTC_3x3x3_OES: return "GL_COMPRESSED_RGBA_ASTC_3x3x3_OES";
	case GL_COMPRESSED_RGBA_ASTC_4x3x3_OES: return "GL_COMPRESSED_RGBA_ASTC_4x3x3_OES";
	case GL_COMPRESSED_RGBA_ASTC_4x4x3_OES: return "GL_COMPRESSED_RGBA_ASTC_4x4x3_OES";
	case GL_COMPRESSED_RGBA_ASTC_4x4x4_OES: return "GL_COMPRESSED_RGBA_ASTC_4x4x4_OES";
	case GL_COMPRESSED_RGBA_ASTC_5x4x4_OES: return "GL_COMPRESSED_RGBA_ASTC_5x4x4_OES";
	case GL_COMPRESSED_RGBA_ASTC_5x5x4_OES: return "GL_COMPRESSED_RGBA_ASTC_5x5x4_OES";
	case GL_COMPRESSED_RGBA_ASTC_5x5x5_OES: return "GL_COMPRESSED_RGBA_ASTC_5x5x5_OES";
	case GL_COMPRESSED_RGBA_ASTC_6x5x5_OES: return "GL_COMPRESSED_RGBA_ASTC_6x5x5_OES";
	case GL_COMPRESSED_RGBA_ASTC_6x6x5_OES: return "GL_COMPRESSED_RGBA_ASTC_6x6x5_OES";
	case GL_COMPRESSED_RGBA_ASTC_6x6x6_OES: return "GL_COMPRESSED_RGBA_ASTC_6x6x6_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES";
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES";

	case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE: return "GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE";

	case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT: return "GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT";

	case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT: return "GL_COMPRESSED_RGBA_BPTC_UNORM_EXT";

	default: return "GL_UNKNOWN_FORMAT";
	}
}

GLsizei util_get_compressed_texture_size(GLenum format, uint32_t width, uint32_t height)
{
	switch (format)
	{
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR: return ((uint32_t)ceil(width / 4.0f) * (uint32_t)ceil(height / 4.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
	case GL_COMPRESSED_RGBA_ASTC_5x4_KHR: return ((uint32_t)ceil(width / 5.0f) * (uint32_t)ceil(height / 4.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_5x5_KHR: return ((uint32_t)ceil(width / 5.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_6x5_KHR: return ((uint32_t)ceil(width / 6.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR: return ((uint32_t)ceil(width / 6.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_8x5_KHR: return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:					   
	case GL_COMPRESSED_RGBA_ASTC_8x6_KHR: return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:					   
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR: return ((uint32_t)ceil(width / 8.0f) * (uint32_t)ceil(height / 8.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x5_KHR: return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 5.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x6_KHR: return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 6.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x8_KHR: return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 8.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
	case GL_COMPRESSED_RGBA_ASTC_10x10_KHR: return ((uint32_t)ceil(width / 10.0f) * (uint32_t)ceil(height / 10.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
	case GL_COMPRESSED_RGBA_ASTC_12x10_KHR: return ((uint32_t)ceil(width / 12.0f) * (uint32_t)ceil(height / 10.0f)) << 4;
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
	case GL_COMPRESSED_RGBA_ASTC_12x12_KHR: return ((uint32_t)ceil(width / 12.0f) * (uint32_t)ceil(height / 12.0f)) << 4;
	case GL_ETC1_RGB8_OES: return ((uint32_t)ceil(width / 4.0f) * (uint32_t)ceil(height / 4.0f)) << 3;
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
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		// Framebuffer status
	case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
	case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";

	default: return "GL_UNKNOWN_ERROR";
	}
}

#if defined(_WINDOWS)
#pragma comment(lib, "opengl32.lib")
#endif
#define SAFE_FREE(p_var)  \
	if (p_var)            \
	{                     \
		tf_free(p_var);	  \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#define CHECK_GLRESULT(exp)                                                            \
{                                                                                      \
	exp;                                                                               \
	GLenum glRes = glGetError();                                                       \
	if (glRes != GL_NO_ERROR)                                                          \
	{                                                                                  \
		LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes));  \
		ASSERT(false);                                                                 \
	}                                                                                  \
}

#define CHECK_GL_RETURN_RESULT(var, exp)									          \
{                                                                                     \
	var = exp;                                                                        \
	GLenum glRes = glGetError();                                                      \
	if (glRes != GL_NO_ERROR)                                                         \
	{                                                                                 \
		LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes)); \
		ASSERT(false);                                                                \
	}                                                                                 \
}

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
	default: ASSERT(false && "Unknown GL type"); return 0;
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
	default: ASSERT(false && "Unknown GL type");
	}
}

static GLint util_to_gl_usage(ResourceMemoryUsage mem)
{
	switch (mem)
	{
	case RESOURCE_MEMORY_USAGE_GPU_ONLY: return GL_STATIC_DRAW;
	case RESOURCE_MEMORY_USAGE_CPU_ONLY: return GL_NONE;
	case RESOURCE_MEMORY_USAGE_CPU_TO_GPU: return GL_DYNAMIC_DRAW;
	case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: return GL_STREAM_DRAW;
	default: ASSERT(false && "Invalid Memory Usage"); return GL_DYNAMIC_DRAW;
	}
}

static GLenum util_to_gl_adress_mode(AddressMode mode)
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
		return GL_CLAMP_TO_BORDER_OES; // Not always available
	default: ASSERT(false && "Invalid AdressMode"); return GL_REPEAT;
	}
}

static GLenum util_to_gl_blend_const(BlendConstant bconst, bool isSrc)
{
	switch (bconst)
	{
		case BC_ZERO				  : return GL_ZERO;
		case BC_ONE					  : return GL_ONE;
		case BC_SRC_COLOR			  : return GL_SRC_COLOR;
		case BC_ONE_MINUS_SRC_COLOR	  : return GL_ONE_MINUS_SRC_COLOR;
		case BC_DST_COLOR			  : return GL_DST_COLOR;
		case BC_ONE_MINUS_DST_COLOR	  : return GL_ONE_MINUS_DST_COLOR;
		case BC_SRC_ALPHA			  : return GL_SRC_ALPHA;
		case BC_ONE_MINUS_SRC_ALPHA	  : return GL_ONE_MINUS_SRC_ALPHA;
		case BC_DST_ALPHA			  : return GL_DST_ALPHA;
		case BC_ONE_MINUS_DST_ALPHA	  : return GL_ONE_MINUS_DST_ALPHA;
		case BC_SRC_ALPHA_SATURATE	  : return GL_SRC_ALPHA_SATURATE;
		case BC_BLEND_FACTOR		  : return GL_CONSTANT_COLOR;
		case BC_ONE_MINUS_BLEND_FACTOR: return GL_ONE_MINUS_CONSTANT_COLOR;
		default:  ASSERT(false && "Invalid BlendConstant"); return isSrc ? GL_ONE : GL_ZERO;
	}
}

static GLenum util_to_gl_blend_mode(BlendMode mode)
{
	switch (mode)
	{
		case BM_ADD: return GL_FUNC_ADD;
		case BM_SUBTRACT: return GL_FUNC_ADD;
		case BM_REVERSE_SUBTRACT: return GL_FUNC_ADD;
		case BM_MIN: return GL_MIN; // GLES 3.2
		case BM_MAX: return GL_MAX; // GLES 3.2
		default:  ASSERT(false && "Invalid BlendMode"); return GL_FUNC_ADD;
	}
}

static GLenum util_to_gl_stencil_op(StencilOp op)
{
	switch (op)
	{
		case STENCIL_OP_KEEP	: return GL_KEEP;
		case STENCIL_OP_SET_ZERO: return GL_ZERO;
		case STENCIL_OP_REPLACE	: return GL_REPLACE;
		case STENCIL_OP_INVERT	: return GL_INVERT;
		case STENCIL_OP_INCR	: return GL_INCR;
		case STENCIL_OP_DECR	: return GL_DECR;
		case STENCIL_OP_INCR_SAT: return GL_INCR_WRAP;
		case STENCIL_OP_DECR_SAT: return GL_DECR_WRAP;
		default:  ASSERT(false && "Invalid StencilOp"); return GL_KEEP;
	}
}

static GLenum util_to_gl_compare_mode(CompareMode mode)
{
	switch (mode)
	{
	case CMP_NEVER: return GL_NEVER;
	case CMP_LESS: return GL_LESS;
	case CMP_EQUAL: return GL_EQUAL;
	case CMP_LEQUAL: return GL_LEQUAL;
	case CMP_GREATER: return GL_GREATER;
	case CMP_NOTEQUAL: return GL_NOTEQUAL;
	case CMP_GEQUAL: return GL_GEQUAL;
	case CMP_ALWAYS: return GL_ALWAYS;
	default: ASSERT(false && "Invalid CompareMode"); return GL_LESS;
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

	blendState.mBlendEnable = blendState.mSrcRGBFunc != GL_ONE || blendState.mDstRGBFunc != GL_ZERO || blendState.mSrcAlphaFunc != GL_ONE || blendState.mDstAlphaFunc != GL_ZERO;

	return blendState;

	// Unhandled
	// pDesc->mMasks[blendDescIndex]
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
	depthState.mDepthTest = pDesc->mDepthTest; // glEnable / glDisalbe(GL_DEPTH_TEST)
	depthState.mDepthWrite = pDesc->mDepthWrite; // glDepthMask(GL_TRUE / GL_FALSE);
	depthState.mDepthFunc = util_to_gl_compare_mode(pDesc->mDepthFunc); // glDepthFunc(GL_LESS)


	depthState.mStencilTest = pDesc->mStencilTest;  // glEnable / glDisalbe(GL_STENCIL_TEST)
	depthState.mStencilWriteMask = pDesc->mStencilWriteMask;  // glStencilMask(mask)

	depthState.mStencilFrontFunc = util_to_gl_compare_mode(pDesc->mStencilFrontFunc); // glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, ref_value?, mask (1))
	// glStencilOpSeparate(GLenum face, GLenum GL_KEEP, GLenum GL_KEEP, GLenum GL_KEEP);
	depthState.mStencilFrontFail = util_to_gl_stencil_op(pDesc->mStencilFrontFail);
	depthState.mDepthFrontFail = util_to_gl_stencil_op(pDesc->mDepthFrontFail);
	depthState.mStencilFrontPass = util_to_gl_stencil_op(pDesc->mStencilFrontPass);

	depthState.mStencilBackFunc = util_to_gl_compare_mode(pDesc->mStencilBackFunc); // glStencilFuncSeparate(GL_BACK, GL_ALWAYS, ref_value?, mask (1))
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
		default: ASSERT(false && "Unknown cull mode"); break;
	}

	switch (pDesc->mFrontFace)
	{
	case FrontFace::FRONT_FACE_CCW:
		rasterizationState.mFrontFace = GL_CCW;
		break;
	case FrontFace::FRONT_FACE_CW:
		rasterizationState.mFrontFace = GL_CW;
		break;
	default: ASSERT(false && "Unknown front face mode"); break;
	}
	rasterizationState.mScissorTest = pDesc->mScissor;

	// Unhandled
	// pDesc->mDepthBias;
	// pDesc->mSlopeScaledDepthBias;
	// pDesc->mFillMode; // Not suported for GLES
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
API_INTERFACE void FORGE_CALLCONV addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void FORGE_CALLCONV removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void FORGE_CALLCONV addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
API_INTERFACE void FORGE_CALLCONV removeTexture(Renderer* pRenderer, Texture* pTexture);
API_INTERFACE void FORGE_CALLCONV mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
API_INTERFACE void FORGE_CALLCONV unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
API_INTERFACE void FORGE_CALLCONV gl_compileShader(Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);

/************************************************************************/
// Internal init functions
/************************************************************************/
static bool addDevice(Renderer* pRenderer, const RendererDesc* pDesc)
{
	pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
	ASSERT(pRenderer->pActiveGpuSettings);

	// Shader caps
	GLboolean isShaderCompilerSupported = GL_FALSE;
	GLint nShaderBinaryFormats = 0;

	GLint maxVertexAttr = 4; // Max number of 4-component generic vertex attributes accessible to a vertex shader
	GLint maxVertexUniformVectors = 128; // Max number of four-element floating-point, integer, or boolean vectors that can be held in uniform variable storage for a vertex shader
	GLint maxFragmentUniformVectors = 16; // Max number of four-element floating-point, integer, or boolean vectors that can be held in uniform variable storage for a fragment shader
	GLint maxVaryingVectors = 8;

	GLint maxTextureImageUnits = 8; // Max supported texture image units for fragment shader using glActiveTexture (initial value GL_TEXTURE0)
	GLint maxVertTextureImageUnits = 0; // Max supported texture image units for fragment shader using glActiveTexture (initial value GL_TEXTURE0)
	GLint maxCombinedTextureImageUnits = 8; // Max supported texture image units vertex/framgent shaders combined

	glGetBooleanv(GL_SHADER_COMPILER, &isShaderCompilerSupported);
	if (!isShaderCompilerSupported)
	{
		LOGF(LogLevel::eERROR, "Unsupported device! No shader compiler support for OpenGL ES 2.0");
		ASSERT(false);
	}

	glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nShaderBinaryFormats);

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttr);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniformVectors);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxFragmentUniformVectors);
	glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);

	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertTextureImageUnits);
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextureImageUnits);

	// Texture caps
	GLint packAlignment = 4;	// Byte alignment used for writing pixel data to memory
	GLint unpackAlignment = 4;	// Byte alignment used for reading pixel data from memory
	GLint nCompressedTextureFormats = 0;

	GLint maxCubeMapTextureSize = 16;
	GLint maxTextureSize = 64;
	GLint maxRenderBufferSize = 1;

	GLint maxViewportDims;

	glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nCompressedTextureFormats);
	//GL_COMPRESSED_TEXTURE_FORMATS

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapTextureSize);
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderBufferSize);

	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, &maxViewportDims);

	// Set active GPU settings
	pRenderer->pActiveGpuSettings->mUniformBufferAlignment = packAlignment; //TODO check if this is correct
	pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment = packAlignment;
	pRenderer->pActiveGpuSettings->mUploadBufferTextureRowAlignment = packAlignment;
	pRenderer->pActiveGpuSettings->mMaxVertexInputBindings = maxVertexAttr;
	pRenderer->pActiveGpuSettings->mMaxTextureImageUnits = maxCombinedTextureImageUnits;

	pRenderer->pActiveGpuSettings->mMultiDrawIndirect = 0;
	pRenderer->pActiveGpuSettings->mROVsSupported = 0;
	pRenderer->pActiveGpuSettings->mTessellationSupported = 0;
	pRenderer->pActiveGpuSettings->mGeometryShaderSupported = 0;

	const char* glRenderer = (const char*)glGetString(GL_RENDERER);
	const char* glVersion = (const char*)glGetString(GL_VERSION);
	const char* glVendor = (const char*)glGetString(GL_VENDOR);
	const char* glSLVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	char* glExtensions = (char*)glGetString(GL_EXTENSIONS);
	LOGF(LogLevel::eINFO, "Renderer: %s", glRenderer);
	LOGF(LogLevel::eINFO, "Version: %s", glVersion);
	LOGF(LogLevel::eINFO, "Vendor: %s", glVendor);
	LOGF(LogLevel::eINFO, "Glsl version: %s", glSLVersion);
	LOGF(LogLevel::eINFO, "Extensions: %s", glExtensions);

	// Validate requested device extensions
	uint32_t supportedExtensions = 0;
	const char* ptr = strtok(glExtensions, " ");
	while (ptr != nullptr)
	{
		for (uint32_t i = 0; i < pDesc->mDeviceExtensionCount; ++i)
		{
			if (strcmp(ptr, pDesc->ppDeviceExtensions[i]) == 0)
			{
				++supportedExtensions;
				break;
			}
		}
		ptr = strtok(nullptr, " ");
	}

	if (supportedExtensions != pDesc->mDeviceExtensionCount)
	{
		LOGF(LogLevel::eERROR, "Not all requested device extensions are supported on the device! Device support %u of %u requested.", supportedExtensions, pDesc->mDeviceExtensionCount);
		return false;
	}

	GPUVendorPreset& gpuVendorPresets = pRenderer->pActiveGpuSettings->mGpuVendorPreset;
	strncpy(gpuVendorPresets.mVendorId, glVendor, strlen(glVendor) + 1);
	strncpy(gpuVendorPresets.mGpuName, glRenderer, strlen(glRenderer) + 1);
	strncpy(gpuVendorPresets.mGpuDriverVersion, glVersion, strlen(glVersion) + 1);
	gpuVendorPresets.mPresetLevel = GPUPresetLevel::GPU_PRESET_LOW; // Default

	pRenderer->mLinkedNodeCount = 1;
	pRenderer->mGpuMode = GPU_MODE_SINGLE;

	return true;
}

static void removeDevice(Renderer* pRenderer)
{
	SAFE_FREE(pRenderer->pActiveGpuSettings);
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
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	gDefaultBlendState = util_to_blend_desc(&blendStateDesc);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilWriteMask = 1;
	gDefaultDepthStencilState = util_to_depth_desc(&depthStateDesc);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	gDefaultRasterizer = util_to_rasterizer_desc(&rasterizerStateDesc);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
	ASSERT(ppRenderer);
	ASSERT(pDesc);

	Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
	ASSERT(pRenderer);

	LOGF(LogLevel::eINFO, "Using OpenGL ES 2.0");
	if(!initGL(&pRenderer->pConfig))
	{
		SAFE_FREE(pRenderer);
		return;
	}

	if (!initGLContext(pRenderer->pConfig, &pRenderer->pContext))
	{
		SAFE_FREE(pRenderer);
		return;
	}

	pRenderer->mGpuMode = pDesc->mGpuMode;
	pRenderer->mShaderTarget = pDesc->mShaderTarget;
	pRenderer->mEnableGpuBasedValidation = pDesc->mEnableGPUBasedValidation;
	pRenderer->mApi = RENDERER_API_GLES;

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	addDevice(pRenderer, pDesc);

	utils_caps_builder(pRenderer);

	add_default_resources(pRenderer);

	// Renderer is good!
	*ppRenderer = pRenderer;
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	
	removeDevice(pRenderer);

	removeGLContext(&pRenderer->pContext);

	removeGL(&pRenderer->pConfig);

	// Free all the renderer components
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(ppFence);

	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	// Fences are not available until OpenGL core 3.2
	// using glFinish for Gpu->Cpu sync

	pFence->mSubmitted = false;
	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pRenderer);
	ASSERT(pFence);

	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(ppSemaphore);

	Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
	ASSERT(pSemaphore);

	// Semaphores are not available on OpenGL ES 2.0
	// using glFlush for Gpu->Gpu sync

	// Set signal inital state.
	pSemaphore->mSignaled = false;

	*ppSemaphore = pSemaphore;
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(pSemaphore);

	SAFE_FREE(pSemaphore)
}

void addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueue);

	// NOTE: We will still use it to reference the renderer in the queue and to be able to generate
	// a dependency graph to serialize parallel GPU workload.
	Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	// Provided description for queue creation.
	// Note these don't really mean much w/ GLES but we can use it for debugging
	// what the client is intending to do.
	pQueue->mNodeIndex = pDesc->mNodeIndex;
	pQueue->mType = pDesc->mType;

	*ppQueue = pQueue;
}

void removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pRenderer);
	ASSERT(pQueue);

	SAFE_FREE(pQueue);
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);
	ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

	SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
	ASSERT(pSwapChain);

	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
	ASSERT(pSwapChain->ppRenderTargets);
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;

	// Create surface
	if (!addGLSurface(pRenderer->pContext, pRenderer->pConfig, &pDesc->mWindowHandle, &pSwapChain->pSurface))
	{
		SAFE_FREE(pSwapChain);
		return;
	}

	// Set swap interval
	setGLSwapInterval(pDesc->mEnableVsync);

	RenderTargetDesc descColor = {};
	descColor.mWidth = pDesc->mWidth;
	descColor.mHeight = pDesc->mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pDesc->mColorFormat;
	descColor.mClearValue = pDesc->mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.mFlags |= TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;

	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
	}

	pSwapChain->mImageCount = pDesc->mImageCount;
	pSwapChain->mImageIndex = 0;


	*ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
	{
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
	}

	removeGLSurface(&pSwapChain->pSurface);

	SAFE_FREE(pSwapChain);
}
/************************************************************************/
// Command Pool Functions
/************************************************************************/
void addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmdPool);

	// initialize to zero
	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pCmdCache = (CmdCache*)tf_calloc(1, sizeof(CmdCache));
	ASSERT(pCmdPool->pCmdCache);

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	//check validity of given renderer and command pool
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	SAFE_FREE(pCmdPool->pCmdCache);
	SAFE_FREE(pCmdPool);
}

void addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
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
	pCmd->pCmdPool = pDesc->pPool;

	*ppCmd = pCmd;
}

void removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	//verify that given command and pool are valid
	ASSERT(pRenderer);
	ASSERT(pCmd);

	SAFE_FREE(pCmd);
}

void addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
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

void removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
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
void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool const isDepth = TinyImageFormat_IsDepthAndStencil(pDesc->mFormat) ||
		TinyImageFormat_IsDepthOnly(pDesc->mFormat);

	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
	ASSERT(pRenderTarget);

	pRenderTarget->mType = isDepth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;

	pRenderTarget->mRenderBuffer = 0;

	pRenderTarget->mWidth = pDesc->mWidth;
	pRenderTarget->mHeight = pDesc->mHeight;
	pRenderTarget->mArraySize = pDesc->mArraySize;
	pRenderTarget->mDepth = pDesc->mDepth;
	pRenderTarget->mMipLevels = pDesc->mMipLevels;
	pRenderTarget->mSampleCount = pDesc->mSampleCount;
	pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
	pRenderTarget->mFormat = pDesc->mFormat;
	pRenderTarget->mClearValue = pDesc->mClearValue;

	*ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
	ASSERT(ppSampler);

	// initialize to zero
	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);

	if (pDesc->mMinFilter == FILTER_NEAREST)
	{
		pSampler->mMinFilter = GL_NEAREST;
		pSampler->mMipMapMode = pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
	}
	else
	{
		pSampler->mMinFilter = GL_LINEAR;
		pSampler->mMipMapMode = pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
	}
	pSampler->mMagFilter = pDesc->mMagFilter == FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
	
	pSampler->mAddressS = util_to_gl_adress_mode(pDesc->mAddressU);
	pSampler->mAddressT = util_to_gl_adress_mode(pDesc->mAddressV);
	pSampler->mCompareFunc = util_to_gl_compare_mode(pDesc->mCompareFunc);

	// GL_TEXTURE_MIN_FILTER Options:
	// GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
	// GL_TEXTURE_MAG_FILTER Options:
	// GL_NEAREST, GL_LINEAR
	// GL_TEXTURE_WRAP_S & GL_TEXTURE_WRAP_T Options:
	// GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, GL_REPEAT

	*ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	SAFE_FREE(pSampler);
}

/************************************************************************/
// Shader Functions
/************************************************************************/
void gl_compileShader(
	Renderer* pRenderer, ShaderTarget shaderTarget, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	ASSERT(code);
	ASSERT(codeSize > 0);
	ASSERT(pOut);

	if (stage != SHADER_STAGE_VERT && stage != SHADER_STAGE_FRAG)
	{
		LOGF(LogLevel::eERROR, "Unsuppored shader stage {%u} for OpenGL ES 2.0!", stage);
		return;
	}

	LOGF(LogLevel::eDEBUG, "Add shader {%s}", fileName);
	GLint compiled = GL_FALSE;
	GLuint shader = GL_NONE;
	CHECK_GL_RETURN_RESULT(shader, glCreateShader(stage == SHADER_STAGE_VERT ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER));

	// Load the shader source
	CHECK_GLRESULT(glShaderSource(shader, 1, (const GLchar*const*)&code, (GLint*)&codeSize));
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
		return;
	}

	pOut->mShader = shader;

	// For now pass on the shader code as "bytecode" 
	pOut->pByteCode = tf_calloc(1, codeSize);
	memcpy(pOut->pByteCode, (void*)code, codeSize);
	pOut->mByteCodeSize = codeSize;
}

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);
	ASSERT(pDesc->mStages & SHADER_STAGE_VERT && "OpenGL ES 2.0 requires a vertex shader");
	ASSERT(pDesc->mStages & SHADER_STAGE_FRAG && "OpenGL ES 2.0 requires a fragment shader");

	Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	// Create GL shader program
	CHECK_GL_RETURN_RESULT(pShaderProgram->mProgram, glCreateProgram());

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
		{
			pStage = &pDesc->mVert;
			pShaderProgram->mVertexShader = pStage->mShader;
		}
		else
		{
			pStage = &pDesc->mFrag;
			pShaderProgram->mFragmentShader = pStage->mShader;
		}

		CHECK_GLRESULT(glAttachShader(pShaderProgram->mProgram, pStage->mShader));
	}

	// Validate GL shader program
	if (!util_link_and_validate_program(pShaderProgram->mProgram))
	{
		glDeleteProgram(pShaderProgram->mProgram);
		ASSERT(false);
		return;
	}
	
	// OpenGL ES 2.0 reflection is done over a shader program instead for each individual shader stage
	// Therefore only one reflection exist.
	gles_createShaderReflection(pShaderProgram, pShaderProgram->pReflection->mStageReflections, pDesc);

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, 1, pShaderProgram->pReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	UNREF_PARAM(pRenderer);

	//remove given shader
	destroyPipelineReflection(pShaderProgram->pReflection);

	SAFE_FREE(pShaderProgram);
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);
	ASSERT(ppBuffer);

	// initialize to zero
	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
	ASSERT(pBuffer);

	// Only options for GLES 2.0 
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER) || (pDesc->mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER))
	{
		pBuffer->mTarget = (pDesc->mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER) ? GL_ELEMENT_ARRAY_BUFFER :  GL_ARRAY_BUFFER;
		CHECK_GLRESULT(glGenBuffers(1, &pBuffer->mBuffer));
		CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, pBuffer->mBuffer));
		GLint usage = util_to_gl_usage(pDesc->mMemoryUsage);
		if (usage != GL_NONE)
		{
			CHECK_GLRESULT(glBufferData(pBuffer->mTarget, pDesc->mSize, NULL, usage));
		}
		CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, GL_NONE));
		pBuffer->mMemoryUsage = pDesc->mMemoryUsage;

		if (pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
		{
			pBuffer->pGLCpuMappedAddress = tf_malloc(pDesc->mSize);
		}
	}
	else 
	{
		uint64_t allocationSize = pDesc->mSize;
		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			allocationSize = round_up_64(allocationSize, pRenderer->pActiveGpuSettings->mUniformBufferAlignment);
		}
		pBuffer->mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		pBuffer->pGLCpuMappedAddress = tf_malloc(allocationSize);
		pBuffer->mTarget = GL_NONE;
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

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pRenderer);
	ASSERT(pBuffer);

	if((pBuffer->mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER) || (pBuffer->mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER))
	{
		CHECK_GLRESULT(glDeleteBuffers(1, &pBuffer->mBuffer));
	}

	if (pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		SAFE_FREE(pBuffer->pGLCpuMappedAddress);
	}

	SAFE_FREE(pBuffer);
}

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ResourceMemoryUsage mem = (ResourceMemoryUsage)pBuffer->mMemoryUsage;
	ASSERT(mem != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

	pBuffer->pCpuMappedAddress = pBuffer->pGLCpuMappedAddress;
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	if (pBuffer->mTarget != GL_NONE)
	{
		CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, pBuffer->mBuffer));
		CHECK_GLRESULT(glBufferSubData(pBuffer->mTarget, 0, pBuffer->mSize, pBuffer->pGLCpuMappedAddress));
		CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, GL_NONE));
	}

	pBuffer->pCpuMappedAddress = nullptr;
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
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
			
	// Check image support
	if (!pRenderer->pCapBits->canShaderReadFrom[pDesc->mFormat])
	{
		LOGF(LogLevel::eERROR, "Compressed format \"%s\" is not supported!", TinyImageFormat_Name(pDesc->mFormat));
		return;
	}

	// initialize to zero
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(Texture));
	ASSERT(pTexture);

	GLuint typeSize;
	TinyImageFormat_ToGL_FORMAT(pDesc->mFormat, &pTexture->mGlFormat, &pTexture->mType, &pTexture->mInternalFormat, &typeSize);
	
	CHECK_GLRESULT(glGenTextures(1, &pTexture->mTexture));
	if(DESCRIPTOR_TYPE_TEXTURE_CUBE == (pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
		pTexture->mTarget = GL_TEXTURE_CUBE_MAP;
	else
		pTexture->mTarget = GL_TEXTURE_2D;

	if (pDesc->pName)
	{
		setTextureName(pRenderer, pTexture, pDesc->pName);
	}

	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;
	pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;
	pTexture->mFormat = pDesc->mFormat;
	pTexture->mUav = false;
	pTexture->mOwnsImage = false;

	*ppTexture = pTexture;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);

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

bool util_compare_shader_variable(const ShaderVariable& a, const ShaderVariable& b) {
	return a.parent_index <= b.parent_index && a.offset < b.offset;
}

void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRootSignatureDesc);
	ASSERT(ppRootSignature);
	ASSERT(pRootSignatureDesc->mShaderCount < 32);
	eastl::vector<ShaderResource>                            shaderResources;
	eastl::vector<uint32_t>                                  uboVariableSizes;
	eastl::vector<ShaderVariable>                            shaderVariables;

	DescriptorIndexMap                                       indexMap;

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
			decltype(indexMap.mMap)::iterator pNode =
				indexMap.mMap.find(pRes->name);
			if (pNode == indexMap.mMap.end())
			{
				indexMap.mMap.insert(pRes->name, (uint32_t)shaderResources.size());
				shaderResources.push_back(*pRes);

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
							shaderVariables.push_back(pReflection->pVariables[v]);
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

				uboVariableSizes.push_back(uboVariableSize);
			}
			else
			{
				// Search for new shader variables within the uniform block
				if (pRes->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				{
					uint32_t uboVariableOffset = 0;
					uint32_t currentIndex = 0;
					// Get current offset in shaderVariables list
					for (; currentIndex < shaderResources.size(); ++currentIndex)
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
								shaderVariables.insert(shaderVariables.begin() + uboVariableOffset + insertLocation, pReflection->pVariables[v]);
								++uboVariableSizes[currentIndex];
							}
						}
					}
				}
			}
		}
	}

	if (uniqueTextureCount >= pRenderer->pActiveGpuSettings->mMaxTextureImageUnits)
	{
		LOGF(LogLevel::eERROR, "Exceed maximum amount of texture units! required: {%d}, max: {%d}", uniqueTextureCount, pRenderer->pActiveGpuSettings->mMaxTextureImageUnits);
		ASSERT(false);
	}

	uint32_t totalSize = sizeof(RootSignature);
	totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount;
	totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount * shaderResources.size();
	totalSize += sizeof(uint32_t) * pRootSignatureDesc->mShaderCount * shaderVariables.size();
	totalSize += shaderResources.size() * sizeof(DescriptorInfo);
	totalSize += shaderVariables.size() * sizeof(GlVariable);
	totalSize += sizeof(DescriptorIndexMap);

	eastl::sort(shaderVariables.begin(), shaderVariables.end(), util_compare_shader_variable);

	RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
	ASSERT(pRootSignature);

	pRootSignature->mProgramCount = pRootSignatureDesc->mShaderCount;
	pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
	pRootSignature->mVariableCount = (uint32_t)shaderVariables.size();

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);
	pRootSignature->pProgramTargets = (uint32_t*)(pRootSignature->pDescriptors + pRootSignature->mDescriptorCount);
	pRootSignature->pDescriptorGlLocations = (int32_t*)(pRootSignature->pProgramTargets + pRootSignature->mProgramCount);
	pRootSignature->pVariables = (GlVariable*)(pRootSignature->pDescriptorGlLocations + pRootSignature->mProgramCount * pRootSignature->mDescriptorCount);
	uint8_t* mem = (uint8_t*)(pRootSignature->pVariables + pRootSignature->mVariableCount);
	for (uint32_t i = 0; i < pRootSignature->mVariableCount; ++i)
	{
		pRootSignature->pVariables[i].pGlLocations = (int32_t*)mem;
		mem += pRootSignature->mProgramCount * sizeof(int32_t);
	}
	pRootSignature->pDescriptorNameToIndexMap = (DescriptorIndexMap*)(mem);

	ASSERT(pRootSignature->pDescriptorNameToIndexMap);

	tf_placement_new<DescriptorIndexMap>(pRootSignature->pDescriptorNameToIndexMap);

	pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;
	pRootSignature->pDescriptorNameToIndexMap->mMap = indexMap.mMap;

	// Set shader program targets
	for (uint32_t sh = 0; sh < pRootSignature->mProgramCount; ++sh)
	{
		pRootSignature->pProgramTargets[sh] = pRootSignatureDesc->ppShaders[sh]->mProgram;
	}

	// Set shader variables
	for (uint32_t i = 0; i < pRootSignature->mVariableCount; ++i)
	{
		GlVariable* pVar = &pRootSignature->pVariables[i];
		ShaderVariable* pRes = &shaderVariables[i];
		pVar->mSize = pRes->size;
		pVar->mType = pRes->type;

		LOGF(LogLevel::eDEBUG, "%u: Shader Variable \"%s\" offset: %u", i, pRes->name, pRes->offset);

		// Get locations per shader program, because this can differ if one of the variables in an uniform block is not used.
		for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
		{
			CHECK_GL_RETURN_RESULT(pVar->pGlLocations[sh], glGetUniformLocation(pRootSignatureDesc->ppShaders[sh]->mProgram, pRes->name));
		}
	}

	uint32_t variableIndex = 0;
	
	// Only one sampler can be applied in a rootSignature for GLES
	if (pRootSignatureDesc->mStaticSamplerCount > 1)
		LOGF(LogLevel::eWARNING, "Only one static sampler can be applied within a rootSignature when using OpenGL ES 2.0. Requested samplers {%u}", pRootSignatureDesc->mStaticSamplerCount);
	pRootSignature->pSampler = pRootSignatureDesc->mStaticSamplerCount > 0 ? pRootSignatureDesc->ppStaticSamplers[0] : nullptr;

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource* pRes = &shaderResources[i];

		pDesc->mSize = pRes->size;
		pDesc->mType = pRes->type;
		pDesc->mGlType = pRes->set; // Not available for GLSL 100 we used it for glType
		pDesc->pName = pRes->name;
		pDesc->mHandleIndex = i;
		pDesc->mRootDescriptor = false;
		for (uint32_t sh = 0; sh < pRootSignature->mProgramCount; ++sh)
		{
			uint32_t index = i + sh * pRootSignature->mDescriptorCount;
			pRootSignature->pDescriptorGlLocations[index] = -1;
			ShaderResource* resource = util_lookup_shader_resource(pRes->name, pRootSignatureDesc->ppShaders[sh]->pReflection);
			if (resource)
				pRootSignature->pDescriptorGlLocations[index] = resource->reg;
		}
		//pDesc->mIndexInParent;
		//pDesc->mUpdateFrequency = updateFreq; //TODO see if we can use this within GLES

		if(pDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			pDesc->mIndexInParent = variableIndex; // Set start location of variables in this UBO
			uint32_t uboSize = 0;
			for (uint32_t var = variableIndex; var < variableIndex + uboVariableSizes[i]; ++var)
			{
				pRootSignature->pVariables[var].mIndexInParent = i;
				uboSize += pRootSignature->pVariables[var].mSize;
			}
			pDesc->mRootDescriptor = true;
			pDesc->mUBOSize = uboSize;

			variableIndex += uboVariableSizes[i];
		}
	}

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	pRootSignature->pDescriptorNameToIndexMap->mMap.clear(true);
	SAFE_FREE(pRootSignature);
}

typedef struct GlVertexAttrib
{
	GLuint	mIndex;
	GLint	mSize;
	GLenum  mType;
	GLboolean mNormalized;
	GLsizei	mStride;
	GLuint	mOffset;
} GlVertexAttrib;

void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;
	
	uint32_t attrib_count = 0;
	const ShaderReflection* pReflection = &pShaderProgram->pReflection->mStageReflections[0];

	// Make sure there's attributes
	if (pVertexLayout != NULL)
	{
		ASSERT(pVertexLayout->mAttribCount < pRenderer->pActiveGpuSettings->mMaxVertexInputBindings);
		attrib_count = min(pVertexLayout->mAttribCount, pRenderer->pActiveGpuSettings->mMaxVertexInputBindings);
	}

	size_t totalSize = sizeof(Pipeline);
	totalSize += sizeof(GlVertexAttrib) * attrib_count;
	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), totalSize);
	ASSERT(pPipeline);

	pPipeline->mVertexLayoutSize = attrib_count;
	pPipeline->pVertexLayout = (GlVertexAttrib*)(pPipeline + 1);

	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;

	pPipeline->mShaderProgram = pDesc->pShaderProgram->mProgram;

	attrib_count = 0;
	if (pVertexLayout != NULL)
	{
		for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->mAttribCount; ++attrib_index)
		{
			// Add vertex layouts
			const VertexAttrib* attrib = &(pVertexLayout->mAttribs[attrib_index]);
			ASSERT(SEMANTIC_UNDEFINED != attrib->mSemantic);
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

			// Set the desired vertex input location in the shader program for given semantic name
			// NOTE: Semantic names much match the attribute name in the .vert shader!
			CHECK_GLRESULT(glBindAttribLocation(pPipeline->mShaderProgram, attrib->mLocation, semanticName));

			GlVertexAttrib* vertexAttrib = &pPipeline->pVertexLayout[attrib_index];
			uint32_t glFormat, glInternalFormat, typeSize;

			vertexAttrib->mIndex = attrib->mLocation;
			vertexAttrib->mSize = TinyImageFormat_ChannelCount(attrib->mFormat);
			TinyImageFormat_ToGL_FORMAT(attrib->mFormat, &glFormat, &vertexAttrib->mType, &glInternalFormat, &typeSize);
			vertexAttrib->mStride = TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8;
			vertexAttrib->mNormalized = TinyImageFormat_IsNormalised(attrib->mFormat);
			vertexAttrib->mOffset = attrib->mOffset;

#if defined(_DEBUG)
			bool vertexAttribExist = false;
			for (uint32_t rfAttrib = 0; rfAttrib < pReflection->mVertexInputsCount; ++rfAttrib)
			{
				VertexInput* vertexInput = &pReflection->pVertexInputs[rfAttrib];

				if (strcmp(semanticName, vertexInput->name) == 0)
				{
					LOGF(LogLevel::eDEBUG, "Pipeline {%u}: Set vertex attribute \"%s\" location to {%u}", pPipeline->mShaderProgram, semanticName, attrib->mLocation);
					vertexAttribExist = true;
					break;
				}
			}
			if (!vertexAttribExist)
			{
				LOGF(LogLevel::eDEBUG, "Pipeline {%u}: Added vertex attribute \"%s\" to location {%u}", pPipeline->mShaderProgram, semanticName, attrib->mLocation);
			}
#endif
		}

		// Re-link the shader program to apply changed vertex input locations.
		util_link_and_validate_program(pPipeline->mShaderProgram);
	}

	// Set texture units of current used shader program
	CHECK_GLRESULT(glUseProgram(pPipeline->mShaderProgram));
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
	CHECK_GLRESULT(glUseProgram(GL_NONE));
	
	pPipeline->pRasterizerState = (GLRasterizerState*)tf_calloc(1, sizeof(GLRasterizerState));
	ASSERT(pPipeline->pRasterizerState);
	*pPipeline->pRasterizerState = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizer;

	pPipeline->pDepthStencilState = (GLDepthStencilState*)tf_calloc(1, sizeof(GLDepthStencilState));
	ASSERT(pPipeline->pDepthStencilState);
	*pPipeline->pDepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthStencilState;

	pPipeline->pBlendState = (GLBlendState*)tf_calloc(1, sizeof(GLBlendState));
	ASSERT(pPipeline->pBlendState);
	*pPipeline->pBlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendState;

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
	pPipeline->mGlPrimitiveTopology = topology;

	*ppPipeline = pPipeline;
}

void addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pDesc);

	if (pDesc->mType != PIPELINE_TYPE_GRAPHICS)
	{
		LOGF(LogLevel::eERROR, "Pipeline {%u} not supported on OpenGL ES 2.0", pDesc->mType);
		return;
	}

	addPipeline(pRenderer, &pDesc->mGraphicsDesc, ppPipeline);
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	SAFE_FREE(pPipeline->pBlendState);
	SAFE_FREE(pPipeline->pDepthStencilState);
	SAFE_FREE(pPipeline->pRasterizerState);
	SAFE_FREE(pPipeline);
}

void addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**)
{
}

void removePipelineCache(Renderer*, PipelineCache*)
{
}

void getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*)
{
}
/************************************************************************/
// Descriptor Set Implementation
/************************************************************************/
const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
	using DescriptorNameToIndexMap = eastl::string_hash_map<uint32_t>;
	DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap->mMap.find(pResName);
	if (it != pRootSignature->pDescriptorNameToIndexMap->mMap.end())
	{
		return &pRootSignature->pDescriptors[it->second];
	}
	else
	{
		LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}

typedef struct TextureDescriptorHandle
{
	bool hasMips;
	GLuint mTexture;
} TextureDescriptorHandle;

typedef struct DescriptorHandle
{
	union 
	{
		void** ppBuffers;
		TextureDescriptorHandle* pTextures;
	};
} DescriptorHandle;

typedef struct DescriptorDataArray
{
	struct DescriptorHandle*	pData;
} DescriptorDataArray;


void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
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
			totalSize += pDesc->mMaxSets * sizeof(void*);
			break;
		case DESCRIPTOR_TYPE_TEXTURE:
		case DESCRIPTOR_TYPE_RW_TEXTURE:
		case DESCRIPTOR_TYPE_TEXTURE_CUBE:
			totalSize += pDesc->mMaxSets * pRootSignature->pDescriptors[i].mSize * sizeof(TextureDescriptorHandle);
			break;
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			totalSize += pDesc->mMaxSets * pRootSignature->pDescriptors[i].mSize * sizeof(void*);
			break;
		}
	}
	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
	ASSERT(pDescriptorSet);
	pDescriptorSet->mMaxSets = pDesc->mMaxSets;
	pDescriptorSet->pRootSignature = pRootSignature;
	pDescriptorSet->mUpdateFrequency = pDesc->mUpdateFrequency;
	pDescriptorSet->pHandles = (DescriptorDataArray*)(pDescriptorSet + 1);

	uint8_t* mem = (uint8_t*)(pDescriptorSet->pHandles + pDesc->mMaxSets);
	for (uint32_t set = 0; set < pDesc->mMaxSets; ++set)
	{
		pDescriptorSet->pHandles[set].pData = (DescriptorHandle*)mem;
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
				pDescriptorSet->pHandles[set].pData[i].ppBuffers = (void**)mem;
				mem += sizeof(void*);
				break;
			case DESCRIPTOR_TYPE_TEXTURE:
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			case DESCRIPTOR_TYPE_TEXTURE_CUBE:
				pDescriptorSet->pHandles[set].pData[i].pTextures = (TextureDescriptorHandle*)mem;
				mem += pRootSignature->pDescriptors[i].mSize * sizeof(TextureDescriptorHandle);
				break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				pDescriptorSet->pHandles[set].pData[i].ppBuffers = (void**)mem;
				mem += pRootSignature->pDescriptors[i].mSize * sizeof(void*);
				break;
			}
		}
	}

	*ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	SAFE_FREE(pDescriptorSet);
}

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor,...)																\
	if (!(descriptor))																					\
	{																									\
		eastl::string msg = __FUNCTION__ + eastl::string(" : ") + eastl::string().sprintf(__VA_ARGS__);	\
		LOGF(LogLevel::eERROR, msg.c_str());															\
		_FailedAssert(__FILE__, __LINE__, msg.c_str());													\
		continue;																						\
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor,...)
#endif

	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mMaxSets);

	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t paramIndex = pParam->mIndex;
		VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != -1), "DescriptorData has NULL name and invalid index");

		const DescriptorInfo* pDesc = (paramIndex != -1) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
		paramIndex = pDesc->mHandleIndex;

		if (paramIndex != -1)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		const DescriptorType type = (DescriptorType)pDesc->mType;
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
					pDescriptorSet->pHandles[index].pData[paramIndex].pTextures[arr] = { pParam->ppTextures[arr]->mMipLevels > 1, pParam->ppTextures[arr]->mTexture };
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
				pDescriptorSet->pHandles[index].pData[paramIndex].ppBuffers = &pParam->ppBuffers[0]->pGLCpuMappedAddress;
				break;
			}
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);
				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
					 pDescriptorSet->pHandles[index].pData[paramIndex].ppBuffers[arr] = pParam->ppBuffers[arr]->pGLCpuMappedAddress;
				}
				break;
			}
		}
	}

}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	UNREF_PARAM(pRenderer);
	UNREF_PARAM(pCmdPool);

	pCmdPool->pCmdCache->mIndexBufferOffset = 0;
	pCmdPool->pCmdCache->mVertexBufferOffset = 0;
	pCmdPool->pCmdCache->mVertexBufferStride = 0;
	pCmdPool->pCmdCache->pPipeline = nullptr;
	pCmdPool->pCmdCache->isStarted = false;
}

void beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(!cmdCache->isStarted);

	cmdCache->isStarted = true;
}

void endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);

	cmdCache->isStarted = false;
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	if (!renderTargetCount && !pDepthStencil)
		return;

	//CHECK_GLRESULT(glBindFramebuffer(GL_FRAMEBUFFER, pCmd->mFramebuffer));
	uint32_t clearMask = 0;

	for (uint32_t rtIndex = 0; rtIndex < renderTargetCount; ++rtIndex)
	{
		CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, ppRenderTargets[rtIndex]->mRenderBuffer))
		if (ppRenderTargets[rtIndex]->mRenderBuffer)
			CHECK_GLRESULT(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + rtIndex, GL_RENDERBUFFER, ppRenderTargets[rtIndex]->mRenderBuffer));


		if (pLoadActions && pLoadActions->mLoadActionsColor[rtIndex] == LOAD_ACTION_CLEAR)
		{
			CHECK_GLRESULT(glClearColor(pLoadActions->mClearColorValues[rtIndex].r, pLoadActions->mClearColorValues[rtIndex].g, pLoadActions->mClearColorValues[rtIndex].b, pLoadActions->mClearColorValues[rtIndex].a));
			clearMask |= GL_COLOR_BUFFER_BIT;
		}
		glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
	}

	if (pDepthStencil && pDepthStencil->mType & GL_DEPTH_BUFFER_BIT)
	{
		CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, pDepthStencil->mRenderBuffer))

		if (pDepthStencil->mRenderBuffer)
			CHECK_GLRESULT(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pDepthStencil->mRenderBuffer));

		if (pLoadActions && pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR)
		{
			CHECK_GLRESULT(glClearDepthf(pLoadActions->mClearDepth.depth));
			clearMask |= GL_DEPTH_BUFFER_BIT;
		}
		glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
	}

	if (pDepthStencil && pDepthStencil->mType & GL_STENCIL_BUFFER_BIT)
	{
		CHECK_GLRESULT(glBindRenderbuffer(GL_RENDERBUFFER, pDepthStencil->mRenderBuffer))

		if (pDepthStencil->mRenderBuffer)
			CHECK_GLRESULT(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pDepthStencil->mRenderBuffer));

		if (pLoadActions && pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
		{
			CHECK_GLRESULT(glClearStencil(pLoadActions->mClearDepth.stencil));
			clearMask |= GL_STENCIL_BUFFER_BIT;
		}
		glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
	}

	if (clearMask)
	{
		CHECK_GLRESULT(glClear(clearMask));
	}

	GLenum result;
	CHECK_GL_RETURN_RESULT(result, glCheckFramebufferStatus(GL_FRAMEBUFFER));
	if (result != GL_FRAMEBUFFER_COMPLETE && result != GL_NO_ERROR)
		LOGF(eERROR, "Incomplete framebuffer! %s", util_get_enum_string(result));
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	// Inverse y location. OpenGL screen origin starts in lower left corner
	uint32_t surfaceHeight;
	getGLSurfaceSize(nullptr, &surfaceHeight);
	uint32_t yOffset = surfaceHeight - y - height;
	CHECK_GLRESULT(glViewport(x, yOffset, width, height));
	CHECK_GLRESULT(glDepthRangef(minDepth, maxDepth));
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	// Inverse y location. OpenGL screen origin starts in lower left corner
	uint32_t surfaceHeight;
	getGLSurfaceSize(nullptr, &surfaceHeight);
	uint32_t yOffset = surfaceHeight - y - height;
	CHECK_GLRESULT(glScissor(x, yOffset, width, height));
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	pCmd->pCmdPool->pCmdCache->pPipeline = pPipeline;

	CHECK_GLRESULT(glUseProgram(pPipeline->mShaderProgram));

	if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
	{
		// Disable all vertex attributes on start
		for (uint32_t vertexLayoutIndex = 0; vertexLayoutIndex < MAX_VERTEX_ATTRIBS; ++vertexLayoutIndex)
		{
			CHECK_GLRESULT(glDisableVertexAttribArray(vertexLayoutIndex));
		}

		if (pPipeline->pRasterizerState->mCullMode != GL_NONE)
		{
			CHECK_GLRESULT(glEnable(GL_CULL_FACE));
			CHECK_GLRESULT(glCullFace(pPipeline->pRasterizerState->mCullMode));
			CHECK_GLRESULT(glFrontFace(pPipeline->pRasterizerState->mFrontFace));
		}
		else
		{
			CHECK_GLRESULT(glDisable(GL_CULL_FACE));
		}

		CHECK_GLRESULT(pPipeline->pRasterizerState->mScissorTest ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST));

		if (pPipeline->pDepthStencilState->mDepthTest)
		{
			CHECK_GLRESULT(glEnable(GL_DEPTH_TEST));
			CHECK_GLRESULT(glDepthMask(pPipeline->pDepthStencilState->mDepthWrite));
			CHECK_GLRESULT(glDepthFunc(pPipeline->pDepthStencilState->mDepthFunc));
		}
		else
		{
			CHECK_GLRESULT(glDisable(GL_DEPTH_TEST));
		}

		if (pPipeline->pDepthStencilState->mStencilTest)
		{
			CHECK_GLRESULT(glEnable(GL_STENCIL_TEST));
			CHECK_GLRESULT(glStencilMask(pPipeline->pDepthStencilState->mStencilWriteMask));

			CHECK_GLRESULT(glStencilFuncSeparate(GL_FRONT, pPipeline->pDepthStencilState->mStencilFrontFunc, 0, ~0));
			CHECK_GLRESULT(glStencilOpSeparate(GL_FRONT, pPipeline->pDepthStencilState->mStencilFrontFail, pPipeline->pDepthStencilState->mDepthFrontFail, pPipeline->pDepthStencilState->mStencilFrontPass));

			CHECK_GLRESULT(glStencilFuncSeparate(GL_BACK, pPipeline->pDepthStencilState->mStencilBackFunc, 0, ~0));
			CHECK_GLRESULT(glStencilOpSeparate(GL_BACK, pPipeline->pDepthStencilState->mStencilBackFail, pPipeline->pDepthStencilState->mDepthBackFail, pPipeline->pDepthStencilState->mStencilBackPass));
		}
		else
		{
			CHECK_GLRESULT(glDisable(GL_STENCIL_TEST));
		}

		if (pPipeline->pBlendState->mBlendEnable)
		{
			CHECK_GLRESULT(glEnable(GL_BLEND));
			CHECK_GLRESULT(glBlendFuncSeparate(pPipeline->pBlendState->mSrcRGBFunc, pPipeline->pBlendState->mDstRGBFunc, pPipeline->pBlendState->mSrcAlphaFunc, pPipeline->pBlendState->mDstAlphaFunc));
			CHECK_GLRESULT(glBlendEquationSeparate(pPipeline->pBlendState->mModeRGB, pPipeline->pBlendState->mModeAlpha));
		}
		else
		{
			CHECK_GLRESULT(glDisable(GL_BLEND));
		}
	}
}

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
	const Sampler* pSampler = pRootSignature->pSampler;
	for (uint32_t sh = 0; sh < pRootSignature->mProgramCount; ++sh)
	{
		GLuint textureIndex = 0;
		CHECK_GLRESULT(glUseProgram(pRootSignature->pProgramTargets[sh]));
		for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
		{
			DescriptorInfo* descInfo = &pRootSignature->pDescriptors[i];
			uint32_t locationIndex = i + sh * pRootSignature->mDescriptorCount;
			GLint location = pRootSignature->pDescriptorGlLocations[locationIndex];
			if (location < 0)
			{
				if(descInfo->mType == DESCRIPTOR_TYPE_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_RW_TEXTURE || descInfo->mType == DESCRIPTOR_TYPE_TEXTURE_CUBE)
					textureIndex += descInfo->mSize;
				continue;
			}

			switch (descInfo->mType)
			{
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				{
					void* data = pDescriptorSet->pHandles[index].pData[i].ppBuffers[0];
					if (data)
					{
						util_gl_set_uniform(location, (uint8_t*)data, descInfo->mGlType, descInfo->mSize);
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
						TextureDescriptorHandle* textureHandle = &pDescriptorSet->pHandles[index].pData[i].pTextures[arr];
						if (textureHandle->mTexture != GL_NONE)
						{
							CHECK_GLRESULT(glActiveTexture(GL_TEXTURE0 + textureIndex));
							CHECK_GLRESULT(glBindTexture(target, textureHandle->mTexture));
							if (pSampler)
							{
								CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, textureHandle->hasMips ? pSampler->mMipMapMode : pSampler->mMinFilter));
								CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, pSampler->mMagFilter));
								CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_WRAP_S, pSampler->mAddressS));
								CHECK_GLRESULT(glTexParameteri(target, GL_TEXTURE_WRAP_T, pSampler->mAddressT));
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
						uint32_t uboOffset = descInfo->mUBOSize * arr;
						uint8_t* data = (uint8_t*)pDescriptorSet->pHandles[index].pData[i].ppBuffers[arr];
						if (!data)
							continue;
						for (uint32_t varIndex = descInfo->mIndexInParent; varIndex < pRootSignature->mVariableCount; ++varIndex)
						{
							GlVariable* variable = &pRootSignature->pVariables[varIndex];
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

	CHECK_GLRESULT(glUseProgram(pCmd->pCmdPool->pCmdCache->pPipeline->mShaderProgram));
}

void util_gl_set_constant(const Cmd* pCmd, const RootSignature* pRootSignature, const DescriptorInfo* pDesc, const void* pData)
{
	for (uint32_t program = 0; program < pRootSignature->mProgramCount; ++program)
	{
		CHECK_GLRESULT(glUseProgram(pRootSignature->pProgramTargets[program]));

		uint32_t locationIndex = pDesc->mHandleIndex + program * pRootSignature->mDescriptorCount;
		GLint location = pRootSignature->pDescriptorGlLocations[locationIndex];
		if (location < 0)
			continue;

		uint8_t* cpuBuffer = (uint8_t*)pData;
		switch (pDesc->mType)
		{
		case DESCRIPTOR_TYPE_BUFFER:
		case DESCRIPTOR_TYPE_RW_BUFFER:
		case DESCRIPTOR_TYPE_BUFFER_RAW:
		case DESCRIPTOR_TYPE_RW_BUFFER_RAW:		
			util_gl_set_uniform(location, cpuBuffer, pDesc->mGlType, pDesc->mSize);
			break;
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			for (uint32_t varIndex = pDesc->mIndexInParent; varIndex < pRootSignature->mVariableCount; ++varIndex)
			{
				GlVariable* variable = &pRootSignature->pVariables[varIndex];
				if (variable->mIndexInParent != pDesc->mHandleIndex)
					break;
				util_gl_set_uniform(variable->pGlLocations[program], cpuBuffer, variable->mType, variable->mSize);

				uint32_t typeSize = gl_type_byte_size(variable->mType);
				cpuBuffer += typeSize * variable->mSize;
			}
			break;
		}
	}
	CHECK_GLRESULT(glUseProgram(pCmd->pCmdPool->pCmdCache->pPipeline->mShaderProgram));
}

void cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pName);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pName);
	ASSERT(pDesc);
	util_gl_set_constant(pCmd, pRootSignature, pDesc, pConstants);
}

void cmdBindPushConstantsByIndex(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
	ASSERT(pDesc);

	util_gl_set_constant(pCmd, pRootSignature, pDesc, pConstants);
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	// GLES 2.0 Only supports GL_UNSIGNED_SHORT
	ASSERT(indexType == INDEX_TYPE_UINT16); 

	ASSERT(pBuffer->mTarget == GL_ELEMENT_ARRAY_BUFFER);

	pCmd->pCmdPool->pCmdCache->mIndexBufferOffset = offset;
	CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, pBuffer->mBuffer));
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(bufferCount);
	ASSERT(ppBuffers);
	ASSERT(pStrides);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);

	// TODO look for a solution for non interleaved buffers
	// Only interleaved vertex buffers expected for GLES
	ASSERT(bufferCount == 1);
	if (bufferCount > 1)
	{
		LOGF(LogLevel::eERROR, "Non interleaved vertex attributes not implemented!");
	}

	for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
	{
		ASSERT(ppBuffers[bufferIndex]->mTarget == GL_ARRAY_BUFFER);
		CHECK_GLRESULT(glBindBuffer(ppBuffers[bufferIndex]->mTarget, ppBuffers[bufferIndex]->mBuffer));
		pCmd->pCmdPool->pCmdCache->mVertexBufferOffset = pOffsets ? pOffsets[bufferIndex] : 0;
		pCmd->pCmdPool->pCmdCache->mVertexBufferStride = pStrides[bufferIndex];
	}

	// Set vertex layouts, should be done after the GL_ARRAY_BUFFER is bound
	for (uint32_t vertexLayoutIndex = 0; vertexLayoutIndex < cmdCache->pPipeline->mVertexLayoutSize; ++vertexLayoutIndex)
	{
		GlVertexAttrib* vertexAttrib = &cmdCache->pPipeline->pVertexLayout[vertexLayoutIndex];
		uint32_t offset = vertexAttrib->mOffset + cmdCache->mVertexBufferOffset;
		CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
			cmdCache->mVertexBufferStride, (void*)(offset)));
		CHECK_GLRESULT(glEnableVertexAttribArray(vertexAttrib->mIndex));
	}
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);

	GLint vertexIDLocation;
	CHECK_GL_RETURN_RESULT(vertexIDLocation, glGetUniformLocation(cmdCache->pPipeline->mShaderProgram, "vertexID"));
	if (vertexIDLocation != -1)
	{
		for (uint32_t vertexID = firstVertex; vertexID < vertexCount + firstVertex; ++vertexID)
		{
			util_gl_set_uniform(vertexIDLocation, (uint8_t*)&vertexID , GL_INT, 1);
			CHECK_GLRESULT(glDrawArrays(cmdCache->pPipeline->mGlPrimitiveTopology, vertexID, 1));
		}
	}
	else
	{
		CHECK_GLRESULT(glDrawArrays(cmdCache->pPipeline->mGlPrimitiveTopology, firstVertex, vertexCount));
	}
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);

	// GLES 2.0 with glsl #version 100 does not support instancing
	// Simulate instancing
	GLint instanceLocation;
	CHECK_GL_RETURN_RESULT(instanceLocation, glGetUniformLocation(cmdCache->pPipeline->mShaderProgram, "instanceID"));
	for (uint32_t instanceIndex = firstInstance; instanceIndex < firstInstance + instanceCount; ++instanceIndex)
	{
		util_gl_set_uniform(instanceLocation, (uint8_t*)&instanceIndex, GL_INT, 1);
		CHECK_GLRESULT(glDrawArrays(cmdCache->pPipeline->mGlPrimitiveTopology, firstVertex, vertexCount));
	}
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);
	
	// GLES has no support for vertex offset during glDrawElements
	// Therefore, we reset the vertex attributes based on the last known buffer stride and offset
	if (firstVertex > 0)
	{
		uint32_t vertexBufferOffset = firstVertex * cmdCache->mVertexBufferStride + cmdCache->mVertexBufferOffset;
		// Set vertex layouts, should be done after the GL_ARRAY_BUFFER is bound
		for (uint32_t i = 0; i < cmdCache->pPipeline->mVertexLayoutSize; ++i)
		{
			GlVertexAttrib* vertexAttrib = &cmdCache->pPipeline->pVertexLayout[i];
			uint32_t vertexOffset = vertexAttrib->mOffset + vertexBufferOffset;
			CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
				cmdCache->mVertexBufferStride, (void*)(vertexOffset)));
			CHECK_GLRESULT(glEnableVertexAttribArray(vertexAttrib->mIndex));
		}
	}

	uint32_t offset = firstIndex * sizeof(GLushort) + cmdCache->mIndexBufferOffset;
	CHECK_GLRESULT(glDrawElements(cmdCache->pPipeline->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)offset));
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);
	CmdCache* cmdCache = pCmd->pCmdPool->pCmdCache;
	ASSERT(cmdCache->isStarted);

	// GLES has no support for vertex offset during glDrawElements
	// Therefore, we reset the vertex attributes based on the last known buffer stride and offset
	if (firstVertex > 0)
	{
		uint32_t vertexBufferOffset = firstVertex * cmdCache->mVertexBufferStride + cmdCache->mVertexBufferOffset;
		// Set vertex layouts, should be done after the GL_ARRAY_BUFFER is bound
		for (uint32_t i = 0; i < cmdCache->pPipeline->mVertexLayoutSize; ++i)
		{
			GlVertexAttrib* vertexAttrib = &cmdCache->pPipeline->pVertexLayout[i];
			uint32_t vertexOffset = vertexAttrib->mOffset + vertexBufferOffset;
			CHECK_GLRESULT(glVertexAttribPointer(vertexAttrib->mIndex, vertexAttrib->mSize, vertexAttrib->mType, vertexAttrib->mNormalized,
				cmdCache->mVertexBufferStride, (void*)(vertexOffset)));
			CHECK_GLRESULT(glEnableVertexAttribArray(vertexAttrib->mIndex));
		}
	}

	uint32_t offset = firstIndex * sizeof(GLushort) + cmdCache->mIndexBufferOffset;

	// GLES 2.0 with glsl #version 100 does not support instancing
	// Simulate instancing
	GLint instanceLocation;
	CHECK_GL_RETURN_RESULT(instanceLocation, glGetUniformLocation(cmdCache->pPipeline->mShaderProgram, "instanceID"));
	for (uint32_t instanceIndex = firstInstance; instanceIndex < firstInstance + instanceCount; ++instanceIndex)
	{
		util_gl_set_uniform(instanceLocation, (uint8_t*)&instanceIndex, GL_INT, 1);
		CHECK_GLRESULT(glDrawElements(cmdCache->pPipeline->mGlPrimitiveTopology, indexCount, GL_UNSIGNED_SHORT, (void*)offset));
	}
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	// Unsuported for GLES 2.0
}

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pBuffer);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, pBuffer->mBuffer));
	CHECK_GLRESULT(glBufferSubData(pBuffer->mTarget, dstOffset, size, (uint8_t*)pSrcBuffer->pCpuMappedAddress + srcOffset));
	CHECK_GLRESULT(glBindBuffer(pBuffer->mTarget, GL_NONE));
}

typedef struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
} SubresourceDataDesc;

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
	ASSERT(pCmd);
	ASSERT(pSubresourceDesc);
	ASSERT(pTexture);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);

	uint32_t width = max(1u, (uint32_t)(pTexture->mWidth >> pSubresourceDesc->mMipLevel));
	uint32_t height = max(1u, (uint32_t)(pTexture->mHeight >> pSubresourceDesc->mMipLevel));
	ASSERT(pSrcBuffer->pCpuMappedAddress);

	GLenum target = pTexture->mTarget;
	if (pTexture->mTarget == GL_TEXTURE_CUBE_MAP)
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

	CHECK_GLRESULT(glBindTexture(pTexture->mTarget, pTexture->mTexture));
	if (pTexture->mType == GL_NONE) // Compressed image
	{
		GLsizei imageByteSize = util_get_compressed_texture_size(pTexture->mInternalFormat, width, height);
		CHECK_GLRESULT(glCompressedTexImage2D(target, pSubresourceDesc->mMipLevel, pTexture->mInternalFormat,
			width, height, 0, imageByteSize, (uint8_t*)pSrcBuffer->pCpuMappedAddress + pSubresourceDesc->mSrcOffset));
	}
	else
	{
		CHECK_GLRESULT(glTexImage2D(target, pSubresourceDesc->mMipLevel, pTexture->mGlFormat,
			width, height, 0, pTexture->mGlFormat, pTexture->mType, (uint8_t*)pSrcBuffer->pCpuMappedAddress + pSubresourceDesc->mSrcOffset));
	}
	CHECK_GLRESULT(glBindTexture(pTexture->mTarget, GL_NONE));
}

/************************************************************************/
// Transition Commands
/************************************************************************/
void cmdResourceBarrier(Cmd* pCmd,
	uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers,
	uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(pSignalSemaphore || pFence);

	pSwapChain->mImageIndex = 0;
	*pSwapChainImageIndex = pSwapChain->mImageIndex;
	if (pFence)
	{
		pFence->mSubmitted = true;
	}
	else
	{
		pSignalSemaphore->mSignaled = true;
	}
}

static void util_handle_wait_sempahores(Semaphore** ppWaitSemaphores, uint32_t waitSemaphoreCount)
{
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}

	bool shouldFlushGl = false;

	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		if (ppWaitSemaphores[i]->mSignaled)
		{
			shouldFlushGl = true;
			ppWaitSemaphores[i]->mSignaled = false;
		}
	}

	if (shouldFlushGl)
	{
		CHECK_GLRESULT(glFlush());
	}
}

void queueSubmit(
	Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	uint32_t cmdCount = pDesc->mCmdCount;
	Cmd** ppCmds = pDesc->ppCmds;
	Fence* pFence = pDesc->pSignalFence;
	uint32_t signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
	Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

	util_handle_wait_sempahores(pDesc->ppWaitSemaphores, pDesc->mWaitSemaphoreCount);

	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);

	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
	{
		if (!ppSignalSemaphores[i]->mSignaled)
		{
			ppSignalSemaphores[i]->mSignaled = true;
		}
	}

	if (pFence)
	{
		pFence->mSubmitted = true;
	}
}

PresentStatus queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	util_handle_wait_sempahores(pDesc->ppWaitSemaphores, pDesc->mWaitSemaphoreCount);

	// Disable scissor test, so we draw full screen and not latest set scissor location
	CHECK_GLRESULT(glDisable(GL_SCISSOR_TEST));

	return swapGLBuffers(pDesc->pSwapChain->pSurface) ? PRESENT_STATUS_SUCCESS : PRESENT_STATUS_FAILED;
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	UNREF_PARAM(pRenderer);
	if (pFence->mSubmitted)
	{
		*pFenceStatus = FENCE_STATUS_INCOMPLETE;
	}
	else
	{
		*pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
	}
}

void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	ASSERT(pRenderer);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	bool waitForGL = false;

	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		if (ppFences[i]->mSubmitted)
		{
			waitForGL = true;
			ppFences[i]->mSubmitted = false;
		}
	}
	
	if (waitForGL)
	{
		CHECK_GLRESULT(glFinish());
	}
}

void waitQueueIdle(Queue* pQueue)
{

}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
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
TinyImageFormat getRecommendedSwapchainFormat(bool hintHDR)
{
	return TinyImageFormat_B8G8R8A8_UNORM;
}

/************************************************************************/
// Indirect Draw functions
/************************************************************************/
void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature) {}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature) {}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
}

/************************************************************************/
// GPU Query Implementation
/************************************************************************/

void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	
}

void addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueryPool);
	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool) + pDesc->mQueryCount * sizeof(GLuint));
	ASSERT(pQueryPool);
	// Queries not available for OpenGL ES 2.0
}

void removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	UNREF_PARAM(pRenderer);
	SAFE_FREE(pQueryPool);
	// Queries not available for OpenGL ES 2.0
}

void cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
	UNREF_PARAM(pCmd);
	UNREF_PARAM(pQueryPool);
	UNREF_PARAM(startQuery);
	UNREF_PARAM(queryCount);
	// Queries not available for OpenGL ES 2.0
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	ASSERT(pCmd);
	ASSERT(pQuery);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);
	// Queries not available for OpenGL ES 2.0
}

void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	ASSERT(pCmd);
	ASSERT(pQuery);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);
	// Queries not available for OpenGL ES 2.0
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	ASSERT(pCmd);
	ASSERT(pReadbackBuffer);
	ASSERT(pCmd->pCmdPool->pCmdCache->isStarted);
	// Queries not available for OpenGL ES 2.0
}

/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats) {}

void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes) {}

void freeMemoryStats(Renderer* pRenderer, char* stats) {}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	ASSERT(pCmd);
	ASSERT(pName);
}

void cmdEndDebugMarker(Cmd* pCmd)
{
	ASSERT(pCmd);
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	ASSERT(pCmd);
	ASSERT(pName);
}

/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	UNREF_PARAM(pRenderer);
#endif
}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	UNREF_PARAM(pRenderer);
#endif
}

void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void setPipelineName(Renderer*, Pipeline*, const char*)
{
}
#endif
#endif