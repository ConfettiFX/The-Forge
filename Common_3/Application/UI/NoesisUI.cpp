#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../../Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../../Common_3/Application/Interfaces/IInput.h"
#include <NsGui/IntegrationAPI.h>
#include <NsRender/RenderDevice.h>
#include <NsGui/XamlProvider.h>
#include <NsGui/CachedFontProvider.h>
#include <NsGui/Stream.h>
#include <NsGui/Uri.h>
#include <NsGui/IView.h>
#include <NsGui/IRenderer.h>
#include <NsGui/FrameworkElement.h>
#include <NsRender/RenderTarget.h>
#include <NsRender/Texture.h>
#include <NsGui/TextureProvider.h>
#include "NoesisUI.h"

static Noesis::MouseButton mouseButtonToNoesis(uint32_t action);
static Noesis::Key keyToNoesis(uint32_t action, bool gamepad);

// Note: This is a Noesis class to replace the filesystem
class ForgeStream final : public Noesis::Stream
{
public:

	ForgeStream(ResourceDirectory resourceDir, const char* path)
	{
		bool res = fsOpenStreamFromPath(resourceDir, path, FM_READ_BINARY, nullptr, &mStream);
		ASSERT(res);
	}

	virtual ~ForgeStream()
	{
		Close();
	}

	/// Set the current position within the stream
	virtual void SetPosition(uint32_t pos) override
	{
		bool res = fsSeekStream(&mStream, SBO_START_OF_FILE, pos);
		ASSERT(res);
	}

	/// Returns the current position within the stream
	virtual uint32_t GetPosition() const override
	{
		return (uint32_t)fsGetStreamSeekPosition(&mStream);
	}

	/// Returns the length of the stream in bytes
	virtual uint32_t GetLength() const override
	{
		return (uint32_t)fsGetStreamFileSize(&mStream);
	}

	/// Reads data at the current position and advances it by the number of bytes read
	/// Returns the total number of bytes read. This can be less than the number of bytes requested
	virtual uint32_t Read(void* buffer, uint32_t size) override
	{
		return (uint32_t)fsReadFromStream(&mStream, buffer, size);
	}

	/// Closes the current stream and releases any resources associated with the current stream
	virtual void Close() override
	{
		if (!mStream.pIO)
			return;

		bool res = fsCloseStream(&mStream);
		mStream = {};
		ASSERT(res);
	}

private:

	FileStream mStream{};
};

// Note: This is a Noesis class to load XAML files
class ForgeXamlProvider final : public Noesis::XamlProvider
{
public:

	/// Loads XAML from the specified URI. Returns null when xaml is not found
	virtual Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override
	{
		return Noesis::MakePtr<ForgeStream>(RD_OTHER_FILES, uri.Str());
	}
};

// Note: This is a Noesis class to register and load fonts
class ForgeFontProvider final : public Noesis::CachedFontProvider
{
public:

	/// This function is called the first time a font is attempted to be loaded
	/// Note: This function does not actually scan a folder, it just loads the single font named after the folder.
	virtual void ScanFolder(const Noesis::Uri& folder) override
	{
		bstring filename = bdynfromcstr(folder.Str());
		bstring findPath = bdynfromcstr("Fonts/");
		bstring replacePath = bempty();
		bfindreplace(&filename, &findPath, &replacePath, 0);

		char fontName[FS_MAX_PATH]{};
		const char* data = bdata(&filename);

		if (data)
			strcpy(fontName, data);

		bstring extStr = bdynfromcstr(".ttf");
		bconcat(&filename, &extStr);

		bstring path = bdynfromcstr(fontName);
		bstring slash = bdynfromcstr("/");
		bconcat(&path, &slash);
		bconcat(&path, &filename);

		RegisterFont(folder, bdata(&filename), 0, fontName, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

		bdestroy(&findPath);
		bdestroy(&extStr);
		bdestroy(&filename);
		bdestroy(&path);
		bdestroy(&slash);
	}

	/// Returns a stream to a previously registered filename
	virtual Noesis::Ptr<Noesis::Stream> OpenFont(const Noesis::Uri& folder, const char* filename) const override
	{
		bstring path = bdynfromcstr(folder.Str());
		bstring slash = bdynfromcstr("/");
		bstring file = bdynfromcstr(filename);
		
		bconcat(&path, &slash);
		bconcat(&path, &file);

		Noesis::Ptr<Noesis::Stream> stream = Noesis::MakePtr<ForgeStream>(RD_FONTS, bdata(&path));

		bdestroy(&path);
		bdestroy(&slash);
		bdestroy(&file);

		return stream;
	}
};

// Note: This is a Noesis class for a texture
class ForgeTexture final : public Noesis::Texture
{
public:

	ForgeTexture(const char* label, uint32_t width, uint32_t height, uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data)
	{
		ASSERT(!data);

		TextureLoadDesc texDesc{};
		texDesc.ppTexture = &pTexture;
		TextureDesc desc{};
		desc.pName = label;
		desc.mWidth = width;
		desc.mHeight = height;
		desc.mMipLevels = numLevels;
		desc.mArraySize = 1;
		desc.mDepth = 1;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mStartState = RESOURCE_STATE_COMMON;
		
		switch (format)
		{
		case Noesis::TextureFormat::Enum::RGBA8:
			desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
			break;

		case Noesis::TextureFormat::Enum::R8:
			desc.mFormat = TinyImageFormat_R8_UNORM;
			break;

		default:
			ASSERT(0);
			break;
		}

		texDesc.pDesc = &desc;

		SyncToken token{};
		addResource(&texDesc, &token);
		waitForToken(&token);

		mFormat = desc.mFormat;
		pName = label;
		mOwns = true;
	}

	ForgeTexture(char const* name)
	{
		TextureLoadDesc texDesc{};
		texDesc.pFileName = name;
		texDesc.ppTexture = &pTexture;
		texDesc.mContainer = TEXTURE_CONTAINER_KTX;

		SyncToken token{};
		addResource(&texDesc, &token);
		waitForToken(&token);

		mFormat = TinyImageFormat_UNDEFINED;
		pName = name;
		mOwns = true;
	}

	ForgeTexture(::Texture* pTexture, TinyImageFormat format)
	{
		this->pTexture = pTexture;
		mFormat = format;
		mOwns = false;
	}

	virtual ~ForgeTexture()
	{
		if (mOwns)
		{
			removeResource(pTexture);

			if (pData)
				tf_free(pData);
		}
	}

	/// Returns the width of the texture, in pixels
	virtual uint32_t GetWidth() const override
	{
		return pTexture->mWidth;
	}

	/// Returns the height of the texture, in pixels
	virtual uint32_t GetHeight() const override
	{
		return pTexture->mHeight;
	}

	/// Returns true if the texture has mipmaps
	virtual bool HasMipMaps() const override
	{
		return pTexture->mMipLevels > 1;
	}

	/// Returns true when texture must be vertically inverted when mapped. This is true for surfaces
	/// on platforms (OpenGL) where texture V coordinate is zero at the "bottom of the texture"
	virtual bool IsInverted() const override
	{
		return false;
	}

	/// Returns true if the texture has an alpha channel that is not completely white. Just a hint
	/// to optimize rendering as alpha blending can be disabled when there is not transparency.
	virtual bool HasAlpha() const override
	{
		return mFormat == TinyImageFormat_R8G8B8A8_UNORM;
	}

	::Texture* pTexture = nullptr;
	TinyImageFormat mFormat = TinyImageFormat_UNDEFINED;
	const char* pName = "";
	bool mOwns = true;
	uint8_t* pData = nullptr;
	uint32_t mSize = 0;
};

// Note: This is a Noesis class to load textures
class ForgeTextureProvider final : public Noesis::TextureProvider
{
public:

	/// Returns metadata for the texture at the given URI. 0 x 0 is returned if texture is not found
	virtual Noesis::TextureInfo GetTextureInfo(const Noesis::Uri& uri) override
	{
		Noesis::Ptr<Noesis::Texture> texture = Noesis::MakePtr<ForgeTexture>(uri.Str());
		ForgeTexture* forgeTexture = (ForgeTexture*)texture.GetPtr();

		Noesis::TextureInfo info{};
		info.width = forgeTexture->pTexture->mWidth;
		info.height = forgeTexture->pTexture->mHeight;

		return info;
	}

	/// Returns a texture compatible with the given device or null if texture is not found
	virtual Noesis::Ptr<Noesis::Texture> LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device) override
	{
		Noesis::Ptr<Noesis::Texture> texture = Noesis::MakePtr<ForgeTexture>(uri.Str());

		return texture;
	}
};

// Note: This is a Noesis class to load and handle render targets
class ForgeRenderTarget final : public Noesis::RenderTarget
{
public:

	static Renderer* pRenderer;

	ForgeRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sampleCount, bool needsStencil, Renderer* pRenderer, TinyImageFormat format)
	{
		ASSERT(sampleCount == 1);

		this->pRenderer = pRenderer;

		RenderTargetDesc targetDesc{};
		targetDesc.mArraySize = 1;
		targetDesc.mClearValue.r = 0.0f;
		targetDesc.mClearValue.g = 0.0f;
		targetDesc.mClearValue.b = 0.0f;
		targetDesc.mClearValue.a = 0.0f;
		targetDesc.mDepth = 1;
		targetDesc.mFormat = format;
		targetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		targetDesc.mHeight = height;
		targetDesc.mSampleCount = SAMPLE_COUNT_1;
		targetDesc.mSampleQuality = 0;
		targetDesc.mWidth = width;
		targetDesc.pName = label;
		addRenderTarget(pRenderer, &targetDesc, &pTarget);

		pTexture = Noesis::MakePtr<ForgeTexture>(pTarget->pTexture, targetDesc.mFormat).GiveOwnership();

		if (needsStencil)
		{
			// Add depth buffer
			RenderTargetDesc stencilDesc{};
			stencilDesc.mArraySize = 1;
			stencilDesc.mClearValue.depth = 0.0f;
			stencilDesc.mClearValue.stencil = 0;
			stencilDesc.mDepth = 1;
			stencilDesc.mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
			stencilDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
			stencilDesc.mHeight = height;
			stencilDesc.mSampleCount = SAMPLE_COUNT_1;
			stencilDesc.mSampleQuality = 0;
			stencilDesc.mWidth = width;
			addRenderTarget(pRenderer, &stencilDesc, &pStencilTarget);
		}

		mOwns = true;
	}

	ForgeRenderTarget(::RenderTarget* pRenderTarget, Renderer* pRenderer, TinyImageFormat format)
	{
		this->pRenderer = pRenderer;
		pTarget = pRenderTarget;
		pTexture = Noesis::MakePtr<ForgeTexture>(pTarget->pTexture, format).GiveOwnership();
		mOwns = false;
	}

	virtual ~ForgeRenderTarget()
	{
		if (pStencilTarget)
			removeRenderTarget(pRenderer, pStencilTarget);

		if (pTexture)
			pTexture->Release();

		if (mOwns)
			removeRenderTarget(pRenderer, pTarget);
	}

	virtual Noesis::Texture* GetTexture() override
	{
		return pTexture;
	}

	::RenderTarget* pTarget = nullptr;
	::RenderTarget* pStencilTarget = nullptr;
	Noesis::Texture* pTexture = nullptr;
	bool mOwns = true;
};

Renderer* ForgeRenderTarget::pRenderer = nullptr;

// Note: This is a Noesis class to manage the renderer
class ForgeRenderDevice final : public Noesis::RenderDevice
{
public:

	static constexpr uint32_t VERTEX_SHADER_COUNT = 21;
	static constexpr uint32_t FRAME_COUNT = 3;
	static constexpr uint32_t SAMPLER_COUNT = 64;

	ForgeRenderDevice(Renderer* pRenderer, uint32_t maxDrawBatches, Queue* pGraphicsQueue) : 
		Noesis::RenderDevice(), pRenderer(pRenderer), mMaxBatches(maxDrawBatches), pGraphicsQueue(pGraphicsQueue)
	{
		mCaps.centerPixelOffset = 0.0f;
		mCaps.linearRendering = true;
		mCaps.subpixelRendering = false;

		for (uint32_t i = 0; i < FRAME_COUNT; ++i)
		{
			BufferLoadDesc vbDesc{};
			vbDesc.ppBuffer = &mVertexBuffers[i];
			vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbDesc.mDesc.mSize = DYNAMIC_VB_SIZE;
			vbDesc.mDesc.mStructStride = 0;
			vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			vbDesc.mDesc.pName = "ForgeVertexBuffer";
			addResource(&vbDesc, nullptr);

			BufferLoadDesc ibDesc{};
			ibDesc.ppBuffer = &mIndexBuffers[i];
			ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
			ibDesc.mDesc.mSize = DYNAMIC_IB_SIZE;
			ibDesc.mDesc.mStructStride = sizeof(uint16_t);
			ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			ibDesc.mDesc.pName = "ForgeIndexBuffer";
			addResource(&ibDesc, nullptr);
		}

		BufferLoadDesc ubDesc{};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.pName = "ForgeUniformBuffer";

		for (uint32_t i = 0; i < FRAME_COUNT; ++i)
		{
			ubDesc.ppBuffer = &mUniformBuffers[i];
			addResource(&ubDesc, nullptr);
		}

		for (uint32_t i = 0; i < Noesis::MinMagFilter::Count; ++i)
		{
			for (uint32_t j = 0; j < Noesis::MipFilter::Count; ++j)
			{
				for (uint32_t k = 0; k < Noesis::WrapMode::Count; ++k)
					InitSampler(i, j, k);
			}
		}

		waitForAllResourceLoads();
	}

	~ForgeRenderDevice()
	{
		for (uint32_t i = 0; i < SAMPLER_COUNT; ++i)
		{
			if (mSamplers[i])
				removeSampler(pRenderer, mSamplers[i]);
		}

		for (uint32_t i = 0; i < FRAME_COUNT; ++i)
		{
			removeResource(mUniformBuffers[i]);
			removeResource(mIndexBuffers[i]);
			removeResource(mVertexBuffers[i]);
		}
	}

	void Load(ReloadDesc const* pReloadDesc, RenderTarget* pRenderTarget)
	{
		pSwapchainTarget = pRenderTarget;
		mCaps.linearRendering = TinyImageFormat_IsSRGB(pRenderTarget->mFormat);

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			InitShader(Noesis::Shader::RGBA);
			InitShader(Noesis::Shader::Mask);
			InitShader(Noesis::Shader::Clear);
			InitShader(Noesis::Shader::Path_Solid);
			InitShader(Noesis::Shader::Path_Linear);
			InitShader(Noesis::Shader::Path_Radial);
			InitShader(Noesis::Shader::Path_Pattern);
			InitShader(Noesis::Shader::Path_AA_Solid);
			InitShader(Noesis::Shader::Path_AA_Linear);
			InitShader(Noesis::Shader::Path_AA_Radial);
			InitShader(Noesis::Shader::Path_AA_Pattern);
			InitShader(Noesis::Shader::SDF_Solid);
			InitShader(Noesis::Shader::SDF_Linear);
			InitShader(Noesis::Shader::SDF_Radial);
			InitShader(Noesis::Shader::SDF_Pattern);
			InitShader(Noesis::Shader::Opacity_Solid);
			InitShader(Noesis::Shader::Opacity_Linear);
			InitShader(Noesis::Shader::Opacity_Radial);
			InitShader(Noesis::Shader::Opacity_Pattern);

			Shader* shaders[Noesis::Shader::Count]{};
			uint32_t shaderCount = 0;

			for (uint32_t i = 0; i < Noesis::Shader::Count; ++i)
			{
				if (mShaders[i])
					shaders[shaderCount++] = mShaders[i];
			}

			RootSignatureDesc rootDesc{};
			rootDesc.mShaderCount = shaderCount;
			rootDesc.ppShaders = shaders;
			addRootSignature(pRenderer, &rootDesc, &pRootSignature);

			DescriptorSetDesc setDesc{};
			setDesc.pRootSignature = pRootSignature;
			setDesc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE;
			setDesc.mMaxSets = FRAME_COUNT;
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorUniform);

			setDesc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_PER_BATCH;
			setDesc.mMaxSets = FRAME_COUNT * mMaxBatches;
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorTexture);

			for (uint32_t i = 0; i < FRAME_COUNT; ++i)
			{
				DescriptorData params[1]{};
				params[0].pName = "uBlock";
				params[0].ppBuffers = &mUniformBuffers[i];
				updateDescriptorSet(pRenderer, i, pDescriptorUniform, 1, params);
			}
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_RESIZE))
		{
			RenderTargetDesc stencilDesc{};
			stencilDesc.mArraySize = 1;
			stencilDesc.mClearValue.depth = 0.0f;
			stencilDesc.mClearValue.stencil = 0;
			stencilDesc.mDepth = 1;
			stencilDesc.mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
			stencilDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
			stencilDesc.mHeight = pRenderTarget->mHeight;
			stencilDesc.mSampleCount = SAMPLE_COUNT_1;
			stencilDesc.mSampleQuality = 0;
			stencilDesc.mWidth = pRenderTarget->mWidth;
			addRenderTarget(pRenderer, &stencilDesc, &pStencilTarget);
		}
	}

	void Unload(ReloadDesc const* pReloadDesc)
	{
		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			for (uint32_t i = 0; i < Noesis::Shader::Count; ++i)
			{
				if (mPipelines[i])
				{
					uint32_t len = (uint32_t)hmlenu(mPipelines[i]);

					for (uint32_t j = 0; j < len; ++j)
					{
						if (mPipelines[i][j].value)
							removePipeline(pRenderer, mPipelines[i][j].value);
					}

					hmfree(mPipelines[i]);
					mPipelines[i] = nullptr;
				}
			}
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSet(pRenderer, pDescriptorTexture);
			pDescriptorTexture = nullptr;
			removeDescriptorSet(pRenderer, pDescriptorUniform);
			pDescriptorUniform = nullptr;
			removeRootSignature(pRenderer, pRootSignature);
			pRootSignature = nullptr;

			for (uint32_t i = 0; i < Noesis::Shader::Count; ++i)
			{
				if (mShaders[i])
				{
					removeShader(pRenderer, mShaders[i]);
					mShaders[i] = nullptr;
				}
			}
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_RESIZE))
			removeRenderTarget(pRenderer, pStencilTarget);
	}

	/// Retrieves device render capabilities
	virtual const Noesis::DeviceCaps& GetCaps() const override
	{
		return mCaps;
	}

	/// Creates render target surface with given dimensions, samples and optional stencil buffer
	virtual Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width, uint32_t height,
		uint32_t sampleCount, bool needsStencil) override
	{
		Noesis::Ptr<Noesis::RenderTarget> target = Noesis::MakePtr<ForgeRenderTarget>(label, width, height, sampleCount, needsStencil, pRenderer,
			pSwapchainTarget->mFormat);

		return target;
	}

	/// Creates render target sharing transient (stencil, colorAA) buffers with the given surface
	virtual Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget(const char* label, Noesis::RenderTarget* surface) override
	{
		ASSERT(0);
		return nullptr;
	}

	/// Creates texture with given dimensions and format. For immutable textures, the content of
	/// each mipmap is given in 'data'. The passed data is tightly packed (no extra pitch). When
	/// 'data' is null the texture is considered dynamic and will be updated using UpdateTexture()
	virtual Noesis::Ptr<Noesis::Texture> CreateTexture(const char* label, uint32_t width, uint32_t height,
		uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data) override
	{
		return Noesis::MakePtr<ForgeTexture>(label, width, height, numLevels, format, data);
	}

	/// Updates texture mipmap copying the given data to desired position. The passed data is
	/// tightly packed (no extra pitch) and is never greater than DYNAMIC_TEX_SIZE bytes.
	/// Origin is located at the left of the first scanline
	virtual void UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y,
		uint32_t width, uint32_t height, const void* data) override
	{
		ForgeTexture* pForgeTexture = (ForgeTexture*)texture;

		if (pCurrentUpdateTexture && (pCurrentUpdateTexture != pForgeTexture || mTextureUpdateDesc.mMipLevel != level))
		{
			waitQueueIdle(pGraphicsQueue);
			memcpy(mTextureUpdateDesc.pMappedData, pCurrentUpdateTexture->pData, pCurrentUpdateTexture->mSize);
			endUpdateResource(&mTextureUpdateDesc, nullptr);
			waitForAllResourceLoads();
			mTextureUpdateDesc = TextureUpdateDesc{};
			pCurrentUpdateTexture = nullptr;
		}

		if (!pCurrentUpdateTexture)
		{
			pCurrentUpdateTexture = pForgeTexture;
			mTextureUpdateDesc.pTexture = pCurrentUpdateTexture->pTexture;
			mTextureUpdateDesc.mMipLevel = level;
			beginUpdateResource(&mTextureUpdateDesc);
		}

		uint32_t pixelSize = mTextureUpdateDesc.mDstRowStride / pCurrentUpdateTexture->pTexture->mWidth; //-V::522
		uint32_t srcRowStride = pixelSize * width;
		uint32_t dstRowStride = mTextureUpdateDesc.mDstRowStride;

		if (!pCurrentUpdateTexture->pData)
		{
			pCurrentUpdateTexture->mSize = dstRowStride * pCurrentUpdateTexture->pTexture->mHeight;
			pCurrentUpdateTexture->pData = (uint8_t*)tf_calloc(1, pCurrentUpdateTexture->mSize);
		}

		uint8_t* pMappedData = pCurrentUpdateTexture->pData + y * dstRowStride + x * pixelSize;

		for (uint32_t i = 0; i < height; ++i)
			memcpy(pMappedData + i * dstRowStride, (uint8_t*)data + i * srcRowStride, srcRowStride);
	}

	/// Begins rendering offscreen commands
	virtual void BeginOffscreenRender() override
	{
		if (mTextureUpdateDesc.pTexture)
		{
			waitQueueIdle(pGraphicsQueue);
			memcpy(mTextureUpdateDesc.pMappedData, pCurrentUpdateTexture->pData, pCurrentUpdateTexture->mSize);
			endUpdateResource(&mTextureUpdateDesc, nullptr);
			waitForAllResourceLoads();
			mTextureUpdateDesc = TextureUpdateDesc{};
		}

		cmdBindIndexBuffer(pCmd, mIndexBuffers[mFrameIndex], INDEX_TYPE_UINT16, 0);

		mOffscreen = true;
	}

	/// Ends rendering offscreen commands
	virtual void EndOffscreenRender() override
	{
		mOffscreen = false;
	}

	/// Begins rendering onscreen commands
	virtual void BeginOnscreenRender() override
	{
		if (mTextureUpdateDesc.pTexture)
		{
			waitQueueIdle(pGraphicsQueue);
			memcpy(mTextureUpdateDesc.pMappedData, pCurrentUpdateTexture->pData, pCurrentUpdateTexture->mSize);
			endUpdateResource(&mTextureUpdateDesc, nullptr);
			waitForAllResourceLoads();
			mTextureUpdateDesc = TextureUpdateDesc{};
		}

		LoadActionsDesc loadActions{};
		loadActions.mClearDepth.depth = 0.0f;
		loadActions.mClearDepth.stencil = 0;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;

		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pSwapchainTarget->mWidth, (float)pSwapchainTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pSwapchainTarget->mWidth, pSwapchainTarget->mHeight);
		cmdBindRenderTargets(pCmd, 1, &pSwapchainTarget, pStencilTarget, &loadActions, nullptr, nullptr, -1, -1);
		cmdBindIndexBuffer(pCmd, mIndexBuffers[mFrameIndex], INDEX_TYPE_UINT16, 0);
	}

	/// Ends rendering onscreen commands
	virtual void EndOnscreenRender() override
	{
		
	}

	/// Binds render target and sets viewport to cover the entire surface. The existing contents of
	/// the surface are discarded and replaced with arbitrary data. Surface is not cleared
	virtual void SetRenderTarget(Noesis::RenderTarget* surface) override
	{
		ForgeRenderTarget* pTarget = (ForgeRenderTarget*)surface;

		cmdBindRenderTargets(pCmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

		RenderTargetBarrier barriers[]
		{
			{ pTarget->pTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
		};

		cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);

		LoadActionsDesc loadAction{};
		loadAction.mClearDepth.depth = 0.0f;
		loadAction.mClearDepth.stencil = 0;
		loadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pTarget->pTarget->mWidth, (float)pTarget->pTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pTarget->pTarget->mWidth, pTarget->pTarget->mHeight);
		cmdBindRenderTargets(pCmd, 1, &pTarget->pTarget, pTarget->pStencilTarget, &loadAction, nullptr, nullptr, -1, -1);
	}

	/// Note: This function does not actually resolve anything. It is just meant to transition the render target.
	virtual void ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t numTiles) override
	{
		ForgeRenderTarget* pTarget = (ForgeRenderTarget*)surface;
		ForgeTexture* pTexture = (ForgeTexture*)pTarget->pTexture;

		if (pTarget->pTarget->pTexture == pTexture->pTexture)
		{
			cmdBindRenderTargets(pCmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

			RenderTargetBarrier barriers[]
			{
				{ pTarget->pTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE }
			};

			cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);
		}
		else
		{
			ASSERT(0);
		}
	}

	/// Gets a pointer to stream vertices (bytes <= DYNAMIC_VB_SIZE)
	virtual void* MapVertices(uint32_t bytes) override
	{
		mVertexUpdateDesc.pBuffer = mVertexBuffers[mFrameIndex];
		mVertexUpdateDesc.mSize = bytes;
		beginUpdateResource(&mVertexUpdateDesc);

		return mVertexUpdateDesc.pMappedData;
	}

	/// Invalidates the pointer previously mapped
	virtual void UnmapVertices() override
	{
		endUpdateResource(&mVertexUpdateDesc, nullptr);
		waitForAllResourceLoads();
		mVertexUpdateDesc = BufferUpdateDesc{};
	}

	/// Gets a pointer to stream 16-bit indices (bytes <= DYNAMIC_IB_SIZE)
	virtual void* MapIndices(uint32_t bytes) override
	{
		mIndexUpdateDesc.pBuffer = mIndexBuffers[mFrameIndex];
		mIndexUpdateDesc.mSize = bytes;
		beginUpdateResource(&mIndexUpdateDesc);

		return mIndexUpdateDesc.pMappedData;
	}

	/// Invalidates the pointer previously mapped
	virtual void UnmapIndices() override
	{
		endUpdateResource(&mIndexUpdateDesc, nullptr);
		waitForAllResourceLoads();
		mIndexUpdateDesc = BufferUpdateDesc{};
	}
   
#ifdef NOESIS_ENABLE_WARNING_DISABLE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

	/// Draws primitives for the given batch
	virtual void DrawBatch(const Noesis::Batch& batch) override
	{
		ASSERT(mBatchIndex < mMaxBatches);

		InitShader(batch.shader.v);
		InitPipeline(batch.shader.v, batch.renderState);

		uint8_t vertexId = Noesis::VertexForShader[batch.shader.v];
		uint8_t layoutId = Noesis::FormatForVertex[vertexId];
		VertexLayout* pLayout = &mVertexLayouts[layoutId];

		BufferUpdateDesc updateDesc{};
		updateDesc.pBuffer = mUniformBuffers[mFrameIndex];
		beginUpdateResource(&updateDesc);

		UniformBlock* pData = (UniformBlock*)updateDesc.pMappedData;
		const Noesis::UniformData* pNData = &batch.vertexUniforms[0];

		if (pNData->numDwords)
		{
			ASSERT(pNData->numDwords * sizeof(float) == sizeof(pData->cbuf0_vs));
			memcpy(&pData->cbuf0_vs, pNData->values, pNData->numDwords * sizeof(float));
			pData->cbuf0_vs = transpose(pData->cbuf0_vs);
		}

		pNData = &batch.vertexUniforms[1];
		if (pNData->numDwords)
		{
			ASSERT((pNData->numDwords * sizeof(float)) <= sizeof(pData->cbuf1_vs));
			memcpy(&pData->cbuf1_vs, pNData->values, pNData->numDwords * sizeof(float));
		}

		pNData = &batch.pixelUniforms[0];
		if (pNData->numDwords)
		{
			ASSERT((pNData->numDwords * sizeof(float)) <= sizeof(pData->cbuf0_ps));
			memcpy(&pData->cbuf0_ps, pNData->values, pNData->numDwords * sizeof(float));
		}

		pNData = &batch.pixelUniforms[1];
		if (pNData->numDwords)
		{
			ASSERT((pNData->numDwords * sizeof(float)) <= sizeof(pData->cbuf1_ps));
			memcpy(&pData->cbuf1_ps, pNData->values, pNData->numDwords * sizeof(float));
		}

		endUpdateResource(&updateDesc, nullptr);
		waitForAllResourceLoads();

		DescriptorData params[10]{};
		uint32_t paramCount = 0;

		if (batch.ramps)
		{
			params[paramCount].pName = "uRampsTex";
			params[paramCount++].ppTextures = &((ForgeTexture*)batch.ramps)->pTexture;

			InitSampler(batch.rampsSampler.f.minmagFilter, batch.rampsSampler.f.mipFilter, batch.rampsSampler.f.wrapMode);
			params[paramCount].pName = "uRampsSampler";
			params[paramCount++].ppSamplers = &mSamplers[batch.rampsSampler.v];
		}

		if (batch.pattern)
		{
			params[paramCount].pName = "uPatternTex";
			params[paramCount++].ppTextures = &((ForgeTexture*)batch.pattern)->pTexture;

			InitSampler(batch.patternSampler.f.minmagFilter, batch.patternSampler.f.mipFilter, batch.patternSampler.f.wrapMode);
			params[paramCount].pName = "uPatternSampler";
			params[paramCount++].ppSamplers = &mSamplers[batch.patternSampler.v];
		}

		if (batch.image)
		{
			params[paramCount].pName = "uImageTex";
			params[paramCount++].ppTextures = &((ForgeTexture*)batch.image)->pTexture;

			InitSampler(batch.imageSampler.f.minmagFilter, batch.imageSampler.f.mipFilter, batch.imageSampler.f.wrapMode);
			params[paramCount].pName = "uImageSampler";
			params[paramCount++].ppSamplers = &mSamplers[batch.imageSampler.v];
		}

		if (batch.glyphs)
		{
			params[paramCount].pName = "uGlyphsTex";
			params[paramCount++].ppTextures = &((ForgeTexture*)batch.glyphs)->pTexture;

			InitSampler(batch.glyphsSampler.f.minmagFilter, batch.glyphsSampler.f.mipFilter, batch.glyphsSampler.f.wrapMode);
			params[paramCount].pName = "uGlyphsSampler";
			params[paramCount++].ppSamplers = &mSamplers[batch.glyphsSampler.v];
		}

		if (batch.shadow)
		{
			params[paramCount].pName = "uShadowTex";
			params[paramCount++].ppTextures = &((ForgeTexture*)batch.shadow)->pTexture;

			InitSampler(batch.shadowSampler.f.minmagFilter, batch.shadowSampler.f.mipFilter, batch.shadowSampler.f.wrapMode);
			params[paramCount].pName = "uShadowSampler";
			params[paramCount++].ppSamplers = &mSamplers[batch.shadowSampler.v];
		}

		uint64_t vertexOffset = (uint64_t)batch.vertexOffset;

		uint32_t setIndex = mFrameIndex * mMaxBatches + mBatchIndex++;
		updateDescriptorSet(pRenderer, setIndex, pDescriptorTexture, paramCount, params);
		cmdBindDescriptorSet(pCmd, mFrameIndex, pDescriptorUniform);
		cmdBindDescriptorSet(pCmd, setIndex, pDescriptorTexture);
		cmdBindPipeline(pCmd, hmget(mPipelines[batch.shader.v], batch.renderState.v));
		cmdBindVertexBuffer(pCmd, 1, &mVertexBuffers[mFrameIndex], &pLayout->mStrides[0], &vertexOffset);
		cmdSetStencilReferenceValue(pCmd, batch.stencilRef);
		cmdDrawIndexed(pCmd, batch.numIndices, batch.startIndex, 0);
	}
   
#ifdef NOESIS_ENABLE_WARNING_DISABLE
#pragma GCC diagnostic pop
#endif

	Cmd* pCmd = nullptr;
	uint32_t mFrameIndex = 0;
	RenderTarget* pSwapchainTarget = nullptr;
	uint32_t mBatchIndex = 0;

private:

	struct UniformBlock
	{
		mat4 cbuf0_vs;
		float4 cbuf1_vs;
		float cbuf0_ps[8];
		float cbuf1_ps[8];
	};

	struct PipelineMap
	{
		uint8_t key;
		Pipeline* value;
	};

	void InitVertexLayout(uint32_t layoutId)
	{
		VertexLayout* pLayout = &mVertexLayouts[layoutId];

		if (pLayout->mAttribCount != 0)
			return;

		uint8_t layoutSize = Noesis::SizeForFormat[layoutId];
		pLayout->mStrides[0] = layoutSize;

		uint8_t attribsId = Noesis::AttributesForFormat[layoutId];

		ShaderSemantic semantics[Noesis::Shader::Vertex::Format::Attr::Count]
		{
			SEMANTIC_POSITION,   // Pos
			SEMANTIC_COLOR,	     // Color
			SEMANTIC_TEXCOORD0,  // UV0
			SEMANTIC_TEXCOORD1,  // UV1
			SEMANTIC_TEXCOORD2,  // Coverage
			SEMANTIC_TEXCOORD3,  // Rect
			SEMANTIC_TEXCOORD4,  // Tile
			SEMANTIC_TEXCOORD5   // ImagePos
		};

		TinyImageFormat formats[Noesis::Shader::Vertex::Format::Attr::Type::Count]
		{
			TinyImageFormat_R32_SFLOAT,          // One 32-bit floating-point component
			TinyImageFormat_R32G32_SFLOAT,       // Two 32-bit floating-point components
			TinyImageFormat_R32G32B32A32_SFLOAT, // Four 32-bit floating-point components
			TinyImageFormat_R8G8B8A8_UNORM,      // Four 8-bit normalized unsigned integer components
			TinyImageFormat_R16G16B16A16_UNORM   // Four 16-bit normalized unsigned integer components
		};

		uint32_t locationCounter = 0;
		uint32_t offsetCounter = 0;
		
		for (uint32_t i = 0; i < Noesis::Shader::Vertex::Format::Attr::Count; ++i)
		{
			if (attribsId & (1 << i))
			{
				VertexAttrib* pAttrib = &pLayout->mAttribs[pLayout->mAttribCount++];
				uint8_t typeId = Noesis::TypeForAttr[i];
				
				pAttrib->mSemantic = semantics[i];
				pAttrib->mFormat = formats[typeId];
				pAttrib->mBinding = 0;
				pAttrib->mLocation = locationCounter++;
				pAttrib->mOffset = offsetCounter;

				offsetCounter += Noesis::SizeForType[typeId];
			}
		}
	}

	void InitShader(uint32_t shaderId)
	{
		if (mShaders[shaderId])
			return;

		uint8_t vertexId = Noesis::VertexForShader[shaderId];
		uint8_t layoutId = Noesis::FormatForVertex[vertexId];

		InitVertexLayout(layoutId);

		const char* vertexShaderPaths[VERTEX_SHADER_COUNT]
		{
			"Noesis.vert",
			"Noesis_HAS_COLOR.vert",
			"Noesis_HAS_UV0.vert",
			"Noesis_HAS_UV0_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_COVERAGE.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_UV1_SDF.vert",
			"Noesis_HAS_UV0_HAS_UV1_SDF.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_SDF.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_HAS_TILE_SDF.vert",
			"Noesis_HAS_COLOR_HAS_UV1.vert",
			"Noesis_HAS_UV0_HAS_UV1.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_UV0_HAS_UV1.vert",
			"Noesis_HAS_UV0_HAS_UV1_DOWNSAMPLE.vert",
			"Noesis_HAS_COLOR_HAS_UV1_HAS_RECT.vert",
			"Noesis_HAS_COLOR_HAS_UV0_HAS_RECT_HAS_IMAGE_POSITION.vert"
		};

		const char* vertexShaderPathsSRGB[VERTEX_SHADER_COUNT]
		{
			"Noesis.vert",
			"Noesis_HAS_COLOR_SRGB.vert",
			"Noesis_HAS_UV0.vert",
			"Noesis_HAS_UV0_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_COVERAGE_SRGB.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_COVERAGE_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_UV1_SDF_SRGB.vert",
			"Noesis_HAS_UV0_HAS_UV1_SDF.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_SDF.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_HAS_TILE_SDF.vert",
			"Noesis_HAS_COLOR_HAS_UV1_SRGB.vert",
			"Noesis_HAS_UV0_HAS_UV1.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT.vert",
			"Noesis_HAS_UV0_HAS_UV1_HAS_RECT_HAS_TILE.vert",
			"Noesis_HAS_COLOR_HAS_UV0_HAS_UV1_SRGB.vert",
			"Noesis_HAS_UV0_HAS_UV1_DOWNSAMPLE.vert",
			"Noesis_HAS_COLOR_HAS_UV1_HAS_RECT_SRGB.vert",
			"Noesis_HAS_COLOR_HAS_UV0_HAS_RECT_HAS_IMAGE_POSITION_SRGB.vert"
		};

		const char* fragmentShaderPaths[Noesis::Shader::Count]
		{
			"Noesis_RGBA.frag",
			"Noesis_Mask.frag",
			"Noesis_Clear.frag",
			"Noesis_Path_Solid.frag",
			"Noesis_Path_Linear.frag",
			"Noesis_Path_Radial.frag",
			"Noesis_Path_Pattern.frag",
			"Noesis_Path_Pattern_Clamp.frag",
			"Noesis_Path_Pattern_Repeat.frag",
			"Noesis_Path_Pattern_MirrorU.frag",
			"Noesis_Path_Pattern_MirrorV.frag",
			"Noesis_Path_Pattern_Mirror.frag",
			"Noesis_Path_AA_Solid.frag",
			"Noesis_Path_AA_Linear.frag",
			"Noesis_Path_AA_Radial.frag",
			"Noesis_Path_AA_Pattern.frag",
			"Noesis_Path_AA_Pattern_Clamp.frag",
			"Noesis_Path_AA_Pattern_Repeat.frag",
			"Noesis_Path_AA_Pattern_MirrorU.frag",
			"Noesis_Path_AA_Pattern_MirrorV.frag",
			"Noesis_Path_AA_Pattern_Mirror.frag",
			"Noesis_SDF_Solid.frag",
			"Noesis_SDF_Linear.frag",
			"Noesis_SDF_Radial.frag",
			"Noesis_SDF_Pattern.frag",
			"Noesis_SDF_Pattern_Clamp.frag",
			"Noesis_SDF_Pattern_Repeat.frag",
			"Noesis_SDF_Pattern_MirrorU.frag",
			"Noesis_SDF_Pattern_MirrorV.frag",
			"Noesis_SDF_Pattern_Mirror.frag",
			"Noesis_SDF_LCD_Solid.frag",
			"Noesis_SDF_LCD_Linear.frag",
			"Noesis_SDF_LCD_Radial.frag",
			"Noesis_SDF_LCD_Pattern.frag",
			"Noesis_SDF_LCD_Pattern_Clamp.frag",
			"Noesis_SDF_LCD_Pattern_Repeat.frag",
			"Noesis_SDF_LCD_Pattern_MirrorU.frag",
			"Noesis_SDF_LCD_Pattern_MirrorV.frag",
			"Noesis_SDF_LCD_Pattern_Mirror.frag",
			"Noesis_Opacity_Solid.frag",
			"Noesis_Opacity_Linear.frag",
			"Noesis_Opacity_Radial.frag",
			"Noesis_Opacity_Pattern.frag",
			"Noesis_Opacity_Pattern_Clamp.frag",
			"Noesis_Opacity_Pattern_Repeat.frag",
			"Noesis_Opacity_Pattern_MirrorU.frag",
			"Noesis_Opacity_Pattern_MirrorV.frag",
			"Noesis_Opacity_Pattern_Mirror.frag",
			"Noesis_Upsample.frag",
			"Noesis_Downsample.frag",
			"Noesis_Shadow.frag",
			"Noesis_Blur.frag",
			"Noesis_Custom_Effect.frag"
		};

		ShaderLoadDesc shaderDesc{};
		shaderDesc.mStages[0].pFileName = mCaps.linearRendering ? vertexShaderPathsSRGB[vertexId] : vertexShaderPaths[vertexId];
		shaderDesc.mStages[1].pFileName = fragmentShaderPaths[shaderId];
		addShader(pRenderer, &shaderDesc, &mShaders[shaderId]);
	}

	void InitPipeline(uint32_t shaderId, Noesis::RenderState state)
	{
		if (!mShaders[shaderId])
			return;

		if (mPipelines[shaderId])
		{
			int32_t pipelineIndex = (int32_t)hmgeti(mPipelines[shaderId], state.v);

			if (pipelineIndex != -1)
				return;
		}

		uint8_t vertexId = Noesis::VertexForShader[shaderId];
		uint32_t layoutId = Noesis::FormatForVertex[vertexId];

		BlendStateDesc blendDesc{};
		blendDesc.mIndependentBlend = false;
		blendDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;

		if (state.f.colorEnable)
		{
			blendDesc.mMasks[0] = ALL;

			switch (state.f.blendMode)
			{
			case Noesis::BlendMode::Src:
				blendDesc.mSrcFactors[0] = BC_SRC_ALPHA;
				blendDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				blendDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
				blendDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				break;

			case Noesis::BlendMode::SrcOver:
				blendDesc.mSrcFactors[0] = BC_ONE;
				blendDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				blendDesc.mSrcAlphaFactors[0] = BC_ONE;
				blendDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				break;

			case Noesis::BlendMode::SrcOver_Multiply:
				blendDesc.mSrcFactors[0] = BC_DST_COLOR;
				blendDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				blendDesc.mSrcAlphaFactors[0] = BC_ONE;
				blendDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				break;

			case Noesis::BlendMode::SrcOver_Screen:
				blendDesc.mSrcFactors[0] = BC_ONE;
				blendDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_COLOR;
				blendDesc.mSrcAlphaFactors[0] = BC_ONE;
				blendDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				break;

			case Noesis::BlendMode::SrcOver_Additive:
				blendDesc.mSrcFactors[0] = BC_ONE;
				blendDesc.mDstFactors[0] = BC_ONE;
				blendDesc.mSrcAlphaFactors[0] = BC_ONE;
				blendDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
				break;

			default:
				ASSERT(0);
				break;
			}
		}
		else
		{
			blendDesc.mMasks[0] = 0;
		}

		DepthStateDesc depthDesc{};
		depthDesc.mDepthTest = false;
		depthDesc.mDepthWrite = false;
		depthDesc.mStencilReadMask = 0xFF;
		depthDesc.mStencilWriteMask = 0xFF;
		depthDesc.mStencilFrontFail = STENCIL_OP_KEEP;
		depthDesc.mStencilBackFail = STENCIL_OP_KEEP;

		switch (state.f.stencilMode)
		{
		case Noesis::StencilMode::Disabled:
			break;

		case Noesis::StencilMode::Equal_Keep:
			depthDesc.mStencilTest = true;
			depthDesc.mStencilFrontFunc = CMP_EQUAL;
			depthDesc.mStencilFrontPass = STENCIL_OP_KEEP;
			depthDesc.mStencilBackFunc = CMP_EQUAL;
			depthDesc.mStencilBackPass = STENCIL_OP_KEEP;
			break;

		case Noesis::StencilMode::Equal_Incr:
			depthDesc.mStencilTest = true;
			depthDesc.mStencilFrontFunc = CMP_EQUAL;
			depthDesc.mStencilFrontPass = STENCIL_OP_INCR;
			depthDesc.mStencilBackFunc = CMP_EQUAL;
			depthDesc.mStencilBackPass = STENCIL_OP_INCR;
			break;

		case Noesis::StencilMode::Equal_Decr:
			depthDesc.mStencilTest = true;
			depthDesc.mStencilFrontFunc = CMP_EQUAL;
			depthDesc.mStencilFrontPass = STENCIL_OP_DECR;
			depthDesc.mStencilBackFunc = CMP_EQUAL;
			depthDesc.mStencilBackPass = STENCIL_OP_DECR;
			break;

		case Noesis::StencilMode::Clear:
			depthDesc.mStencilTest = true;
			depthDesc.mStencilFrontFunc = CMP_ALWAYS;
			depthDesc.mStencilFrontPass = STENCIL_OP_SET_ZERO;
			depthDesc.mStencilBackFunc = CMP_ALWAYS;
			depthDesc.mStencilBackPass = STENCIL_OP_SET_ZERO;
			break;

		default:
			ASSERT(0);
			break;
		}

		if (depthDesc.mStencilTest)
			ASSERT(pStencilTarget);

		RasterizerStateDesc rasterizerDesc{};
		rasterizerDesc.mCullMode = CULL_MODE_NONE;
		rasterizerDesc.mScissor = true;
		rasterizerDesc.mFrontFace = FRONT_FACE_CW;

		if (state.f.wireframe)
			rasterizerDesc.mFillMode = FILL_MODE_WIREFRAME;
		else
			rasterizerDesc.mFillMode = FILL_MODE_SOLID;

		Pipeline* pPipeline = nullptr;

		PipelineDesc pipelineDesc{};
		pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc* pGfxPipelineDesc = &pipelineDesc.mGraphicsDesc;
		pGfxPipelineDesc->mDepthStencilFormat = pStencilTarget ? pStencilTarget->mFormat : TinyImageFormat_UNDEFINED;
		pGfxPipelineDesc->mRenderTargetCount = 1;
		pGfxPipelineDesc->mSampleCount = pSwapchainTarget->mSampleCount;
		pGfxPipelineDesc->pBlendState = &blendDesc; //-V::506
		pGfxPipelineDesc->mSampleQuality = pSwapchainTarget->mSampleQuality;
		pGfxPipelineDesc->pColorFormats = &pSwapchainTarget->mFormat;
		pGfxPipelineDesc->pDepthState = &depthDesc; //-V::506
		pGfxPipelineDesc->pRasterizerState = &rasterizerDesc; //-V::506
		pGfxPipelineDesc->pRootSignature = pRootSignature;
		pGfxPipelineDesc->pVertexLayout = &mVertexLayouts[layoutId];
		pGfxPipelineDesc->mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pGfxPipelineDesc->pShaderProgram = mShaders[shaderId];
		addPipeline(pRenderer, &pipelineDesc, &pPipeline);

		hmput(mPipelines[shaderId], state.v, pPipeline);
	}

	void InitSampler(uint32_t minMagFilter, uint32_t mipFilter, uint32_t wrapMode)
	{
		Noesis::SamplerState state{ {(uint8_t)wrapMode, (uint8_t)minMagFilter, (uint8_t)mipFilter} };

		if (mSamplers[state.v])
			return;

		SamplerDesc samplerDesc{};

		switch (minMagFilter)
		{
		case Noesis::MinMagFilter::Nearest:
			samplerDesc.mMinFilter = FILTER_NEAREST;
			samplerDesc.mMagFilter = FILTER_NEAREST;
			break;

		case Noesis::MinMagFilter::Linear:
			samplerDesc.mMinFilter = FILTER_LINEAR;
			samplerDesc.mMagFilter = FILTER_LINEAR;
			break;

		default:
			ASSERT(0);
			break;
		}

		switch (mipFilter)
		{
		case Noesis::MipFilter::Disabled:
		case Noesis::MipFilter::Nearest:
			samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
			break;

		case Noesis::MipFilter::Linear:
			samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
			break;

		default:
			ASSERT(0);
			break;
		}

		switch (wrapMode)
		{
		case Noesis::WrapMode::ClampToEdge:
			samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
			break;

		case Noesis::WrapMode::ClampToZero: // By default our border color is already all 0
			samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
			break;

		case Noesis::WrapMode::Repeat:
			samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
			samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
			samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
			break;

		case Noesis::WrapMode::MirrorU:
			samplerDesc.mAddressU = ADDRESS_MODE_MIRROR;
			samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
			samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
			break;

		case Noesis::WrapMode::MirrorV:
			samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
			samplerDesc.mAddressV = ADDRESS_MODE_MIRROR;
			samplerDesc.mAddressW = ADDRESS_MODE_REPEAT; //-V::778
			break;

		case Noesis::WrapMode::Mirror:
			samplerDesc.mAddressU = ADDRESS_MODE_MIRROR;
			samplerDesc.mAddressV = ADDRESS_MODE_MIRROR;
			samplerDesc.mAddressW = ADDRESS_MODE_MIRROR;
			break;

		default:
			ASSERT(0);
			break;
		}
		
		addSampler(pRenderer, &samplerDesc, &mSamplers[state.v]);
	}

	Noesis::DeviceCaps mCaps{};
	Renderer* pRenderer = nullptr;
	Buffer* mVertexBuffers[FRAME_COUNT]{};
	Buffer* mIndexBuffers[FRAME_COUNT]{};
	BufferUpdateDesc mVertexUpdateDesc{};
	BufferUpdateDesc mIndexUpdateDesc{};
	Shader* mShaders[Noesis::Shader::Count]{};
	VertexLayout mVertexLayouts[Noesis::Shader::Vertex::Format::Count]{};
	DescriptorSet* pDescriptorUniform = nullptr;
	Buffer* mUniformBuffers[FRAME_COUNT]{};
	RootSignature* pRootSignature = nullptr;
	PipelineMap* mPipelines[Noesis::Shader::Count]{};
	DescriptorSet* pDescriptorTexture = nullptr;
	Sampler* mSamplers[SAMPLER_COUNT]{};
	TextureUpdateDesc mTextureUpdateDesc{};
	ForgeTexture* pCurrentUpdateTexture = nullptr;
	RenderTarget* pStencilTarget = nullptr;
	bool mOffscreen = false;
	uint32_t mMaxBatches = 0;
	Queue* pGraphicsQueue = nullptr;
};

struct NoesisXAMLMap
{
	const char* key;
	Noesis::FrameworkElement* value;
};

static Noesis::Ptr<Noesis::RenderDevice> renderer = nullptr;
static float2 oldPosition = float2(0.0f, 0.0f);
static float elapsedTime = 0.0f;
static IApp::Settings const* pSettings = nullptr;
static bool loaded = false;
static bool unloaded = false;

static void logHandler(const char* file, uint32_t line, uint32_t level, const char* channel, const char* message)
{
	LogLevel levels[5] = { LogLevel::eRAW, LogLevel::eDEBUG, LogLevel::eINFO, LogLevel::eWARNING, LogLevel::eERROR };
	LOGF(levels[level], message);
}

static void errorHandler(const char* file, uint32_t line, const char* message, bool fatal)
{
	LOGF(LogLevel::eERROR, "Noesis Error: %s, %u, %s", file, line, message);
}

static bool assertHandler(const char* file, uint32_t line, const char* expr)
{
	LOGF(LogLevel::eERROR, "Noesis Assert: %s, %u, %s", file, line, expr);
	return true;
}

static void* allocFunc(void* user, Noesis::SizeT size)
{
	return tf_malloc(size);
}

static void deallocFunc(void* user, void* ptr)
{
	tf_free(ptr);
}

static void* reallocFunc(void* user, void* ptr, Noesis::SizeT size)
{
	return tf_realloc(ptr, size);
}

static Noesis::SizeT allocSizeFunc(void* user, void* ptr)
{
	return 0;
}

void initNoesisUI(IApp::Settings const* pSettings, Renderer* pRenderer, uint32_t maxDrawBatches, Queue* pGraphicsQueue)
{
	ASSERT(pSettings && pRenderer && maxDrawBatches && pGraphicsQueue);

	Noesis::SetLogHandler(logHandler);
	Noesis::SetErrorHandler(errorHandler);
	Noesis::SetAssertHandler(assertHandler);

	Noesis::MemoryCallbacks mem{};
	mem.alloc = allocFunc;
	mem.dealloc = deallocFunc;
	mem.realloc = reallocFunc;
	mem.allocSize = allocSizeFunc;
	Noesis::SetMemoryCallbacks(mem);

	if (!::pSettings)
	{
		Noesis::GUI::DisableInspector();

		Noesis::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
		Noesis::Init();

		Noesis::Ptr<ForgeXamlProvider> xamlProvider = Noesis::MakePtr<ForgeXamlProvider>();
		Noesis::GUI::SetXamlProvider(xamlProvider);

		Noesis::Ptr<ForgeFontProvider> fontProvider = Noesis::MakePtr<ForgeFontProvider>();
		Noesis::GUI::SetFontProvider(fontProvider);

		Noesis::Ptr<ForgeTextureProvider> textureProvider = Noesis::MakePtr<ForgeTextureProvider>();
		Noesis::GUI::SetTextureProvider(textureProvider);

		::pSettings = pSettings;
	}

	renderer = Noesis::MakePtr<ForgeRenderDevice>(pRenderer, maxDrawBatches, pGraphicsQueue);
}

void exitNoesisUI()
{
	renderer.Reset();

	if (pSettings->mQuit)
	{
		Noesis::Shutdown();
		pSettings = nullptr;
	}

	elapsedTime = 0.0f;
	oldPosition = float2(0.0f, 0.0f);
	loaded = false;
	unloaded = false;
}

void setNoesisUIResources(char const* appResourcePath, uint32_t fallbackFontCount, char const** fallbackFontPaths)
{
	if (fallbackFontCount && fallbackFontPaths)
		Noesis::GUI::SetFontFallbacks(fallbackFontPaths, fallbackFontCount);

	Noesis::GUI::SetFontDefaultProperties(15.0f, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

	if (appResourcePath)
		Noesis::GUI::LoadApplicationResources(appResourcePath);
}

void loadNoesisUI(NoesisView* view, ReloadDesc const* pReloadDesc, RenderTarget* pRenderTarget)
{
	ASSERT(view && pReloadDesc && pRenderTarget);

	if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
	{
		Noesis::IView* noesisView = (Noesis::IView*)view;
		noesisView->SetSize(pSettings->mWidth, pSettings->mHeight);
	}

	if (!loaded)
	{
		((ForgeRenderDevice*)renderer.GetPtr())->Load(pReloadDesc, pRenderTarget);
		loaded = true;
		unloaded = false;
	}
}

void unloadNoesisUI(NoesisView* view, ReloadDesc const* pReloadDesc)
{
	ASSERT(view && pReloadDesc);

	if (!unloaded)
	{
		((ForgeRenderDevice*)renderer.GetPtr())->Unload(pReloadDesc);
		unloaded = true;
		loaded = false;
	}
}

void updateNoesisUI(NoesisView* view, float deltaTime)
{
	ASSERT(view);

	elapsedTime += deltaTime;

	Noesis::IView* noesisView = (Noesis::IView*)view;
	noesisView->Update((double)elapsedTime);

	ForgeRenderDevice* pForgeRenderer = (ForgeRenderDevice*)renderer.GetPtr();
	pForgeRenderer->mBatchIndex = 0;
}

void drawNoesisUI(NoesisView* view, Cmd* pCmd, uint32_t frameIndex, RenderTarget* pRenderTarget)
{
	ASSERT(view && pCmd && pRenderTarget);

	ForgeRenderDevice* pForgeRenderer = (ForgeRenderDevice*)renderer.GetPtr();
	pForgeRenderer->pCmd = pCmd;
	pForgeRenderer->mFrameIndex = frameIndex;
	pForgeRenderer->pSwapchainTarget = pRenderTarget;

	Noesis::IView* noesisView = (Noesis::IView*)view;
	Noesis::IRenderer* renderer = noesisView->GetRenderer();
	renderer->UpdateRenderTree();
	renderer->RenderOffscreen();
	renderer->Render();
}

void inputNoesisUI(NoesisView* view, InputActionContext const* ctx)
{
	ASSERT(view && ctx);

	bool mouseMove = false;
	bool upScroll = ctx->mActionId == UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP;
	bool downScroll = ctx->mActionId == UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN;
	bool mouseScroll = upScroll || downScroll;

	if (ctx->pPosition)
		mouseMove = oldPosition.x != ctx->pPosition->x || oldPosition.y != ctx->pPosition->y;

	Noesis::IView* noesisView = (Noesis::IView*)view;

	if (ctx->mDeviceType == INPUT_DEVICE_MOUSE && ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
	{
		Noesis::MouseButton button = Noesis::MouseButton::MouseButton_Count;
		if (!mouseScroll)
			button = mouseButtonToNoesis(ctx->mActionId);

		if (button != Noesis::MouseButton::MouseButton_Count)
		{
			if (ctx->pPosition)
			{
				if (ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_STARTED)
					noesisView->MouseButtonDown((int32_t)ctx->pPosition->x, (int32_t)ctx->pPosition->y, button);
				if (!ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_CANCELED)
					noesisView->MouseButtonUp((int32_t)ctx->pPosition->x, (int32_t)ctx->pPosition->y, button);
			}
		}

		if (mouseScroll && ctx->pPosition)
		{
			int32_t scrollValue = 60;
			noesisView->MouseWheel((int32_t)ctx->pPosition->x, (int32_t)ctx->pPosition->y, upScroll ? scrollValue : -scrollValue);
		}
	}

	if (mouseMove && ctx->pPosition)
		noesisView->MouseMove((int32_t)ctx->pPosition->x, (int32_t)ctx->pPosition->y);

	if (ctx->mDeviceType == INPUT_DEVICE_GAMEPAD)
	{
		Noesis::Key key = keyToNoesis(ctx->mActionId, true);
		if (key != Noesis::Key::Key_Count)
		{
			if (ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_STARTED)
			{
				noesisView->KeyDown(key);
			}

			if (!ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_CANCELED)
				noesisView->KeyUp(key);
		}
	}

	if (ctx->mDeviceType == INPUT_DEVICE_KEYBOARD)
	{
		Noesis::Key key = keyToNoesis(ctx->mActionId, false);
		if (key != Noesis::Key::Key_Count)
		{
			if (ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_STARTED)
				noesisView->KeyDown(key);
			if (!ctx->mBool && ctx->mPhase == INPUT_ACTION_PHASE_CANCELED)
				noesisView->KeyUp(key);
		}
	}

	if (ctx->pPosition)
		oldPosition = *ctx->pPosition;
}

NoesisXaml* addNoesisUIXaml(const char* xamlName)
{
	ASSERT(xamlName);

	Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(xamlName);
	return (NoesisXaml*)xaml.GiveOwnership();
}

void removeNoesisUIXaml(NoesisXaml* xaml)
{
	ASSERT(xaml);

	Noesis::FrameworkElement* noesisXaml = (Noesis::FrameworkElement*)xaml;
	noesisXaml->Release();
}

NoesisView* addNoesisUIView(NoesisXaml* xaml)
{
	ASSERT(xaml);

	Noesis::Ptr<Noesis::IView> view = Noesis::GUI::CreateView((Noesis::FrameworkElement*)xaml);
	view->SetSize(pSettings->mWidth, pSettings->mHeight);
	view->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
	view->GetRenderer()->Init(renderer);

	return (NoesisView*)view.GiveOwnership();
}

void removeNoesisUIView(NoesisView* view)
{
	ASSERT(view);

	Noesis::IView* noesisView = (Noesis::IView*)view;
	noesisView->GetRenderer()->Shutdown();
	noesisView->Release();
}

FORGE_API NoesisTexture* addNoesisUITexture(Texture* pTexture, TinyImageFormat format)
{
	ASSERT(pTexture);

	Noesis::Ptr<Noesis::Texture> texture = Noesis::MakePtr<ForgeTexture>(pTexture, format);

	return (NoesisTexture*)texture.GiveOwnership();
}

FORGE_API void removeNoesisUITexture(NoesisTexture* texture)
{
	ASSERT(texture);

	Noesis::Texture* noesisTexture = (Noesis::Texture*)texture;
	noesisTexture->Release();
}

FORGE_API NoesisRenderTarget* addNoesisUIRenderTarget(RenderTarget* pTarget, Renderer* pRenderer, TinyImageFormat format)
{
	ASSERT(pTarget);

	Noesis::Ptr<Noesis::RenderTarget> target = Noesis::MakePtr<ForgeRenderTarget>(pTarget, pRenderer, format);

	return (NoesisRenderTarget*)target.GiveOwnership();
}

FORGE_API void removeNoesisUIRenderTarget(NoesisRenderTarget* target)
{
	ASSERT(target);

	Noesis::RenderTarget* noesisTarget = (Noesis::RenderTarget*)target;
	noesisTarget->Release();
}

FORGE_API NoesisTexture* getNoesisUIRenderTargetTexture(NoesisRenderTarget* target)
{
	ASSERT(target);

	Noesis::RenderTarget* noesisTarget = (Noesis::RenderTarget*)target;
	ForgeRenderTarget* forgeTarget = (ForgeRenderTarget*)noesisTarget;

	return (NoesisTexture*)forgeTarget->pTexture;
}

FORGE_API RenderTarget* getNoesisUIRenderTargetTF(NoesisRenderTarget* target)
{
	ASSERT(target);

	Noesis::RenderTarget* noesisTarget = (Noesis::RenderTarget*)target;
	ForgeRenderTarget* forgeTarget = (ForgeRenderTarget*)noesisTarget;

	return forgeTarget->pTarget;
}

// Internal

Noesis::MouseButton mouseButtonToNoesis(uint32_t action)
{
	switch (action)
	{
	case UISystemInputActions::UI_ACTION_MOUSE_LEFT:
		return Noesis::MouseButton::MouseButton_Left;

	case UISystemInputActions::UI_ACTION_MOUSE_MIDDLE:
		return Noesis::MouseButton::MouseButton_Middle;

	case UISystemInputActions::UI_ACTION_MOUSE_RIGHT:
		return Noesis::MouseButton::MouseButton_Right;

	default:
		ASSERT(0);
		break;
	}

	return Noesis::MouseButton::MouseButton_Count;
}

Noesis::Key keyToNoesis(uint32_t action, bool gamepad)
{
	switch (action)
	{
	case UISystemInputActions::UI_ACTION_NAV_TOGGLE_UI: return gamepad ? Noesis::Key::Key_GamepadContext1 : Noesis::Key::Key_F1;
	case UISystemInputActions::UI_ACTION_NAV_ACTIVATE: return Noesis::Key::Key_GamepadAccept;
	case UISystemInputActions::UI_ACTION_NAV_CANCEL: return Noesis::Key::Key_GamepadCancel;
	case UISystemInputActions::UI_ACTION_NAV_INPUT: return Noesis::Key::Key_GamepadView;
	case UISystemInputActions::UI_ACTION_NAV_MENU: return Noesis::Key::Key_GamepadMenu;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_LEFT: return Noesis::Key::Key_GamepadLeft;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_RIGHT: return Noesis::Key::Key_GamepadRight;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_UP: return Noesis::Key::Key_GamepadUp;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_DOWN: return Noesis::Key::Key_GamepadDown;
	case UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW: return Noesis::Key_Scroll;
	case UISystemInputActions::UI_ACTION_NAV_FOCUS_PREV: return Noesis::Key::Key_GamepadPageLeft;
	case UISystemInputActions::UI_ACTION_NAV_FOCUS_NEXT: return Noesis::Key::Key_GamepadPageRight;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_SLOW: return Noesis::Key::Key_GamepadPageUp;
	case UISystemInputActions::UI_ACTION_NAV_TWEAK_FAST: return Noesis::Key::Key_GamepadPageDown;

	case UISystemInputActions::UI_ACTION_KEY_TAB: return Noesis::Key::Key_Tab;
	case UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW: return Noesis::Key::Key_Left;
	case UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW: return Noesis::Key::Key_Right;
	case UISystemInputActions::UI_ACTION_KEY_UP_ARROW: return Noesis::Key::Key_Up;
	case UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW: return Noesis::Key::Key_Down;
	case UISystemInputActions::UI_ACTION_KEY_PAGE_UP: return Noesis::Key::Key_PageUp;
	case UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN: return Noesis::Key::Key_PageDown;
	case UISystemInputActions::UI_ACTION_KEY_HOME: return Noesis::Key::Key_Home;
	case UISystemInputActions::UI_ACTION_KEY_END: return Noesis::Key::Key_End;
	case UISystemInputActions::UI_ACTION_KEY_INSERT: return Noesis::Key::Key_Insert;
	case UISystemInputActions::UI_ACTION_KEY_DELETE: return Noesis::Key::Key_Delete;
	case UISystemInputActions::UI_ACTION_KEY_BACK_SPACE: return Noesis::Key::Key_Back;
	case UISystemInputActions::UI_ACTION_KEY_SPACE: return Noesis::Key::Key_Space;
	case UISystemInputActions::UI_ACTION_KEY_ENTER: return Noesis::Key::Key_Enter;
	case UISystemInputActions::UI_ACTION_KEY_ESCAPE: return Noesis::Key::Key_Escape;
	case UISystemInputActions::UI_ACTION_KEY_A: return Noesis::Key::Key_A;
	case UISystemInputActions::UI_ACTION_KEY_C: return Noesis::Key::Key_C;
	case UISystemInputActions::UI_ACTION_KEY_V: return Noesis::Key::Key_V;
	case UISystemInputActions::UI_ACTION_KEY_X: return Noesis::Key::Key_X;
	case UISystemInputActions::UI_ACTION_KEY_Y: return Noesis::Key::Key_Y;
	case UISystemInputActions::UI_ACTION_KEY_Z: return Noesis::Key::Key_Z;

	default:
		break;
	}

	return Noesis::Key::Key_Count;
}
