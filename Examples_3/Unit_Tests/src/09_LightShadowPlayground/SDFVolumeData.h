#pragma once

#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "AABBox.h"
#include "Triangle.h"




class SDFVolumeData;

struct SDFMesh;
struct SDFMeshInstance;
struct ThreadSystem;


class SDFVolumeTextureNode
{
public:
	SDFVolumeTextureNode(SDFVolumeData* sdfVolumeData, SDFMesh* mainMesh, SDFMeshInstance* meshInstance);


	SDFVolumeData* mSDFVolumeData;
	SDFMesh* mMainMesh;
	SDFMeshInstance* mMeshInstance;

	//the coordinate of this node inside the volume texture atlases
	//not in texel space
	ivec3 mAtlasAllocationCoord;
	bool mHasBeenAdded;
};

struct BVHMeshInstance;


class SDFVolumeData
{
public:
	typedef eastl::vector<vec3> SampleDirectionsList;
	typedef eastl::vector<float> SDFVolumeList;
	typedef eastl::vector<Triangle> TriangeList;
public:
	SDFVolumeData(SDFMesh* mainMesh, SDFMeshInstance* meshInstance);
	SDFVolumeData();

	~SDFVolumeData();

	static eastl::string GetFullDirBakedFileName(const eastl::string& fileName);
	static eastl::string GetBakedFileName(const eastl::string& fileName);

	static bool GenerateVolumeData(SDFVolumeData** outVolumeData, 
		const eastl::string& toCheckFullFileName, const eastl::string& fileName, float twoSidedWorldSpaceBias);

	static void GenerateVolumeData(ThreadSystem* threadSystem,  SDFMesh* mainMesh, SDFMeshInstance* subMesh, 
		float sdfResolutionScale, bool generateAsIfTwoSided, SDFVolumeData** outVolumeData, 
		const eastl::string& cacheFileNameToCheck, float twoSidedWorldSpaceBias = 0.4f, 
		const ivec3& specialMaxVoxelValue = ivec3(0));

	//
	SDFVolumeList mSDFVolumeList;
	//
	//Size of the distance volume
	ivec3 mSDFVolumeSize;
	//
	//Local Space of the Bounding Box volume
	AABBox mLocalBoundingBox;

	//stores the min & the maximum distances found in the volume
	//in the space of the world voxel volume
	//x stores the minimum while y stores the maximum
	vec2 mDistMinMax;
	//
	bool mIsTwoSided;
	//
	float mTwoSidedWorldSpaceBias;

	SDFVolumeTextureNode mSDFVolumeTextureNode;

	eastl::string mSubMeshName;
};