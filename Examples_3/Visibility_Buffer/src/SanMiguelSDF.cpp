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
#include "SanMiguelSDF.h"

#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

void loadBakedSDFData(SDFMesh* outMesh, uint32_t startIdx, bool generateSDFVolumeData, SDFVolumeData** sdfVolumeInstances,
                      GenerateVolumeDataFromFileFunc generateVolumeDataFromFileFunc)
{
    UNREF_PARAM(generateSDFVolumeData);
    uint32_t idxFirstMeshInGroup = 0;

    // for each submesh group.
    // in case there is no submesh groups, numSubMeshesGroups = numSubMeshes in the geometry
    for (uint32_t groupNum = 0; groupNum < outMesh->numSubMeshesGroups; ++groupNum)
    {
        MeshInfo&      meshInfo = outMesh->pSubMeshesInfo[idxFirstMeshInGroup];
        uint32_t       meshGroupSize = outMesh->pSubMeshesGroupsSizes ? outMesh->pSubMeshesGroupsSizes[groupNum] : 1;
        SDFVolumeData* volumeData = NULL;

        (*generateVolumeDataFromFileFunc)(&volumeData, &meshInfo);

        sdfVolumeInstances[startIdx++] = volumeData;

        if (volumeData)
        {
            meshInfo.sdfGenerated = true;
            ++outMesh->numGeneratedSDFMeshes;
        }

        idxFirstMeshInGroup += meshGroupSize;
    }
}

void adjustAABB(AABB* ownerAABB, const vec3& point)
{
    ownerAABB->minBounds.setX(fmin(point.getX(), ownerAABB->minBounds.getX()));
    ownerAABB->minBounds.setY(fmin(point.getY(), ownerAABB->minBounds.getY()));
    ownerAABB->minBounds.setZ(fmin(point.getZ(), ownerAABB->minBounds.getZ()));

    ownerAABB->maxBounds.setX(fmax(point.getX(), ownerAABB->maxBounds.getX()));
    ownerAABB->maxBounds.setY(fmax(point.getY(), ownerAABB->maxBounds.getY()));
    ownerAABB->maxBounds.setZ(fmax(point.getZ(), ownerAABB->maxBounds.getZ()));
}
void adjustAABB(AABB* ownerAABB, const AABB& otherAABB)
{
    ownerAABB->minBounds.setX(fmin(otherAABB.minBounds.getX(), ownerAABB->minBounds.getX()));
    ownerAABB->minBounds.setY(fmin(otherAABB.minBounds.getY(), ownerAABB->minBounds.getY()));
    ownerAABB->minBounds.setZ(fmin(otherAABB.minBounds.getZ(), ownerAABB->minBounds.getZ()));

    ownerAABB->maxBounds.setX(fmax(otherAABB.maxBounds.getX(), ownerAABB->maxBounds.getX()));
    ownerAABB->maxBounds.setY(fmax(otherAABB.maxBounds.getY(), ownerAABB->maxBounds.getY()));
    ownerAABB->maxBounds.setZ(fmax(otherAABB.maxBounds.getZ(), ownerAABB->maxBounds.getZ()));
}

void alignAABB(AABB* ownerAABB, float alignment)
{
    vec3 boxMin = ownerAABB->minBounds / alignment;
    boxMin = vec3(floorf(boxMin.getX()) * alignment, floorf(boxMin.getY()) * alignment, 0.0f);
    vec3 boxMax = ownerAABB->maxBounds / alignment;
    boxMax = vec3(ceilf(boxMax.getX()) * alignment, ceilf(boxMax.getY()) * alignment, 0.0f);
    *ownerAABB = AABB(boxMin, boxMax);
}

vec3 calculateAABBSize(const AABB* ownerAABB) { return ownerAABB->maxBounds - ownerAABB->minBounds; }

vec3 calculateAABBExtent(const AABB* ownerAABB) { return 0.5f * (ownerAABB->maxBounds - ownerAABB->minBounds); }

vec3 calculateAABBCenter(const AABB* ownerAABB) { return (ownerAABB->maxBounds + ownerAABB->minBounds) * 0.5f; }
