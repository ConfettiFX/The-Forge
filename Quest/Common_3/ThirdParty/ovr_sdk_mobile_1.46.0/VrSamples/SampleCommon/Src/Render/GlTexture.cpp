/************************************************************************************

Filename    :   GlTexture.cpp
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

// Make sure we get PRIu64
#define __STDC_FORMAT_MACROS 1

#include "GlTexture.h"

#include "Egl.h"
#include "GL/gl_format.h"
#include "Misc/Log.h"
#include "CompilerUtils.h"
#include "PackageFiles.h"
#include "stb_image.h"

//#define OVR_USE_PERF_TIMER
#if defined(OVR_USE_PERF_TIMER)
#include "OVR_PerfTimer.h"
#else
#define OVR_PERF_TIMER(x) \
    { ; }
#endif

#include <algorithm>
#include <fstream>
#include <locale>
#include <cmath>

#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD

uint32_t DecodeNextChar_Advance0(const char** putf8Buffer) {
    uint32_t uc;
    char c;

    // Security considerations:
    //
    // Changed, this is now only the case for DecodeNextChar:
    //  - If we hit a zero byte, we want to return 0 without stepping
    //    the buffer pointer past the 0. th
    //
    // If we hit an "overlong sequence"; i.e. a character encoded
    // in a longer multibyte string than is necessary, then we
    // need to discard the character.  This is so attackers can't
    // disguise dangerous characters or character sequences --
    // there is only one valid encoding for each character.
    //
    // If we decode characters { 0xD800 .. 0xDFFF } or { 0xFFFE,
    // 0xFFFF } then we ignore them; they are not valid in UTF-8.

    // This isn't actually an invalid character; it's a valid char that
    // looks like an inverted question mark.
#define INVALID_CHAR 0x0FFFD

#define FIRST_BYTE(mask, shift) uc = (c & (mask)) << (shift);

#define NEXT_BYTE(shift)                              \
    c = **putf8Buffer;                                \
    if (c == 0)                                       \
        return 0; /* end of buffer, do not advance */ \
    if ((c & 0xC0) != 0x80)                           \
        return INVALID_CHAR; /* standard check */     \
    (*putf8Buffer)++;                                 \
    uc |= (c & 0x3F) << shift;

    c = **putf8Buffer;
    (*putf8Buffer)++;
    if (c == 0)
        return 0; // End of buffer.

    if ((c & 0x80) == 0)
        return (uint32_t)c; // Conventional 7-bit ASCII.

    // Multi-byte sequences.
    if ((c & 0xE0) == 0xC0) {
        // Two-byte sequence.
        FIRST_BYTE(0x1F, 6);
        NEXT_BYTE(0);
        if (uc < 0x80)
            return INVALID_CHAR; // overlong
        return uc;
    } else if ((c & 0xF0) == 0xE0) {
        // Three-byte sequence.
        FIRST_BYTE(0x0F, 12);
        NEXT_BYTE(6);
        NEXT_BYTE(0);
        if (uc < 0x800)
            return INVALID_CHAR; // overlong
        // Not valid ISO 10646, but Flash requires these to work
        // see AS3 test e15_5_3_2_3 for String.fromCharCode().charCodeAt(0)
        // if (uc >= 0x0D800 && uc <= 0x0DFFF) return INVALID_CHAR;
        // if (uc == 0x0FFFE || uc == 0x0FFFF) return INVALID_CHAR; // not valid ISO 10646
        return uc;
    } else if ((c & 0xF8) == 0xF0) {
        // Four-byte sequence.
        FIRST_BYTE(0x07, 18);
        NEXT_BYTE(12);
        NEXT_BYTE(6);
        NEXT_BYTE(0);
        if (uc < 0x010000)
            return INVALID_CHAR; // overlong
        return uc;
    } else if ((c & 0xFC) == 0xF8) {
        // Five-byte sequence.
        FIRST_BYTE(0x03, 24);
        NEXT_BYTE(18);
        NEXT_BYTE(12);
        NEXT_BYTE(6);
        NEXT_BYTE(0);
        if (uc < 0x0200000)
            return INVALID_CHAR; // overlong
        return uc;
    } else if ((c & 0xFE) == 0xFC) {
        // Six-byte sequence.
        FIRST_BYTE(0x01, 30);
        NEXT_BYTE(24);
        NEXT_BYTE(18);
        NEXT_BYTE(12);
        NEXT_BYTE(6);
        NEXT_BYTE(0);
        if (uc < 0x04000000)
            return INVALID_CHAR; // overlong
        return uc;
    } else {
        // Invalid.
        return INVALID_CHAR;
    }

#undef INVALID_CHAR
#undef FIRST_BYTE
#undef NEXT_BYTE
}

// Safer version of DecodeNextChar, which doesn't advance pointer if
// null character is hit.
inline uint32_t DecodeNextChar(const char** putf8Buffer) {
    uint32_t ch = DecodeNextChar_Advance0(putf8Buffer);
    if (ch == 0)
        (*putf8Buffer)--;
    return ch;
}

static void ScanFilePath(const char* url, const char** pfilename, const char** pext) {
    const char* filename = url;
    const char* lastDot = nullptr;

    uint32_t charVal = DecodeNextChar(&url);

    while (charVal != 0) {
        if ((charVal == '/') || (charVal == '\\')) {
            filename = url;
            lastDot = nullptr;
        } else if (charVal == '.') {
            lastDot = url - 1;
        }

        charVal = DecodeNextChar(&url);
    }

    if (pfilename) {
        *pfilename = filename;
    }

    if (pext) {
        *pext = lastDot;
    }
}

static std::string GetExtension(const std::string& s) {
    const char* ext = nullptr;
    ScanFilePath(s.c_str(), nullptr, &ext);
    return std::string(ext);
}

namespace OVRFW {

// Not declared inline in the header to avoid having to use GL_TEXTURE_2D
GlTexture::GlTexture(const unsigned texture_, const int w, const int h)
    : texture(texture_), target(GL_TEXTURE_2D), Width(w), Height(h) {}

static int RoundUpToPow2(int i) {
    if (i == 0) {
        return 0;
    }
    return static_cast<int>(pow(2, ceil(log(double(i)) / log(2))));
}

static int IntegerLog2(int i) {
    if (i == 0) {
        return 0;
    }
    return static_cast<int>(log(double(i)) / log(2.0));
}

int ComputeFullMipChainNumLevels(const int width, const int height) {
    return IntegerLog2(RoundUpToPow2(std::max(width, height)));
}

static bool IsCompressedFormat(const eTextureFormat format) {
    switch (format) {
        case Texture_None:
        case Texture_R:
        case Texture_RGB:
        case Texture_RGBA:
            return false;
        case Texture_DXT1:
        case Texture_DXT3:
        case Texture_DXT5:
        case Texture_PVR4bRGB:
        case Texture_PVR4bRGBA:
        case Texture_ATC_RGB:
        case Texture_ATC_RGBA:
        case Texture_ETC1:
        case Texture_ETC2_RGB:
        case Texture_ETC2_RGBA:
        case Texture_ASTC_4x4:
        case Texture_ASTC_5x4:
        case Texture_ASTC_5x5:
        case Texture_ASTC_6x5:
        case Texture_ASTC_6x6:
        case Texture_ASTC_8x5:
        case Texture_ASTC_8x6:
        case Texture_ASTC_8x8:
        case Texture_ASTC_10x5:
        case Texture_ASTC_10x6:
        case Texture_ASTC_10x8:
        case Texture_ASTC_10x10:
        case Texture_ASTC_12x10:
        case Texture_ASTC_12x12:
        case Texture_ASTC_SRGB_4x4:
        case Texture_ASTC_SRGB_5x4:
        case Texture_ASTC_SRGB_5x5:
        case Texture_ASTC_SRGB_6x5:
        case Texture_ASTC_SRGB_6x6:
        case Texture_ASTC_SRGB_8x5:
        case Texture_ASTC_SRGB_8x6:
        case Texture_ASTC_SRGB_8x8:
        case Texture_ASTC_SRGB_10x5:
        case Texture_ASTC_SRGB_10x6:
        case Texture_ASTC_SRGB_10x8:
        case Texture_ASTC_SRGB_10x10:
        case Texture_ASTC_SRGB_12x10:
        case Texture_ASTC_SRGB_12x12:
            return true;
        default:
            assert(false);
            return false;
    }
}

static int GetASTCIndex(const eTextureFormat format) {
    int const formatType = format & Texture_TypeMask;
    int const index = (formatType - Texture_ASTC_Start) >> 8;
    return index;
}

GLenum GetASTCInternalFormat(eTextureFormat const format) {
    int const NUM_ASTC_FORMATS = (Texture_ASTC_End - Texture_ASTC_Start) >> 8;

    int const index = GetASTCIndex(format);

    GLenum internalFormats[NUM_ASTC_FORMATS] = {
        GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
        GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
        GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
        GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
        GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
        GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
        GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR};
    return internalFormats[index];
}

static int
GetASTCTextureSize(const eTextureFormat format, const int w, const int h, const int depth) {
    struct blockDims_t {
        int x;
        int y;
        int z;
    };

    int const NUM_ASTC_FORMATS = (Texture_ASTC_End - Texture_ASTC_Start) >> 8;
    blockDims_t const blockDims[NUM_ASTC_FORMATS] = {
        {4, 4, 1}, {5, 4, 1},  {5, 5, 1},  {6, 5, 1},  {6, 6, 1},   {8, 5, 1},   {8, 6, 1},
        {8, 8, 1}, {10, 5, 1}, {10, 6, 1}, {10, 8, 1}, {10, 10, 1}, {12, 10, 1}, {12, 12, 1},
        {4, 4, 1}, {5, 4, 1},  {5, 5, 1},  {6, 5, 1},  {6, 6, 1},   {8, 5, 1},   {8, 6, 1},
        {8, 8, 1}, {10, 5, 1}, {10, 6, 1}, {10, 8, 1}, {10, 10, 1}, {12, 10, 1}, {12, 12, 1}};

    int const index = GetASTCIndex(format);

    blockDims_t const& dims = blockDims[index];

    // Compute number of blocks in each direction
    int const xblocks = (w + dims.x - 1) / dims.x;
    int const yblocks = (h + dims.y - 1) / dims.y;
    int const zblocks = (depth + dims.z - 1) / dims.z;

    // Each block is encoded on 16 bytes, so calculate total compressed image data size.
    int const numBytes = xblocks * yblocks * zblocks * 16;
    return numBytes;
}

static int32_t GetOvrTextureSize(const eTextureFormat format, const int w, const int h) {
    switch (format & Texture_TypeMask) {
        case Texture_R:
            return w * h;
        case Texture_RGB:
            return w * h * 3;
        case Texture_RGBA:
            return w * h * 4;
        case Texture_ATC_RGB:
        case Texture_ETC1:
        case Texture_ETC2_RGB:
        case Texture_DXT1: {
            int bw = (w + 3) / 4, bh = (h + 3) / 4;
            return bw * bh * 8;
        }
        case Texture_ATC_RGBA:
        case Texture_ETC2_RGBA:
        case Texture_DXT3:
        case Texture_DXT5: {
            int bw = (w + 3) / 4, bh = (h + 3) / 4;
            return bw * bh * 16;
        }
        case Texture_PVR4bRGB:
        case Texture_PVR4bRGBA: {
            unsigned int width = (unsigned int)w;
            unsigned int height = (unsigned int)h;
            unsigned int min_width = 8;
            unsigned int min_height = 8;

            // pad the dimensions
            width = width + ((-1 * width) % min_width);
            height = height + ((-1 * height) % min_height);
            unsigned int depth = 1;

            unsigned int bpp = 4;
            unsigned int bits = bpp * width * height * depth;
            return (int)(bits / 8);
        }
        case Texture_ASTC_4x4:
        case Texture_ASTC_5x4:
        case Texture_ASTC_5x5:
        case Texture_ASTC_6x5:
        case Texture_ASTC_6x6:
        case Texture_ASTC_8x5:
        case Texture_ASTC_8x6:
        case Texture_ASTC_8x8:
        case Texture_ASTC_10x5:
        case Texture_ASTC_10x6:
        case Texture_ASTC_10x8:
        case Texture_ASTC_10x10:
        case Texture_ASTC_12x10:
        case Texture_ASTC_12x12:
        case Texture_ASTC_SRGB_4x4:
        case Texture_ASTC_SRGB_5x4:
        case Texture_ASTC_SRGB_5x5:
        case Texture_ASTC_SRGB_6x5:
        case Texture_ASTC_SRGB_6x6:
        case Texture_ASTC_SRGB_8x5:
        case Texture_ASTC_SRGB_8x6:
        case Texture_ASTC_SRGB_8x8:
        case Texture_ASTC_SRGB_10x5:
        case Texture_ASTC_SRGB_10x6:
        case Texture_ASTC_SRGB_10x8:
        case Texture_ASTC_SRGB_10x10:
        case Texture_ASTC_SRGB_12x10:
        case Texture_ASTC_SRGB_12x12: {
            return GetASTCTextureSize(format, w, h, 1);
        }
        default: {
            assert(false);
            break;
        }
    }
    return 0;
}

bool TextureFormatToGlFormat(
    const eTextureFormat format,
    const bool useSrgbFormat,
    GLenum& glFormat,
    GLenum& glInternalFormat) {
    switch (format & Texture_TypeMask) {
        case Texture_RGB: {
            glFormat = GL_RGB;
            if (useSrgbFormat) {
                glInternalFormat = GL_SRGB8;
                //				LOG( "GL texture format is GL_RGB / GL_SRGB8" );
            } else {
                glInternalFormat = GL_RGB;
                //				LOG( "GL texture format is GL_RGB / GL_RGB" );
            }
            return true;
        }
        case Texture_RGBA: {
            glFormat = GL_RGBA;
            if (useSrgbFormat) {
                glInternalFormat = GL_SRGB8_ALPHA8;
                //				LOG( "GL texture format is GL_RGBA / GL_SRGB8_ALPHA8" );
            } else {
                glInternalFormat = GL_RGBA;
                //				LOG( "GL texture format is GL_RGBA / GL_RGBA" );
            }
            return true;
        }
        case Texture_R: {
            glInternalFormat = GL_R8;
            glFormat = GL_RED;
            //			LOG( "GL texture format is GL_R8" );
            return true;
        }
        case Texture_DXT1: {
            glFormat = glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            //			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
            return true;
        }
            // unsupported on OpenGL ES:
            //    case Texture_DXT3:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
            //    case Texture_DXT5:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
        case Texture_PVR4bRGB: {
            glFormat = GL_RGB;
            glInternalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
            //			LOG( "GL texture format is GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG" );
            return true;
        }
        case Texture_PVR4bRGBA: {
            glFormat = GL_RGBA;
            glInternalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
            //			LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG" );
            return true;
        }
        case Texture_ETC1: {
            glFormat = GL_RGB;
            if (useSrgbFormat) {
                // Note that ETC2 is backwards compatible with ETC1.
                glInternalFormat = GL_COMPRESSED_SRGB8_ETC2;
                //				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_SRGB8_ETC2 " );
            } else {
                glInternalFormat = GL_ETC1_RGB8_OES;
                //				LOG( "GL texture format is GL_RGB / GL_ETC1_RGB8_OES" );
            }
            return true;
        }
        case Texture_ETC2_RGB: {
            glFormat = GL_RGB;
            if (useSrgbFormat) {
                glInternalFormat = GL_COMPRESSED_SRGB8_ETC2;
                //				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_SRGB8_ETC2 " );
            } else {
                glInternalFormat = GL_COMPRESSED_RGB8_ETC2;
                //				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_RGB8_ETC2 " );
            }
            return true;
        }
        case Texture_ETC2_RGBA: {
            glFormat = GL_RGBA;
            if (useSrgbFormat) {
                glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
                //				LOG( "GL texture format is GL_RGBA /
                // GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
                //"
                //);
            } else {
                glInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
                //				LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_RGBA8_ETC2_EAC "
                //);
            }
            return true;
        }
        case Texture_ASTC_4x4:
        case Texture_ASTC_5x4:
        case Texture_ASTC_5x5:
        case Texture_ASTC_6x5:
        case Texture_ASTC_6x6:
        case Texture_ASTC_8x5:
        case Texture_ASTC_8x6:
        case Texture_ASTC_8x8:
        case Texture_ASTC_10x5:
        case Texture_ASTC_10x6:
        case Texture_ASTC_10x8:
        case Texture_ASTC_10x10:
        case Texture_ASTC_12x10:
        case Texture_ASTC_12x12:
        case Texture_ASTC_SRGB_4x4:
        case Texture_ASTC_SRGB_5x4:
        case Texture_ASTC_SRGB_5x5:
        case Texture_ASTC_SRGB_6x5:
        case Texture_ASTC_SRGB_6x6:
        case Texture_ASTC_SRGB_8x5:
        case Texture_ASTC_SRGB_8x6:
        case Texture_ASTC_SRGB_8x8:
        case Texture_ASTC_SRGB_10x5:
        case Texture_ASTC_SRGB_10x6:
        case Texture_ASTC_SRGB_10x8:
        case Texture_ASTC_SRGB_10x10:
        case Texture_ASTC_SRGB_12x10:
        case Texture_ASTC_SRGB_12x12: {
            glFormat = GL_RGBA;
            glInternalFormat = GetASTCInternalFormat(format);

            // Force the format to be correct for the given useSrgbFormat state
            if (useSrgbFormat && glInternalFormat >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR &&
                glInternalFormat <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR) {
                glInternalFormat +=
                    (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR - GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            }
            if (!useSrgbFormat && glInternalFormat >= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR &&
                glInternalFormat <= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR) {
                glInternalFormat -=
                    (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR - GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            }
            return true;
        }
        case Texture_ATC_RGB: {
            glFormat = GL_RGB;
            glInternalFormat = GL_ATC_RGB_AMD;
            //			LOG( "GL texture format is GL_RGB / GL_ATC_RGB_AMD" );
            return true;
        }
        case Texture_ATC_RGBA: {
            glFormat = GL_RGBA;
            glInternalFormat = GL_ATC_RGBA_EXPLICIT_ALPHA_AMD;
            //			LOG( "GL texture format is GL_RGBA / GL_ATC_RGBA_EXPLICIT_ALPHA_AMD" );
            return true;
        }
    }
    return false;
}

bool GlFormatToTextureFormat(
    eTextureFormat& format,
    const GLenum glFormat,
    const GLenum glInternalFormat) {
    if (glFormat == GL_RED && glInternalFormat == GL_R8) {
        format = Texture_R;
        return true;
    }
    if (glFormat == GL_RGB && (glInternalFormat == GL_RGB || glInternalFormat == GL_SRGB8)) {
        format = Texture_RGB;
        return true;
    }
    if (glFormat == GL_RGBA &&
        (glInternalFormat == GL_RGBA || glInternalFormat == GL_RGBA8 ||
         glInternalFormat == GL_SRGB8_ALPHA8)) {
        format = Texture_RGBA;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) &&
        glInternalFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
        format = Texture_DXT1;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGB) &&
        glInternalFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG) {
        format = Texture_PVR4bRGB;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGBA) &&
        glInternalFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG) {
        format = Texture_PVR4bRGBA;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGB) &&
        (glInternalFormat == GL_ETC1_RGB8_OES || glInternalFormat == GL_COMPRESSED_SRGB8_ETC2)) {
        format = Texture_ETC1;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGB) &&
        (glInternalFormat == GL_COMPRESSED_RGB8_ETC2 ||
         glInternalFormat == GL_COMPRESSED_SRGB8_ETC2)) {
        format = Texture_ETC2_RGB;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGBA) &&
        (glInternalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC ||
         glInternalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)) {
        format = Texture_ETC2_RGBA;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGB) && glInternalFormat == GL_ATC_RGB_AMD) {
        format = Texture_ATC_RGB;
        return true;
    }
    if ((glFormat == 0 || glFormat == GL_RGBA) &&
        glInternalFormat == GL_ATC_RGBA_EXPLICIT_ALPHA_AMD) {
        format = Texture_ATC_RGBA;
        return true;
    }
    if (glFormat == 0 || glFormat == GL_RGBA) {
        if (glInternalFormat >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR &&
            glInternalFormat <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR) {
            format = (eTextureFormat)(
                Texture_ASTC_4x4 + ((glInternalFormat - GL_COMPRESSED_RGBA_ASTC_4x4_KHR) << 8));
            return true;
        }
        if (glInternalFormat >= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR &&
            glInternalFormat <= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR) {
            format = (eTextureFormat)(
                Texture_ASTC_SRGB_4x4 +
                ((glInternalFormat - GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR) << 8));
            return true;
        }
    }

    return false;
}

static GlTexture CreateGlTexture(
    const char* fileName,
    const eTextureFormat format,
    const int width,
    const int height,
    const void* data,
    const size_t dataSize,
    const int mipcount,
    const bool useSrgbFormat,
    const bool imageSizeStored) {
#if defined(OVR_USE_PERF_TIMER)
    ALOG("Loading '%s', w = %i, h = %i, mipcount = %i", fileName, width, height, mipcount);
#endif

    GLCheckErrorsWithTitle("pre-CreateGlTexture");

    OVR_PERF_TIMER(CreateGlTexture);

    // LOG( "CreateGLTexture(): format %s", NameForTextureFormat( static_cast< TextureFormat >(
    // format ) ) );

    GLenum glFormat;
    GLenum glInternalFormat;
    if (!TextureFormatToGlFormat(format, useSrgbFormat, glFormat, glInternalFormat)) {
        return GlTexture(0, 0, 0);
    }

    if (mipcount <= 0) {
        ALOG("%s: Invalid mip count %d", fileName, mipcount);
        return GlTexture(0, 0, 0);
    }

    // larger than this would require mipSize below to be a larger type
    if (width <= 0 || width > 32768 || height <= 0 || height > 32768) {
        ALOG("%s: Invalid texture size (%dx%d)", fileName, width, height);
        return GlTexture(0, 0, 0);
    }

    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    const unsigned char* level = (const unsigned char*)data;
    const unsigned char* endOfBuffer = level + dataSize;

    int w = width;
    int h = height;
    for (int i = 0; i < mipcount; i++) {
        int32_t mipSize = GetOvrTextureSize(format, w, h);
        if (imageSizeStored) {
            mipSize = static_cast<int32_t>(*(const size_t*)level);

            level += 4;
            if (level > endOfBuffer) {
                ALOG("%s: Image data exceeds buffer size", fileName);
                glBindTexture(GL_TEXTURE_2D, 0);
                return GlTexture(texId, GL_TEXTURE_2D, width, height);
            }
        }

        if (mipSize <= 0 || mipSize > endOfBuffer - level) {
            ALOG(
                "%s: Mip level %d exceeds buffer size (%d > %td)",
                fileName,
                i,
                mipSize,
                ptrdiff_t(endOfBuffer - level));
            glBindTexture(GL_TEXTURE_2D, 0);
            return GlTexture(texId, GL_TEXTURE_2D, width, height);
        }

        if (IsCompressedFormat(format)) {
            OVR_PERF_TIMER(CreateGlTexture_CompressedTexImage2D);
            glCompressedTexImage2D(GL_TEXTURE_2D, i, glInternalFormat, w, h, 0, mipSize, level);
            GLCheckErrorsWithTitle("Texture_Compressed");
        } else {
            OVR_PERF_TIMER(CreateGlTexture_TexImage2D);
            glTexImage2D(
                GL_TEXTURE_2D, i, glInternalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, level);
        }

        level += mipSize;
        if (imageSizeStored) {
            level += 3 - ((mipSize + 3) % 4);
            if (level > endOfBuffer) {
                ALOG("%s: Image data exceeds buffer size", fileName);
                glBindTexture(GL_TEXTURE_2D, 0);
                return GlTexture(texId, GL_TEXTURE_2D, width, height);
            }
        }

        w >>= 1;
        h >>= 1;
        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Surfaces look pretty terrible without trilinear filtering
    if (mipcount <= 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLCheckErrorsWithTitle("Texture load");

    glBindTexture(GL_TEXTURE_2D, 0);

    return GlTexture(texId, GL_TEXTURE_2D, width, height);
}

static GlTexture CreateGlCubeTexture(
    const char* fileName,
    const eTextureFormat format,
    const int width,
    const int height,
    const void* data,
    const size_t dataSize,
    const int mipcount,
    const bool useSrgbFormat,
    const bool imageSizeStored) {
    assert(width == height);

    GLCheckErrorsWithTitle("Pre Cube Texture load");

    if (mipcount <= 0) {
        ALOG("%s: Invalid mip count %d", fileName, mipcount);
        return GlTexture(0, 0, 0);
    }

    // larger than this would require mipSize below to be a larger type
    if (width <= 0 || width > 32768 || height <= 0 || height > 32768) {
        ALOG("%s: Invalid texture size (%dx%d)", fileName, width, height);
        return GlTexture(0, 0, 0);
    }

    GLenum glFormat;
    GLenum glInternalFormat;
    if (!TextureFormatToGlFormat(format, useSrgbFormat, glFormat, glInternalFormat)) {
        ALOG(
            "%s: TextureFormatToGlFormat 0x%x %s failed",
            fileName,
            (int)format,
            useSrgbFormat ? "true" : "false");
        return GlTexture(0, 0, 0);
    }

    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texId);

    const unsigned char* level = (const unsigned char*)data;
    const unsigned char* endOfBuffer = level + dataSize;

    for (int i = 0; i < mipcount; i++) {
        const int w = width >> i;
        int32_t mipSize = GetOvrTextureSize(format, w, w);
        if (imageSizeStored) {
            mipSize = static_cast<int32_t>(*(const size_t*)level);
            level += 4;
            if (level > endOfBuffer) {
                ALOG("%s: Image data exceeds buffer size: %p > %p", fileName, level, endOfBuffer);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                return GlTexture(texId, GL_TEXTURE_CUBE_MAP, width, height);
            }
        }

        for (int side = 0; side < 6; side++) {
            if (mipSize <= 0 || mipSize > endOfBuffer - level) {
                ALOG(
                    "%s: Mip level %d exceeds buffer size (%u > %td)",
                    fileName,
                    i,
                    mipSize,
                    ptrdiff_t(endOfBuffer - level));
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                return GlTexture(texId, GL_TEXTURE_CUBE_MAP, width, height);
            }

            if (IsCompressedFormat(format)) {
                glCompressedTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + side,
                    i,
                    glInternalFormat,
                    w,
                    w,
                    0,
                    mipSize,
                    level);
            } else {
                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + side,
                    i,
                    glInternalFormat,
                    w,
                    w,
                    0,
                    glFormat,
                    GL_UNSIGNED_BYTE,
                    level);
            }

            level += mipSize;
            if (imageSizeStored) {
                level += 3 - ((mipSize + 3) % 4);
                if (level > endOfBuffer) {
                    ALOG("%s: Image data exceeds buffer size", fileName);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    return GlTexture(texId, GL_TEXTURE_CUBE_MAP, width, height);
                }
            }
        }
    }

    // Surfaces look pretty terrible without trilinear filtering
    if (mipcount <= 1) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLCheckErrorsWithTitle("Cube Texture load");

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return GlTexture(texId, GL_TEXTURE_CUBE_MAP, width, height);
}

GlTexture LoadRGBATextureFromMemory(
    const uint8_t* texture,
    const int width,
    const int height,
    const bool useSrgbFormat) {
    const size_t dataSize = GetOvrTextureSize(Texture_RGBA, width, height);
    return CreateGlTexture(
        "memory-RGBA", Texture_RGBA, width, height, texture, dataSize, 1, useSrgbFormat, false);
}

GlTexture
LoadRGBACubeTextureFromMemory(const uint8_t* texture, const int dim, const bool useSrgbFormat) {
    const size_t dataSize = GetOvrTextureSize(Texture_RGBA, dim, dim) * 6;
    return CreateGlCubeTexture(
        "memory-CubeRGBA", Texture_RGBA, dim, dim, texture, dataSize, 1, useSrgbFormat, false);
}

GlTexture LoadRGBTextureFromMemory(
    const uint8_t* texture,
    const int width,
    const int height,
    const bool useSrgbFormat) {
    const size_t dataSize = GetOvrTextureSize(Texture_RGB, width, height);
    return CreateGlTexture(
        "memory-RGB", Texture_RGB, width, height, texture, dataSize, 1, useSrgbFormat, false);
}

GlTexture LoadRTextureFromMemory(const uint8_t* texture, const int width, const int height) {
    const size_t dataSize = GetOvrTextureSize(Texture_R, width, height);
    return CreateGlTexture(
        "memory-R", Texture_R, width, height, texture, dataSize, 1, false, false);
}

// .astc files are created by the reference Mali compression tool.
// As of 9/17/2016, it appears to automatically flip y, which is a good
// reason to avoid it.
struct astcHeader {
    unsigned char magic[4];
    unsigned char blockDim_x;
    unsigned char blockDim_y;
    unsigned char blockDim_z;
    unsigned char xsize[3];
    unsigned char ysize[3];
    unsigned char zsize[3];
};

GlTexture LoadASTCTextureFromMemory(
    uint8_t const* buffer,
    const size_t bufferSize,
    const int numPlanes,
    const bool useSrgbFormat) {
    astcHeader const* header = reinterpret_cast<astcHeader const*>(buffer);

    int const w =
        ((int)header->xsize[2] << 16) | ((int)header->xsize[1] << 8) | ((int)header->xsize[0]);
    int const h =
        ((int)header->ysize[2] << 16) | ((int)header->ysize[1] << 8) | ((int)header->ysize[0]);

    assert(numPlanes == 1 || numPlanes == 4);
    OVR_UNUSED(numPlanes);
    if (header->blockDim_z != 1) {
        assert(header->blockDim_z == 1);
        ALOG("Only 2D ASTC textures are supported");
        return GlTexture();
    }

    eTextureFormat format = Texture_None;
    if (header->blockDim_x == 4) {
        if (header->blockDim_y == 4) {
            format = Texture_ASTC_4x4;
        }
    } else if (header->blockDim_x == 5) {
        if (header->blockDim_y == 4) {
            format = Texture_ASTC_5x4;
        } else if (header->blockDim_y == 5) {
            format = Texture_ASTC_5x5;
        }
    } else if (header->blockDim_x == 6) {
        if (header->blockDim_y == 5) {
            format = Texture_ASTC_6x5;
        } else if (header->blockDim_y == 6) {
            format = Texture_ASTC_6x6;
        }
    } else if (header->blockDim_x == 8) {
        if (header->blockDim_y == 5) {
            format = Texture_ASTC_8x5;
        } else if (header->blockDim_y == 6) {
            format = Texture_ASTC_8x6;
        } else if (header->blockDim_y == 8) {
            format = Texture_ASTC_8x8;
        }
    } else if (header->blockDim_x == 10) {
        if (header->blockDim_y == 5) {
            format = Texture_ASTC_10x5;
        } else if (header->blockDim_y == 6) {
            format = Texture_ASTC_10x6;
        } else if (header->blockDim_y == 8) {
            format = Texture_ASTC_10x8;
        } else if (header->blockDim_y == 10) {
            format = Texture_ASTC_10x10;
        }
    } else if (header->blockDim_x == 12) {
        if (header->blockDim_y == 10) {
            format = Texture_ASTC_12x10;
        } else if (header->blockDim_y == 12) {
            format = Texture_ASTC_12x12;
        }
    }

    if (format == Texture_None) {
        assert(format != Texture_None);
        ALOG("Unhandled ASTC block size: %i x %i", header->blockDim_x, header->blockDim_y);
        return GlTexture();
    }
    return CreateGlTexture(
        "memory-ASTC",
        format,
        w,
        h,
        buffer + sizeof(struct astcHeader),
        bufferSize - sizeof(struct astcHeader),
        1,
        useSrgbFormat,
        false);
}
/*

PVR Container Format

Offset    Size       Name           Description
0x0000    4 [DWORD]  Version        0x03525650
0x0004    4 [DWORD]  Flags          0x0000 if no flags set
                                    0x0002 if colors within the texture
0x0008    8 [Union]  Pixel Format   This can either be one of several predetermined enumerated
                                    values (a DWORD) or a 4-character array and a 4-byte array (8
bytes). If the most significant 4 bytes of the 64-bit (8-byte) value are all zero, then it indicates
that it is the enumeration with the following values: Value  Pixel Type 0      PVRTC 2bpp RGB 1
PVRTC 2bpp RGBA 2      PVRTC 4bpp RGB 3      PVRTC 4bpp RGBA 4      PVRTC-II 2bpp 5      PVRTC-II
4bpp 6      ETC1 7      DXT1 / BC1 8      DXT2 9      DXT3 / BC2 10     DXT4 11     DXT5 / BC3 12
BC4 13     BC5 14     BC6 15     BC7 16     UYVY 17     YUY2 18     BW1bpp 19     R9G9B9E5 Shared
Exponent 20     RGBG8888 21     GRGB8888 22     ETC2 RGB 23     ETC2 RGBA 24     ETC2 RGB A1 25 EAC
R11 Unsigned 26     EAC R11 Signed 27     EAC RG11 Unsigned 28     EAC RG11 Signed If the most
significant 4 bytes are not zero then the 8-byte character array indicates the pixel format as
follows: The least significant 4 bytes indicate channel order, such as: { 'b', 'g', 'r', 'a' } or {
'b', 'g', 'r', '\0' } The most significant 4 bytes indicate the width of each channel in bits, as
follows: { 4, 4, 4, 4 } or { 2, 2, 2, 2 }, or {5, 5, 5, 0 } 0x0010  4 [DWORD]    Color Space    This
is an enumerated field, currently two values: Value   Color Space 0       Linear RGB 1 Standard RGB
0x0014  4 [DWORD]    Channel Type   This is another enumerated field:
                                    Value   Data Type
                                    0       Unsigned Byte Normalized
                                    1       Signed Byte Normalized
                                    2       Unsigned Byte
                                    3       Signed Byte
                                    4       Unsigned Short Normalized
                                    5       Signed Short Normalized
                                    6       Unsigned Short
                                    7       Signed Short
                                    8       Unsigned Integer Normalized
                                    9       Signed Integer Normalized
                                    10      Unsigned Integer
                                    11      Signed Integer
                                    12      Float (no size specified)
0x0018  4 [DWORD]    Height         Height of the image.
0x001C  4 [DWORD]    Width          Width of the image.
0x0020  4 [DWORD]    Depth          Depth of the image, in pixels.
0x0024  4 [DWORD]    Surface Count  The number of surfaces to this texture, used for texture arrays.
0x0028  4 [DWORD]    Face Count     The number of faces to this texture, used for cube maps.
0x002C  4 [DWORD]    MIP-Map Count  The number of MIP-Map levels, including a top level.
0x0030  4 [DWORD]    Metadata Size  The size, in bytes, of meta data that immediately follows this
header.

*/

#pragma pack(1)
struct OVR_PVR_HEADER {
    std::uint32_t Version;
    std::uint32_t Flags;
    std::uint64_t PixelFormat;
    std::uint32_t ColorSpace;
    std::uint32_t ChannelType;
    std::uint32_t Height;
    std::uint32_t Width;
    std::uint32_t Depth;
    std::uint32_t NumSurfaces;
    std::uint32_t NumFaces;
    std::uint32_t MipMapCount;
    std::uint32_t MetaDataSize;
};
#pragma pack()

GlTexture LoadTexturePVR(
    const char* fileName,
    const unsigned char* buffer,
    const int bufferLength,
    bool useSrgbFormat,
    bool noMipMaps,
    int& width,
    int& height) {
    width = 0;
    height = 0;

    if (bufferLength < (int)(sizeof(OVR_PVR_HEADER))) {
        ALOG("%s: Invalid PVR file", fileName);
        return GlTexture(0, 0, 0);
    }

    const OVR_PVR_HEADER& header = *(OVR_PVR_HEADER*)buffer;
    if (header.Version != 0x03525650) {
        ALOG("%s: Invalid PVR file version", fileName);
        return GlTexture(0, 0, 0);
    }

    eTextureFormat format = Texture_None;
    switch (header.PixelFormat) {
        case 2:
            format = Texture_PVR4bRGB;
            break;
        case 3:
            format = Texture_PVR4bRGBA;
            break;
        case 6:
            format = Texture_ETC1;
            break;
        case 22:
            format = Texture_ETC2_RGB;
            break;
        case 23:
            format = Texture_ETC2_RGBA;
            break;
        case 578721384203708274llu:
            format = Texture_RGBA;
            break;
        default:
            ALOG(
                "%s: Unknown PVR texture format %u, size %ix%i",
                fileName,
                static_cast<uint32_t>(header.PixelFormat),
                width,
                height);
            return GlTexture(0, 0, 0);
    }

    // skip the metadata
    const std::uint32_t startTex = sizeof(OVR_PVR_HEADER) + header.MetaDataSize;
    if ((startTex < sizeof(OVR_PVR_HEADER)) || (startTex >= static_cast<size_t>(bufferLength))) {
        ALOG("%s: Invalid PVR header sizes", fileName);
        return GlTexture(0, 0, 0);
    }

    const std::uint32_t mipCount = (noMipMaps)
        ? 1
        : std::max<std::uint32_t>(static_cast<std::uint32_t>(1u), header.MipMapCount);

    width = header.Width;
    height = header.Height;

    if (header.NumFaces == 1) {
        return CreateGlTexture(
            fileName,
            format,
            width,
            height,
            buffer + startTex,
            bufferLength - startTex,
            mipCount,
            useSrgbFormat,
            false);
    } else if (header.NumFaces == 6) {
        return CreateGlCubeTexture(
            fileName,
            format,
            width,
            height,
            buffer + startTex,
            bufferLength - startTex,
            mipCount,
            useSrgbFormat,
            false);
    } else {
        ALOG("%s: PVR file has unsupported number of faces %d", fileName, header.NumFaces);
    }

    width = 0;
    height = 0;
    return GlTexture(0, 0, 0);
}

unsigned char* LoadPVRBuffer(const char* fileName, int& width, int& height) {
    width = 0;
    height = 0;

    std::vector<uint8_t> buffer;
    std::ifstream is;
    is.open(fileName, std::ios::binary | std::ios::in);
    if (!is.is_open()) {
        return nullptr;
    }
    // get size
    is.seekg(0, is.end);
    size_t file_size = (size_t)is.tellg();
    is.seekg(0, is.beg);
    // allocate buffer
    buffer.resize(file_size);
    // read all file
    if (!is.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        return nullptr;
    }
    // close
    is.close();

    if (buffer.size() < sizeof(OVR_PVR_HEADER)) {
        ALOG("Invalid PVR file");
        return nullptr;
    }

    const OVR_PVR_HEADER& header = *(OVR_PVR_HEADER*)buffer.data();
    if (header.Version != 0x03525650) {
        ALOG("Invalid PVR file version");
        return nullptr;
    }

    eTextureFormat format = Texture_None;
    switch (header.PixelFormat) {
        case 578721384203708274llu:
            format = Texture_RGBA;
            break;
        default:
            ALOG(
                "Unknown PVR texture format %u, size %ix%i",
                static_cast<uint32_t>(header.PixelFormat),
                width,
                height);
            return nullptr;
    }

    // skip the metadata
    size_t startTex = sizeof(OVR_PVR_HEADER) + header.MetaDataSize;
    if ((startTex < sizeof(OVR_PVR_HEADER)) || (startTex >= buffer.size())) {
        ALOG("Invalid PVR header sizes");
        return nullptr;
    }

    size_t mipSize = GetOvrTextureSize(format, header.Width, header.Height);

    // NOTE: cast to int before subtracting!!!!
    const int outBufferSizeBytes = static_cast<int>(buffer.size()) - static_cast<int>(startTex);
    if (outBufferSizeBytes < 0 || mipSize > static_cast<size_t>(outBufferSizeBytes)) {
        return nullptr;
    }

    width = header.Width;
    height = header.Height;

    // skip the metadata
    unsigned char* outBuffer = (unsigned char*)malloc(outBufferSizeBytes);
    memcpy(outBuffer, (unsigned char*)buffer.data() + startTex, outBufferSizeBytes);
    return outBuffer;
}

/*

KTX Container Format

KTX is a format for storing textures for OpenGL and OpenGL ES applications.
It is distinguished by the simplicity of the loader required to instantiate
a GL texture object from the file contents.

Byte[12] identifier
std::uint32_t endianness
std::uint32_t glType
std::uint32_t glTypeSize
std::uint32_t glFormat
Uint32 glInternalFormat
Uint32 glBaseInternalFormat
std::uint32_t pixelWidth
std::uint32_t pixelHeight
std::uint32_t pixelDepth
std::uint32_t numberOfArrayElements
std::uint32_t numberOfFaces
std::uint32_t numberOfMipmapLevels
std::uint32_t bytesOfKeyValueData

for each keyValuePair that fits in bytesOfKeyValueData
    std::uint32_t   keyAndValueByteSize
    Byte     keyAndValue[keyAndValueByteSize]
    Byte     valuePadding[3 - ((keyAndValueByteSize + 3) % 4)]
end

for each mipmap_level in numberOfMipmapLevels*
    std::uint32_t imageSize;
    for each array_element in numberOfArrayElements*
       for each face in numberOfFaces
           for each z_slice in pixelDepth*
               for each row or row_of_blocks in pixelHeight*
                   for each pixel or block_of_pixels in pixelWidth
                       Byte data[format-specific-number-of-bytes]**
                   end
               end
           end
           Byte cubePadding[0-3]
       end
    end
    Byte mipPadding[3 - ((imageSize + 3) % 4)]
end

*/

#pragma pack(1)
struct OVR_KTX_HEADER {
    std::uint8_t identifier[12];
    std::uint32_t endianness;
    std::uint32_t glType;
    std::uint32_t glTypeSize;
    std::uint32_t glFormat;
    std::uint32_t glInternalFormat;
    std::uint32_t glBaseInternalFormat;
    std::uint32_t pixelWidth;
    std::uint32_t pixelHeight;
    std::uint32_t pixelDepth;
    std::uint32_t numberOfArrayElements;
    std::uint32_t numberOfFaces;
    std::uint32_t numberOfMipmapLevels;
    std::uint32_t bytesOfKeyValueData;
};
#pragma pack()

GlTexture LoadTextureKTX(
    const char* fileName,
    const unsigned char* buffer,
    const int bufferLength,
    bool useSrgbFormat,
    bool noMipMaps,
    int& width,
    int& height) {
    width = 0;
    height = 0;

    if (bufferLength < (int)(sizeof(OVR_KTX_HEADER))) {
        ALOG("%s: Invalid KTX file", fileName);
        return GlTexture(0, 0, 0);
    }

    const char fileIdentifier[12] = {
        '\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n'};

    const OVR_KTX_HEADER& header = *(OVR_KTX_HEADER*)buffer;
    if (memcmp(header.identifier, fileIdentifier, sizeof(fileIdentifier)) != 0) {
        ALOG("%s: Invalid KTX file", fileName);
        return GlTexture(0, 0, 0);
    }
    // only support little endian
    if (header.endianness != 0x04030201) {
        ALOG("%s: KTX file has wrong endianess", fileName);
        return GlTexture(0, 0, 0);
    }
    // only support compressed or unsigned byte
    if (header.glType != 0 && header.glType != GL_UNSIGNED_BYTE) {
        ALOG("%s: KTX file has unsupported glType %d", fileName, header.glType);
        return GlTexture(0, 0, 0);
    }
    // no support for texture arrays
    if (header.numberOfArrayElements != 0) {
        ALOG(
            "%s: KTX file has unsupported number of array elements %d",
            fileName,
            header.numberOfArrayElements);
        return GlTexture(0, 0, 0);
    }

    // derive the texture format from the GL format
    eTextureFormat format = Texture_None;
    if (!GlFormatToTextureFormat(format, header.glFormat, header.glInternalFormat)) {
        ALOG(
            "%s: KTX file has unsupported glFormat %d, glInternalFormat %d",
            fileName,
            header.glFormat,
            header.glInternalFormat);
        return GlTexture(0, 0, 0);
    }

    // skip the key value data
    const uintptr_t startTex = sizeof(OVR_KTX_HEADER) + header.bytesOfKeyValueData;
    if ((startTex < sizeof(OVR_KTX_HEADER)) || (startTex >= static_cast<size_t>(bufferLength))) {
        ALOG("%s: Invalid KTX header sizes", fileName);
        return GlTexture(0, 0, 0);
    }

    width = header.pixelWidth;
    height = header.pixelHeight;

    const std::uint32_t mipCount = (noMipMaps)
        ? 1
        : std::max<std::uint32_t>(static_cast<std::uint32_t>(1u), header.numberOfMipmapLevels);

    if (header.numberOfFaces == 1) {
        return CreateGlTexture(
            fileName,
            format,
            width,
            height,
            buffer + startTex,
            bufferLength - startTex,
            mipCount,
            useSrgbFormat,
            true);
    } else if (header.numberOfFaces == 6) {
        return CreateGlCubeTexture(
            fileName,
            format,
            width,
            height,
            buffer + startTex,
            bufferLength - startTex,
            mipCount,
            useSrgbFormat,
            true);
    } else {
        ALOG("%s: KTX file has unsupported number of faces %d", fileName, header.numberOfFaces);
    }

    width = 0;
    height = 0;
    return GlTexture(0, 0, 0);
}

unsigned char* LoadImageToRGBABuffer(
    const char* fileName,
    const unsigned char* inBuffer,
    const size_t inBufferLen,
    int& width,
    int& height) {
    std::string ext = GetExtension(fileName);
    auto& loc = std::use_facet<std::ctype<char>>(std::locale());
    loc.tolower(&ext[0], &ext[0] + ext.length());

    width = 0;
    height = 0;

    if (ext == ".jpg" || ext == ".tga" || ext == ".png" || ext == ".bmp" || ext == ".psd" ||
        ext == ".gif" || ext == ".hdr" || ext == ".pic") {
        // Uncompressed files loaded by stb_image
        int comp;
        stbi_uc* image = stbi_load_from_memory(
            (unsigned char*)inBuffer, (int)inBufferLen, &width, &height, &comp, 4);
        return image;
    }
    return nullptr;
}

void FreeRGBABuffer(const unsigned char* buffer) {
    stbi_image_free((void*)buffer);
}

static int MipLevelsForSize(int width, int height) {
    int levels = 1;

    while (width > 1 || height > 1) {
        levels++;
        width >>= 1;
        height >>= 1;
    }
    return levels;
}

GlTexture LoadTextureFromBuffer(
    const char* fileName,
    const uint8_t* buffer,
    size_t bufferSize,
    const TextureFlags_t& flags,
    int& width,
    int& height) {
    std::string ext = GetExtension(fileName);
    auto& loc = std::use_facet<std::ctype<char>>(std::locale());
    loc.tolower(&ext[0], &ext[0] + ext.length());

    // LOG( "Loading texture buffer %s (%s), length %i", fileName, ext.c_str(), buffer.Length );

    GlTexture texId;
    width = 0;
    height = 0;

    if (fileName == nullptr || buffer == nullptr || bufferSize < 1) {
        // can't load anything from an empty buffer
#if defined(OVR_BUILD_DEBUG)
        ALOG(
            "LoadTextureFromBuffer - can't load from empties: fileName = %s buffer = %p bufferSize = %d",
            fileName == nullptr ? "<null>" : fileName,
            buffer == nullptr ? 0 : buffer,
            static_cast<int>(bufferSize));
#endif
    } else if (
        ext == ".jpg" || ext == ".tga" || ext == ".png" || ext == ".bmp" || ext == ".psd" ||
        ext == ".gif" || ext == ".hdr" || ext == ".pic") {
        // Uncompressed files loaded by stb_image
        int comp;
        stbi_uc* image = stbi_load_from_memory(buffer, bufferSize, &width, &height, &comp, 4);
        if (image != NULL) {
            // Optionally outline the border alpha.
            if (flags & TEXTUREFLAG_ALPHA_BORDER) {
                for (int i = 0; i < width; i++) {
                    image[i * 4 + 3] = 0;
                    image[((height - 1) * width + i) * 4 + 3] = 0;
                }
                for (int i = 0; i < height; i++) {
                    image[i * width * 4 + 3] = 0;
                    image[(i * width + width - 1) * 4 + 3] = 0;
                }
            }

            const size_t dataSize = GetOvrTextureSize(Texture_RGBA, width, height);
            texId = CreateGlTexture(
                fileName,
                Texture_RGBA,
                width,
                height,
                image,
                dataSize,
                (flags & TEXTUREFLAG_NO_MIPMAPS) ? 1 : MipLevelsForSize(width, height),
                flags & TEXTUREFLAG_USE_SRGB,
                false);
            free(image);
            if (!(flags & TEXTUREFLAG_NO_MIPMAPS)) {
                glBindTexture(texId.target, texId.texture);
                glGenerateMipmap(texId.target);
                glTexParameteri(texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }
        } else {
            ALOG("stbi_load_from_memory() failed!");
        }
    } else if (ext == ".pvr") {
        texId = LoadTexturePVR(
            fileName,
            buffer,
            bufferSize,
            (flags & TEXTUREFLAG_USE_SRGB),
            (flags & TEXTUREFLAG_NO_MIPMAPS),
            width,
            height);
    } else if (ext == ".ktx") {
        texId = LoadTextureKTX(
            fileName,
            buffer,
            bufferSize,
            (flags & TEXTUREFLAG_USE_SRGB),
            (flags & TEXTUREFLAG_NO_MIPMAPS),
            width,
            height);
    } else if (ext == ".astc") {
        texId = LoadASTCTextureFromMemory(buffer, bufferSize, 4, flags & TEXTUREFLAG_USE_SRGB);
    } else if (ext == ".pkm") {
        ALOG("PKM format not supported");
    } else {
        ALOG("unsupported file extension '%s', for file '%s'", ext.c_str(), fileName);
    }

    // Create a default texture if the load failed
    if (texId.texture == 0) {
        ALOGW("Failed to load %s", fileName);
        if ((flags & TEXTUREFLAG_NO_DEFAULT) == 0) {
            static uint8_t defaultTexture[8 * 8 * 3] = {
                255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 64,  64,  64,  64,  64,
                64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  255, 255, 255,
                255, 255, 255, 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
                64,  64,  64,  64,  64,  255, 255, 255, 255, 255, 255, 64,  64,  64,  64,  64,
                64,  255, 255, 255, 255, 255, 255, 64,  64,  64,  64,  64,  64,  255, 255, 255,
                255, 255, 255, 64,  64,  64,  64,  64,  64,  255, 255, 255, 255, 255, 255, 64,
                64,  64,  64,  64,  64,  255, 255, 255, 255, 255, 255, 64,  64,  64,  64,  64,
                64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  255, 255, 255,
                255, 255, 255, 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
                64,  64,  64,  64,  64,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
            texId = LoadRGBTextureFromMemory(defaultTexture, 8, 8, flags & TEXTUREFLAG_USE_SRGB);
#if defined(OVR_BUILD_DEBUG)
            ALOG("FAILD to load '%s' -> using default via LoadRGBTextureFromMemory", fileName);
#endif
        }
    }
#if defined(OVR_BUILD_DEBUG)
    else {
        ALOG("Finish loading '%s' -> SUCCESS", fileName);
    }
#endif

    return texId;
}

GlTexture LoadTextureFromOtherApplicationPackage(
    void* zipFile,
    const char* nameInZip,
    const TextureFlags_t& flags,
    int& width,
    int& height) {
    width = 0;
    height = 0;
    if (zipFile == 0) {
        return GlTexture(0, 0, 0);
    }

    std::vector<uint8_t> buffer;
    ovr_ReadFileFromOtherApplicationPackage(zipFile, nameInZip, buffer);
    if (buffer.size() == 0) {
        return GlTexture(0, 0, 0);
    }

    return LoadTextureFromBuffer(nameInZip, buffer, flags, width, height);
}

GlTexture LoadTextureFromApplicationPackage(
    const char* nameInZip,
    const TextureFlags_t& flags,
    int& width,
    int& height) {
    return LoadTextureFromOtherApplicationPackage(
        ovr_GetApplicationPackageFile(), nameInZip, flags, width, height);
}

GlTexture LoadTextureFromUri(
    class ovrFileSys& fileSys,
    const char* uri,
    const TextureFlags_t& flags,
    int& width,
    int& height) {
    std::vector<uint8_t> buffer;
    if (!fileSys.ReadFile(uri, buffer)) {
        return GlTexture();
    }

    return LoadTextureFromBuffer(uri, buffer, flags, width, height);
}

void FreeTexture(GlTexture texId) {
    if (texId.texture) {
        glDeleteTextures(1, &texId.texture);
    }
}

void DeleteTexture(GlTexture& texture) {
    if (texture.texture != 0) {
        glDeleteTextures(1, &texture.texture);
        texture.texture = 0;
        texture.target = 0;
        texture.Width = 0;
        texture.Height = 0;
    }
}

void MakeTextureClamped(GlTexture texId) {
    glBindTexture(texId.target, texId.texture);
    glTexParameteri(texId.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(texId.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(texId.target, 0);
}

void MakeTextureLodClamped(GlTexture texId, int maxLod) {
    glBindTexture(texId.target, texId.texture);
    glTexParameteri(texId.target, GL_TEXTURE_MAX_LEVEL, maxLod);
    glBindTexture(texId.target, 0);
}

void MakeTextureTrilinear(GlTexture texId) {
    glBindTexture(texId.target, texId.texture);
    glTexParameteri(texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(texId.target, 0);
}

void MakeTextureLinearNearest(GlTexture texId) {
    glBindTexture(texId.target, texId.texture);
    glTexParameteri(texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(texId.target, 0);
}

void MakeTextureLinear(GlTexture texId) {
    glBindTexture(texId.target, texId.texture);
    glTexParameteri(texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(texId.target, 0);
}

void MakeTextureAniso(GlTexture texId, float maxAniso) {
    glBindTexture(texId.target, texId.texture);
    glTexParameterf(texId.target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    glBindTexture(texId.target, 0);
}

void BuildTextureMipmaps(GlTexture texId) {
    glBindTexture(texId.target, texId.texture);
    glGenerateMipmap(texId.target);
    glBindTexture(texId.target, 0);
}

} // namespace OVRFW
