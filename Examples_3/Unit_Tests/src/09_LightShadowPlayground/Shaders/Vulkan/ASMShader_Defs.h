#ifndef _ASMSHADER_DEFS_H
#define _ASMSHADER_DEFS_H

#include "../../ASMConstant.h"

const vec2 ASMDepthAtlasSizeOverDepthTileSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight) / gs_ASMTileSize;
const vec2 ASMDEMTileSizeOverDEMAtlasSize = gs_ASMDEMTileSize / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDEMTileCoord = 1.5 / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDEMTileSize = (gs_ASMDEMTileSize - 3.0) / vec2(gs_ASMDEMAtlasTextureWidth, gs_ASMDEMAtlasTextureHeight);
const vec2 ASMDepthAtlasSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
const vec2 ASMOneOverDepthAtlasSize = 1.0 / vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
const float ASMHalfOverDepthTileSize = 0.5 / gs_ASMTileSize;

#endif