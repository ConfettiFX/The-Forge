#ifndef Geometry_h
#define Geometry_h


#include "../../../../Common_3/OS/Math/MathTypes.h"
//EA stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

namespace eastl
{
	template <>
	struct has_equality<vec3> : eastl::false_type {};
}

//#define NO_HLSL_DEFINITIONS 1
//#include "Shader_Defs.h"
#if defined(METAL)
#include "Shaders/Metal/Shader_Defs.h"
#include "Shaders/Metal/ASMConstant.h"
#include "Shaders/Metal/SDF_Constant.h"
#elif defined(DIRECT3D12) || defined(XBOX)
#define NO_HLSL_DEFINITIONS
#include "Shaders/D3D12/Shader_Defs.h"
#include "Shaders/D3D12/ASMShader_Defs.h"
#include "Shaders/D3D12/SDF_Constant.h"
#elif defined(VULKAN)
#define NO_GLSL_DEFINITIONS
#include "Shaders/Vulkan/Shader_Defs.h"
#include "Shaders/Vulkan/ASMShader_Defs.h"
#include "Shaders/Vulkan/SDF_Constant.h"
#elif defined(ORBIS)
#define NO_ORBIS_DEFINITIONS
#include "../../../../PS4/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/Shader_Defs.h"
#include "../../../../PS4/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/ASMShader_Defs.h"
#include "../../../../PS4/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/SDF_Constant.h"
#elif defined(PROSPERO)
#define NO_PROSPERO_DEFINITIONS
#include "../../../../Prospero/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/Shader_Defs.h"
#include "../../../../Prospero/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/ASMShader_Defs.h"
#include "../../../../Prospero/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/SDF_Constant.h"
#endif


struct Buffer;
struct ThreadSystem;

struct SDFVolumeData;

typedef struct ClusterCompact
{
	uint32_t triangleCount;
	uint32_t clusterStart;
} ClusterCompact;

typedef struct Cluster
{
	float3 aabbMin, aabbMax;
	float3 coneCenter, coneAxis;
	float  coneAngleCosine;
	float  distanceFromCamera;
	bool   valid;
} Cluster;

struct FilterBatchData
{
	uint meshIndex; // Index into meshConstants
	uint indexOffset; // Index relative to meshConstants[meshIndex].indexOffset
	uint faceCount; // Number of faces in this small batch
	uint outputIndexOffset; // Offset into the output index buffer
	uint drawBatchStart; // First slot for the current draw call
	uint accumDrawIndex;
	uint _pad0;
	uint _pad1;
};

struct FilterBatchChunk
{
	uint32_t			currentBatchCount;
	uint32_t			currentDrawCallCount;
};



/************************************************************************/
// Meshes
/************************************************************************/


typedef struct SceneVertexPos
{
	float x, y, z;

	static vec3 ToVec3(const SceneVertexPos& v)
	{
		return vec3(v.x, v.y, v.z);
	}

} SceneVertexPos;

typedef struct ClusterContainer
{
	uint32_t        clusterCount;
	ClusterCompact* clusterCompacts;
	Cluster*        clusters;
} ClusterContainer;

typedef struct Material
{
	bool twoSided;
	bool alphaTested;
} Material;

typedef struct Scene
{
	Geometry*                          geom;
	Material*                          materials;
	char**                             textures;
	char**                             normalMaps;
	char**                             specularMaps;
} Scene;

struct SDFCustomSubMeshData;

struct SDFCustomSubMeshData
{
	struct GLTFMesh* m_PSubMesh;
	eastl::string mMeshName;
	bool mIsSDFMesh;
	bool mIsTwoSided;
	bool mUseDoubleVoxelSize;
	bool mIsAlphaTested;
	float mTwoSidedWorldSpaceBias;


	bool mIsAStack;
	eastl::vector<SDFCustomSubMeshData> mStacks;
	eastl::string mStackStrID = "";
};


struct SDFMesh;

struct SDFMeshInstance
{
	uint32_t mMaterialId;
	uint32_t mIndexCount;
	uint32_t mStartIndex;
	uint32_t mStartVertex;

	AABB mLocalBoundingBox = AABB(vec3(FLT_MAX), vec3(-FLT_MAX));
	SDFMesh* mMainMesh = NULL;

	bool mHasGeneratedSDFVolumeData = false;

	bool mIsAlphaTested = false;
	eastl::vector<SDFMeshInstance> mStackInstances;
};

typedef eastl::vector<SDFMeshInstance> SDFMeshInstances;
typedef eastl::vector<SDFVolumeData*> BakedSDFVolumeInstances;
typedef eastl::vector<SDFCustomSubMeshData> CustomSDFSubMeshDataList;


struct SDFMesh
{
	eastl::vector<SceneVertexPos>      mPositions;
	
	eastl::vector<vec2> mUncompressedTexCoords;
	eastl::vector<vec3> mUncompressedNormals;
	eastl::vector<uint32_t> mIndices;

	eastl::vector<SDFMeshInstance> mMeshInstances;
	CustomSDFSubMeshDataList mCustomSubMeshDataList;

	uint32_t mTotalSDFMeshes = 0;
	uint32_t mTotalGeneratedSDFMeshes = 0;
};


struct Vertex
{
	float3 mPos;
	float3 mNormal;
	float3 mTangent;
	float2 mUV;
};


typedef bool (*GenerateVolumeDataFromFileFunc) (SDFVolumeData**, const eastl::string&, float);


void adjustAABB(AABB* ownerAABB, const vec3& point);
void adjustAABB(AABB* ownerAABB, const AABB& otherAABB);
vec3 calculateAABBSize(const AABB* ownerAABB);
vec3 calculateAABBExtent(const AABB* ownerAABB);
vec3 calculateAABBCenter(const AABB* ownerAABB);

void alignAABB(AABB* ownerAABB, float alignment);

void destroyClusters(ClusterContainer* pMesh);
Scene* loadScene(const char* fileName, SyncToken* token, float scale, float offsetX, float offsetY, float offsetZ);
	
void   removeScene(Scene* scene);

void   createClusters(bool twoSided, const Scene* scene, IndirectDrawIndexArguments* draw, ClusterContainer* subMesh);


void loadSDFMeshAlphaTested(ThreadSystem* threadSystem, const char* fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData,
	BakedSDFVolumeInstances& sdfMeshInstances, 
	GenerateVolumeDataFromFileFunc generateVolumeDataFromFileFunc);

void loadSDFMesh(ThreadSystem* threadSystem, const char* fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData, BakedSDFVolumeInstances& sdfMeshInstances,
	GenerateVolumeDataFromFileFunc generateVolumeDataFromFileFunc);


void addClusterToBatchChunk(
	const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex,
	FilterBatchChunk* batchChunk, FilterBatchData* batches);

#endif
