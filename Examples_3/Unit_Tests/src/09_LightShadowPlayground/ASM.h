#ifndef ASM_H
#define ASM_H





//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"


#include "Camera.h"

#include "Constant.h"
#include "Geometry.h"
#include "AABBox.h"

class ASMFrustum;
class ASMTileCache;
class ASMTileCacheEntry;
class ASMQuadTree;
class QuadTreeNode;

struct ObjectInfoUniformBlock;
struct GpuProfiler;


struct RendererContext
{
	Renderer* m_pRenderer;
	Cmd* m_pCmd;
	DescriptorBinder* m_pDescriptorBinder;
	GpuProfiler* m_pGpuProfiler;
};



class ASM
{
public:
	ASMTileCache* m_cache;
	ASMFrustum* m_longRangeShadows;
	ASMFrustum* m_longRangePreRender;
public:
	static unsigned int s_frameCounter;
	static float s_tileFarPlane;
	friend class ASMTileCacheEntry;
	friend class ASMTileCache;
	friend class ASMFrustum;
	

	struct RenderTargets
	{
		RenderTarget* m_pASMDepthAtlas;
		RenderTarget* m_pASMDEMAtlas;

		RenderTarget* m_pRenderTargetASMLodClamp;
		RenderTarget* m_pRenderTargetASMPrerenderLodClamp;

		eastl::vector<RenderTarget*> m_pASMPrerenderIndirectionMips;
		eastl::vector<RenderTarget*> m_pASMIndirectionMips;
	};

	struct TileIndirectMeshSceneRenderData
	{
		RootSignature* m_pRootSignature;

		Pipeline* m_pGraphicsPipeline[2];
		Buffer* m_pIndirectMaterialBuffer;
		Buffer* m_pIndirectDrawArgsBuffer[2];
		Buffer* m_pIndirectIndexBuffer;

		uint32_t mDrawCount[2];

		CommandSignature* m_pCmdSignatureVBPass;
		Scene* m_PScene;
		Buffer* m_pBufferObjectInfoUniform;
		//TODO:
		//MeshInfoUniformBlock* m_pObjectInfoUniformBlock;
		MeshInfoUniformBlock mObjectInfoUniformBlock;

		eastl::vector<Texture*>* mDiffuseMaps;
	};
	
	struct CopyDepthAtlasRenderData
	{
		Buffer* pBufferASMAtlasQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;
	};

	struct CopyDEMAtlasRenderData
	{
		Buffer* pBufferASMCopyDEMPackedQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;
	};

	struct GenerateDEMAtlasToColorRenderData
	{
		Buffer* pBufferASMAtlasToColorPackedQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;
	};

	struct GenerateDEMColorToAtlasRenderData
	{
		Buffer* pBufferASMColorToAtlasPackedQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;
	};

	struct GenerateLodClampRenderData
	{
		Buffer* pBufferLodClampPackedQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;
	};


	struct IndirectionRenderData
	{
		Buffer* pBufferASMPackedIndirectionQuadsUniform[gs_ASMMaxRefinement + 1];
		Buffer* pBufferASMClearIndirectionQuadsUniform;
		Pipeline* m_pGraphicsPipeline;
		RootSignature* m_pRootSignature;


		IndirectionRenderData& operator=(const IndirectionRenderData& right)
		{
			for (int i = 0; i < gs_ASMMaxRefinement + 1; ++i)
			{
				this->pBufferASMPackedIndirectionQuadsUniform[i] = 
					right.pBufferASMPackedIndirectionQuadsUniform[i];
			}
			this->pBufferASMClearIndirectionQuadsUniform = 
				right.pBufferASMClearIndirectionQuadsUniform;
			
			this->m_pGraphicsPipeline = right.m_pGraphicsPipeline;
			this->m_pRootSignature = right.m_pRootSignature;

			return *this;
		}
	};


	struct TickData
	{
		TileIndirectMeshSceneRenderData mTileIndirectMeshSceneRenderData;
		
		CopyDepthAtlasRenderData mCopyDepthRenderData;
		CopyDEMAtlasRenderData mCopyDEMRenderData;
		IndirectionRenderData mIndirectionRenderData;
		IndirectionRenderData mPrerenderIndirectionRenderData;
		GenerateDEMAtlasToColorRenderData mDEMAtlasToColorRenderData;
		GenerateDEMColorToAtlasRenderData mDEMColorToAtlasRenderData;
		GenerateLodClampRenderData mGenerateLodClampRenderData;
	};


	
	friend class ASMFrustum;
	friend class ASMTileCacheEntry;

	static const unsigned int MAX_REFINEMENT = gs_ASMMaxRefinement;
	static const unsigned int TILE_BORDER_TEXELS = gs_ASMTileBorderTexels;
	static const unsigned int TILE_SIZE = gs_ASMTileSize;
	static const unsigned int DEM_DOWNSAMPLE_LEVEL = gs_ASMDEMDownsampleLevel;
	static const unsigned int DEM_TILE_SIZE = gs_ASMDEMTileSize;
	static const unsigned int BORDERLESS_TILE_SIZE = gs_ASMBorderlessTileSize;

	struct SShadowMapPrepareRenderContext
	{
		const vec3* m_worldCenter;
	};

	struct SShadowMapRenderContext
	{
		RendererContext* m_pRendererContext;
		//DeviceContext11* m_dc;
		ASM* m_shadowMapRenderer;
		Camera* m_pMainViewCamera;
	};
public:
	ASM();
	~ASM();


	void Load(const RenderTargets& renderTargets);

	bool PrepareRender(
		const Camera& mainViewCamera,
		bool disablePreRender);

	void Render(
		RenderTarget* pRenderTargetDepth,
		RenderTarget* pRenderTargetColor,
		RendererContext& renderContext,
		Camera* mainViewCamera);

	void Reset();

	void Update_Tick_Data(const TickData& tickData);

	void Tick(const ASMCpuSettings& asmCpuSettings, 
		ICameraController* lightCameraController, const vec3& lightDir,
		const vec3& halfwayLightDir, unsigned int currentTime, unsigned int dt, 
		bool disableWarping, bool forceUpdate, unsigned int updateDeltaTime);

	const vec3& GetLightDir() const;
	const vec3& GetPreRenderLightDir() const;

	bool NothingToRender() const;
	bool PreRenderAvailable() const;

	/*void RenderModelSceneTile(
		
		const vec2& viewPortLoc,
		const vec2& viewPortSize,
		const Camera& renderCamera,
		bool isLayer,
		SShadowMapRenderContext& renderContext);*/

	void RenderIndirectModelSceneTile(
		Scene* pScene,
		const vec2& viewPortLoc,
		const vec2& viewPortSize,
		const Camera& renderCamera,
		bool isLayer,
		SShadowMapRenderContext& renderContext);

	void RenderTile(
		const vec2& viewPortLoc,
		const vec2& viewPortSize,
		const Camera& renderCamera,
		bool isLayer,
		SShadowMapRenderContext& renderContext,
		uint32 verticesCount);

	static float GetRefinementDistanceSq(const AABBox& BBox, const vec2& refinementPos);
protected:
	

	

	bool m_preRenderDone;

	TickData mTickData;

	

	static bool IsTileAcceptableForIndexing(const ASMTileCacheEntry* pTile);
	static bool IsNodeAcceptableForIndexing(const QuadTreeNode* pNode);

	static void SortNodes(
		const vec2& refinementPoint,
		const vec2& sortRegionMaxSize,
		float tileSize,
		const eastl::vector<QuadTreeNode*>& nodes,
		eastl::vector<QuadTreeNode*>& sortedNodes,
		AABBox& sortedBBox);

	//template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)() const >
	static bool GetRectangleWithinParent(
		const int NUp,
		QuadTreeNode* NList[ASM::MAX_REFINEMENT + 1],
		ivec4& parentRect,
		ivec4& tileRect);

private:


};


#endif
