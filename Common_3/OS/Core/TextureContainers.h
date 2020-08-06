#pragma once

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"

#include "../../Renderer/IRenderer.h"

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "../../ThirdParty/OpenSource/tinyktx/tinyktx.h"

#include "../../ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

/************************************************************************/
// Surface Utils
/************************************************************************/
static inline bool util_get_surface_info(
	uint32_t width,
	uint32_t height,
	TinyImageFormat fmt,
	uint32_t* outNumBytes,
	uint32_t* outRowBytes,
	uint32_t* outNumRows)
{
	uint64_t numBytes = 0;
	uint64_t rowBytes = 0;
	uint64_t numRows = 0;

	uint32_t bpp = TinyImageFormat_BitSizeOfBlock(fmt);
	bool compressed = TinyImageFormat_IsCompressed(fmt);
	// #TODO
	bool packed = false;
	bool planar = false;

	if (compressed)
	{
		uint32_t blockWidth = TinyImageFormat_WidthOfBlock(fmt);
		uint32_t blockHeight = TinyImageFormat_HeightOfBlock(fmt);
		uint32_t numBlocksWide = 0;
		uint32_t numBlocksHigh = 0;
		if (width > 0)
		{
			numBlocksWide = max(1U, (width + (blockWidth - 1)) / blockWidth);
		}
		if (height > 0)
		{
			numBlocksHigh = max(1u, (height + (blockHeight - 1)) / blockHeight);
		}

		rowBytes = numBlocksWide * (bpp >> 3);
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if (packed)
	{
		LOGF(eERROR, "Not implemented");
		return false;
		//rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		//numRows = uint64_t(height);
		//numBytes = rowBytes * height;
	}
	//else if (dxgiFormat == DXGI_FORMAT_NV11)
	//{
	//	rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
	//	numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
	//	numBytes = rowBytes * numRows;
	//}
	else if (planar)
	{
		LOGF(eERROR, "Not implemented");
		return false;
		//rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		//numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
		//numRows = height + ((uint64_t(height) + 1u) >> 1);
	}
	else
	{
		if (!bpp)
			return false;

		rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
		numRows = uint64_t(height);
		numBytes = rowBytes * height;
	}

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
	static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
	if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
		return false;
#else
	static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

	if (outNumBytes)
	{
		*outNumBytes = (uint32_t)numBytes;
	}
	if (outRowBytes)
	{
		*outRowBytes = (uint32_t)rowBytes;
	}
	if (outNumRows)
	{
		*outNumRows = (uint32_t)numRows;
	}

	return true;
}

static inline uint32_t util_get_surface_size(
	TinyImageFormat format,
	uint32_t width, uint32_t height, uint32_t depth,
	uint32_t rowStride, uint32_t sliceStride,
	uint32_t baseMipLevel, uint32_t mipLevels,
	uint32_t baseArrayLayer, uint32_t arrayLayers)
{
	uint32_t requiredSize = 0;
	for (uint32_t s = baseArrayLayer; s < baseArrayLayer + arrayLayers; ++s)
	{
		uint32_t w = width;
		uint32_t h = height;
		uint32_t d = depth;

		for (uint32_t m = baseMipLevel; m < baseMipLevel + mipLevels; ++m)
		{
			uint32_t rowBytes = 0;
			uint32_t numRows = 0;
			if (!util_get_surface_info(w, h, format, NULL, &rowBytes, &numRows))
			{
				return false;
			}

			requiredSize += round_up(d * round_up(rowBytes, rowStride) * numRows, sliceStride);

			w = w >> 1;
			h = h >> 1;
			d = d >> 1;
			if (w == 0)
			{
				w = 1;
			}
			if (h == 0)
			{
				h = 1;
			}
			if (d == 0)
			{
				d = 1;
			}
		}
	}

	return requiredSize;
}
/************************************************************************/
// DDS Loading
/************************************************************************/
//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//
// See DDS.h in the 'Texconv' sample and the 'DirectXTex' library
//--------------------------------------------------------------------------------------
#pragma pack(push,1)

const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

struct DDS_PIXELFORMAT
{
	uint32_t    size;
	uint32_t    flags;
	uint32_t    fourCC;
	uint32_t    RGBBitCount;
	uint32_t    RBitMask;
	uint32_t    GBitMask;
	uint32_t    BBitMask;
	uint32_t    ABitMask;
};

#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
#define DDS_RGB         0x00000040  // DDPF_RGB
#define DDS_LUMINANCE   0x00020000  // DDPF_LUMINANCE
#define DDS_ALPHA       0x00000002  // DDPF_ALPHA
#define DDS_BUMPDUDV    0x00080000  // DDPF_BUMPDUDV

#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH

#define DDS_HEIGHT 0x00000002 // DDSD_HEIGHT

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

enum DDS_MISC_FLAGS2
{
	DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
};

struct DDS_HEADER
{
	uint32_t        size;
	uint32_t        flags;
	uint32_t        height;
	uint32_t        width;
	uint32_t        pitchOrLinearSize;
	uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
	uint32_t        mipMapCount;
	uint32_t        reserved1[11];
	DDS_PIXELFORMAT ddspf;
	uint32_t        caps;
	uint32_t        caps2;
	uint32_t        caps3;
	uint32_t        caps4;
	uint32_t        reserved2;
};

struct DDS_HEADER_DXT10
{
	TinyImageFormat_DXGI_FORMAT     dxgiFormat;
	uint32_t                        resourceDimension;
	uint32_t                        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
	uint32_t                        arraySize;
	uint32_t                        miscFlags2;
};

#pragma pack(pop)

#define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

static constexpr TinyImageFormat_DXGI_FORMAT util_get_dxgi_format(const DDS_PIXELFORMAT& ddpf) noexcept
{
	if (ddpf.flags & DDS_RGB)
	{
		// Note that sRGB formats are written using the "DX10" extended header

		switch (ddpf.RGBBitCount)
		{
		case 32:
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
			{
				return TIF_DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
			{
				return TIF_DXGI_FORMAT_B8G8R8A8_UNORM;
			}

			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0))
			{
				return TIF_DXGI_FORMAT_B8G8R8X8_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0) aka D3DFMT_X8B8G8R8

			// Note that many common DDS reader/writers (including D3DX) swap the
			// the RED/BLUE masks for 10:10:10:2 formats. We assume
			// below that the 'backwards' header mask is being used since it is most
			// likely written by D3DX. The more robust solution is to use the 'DX10'
			// header extension and specify the TIF_DXGI_FORMAT_R10G10B10A2_UNORM format directly

			// For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
			if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
			{
				return TIF_DXGI_FORMAT_R10G10B10A2_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

			if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
			{
				return TIF_DXGI_FORMAT_R16G16_UNORM;
			}

			if (ISBITMASK(0xffffffff, 0, 0, 0))
			{
				// Only 32-bit color channel format in D3D9 was R32F
				return TIF_DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
			}
			break;

		case 24:
			// No 24bpp DXGI formats aka D3DFMT_R8G8B8
			break;

		case 16:
			if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
			{
				return TIF_DXGI_FORMAT_B5G5R5A1_UNORM;
			}
			if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0))
			{
				return TIF_DXGI_FORMAT_B5G6R5_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0) aka D3DFMT_X1R5G5B5

			if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
			{
				return TIF_DXGI_FORMAT_B4G4R4A4_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0) aka D3DFMT_X4R4G4B4

			// No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
			break;
		}
	}
	else if (ddpf.flags & DDS_LUMINANCE)
	{
		if (8 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0xff, 0, 0, 0))
			{
				return TIF_DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}

			// No DXGI format maps to ISBITMASK(0x0f,0,0,0xf0) aka D3DFMT_A4L4

			if (ISBITMASK(0x00ff, 0, 0, 0xff00))
			{
				return TIF_DXGI_FORMAT_R8G8_UNORM; // Some DDS writers assume the bitcount should be 8 instead of 16
			}
		}

		if (16 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0xffff, 0, 0, 0))
			{
				return TIF_DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
			if (ISBITMASK(0x00ff, 0, 0, 0xff00))
			{
				return TIF_DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
		}
	}
	else if (ddpf.flags & DDS_ALPHA)
	{
		if (8 == ddpf.RGBBitCount)
		{
			return TIF_DXGI_FORMAT_A8_UNORM;
		}
	}
	else if (ddpf.flags & DDS_BUMPDUDV)
	{
		if (16 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x00ff, 0xff00, 0, 0))
			{
				return TIF_DXGI_FORMAT_R8G8_SNORM; // D3DX10/11 writes this out as DX10 extension
			}
		}

		if (32 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
			{
				return TIF_DXGI_FORMAT_R8G8B8A8_SNORM; // D3DX10/11 writes this out as DX10 extension
			}
			if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
			{
				return TIF_DXGI_FORMAT_R16G16_SNORM; // D3DX10/11 writes this out as DX10 extension
			}

			// No DXGI format maps to ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000) aka D3DFMT_A2W10V10U10
		}

		// No DXGI format maps to DDPF_BUMPLUMINANCE aka D3DFMT_L6V5U5, D3DFMT_X8L8V8U8
	}
	else if (ddpf.flags & DDS_FOURCC)
	{
		if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC1_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC2_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC3_UNORM;
		}

		// While pre-multiplied alpha isn't directly supported by the DXGI formats,
		// they are basically the same as these BC formats so they can be mapped
		if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC2_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC3_UNORM;
		}

		if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC4_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC4_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC4_SNORM;
		}

		if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC5_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC5_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_BC5_SNORM;
		}

		// BC6H and BC7 are written using the "DX10" extended header

		if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_R8G8_B8G8_UNORM;
		}
		if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_G8R8_G8B8_UNORM;
		}

		if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
		{
			return TIF_DXGI_FORMAT_YUY2;
		}

		// Check for D3DFORMAT enums being set here
		switch (ddpf.fourCC)
		{
		case 36: // D3DFMT_A16B16G16R16
			return TIF_DXGI_FORMAT_R16G16B16A16_UNORM;

		case 110: // D3DFMT_Q16W16V16U16
			return TIF_DXGI_FORMAT_R16G16B16A16_SNORM;

		case 111: // D3DFMT_R16F
			return TIF_DXGI_FORMAT_R16_FLOAT;

		case 112: // D3DFMT_G16R16F
			return TIF_DXGI_FORMAT_R16G16_FLOAT;

		case 113: // D3DFMT_A16B16G16R16F
			return TIF_DXGI_FORMAT_R16G16B16A16_FLOAT;

		case 114: // D3DFMT_R32F
			return TIF_DXGI_FORMAT_R32_FLOAT;

		case 115: // D3DFMT_G32R32F
			return TIF_DXGI_FORMAT_R32G32_FLOAT;

		case 116: // D3DFMT_A32B32G32R32F
			return TIF_DXGI_FORMAT_R32G32B32A32_FLOAT;

			// No DXGI format maps to D3DFMT_CxV8U8
		}
	}

	return TIF_DXGI_FORMAT_UNKNOWN;
}

static bool loadDDSTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
#define RETURN_IF_FAILED(exp) \
if (!(exp))                   \
{                             \
	return false;             \
}

#ifndef DIRECT3D12
	enum D3D12_RESOURCE_DIMENSION
	{
		D3D12_RESOURCE_DIMENSION_UNKNOWN = 0,
		D3D12_RESOURCE_DIMENSION_BUFFER = 1,
		D3D12_RESOURCE_DIMENSION_TEXTURE1D = 2,
		D3D12_RESOURCE_DIMENSION_TEXTURE2D = 3,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D = 4
	};
#endif

	RETURN_IF_FAILED(pStream);

	ssize_t ddsDataSize = fsGetStreamFileSize(pStream);
	RETURN_IF_FAILED(ddsDataSize <= UINT32_MAX);
	RETURN_IF_FAILED((ddsDataSize > (sizeof(uint32_t) + sizeof(DDS_HEADER))));

	// DDS files always start with the same magic number ("DDS ")
	uint32_t dwMagicNumber = 0;
	fsReadFromStream(pStream, &dwMagicNumber, sizeof(dwMagicNumber));
	RETURN_IF_FAILED(dwMagicNumber == DDS_MAGIC);

	DDS_HEADER headerStruct = {};
	DDS_HEADER_DXT10 hdrDx10Struct = {};
	DDS_HEADER* header = &headerStruct;
	DDS_HEADER_DXT10* d3d10ext = NULL;

	ssize_t bytesRead = fsReadFromStream(pStream, header, sizeof(DDS_HEADER));
	RETURN_IF_FAILED(bytesRead == sizeof(DDS_HEADER));

	// Verify header to validate DDS file
	RETURN_IF_FAILED(header->size == sizeof(DDS_HEADER) || header->ddspf.size == sizeof(DDS_PIXELFORMAT));

	// Check for DX10 extension
	if ((header->ddspf.flags & DDS_FOURCC) &&
		(MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		// Must be long enough for both headers and magic value
		RETURN_IF_FAILED(ddsDataSize >= (sizeof(DDS_HEADER) + sizeof(uint32_t) + sizeof(DDS_HEADER_DXT10)));
		d3d10ext = &hdrDx10Struct;
		bytesRead = fsReadFromStream(pStream, d3d10ext, sizeof(DDS_HEADER_DXT10));
		RETURN_IF_FAILED(bytesRead == sizeof(DDS_HEADER_DXT10));
	}

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = header->width;
	textureDesc.mHeight = header->height;
	textureDesc.mDepth = header->depth;
	textureDesc.mMipLevels = max(1U, header->mipMapCount);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;

	if (d3d10ext)
	{
		textureDesc.mArraySize = d3d10ext->arraySize;
		RETURN_IF_FAILED(textureDesc.mArraySize);

		textureDesc.mFormat = TinyImageFormat_FromDXGI_FORMAT(d3d10ext->dxgiFormat);

		switch (d3d10ext->dxgiFormat)
		{
		case TIF_DXGI_FORMAT_AI44:
		case TIF_DXGI_FORMAT_IA44:
		case TIF_DXGI_FORMAT_P8:
		case TIF_DXGI_FORMAT_A8P8:
			RETURN_IF_FAILED(false);
		default:
			RETURN_IF_FAILED(TinyImageFormat_BitSizeOfBlock(textureDesc.mFormat) != 0);
		}

		switch (d3d10ext->resourceDimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			// D3DX writes 1D textures with a fixed Height of 1
			if ((header->flags & DDS_HEIGHT) && textureDesc.mHeight != 1)
			{
				RETURN_IF_FAILED(false);
			}
			textureDesc.mHeight = textureDesc.mDepth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (d3d10ext->miscFlag & 0x4 /* RESOURCE_MISC_TEXTURECUBE */)
			{
				textureDesc.mArraySize *= 6;
				textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
			}
			textureDesc.mDepth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
			{
				RETURN_IF_FAILED(false);
			}

			if (textureDesc.mArraySize > 1)
			{
				RETURN_IF_FAILED(false);
			}
			break;

		default:
			RETURN_IF_FAILED(false);
		}
	}
	else
	{
		textureDesc.mArraySize = 1;

		TinyImageFormat_DXGI_FORMAT dxgiFormat = util_get_dxgi_format(header->ddspf);
		textureDesc.mFormat = TinyImageFormat_FromDXGI_FORMAT(dxgiFormat);
		RETURN_IF_FAILED(textureDesc.mFormat != TinyImageFormat_UNDEFINED);
		RETURN_IF_FAILED(TinyImageFormat_BitSizeOfBlock(textureDesc.mFormat) != 0);

		if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
		{
			if (header->caps2 & DDS_CUBEMAP)
			{
				// We require all six faces to be defined
				if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
				{
					RETURN_IF_FAILED(false);
				}

				textureDesc.mArraySize = 6;
				textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
			}

			textureDesc.mDepth = 1;
			// Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
		}
	}

	return true;
}
/************************************************************************/
// KTX Loading
/************************************************************************/
static bool loadKTXTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
#define RETURN_IF_FAILED(exp) \
if (!(exp))                   \
{                             \
	return false;             \
}

	RETURN_IF_FAILED(pStream);

	ssize_t ktxDataSize = fsGetStreamFileSize(pStream);
	RETURN_IF_FAILED(ktxDataSize <= UINT32_MAX);

	TinyKtx_Callbacks callbacks
	{
		[](void* user, char const* msg) { LOGF(eERROR, msg); },
		[](void* user, size_t size) { return tf_malloc(size); },
		[](void* user, void* memory) { tf_free(memory); },
		[](void* user, void* buffer, size_t byteCount) { return fsReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
		[](void* user, int64_t offset) { return fsSeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
		[](void *user) { return (int64_t)fsGetStreamSeekPosition((FileStream*)user); }
	};

	TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, (void*)pStream);
	bool headerOkay = TinyKtx_ReadHeader(ctx);
	if (!headerOkay)
	{
		TinyKtx_DestroyContext(ctx);
		return false;
	}

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = TinyKtx_Width(ctx);
	textureDesc.mHeight = TinyKtx_Height(ctx);
	textureDesc.mDepth = max(1U, TinyKtx_Depth(ctx));
	textureDesc.mArraySize = max(1U, TinyKtx_ArraySlices(ctx));
	textureDesc.mMipLevels = max(1U, TinyKtx_NumberOfMipmaps(ctx));
	textureDesc.mFormat = TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(ctx));
	if (textureDesc.mFormat == TinyImageFormat_UNDEFINED)
	{
		TinyKtx_DestroyContext(ctx);
		return false;
	}

	if (TinyKtx_IsCubemap(ctx))
	{
		textureDesc.mArraySize *= 6;
		textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
	}

	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;

	TinyKtx_DestroyContext(ctx);

	return true;
}
/************************************************************************/
// BASIS Loading
/************************************************************************/
static bool loadBASISTextureDesc(FileStream* pStream, TextureDesc* pOutDesc, void** ppOutData, uint32_t* pOutDataSize)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) <= 0)
		return false;

	basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);

	size_t memSize = (size_t)fsGetStreamFileSize(pStream);
	void* basisData = tf_malloc(memSize);
	fsReadFromStream(pStream, basisData, memSize);

	basist::basisu_transcoder decoder(&sel_codebook);

	basist::basisu_file_info fileinfo;
	if (!decoder.get_file_info(basisData, (uint32_t)memSize, fileinfo))
	{
		LOGF(LogLevel::eERROR, "Failed retrieving Basis file information!");
		return false;
	}

	ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	ASSERT(fileinfo.m_total_images == decoder.get_total_images(basisData, (uint32_t)memSize));

	basist::basisu_image_info imageinfo;
	decoder.get_image_info(basisData, (uint32_t)memSize, imageinfo, 0);

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = imageinfo.m_width;
	textureDesc.mHeight = imageinfo.m_height;
	textureDesc.mDepth = 1;
	textureDesc.mMipLevels = fileinfo.m_image_mipmap_levels[0];
	textureDesc.mArraySize = fileinfo.m_total_images;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mFormat = TinyImageFormat_UNDEFINED;

	bool isNormalMap;

	if (fileinfo.m_userdata0 == 1)
		isNormalMap = true;
	else
		isNormalMap = false;

	basist::transcoder_texture_format basisTextureFormat = basist::transcoder_texture_format::cTFTotalTextureFormats;

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#if defined(TARGET_IOS)
	// Use PVRTC on iOS whenever possible
	// This makes sure that PVRTC support is maintained
	if (isPowerOf2(textureDesc.mWidth) && isPowerOf2(textureDesc.mHeight))
	{
		textureDesc.mFormat = TinyImageFormat_PVRTC1_4BPP_UNORM;
		basisTextureFormat = imageinfo.m_alpha_flag ? basist::transcoder_texture_format::cTFPVRTC1_4_RGB : basist::transcoder_texture_format::cTFPVRTC1_4_RGBA;
	}
#endif
	if (TinyImageFormat_UNDEFINED == textureDesc.mFormat)
	{
		textureDesc.mFormat = TinyImageFormat_ASTC_4x4_UNORM;
		basisTextureFormat = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
	}
#else
	if (!isNormalMap)
	{
		if (!imageinfo.m_alpha_flag)
		{
			textureDesc.mFormat = TinyImageFormat_DXBC7_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC7_M6_RGB;
		}
		else
		{
			textureDesc.mFormat = TinyImageFormat_DXBC7_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC7_M5;
		}
	}
	else
	{
		textureDesc.mFormat = TinyImageFormat_DXBC5_UNORM;
		basisTextureFormat = basist::transcoder_texture_format::cTFBC5_RG;
	}
#endif

	decoder.start_transcoding(basisData, (uint32_t)memSize);

	uint32_t requiredSize = util_get_surface_size(textureDesc.mFormat,
		textureDesc.mWidth, textureDesc.mHeight, textureDesc.mDepth, 1, 1,
		0, textureDesc.mMipLevels,
		0, textureDesc.mArraySize);
	void* startData = tf_malloc(requiredSize);
	uint8_t* data = (uint8_t*)startData;

	for (uint32_t s = 0; s < fileinfo.m_total_images; ++s)
	{
		uint32_t w = textureDesc.mWidth;
		uint32_t h = textureDesc.mHeight;
		uint32_t d = textureDesc.mDepth;

		for (uint32_t m = 0; m < fileinfo.m_image_mipmap_levels[s]; ++m)
		{
			uint32_t rowPitch = 0;
			uint32_t numBytes = 0;
			if (!util_get_surface_info(w, h, textureDesc.mFormat, &numBytes, &rowPitch, NULL))
			{
				return false;
			}

			uint32_t rowPitchInBlocks = rowPitch / (TinyImageFormat_BitSizeOfBlock(textureDesc.mFormat) >> 3);
			basist::basisu_image_level_info level_info;

			if (!decoder.get_image_level_info(basisData, (uint32_t)memSize, level_info, s, m))
			{
				LOGF(LogLevel::eERROR, "Failed retrieving image level information (%u %u)!\n", s, m);
				tf_free(basisData);
				tf_free(startData);
				return false;
			}

			if (!decoder.transcode_image_level(basisData, (uint32_t)memSize, s, m, data,
				(uint32_t)(rowPitchInBlocks * imageinfo.m_num_blocks_y), basisTextureFormat, 0, rowPitchInBlocks))
			{
				LOGF(LogLevel::eERROR, "Failed transcoding image level (%u %u)!", s, m);
				tf_free(basisData);
				tf_free(startData);
				return false;
			}

			data += numBytes;

			w = w >> 1;
			h = h >> 1;
			d = d >> 1;
			if (w == 0)
			{
				w = 1;
			}
			if (h == 0)
			{
				h = 1;
			}
			if (d == 0)
			{
				d = 1;
			}
		}
	}

	tf_free(basisData);

	*ppOutData = startData;
	*pOutDataSize = requiredSize;

	return true;
}
/************************************************************************/
// SVT Loading
/************************************************************************/
struct SVT_HEADER
{
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mMipLevels;
	uint32_t mPageSize;
	uint32_t mComponentCount;
};

#if defined(DIRECT3D12) || defined(VULKAN)
static bool loadSVTTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
#define RETURN_IF_FAILED(exp) \
if (!(exp))                   \
{                             \
	return false;             \
}

	RETURN_IF_FAILED(pStream);

	ssize_t svtDataSize = fsGetStreamFileSize(pStream);
	RETURN_IF_FAILED(svtDataSize <= UINT32_MAX);
	RETURN_IF_FAILED((svtDataSize > (sizeof(SVT_HEADER))));

	SVT_HEADER header = {};
	ssize_t bytesRead = fsReadFromStream(pStream, &header, sizeof(SVT_HEADER));
	RETURN_IF_FAILED(bytesRead == sizeof(SVT_HEADER));

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = header.mWidth;
	textureDesc.mHeight = header.mHeight;
	textureDesc.mMipLevels = header.mMipLevels;
	textureDesc.mDepth = 1;
	textureDesc.mArraySize = 1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;

	return true;
}
#endif
