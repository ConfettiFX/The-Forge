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

#ifndef SanMiguel_h
#define SanMiguel_h

#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Type definitions

typedef uint32_t MaterialFlags;

enum MaterialFlagBits
{
    MATERIAL_FLAG_NONE = 0,
    MATERIAL_FLAG_TWO_SIDED = (1 << 0),
    MATERIAL_FLAG_ALPHA_TESTED = (1 << 1),
    MATERIAL_FLAG_TRANSPARENT = (1 << 2),
    MATERIAL_FLAG_DOUBLE_VOXEL_SIZE = (1 << 3),
    MATERIAL_FLAG_ALL = MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED | MATERIAL_FLAG_DOUBLE_VOXEL_SIZE
};

typedef struct Scene
{
    Geometry*      geom;
    GeometryData*  geomData;
    MaterialFlags* materialFlags;
    char**         textures;
    char**         normalMaps;
    char**         specularMaps;
    uint32_t       materialCount;
} Scene;

Scene* initSanMiguel(const GeometryLoadDesc* pTemplate, SyncToken& token, bool transparentFlags);
void   exitSanMiguel(Scene* scene);

void createCubeBuffers(Renderer* pRenderer, Buffer** outVertexBuffer, Buffer** outIndexBuffer);

#endif
