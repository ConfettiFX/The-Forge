#pragma once

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void utils_caps_builder(Renderer* pRenderer)
{
	pRenderer->pCapBits = (GPUCapBits*)tf_calloc(1, sizeof(GPUCapBits));

	GLint nCompressedTextureFormats = 0;
	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &nCompressedTextureFormats);
	GLint* pCompressedTextureSupport = (GLint*)tf_calloc(1, nCompressedTextureFormats * sizeof(GLint));
	glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, pCompressedTextureSupport);

	GLuint format, typeSize, internalFormat, type;
	for (uint32_t i = 0; i < TinyImageFormat_Count; ++i) 
	{
		if (TinyImageFormat_IsCompressed((TinyImageFormat)i))
		{
			TinyImageFormat_ToGL_FORMAT((TinyImageFormat)i, &format, &type, &internalFormat, &typeSize);
			uint32_t j = 0;
			for (; j < nCompressedTextureFormats; ++j)
			{
				if (pCompressedTextureSupport[j] == internalFormat)
					break;
			}

			if (j == nCompressedTextureFormats)
				continue;
		}

		pRenderer->pCapBits->canShaderReadFrom[i] = 1;
				
		pRenderer->pCapBits->canShaderWriteTo[i] = 1;

		pRenderer->pCapBits->canRenderTargetWriteTo[i] = 1;
	}

	tf_free(pCompressedTextureSupport);
}
