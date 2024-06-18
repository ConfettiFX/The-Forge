// MIT license see full LICENSE text at end of file
#pragma once
#ifndef TINY_KTX_TINYKTX_H
#define TINY_KTX_TINYKTX_H

#ifndef TINYKTX_HAVE_UINTXX_T
#include <stdint.h> 	// for uint32_t and int64_t
#endif
#ifndef TINYKTX_HAVE_BOOL
#include <stdbool.h>	// for bool
#endif
#ifndef TINYKTX_HAVE_SIZE_T
#include <stddef.h>		// for size_t
#endif
#ifndef TINYKTX_HAVE_MEMCPY
#include <string.h> 	// for memcpy
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TINYKTX_MAX_MIPMAPLEVELS 16

typedef struct TinyKtx_Context *TinyKtx_ContextHandle;

typedef void *(*TinyKtx_AllocFunc)(void *user, size_t size);
typedef void (*TinyKtx_FreeFunc)(void *user, void *memory);
typedef size_t (*TinyKtx_ReadFunc)(void *user, void *buffer, size_t byteCount);
typedef bool (*TinyKtx_SeekFunc)(void *user, int64_t offset);
typedef int64_t (*TinyKtx_TellFunc)(void *user);
typedef void (*TinyKtx_ErrorFunc)(void *user, char const *msg);

typedef struct TinyKtx_Callbacks {
	TinyKtx_ErrorFunc errorFn;
	TinyKtx_AllocFunc allocFn;
	TinyKtx_FreeFunc freeFn;
	TinyKtx_ReadFunc readFn;
	TinyKtx_SeekFunc seekFn;
	TinyKtx_TellFunc tellFn;
} TinyKtx_Callbacks;

TinyKtx_ContextHandle TinyKtx_CreateContext(TinyKtx_Callbacks const *callbacks, void *user);
void TinyKtx_DestroyContext(TinyKtx_ContextHandle handle);

// reset lets you reuse the context for another file (saves an alloc/free cycle)
void TinyKtx_Reset(TinyKtx_ContextHandle handle);

// call this to read the header file should already be at the start of the KTX data
bool TinyKtx_ReadHeader(TinyKtx_ContextHandle handle);

// this is slow linear search. TODO add iterator style reading of key value pairs
bool TinyKtx_GetValue(TinyKtx_ContextHandle handle, char const *key, void const **value);

bool TinyKtx_Is1D(TinyKtx_ContextHandle handle);
bool TinyKtx_Is2D(TinyKtx_ContextHandle handle);
bool TinyKtx_Is3D(TinyKtx_ContextHandle handle);
bool TinyKtx_IsCubemap(TinyKtx_ContextHandle handle);
bool TinyKtx_IsArray(TinyKtx_ContextHandle handle);

bool TinyKtx_Dimensions(TinyKtx_ContextHandle handle, uint32_t* width, uint32_t* height, uint32_t* depth, uint32_t* slices);
uint32_t TinyKtx_Width(TinyKtx_ContextHandle handle);
uint32_t TinyKtx_Height(TinyKtx_ContextHandle handle);
uint32_t TinyKtx_Depth(TinyKtx_ContextHandle handle);
uint32_t TinyKtx_ArraySlices(TinyKtx_ContextHandle handle);

bool TinyKtx_GetFormatGL(TinyKtx_ContextHandle handle, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t* typesize, uint32_t* glbaseinternalformat);

bool TinyKtx_NeedsGenerationOfMipmaps(TinyKtx_ContextHandle handle);
bool TinyKtx_NeedsEndianCorrecting(TinyKtx_ContextHandle handle);

uint32_t TinyKtx_NumberOfMipmaps(TinyKtx_ContextHandle handle);
uint32_t TinyKtx_ImageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

bool TinyKtx_IsMipMapLevelUnpacked(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);
// this is required to read Unpacked data correctly
uint32_t TinyKtx_UnpackedRowStride(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

// data return by ImageRawData is owned by the context. Don't free it!
void const *TinyKtx_ImageRawData(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

typedef void (*TinyKtx_WriteFunc)(void *user, void const *buffer, size_t byteCount);

typedef struct TinyKtx_WriteCallbacks {
	TinyKtx_ErrorFunc errorFn;
	TinyKtx_AllocFunc allocFn;
	TinyKtx_FreeFunc freeFn;
	TinyKtx_WriteFunc writeFn;
} TinyKtx_WriteCallbacks;


bool TinyKtx_WriteImageGL(TinyKtx_WriteCallbacks const *callbacks,
													void *user,
													uint32_t width,
													uint32_t height,
													uint32_t depth,
													uint32_t slices,
													uint32_t mipmaplevels,
													uint32_t format,
													uint32_t internalFormat,
													uint32_t baseFormat,
													uint32_t type,
													uint32_t typeSize,
													bool cubemap,
													uint32_t const *mipmapsizes,
													void const **mipmaps);

// ktx v1 is based on GL (slightly confusing imho) texture format system
// there is format, internal format, type etc.

// we try and expose a more dx12/vulkan/metal style of format
// but obviously still need to GL data so bare with me.
// a TinyKTX_Format is the equivilent to GL/KTX Format and Type
// the API doesn't expose the actual values (which come from GL itself)
// but provide an API call to crack them back into the actual GL values).

// Ktx v2 is based on VkFormat and also DFD, so we now base the
// enumeration values of TinyKtx_Format on the Vkformat values where possible

#ifndef TINYIMAGEFORMAT_VKFORMAT
#define TINYIMAGEFORMAT_VKFORMAT
typedef enum TinyImageFormat_VkFormat {
	TIF_VK_FORMAT_UNDEFINED = 0,
	TIF_VK_FORMAT_R4G4_UNORM_PACK8 = 1,
	TIF_VK_FORMAT_R4G4B4A4_UNORM_PACK16 = 2,
	TIF_VK_FORMAT_B4G4R4A4_UNORM_PACK16 = 3,
	TIF_VK_FORMAT_R5G6B5_UNORM_PACK16 = 4,
	TIF_VK_FORMAT_B5G6R5_UNORM_PACK16 = 5,
	TIF_VK_FORMAT_R5G5B5A1_UNORM_PACK16 = 6,
	TIF_VK_FORMAT_B5G5R5A1_UNORM_PACK16 = 7,
	TIF_VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8,
	TIF_VK_FORMAT_R8_UNORM = 9,
	TIF_VK_FORMAT_R8_SNORM = 10,
	TIF_VK_FORMAT_R8_USCALED = 11,
	TIF_VK_FORMAT_R8_SSCALED = 12,
	TIF_VK_FORMAT_R8_UINT = 13,
	TIF_VK_FORMAT_R8_SINT = 14,
	TIF_VK_FORMAT_R8_SRGB = 15,
	TIF_VK_FORMAT_R8G8_UNORM = 16,
	TIF_VK_FORMAT_R8G8_SNORM = 17,
	TIF_VK_FORMAT_R8G8_USCALED = 18,
	TIF_VK_FORMAT_R8G8_SSCALED = 19,
	TIF_VK_FORMAT_R8G8_UINT = 20,
	TIF_VK_FORMAT_R8G8_SINT = 21,
	TIF_VK_FORMAT_R8G8_SRGB = 22,
	TIF_VK_FORMAT_R8G8B8_UNORM = 23,
	TIF_VK_FORMAT_R8G8B8_SNORM = 24,
	TIF_VK_FORMAT_R8G8B8_USCALED = 25,
	TIF_VK_FORMAT_R8G8B8_SSCALED = 26,
	TIF_VK_FORMAT_R8G8B8_UINT = 27,
	TIF_VK_FORMAT_R8G8B8_SINT = 28,
	TIF_VK_FORMAT_R8G8B8_SRGB = 29,
	TIF_VK_FORMAT_B8G8R8_UNORM = 30,
	TIF_VK_FORMAT_B8G8R8_SNORM = 31,
	TIF_VK_FORMAT_B8G8R8_USCALED = 32,
	TIF_VK_FORMAT_B8G8R8_SSCALED = 33,
	TIF_VK_FORMAT_B8G8R8_UINT = 34,
	TIF_VK_FORMAT_B8G8R8_SINT = 35,
	TIF_VK_FORMAT_B8G8R8_SRGB = 36,
	TIF_VK_FORMAT_R8G8B8A8_UNORM = 37,
	TIF_VK_FORMAT_R8G8B8A8_SNORM = 38,
	TIF_VK_FORMAT_R8G8B8A8_USCALED = 39,
	TIF_VK_FORMAT_R8G8B8A8_SSCALED = 40,
	TIF_VK_FORMAT_R8G8B8A8_UINT = 41,
	TIF_VK_FORMAT_R8G8B8A8_SINT = 42,
	TIF_VK_FORMAT_R8G8B8A8_SRGB = 43,
	TIF_VK_FORMAT_B8G8R8A8_UNORM = 44,
	TIF_VK_FORMAT_B8G8R8A8_SNORM = 45,
	TIF_VK_FORMAT_B8G8R8A8_USCALED = 46,
	TIF_VK_FORMAT_B8G8R8A8_SSCALED = 47,
	TIF_VK_FORMAT_B8G8R8A8_UINT = 48,
	TIF_VK_FORMAT_B8G8R8A8_SINT = 49,
	TIF_VK_FORMAT_B8G8R8A8_SRGB = 50,
	TIF_VK_FORMAT_A8B8G8R8_UNORM_PACK32 = 51,
	TIF_VK_FORMAT_A8B8G8R8_SNORM_PACK32 = 52,
	TIF_VK_FORMAT_A8B8G8R8_USCALED_PACK32 = 53,
	TIF_VK_FORMAT_A8B8G8R8_SSCALED_PACK32 = 54,
	TIF_VK_FORMAT_A8B8G8R8_UINT_PACK32 = 55,
	TIF_VK_FORMAT_A8B8G8R8_SINT_PACK32 = 56,
	TIF_VK_FORMAT_A8B8G8R8_SRGB_PACK32 = 57,
	TIF_VK_FORMAT_A2R10G10B10_UNORM_PACK32 = 58,
	TIF_VK_FORMAT_A2R10G10B10_SNORM_PACK32 = 59,
	TIF_VK_FORMAT_A2R10G10B10_USCALED_PACK32 = 60,
	TIF_VK_FORMAT_A2R10G10B10_SSCALED_PACK32 = 61,
	TIF_VK_FORMAT_A2R10G10B10_UINT_PACK32 = 62,
	TIF_VK_FORMAT_A2R10G10B10_SINT_PACK32 = 63,
	TIF_VK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64,
	TIF_VK_FORMAT_A2B10G10R10_SNORM_PACK32 = 65,
	TIF_VK_FORMAT_A2B10G10R10_USCALED_PACK32 = 66,
	TIF_VK_FORMAT_A2B10G10R10_SSCALED_PACK32 = 67,
	TIF_VK_FORMAT_A2B10G10R10_UINT_PACK32 = 68,
	TIF_VK_FORMAT_A2B10G10R10_SINT_PACK32 = 69,
	TIF_VK_FORMAT_R16_UNORM = 70,
	TIF_VK_FORMAT_R16_SNORM = 71,
	TIF_VK_FORMAT_R16_USCALED = 72,
	TIF_VK_FORMAT_R16_SSCALED = 73,
	TIF_VK_FORMAT_R16_UINT = 74,
	TIF_VK_FORMAT_R16_SINT = 75,
	TIF_VK_FORMAT_R16_SFLOAT = 76,
	TIF_VK_FORMAT_R16G16_UNORM = 77,
	TIF_VK_FORMAT_R16G16_SNORM = 78,
	TIF_VK_FORMAT_R16G16_USCALED = 79,
	TIF_VK_FORMAT_R16G16_SSCALED = 80,
	TIF_VK_FORMAT_R16G16_UINT = 81,
	TIF_VK_FORMAT_R16G16_SINT = 82,
	TIF_VK_FORMAT_R16G16_SFLOAT = 83,
	TIF_VK_FORMAT_R16G16B16_UNORM = 84,
	TIF_VK_FORMAT_R16G16B16_SNORM = 85,
	TIF_VK_FORMAT_R16G16B16_USCALED = 86,
	TIF_VK_FORMAT_R16G16B16_SSCALED = 87,
	TIF_VK_FORMAT_R16G16B16_UINT = 88,
	TIF_VK_FORMAT_R16G16B16_SINT = 89,
	TIF_VK_FORMAT_R16G16B16_SFLOAT = 90,
	TIF_VK_FORMAT_R16G16B16A16_UNORM = 91,
	TIF_VK_FORMAT_R16G16B16A16_SNORM = 92,
	TIF_VK_FORMAT_R16G16B16A16_USCALED = 93,
	TIF_VK_FORMAT_R16G16B16A16_SSCALED = 94,
	TIF_VK_FORMAT_R16G16B16A16_UINT = 95,
	TIF_VK_FORMAT_R16G16B16A16_SINT = 96,
	TIF_VK_FORMAT_R16G16B16A16_SFLOAT = 97,
	TIF_VK_FORMAT_R32_UINT = 98,
	TIF_VK_FORMAT_R32_SINT = 99,
	TIF_VK_FORMAT_R32_SFLOAT = 100,
	TIF_VK_FORMAT_R32G32_UINT = 101,
	TIF_VK_FORMAT_R32G32_SINT = 102,
	TIF_VK_FORMAT_R32G32_SFLOAT = 103,
	TIF_VK_FORMAT_R32G32B32_UINT = 104,
	TIF_VK_FORMAT_R32G32B32_SINT = 105,
	TIF_VK_FORMAT_R32G32B32_SFLOAT = 106,
	TIF_VK_FORMAT_R32G32B32A32_UINT = 107,
	TIF_VK_FORMAT_R32G32B32A32_SINT = 108,
	TIF_VK_FORMAT_R32G32B32A32_SFLOAT = 109,
	TIF_VK_FORMAT_R64_UINT = 110,
	TIF_VK_FORMAT_R64_SINT = 111,
	TIF_VK_FORMAT_R64_SFLOAT = 112,
	TIF_VK_FORMAT_R64G64_UINT = 113,
	TIF_VK_FORMAT_R64G64_SINT = 114,
	TIF_VK_FORMAT_R64G64_SFLOAT = 115,
	TIF_VK_FORMAT_R64G64B64_UINT = 116,
	TIF_VK_FORMAT_R64G64B64_SINT = 117,
	TIF_VK_FORMAT_R64G64B64_SFLOAT = 118,
	TIF_VK_FORMAT_R64G64B64A64_UINT = 119,
	TIF_VK_FORMAT_R64G64B64A64_SINT = 120,
	TIF_VK_FORMAT_R64G64B64A64_SFLOAT = 121,
	TIF_VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
	TIF_VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123,
	TIF_VK_FORMAT_D16_UNORM = 124,
	TIF_VK_FORMAT_X8_D24_UNORM_PACK32 = 125,
	TIF_VK_FORMAT_D32_SFLOAT = 126,
	TIF_VK_FORMAT_S8_UINT = 127,
	TIF_VK_FORMAT_D16_UNORM_S8_UINT = 128,
	TIF_VK_FORMAT_D24_UNORM_S8_UINT = 129,
	TIF_VK_FORMAT_D32_SFLOAT_S8_UINT = 130,
	TIF_VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
	TIF_VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
	TIF_VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
	TIF_VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
	TIF_VK_FORMAT_BC2_UNORM_BLOCK = 135,
	TIF_VK_FORMAT_BC2_SRGB_BLOCK = 136,
	TIF_VK_FORMAT_BC3_UNORM_BLOCK = 137,
	TIF_VK_FORMAT_BC3_SRGB_BLOCK = 138,
	TIF_VK_FORMAT_BC4_UNORM_BLOCK = 139,
	TIF_VK_FORMAT_BC4_SNORM_BLOCK = 140,
	TIF_VK_FORMAT_BC5_UNORM_BLOCK = 141,
	TIF_VK_FORMAT_BC5_SNORM_BLOCK = 142,
	TIF_VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
	TIF_VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
	TIF_VK_FORMAT_BC7_UNORM_BLOCK = 145,
	TIF_VK_FORMAT_BC7_SRGB_BLOCK = 146,
	TIF_VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
	TIF_VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
	TIF_VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
	TIF_VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
	TIF_VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
	TIF_VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,
	TIF_VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
	TIF_VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
	TIF_VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
	TIF_VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
	TIF_VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
	TIF_VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
	TIF_VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
	TIF_VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
	TIF_VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
	TIF_VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
	TIF_VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
	TIF_VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
	TIF_VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
	TIF_VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
	TIF_VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
	TIF_VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
	TIF_VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
	TIF_VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,
	TIF_VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
	TIF_VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
	TIF_VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
	TIF_VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
	TIF_VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
	TIF_VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
	TIF_VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
	TIF_VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
	TIF_VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
	TIF_VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
	TIF_VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
	TIF_VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
	TIF_VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
	TIF_VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,

	TIF_VK_FORMAT_G8B8G8R8_422_UNORM = 1000156000,
	TIF_VK_FORMAT_B8G8R8G8_422_UNORM = 1000156001,
	TIF_VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM = 1000156002,
	TIF_VK_FORMAT_G8_B8R8_2PLANE_420_UNORM = 1000156003,
	TIF_VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM = 1000156004,
	TIF_VK_FORMAT_G8_B8R8_2PLANE_422_UNORM = 1000156005,
	TIF_VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM = 1000156006,
	TIF_VK_FORMAT_R10X6_UNORM_PACK16 = 1000156007,
	TIF_VK_FORMAT_R10X6G10X6_UNORM_2PACK16 = 1000156008,
	TIF_VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009,
	TIF_VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010,
	TIF_VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011,
	TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
	TIF_VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013,
	TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
	TIF_VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015,
	TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
	TIF_VK_FORMAT_R12X4_UNORM_PACK16 = 1000156017,
	TIF_VK_FORMAT_R12X4G12X4_UNORM_2PACK16 = 1000156018,
	TIF_VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019,
	TIF_VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020,
	TIF_VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021,
	TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
	TIF_VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023,
	TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
	TIF_VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025,
	TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
	TIF_VK_FORMAT_G16B16G16R16_422_UNORM = 1000156027,
	TIF_VK_FORMAT_B16G16R16G16_422_UNORM = 1000156028,
	TIF_VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM = 1000156029,
	TIF_VK_FORMAT_G16_B16R16_2PLANE_420_UNORM = 1000156030,
	TIF_VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM = 1000156031,
	TIF_VK_FORMAT_G16_B16R16_2PLANE_422_UNORM = 1000156032,
	TIF_VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM = 1000156033,
	TIF_VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG = 1000054000,
	TIF_VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG = 1000054001,
	TIF_VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG = 1000054002,
	TIF_VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG = 1000054003,
	TIF_VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004,
	TIF_VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG = 1000054005,
	TIF_VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
	TIF_VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG = 1000054007,
} TinyImageFormat_VkFormat;
#endif // TINYIMAGEFORMAT_VKFORMAT

#define TINYKTX_MEV(x) TKTX_##x = TIF_VK_FORMAT_##x
typedef enum TinyKtx_Format {
	TINYKTX_MEV(UNDEFINED),
	TINYKTX_MEV(R4G4_UNORM_PACK8),
	TINYKTX_MEV(R4G4B4A4_UNORM_PACK16),
	TINYKTX_MEV(B4G4R4A4_UNORM_PACK16),
	TINYKTX_MEV(R5G6B5_UNORM_PACK16),
	TINYKTX_MEV(B5G6R5_UNORM_PACK16),
	TINYKTX_MEV(R5G5B5A1_UNORM_PACK16),
	TINYKTX_MEV(B5G5R5A1_UNORM_PACK16),
	TINYKTX_MEV(A1R5G5B5_UNORM_PACK16),

	TINYKTX_MEV(R8_UNORM),
	TINYKTX_MEV(R8_SNORM),
	TINYKTX_MEV(R8_UINT),
	TINYKTX_MEV(R8_SINT),
	TINYKTX_MEV(R8_SRGB),

	TINYKTX_MEV(R8G8_UNORM),
	TINYKTX_MEV(R8G8_SNORM),
	TINYKTX_MEV(R8G8_UINT),
	TINYKTX_MEV(R8G8_SINT),
	TINYKTX_MEV(R8G8_SRGB),

	TINYKTX_MEV(R8G8B8_UNORM),
	TINYKTX_MEV(R8G8B8_SNORM),
	TINYKTX_MEV(R8G8B8_UINT),
	TINYKTX_MEV(R8G8B8_SINT),
	TINYKTX_MEV(R8G8B8_SRGB),
	TINYKTX_MEV(B8G8R8_UNORM),
	TINYKTX_MEV(B8G8R8_SNORM),
	TINYKTX_MEV(B8G8R8_UINT),
	TINYKTX_MEV(B8G8R8_SINT),
	TINYKTX_MEV(B8G8R8_SRGB),

	TINYKTX_MEV(R8G8B8A8_UNORM),
	TINYKTX_MEV(R8G8B8A8_SNORM),
	TINYKTX_MEV(R8G8B8A8_UINT),
	TINYKTX_MEV(R8G8B8A8_SINT),
	TINYKTX_MEV(R8G8B8A8_SRGB),
	TINYKTX_MEV(B8G8R8A8_UNORM),
	TINYKTX_MEV(B8G8R8A8_SNORM),
	TINYKTX_MEV(B8G8R8A8_UINT),
	TINYKTX_MEV(B8G8R8A8_SINT),
	TINYKTX_MEV(B8G8R8A8_SRGB),

	TINYKTX_MEV(A8B8G8R8_UNORM_PACK32),
	TINYKTX_MEV(A8B8G8R8_SNORM_PACK32),
	TINYKTX_MEV(A8B8G8R8_UINT_PACK32),
	TINYKTX_MEV(A8B8G8R8_SINT_PACK32),
	TINYKTX_MEV(A8B8G8R8_SRGB_PACK32),

	TINYKTX_MEV(E5B9G9R9_UFLOAT_PACK32),
	TINYKTX_MEV(A2R10G10B10_UNORM_PACK32),
	TINYKTX_MEV(A2R10G10B10_UINT_PACK32),
	TINYKTX_MEV(A2B10G10R10_UNORM_PACK32),
	TINYKTX_MEV(A2B10G10R10_UINT_PACK32),
	TINYKTX_MEV(B10G11R11_UFLOAT_PACK32),

	TINYKTX_MEV(R16_UNORM),
	TINYKTX_MEV(R16_SNORM),
	TINYKTX_MEV(R16_UINT),
	TINYKTX_MEV(R16_SINT),
	TINYKTX_MEV(R16_SFLOAT),
	TINYKTX_MEV(R16G16_UNORM),
	TINYKTX_MEV(R16G16_SNORM),
	TINYKTX_MEV(R16G16_UINT),
	TINYKTX_MEV(R16G16_SINT),
	TINYKTX_MEV(R16G16_SFLOAT),
	TINYKTX_MEV(R16G16B16_UNORM),
	TINYKTX_MEV(R16G16B16_SNORM),
	TINYKTX_MEV(R16G16B16_UINT),
	TINYKTX_MEV(R16G16B16_SINT),
	TINYKTX_MEV(R16G16B16_SFLOAT),
	TINYKTX_MEV(R16G16B16A16_UNORM),
	TINYKTX_MEV(R16G16B16A16_SNORM),
	TINYKTX_MEV(R16G16B16A16_UINT),
	TINYKTX_MEV(R16G16B16A16_SINT),
	TINYKTX_MEV(R16G16B16A16_SFLOAT),
	TINYKTX_MEV(R32_UINT),
	TINYKTX_MEV(R32_SINT),
	TINYKTX_MEV(R32_SFLOAT),
	TINYKTX_MEV(R32G32_UINT),
	TINYKTX_MEV(R32G32_SINT),
	TINYKTX_MEV(R32G32_SFLOAT),
	TINYKTX_MEV(R32G32B32_UINT),
	TINYKTX_MEV(R32G32B32_SINT),
	TINYKTX_MEV(R32G32B32_SFLOAT),
	TINYKTX_MEV(R32G32B32A32_UINT),
	TINYKTX_MEV(R32G32B32A32_SINT),
	TINYKTX_MEV(R32G32B32A32_SFLOAT),

	TINYKTX_MEV(BC1_RGB_UNORM_BLOCK),
	TINYKTX_MEV(BC1_RGB_SRGB_BLOCK),
	TINYKTX_MEV(BC1_RGBA_UNORM_BLOCK),
	TINYKTX_MEV(BC1_RGBA_SRGB_BLOCK),
	TINYKTX_MEV(BC2_UNORM_BLOCK),
	TINYKTX_MEV(BC2_SRGB_BLOCK),
	TINYKTX_MEV(BC3_UNORM_BLOCK),
	TINYKTX_MEV(BC3_SRGB_BLOCK),
	TINYKTX_MEV(BC4_UNORM_BLOCK),
	TINYKTX_MEV(BC4_SNORM_BLOCK),
	TINYKTX_MEV(BC5_UNORM_BLOCK),
	TINYKTX_MEV(BC5_SNORM_BLOCK),
	TINYKTX_MEV(BC6H_UFLOAT_BLOCK),
	TINYKTX_MEV(BC6H_SFLOAT_BLOCK),
	TINYKTX_MEV(BC7_UNORM_BLOCK),
	TINYKTX_MEV(BC7_SRGB_BLOCK),

	TINYKTX_MEV(ETC2_R8G8B8_UNORM_BLOCK),
	TINYKTX_MEV(ETC2_R8G8B8A1_UNORM_BLOCK),
	TINYKTX_MEV(ETC2_R8G8B8A8_UNORM_BLOCK),
	TINYKTX_MEV(ETC2_R8G8B8_SRGB_BLOCK),
	TINYKTX_MEV(ETC2_R8G8B8A1_SRGB_BLOCK),
	TINYKTX_MEV(ETC2_R8G8B8A8_SRGB_BLOCK),
	TINYKTX_MEV(EAC_R11_UNORM_BLOCK),
	TINYKTX_MEV(EAC_R11G11_UNORM_BLOCK),
	TINYKTX_MEV(EAC_R11_SNORM_BLOCK),
	TINYKTX_MEV(EAC_R11G11_SNORM_BLOCK),

	TKTX_PVR_2BPP_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
	TKTX_PVR_2BPPA_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
	TKTX_PVR_4BPP_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
	TKTX_PVR_4BPPA_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
	TKTX_PVR_2BPP_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
	TKTX_PVR_2BPPA_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
	TKTX_PVR_4BPP_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
	TKTX_PVR_4BPPA_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,

	TINYKTX_MEV(ASTC_4x4_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_4x4_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_5x4_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_5x4_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_5x5_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_5x5_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_6x5_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_6x5_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_6x6_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_6x6_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_8x5_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_8x5_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_8x6_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_8x6_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_8x8_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_8x8_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_10x5_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_10x5_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_10x6_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_10x6_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_10x8_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_10x8_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_10x10_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_10x10_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_12x10_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_12x10_SRGB_BLOCK),
	TINYKTX_MEV(ASTC_12x12_UNORM_BLOCK),
	TINYKTX_MEV(ASTC_12x12_SRGB_BLOCK),

} TinyKtx_Format;
#undef TINYKTX_MEV

// tiny_imageformat/format needs included before tinyktx.h for this functionality
#ifdef TINYIMAGEFORMAT_BASE_H_
TinyImageFormat TinyImageFormat_FromTinyKtxFormat(TinyKtx_Format format);
TinyKtx_Format TinyImageFormat_ToTinyKtxFormat(TinyImageFormat format);
#endif

TinyKtx_Format TinyKtx_GetFormat(TinyKtx_ContextHandle handle);
bool TinyKtx_CrackFormatToGL(TinyKtx_Format format, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t* typesize);
bool TinyKtx_WriteImage(TinyKtx_WriteCallbacks const *callbacks,
												void *user,
												uint32_t width,
												uint32_t height,
												uint32_t depth,
												uint32_t slices,
												uint32_t mipmaplevels,
												TinyKtx_Format format,
												bool cubemap,
												uint32_t const *mipmapsizes,
												void const **mipmaps);
// GL types
#define TINYKTX_GL_TYPE_COMPRESSED                      0x0
#define TINYKTX_GL_TYPE_BYTE                            0x1400
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE                    0x1401
#define TINYKTX_GL_TYPE_SHORT                            0x1402
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT                  0x1403
#define TINYKTX_GL_TYPE_INT                              0x1404
#define TINYKTX_GL_TYPE_UNSIGNED_INT                    0x1405
#define TINYKTX_GL_TYPE_FLOAT                            0x1406
#define TINYKTX_GL_TYPE_DOUBLE                          0x140A
#define TINYKTX_GL_TYPE_HALF_FLOAT                      0x140B
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE_3_3_2              0x8032
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4          0x8033
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1          0x8034
#define TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8            0x8035
#define TINYKTX_GL_TYPE_UNSIGNED_INT_10_10_10_2          0x8036
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE_2_3_3_REV          0x8362
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5            0x8363
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5_REV        0x8364
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4_REV      0x8365
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV      0x8366
#define TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8_REV        0x8367
#define TINYKTX_GL_TYPE_UNSIGNED_INT_2_10_10_10_REV      0x8368
#define TINYKTX_GL_TYPE_UNSIGNED_INT_24_8                0x84FA
#define TINYKTX_GL_TYPE_UNSIGNED_INT_5_9_9_9_REV        0x8C3E
#define TINYKTX_GL_TYPE_UNSIGNED_INT_10F_11F_11F_REV    0x8C3B
#define TINYKTX_GL_TYPE_FLOAT_32_UNSIGNED_INT_24_8_REV  0x8DAD

// formats
#define TINYKTX_GL_FORMAT_RED                              0x1903
#define TINYKTX_GL_FORMAT_GREEN                            0x1904
#define TINYKTX_GL_FORMAT_BLUE                            0x1905
#define TINYKTX_GL_FORMAT_ALPHA                            0x1906
#define TINYKTX_GL_FORMAT_RGB                              0x1907
#define TINYKTX_GL_FORMAT_RGBA                            0x1908
#define TINYKTX_GL_FORMAT_LUMINANCE                        0x1909
#define TINYKTX_GL_FORMAT_LUMINANCE_ALPHA                  0x190A
#define TINYKTX_GL_FORMAT_ABGR                            0x8000
#define TINYKTX_GL_FORMAT_INTENSITY                        0x8049
#define TINYKTX_GL_FORMAT_BGR                              0x80E0
#define TINYKTX_GL_FORMAT_BGRA                            0x80E1
#define TINYKTX_GL_FORMAT_RG                              0x8227
#define TINYKTX_GL_FORMAT_RG_INTEGER                      0x8228
#define TINYKTX_GL_FORMAT_SRGB                            0x8C40
#define TINYKTX_GL_FORMAT_SRGB_ALPHA                      0x8C42
#define TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA                0x8C44
#define TINYKTX_GL_FORMAT_SLUMINANCE                      0x8C46
#define TINYKTX_GL_FORMAT_RED_INTEGER                      0x8D94
#define TINYKTX_GL_FORMAT_GREEN_INTEGER                    0x8D95
#define TINYKTX_GL_FORMAT_BLUE_INTEGER                    0x8D96
#define TINYKTX_GL_FORMAT_ALPHA_INTEGER                    0x8D97
#define TINYKTX_GL_FORMAT_RGB_INTEGER                      0x8D98
#define TINYKTX_GL_FORMAT_RGBA_INTEGER                    0x8D99
#define TINYKTX_GL_FORMAT_BGR_INTEGER                      0x8D9A
#define TINYKTX_GL_FORMAT_BGRA_INTEGER                    0x8D9B
#define TINYKTX_GL_FORMAT_RED_SNORM                        0x8F90
#define TINYKTX_GL_FORMAT_RG_SNORM                        0x8F91
#define TINYKTX_GL_FORMAT_RGB_SNORM                        0x8F92
#define TINYKTX_GL_FORMAT_RGBA_SNORM                      0x8F93

#define TINYKTX_GL_INTFORMAT_ALPHA4                          0x803B
#define TINYKTX_GL_INTFORMAT_ALPHA8                          0x803C
#define TINYKTX_GL_INTFORMAT_ALPHA12                          0x803D
#define TINYKTX_GL_INTFORMAT_ALPHA16                          0x803E
#define TINYKTX_GL_INTFORMAT_LUMINANCE4                      0x803F
#define TINYKTX_GL_INTFORMAT_LUMINANCE8                      0x8040
#define TINYKTX_GL_INTFORMAT_LUMINANCE12                      0x8041
#define TINYKTX_GL_INTFORMAT_LUMINANCE16                      0x8042
#define TINYKTX_GL_INTFORMAT_LUMINANCE4_ALPHA4                0x8043
#define TINYKTX_GL_INTFORMAT_LUMINANCE6_ALPHA2                0x8044
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8                0x8045
#define TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA4              0x8046
#define TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA12              0x8047
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16              0x8048
#define TINYKTX_GL_INTFORMAT_INTENSITY4                      0x804A
#define TINYKTX_GL_INTFORMAT_INTENSITY8                      0x804B
#define TINYKTX_GL_INTFORMAT_INTENSITY12                      0x804C
#define TINYKTX_GL_INTFORMAT_INTENSITY16                      0x804D
#define TINYKTX_GL_INTFORMAT_RGB2                            0x804E
#define TINYKTX_GL_INTFORMAT_RGB4                            0x804F
#define TINYKTX_GL_INTFORMAT_RGB5                            0x8050
#define TINYKTX_GL_INTFORMAT_RGB8                              0x8051
#define TINYKTX_GL_INTFORMAT_RGB10                            0x8052
#define TINYKTX_GL_INTFORMAT_RGB12                            0x8053
#define TINYKTX_GL_INTFORMAT_RGB16                            0x8054
#define TINYKTX_GL_INTFORMAT_RGBA2                            0x8055
#define TINYKTX_GL_INTFORMAT_RGBA4                            0x8056
#define TINYKTX_GL_INTFORMAT_RGB5_A1                          0x8057
#define TINYKTX_GL_INTFORMAT_RGBA8                            0x8058
#define TINYKTX_GL_INTFORMAT_RGB10_A2                        0x8059
#define TINYKTX_GL_INTFORMAT_RGBA12                          0x805A
#define TINYKTX_GL_INTFORMAT_RGBA16                          0x805B
#define TINYKTX_GL_INTFORMAT_R8                              0x8229
#define TINYKTX_GL_INTFORMAT_R16                              0x822A
#define TINYKTX_GL_INTFORMAT_RG8                              0x822B
#define TINYKTX_GL_INTFORMAT_RG16                            0x822C
#define TINYKTX_GL_INTFORMAT_R16F                            0x822D
#define TINYKTX_GL_INTFORMAT_R32F                            0x822E
#define TINYKTX_GL_INTFORMAT_RG16F                            0x822F
#define TINYKTX_GL_INTFORMAT_RG32F                            0x8230
#define TINYKTX_GL_INTFORMAT_R8I                              0x8231
#define TINYKTX_GL_INTFORMAT_R8UI                            0x8232
#define TINYKTX_GL_INTFORMAT_R16I                            0x8233
#define TINYKTX_GL_INTFORMAT_R16UI                            0x8234
#define TINYKTX_GL_INTFORMAT_R32I                            0x8235
#define TINYKTX_GL_INTFORMAT_R32UI                            0x8236
#define TINYKTX_GL_INTFORMAT_RG8I                            0x8237
#define TINYKTX_GL_INTFORMAT_RG8UI                            0x8238
#define TINYKTX_GL_INTFORMAT_RG16I                            0x8239
#define TINYKTX_GL_INTFORMAT_RG16UI                          0x823A
#define TINYKTX_GL_INTFORMAT_RG32I                            0x823B
#define TINYKTX_GL_INTFORMAT_RG32UI                          0x823C
#define TINYKTX_GL_INTFORMAT_RGBA32F                          0x8814
#define TINYKTX_GL_INTFORMAT_RGB32F                          0x8815
#define TINYKTX_GL_INTFORMAT_RGBA16F                          0x881A
#define TINYKTX_GL_INTFORMAT_RGB16F                          0x881B
#define TINYKTX_GL_INTFORMAT_R11F_G11F_B10F                  0x8C3A
#define TINYKTX_GL_INTFORMAT_UNSIGNED_INT_10F_11F_11F_REV      0x8C3B
#define TINYKTX_GL_INTFORMAT_RGB9_E5                          0x8C3D
#define TINYKTX_GL_INTFORMAT_SRGB8                            0x8C41
#define TINYKTX_GL_INTFORMAT_SRGB8_ALPHA8                      0x8C43
#define TINYKTX_GL_INTFORMAT_SLUMINANCE8_ALPHA8              0x8C45
#define TINYKTX_GL_INTFORMAT_SLUMINANCE8                      0x8C47
#define TINYKTX_GL_INTFORMAT_RGB565                          0x8D62
#define TINYKTX_GL_INTFORMAT_RGBA32UI                        0x8D70
#define TINYKTX_GL_INTFORMAT_RGB32UI                          0x8D71
#define TINYKTX_GL_INTFORMAT_RGBA16UI                        0x8D76
#define TINYKTX_GL_INTFORMAT_RGB16UI                          0x8D77
#define TINYKTX_GL_INTFORMAT_RGBA8UI                          0x8D7C
#define TINYKTX_GL_INTFORMAT_RGB8UI                          0x8D7D
#define TINYKTX_GL_INTFORMAT_RGBA32I                          0x8D82
#define TINYKTX_GL_INTFORMAT_RGB32I                          0x8D83
#define TINYKTX_GL_INTFORMAT_RGBA16I                          0x8D88
#define TINYKTX_GL_INTFORMAT_RGB16I                          0x8D89
#define TINYKTX_GL_INTFORMAT_RGBA8I                          0x8D8E
#define TINYKTX_GL_INTFORMAT_RGB8I                            0x8D8F
#define TINYKTX_GL_INTFORMAT_FLOAT_32_UNSIGNED_INT_24_8_REV  0x8DAD
#define TINYKTX_GL_INTFORMAT_R8_SNORM                        0x8F94
#define TINYKTX_GL_INTFORMAT_RG8_SNORM                        0x8F95
#define TINYKTX_GL_INTFORMAT_RGB8_SNORM                      0x8F96
#define TINYKTX_GL_INTFORMAT_RGBA8_SNORM                      0x8F97
#define TINYKTX_GL_INTFORMAT_R16_SNORM                        0x8F98
#define TINYKTX_GL_INTFORMAT_RG16_SNORM                      0x8F99
#define TINYKTX_GL_INTFORMAT_RGB16_SNORM                      0x8F9A
#define TINYKTX_GL_INTFORMAT_RGBA16_SNORM                    0x8F9B
#define TINYKTX_GL_INTFORMAT_ALPHA8_SNORM                    0x9014
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_SNORM                0x9015
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8_SNORM          0x9016
#define TINYKTX_GL_INTFORMAT_INTENSITY8_SNORM                0x9017
#define TINYKTX_GL_INTFORMAT_ALPHA16_SNORM                    0x9018
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_SNORM                0x9019
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16_SNORM        0x901A
#define TINYKTX_GL_INTFORMAT_INTENSITY16_SNORM                0x901B

#define TINYKTX_GL_PALETTE4_RGB8_OES              0x8B90
#define TINYKTX_GL_PALETTE4_RGBA8_OES             0x8B91
#define TINYKTX_GL_PALETTE4_R5_G6_B5_OES          0x8B92
#define TINYKTX_GL_PALETTE4_RGBA4_OES             0x8B93
#define TINYKTX_GL_PALETTE4_RGB5_A1_OES           0x8B94
#define TINYKTX_GL_PALETTE8_RGB8_OES              0x8B95
#define TINYKTX_GL_PALETTE8_RGBA8_OES             0x8B96
#define TINYKTX_GL_PALETTE8_R5_G6_B5_OES          0x8B97
#define TINYKTX_GL_PALETTE8_RGBA4_OES             0x8B98
#define TINYKTX_GL_PALETTE8_RGB5_A1_OES           0x8B99

// compressed formats

#define TINYKTX_GL_COMPRESSED_RGB_S3TC_DXT1                  	0x83F0
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT1                  0x83F1
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT3                  0x83F2
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT5                  0x83F3
#define TINYKTX_GL_COMPRESSED_3DC_X_AMD                       0x87F9
#define TINYKTX_GL_COMPRESSED_3DC_XY_AMD                      0x87FA
#define TINYKTX_GL_COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA    	0x87EE
#define TINYKTX_GL_COMPRESSED_SRGB_PVRTC_2BPPV1               0x8A54
#define TINYKTX_GL_COMPRESSED_SRGB_PVRTC_4BPPV1               0x8A55
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1         0x8A56
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1         0x8A57
#define TINYKTX_GL_COMPRESSED_RGB_PVRTC_4BPPV1                0x8C00
#define TINYKTX_GL_COMPRESSED_RGB_PVRTC_2BPPV1                0x8C01
#define TINYKTX_GL_COMPRESSED_RGBA_PVRTC_4BPPV1               0x8C02
#define TINYKTX_GL_COMPRESSED_RGBA_PVRTC_2BPPV1               0x8C03
#define TINYKTX_GL_COMPRESSED_SRGB_S3TC_DXT1                  0x8C4C
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1            0x8C4D
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3            0x8C4E
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5            0x8C4F
#define TINYKTX_GL_COMPRESSED_LUMINANCE_LATC1                	0x8C70
#define TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_LATC1          0x8C71
#define TINYKTX_GL_COMPRESSED_LUMINANCE_ALPHA_LATC2           0x8C72
#define TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2    0x8C73
#define TINYKTX_GL_COMPRESSED_ATC_RGB                         0x8C92
#define TINYKTX_GL_COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA         0x8C93
#define TINYKTX_GL_COMPRESSED_RED_RGTC1                       0x8DBB
#define TINYKTX_GL_COMPRESSED_SIGNED_RED_RGTC1                0x8DBC
#define TINYKTX_GL_COMPRESSED_RED_GREEN_RGTC2                	0x8DBD
#define TINYKTX_GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2          0x8DBE
#define TINYKTX_GL_COMPRESSED_ETC1_RGB8_OES                   0x8D64
#define TINYKTX_GL_COMPRESSED_RGBA_BPTC_UNORM                	0x8E8C
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM          	0x8E8D
#define TINYKTX_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT          	0x8E8E
#define TINYKTX_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT        	0x8E8F
#define TINYKTX_GL_COMPRESSED_R11_EAC                        	0x9270
#define TINYKTX_GL_COMPRESSED_SIGNED_R11_EAC                  0x9271
#define TINYKTX_GL_COMPRESSED_RG11_EAC                        0x9272
#define TINYKTX_GL_COMPRESSED_SIGNED_RG11_EAC                	0x9273
#define TINYKTX_GL_COMPRESSED_RGB8_ETC2                      	0x9274
#define TINYKTX_GL_COMPRESSED_SRGB8_ETC2                      0x9275
#define TINYKTX_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2   0x9276
#define TINYKTX_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2  0x9277
#define TINYKTX_GL_COMPRESSED_RGBA8_ETC2_EAC                  0x9278
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC           0x9279
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2         0x93F0
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2         0x93F1
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_4x4		   							0x93B0
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x4		              	0x93B1
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x5		              	0x93B2
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x5		              	0x93B3
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x6		              	0x93B4
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x5		              	0x93B5
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x6		              	0x93B6
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x8		              	0x93B7
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x5		              0x93B8
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x6		              0x93B9
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x8		              0x93BA
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x10	            		0x93BB
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x10	            		0x93BC
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x12	            		0x93BD
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4			      0x93D0
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4			      0x93D1
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5			      0x93D2
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5						0x93D3
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6						0x93D4
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5						0x93D5
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6						0x93D6
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8						0x93D7
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5					0x93D8
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6					0x93D9
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8					0x93DA
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10					0x93DB
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10     		0x93DC
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12     		0x93DD

#ifdef TINYKTX_IMPLEMENTATION

typedef struct TinyKtx_Header {
	uint8_t identifier[12];
	uint32_t endianness;
	uint32_t glType;
	uint32_t glTypeSize;
	uint32_t glFormat;
	uint32_t glInternalFormat;
	uint32_t glBaseInternalFormat;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t numberOfArrayElements;
	uint32_t numberOfFaces;
	uint32_t numberOfMipmapLevels;
	uint32_t bytesOfKeyValueData;

} TinyKtx_Header;

typedef struct TinyKtx_KeyValuePair {
	uint32_t size;
} TinyKtx_KeyValuePair; // followed by at least size bytes (aligned to 4)


typedef struct TinyKtx_Context {
	TinyKtx_Callbacks callbacks;
	void *user;
	uint64_t headerPos;
	uint64_t firstImagePos;

	TinyKtx_Header header;

	TinyKtx_KeyValuePair const *keyData;
	bool headerValid;
	bool sameEndian;

	uint32_t mipMapSizes[TINYKTX_MAX_MIPMAPLEVELS];
	uint8_t const *mipmaps[TINYKTX_MAX_MIPMAPLEVELS];

} TinyKtx_Context;

static uint8_t TinyKtx_fileIdentifier[12] = {
		0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static void TinyKtx_NullErrorFunc(void *user, char const *msg) { UNREF_PARAM(user); UNREF_PARAM(msg); }

TinyKtx_ContextHandle TinyKtx_CreateContext(TinyKtx_Callbacks const *callbacks, void *user) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) callbacks->allocFn(user, sizeof(TinyKtx_Context));
	if (ctx == NULL)
		return NULL;

	memset(ctx, 0, sizeof(TinyKtx_Context));
	memcpy(&ctx->callbacks, callbacks, sizeof(TinyKtx_Callbacks));
	ctx->user = user;
	if (ctx->callbacks.errorFn == NULL) {
		ctx->callbacks.errorFn = &TinyKtx_NullErrorFunc;
	}

	if (ctx->callbacks.readFn == NULL) {
		ctx->callbacks.errorFn(user, "TinyKtx must have read callback");
		return NULL;
	}
	if (ctx->callbacks.allocFn == NULL) {
		ctx->callbacks.errorFn(user, "TinyKtx must have alloc callback");
		return NULL;
	}
	if (ctx->callbacks.freeFn == NULL) {
		ctx->callbacks.errorFn(user, "TinyKtx must have free callback");
		return NULL;
	}
	if (ctx->callbacks.seekFn == NULL) {
		ctx->callbacks.errorFn(user, "TinyKtx must have seek callback");
		return NULL;
	}
	if (ctx->callbacks.tellFn == NULL) {
		ctx->callbacks.errorFn(user, "TinyKtx must have tell callback");
		return NULL;
	}

	TinyKtx_Reset(ctx);

	return ctx;
}

void TinyKtx_DestroyContext(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return;
	TinyKtx_Reset(handle);

	ctx->callbacks.freeFn(ctx->user, ctx);
}

void TinyKtx_Reset(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return;

	// backup user provided callbacks and data
	TinyKtx_Callbacks callbacks;
	memcpy(&callbacks, &ctx->callbacks, sizeof(TinyKtx_Callbacks));
	void *user = ctx->user;

	// free memory of sub data
	if (ctx->keyData != NULL) {
		callbacks.freeFn(user, (void *) ctx->keyData);
	}

	for (int i = 0; i < TINYKTX_MAX_MIPMAPLEVELS; ++i) {
		if (ctx->mipmaps[i] != NULL) {
			callbacks.freeFn(user, (void *) ctx->mipmaps[i]);
		}
	}

	// reset to default state
	memset(ctx, 0, sizeof(TinyKtx_Context));
	memcpy(&ctx->callbacks, &callbacks, sizeof(TinyKtx_Callbacks));
	ctx->user = user;

}


bool TinyKtx_ReadHeader(TinyKtx_ContextHandle handle) {

	static uint32_t const sameEndianDecider = 0x04030201;
	static uint32_t const differentEndianDecider = 0x01020304;

	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;

	ctx->headerPos = ctx->callbacks.tellFn(ctx->user);
	ctx->callbacks.readFn(ctx->user, &ctx->header, sizeof(TinyKtx_Header));

	if (memcmp(&ctx->header.identifier, TinyKtx_fileIdentifier, 12) != 0) {
		ctx->callbacks.errorFn(ctx->user, "Not a KTX file or corrupted as identified isn't valid");
		return false;
	}

	if (ctx->header.endianness == sameEndianDecider) {
		ctx->sameEndian = true;
	} else if (ctx->header.endianness == differentEndianDecider) {
		ctx->sameEndian = false;
	} else {
		// corrupt or mid endian?
		ctx->callbacks.errorFn(ctx->user, "Endian Error");
		return false;
	}

	if (ctx->header.numberOfFaces != 1 && ctx->header.numberOfFaces != 6) {
		ctx->callbacks.errorFn(ctx->user, "no. of Faces must be 1 or 6");
		return false;
	}

	ctx->keyData = (TinyKtx_KeyValuePair const *) ctx->callbacks.allocFn(ctx->user, ctx->header.bytesOfKeyValueData);
	ctx->callbacks.readFn(ctx->user, (void *) ctx->keyData, ctx->header.bytesOfKeyValueData);

	ctx->firstImagePos = ctx->callbacks.tellFn(ctx->user);

	ctx->headerValid = true;
	return true;
}

bool TinyKtx_GetValue(TinyKtx_ContextHandle handle, char const *key, void const **value) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	if (ctx->keyData == NULL) {
		ctx->callbacks.errorFn(ctx->user, "No key value data in this KTX");
		return false;
	}

	TinyKtx_KeyValuePair const *curKey = ctx->keyData;
	while (((uint8_t *) curKey - (uint8_t *) ctx->keyData) < (int32_t)ctx->header.bytesOfKeyValueData) {
		char const *kvp = (char const *) curKey;

		if (strcmp(kvp, key) == 0) {
			size_t sl = strlen(kvp);
			*value = (void const *) (kvp + sl);
			return true;
		}
		curKey = curKey + ((curKey->size + 3u) & ~3u);
	}
	return false;
}

bool TinyKtx_Is1D(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}
	return (ctx->header.pixelHeight <= 1) && (ctx->header.pixelDepth <= 1 );
}
bool TinyKtx_Is2D(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return (ctx->header.pixelHeight > 1 && ctx->header.pixelDepth <= 1);
}
bool TinyKtx_Is3D(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return (ctx->header.pixelHeight > 1 && ctx->header.pixelDepth > 1);
}

bool TinyKtx_IsCubemap(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return (ctx->header.numberOfFaces == 6);
}
bool TinyKtx_IsArray(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return (ctx->header.numberOfArrayElements > 1);
}

bool TinyKtx_Dimensions(TinyKtx_ContextHandle handle,
												uint32_t *width,
												uint32_t *height,
												uint32_t *depth,
												uint32_t *slices) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	if (width)
		*width = ctx->header.pixelWidth;
	if (height)
		*height = ctx->header.pixelWidth;
	if (depth)
		*depth = ctx->header.pixelDepth;
	if (slices)
		*slices = ctx->header.numberOfArrayElements;
	return true;
}

uint32_t TinyKtx_Width(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}
	return ctx->header.pixelWidth;

}

uint32_t TinyKtx_Height(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}

	return ctx->header.pixelHeight;
}

uint32_t TinyKtx_Depth(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}
	return ctx->header.pixelDepth;
}

uint32_t TinyKtx_ArraySlices(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}

	return ctx->header.numberOfArrayElements;
}

uint32_t TinyKtx_NumberOfMipmaps(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}

	return ctx->header.numberOfMipmapLevels ? ctx->header.numberOfMipmapLevels : 1;
}

bool TinyKtx_NeedsGenerationOfMipmaps(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return ctx->header.numberOfMipmapLevels == 0;
}

bool TinyKtx_NeedsEndianCorrecting(TinyKtx_ContextHandle handle) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	return ctx->sameEndian == false;
}

bool TinyKtx_GetFormatGL(TinyKtx_ContextHandle handle, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t* typesize, uint32_t* glbaseinternalformat) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	*glformat = ctx->header.glFormat;
	*gltype = ctx->header.glType;
	*glinternalformat = ctx->header.glInternalFormat;
	*glbaseinternalformat = ctx->header.glBaseInternalFormat;
	*typesize = ctx->header.glTypeSize;

	return true;
}

static uint32_t TinyKtx_imageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel, bool seekLast) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;

	if (mipmaplevel >= ctx->header.numberOfMipmapLevels) {
		ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
		return 0;
	}
	if (mipmaplevel >= TINYKTX_MAX_MIPMAPLEVELS) {
		ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
		return 0;
	}
	if (!seekLast && ctx->mipMapSizes[mipmaplevel] != 0)
		return ctx->mipMapSizes[mipmaplevel];

	uint64_t currentOffset = ctx->firstImagePos;
	for (uint32_t i = 0; i <= mipmaplevel; ++i) {
		uint32_t size;
		// if we have already read this mipmaps size use it
		if (ctx->mipMapSizes[i] != 0) {
			size = ctx->mipMapSizes[i];
			if (seekLast && i == mipmaplevel) {
				ctx->callbacks.seekFn(ctx->user, currentOffset + sizeof(uint32_t));
			}
		} else {
			// otherwise seek and read it
			ctx->callbacks.seekFn(ctx->user, currentOffset);
			size_t readchk = ctx->callbacks.readFn(ctx->user, &size, sizeof(uint32_t));
			if(readchk != 4) {
				ctx->callbacks.errorFn(ctx->user, "Reading image size error");
				return 0;
			}
			// so in the really small print KTX v1 states GL_UNPACK_ALIGNMENT = 4
			// which PVR Texture Tool and I missed. It means pad to 1, 2, 4, 8
			// note 3 or 6 bytes are rounded up.
			// we rely on the loader setting this right, we should handle file with
			// it not to standard but its really the level up that has to do this

			if (ctx->header.numberOfFaces == 6 && ctx->header.numberOfArrayElements == 0) {
				size = ((size + 3u) & ~3u) * 6; // face padding and 6 faces
			}

			ctx->mipMapSizes[i] = size;
		}
		currentOffset += (size + sizeof(uint32_t) + 3u) & ~3u; // size + mip padding
	}

	return ctx->mipMapSizes[mipmaplevel];
}

uint32_t TinyKtx_ImageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL) return 0;

	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}
	return TinyKtx_imageSize(handle, mipmaplevel, false);
}

void const *TinyKtx_ImageRawData(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return NULL;

	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return NULL;
	}

	if (mipmaplevel >= ctx->header.numberOfMipmapLevels) {
		ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
		return NULL;
	}

	if (mipmaplevel >= TINYKTX_MAX_MIPMAPLEVELS) {
		ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
		return NULL;
	}

	if (ctx->mipmaps[mipmaplevel] != NULL)
		return ctx->mipmaps[mipmaplevel];

	uint32_t size = TinyKtx_imageSize(handle, mipmaplevel, true);
	if (size == 0)
		return NULL;

	ctx->mipmaps[mipmaplevel] = (uint8_t const*) ctx->callbacks.allocFn(ctx->user, size);
	if (ctx->mipmaps[mipmaplevel]) {
		ctx->callbacks.readFn(ctx->user, (void *) ctx->mipmaps[mipmaplevel], size);
	}

	return ctx->mipmaps[mipmaplevel];
}

#define FT(fmt, type, intfmt, size) *glformat = TINYKTX_GL_FORMAT_##fmt; \
                                    *gltype = TINYKTX_GL_TYPE_##type; \
                                    *glinternalformat = TINYKTX_GL_INTFORMAT_##intfmt; \
                                    *typesize = size; \
                                    return true;
#define FTC(fmt, intfmt) *glformat = TINYKTX_GL_FORMAT_##fmt; \
                                    *gltype = TINYKTX_GL_TYPE_COMPRESSED; \
                                    *glinternalformat = TINYKTX_GL_COMPRESSED_##intfmt; \
                                    *typesize = 1; \
                                    return true;

bool TinyKtx_CrackFormatToGL(TinyKtx_Format format,
														 uint32_t *glformat,
														 uint32_t *gltype,
														 uint32_t *glinternalformat,
														 uint32_t *typesize) {
	switch (format) {
	case TKTX_R4G4_UNORM_PACK8: FT(RG, UNSIGNED_SHORT_4_4_4_4, RGB4, 1)
	case TKTX_R4G4B4A4_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_4_4_4_4, RGBA4, 2)

	case TKTX_B4G4R4A4_UNORM_PACK16: FT(BGRA, UNSIGNED_SHORT_4_4_4_4_REV, RGBA4, 2)

	case TKTX_R5G6B5_UNORM_PACK16: FT(RGB, UNSIGNED_SHORT_5_6_5_REV, RGB565, 2)
	case TKTX_B5G6R5_UNORM_PACK16: FT(BGR, UNSIGNED_SHORT_5_6_5, RGB565, 2)

	case TKTX_R5G5B5A1_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_5_5_5_1, RGB5_A1, 2)
	case TKTX_A1R5G5B5_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_1_5_5_5_REV, RGB5_A1, 2)
	case TKTX_B5G5R5A1_UNORM_PACK16: FT(BGRA, UNSIGNED_SHORT_5_5_5_1, RGB5_A1, 2)

	case TKTX_A2R10G10B10_UNORM_PACK32: FT(BGRA, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
	case TKTX_A2R10G10B10_UINT_PACK32: FT(BGRA_INTEGER, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
	case TKTX_A2B10G10R10_UNORM_PACK32: FT(RGBA, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
	case TKTX_A2B10G10R10_UINT_PACK32: FT(RGBA_INTEGER, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)

	case TKTX_R8_UNORM: FT(RED, UNSIGNED_BYTE, R8, 1)
	case TKTX_R8_SNORM: FT(RED, BYTE, R8_SNORM, 1)
	case TKTX_R8_UINT: FT(RED_INTEGER, UNSIGNED_BYTE, R8UI, 1)
	case TKTX_R8_SINT: FT(RED_INTEGER, BYTE, R8I, 1)
	case TKTX_R8_SRGB: FT(SLUMINANCE, UNSIGNED_BYTE, SRGB8, 1)

	case TKTX_R8G8_UNORM: FT(RG, UNSIGNED_BYTE, RG8, 1)
	case TKTX_R8G8_SNORM: FT(RG, BYTE, RG8_SNORM, 1)
	case TKTX_R8G8_UINT: FT(RG_INTEGER, UNSIGNED_BYTE, RG8UI, 1)
	case TKTX_R8G8_SINT: FT(RG_INTEGER, BYTE, RG8I, 1)
	case TKTX_R8G8_SRGB: FT(SLUMINANCE_ALPHA, UNSIGNED_BYTE, SRGB8, 1)

	case TKTX_R8G8B8_UNORM: FT(RGB, UNSIGNED_BYTE, RGB8, 1)
	case TKTX_R8G8B8_SNORM: FT(RGB, BYTE, RGB8_SNORM, 1)
	case TKTX_R8G8B8_UINT: FT(RGB_INTEGER, UNSIGNED_BYTE, RGB8UI, 1)
	case TKTX_R8G8B8_SINT: FT(RGB_INTEGER, BYTE, RGB8I, 1)
	case TKTX_R8G8B8_SRGB: FT(SRGB, UNSIGNED_BYTE, SRGB8, 1)

	case TKTX_B8G8R8_UNORM: FT(BGR, UNSIGNED_BYTE, RGB8, 1)
	case TKTX_B8G8R8_SNORM: FT(BGR, BYTE, RGB8_SNORM, 1)
	case TKTX_B8G8R8_UINT: FT(BGR_INTEGER, UNSIGNED_BYTE, RGB8UI, 1)
	case TKTX_B8G8R8_SINT: FT(BGR_INTEGER, BYTE, RGB8I, 1)
	case TKTX_B8G8R8_SRGB: FT(BGR, UNSIGNED_BYTE, SRGB8, 1)

	case TKTX_R8G8B8A8_UNORM:FT(RGBA, UNSIGNED_BYTE, RGBA8, 1)
	case TKTX_R8G8B8A8_SNORM:FT(RGBA, BYTE, RGBA8_SNORM, 1)
	case TKTX_R8G8B8A8_UINT: FT(RGBA_INTEGER, UNSIGNED_BYTE, RGBA8UI, 1)
	case TKTX_R8G8B8A8_SINT: FT(RGBA_INTEGER, BYTE, RGBA8I, 1)
	case TKTX_R8G8B8A8_SRGB: FT(SRGB_ALPHA, UNSIGNED_BYTE, SRGB8_ALPHA8, 1)

	case TKTX_B8G8R8A8_UNORM: FT(BGRA, UNSIGNED_BYTE, RGBA8, 1)
	case TKTX_B8G8R8A8_SNORM: FT(BGRA, BYTE, RGBA8_SNORM, 1)
	case TKTX_B8G8R8A8_UINT: FT(BGRA_INTEGER, UNSIGNED_BYTE, RGBA8UI, 1)
	case TKTX_B8G8R8A8_SINT: FT(BGRA_INTEGER, BYTE, RGBA8I, 1)
	case TKTX_B8G8R8A8_SRGB: FT(BGRA, UNSIGNED_BYTE, SRGB8, 1)

	case TKTX_E5B9G9R9_UFLOAT_PACK32: FT(BGR, UNSIGNED_INT_5_9_9_9_REV, RGB9_E5, 4);
	case TKTX_A8B8G8R8_UNORM_PACK32: FT(ABGR, UNSIGNED_BYTE, RGBA8, 1)
	case TKTX_A8B8G8R8_SNORM_PACK32: FT(ABGR, BYTE, RGBA8, 1)
	case TKTX_A8B8G8R8_UINT_PACK32: FT(ABGR, UNSIGNED_BYTE, RGBA8UI, 1)
	case TKTX_A8B8G8R8_SINT_PACK32: FT(ABGR, BYTE, RGBA8I, 1)
	case TKTX_A8B8G8R8_SRGB_PACK32: FT(ABGR, UNSIGNED_BYTE, SRGB8, 1)
	case TKTX_B10G11R11_UFLOAT_PACK32: FT(BGR, UNSIGNED_INT_10F_11F_11F_REV, R11F_G11F_B10F, 4)

	case TKTX_R16_UNORM: FT(RED, UNSIGNED_SHORT, R16, 2)
	case TKTX_R16_SNORM: FT(RED, SHORT, R16_SNORM, 2)
	case TKTX_R16_UINT: FT(RED_INTEGER, UNSIGNED_SHORT, R16UI, 2)
	case TKTX_R16_SINT: FT(RED_INTEGER, SHORT, R16I, 2)
	case TKTX_R16_SFLOAT:FT(RED, HALF_FLOAT, R16F, 2)

	case TKTX_R16G16_UNORM: FT(RG, UNSIGNED_SHORT, RG16, 2)
	case TKTX_R16G16_SNORM: FT(RG, SHORT, RG16_SNORM, 2)
	case TKTX_R16G16_UINT: FT(RG_INTEGER, UNSIGNED_SHORT, RG16UI, 2)
	case TKTX_R16G16_SINT: FT(RG_INTEGER, SHORT, RG16I, 2)
	case TKTX_R16G16_SFLOAT:FT(RG, HALF_FLOAT, RG16F, 2)

	case TKTX_R16G16B16_UNORM: FT(RGB, UNSIGNED_SHORT, RGB16, 2)
	case TKTX_R16G16B16_SNORM: FT(RGB, SHORT, RGB16_SNORM, 2)
	case TKTX_R16G16B16_UINT: FT(RGB_INTEGER, UNSIGNED_SHORT, RGB16UI, 2)
	case TKTX_R16G16B16_SINT: FT(RGB_INTEGER, SHORT, RGB16I, 2)
	case TKTX_R16G16B16_SFLOAT: FT(RGB, HALF_FLOAT, RGB16F, 2)

	case TKTX_R16G16B16A16_UNORM: FT(RGBA, UNSIGNED_SHORT, RGBA16, 2)
	case TKTX_R16G16B16A16_SNORM: FT(RGBA, SHORT, RGBA16_SNORM, 2)
	case TKTX_R16G16B16A16_UINT: FT(RGBA_INTEGER, UNSIGNED_SHORT, RGBA16UI, 2)
	case TKTX_R16G16B16A16_SINT: FT(RGBA_INTEGER, SHORT, RGBA16I, 2)
	case TKTX_R16G16B16A16_SFLOAT:FT(RGBA, HALF_FLOAT, RGBA16F, 2)

	case TKTX_R32_UINT: FT(RED_INTEGER, UNSIGNED_INT, R32UI, 4)
	case TKTX_R32_SINT: FT(RED_INTEGER, INT, R32I, 4)
	case TKTX_R32_SFLOAT: FT(RED, FLOAT, R32F, 4)

	case TKTX_R32G32_UINT: FT(RG_INTEGER, UNSIGNED_INT, RG32UI, 4)
	case TKTX_R32G32_SINT: FT(RG_INTEGER, INT, RG32I, 4)
	case TKTX_R32G32_SFLOAT: FT(RG, FLOAT, RG32F, 4)

	case TKTX_R32G32B32_UINT: FT(RGB_INTEGER, UNSIGNED_INT, RGB32UI, 4)
	case TKTX_R32G32B32_SINT: FT(RGB_INTEGER, INT, RGB32I, 4)
	case TKTX_R32G32B32_SFLOAT: FT(RGB_INTEGER, FLOAT, RGB32F, 4)

	case TKTX_R32G32B32A32_UINT: FT(RGBA_INTEGER, UNSIGNED_INT, RGBA32UI, 4)
	case TKTX_R32G32B32A32_SINT: FT(RGBA_INTEGER, INT, RGBA32I, 4)
	case TKTX_R32G32B32A32_SFLOAT:FT(RGBA, FLOAT, RGBA32F, 4)

	case TKTX_BC1_RGB_UNORM_BLOCK: FTC(RGB, RGB_S3TC_DXT1)
	case TKTX_BC1_RGB_SRGB_BLOCK: FTC(RGB, SRGB_S3TC_DXT1)
	case TKTX_BC1_RGBA_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT1)
	case TKTX_BC1_RGBA_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT1)
	case TKTX_BC2_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT3)
	case TKTX_BC2_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT3)
	case TKTX_BC3_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT5)
	case TKTX_BC3_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT5)
	case TKTX_BC4_UNORM_BLOCK: FTC(RED, RED_RGTC1)
	case TKTX_BC4_SNORM_BLOCK: FTC(RED, SIGNED_RED_RGTC1)
	case TKTX_BC5_UNORM_BLOCK: FTC(RG, RED_GREEN_RGTC2)
	case TKTX_BC5_SNORM_BLOCK: FTC(RG, SIGNED_RED_GREEN_RGTC2)
	case TKTX_BC6H_UFLOAT_BLOCK: FTC(RGB, RGB_BPTC_UNSIGNED_FLOAT)
	case TKTX_BC6H_SFLOAT_BLOCK: FTC(RGB, RGB_BPTC_SIGNED_FLOAT)
	case TKTX_BC7_UNORM_BLOCK: FTC(RGBA, RGBA_BPTC_UNORM)
	case TKTX_BC7_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_BPTC_UNORM)

	case TKTX_ETC2_R8G8B8_UNORM_BLOCK: FTC(RGB, RGB8_ETC2)
	case TKTX_ETC2_R8G8B8A1_UNORM_BLOCK: FTC(RGBA, RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
	case TKTX_ETC2_R8G8B8A8_UNORM_BLOCK: FTC(RGBA, RGBA8_ETC2_EAC)
	case TKTX_ETC2_R8G8B8_SRGB_BLOCK: FTC(SRGB, SRGB8_ETC2)
	case TKTX_ETC2_R8G8B8A1_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
	case TKTX_ETC2_R8G8B8A8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ETC2_EAC)
	case TKTX_EAC_R11_UNORM_BLOCK: FTC(RED, R11_EAC)
	case TKTX_EAC_R11G11_UNORM_BLOCK: FTC(RG, RG11_EAC)
	case TKTX_EAC_R11_SNORM_BLOCK: FTC(RED, SIGNED_R11_EAC)
	case TKTX_EAC_R11G11_SNORM_BLOCK: FTC(RG, SIGNED_RG11_EAC)

	case TKTX_PVR_2BPP_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_2BPPV1)
	case TKTX_PVR_2BPPA_UNORM_BLOCK: FTC(RGBA, RGBA_PVRTC_2BPPV1);
	case TKTX_PVR_4BPP_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_4BPPV1)
	case TKTX_PVR_4BPPA_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_4BPPV1)
	case TKTX_PVR_2BPP_SRGB_BLOCK: FTC(SRGB, SRGB_PVRTC_2BPPV1)
	case TKTX_PVR_2BPPA_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB_ALPHA_PVRTC_2BPPV1);
	case TKTX_PVR_4BPP_SRGB_BLOCK: FTC(SRGB, SRGB_PVRTC_2BPPV1)
	case TKTX_PVR_4BPPA_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB_ALPHA_PVRTC_2BPPV1);

	case TKTX_ASTC_4x4_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_4x4)
	case TKTX_ASTC_4x4_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_4x4)
	case TKTX_ASTC_5x4_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_5x4)
	case TKTX_ASTC_5x4_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_5x4)
	case TKTX_ASTC_5x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_5x5)
	case TKTX_ASTC_5x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_5x5)
	case TKTX_ASTC_6x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_6x5)
	case TKTX_ASTC_6x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_6x5)
	case TKTX_ASTC_6x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_6x6)
	case TKTX_ASTC_6x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_6x6)
	case TKTX_ASTC_8x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x5)
	case TKTX_ASTC_8x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x5)
	case TKTX_ASTC_8x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x6)
	case TKTX_ASTC_8x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x6)
	case TKTX_ASTC_8x8_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x8)
	case TKTX_ASTC_8x8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x8)
	case TKTX_ASTC_10x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x5)
	case TKTX_ASTC_10x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x5)
	case TKTX_ASTC_10x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x6)
	case TKTX_ASTC_10x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x6)
	case TKTX_ASTC_10x8_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x8);
	case TKTX_ASTC_10x8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x8)
	case TKTX_ASTC_10x10_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x10)
	case TKTX_ASTC_10x10_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x10)
	case TKTX_ASTC_12x10_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_12x10)
	case TKTX_ASTC_12x10_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_12x10)
	case TKTX_ASTC_12x12_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_12x12)
	case TKTX_ASTC_12x12_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_12x12)

	default:break;
	}
	return false;
}
#undef FT
#undef FTC

TinyKtx_Format TinyKtx_CrackFormatFromGL(uint32_t const glformat,
																				 uint32_t const gltype,
																				 uint32_t const glinternalformat,
																				 uint32_t const typesize) {
	UNREF_PARAM(typesize); 
	switch (glinternalformat) {
	case TINYKTX_GL_COMPRESSED_RGB_S3TC_DXT1: return TKTX_BC1_RGB_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT1: return TKTX_BC1_RGBA_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT3: return TKTX_BC2_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT5: return TKTX_BC3_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_3DC_X_AMD: return TKTX_BC4_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_3DC_XY_AMD: return TKTX_BC5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_PVRTC_2BPPV1: return TKTX_PVR_2BPP_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_PVRTC_4BPPV1: return TKTX_PVR_2BPPA_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1: return TKTX_PVR_2BPPA_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1: return TKTX_PVR_4BPPA_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB_PVRTC_4BPPV1: return TKTX_PVR_4BPP_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB_PVRTC_2BPPV1: return TKTX_PVR_2BPP_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_PVRTC_4BPPV1: return TKTX_PVR_4BPPA_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_PVRTC_2BPPV1: return TKTX_PVR_2BPPA_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_S3TC_DXT1: return TKTX_BC1_RGB_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1: return TKTX_BC1_RGBA_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3: return TKTX_BC2_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5: return TKTX_BC3_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_LUMINANCE_LATC1: return TKTX_BC4_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_LATC1: return TKTX_BC4_SNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_LUMINANCE_ALPHA_LATC2: return TKTX_BC5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2: return TKTX_BC5_SNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RED_RGTC1: return TKTX_BC4_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_RED_RGTC1: return TKTX_BC4_SNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RED_GREEN_RGTC2: return TKTX_BC5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2: return TKTX_BC5_SNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_ETC1_RGB8_OES: return TKTX_ETC2_R8G8B8_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_BPTC_UNORM: return TKTX_BC7_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM: return TKTX_BC7_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT: return TKTX_BC6H_SFLOAT_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return TKTX_BC6H_UFLOAT_BLOCK;
	case TINYKTX_GL_COMPRESSED_R11_EAC: return TKTX_EAC_R11_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_R11_EAC: return TKTX_EAC_R11_SNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RG11_EAC: return TKTX_EAC_R11G11_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SIGNED_RG11_EAC: return TKTX_EAC_R11G11_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB8_ETC2: return TKTX_ETC2_R8G8B8_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ETC2: return TKTX_ETC2_R8G8B8_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: return TKTX_ETC2_R8G8B8A1_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: return TKTX_ETC2_R8G8B8A1_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA8_ETC2_EAC: return TKTX_ETC2_R8G8B8A8_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return TKTX_ETC2_R8G8B8A8_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_4x4: return TKTX_ASTC_4x4_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x4: return TKTX_ASTC_5x4_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x5: return TKTX_ASTC_5x5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x5: return TKTX_ASTC_6x5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x6: return TKTX_ASTC_6x6_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x5: return TKTX_ASTC_8x5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x6: return TKTX_ASTC_8x6_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x8: return TKTX_ASTC_8x8_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x5: return TKTX_ASTC_10x5_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x6: return TKTX_ASTC_10x6_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x8: return TKTX_ASTC_10x8_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x10: return TKTX_ASTC_10x10_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x10: return TKTX_ASTC_12x10_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x12: return TKTX_ASTC_12x12_UNORM_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4: return TKTX_ASTC_4x4_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4: return TKTX_ASTC_5x4_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5: return TKTX_ASTC_5x5_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5: return TKTX_ASTC_6x5_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6: return TKTX_ASTC_6x6_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5: return TKTX_ASTC_8x5_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6: return TKTX_ASTC_8x6_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8: return TKTX_ASTC_8x8_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5: return TKTX_ASTC_10x5_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6: return TKTX_ASTC_10x6_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8: return TKTX_ASTC_10x8_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10: return TKTX_ASTC_10x10_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10: return TKTX_ASTC_12x10_SRGB_BLOCK;
	case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12: return TKTX_ASTC_12x12_SRGB_BLOCK;

	// non compressed
	case TINYKTX_GL_INTFORMAT_RGB4:
		if(glformat == TINYKTX_GL_FORMAT_RG) {
			return TKTX_R4G4_UNORM_PACK8;
		}
		break;

	case TINYKTX_GL_INTFORMAT_RGBA4:
		if(glformat == TINYKTX_GL_FORMAT_RGBA) {
			return TKTX_R4G4B4A4_UNORM_PACK16;
		} else if(glformat == TINYKTX_GL_FORMAT_BGRA) {
			return TKTX_B4G4R4A4_UNORM_PACK16;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGB565:
		if(gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5 || glformat == TINYKTX_GL_FORMAT_BGR) {
			return TKTX_B5G6R5_UNORM_PACK16;
		} else {
			return TKTX_R5G6B5_UNORM_PACK16;
		}
		break;

	case TINYKTX_GL_INTFORMAT_RGB5_A1:
		if (gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV) {
			return TKTX_A1R5G5B5_UNORM_PACK16;
		} else if (gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1) {
			if(glformat == TINYKTX_GL_FORMAT_BGRA) {
				return TKTX_B5G5R5A1_UNORM_PACK16;
			} else {
				return TKTX_R5G5B5A1_UNORM_PACK16;
			}
		}
		break;
	case TINYKTX_GL_INTFORMAT_R8_SNORM:
	case TINYKTX_GL_INTFORMAT_R8:
		if(gltype == TINYKTX_GL_TYPE_BYTE || glinternalformat == TINYKTX_GL_INTFORMAT_R8_SNORM) {
			return TKTX_R8_SNORM;
		}	else {
			return TKTX_R8_UNORM;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RG8_SNORM:
	case TINYKTX_GL_INTFORMAT_RG8:
		if(gltype == TINYKTX_GL_TYPE_BYTE ||
				glinternalformat == TINYKTX_GL_INTFORMAT_RG8_SNORM) {
			return TKTX_R8G8_SNORM;
		}	else {
			return TKTX_R8G8_UNORM;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGB8_SNORM:
	case TINYKTX_GL_INTFORMAT_RGB8:
		if(glformat == TINYKTX_GL_FORMAT_BGR) {
			if(gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGB8_SNORM) {
				return TKTX_B8G8R8_SNORM;
			}	else {
				return TKTX_B8G8R8_UNORM;
			}
		} else {
			if(gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGB8_SNORM) {
				return TKTX_R8G8B8_SNORM;
			}	else {
				return TKTX_R8G8B8_UNORM;
			}
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGBA8_SNORM:
	case TINYKTX_GL_INTFORMAT_RGBA8:
		if(glformat == TINYKTX_GL_FORMAT_RGBA) {
			if(gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
				return TKTX_R8G8B8A8_SNORM;
			} else if(gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
				return TKTX_R8G8B8A8_UNORM;
			}
		} else if(glformat == TINYKTX_GL_FORMAT_BGRA) {
			if(gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
				return TKTX_B8G8R8A8_SNORM;
			} else if(gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
				return TKTX_B8G8R8A8_UNORM;
			}
		} else if(glformat == TINYKTX_GL_FORMAT_ABGR) {
			if(gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
				return TKTX_A8B8G8R8_SNORM_PACK32;
			} else if(gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
				return TKTX_A8B8G8R8_UNORM_PACK32;
			}
		}
		break;

	case TINYKTX_GL_INTFORMAT_R16: return TKTX_R16_UNORM;
	case TINYKTX_GL_INTFORMAT_RG16: return TKTX_R16G16_UNORM;
	case TINYKTX_GL_INTFORMAT_RGB16: return TKTX_R16G16B16_UNORM;
	case TINYKTX_GL_INTFORMAT_RGBA16: return TKTX_R16G16B16A16_UNORM;

	case TINYKTX_GL_INTFORMAT_R16_SNORM:return TKTX_R16_SNORM;
	case TINYKTX_GL_INTFORMAT_RG16_SNORM: return TKTX_R16G16_SNORM;
	case TINYKTX_GL_INTFORMAT_RGB16_SNORM: return TKTX_R16G16B16_SNORM;
	case TINYKTX_GL_INTFORMAT_RGBA16_SNORM: return TKTX_R16G16B16A16_SNORM;

	case TINYKTX_GL_INTFORMAT_R8I: return TKTX_R8_SINT;
	case TINYKTX_GL_INTFORMAT_RG8I: return TKTX_R8G8_SINT;
	case TINYKTX_GL_INTFORMAT_RGB8I:
		if(glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
			return TKTX_R8G8B8_SINT;
		} else if(glformat == TINYKTX_GL_FORMAT_BGR || glformat == TINYKTX_GL_FORMAT_BGR_INTEGER) {
			return TKTX_B8G8R8_SINT;
		}
		break;

	case TINYKTX_GL_INTFORMAT_RGBA8I:
		if(glformat == TINYKTX_GL_FORMAT_RGBA || glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
			return TKTX_R8G8B8A8_SINT;
		} else if(glformat == TINYKTX_GL_FORMAT_BGRA || glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
			return TKTX_B8G8R8A8_SINT;
		} else if(glformat == TINYKTX_GL_FORMAT_ABGR) {
			return TKTX_A8B8G8R8_SINT_PACK32;
		}
		break;

	case TINYKTX_GL_INTFORMAT_R16I: return TKTX_R16_SINT;
	case TINYKTX_GL_INTFORMAT_RG16I: return TKTX_R16G16_SINT;
	case TINYKTX_GL_INTFORMAT_RGB16I:return TKTX_R16G16B16_SINT;
	case TINYKTX_GL_INTFORMAT_RGBA16I:return TKTX_R16G16B16A16_SINT;

	case TINYKTX_GL_INTFORMAT_R32I: return TKTX_R32_SINT;
	case TINYKTX_GL_INTFORMAT_RG32I: return TKTX_R32G32_SINT;
	case TINYKTX_GL_INTFORMAT_RGB32I:
		if(glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
			return TKTX_R32G32B32_SINT;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGBA32I:return TKTX_R32G32B32A32_SINT;

	case TINYKTX_GL_INTFORMAT_R8UI: return TKTX_R8_UINT;
	case TINYKTX_GL_INTFORMAT_RG8UI: return TKTX_R8G8_UINT;
	case TINYKTX_GL_INTFORMAT_RGB8UI:
		if(glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
			return TKTX_R8G8B8_UINT;
		} else if(glformat == TINYKTX_GL_FORMAT_BGR || glformat == TINYKTX_GL_FORMAT_BGR_INTEGER) {
			return TKTX_B8G8R8_UINT;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGBA8UI:
		if(glformat == TINYKTX_GL_FORMAT_RGBA || glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
			return TKTX_R8G8B8A8_UINT;
		} else if(glformat == TINYKTX_GL_FORMAT_BGRA || glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
			return TKTX_B8G8R8A8_UINT;
		} else if(glformat == TINYKTX_GL_FORMAT_ABGR) {
			return TKTX_A8B8G8R8_UINT_PACK32;
		}
		break;

	case TINYKTX_GL_INTFORMAT_R16UI: return TKTX_R16_UINT;
	case TINYKTX_GL_INTFORMAT_RG16UI: return TKTX_R16G16_UINT;
	case TINYKTX_GL_INTFORMAT_RGB16UI:return TKTX_R16G16B16_UINT;
	case TINYKTX_GL_INTFORMAT_RGBA16UI: return TKTX_R16G16B16A16_UINT;

	case TINYKTX_GL_INTFORMAT_R32UI: return TKTX_R32_UINT;
	case TINYKTX_GL_INTFORMAT_RG32UI: return TKTX_R32G32_UINT;
	case TINYKTX_GL_INTFORMAT_RGB32UI: return TKTX_R32G32B32_UINT;
	case TINYKTX_GL_INTFORMAT_RGBA32UI: return TKTX_R32G32B32A32_UINT;

	case TINYKTX_GL_INTFORMAT_R16F: return TKTX_R16_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RG16F: return TKTX_R16G16_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RGB16F: return TKTX_R16G16B16_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RGBA16F: return TKTX_R16G16B16A16_SFLOAT;

	case TINYKTX_GL_INTFORMAT_R32F: return TKTX_R32_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RG32F: return TKTX_R32G32_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RGB32F: return TKTX_R32G32B32_SFLOAT;
	case TINYKTX_GL_INTFORMAT_RGBA32F: return TKTX_R32G32B32A32_SFLOAT;

	case TINYKTX_GL_INTFORMAT_R11F_G11F_B10F: return TKTX_B10G11R11_UFLOAT_PACK32; //??
	case TINYKTX_GL_INTFORMAT_UNSIGNED_INT_10F_11F_11F_REV: return TKTX_B10G11R11_UFLOAT_PACK32; //?
	case TINYKTX_GL_INTFORMAT_RGB9_E5: return TKTX_E5B9G9R9_UFLOAT_PACK32;
	case TINYKTX_GL_INTFORMAT_SLUMINANCE8_ALPHA8: return TKTX_R8G8_SRGB;
	case TINYKTX_GL_INTFORMAT_SLUMINANCE8: return TKTX_R8_SRGB;

	case TINYKTX_GL_INTFORMAT_ALPHA8: return TKTX_R8_UNORM;
	case TINYKTX_GL_INTFORMAT_ALPHA16: return TKTX_R16_UNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE8: return TKTX_R8_UNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE16: return TKTX_R16_UNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8: return TKTX_R8G8_UNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16: return TKTX_R16G16_UNORM;
	case TINYKTX_GL_INTFORMAT_INTENSITY8: return TKTX_R8_UNORM;
	case TINYKTX_GL_INTFORMAT_INTENSITY16: return TKTX_R16_UNORM;

	case TINYKTX_GL_INTFORMAT_ALPHA8_SNORM: return TKTX_R8_SNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE8_SNORM: return TKTX_R8_SNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8_SNORM: return TKTX_R8G8_SNORM;
	case TINYKTX_GL_INTFORMAT_INTENSITY8_SNORM: return TKTX_R8_SNORM;
	case TINYKTX_GL_INTFORMAT_ALPHA16_SNORM: return TKTX_R16_SNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE16_SNORM: return TKTX_R16_SNORM;
	case TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16_SNORM: return TKTX_R16G16_SNORM;
	case TINYKTX_GL_INTFORMAT_INTENSITY16_SNORM: return TKTX_R16_SNORM;

	case TINYKTX_GL_INTFORMAT_SRGB8:
	case TINYKTX_GL_INTFORMAT_SRGB8_ALPHA8:
		if (glformat == TINYKTX_GL_FORMAT_SLUMINANCE || glformat == TINYKTX_GL_FORMAT_RED) {
			return TKTX_R8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA || glformat == TINYKTX_GL_FORMAT_RG) {
			return TKTX_R8G8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_SRGB || glformat == TINYKTX_GL_FORMAT_RGB) {
			return TKTX_R8G8B8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_SRGB_ALPHA || glformat == TINYKTX_GL_FORMAT_RGBA) {
			return TKTX_R8G8B8A8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_BGR) {
			return TKTX_B8G8R8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_BGRA) {
			return TKTX_B8G8R8A8_SRGB;
		} else if (glformat == TINYKTX_GL_FORMAT_ABGR) {
			return TKTX_A8B8G8R8_SRGB_PACK32;
		}
		break;
	case TINYKTX_GL_INTFORMAT_RGB10_A2:
		if(glformat == TINYKTX_GL_FORMAT_BGRA) {
			return TKTX_A2R10G10B10_UNORM_PACK32;
		} else if(glformat == TINYKTX_GL_FORMAT_RGBA) {
			return TKTX_A2B10G10R10_UNORM_PACK32;
		} else		if(glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
			return TKTX_A2R10G10B10_UINT_PACK32;
		} else if(glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
			return TKTX_A2B10G10R10_UINT_PACK32;
		}
		break;

		// apparently we get FORMAT formats in the internal format slot sometimes
	case TINYKTX_GL_FORMAT_RED:
		switch(gltype) {
		case TINYKTX_GL_TYPE_BYTE: return TKTX_R8_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8_UNORM;
		case TINYKTX_GL_TYPE_SHORT: return TKTX_R16_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16_UNORM;
		case TINYKTX_GL_TYPE_INT: return TKTX_R32_SINT;
		case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32_UINT;
		case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32_SFLOAT;
			//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64A64_SFLOAT;
		case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16_SFLOAT;
		default: return TKTX_UNDEFINED;
		}
	case TINYKTX_GL_FORMAT_RG:
		switch(gltype) {
		case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8_UNORM;
		case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16_UNORM;
		case TINYKTX_GL_TYPE_INT: return TKTX_R32G32_SINT;
		case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32_UINT;
		case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32_SFLOAT;
			//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64_SFLOAT;
		case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16_SFLOAT;
		default: return TKTX_UNDEFINED;
		}

	case TINYKTX_GL_FORMAT_RGB:
		switch(gltype) {
		case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8B8_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8B8_UNORM;
		case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16B16_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16B16_UNORM;
		case TINYKTX_GL_TYPE_INT: return TKTX_R32G32B32_SINT;
		case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32B32_UINT;
		case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32B32_SFLOAT;
			//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64_SFLOAT;
		case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16B16_SFLOAT;
		default: return TKTX_UNDEFINED;
		}
	case TINYKTX_GL_FORMAT_RGBA:
		switch(gltype) {
		case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8B8A8_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8B8A8_UNORM;
		case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16B16A16_SNORM;
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16B16A16_UNORM;
		case TINYKTX_GL_TYPE_INT: return TKTX_R32G32B32A32_SINT;
		case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32B32A32_UINT;
		case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32B32A32_SFLOAT;
			//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64A64_SFLOAT;
		case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16B16A16_SFLOAT;
		default: return TKTX_UNDEFINED;
		}

		// can't handle yet
	case TINYKTX_GL_INTFORMAT_ALPHA4:
	case TINYKTX_GL_INTFORMAT_ALPHA12:
	case TINYKTX_GL_INTFORMAT_LUMINANCE4:
	case TINYKTX_GL_INTFORMAT_LUMINANCE12:
	case TINYKTX_GL_INTFORMAT_LUMINANCE4_ALPHA4:
	case TINYKTX_GL_INTFORMAT_LUMINANCE6_ALPHA2:
	case TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA4:
	case TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA12:
	case TINYKTX_GL_INTFORMAT_INTENSITY4:
	case TINYKTX_GL_INTFORMAT_INTENSITY12:
	case TINYKTX_GL_INTFORMAT_RGB2:
	case TINYKTX_GL_INTFORMAT_RGB5:
	case TINYKTX_GL_INTFORMAT_RGB10:
	case TINYKTX_GL_INTFORMAT_RGB12:
	case TINYKTX_GL_INTFORMAT_RGBA2:
	case TINYKTX_GL_INTFORMAT_RGBA12:
	case TINYKTX_GL_INTFORMAT_FLOAT_32_UNSIGNED_INT_24_8_REV:
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2:
	case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2:
	case TINYKTX_GL_COMPRESSED_ATC_RGB:
	case TINYKTX_GL_COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA:
	case TINYKTX_GL_COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA:
	default: break;
	}
	return TKTX_UNDEFINED;
}

uint32_t TinyKtx_ElementCountFromGLFormat(uint32_t fmt){
	switch(fmt) {
	case TINYKTX_GL_FORMAT_RED:
	case TINYKTX_GL_FORMAT_GREEN:
	case TINYKTX_GL_FORMAT_BLUE:
	case TINYKTX_GL_FORMAT_ALPHA:
	case TINYKTX_GL_FORMAT_LUMINANCE:
	case TINYKTX_GL_FORMAT_INTENSITY:
	case TINYKTX_GL_FORMAT_RED_INTEGER:
	case TINYKTX_GL_FORMAT_GREEN_INTEGER:
	case TINYKTX_GL_FORMAT_BLUE_INTEGER:
	case TINYKTX_GL_FORMAT_ALPHA_INTEGER:
	case TINYKTX_GL_FORMAT_SLUMINANCE:
	case TINYKTX_GL_FORMAT_RED_SNORM:
		return 1;

	case TINYKTX_GL_FORMAT_RG_INTEGER:
	case TINYKTX_GL_FORMAT_RG:
	case TINYKTX_GL_FORMAT_LUMINANCE_ALPHA:
	case TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA:
	case TINYKTX_GL_FORMAT_RG_SNORM:
		return 2;

	case TINYKTX_GL_FORMAT_BGR:
	case TINYKTX_GL_FORMAT_RGB:
	case TINYKTX_GL_FORMAT_SRGB:
	case TINYKTX_GL_FORMAT_RGB_INTEGER:
	case TINYKTX_GL_FORMAT_BGR_INTEGER:
	case TINYKTX_GL_FORMAT_RGB_SNORM:
		return 3;

	case TINYKTX_GL_FORMAT_BGRA:
	case TINYKTX_GL_FORMAT_RGBA:
	case TINYKTX_GL_FORMAT_ABGR:
	case TINYKTX_GL_FORMAT_SRGB_ALPHA:
	case TINYKTX_GL_FORMAT_RGBA_INTEGER:
	case TINYKTX_GL_FORMAT_BGRA_INTEGER:
	case TINYKTX_GL_FORMAT_RGBA_SNORM:
		return 4;
	}

	return 0;
}
bool TinyKtx_ByteDividableFromGLType(uint32_t type){
	switch(type) {
	case TINYKTX_GL_TYPE_COMPRESSED:
	case TINYKTX_GL_TYPE_UNSIGNED_BYTE_3_3_2:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_10_10_10_2:
	case TINYKTX_GL_TYPE_UNSIGNED_BYTE_2_3_3_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_2_10_10_10_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_24_8:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_5_9_9_9_REV:
	case TINYKTX_GL_TYPE_UNSIGNED_INT_10F_11F_11F_REV:
	case TINYKTX_GL_TYPE_FLOAT_32_UNSIGNED_INT_24_8_REV:
		return false;
	case TINYKTX_GL_TYPE_BYTE:
	case TINYKTX_GL_TYPE_UNSIGNED_BYTE:
	case TINYKTX_GL_TYPE_SHORT:
	case TINYKTX_GL_TYPE_UNSIGNED_SHORT:
	case TINYKTX_GL_TYPE_INT:
	case TINYKTX_GL_TYPE_UNSIGNED_INT:
	case TINYKTX_GL_TYPE_FLOAT:
	case TINYKTX_GL_TYPE_DOUBLE:
	case TINYKTX_GL_TYPE_HALF_FLOAT:
		return true;
	}
	return false;
}
TinyKtx_Format TinyKtx_GetFormat(TinyKtx_ContextHandle handle) {
	uint32_t glformat;
	uint32_t gltype;
	uint32_t glinternalformat;
	uint32_t typesize;
	uint32_t glbaseinternalformat;

	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return TKTX_UNDEFINED;

	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return TKTX_UNDEFINED;
	}

	if(TinyKtx_GetFormatGL(handle, &glformat, &gltype, &glinternalformat, &typesize, &glbaseinternalformat) == false)
		return TKTX_UNDEFINED;

	return TinyKtx_CrackFormatFromGL(glformat, gltype, glinternalformat, typesize);
}
static uint32_t TinyKtx_MipMapReduce(uint32_t value, uint32_t mipmaplevel) {

	// handle 0 being passed in
	if(value <= 1) return 1;

	// there are better ways of doing this (log2 etc.) but this doesn't require any
	// dependecies and isn't used enough to matter imho
	for (uint32_t i = 0u; i < mipmaplevel;++i) {
		if(value <= 1) return 1;
		value = value / 2;
	}
	return value;
}

// KTX specifys GL_UNPACK_ALIGNMENT = 4 which means some files have unexpected padding
// that probably means you can't just memcpy the data out if you aren't using a GL
// texture with GL_UNPACK_ALIGNMENT of 4
// this will be true if this mipmap level is 'unpacked' so has padding on each row
// you will need to handle.
bool TinyKtx_IsMipMapLevelUnpacked(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return false;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return false;
	}

	if (ctx->header.glTypeSize < 4 &&
			TinyKtx_ByteDividableFromGLType(ctx->header.glType)) {

		uint32_t const s = ctx->header.glTypeSize;
		uint32_t const n = TinyKtx_ElementCountFromGLFormat(ctx->header.glFormat);
		if (n == 0) {
			ctx->callbacks.errorFn(ctx->user, "TinyKtx_ElementCountFromGLFormat error");
			return false;
		}

		uint32_t const w = TinyKtx_MipMapReduce(ctx->header.pixelWidth, mipmaplevel);
		uint32_t const snl = s * n * w;
		uint32_t const k = ((snl + 3u) & ~3u);

		if(k != snl) {
			return true;
		}
	}
	return false;
}
uint32_t TinyKtx_UnpackedRowStride(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
	TinyKtx_Context *ctx = (TinyKtx_Context *) handle;
	if (ctx == NULL)
		return 0;
	if (ctx->headerValid == false) {
		ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
		return 0;
	}

	if (ctx->header.glTypeSize < 4 &&
			TinyKtx_ByteDividableFromGLType(ctx->header.glType)) {

		uint32_t const s = ctx->header.glTypeSize;
		uint32_t const n = TinyKtx_ElementCountFromGLFormat(ctx->header.glFormat);
		if (n == 0) {
			ctx->callbacks.errorFn(ctx->user, "TinyKtx_ElementCountFromGLFormat error");
			return 0;
		}

		uint32_t const w = TinyKtx_MipMapReduce(ctx->header.pixelWidth, mipmaplevel);
		uint32_t const snl = s * n * w;
		uint32_t const k = ((snl + 3u) & ~3u);
		return k;
	}
	return 0;
}


bool TinyKtx_WriteImageGL(TinyKtx_WriteCallbacks const *callbacks,
													void *user,
													uint32_t width,
													uint32_t height,
													uint32_t depth,
													uint32_t slices,
													uint32_t mipmaplevels,
													uint32_t format,
													uint32_t internalFormat,
													uint32_t baseFormat,
													uint32_t type,
													uint32_t typeSize,
													bool cubemap,
													uint32_t const *mipmapsizes,
													void const **mipmaps) {

	TinyKtx_Header header;
	memcpy(header.identifier, TinyKtx_fileIdentifier, 12);
	header.endianness = 0x04030201;
	header.glFormat = format;
	header.glInternalFormat = internalFormat;
	header.glBaseInternalFormat = baseFormat;
	header.glType = type;
	header.glTypeSize = typeSize;
	header.pixelWidth = width;
	header.pixelHeight = (height == 1) ? 0 : height;
	header.pixelDepth = (depth == 1) ? 0 : depth;
	header.numberOfArrayElements = (slices == 1) ? 0 : slices;
	header.numberOfFaces = cubemap ? 6 : 1;
	header.numberOfMipmapLevels = mipmaplevels;
	// TODO keyvalue pair data
	header.bytesOfKeyValueData = 0;
	callbacks->writeFn(user, &header, sizeof(TinyKtx_Header));

	uint32_t w = (width == 0) ? 1 : width;
	uint32_t h = (height == 0) ? 1 : height;
	uint32_t d = (depth == 0) ? 1 : depth;
	uint32_t sl = (slices == 0) ? 1 : slices;
	static uint8_t const padding[4] = {0, 0, 0, 0};

	for (uint32_t i = 0u; i < mipmaplevels; ++i) {

		bool writeRaw = true;

		if(	typeSize < 4 &&
				TinyKtx_ByteDividableFromGLType(type)) {

			uint32_t const s = typeSize;
			uint32_t const n = TinyKtx_ElementCountFromGLFormat(format);
			if (n == 0) {
				callbacks->errorFn(user, "TinyKtx_ElementCountFromGLFormat error");
				return false;
			}
			uint32_t const snl = s * n * w;
			uint32_t const k = ((snl + 3u) & ~3u);

			// Do NOT consider the number of faces in cubemaps for calculating the size according to spec
			uint32_t const size = (k * h * d * sl);
			uint32_t const compare_size = cubemap ? size * 6 : size;
			if (compare_size < mipmapsizes[i]) {
				callbacks->errorFn(user, "Internal size error, padding should only ever expand");
				return false;
			}

			// if we need to expand for padding take the slow per row write route
			if (compare_size > mipmapsizes[i]) {
				callbacks->writeFn(user, &size, sizeof(uint32_t));

				uint8_t const *src = (uint8_t const*) mipmaps[i];
				for (uint32_t ww = 0u; ww < sl; ++ww) {
					for (uint32_t zz = 0; zz < d; ++zz) {
						for (uint32_t yy = 0; yy < h; ++yy) {
							callbacks->writeFn(user, src, snl);
							callbacks->writeFn(user, padding, k - snl);
							src += snl;
						}
					}
				}
				uint32_t paddCount = ((size + 3u) & ~3u) - size;
				if(paddCount > 3) {
					callbacks->errorFn(user, "Internal error: padding bytes > 3");
					return false;
				}

				callbacks->writeFn(user, padding, paddCount);
				writeRaw = false;
			}
		}

		if(writeRaw) {
			uint32_t const size = ((mipmapsizes[i] + 3u) & ~3u);
			uint32_t const write_size = cubemap ? mipmapsizes[i] / 6 : mipmapsizes[i];
			callbacks->writeFn(user, &write_size, sizeof(uint32_t));
			callbacks->writeFn(user, mipmaps[i], mipmapsizes[i]);
			callbacks->writeFn(user, padding, size - mipmapsizes[i]);
		}

		if(w > 1) w = w / 2;
		if(h > 1) h = h / 2;
		if(d > 1) d = d / 2;
	}

	return true;
}

bool TinyKtx_WriteImage(TinyKtx_WriteCallbacks const *callbacks,
												void *user,
												uint32_t width,
												uint32_t height,
												uint32_t depth,
												uint32_t slices,
												uint32_t mipmaplevels,
												TinyKtx_Format format,
												bool cubemap,
												uint32_t const *mipmapsizes,
												void const **mipmaps) {
	uint32_t glformat;
	uint32_t glinternalFormat;
	uint32_t gltype;
	uint32_t gltypeSize;
	uint32_t glbaseInternalFormat;
	if (TinyKtx_CrackFormatToGL(format, &glformat, &gltype, &glinternalFormat, &gltypeSize) == false)
		return false;

	// According to KTX spec v1 https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html :
	// glFormat must equal 0 for compressed textures
	// glBaseInternal format will be the same as glFormat for uncompressed textures
	// glBaseInternal format will be one of the base formats for compressed textures
	glbaseInternalFormat = glformat;
	if (gltype == TINYKTX_GL_TYPE_COMPRESSED)
		glformat = 0;

	return TinyKtx_WriteImageGL(callbacks,
															user,
															width,
															height,
															depth,
															slices,
															mipmaplevels,
															glformat,
															glinternalFormat,
															glbaseInternalFormat,
															gltype,
															gltypeSize,
															cubemap,
															mipmapsizes,
															mipmaps
	);

}

// tiny_imageformat/tinyimageformat.h pr tinyimageformat_base.h needs included
// before tinyktx.h for this functionality
#ifdef TINYIMAGEFORMAT_BASE_H_
TinyImageFormat TinyImageFormat_FromTinyKtxFormat(TinyKtx_Format format)
{
	switch(format) {
	case TKTX_UNDEFINED: return TinyImageFormat_UNDEFINED;
	case TKTX_R4G4_UNORM_PACK8: return TinyImageFormat_R4G4_UNORM;
	case TKTX_R4G4B4A4_UNORM_PACK16: return TinyImageFormat_R4G4B4A4_UNORM;
	case TKTX_B4G4R4A4_UNORM_PACK16: return TinyImageFormat_B4G4R4A4_UNORM;
	case TKTX_R5G6B5_UNORM_PACK16: return TinyImageFormat_R5G6B5_UNORM;
	case TKTX_B5G6R5_UNORM_PACK16: return TinyImageFormat_B5G6R5_UNORM;
	case TKTX_R5G5B5A1_UNORM_PACK16: return TinyImageFormat_R5G5B5A1_UNORM;
	case TKTX_B5G5R5A1_UNORM_PACK16: return TinyImageFormat_B5G5R5A1_UNORM;
	case TKTX_A1R5G5B5_UNORM_PACK16: return TinyImageFormat_A1R5G5B5_UNORM;
	case TKTX_R8_UNORM: return TinyImageFormat_R8_UNORM;
	case TKTX_R8_SNORM: return TinyImageFormat_R8_SNORM;
	case TKTX_R8_UINT: return TinyImageFormat_R8_UINT;
	case TKTX_R8_SINT: return TinyImageFormat_R8_SINT;
	case TKTX_R8_SRGB: return TinyImageFormat_R8_SRGB;
	case TKTX_R8G8_UNORM: return TinyImageFormat_R8G8_UNORM;
	case TKTX_R8G8_SNORM: return TinyImageFormat_R8G8_SNORM;
	case TKTX_R8G8_UINT: return TinyImageFormat_R8G8_UINT;
	case TKTX_R8G8_SINT: return TinyImageFormat_R8G8_SINT;
	case TKTX_R8G8_SRGB: return TinyImageFormat_R8G8_SRGB;
	case TKTX_R8G8B8_UNORM: return TinyImageFormat_R8G8B8_UNORM;
	case TKTX_R8G8B8_SNORM: return TinyImageFormat_R8G8B8_SNORM;
	case TKTX_R8G8B8_UINT: return TinyImageFormat_R8G8B8_UINT;
	case TKTX_R8G8B8_SINT: return TinyImageFormat_R8G8B8_SINT;
	case TKTX_R8G8B8_SRGB: return TinyImageFormat_R8G8B8_SRGB;
	case TKTX_B8G8R8_UNORM: return TinyImageFormat_B8G8R8_UNORM;
	case TKTX_B8G8R8_SNORM: return TinyImageFormat_B8G8R8_SNORM;
	case TKTX_B8G8R8_UINT: return TinyImageFormat_B8G8R8_UINT;
	case TKTX_B8G8R8_SINT: return TinyImageFormat_B8G8R8_SINT;
	case TKTX_B8G8R8_SRGB: return TinyImageFormat_B8G8R8_SRGB;
	case TKTX_R8G8B8A8_UNORM: return TinyImageFormat_R8G8B8A8_UNORM;
	case TKTX_R8G8B8A8_SNORM: return TinyImageFormat_R8G8B8A8_SNORM;
	case TKTX_R8G8B8A8_UINT: return TinyImageFormat_R8G8B8A8_UINT;
	case TKTX_R8G8B8A8_SINT: return TinyImageFormat_R8G8B8A8_SINT;
	case TKTX_R8G8B8A8_SRGB: return TinyImageFormat_R8G8B8A8_SRGB;
	case TKTX_B8G8R8A8_UNORM: return TinyImageFormat_B8G8R8A8_UNORM;
	case TKTX_B8G8R8A8_SNORM: return TinyImageFormat_B8G8R8A8_SNORM;
	case TKTX_B8G8R8A8_UINT: return TinyImageFormat_B8G8R8A8_UINT;
	case TKTX_B8G8R8A8_SINT: return TinyImageFormat_B8G8R8A8_SINT;
	case TKTX_B8G8R8A8_SRGB: return TinyImageFormat_B8G8R8A8_SRGB;
	case TKTX_E5B9G9R9_UFLOAT_PACK32: return TinyImageFormat_E5B9G9R9_UFLOAT;
	case TKTX_A2R10G10B10_UNORM_PACK32: return TinyImageFormat_B10G10R10A2_UNORM;
	case TKTX_A2R10G10B10_UINT_PACK32: return TinyImageFormat_B10G10R10A2_UINT;
	case TKTX_A2B10G10R10_UNORM_PACK32: return TinyImageFormat_R10G10B10A2_UNORM;
	case TKTX_A2B10G10R10_UINT_PACK32: return TinyImageFormat_R10G10B10A2_UINT;
	case TKTX_B10G11R11_UFLOAT_PACK32: return TinyImageFormat_B10G11R11_UFLOAT;
	case TKTX_R16_UNORM: return TinyImageFormat_R16_UNORM;
	case TKTX_R16_SNORM: return TinyImageFormat_R16_SNORM;
	case TKTX_R16_UINT: return TinyImageFormat_R16_UINT;
	case TKTX_R16_SINT: return TinyImageFormat_R16_SINT;
	case TKTX_R16_SFLOAT: return TinyImageFormat_R16_SFLOAT;
	case TKTX_R16G16_UNORM: return TinyImageFormat_R16G16_UNORM;
	case TKTX_R16G16_SNORM: return TinyImageFormat_R16G16_SNORM;
	case TKTX_R16G16_UINT: return TinyImageFormat_R16G16_UINT;
	case TKTX_R16G16_SINT: return TinyImageFormat_R16G16_SINT;
	case TKTX_R16G16_SFLOAT: return TinyImageFormat_R16G16_SFLOAT;
	case TKTX_R16G16B16_UNORM: return TinyImageFormat_R16G16B16_UNORM;
	case TKTX_R16G16B16_SNORM: return TinyImageFormat_R16G16B16_SNORM;
	case TKTX_R16G16B16_UINT: return TinyImageFormat_R16G16B16_UINT;
	case TKTX_R16G16B16_SINT: return TinyImageFormat_R16G16B16_SINT;
	case TKTX_R16G16B16_SFLOAT: return TinyImageFormat_R16G16B16_SFLOAT;
	case TKTX_R16G16B16A16_UNORM: return TinyImageFormat_R16G16B16A16_UNORM;
	case TKTX_R16G16B16A16_SNORM: return TinyImageFormat_R16G16B16A16_SNORM;
	case TKTX_R16G16B16A16_UINT: return TinyImageFormat_R16G16B16A16_UINT;
	case TKTX_R16G16B16A16_SINT: return TinyImageFormat_R16G16B16A16_SINT;
	case TKTX_R16G16B16A16_SFLOAT: return TinyImageFormat_R16G16B16A16_SFLOAT;
	case TKTX_R32_UINT: return TinyImageFormat_R32_UINT;
	case TKTX_R32_SINT: return TinyImageFormat_R32_SINT;
	case TKTX_R32_SFLOAT: return TinyImageFormat_R32_SFLOAT;
	case TKTX_R32G32_UINT: return TinyImageFormat_R32G32_UINT;
	case TKTX_R32G32_SINT: return TinyImageFormat_R32G32_SINT;
	case TKTX_R32G32_SFLOAT: return TinyImageFormat_R32G32_SFLOAT;
	case TKTX_R32G32B32_UINT: return TinyImageFormat_R32G32B32_UINT;
	case TKTX_R32G32B32_SINT: return TinyImageFormat_R32G32B32_SINT;
	case TKTX_R32G32B32_SFLOAT: return TinyImageFormat_R32G32B32_SFLOAT;
	case TKTX_R32G32B32A32_UINT: return TinyImageFormat_R32G32B32A32_UINT;
	case TKTX_R32G32B32A32_SINT: return TinyImageFormat_R32G32B32A32_SINT;
	case TKTX_R32G32B32A32_SFLOAT: return TinyImageFormat_R32G32B32A32_SFLOAT;
	case TKTX_BC1_RGB_UNORM_BLOCK: return TinyImageFormat_DXBC1_RGB_UNORM;
	case TKTX_BC1_RGB_SRGB_BLOCK: return TinyImageFormat_DXBC1_RGB_SRGB;
	case TKTX_BC1_RGBA_UNORM_BLOCK: return TinyImageFormat_DXBC1_RGBA_UNORM;
	case TKTX_BC1_RGBA_SRGB_BLOCK: return TinyImageFormat_DXBC1_RGBA_SRGB;
	case TKTX_BC2_UNORM_BLOCK: return TinyImageFormat_DXBC2_UNORM;
	case TKTX_BC2_SRGB_BLOCK: return TinyImageFormat_DXBC2_SRGB;
	case TKTX_BC3_UNORM_BLOCK: return TinyImageFormat_DXBC3_UNORM;
	case TKTX_BC3_SRGB_BLOCK: return TinyImageFormat_DXBC3_SRGB;
	case TKTX_BC4_UNORM_BLOCK: return TinyImageFormat_DXBC4_UNORM;
	case TKTX_BC4_SNORM_BLOCK: return TinyImageFormat_DXBC4_SNORM;
	case TKTX_BC5_UNORM_BLOCK: return TinyImageFormat_DXBC5_UNORM;
	case TKTX_BC5_SNORM_BLOCK: return TinyImageFormat_DXBC5_SNORM;
	case TKTX_BC6H_UFLOAT_BLOCK: return TinyImageFormat_DXBC6H_UFLOAT;
	case TKTX_BC6H_SFLOAT_BLOCK: return TinyImageFormat_DXBC6H_SFLOAT;
	case TKTX_BC7_UNORM_BLOCK: return TinyImageFormat_DXBC7_UNORM;
	case TKTX_BC7_SRGB_BLOCK: return TinyImageFormat_DXBC7_SRGB;
	case TKTX_PVR_2BPP_UNORM_BLOCK: return TinyImageFormat_PVRTC1_2BPP_UNORM;
	case TKTX_PVR_2BPPA_UNORM_BLOCK: return TinyImageFormat_PVRTC1_2BPP_UNORM;
	case TKTX_PVR_4BPP_UNORM_BLOCK: return TinyImageFormat_PVRTC1_4BPP_UNORM;
	case TKTX_PVR_4BPPA_UNORM_BLOCK: return TinyImageFormat_PVRTC1_4BPP_UNORM;
	case TKTX_PVR_2BPP_SRGB_BLOCK: return TinyImageFormat_PVRTC1_2BPP_SRGB;
	case TKTX_PVR_2BPPA_SRGB_BLOCK: return TinyImageFormat_PVRTC1_2BPP_SRGB;
	case TKTX_PVR_4BPP_SRGB_BLOCK: return TinyImageFormat_PVRTC1_4BPP_SRGB;
	case TKTX_PVR_4BPPA_SRGB_BLOCK: return TinyImageFormat_PVRTC1_4BPP_SRGB;

	case TKTX_ETC2_R8G8B8_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8_UNORM;
	case TKTX_ETC2_R8G8B8A1_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8A1_UNORM;
	case TKTX_ETC2_R8G8B8A8_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8A8_UNORM;
	case TKTX_ETC2_R8G8B8_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8_SRGB;
	case TKTX_ETC2_R8G8B8A1_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8A1_SRGB;
	case TKTX_ETC2_R8G8B8A8_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8A8_SRGB;
	case TKTX_EAC_R11_UNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11_UNORM;
	case TKTX_EAC_R11G11_UNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11G11_UNORM;
	case TKTX_EAC_R11_SNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11_SNORM;
	case TKTX_EAC_R11G11_SNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11G11_SNORM;
	case TKTX_ASTC_4x4_UNORM_BLOCK: return TinyImageFormat_ASTC_4x4_UNORM;
	case TKTX_ASTC_4x4_SRGB_BLOCK: return TinyImageFormat_ASTC_4x4_SRGB;
	case TKTX_ASTC_5x4_UNORM_BLOCK: return TinyImageFormat_ASTC_5x4_UNORM;
	case TKTX_ASTC_5x4_SRGB_BLOCK: return TinyImageFormat_ASTC_5x4_SRGB;
	case TKTX_ASTC_5x5_UNORM_BLOCK: return TinyImageFormat_ASTC_5x5_UNORM;
	case TKTX_ASTC_5x5_SRGB_BLOCK: return TinyImageFormat_ASTC_5x5_SRGB;
	case TKTX_ASTC_6x5_UNORM_BLOCK: return TinyImageFormat_ASTC_6x5_UNORM;
	case TKTX_ASTC_6x5_SRGB_BLOCK: return TinyImageFormat_ASTC_6x5_SRGB;
	case TKTX_ASTC_6x6_UNORM_BLOCK: return TinyImageFormat_ASTC_6x6_UNORM;
	case TKTX_ASTC_6x6_SRGB_BLOCK: return TinyImageFormat_ASTC_6x6_SRGB;
	case TKTX_ASTC_8x5_UNORM_BLOCK: return TinyImageFormat_ASTC_8x5_UNORM;
	case TKTX_ASTC_8x5_SRGB_BLOCK: return TinyImageFormat_ASTC_8x5_SRGB;
	case TKTX_ASTC_8x6_UNORM_BLOCK: return TinyImageFormat_ASTC_8x6_UNORM;
	case TKTX_ASTC_8x6_SRGB_BLOCK: return TinyImageFormat_ASTC_8x6_SRGB;
	case TKTX_ASTC_8x8_UNORM_BLOCK: return TinyImageFormat_ASTC_8x8_UNORM;
	case TKTX_ASTC_8x8_SRGB_BLOCK: return TinyImageFormat_ASTC_8x8_SRGB;
	case TKTX_ASTC_10x5_UNORM_BLOCK: return TinyImageFormat_ASTC_10x5_UNORM;
	case TKTX_ASTC_10x5_SRGB_BLOCK: return TinyImageFormat_ASTC_10x5_SRGB;
	case TKTX_ASTC_10x6_UNORM_BLOCK: return TinyImageFormat_ASTC_10x6_UNORM;
	case TKTX_ASTC_10x6_SRGB_BLOCK: return TinyImageFormat_ASTC_10x6_SRGB;
	case TKTX_ASTC_10x8_UNORM_BLOCK: return TinyImageFormat_ASTC_10x8_UNORM;
	case TKTX_ASTC_10x8_SRGB_BLOCK: return TinyImageFormat_ASTC_10x8_SRGB;
	case TKTX_ASTC_10x10_UNORM_BLOCK: return TinyImageFormat_ASTC_10x10_UNORM;
	case TKTX_ASTC_10x10_SRGB_BLOCK: return TinyImageFormat_ASTC_10x10_SRGB;
	case TKTX_ASTC_12x10_UNORM_BLOCK: return TinyImageFormat_ASTC_12x10_UNORM;
	case TKTX_ASTC_12x10_SRGB_BLOCK: return TinyImageFormat_ASTC_12x10_SRGB;
	case TKTX_ASTC_12x12_UNORM_BLOCK: return TinyImageFormat_ASTC_12x12_UNORM;
	case TKTX_ASTC_12x12_SRGB_BLOCK: return TinyImageFormat_ASTC_12x12_SRGB;

	case TKTX_A8B8G8R8_UNORM_PACK32:break;
	case TKTX_A8B8G8R8_SNORM_PACK32:break;
	case TKTX_A8B8G8R8_UINT_PACK32:break;
	case TKTX_A8B8G8R8_SINT_PACK32:break;
	case TKTX_A8B8G8R8_SRGB_PACK32:break;
	}

	return TinyImageFormat_UNDEFINED;
}

TinyKtx_Format TinyImageFormat_ToTinyKtxFormat(TinyImageFormat format) {

	switch (format) {
	case TinyImageFormat_UNDEFINED: return TKTX_UNDEFINED;
	case TinyImageFormat_R4G4_UNORM: return TKTX_R4G4_UNORM_PACK8;
	case TinyImageFormat_R4G4B4A4_UNORM: return TKTX_R4G4B4A4_UNORM_PACK16;
	case TinyImageFormat_B4G4R4A4_UNORM: return TKTX_B4G4R4A4_UNORM_PACK16;
	case TinyImageFormat_R5G6B5_UNORM: return TKTX_R5G6B5_UNORM_PACK16;
	case TinyImageFormat_B5G6R5_UNORM: return TKTX_B5G6R5_UNORM_PACK16;
	case TinyImageFormat_R5G5B5A1_UNORM: return TKTX_R5G5B5A1_UNORM_PACK16;
	case TinyImageFormat_B5G5R5A1_UNORM: return TKTX_B5G5R5A1_UNORM_PACK16;
	case TinyImageFormat_A1R5G5B5_UNORM: return TKTX_A1R5G5B5_UNORM_PACK16;
	case TinyImageFormat_R8_UNORM: return TKTX_R8_UNORM;
	case TinyImageFormat_R8_SNORM: return TKTX_R8_SNORM;
	case TinyImageFormat_R8_UINT: return TKTX_R8_UINT;
	case TinyImageFormat_R8_SINT: return TKTX_R8_SINT;
	case TinyImageFormat_R8_SRGB: return TKTX_R8_SRGB;
	case TinyImageFormat_R8G8_UNORM: return TKTX_R8G8_UNORM;
	case TinyImageFormat_R8G8_SNORM: return TKTX_R8G8_SNORM;
	case TinyImageFormat_R8G8_UINT: return TKTX_R8G8_UINT;
	case TinyImageFormat_R8G8_SINT: return TKTX_R8G8_SINT;
	case TinyImageFormat_R8G8_SRGB: return TKTX_R8G8_SRGB;
	case TinyImageFormat_R8G8B8_UNORM: return TKTX_R8G8B8_UNORM;
	case TinyImageFormat_R8G8B8_SNORM: return TKTX_R8G8B8_SNORM;
	case TinyImageFormat_R8G8B8_UINT: return TKTX_R8G8B8_UINT;
	case TinyImageFormat_R8G8B8_SINT: return TKTX_R8G8B8_SINT;
	case TinyImageFormat_R8G8B8_SRGB: return TKTX_R8G8B8_SRGB;
	case TinyImageFormat_B8G8R8_UNORM: return TKTX_B8G8R8_UNORM;
	case TinyImageFormat_B8G8R8_SNORM: return TKTX_B8G8R8_SNORM;
	case TinyImageFormat_B8G8R8_UINT: return TKTX_B8G8R8_UINT;
	case TinyImageFormat_B8G8R8_SINT: return TKTX_B8G8R8_SINT;
	case TinyImageFormat_B8G8R8_SRGB: return TKTX_B8G8R8_SRGB;
	case TinyImageFormat_R8G8B8A8_UNORM: return TKTX_R8G8B8A8_UNORM;
	case TinyImageFormat_R8G8B8A8_SNORM: return TKTX_R8G8B8A8_SNORM;
	case TinyImageFormat_R8G8B8A8_UINT: return TKTX_R8G8B8A8_UINT;
	case TinyImageFormat_R8G8B8A8_SINT: return TKTX_R8G8B8A8_SINT;
	case TinyImageFormat_R8G8B8A8_SRGB: return TKTX_R8G8B8A8_SRGB;
	case TinyImageFormat_B8G8R8A8_UNORM: return TKTX_B8G8R8A8_UNORM;
	case TinyImageFormat_B8G8R8A8_SNORM: return TKTX_B8G8R8A8_SNORM;
	case TinyImageFormat_B8G8R8A8_UINT: return TKTX_B8G8R8A8_UINT;
	case TinyImageFormat_B8G8R8A8_SINT: return TKTX_B8G8R8A8_SINT;
	case TinyImageFormat_B8G8R8A8_SRGB: return TKTX_B8G8R8A8_SRGB;
	case TinyImageFormat_R10G10B10A2_UNORM: return TKTX_A2B10G10R10_UNORM_PACK32;
	case TinyImageFormat_R10G10B10A2_UINT: return TKTX_A2B10G10R10_UINT_PACK32;
	case TinyImageFormat_B10G10R10A2_UNORM: return TKTX_A2R10G10B10_UNORM_PACK32;
	case TinyImageFormat_B10G10R10A2_UINT: return TKTX_A2R10G10B10_UINT_PACK32;
	case TinyImageFormat_R16_UNORM: return TKTX_R16_UNORM;
	case TinyImageFormat_R16_SNORM: return TKTX_R16_SNORM;
	case TinyImageFormat_R16_UINT: return TKTX_R16_UINT;
	case TinyImageFormat_R16_SINT: return TKTX_R16_SINT;
	case TinyImageFormat_R16_SFLOAT: return TKTX_R16_SFLOAT;
	case TinyImageFormat_R16G16_UNORM: return TKTX_R16G16_UNORM;
	case TinyImageFormat_R16G16_SNORM: return TKTX_R16G16_SNORM;
	case TinyImageFormat_R16G16_UINT: return TKTX_R16G16_UINT;
	case TinyImageFormat_R16G16_SINT: return TKTX_R16G16_SINT;
	case TinyImageFormat_R16G16_SFLOAT: return TKTX_R16G16_SFLOAT;
	case TinyImageFormat_R16G16B16_UNORM: return TKTX_R16G16B16_UNORM;
	case TinyImageFormat_R16G16B16_SNORM: return TKTX_R16G16B16_SNORM;
	case TinyImageFormat_R16G16B16_UINT: return TKTX_R16G16B16_UINT;
	case TinyImageFormat_R16G16B16_SINT: return TKTX_R16G16B16_SINT;
	case TinyImageFormat_R16G16B16_SFLOAT: return TKTX_R16G16B16_SFLOAT;
	case TinyImageFormat_R16G16B16A16_UNORM: return TKTX_R16G16B16A16_UNORM;
	case TinyImageFormat_R16G16B16A16_SNORM: return TKTX_R16G16B16A16_SNORM;
	case TinyImageFormat_R16G16B16A16_UINT: return TKTX_R16G16B16A16_UINT;
	case TinyImageFormat_R16G16B16A16_SINT: return TKTX_R16G16B16A16_SINT;
	case TinyImageFormat_R16G16B16A16_SFLOAT: return TKTX_R16G16B16A16_SFLOAT;
	case TinyImageFormat_R32_UINT: return TKTX_R32_UINT;
	case TinyImageFormat_R32_SINT: return TKTX_R32_SINT;
	case TinyImageFormat_R32_SFLOAT: return TKTX_R32_SFLOAT;
	case TinyImageFormat_R32G32_UINT: return TKTX_R32G32_UINT;
	case TinyImageFormat_R32G32_SINT: return TKTX_R32G32_SINT;
	case TinyImageFormat_R32G32_SFLOAT: return TKTX_R32G32_SFLOAT;
	case TinyImageFormat_R32G32B32_UINT: return TKTX_R32G32B32_UINT;
	case TinyImageFormat_R32G32B32_SINT: return TKTX_R32G32B32_SINT;
	case TinyImageFormat_R32G32B32_SFLOAT: return TKTX_R32G32B32_SFLOAT;
	case TinyImageFormat_R32G32B32A32_UINT: return TKTX_R32G32B32A32_UINT;
	case TinyImageFormat_R32G32B32A32_SINT: return TKTX_R32G32B32A32_SINT;
	case TinyImageFormat_R32G32B32A32_SFLOAT: return TKTX_R32G32B32A32_SFLOAT;
	case TinyImageFormat_B10G11R11_UFLOAT: return TKTX_B10G11R11_UFLOAT_PACK32;
	case TinyImageFormat_E5B9G9R9_UFLOAT: return TKTX_E5B9G9R9_UFLOAT_PACK32;

	case TinyImageFormat_DXBC1_RGB_UNORM: return TKTX_BC1_RGB_UNORM_BLOCK;
	case TinyImageFormat_DXBC1_RGB_SRGB: return TKTX_BC1_RGB_SRGB_BLOCK;
	case TinyImageFormat_DXBC1_RGBA_UNORM: return TKTX_BC1_RGBA_UNORM_BLOCK;
	case TinyImageFormat_DXBC1_RGBA_SRGB: return TKTX_BC1_RGBA_SRGB_BLOCK;
	case TinyImageFormat_DXBC2_UNORM: return TKTX_BC2_UNORM_BLOCK;
	case TinyImageFormat_DXBC2_SRGB: return TKTX_BC2_SRGB_BLOCK;
	case TinyImageFormat_DXBC3_UNORM: return TKTX_BC3_UNORM_BLOCK;
	case TinyImageFormat_DXBC3_SRGB: return TKTX_BC3_SRGB_BLOCK;
	case TinyImageFormat_DXBC4_UNORM: return TKTX_BC4_UNORM_BLOCK;
	case TinyImageFormat_DXBC4_SNORM: return TKTX_BC4_SNORM_BLOCK;
	case TinyImageFormat_DXBC5_UNORM: return TKTX_BC5_UNORM_BLOCK;
	case TinyImageFormat_DXBC5_SNORM: return TKTX_BC5_SNORM_BLOCK;
	case TinyImageFormat_DXBC6H_UFLOAT: return TKTX_BC6H_UFLOAT_BLOCK;
	case TinyImageFormat_DXBC6H_SFLOAT: return TKTX_BC6H_SFLOAT_BLOCK;
	case TinyImageFormat_DXBC7_UNORM: return TKTX_BC7_UNORM_BLOCK;
	case TinyImageFormat_DXBC7_SRGB: return TKTX_BC7_SRGB_BLOCK;
	case TinyImageFormat_PVRTC1_2BPP_UNORM: return TKTX_PVR_2BPPA_UNORM_BLOCK;
	case TinyImageFormat_PVRTC1_4BPP_UNORM: return TKTX_PVR_4BPPA_UNORM_BLOCK;
	case TinyImageFormat_PVRTC1_2BPP_SRGB: return TKTX_PVR_2BPPA_SRGB_BLOCK;
	case TinyImageFormat_PVRTC1_4BPP_SRGB: return TKTX_PVR_4BPPA_SRGB_BLOCK;

	// ASTC
	case TinyImageFormat_ASTC_4x4_UNORM:	return TKTX_ASTC_4x4_UNORM_BLOCK;
	case TinyImageFormat_ASTC_4x4_SRGB:		return TKTX_ASTC_4x4_SRGB_BLOCK;
	case TinyImageFormat_ASTC_5x4_UNORM:	return TKTX_ASTC_5x4_UNORM_BLOCK;
	case TinyImageFormat_ASTC_5x4_SRGB:		return TKTX_ASTC_5x4_SRGB_BLOCK;
	case TinyImageFormat_ASTC_5x5_UNORM:	return TKTX_ASTC_5x5_UNORM_BLOCK;
	case TinyImageFormat_ASTC_5x5_SRGB:		return TKTX_ASTC_5x5_SRGB_BLOCK;
	case TinyImageFormat_ASTC_6x5_UNORM:	return TKTX_ASTC_6x5_UNORM_BLOCK;
	case TinyImageFormat_ASTC_6x5_SRGB:		return TKTX_ASTC_6x5_SRGB_BLOCK;
	case TinyImageFormat_ASTC_6x6_UNORM:	return TKTX_ASTC_6x6_UNORM_BLOCK;
	case TinyImageFormat_ASTC_6x6_SRGB:		return TKTX_ASTC_6x6_SRGB_BLOCK;
	case TinyImageFormat_ASTC_8x5_UNORM:	return TKTX_ASTC_8x5_UNORM_BLOCK;
	case TinyImageFormat_ASTC_8x5_SRGB:		return TKTX_ASTC_8x5_SRGB_BLOCK;
	case TinyImageFormat_ASTC_8x6_UNORM:	return TKTX_ASTC_8x6_UNORM_BLOCK;
	case TinyImageFormat_ASTC_8x6_SRGB:		return TKTX_ASTC_8x6_SRGB_BLOCK;
	case TinyImageFormat_ASTC_8x8_UNORM:	return TKTX_ASTC_8x8_UNORM_BLOCK;
	case TinyImageFormat_ASTC_8x8_SRGB:		return TKTX_ASTC_8x8_SRGB_BLOCK;
	case TinyImageFormat_ASTC_10x5_UNORM:	return TKTX_ASTC_10x5_UNORM_BLOCK;
	case TinyImageFormat_ASTC_10x5_SRGB:	return TKTX_ASTC_10x5_SRGB_BLOCK;
	case TinyImageFormat_ASTC_10x6_UNORM:	return TKTX_ASTC_10x6_UNORM_BLOCK;
	case TinyImageFormat_ASTC_10x6_SRGB:	return TKTX_ASTC_10x6_SRGB_BLOCK;
	case TinyImageFormat_ASTC_10x8_UNORM:	return TKTX_ASTC_10x8_UNORM_BLOCK;
	case TinyImageFormat_ASTC_10x8_SRGB:	return TKTX_ASTC_10x8_SRGB_BLOCK;
	case TinyImageFormat_ASTC_10x10_UNORM:	return TKTX_ASTC_10x10_UNORM_BLOCK;
	case TinyImageFormat_ASTC_10x10_SRGB:	return TKTX_ASTC_10x10_SRGB_BLOCK;
	case TinyImageFormat_ASTC_12x10_UNORM:	return TKTX_ASTC_12x10_UNORM_BLOCK;
	case TinyImageFormat_ASTC_12x10_SRGB:	return TKTX_ASTC_12x10_SRGB_BLOCK;
	case TinyImageFormat_ASTC_12x12_UNORM:	return TKTX_ASTC_12x12_UNORM_BLOCK;
	case TinyImageFormat_ASTC_12x12_SRGB:	return TKTX_ASTC_12x12_SRGB_BLOCK;

	default: return TKTX_UNDEFINED;
	};
}
#endif // end TinyImageFormat conversion

#endif // end implementation

#ifdef __cplusplus
};
#endif

#endif // end header
/*
MIT License

Copyright (c) 2019 DeanoC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
