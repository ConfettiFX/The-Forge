/************************************************************************************

Filename    :   TextureAtlas.h
Content     :   A simple particle system for System Activities.
Created     :   October 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <vector>
#include <string>

#include "OVR_Math.h"

#include "OVR_FileSys.h"
#include "SurfaceRender.h"
#include "GlTexture.h"

namespace OVRFW {

// typedef float (*ovrAlphaFunc_t)( const double t );

class ovrTextureAtlas {
   public:
    // class describing each sprite in the atlas
    class ovrSpriteDef {
       public:
        ovrSpriteDef() : uvMins(0.0f, 0.0f), uvMaxs(1.0f, 1.0f) {}

        ovrSpriteDef(const char* name, const OVR::Vector2f& uvMins, const OVR::Vector2f& uvMaxs)
            : Name(name), uvMins(uvMins), uvMaxs(uvMaxs) {}

        ovrSpriteDef(
            const char* name,
            const float u0,
            const float v0,
            const float u1,
            const float v1)
            : Name(name), uvMins(u0, v0), uvMaxs(u1, v1) {}

        std::string Name; // name of the sprite texture
        OVR::Vector2f uvMins; // bounds in texture space
        OVR::Vector2f uvMaxs;
    };

    ovrTextureAtlas();
    ~ovrTextureAtlas();

    // Specify the texture to load for this atlas.
    bool Init(ovrFileSys& fileSys, const char* atlasTextureName);

    bool SetSpriteDefs(const std::vector<ovrSpriteDef>& sprites);
    void SetSpriteName(const int index, const char* name);

    void Shutdown();

    // Divides the atlas texture evenly into columns * rows sprites. If numSprites is 0
    // then the function assumes all grid blocks are occupied by a valid sprite. If
    // numSprites != 0 then only the first numSprites slots are presumed filled.
    // The texture width and height should be evenly divisiable by the colums and rows to
    // avoid sub-pixel UV boundaries.
    bool
    BuildSpritesFromGrid(const int numSpriteColumns, const int numSpriteRows, const int numSprites);

    int GetNumSprites() const {
        return static_cast<int>(Sprites.size());
    }
    const GlTexture& GetTexture() const {
        return AtlasTexture;
    }
    const ovrSpriteDef& GetSpriteDef(const int index) const {
        return Sprites[index];
    }
    const ovrSpriteDef& GetSpriteDef(const char* spriteName) const;
    const std::string& GetTextureName() const {
        return TextureName;
    }
    int GetTextureWidth() const {
        return TextureWidth;
    }
    int GetTextureHeight() const {
        return TextureHeight;
    }

    static void GetUVsForGridCell(
        const int x,
        const int y,
        const int numColumns,
        const int numRows,
        const int textureWidth,
        const int textureHeight,
        const int borderX,
        const int borderY,
        OVR::Vector2f& uvMins,
        OVR::Vector2f& uvMaxs);

   private:
    std::vector<ovrSpriteDef> Sprites;
    GlTexture AtlasTexture;
    int TextureWidth;
    int TextureHeight;
    std::string TextureName;
};

} // namespace OVRFW
