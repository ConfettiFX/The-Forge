#pragma once
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "Ray.h"
#include "Intersection.h"
#include "Triangle.h"
#include "Helper.h"

#include <float.h>


struct SDFMeshInstance;
struct SDFMesh;

struct Scene;
struct MeshIn;

struct BVHAABBox
{
	vec3  MinBounds;
	vec3  MaxBounds;
	vec3  Center;
	Triangle mTriangle;

	int   InstanceID;
	float SurfaceAreaLeft;
	float SurfaceAreaRight;

	BVHAABBox()
	{
		MinBounds = vec3(FLT_MAX);
		MaxBounds = vec3(-FLT_MAX);
		InstanceID = 0;
	}

	void Expand(vec3& point)
	{
		MinBounds = Helper::Min_Vec3(MinBounds, point);
		MaxBounds = Helper::Max_Vec3 (MaxBounds, point);

		Center = 0.5f * (MaxBounds + MinBounds);
	}

	void Expand(BVHAABBox& aabox)
	{
		Expand(aabox.MinBounds);
		Expand(aabox.MaxBounds);
	}
};

struct BVHNodeBBox
{
	float4 MinBounds;    // OffsetToNextNode in w component
	float4 MaxBounds;
};

struct BVHLeafBBox
{
	float4 Vertex0;    // OffsetToNextNode in w component
	float4 Vertex1MinusVertex0;
	float4 Vertex2MinusVertex0;
};

struct BVHNode
{
	float    SplitCost;
	BVHAABBox   BoundingBox;
	BVHNode* Left;
	BVHNode* Right;
};


struct BVHMeshInstance;


class BVHTree
{
public:
	BVHTree() :mRootNode(NULL),mBVHNodeCount(0), mTransitionNodeCount(0)
	{
		mBBOXDataList.reserve(1000000);
	}



	void IntersectRay(const Ray& ray, Intersection& outIntersection);
	void AddMeshInstanceToBBOX(const mat4& meshWorldMat, SDFMesh* mesh,
		SDFMeshInstance* meshInst);

	BVHNode* CreateBVHNodeSHA(int32_t begin, int32_t end, float parentSplitCost);

	void FindBestSplit(int begin, int end, int& split, int& axis, float& splitCost);
	void CalculateBounds(int32_t begin, int32_t end, vec3& outMinBounds, vec3& outMaxBounds);

	void SortAlongAxis(int32_t begin, int32_t end, int32_t axis);


	static float CalculateSurfaceArea(const BVHAABBox& bbox);
	static void DeleteBVHTree(BVHNode* node);



	

	eastl::vector<BVHAABBox> mBBOXDataList;
	BVHNode* mRootNode;

	uint32_t mBVHNodeCount;
	uint32_t mTransitionNodeCount;
private:
	
	void Aux_IntersectRay(BVHNode* rootNode, BVHNode* node, 
		const Ray& ray, Intersection& outIntersection);
};