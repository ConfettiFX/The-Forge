#include "BVHTree.h"
#include "Geometry.h"

void BVHTree::AddMeshInstanceToBBOX(const mat4& meshWorldMat, 
	SDFMesh* mesh, SDFMeshInstance* meshInst)
{
	if (!meshInst->mStackInstances.empty())
	{
		for (uint32_t stackIndex = 0; stackIndex <
			meshInst->mStackInstances.size(); ++stackIndex)
		{
			SDFMeshInstance* stackInstance = &meshInst->mStackInstances[stackIndex];

			int32_t lastIndex = stackInstance->mIndexCount + stackInstance->mStartIndex;
			for (int32_t index = stackInstance->mStartIndex; index < lastIndex; index += 3)
			{
				int32_t index0 = mesh->mIndices[index + 0];
				int32_t index1 = mesh->mIndices[index + 1];
				int32_t index2 = mesh->mIndices[index + 2];

				vec3 v0 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index0]), 1.f)).getXYZ();
				vec3 v1 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index1]), 1.f)).getXYZ();
				vec3 v2 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index2]), 1.f)).getXYZ();

				//TODO: multiply normal by world mat
				vec3 n0 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index0];
				vec3 n1 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index1];
				vec3 n2 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index2];


				vec2 UV0 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index0];
				vec2 UV1 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index1];
				vec2 UV2 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index2];

				mBBOXDataList.push_back(BVHAABBox());
				BVHAABBox& bvhAABBOX = mBBOXDataList.back();

				bvhAABBOX.mTriangle.Init(v0, v1, v2, n0, n1, n2, UV0, UV1, UV2);
				bvhAABBOX.mTriangle.m_pMeshInstance = stackInstance;
				bvhAABBOX.Expand(v0);
				bvhAABBOX.Expand(v1);
				bvhAABBOX.Expand(v2);

				bvhAABBOX.InstanceID = 0;
			}
		}
	}
	else
	{
		int32_t lastIndex = meshInst->mIndexCount + meshInst->mStartIndex;
		for (int32_t index = meshInst->mStartIndex; index < lastIndex; index += 3)
		{
			int32_t index0 = mesh->mIndices[index + 0];
			int32_t index1 = mesh->mIndices[index + 1];
			int32_t index2 = mesh->mIndices[index + 2];

			vec3 v0 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index0]), 1.f)).getXYZ();
			vec3 v1 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index1]), 1.f)).getXYZ();
			vec3 v2 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index2]), 1.f)).getXYZ();

			//TODO: multiply normal by world mat
			vec3 n0 = mesh->mUncompressedNormals[meshInst->mStartVertex + index0];
			vec3 n1 = mesh->mUncompressedNormals[meshInst->mStartVertex + index1];
			vec3 n2 = mesh->mUncompressedNormals[meshInst->mStartVertex + index2];


			vec2 UV0 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index0];
			vec2 UV1 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index1];
			vec2 UV2 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index2];

			mBBOXDataList.push_back(BVHAABBox());
			BVHAABBox& bvhAABBOX = mBBOXDataList.back();

			bvhAABBOX.mTriangle.Init(v0, v1, v2, n0, n1, n2, UV0, UV1, UV2);
			bvhAABBOX.mTriangle.m_pMeshInstance = meshInst;
			bvhAABBOX.Expand(v0);
			bvhAABBOX.Expand(v1);
			bvhAABBOX.Expand(v2);

			bvhAABBOX.InstanceID = 0;
		}
	}
}



void BVHTree::CalculateBounds(int32_t begin, int32_t end, vec3& outMinBounds, vec3& outMaxBounds)
{
	outMinBounds = vec3(FLT_MAX);
	outMaxBounds = vec3(-FLT_MAX);

	for (int32_t i = begin; i <= end; ++i)
	{
		outMinBounds = Helper::Min_Vec3(mBBOXDataList[i].MinBounds, outMinBounds);
		outMaxBounds = Helper::Max_Vec3(mBBOXDataList[i].MaxBounds, outMaxBounds);
	}
}

BVHNode* BVHTree::CreateBVHNodeSHA(int32_t begin, int32_t end, float parentSplitCost)
{
	int32_t count = end - begin + 1;

	vec3 minBounds;
	vec3 maxBounds;

	CalculateBounds(begin, end, minBounds, maxBounds);

	BVHNode* node = (BVHNode*)conf_placement_new<BVHNode>(conf_calloc(1, sizeof(BVHNode)));


	++mBVHNodeCount;

	node->BoundingBox.Expand(minBounds);
	node->BoundingBox.Expand(maxBounds);

	if (count == 1)
	{
		//this is a leaf node
		node->Left = NULL;
		node->Right = NULL;

		node->BoundingBox.InstanceID = mBBOXDataList[begin].InstanceID;


		node->BoundingBox.mTriangle = mBBOXDataList[begin].mTriangle;
		//node->BoundingBox.Vertex0 = mBBOXDataList[begin].Vertex0;
		//node->BoundingBox.Vertex1 = mBBOXDataList[begin].Vertex1;
		//node->BoundingBox.Vertex2 = mBBOXDataList[begin].Vertex2;
	}
	else
	{
		++mTransitionNodeCount;

		int32_t   split;
		int32_t   axis;
		float splitCost;

		//find the best axis to sort along and where the split should be according to SAH
		FindBestSplit(begin, end, split, axis, splitCost);

		//sort along that axis
		SortAlongAxis(begin, end, axis);

		//create the two branches
		node->Left = CreateBVHNodeSHA(begin, split - 1, splitCost);
		node->Right = CreateBVHNodeSHA(split, end, splitCost);

		//Access the child with the largest probability of collision first.
		float surfaceAreaLeft = CalculateSurfaceArea(node->Left->BoundingBox);
		float surfaceAreaRight = CalculateSurfaceArea(node->Right->BoundingBox);

		if (surfaceAreaRight > surfaceAreaLeft)
		{
			BVHNode* temp = node->Right;
			node->Right = node->Left;
			node->Left = temp;
		}

		//node->BoundingBox.Vertex0 = vec3(0.0f);
		//node->BoundingBox.Vertex1 = vec3(0.0f);
		//node->BoundingBox.Vertex2 = vec3(0.0f);

		//this is an intermediate Node
		node->BoundingBox.InstanceID = -1;
	}

	return node;
}

void BVHTree::SortAlongAxis(int32_t begin, int32_t end, int32_t axis)
{
	BVHAABBox* data = mBBOXDataList.data() + begin;
	int     count = end - begin + 1;

	if (axis == 0)
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[0];
		float midPointB = arg2->Center[0];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
	else if (axis == 1)
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[1];
		float midPointB = arg2->Center[1];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
	else
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[2];
		float midPointB = arg2->Center[2];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
}

float  BVHTree::CalculateSurfaceArea(const BVHAABBox& bbox)
{
	vec3 extents = bbox.MaxBounds - bbox.MinBounds;
	return (extents[0] * extents[1] + extents[1] * extents[2] + extents[2] * extents[0]) * 2.f;
}


void BVHTree::FindBestSplit(int begin, int end, int& split, int& axis, float& splitCost)
{
	int count = end - begin + 1;
	int bestSplit = begin;
	int globalBestSplit = begin;
	splitCost = FLT_MAX;

	split = begin;
	axis = 0;

	for (int i = 0; i < 3; i++)
	{
		SortAlongAxis(begin, end, i);

		BVHAABBox boundsLeft;
		BVHAABBox boundsRight;

		for (int indexLeft = 0; indexLeft < count; ++indexLeft)
		{
			int indexRight = count - indexLeft - 1;

			boundsLeft.Expand(mBBOXDataList[begin + indexLeft].MinBounds);
			boundsLeft.Expand(mBBOXDataList[begin + indexLeft].MaxBounds);

			boundsRight.Expand(mBBOXDataList[begin + indexRight].MinBounds);
			boundsRight.Expand(mBBOXDataList[begin + indexRight].MaxBounds);

			float surfaceAreaLeft = CalculateSurfaceArea(boundsLeft);
			float surfaceAreaRight = CalculateSurfaceArea(boundsRight);

			mBBOXDataList[begin + indexLeft].SurfaceAreaLeft = surfaceAreaLeft;
			mBBOXDataList[begin + indexRight].SurfaceAreaRight = surfaceAreaRight;
		}

		float bestCost = FLT_MAX;
		for (int mid = begin + 1; mid <= end; ++mid)
		{
			float surfaceAreaLeft = mBBOXDataList[mid - 1].SurfaceAreaLeft;
			float surfaceAreaRight = mBBOXDataList[mid].SurfaceAreaRight;

			int countLeft = mid - begin;
			int countRight = end - mid;

			float costLeft = surfaceAreaLeft * (float)countLeft;
			float costRight = surfaceAreaRight * (float)countRight;

			float cost = costLeft + costRight;
			if (cost < bestCost)
			{
				bestSplit = mid;
				bestCost = cost;
			}
		}

		if (bestCost < splitCost)
		{
			split = bestSplit;
			splitCost = bestCost;
			axis = i;
		}
	}
}


void BVHTree::DeleteBVHTree(BVHNode* node)
{
	if (node)
	{
		if (node->Left)
		{
			DeleteBVHTree(node->Left);
		}

		if (node->Right)
		{
			DeleteBVHTree(node->Right);
		}

		node->~BVHNode();
		conf_free(node);
	}
}



void BVHTree::IntersectRay(const Ray& ray, Intersection& outIntersection)
{
	Aux_IntersectRay(mRootNode, mRootNode, ray, outIntersection);
}


bool RayIntersectsBox(const vec3& origin, const vec3& rayDirInv, const vec3& BboxMin, const vec3& BboxMax)
{
	const vec3 t0 = Helper::Piecewise_Prod( (BboxMin - origin), rayDirInv );
	const vec3 t1 = Helper::Piecewise_Prod( (BboxMax - origin), rayDirInv );

	const vec3 tmax = Helper::Max_Vec3(t0, t1);
	const vec3 tmin = Helper::Min_Vec3(t0, t1);

	const float a1 = fmin(tmax.getX(), fmin(tmax.getY(), tmax.getZ()));
	const float a0 = fmax(fmax(tmin.getX(), tmin.getY()), fmax(tmin.getZ(), 0.0f));

	return a1 >= a0;
}

void BVHTree::Aux_IntersectRay(BVHNode* rootNode, BVHNode* node,
	const Ray& ray, Intersection& outIntersection)
{
	if(!node)
	{
		return;
	}

	if (node->BoundingBox.InstanceID < 0.f)
	{
		bool intersects = RayIntersectsBox(ray.mStartPos, ray.GetInvDir(),
			node->BoundingBox.MinBounds, node->BoundingBox.MaxBounds);

		if (intersects)
		{
			Aux_IntersectRay(rootNode, node->Left, ray, outIntersection);
			Aux_IntersectRay(rootNode, node->Right, ray, outIntersection);
		}
	}
	else
	{
		node->BoundingBox.mTriangle.Intersect(ray, outIntersection);

	}

	/*if (node != rootNode)
	{
		
	}
	else
	{
		Aux_IntersectRay(rootNode, node->Left, ray, outIntersection);
		Aux_IntersectRay(rootNode, node->Right, ray, outIntersection);
	}*/
}


