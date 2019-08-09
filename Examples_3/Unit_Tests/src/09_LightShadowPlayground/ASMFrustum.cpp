#include "ASMFrustum.h"
#include "ASMTileCache.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/sort.h"

//original value, 0.999f and 0.994
static const float lightDirUpdateThreshold = 0.999f;
static const float maxWarpAngleCos = 0.994f;


bool ASMFrustum::IsLightDirDifferent(const vec3& lightDir) const
{
	return dot(m_lightDir, lightDir) < lightDirUpdateThreshold;
}

float ASMFrustum::GetRefinementDistanceSq(const AABBox& BBox, const vec2& refinementPos)
{
	return lengthSqr((BBox.GetCenter()) - Helper::Vec2_To_Vec3(refinementPos));
}

ASMFrustum::ASMFrustum(const Config& cfg, bool useMRF, bool isPreRender)
	:m_cfg(cfg),
	mIsPrerender(isPreRender),
	mFirstTimeRender(true)
{
	m_demMinRefinement[0] = useMRF ? (UseLayers() ? 1 : 0) : -1;
	m_demMinRefinement[1] = m_cfg.m_minRefinementForLayer;
	m_indirectionTextureSize = (1 << m_cfg.m_maxRefinement) * m_cfg.m_indexSize;
	Reset();
}


ASMFrustum::~ASMFrustum()
{
}

void ASMFrustum::Reset()
{
	m_quadTree.Reset();
	m_ID = 0;
	m_lightDir = vec3(0.0);
	m_lightRotMat = mat4::identity();
	m_invLightRotMat = mat4::identity();

	m_indexTexMat = mat4::identity();
	m_indexViewMat = mat4::identity();

	m_refinementPoint = vec2(0.0);

	m_frustumHull.Reset();
	m_largerHull.Reset();
	m_prevLargerHull.Reset();

	m_receiverWarpVector = vec3(0.0);
	m_blockerSearchVector = vec3(0.0);
	m_disableWarping = false;

	ResetIndirectionTextureData();
}

void ASMFrustum::Set(ICameraController* lightCameraController, const vec3& lightDir)
{
	Reset();
	m_lightDir = lightDir;
	
	lightCameraController->moveTo(vec3(0.f));
	lightCameraController->lookAt(-m_lightDir);

	m_lightRotMat = lightCameraController->getViewMatrix();
	m_invLightRotMat = inverse(m_lightRotMat);
	
	
	static unsigned int s_IDGen = 1;
	m_ID = s_IDGen; s_IDGen += 2;
}

const Camera ASMFrustum::CalcCamera( const vec3& cameraPos, const AABBox& BBoxLS, const vec2& viewportScaleFactor,
	bool reverseZ, bool customCamera) const
{
	mat4 viewMat = mat4::lookAt(Point3(cameraPos), Point3(cameraPos + m_lightDir), vec3(0.f, 1.f, 0.f));
    Camera camera;
	camera.SetViewMatrix(viewMat);

    float hw = 0.5f * BBoxLS.GetSizeX() * viewportScaleFactor.getX();
    float hh = 0.5f * BBoxLS.GetSizeY() * viewportScaleFactor.getY();

	float farPlane = ASM::s_tileFarPlane;

	if (reverseZ)
	{
		camera.SetProjection(mat4::orthographic(-hw, hw, -hh, hh, farPlane, 0));
	}
	else
	{
		camera.SetProjection(mat4::orthographic(-hw, hw, -hh, hh, 0, farPlane));
	}

    return camera;
}

const Camera ASMFrustum::CalcCamera( const AABBox& BBoxLS, 
	const vec3& worldCenter, const vec2& viewportScaleFactor, bool customCamera) const
{
    vec3 aabbMin = worldCenter + vec3(-800.0f,-200.0f,-800.0f );
    vec3 aabbMax = worldCenter + vec3( 800.0f, 500.0f, 800.0f );

    float minZ = FLT_MAX;
    for( int i = 0; i < 8; ++i )
    {
        vec3 aabbCorner(
             i & 1 ? aabbMin.getX() : aabbMax.getX(),
             i & 2 ? aabbMin.getY() : aabbMax.getY(),
             i & 4 ? aabbMin.getZ() : aabbMax.getZ() );
        minZ = fmin( minZ, -dot( aabbCorner, m_lightDir ) );
    }
    //vec3 cameraPos = BBoxLS.GetCenter() * m_invLightRotMat - minZ * m_lightDir;
	vec3 cameraPos = Helper::Vec4_To_Vec3( m_invLightRotMat * Helper::Vec3_To_Vec4( BBoxLS.GetCenter(), 1.f)) - minZ * m_lightDir;

    static const vec3 boundsN[] =
    {
         vec3(-1.0f, 0.0f, 0.0f ),
         vec3( 0.0f,-1.0f, 0.0f ),
         vec3( 0.0f, 0.0f,-1.0f ),
         vec3( 1.0f, 0.0f, 0.0f ),
         vec3( 0.0f, 1.0f, 0.0f ),
         vec3( 0.0f, 0.0f, 1.0f ),
    };

    float boundsD[] =
    {
         aabbMax.getX(),  aabbMax.getY(),  aabbMax.getZ(),
        -aabbMin.getX(), -aabbMin.getY(), -aabbMin.getZ(),
    };

    float minF = 0;
    for( unsigned int i = 0; i < 6; ++i )
    {
        float f1 = dot( boundsN[i], cameraPos ) + boundsD[i];
        float f2 = dot( boundsN[i], m_lightDir );
        if( f1 <= 0 && f2 < 0 )
        {
            minF = max( minF, f1 / f2 );
        }
    }

    return CalcCamera( cameraPos - minF * m_lightDir, BBoxLS, viewportScaleFactor, true, customCamera);
}

void ASMFrustum::UpdateWarpVector(const ASMCpuSettings& asmCpuSettings, 
	const vec3& lightDir, bool disableWarping)
{
	if (!IsValid())
	{
		return;
	}
	m_disableWarping |= disableWarping;
	if (m_disableWarping)
	{
		return;
	}

	if (dot(m_lightDir, lightDir) < maxWarpAngleCos)
	{
		return;
	}

	vec3 shadowDir = -m_lightDir;
	vec3 dir = lightDir - 2.0f * dot(shadowDir, lightDir) * shadowDir;

	float warpBias = 1.0f - 0.9f * length(dir - shadowDir);
	m_receiverWarpVector = warpBias * dir - shadowDir;


	vec3 warpDirVS = Helper::Vec4_To_Vec3(  m_indexViewMat * 
		Helper::Vec3_To_Vec4(m_receiverWarpVector, 0.f) );
	   	

	float stepDistance = asmCpuSettings.mParallaxStepDistance;
	float stepBias = asmCpuSettings.mParallaxStepBias;
	m_blockerSearchVector = vec3(
		stepDistance * warpDirVS.getX() / gs_ASMDEMAtlasTextureWidth,
		stepDistance * warpDirVS.getY() / gs_ASMDEMAtlasTextureHeight,
		-stepBias / gs_ASMTileFarPlane);
}
//

void ASMFrustum::Load(const ASM::RenderTargets& renderTargets, bool isPreRender)
{
	m_indirectionTexturesMips.clear();
	if (!isPreRender)
	{
		m_indirectionTexturesMips = renderTargets.m_pASMIndirectionMips;
		m_lodClampTexture = renderTargets.m_pRenderTargetASMLodClamp;
	}
	else
	{
		m_indirectionTexturesMips = renderTargets.m_pASMPrerenderIndirectionMips;
		m_lodClampTexture = renderTargets.m_pRenderTargetASMPrerenderLodClamp;
	}
	//m_indirectionTexture = renderTargets.m_pASMIndirection;
}


void ASMFrustum::BuildTextures(const ASM::SShadowMapRenderContext& context, bool isPreRender)
{
	FindIndexedNodes();
	ResetIndirectionTextureData();
	FillIndirectionTextureData(false);
	UpdateIndirectionTexture(NULL, context, isPreRender, isPreRender);

	if (isPreRender)
	{
		FillLODClampTextureData();
		UpdateLODClampTexture(m_lodClampTexture, context);
	}

	if (UseLayers())
	{
		ResetIndirectionTextureData();
		FillIndirectionTextureData(true);
		UpdateIndirectionTexture(m_layerIndirectionTexture, context, isPreRender, isPreRender);
	}

}


void ASMFrustum::CreateTiles(ASMTileCache* pCache, const Camera& mainViewCamera)
{
	if (!IsValid() || IsLightBelowHorizon())
	{
		return;
	}

	m_refinementPoint = m_frustumHull.FindFrustumConvexHull(mainViewCamera, 
		m_cfg.m_shadowDistance, m_lightRotMat);

	m_prevLargerHull = m_largerHull;

	m_largerHull.FindFrustumConvexHull(mainViewCamera, 
		1.01f * m_cfg.m_shadowDistance, m_lightRotMat);

	for (size_t i = m_quadTree.GetRoots().size(); i > 0; --i)
	{
		RemoveNonIntersectingNodes(m_quadTree.GetRoots()[i - 1]);
	}

	AABBox hullBBox;
	hullBBox.Reset();

	for (int i = 0; i < m_frustumHull.m_size; ++i)
	{
		hullBBox.Add( Helper::Vec2_To_Vec3( m_frustumHull.m_vertices[i] ) );
	}

	hullBBox.Add(Helper::Vec2_To_Vec3(m_refinementPoint) + vec3(m_cfg.m_minExtentLS, m_cfg.m_minExtentLS, 0.f));
	hullBBox.Add(Helper::Vec2_To_Vec3(m_refinementPoint) - vec3(m_cfg.m_minExtentLS, m_cfg.m_minExtentLS, 0.f));

	AABBox::AlignBBox(hullBBox, m_cfg.m_largestTileWorldSize);
	

	AABBox nodeBBox(vec3(0.f), vec3(0.f));
	float minY = nodeBBox.m_min.getY();
	float minX = nodeBBox.m_min.getX();
	for (minY = hullBBox.m_min.getY(); minY < hullBBox.m_max.getY(); minY += m_cfg.m_largestTileWorldSize)
	{
		
		for (minX = hullBBox.m_min.getX(); minX < hullBBox.m_max.getX(); minX += m_cfg.m_largestTileWorldSize)
		{
			nodeBBox.m_min.setY(minY);
			nodeBBox.m_min.setX(minX);
			nodeBBox.m_max = nodeBBox.m_min + vec3(m_cfg.m_largestTileWorldSize, m_cfg.m_largestTileWorldSize, 0.0f);
			if (ShouldNodeExist(nodeBBox, 0))
			{
				QuadTreeNode* pNode = m_quadTree.FindRoot(nodeBBox);
				if (pNode == nullptr)
				{
					QuadTreeNode* temp = (QuadTreeNode*)conf_malloc(sizeof(QuadTreeNode));
					pNode = conf_placement_new<QuadTreeNode>(temp, &m_quadTree, (QuadTreeNode*)0);
					pNode->mBBox = nodeBBox;
				}
				
				RefineNode < ASMFrustum, &ASMFrustum::RefineAgainstFrustum > (pNode, m_cfg.m_maxRefinement, *this);
			}
		}
	}

	for (auto it = m_quadTree.GetRoots().begin(); it != m_quadTree.GetRoots().end(); ++it)
	{
		AllocateTiles(pCache, *it);
	}
}

bool ASMFrustum::ShouldNodeExist(const AABBox& bbox, unsigned char refinement) const
{
	return GetRefinementDistanceSq(bbox, m_refinementPoint) < fabsf(m_cfg.m_refinementDistanceSq[refinement]) ?
		(m_cfg.m_refinementDistanceSq[refinement] < 0 || m_frustumHull.Intersects(bbox)) : false;
}


void ASMFrustum::RemoveNonIntersectingNodes(QuadTreeNode* pNode)
{
	for (int i = 0; i < 4; ++i)
	{
		if (pNode->mChildren[i])
		{
			RemoveNonIntersectingNodes(pNode->mChildren[i]);
		}
	}

	if (pNode->mLastFrameVerified != ASM::s_frameCounter)
	{
		pNode->mLastFrameVerified = ASM::s_frameCounter;

		if (ShouldNodeExist(pNode->mBBox, pNode->mRefinement))
		{
			if (pNode->m_pParent)
			{
				pNode->m_pParent->mLastFrameVerified = ASM::s_frameCounter;
			}
			return;
		}
		conf_delete(pNode);
	}
}


bool ASMFrustum::RefineAgainstFrustum(
	const AABBox& childbbox,
	const QuadTreeNode* pParent,
	const ASMFrustum& frustum)
{
	return frustum.ShouldNodeExist (childbbox, pParent->mRefinement + 1);
}

template< class T, bool(*isRefinable)(const AABBox&, const QuadTreeNode*, const T&) >
void ASMFrustum::RefineNode(QuadTreeNode* pParent, int maxRefinement, const T& userData)
{
	if (pParent->mRefinement < maxRefinement)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (pParent->mChildren[i])
			{
				RefineNode <T, isRefinable> (
					pParent->mChildren[i], maxRefinement, userData);
			}
			else
			{
				//Here we check if any of the nodes requires new 
				//child node or not
				AABBox childBBox = pParent->GetChildBBox(i);
				if (isRefinable(childBBox, pParent, userData))
				{
					QuadTreeNode* pNode = pParent->AddChild(i);
					pNode->mBBox = childBBox;
					RefineNode <T, isRefinable> (pNode, maxRefinement, userData);
				}
			}
		}
	}
}

struct SortStruct
{
	QuadTreeNode* m_pNode;
	float mKey;
};

void ASM::SortNodes(
	const vec2& refinementPoint,
	const vec2& sortRegionMaxSize,
	float tileSize,
	const eastl::vector<QuadTreeNode*>& nodes,
	eastl::vector<QuadTreeNode*>& sortedNodes,
	AABBox& sortedBBox)
{
	
	struct CompareSortStruct
	{
		int operator () (const SortStruct& left, const SortStruct& right)
		{
			return left.mKey < right.mKey;
		}

		int Compare(const SortStruct& left, const SortStruct& right)
		{
			return (left.mKey < right.mKey) ? 1 : 0;
		}
	};

	
	//SortStruct* nodesToSort = (SortStruct*)	conf_malloc(sizeof(SortStruct) * nodes.size());

	eastl::vector<SortStruct> nodesToSort;
	nodesToSort.reserve(nodes.size());

	//SortStruct* nodesToSort = (SortStruct*)conf_malloc(
		//sizeof(SortStruct) * nodes.size());

	vec2 distMax = sortRegionMaxSize + vec2(tileSize, tileSize);
	float distMaxSq = dot(distMax, distMax);

	unsigned int numNodesToSort = 0;

	for (int i = 0; i < nodes.size(); ++i)
	{
		QuadTreeNode* pNode = nodes[i];
		if (IsNodeAcceptableForIndexing(pNode))
		{
			AABBox& bbox = pNode->mBBox;
			vec3 bboxCenter = bbox.GetCenter();
			float dx = max(fabsf(refinementPoint.getX() - bboxCenter.getX()) - bbox.GetSizeX() * 0.5f, 0.0f);
			float dy = max(fabsf(refinementPoint.getY() - bboxCenter.getY()) - bbox.GetSizeY() * 0.5f, 0.0f);

			float distSq = dx * dx + dy * dy;
			if (distSq < distMaxSq)
			{
				nodesToSort.push_back(SortStruct());
				SortStruct& ss = nodesToSort[numNodesToSort++];
				ss.mKey = fabsf(bbox.m_min.getX() - refinementPoint.getX());
				ss.mKey = max(fabsf(bbox.m_min.getY() - refinementPoint.getY()), ss.mKey);
				ss.mKey = max(fabsf(bbox.m_max.getX() - refinementPoint.getX()), ss.mKey);
				ss.mKey = max(fabsf(bbox.m_max.getY() - refinementPoint.getY()), ss.mKey);
				ss.m_pNode = pNode;
			}
		}
	}


	SortStruct* data = nodesToSort.data();
	int32_t    count = (int32_t)nodesToSort.size();

	std::qsort(data, count, sizeof(SortStruct), [](const void* a, const void* b) {
		const SortStruct* left = (SortStruct*)(a);
		const SortStruct* right = (SortStruct*)(b);

		if (left->mKey < right->mKey)
		{
			return -1;
		}
		else if(left->mKey > right->mKey)
		{
			return 1;
		}

		return 0;
	});

	sortedBBox = AABBox(Helper::Vec2_To_Vec3(refinementPoint, 0.f), 
		Helper::Vec2_To_Vec3(refinementPoint, 0.f));
	AABBox::AlignBBox(sortedBBox, tileSize);

	sortedNodes.resize(0);

	for (unsigned int i = 0; i < numNodesToSort; ++i)
	{
		SortStruct& ss = nodesToSort[i];
		const AABBox& nodeBBox = ss.m_pNode->mBBox;
		vec3 testMin(min(sortedBBox.m_min.getX(), nodeBBox.m_min.getX()),
			min(sortedBBox.m_min.getY(), nodeBBox.m_min.getY()), 0.f);
		vec3 testMax(max(sortedBBox.m_max.getX(), nodeBBox.m_max.getX()),
			max(sortedBBox.m_max.getY(), nodeBBox.m_max.getY()), 0.f);

		if ((testMax.getX() - testMin.getX()) > sortRegionMaxSize.getX()
			|| (testMax.getY() - testMin.getY()) > sortRegionMaxSize.getY())
		{
			if (ss.mKey > distMax.getX())
			{
				break;
			}
		}
		else
		{
			sortedBBox = AABBox(testMin, testMax);
			sortedNodes.push_back(ss.m_pNode);
		}
	}
	
}

void ASMFrustum::FindIndexedNodes()
{
	if (!IsValid())
	{
		return;
	}

	float sortRegionSizeMax = static_cast<float>(m_cfg.m_indexSize) 
		* m_cfg.m_largestTileWorldSize;
	
	ASM::SortNodes(
		m_refinementPoint,
		vec2(sortRegionSizeMax),
		m_cfg.m_largestTileWorldSize,
		m_quadTree.GetRoots(),
		m_indexedNodes,
		m_indexBBox
		);

	m_indexBBox = AABBox(m_indexBBox.m_min, m_indexBBox.m_min + 
		vec3(sortRegionSizeMax, sortRegionSizeMax, 0.f));

	if (!m_indexedNodes.empty())
	{
		float offset = -FLT_MAX;

		for (int i = 0; i < m_indexedNodes.size(); ++i)
		{
			QuadTreeNode* indexedNode = m_indexedNodes[i];
			offset = fmax(offset, dot(m_lightDir, 
				indexedNode->m_pTile->m_renderCamera.GetPosition()));
		}
		m_indexCameraPos = Helper::Vec4_To_Vec3( m_invLightRotMat * Helper::Vec3_To_Vec4(
			m_indexBBox.GetCenter(), 1.f))  + offset * m_lightDir;

		//TODO: figure out the calc camera logic and how it exactly works
		Camera camera = CalcCamera(m_indexCameraPos, m_indexBBox, vec2(1.0f), true);
		m_indexViewMat = camera.GetViewMatrix();

		Camera otherCamera = CalcCamera(m_indexCameraPos, m_indexBBox, vec2(1.0f), false);
		
		static const mat4 screenToTexCoordMatrix = mat4::translation(vec3(0.5f, 0.5f, 0.f)) * mat4::scale(vec3(0.5f, -0.5f, 1.f));
		m_indexTexMat = screenToTexCoordMatrix * camera.GetViewProjection();
	}
}

const vec3 ASMFrustum::ProjectToTS(const vec3& pointLS, 
	const AABBox& bboxLS, const vec3& cameraOffset)
{
	return vec3(
		(pointLS.getX() - bboxLS.m_min.getX()) / bboxLS.GetSizeX(),
		1.0f - (pointLS.getY() - bboxLS.m_min.getY()) / bboxLS.GetSizeY(),
		-dot(m_lightDir, Helper::Vec4_To_Vec3( m_invLightRotMat * Helper::Vec3_To_Vec4(pointLS, 1.0) ) + cameraOffset) / ASM::s_tileFarPlane);
}

void ASMFrustum::ResetIndirectionTextureData()
{
	memset(m_quadsCnt, 0, sizeof(m_quadsCnt));
	m_quads.resize(0);
	m_quads.reserve(PACKED_QUADS_ARRAY_REGS);
	
	m_lodClampQuads.resize(1);
	m_lodClampQuads[0] = AtlasQuads::SFillQuad::Get(vec4(1.f), m_indirectionTextureSize, 
		m_indirectionTextureSize, 0, 0,  m_indirectionTextureSize, m_indirectionTextureSize);
}

void ASMFrustum::GetIndirectionTextureData(ASMTileCacheEntry* pTile, vec4& packedData, ivec4& dstCoord)
{
	float invAtlasWidth = 1.f / float(pTile->GetCache()->GetDepthAtlasWidth());
	float invAtlasHeight = 1.f / float(pTile->GetCache()->GetDepthAtlasHeight());

	vec3 tileMin(0.f, 1.f, 0.f);
	vec3 tileMax(1.f, 0.f, 0.f);

	vec3 indexMin = ProjectToTS(pTile->m_BBox.m_min, m_indexBBox,
		pTile->m_renderCamera.GetPosition() - m_indexCameraPos);
	vec3 indexMax = ProjectToTS(pTile->m_BBox.m_max, m_indexBBox,
		pTile->m_renderCamera.GetPosition() - m_indexCameraPos);

	int x0 = static_cast<int>( indexMin.getX() * 
		static_cast<float>(m_indirectionTextureSize) + 0.25f );
	int y0 = static_cast<int>(indexMax.getY() *
		static_cast<float>(m_indirectionTextureSize) + 0.25f);

	int x1 = static_cast<int>(indexMax.getX() *
		static_cast<float>(m_indirectionTextureSize) - 0.25f);
	int y1 = static_cast<int>(indexMin.getY() *
		static_cast<float>(m_indirectionTextureSize) - 0.25f);

	const int mipMask = (1 << (m_cfg.m_maxRefinement - pTile->m_refinement)) - 1;

	// Compute affine transform (scale and offset) from index normalized cube to tile normalized cube.
	vec3 scale1(
		(tileMax.getX() - tileMin.getX()) / (indexMax.getX() - indexMin.getX()),
		(tileMax.getY() - tileMin.getY()) / (indexMax.getY() - indexMin.getY()),
		1.0f);
	vec3 offset1 = tileMin - Helper::Piecewise_Prod( indexMin, scale1 );

	// Compute affine transform (scale and offset) from tile normalized cube to shadowmap atlas.
	vec3 scale2(
		float(pTile->m_viewport.w) * invAtlasWidth,
		float(pTile->m_viewport.h) * invAtlasHeight,
		1.0f);
	vec3 offset2(
		(float(pTile->m_viewport.x) + 0.5f) * invAtlasWidth,
		(float(pTile->m_viewport.y) + 0.5f) * invAtlasHeight,
		0.0f);

	// Compute combined affine transform from index normalized cube to shadowmap atlas.
	vec3 scale = Helper::Piecewise_Prod(scale1, scale2);
	vec3 offset = Helper::Piecewise_Prod(offset1, scale2) + offset2;

	// Assemble data for indirection texture:
	//   packedData.xyz contains transform from view frustum of index texture to view frustum of individual tile
	//   packedData.w contains packed data: integer part is refinement-dependent factor for texcoords computation,
	//      fractional part is bias for smooth tile transition unpacked via getFadeInConstant() in shader,
	//      sign indicates if the tile is a layer tile or just a regular tile.
	packedData.setX(offset.getX());
	packedData.setY(offset.getY());
	packedData.setZ(offset.getZ());
	packedData.setW(
		float((1 << pTile->m_refinement) * ASM::BORDERLESS_TILE_SIZE * m_cfg.m_indexSize)
	);

	dstCoord = ivec4(x0, y0, x1, y1);
}


void ASMFrustum::FillIndirectionTextureData(bool processLayers)
{
	if (!IsValid())
	{
		return;
	}

	if (m_indexedNodes.empty())
	{
		return;
	}

	size_t numIndexedNodes = m_indexedNodes.size();
	unsigned int i = 0;

	for (int z = m_cfg.m_maxRefinement; z >= 0; --z)
	{
		size_t numNodes = m_indexedNodes.size();
		for ( ; i < numNodes ; ++i)
		{
			QuadTreeNode* pNode = m_indexedNodes[i];
			ASMTileCacheEntry* pTile = pNode->m_pTile;
			bool useRegularShadowMapAsLayer = false;
			if (processLayers)
			{
				if (!ASM::IsTileAcceptableForIndexing(pNode->m_pLayerTile))
				{

				}
				else
				{
					pTile = pNode->m_pLayerTile;
				}
			}

			vec4 packedData;
			ivec4 destCoord;
			GetIndirectionTextureData(pTile, packedData, destCoord);

			packedData.setW(packedData.getW() + pTile->m_fadeInFactor);

			if (useRegularShadowMapAsLayer)
			{
				packedData.setW(-packedData.getW());
			}

			m_quads.push_back(AtlasQuads::SFillQuad::Get(
				packedData,
				destCoord.getZ() - destCoord.getX() + 1,
				destCoord.getW() - destCoord.getY() + 1,
				destCoord.getX(),
				destCoord.getY(),
				m_indirectionTextureSize, m_indirectionTextureSize
			));

			++m_quadsCnt[z];

			for (int j = 0; j < 4; ++j)
			{
				QuadTreeNode* pChild = pNode->mChildren[j];
				if (pChild && ASM::IsNodeAcceptableForIndexing(pChild))
				{
					m_indexedNodes.push_back(pChild);
				}
			}

		}
	}
	m_indexedNodes.resize(numIndexedNodes);
}


void ASMFrustum::FillLODClampTextureData()
{
	if (!IsValid() || m_indexedNodes.empty())
	{
		return;
	}

	size_t numIndexedNodes = m_indexedNodes.size();
	unsigned int i = 0;

	for (int z = m_cfg.m_maxRefinement; z >= 0; --z)
	{
		float clampValue = static_cast<float>(z) / 
			static_cast<float>(ASM::MAX_REFINEMENT);

		size_t numNodes = m_indexedNodes.size();
		
		for (; i < numNodes; ++i)
		{
			QuadTreeNode* pNode = m_indexedNodes[i];
			ASMTileCacheEntry* pTile = pNode->m_pTile;

			if (z < m_cfg.m_maxRefinement)
			{
				vec4 packedData;
				ivec4 destCoord;
				GetIndirectionTextureData(pTile, packedData, destCoord);

				m_lodClampQuads.push_back(AtlasQuads::SFillQuad::Get(
					vec4(clampValue),
					destCoord.getZ() - destCoord.getX() + 1,
					destCoord.getW() - destCoord.getY() + 1,
					destCoord.getX(),
					destCoord.getY(),
					m_indirectionTextureSize, m_indirectionTextureSize
				));
			}

			for (int j = 0; j < 4; ++j)
			{
				QuadTreeNode* pChild = pNode->mChildren[j];
				if (pChild && pChild->m_pTile)
				{
					m_indexedNodes.push_back(pChild);
				}
			}

		}
	}
	m_indexedNodes.resize(numIndexedNodes);
}


void ASMFrustum::UpdateLODClampTexture(RenderTarget* lodClampTexture,
	const ASM::SShadowMapRenderContext& context)
{
	RendererContext* rendererContext = context.m_pRendererContext;
	Cmd* pCurCmd = rendererContext->m_pCmd;

	ASM::GenerateLodClampRenderData& generateLodClampRenderData =
		context.m_shadowMapRenderer->mTickData.mGenerateLodClampRenderData;


	TextureBarrier lodBarrier[] = { 
		{lodClampTexture->pTexture, RESOURCE_STATE_RENDER_TARGET} };

	cmdResourceBarrier(pCurCmd, 0, NULL, 1, lodBarrier, false);


	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
	loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
	
	Helper::setRenderTarget(pCurCmd, 1, &lodClampTexture, NULL, &loadActions,
		vec2(0.f, 0.f), vec2((float)lodClampTexture->mDesc.mWidth, 
		(float)lodClampTexture->mDesc.mHeight));

	
	BufferUpdateDesc updateBufferDesc = {};
	updateBufferDesc.pBuffer = generateLodClampRenderData.
		pBufferLodClampPackedQuadsUniform;
	updateBufferDesc.pData = &m_lodClampQuads[0];
	updateBufferDesc.mSize = 
		sizeof(AtlasQuads::SFillQuad) * m_lodClampQuads.size();
	
	updateResource(&updateBufferDesc);

	cmdBindPipeline(pCurCmd, 
		generateLodClampRenderData.m_pGraphicsPipeline);

	DescriptorData params[1] = {};
	params[0].pName = "PackedAtlasQuads_CB";
	params[0].ppBuffers = &generateLodClampRenderData.
		pBufferLodClampPackedQuadsUniform;
	
	cmdBindDescriptors(pCurCmd, rendererContext->m_pDescriptorBinder, 
		generateLodClampRenderData.m_pRootSignature, 1, params);

	cmdDraw(pCurCmd, static_cast<unsigned int>(m_lodClampQuads.size()) * 6u, 0);

	cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
}

void ASMFrustum::UpdateIndirectionTexture(RenderTarget* indirectionTexture,
	const ASM::SShadowMapRenderContext& context, bool disableHierarchy, bool isPreRender)
{
	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
#ifdef _DURANGO
	if (mFirstTimeRender)
	{
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = m_indirectionTexturesMips[m_cfg.m_maxRefinement]->mDesc.mClearValue;
		mFirstTimeRender = false;
	}
#endif
	loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
	loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

	RendererContext* curRendererContext = context.m_pRendererContext;

	ASM::IndirectionRenderData* finalIndirectionRenderData = disableHierarchy ? 
		(&context.m_shadowMapRenderer->mTickData.mPrerenderIndirectionRenderData) : 
		(&context.m_shadowMapRenderer->mTickData.mIndirectionRenderData);



	ASM::IndirectionRenderData& indirectionRenderData = *finalIndirectionRenderData;

	AtlasQuads::SFillQuad clearQuad = AtlasQuads::SFillQuad::Get(vec4(0.f),
		m_indirectionTextureSize, m_indirectionTextureSize, 0, 0, m_indirectionTextureSize, m_indirectionTextureSize);


	ASMPackedAtlasQuadsUniform packedClearAtlasQuads = {};
	packedClearAtlasQuads = *(ASMPackedAtlasQuadsUniform*) &clearQuad;
	   

	uint64_t fullPackedBufferSize = PACKED_QUADS_ARRAY_REGS * sizeof(float) * 4;
	
	BufferUpdateDesc clearUpdateUbDesc =
	{ indirectionRenderData.pBufferASMClearIndirectionQuadsUniform,
		&packedClearAtlasQuads };
	
	
	updateResource(&clearUpdateUbDesc);
		
	unsigned int firstQuad = 0;
	unsigned int numQuads = 0;
	
	
	
	for (int mip = m_cfg.m_maxRefinement; mip >= 0; --mip)
	{
		numQuads += m_quadsCnt[mip];

		

		TextureBarrier targetBarrier[] = { 
			{m_indirectionTexturesMips[mip]->pTexture, RESOURCE_STATE_RENDER_TARGET} };

		cmdResourceBarrier(curRendererContext->m_pCmd, 0, NULL, 1, targetBarrier, false);

		Helper::setRenderTarget(curRendererContext->m_pCmd, 1, &m_indirectionTexturesMips[mip],
			NULL, &loadActions, vec2(0.f),
			vec2((float)m_indirectionTexturesMips[mip]->mDesc.mWidth,
				(float)m_indirectionTexturesMips[mip]->mDesc.mHeight)
		);

		//------------------Clear ASM indirection quad

		DescriptorData clearQuadParams[1] = {};
		clearQuadParams[0].pName = "PackedAtlasQuads_CB";
		clearQuadParams[0].ppBuffers = &indirectionRenderData.
			pBufferASMClearIndirectionQuadsUniform;
		//clearQuadParams[0].pSizes = &fullPackedBufferSize;
		
		
		cmdBindPipeline(curRendererContext->m_pCmd, indirectionRenderData.m_pGraphicsPipeline);
		cmdBindDescriptors(curRendererContext->m_pCmd, 
			curRendererContext->m_pDescriptorBinder,
			indirectionRenderData.m_pRootSignature, 1, clearQuadParams);
		

		cmdDraw(curRendererContext->m_pCmd, 6, 0);



		//------------------
		


		if (numQuads > 0)
		{
			
			//ASMPackedAtlasQuadsUniform* packedIndirectionAtlasQuads = NULL;

			//packedIndirectionAtlasQuads = reinterpret_cast<ASMPackedAtlasQuadsUniform*>(&m_quads[firstQuad]);

			BufferUpdateDesc updateIndirectionUBDesc = 
			{
				indirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[mip],
				&m_quads[firstQuad]
			};
			uint64_t curBufferSize = sizeof(vec4) * 2 * numQuads;
			updateIndirectionUBDesc.mSize = curBufferSize;
			
			updateResource(&updateIndirectionUBDesc);


			DescriptorData updateQuadParams[1] = {};
			updateQuadParams[0].pName = "PackedAtlasQuads_CB";
			updateQuadParams[0].ppBuffers = &indirectionRenderData.
				pBufferASMPackedIndirectionQuadsUniform[mip];

			cmdBindDescriptors(curRendererContext->m_pCmd,
				curRendererContext->m_pDescriptorBinder,
				indirectionRenderData.m_pRootSignature, 1, updateQuadParams);

			cmdDraw(curRendererContext->m_pCmd, 6 * numQuads, 0);
		}

		if (disableHierarchy)
		{
			firstQuad += numQuads;
			numQuads = 0;
		}

		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	//LOGF(LogLevel::eINFO, "Indirection num quads %d", numQuads);

}
