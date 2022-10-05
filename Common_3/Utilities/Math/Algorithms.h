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

#pragma once
#include <stdbool.h>

#include "../../Application/Config.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


typedef bool(*LessFn)(const void* pLhs, const void *pRhs, void* pUserData);

/*
 * Generic C algorithms
 * 2-8 times slower than non-generic functions. 
 * Use macros from AlgorithmsImpl to create non-generic functions for performance critical code
*/

void sort(void* pData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

void stableSort(void* pData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

size_t partition(void* pData, size_t pivot, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

/*
 * Numeric C algorithms
*/

void sortInt8(int8_t* pData, size_t memberCount);
void sortInt16(int16_t* pData, size_t memberCount);
void sortInt32(int32_t* pData, size_t memberCount);
void sortInt64(int64_t* pData, size_t memberCount);

void sortUInt8(uint8_t* pData, size_t memberCount);
void sortUInt16(uint16_t* pData, size_t memberCount);
void sortUInt32(uint32_t* pData, size_t memberCount);
void sortUInt64(uint64_t* pData, size_t memberCount);

void sortFloat(float* pData, size_t memberCount);
void sortDouble(double* pData, size_t memberCount);



void stableSortInt8(int8_t* pData, size_t memberCount);
void stableSortInt16(int16_t* pData, size_t memberCount);
void stableSortInt32(int32_t* pData, size_t memberCount);
void stableSortInt64(int64_t* pData, size_t memberCount);

void stableSortUInt8(uint8_t* pData, size_t memberCount);
void stableSortUInt16(uint16_t* pData, size_t memberCount);
void stableSortUInt32(uint32_t* pData, size_t memberCount);
void stableSortUInt64(uint64_t* pData, size_t memberCount);

void stableSortFloat(float* pData, size_t memberCount);
void stableSortDouble(double* pData, size_t memberCount);

size_t partitionInt8(int8_t* pData, size_t pivot, size_t memberCount);
size_t partitionInt16(int16_t* pData, size_t pivot, size_t memberCount);
size_t partitionInt32(int32_t* pData, size_t pivot, size_t memberCount);
size_t partitionInt64(int64_t* pData, size_t pivot, size_t memberCount);

size_t partitionUInt8(uint8_t* pData, size_t pivot, size_t memberCount);
size_t partitionUInt16(uint16_t* pData, size_t pivot, size_t memberCount);
size_t partitionUInt32(uint32_t* pData, size_t pivot, size_t memberCount);
size_t partitionUInt64(uint64_t* pData, size_t pivot, size_t memberCount);

size_t partitionFloat(float* pData, size_t pivot, size_t memberCount);
size_t partitionDouble(double* pData, size_t pivot, size_t memberCount);

#ifdef __cplusplus
}
#endif // __cplusplus
