/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#ifndef SDF_CONSTANTS
#define SDF_CONSTANTS


#define ENABLE_SDF_MESH_GENERATION 1

#define SDF_VOLUME_TEXTURE_ATLAS_WIDTH 512
#define SDF_VOLUME_TEXTURE_ATLAS_HEIGHT 512
#define SDF_VOLUME_TEXTURE_ATLAS_DEPTH 1024


#define SDF_MAX_OBJECT_COUNT 256
#define SDF_OBJECT_COUNT 163 // mNumObjects from uniform buffer always returns 0 on Quest. Unsure why. In RenderDoc the value is correct.

#define SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X 128
#define SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y 128
#define SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z 128


#define SDF_MAX_VOXEL_ONE_DIMENSION_X 96
#define SDF_MAX_VOXEL_ONE_DIMENSION_Y 96
#define SDF_MAX_VOXEL_ONE_DIMENSION_Z 96

#define SDF_MIN_VOXEL_ONE_DIMENSION_X 96
#define SDF_MIN_VOXEL_ONE_DIMENSION_Y 96
#define SDF_MIN_VOXEL_ONE_DIMENSION_Z 96

#define SDF_STRATIFIED_DIRECTIONS_NUM 600


//Approximation margin added to SDF bounding box
#define SDF_APPROX_MARGIN 0.9

#define SDF_MESH_VISUALIZATION_THREAD_X 8
#define SDF_MESH_VISUALIZATION_THREAD_Y 8

#define SDF_MESH_SHADOW_THREAD_X 8
#define SDF_MESH_SHADOW_THREAD_Y 8


#define ENABLE_SDF_SHADOW_DOWNSAMPLE 1

#if ENABLE_SDF_SHADOW_DOWNSAMPLE
#define SDF_SHADOW_DOWNSAMPLE_VALUE 2
#else
#define SDF_SHADOW_DOWNSAMPLE_VALUE 1
#endif


#define SDF_LIGHT_THERESHOLD_VAL 10000

#endif // SDF_CONSTANTS

