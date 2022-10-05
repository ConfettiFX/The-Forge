#pragma once

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void gl_utils_caps_builder(Renderer* pRenderer, const char* availableExtensions)
{

	const bool supportFloatTexture = strstr(availableExtensions, "GL_OES_texture_float") != nullptr;
	const bool supportHalfFloatTexture = strstr(availableExtensions, "GL_OES_texture_half_float") != nullptr;
	const bool supportFloatColorBuffer = strstr(availableExtensions, "GL_EXT_color_buffer_float") != nullptr;
	const bool supportHalfFloatColorBuffer = strstr(availableExtensions, "GL_EXT_color_buffer_half_float") != nullptr;
	const bool supportPackedDepthStencil = strstr(availableExtensions, "GL_OES_packed_depth_stencil") != nullptr;
	const bool supportDepth32 = strstr(availableExtensions, "GL_OES_depth32") != nullptr;

	pRenderer->pCapBits = (GPUCapBits*)tf_calloc(1, sizeof(GPUCapBits));

	GLint nCompressedTextureFormats = 0;
	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nCompressedTextureFormats);
	GLint* pCompressedTextureSupport = (GLint*)tf_calloc(1, nCompressedTextureFormats * sizeof(GLint));
	glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, pCompressedTextureSupport);

	GLuint format, typeSize, internalFormat, type;
	for (uint32_t i = 0; i < TinyImageFormat_Count; ++i) 
	{
		TinyImageFormat imgFormat = (TinyImageFormat)i;
		if (TinyImageFormat_IsCompressed(imgFormat))
		{
			TinyImageFormat_ToGL_FORMAT(imgFormat, &format, &type, &internalFormat, &typeSize);
			uint32_t j = 0;
			for (; j < nCompressedTextureFormats; ++j)
			{
				if (pCompressedTextureSupport[j] == internalFormat)
					break;
			}

			if (j == nCompressedTextureFormats)
				continue;
		}
		
		bool shaderResult = 1;
		bool renderTargetResult = 1;

		if (TinyImageFormat_IsDepthAndStencil(imgFormat) && !supportPackedDepthStencil)
		{
			shaderResult = 0;
			renderTargetResult = 0;
		}

		if (TinyImageFormat_IsFloat(imgFormat))
		{
			if (TinyImageFormat_MaxAtPhysical(imgFormat, 0) == 65504.000000)
			{
				shaderResult = supportHalfFloatTexture;
				renderTargetResult = supportHalfFloatColorBuffer;
			}
			else
			{
				shaderResult = supportFloatTexture;
				renderTargetResult = supportFloatColorBuffer;
			}

		}

		if (imgFormat == TinyImageFormat_D32_SFLOAT && !supportDepth32)
		{

			shaderResult = 0;
			renderTargetResult = 0;
		}

		pRenderer->pCapBits->canShaderReadFrom[i] = shaderResult;
		pRenderer->pCapBits->canShaderWriteTo[i] = shaderResult;

		pRenderer->pCapBits->canRenderTargetWriteTo[i] = renderTargetResult;
	}

	tf_free(pCompressedTextureSupport);
}
