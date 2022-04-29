/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

 /*
  * Variant on RTree implementation using binary tree with leaf nodes containg between min/max allowed points
  * 
  * Include "Rtree.h" file in each .c/.cpp file where you want to use the RTree.
  *
  * - In exacly *one* .c/.cpp file define following macro before this include:
  *
  * \code
  * #define RTREE_IMPLEMENTATION
  * #include "RTree.h"
  * \endcode
  *
  * This enables the internal definitions.
  */

#ifndef RTREE_H
#define RTREE_H

#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif
	typedef void(*ForEachRLeafItemIntersectionFn)(void* pUserData, void* pData);
	typedef bool(*CompareRLeafItemFn)(void* pUserData, void* pData);

	typedef struct RTreeDescriptor
	{
		uint32_t minElementsPerNode;
		uint32_t maxElementsPerNode;
		uint32_t maxElements;
	} RTreeDescriptor;

	typedef struct RTree RTree;

	void initRTree(const RTreeDescriptor* pDesc, struct RTree** ppRTree);
	void exitRTree(RTree* pRTree);
	void insertRTreePoint(RTree* pRTree, const float point[2], void* pData);
	bool removeRTreePoint(RTree* pRTree, const float point[2], void* pToCompare, CompareRLeafItemFn pFn);
	void queryRTree(RTree* pRTree, const float minMax[4], ForEachRLeafItemIntersectionFn pFn, void* pUserData);

	// For Visual Studio IntelliSense.
#if defined(__cplusplus) && defined(__INTELLISENSE__)
#define RTREE_IMPLEMENTATION
#endif

#ifdef RTREE_IMPLEMENTATION
#undef RTREE_IMPLEMENTATION

#include "../Interfaces/ILog.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define INTERNAL_NODE UINT32_MAX
#define ROOT_NODE UINT32_MAX
#define RTREE_STACK_SIZE 64


	typedef float rtree_box[4]; // [0]min-x, [1]min-y, [2]max-x, [3],max-y
	typedef float rtree_point[2]; // [0]x, [1]y

#define INIT_RTREE_BOX { FLT_MAX, FLT_MAX, FLT_MIN, FLT_MIN }
	const rtree_box c_init_rtree_box = { FLT_MAX, FLT_MAX, FLT_MIN, FLT_MIN };

	static void rtree_set_box(float* dst, const float src[4])
	{
		dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
	}

	static bool rtree_box_contains_point(const float* b, const float* p)
	{
		return !(b[0] > p[0] || b[1] > p[1] || b[2] < p[0] || b[3] < p[1]);
	}

	static bool rtree_box_overlap(const float* b1, const float* b2)
	{
		return !(b1[0] > b2[2] || b1[1] > b2[3] || b1[2] < b2[0] || b1[3] < b2[1]);
	}

	static void rtree_box_enlarge(float* b, const float* p)
	{
		b[0] = MIN(b[0], p[0]);
		b[1] = MIN(b[1], p[1]);
		b[2] = MAX(b[2], p[0]);
		b[3] = MAX(b[3], p[1]);
	}

	static float rtree_box_area(const float* box)
	{
		return (box[2] - box[0]) * (box[3] - box[1]);
	}

	static float rtree_box_enlarge_cost(const float* b, const float* p)
	{
		return (MAX(p[0], b[2]) - MIN(p[0], b[0])) * (MAX(p[1], b[3]) - MIN(p[1], b[1])) - rtree_box_area(b);
	}

	typedef struct RTreeNode
	{
		rtree_box	bb; // 16
		uint32_t	leftOrFirst; // 4
		uint32_t	countOrNode; // 4
		uint32_t	parentIndex; // 4
		uint32_t	padding; // 4
	} RTreeNode;

	typedef struct RTree
	{
		uint32_t		mMaxElementsPerNode;
		uint32_t		mMinElementsPerNode;
		uint32_t		mMaxTreeSize;

		uint32_t		mLeafNodes;
		uint32_t		mDataCount;

		RTreeNode*		pTree;
		rtree_point*	pPoints;
		void**			ppData;
		uint32_t*		pIndices;

		uint32_t*		pSplitIndices;
	} RTree;

	void initRTree(const RTreeDescriptor *pDesc, RTree** ppRTree)
	{
		ASSERT(ppRTree);
		ASSERT(pDesc);

		uint32_t maxElementsPerNode = MAX(2u, pDesc->maxElementsPerNode);
		uint32_t minElementsPerNode = MAX(1u, MIN(maxElementsPerNode, pDesc->minElementsPerNode));
		uint32_t maxTreeSize = pDesc->maxElements / minElementsPerNode * 2;

		size_t totalSize = sizeof(RTree);
		totalSize += maxTreeSize * sizeof(RTreeNode);
		totalSize += pDesc->maxElements * sizeof(rtree_point);
		totalSize += pDesc->maxElements * sizeof(void*);
		totalSize += pDesc->maxElements * maxElementsPerNode * sizeof(uint32_t);
		totalSize += maxElementsPerNode * 2 * sizeof(uint32_t);

		RTree* pRTree = (RTree*)tf_malloc(totalSize);
		ASSERT(pRTree);

		pRTree->mMaxElementsPerNode = maxElementsPerNode;
		pRTree->mMinElementsPerNode = minElementsPerNode;
		pRTree->mMaxTreeSize = maxTreeSize;
		pRTree->mDataCount = 0;
		pRTree->mLeafNodes = 1;

		pRTree->pTree = (RTreeNode*)(pRTree + 1);
		pRTree->pPoints = (rtree_point*)(pRTree->pTree + maxTreeSize);
		pRTree->ppData = (void**)(pRTree->pPoints + pDesc->maxElements);
		pRTree->pIndices = (uint32_t*)(pRTree->ppData + pDesc->maxElements);
		pRTree->pSplitIndices = (uint32_t*)(pRTree->pIndices + pDesc->maxElements * maxElementsPerNode);

		//Init Root node
		rtree_set_box(pRTree->pTree[0].bb, c_init_rtree_box);
		pRTree->pTree[0].countOrNode = 0;
		pRTree->pTree[0].leftOrFirst = 0;
		pRTree->pTree[0].parentIndex = ROOT_NODE;

		*ppRTree = pRTree;
	}

	void exitRTree(RTree* pRTree)
	{
		ASSERT(pRTree);
		tf_free(pRTree);
	}

	void rtree_update_node_box(RTree* pRTree, const uint32_t treeIndex, bool recursive)
	{
		if (treeIndex == ROOT_NODE)
			return;

		RTreeNode* node = &pRTree->pTree[treeIndex];
		ASSERT(node->countOrNode == INTERNAL_NODE);
		node->bb[0] = MIN(pRTree->pTree[node->leftOrFirst].bb[0], pRTree->pTree[node->leftOrFirst + 1].bb[0]);
		node->bb[1] = MIN(pRTree->pTree[node->leftOrFirst].bb[1], pRTree->pTree[node->leftOrFirst + 1].bb[1]);
		node->bb[2] = MAX(pRTree->pTree[node->leftOrFirst].bb[2], pRTree->pTree[node->leftOrFirst + 1].bb[2]);
		node->bb[3] = MAX(pRTree->pTree[node->leftOrFirst].bb[3], pRTree->pTree[node->leftOrFirst + 1].bb[3]);

		if (recursive)
			rtree_update_node_box(pRTree, node->parentIndex, recursive);
	}

	void rtree_left_rotate(RTree* pRTree, const uint32_t treeIndex)
	{
		ASSERT(treeIndex % 2 == 0 && "Must be a right child node");
		RTreeNode pivot = pRTree->pTree[treeIndex];
		RTreeNode root = pRTree->pTree[pivot.parentIndex];
		RTreeNode rightPivot = pRTree->pTree[pivot.leftOrFirst + 1];
		RTreeNode leftRoot = pRTree->pTree[root.leftOrFirst];

		// 	| Root | leftRoot | Pivot | leftPivot | rightPivot |
		// P:  -	    0		  0 	    2			2
		//
		// 	| Pivot | Root | rightPivot	| leftPivot | leftRoot |
		// P:	-       0		0		    1			 1

		leftRoot.parentIndex = root.leftOrFirst; // set leftRoot parent index
		pRTree->pTree[pivot.leftOrFirst].parentIndex = root.leftOrFirst; // set leftPivot parent index
		pRTree->pTree[pivot.leftOrFirst + 1] = leftRoot; // set leftRoot
		if (leftRoot.countOrNode == INTERNAL_NODE)
		{
			pRTree->pTree[leftRoot.leftOrFirst].parentIndex = pivot.leftOrFirst + 1;
			pRTree->pTree[leftRoot.leftOrFirst + 1].parentIndex = pivot.leftOrFirst + 1;
		}

		pivot.parentIndex = root.parentIndex; // set pivot parent index
		root.parentIndex = pRTree->pTree[treeIndex].parentIndex; // set root parent index
		rightPivot.parentIndex = pRTree->pTree[treeIndex].parentIndex; // set leftPivot parent index

		pivot.leftOrFirst = root.leftOrFirst;
		root.leftOrFirst = pRTree->pTree[treeIndex].leftOrFirst;

		pRTree->pTree[pivot.leftOrFirst + 1] = rightPivot;
		if (rightPivot.countOrNode == INTERNAL_NODE)
		{
			pRTree->pTree[rightPivot.leftOrFirst].parentIndex = pivot.leftOrFirst + 1;
			pRTree->pTree[rightPivot.leftOrFirst + 1].parentIndex = pivot.leftOrFirst + 1;
		}

		pRTree->pTree[pivot.leftOrFirst] = root;
		pRTree->pTree[root.parentIndex] = pivot;

		rtree_update_node_box(pRTree, pivot.leftOrFirst, false);
		rtree_update_node_box(pRTree, root.parentIndex, false);
	}

	void rtree_right_rotate(RTree* pRTree, const uint32_t treeIndex)
	{
		ASSERT(treeIndex % 2 == 1 && "Must be a left child node");
		RTreeNode pivot = pRTree->pTree[treeIndex];
		RTreeNode root = pRTree->pTree[pivot.parentIndex];
		RTreeNode leftPivot = pRTree->pTree[pivot.leftOrFirst];
		RTreeNode rightRoot = pRTree->pTree[root.leftOrFirst + 1];
		// 	| Root 	| pivot		| rightRoot | leftPivot | rightPivot |
		// P:  -		0			0 			1			1
		//
		// 	| pivot | leftPivot | Root 		| rightRoot | rightPivot |
		// P:	-  		0			0			2			2

		rightRoot.parentIndex = root.leftOrFirst + 1; // set rightRoot parent index
		pRTree->pTree[pivot.leftOrFirst + 1].parentIndex = root.leftOrFirst + 1; // set rightPivot parent index
		pRTree->pTree[pivot.leftOrFirst] = rightRoot; // set rightRoot
		if (rightRoot.countOrNode == INTERNAL_NODE)
		{
			pRTree->pTree[rightRoot.leftOrFirst].parentIndex = pivot.leftOrFirst;
			pRTree->pTree[rightRoot.leftOrFirst + 1].parentIndex = pivot.leftOrFirst;
		}

		pivot.parentIndex = root.parentIndex; // set pivot parent index
		root.parentIndex = pRTree->pTree[treeIndex].parentIndex; // set root parent index
		leftPivot.parentIndex = pRTree->pTree[treeIndex].parentIndex; // set leftPivot parent index

		pivot.leftOrFirst = root.leftOrFirst;
		root.leftOrFirst = pRTree->pTree[treeIndex].leftOrFirst;

		pRTree->pTree[pivot.leftOrFirst] = leftPivot;
		if (leftPivot.countOrNode == INTERNAL_NODE)
		{
			pRTree->pTree[leftPivot.leftOrFirst].parentIndex = pivot.leftOrFirst;
			pRTree->pTree[leftPivot.leftOrFirst + 1].parentIndex = pivot.leftOrFirst;
		}

		pRTree->pTree[pivot.leftOrFirst + 1] = root;
		pRTree->pTree[root.parentIndex] = pivot;

		rtree_update_node_box(pRTree, pivot.leftOrFirst + 1, false);
		rtree_update_node_box(pRTree, root.parentIndex, false);
	}

	uint32_t rtree_rebalance(RTree* pRTree, const uint32_t treeIndex, bool* isLeftLargest)
	{
		if (pRTree->pTree[treeIndex].countOrNode != INTERNAL_NODE)
			return 0;

		bool leftSide, rightSide;
		uint32_t leftDepth = rtree_rebalance(pRTree, pRTree->pTree[treeIndex].leftOrFirst, &leftSide);
		uint32_t rightDepth = rtree_rebalance(pRTree, pRTree->pTree[treeIndex].leftOrFirst + 1, &rightSide);

		if (leftDepth < rightDepth && rightDepth - leftDepth > 1)
		{
			if (rightSide)
			{
				rtree_right_rotate(pRTree, pRTree->pTree[pRTree->pTree[treeIndex].leftOrFirst + 1].leftOrFirst);
			}
			rtree_left_rotate(pRTree, pRTree->pTree[treeIndex].leftOrFirst + 1);
			++leftDepth;
			--rightDepth;
		}
		else if (rightDepth < leftDepth && leftDepth - rightDepth > 1)
		{
			if (!leftSide)
			{
				rtree_left_rotate(pRTree, pRTree->pTree[pRTree->pTree[treeIndex].leftOrFirst].leftOrFirst + 1);
			}
			rtree_right_rotate(pRTree, pRTree->pTree[treeIndex].leftOrFirst);
			--leftDepth;
			++rightDepth;
		}
		*isLeftLargest = leftDepth > rightDepth;
		return MAX(leftDepth, rightDepth) + 1;
	}

#if defined(_DEBUG)
	void printRtree(RTree* pRTree)
	{
		ASSERT(pRTree);
		const uint32_t print_stack_size = 2048;
		uint32_t tree_stack[print_stack_size];
		uint32_t node_stack[print_stack_size];
		uint32_t depth = 0;
		uint8_t stack_ptr = 0;

		tree_stack[stack_ptr++] = 0; // Set root node as start point
		RAW_LOGF(LogLevel::eINFO, "Print RTree \n");
		do {
			uint8_t node_ptr = stack_ptr;
			stack_ptr = 0;
			memcpy(node_stack, tree_stack, print_stack_size * sizeof(uint32_t));

			char buffer[8192] = {};
			sprintf(buffer, "%u: ", depth);
			uint32_t offset = strlen(buffer);
			do {
				uint32_t index = node_stack[--node_ptr];
				const RTreeNode *node = &pRTree->pTree[index];
				if (node->countOrNode != INTERNAL_NODE) // found leaf node
				{
					sprintf(buffer + offset, "L%u ", index);
					offset = strlen(buffer);
				}
				else
				{
					sprintf(buffer + offset, "N%u ", index);
					offset = strlen(buffer);

					// Add child nodes to stack
					ASSERT(stack_ptr + 2 <= print_stack_size &&
						"Undefined behavior, exceeds stack size");
					tree_stack[stack_ptr++] = node->leftOrFirst + 1;
					tree_stack[stack_ptr++] = node->leftOrFirst;
				}

			} while (node_ptr > 0);
			sprintf(buffer + offset, "\n");
			RAW_LOGF(LogLevel::eINFO, buffer);
			++depth;
		} while (stack_ptr > 0);
	}
#else
	void printRtree(RTree* pRTree) {}
#endif

	void rtree_partition(RTree* pRTree, const uint32_t treeIndex, float* outLeft, float* outRight, uint32_t* outLeftCount, uint32_t* outRightCount)
	{
		*outLeftCount = 0;
		*outRightCount = 0;
		RTreeNode* node = &pRTree->pTree[treeIndex];
		ASSERT(node->countOrNode >= 2 && node->countOrNode != INTERNAL_NODE && "Required to be a filled leave node");

		uint8_t bestAxis = 0;
		uint32_t bestSplitrtree_point = 0;
		float bestSplitCost = FLT_MAX;
		// Find best split point with the least amount of area over all elements it the leaf and on both axises
		for (uint8_t axis = 0; axis < 2; ++axis)
		{
			for (uint32_t i = 0; i < node->countOrNode; ++i)
			{
				const rtree_point splitrtree_point = { pRTree->pPoints[pRTree->pIndices[node->leftOrFirst + i]][0] - 0.000000001f, 
					pRTree->pPoints[pRTree->pIndices[node->leftOrFirst + i]][1] - 0.000000001f };
				rtree_box left = INIT_RTREE_BOX, right = INIT_RTREE_BOX;
				uint32_t leftCount = 0, rightCount = 0;
				for (uint32_t j = 0; j < node->countOrNode; ++j)
				{
					const uint32_t index = node->leftOrFirst + j;
					if (pRTree->pPoints[pRTree->pIndices[index]][axis] <= splitrtree_point[axis])
					{
						rtree_box_enlarge(left, pRTree->pPoints[pRTree->pIndices[index]]);
						++leftCount;
					}
					else
					{
						rtree_box_enlarge(right, pRTree->pPoints[pRTree->pIndices[index]]);
						++rightCount;
					}
				}

				const float cost = rtree_box_area(left) * leftCount + rtree_box_area(right) * rightCount;
				if (rightCount != 0 && leftCount != 0 && cost < bestSplitCost)
				{
					bestSplitCost = cost;
					bestAxis = axis;
					bestSplitrtree_point = i;
				}
			}
		}

		const rtree_point splitrtree_point = { pRTree->pPoints[pRTree->pIndices[node->leftOrFirst + bestSplitrtree_point]][0] - 0.000000001f, 
			pRTree->pPoints[pRTree->pIndices[node->leftOrFirst + bestSplitrtree_point]][1] - 0.000000001f };
		for (uint32_t i = 0; i < node->countOrNode; ++i)
		{
			const uint32_t index = node->leftOrFirst + i;
			if (pRTree->pPoints[pRTree->pIndices[index]][bestAxis] <= splitrtree_point[bestAxis])
			{
				rtree_box_enlarge(outLeft, pRTree->pPoints[pRTree->pIndices[index]]);
				pRTree->pSplitIndices[(*outLeftCount)++] = pRTree->pIndices[index];
			}
			else
			{
				rtree_box_enlarge(outRight, pRTree->pPoints[pRTree->pIndices[index]]);
				pRTree->pSplitIndices[pRTree->mMaxElementsPerNode + (*outRightCount)++] = pRTree->pIndices[index];
			}
		}
		ASSERT(outLeftCount != 0 && outRightCount != 0);
	}

	void rtree_split(RTree* pRTree, const uint32_t treeIndex, const float* leftrtree_box, const float* rightrtree_box, const uint32_t leftCount, const uint32_t rightCount)
	{
		ASSERT(treeIndex + 2 < pRTree->mMaxTreeSize && "Max tree size reached");
		RTreeNode* currentLeaf = &pRTree->pTree[treeIndex];
		ASSERT(currentLeaf->countOrNode >= 2 && currentLeaf->countOrNode != INTERNAL_NODE && "Required to be a filled leave node");

		// Rearrange left and right indices
		for (uint32_t i = 0; i < leftCount; ++i)
		{
			pRTree->pIndices[currentLeaf->leftOrFirst + i] = pRTree->pSplitIndices[i];
		}
		for (uint32_t i = 0; i < rightCount; ++i)
		{
			// Append right indices at end of the list
			pRTree->pIndices[pRTree->mMaxElementsPerNode * pRTree->mLeafNodes + i] = pRTree->pSplitIndices[pRTree->mMaxElementsPerNode + i];
		}

		// Set leaves
		RTreeNode* left = &pRTree->pTree[pRTree->mLeafNodes * 2 - 1];
		RTreeNode* right = &pRTree->pTree[pRTree->mLeafNodes * 2];

		rtree_set_box(left->bb, leftrtree_box);
		left->leftOrFirst = currentLeaf->leftOrFirst;
		left->countOrNode = leftCount;
		left->parentIndex = treeIndex;

		rtree_set_box(right->bb, rightrtree_box);
		right->leftOrFirst = pRTree->mMaxElementsPerNode * pRTree->mLeafNodes;
		right->countOrNode = rightCount;
		right->parentIndex = treeIndex;

		// Set current leaf as node
		currentLeaf->leftOrFirst = pRTree->mLeafNodes * 2 - 1;
		currentLeaf->countOrNode = INTERNAL_NODE;

		// Increased amount of leaf nodes by one
		++pRTree->mLeafNodes;
	}

	void rtree_split_partition(RTree* pRTree, const uint32_t treeIndex)
	{
		// Partition
		rtree_box leftrtree_box = INIT_RTREE_BOX, rightrtree_box = INIT_RTREE_BOX;
		uint32_t leftCount, rightCount;
		rtree_partition(pRTree, treeIndex, leftrtree_box, rightrtree_box, &leftCount, &rightCount);
		rtree_split(pRTree, treeIndex, leftrtree_box, rightrtree_box, leftCount, rightCount);
	}

	void rtree_insert_point(RTree* pRTree, const uint32_t treeIndex, const uint32_t index)
	{
		RTreeNode* node = &pRTree->pTree[treeIndex];

		// Enlarge current node bb since the point is added or added to one of it child nodes
		rtree_box_enlarge(node->bb, pRTree->pPoints[index]);

		if (node->countOrNode != INTERNAL_NODE) // found leaf node
		{
			ASSERT(node->countOrNode < pRTree->mMaxElementsPerNode);

			pRTree->pIndices[node->leftOrFirst + node->countOrNode] = index;
			++node->countOrNode;
			if (node->countOrNode == pRTree->mMaxElementsPerNode) // Forced split
			{
				rtree_split_partition(pRTree, treeIndex);

				// Rebalance
				bool isLeftLarger;
				rtree_rebalance(pRTree, 0, &isLeftLarger);
			}
			else if (node->countOrNode >= pRTree->mMinElementsPerNode) // Check if splitting results in better heuristic
			{
				// Partition
				rtree_box leftrtree_box, rightrtree_box;
				uint32_t leftCount, rightCount;
				rtree_partition(pRTree, treeIndex, leftrtree_box, rightrtree_box, &leftCount, &rightCount);
				if (rtree_box_area(leftrtree_box) * leftCount + rtree_box_area(rightrtree_box) * rightCount < rtree_box_area(node->bb) * node->countOrNode)
				{
					rtree_split(pRTree, treeIndex, leftrtree_box, rightrtree_box, leftCount, rightCount);

					// Rebalance
					bool isLeftLarger;
					rtree_rebalance(pRTree, 0, &isLeftLarger);
				}
			}
		}
		else
		{
			RTreeNode* leftNode = &pRTree->pTree[node->leftOrFirst];
			RTreeNode* rightNode = &pRTree->pTree[node->leftOrFirst + 1];
			if (rtree_box_contains_point(leftNode->bb, pRTree->pPoints[index]))
			{
				rtree_insert_point(pRTree, node->leftOrFirst, index);
			}
			else if (rtree_box_contains_point(rightNode->bb, pRTree->pPoints[index]))
			{
				rtree_insert_point(pRTree, node->leftOrFirst + 1, index);
			}
			else if (rtree_box_enlarge_cost(leftNode->bb, pRTree->pPoints[index]) < rtree_box_enlarge_cost(rightNode->bb, pRTree->pPoints[index]))
			{
				rtree_insert_point(pRTree, node->leftOrFirst, index);
			}
			else
			{
				rtree_insert_point(pRTree, node->leftOrFirst + 1, index);
			}
		}
	}

	void insertRTreePoint(RTree* pRTree, const float point[2], void* pData)
	{
		ASSERT(pRTree);
		pRTree->pPoints[pRTree->mDataCount][0] = point[0];
		pRTree->pPoints[pRTree->mDataCount][1] = point[1];
		pRTree->ppData[pRTree->mDataCount] = pData;
		rtree_insert_point(pRTree, 0, pRTree->mDataCount);
		++pRTree->mDataCount;
	}

	void queryRTree(RTree* pRTree, const float minMax[4], ForEachRLeafItemIntersectionFn pFn, void* pUserData)
	{
		ASSERT(pRTree);
		uint32_t tree_stack[RTREE_STACK_SIZE];
		uint8_t stack_ptr = 0;
		tree_stack[stack_ptr++] = 0; // Set root node as start point
		do {
			const RTreeNode* node = &pRTree->pTree[tree_stack[--stack_ptr]];
			if (rtree_box_overlap(node->bb, minMax))
			{
				if (node->countOrNode != INTERNAL_NODE) // found leaf node
				{
					for (uint32_t i = 0; i < node->countOrNode; ++i)
					{
						// check element against area
						if (rtree_box_contains_point(minMax, pRTree->pPoints[pRTree->pIndices[node->leftOrFirst + i]]))
						{
							pFn(pUserData, pRTree->ppData[pRTree->pIndices[node->leftOrFirst + i]]);
						}
					}
				}
				else
				{
					// Add child nodes to stack
					ASSERT(stack_ptr + 2 <= RTREE_STACK_SIZE && "Undefined behavior, exceeds stack size");
					tree_stack[stack_ptr++] = node->leftOrFirst;
					tree_stack[stack_ptr++] = node->leftOrFirst + 1;
				}
			}
		} while (stack_ptr > 0);
	}

	bool rtree_get_leave_node(RTree* pRTree, const uint32_t leftOrFirst, uint32_t* outNodeIndex)
	{
		uint32_t tree_stack[RTREE_STACK_SIZE];
		uint8_t stack_ptr = 0;
		tree_stack[stack_ptr++] = 0; // Set root node as start point
		do {
			uint32_t treeIndex = tree_stack[--stack_ptr];
			RTreeNode* node = &pRTree->pTree[treeIndex];
			if (node->countOrNode != INTERNAL_NODE) // found leaf node
			{
				if (node->leftOrFirst == leftOrFirst)
				{
					*outNodeIndex = treeIndex;
					return true;
				}
			}
			else
			{
				// Add child nodes to stack
				ASSERT(stack_ptr + 2 <= RTREE_STACK_SIZE && "Undefined behavior, exceeds stack size");
				tree_stack[stack_ptr++] = node->leftOrFirst;
				tree_stack[stack_ptr++] = node->leftOrFirst + 1;
			}
		} while (stack_ptr > 0);

		return false;
	}

	void rtree_remove_at_index(RTree* pRTree, const uint32_t treeIndex, const uint32_t index)
	{
		RTreeNode* pTree = pRTree->pTree;
		RTreeNode* node = &pTree[treeIndex];
		ASSERT(index < node->leftOrFirst + node->countOrNode && index >= node->leftOrFirst);


		// Reduce data count and switch data last data location
		// This ensures usability of dataCount as last index for inserting new points
		--pRTree->mDataCount;
		pRTree->ppData[pRTree->pIndices[index]] = pRTree->ppData[pRTree->mDataCount];
		pRTree->pPoints[pRTree->pIndices[index]][0] = pRTree->pPoints[pRTree->mDataCount][0];
		pRTree->pPoints[pRTree->pIndices[index]][1] = pRTree->pPoints[pRTree->mDataCount][1];
		pRTree->ppData[pRTree->mDataCount] = NULL;
		pRTree->pPoints[pRTree->mDataCount][0] = 0;
		pRTree->pPoints[pRTree->mDataCount][1] = 0;

		// Data index previously located on index dataCount, should be updated with new index pIndices[index]
		// Is there a faster way then walk over the tree?
		uint32_t tree_stack[RTREE_STACK_SIZE];
		uint8_t stack_ptr = 0;
		tree_stack[stack_ptr++] = 0; // Set root node as start point
		bool found = false;
		do {
			uint32_t treeIndex = tree_stack[--stack_ptr];
			RTreeNode* traversedNode = &pRTree->pTree[treeIndex];
			if (traversedNode->countOrNode != INTERNAL_NODE) // found leaf node
			{
				for (uint32_t i = 0; i < traversedNode->countOrNode; ++i)
				{
					if (pRTree->pIndices[traversedNode->leftOrFirst + i] == pRTree->mDataCount)
					{
						pRTree->pIndices[traversedNode->leftOrFirst + i] = pRTree->pIndices[index];
						found = true;
						break;
					}
				}
			}
			else
			{
				// Add child nodes to stack
				ASSERT(stack_ptr + 2 <= RTREE_STACK_SIZE && "Undefined behavior, exceeds stack size");
				tree_stack[stack_ptr++] = traversedNode->leftOrFirst;
				tree_stack[stack_ptr++] = traversedNode->leftOrFirst + 1;
			}
		} while (stack_ptr > 0 && !found);


		// Switch last index, with removed location
		--node->countOrNode;
		pRTree->pIndices[index] = pRTree->pIndices[node->leftOrFirst + node->countOrNode];

		// Merge with sibling in parent
		if (node->countOrNode == 0 && node->parentIndex != ROOT_NODE)
		{
			--pRTree->mLeafNodes; // Amount of leave nodes is reduced by one
			uint8_t isLeftNode = treeIndex % 2;
			// Replace empty data space with the current outer leave node
			uint32_t outerNodeIndex;
			bool succeed = rtree_get_leave_node(pRTree, pRTree->mMaxElementsPerNode * pRTree->mLeafNodes, &outerNodeIndex);
			ASSERT(succeed);
			RTreeNode* outerNode = &pTree[outerNodeIndex];
			for (uint32_t i = 0; i < outerNode->countOrNode; ++i)
			{
				pRTree->pIndices[node->leftOrFirst + i] = pRTree->pIndices[outerNode->leftOrFirst + i];
			}
			outerNode->leftOrFirst = node->leftOrFirst;

			// Update parent node with sibling
			const uint32_t siblingIndex = (treeIndex - 1) + isLeftNode * 2;
			RTreeNode* sibling = &pTree[siblingIndex];
			ASSERT(sibling->parentIndex == node->parentIndex);
			sibling->parentIndex = pTree[node->parentIndex].parentIndex;
			if (sibling->countOrNode == INTERNAL_NODE) // update child node parent index
			{
				pTree[sibling->leftOrFirst].parentIndex = node->parentIndex;
				pTree[sibling->leftOrFirst + 1].parentIndex = node->parentIndex;
			}
			pTree[node->parentIndex] = *sibling;

			// Update bounding boxes of parents in tree
			rtree_update_node_box(pRTree, sibling->parentIndex, true);

			// Move outer nodes to free locations if not working on outer nodes
			// pTree[treeIndex] + pTree[siblingIndex] going to be free locations
			// get "mLeafNodes * 2 (-1)" left and right and move to free space
			// update parent leftOrFirst location
			if (pRTree->mLeafNodes * 2 != treeIndex && pRTree->mLeafNodes * 2 != siblingIndex)
			{
				pTree[pTree[pRTree->mLeafNodes * 2].parentIndex].leftOrFirst = isLeftNode ? treeIndex : treeIndex - 1;
				pTree[treeIndex] = isLeftNode ? pTree[pRTree->mLeafNodes * 2 - 1] : pTree[pRTree->mLeafNodes * 2];
				pTree[siblingIndex] = isLeftNode ? pTree[pRTree->mLeafNodes * 2] : pTree[pRTree->mLeafNodes * 2 - 1];

				// update child node parent index
				if (pTree[treeIndex].countOrNode == INTERNAL_NODE)
				{
					pTree[pTree[treeIndex].leftOrFirst].parentIndex = treeIndex;
					pTree[pTree[treeIndex].leftOrFirst + 1].parentIndex = treeIndex;
				}

				if (pTree[siblingIndex].countOrNode == INTERNAL_NODE)
				{
					pTree[pTree[siblingIndex].leftOrFirst].parentIndex = siblingIndex;
					pTree[pTree[siblingIndex].leftOrFirst + 1].parentIndex = siblingIndex;
				}
			}
		}
		else
		{
			// Reduce node bounding box if still contains items
			rtree_set_box(node->bb, c_init_rtree_box);
			for (uint32_t i = 0; i < node->countOrNode; ++i)
			{
				const float* point = pRTree->pPoints[pRTree->pIndices[i + node->leftOrFirst]];
				node->bb[0] = MIN(node->bb[0], point[0]);
				node->bb[1] = MIN(node->bb[1], point[1]);
				node->bb[2] = MAX(node->bb[2], point[0]);
				node->bb[3] = MAX(node->bb[3], point[1]);
			}

			// Update bounding boxes of parents in tree
			rtree_update_node_box(pRTree, node->parentIndex, true);
		}
	}

	bool removeRTreePoint(RTree* pRTree, const float point[2], void* toCompare, CompareRLeafItemFn pFn)
	{
		ASSERT(pRTree);
		uint32_t tree_stack[RTREE_STACK_SIZE];
		uint8_t stack_ptr = 0;
		tree_stack[stack_ptr++] = 0; // Set root node as start point
		do {
			const uint32_t treeIndex = tree_stack[--stack_ptr];
			const RTreeNode* node = &pRTree->pTree[treeIndex];
			if (rtree_box_contains_point(node->bb, point))
			{
				if (node->countOrNode != INTERNAL_NODE) // found leaf node
				{
					// check elements against area
					// execute query function if inside area
					for (uint32_t i = 0; i < node->countOrNode; ++i)
					{
						if (pFn(toCompare, pRTree->ppData[pRTree->pIndices[node->leftOrFirst + i]]))
						{
							//Remove
							rtree_remove_at_index(pRTree, treeIndex, node->leftOrFirst + i);
							return true;
						}
					}
				}
				else
				{
					// Add child nodes to stack
					ASSERT(stack_ptr + 2 <= RTREE_STACK_SIZE && "Undefined behavior, exceeds stack size");
					tree_stack[stack_ptr++] = node->leftOrFirst;
					tree_stack[stack_ptr++] = node->leftOrFirst + 1;
				}
			}
		} while (stack_ptr > 0);

		return false;
	}

#endif // RTREE_IMPLEMENTATION

#ifdef __cplusplus
}    // extern "C"
#endif

#endif // RTREE_H