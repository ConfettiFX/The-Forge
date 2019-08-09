#include "ASMTileCache.h"
#include "ASMQuadTree.h"
#include "ASMFrustum.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"


//template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)() const >
bool ASM::GetRectangleWithinParent(
	const int NUp,
	QuadTreeNode* nodeList[ASM::MAX_REFINEMENT + 1],
	ivec4& parentRect,
	ivec4& tileRect)
{
	for (int i = 0; i < NUp; ++i)
	{
		nodeList[i + 1] = nodeList[i]->m_pParent;
	}

	ASMTileCacheEntry* pParentTile = nodeList[NUp]->m_pTile;
	if (pParentTile)
	{
		parentRect = ASMTileCacheEntry::GetRect(pParentTile->m_viewport, 
			ASM::TILE_BORDER_TEXELS);

		ivec4 src = parentRect;
		for (int i = 0; i < NUp; ++i)
		{
			vec3 d = nodeList[NUp - i - 1]->mBBox.m_min - nodeList[NUp - i]->mBBox.m_min;

			ivec4 rect;
			rect.setX(d.getX() <= 0 ? src.getX() : ((src.getZ() + src.getX()) / 2));
			rect.setY(d.getY() > 0 ? src.getY() : ((src.getW() + src.getW()) / 2));
			rect.setZ(rect.getX() + (src.getZ() - src.getX()) / 2);
			rect.setW(rect.getY() + (src.getW() - src.getY()) / 2);

			const int border = ASM::TILE_BORDER_TEXELS >> (i + 1);
			rect.setX( rect.getX() + (d.getX() <= 0 ? border : -border));
			rect.setZ( rect.getZ() + (d.getX() <= 0 ? border : -border));
			rect.setY( rect.getY() +  (d.getY() > 0 ? border : -border));
			rect.setW( rect.getW() + (d.getY() > 0 ? border : -border));

			src = rect;
		}

		tileRect = src;
		return true;
	}

	return false;
}


ASMTileCache::ASMTileCache()
	:m_cacheHits(0),
	m_tileAllocs(0),
	m_numTilesRendered(0),
	m_numTilesUpdated(0),
	m_depthAtlasWidth(0),
	m_depthAtlasHeight(0),
	m_demAtlasWidth(0),
	m_demAtlasHeight(0),
	mDEMFirstTimeRender(true),
	mDepthFirstTimeRender(true)
{
	
	m_depthAtlasWidth = gs_ASMDepthAtlasTextureWidth;
	m_depthAtlasHeight = gs_ASMDepthAtlasTextureHeight;

	m_demAtlasWidth = gs_ASMDEMAtlasTextureWidth;
	m_demAtlasHeight = gs_ASMDEMAtlasTextureHeight;


	int gridWidth = m_depthAtlasWidth / ASM::TILE_SIZE;
	int gridHeight = m_depthAtlasHeight / ASM::TILE_SIZE;

	for (int i = 0; i < gridHeight; ++i)
	{
		for (int j = 0; j < gridWidth; ++j)
		{
			conf_new(ASMTileCacheEntry, this, j * ASM::TILE_SIZE, i * ASM::TILE_SIZE);
		}
	}
}

ASMTileCache::~ASMTileCache()
{
	for (size_t i = m_tiles.size(); i > 0; --i)
	{
		size_t newI = i - 1;
		ASMTileCacheEntry* curEntry = m_tiles[newI];
		if (curEntry->m_pOwner)
		{
			//continue;
		}

		conf_delete(m_tiles[i - 1]);
	}
	
}


void ASMTileCache::Load(const ASM::RenderTargets& renderTargets)
{
	m_depthAtlas = renderTargets.m_pASMDepthAtlas;
	m_demAtlas = renderTargets.m_pASMDEMAtlas;
}


void ASMFrustum::AllocateTiles(ASMTileCache* pCache, QuadTreeNode* pNode)
{
	for (int i = 0; i < 4; ++i)
	{
		if (pNode->mChildren[i])
		{
			AllocateTiles(pCache, pNode->mChildren[i]);
		}
	}

	if (!pNode->m_pTile)
	{
		pCache->Allocate<&QuadTreeNode::GetTile, false>(pNode, this);
	}
}


template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
ASMTileCacheEntry* ASMTileCache::Allocate(QuadTreeNode* pNode, ASMFrustum* pFrustum)
{
	ASMTileCacheEntry* pTileToAlloc = NULL;

	if (m_freeTiles.empty())
	{
		unsigned char minRefinement = pNode->mRefinement;
		float minDistSq = ASM::GetRefinementDistanceSq(pNode->mBBox, pFrustum->m_refinementPoint);


		// try to free visually less important tile (the one further from viewer or deeper in hierarchy)
		for (unsigned int i = 0; i < m_tiles.size(); ++i)
		{
		
			ASMTileCacheEntry* pTile = m_tiles[i];

			if (pTile->m_refinement < minRefinement)
			{
				continue;
			}

			float distSq = ASM::GetRefinementDistanceSq(pTile->m_BBox, 
				pTile->m_pFrustum->m_refinementPoint);

			if (pTile->m_refinement == minRefinement)
			{
				if ((distSq == minDistSq && !pTile->m_isLayer) || distSq < minDistSq)
				{
					continue;
				}
			}

			pTileToAlloc = pTile;
			minRefinement = pTile->m_refinement;
			minDistSq = distSq;
		}

		if (!pTileToAlloc)
		{
			return NULL;
		}
		pTileToAlloc->Free();
	}

	for (unsigned int i = 0; i < m_freeTiles.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_freeTiles[i];

		if (pTile->m_frustumID == pFrustum->m_ID &&
			!(pTile->m_BBox != pNode->mBBox) &&
			pTile->m_isLayer == isLayer)
		{
			pTileToAlloc = pTile;
			++m_cacheHits;
			break;
		}
	}

	if (!pTileToAlloc)
	{
		unsigned char refinement = 0;
		unsigned int LRUdt = 0;

		for (unsigned int i = 0; i < m_freeTiles.size(); ++i)
		{
			ASMTileCacheEntry* pTile = m_freeTiles[i];
			if (pTile->m_refinement < refinement)
			{
				continue;
			}  
			unsigned int dt = ASM::s_frameCounter - pTile->m_lastFrameUsed;
			if (pTile->m_refinement == refinement && dt < LRUdt)
			{
				continue;
			}
			pTileToAlloc = pTile;
			refinement = pTile->m_refinement;
			LRUdt = dt;
		}

		if (pTileToAlloc)
		{
			pTileToAlloc->Invalidate();
		}
	}

	if (pTileToAlloc)
	{
		pTileToAlloc->Allocate<TileAccessor, isLayer>(pNode, pFrustum);
		++m_tileAllocs;
	}
	return pTileToAlloc;
}

void ASMTileCache::Tick(float deltaTime)
{
	for (int i = 0; i < m_readyTiles.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_readyTiles[i];
		pTile->m_fadeInFactor = max(0.0f, pTile->m_fadeInFactor - deltaTime);
	}
}


/////////////////////////////////////////////////////////////

ASMTileCacheEntry::ASMTileCacheEntry(ASMTileCache* pCache, int x, int y)
	:m_pCache(pCache),
	m_pOwner(NULL),
	m_pFrustum(NULL),
	m_lastFrameUsed(0),
	m_frustumID(0),
	m_isLayer(false),
	m_fadeInFactor(0.f)
{
	m_viewport.x = x + ASM::TILE_BORDER_TEXELS;
	m_viewport.w = ASM::BORDERLESS_TILE_SIZE;
	m_viewport.y = y + ASM::TILE_BORDER_TEXELS;
	m_viewport.h = ASM::BORDERLESS_TILE_SIZE;

	Invalidate();

	m_pCache->m_tiles.Add(this);
	m_pCache->m_freeTiles.Add(this);
}

ASMTileCacheEntry::~ASMTileCacheEntry()
{
	if (IsAllocated())
	{
		Free();
	}

	m_pCache->m_tiles.Remove(this);
	m_pCache->m_freeTiles.Remove(this);
}


void ASMTileCacheEntry::Invalidate()
{
	m_BBox = AABBox();
	m_refinement = ASM::MAX_REFINEMENT;
	m_lastFrameUsed = ASM::s_frameCounter - 0x7fFFffFF;
	m_frustumID = 0;
}

template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
void ASMTileCacheEntry::Allocate(QuadTreeNode* pOwner, ASMFrustum* pFrustum)
{
	m_pCache->m_freeTiles.Remove(this);
	m_pOwner = pOwner;
	m_pFrustum = pFrustum;
	m_refinement = pOwner->mRefinement;

	(pOwner->*TileAccessor)() = this;

	if (m_frustumID == pFrustum->m_ID && !(m_BBox != pOwner->mBBox) && m_isLayer == isLayer)
	{
		MarkReady();
	}
	else
	{
		m_frustumID = pFrustum->m_ID;
		m_BBox = pOwner->mBBox;
		m_isLayer = isLayer;
		m_pCache->m_renderQueue.Add(this);
	}

}


void ASMTileCacheEntry::Free()
{
	 if( m_renderQueuePos.IsInserted() || 
        m_renderBatchPos.IsInserted() || 
        m_updateQueuePos.IsInserted() || 
        m_demQueuePos.IsInserted() )
    {
        m_pCache->m_renderQueue.Remove( this, true );
        m_pCache->m_renderBatch.Remove( this, true );
        m_pCache->m_updateQueue.Remove( this, true );
        m_pCache->m_demQueue.Remove( this, true );
        m_pCache->m_readyTiles.Remove( this, true );
        Invalidate();
    }
    else
    {
        MarkNotReady();
        m_lastFrameUsed = ASM::s_frameCounter;
    }
    m_pCache->m_freeTiles.Add( this );
    ( m_isLayer ? m_pOwner->m_pLayerTile : m_pOwner->m_pTile ) = nullptr;
    m_pOwner = NULL;
    m_pFrustum = NULL;
}


void ASMTileCacheEntry::MarkReady()
{
	
	m_pCache->m_readyTiles.Add(this);
	m_fadeInFactor = 0.5f;
}

void ASMTileCacheEntry::MarkNotReady()
{
	m_pCache->m_readyTiles.Remove(this);
}



void ASMTileCacheEntry::PrepareRender(const ASM::SShadowMapPrepareRenderContext& context)
{
	m_renderCamera = m_pFrustum->CalcCamera(m_BBox, *context.m_worldCenter, 
		vec2( (static_cast<float> (ASM::TILE_SIZE) / static_cast<float>(m_viewport.w)) ) 
	);

}


///////////////////////////////////////


void ASMTileCache::StartDEM(ASMTileCacheEntry* pTile, AtlasQuads::SCopyQuad& copyDEMQuad)
{
	m_demQueue.Add(pTile, true);
	
	int demAtlasX = (pTile->m_viewport.x - ASM::TILE_BORDER_TEXELS) >> ASM::DEM_DOWNSAMPLE_LEVEL;
	int demAtlasY = (pTile->m_viewport.y - ASM::TILE_BORDER_TEXELS) >> ASM::DEM_DOWNSAMPLE_LEVEL;

	copyDEMQuad = AtlasQuads::SCopyQuad::Get(
		vec4(0.f),
		ASM::DEM_TILE_SIZE, ASM::DEM_TILE_SIZE,
		demAtlasX, demAtlasY,
		m_demAtlasWidth, m_demAtlasHeight,
		ASM::TILE_SIZE, ASM::TILE_SIZE,
		pTile->m_viewport.x - ASM::TILE_BORDER_TEXELS,
		pTile->m_viewport.y - ASM::TILE_BORDER_TEXELS,
		m_depthAtlasWidth, m_depthAtlasHeight
	);
}

static float CalcDepthBias(
	const mat4& orthoProjMat,
	const vec3& kernelSize,
	int viewportWidth,
	int viewportHeight,
	int depthBitsPerPixel)
{
	vec3 texelSizeWS(
		fabsf(2.0f / (orthoProjMat.getCol0().getX() * float(viewportWidth))),
		fabsf(2.0f / (orthoProjMat.getCol1().getY() * float(viewportHeight))),
		fabsf(1.0f / (orthoProjMat.getCol2().getZ() * float(1 << depthBitsPerPixel))));
	vec3 kernelSizeWS = Helper::Piecewise_Prod(texelSizeWS , kernelSize);
	float kernelSizeMax = max(max(kernelSizeWS.getX(), kernelSizeWS.getY()), kernelSizeWS.getZ());
	return kernelSizeMax * fabsf(orthoProjMat.getCol2().getZ());
}

void ASMTileCache::RenderTiles(unsigned int numTiles,
	ASMTileCacheEntry** tiles,
	RenderTarget* workBufferDepth,
	RenderTarget* workBufferColor,
	ASM::SShadowMapRenderContext& context,
	bool allowDEM)
{
	if (!numTiles)
	{
		return;
	}

	RendererContext* curRendererContext = context.m_pRendererContext;

	Cmd* pCurCmd = curRendererContext->m_pCmd;
 	Renderer* pRenderer = curRendererContext->m_pRenderer;
	DescriptorBinder* pDescriptorBinder = curRendererContext->m_pDescriptorBinder;

    unsigned int workBufferWidth = workBufferDepth->mDesc.mWidth;
    unsigned int workBufferHeight = workBufferDepth->mDesc.mHeight;
    unsigned int numTilesW = workBufferWidth / ASM::TILE_SIZE;
    unsigned int numTilesH = workBufferHeight / ASM::TILE_SIZE;
    unsigned int maxTilesPerPass = numTilesW * numTilesH;

    
	//basically this code changes pixel center from DX10 to DX9 (DX9 pixel center is integer while DX10 is (0.5, 0.5)
    mat4 pixelCenterOffsetMatrix = mat4::translation(vec3(1.f / static_cast<float>(workBufferWidth), 
		-1.f / static_cast<float>(workBufferHeight), 0.f));


	AtlasQuads::SCopyQuad* copyDepthQuads = (AtlasQuads::SCopyQuad*)conf_malloc(sizeof(AtlasQuads::SCopyQuad) * (maxTilesPerPass + numTiles));
	AtlasQuads::SCopyQuad* copyDEMQuads = copyDepthQuads + maxTilesPerPass;

    float invAtlasWidth = 1.0f / float( m_depthAtlasWidth );
    float invAtlasHeight = 1.0f / float( m_depthAtlasHeight );

    unsigned int numCopyDEMQuads = 0;
    for( unsigned int i = 0; i < numTiles;)
    {
        unsigned int tilesToRender = min( maxTilesPerPass, numTiles - i );

		LoadActionsDesc loadActions = {};
		loadActions.mClearDepth = workBufferDepth->mDesc.mClearValue;
		loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
		
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;

	

		

		if (tilesToRender > 0)
		{

			ASM::TileIndirectMeshSceneRenderData& indirectRenderData =
				context.m_shadowMapRenderer->mTickData.mTileIndirectMeshSceneRenderData;


			TextureBarrier textureBarriers[] = { { 
					workBufferDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE} };
			cmdResourceBarrier(context.m_pRendererContext->m_pCmd, 0, NULL, 1, textureBarriers, false);

			BufferBarrier bufferBarriers[] = { {indirectRenderData.m_PScene->m_pIndirectPosBuffer, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER} };
			cmdResourceBarrier(pCurCmd, 1, bufferBarriers, 0, NULL, true);
			cmdFlushBarriers(pCurCmd);


			Helper::setRenderTarget(context.m_pRendererContext->m_pCmd, 0, NULL,
				workBufferDepth, &loadActions, vec2(0.f, 0.f), vec2((float)workBufferDepth->mDesc.mWidth, (float)workBufferDepth->mDesc.mHeight));
		}

        for( unsigned int j = 0; j < tilesToRender; ++j )
        {
            ASMTileCacheEntry* pTile = tiles[ i + j ];

            //_ASSERT( !pTile->m_isLayer || pTile->m_pOwner->m_pTile->IsReady() );

			vec2 viewPortLoc( static_cast<float>( (j % numTilesW) * ASM::TILE_SIZE ),
				static_cast<float>( (j / numTilesW) * ASM::TILE_SIZE) );
			vec2 viewPortSize(ASM::TILE_SIZE);
					   
            Camera renderCamera;
            renderCamera.SetViewMatrix( pTile->m_renderCamera.GetViewMatrix() );
            renderCamera.SetProjection( pixelCenterOffsetMatrix * pTile->m_renderCamera.GetProjection());
			

            if( pTile->m_isLayer )
            {
               
            }
            else
            {	
				ASM::TileIndirectMeshSceneRenderData& indirectRenderData =
					context.m_shadowMapRenderer->mTickData.mTileIndirectMeshSceneRenderData;

				context.m_shadowMapRenderer->RenderIndirectModelSceneTile(indirectRenderData.m_PScene,
					viewPortLoc, viewPortSize,
					renderCamera, false, context);				
            }
			//WARNING -1 multiplied cause reversed z buffer
			const float depthBias = 
				-1.0f * CalcDepthBias(pTile->m_renderCamera.GetProjection(),
				vec3(3.5f, 3.5f, 1.0f),
				ASM::TILE_SIZE,
				ASM::TILE_SIZE,
				16);
            copyDepthQuads[ j ] = AtlasQuads::SCopyQuad::Get(
                vec4( 0, 0, depthBias, 0 ),
                ASM::TILE_SIZE, ASM::TILE_SIZE,
                pTile->m_viewport.x - ASM::TILE_BORDER_TEXELS,
                pTile->m_viewport.y - ASM::TILE_BORDER_TEXELS,
                m_depthAtlasWidth, m_depthAtlasHeight,
                ASM::TILE_SIZE, ASM::TILE_SIZE,
                static_cast<unsigned int>(viewPortLoc.getX()), static_cast<unsigned int>(viewPortLoc.getY()),
                workBufferWidth, workBufferHeight );

           bool generateDEM = pTile->m_refinement <= pTile->m_pFrustum->GetDEMMinRefinement( pTile->m_isLayer );
		   if (generateDEM && (allowDEM || pTile->IsReady()))
		   {
			   StartDEM(pTile, copyDEMQuads[numCopyDEMQuads++]);
		   }
        }


		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier copyDepthBarrier[] = { 
			{ m_depthAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ workBufferDepth->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
		};

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, copyDepthBarrier, false);

		LoadActionsDesc copyDepthQuadLoadAction = {};
		copyDepthQuadLoadAction.mClearColorValues[0] = m_depthAtlas->mDesc.mClearValue;
		copyDepthQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;

#ifdef _DURANGO
		if (mDepthFirstTimeRender)
		{
			copyDepthQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			mDepthFirstTimeRender = false;
		}
#endif

		copyDepthQuadLoadAction.mClearDepth = m_depthAtlas->mDesc.mClearValue;
		copyDepthQuadLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		copyDepthQuadLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		Helper::setRenderTarget(pCurCmd, 1, &m_depthAtlas, NULL,
			&copyDepthQuadLoadAction, vec2(0.f, 0.f),
			vec2((float)m_depthAtlas->mDesc.mWidth, (float)m_depthAtlas->mDesc.mHeight));



		ASM::TickData& asmTickData = context.m_shadowMapRenderer->mTickData;
		ASM::CopyDepthAtlasRenderData& copyDepthAtlasRenderData = asmTickData.mCopyDepthRenderData;




		ASMAtlasQuadsUniform asmAtlasQuadsData = {};

		//WARNING: only using one buffer, but there is a possibility of multiple tile copying
		//this code won't work if tilesToRender exceeded more than one
		//however based on sample code tilesToRender never exceed 1, this assumption may be wrong
		//or they maybe correct if we follow exactly as the sample code settings
		for (unsigned int i = 0; i < tilesToRender; ++i)
		{
			const AtlasQuads::SCopyQuad& quad = copyDepthQuads[i];
			
			asmAtlasQuadsData.mMiscData = quad.m_misc;
			asmAtlasQuadsData.mPosData = quad.m_pos;
			asmAtlasQuadsData.mTexCoordData = quad.m_texCoord;

			BufferUpdateDesc updateUbDesc = { 
				copyDepthAtlasRenderData.pBufferASMAtlasQuadsUniform,
				&asmAtlasQuadsData
			};
			updateResource(&updateUbDesc);
		}

		cmdBindPipeline(pCurCmd, copyDepthAtlasRenderData.m_pGraphicsPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "DepthPassTexture";
		params[0].ppTextures = &workBufferDepth->pTexture;
		params[1].pName = "AtlasQuads_CB";
		params[1].ppBuffers = &copyDepthAtlasRenderData.pBufferASMAtlasQuadsUniform;

		cmdBindDescriptors(pCurCmd, curRendererContext->m_pDescriptorBinder,
			copyDepthAtlasRenderData.m_pRootSignature, 2, params);
		
		cmdDraw(pCurCmd, 6, 0);

		i += tilesToRender;

		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
    }

    if( numCopyDEMQuads > 0 )
    {

		TextureBarrier asmCopyDEMBarrier[] = { {  m_demAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET},
		{m_depthAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE} };

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmCopyDEMBarrier, false);

		LoadActionsDesc copyDEMQuadLoadAction = {};
		copyDEMQuadLoadAction.mClearColorValues[0] = m_depthAtlas->mDesc.mClearValue;
		copyDEMQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		copyDEMQuadLoadAction.mClearDepth = m_depthAtlas->mDesc.mClearValue;
		copyDEMQuadLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		copyDEMQuadLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		Helper::setRenderTarget(pCurCmd, 1, &m_demAtlas, NULL, 
			&copyDEMQuadLoadAction, vec2(0.f, 0.f), vec2((float) m_demAtlas->mDesc.mWidth, 
				(float)m_demAtlas->mDesc.mHeight));

		ASM::TickData& tickData = context.m_shadowMapRenderer->mTickData;
		ASM::CopyDEMAtlasRenderData& copyDEMAtlasRenderData = tickData.mCopyDEMRenderData;

		BufferUpdateDesc copyDEMQuadUpdateUbDesc = {};
		copyDEMQuadUpdateUbDesc.pBuffer = copyDEMAtlasRenderData.pBufferASMCopyDEMPackedQuadsUniform;
		copyDEMQuadUpdateUbDesc.pData = &copyDEMQuads[0];
		copyDEMQuadUpdateUbDesc.mSize = sizeof(AtlasQuads::SCopyQuad) * numCopyDEMQuads;

		updateResource(&copyDEMQuadUpdateUbDesc);
		
		cmdBindPipeline(pCurCmd, copyDEMAtlasRenderData.m_pGraphicsPipeline);

		DescriptorData params[2] = {};
		params[0].pName = "DepthPassTexture";
		params[0].ppTextures = &m_depthAtlas->pTexture;
		params[1].pName = "PackedAtlasQuads_CB";
		params[1].ppBuffers = &copyDEMAtlasRenderData.pBufferASMCopyDEMPackedQuadsUniform;

		cmdBindDescriptors(pCurCmd, curRendererContext->m_pDescriptorBinder,
			copyDEMAtlasRenderData.m_pRootSignature, 2, params);

		cmdDraw(pCurCmd, numCopyDEMQuads * 6, 0);

		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		
    }
	conf_free(copyDepthQuads);
}


template<typename T>
int ASMTileCache::AddTileToRenderBatch(
	T& tilesQueue,
	ASMFrustum* pFrustum,
	int maxRefinement,
	bool isLayer)
{
	if (!pFrustum->IsValid())
	{
		return -1;
	}

	ASMTileCacheEntry* pTileToRender = nullptr;
	float minDistSq = FLT_MAX;
	unsigned char refinement = UCHAR_MAX;
	for (unsigned int i = 0; i < tilesQueue.size(); ++i)
	{
		ASMTileCacheEntry* pTile = tilesQueue[i];
		if (pFrustum == pTile->m_pFrustum && isLayer == pTile->m_isLayer &&
			(!pTile->m_isLayer || pTile->m_pOwner->m_pTile->IsReady()))
		{
			float distSq = ASM::GetRefinementDistanceSq(pTile->m_BBox, pFrustum->m_refinementPoint);
			if (pTile->m_refinement < refinement ||
				(refinement == pTile->m_refinement && distSq < minDistSq))
			{
				refinement = pTile->m_refinement;
				minDistSq = distSq;
				pTileToRender = pTile;
			}
		}
	}

	if (pTileToRender == nullptr ||
		pTileToRender->m_refinement > maxRefinement)
	{
		return -1;
	}

	tilesQueue.Remove(pTileToRender);
	m_renderBatch.Add(pTileToRender);
	return pTileToRender->m_refinement;
}

int ASMTileCache::AddTileFromRenderQueueToRenderBatch(
	ASMFrustum* pFrustum,
	int maxRefinement,
	bool isLayer)
{
	return AddTileToRenderBatch(
		m_renderQueue,
		pFrustum,
		maxRefinement,
		isLayer);
}

int ASMTileCache::AddTileFromUpdateQueueToRenderBatch(
	ASMFrustum* pFrustum,
	int maxRefinement,
	bool isLayer)
{
	return AddTileToRenderBatch(
		m_updateQueue,
		pFrustum,
		maxRefinement,
		isLayer);
}



bool ASMTileCache::PrepareRenderTilesBatch(const ASM::SShadowMapPrepareRenderContext& context)
{
	//LOGF(LogLevel::eWARNING, "Prepare render size: %d", m_renderBatch.size());
	for (int i = 0; i < m_renderBatch.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_renderBatch[i];
		pTile->PrepareRender(context);
	}
	return !m_renderBatch.empty();
}

const Camera* ASMTileCache::GetFirstRenderBatchCamera() const
{
	if (m_renderBatch.empty())
	{
		return NULL;
	}
	return &m_renderBatch[0]->m_renderCamera;
}

void ASMTileCache::RenderTilesBatch(
		RenderTarget* workBufferDepth,
		RenderTarget* workBufferColor,
		ASM::SShadowMapRenderContext& context
	)
{
	if (!m_renderBatch.empty())
	{
		RenderTiles( static_cast<unsigned int>( m_renderBatch.size()),
			&m_renderBatch[0], workBufferDepth, 
			workBufferColor,  context, true);
	}

	for (size_t i = m_renderBatch.size(); i > 0; --i)
	{
		ASMTileCacheEntry* pTile = m_renderBatch[i - 1];
		m_renderBatch.Remove(pTile);

		if (!pTile->IsReady())
		{
			pTile->MarkReady();
			++m_numTilesRendered;
		}
		else
		{
			++m_numTilesRendered;
		}
	}
}


void ASMTileCache::CreateDEM(
	RenderTarget* demWorkBufferColor,
	const ASM::ASM::SShadowMapRenderContext& context,
	bool createDemForLayerRendering)
{

	if (m_demQueue.empty())
	{
		return;
	}

	RendererContext* rendererContext = context.m_pRendererContext;
	Cmd* pCurCmd = rendererContext->m_pCmd;

    unsigned int workBufferWidth = demWorkBufferColor->mDesc.mWidth;
    unsigned int workBufferHeight = demWorkBufferColor->mDesc.mHeight;
    unsigned int numTilesW = workBufferWidth / ASM::DEM_TILE_SIZE;
    unsigned int numTilesH = workBufferHeight / ASM::DEM_TILE_SIZE;
    unsigned int maxTilesPerPass = numTilesW * numTilesH;


	AtlasQuads::SCopyQuad* atlasToBulkQuads = (AtlasQuads::SCopyQuad*) conf_malloc(
		(sizeof(AtlasQuads::SCopyQuad) * 2 + sizeof(ASMTileCacheEntry*)) * maxTilesPerPass);

    AtlasQuads::SCopyQuad* bulkToAtlasQuads = atlasToBulkQuads + maxTilesPerPass;
    ASMTileCacheEntry** tilesToUpdate = reinterpret_cast< ASMTileCacheEntry** >( bulkToAtlasQuads + maxTilesPerPass );


    while(true)
    {
        unsigned int numTiles = 0; 
        for( int i = 0; i < m_demQueue.size() && numTiles < maxTilesPerPass; ++i )
        {
            ASMTileCacheEntry* pTile = m_demQueue[i];

            bool isDemForLayerRendering = pTile->m_refinement > 0 && !pTile->m_isLayer;
            if( isDemForLayerRendering == createDemForLayerRendering )
            {
                static const unsigned int rectSize = ASM::DEM_TILE_SIZE - 2;

                unsigned int workX = ( numTiles % numTilesW ) * ASM::DEM_TILE_SIZE;
                unsigned int workY = ( numTiles / numTilesW ) * ASM::DEM_TILE_SIZE;

                unsigned int atlasX = ( ( pTile->m_viewport.x - ASM::TILE_BORDER_TEXELS ) >> ASM::DEM_DOWNSAMPLE_LEVEL ) + 1;
                unsigned int atlasY = ( ( pTile->m_viewport.y - ASM::TILE_BORDER_TEXELS ) >> ASM::DEM_DOWNSAMPLE_LEVEL ) + 1;

                if( createDemForLayerRendering )
                {
                    const int NUp = pTile->m_refinement;
                    QuadTreeNode* NList[ ASM::MAX_REFINEMENT + 1 ] = { pTile->m_pOwner };
                    ivec4 parentRect, src;
                    bool isTileAttached = ASM::GetRectangleWithinParent( NUp, NList, parentRect, src );
                    //_ASSERT( isTileAttached );

					//pTile->m_renderCamera.GetViewProjection().e43
                    float depthOffset = pTile->m_renderCamera.GetViewProjection().getCol3().getZ() - 
						NList[ NUp ]->m_pTile->m_renderCamera.GetViewProjection().getCol3().getZ();

					//float depthOffset = pTile->m_renderCamera.GetViewProjection().getCol2().getW() -
						//NList[NUp]->m_pTile->m_renderCamera.GetViewProjection().getCol2().getW();

                    atlasToBulkQuads[ numTiles ] = AtlasQuads::SCopyQuad::Get(
                        vec4( 1.0f/ float( m_demAtlasWidth ), 1.0f/ float( m_demAtlasHeight ), depthOffset, 0.0f ),
                        ASM::DEM_TILE_SIZE, ASM::DEM_TILE_SIZE, workX, workY, workBufferWidth, workBufferHeight,
                        ( src.getZ() - src.getX() ) >> ASM::DEM_DOWNSAMPLE_LEVEL,
                        ( src.getW() - src.getY() ) >> ASM::DEM_DOWNSAMPLE_LEVEL,
                        src.getX() >> ASM::DEM_DOWNSAMPLE_LEVEL,
                        src.getY() >> ASM::DEM_DOWNSAMPLE_LEVEL,
                        m_demAtlasWidth, m_demAtlasHeight );
                }
                else
                {
                    atlasToBulkQuads[ numTiles ] = AtlasQuads::SCopyQuad::Get(
                        vec4( 1.0f/ float( m_demAtlasWidth ), 1.0f/ float( m_demAtlasHeight ), 0.0f, 0.0f ),
                        rectSize, rectSize, workX + 1, workY + 1, workBufferWidth, workBufferHeight,
                        rectSize, rectSize, atlasX, atlasY, m_demAtlasWidth, m_demAtlasHeight );
					
					float zTest = atlasToBulkQuads[numTiles].m_misc.getZ();

					if (zTest != 0.f)
					{
						int i = 0;
					}
                }

                bulkToAtlasQuads[ numTiles ] = AtlasQuads::SCopyQuad::Get(
                    vec4( 1.0f/ float( workBufferWidth ), 1.0f/ float( workBufferHeight ), 0.0f, 0.0f ),
                    rectSize, rectSize, atlasX, atlasY, m_demAtlasWidth, m_demAtlasHeight,
                    rectSize, rectSize, workX + 1, workY + 1, workBufferWidth, workBufferHeight );

                tilesToUpdate[ numTiles++ ] = pTile;
            }
        }

		if (numTiles == 0)
		{
			break;
		}

#ifdef _DURANGO
		//On Xbox somehow the texture is initialized with garbage value, so texture that isn't cleared every frame
		//needs to be cleared at the begininng for xbox.
		if (mDEMFirstTimeRender)
		{
			TextureBarrier demRenderTargetBarrier[] = { {m_demAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET} };
			cmdResourceBarrier(pCurCmd, 0, NULL, 1, demRenderTargetBarrier, false);


			LoadActionsDesc clearDEMLoadActions = {};
			clearDEMLoadActions.mClearColorValues[0] = m_demAtlas->mDesc.mClearValue;
			clearDEMLoadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			clearDEMLoadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			clearDEMLoadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

			Helper::setRenderTarget(pCurCmd, 1, &m_demAtlas, NULL, &clearDEMLoadActions, 
				vec2(0.f, 0.f), vec2((float)m_demAtlas->pTexture->mDesc.mWidth, (float)m_demAtlas->pTexture->mDesc.mHeight));

			Helper::setRenderTarget(pCurCmd, 0, NULL, NULL, NULL, vec2(0.f), vec2(0.f));
			mDEMFirstTimeRender = false;
		}
#endif

		cmdBeginGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, "DEM Atlas To Color", true);
		
		TextureBarrier asmAtlasToColorBarrier[] = { 
			{  demWorkBufferColor->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ m_demAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE} };

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmAtlasToColorBarrier, false);

		LoadActionsDesc atlasToColorLoadAction = {};
		atlasToColorLoadAction.mClearColorValues[0] = demWorkBufferColor->mDesc.mClearValue;
		atlasToColorLoadAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		//atlasToColorLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		atlasToColorLoadAction.mClearDepth = m_depthAtlas->mDesc.mClearValue;
		atlasToColorLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;
		atlasToColorLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		Helper::setRenderTarget(pCurCmd, 1, &demWorkBufferColor, NULL,
			&atlasToColorLoadAction, vec2(0.f, 0.f), 
			vec2((float)demWorkBufferColor->mDesc.mWidth,
			(float)demWorkBufferColor->mDesc.mHeight));

		ASM::GenerateDEMAtlasToColorRenderData& atlasToColorRenderData =
			context.m_shadowMapRenderer->mTickData.mDEMAtlasToColorRenderData;

		cmdBindPipeline(pCurCmd, atlasToColorRenderData.m_pGraphicsPipeline);
		
		BufferUpdateDesc atlasToColorUpdateDesc = {};
		atlasToColorUpdateDesc.pBuffer = atlasToColorRenderData.
			pBufferASMAtlasToColorPackedQuadsUniform;
		atlasToColorUpdateDesc.pData = &atlasToBulkQuads[0];
		atlasToColorUpdateDesc.mSize = sizeof(vec4) * 3 * numTiles;
		updateResource(&atlasToColorUpdateDesc);


		DescriptorData params[2] = {};
		params[0].pName = "DepthPassTexture";
		params[0].ppTextures = &m_demAtlas->pTexture;
		params[1].pName = "PackedAtlasQuads_CB";
		params[1].ppBuffers = &atlasToColorRenderData.pBufferASMAtlasToColorPackedQuadsUniform;


		cmdBindDescriptors(pCurCmd, 
			rendererContext->m_pDescriptorBinder, 
			atlasToColorRenderData.m_pRootSignature, 2, params);

		cmdDraw(pCurCmd, numTiles * 6, 0);


		cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, NULL);
		
		cmdBeginGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, "DEM Color To Atlas", true);

		LoadActionsDesc colorToAtlasLoadAction = {};
		colorToAtlasLoadAction.mClearColorValues[0] = m_demAtlas->mDesc.mClearValue;
		colorToAtlasLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		colorToAtlasLoadAction.mClearDepth = m_demAtlas->mDesc.mClearValue;
		colorToAtlasLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;
		colorToAtlasLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;


		TextureBarrier asmColorToAtlasBarriers[] = {
			{  demWorkBufferColor->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			{ m_demAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET} };

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmColorToAtlasBarriers, false);

		Helper::setRenderTarget(pCurCmd, 1, &m_demAtlas, NULL,
			&colorToAtlasLoadAction, vec2(0.f, 0.f), vec2((float)m_demAtlas->mDesc.mWidth,
			(float)m_demAtlas->mDesc.mHeight));

		ASM::GenerateDEMColorToAtlasRenderData& colorToAtlasRenderData =
			context.m_shadowMapRenderer->mTickData.mDEMColorToAtlasRenderData;

		cmdBindPipeline(pCurCmd, 
			colorToAtlasRenderData.m_pGraphicsPipeline);

		BufferUpdateDesc colorToAtlasBufferUbDesc = {};
		colorToAtlasBufferUbDesc.pBuffer = 
			colorToAtlasRenderData.pBufferASMColorToAtlasPackedQuadsUniform;
		colorToAtlasBufferUbDesc.pData = &bulkToAtlasQuads[0];
		colorToAtlasBufferUbDesc.mSize = numTiles * sizeof(vec4) * 3;
		updateResource(&colorToAtlasBufferUbDesc);

		DescriptorData colorToAtlasParams[2] = {};
		colorToAtlasParams[0].pName = "DepthPassTexture";
		colorToAtlasParams[0].ppTextures = &demWorkBufferColor->pTexture;
		colorToAtlasParams[1].pName = "PackedAtlasQuads_CB";
		colorToAtlasParams[1].ppBuffers = &colorToAtlasRenderData.
			pBufferASMColorToAtlasPackedQuadsUniform;

		cmdBindDescriptors(pCurCmd, rendererContext->m_pDescriptorBinder, 
			colorToAtlasRenderData.m_pRootSignature, 2, colorToAtlasParams);

		cmdDraw(pCurCmd, numTiles * 6, 0);

		for (unsigned int i = 0; i < numTiles; ++i)
		{
			m_demQueue.Remove(tilesToUpdate[i]);
		}

		cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, NULL);
    }

	conf_free(atlasToBulkQuads);
}


bool ASMTileCache::IsFadeInFinished(const ASMFrustum* pFrustum) const
{
	for (int i = 0; i < m_readyTiles.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_readyTiles[i];
		if (pTile->m_frustumID == pFrustum->m_ID && pTile->m_fadeInFactor > 0)
		{
			return false;
		}
	}
	return true;
}