/**********************************************************************
 Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ********************************************************************/

#include <metal_stdlib>
using namespace metal;

#ifndef UPDATE_FREQ_PER_DRAW
#define UPDATE_FREQ_PER_DRAW 0
#endif

#ifndef UPDATE_FREQ_USER
#define UPDATE_FREQ_USER 1
#endif

#define GROUP_SIZE 64
#define NUMBER_OF_BLOCKS_PER_GROUP 8
#define NUM_BINS 16

#ifdef TARGET_IOS
#define USE_SIMD 0
#else
#define USE_SIMD 1
#endif

// [numthreads(64, 1, 1)]

// Multitype macros to handle parallel primitives
template <typename T>
inline T safe_load_4(const device T* source, uint idx, uint sizeInTypeUnits)
{
    auto res = T(0, 0, 0, 0);
    if (((idx + 1) << 2)  <= sizeInTypeUnits)
        res = source[idx];
    else
    {
        if ((idx << 2) < sizeInTypeUnits) res.x = source[idx].x;
        if ((idx << 2) + 1 < sizeInTypeUnits) res.y = source[idx].y;
        if ((idx << 2) + 2 < sizeInTypeUnits) res.z = source[idx].z;
    }
    return res;
}

template <typename T>
inline void safe_store_4(T val, device T* dest, uint idx, uint sizeInTypeUnits)
{
    if ((idx + 1) * 4  <= sizeInTypeUnits)
        dest[idx] = val;
    else
    {
        if (idx*4 < sizeInTypeUnits) dest[idx].x = val.x;
        if (idx*4 + 1 < sizeInTypeUnits) dest[idx].y = val.y;
        if (idx*4 + 2 < sizeInTypeUnits) dest[idx].z = val.z;
    }
}

template <typename type>
inline void group_scan_exclusive(int localId, int groupSize, threadgroup type* shmem)
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + shmem[(2*localId + 1)*stride-1];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    if (localId == 0)
        shmem[groupSize - 1] = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            type temp = shmem[(2*localId + 1)*stride-1];
            shmem[(2*localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1];
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + temp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

#if USE_SIMD
template <typename type>
inline void group_scan_exclusive_simd(uint localId, uint groupSize, uint simdLaneId, uint threadsPerSIMDGroup, uint simdGroupId, threadgroup type* shmem)
{
    
    type value = (localId < groupSize) ? shmem[localId] : 0;
    
    type prefixSum = simd_prefix_exclusive_sum(value);
    
    if (groupSize <= threadsPerSIMDGroup) {
        shmem[localId] = prefixSum;
        return;
    }
    
    if (simdLaneId == threadsPerSIMDGroup - 1) {
        shmem[simdGroupId] = prefixSum;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    const ushort simdGroupCount = groupSize / threadsPerSIMDGroup;
    
    type simdGroupValue = 0;
    if (localId < simdGroupCount) {
        simdGroupValue = shmem[localId];
    }
    
    type simdGroupSum = simd_prefix_exclusive_sum(simdGroupValue);
    
    if (localId < simdGroupCount) {
        shmem[localId] = simdGroupSum;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    type simdgroupTotal = shmem[simdGroupId];
    if (localId < groupSize) {
        shmem[localId] = simdgroupTotal + prefixSum;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
}
#endif // USE_SIMD

template <typename type, typename T>
inline void group_scan_exclusive_sum(int localId, int groupSize, threadgroup type* shmem, T sum)
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + shmem[(2*localId + 1)*stride-1];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    *sum = shmem[groupSize - 1];
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (localId == 0){
        shmem[groupSize - 1] = 0;}
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            type temp = shmem[(2*localId + 1)*stride-1];
            shmem[(2*localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1];
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + temp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}


template <typename type>
inline type group_scan_exclusive_part( int localId, int groupSize, threadgroup type* shmem)
{
    type sum = 0;
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + shmem[(2*localId + 1)*stride-1];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    if (localId == 0)
    {
        sum = shmem[groupSize - 1];
        shmem[groupSize - 1] = 0;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            type temp = shmem[(2*localId + 1)*stride-1];
            shmem[(2*localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1];
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + temp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    return sum;
}

template <typename type>
kernel void scan_exclusive(device type const* inputArray, device type* outputArray, threadgroup type* shmem, uint globalId [[ thread_position_in_grid ]], uint localId [[ thread_position_in_threadgroup ]], uint groupSize [[ threads_per_threadgroup ]], uint groupId [[ threadgroup_position_in_grid ]]
#if USE_SIMD
						   , uint simdLaneId [[ thread_index_in_simdgroup ]],
						   uint threadsPerSIMDGroup [[ threads_per_simdgroup ]],
						   uint simdGroupId [[ simdgroup_index_in_threadgroup ]]
#endif
						   )
{
    shmem[localId] = inputArray[2*globalId] + inputArray[2*globalId + 1];
    threadgroup_barrier(mem_flags::mem_threadgroup);
#if USE_SIMD
	group_scan_exclusive_simd(localId, groupSize, simdLaneId, threadsPerSIMDGroup, simdGroupId, shmem);
#else
	group_scan_exclusive(localId, groupSize, shmem);
#endif
    outputArray[2 * globalId + 1] = shmem[localId] + inputArray[2*globalId];
    outputArray[2 * globalId] = shmem[localId];
}

//constant VSData& vsData                                     [[buffer(UPDATE_FREQ_PER_DRAW)]],
//constant particleRootConstantBlock& particleRootConstant    [[buffer(UPDATE_FREQ_USER)]]

#if USE_SIMD
#define DEFINE_SCAN_EXCLUSIVE_4(type)\
struct ScanExclusiveArgs_##type##4 {\
device type##4 const* inputArray;\
device type##4* outputArray;\
};\
kernel void scan_exclusive_##type##4(constant ScanExclusiveArgs_##type##4 &arguments [[buffer(UPDATE_FREQ_PER_DRAW)]], constant uint& elementCountRootConstant [[buffer(UPDATE_FREQ_USER)]], uint globalId [[ thread_position_in_grid ]], uint localId [[ thread_position_in_threadgroup ]], uint groupSize [[ threads_per_threadgroup ]], uint simdLaneId [[ thread_index_in_simdgroup ]], uint threadsPerSIMDGroup [[ threads_per_simdgroup ]], uint simdGroupId [[ simdgroup_index_in_threadgroup ]]) {\
threadgroup type shmem[GROUP_SIZE];\
type##4 v1 = safe_load_4(arguments.inputArray, 2*globalId, elementCountRootConstant);\
type##4 v2 = safe_load_4(arguments.inputArray, 2*globalId + 1, elementCountRootConstant);\
v1.y += v1.x; v1.w += v1.z; v1.w += v1.y;\
v2.y += v2.x; v2.w += v2.z; v2.w += v2.y;\
v2.w += v1.w;\
shmem[localId] = v2.w;\
threadgroup_barrier(mem_flags::mem_threadgroup);\
group_scan_exclusive_simd(localId, groupSize, simdLaneId, threadsPerSIMDGroup, simdGroupId, shmem);\
v2.w = shmem[localId];\
type t = v1.w; v1.w = v2.w; v2.w += t;\
t = v1.y; v1.y = v1.w; v1.w += t;\
t = v2.y; v2.y = v2.w; v2.w += t;\
t = v1.x; v1.x = v1.y; v1.y += t;\
t = v2.x; v2.x = v2.y; v2.y += t;\
t = v1.z; v1.z = v1.w; v1.w += t;\
t = v2.z; v2.z = v2.w; v2.w += t;\
safe_store_4(v2, arguments.outputArray, 2 * globalId + 1, elementCountRootConstant);\
safe_store_4(v1, arguments.outputArray, 2 * globalId, elementCountRootConstant);\
}
#else
#define DEFINE_SCAN_EXCLUSIVE_4(type)\
struct ScanExclusiveArgs_##type##4 {\
device type##4 const* inputArray;\
device type##4* outputArray;\
};\
kernel void scan_exclusive_##type##4(constant ScanExclusiveArgs_##type##4 &arguments [[buffer(UPDATE_FREQ_PER_DRAW)]], constant uint& elementCountRootConstant [[buffer(UPDATE_FREQ_USER)]], uint globalId [[ thread_position_in_grid ]], uint localId [[ thread_position_in_threadgroup ]], uint groupSize [[ threads_per_threadgroup ]]) {\
threadgroup type shmem[GROUP_SIZE];\
type##4 v1 = safe_load_4(arguments.inputArray, 2*globalId, elementCountRootConstant);\
type##4 v2 = safe_load_4(arguments.inputArray, 2*globalId + 1, elementCountRootConstant);\
v1.y += v1.x; v1.w += v1.z; v1.w += v1.y;\
v2.y += v2.x; v2.w += v2.z; v2.w += v2.y;\
v2.w += v1.w;\
shmem[localId] = v2.w;\
threadgroup_barrier(mem_flags::mem_threadgroup);\
group_scan_exclusive(localId, groupSize, shmem); \
v2.w = shmem[localId];\
type t = v1.w; v1.w = v2.w; v2.w += t;\
t = v1.y; v1.y = v1.w; v1.w += t;\
t = v2.y; v2.y = v2.w; v2.w += t;\
t = v1.x; v1.x = v1.y; v1.y += t;\
t = v2.x; v2.x = v2.y; v2.y += t;\
t = v1.z; v1.z = v1.w; v1.w += t;\
t = v2.z; v2.z = v2.w; v2.w += t;\
safe_store_4(v2, arguments.outputArray, 2 * globalId + 1, elementCountRootConstant);\
safe_store_4(v1, arguments.outputArray, 2 * globalId, elementCountRootConstant);\
}
#endif // USE_SIMD

DEFINE_SCAN_EXCLUSIVE_4(int)
DEFINE_SCAN_EXCLUSIVE_4(float)

#define DEFINE_SCAN_EXCLUSIVE_PART_4(type)\
struct ScanExclusivePartArgs_##type##4 {\
device type##4 const* inputArray;\
device type##4* outputArray;\
device type* outputSums;\
};\
kernel void scan_exclusive_part_##type##4(constant ScanExclusivePartArgs_##type##4& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]], constant uint& elementCountRootConstant [[ buffer(UPDATE_FREQ_USER) ]], uint globalId [[ thread_position_in_grid ]], uint localId [[ thread_position_in_threadgroup ]], uint groupSize [[ threads_per_threadgroup ]], uint groupId [[ threadgroup_position_in_grid ]]) {\
threadgroup type shmem[GROUP_SIZE];\
type##4 v1 = safe_load_4(arguments.inputArray, 2*globalId, elementCountRootConstant);\
type##4 v2 = safe_load_4(arguments.inputArray, 2*globalId + 1, elementCountRootConstant);\
v1.y += v1.x; v1.w += v1.z; v1.w += v1.y;\
v2.y += v2.x; v2.w += v2.z; v2.w += v2.y;\
v2.w += v1.w;\
shmem[localId] = v2.w;\
threadgroup_barrier(mem_flags::mem_threadgroup);\
type sum = group_scan_exclusive_part(localId, groupSize, shmem);\
if (localId == 0) arguments.outputSums[groupId] = sum;\
v2.w = shmem[localId];\
type t = v1.w; v1.w = v2.w; v2.w += t;\
t = v1.y; v1.y = v1.w; v1.w += t;\
t = v2.y; v2.y = v2.w; v2.w += t;\
t = v1.x; v1.x = v1.y; v1.y += t;\
t = v2.x; v2.x = v2.y; v2.y += t;\
t = v1.z; v1.z = v1.w; v1.w += t;\
t = v2.z; v2.z = v2.w; v2.w += t;\
safe_store_4(v2, arguments.outputArray, 2 * globalId + 1, elementCountRootConstant);\
safe_store_4(v1, arguments.outputArray, 2 * globalId, elementCountRootConstant);\
}

DEFINE_SCAN_EXCLUSIVE_PART_4(int)
DEFINE_SCAN_EXCLUSIVE_PART_4(float)
DEFINE_SCAN_EXCLUSIVE_PART_4(uchar)

template <typename type>
inline void group_reduce(int localId, int groupSize, threadgroup type* shmem)
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize/(2*stride))
        {
            shmem[2*(localId + 1)*stride-1] = shmem[2*(localId + 1)*stride-1] + shmem[(2*localId + 1)*stride-1];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

#define DEFINE_DISTRIBUTE_PART_SUM_4(type)\
struct DistributePartSumArgs_##type##4 {\
device type const* inputSums;\
device type##4* inoutArray;\
};\
kernel void distribute_part_sum_##type##4(constant DistributePartSumArgs_##type##4& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]], constant uint& elementCountRootConstant [[ buffer(UPDATE_FREQ_USER) ]], uint globalId [[ thread_position_in_grid ]], uint groupId [[ threadgroup_position_in_grid ]])\
{\
type##4 v1 = safe_load_4(arguments.inoutArray, globalId, elementCountRootConstant);\
type    sum = arguments.inputSums[groupId >> 1];\
v1.xyzw += sum;\
safe_store_4(v1, arguments.inoutArray, globalId, elementCountRootConstant);\
}

DEFINE_DISTRIBUTE_PART_SUM_4(int)
DEFINE_DISTRIBUTE_PART_SUM_4(float)
DEFINE_DISTRIBUTE_PART_SUM_4(uchar)

template <typename T>
inline void atom_inc(T object) {
    atomic_fetch_add_explicit(object, 1, memory_order::memory_order_relaxed);
}

/// Specific function for radix-sort needs
/// Group exclusive add multiscan on 4 arrays of shorts in parallel
/// with 4x reduction in registers
template<typename T, typename U>
void group_scan_short_4way(int localId, int groupSize,
                           short4 mask0,
                           short4 mask1,
                           short4 mask2,
                           short4 mask3,
                           threadgroup short* shmem0,
                           threadgroup short* shmem1,
                           threadgroup short* shmem2,
                           threadgroup short* shmem3,
                           T offset0,
                           T offset1,
                           T offset2,
                           T offset3,
                           U histogram)
{
    short4 v1 = mask0;
    v1.y += v1.x; v1.w += v1.z; v1.w += v1.y;
    shmem0[localId] = v1.w;
    
    short4 v2 = mask1;
    v2.y += v2.x; v2.w += v2.z; v2.w += v2.y;
    shmem1[localId] = v2.w;
    
    short4 v3 = mask2;
    v3.y += v3.x; v3.w += v3.z; v3.w += v3.y;
    shmem2[localId] = v3.w;
    
    short4 v4 = mask3;
    v4.y += v4.x; v4.w += v4.z; v4.w += v4.y;
    shmem3[localId] = v4.w;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            shmem0[2 * (localId + 1)*stride - 1] = shmem0[2 * (localId + 1)*stride - 1] + shmem0[(2 * localId + 1)*stride - 1];
            shmem1[2 * (localId + 1)*stride - 1] = shmem1[2 * (localId + 1)*stride - 1] + shmem1[(2 * localId + 1)*stride - 1];
            shmem2[2 * (localId + 1)*stride - 1] = shmem2[2 * (localId + 1)*stride - 1] + shmem2[(2 * localId + 1)*stride - 1];
            shmem3[2 * (localId + 1)*stride - 1] = shmem3[2 * (localId + 1)*stride - 1] + shmem3[(2 * localId + 1)*stride - 1];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    short4 total;
    total.x = shmem0[groupSize - 1];
    total.y = shmem1[groupSize - 1];
    total.z = shmem2[groupSize - 1];
    total.w = shmem3[groupSize - 1];
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    if (localId == 0)
    {
        shmem0[groupSize - 1] = 0;
        shmem1[groupSize - 1] = 0;
        shmem2[groupSize - 1] = 0;
        shmem3[groupSize - 1] = 0;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            int temp = shmem0[(2 * localId + 1)*stride - 1];
            shmem0[(2 * localId + 1)*stride - 1] = shmem0[2 * (localId + 1)*stride - 1];
            shmem0[2 * (localId + 1)*stride - 1] = shmem0[2 * (localId + 1)*stride - 1] + temp;
            
            temp = shmem1[(2 * localId + 1)*stride - 1];
            shmem1[(2 * localId + 1)*stride - 1] = shmem1[2 * (localId + 1)*stride - 1];
            shmem1[2 * (localId + 1)*stride - 1] = shmem1[2 * (localId + 1)*stride - 1] + temp;
            
            temp = shmem2[(2 * localId + 1)*stride - 1];
            shmem2[(2 * localId + 1)*stride - 1] = shmem2[2 * (localId + 1)*stride - 1];
            shmem2[2 * (localId + 1)*stride - 1] = shmem2[2 * (localId + 1)*stride - 1] + temp;
            
            temp = shmem3[(2 * localId + 1)*stride - 1];
            shmem3[(2 * localId + 1)*stride - 1] = shmem3[2 * (localId + 1)*stride - 1];
            shmem3[2 * (localId + 1)*stride - 1] = shmem3[2 * (localId + 1)*stride - 1] + temp;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    v1.w = shmem0[localId];
    
    short t = v1.y; v1.y = v1.w; v1.w += t;
    t = v1.x; v1.x = v1.y; v1.y += t;
    t = v1.z; v1.z = v1.w; v1.w += t;
    *offset0 = v1;
    
    v2.w = shmem1[localId];
    
    t = v2.y; v2.y = v2.w; v2.w += t;
    t = v2.x; v2.x = v2.y; v2.y += t;
    t = v2.z; v2.z = v2.w; v2.w += t;
    *offset1 = v2;
    
    v3.w = shmem2[localId];
    
    t = v3.y; v3.y = v3.w; v3.w += t;
    t = v3.x; v3.x = v3.y; v3.y += t;
    t = v3.z; v3.z = v3.w; v3.w += t;
    *offset2 = v3;
    
    v4.w = shmem3[localId];
    
    t = v4.y; v4.y = v4.w; v4.w += t;
    t = v4.x; v4.x = v4.y; v4.y += t;
    t = v4.z; v4.z = v4.w; v4.w += t;
    *offset3 = v4;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    *histogram = total;
}

// Calculate bool radix mask
short4 radix_mask(int offset, uchar digit, uint4 val)
{
    short4 res;
    res.x = ((val.x >> offset) & 3) == digit ? 1 : 0;
    res.y = ((val.y >> offset) & 3) == digit ? 1 : 0;
    res.z = ((val.z >> offset) & 3) == digit ? 1 : 0;
    res.w = ((val.w >> offset) & 3) == digit ? 1 : 0;
    return res;
}

// Choose offset based on radix mask value
short offset_4way(int val, int offset, short offset0, short offset1, short offset2, short offset3, short4 hist)
{
    switch ((val >> offset) & 3)
    {
        case 0:
            return offset0;
        case 1:
            return offset1 + hist.x;
        case 2:
            return offset2 + hist.x + hist.y;
        case 3:
            return offset3 + hist.x + hist.y + hist.z;
    }
    
    return 0;
}



// Perform group split using 2-bits pass
template <typename T, typename U>
void group_split_radix_2bits(
                             int localId,
                             int groupSize,
                             int offset,
                             uint4 val,
                             threadgroup short* shmem,
                             T localOffset,
                             U histogram)
{
    /// Pointers to radix flag arrays
    threadgroup short* shmem0 = shmem;
    threadgroup short* shmem1 = shmem0 + groupSize;
    threadgroup short* shmem2 = shmem1 + groupSize;
    threadgroup short* shmem3 = shmem2 + groupSize;
    
    /// Radix masks for each digit
    short4 mask0 = radix_mask(offset, 0, val);
    short4 mask1 = radix_mask(offset, 1, val);
    short4 mask2 = radix_mask(offset, 2, val);
    short4 mask3 = radix_mask(offset, 3, val);
    
    /// Resulting offsets
    short4 offset0;
    short4 offset1;
    short4 offset2;
    short4 offset3;
    
    group_scan_short_4way(localId, groupSize,
                          mask0, mask1, mask2, mask3,
                          shmem0, shmem1, shmem2, shmem3,
                          &offset0, &offset1, &offset2, &offset3,
                          histogram);
    
    (*localOffset).x = offset_4way(val.x, offset, offset0.x, offset1.x, offset2.x, offset3.x, *histogram);
    (*localOffset).y = offset_4way(val.y, offset, offset0.y, offset1.y, offset2.y, offset3.y, *histogram);
    (*localOffset).z = offset_4way(val.z, offset, offset0.z, offset1.z, offset2.z, offset3.z, *histogram);
    (*localOffset).w = offset_4way(val.w, offset, offset0.w, offset1.w, offset2.w, offset3.w, *histogram);
}

template <typename T>
uint4 safe_load_uint4_intmax(T source, uint idx, uint sizeInInts)
{
    uint4 res = uint4(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
    if (((idx + 1) << 2) <= sizeInInts)
        res = source[idx];
    else
    {
        if ((idx << 2) < sizeInInts) res.x = source[idx].x;
        if ((idx << 2) + 1 < sizeInInts) res.y = source[idx].y;
        if ((idx << 2) + 2 < sizeInInts) res.z = source[idx].z;
    }
    return res;
}

template <typename T>
int4 safe_load_int4_intmax(T source, uint idx, uint sizeInInts)
{
    int4 res = int4(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
    if (((idx + 1) << 2) <= sizeInInts)
        res = source[idx];
    else
    {
        if ((idx << 2) < sizeInInts) res.x = source[idx].x;
        if ((idx << 2) + 1 < sizeInInts) res.y = source[idx].y;
        if ((idx << 2) + 2 < sizeInInts) res.z = source[idx].z;
    }
    return res;
}

template <typename T>
uint4 safe_load_uint4_intmax_mask(T source, uint idx, uint sizeInInts, thread bool4 *validIndices)
{
    uint4 res = uint4(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
    bool4 valid = bool4(false, false, false, false);
    
    if (((idx + 1) << 2) <= sizeInInts) {
        res = source[idx];
        valid = bool4(true, true, true, true);
    } else
    {
        if ((idx << 2) < sizeInInts) {
            res.x = source[idx].x;
            valid.x = true;
        }
        if ((idx << 2) + 1 < sizeInInts) {
            res.y = source[idx].y;
            valid.y = true;
        }
        if ((idx << 2) + 2 < sizeInInts) {
            res.z = source[idx].z;
            valid.z = true;
        }
    }
    *validIndices = valid;
    return res;
}

template <typename T>
uint4 safe_load_uint4_intmax_validcount(T source, uint idx, uint sizeInInts, thread ushort *validCount)
{
    uint4 res = uint4(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
    
    ushort valid = 0;
    
    if (((idx + 1) << 2) <= sizeInInts) {
        res = source[idx];
        valid = 4;
    } else
    {
        if ((idx << 2) < sizeInInts) {
            res.x = source[idx].x;
            valid = 1;
        }
        if ((idx << 2) + 1 < sizeInInts) {
            res.y = source[idx].y;
            valid = 2;
        }
        if ((idx << 2) + 2 < sizeInInts) {
            res.z = source[idx].z;
            valid = 3;
        }
    }
    
    *validCount = valid;
    return res;
}

void safe_store_int(int val, device int* dest, uint idx, uint sizeInInts)
{
    if (idx < sizeInInts)
        dest[idx] = val;
}

// Split kernel launcher
kernel void split4way(constant int& bitShift, device uint4* inputArray, constant uint& elementCountRootConstant, device int* outputHistograms, device uint4* outputArray,
                      device int* out_local_histograms,
                      device uint4* out_debug_offset,
                      threadgroup short* shmem,
                      uint globalId [[ thread_position_in_grid ]],
                      uint localId [[ thread_position_in_threadgroup ]],
                      uint groupSize [[ threads_per_threadgroup ]],
                      uint groupId [[ threadgroup_position_in_grid ]],
                      uint numGroups [[ threadgroups_per_grid ]])
{
    /// Load single uint4 value
    uint4 val = safe_load_uint4_intmax(inputArray, globalId, elementCountRootConstant);
    
    uint4 localOffset;
    short4 localHistogram;
    group_split_radix_2bits(localId, groupSize, bitShift, val, shmem, &localOffset,
                            &localHistogram);
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    threadgroup int* sharedData = (threadgroup int*)shmem;
    threadgroup uint4* sharedData4 = (threadgroup uint4*)shmem;
    
    sharedData[localOffset.x] = val.x;
    sharedData[localOffset.y] = val.y;
    sharedData[localOffset.z] = val.z;
    sharedData[localOffset.w] = val.w;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    // Now store to memory
    if (((globalId + 1) << 2) <= elementCountRootConstant)
    {
        outputArray[globalId] = sharedData4[localId];
        out_debug_offset[globalId] = localOffset;
    }
    else
    {
        if ((globalId << 2) < elementCountRootConstant) outputArray[globalId].x = sharedData4[localId].x;
        if ((globalId << 2) + 1 < elementCountRootConstant) outputArray[globalId].y = sharedData4[localId].y;
        if ((globalId << 2) + 2 < elementCountRootConstant) outputArray[globalId].z = sharedData4[localId].z;
    }
    
    if (localId == 0)
    {
        outputHistograms[groupId] = localHistogram.x;
        outputHistograms[groupId + numGroups] = localHistogram.y;
        outputHistograms[groupId + 2 * numGroups] = localHistogram.z;
        outputHistograms[groupId + 3 * numGroups] = localHistogram.w;
        
        out_local_histograms[groupId] = 0;
        out_local_histograms[groupId + numGroups] = localHistogram.x;
        out_local_histograms[groupId + 2 * numGroups] = localHistogram.x + localHistogram.y;
        out_local_histograms[groupId + 3 * numGroups] = localHistogram.x + localHistogram.y + localHistogram.z;
    }
}



struct BitHistogramPushConstants {
	int bitShift;
};

struct BitHistogramArguments {
    // Input array
    device uint4 const* restrict inputArray;
    // Output histograms in column layout
    // [bin0_group0, bin0_group1, ... bin0_groupN, bin1_group0, bin1_group1, ... bin1_groupN, ...]
    device uint* restrict outHistogram;
	constant uint& elementCount;
};

// The kernel computes 16 bins histogram of the 256 input elements.
// The bin is determined by (inputArray[tid] >> bitShift) & 0xF
#if USE_SIMD
kernel void BitHistogram(
                         constant BitHistogramArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
						 constant BitHistogramPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
                         uint globalId [[ thread_position_in_grid ]],
                         uint localId [[ thread_position_in_threadgroup ]],
                         uint simdLaneId [[ thread_index_in_simdgroup ]],
                         uint simdGroupSize [[ threads_per_simdgroup ]],
                         uint simdGroupId [[ simdgroup_index_in_threadgroup ]],
                         uint simdGroupCount [[ simdgroups_per_threadgroup ]],
                         uint groupSize [[ threads_per_threadgroup ]],
                         uint groupId [[ threadgroup_position_in_grid ]],
                         uint groupCount [[ threadgroups_per_grid ]]
                         ) {
    
    uint threadCounts[16] = {0};
    
    const int blockCountPerGroup = NUMBER_OF_BLOCKS_PER_GROUP;
    const int elementCountPerGroup = blockCountPerGroup * GROUP_SIZE;
    
    int totalBlockCount = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
    int maxBlocks = totalBlockCount - groupId * blockCountPerGroup;
    
    int loadIndex = groupId * elementCountPerGroup + localId;
    
    for (int block = 0; block < min(blockCountPerGroup, maxBlocks); ++block, loadIndex += GROUP_SIZE) {
        /// Load single uint4 value
        uint4 value = safe_load_uint4_intmax(arguments.inputArray, loadIndex, arguments.elementCount);
        uint4 bin = ((value >> rootConstants.bitShift) & 0b1111);
        
        threadCounts[bin.x] += 1;
        threadCounts[bin.y] += 1;
        threadCounts[bin.z] += 1;
        threadCounts[bin.w] += 1;
    }
    
    threadgroup ushort4 totals[4]; // groupSize is 64, so the count of this should be groupSize / simdSize; conservatively assume a minimum SIMD width of 16
    
    for (ushort i = 0; i < 4; i += 1) {
        ushort4 threadCountsVal = ushort4(threadCounts[4 * i], threadCounts[4 * i + 1], threadCounts[4 * i + 2], threadCounts[4 * i + 3]);
        ushort4 count = simd_sum(threadCountsVal);
        
        if (simdGroupCount > 1) {
            if (simdLaneId == 0) {
                totals[simdGroupId] = count;
            }
            
            threadgroup_barrier(mem_flags::mem_threadgroup);
            
            if (simdGroupId == 0) {
                if (simdLaneId < simdGroupCount) {
                    count = totals[simdLaneId];
                } else {
                    count = 0;
                }
                
                count = simd_sum(count);
            }
        }
        if (localId < 4) {
            ushort baseIndex = 4 * i + localId;
            arguments.outHistogram[groupCount * baseIndex + groupId] = count[localId];
        }
    }
}
#else
kernel void BitHistogram(
							constant BitHistogramArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
							constant BitHistogramPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
                            uint globalId [[ thread_position_in_grid ]],
                            uint localId [[ thread_position_in_threadgroup ]],
                            uint groupSize [[ threads_per_threadgroup ]],
                            uint groupId [[ threadgroup_position_in_grid ]],
                            uint numGroups [[ threadgroups_per_grid ]]
                  )
{
    // Histogram storage
    threadgroup atomic_int histogram[NUM_BINS * GROUP_SIZE];
    
    /// Clear local histogram
    for (int i = 0; i < NUM_BINS; ++i)
    {
        atomic_store_explicit(&histogram[i*GROUP_SIZE + localId], 0, memory_order::memory_order_relaxed);
    }
    
    // Make sure everything is up to date
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    const int numblocks_per_group = NUMBER_OF_BLOCKS_PER_GROUP;
    const int elementCount_per_group = numblocks_per_group * GROUP_SIZE;
    
    int numblocks_total = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
    int maxblocks = numblocks_total - groupId * numblocks_per_group;
    
    int loadidx = groupId * elementCount_per_group + localId;
    for (int block = 0; block < min(numblocks_per_group, maxblocks); ++block, loadidx += GROUP_SIZE)
    {
        /// Load single int4 value
        uint4 value = safe_load_uint4_intmax(arguments.inputArray, loadidx, arguments.elementCount);
        
        /// Handle value adding histogram bins
        /// for all 4 elements
        uint4 bin = ((value >> rootConstants.bitShift) & 0xF);
        //++histogram[localId*kNumBins + bin];
        atomic_fetch_add_explicit(&histogram[bin.x*GROUP_SIZE + localId], 1, memory_order::memory_order_relaxed);
        //bin = ((value.y >> bitShift) & 0xF);
        //++histogram[localId*kNumBins + bin];
        atomic_fetch_add_explicit(&histogram[bin.y*GROUP_SIZE + localId], 1, memory_order::memory_order_relaxed);
        //bin = ((value.z >> bitShift) & 0xF);
        //++histogram[localId*kNumBins + bin];
        atomic_fetch_add_explicit(&histogram[bin.z*GROUP_SIZE + localId], 1, memory_order::memory_order_relaxed);
        //bin = ((value.w >> bitShift) & 0xF);
        //++histogram[localId*kNumBins + bin];
        atomic_fetch_add_explicit(&histogram[bin.w*GROUP_SIZE + localId], 1, memory_order::memory_order_relaxed);
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    int sum = 0;
    if (localId < NUM_BINS)
    {
        for (int i = 0; i < GROUP_SIZE; ++i)
        {
            sum += atomic_load_explicit(&histogram[localId * GROUP_SIZE + i], memory_order::memory_order_relaxed);
        }
        
        arguments.outHistogram[numGroups*localId + groupId] = sum;
    }
}
#endif // USE_SIMD

struct ScatterKeysPushConstants {
	int bitShift;
};

struct ScatterKeysArguments {
	// Input keys
	device uint4 const* restrict inputKeys;
	// Number of input keys
	constant uint& elementCount;
	// Scanned histograms
	device int const* restrict inputHistograms;
	// Output keys
	device uint* restrict outputKeys;
};

#if USE_SIMD
kernel
//__attribute__((reqd_work_group_size(GROUP_SIZE, 1, 1)))
void ScatterKeys(
				 constant ScatterKeysPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
				 constant ScatterKeysArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
				 uint globalId [[ thread_position_in_grid ]],
				 uint localId [[ thread_position_in_threadgroup ]],
				 uint groupSize [[ threads_per_threadgroup ]],
				 uint groupId [[ threadgroup_position_in_grid ]],
				 uint groupCount [[ threadgroups_per_grid ]],
				 uint simdLaneId [[ thread_index_in_simdgroup ]],
				 uint threadsPerSIMDGroup [[ threads_per_simdgroup ]],
				 uint simdGroupId [[ simdgroup_index_in_threadgroup ]]) {
	
	threadgroup uint scannedHistogram[16];
	
	// Copy scanned histogram for the group to local memory for fast indexing
	if (localId < 16) {
        scannedHistogram[localId] = arguments.inputHistograms[groupId + localId * groupCount];
    }
    
    // Make sure everything is up to date
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    threadgroup uint sortedKeys[GROUP_SIZE * 4];
    
    const int blockCountPerGroup = NUMBER_OF_BLOCKS_PER_GROUP;
    const int elementCountPerGroup = blockCountPerGroup * GROUP_SIZE;
    
    int totalBlockCount = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
    int maxBlocks = totalBlockCount - groupId * blockCountPerGroup;
    
    int loadIndex = groupId * elementCountPerGroup + localId;
    int blockCount = min(blockCountPerGroup, maxBlocks);
    
    for (int block = 0; block < blockCount; ++block, loadIndex += GROUP_SIZE) {
        
        // Radix sort to local memory, and then scatter to global memory.
        
        /// Load single uint4 value
        ushort validCount;
        uint4 keys = safe_load_uint4_intmax_validcount(arguments.inputKeys, loadIndex, arguments.elementCount, &validCount);
        
        // Do a 2-bit sort twice.
        for (uint bit = 0; bit <= 2; bit += 2) {
            uchar4 bin = uchar4((keys >> (rootConstants.bitShift + bit)) & 0b11);

            ushort4 threadCounts = ushort4(0);

            for (ushort i = 0; i < validCount; i += 1) {
                threadCounts[bin[i]] += 1;
            }

            const ushort simdGroupCount = groupSize / threadsPerSIMDGroup;

            ushort4 threadBucketOffsets; // The offset for the current thread _within_ each bucket
            threadgroup ushort4 threadgroupBucketCounts;

            // Compute a exclusive scan across each thread in the threadgroup.
            {
                threadBucketOffsets = simd_prefix_exclusive_sum(threadCounts);

                if (groupSize > threadsPerSIMDGroup) {
                    threadgroup ushort4 threadgroupTotals[4]; // groupSize is 64, so the count of this should be groupSize / simdSize; conservatively assume a minimum SIMD width of 16

                    if (simdLaneId == threadsPerSIMDGroup - 1) {
                        threadgroupTotals[simdGroupId] = threadBucketOffsets + threadCounts;
                    }

                    threadgroup_barrier(mem_flags::mem_threadgroup);

                    ushort4 simdGroupValue = 0;
                    if (localId < simdGroupCount) {
                        simdGroupValue = threadgroupTotals[localId];
                    }

                    simdGroupValue = simd_prefix_exclusive_sum(simdGroupValue); // There are at most four active lanes.
                    if (localId < simdGroupCount) {
                        threadgroupTotals[localId] = simdGroupValue;
                    }

                    threadgroup_barrier(mem_flags::mem_threadgroup);

                    threadBucketOffsets += threadgroupTotals[simdGroupId];
                }

                if (localId == groupSize - 1) {
                    threadgroupBucketCounts = threadBucketOffsets + threadCounts;
                }
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            // Convert the threadgroup per-bucket counts into per-bucket offsets.
            threadgroup ushort4 threadgroupBucketOffsets;
            if (localId == 0) {
                threadgroupBucketOffsets.x = 0;
                threadgroupBucketOffsets.y = threadgroupBucketCounts.x;
                threadgroupBucketOffsets.z = (threadgroupBucketOffsets.y + threadgroupBucketCounts.y);
                threadgroupBucketOffsets.w = (threadgroupBucketOffsets.z + threadgroupBucketCounts.z);
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            for (uchar i = 0; i < validCount; i += 1) {
                ushort offset = threadgroupBucketOffsets[bin[i]] + threadBucketOffsets[bin[i]];
                sortedKeys[offset] = keys[i];
                threadBucketOffsets[bin[i]] += 1;
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            // Retrieve the new, sorted value.
            for (ushort threadgroupLoadIndex = 4 * localId, i = 0; i < validCount; i += 1, threadgroupLoadIndex += 1) {
                keys[i] = sortedKeys[threadgroupLoadIndex];
            }
        }
        
        // At this point, sortedKeys contains all of the sorted keys from the current block. Now, we need to distribute them to device memory.
        // First, compute how many items in each threadgroup
        threadgroup ushort threadgroupBucketOffsets[16];
        threadgroup atomic_uint threadgroupBucketCountsAtomic[16];
        
        if (localId < 16) {
            atomic_store_explicit(&threadgroupBucketCountsAtomic[localId], 0, memory_order::memory_order_relaxed);
        }
        
        uchar previousBucket = 0;
        if (localId > 0) {
            previousBucket = (sortedKeys[4 * localId - 1] >> rootConstants.bitShift) & 0b1111;
        } else {
            threadgroupBucketOffsets[0] = 0;
        }
        
        uchar4 buckets = uchar4((keys >> rootConstants.bitShift) & 0b1111);
        for (ushort i = 0; i < validCount; i += 1) {
            uchar bucket = buckets[i];
            if (bucket != previousBucket) {
                threadgroupBucketOffsets[bucket] = 4 * localId + i;
                previousBucket = buckets[i];
            }
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        for (ushort threadgroupLoadIndex = 4 * localId, i = 0; i < validCount; i += 1, threadgroupLoadIndex += 1) {
            atomic_fetch_add_explicit(&threadgroupBucketCountsAtomic[buckets[i]], 1, memory_order::memory_order_relaxed);
            
            ushort bucketOffset = threadgroupBucketOffsets[buckets[i]];
            uint globalOffset = scannedHistogram[buckets[i]] + uint(threadgroupLoadIndex - bucketOffset);
            arguments.outputKeys[globalOffset] = keys[i];
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        // Update the scanned histogram with the new global offsets.
        
        if (localId < 16) {
            scannedHistogram[localId] += atomic_load_explicit(&threadgroupBucketCountsAtomic[localId], memory_order::memory_order_relaxed);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}
#else
kernel
//__attribute__((reqd_work_group_size(GROUP_SIZE, 1, 1)))
void ScatterKeys(
				 constant ScatterKeysPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
				 constant ScatterKeysArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
				 uint globalId [[ thread_position_in_grid ]],
				 uint localId [[ thread_position_in_threadgroup ]],
				 uint groupSize [[ threads_per_threadgroup ]],
				 uint groupId [[ threadgroup_position_in_grid ]],
				 uint groupCount [[ threadgroups_per_grid ]]) {
	 // Local memory for offsets counting
	 threadgroup uint  keys[GROUP_SIZE * 4];
	 threadgroup uint  scanned_histogram[NUM_BINS];
	 
	 threadgroup uint* histogram = (threadgroup uint*)keys;
	 
	 int numblocks_per_group = NUMBER_OF_BLOCKS_PER_GROUP;
	 int num_elements_per_group = numblocks_per_group * GROUP_SIZE;
	 int numblocks_total = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
	 int maxblocks = numblocks_total - groupId * numblocks_per_group;
	 
	 // Copy scanned histogram for the group to local memory for fast indexing
	 if (localId < NUM_BINS)
	 {
		 scanned_histogram[localId] = arguments.inputHistograms[groupId + localId * groupCount];
	 }
	 
	 // Make sure everything is up to date
	 threadgroup_barrier(mem_flags::mem_threadgroup);
	 
	 int loadidx = groupId * num_elements_per_group + localId;
	 for (int block = 0; block < min(numblocks_per_group, maxblocks); ++block, loadidx += GROUP_SIZE)
	 {
		 // Load single uint4 value
		 uint4 localkeys = safe_load_uint4_intmax(arguments.inputKeys, loadidx, arguments.elementCount);
		 
		 // Clear the histogram
		 histogram[localId] = 0;
		 
		 // Make sure everything is up to date
		 threadgroup_barrier(mem_flags::mem_threadgroup);
		 
		 // Do 2 bits per pass
		 for (int bit = 0; bit <= 2; bit += 2)
		 {
			 // Count histogram
			 uint4 b = ((localkeys >> rootConstants.bitShift) >> bit) & 0x3;
			 
			 uint4 p;
			 p.x = 1 << (8 * b.x);
			 p.y = 1 << (8 * b.y);
			 p.z = 1 << (8 * b.z);
			 p.w = 1 << (8 * b.w);
			 
			 // Pack the histogram
			 uint packed_key = (uint)(p.x + p.y + p.z + p.w);
			 
			 // Put into LDS
			 histogram[localId] = packed_key;
			 
			 // Make sure everything is up to date
			 threadgroup_barrier(mem_flags::mem_threadgroup);
			 
			 // Scan the histogram in LDS with 4-way plus scan
			 uint total = 0;
			 group_scan_exclusive_sum(localId, GROUP_SIZE, histogram, &total);
			 
			 // Load value back
			 packed_key = histogram[localId];
			 
			 // Make sure everything is up to date
			 threadgroup_barrier(mem_flags::mem_threadgroup);
			 
			 // Scan total histogram (4 chars)
			 total = (total << 8) + (total << 16) + (total << 24);
			 uint offset = total + packed_key;
			 
			 uint4 newoffset;
			 
			 int t = p.y + p.x;
			 p.w = p.z + t;
			 p.z = t;
			 p.y = p.x;
			 p.x = 0;
			 
			 p += (int)offset;
			 newoffset = (p >> (b * 8)) & 0xFF;
			 
			 keys[newoffset.x] = localkeys.x;
			 keys[newoffset.y] = localkeys.y;
			 keys[newoffset.z] = localkeys.z;
			 keys[newoffset.w] = localkeys.w;
			 
			 // Make sure everything is up to date
			 threadgroup_barrier(mem_flags::mem_threadgroup);
			 
			 // Reload values back to registers for the second bit pass
			 localkeys.x = keys[localId << 2];
			 localkeys.y = keys[(localId << 2) + 1];
			 localkeys.z = keys[(localId << 2) + 2];
			 localkeys.w = keys[(localId << 2) + 3];
			 
			 // Make sure everything is up to date
			 threadgroup_barrier(mem_flags::mem_threadgroup);
		 }
		 
		 // Clear LDS
		 histogram[localId] = 0;
		 
		 // Make sure everything is up to date
		 threadgroup_barrier(mem_flags::mem_threadgroup);
		 
		 threadgroup atomic_uint* histogramAtomic = (threadgroup atomic_uint*)histogram;
		 
		 // Reconstruct 16 bins histogram
		 uint4 bin = (localkeys >> rootConstants.bitShift) & 0xF;
		 atom_inc(&histogramAtomic[bin.x]);
		 atom_inc(&histogramAtomic[bin.y]);
		 atom_inc(&histogramAtomic[bin.z]);
		 atom_inc(&histogramAtomic[bin.w]);
		 
		 threadgroup_barrier(mem_flags::mem_threadgroup);
		 
		 int sum = 0;
		 if (localId < NUM_BINS)
		 {
			 sum = histogram[localId];
		 }
		 
		 // Make sure everything is up to date
		 threadgroup_barrier(mem_flags::mem_threadgroup);
		 
		 // Scan reconstructed histogram
		 group_scan_exclusive(localId, 16, histogram);
		 
		 // Put data back to global memory
		 uint offset = scanned_histogram[bin.x] + (localId << 2) - histogram[bin.x];
		 if (offset < arguments.elementCount)
		 {
			 arguments.outputKeys[offset] = localkeys.x;
		 }
		 
		 offset = scanned_histogram[bin.y] + (localId << 2) + 1 - histogram[bin.y];
		 if (offset < arguments.elementCount)
		 {
			 arguments.outputKeys[offset] = localkeys.y;
		 }
		 
		 offset = scanned_histogram[bin.z] + (localId << 2) + 2 - histogram[bin.z];
		 if (offset < arguments.elementCount)
		 {
			 arguments.outputKeys[offset] = localkeys.z;
		 }
		 
		 offset = scanned_histogram[bin.w] + (localId << 2) + 3 - histogram[bin.w];
		 if (offset < arguments.elementCount)
		 {
			 arguments.outputKeys[offset] = localkeys.w;
		 }
		 
		 threadgroup_barrier(mem_flags::mem_threadgroup);
		 
		 scanned_histogram[localId] += sum;
	 }
}
#endif // USE_SIMD

struct ScatterKeysAndValuesPushConstants {
	int bitShift;
};

struct ScatterKeysAndValuesArguments {
	// Input keys
	device uint4 const* restrict inputKeys;
	// Input values
	device uint4 const* restrict inputValues;
	// Number of input keys
	constant uint&           elementCount;
	// Scanned histograms
	device int const* restrict  inputHistograms;
	// Output keys
	device int* restrict  outputKeys;
	// Output values
	device int* restrict  outputValues;
};

#if USE_SIMD
kernel
//__attribute__((reqd_work_group_size(GROUP_SIZE, 1, 1)))
void ScatterKeysAndValues(// Number of bits to shift
						  constant ScatterKeysAndValuesPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
						  constant ScatterKeysAndValuesArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
                          uint globalId [[ thread_position_in_grid ]],
                          uint localId [[ thread_position_in_threadgroup ]],
                          uint groupSize [[ threads_per_threadgroup ]],
                          uint groupId [[ threadgroup_position_in_grid ]],
                          uint groupCount [[ threadgroups_per_grid ]],
                          uint simdLaneId [[ thread_index_in_simdgroup ]],
						  uint threadsPerSIMDGroup [[ threads_per_simdgroup ]],
						  uint simdGroupId [[ simdgroup_index_in_threadgroup ]]
                          )
{
   threadgroup uint scannedHistogram[16];
	
	// Copy scanned histogram for the group to local memory for fast indexing
	if (localId < 16) {
        scannedHistogram[localId] = arguments.inputHistograms[groupId + localId * groupCount];
    }
    
    // Make sure everything is up to date
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    threadgroup uint sortedKeys[GROUP_SIZE * 4];
    threadgroup uint sortedValues[GROUP_SIZE * 4];
    
    const int blockCountPerGroup = NUMBER_OF_BLOCKS_PER_GROUP;
    const int elementCountPerGroup = blockCountPerGroup * GROUP_SIZE;
    
    int totalBlockCount = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
    int maxBlocks = totalBlockCount - groupId * blockCountPerGroup;
    
    int loadIndex = groupId * elementCountPerGroup + localId;
    int blockCount = min(blockCountPerGroup, maxBlocks);
    
    for (int block = 0; block < blockCount; ++block, loadIndex += GROUP_SIZE) {
        
        // Radix sort to local memory, and then scatter to global memory.
        
        /// Load single uint4 value
        ushort validCount;
        uint4 keys = safe_load_uint4_intmax_validcount(arguments.inputKeys, loadIndex, arguments.elementCount, &validCount);
        uint4 values = safe_load_uint4_intmax_validcount(arguments.inputValues, loadIndex, arguments.elementCount, &validCount);
        
        // Do a 2-bit sort twice.
        for (uint bit = 0; bit <= 2; bit += 2) {
            uchar4 bin = uchar4((keys >> (rootConstants.bitShift + bit)) & 0b11);

            ushort4 threadCounts = ushort4(0);

            for (ushort i = 0; i < validCount; i += 1) {
                threadCounts[bin[i]] += 1;
            }

            const ushort simdGroupCount = groupSize / threadsPerSIMDGroup;

            ushort4 threadBucketOffsets; // The offset for the current thread _within_ each bucket
            threadgroup ushort4 threadgroupBucketCounts;

            // Compute a exclusive scan across each thread in the threadgroup.
            {
                threadBucketOffsets = simd_prefix_exclusive_sum(threadCounts);

                if (groupSize > threadsPerSIMDGroup) {
                    threadgroup ushort4 threadgroupTotals[4]; // groupSize is 64, so the count of this should be groupSize / simdSize; conservatively assume a minimum SIMD width of 16

                    if (simdLaneId == threadsPerSIMDGroup - 1) {
                        threadgroupTotals[simdGroupId] = threadBucketOffsets + threadCounts;
                    }

                    threadgroup_barrier(mem_flags::mem_threadgroup);

                    ushort4 simdGroupValue = 0;
                    if (localId < simdGroupCount) {
                        simdGroupValue = threadgroupTotals[localId];
                    }

                    simdGroupValue = simd_prefix_exclusive_sum(simdGroupValue); // There are at most four active lanes.
                    if (localId < simdGroupCount) {
                        threadgroupTotals[localId] = simdGroupValue;
                    }

                    threadgroup_barrier(mem_flags::mem_threadgroup);

                    threadBucketOffsets += threadgroupTotals[simdGroupId];
                }

                if (localId == groupSize - 1) {
                    threadgroupBucketCounts = threadBucketOffsets + threadCounts;
                }
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            // Convert the threadgroup per-bucket counts into per-bucket offsets.
            threadgroup ushort4 threadgroupBucketOffsets;
            if (localId == 0) {
                threadgroupBucketOffsets.x = 0;
                threadgroupBucketOffsets.y = threadgroupBucketCounts.x;
                threadgroupBucketOffsets.z = (threadgroupBucketOffsets.y + threadgroupBucketCounts.y);
                threadgroupBucketOffsets.w = (threadgroupBucketOffsets.z + threadgroupBucketCounts.z);
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            for (uchar i = 0; i < validCount; i += 1) {
                ushort offset = threadgroupBucketOffsets[bin[i]] + threadBucketOffsets[bin[i]];
                sortedKeys[offset] = keys[i];
                sortedValues[offset] = values[i];
                threadBucketOffsets[bin[i]] += 1;
            }

            threadgroup_barrier(mem_flags::mem_threadgroup);

            // Retrieve the new, sorted value.
            for (ushort threadgroupLoadIndex = 4 * localId, i = 0; i < validCount; i += 1, threadgroupLoadIndex += 1) {
                keys[i] = sortedKeys[threadgroupLoadIndex];
                values[i] = sortedValues[threadgroupLoadIndex];
            }
        }
        
        // At this point, sortedKeys contains all of the sorted keys from the current block. Now, we need to distribute them to device memory.
        // First, compute how many items in each threadgroup
        threadgroup ushort threadgroupBucketOffsets[16];
        threadgroup atomic_uint threadgroupBucketCountsAtomic[16];
        
        if (localId < 16) {
            atomic_store_explicit(&threadgroupBucketCountsAtomic[localId], 0, memory_order::memory_order_relaxed);
        }
        
        uchar previousBucket = 0;
        if (localId > 0) {
            previousBucket = (sortedKeys[4 * localId - 1] >> rootConstants.bitShift) & 0b1111;
        } else {
            threadgroupBucketOffsets[0] = 0;
        }
        
        uchar4 buckets = uchar4((keys >> rootConstants.bitShift) & 0b1111);
        for (ushort i = 0; i < validCount; i += 1) {
            uchar bucket = buckets[i];
            if (bucket != previousBucket) {
                threadgroupBucketOffsets[bucket] = 4 * localId + i;
                previousBucket = buckets[i];
            }
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        for (ushort threadgroupLoadIndex = 4 * localId, i = 0; i < validCount; i += 1, threadgroupLoadIndex += 1) {
            atomic_fetch_add_explicit(&threadgroupBucketCountsAtomic[buckets[i]], 1, memory_order::memory_order_relaxed);
            
            ushort bucketOffset = threadgroupBucketOffsets[buckets[i]];
            uint globalOffset = scannedHistogram[buckets[i]] + uint(threadgroupLoadIndex - bucketOffset);
            arguments.outputKeys[globalOffset] = keys[i];
            arguments.outputValues[globalOffset] = values[i];
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        // Update the scanned histogram with the new global offsets.
        
        if (localId < 16) {
            scannedHistogram[localId] += atomic_load_explicit(&threadgroupBucketCountsAtomic[localId], memory_order::memory_order_relaxed);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}
#else
kernel
//__attribute__((reqd_work_group_size(GROUP_SIZE, 1, 1)))
void ScatterKeysAndValues(// Number of bits to shift
						  constant ScatterKeysAndValuesPushConstants& rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
						  constant ScatterKeysAndValuesArguments& arguments [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
                          uint globalId [[ thread_position_in_grid ]],
                          uint localId [[ thread_position_in_threadgroup ]],
                          uint groupSize [[ threads_per_threadgroup ]],
                          uint groupId [[ threadgroup_position_in_grid ]],
                          uint groupCount [[ threadgroups_per_grid ]],
                          uint simdLaneId [[ thread_index_in_simdgroup ]],
						  uint threadsPerSIMDGroup [[ threads_per_simdgroup ]],
						  uint simdGroupId [[ simdgroup_index_in_threadgroup ]]
                          )
{
   // Local memory for offsets counting
   threadgroup uint  keys[GROUP_SIZE * 4];
   threadgroup uint  scanned_histogram[NUM_BINS];
   
   threadgroup uint* histogram = (threadgroup uint*)keys;
   
   int numblocks_per_group = NUMBER_OF_BLOCKS_PER_GROUP;
   int num_elements_per_group = numblocks_per_group * GROUP_SIZE;
   int numblocks_total = (arguments.elementCount + GROUP_SIZE * 4 - 1) / (GROUP_SIZE * 4);
   int maxblocks = numblocks_total - groupId * numblocks_per_group;
   
   // Copy scanned histogram for the group to local memory for fast indexing
   if (localId < NUM_BINS)
   {
	   scanned_histogram[localId] = arguments.inputHistograms[groupId + localId * groupCount];
   }
   
   // Make sure everything is up to date
   threadgroup_barrier(mem_flags::mem_threadgroup);
   
   int loadidx = groupId * num_elements_per_group + localId;
   for (int block = 0; block < min(numblocks_per_group, maxblocks); ++block, loadidx += GROUP_SIZE)
   {
	   // Load single uint4 value
	   uint4 localkeys = safe_load_uint4_intmax(arguments.inputKeys, loadidx, arguments.elementCount);
	   uint4 localvals = safe_load_uint4_intmax(arguments.inputValues, loadidx, arguments.elementCount);
	   
	   // Clear the histogram
	   histogram[localId] = 0;
	   
	   // Make sure everything is up to date
	   threadgroup_barrier(mem_flags::mem_threadgroup);
	   
	   // Do 2 bits per pass
	   for (int bit = 0; bit <= 2; bit += 2)
	   {
		   // Count histogram
		   uint4 b = ((localkeys >> rootConstants.bitShift) >> bit) & 0x3;
		   
		   uint4 p;
		   p.x = 1 << (8 * b.x);
		   p.y = 1 << (8 * b.y);
		   p.z = 1 << (8 * b.z);
		   p.w = 1 << (8 * b.w);
		   
		   // Pack the histogram
		   uint packed_key = (uint)(p.x + p.y + p.z + p.w);
		   
		   // Put into LDS
		   histogram[localId] = packed_key;
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
		   
		   // Scan the histogram in LDS with 4-way plus scan
		   uint total = 0;
		   group_scan_exclusive_sum(localId, GROUP_SIZE, histogram, &total);
		   
		   // Load value back
		   packed_key = histogram[localId];
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
		   
		   // Scan total histogram (4 chars)
		   total = (total << 8) + (total << 16) + (total << 24);
		   uint offset = total + packed_key;
		   
		   uint4 newoffset;
		   
		   int t = p.y + p.x;
		   p.w = p.z + t;
		   p.z = t;
		   p.y = p.x;
		   p.x = 0;
		   
		   p += (int)offset;
		   newoffset = (p >> (b * 8)) & 0xFF;
		   
		   keys[newoffset.x] = localkeys.x;
		   keys[newoffset.y] = localkeys.y;
		   keys[newoffset.z] = localkeys.z;
		   keys[newoffset.w] = localkeys.w;
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
		   
		   // Reload values back to registers for the second bit pass
		   localkeys.x = keys[localId << 2];
		   localkeys.y = keys[(localId << 2) + 1];
		   localkeys.z = keys[(localId << 2) + 2];
		   localkeys.w = keys[(localId << 2) + 3];
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
		   
		   keys[newoffset.x] = localvals.x;
		   keys[newoffset.y] = localvals.y;
		   keys[newoffset.z] = localvals.z;
		   keys[newoffset.w] = localvals.w;
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
		   
		   // Reload values back to registers for the second bit pass
		   localvals.x = keys[localId << 2];
		   localvals.y = keys[(localId << 2) + 1];
		   localvals.z = keys[(localId << 2) + 2];
		   localvals.w = keys[(localId << 2) + 3];
		   
		   // Make sure everything is up to date
		   threadgroup_barrier(mem_flags::mem_threadgroup);
	   }
	   
	   // Clear LDS
	   histogram[localId] = 0;
	   
	   // Make sure everything is up to date
	   threadgroup_barrier(mem_flags::mem_threadgroup);
	   
	   threadgroup atomic_uint* histogramAtomic = (threadgroup atomic_uint*)histogram;
	   
	   // Reconstruct 16 bins histogram
	   uint4 bin = (localkeys >> rootConstants.bitShift) & 0xF;
	   atom_inc(&histogramAtomic[bin.x]);
	   atom_inc(&histogramAtomic[bin.y]);
	   atom_inc(&histogramAtomic[bin.z]);
	   atom_inc(&histogramAtomic[bin.w]);
	   
	   threadgroup_barrier(mem_flags::mem_threadgroup);
	   
	   int sum = 0;
	   if (localId < NUM_BINS)
	   {
		   sum = histogram[localId];
	   }
	   
	   // Make sure everything is up to date
	   threadgroup_barrier(mem_flags::mem_threadgroup);
	   
	   // Scan reconstructed histogram
	   group_scan_exclusive(localId, 16, histogram);
	   
	   // Put data back to global memory
	   uint offset = scanned_histogram[bin.x] + (localId << 2) - histogram[bin.x];
	   if (offset < arguments.elementCount)
	   {
		   arguments.outputKeys[offset] = localkeys.x;
		   arguments.outputValues[offset] = localvals.x;
	   }
	   
	   offset = scanned_histogram[bin.y] + (localId << 2) + 1 - histogram[bin.y];
	   if (offset < arguments.elementCount)
	   {
		   arguments.outputKeys[offset] = localkeys.y;
		   arguments.outputValues[offset] = localvals.y;
	   }
	   
	   offset = scanned_histogram[bin.z] + (localId << 2) + 2 - histogram[bin.z];
	   if (offset < arguments.elementCount)
	   {
		   arguments.outputKeys[offset] = localkeys.z;
		   arguments.outputValues[offset] = localvals.z;
	   }
	   
	   offset = scanned_histogram[bin.w] + (localId << 2) + 3 - histogram[bin.w];
	   if (offset < arguments.elementCount)
	   {
		   arguments.outputKeys[offset] = localkeys.w;
		   arguments.outputValues[offset] = localvals.w;
	   }
	   
	   threadgroup_barrier(mem_flags::mem_threadgroup);
	   
	   scanned_histogram[localId] += sum;
   }
}
#endif // USE_SIMD

kernel void compact_int(device int* in_predicate, device int* in_address,
                        constant uint& in_size,
                        device int* in_input,
                        device int* out_output,
                        uint global_id [[ thread_position_in_grid ]],
                        uint group_id [[ threadgroup_position_in_grid ]])
{
    
    if (global_id < in_size)
    {
        if (in_predicate[global_id])
        {
            out_output[in_address[global_id]] = in_input[global_id];
        }
    }
}

kernel void compact_int_1(device int* in_predicate, device int* in_address,
                          constant uint& in_size,
                          device int* in_input,
                          device int* out_output,
                          device int* out_size,
                          uint global_id [[ thread_position_in_grid ]],
                          uint group_id [[ threadgroup_position_in_grid ]])
{
    
    if (global_id < in_size)
    {
        if (in_predicate[global_id])
        {
            out_output[in_address[global_id]] = in_input[global_id];
        }
    }
    
    if (global_id == 0)
    {
        *out_size = in_address[in_size - 1] + in_predicate[in_size - 1];
    }
}

kernel void copy(device uint4* in_input,
                 constant uint&  in_size,
                 device uint4* out_output,
                 uint global_id [[ thread_position_in_grid ]])
{
    uint4 value = safe_load_4(in_input, global_id, in_size);
    safe_store_4(value, out_output, global_id, in_size);
}


#define FLAG(x) (flags[(x)] & 0x1)
#define FLAG_COMBINED(x) (flags[(x)])
#define FLAG_ORIG(x) ((flags[(x)] >> 1) & 0x1)

void group_segmented_scan_exclusive_int(
                                        int localId,
                                        int groupSize,
                                        threadgroup int* shmem,
                                        threadgroup char* flags
                                        )
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            if (FLAG(2 * (localId + 1)*stride - 1) == 0)
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + shmem[(2 * localId + 1)*stride - 1];
            }
            
            FLAG_COMBINED(2 * (localId + 1)*stride - 1) = FLAG_COMBINED(2 * (localId + 1)*stride - 1) | FLAG((2 * localId + 1)*stride - 1);
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (localId == 0)
        shmem[groupSize - 1] = 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            int temp = shmem[(2 * localId + 1)*stride - 1];
            shmem[(2 * localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1];
            
            // optimize with a conditional = operator
            if (FLAG_ORIG((2 * localId + 1)*stride) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = 0;
            }
            else if (FLAG((2 * localId + 1)*stride - 1) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = temp;
            }
            else
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + temp;
            }
            
            FLAG_COMBINED((2 * localId + 1)*stride - 1) = FLAG_COMBINED((2 * localId + 1)*stride - 1) & 2;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

void group_segmented_scan_exclusive_int_nocut(
                                              int localId,
                                              int groupSize,
                                              threadgroup int* shmem,
                                              threadgroup char* flags
                                              )
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            if (FLAG(2 * (localId + 1)*stride - 1) == 0)
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + shmem[(2 * localId + 1)*stride - 1];
            }
            
            FLAG_COMBINED(2 * (localId + 1)*stride - 1) = FLAG_COMBINED(2 * (localId + 1)*stride - 1) | FLAG((2 * localId + 1)*stride - 1);
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (localId == 0)
        shmem[groupSize - 1] = 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            int temp = shmem[(2 * localId + 1)*stride - 1];
            shmem[(2 * localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1];
            
            if (FLAG((2 * localId + 1)*stride - 1) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = temp;
            }
            else
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + temp;
            }
            
            FLAG_COMBINED((2 * localId + 1)*stride - 1) = FLAG_COMBINED((2 * localId + 1)*stride - 1) & 2;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}


void group_segmented_scan_exclusive_int_part(
                                             int localId,
                                             int groupId,
                                             int groupSize,
                                             threadgroup int* shmem,
                                             threadgroup char* flags,
                                             device int* part_sums,
                                             device int* part_flags
                                             )
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            if (FLAG(2 * (localId + 1)*stride - 1) == 0)
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + shmem[(2 * localId + 1)*stride - 1];
            }
            
            FLAG_COMBINED(2 * (localId + 1)*stride - 1) = FLAG_COMBINED(2 * (localId + 1)*stride - 1) | FLAG((2 * localId + 1)*stride - 1);
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (localId == 0)
    {
        part_sums[groupId] = shmem[groupSize - 1];
        part_flags[groupId] = FLAG(groupSize - 1);
        shmem[groupSize - 1] = 0;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            int temp = shmem[(2 * localId + 1)*stride - 1];
            shmem[(2 * localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1];
            
            // optimize with a conditional = operator
            if (FLAG_ORIG((2 * localId + 1)*stride) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = 0;
            }
            else if (FLAG((2 * localId + 1)*stride - 1) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = temp;
            }
            else
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + temp;
            }
            
            FLAG_COMBINED((2 * localId + 1)*stride - 1) = FLAG_COMBINED((2 * localId + 1)*stride - 1) & 2;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

void group_segmented_scan_exclusive_int_nocut_part(
                                                   int localId,
                                                   int groupId,
                                                   int groupSize,
                                                   threadgroup int* shmem,
                                                   threadgroup char* flags,
                                                   device int* part_sums,
                                                   device int* part_flags
                                                   )
{
    for (int stride = 1; stride <= (groupSize >> 1); stride <<= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            if (FLAG(2 * (localId + 1)*stride - 1) == 0)
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + shmem[(2 * localId + 1)*stride - 1];
            }
            
            FLAG_COMBINED(2 * (localId + 1)*stride - 1) = FLAG_COMBINED(2 * (localId + 1)*stride - 1) | FLAG((2 * localId + 1)*stride - 1);
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (localId == 0)
    {
        part_sums[groupId] = shmem[groupSize - 1];
        part_flags[groupId] = FLAG(groupSize - 1);
        shmem[groupSize - 1] = 0;
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (int stride = (groupSize >> 1); stride > 0; stride >>= 1)
    {
        if (localId < groupSize / (2 * stride))
        {
            int temp = shmem[(2 * localId + 1)*stride - 1];
            shmem[(2 * localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1];
            
            if (FLAG((2 * localId + 1)*stride - 1) == 1)
            {
                shmem[2 * (localId + 1)*stride - 1] = temp;
            }
            else
            {
                shmem[2 * (localId + 1)*stride - 1] = shmem[2 * (localId + 1)*stride - 1] + temp;
            }
            
            FLAG_COMBINED((2 * localId + 1)*stride - 1) = FLAG_COMBINED((2 * localId + 1)*stride - 1) & 2;
        }
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}


kernel void segmented_scan_exclusive_int_nocut(device int const* inputArray,
                                               device int const* in_segment_heads_array,
                                               constant uint& elementCountRootConstant,
                                               device int* outputArray,
                                               threadgroup int* shmem,
                                               uint globalId [[ thread_position_in_grid ]],
                                               uint localId [[ thread_position_in_threadgroup ]],
                                               uint groupSize [[ threads_per_threadgroup ]],
                                               uint groupId [[ threadgroup_position_in_grid ]])
{
    
    threadgroup int* keys = shmem;
    threadgroup char* flags = (threadgroup char*)(keys + groupSize);
    
    keys[localId] = globalId < elementCountRootConstant ? inputArray[globalId] : 0;
    flags[localId] = globalId < elementCountRootConstant ? (in_segment_heads_array[globalId] ? 3 : 0) : 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    group_segmented_scan_exclusive_int_nocut(localId, groupSize, keys, flags);
    
    outputArray[globalId] = keys[localId];
}

kernel void segmented_scan_exclusive_int(device int const* inputArray,
                                         device int const* in_segment_heads_array,
                                         constant uint& elementCountRootConstant,
                                         device int* outputArray,
                                         threadgroup int* shmem,
                                         uint globalId [[ thread_position_in_grid ]],
                                         uint localId [[ thread_position_in_threadgroup ]],
                                         uint groupSize [[ threads_per_threadgroup ]],
                                         uint groupId [[ threadgroup_position_in_grid ]])
{
    
    threadgroup int* keys = shmem;
    threadgroup char* flags = (threadgroup char*)(keys + groupSize);
    
    keys[localId] = globalId < elementCountRootConstant ? inputArray[globalId] : 0;
    flags[localId] = globalId < elementCountRootConstant ? (in_segment_heads_array[globalId] ? 3 : 0) : 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    group_segmented_scan_exclusive_int(localId, groupSize, keys, flags);
    
    outputArray[globalId] = keys[localId];
}

kernel void segmented_scan_exclusive_int_part(device int const* inputArray,
                                              device int const* in_segment_heads_array,
                                              constant uint& elementCountRootConstant,
                                              device int* outputArray,
                                              device int* out_part_sums,
                                              device int* out_part_flags,
                                              threadgroup int* shmem,
                                              uint globalId [[ thread_position_in_grid ]],
                                              uint localId [[ thread_position_in_threadgroup ]],
                                              uint groupSize [[ threads_per_threadgroup ]],
                                              uint groupId [[ threadgroup_position_in_grid ]])
{
    
    threadgroup int* keys = shmem;
    threadgroup char* flags = (threadgroup char*)(keys + groupSize);
    
    keys[localId] = globalId < elementCountRootConstant ? inputArray[globalId] : 0;
    flags[localId] = globalId < elementCountRootConstant ? (in_segment_heads_array[globalId] ? 3 : 0) : 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    group_segmented_scan_exclusive_int_part(localId, groupId, groupSize, keys, flags, out_part_sums, out_part_flags);
    
    outputArray[globalId] = keys[localId];
}

kernel void segmented_scan_exclusive_int_nocut_part(device int const* inputArray,
                                                    device int const* in_segment_heads_array,
                                                    constant uint& elementCountRootConstant,
                                                    device int* outputArray,
                                                    device int* out_part_sums,
                                                    device int* out_part_flags,
                                                    threadgroup int* shmem,
                                                    uint globalId [[ thread_position_in_grid ]],
                                                    uint localId [[ thread_position_in_threadgroup ]],
                                                    uint groupSize [[ threads_per_threadgroup ]],
                                                    uint groupId [[ threadgroup_position_in_grid ]],
                                                    uint numGroups [[ threadgroups_per_grid ]])
{
    
    threadgroup int* keys = shmem;
    threadgroup char* flags = (threadgroup char*)(keys + groupSize);
    
    keys[localId] = globalId < elementCountRootConstant ? inputArray[globalId] : 0;
    flags[localId] = globalId < elementCountRootConstant ? (in_segment_heads_array[globalId] ? 3 : 0) : 0;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    group_segmented_scan_exclusive_int_nocut_part(localId, groupId, groupSize, keys, flags, out_part_sums, out_part_flags);
    
    outputArray[globalId] = keys[localId];
}


kernel void segmented_distribute_part_sum_int(
                                              device int* inoutArray,
                                              device int* in_flags,
                                              constant uint& elementCountRootConstant,
                                              device int* inputSums,
                                              uint globalId [[ thread_position_in_grid ]],
                                              uint localId [[ thread_position_in_threadgroup ]],
                                              uint groupSize [[ threads_per_threadgroup ]],
                                              uint groupId [[ threadgroup_position_in_grid ]]
                                              )
{
    
    int sum = inputSums[groupId];
    //inoutArray[globalId] += sum;
    
    if (localId == 0)
    {
        for (uint i = 0; in_flags[globalId + i] == 0 && i < groupSize; ++i)
        {
            if (globalId + i < elementCountRootConstant)
            {
                inoutArray[globalId + i] += sum;
            }
        }
    }
}

kernel void segmented_distribute_part_sum_int_nocut(
                                                    device int* inoutArray,
                                                    device int* in_flags,
                                                    constant uint& elementCountRootConstant,
                                                    device int* inputSums,
                                                    uint globalId [[ thread_position_in_grid ]],
                                                    uint localId [[ thread_position_in_threadgroup ]],
                                                    uint groupSize [[ threads_per_threadgroup ]],
                                                    uint groupId [[ threadgroup_position_in_grid ]]
                                                    )
{
    
    int sum = inputSums[groupId];
    bool stop = false;
    //inoutArray[globalId] += sum;
    
    if (localId == 0)
    {
        for (uint i = 0; i < groupSize; ++i)
        {
            if (globalId + i < elementCountRootConstant)
            {
                if (in_flags[globalId + i] == 0)
                {
                    inoutArray[globalId + i] += sum;
                }
                else
                {
                    if (stop)
                    {
                        break;
                    }
                    else
                    {
                        inoutArray[globalId + i] += sum;
                        stop = true;
                    }
                }
            }
        }
    }
}

struct OffsetBufferGenerationPushConstants {
	uint categoryCount;
	uint indirectThreadsPerThreadgroup;
};

struct OffsetBufferGenerationArguments {
	const device uint *sortedIndices;
	const device uint& sortedIndicesCount;
	device uint* offsetBuffer;  // must be initialised to some flag value (e.g. 0xFFFFFFFF) before this.
	device uint* totalCountAndIndirectArgs;
};

// Input is a sorted buffer of category indices, and output is where in that buffer each category starts.
kernel void ClearOffsetBuffer(constant OffsetBufferGenerationPushConstants &rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
							  constant OffsetBufferGenerationArguments& args [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
                                uint globalId [[ thread_position_in_grid ]]) {
	uint threadsNeeded = (rootConstants.categoryCount + 3) / 4;
	if (globalId >= threadsNeeded) {
		return;
	}
	
	uint lowerBound = globalId * 4;
	uint upperBound = min(rootConstants.categoryCount, lowerBound + 4);
	for (uint i = lowerBound; i < upperBound; i += 1) {
		args.offsetBuffer[i] = ~0;
	}
}

// Input is a sorted buffer of category indices, and output is where in that buffer each category starts.
kernel void GenerateOffsetBuffer(constant OffsetBufferGenerationPushConstants &rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
                                constant OffsetBufferGenerationArguments& args [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
                                uint globalId [[ thread_position_in_grid ]]) {
	// If no threads will get to run, make sure we clear the indirect count buffer.
	if (args.sortedIndicesCount == 0 && globalId == 0) {
		args.totalCountAndIndirectArgs[0] = 0;
		args.totalCountAndIndirectArgs[1] = 0;
		args.totalCountAndIndirectArgs[2] = 0;
		args.totalCountAndIndirectArgs[3] = 0;
	}
	
    if (globalId >= args.sortedIndicesCount) {
        return;
    }
    
    uint indexInIndices = globalId + 1;
    
    uint previousIndex = args.sortedIndices[indexInIndices - 1];
    uint currentIndex = indexInIndices == args.sortedIndicesCount ? rootConstants.categoryCount : args.sortedIndices[indexInIndices];
    
    if (previousIndex < currentIndex && previousIndex + 1 < rootConstants.categoryCount) {
        args.offsetBuffer[previousIndex + 1] = indexInIndices;
        
        if (currentIndex < rootConstants.categoryCount) {
            args.offsetBuffer[currentIndex] = indexInIndices;
        }
    }
    
    if (globalId == 0) {
        uint currentIndex = args.sortedIndices[0];
		args.offsetBuffer[0] = 0;
            
        if (currentIndex < rootConstants.categoryCount) {
            args.offsetBuffer[currentIndex] = 0;
        }
    }
    
    if (currentIndex >= rootConstants.categoryCount &&
        (previousIndex < rootConstants.categoryCount || (globalId == 0 && previousIndex >= rootConstants.categoryCount))) { // Will only be true for one element.
        // Put into the rayCount buffer: [count], [threadgroupsX, 1, 1], [max(threadgroupsX, 1), 1, 1]
        uint activeCount = indexInIndices;
        if (globalId == 0 && previousIndex >= rootConstants.categoryCount) {
            activeCount = 0;
        }
        
        args.totalCountAndIndirectArgs[0] = activeCount;
        
        uint threadgroupsX = (activeCount + rootConstants.indirectThreadsPerThreadgroup - 1) / rootConstants.indirectThreadsPerThreadgroup;
        args.totalCountAndIndirectArgs[1] = max(1u, threadgroupsX);
        args.totalCountAndIndirectArgs[2] = 1;
        args.totalCountAndIndirectArgs[3] = 1;
        
//        args.totalCountAndIndirectArgs[4] = max(threadgroupsX, 1u);
//        args.totalCountAndIndirectArgs[5] = 1;
//        args.totalCountAndIndirectArgs[6] = 1;
    }
}

struct IndirectArgsFromOffsetBufferPushConstants {
	uint categoryCount;
	uint indirectThreadsPerThreadgroup;
};

struct IndirectArgsFromOffsetBufferArguments {
	const device uint* offsetBuffer;  // must be initialised to some flag value (e.g. 0xFFFFFFFF) before this.
	constant uint& totalIndexCount;
	device uint* indirectArgumentsBuffer; // Offset, total count, then threadgroups X, Y, Z (stride of 5)
};

kernel void GenerateIndirectArgumentsFromOffsetBuffer(constant IndirectArgsFromOffsetBufferPushConstants &rootConstants [[ buffer(UPDATE_FREQ_USER) ]],
                                constant IndirectArgsFromOffsetBufferArguments& args [[ buffer(UPDATE_FREQ_PER_DRAW) ]],
                                uint globalId [[ thread_position_in_grid ]]) {

	if (globalId >= rootConstants.categoryCount) {
		return;
	}
	
	uint categoryIndex = globalId;
	
	uint threadIndexLowerBound = args.offsetBuffer[categoryIndex];
	uint threadIndexUpperBound;
	if (threadIndexLowerBound == UINT_MAX) {
		threadIndexUpperBound = threadIndexLowerBound;
	} else {
		threadIndexUpperBound = (categoryIndex + 1 >= rootConstants.categoryCount) ? args.totalIndexCount : args.offsetBuffer[categoryIndex + 1];
		if (threadIndexUpperBound == UINT_MAX) {
			threadIndexUpperBound = threadIndexLowerBound;
		}
	}
	
	uint count = threadIndexUpperBound - threadIndexLowerBound;
	
	device uint* output = &args.indirectArgumentsBuffer[8 * categoryIndex];
	output[0] = threadIndexLowerBound;
	output[1] = count;
	output[2] = (count + rootConstants.indirectThreadsPerThreadgroup - 1) / rootConstants.indirectThreadsPerThreadgroup;
	output[3] = 1;
	output[4] = 1;
}
