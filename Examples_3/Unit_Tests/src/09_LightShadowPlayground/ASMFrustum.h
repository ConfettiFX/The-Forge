#pragma once
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "ConvexHull2D.h"
#include "ASMQuadTree.h"
#include "ASM.h"
#include "AtlasQuads.h"


class ASMTileCacheEntry;

class ASMFrustum
{
public:
	struct Config
	{
		float m_largestTileWorldSize;
		float m_shadowDistance;
		int m_maxRefinement;
		int m_minRefinementForLayer;
		int m_indexSize;
		bool m_forceImpostors;
		float m_refinementDistanceSq[ASM::MAX_REFINEMENT + 2];
		float m_minExtentLS;
	};

	ASMFrustum(const Config& cfg, bool useMRF, bool isPreRender);
	~ASMFrustum();



	static float GetRefinementDistanceSq(const AABBox& BBox, const vec2& refinementPos);


	bool mIsPrerender;
	unsigned int m_ID;
	vec3 m_lightDir;
	mat4 m_lightRotMat;
	mat4 m_invLightRotMat;
	vec2 m_refinementPoint;

	vec3 m_receiverWarpVector;
	vec3 m_blockerSearchVector;
	bool m_disableWarping;

	ConvexHull2D m_frustumHull;
	ConvexHull2D m_largerHull;
	ConvexHull2D m_prevLargerHull;

	ASMQuadTree m_quadTree;

	Config m_cfg;
	int m_indirectionTextureSize;
	int m_demMinRefinement[2];

	eastl::vector<RenderTarget*> m_indirectionTexturesMips;
	RenderTarget* m_lodClampTexture;

	mat4 m_indexTexMat;
	mat4 m_indexViewMat;

	

	void Load(const ASM::RenderTargets& renderTargets, bool isPreRender);

	bool IsLightDirDifferent(const vec3& lightDir) const;
	void Set(ICameraController* lightCameraController, const vec3& lightDir);
	void Reset();

	void CreateTiles(ASMTileCache* pCache, const Camera& mainViewCamera);

	void BuildTextures(const ASM::SShadowMapRenderContext& context, bool isPreRender);

	//void DrawDebug(DebugRenderer& debug, float scale);

	bool IsValid() const { return m_ID != 0; }
	bool UseLayers() const { return m_cfg.m_minRefinementForLayer <= m_cfg.m_maxRefinement; }
	int GetDEMMinRefinement(bool isLayer) const { return m_demMinRefinement[isLayer]; }
	bool IsLightBelowHorizon() const { return false; }//IsValid() && m_lightDir.y < 0; }

	//const RenderTarget* GetIndirectionTexture() const { return m_indirectionTexture; }
	const RenderTarget* GetLODClampTexture() const { return m_lodClampTexture; }
	const RenderTarget* GetLayerIndirectionTexture() const { return m_layerIndirectionTexture; }

	const Camera CalcCamera(const vec3& cameraPos, const AABBox& BBoxLS,
		const vec2& viewportScaleFactor, bool reverseZ = true, bool customCamera = false) const;

	const Camera CalcCamera(const AABBox& BBoxLS, 
		const vec3& worldCenter, const vec2& viewportScaleFactor, bool customCamera = false) const;

	void UpdateWarpVector(const ASMCpuSettings& asmCpuSettings, const vec3& sunDir, bool disableWarping);

	void GetIndirectionTextureData(ASMTileCacheEntry* pTile, vec4& packedData, ivec4& dstCoord);

private:
	//RenderTarget* m_indirectionTexture;
	
	RenderTarget* m_layerIndirectionTexture;

	eastl::vector< AtlasQuads::SFillQuad > m_quads;
	unsigned int m_quadsCnt[ASM::MAX_REFINEMENT + 1];

	eastl::vector< AtlasQuads::SFillQuad > m_lodClampQuads;

	eastl::vector< QuadTreeNode* > m_indexedNodes;
	AABBox m_indexBBox;
	vec3 m_indexCameraPos;

	static bool RefineAgainstFrustum(
		const AABBox& childBBox,
		const QuadTreeNode* pParent,
		const ASMFrustum& frustum);

	template< class T, bool(*isRefinable)(const AABBox&, const QuadTreeNode*, const T&) >
	static void RefineNode(QuadTreeNode* pParent, int maxRefinement, const T& userData);

	void AllocateTiles(ASMTileCache* pCache, QuadTreeNode* pNode);
	void RemoveNonIntersectingNodes(QuadTreeNode* pNode);

	void FindIndexedNodes();
	void FillIndirectionTextureData(bool processLayers);
	void ResetIndirectionTextureData();

	const vec3 ProjectToTS(const vec3& pointLS, const AABBox& BBoxLS, const vec3& cameraOffset);

	bool ShouldNodeExist(const AABBox& BBox, unsigned char refinement) const;

	void FillLODClampTextureData();
	void UpdateIndirectionTexture(RenderTarget* indirectionTexture, 
		const ASM::SShadowMapRenderContext& context, bool disableHierarchy, bool isPreRender);
	void UpdateLODClampTexture(RenderTarget* lodClampTexture, 
		const ASM::SShadowMapRenderContext& context);

private:
	bool mFirstTimeRender;
};

