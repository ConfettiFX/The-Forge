/************************************************************************************

Filename    :   GlTexture.h
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include <cstdint>
#include <inttypes.h>
#include "OVR_BitFlags.h"
#include "Egl.h"
#include "OVR_FileSys.h"

#include <vector>

// Explicitly using unsigned instead of GLUint / GLenum to avoid including GL headers

namespace OVRFW {

enum eTextureFlags {
    // Normally, a failure to load will create an 8x8 default texture, but
    // if you want to take explicit action, setting this flag will cause
    // it to return 0 for the texId.
    TEXTUREFLAG_NO_DEFAULT,

    // Use GL_SRGB8 / GL_SRGB8_ALPHA8 / GL_COMPRESSED_SRGB8_ETC2 formats instead
    // of GL_RGB / GL_RGBA / GL_ETC1_RGB8_OES
    TEXTUREFLAG_USE_SRGB,

    // No mip maps are loaded or generated when this flag is specified.
    TEXTUREFLAG_NO_MIPMAPS,

    // Forces a one pixel border around the texture to have
    // zero alpha, so a blended quad will be perfectly anti-aliased.
    // Will only work for uncompressed textures.
    // TODO: this only does the top mip level, since we use genMipmaps
    // to create the rest. Consider manually building the mip levels.
    TEXTUREFLAG_ALPHA_BORDER
};

typedef OVR::BitFlagsT<eTextureFlags> TextureFlags_t;

enum eTextureFormat {
    Texture_None = 0x00000,
    Texture_R = 0x00100,
    Texture_RGB = 0x00200,
    Texture_RGBA = 0x00300,
    Texture_DXT1 = 0x01100,
    Texture_DXT3 = 0x01200,
    Texture_DXT5 = 0x01300,
    Texture_PVR4bRGB = 0x01400,
    Texture_PVR4bRGBA = 0x01500,
    Texture_ATC_RGB = 0x01600,
    Texture_ATC_RGBA = 0x01700,
    Texture_ETC1 = 0x01800,
    Texture_ETC2_RGB = 0x01900,
    Texture_ETC2_RGBA = 0x01A00,

    // ASTC values must be sequential between Texture_ASTC_Start and Texture_ASTC_End
    Texture_ASTC_Start = 0x01B00,
    Texture_ASTC_4x4 = Texture_ASTC_Start,

    Texture_ASTC_5x4 = 0x01C00,
    Texture_ASTC_5x5 = 0x01D00,
    Texture_ASTC_6x5 = 0x01E00,
    Texture_ASTC_6x6 = 0x01F00,
    Texture_ASTC_8x5 = 0x02000,
    Texture_ASTC_8x6 = 0x02100,
    Texture_ASTC_8x8 = 0x02200,
    Texture_ASTC_10x5 = 0x02300,
    Texture_ASTC_10x6 = 0x02400,
    Texture_ASTC_10x8 = 0x02500,
    Texture_ASTC_10x10 = 0x02600,
    Texture_ASTC_12x10 = 0x02700,
    Texture_ASTC_12x12 = 0x02800,

    Texture_ASTC_SRGB_4x4 = 0x02900,
    Texture_ASTC_SRGB_5x4 = 0x02A00,
    Texture_ASTC_SRGB_5x5 = 0x02B00,
    Texture_ASTC_SRGB_6x5 = 0x02C00,
    Texture_ASTC_SRGB_6x6 = 0x02D00,
    Texture_ASTC_SRGB_8x5 = 0x02E00,
    Texture_ASTC_SRGB_8x6 = 0x02F00,
    Texture_ASTC_SRGB_8x8 = 0x03000,
    Texture_ASTC_SRGB_10x5 = 0x03100,
    Texture_ASTC_SRGB_10x6 = 0x03200,
    Texture_ASTC_SRGB_10x8 = 0x03300,
    Texture_ASTC_SRGB_10x10 = 0x03400,
    Texture_ASTC_SRGB_12x10 = 0x03500,
    Texture_ASTC_SRGB_12x12 = 0x03600,

    Texture_ASTC_End = 0x03700,

    Texture_Depth = 0x08000,

    Texture_TypeMask = 0x0ff00,
    Texture_SamplesMask = 0x000ff,
    Texture_RenderTarget = 0x10000,
    Texture_GenMipmaps = 0x20000
};

// texture id/target pair
// the auto-casting should be removed but allows the target to be ignored by the code that does not
// care
class GlTexture {
   public:
    GlTexture() : texture(0), target(0), Width(0), Height(0) {}

    GlTexture(const unsigned texture_, const int w, const int h);

    GlTexture(unsigned texture_, unsigned target_, const int w, const int h)
        : texture(texture_), target(target_), Width(w), Height(h) {}
    operator unsigned() const {
        return texture;
    }

    bool IsValid() const {
        return texture != 0;
    }

    unsigned texture;
    unsigned target;
    int Width;
    int Height;
};

bool TextureFormatToGlFormat(
    const eTextureFormat format,
    const bool useSrgbFormat,
    GLenum& glFormat,
    GLenum& glInternalFormat);
bool GlFormatToTextureFormat(
    eTextureFormat& format,
    const GLenum glFormat,
    const GLenum glInternalFormat);

// Calculate the full mip chain levels based on width and height.
int ComputeFullMipChainNumLevels(const int width, const int height);

// Allocates a GPU texture and uploads the raw data.
GlTexture LoadRGBATextureFromMemory(
    const uint8_t* texture,
    const int width,
    const int height,
    const bool useSrgbFormat);
GlTexture
LoadRGBACubeTextureFromMemory(const uint8_t* texture, const int dim, const bool useSrgbFormat);
GlTexture LoadRGBTextureFromMemory(
    const uint8_t* texture,
    const int width,
    const int height,
    const bool useSrgbFormat);
GlTexture LoadRTextureFromMemory(const uint8_t* texture, const int width, const int height);
GlTexture LoadASTCTextureFromMemory(
    const uint8_t* buffer,
    const size_t bufferSize,
    const int numPlanes,
    const bool useSrgbFormat);

void MakeTextureClamped(GlTexture texid);
void MakeTextureLodClamped(GlTexture texId, int maxLod);
void MakeTextureTrilinear(GlTexture texid);
void MakeTextureLinear(GlTexture texId);
void MakeTextureLinearNearest(GlTexture texId);
void MakeTextureAniso(GlTexture texId, float maxAniso);
void BuildTextureMipmaps(GlTexture texid);

// Loads an image file to an RGBA buffer.
// Supported formats are:
//	.jpg .tga .png .bmp .psd .gif .hdr and .pic
unsigned char* LoadImageToRGBABuffer(
    const char* fileName,
    const unsigned char* inBuffer,
    const size_t inBufferLen,
    int& width,
    int& height);

// Free image data allocated by LoadImageToRGBABuffer
void FreeRGBABuffer(const unsigned char* buffer);

// FileName's extension determines the file type, but the data is taken from an
// already loaded buffer.
//
// The stb_image file formats are supported:
// .jpg .tga .png .bmp .psd .gif .hdr .pic
//
// Limited support for the PVR and KTX container formats.
//
// If TEXTUREFLAG_NO_DEFAULT, no default texture will be created.
// Otherwise a default square texture will be created on any failure.
//
// Uncompressed image formats will have mipmaps generated and trilinear filtering set.
GlTexture LoadTextureFromBuffer(
    const char* fileName,
    const uint8_t* buffer,
    size_t bufferSize,
    const TextureFlags_t& flags,
    int& width,
    int& height);

inline GlTexture LoadTextureFromBuffer(
    const char* fileName,
    const std::vector<uint8_t>& buffer,
    const TextureFlags_t& flags,
    int& width,
    int& height) {
    return LoadTextureFromBuffer(fileName, buffer.data(), buffer.size(), flags, width, height);
}

// Returns 0 if the file is not found.
// For a file placed in the project assets folder, nameInZip would be
// something like "assets/cube.pvr".
// See GlTexture.h for supported formats.
/// DEPRECATED! Use LoadTextureFromUri instead or your asset will not be loadable in Windows ports!
GlTexture LoadTextureFromOtherApplicationPackage(
    void* zipFile,
    const char* nameInZip,
    const TextureFlags_t& flags,
    int& width,
    int& height);
/// DEPRECATED! Use the version that takes a fileSys or your asset will not be loadable in Windows
/// ports!
GlTexture LoadTextureFromApplicationPackage(
    const char* nameInZip,
    const TextureFlags_t& flags,
    int& width,
    int& height);

// takes a ovrFileSys compatible URI specifying a texture resource. To load from the application's
// own apk use the apk scheme with the form:
//     apk:///res/raw/texture_name.tga
//
// You can also specify an explicit host:
//
// localhost will load from the application's own apk:
//     apk://localhost/res/raw/texture_name.tga
//
// Other apk's (assuming they were added to the apk scheme in ovrFileSys::Init() can be specified
// by package name:
//     apk://com.oculus/systemactivities/res/raw/texture_name.tga
//
// Finally, fonts have their own host because their actual location may change in the future:
//     apk://font/res/raw/efigs.tga
//
// See ovrFileSys comments for more information.
GlTexture LoadTextureFromUri(
    class ovrFileSys& fileSys,
    const char* uri,
    const TextureFlags_t& flags,
    int& width,
    int& height);

unsigned char* LoadPVRBuffer(const char* fileName, int& width, int& height);

// glDeleteTextures()
// Can be safely called on a 0 texture without checking.
void FreeTexture(GlTexture texId);
void DeleteTexture(GlTexture& texture);

} // namespace OVRFW
