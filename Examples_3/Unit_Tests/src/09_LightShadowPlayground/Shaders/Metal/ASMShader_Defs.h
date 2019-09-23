//
//  ASMShader_Defs.h
//  09_LightShadowPlayground
//
//  Created by macOS Metal Machine on 7/28/19.
//  Copyright © 2019 Confetti-FX. All rights reserved.
//
#ifndef _ASMSHADER_DEFS_H
#define _ASMSHADER_DEFS_H

#include "ASMConstant.h"


constant float2 ASMDepthAtlasSizeOverDepthTileSize = float2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight) / gs_ASMTileSize ;
constant float2 ASMDEMTileSizeOverDEMAtlasSize = gs_ASMDEMTileSize / float2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight) ;
constant float2 ASMDEMTileCoord = 1.5 / float2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight) ;
constant float2 ASMDEMTileSize = (gs_ASMDEMTileSize - 3.0) / float2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight) ;
constant float2 ASMDepthAtlasSize = float2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight) ;
constant float2 ASMOneOverDepthAtlasSize = 1.0 / float2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight) ;
constant float2 ASMHalfOverDepthTileSize = 0.5 / gs_ASMTileSize ;


#endif
