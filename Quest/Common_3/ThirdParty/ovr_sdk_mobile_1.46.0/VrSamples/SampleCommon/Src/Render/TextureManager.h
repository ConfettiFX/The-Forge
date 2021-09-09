/************************************************************************************

Filename    :   OVR_TextureManager.cpp
Content     :   Keeps track of textures so they don't need to be loaded more than once.
Created     :   1/22/2016
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include "OVR_TypesafeNumber.h"
#include "GlTexture.h"

#include <string>

namespace OVRFW {

enum ovrTextureHandle { INVALID_TEXTURE_HANDLE = -1 };

typedef OVR::TypesafeNumberT<int, ovrTextureHandle, INVALID_TEXTURE_HANDLE> textureHandle_t;

class ovrManagedTexture {
   public:
    enum ovrTextureSource { TEXTURE_SOURCE_URI, TEXTURE_SOURCE_ICON, TEXTURE_SOURCE_MAX };

    ovrManagedTexture() : Source(TEXTURE_SOURCE_MAX), IconId(-1) {}
    ovrManagedTexture(textureHandle_t const handle, char const* uri, GlTexture const& texture)
        : Handle(handle), Texture(texture), Source(TEXTURE_SOURCE_URI), Uri(uri) {}

    ovrManagedTexture(textureHandle_t const handle, int const iconId, GlTexture const& texture)
        : Handle(handle), Texture(texture), Source(TEXTURE_SOURCE_ICON), IconId(iconId) {}

    void Free();

    textureHandle_t GetHandle() const {
        return Handle;
    }
    GlTexture const& GetTexture() const {
        return Texture;
    }
    ovrTextureSource GetSource() const {
        return Source;
    }
    std::string const& GetUri() const {
        return Uri;
    }
    int GetIconId() const {
        return IconId;
    }
    bool IsValid() const {
        return Texture.IsValid();
    }

   private:
    textureHandle_t Handle; // handle of the texture
    GlTexture Texture; // the GL texture handle
    ovrTextureSource Source; // where this texture came from
    std::string Uri; // name of the uri, if the texture was loaded from a uri
    int IconId; // id of the icon, if loaded from an icon
};

class ovrTextureManager {
   public:
    enum ovrTextureWrap { WRAP_DEFAULT, WRAP_CLAMP, WRAP_REPEAT };
    enum ovrTextureFilter {
        FILTER_DEFAULT, // decided by load time based on image properties (i.e. mipmaps or not)
        FILTER_LINEAR,
        FILTER_MIPMAP_LINEAR,
        FILTER_MIPMAP_TRILINEAR,
        FILTER_MIPMAP_ANISOTROPIC2,
        FILTER_MIPMAP_ANISOTROPIC4,
        FILTER_MIPMAP_ANISOTROPIC8,
        FILTER_MIPMAP_ANISOTROPIC16

    };

    virtual ~ovrTextureManager() {}

    static ovrTextureManager* Create();
    static void Destroy(ovrTextureManager*& m);

    virtual void Init() = 0;
    virtual void Shutdown() = 0;

    virtual textureHandle_t LoadTexture(
        class ovrFileSys& fileSys,
        char const* uri,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) = 0;
    virtual textureHandle_t LoadTexture(
        char const* uri,
        void const* buffer,
        size_t const bufferSize,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) = 0;
    virtual textureHandle_t LoadRGBATexture(
        char const* uri,
        void const* imageData,
        int const imageWidth,
        int const imageHeight,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) = 0;
    virtual textureHandle_t LoadRGBATexture(
        int const iconId,
        void const* imageData,
        int const imageWidth,
        int const imageHeight,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) = 0;

    virtual void FreeTexture(textureHandle_t const handle) = 0;

    virtual ovrManagedTexture GetTexture(textureHandle_t const handle) const = 0;
    virtual GlTexture GetGlTexture(textureHandle_t const handle) const = 0;

    virtual textureHandle_t GetTextureHandle(char const* uri) const = 0;
    virtual textureHandle_t GetTextureHandle(int const iconId) const = 0;

    virtual void PrintStats() const = 0;
};

} // namespace OVRFW
