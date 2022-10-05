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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#define QUICKSORT_THRESHOLD 30
#define SIMPLESORT_THRESHOLD 4
#define TMP_BUF_STACK_SIZE 256	

// PVS-Studio warning suppression
//-V:DEFINE_SORT_ALGORITHMS_FOR_TYPE:769
//-V:DEFINE_QUICK_SORT_IMPL_FUNCTION:769


/*
 * Generates definitions for following functions:
 * 
 * Interface functions:
 * 
 * attrs void sort<type> (type* pData, size_t memberCount)
 *	regular sort
 *	if size < QUICKSORT_THRESHOLD 
 *		use insertion sort
 *	else
 *		use quick sort
 *
 * attrs void stableSort<type> (type* pArr, size_t memberCount)
 *	stable sort (uses insertion sort)
 *	NOTE: 2-3 times slower than regular sort for big arrays
 * 
 * attrs size_t partition<type> (type* pArr, size_t pivot, size_t memberCount)
 *	partitions array around the pivot(similar to std::nth_element)
 * 
 * Implementation functions:
 *
 * attrs void simplSort<type> (type* pArr, size_t memberCount)
 *	sort used for small arrays (used in stableSort)
 * 
 * attrs type* partitionImpl<type> (type* pBegin, type* pEnd, type* pPivot)
 *	implementation function for partition
 * 
 * attrs void quickSortImpl<type> (type* pBegin, type* pEnd, type* tmp)
 *	implementation function for sort
 * 
 */
#define DEFINE_SORT_ALGORITHMS_FOR_TYPE(attrs, type, lessFn)																					\
	DEFINE_SIMPLE_SORT_FUNCTION(attrs, CONCAT(simpleSort, type), type, lessFn)																	\
	DEFINE_INSERTION_SORT_FUNCTION(attrs, CONCAT(stableSort, type), type, lessFn, CONCAT(simpleSort, type))										\
	DEFINE_PARTITION_IMPL_FUNCTION(attrs, CONCAT(partitionImpl, type), type, lessFn)															\
	DEFINE_PARTITION_FUNCTION(attrs, CONCAT(partition, type), type, lessFn, CONCAT(partitionImpl, type))										\
	DEFINE_QUICK_SORT_IMPL_FUNCTION(attrs, CONCAT(quickSortImpl, type), type, lessFn, CONCAT(stableSort, type), CONCAT(partitionImpl, type))	\
	DEFINE_QUICK_SORT_FUNCTION(attrs, CONCAT(sort, type), type, CONCAT(quickSortImpl, type))

#define DEFINE_SIMPLE_SORT_FUNCTION(attrs, fnName, type, lessFn)	\
	attrs void fnName (type* pArr, size_t memberCount)				\
	{																\
		SIMPLE_SORT_IMPL(fnName, type, pArr, memberCount, lessFn, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_STRUCT, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)	\
	}

#define DEFINE_INSERTION_SORT_FUNCTION(attrs, fnName, type, lessFn, simpleSortFn)	\
	attrs void fnName (type* pArr, size_t memberCount)								\
	{																				\
		INSERTION_SORT_IMPL(type, pArr, memberCount, simpleSortFn, lessFn, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_STRUCT, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)	\
	}
#define DEFINE_PARTITION_IMPL_FUNCTION(attrs, fnName, type, lessFn)																	\
	attrs type* fnName (type* pBegin, type* pEnd, type* pPivot)																		\
	{																																\
		PARTITION_IMPL(type, pBegin, pEnd, pPivot, lessFn, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_STRUCT, PTR_INC_NUMERIC)	\
	}
#define DEFINE_PARTITION_FUNCTION(attrs, fnName, type, lessFn, partitionImplFn)		\
	attrs size_t fnName (type* pArr, size_t pivot, size_t memberCount)				\
	{																				\
		if (memberCount == 0)														\
			return 0;																\
		type* pPivot = partitionImplFn (pArr, pArr + memberCount, pArr + pivot);	\
		ASSERT(pPivot && pArr);														\
		return pPivot && pArr ? pPivot - pArr : 0;									\
	}

#define DEFINE_QUICK_SORT_IMPL_FUNCTION(attrs, fnName, type, lessFn, fallbackSort, partitionImpl)																\
	attrs void fnName (type* pBegin, type* pEnd, type* tmp)																										\
	{																																							\
		QUICKSORT_IMPL(type, pBegin, pEnd, tmp, fnName, fallbackSort, partitionImpl, lessFn, COPY_STRUCT, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)	\
	}

#define DEFINE_QUICK_SORT_FUNCTION(attrs, fnName, type, quickSortImplFn)	\
	attrs void fnName (type* pData, size_t memberCount)						\
	{																		\
		type tmp;															\
		quickSortImplFn (pData, pData + memberCount, &tmp);					\
	}	

#define CONCAT(x, y) CONCAT_IMPL(x,y)
#define CONCAT_IMPL(x,y) x##y

#define CREATE_TEMP_NUMERIC(type, name) \
	type CONCAT(name, Buf);				\
	type* name = & CONCAT(name,Buf)
#define CREATE_TEMP_GENERIC(type, name)							\
	ALIGNAS(8) char CONCAT(name, Buf)[TMP_BUF_STACK_SIZE];		\
	char* name = CONCAT(name, Buf);								\
	if (memberSize > sizeof(CONCAT(name, Buf)))					\
		name = tf_malloc(memberSize);							\
	(void)0
#define DESTROY_TEMP_NUMERIC(name) ((void)0)
#define DESTROY_TEMP_GENERIC(name) ((memberSize > sizeof(CONCAT(name, Buf))) ? tf_free(name) : (void)0)
#define LESS_NUMERIC(pX, pY) (*(pX) < *(pY))
#define LESS_GENERIC(pX, pY) less((pX), (pY), pUserData)

#define COPY_NUMERIC(pDst, pSrc) (*(pDst) = *(pSrc), (void)0)
#define COPY_GENERIC(pDst, pSrc) (memcpy(pDst, pSrc, memberSize), (void)0)
#define COPY_STRUCT(pDst, pSrc) (memcpy(pDst, pSrc, sizeof(*pDst)), (void)0)

#define SWAP(pX, pY, tmp, copy) (	\
	copy((tmp), (pX)),			\
	copy((pX), (pY)),			\
	copy((pY), (tmp)),			\
	(void)0)

#define PTR_INC_GENERIC(ptr) ((ptr) += memberSize)
#define PTR_DEC_GENERIC(ptr) ((ptr) -= memberSize)
#define PTR_ADD_GENERIC(ptr, val) ((ptr) + (val) * memberSize)
#define PTR_SUB_GENERIC(ptr, val) ((ptr) - (val) * memberSize)
#define PTR_DIFF_GENERIC(p0, p1) (((p0) - (p1)) / memberSize)

#define PTR_INC_NUMERIC(ptr) (++(ptr))
#define PTR_DEC_NUMERIC(ptr) (--(ptr))
#define PTR_ADD_NUMERIC(ptr, val) ((ptr) + (val))
#define PTR_SUB_NUMERIC(ptr, val) ((ptr) - (val))
#define PTR_DIFF_NUMERIC(p0, p1) ((p0) - (p1))

// SIMPLE SORT
// Used for array sizes 0..5

#define SIMPLE_SORT_2(pArr, tmp, LESS, COPY, PTR_ADD)	\
	{													\
		if (LESS(PTR_ADD(pArr, 1), pArr))				\
			SWAP(pArr, PTR_ADD(pArr, 1), tmp, COPY);	\
	} (void)0


#define SIMPLE_SORT_3(pArr, tmp, LESS, COPY, PTR_ADD)					\
	{																	\
		if (!LESS(PTR_ADD(pArr, 1), pArr))								\
		{																\
			if (LESS(PTR_ADD(pArr, 2), PTR_ADD(pArr, 1)))				\
			{															\
				SWAP(PTR_ADD(pArr, 1), PTR_ADD(pArr, 2), tmp, COPY);	\
				if (LESS(PTR_ADD(pArr, 1), pArr))						\
					SWAP(pArr, PTR_ADD(pArr, 1), tmp, COPY);			\
			}															\
		}																\
		else															\
		{																\
			if (!LESS(PTR_ADD(pArr, 2), PTR_ADD(pArr, 1)))				\
			{															\
				SWAP(pArr, PTR_ADD(pArr, 1), tmp, COPY);				\
				if (LESS(PTR_ADD(pArr, 2), PTR_ADD(pArr, 1)))			\
					SWAP(PTR_ADD(pArr, 1), PTR_ADD(pArr, 2), tmp, COPY);\
			}															\
			else														\
				SWAP(pArr, PTR_ADD(pArr, 2), tmp, COPY);				\
																		\
		}																\
	} (void)0															

#define SIMPLE_SORT_IMPL(SIMPLE_SORT_FN, type, pArr, memberCount, LESS, CREATE_TEMP, DESTROY_TEMP, COPY, PTR_INC, PTR_ADD, PTR_SUB)	\
	CREATE_TEMP(type, tmp);													\
	switch (memberCount)													\
	{																		\
	case 0:																	\
	case 1:																	\
		return;																\
	case 2:																	\
		SIMPLE_SORT_2(pArr, tmp, LESS, COPY, PTR_ADD);						\
		DESTROY_TEMP(tmp);													\
		return;																\
	case 3:																	\
		SIMPLE_SORT_3(pArr, tmp, LESS, COPY, PTR_ADD);						\
		DESTROY_TEMP(tmp);													\
		return;																\
	default:																\
		SIMPLE_SORT_FN(pArr, memberCount - 1);								\
		break;																\
	}																		\
																			\
	for (type* pCurrent = PTR_ADD(pArr, memberCount - 1);					\
		 pCurrent > pArr && LESS(pCurrent, PTR_SUB(pCurrent, 1));			\
		 pCurrent = PTR_SUB(pCurrent, 1) )									\
	{																		\
		SWAP(pCurrent, PTR_SUB(pCurrent, 1), tmp, COPY);					\
	}																		\
	DESTROY_TEMP(tmp);	



// INSERTION SORT (stable sort)
// TODO: Optimize for large arrays (shell sort can be used)
#define INSERTION_SORT_IMPL(type, pArr, memberCount, SIMPLE_SORT, LESS, CREATE_TEMP, DESTROY_TEMP, COPY, PTR_INC, PTR_DEC, PTR_ADD, PTR_SUB)	\
	CREATE_TEMP(type, tmp);																										\
	if (memberCount <= SIMPLESORT_THRESHOLD)																					\
	{																															\
		SIMPLE_SORT(pArr, memberCount);																							\
		return;																													\
	}																															\
	type* pBegin = pArr;																										\
	type* pEnd = PTR_ADD(pBegin, memberCount);																					\
	type* pPrev = pBegin;																										\
																																\
	for (type* pCurrent = PTR_ADD(pBegin, 1); pCurrent != pEnd; PTR_INC(pCurrent), PTR_INC(pPrev))								\
	{																															\
		type* pTmpCurrent = pCurrent;																							\
		type* pTmpPrev = pPrev;																									\
		COPY(tmp, pTmpCurrent);																									\
		for (;pTmpCurrent != pBegin && LESS(tmp, pTmpPrev) ; PTR_DEC(pTmpCurrent), PTR_DEC(pTmpPrev))							\
			COPY(pTmpCurrent, pTmpPrev);																						\
		COPY(pTmpCurrent, tmp);																									\
	}																															\
	DESTROY_TEMP(tmp);																										

// PARTITION

#define PARTITION_IMPL(type, pBegin, pEnd, pPivot, LESS, CREATE_TEMP, DESTROY_TEMP, COPY, PTR_INC)	\
	if (pBegin == pEnd)																				\
		return NULL;																				\
	/* skip all elements that are in correct place */												\
	while (pBegin < pEnd && LESS(pBegin, pPivot))													\
		PTR_INC(pBegin);																			\
																									\
	CREATE_TEMP(type, tmp);																			\
																									\
	type* pNewPivot = pBegin;																		\
	for (type* pCurrent = pBegin; pCurrent < pEnd; PTR_INC(pCurrent))								\
	{																								\
		if (LESS(pCurrent, pPivot))																	\
		{																							\
			SWAP(pCurrent, pNewPivot, tmp, COPY);													\
			PTR_INC(pNewPivot);																		\
		}																							\
	}																								\
																									\
	SWAP(pPivot, pNewPivot, tmp, COPY);																\
																									\
	DESTROY_TEMP(tmp);																				\
																									\
	return pNewPivot;		

// Picks middle element out of 5 elements and sorts them
#define PICK_PIVOT_AND_SORT(type, pBegin, pEnd, pRes, memberCount, tmp, LESS, PTR_ADD, PTR_SUB, COPY) \
{													\
	type* p0 = pBegin;								\
	type* p4 = PTR_SUB(pEnd, 1);					\
	size_t halfSize = memberCount / 2;				\
	type* p2 = PTR_ADD(pBegin, halfSize);			\
	type* p1 = p0 + (p2 - p0) / 2;					\
	type* p3 = p2 + (p2 - p0) / 2;					\
													\
	if (LESS(p2, p0))								\
		SWAP(p0, p2, tmp, COPY);					\
													\
	if (LESS(p1, p0))								\
		SWAP(p0, p1, tmp, COPY);					\
	else if (LESS(p2, p1))							\
		SWAP(p1, p2, tmp, COPY);					\
													\
	/* p0 < p1 < p2 */								\
													\
	if (LESS(p3, p2))								\
	{												\
		SWAP(p2, p3, tmp, COPY);					\
		if (LESS(p2, p1))							\
		{											\
			SWAP(p1, p2, tmp, COPY);				\
			if (LESS(p1, p0))						\
				SWAP(p0, p1, tmp, COPY);			\
		}											\
	}												\
													\
	if (LESS(p4, p3))								\
	{												\
		SWAP(p3, p4, tmp, COPY);					\
		if (LESS(p3, p2))							\
		{											\
			SWAP(p2, p3, tmp, COPY);				\
			if (LESS(p2, p1))						\
			{										\
				SWAP(p1, p2, tmp, COPY);			\
				if (LESS(p1, p0))					\
					SWAP(p0, p1, tmp, COPY);		\
			}										\
		}											\
	}												\
	pRes = p2;										\
}((void)0)

#define QUICKSORT_IMPL(type, pBegin, pEnd, tmp, QUICKSORT_IMPL_FN, FALLBACK_SORT, PARTITION_IMPL_FN, LESS, COPY, PTR_ADD, PTR_SUB, PTR_DIFF)	\
	size_t memberCount = PTR_DIFF(pEnd, pBegin);														\
	if (memberCount <= QUICKSORT_THRESHOLD)																\
	{																									\
		FALLBACK_SORT(pBegin, memberCount);																\
		return;																							\
	}																									\
																										\
	type* pPivot;																						\
	PICK_PIVOT_AND_SORT(type, pBegin, pEnd, pPivot, memberCount, tmp, LESS, PTR_ADD, PTR_SUB, COPY);	\
																										\
	pPivot = PARTITION_IMPL_FN(pBegin, pEnd, pPivot);													\
	QUICKSORT_IMPL_FN(pBegin, pPivot, tmp);																\
	QUICKSORT_IMPL_FN(PTR_ADD(pPivot, 1), pEnd, tmp);																																													
