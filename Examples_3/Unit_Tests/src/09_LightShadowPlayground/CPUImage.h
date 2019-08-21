#pragma once
#include "../../../../Common_3/OS/Image/ImageEnums.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

#define ALL_MIPLEVELS 127

/************************************************************************************/
// Define some useful macros
#define MCHAR2(a, b) (a | (b << 8))
#define MAKE_CHAR4(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

class CPUImage
{
public:
	CPUImage();
	~CPUImage();

	bool loadImage(
		const char* fileName, void* pUserData = NULL, FSRoot root = FSR_Textures);

	void RedefineDimensions(const ImageFormat::Enum fmt, const int w, const int h, const int d, 
		const int mipMapCount, const int arraySize = 1, bool srgb = false);


	void SetPixels(unsigned char* pixelData, bool own = false)
	{
		mOwnsMemory = own;
		pData = pixelData;
	}

	void Destroy();
	void Clear();
	uint                 GetWidth() const { return mWidth; }
	uint                 GetHeight() const { return mHeight; }
	uint                 GetDepth() const { return mDepth; }
	uint                 GetWidth(const int mipMapLevel) const;
	uint                 GetHeight(const int mipMapLevel) const;
	uint                 GetDepth(const int mipMapLevel) const;
	unsigned char* GetPixels(const uint mipMapLevel) const;


	uint                 GetArraySliceSize(const uint mipMapLevel = 0, ImageFormat::Enum srcFormat = ImageFormat::NONE) const;

	unsigned char* GetPixels() const { return pData; }

	bool           Uncompress();

	uint GetMipMappedSize(
		const uint firstMipLevel = 0, uint numMipLevels = ALL_MIPLEVELS, ImageFormat::Enum srcFormat = ImageFormat::NONE) const;

	typedef bool(*CPUImageLoaderFunction)(
		CPUImage* pImage, const char* memory, uint32_t memSize, void* pUserData);
	static void AddImageLoader(const char* pExtension, CPUImageLoaderFunction pFunc);
private:
	unsigned char*    pData;
	eastl::string	  mLoadFileName;
	uint              mWidth, mHeight, mDepth;
	uint              mMipMapCount;
	uint              mArrayCount;
	ImageFormat::Enum mFormat;
	int               mAdditionalDataSize;
	unsigned char*    pAdditionalData;
	bool              mLinearLayout;
	bool              mSrgb;
	bool              mOwnsMemory;
};