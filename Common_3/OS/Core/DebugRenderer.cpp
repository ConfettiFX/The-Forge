#include "DebugRenderer.h"

#include "../../Renderer/IRenderer.h"
#include "../../Renderer/GpuProfiler.h"

#include "../../../Middleware_3/Text/Fontstash.h"

#include "../../OS/Interfaces/IMemoryManager.h"

typedef struct GpuProfileDrawDesc
{
	float        mChildIndent = 25.0f;
	float        mHeightOffset = 25.0f;
	TextDrawDesc mDrawDesc = TextDrawDesc(0, 0xFF00CCAA, 15);
} GpuProfileDrawDesc;

using PipelineMap = tinystl::unordered_map<uint64_t, Pipeline*>;
static Fontstash*         pFontstash = NULL;
static TextDrawDesc       gDefaultTextDrawDesc = TextDrawDesc(0, 0xffffffff, 16);
static GpuProfileDrawDesc gDefaultGpuProfileDrawDesc = {};
#ifndef METAL
static Shader*        pShaderTextured;
static RootSignature* pRootSignatureTextured;
static PipelineMap    gPipelinesTextured;
static Sampler*       pDefaultSampler;
#endif

#if defined(__linux__)
#define sprintf_s sprintf    // On linux, we should use sprintf as sprintf_s is not part of the standard c library
#endif

static void draw_gpu_profile_recurse(
	Cmd* pCmd, Fontstash* pFontStash, float2& startPos, const GpuProfileDrawDesc* pDrawDesc, struct GpuProfiler* pGpuProfiler,
	GpuTimerTree* pRoot)
{
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	if (!pRoot)
		return;

	float originalX = startPos.getX();

	if (pRoot->mGpuTimer.mIndex > 0 && pRoot != &pGpuProfiler->mRoot)
	{
		char   buffer[128];
		double time = getAverageGpuTime(pGpuProfiler, &pRoot->mGpuTimer);
		sprintf_s(buffer, "%s -  %f ms", pRoot->mGpuTimer.mName.c_str(), time * 1000.0);

		pFontStash->drawText(
			pCmd, buffer, startPos.x, startPos.y, pDrawDesc->mDrawDesc.mFontID, pDrawDesc->mDrawDesc.mFontColor,
			pDrawDesc->mDrawDesc.mFontSize, pDrawDesc->mDrawDesc.mFontSpacing, pDrawDesc->mDrawDesc.mFontBlur);
		startPos.y += pDrawDesc->mHeightOffset;

		if ((uint32_t)pRoot->mChildren.size())
			startPos.setX(startPos.getX() + pDrawDesc->mChildIndent);
	}

	for (uint32_t i = 0; i < (uint32_t)pRoot->mChildren.size(); ++i)
	{
		draw_gpu_profile_recurse(pCmd, pFontStash, startPos, pDrawDesc, pGpuProfiler, pRoot->mChildren[i]);
	}

	startPos.x = originalX;
#endif
}

void initDebugRendererInterface(Renderer* pRenderer, const char* pDebugFontPath, FSRoot root)
{
	pFontstash = conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), pRenderer, (int)512, (int)512);

	if (pDebugFontPath)
	{
		pFontstash->defineFont("default", pDebugFontPath, root);
	}
}

void removeDebugRendererInterface()
{
	pFontstash->destroy();
	conf_free(pFontstash);
}

uint32_t addDebugFont(const char* pDebugFontPath, FSRoot root) { return pFontstash->defineFont("default", pDebugFontPath, root); }

void drawDebugText(Cmd* pCmd, float x, float y, const char* pText, const TextDrawDesc* pDrawDesc)
{
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pFontstash->drawText(pCmd, pText, x, y, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

//text rendering in world space
void drawDebugText(Cmd* pCmd, const mat4& mProjView, const mat4& mWorldMat, const char* pText, const TextDrawDesc* pDrawDesc)
{
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pFontstash->drawText(
		pCmd, pText, mProjView, mWorldMat, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

void drawDebugTexture(Cmd* pCmd, float x, float y, float w, float h, Texture* pTexture, float r, float g, float b)
{
	//// the last variable can be used to create a border
	//TexVertex pVertices[] = { MAKETEXQUAD(x, y, x + w, y + h, 0) };
	//int nVertices = sizeof(pVertices) / sizeof(pVertices[0]);
	//float4 color = { r, g, b, 1.0f };
	////pUIRenderer->drawTextured(pCmd, PRIMITIVE_TOPO_TRI_STRIP, pVertices, nVertices, pTexture, &color);
}

void drawDebugGpuProfile(Cmd* pCmd, float x, float y, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc)
{
	const GpuProfileDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultGpuProfileDrawDesc;
	float2                    pos = { x, y };
	pFontstash->drawText(
		pCmd, "-----GPU Times-----", pos.x, pos.y, pDesc->mDrawDesc.mFontID, pDesc->mDrawDesc.mFontColor, pDesc->mDrawDesc.mFontSize,
		pDesc->mDrawDesc.mFontSpacing, pDesc->mDrawDesc.mFontBlur);
	pos.y += pDesc->mHeightOffset;

	draw_gpu_profile_recurse(pCmd, pFontstash, pos, pDesc, pGpuProfiler, &pGpuProfiler->mRoot);
}
