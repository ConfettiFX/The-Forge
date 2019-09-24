//
//  ASMShader_Defs.h
//  09_LightShadowPlayground
//
//  Created by macOS Metal Machine on 7/28/19.
//  Copyright © 2019 Confetti-FX. All rights reserved.
//
#ifndef _ASMCONSTANT_H
#define _ASMCONSTANT_H

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

#endif
