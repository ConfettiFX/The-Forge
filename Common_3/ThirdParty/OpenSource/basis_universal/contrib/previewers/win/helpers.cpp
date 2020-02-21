#include "helpers.h"

#include <cassert>
#include <cstdio>

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

HBITMAP rgbToBitmap(const uint32_t* src, uint32_t const imgW, uint32_t const imgH, bool const flip) {
	/*
	 * Creates a bitmap (a DIB) for the passed-in pixel size. Note that
	 * negation of the height means top-down, origin upper-left, which is the
	 * regular case.
	 * 
	 * TODO: 16-bit variant instead?
	 */
	assert(src && imgW && imgH);
	BITMAPINFO bmi = {
		sizeof(bmi.bmiHeader)
	};
	bmi.bmiHeader.biWidth  =          imgW;
	bmi.bmiHeader.biHeight = (flip) ? imgH : -static_cast<int32_t>(imgH);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	void* pixels = NULL;
	HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
	/*
	 * RGBA to BGRA conversion.
	 * 
	 * Note: we keep the alpha.
	 */
	if (hbmp && pixels) {
		uint32_t* dst = static_cast<uint32_t*>(pixels);
		for (unsigned xy = imgW * imgH; xy > 0; xy--) {
			uint32_t rgba = *src++;
			*dst++ = ((rgba & 0x000000FF) << 16)
				   | ((rgba & 0xFF00FF00)      )
				   | ((rgba & 0x00FF0000) >> 16);
		}
		GdiFlush();
	}
	return hbmp;
}
