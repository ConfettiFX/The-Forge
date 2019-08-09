#pragma once
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/Renderer/IRenderer.h"

#include "IntrusiveUnorderedPtrSet.h"

#include "AABBox.h"
#include "Camera.h"
#include "AtlasQuads.h"
#include "ASM.h"
#include "ASMQuadTree.h"

class QuadTreeNode;
class ASMFrustum;
class ASMTileCache;


class ASMTileCacheEntry
{
public:
	struct SViewport { int x, y, w, h; } m_viewport;
	unsigned char m_refinement;

	AABBox m_BBox;
	QuadTreeNode* m_pOwner;
	ASMFrustum* m_pFrustum;
	unsigned int m_lastFrameUsed;
	unsigned int m_frustumID;
	bool m_isLayer;
	float m_fadeInFactor;

	Camera m_renderCamera;

	ASMTileCacheEntry(ASMTileCache* pCache, int x, int y);
	~ASMTileCacheEntry();

	template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
	void Allocate(QuadTreeNode* pOwner, ASMFrustum* pFrustum);
	void Free();
	void Invalidate();
	void MarkReady();
	void MarkNotReady();

	bool IsReady() const { return m_readyTilesPos.IsInserted(); }
	ASMTileCache* GetCache() const { return m_pCache; }
	bool IsAllocated() const { return m_pOwner != nullptr; }
	bool IsBeingUpdated() const { return m_updateQueuePos.IsInserted(); }
	bool IsQueuedForRendering() const { return m_renderQueuePos.IsInserted(); }

	static const ivec4 GetRect(const SViewport& vp, int border) 
	{ 
		return ivec4(vp.x - border, 
			vp.y - border,
			vp.x + vp.w + border, 
			vp.y + vp.h + border);
	}

protected:
	ASMTileCache* m_pCache;

	CIntrusiveUnorderedSetItemHandle m_tilesPos;
	CIntrusiveUnorderedSetItemHandle m_freeTilesPos;
	CIntrusiveUnorderedSetItemHandle m_renderQueuePos;
	CIntrusiveUnorderedSetItemHandle m_readyTilesPos;
	CIntrusiveUnorderedSetItemHandle m_demQueuePos;
	CIntrusiveUnorderedSetItemHandle m_renderBatchPos;
	CIntrusiveUnorderedSetItemHandle m_updateQueuePos;

	CIntrusiveUnorderedSetItemHandle& GetTilesPos() { return m_tilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetFreeTilesPos() { return m_freeTilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetRenderQueuePos() { return m_renderQueuePos; }
	CIntrusiveUnorderedSetItemHandle& GetReadyTilesPos() { return m_readyTilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetDemQueuePos() { return m_demQueuePos; }
	CIntrusiveUnorderedSetItemHandle& GetRenderBatchPos() { return m_renderBatchPos; }
	CIntrusiveUnorderedSetItemHandle& GetUpdateQueuePos() { return m_updateQueuePos; }

	void PrepareRender(const ASM::SShadowMapPrepareRenderContext& context);

	friend class ASMTileCache;
};

class ASMTileCache
{
public:
	ASMTileCache();
	~ASMTileCache();


	void Load(const ASM::RenderTargets& renderTargets);

	template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
	ASMTileCacheEntry* Allocate(QuadTreeNode* pNode, ASMFrustum* pFrustum);

	int AddTileFromRenderQueueToRenderBatch(
		ASMFrustum* pFrustum,
		int maxRefinement,
		bool isLayer);

	int AddTileFromUpdateQueueToRenderBatch(
		ASMFrustum* pFrustum,
		int maxRefinement,
		bool isLayer);

	bool PrepareRenderTilesBatch(const ASM::SShadowMapPrepareRenderContext& context);

	void RenderTilesBatch(
		RenderTarget* workBufferDepth,
		RenderTarget* workBufferColor,
		ASM::ASM::SShadowMapRenderContext& context);

	void CreateDEM(
		RenderTarget* demWorkBufferColor,
		const ASM::ASM::SShadowMapRenderContext& context,
		bool createDemForLayerRendering);

	const Camera* GetFirstRenderBatchCamera() const;


	/*void UpdateTiles(
		ASMFrustum* pFrustum,
		const AABBox& BBoxWS);*/

	RenderTarget* GetDepthAtlas()  { return m_depthAtlas; }
	RenderTarget* GetDEMAtlas()  { return m_demAtlas; }
	const unsigned int GetDepthAtlasWidth() const { return m_depthAtlasWidth; }
	const unsigned int GetDepthAtlasHeight() const { return m_depthAtlasHeight; }
	const unsigned int GetDEMAtlasWidth() const { return m_demAtlasWidth; }
	const unsigned int GetDEMAtlasHeight() const { return m_demAtlasHeight; }

	void Tick(float deltaTime);
	//void DrawDebug(DebugRenderer& debug);

	bool NothingToRender() const { return m_renderBatch.empty(); }
	bool IsFadeInFinished(const ASMFrustum* pFrustum) const;

	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetTilesPos > m_tiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetFreeTilesPos > m_freeTiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetRenderQueuePos > m_renderQueue;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetReadyTilesPos > m_readyTiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetDemQueuePos > m_demQueue;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetRenderBatchPos > m_renderBatch;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetUpdateQueuePos > m_updateQueue;

	unsigned int m_cacheHits;
	unsigned int m_tileAllocs;
	unsigned int m_numTilesRendered;
	unsigned int m_numTilesUpdated;
	unsigned int m_depthAtlasWidth;
	unsigned int m_depthAtlasHeight;
	unsigned int m_demAtlasWidth;
	unsigned int m_demAtlasHeight;

	RenderTarget* m_depthAtlas;
	RenderTarget* m_demAtlas;

	template< class T >
	int AddTileToRenderBatch(
		T& tilesQueue,
		ASMFrustum* pFrustum,
		int maxRefinement,
		bool isLayer);

	void RenderTiles(
		unsigned int numTiles,
		ASMTileCacheEntry** tiles,
		RenderTarget* workBufferDepth,
		RenderTarget* workBufferColor,
		ASM::SShadowMapRenderContext& context,
		bool allowDEM);

	void StartDEM(ASMTileCacheEntry* pTile, AtlasQuads::SCopyQuad& copyDEMQuad);

	friend class ASMTileCacheEntry;

	bool mDEMFirstTimeRender;
	bool mDepthFirstTimeRender;
};


