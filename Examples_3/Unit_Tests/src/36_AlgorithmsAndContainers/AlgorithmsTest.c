/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../../../Common_3/Utilities/Interfaces/ILog.h"

#include "../../../../Common_3/Utilities/Math/Algorithms.h"

//-V:TEST_STABLE_SORT:736
#define TEST_STABLE_SORT(arr, expected, comp)                                 \
    (checkassert(sizeof(arr) == sizeof(expected) && &arr[0] != &expected[0]), \
     testStableSortImpl(arr, expected, sizeof(arr) / sizeof(arr[0]), sizeof(arr[0]), comp))

#define ARR_SIZE(arr) sizeof(arr) / sizeof(arr[0])

static void checkassert(bool expr) { ASSERT(expr); }

static int testStableSortImpl(void* pArray, void* pExpected, size_t memberCount, size_t memberSize, LessFn comp)
{
    stableSort(pArray, memberCount, memberSize, comp, NULL);
    return memcmp(pArray, pExpected, memberCount * memberSize);
}

static bool intCompare(const void* pLhs, const void* pRhs, void* pUserData)
{
    UNREF_PARAM(pUserData);
    int lhs = *(int*)pLhs;
    int rhs = *(int*)pRhs;
    // use negative numbers to show duplicates
    lhs = abs(lhs);
    rhs = abs(rhs);
    return lhs < rhs;
}

static void sprintfIntArr(char* buf, size_t bufSize, int* arr, size_t size)
{
    char* current = buf;
    for (size_t i = 0; i < size; ++i)
    {
        current += snprintf(current, bufSize - (current - buf), "%d, ", arr[i]);
    }
}
//-V:RUN_STABLE_SORT_TEST:736, 627
#define RUN_STABLE_SORT_TEST(arr, exp, comp, strBuf)                 \
    if (TEST_STABLE_SORT(arr, exp, intCompare) != 0)                 \
    {                                                                \
        sprintfIntArr(strBuf, ARR_SIZE(strBuf), exp, ARR_SIZE(exp)); \
        LOGF(eERROR, "Expected: %s", strBuf);                        \
        sprintfIntArr(strBuf, ARR_SIZE(strBuf), arr, ARR_SIZE(arr)); \
        LOGF(eERROR, "     Got: %s", strBuf);                        \
        ASSERT(false);                                               \
        return -1;                                                   \
    }

int testStableSort(void)
{
    char strBuf[2048];
    {
        int arr[] = { 1 };
        int exp[] = { 1 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 1, 2 };
        int exp[] = { 1, 2 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 2, 1 };
        int exp[] = { 1, 2 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 1, -1 };
        int exp[] = { 1, -1 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { -1, 1 };
        int exp[] = { -1, 1 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 1, 2, 3 };
        int exp[] = { 1, 2, 3 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 3, 2, 1 };
        int exp[] = { 1, 2, 3 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 1, 2, -2 };
        int exp[] = { 1, 2, -2 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 1, -2, 2 };
        int exp[] = { 1, -2, 2 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 3, -2, 2 };
        int exp[] = { -2, 2, 3 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 3, -3, 1, -1, 1, -2, 2 };
        int exp[] = { 1, -1, 1, -2, 2, 3, -3 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    {
        int arr[] = { 3, -2, 1, -3, -1, 2, 1 };
        int exp[] = { 1, -1, 1, -2, 2, 3, -3 };

        RUN_STABLE_SORT_TEST(arr, exp, intCompare, strBuf);
    }

    return 0;
}
