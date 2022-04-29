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

#include <metal_stdlib>
using namespace metal;

#include "ShaderTypes.h"

struct ClassifyHitGroupsArguments {
    const device uint& activePathCount;
    const device Intersection* intersections;
    const device uint* instanceHitGroups;
    const device uchar* pathMissShaderIndices;
    device uint* pathHitGroups;
    uint hitGroupCount;
}

// [numthreads(64, 1, 1)]
kernel void ClassifyHitGroups(const ClassifyHitGroupsArguments& arguments [[ buffer(0) ]],
                              uint pathIndex [[ thread_position_in_grid ]]) {
    if (pathIndex >= arguments.activePathCount) {
        return;
    }
    
    const device Intersection& intersection = arguments.intersections[pathIndex];
    
    if (intersection.distance < 0) {
        pathHitGroups[pathIndex] = arguments.hitGroupCount + (uint)arguments.pathMissShaderIndices[pathIndex];
    } else {
        pathHitGroups[pathIndex] = arguments.instanceHitGroups[intersection.instanceIndex];
    }
}
