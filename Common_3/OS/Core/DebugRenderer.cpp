#include "DebugRenderer.h"

#include "../../Renderer/IRenderer.h"
#include "../../Renderer/GpuProfiler.h"
#include "../../OS/Math/MathTypes.h"

#include "../../../Middleware_3/UI/UIRenderer.h"
#include "../../../Middleware_3/UI/Fontstash.h"

#include "../../OS/Interfaces/IMemoryManager.h"

static UIRenderer*			pUIRenderer = NULL;
static DebugTextDrawDesc	gDefaultTextDrawDesc = DebugTextDrawDesc(0xffffffff, 16);
static GpuProfileDrawDesc	gDefaultGpuProfileDrawDesc = {};

#if defined(LINUX)
	#define sprintf_s sprintf // On linux, we should use sprintf as sprintf_s is not part of the standard c library
#endif

static void draw_gpu_profile_recurse(Cmd* pCmd, Fontstash* pFontStash, float2& startPos, const GpuProfileDrawDesc* pDrawDesc, struct GpuProfiler* pGpuProfiler, GpuTimerTree* pRoot)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	if (!pRoot)
		return;

	float originalX = startPos.getX();

	if (pRoot->mGpuTimer.mIndex > 0 && pRoot != &pGpuProfiler->mRoot)
	{
		char buffer[128];
		double time = getAverageGpuTime(pGpuProfiler, &pRoot->mGpuTimer);
		sprintf_s(buffer, "%s -  %f ms", pRoot->mGpuTimer.mName.c_str(), time * 1000.0);

		pFontStash->drawText(pCmd, buffer, startPos.x, startPos.y, pDrawDesc->mDrawDesc.mFontID,
			pDrawDesc->mDrawDesc.mFontColor, pDrawDesc->mDrawDesc.mFontSize, pDrawDesc->mDrawDesc.mFontSpacing, pDrawDesc->mDrawDesc.mFontBlur);
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

void initDebugRendererInterface(Renderer* pRenderer, const char* pDebugFontPath)
{
	pUIRenderer = conf_placement_new<UIRenderer>(conf_calloc(1, sizeof(*pUIRenderer)), pRenderer);
	pUIRenderer->addFontstash(512, 512);

	if (pDebugFontPath)
	{
		pUIRenderer->getFontstash(0)->defineFont("default", pDebugFontPath);
	}
}

void removeDebugRendererInterface()
{
	pUIRenderer->~UIRenderer();
	conf_free(pUIRenderer);
}

uint32_t addDebugFont(const char* pDebugFontPath)
{
	return pUIRenderer->getFontstash(0)->defineFont("default", pDebugFontPath);
}

void drawDebugText(Cmd* pCmd, float x, float y, const char* pText, const DebugTextDrawDesc* pDrawDesc)
{
	pUIRenderer->beginRender(
		pCmd->mBoundWidth, pCmd->mBoundHeight,
		pCmd->mBoundRenderTargetCount, (ImageFormat::Enum*)pCmd->pBoundColorFormats, pCmd->pBoundSrgbValues,
		(ImageFormat::Enum)pCmd->mBoundDepthStencilFormat,
		pCmd->mBoundSampleCount, pCmd->mBoundSampleQuality);

	const DebugTextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pUIRenderer->getFontstash(0)->drawText(pCmd, pText, x, y,
		pDesc->mFontID, pDesc->mFontColor,
		pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

void drawDebugTexture(Cmd* pCmd, float x, float y, float w, float h, Texture* pTexture, float r, float g, float b)
{
	pUIRenderer->beginRender(
		pCmd->mBoundWidth, pCmd->mBoundHeight,
		pCmd->mBoundRenderTargetCount, (ImageFormat::Enum*)pCmd->pBoundColorFormats, pCmd->pBoundSrgbValues,
		(ImageFormat::Enum)pCmd->mBoundDepthStencilFormat,
		pCmd->mBoundSampleCount, pCmd->mBoundSampleQuality);

	// the last variable can be used to create a border
	TexVertex pVertices[] = { MAKETEXQUAD(x, y, x + w, y + h, 0) };
	int nVertices = sizeof(pVertices) / sizeof(pVertices[0]);
	float4 color = { r, g, b, 1.0f };
	pUIRenderer->drawTextured(pCmd, PRIMITIVE_TOPO_TRI_STRIP, pVertices, nVertices, pTexture, &color);
}

void drawDebugGpuProfile(Cmd* pCmd, float x, float y, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc)
{
	pUIRenderer->beginRender(
		pCmd->mBoundWidth, pCmd->mBoundHeight,
		pCmd->mBoundRenderTargetCount, (ImageFormat::Enum*)pCmd->pBoundColorFormats, pCmd->pBoundSrgbValues,
		(ImageFormat::Enum)pCmd->mBoundDepthStencilFormat,
		pCmd->mBoundSampleCount, pCmd->mBoundSampleQuality);

	Fontstash* pFontStash = pUIRenderer->getFontstash(0);
	const GpuProfileDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultGpuProfileDrawDesc;
	float2 pos = { x, y };
	pFontStash->drawText(pCmd, "-----GPU Times-----",
		pos.x, pos.y, pDesc->mDrawDesc.mFontID, pDesc->mDrawDesc.mFontColor, pDesc->mDrawDesc.mFontSize, pDesc->mDrawDesc.mFontSpacing, pDesc->mDrawDesc.mFontBlur);
	pos.y += pDesc->mHeightOffset;

	draw_gpu_profile_recurse(pCmd, pFontStash, pos, pDesc, pGpuProfiler, &pGpuProfiler->mRoot);
}
