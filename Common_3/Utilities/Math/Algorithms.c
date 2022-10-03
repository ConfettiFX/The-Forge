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

#include "Algorithms.h"
#include "AlgorithmsImpl.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IMemory.h"



// SIMPLE SORT
// Used for array sizes 0..5


static void simpleSort(void* pVoidData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData)
{
#define SIMPLE_SORT_FN(pArr, count) simpleSort(pArr, count, memberSize, less, pUserData)
	SIMPLE_SORT_IMPL(SIMPLE_SORT_FN, char, (char*)pVoidData, memberCount, LESS_GENERIC, CREATE_TEMP_GENERIC, DESTROY_TEMP_GENERIC, COPY_GENERIC, PTR_INC_GENERIC, PTR_ADD_GENERIC, PTR_SUB_GENERIC)
#undef SIMPLE_SORT_FN
}

static void simpleSortInt8(int8_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortInt8, int8_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt16(int16_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortInt16, int16_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt32(int32_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortInt32, int32_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt64(int64_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortInt64, int64_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

static void simpleSortUInt8(uint8_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortUInt8, uint8_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt16(uint16_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortUInt16, uint16_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt32(uint32_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortUInt32, uint32_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt64(uint64_t* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortUInt64, uint64_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

static void simpleSortFloat(float* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortFloat, float, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortDouble(double* pArr, size_t memberCount)
{
	SIMPLE_SORT_IMPL(simpleSortDouble, double, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

// INSERTION SORT (stable sort)																									

void stableSort(void* pVoidData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData)
{
#define SIMPLE_SORT(pArr, memberCount) simpleSort(pArr, memberCount, memberSize, less, pUserData)
	INSERTION_SORT_IMPL(char, (char*)pVoidData, memberCount, SIMPLE_SORT, LESS_GENERIC, CREATE_TEMP_GENERIC, DESTROY_TEMP_GENERIC, COPY_GENERIC, PTR_INC_GENERIC, PTR_DEC_GENERIC, PTR_ADD_GENERIC, PTR_SUB_GENERIC)
#undef SIMPLE_SORT
}

void stableSortInt8(int8_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(int8_t, pArr, memberCount, simpleSortInt8, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortInt16(int16_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(int16_t, pArr, memberCount, simpleSortInt16, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortInt32(int32_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(int32_t, pArr, memberCount, simpleSortInt32, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortInt64(int64_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(int64_t, pArr, memberCount, simpleSortInt64, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

void stableSortUInt8(uint8_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(uint8_t, pArr, memberCount, simpleSortUInt8, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortUInt16(uint16_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(uint16_t, pArr, memberCount, simpleSortUInt16, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortUInt32(uint32_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(uint32_t, pArr, memberCount, simpleSortUInt32, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortUInt64(uint64_t *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(uint64_t, pArr, memberCount, simpleSortUInt64, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

void stableSortFloat(float *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(float, pArr, memberCount, simpleSortFloat, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void stableSortDouble(double *pArr, size_t memberCount)
{
	INSERTION_SORT_IMPL(double, pArr, memberCount, simpleSortDouble, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}


// PARTITION
//V_RET_NOT_NULL, function:partitionImpl
static char* partitionImpl(char* pBegin, char* pEnd, char* pPivot, size_t memberSize, LessFn less, void* pUserData)
{
	PARTITION_IMPL(char, pBegin, pEnd, pPivot, LESS_GENERIC, CREATE_TEMP_GENERIC, DESTROY_TEMP_GENERIC, COPY_GENERIC, PTR_INC_GENERIC)
}
size_t partition(void* pData, size_t pivot, size_t memberCount, size_t memberSize, LessFn less, void* pUserData)
{
	if (memberCount == 0)
		return 0;
	char* pPivot = partitionImpl((char*)pData, (char*)pData + memberSize * memberCount, 
		(char*)pData + pivot * memberSize, memberSize, less, pUserData);
	return (pPivot - (char*)pData) / memberSize;
}

//V_RET_NOT_NULL, function:partitionImplInt8
static int8_t* partitionImplInt8(int8_t* pBegin, int8_t* pEnd, int8_t* pPivot)
{
	PARTITION_IMPL(int8_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionInt8(int8_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	int8_t* pPivot = partitionImplInt8(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}

//V_RET_NOT_NULL, function:partitionImplInt16
static int16_t* partitionImplInt16(int16_t* pBegin, int16_t* pEnd, int16_t* pPivot)
{
	PARTITION_IMPL(int16_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionInt16(int16_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	int16_t* pPivot = partitionImplInt16(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplInt32
static int32_t* partitionImplInt32(int32_t* pBegin, int32_t* pEnd, int32_t* pPivot)
{
	PARTITION_IMPL(int32_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionInt32(int32_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	int32_t* pPivot = partitionImplInt32(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplInt64
static int64_t* partitionImplInt64(int64_t* pBegin, int64_t* pEnd, int64_t* pPivot)
{
	PARTITION_IMPL(int64_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionInt64(int64_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	int64_t* pPivot = partitionImplInt64(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}

//V_RET_NOT_NULL, function:partitionImplUInt8
static uint8_t* partitionImplUInt8(uint8_t* pBegin, uint8_t* pEnd, uint8_t* pPivot)
{
	PARTITION_IMPL(uint8_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
//V_RET_NOT_NULL, function:partitionUInt8
size_t partitionUInt8(uint8_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	uint8_t* pPivot = partitionImplUInt8(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplUInt16
static uint16_t* partitionImplUInt16(uint16_t* pBegin, uint16_t* pEnd, uint16_t* pPivot)
{
	PARTITION_IMPL(uint16_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
//V_RET_NOT_NULL, function:partitionUInt16
size_t partitionUInt16(uint16_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	uint16_t* pPivot = partitionImplUInt16(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplUInt32
static uint32_t* partitionImplUInt32(uint32_t* pBegin, uint32_t* pEnd, uint32_t* pPivot)
{
	PARTITION_IMPL(uint32_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionUInt32(uint32_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	uint32_t* pPivot = partitionImplUInt32(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplUInt64
static uint64_t* partitionImplUInt64(uint64_t* pBegin, uint64_t* pEnd, uint64_t* pPivot)
{
	PARTITION_IMPL(uint64_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
//V_RET_NOT_NULL, function:partitionUInt64
size_t partitionUInt64(uint64_t* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	uint64_t* pPivot = partitionImplUInt64(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}

//V_RET_NOT_NULL, function:partitionImplFloat
static float* partitionImplFloat(float* pBegin, float* pEnd, float* pPivot)
{
	PARTITION_IMPL(float, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionFloat(float* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	float* pPivot = partitionImplFloat(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}
//V_RET_NOT_NULL, function:partitionImplDouble
static double* partitionImplDouble(double* pBegin, double* pEnd, double* pPivot)
{
	PARTITION_IMPL(double, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC)
}
size_t partitionDouble(double* pArr, size_t pivot, size_t memberCount)
{
	if (memberCount == 0)
		return 0;
	double* pPivot = partitionImplDouble(pArr, pArr + memberCount, pArr + pivot);
	return pPivot - pArr;
}

static void quickSort(char* pBegin, char* pEnd, char* tmp, size_t memberSize, LessFn less, void* pUserData)
{
#define QUICKSORT_IMPL_FN(pBegin, pEnd, tmp) quickSort(pBegin, pEnd, tmp, memberSize, less, pUserData);
#define FALLBACK_SORT(pArr, memberCount) stableSort(pArr, memberCount, memberSize, less, pUserData)
#define PARTITION_IMPL_FN(pBegin, pEnd, pPivot) partitionImpl(pBegin, pEnd, pPivot, memberSize, less, pUserData)

	QUICKSORT_IMPL(char, pBegin, pEnd, tmp, QUICKSORT_IMPL_FN, FALLBACK_SORT, PARTITION_IMPL_FN, LESS_GENERIC, COPY_GENERIC, PTR_ADD_GENERIC, PTR_SUB_GENERIC, PTR_DIFF_GENERIC)
#undef QUICKSORT_IMPL_FN
#undef FALLBACK_SORT
#undef PARTITION_IMPL_FN
}

void sort(void* pData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData)
{
	CREATE_TEMP_GENERIC(char, tmp);
	quickSort((char*)pData, (char*)pData + memberCount * memberSize, tmp, memberSize, less, pUserData);
	DESTROY_TEMP_GENERIC(tmp);
}

static void quickSortInt8(int8_t* pBegin, int8_t* pEnd, int8_t* tmp)
{
	QUICKSORT_IMPL(int8_t, pBegin, pEnd, tmp, quickSortInt8, stableSortInt8, partitionImplInt8, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortInt8(int8_t* pData, size_t memberCount)
{
	int8_t tmp;
	quickSortInt8(pData, pData + memberCount, &tmp);
}
static void quickSortInt16(int16_t* pBegin, int16_t* pEnd, int16_t* tmp)
{
	QUICKSORT_IMPL(int16_t, pBegin, pEnd, tmp, quickSortInt16, stableSortInt16, partitionImplInt16, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortInt16(int16_t* pData, size_t memberCount)
{
	int16_t tmp;
	quickSortInt16(pData, pData + memberCount, &tmp);
}
static void quickSortInt32(int32_t* pBegin, int32_t* pEnd, int32_t* tmp)
{
	QUICKSORT_IMPL(int32_t, pBegin, pEnd, tmp, quickSortInt32, stableSortInt32, partitionImplInt32, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortInt32(int32_t* pData, size_t memberCount)
{
	int32_t tmp;
	quickSortInt32(pData, pData + memberCount, &tmp);
}
static void quickSortInt64(int64_t* pBegin, int64_t* pEnd, int64_t* tmp)
{
	QUICKSORT_IMPL(int64_t, pBegin, pEnd, tmp, quickSortInt64, stableSortInt64, partitionImplInt64, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortInt64(int64_t* pData, size_t memberCount)
{
	int64_t tmp;
	quickSortInt64(pData, pData + memberCount, &tmp);
}

static void quickSortUInt8(uint8_t* pBegin, uint8_t* pEnd, uint8_t* tmp)
{
	QUICKSORT_IMPL(uint8_t, pBegin, pEnd, tmp, quickSortUInt8, stableSortUInt8, partitionImplUInt8, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortUInt8(uint8_t* pData, size_t memberCount)
{
	uint8_t tmp;
	quickSortUInt8(pData, pData + memberCount, &tmp);
}
static void quickSortUInt16(uint16_t* pBegin, uint16_t* pEnd, uint16_t* tmp)
{
	QUICKSORT_IMPL(uint16_t, pBegin, pEnd, tmp, quickSortUInt16, stableSortUInt16, partitionImplUInt16, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortUInt16(uint16_t* pData, size_t memberCount)
{
	uint16_t tmp;
	quickSortUInt16(pData, pData + memberCount, &tmp);
}
static void quickSortUInt32(uint32_t* pBegin, uint32_t* pEnd, uint32_t* tmp)
{
	QUICKSORT_IMPL(uint32_t, pBegin, pEnd, tmp, quickSortUInt32, stableSortUInt32, partitionImplUInt32, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortUInt32(uint32_t* pData, size_t memberCount)
{
	uint32_t tmp;
	quickSortUInt32(pData, pData + memberCount, &tmp);
}
static void quickSortUInt64(uint64_t* pBegin, uint64_t* pEnd, uint64_t* tmp)
{
	QUICKSORT_IMPL(uint64_t, pBegin, pEnd, tmp, quickSortUInt64, stableSortUInt64, partitionImplUInt64, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortUInt64(uint64_t* pData, size_t memberCount)
{
	uint64_t tmp;
	quickSortUInt64(pData, pData + memberCount, &tmp);
}


static void quickSortFloat(float* pBegin, float* pEnd, float* tmp)
{
	QUICKSORT_IMPL(float, pBegin, pEnd, tmp, quickSortFloat, stableSortFloat, partitionImplFloat, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortFloat(float* pData, size_t memberCount)
{
	float tmp;
	quickSortFloat(pData, pData + memberCount, &tmp);
}
static void quickSortDouble(double* pBegin, double* pEnd, double* tmp)
{
	QUICKSORT_IMPL(double, pBegin, pEnd, tmp, quickSortDouble, stableSortDouble, partitionImplDouble, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void sortDouble(double* pData, size_t memberCount)
{
	double tmp;
	quickSortDouble(pData, pData + memberCount, &tmp);
}
