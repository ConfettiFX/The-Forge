/*
 * Copyright (c) 2018-2022 The Forge Interactive Inc.
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

#define MAX_TEXTURE_UNITS 256U

// The following value defines the maximum amount of indirect draw calls that will be
// drawn at once. This value depends on the number of submeshes or individual objects
// in the scene. Changing a scene will require to change this value accordingly.
#define MAX_DRAWS_INDIRECT 256

// This defines the amount of viewports that are going to be culled in parallel.
#define NUM_CULLING_VIEWPORTS 1

#if defined(DIRECT3D12)
#define INDIRECT_ROOT_CONSTANT 1
#define SET_DRAW_ID(o, drawId, instanceId)
#else
#define SET_DRAW_ID(o, drawId, instanceId) o.drawId = instanceId
#endif

#if defined(ORBIS)
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS 5
#else
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS 8
#endif
#if defined(INDIRECT_ROOT_CONSTANT)
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_OFFSET   1
#else
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_OFFSET   0
#endif

// The following values point to the position in the indirect draw buffer that holds the
// number of draw calls to draw after triangle filtering and batch compaction.
// This value number is stored in the last position of the indirect draw buffer.
// So it depends on MAX_DRAWS_INDIRECT
#define DRAW_COUNTER_SLOT_POS ((MAX_DRAWS_INDIRECT - 1) * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS)
#define DRAW_COUNTER_SLOT_OFFSET_IN_BYTES (DRAW_COUNTER_SLOT_POS * sizeof(uint))

// Size for the material buffer assuming each draw call uses one material index.
// The 4 values here stands for the 4 types of rendering passes used in the demo:
// alpha_tested_view0, opaque_view0, alpha_tested_view1, opaque_view1
#define MATERIAL_BUFFER_SIZE (MAX_DRAWS_INDIRECT * 2 * NUM_CULLING_VIEWPORTS)

// This function is used to get the offset of the current material base index depending
// on the type of geometry and on the culling view.
#define BaseMaterialBuffer(alpha, viewID) (((viewID) * 2 + ((alpha) ? 0 : 1)) * MAX_DRAWS_INDIRECT)
