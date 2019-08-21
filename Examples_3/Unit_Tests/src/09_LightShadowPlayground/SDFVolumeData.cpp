#include "SDFVolumeData.h"
#include "Intersection.h"
#include "Ray.h"
#include "Geometry.h"
#include "BVHTree.h"

#include "../../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../../Common_3/OS/Core/ThreadSystem.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"

#include <sys/stat.h>


#ifdef _WIN32
#include <io.h> 
#define access    _access_s
#else
#include <unistd.h>
#endif


SDFVolumeTextureNode::SDFVolumeTextureNode(SDFVolumeData* sdfVolumeData, SDFMesh* mainMesh, SDFMeshInstance* meshInstance)
	:mSDFVolumeData(sdfVolumeData),
	mAtlasAllocationCoord(-1, -1, -1),
	mMainMesh(mainMesh),
	mMeshInstance(meshInstance),
	mHasBeenAdded(false)
{
}


SDFVolumeData::SDFVolumeData(SDFMesh* mainMesh, SDFMeshInstance* meshInstance)
	:mSDFVolumeSize(0),
	mLocalBoundingBox(),
	mDistMinMax(FLT_MAX, FLT_MIN),
	mIsTwoSided(false),
	mSDFVolumeTextureNode(this, mainMesh, meshInstance),
	mTwoSidedWorldSpaceBias(0.f),
	mSubMeshName("")
{
}

SDFVolumeData::SDFVolumeData()
:mSDFVolumeSize(0),
mLocalBoundingBox(),
mDistMinMax(FLT_MAX, FLT_MIN),
mIsTwoSided(false),
mSDFVolumeTextureNode(this, NULL, NULL),
mTwoSidedWorldSpaceBias(0.f),
mSubMeshName("")
{
}

SDFVolumeData::~SDFVolumeData()
{
}

void GenerateSampleDirections(int32_t thetaSteps, int32_t phiSteps, SDFVolumeData::SampleDirectionsList& outDirectionsList, int32_t finalThetaModifier = 1)
{

	for (int32_t theta = 0; theta < thetaSteps; ++theta)
	{
		for (int32_t phi = 0; phi < phiSteps; ++phi)
		{
			float random1 = Helper::GenerateRandomFloat();
			float random2 = Helper::GenerateRandomFloat();

			float thetaFrac = (theta + random1) / (float)thetaSteps;
			float phiFrac = (phi + random2) / (float)phiSteps;

			float rVal = sqrt(1.0f - thetaFrac * thetaFrac);

			const float finalPhi = 2.0f * (float)PI * phiFrac;
			
			outDirectionsList.push_back(vec3(cos(finalPhi) * rVal,
				sin(finalPhi) * rVal, thetaFrac * finalThetaModifier));
		}
	}
}



struct CalculateMeshSDFTask
{
	const SDFVolumeData::SampleDirectionsList* mDirectionsList; 
	const SDFVolumeData::TriangeList* mMeshTrianglesList;
	const AABBox* mSDFVolumeBounds;
	const ivec3*  mSDFVolumeDimension;
	int32_t mZIndex; 
	float mSDFVolumeMaxDist;
	BVHTree* mBVHTree;
	SDFVolumeData::SDFVolumeList* mSDFVolumeList;
	bool mIsTwoSided;
};


void DoCalculateMeshSDFTask(void* dataPtr, uintptr_t index)
{
	CalculateMeshSDFTask* task = (CalculateMeshSDFTask*)(dataPtr);

	const AABBox& sdfVolumeBounds = *task->mSDFVolumeBounds;
	const ivec3& sdfVolumeDimension = *task->mSDFVolumeDimension;
	int32_t zIndex = task->mZIndex;
	float sdfVolumeMaxDist = task->mSDFVolumeMaxDist;

	const SDFVolumeData::SampleDirectionsList& directionsList = *task->mDirectionsList;
	//const SDFVolumeData::TriangeList& meshTrianglesList = *task->mMeshTrianglesList;

	SDFVolumeData::SDFVolumeList& sdfVolumeList = *task->mSDFVolumeList;

	BVHTree* bvhTree = task->mBVHTree;
	   	 
	vec3 sdfVoxelSize
	(
		Helper::Piecewise_Division
		(
			sdfVolumeBounds.GetSize(),
			vec3((float)sdfVolumeDimension.getX(), (float)sdfVolumeDimension.getY(), (float)sdfVolumeDimension.getZ())
		)
	);

	float voxelDiameterSquared = dot(sdfVoxelSize, sdfVoxelSize);

	for (int32_t yIndex = 0; yIndex < sdfVolumeDimension.getY(); ++yIndex)
	{
		for (int32_t xIndex = 0; xIndex < sdfVolumeDimension.getX(); ++xIndex)
		{
			vec3 voxelPos =
				Helper::Piecewise_Prod
				(
					vec3((float)(xIndex)+0.5f, float(yIndex) + 0.5f, float(zIndex) + 0.5f)
					, sdfVoxelSize
				)
				+ sdfVolumeBounds.m_min;

			int32 outIndex = (zIndex * sdfVolumeDimension.getY() *
				sdfVolumeDimension.getX() + yIndex * sdfVolumeDimension.getX() + xIndex);


			float minDistance = sdfVolumeMaxDist;
			int32 hit = 0;
			int32 hitBack = 0;

			for (int32_t sampleIndex = 0; sampleIndex < directionsList.size(); ++sampleIndex)
			{
				vec3 rayDir = directionsList[sampleIndex];
				vec3 endPos = voxelPos + rayDir * sdfVolumeMaxDist;

				Ray newRay(voxelPos, rayDir);

				Intersection intersectData;
				sdfVolumeBounds.Intersect(newRay, intersectData);

				//if we pass the cheap bbox testing
				if (intersectData.mIsIntersected)
				{
					Intersection meshTriangleIntersect;
					//optimized version
					bvhTree->IntersectRay(newRay, meshTriangleIntersect);
					if (meshTriangleIntersect.mIsIntersected)
					{
						++hit;
						const vec3& hitNormal = meshTriangleIntersect.mHittedNormal;
						if (dot(rayDir, hitNormal) > 0 && !task->mIsTwoSided)
						{
							++hitBack;
						}

						const vec3 finalEndPos = newRay.Eval(
							meshTriangleIntersect.mIntersection_TVal);

						float newDist = length(newRay.mStartPos - finalEndPos);

						if (newDist < minDistance)
						{
							minDistance = newDist;
						}
					}

				}

			}

			//


			float unsignedDist = minDistance;

			//if 50% hit backface, we consider the voxel sdf value to be inside the mesh
			minDistance *= (hit == 0 || hitBack < (directionsList.size() * 0.5f)) ? 1 : -1;

			//if we are very close to the surface and 95% of our rays hit backfaces, the sdf value
			//is inside the mesh
			if ((unsignedDist * unsignedDist) < voxelDiameterSquared && hitBack > 0.95f * hit)
			{
				minDistance = -unsignedDist;
			}

			minDistance = fmin(minDistance, sdfVolumeMaxDist);
			float volumeSpaceDist = minDistance / Helper::GetMaxElem(sdfVolumeBounds.GetExtent());

			sdfVolumeList[outIndex] = volumeSpaceDist;
		}
	}

}



bool doesFileExist(const eastl::string& name)
{
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}


bool FileExists(const eastl::string &Filename)
{
	return access(Filename.c_str(), 0) == 0;
}


eastl::string SDFVolumeData::GetFullDirBakedFileName(const eastl::string& fileName)
{
	return gGeneratedSDFBinaryDir + GetBakedFileName(fileName);
}

eastl::string SDFVolumeData::GetBakedFileName(const eastl::string& fileName)
{
	eastl::string newCompleteCacheFileName = "Baked_" + fileName + ".bin";
	return newCompleteCacheFileName;
}

bool SDFVolumeData::GenerateVolumeData(SDFVolumeData** outVolumeDataPP,
	const eastl::string& toCheckFullFileName, const eastl::string& fileName, float twoSidedWorldSpaceBias)
{
	if (!FileExists(toCheckFullFileName))
	{
		return false;
	}

	*outVolumeDataPP = conf_new(SDFVolumeData);
	SDFVolumeData& outVolumeData = **outVolumeDataPP;
	
	File newBakedFile;
	newBakedFile.Open(fileName, FM_ReadBinary, FSR_OtherFiles);
	outVolumeData.mSDFVolumeSize.setX(newBakedFile.ReadInt());
	outVolumeData.mSDFVolumeSize.setY(newBakedFile.ReadInt());
	outVolumeData.mSDFVolumeSize.setZ(newBakedFile.ReadInt());

	uint32_t finalSDFVolumeDataCount = outVolumeData.mSDFVolumeSize.getX() * outVolumeData.mSDFVolumeSize.getY()
		* outVolumeData.mSDFVolumeSize.getZ();

	outVolumeData.mSDFVolumeList.resize	(finalSDFVolumeDataCount);

	newBakedFile.Read(&outVolumeData.mSDFVolumeList[0], (unsigned int)(finalSDFVolumeDataCount * sizeof(float)));
	outVolumeData.mLocalBoundingBox.m_min = f3Tov3(newBakedFile.ReadVector3());
	outVolumeData.mLocalBoundingBox.m_max = f3Tov3(newBakedFile.ReadVector3());
	outVolumeData.mIsTwoSided = newBakedFile.ReadBool();
	//outVolumeData.mTwoSidedWorldSpaceBias = newBakedFile.ReadFloat();
	outVolumeData.mTwoSidedWorldSpaceBias = twoSidedWorldSpaceBias;
	/*
	only uses the minimum & maximum of SDF if we ever want to quantized the SDF data
	for (int32 index = 0; index < outVolumeData.mSDFVolumeList.size(); ++index)
	{
		const float volumeSpaceDist = outVolumeData.mSDFVolumeList[index];
		outVolumeData.mDistMinMax.setX(fmin(volumeSpaceDist, outVolumeData.mDistMinMax.getX()));
		outVolumeData.mDistMinMax.setY(fmax(volumeSpaceDist, outVolumeData.mDistMinMax.getY()));
	}*/
	newBakedFile.Close();

	LOGF(LogLevel::eINFO, "SDF binary data for %s found & parsed", toCheckFullFileName.c_str());
	return true;
}

void SDFVolumeData::GenerateVolumeData(ThreadSystem* threadSystem, SDFMesh* mainMesh, SDFMeshInstance* subMesh,
	float sdfResolutionScale, bool generateAsIfTwoSided, SDFVolumeData** outVolumeDataPP,
	const eastl::string& cacheFileNameToCheck, float twoSidedWorldSpaceBias, const ivec3& specialMaxVoxelValue)
{
	eastl::string newCompleteCacheFileName = SDFVolumeData::GetBakedFileName(cacheFileNameToCheck);
	eastl::string toCheckCacheFileName = SDFVolumeData::GetFullDirBakedFileName(cacheFileNameToCheck);
	
	if (GenerateVolumeData(outVolumeDataPP, toCheckCacheFileName, newCompleteCacheFileName, twoSidedWorldSpaceBias))
	{
		return;
	}

	LOGF(LogLevel::eINFO, "Generating SDF binary data for %s", cacheFileNameToCheck.c_str());

	*outVolumeDataPP = conf_new(SDFVolumeData, mainMesh, subMesh);

	SDFVolumeData& outVolumeData = **outVolumeDataPP;

	//for now assume all triangles are valid and useable
	ivec3 maxNumVoxelsOneDimension;
	ivec3 minNumVoxelsOneDimension;

	if (specialMaxVoxelValue.getX() == 0.f || specialMaxVoxelValue.getY() == 0.f || specialMaxVoxelValue.getZ() == 0.f)
	{
		maxNumVoxelsOneDimension = ivec3(
			SDF_MAX_VOXEL_ONE_DIMENSION_X,
			SDF_MAX_VOXEL_ONE_DIMENSION_Y,
			SDF_MAX_VOXEL_ONE_DIMENSION_Z);
		minNumVoxelsOneDimension = ivec3(
			SDF_MIN_VOXEL_ONE_DIMENSION_X,
			SDF_MIN_VOXEL_ONE_DIMENSION_Y,
			SDF_MIN_VOXEL_ONE_DIMENSION_Z);
	}
	else
	{
		maxNumVoxelsOneDimension = specialMaxVoxelValue;
		minNumVoxelsOneDimension = specialMaxVoxelValue;
	}


	const float voxelDensity = 1.0f;

	const float numVoxelPerLocalSpaceUnit = voxelDensity * sdfResolutionScale;

	AABBox subMeshBBox;
	subMeshBBox = subMesh->mLocalBoundingBox;
	
	

	float maxExtentSize = Helper::GetMaxElem(subMeshBBox.GetExtent());


	vec3 minNewExtent(0.2f* maxExtentSize);
	vec3 dynamicNewExtent(Helper::Piecewise_Division(4.f * subMeshBBox.GetExtent(), Helper::ivec3ToVec3f(minNumVoxelsOneDimension)));

	vec3 finalNewExtent = subMeshBBox.GetExtent() + Helper::Max_Vec3(minNewExtent, dynamicNewExtent);

	AABBox newSDFVolumeBound(subMeshBBox.GetCenter() - finalNewExtent,
		subMeshBBox.GetCenter() + finalNewExtent);

	float newSDFVolumeMaxDistance =
		length(newSDFVolumeBound.GetExtent());

	vec3 dynamicDimension = Helper::Piecewise_Prod(
		newSDFVolumeBound.GetSize(), vec3(numVoxelPerLocalSpaceUnit));

	ivec3 finalSDFVolumeDimension
	(
		clamp((int32_t)(dynamicDimension.getX()), minNumVoxelsOneDimension.getX(), maxNumVoxelsOneDimension.getX()),
		clamp((int32_t)(dynamicDimension.getY()), minNumVoxelsOneDimension.getY(), maxNumVoxelsOneDimension.getY()),
		clamp((int32_t)(dynamicDimension.getZ()), minNumVoxelsOneDimension.getZ(), maxNumVoxelsOneDimension.getZ())
	);

	unsigned int finalSDFVolumeDataCount = finalSDFVolumeDimension.getX() *
		finalSDFVolumeDimension.getY() *
		finalSDFVolumeDimension.getZ();

	outVolumeData.mSDFVolumeList.resize
	(
		finalSDFVolumeDimension.getX() *
		finalSDFVolumeDimension.getY() *
		finalSDFVolumeDimension.getZ()
	);

	BVHTree bvhTree;
	bvhTree.AddMeshInstanceToBBOX(mat4::identity(), mainMesh, subMesh);
	bvhTree.mRootNode = bvhTree.CreateBVHNodeSHA(0, (int32_t)bvhTree.mBBOXDataList.size() - 1, FLT_MAX);


	// here we begin our stratified sampling calculation
	const uint32_t numVoxelDistanceSample = SDF_STRATIFIED_DIRECTIONS_NUM;

	SDFVolumeData::SampleDirectionsList sampleDirectionsList;

	int32_t thetaStep = (int32_t)floor((sqrt((float)numVoxelDistanceSample / (PI * 2.f))));
	int32 phiStep = (int32_t)floor((float)thetaStep * PI);


	sampleDirectionsList.reserve(thetaStep * phiStep * 2);

	GenerateSampleDirections(thetaStep, phiStep, sampleDirectionsList);

	SDFVolumeData::SampleDirectionsList otherHemisphereSampleDirectionList;
	GenerateSampleDirections(thetaStep, phiStep, sampleDirectionsList, -1);


	CalculateMeshSDFTask calculateMeshSDFTask = {};
	calculateMeshSDFTask.mDirectionsList = &sampleDirectionsList;
	//	calculateMeshSDFTask.mMeshTrianglesList = &triangleList;
	calculateMeshSDFTask.mSDFVolumeBounds = &newSDFVolumeBound;
	calculateMeshSDFTask.mSDFVolumeDimension = &finalSDFVolumeDimension;
	calculateMeshSDFTask.mSDFVolumeMaxDist = newSDFVolumeMaxDistance;
	calculateMeshSDFTask.mSDFVolumeList = &outVolumeData.mSDFVolumeList;
	calculateMeshSDFTask.mBVHTree = &bvhTree;
	calculateMeshSDFTask.mIsTwoSided = generateAsIfTwoSided;


	eastl::vector<CalculateMeshSDFTask*> taskList;
	taskList.reserve(finalSDFVolumeDimension.getZ());

	for (int32_t zIndex = 0; zIndex < finalSDFVolumeDimension.getZ(); ++zIndex)
	{
		CalculateMeshSDFTask* individualTask = (CalculateMeshSDFTask*)conf_malloc(sizeof(CalculateMeshSDFTask));

		*individualTask = calculateMeshSDFTask;
		individualTask->mZIndex = zIndex;

		taskList.push_back(individualTask);
		DoCalculateMeshSDFTask(individualTask, 0);
		//addThreadSystemTask(threadSystem, DoCalculateMeshSDFTask, individualTask, 0);
	}

	//waitThreadSystemIdle(threadSystem);

	for (uint32_t i = 0; i < taskList.size(); ++i)
	{
		conf_free(taskList[i]);
	}

	BVHTree::DeleteBVHTree(bvhTree.mRootNode);

	File portDataFile;
	portDataFile.Open(newCompleteCacheFileName, FM_WriteBinary, FSR_OtherFiles);
	portDataFile.WriteInt(finalSDFVolumeDimension.getX());
	portDataFile.WriteInt(finalSDFVolumeDimension.getY());
	portDataFile.WriteInt(finalSDFVolumeDimension.getZ());
	portDataFile.Write(&outVolumeData.mSDFVolumeList[0], (unsigned int)(finalSDFVolumeDataCount * sizeof(float)));
	portDataFile.WriteVector3(v3ToF3(newSDFVolumeBound.m_min));
	portDataFile.WriteVector3(v3ToF3(newSDFVolumeBound.m_max));
	portDataFile.WriteBool(generateAsIfTwoSided);
	//portDataFile.WriteFloat(twoSidedWorldSpaceBias);
	portDataFile.Close();



	float minVolumeDist = 1.0f;
	float maxVolumeDist = -1.0f;

	//we can probably move the calculation of the minimum & maximum distance of the SDF value
	//into the CalculateMeshSDFValue function
	for (int32 index = 0; index < outVolumeData.mSDFVolumeList.size(); ++index)
	{
		const float volumeSpaceDist = outVolumeData.mSDFVolumeList[index];
		minVolumeDist = fmin(volumeSpaceDist, minVolumeDist);
		maxVolumeDist = fmax(volumeSpaceDist, maxVolumeDist);
	}

	//TODO, not every mesh is going to be closed
	//do the check sometime in the future
	outVolumeData.mIsTwoSided = generateAsIfTwoSided;
	outVolumeData.mSDFVolumeSize = finalSDFVolumeDimension;
	outVolumeData.mLocalBoundingBox = newSDFVolumeBound;
	outVolumeData.mDistMinMax = vec2(minVolumeDist, maxVolumeDist);
	outVolumeData.mTwoSidedWorldSpaceBias = twoSidedWorldSpaceBias;


	LOGF(LogLevel::eINFO, "Generated SDF binary data for %s successfully!", cacheFileNameToCheck.c_str());
}
