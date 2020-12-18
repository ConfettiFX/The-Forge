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

#ifndef _ASMSHADER_DEFS_H
#define _ASMSHADER_DEFS_H

#define ASM_WORK_BUFFER_COLOR_PASS_WIDTH 512
#define ASM_WORK_BUFFER_COLOR_PASS_HEIGHT 256
#define ASM_WORK_BUFFER_DEPTH_PASS_WIDTH 512
#define ASM_WORK_BUFFER_DEPTH_PASS_HEIGHT 512

#define gsASMIndexSize 8

#define gs_ASMMaxRefinement 4
#define gs_ASMTileBorderTexels 8
#define gs_ASMTileSize 256
#define gs_ASMDEMDownsampleLevel 1
#define gs_ASMDEMTileSize ( gs_ASMTileSize >> gs_ASMDEMDownsampleLevel )
#define gs_ASMBorderlessTileSize ( gs_ASMTileSize - 2 * gs_ASMTileBorderTexels )

#define gs_ASMDepthAtlasTextureWidth 4096
#define gs_ASMDepthAtlasTextureHeight 4096
#define gs_ASMDEMAtlasTextureWidth ( gs_ASMDepthAtlasTextureWidth >> gs_ASMDEMDownsampleLevel )
#define gs_ASMDEMAtlasTextureHeight ( gs_ASMDepthAtlasTextureHeight >> gs_ASMDEMDownsampleLevel )

#define gs_ASMTileFarPlane 2000
#define gs_ASMDistanceMax 1000
#define gs_ASMLargestTileWorldSize 120


#define PACKED_QUADS_ARRAY_REGS 192

#ifndef NO_GLSL_DEFINITIONS
const vec2 ASMDepthAtlasSizeOverDepthTileSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight) / gs_ASMTileSize;
const vec2 ASMDEMTileSizeOverDEMAtlasSize = gs_ASMDEMTileSize / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDEMTileCoord = 1.5 / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDEMTileSize = (gs_ASMDEMTileSize - 3.0) / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDepthAtlasSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
const vec2 ASMOneOverDepthAtlasSize = 1.0 / vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
const float ASMHalfOverDepthTileSize = 0.5 / gs_ASMTileSize;
#endif


#endif