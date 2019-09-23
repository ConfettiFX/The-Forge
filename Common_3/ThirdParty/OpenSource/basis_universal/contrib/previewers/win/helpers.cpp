#include "helpers.h"

#include <cassert>
#include <cstdio>

//************************* See cubic/texture-dxt.cpp ************************/

/**
 * Expands an RGB565 format colour into BGR888.
 * 
 * \note Unlike the original code, which was RGB888, for a Windows bitmap we
 * need this is as \e BGR.
 * 
 * \param[in] color RGB565 format colour
 * \return a \e bit \e expansion of the supplied colour as 8-bit per channel (with zero alpha)
 */
static uint32_t rgbFrom565(unsigned const color) {
	unsigned r = (color >> 11) & 0x1F;
	unsigned g = (color >>  5) & 0x3F;
	unsigned b = (color >>  0) & 0x1F;
	return (((r << 3) | (r >> 2)) << 16)
		 | (((g << 2) | (g >> 4)) <<  8)
		 | (((b << 3) | (b >> 2)) <<  0);
}

/**
 * Calculates the DXT decompressor's \e midpoint colour, where the weighting
 * is \c 2:1 (the parameters can be exchanged to calculate both midpoints).
 * Used by the DXT block decode.
 * 
 * \param[in] color0 \c endpoint colour receiving a double weighting
 * \param[in] color1 \c endpoint colour receiving a single weighting
 * \return blended colour (excluding alpha)
 */
static uint32_t midpoint21(uint32_t const color0, uint32_t const color1) {
	return (((2 * (color0 & 0x0000FF) + (color1 & 0x0000FF)) / 3) & 0x0000FF)
		 | (((2 * (color0 & 0x00FF00) + (color1 & 0x00FF00)) / 3) & 0x00FF00)
		 | (((2 * (color0 & 0xFF0000) + (color1 & 0xFF0000)) / 3) & 0xFF0000);
}

/**
 * Calculates the DXT decompressor's \e midpoint colour, where the weighting
 * is \c 1:1. Used by the DXT block decode.
 * 
 * \param[in] color0 \c endpoint colour (any of the endpoints)
 * \param[in] color1 \c endpoint colour (any of the endpoints)
 * \return blended colour (excluding alpha)
 */
static uint32_t midpoint11(uint32_t const color0, uint32_t const color1) {
	return ((((color0 & 0x0000FF) + (color1 & 0x0000FF)) / 2) & 0x0000FF)
		 | ((((color0 & 0x00FF00) + (color1 & 0x00FF00)) / 2) & 0x00FF00)
		 | ((((color0 & 0xFF0000) + (color1 & 0xFF0000)) / 2) & 0xFF0000);
}

/**
 * Decodes a DXT1 block.
 * 
 * \note Unlike the original code, which was RGB888, for a Windows bitmap we
 * need this is as \e BGR.
 * 
 * \param[in] src start of the encoded data (eight contiguous bytes)
 * \param[in] dst destination of the block's top-left pixel
 * \param[in] span number of pixels to advance to the next line (usually the texture width)
 */
static void decodeDxt1(const uint8_t* const src, uint32_t* dst, unsigned const span) {
	assert(src && dst && span);
	/*
	 * Extract the two 16-bit 'endpoints'. These are little Endian, regardless
	 * of the platform. Note that the midpoint choices are made against these
	 * (not the platform Endian RGB/BGR versions) and that (specifically in
	 * this implementation) DXT1 will always have solid alpha (which we bake
	 * into the colour table). The color 'codes' are collated here into a
	 * 32-bit value (which simplifies addressing the bits directly later).
	 */
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)
	unsigned const color0((src[0] <<  0) | (src[1] <<  8));
	unsigned const color1((src[2] <<  0) | (src[3] <<  8));
	unsigned const ccodes((src[4] <<  0) | (src[5] <<  8)
						| (src[6] << 16) | (src[7] << 24));
#else
	unsigned const color0(*reinterpret_cast<const uint16_t*>(src + 0));
	unsigned const color1(*reinterpret_cast<const uint16_t*>(src + 2));
	unsigned const ccodes(*reinterpret_cast<const uint32_t*>(src + 4));
#endif
	uint32_t color[4];
	color[0] = 0xFF000000 | rgbFrom565(color0);
	color[1] = 0xFF000000 | rgbFrom565(color1);
	color[2] = 0xFF000000 | ((color0 > color1)
						  ? midpoint21(color[0], color[1])
						  : midpoint11(color[0], color[1]));
	color[3] = 0xFF000000 | ((color0 > color1)
						  ? midpoint21(color[1], color[0])
						  : 0);
	/*
	 * The 4x4 block is unrolled. For destinations smaller than 4x4 the pixel
	 * overwrites here are inefficient, but the overhead isn't enough to worry
	 * about (when compared with the GL texture upload).
	 */
	dst[0] = color[(ccodes >>  0) & 0x03];
	dst[1] = color[(ccodes >>  2) & 0x03];
	dst[2] = color[(ccodes >>  4) & 0x03];
	dst[3] = color[(ccodes >>  6) & 0x03];
	dst   += span;
	dst[0] = color[(ccodes >>  8) & 0x03];
	dst[1] = color[(ccodes >> 10) & 0x03];
	dst[2] = color[(ccodes >> 12) & 0x03];
	dst[3] = color[(ccodes >> 14) & 0x03];
	dst   += span;
	dst[0] = color[(ccodes >> 16) & 0x03];
	dst[1] = color[(ccodes >> 18) & 0x03];
	dst[2] = color[(ccodes >> 20) & 0x03];
	dst[3] = color[(ccodes >> 22) & 0x03];
	dst   += span;
	dst[0] = color[(ccodes >> 24) & 0x03];
	dst[1] = color[(ccodes >> 26) & 0x03];
	dst[2] = color[(ccodes >> 28) & 0x03];
	dst[3] = color[(ccodes >> 30) & 0x03];
}

//******************************** Public API ********************************/

void dprintf(char* const fmt, ...) {
	va_list  args;
	char buf[256];
	va_start(args, fmt);
	int len = vsnprintf_s(buf, sizeof buf, fmt, args);
	va_end  (args);
	if (len > 0) {
		buf[sizeof buf - 1] = 0;
		OutputDebugStringA(buf);
	}
}

HBITMAP dxtToBitmap(const uint8_t* src, uint32_t const imgW, uint32_t const imgH, bool const flip) {
	assert(src && imgW && imgH);
	/*
	 * Creates a bitmap (a DIB) for the passed-in pixel size. Note that
	 * negation of the height means top-down, origin upper-left, which is the
	 * regular case.
	 * 
	 * TODO: 16-bit variant instead?
	 * TODO: we're growing the nearest 4x4 but not cropping afterwards (what about shrinking and skipping the last blocks?)
	 */
	uint32_t nearW = (imgW + 3) & ~3;
	uint32_t nearH = (imgH + 3) & ~3;
	BITMAPINFO bmi = {
		sizeof(bmi.bmiHeader)
	};
	bmi.bmiHeader.biWidth  =          nearW;
	bmi.bmiHeader.biHeight = (flip) ? nearH : -static_cast<int32_t>(nearH);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	void* pixels = NULL;
	HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
	/*
	 * Decode the BC1 blocks.
	 */
	if (hbmp && pixels) {
		uint32_t* dst = static_cast<uint32_t*>(pixels);
		for (unsigned y = 0; y < nearH; y += 4) {
			uint32_t* row = dst;
			for (unsigned x = 0; x < nearW; x += 4) {
				decodeDxt1(src, row, nearW);
				src += 8;
				row += 4;
			}
			dst += 4 * nearW;
		}
		GdiFlush();
	}
	return hbmp;
}
