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

#ifndef Geometry_h
#define Geometry_h

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/shader_defs.h"

// Type definitions

typedef struct SceneVertexPos
{
	float x, y, z;
} SceneVertexPos;

typedef struct Material
{
	bool twoSided;
	bool alphaTested;
} Material;

typedef struct Scene
{
	Geometry*                          geom;
	Material*                          materials;
	char**                             textures;
	char**                             normalMaps;
	char**                             specularMaps;
} Scene;

// Exposed functions

Scene* addScene(const char* pFileName, SyncToken& token);
void   removeScene(Scene* scene);

#endif
