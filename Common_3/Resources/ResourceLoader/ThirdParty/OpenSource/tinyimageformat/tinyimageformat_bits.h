#pragma once

typedef enum TinyImageFormat_Namespace {
	TinyImageFormat_NAMESPACE_PACK = 0ULL,
	TinyImageFormat_NAMESPACE_DEPTH_STENCIL = 1ULL,
	TinyImageFormat_NAMESPACE_DXTC = 2ULL,
	TinyImageFormat_NAMESPACE_PVRTC = 3ULL,
	TinyImageFormat_NAMESPACE_ETC = 4ULL,
	TinyImageFormat_NAMESPACE_ASTC = 5ULL,
	TinyImageFormat_NAMESPACE_CLUT = 6ULL,
} TinyImageFormat_Namespace;

typedef enum TinyImageFormat_Pack_Special {
	TinyImageFormat_PACK_SPECIAL_NONE = 0ULL,
	TinyImageFormat_PACK_SPECIAL_PACK = 1ULL,
	TinyImageFormat_PACK_SPECIAL_MULTI2 = 2ULL,
	TinyImageFormat_PACK_SPECIAL_MULTI4 = 3ULL,
	TinyImageFormat_PACK_SPECIAL_MULTI8 = 4ULL,
} TinyImageFormat_Pack_Special;

typedef enum TinyImageFormat_Pack_Bits {
	TinyImageFormat_PACK_BITS_0 = 0ULL,
	TinyImageFormat_PACK_BITS_1 = 1ULL,
	TinyImageFormat_PACK_BITS_2 = 2ULL,
	TinyImageFormat_PACK_BITS_3 = 3ULL,
	TinyImageFormat_PACK_BITS_4 = 4ULL,
	TinyImageFormat_PACK_BITS_5 = 5ULL,
	TinyImageFormat_PACK_BITS_6 = 6ULL,
	TinyImageFormat_PACK_BITS_7 = 7ULL,
	TinyImageFormat_PACK_BITS_8 = 8ULL,
	TinyImageFormat_PACK_BITS_9 = 9ULL,
	TinyImageFormat_PACK_BITS_10 = 10ULL,
	TinyImageFormat_PACK_BITS_11 = 11ULL,
	TinyImageFormat_PACK_BITS_12 = 12ULL,
	TinyImageFormat_PACK_BITS_16 = 13ULL,
	TinyImageFormat_PACK_BITS_24 = 14ULL,
	TinyImageFormat_PACK_BITS_32 = 15ULL,
	TinyImageFormat_PACK_BITS_64 = 16ULL,
} TinyImageFormat_Pack_Bits;

typedef enum TinyImageFormat_Pack_Swizzle {
	TinyImageFormat_PACK_SWIZZLE_R = 0ULL,
	TinyImageFormat_PACK_SWIZZLE_G = 1ULL,
	TinyImageFormat_PACK_SWIZZLE_B = 2ULL,
	TinyImageFormat_PACK_SWIZZLE_A = 3ULL,
	TinyImageFormat_PACK_SWIZZLE_0 = 4ULL,
	TinyImageFormat_PACK_SWIZZLE_1 = 5ULL,
} TinyImageFormat_Pack_Swizzle;

typedef enum TinyImageFormat_Pack_Type {
	TinyImageFormat_PACK_TYPE_NONE = 0ULL,
	TinyImageFormat_PACK_TYPE_UNORM = 1ULL,
	TinyImageFormat_PACK_TYPE_SNORM = 2ULL,
	TinyImageFormat_PACK_TYPE_UINT = 3ULL,
	TinyImageFormat_PACK_TYPE_SINT = 4ULL,
	TinyImageFormat_PACK_TYPE_UFLOAT = 5ULL,
	TinyImageFormat_PACK_TYPE_SFLOAT = 6ULL,
	TinyImageFormat_PACK_TYPE_SRGB = 7ULL,
	TinyImageFormat_PACK_TYPE_SBFLOAT = 8ULL,
} TinyImageFormat_Pack_Type;

typedef enum TinyImageFormat_DepthStencil_Total_Size {
	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_8 = 0ULL,
	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_16 = 1ULL,
	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_32 = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_64 = 3ULL,
} TinyImageFormat_DepthStencil_Total_Size;

typedef enum TinyImageFormat_DepthStencil_Bits {
	TinyImageFormat_DEPTH_STENCIL_BITS_0 = 0ULL,
	TinyImageFormat_DEPTH_STENCIL_BITS_8 = 1ULL,
	TinyImageFormat_DEPTH_STENCIL_BITS_16 = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_BITS_24 = 3ULL,
	TinyImageFormat_DEPTH_STENCIL_BITS_32 = 4ULL,
} TinyImageFormat_DepthStencil_Bits;

typedef enum TinyImageFormat_DepthStencil_Swizzle {
	TinyImageFormat_DEPTH_STENCIL_SWIZZLE_D = 0ULL,
	TinyImageFormat_DEPTH_STENCIL_SWIZZLE_S = 1ULL,
	TinyImageFormat_DEPTH_STENCIL_SWIZZLE_0 = 2ULL,
} TinyImageFormat_DepthStencil_Swizzle;

typedef enum TinyImageFormat_DepthStencil_Type {
	TinyImageFormat_DEPTH_STENCIL_TYPE_NONE = 0ULL,
	TinyImageFormat_DEPTH_STENCIL_TYPE_UNORM = 1ULL,
	TinyImageFormat_DEPTH_STENCIL_TYPE_UINT = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_TYPE_SFLOAT = 3ULL,
} TinyImageFormat_DepthStencil_Type;

typedef enum TinyImageFormat_DXTC_Alpha {
	TinyImageFormat_DXTC_ALPHA_NONE = 0ULL,
	TinyImageFormat_DXTC_ALPHA_PUNCHTHROUGH = 1ULL,
	TinyImageFormat_DXTC_ALPHA_BLOCK = 2ULL,
	TinyImageFormat_DXTC_ALPHA_FULL = 3ULL,
} TinyImageFormat_DXTC_Alpha;

typedef enum TinyImageFormat_DXTC_Type {
	TinyImageFormat_DXTC_TYPE_UNORM = 0ULL,
	TinyImageFormat_DXTC_TYPE_SNORM = 1ULL,
	TinyImageFormat_DXTC_TYPE_SRGB = 2ULL,
	TinyImageFormat_DXTC_TYPE_SFLOAT = 3ULL,
	TinyImageFormat_DXTC_TYPE_UFLOAT = 4ULL,
} TinyImageFormat_DXTC_Type;

typedef enum TinyImageFormat_DXTC_BlockBytes {
	TinyImageFormat_DXTC_BLOCKBYTES_8 = 0ULL,
	TinyImageFormat_DXTC_BLOCKBYTES_16 = 1ULL,
} TinyImageFormat_DXTC_BlockBytes;

typedef enum TinyImageFormat_DXTC_ChannelCount {
	TinyImageFormat_DXTC_CHANNELCOUNT_1 = 0ULL,
	TinyImageFormat_DXTC_CHANNELCOUNT_2 = 1ULL,
	TinyImageFormat_DXTC_CHANNELCOUNT_3 = 2ULL,
	TinyImageFormat_DXTC_CHANNELCOUNT_4 = 3ULL,
} TinyImageFormat_DXTC_ChannelCount;

typedef enum TinyImageFormat_DXTC_ModeCount {
	TinyImageFormat_DXTC_MODECOUNT_1 = 0ULL,
	TinyImageFormat_DXTC_MODECOUNT_8 = 1ULL,
	TinyImageFormat_DXTC_MODECOUNT_14 = 2ULL,
} TinyImageFormat_DXTC_ModeCount;

typedef enum TinyImageFormat_PVRTC_Version {
	TinyImageFormat_PVRTC_VERSION_V1 = 0ULL,
	TinyImageFormat_PVRTC_VERSION_V2 = 1ULL,
} TinyImageFormat_PVRTC_Version;

typedef enum TinyImageFormat_PVRTC_Bits {
	TinyImageFormat_PVRTC_BITS_2 = 0ULL,
	TinyImageFormat_PVRTC_BITS_4 = 1ULL,
} TinyImageFormat_PVRTC_Bits;

typedef enum TinyImageFormat_PVRTC_Type {
	TinyImageFormat_PVRTC_TYPE_UNORM = 0ULL,
	TinyImageFormat_PVRTC_TYPE_SRGB = 1ULL,
} TinyImageFormat_PVRTC_Type;

typedef enum TinyImageFormat_ETC_Bits {
	TinyImageFormat_ETC_BITS_8 = 0ULL,
	TinyImageFormat_ETC_BITS_11 = 1ULL,
} TinyImageFormat_ETC_Bits;

typedef enum TinyImageFormat_ETC_Alpha {
	TinyImageFormat_ETC_ALPHA_NONE = 0ULL,
	TinyImageFormat_ETC_ALPHA_PUNCHTHROUGH = 1ULL,
	TinyImageFormat_ETC_ALPHA_BLOCK = 2ULL,
} TinyImageFormat_ETC_Alpha;

typedef enum TinyImageFormat_ETC_Type {
	TinyImageFormat_ETC_TYPE_UNORM = 0ULL,
	TinyImageFormat_ETC_TYPE_SNORM = 1ULL,
	TinyImageFormat_ETC_TYPE_SRGB = 2ULL,
} TinyImageFormat_ETC_Type;

typedef enum TinyImageFormat_ETC_ChannelCount {
	TinyImageFormat_ETC_CHANNELCOUNT_1 = 0ULL,
	TinyImageFormat_ETC_CHANNELCOUNT_2 = 1ULL,
	TinyImageFormat_ETC_CHANNELCOUNT_3 = 2ULL,
	TinyImageFormat_ETC_CHANNELCOUNT_4 = 3ULL,
} TinyImageFormat_ETC_ChannelCount;

typedef enum TinyImageFormat_ASTC_Size {
	TinyImageFormat_ASTC_SIZE_1 = 0ULL,
	TinyImageFormat_ASTC_SIZE_4 = 2ULL,
	TinyImageFormat_ASTC_SIZE_5 = 3ULL,
	TinyImageFormat_ASTC_SIZE_6 = 4ULL,
	TinyImageFormat_ASTC_SIZE_8 = 5ULL,
	TinyImageFormat_ASTC_SIZE_10 = 6ULL,
	TinyImageFormat_ASTC_SIZE_12 = 7ULL,
} TinyImageFormat_ASTC_Size;

typedef enum TinyImageFormat_ASTC_Type {
	TinyImageFormat_ASTC_TYPE_UNORM = 0ULL,
	TinyImageFormat_ASTC_TYPE_SRGB = 1ULL,
} TinyImageFormat_ASTC_Type;

typedef enum TinyImageFormat_CLUT_BlockSize {
	TinyImageFormat_CLUT_BLOCKSIZE_1 = 0ULL,
	TinyImageFormat_CLUT_BLOCKSIZE_2 = 1ULL,
	TinyImageFormat_CLUT_BLOCKSIZE_4 = 2ULL,
	TinyImageFormat_CLUT_BLOCKSIZE_8 = 3ULL,
} TinyImageFormat_CLUT_BlockSize;

typedef enum TinyImageFormat_CLUT_Bits {
	TinyImageFormat_CLUT_BITS_0 = 0ULL,
	TinyImageFormat_CLUT_BITS_1 = 1ULL,
	TinyImageFormat_CLUT_BITS_2 = 2ULL,
	TinyImageFormat_CLUT_BITS_4 = 3ULL,
	TinyImageFormat_CLUT_BITS_8 = 4ULL,
} TinyImageFormat_CLUT_Bits;

typedef enum TinyImageFormat_CLUT_Type {
	TinyImageFormat_CLUT_TYPE_NONE = 0ULL,
	TinyImageFormat_CLUT_TYPE_RGB = 1ULL,
	TinyImageFormat_CLUT_TYPE_SINGLE = 2ULL,
	TinyImageFormat_CLUT_TYPE_EXPLICIT_ALPHA = 3ULL,
} TinyImageFormat_CLUT_Type;

typedef enum TinyImageFormat_Bits {
	TinyImageFormat_NAMESPACE_REQUIRED_BITS = 12ULL,
	TinyImageFormat_NAMESPACE_MASK = (1 << TinyImageFormat_NAMESPACE_REQUIRED_BITS) - 1,

	TinyImageFormat_PACK_SPECIAL_REQUIRED_BITS = 3ULL,
	TinyImageFormat_PACK_BITS_REQUIRED_BITS = 5ULL,
	TinyImageFormat_PACK_SWIZZLE_REQUIRED_BITS = 3ULL,

	TinyImageFormat_PACK_TYPE_REQUIRED_BITS = 4ULL,
	TinyImageFormat_PACK_NUM_CHANNELS = 4ULL,

	TinyImageFormat_PACK_SPECIAL_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_PACK_BITS_SHIFT = (TinyImageFormat_PACK_SPECIAL_REQUIRED_BITS + TinyImageFormat_PACK_SPECIAL_SHIFT),
	TinyImageFormat_PACK_SWIZZLE_SHIFT =
	((TinyImageFormat_PACK_BITS_REQUIRED_BITS * TinyImageFormat_PACK_NUM_CHANNELS) + TinyImageFormat_PACK_BITS_SHIFT),
	TinyImageFormat_PACK_TYPE_SHIFT = ((TinyImageFormat_PACK_SWIZZLE_REQUIRED_BITS * TinyImageFormat_PACK_NUM_CHANNELS)
			+ TinyImageFormat_PACK_SWIZZLE_SHIFT),

	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_BITS_REQUIRED_BITS = 3ULL,
	TinyImageFormat_DEPTH_STENCIL_SWIZZLE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_TYPE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DEPTH_STENCIL_NUM_CHANNELS = 2ULL,

	TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_DEPTH_STENCIL_BITS_SHIFT =
	((TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_REQUIRED_BITS) + TinyImageFormat_DEPTH_STENCIL_TOTAL_SIZE_SHIFT),
	TinyImageFormat_DEPTH_STENCIL_SWIZZLE_SHIFT =
	((TinyImageFormat_DEPTH_STENCIL_BITS_REQUIRED_BITS * TinyImageFormat_DEPTH_STENCIL_NUM_CHANNELS)
			+ TinyImageFormat_DEPTH_STENCIL_BITS_SHIFT),
	TinyImageFormat_DEPTH_STENCIL_TYPE_SHIFT =
	((TinyImageFormat_DEPTH_STENCIL_SWIZZLE_REQUIRED_BITS * TinyImageFormat_DEPTH_STENCIL_NUM_CHANNELS)
			+ TinyImageFormat_DEPTH_STENCIL_SWIZZLE_SHIFT),

	TinyImageFormat_DXTC_ALPHA_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DXTC_TYPE_REQUIRED_BITS = 3ULL,
	TinyImageFormat_DXTC_BLOCKBYTES_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DXTC_CHANNELCOUNT_REQUIRED_BITS = 2ULL,
	TinyImageFormat_DXTC_MODECOUNT_REQUIRED_BITS = 3ULL,

	TinyImageFormat_DXTC_ALPHA_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_DXTC_TYPE_SHIFT = (TinyImageFormat_DXTC_ALPHA_REQUIRED_BITS + TinyImageFormat_DXTC_ALPHA_SHIFT),
	TinyImageFormat_DXTC_BLOCKBYTES_SHIFT = (TinyImageFormat_DXTC_TYPE_REQUIRED_BITS + TinyImageFormat_DXTC_TYPE_SHIFT),
	TinyImageFormat_DXTC_CHANNELCOUNT_SHIFT =
	(TinyImageFormat_DXTC_BLOCKBYTES_REQUIRED_BITS + TinyImageFormat_DXTC_BLOCKBYTES_SHIFT),
	TinyImageFormat_DXTC_MODECOUNT_SHIFT =
	(TinyImageFormat_DXTC_CHANNELCOUNT_REQUIRED_BITS + TinyImageFormat_DXTC_CHANNELCOUNT_SHIFT),

	TinyImageFormat_PVRTC_VERSION_REQUIRED_BITS = 2ULL,
	TinyImageFormat_PVRTC_BITS_REQUIRED_BITS = 2ULL,
	TinyImageFormat_PVRTC_TYPE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_PVRTC_VERSION_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_PVRTC_BITS_SHIFT =
	(TinyImageFormat_PVRTC_VERSION_REQUIRED_BITS + TinyImageFormat_PVRTC_VERSION_SHIFT),
	TinyImageFormat_PVRTC_TYPE_SHIFT = (TinyImageFormat_PVRTC_BITS_REQUIRED_BITS + TinyImageFormat_PVRTC_BITS_SHIFT),

	TinyImageFormat_ETC_BITS_REQUIRED_BITS = 2ULL,
	TinyImageFormat_ETC_ALPHA_REQUIRED_BITS = 2ULL,
	TinyImageFormat_ETC_TYPE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_ETC_CHANNELCOUNT_REQUIRED_BITS = 2ULL,
	TinyImageFormat_ETC_BITS_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_ETC_ALPHA_SHIFT = (TinyImageFormat_ETC_BITS_REQUIRED_BITS + TinyImageFormat_ETC_BITS_SHIFT),
	TinyImageFormat_ETC_TYPE_SHIFT = (TinyImageFormat_ETC_ALPHA_REQUIRED_BITS + TinyImageFormat_ETC_ALPHA_SHIFT),
	TinyImageFormat_ETC_CHANNELCOUNT_SHIFT = (TinyImageFormat_ETC_TYPE_REQUIRED_BITS + TinyImageFormat_ETC_TYPE_SHIFT),

	TinyImageFormat_ASTC_SIZE_REQUIRED_BITS = 3,
	TinyImageFormat_ASTC_TYPE_REQUIRED_BITS = 2ULL,
	TinyImageFormat_ASTC_NUM_DIMS = 3,
	TinyImageFormat_ASTC_SIZE_SHIFT = (TinyImageFormat_NAMESPACE_REQUIRED_BITS),
	TinyImageFormat_ASTC_TYPE_SHIFT =
			((TinyImageFormat_ASTC_SIZE_REQUIRED_BITS * TinyImageFormat_ASTC_NUM_DIMS) + TinyImageFormat_ASTC_SIZE_SHIFT),

	TinyImageFormat_CLUT_BLOCKSIZE_REQUIRED_BITS = 2,
	TinyImageFormat_CLUT_BITS_REQUIRED_BITS = 3,
	TinyImageFormat_CLUT_TYPE_REQUIRED_BITS = 2,
	TinyImageFormat_CLUT_NUM_CHANNELS = 2,

	TinyImageFormat_CLUT_BLOCKSIZE_SHIFT = TinyImageFormat_NAMESPACE_REQUIRED_BITS,
	TinyImageFormat_CLUT_BITS_SHIFT = TinyImageFormat_CLUT_BLOCKSIZE_REQUIRED_BITS + TinyImageFormat_CLUT_BLOCKSIZE_SHIFT,
	TinyImageFormat_CLUT_TYPE_SHIFT =
				((TinyImageFormat_CLUT_BITS_REQUIRED_BITS * TinyImageFormat_CLUT_NUM_CHANNELS) + TinyImageFormat_CLUT_BITS_SHIFT),

} TinyImageFormat_Bits;
