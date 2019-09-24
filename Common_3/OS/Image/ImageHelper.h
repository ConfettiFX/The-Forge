#ifndef COMMON_3_OS_IMAGE_IMAGE_HELPER_H_
#define COMMON_3_OS_IMAGE_IMAGE_HELPER_H_

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

static inline uint32_t Image_GetMipMappedSize(uint32_t w, uint32_t h, uint32_t d,
																				uint32_t nMipMapLevels, TinyImageFormat srcFormat)
{
	// PVR formats get special case
	if (srcFormat == TinyImageFormat_PVRTC1_2BPP_UNORM || srcFormat == TinyImageFormat_PVRTC1_2BPP_SRGB ||
			srcFormat == TinyImageFormat_PVRTC1_4BPP_UNORM || srcFormat == TinyImageFormat_PVRTC1_4BPP_SRGB)
	{
		uint32_t totalSize = 0;
		uint32_t sizeX = w;
		uint32_t sizeY = h;
		uint32_t sizeD = d;
		int level = nMipMapLevels;

		uint32_t minWidth = 8;
		uint32_t minHeight = 8;
		uint32_t minDepth = 1;
		int bpp = 4;

		if (srcFormat == TinyImageFormat_PVRTC1_2BPP_UNORM || srcFormat == TinyImageFormat_PVRTC1_2BPP_SRGB)
		{
			minWidth = 16;
			minHeight = 8;
			bpp = 2;
		}

		while (level > 0)
		{
			// If pixel format is compressed, the dimensions need to be padded.
			uint paddedWidth = sizeX + ((-1 * sizeX) % minWidth);
			uint paddedHeight = sizeY + ((-1 * sizeY) % minHeight);
			uint paddedDepth = sizeD + ((-1 * sizeD) % minDepth);

			uint32_t mipSize = paddedWidth * paddedHeight * paddedDepth * bpp / 8;

			totalSize += mipSize;

			unsigned int MinimumSize = 1;
			sizeX = max(sizeX / 2, MinimumSize);
			sizeY = max(sizeY / 2, MinimumSize);
			sizeD = max(sizeD / 2, MinimumSize);
			level--;
		}

		return totalSize;
	}

	uint32_t size = 0;
	while (nMipMapLevels)
	{
		uint32_t bx = TinyImageFormat_WidthOfBlock(srcFormat);
		uint32_t by = TinyImageFormat_HeightOfBlock(srcFormat);
		uint32_t bz = TinyImageFormat_DepthOfBlock(srcFormat);
		size += ((w + bx - 1) / bx) * ((h + by - 1) / by) * ((d + bz - 1) / bz);

		w >>= 1;
		h >>= 1;
		d >>= 1;
		if (w + h + d == 0)
			break;
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;
		if (d == 0)
			d = 1;

		nMipMapLevels--;
	}

	size *= TinyImageFormat_BitSizeOfBlock(srcFormat) / 8;

	return size;
}

#endif