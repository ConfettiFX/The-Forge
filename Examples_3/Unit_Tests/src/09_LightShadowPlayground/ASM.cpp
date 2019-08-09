#include "ASM.h"
#include "ASMFrustum.h"
#include "ASMTileCache.h"
#include "ASMQuadTree.h"


#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#define SQR(a) ( (a) * (a) )


unsigned int ASM::s_frameCounter;
float ASM::s_tileFarPlane = gs_ASMTileFarPlane;

float ASM::GetRefinementDistanceSq(const AABBox& BBox, const vec2& refinementPos)
{
	return lengthSqr(Helper::Vec3_To_Vec2(BBox.GetCenter()) - refinementPos);
}




ASM::ASM()
	:m_preRenderDone(false)
{
	m_cache = conf_new(ASMTileCache);
	static const ASMFrustum::Config longRangeCfg =
	{
		gs_ASMLargestTileWorldSize, gs_ASMDistanceMax, MAX_REFINEMENT, INT_MAX, gsASMIndexSize, true,
		{ SQR(gs_ASMDistanceMax), SQR(120.0f), SQR(60.0f), SQR(30.0f), SQR(10.0f) }
	};
	m_longRangeShadows = conf_new(ASMFrustum, longRangeCfg, true, false);
	m_longRangePreRender = conf_new(ASMFrustum, longRangeCfg, true, true);
	Reset();
}


ASM::~ASM()
{
	conf_delete(m_cache);
	conf_delete(m_longRangeShadows);
	conf_delete(m_longRangePreRender);
}


void ASM::Load(const ASM::RenderTargets& renderTargets)
{
	m_cache->Load(renderTargets);
	m_longRangePreRender->Load(renderTargets, true);
	m_longRangeShadows->Load(renderTargets, false);
}

bool ASM::PrepareRender(
	const Camera& mainViewCamera,
	bool disablePreRender)
{
	m_longRangeShadows->CreateTiles(m_cache, mainViewCamera);
	m_longRangePreRender->CreateTiles(m_cache, mainViewCamera);

	if (m_cache->NothingToRender())
	{
		bool keepRendering = true;

		for (unsigned int i = 0; i <= MAX_REFINEMENT && keepRendering; ++i)
		{
			keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, i, false) < 0;

			if (keepRendering)
			{
				keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, i, true) < 0;
			}
			if (keepRendering && m_longRangePreRender->IsValid() && i == 0 && !disablePreRender)
				keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, i, false) < 0;
		}

		if (keepRendering && m_longRangePreRender->IsValid() && !disablePreRender)
		{
			
			keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, INT_MAX, false) < 0;

			if (keepRendering)
			{
				keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, INT_MAX, true) < 0;
			}
			if (keepRendering)
			{
				m_preRenderDone = true;
			}
			
		}

		if (keepRendering)
		{
			m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, INT_MAX, false);
		}
	}

	vec3 mainViewCameraPosition = mainViewCamera.GetPosition();
	SShadowMapPrepareRenderContext context = { &mainViewCameraPosition };
	return m_cache->PrepareRenderTilesBatch(context);
}




void ASM::Render(
	RenderTarget* pRenderTargetDepth,
	RenderTarget* pRenderTargetColor,
	RendererContext& renderContext,
	Camera* mainViewCamera)
{
	SShadowMapRenderContext context = { &renderContext, this, mainViewCamera  };
	
	if (!m_cache->NothingToRender())
	{
		m_cache->RenderTilesBatch(
			pRenderTargetDepth,
			pRenderTargetColor,
			context);
	}



	
	m_cache->CreateDEM(pRenderTargetColor, context, false);
	m_cache->CreateDEM(pRenderTargetColor, context, true);

	m_longRangeShadows->BuildTextures(context, false);

	if (m_longRangePreRender->IsValid())
	{
		m_longRangePreRender->BuildTextures(context, true);
	}
	
}


void ASM::Reset()
{
	m_longRangeShadows->Reset();
	m_longRangePreRender->Reset();
	m_preRenderDone = false;
}



void ASM::Update_Tick_Data(const TickData& tickData)
{
	mTickData = tickData;
}

void ASM::Tick(const ASMCpuSettings& asmCpuSettings, 
	ICameraController* lightCameraController,
	const vec3& lightDir, 
	const vec3& halfwayLightDir,
	unsigned int currentTime, 
	unsigned int dt,
	bool disableWarping, 
	bool forceUpdate, 
	unsigned int updateDeltaTime)
{
	//mTickData = tickData;

	vec3 sunDir = lightDir;

	//vec3 sunDir = GetLightDirection(currentTime);

	float deltaTime = float(dt) * 0.001f;

	bool isUpdated = false;
	if (!m_longRangeShadows->IsValid())
	{
		m_longRangeShadows->Set(lightCameraController, sunDir);
		isUpdated = true;
	}
	else if (forceUpdate)
	{
		m_longRangePreRender->Reset();

		m_longRangePreRender->Set(lightCameraController, sunDir);
		m_preRenderDone = false;

		isUpdated = true;
	}
	else if (!m_longRangePreRender->IsValid())
	{
		//vec3 nextSunDir = GetLightDirection(currentTime + (updateDeltaTime >> 1));
		//vec3 nextSunDir = lightDir;
		vec3 nextSunDir = halfwayLightDir;
		isUpdated = m_longRangeShadows->IsLightDirDifferent(nextSunDir);

		if (isUpdated)
		{
			m_longRangePreRender->Set(lightCameraController, nextSunDir);
			m_preRenderDone = false;
		}
	}
	m_longRangeShadows->UpdateWarpVector(asmCpuSettings, sunDir, disableWarping);
	m_longRangePreRender->UpdateWarpVector(asmCpuSettings, sunDir, disableWarping);

	m_cache->Tick(deltaTime);

	if (m_longRangePreRender->IsValid() && m_preRenderDone && m_cache->IsFadeInFinished(m_longRangePreRender))
	{
		eastl::swap(m_longRangeShadows, m_longRangePreRender);

		m_longRangePreRender->Reset();
		m_preRenderDone = false;
	}

	++s_frameCounter;
}

const vec3& ASM::GetLightDir() const
{
	return m_longRangeShadows->m_lightDir;
}

const vec3& ASM::GetPreRenderLightDir() const
{
	return m_longRangePreRender->m_lightDir;
}

bool ASM::NothingToRender() const
{
	return m_cache->NothingToRender();
}

bool ASM::PreRenderAvailable() const
{
	return m_longRangePreRender->IsValid();
}

bool ASM::IsTileAcceptableForIndexing(const ASMTileCacheEntry* pTile)
{
	return pTile && pTile->IsReady();
}

bool ASM::IsNodeAcceptableForIndexing(const QuadTreeNode* pNode)
{
	return IsTileAcceptableForIndexing(pNode->m_pTile);
}



void ASM::RenderIndirectModelSceneTile(
	Scene* pScene,
	const vec2& viewPortLoc,
	const vec2& viewPortSize,
	const Camera& renderCamera,
	bool isLayer,
	SShadowMapRenderContext& renderContext)
{
	Cmd* pCurCmd = renderContext.m_pRendererContext->m_pCmd;
	ASM::TileIndirectMeshSceneRenderData& indirectMeshRenderData =
		renderContext.m_shadowMapRenderer->mTickData.mTileIndirectMeshSceneRenderData;
	
	cmdBindIndexBuffer(pCurCmd, indirectMeshRenderData.m_pIndirectIndexBuffer, 0);
	

	
	cmdSetViewport(pCurCmd,
		static_cast<float>(viewPortLoc.getX()),
		static_cast<float>(viewPortLoc.getY()),
		static_cast<float>(viewPortSize.getX()),
		static_cast<float>(viewPortSize.getY()), 0.f, 1.f);
	for (int i = 0; i < MESH_COUNT; ++i)
	{
		mat4& worldMat = indirectMeshRenderData.mObjectInfoUniformBlock.mWorldMat;
		indirectMeshRenderData.mObjectInfoUniformBlock.mWorldViewProjMat = renderCamera.GetViewProjection() * worldMat;
	}

	BufferUpdateDesc updateDesc = { indirectMeshRenderData.m_pBufferObjectInfoUniform,
		&indirectMeshRenderData.mObjectInfoUniformBlock };
	updateResource(&updateDesc);

	DescriptorData depthPassParams[3] = {};
	depthPassParams[0].pName = "objectUniformBlock";
	depthPassParams[0].ppBuffers = &indirectMeshRenderData.m_pBufferObjectInfoUniform;
	depthPassParams[1].pName = "diffuseMaps";
	depthPassParams[1].mCount = (uint32_t)indirectMeshRenderData.mDiffuseMaps->size();
	depthPassParams[1].ppTextures = indirectMeshRenderData.mDiffuseMaps->data();
	depthPassParams[2].pName = "indirectMaterialBuffer";
	depthPassParams[2].ppBuffers = &indirectMeshRenderData.m_pIndirectMaterialBuffer;
	
	cmdBindVertexBuffer(pCurCmd, 1, &pScene->m_pIndirectPosBuffer, NULL);
	cmdBindPipeline(pCurCmd, indirectMeshRenderData.m_pGraphicsPipeline[0]);
	
#ifndef METAL
	cmdBindDescriptors(pCurCmd,
		renderContext.m_pRendererContext->m_pDescriptorBinder,
		indirectMeshRenderData.m_pRootSignature, 3, depthPassParams);
#else
	cmdBindDescriptors(pCurCmd,
	renderContext.m_pRendererContext->m_pDescriptorBinder,
	indirectMeshRenderData.m_pRootSignature, 1, depthPassParams);
#endif



	

	cmdExecuteIndirect(
		pCurCmd, indirectMeshRenderData.m_pCmdSignatureVBPass, 
		indirectMeshRenderData.mDrawCount[0],
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[0], 
		0, 
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[0],
		DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

	Buffer* pVertexBuffersPosTex[] = { pScene->m_pIndirectPosBuffer,
		pScene->m_pIndirectTexCoordBuffer };
	cmdBindVertexBuffer(pCurCmd, 2, pVertexBuffersPosTex, NULL);

	cmdBindPipeline(pCurCmd, indirectMeshRenderData.m_pGraphicsPipeline[1]);
	
#ifdef METAL
	cmdBindDescriptors(pCurCmd, renderContext.m_pRendererContext->m_pDescriptorBinder,
					   indirectMeshRenderData.m_pRootSignature, 3, depthPassParams);
#endif

	cmdExecuteIndirect(
		pCurCmd, indirectMeshRenderData.m_pCmdSignatureVBPass,
		indirectMeshRenderData.mDrawCount[1],
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[1],
		0,
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[1],
		DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
}
