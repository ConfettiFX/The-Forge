#include "ProfilerEnableMacro.h"

#if (PROFILE_ENABLED)

#include "Profiler.h"

#include "../../Renderer/ResourceLoader.h"
#include "../../../Middleware_3/Text/Fontstash.h"
#include "../../../Middleware_3/UI/AppUI.h"
#include "../../OS/Interfaces/IMemoryManager.h"

static UIApp* gUIApp_MP = NULL;

struct ProfileVertex
{
	float mX, mY;
	uint32_t mColor/*, mUV*/;
};

#define PROFILE_MAX_VERTEX_COUNT 8192
static tinystl::vector<ProfileVertex> ProfileVertices(PROFILE_MAX_VERTEX_COUNT);

#define MAX_TEXTS 512
struct ProfileTextData
{
	tinystl::string mText;
	uint32_t mColor;
	int mX, mY;
};
static ProfileTextData Texts[MAX_TEXTS];
static uint32_t TextCounter = 0;

struct ProfileDrawCommand
{
	enum Type{ TEXT = 0, BOX, LINE };
	
	ProfileDrawCommand(Type type, uint32_t size)
		: mType(type)
		, mSize(size)
	{ }

	Type mType;
	uint32_t mSize = 0;
};

static tinystl::vector<ProfileDrawCommand> DrawCommands;

static float HalfWidth;
static float HalfHeight;

// Pipeline data
static RootSignature * pProfileRootSignature = nullptr;
static DescriptorBinder * pProfileDescriptorBinder = nullptr;
static RasterizerState * pProfileRasterizerState = nullptr;
static BlendState * pBlendState = nullptr;
static Pipeline * pProfilePipelineBox = nullptr;
static Pipeline * pProfilePipelineLine = nullptr;

// Draw data
static int BufferCount;
static Buffer ** ppProfileBuffers = nullptr;
static Shader * pProfileShader = nullptr;

extern void ProfileInitUI();

void ProfileInitialize(Renderer * pRenderer, int image_count)
{
	// Initialize UI
	ProfileInitUI();

	// Reserve pointer for line resources
	BufferCount = image_count;
	ppProfileBuffers = (Buffer **)conf_malloc(sizeof(Buffer *) * image_count);
	memset(ppProfileBuffers, 0, sizeof(Buffer *) * image_count);

	// Box shader
	ShaderLoadDesc shader_descriptor{};
	shader_descriptor.mStages[0] = { "profile.vert", NULL, 0, FSR_SrcShaders };
	shader_descriptor.mStages[1] = { "profile.frag", NULL, 0, FSR_SrcShaders };
	addShader(pRenderer, &shader_descriptor, &pProfileShader);

	// Root signature
	Shader * shaders[] = { pProfileShader };
	RootSignatureDesc root_descriptor{};
	root_descriptor.mStaticSamplerCount = 0;
	root_descriptor.ppStaticSamplerNames = nullptr;
	root_descriptor.ppStaticSamplers = nullptr;
	root_descriptor.mShaderCount = 1;
	root_descriptor.ppShaders = shaders;
	addRootSignature(pRenderer, &root_descriptor, &pProfileRootSignature);

	// Descriptor binder
	DescriptorBinderDesc descriptor_binder_descriptor[1] = { { pProfileRootSignature } };
	addDescriptorBinder(pRenderer, 0, 1, descriptor_binder_descriptor, &pProfileDescriptorBinder);

	// Rasterizer
	RasterizerStateDesc rasterizer_state_descriptor{};
	rasterizer_state_descriptor.mCullMode = CULL_MODE_NONE;
	addRasterizerState(pRenderer, &rasterizer_state_descriptor, &pProfileRasterizerState);

	BlendStateDesc blend_state_descriptor = {};
	blend_state_descriptor.mSrcFactors[0] = BC_SRC_ALPHA;
	blend_state_descriptor.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
	blend_state_descriptor.mBlendModes[0] = BM_ADD;
	blend_state_descriptor.mSrcAlphaFactors[0] = BC_ONE;
	blend_state_descriptor.mDstAlphaFactors[0] = BC_ZERO;
	blend_state_descriptor.mBlendAlphaModes[0] = BM_ADD;
	blend_state_descriptor.mMasks[0] = ALL;
	blend_state_descriptor.mRenderTargetMask = BLEND_STATE_TARGET_0;
	blend_state_descriptor.mIndependentBlend = false;
	addBlendState(pRenderer, &blend_state_descriptor, &pBlendState);

	// Profile buffers used for rendering
	BufferLoadDesc line_buffer_descriptor{};
	line_buffer_descriptor.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	line_buffer_descriptor.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	line_buffer_descriptor.mDesc.mSize = sizeof(ProfileVertex) * PROFILE_MAX_VERTEX_COUNT;
	line_buffer_descriptor.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	line_buffer_descriptor.mDesc.mVertexStride = sizeof(ProfileVertex);
	line_buffer_descriptor.pData = nullptr;

	for (int i = 0; i < image_count; ++i)
	{
		line_buffer_descriptor.ppBuffer = &ppProfileBuffers[i];
		addResource(&line_buffer_descriptor);
	}
}

void ProfileLoad(Renderer * pRenderer, SwapChain * pSwapChain)
{
	// Box pipeline
	// Vertex input layout
	VertexLayout vertex_layout{};
	vertex_layout.mAttribCount = 2;
	vertex_layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertex_layout.mAttribs[0].mFormat = ImageFormat::RG32F;
	vertex_layout.mAttribs[0].mBinding = 0;
	vertex_layout.mAttribs[0].mLocation = 0;
	vertex_layout.mAttribs[0].mOffset = 0;
	vertex_layout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
	vertex_layout.mAttribs[1].mFormat = ImageFormat::RGBA8;
	vertex_layout.mAttribs[1].mBinding = 0;
	vertex_layout.mAttribs[1].mLocation = 1;
	vertex_layout.mAttribs[1].mOffset = sizeof(float) * 2;

	PipelineDesc pipeline_descriptor{};
	pipeline_descriptor.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc & pipeline_settings = pipeline_descriptor.mGraphicsDesc;
	pipeline_settings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipeline_settings.mRenderTargetCount = 1;
	pipeline_settings.pDepthState = nullptr;
	pipeline_settings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
	pipeline_settings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
	pipeline_settings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
	pipeline_settings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
	pipeline_settings.mDepthStencilFormat = ImageFormat::NONE;
	pipeline_settings.pRootSignature = pProfileRootSignature;
	pipeline_settings.pShaderProgram = pProfileShader;
	pipeline_settings.pVertexLayout = &vertex_layout;
	pipeline_settings.pRasterizerState = pProfileRasterizerState;
	pipeline_settings.pBlendState = pBlendState;
	addPipeline(pRenderer, &pipeline_descriptor, &pProfilePipelineBox);

	// Line pipeline
	pipeline_settings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
	addPipeline(pRenderer, &pipeline_descriptor, &pProfilePipelineLine);
}

void ProfileUnload(Renderer * pRenderer)
{
	removePipeline(pRenderer, pProfilePipelineBox);
	removePipeline(pRenderer, pProfilePipelineLine);
}

extern void ProfileShutdown();

void ProfileExit(Renderer * pRenderer)
{
	removeBlendState(pBlendState);
	removeRasterizerState(pProfileRasterizerState);

	removeDescriptorBinder(pRenderer, pProfileDescriptorBinder);
	removeRootSignature(pRenderer, pProfileRootSignature);

	removeShader(pRenderer, pProfileShader);

	// Free line buffers if any
	for (int i = 0; i < BufferCount; ++i)
	{
		if (ppProfileBuffers[i])
			removeResource(ppProfileBuffers[i]);
	}
	
	conf_free(ppProfileBuffers);
	BufferCount = 0;

	ProfileShutdown();
}

float ConvertToNDCX(int value)
{
	return static_cast<float>(value) / HalfWidth - 1.0f;
}

float ConvertToNDCX(float value)
{
	return value / HalfWidth - 1.0f;
}

float ConvertToNDCY(int value)
{
	return -(static_cast<float>(value) / HalfHeight - 1.0f);
}

float ConvertToNDCY(float value)
{
	return -(value / HalfHeight - 1.0f);
}

void ProfileDrawText(int nX, int nY, uint32_t nColor, const char* pText, uint32_t nNumCharacters)
{
	if (TextCounter < MAX_TEXTS)
	{
		Texts[TextCounter].mColor = 0xff000000 | ((nColor & 0xff) << 16) | (nColor & 0xff00) | ((nColor >> 16) & 0xff);
		Texts[TextCounter].mText = pText;
		Texts[TextCounter].mX = nX;
		Texts[TextCounter++].mY = nY;

		if (DrawCommands.empty() || DrawCommands.back().mType != ProfileDrawCommand::TEXT)
			DrawCommands.emplace_back(ProfileDrawCommand{ ProfileDrawCommand::TEXT, 1u });
		else
			++DrawCommands.back().mSize;
	}
}

void ProfileDrawBox(int nX, int nY, int nX1, int nY1, uint32_t nColor, ProfileBoxType type)
{
	// Store box
	if (ProfileVertices.size() < PROFILE_MAX_VERTEX_COUNT && nX != nX1 && nY != nY1)
	{
		nColor = ((nColor & 0xff) << 16) | ((nColor >> 16) & 0xff) | (0xff00ff00 & nColor);
		float left = ConvertToNDCX(nX);
		float right = ConvertToNDCX(nX1);
		float top = ConvertToNDCY(nY);
		float bot = ConvertToNDCY(nY1);
	
		// Create box
		ProfileVertices.emplace_back(ProfileVertex{ left, top, nColor/*, 0*/ });
		ProfileVertices.emplace_back(ProfileVertex{ left, bot, nColor/*, 0*/ });
		ProfileVertices.emplace_back(ProfileVertex{ right, bot, nColor/*, 0*/ });
		ProfileVertices.emplace_back(ProfileVertex{ left, top, nColor/*, 0*/ });
		ProfileVertices.emplace_back(ProfileVertex{ right, bot, nColor/*, 0*/ });
		ProfileVertices.emplace_back(ProfileVertex{ right, top, nColor/*, 0*/ });
	
		// Add command
		if (DrawCommands.empty() || DrawCommands.back().mType != ProfileDrawCommand::BOX)
			DrawCommands.emplace_back(ProfileDrawCommand{ ProfileDrawCommand::BOX, 6u });
		else
			DrawCommands.back().mSize += 6u;
	}
}

void ProfileDrawLine2D(uint32_t nVertices, float* pVertices, uint32_t nColor)
{
	// Store line
	if (ProfileVertices.size() < PROFILE_MAX_VERTEX_COUNT)
	{
		nColor = ((nColor & 0xff) << 16) | (nColor & 0xff00ff00) | ((nColor >> 16) & 0xff);
		float prev_x = ConvertToNDCX(pVertices[0]);
		float prev_y = ConvertToNDCY(pVertices[1]);
	
		for (uint32_t i = 1; i < nVertices; ++i)
		{
			ProfileVertices.emplace_back(ProfileVertex{ prev_x, prev_y, nColor });
			prev_x = ConvertToNDCX(pVertices[i * 2]);
			prev_y = ConvertToNDCY(pVertices[i * 2 + 1]);
			ProfileVertices.emplace_back(ProfileVertex{ prev_x, prev_y, nColor });
		}
	
		if (DrawCommands.empty() || DrawCommands.back().mType != ProfileDrawCommand::LINE)
			DrawCommands.emplace_back(ProfileDrawCommand{ ProfileDrawCommand::LINE, (nVertices - 1) * 2 });
		else
			DrawCommands.back().mSize += (nVertices - 1) * 2;
	}
}

void ActivateMicroProfile(UIApp * app, bool isActive)
{
	gUIApp_MP = app;
	ProfileSetDisplayMode(isActive);
}

void ProfileBeginDraw(uint32_t width, uint32_t height)
{
	HalfWidth = static_cast<float>(width) / 2.0f;
	HalfHeight = static_cast<float>(height) / 2.0f;
	ProfileVertices.clear();
	DrawCommands.clear();
	TextCounter = 0;
}

void ProfileEndDraw(Cmd * pCmd)
{
	// Update buffer
	Buffer * frame_buffer = ppProfileBuffers[pCmd->pRenderer->mCurrentFrameIdx];
	BufferUpdateDesc update_desc{ frame_buffer, ProfileVertices.data(), 0, 0, ProfileVertices.size() * sizeof(ProfileVertex) };
	updateResource(&update_desc);

	// Draw all commands
	TextCounter = 0;
	uint32_t offset = 0;
	const int font_id = 0, font_size = 15, font_spacing = 0, font_blur = 0;
	for (uint32_t i = 0; i < DrawCommands.size(); ++i)
	{
		switch (DrawCommands[i].mType)
		{
		case ProfileDrawCommand::TEXT:
			for (uint32_t j = 0; j < DrawCommands[i].mSize; ++j, ++TextCounter)
			{
				gUIApp_MP->pImpl->pFontStash->drawText(
						pCmd, Texts[TextCounter].mText.c_str(),
						static_cast<float>(Texts[TextCounter].mX), static_cast<float>(Texts[TextCounter].mY),
						font_id, Texts[TextCounter].mColor, font_size,
						font_spacing, font_blur);
			}
			continue;
		case ProfileDrawCommand::BOX:
			cmdBindPipeline(pCmd, pProfilePipelineBox);
			break;
		case ProfileDrawCommand::LINE:
			cmdBindPipeline(pCmd, pProfilePipelineLine);
			break;
		}

		cmdBindDescriptors(pCmd, pProfileDescriptorBinder, pProfileRootSignature, 0, nullptr);
		cmdBindVertexBuffer(pCmd, 1, &frame_buffer, nullptr);
		cmdDraw(pCmd, DrawCommands[i].mSize, offset);
		offset += DrawCommands[i].mSize;
	}
}

#endif
