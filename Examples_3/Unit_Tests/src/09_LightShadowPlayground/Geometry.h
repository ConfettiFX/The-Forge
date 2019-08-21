#ifndef Geometry_h
#define Geometry_h


#include "../../../../Common_3/OS/Math/MathTypes.h"
//EA stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"


#include "AABBox.h"
#include "SDFVolumeData.h"
#include "CPUImage.h"

namespace eastl
{
	template <>
	struct has_equality<vec3> : eastl::false_type {};
}




//#define NO_HLSL_DEFINITIONS 1
//#include "Shader_Defs.h"
#if defined(METAL)
#include "Shaders/Metal/Shader_Defs.h"
#elif defined(DIRECT3D12) || defined(_DURANGO)
#define NO_HLSL_DEFINITIONS
#include "Shaders/D3D12/Shader_Defs.h"
#elif defined(VULKAN)
#define NO_GLSL_DEFINITIONS
#include "Shaders/Vulkan/Shader_Defs.h"
#endif

struct Buffer;
struct ThreadSystem;

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

typedef struct AABoundingBox
{
	float4 Center;     // Center of the box.
	float4 Extents;    // Distance from the center to each side.

	float4 minPt;
	float4 maxPt;

	float4 corners[8];
} AABoundingBox;


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
	FilterBatchData*	batches;
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

typedef struct SceneVertexTexCoord
{
#if defined(METAL) || defined(__linux__)
	float u, v;    // texture coords
#else
	uint32_t texCoord;
#endif
} SceneVertexTexCoord;

typedef struct SceneVertexNormal
{
#if defined(METAL) || defined(__linux__)
	float nx, ny, nz;    // normals
#else



	uint32_t normal;
#endif
} SceneVertexNormal;

typedef struct SceneVertexTangent
{
#if defined(METAL) || defined(__linux__)
	float tx, ty, tz;    // tangents
#else
	uint32_t tangent;
#endif
} SceneVertexTangent;

typedef struct MeshIn
{
#if 0 //defined(METAL)
	uint32_t startVertex;
	uint32_t triangleCount;
#else
	uint32_t startIndex;
	uint32_t indexCount;
#endif
	uint32_t        vertexCount;
	float3          minBBox, maxBBox;
	uint32_t        clusterCount;
	ClusterCompact* clusterCompacts;
	Cluster*        clusters;
	uint32_t        materialId;

	AABoundingBox AABB;

	Buffer* pVertexBuffer;
	Buffer* pIndexBuffer;

} MeshIn;


typedef struct Material
{
	bool twoSided;
	bool alphaTested;
} Material;

typedef struct Scene
{
	uint32_t                           numMeshes;
	uint32_t                           numMaterials;
	uint32_t                           totalTriangles;
	uint32_t                           totalVertices;
	MeshIn*                            meshes;
	Material*                          materials;
	eastl::vector<SceneVertexPos>      positions;
	eastl::vector<SceneVertexTexCoord> texCoords;
	eastl::vector<SceneVertexNormal>   normals;
	eastl::vector<SceneVertexTangent>  tangents;
	char**                             textures;
	char**                             normalMaps;
	char**                             specularMaps;

	eastl::vector<uint32_t> indices;

	Buffer*                   mPVertexBuffer = NULL;
	Buffer*                   mPIndexBuffer = NULL;

	Buffer* m_pIndirectPosBuffer = NULL;
	Buffer* m_pIndirectTexCoordBuffer = NULL;
	Buffer* m_pIndirectNormalBuffer = NULL;
	Buffer* m_pIndirectTangentBuffer = NULL;
	Buffer* m_pIndirectIndexBuffer = NULL;
} Scene;




typedef eastl::vector<eastl::pair<eastl::string, eastl::string> > AlphaTestedMaterialMaps;
typedef eastl::unordered_map<eastl::string, CPUImage*> AlphaTestedImageMaps;

struct SDFCustomSubMeshData;

struct SDFCustomSubMeshData
{
	AssimpImporter::Mesh* m_PSubMesh;
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


struct SDFMeshInstance;

struct SDFMeshInstance
{
	uint32_t mMaterialId;
	uint32_t mIndexCount;
	uint32_t mStartIndex;
	uint32_t mStartVertex;

	AABBox mLocalBoundingBox = AABBox(vec3(FLT_MAX), vec3(FLT_MIN));
	SDFMesh* mMainMesh = NULL;

	bool mHasGeneratedSDFVolumeData = false;

	bool mIsAlphaTested = false;
	CPUImage* mTextureImgRef = NULL;
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


	uint32_t mNumMaterials = 0;
	uint32_t mNumMeshes = 0;
	uint32_t mTotalVertices = 0;
	uint32_t mTotalTriangles = 0;
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


void destroyClusters(MeshIn* pMesh);
Scene* loadScene(const char* fileName, float scale, float offsetX, float offsetY, float offsetZ);
	
void   removeScene(Scene* scene);

void   createAABB(const Scene* scene, MeshIn* subMesh);
void   createClusters(bool twoSided, const Scene* scene, MeshIn* subMesh);


void generateMissingSDF(ThreadSystem* threadSystem, SDFMesh* sdfMesh, BakedSDFVolumeInstances& sdfMeshInstances);


void loadSDFMeshAlphaTested(ThreadSystem* threadSystem, const eastl::string& fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData, AlphaTestedImageMaps& alphaTestedImageMaps, AlphaTestedMaterialMaps& alphaTestedMaterialMaps,
	BakedSDFVolumeInstances& sdfMeshInstances);

void loadSDFMesh(ThreadSystem* threadSystem, const eastl::string& fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData, BakedSDFVolumeInstances& sdfMeshInstances);


void addClusterToBatchChunk(
	const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex,
	FilterBatchChunk* batchChunk);


void initAlphaTestedImageMaps(AlphaTestedImageMaps& imageMaps);
void initAlphaTestedMaterialTexturesMaps(AlphaTestedMaterialMaps& alphaTestedMaterialMaps);

#endif