/************************************************************************************

Filename    :   VRMenuObject.cpp
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "VRMenuObject.h"

#include "Misc/Log.h"
#include "Render/Egl.h"
#include "Render/GlTexture.h"
#include "Render/BitmapFont.h"
#include "Render/TextureManager.h"

#include "Locale/OVR_Locale.h"

#include "OVR_FileSys.h"

#include "OVR_Math.h"

#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "VRMenuComponent.h"
#include "ui_default.h" // embedded default UI texture (loaded as a placeholder when something doesn't load)
#include "Reflection.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

#define USE_TEXTURE_MANAGER

inline bool Intersect_RayBounds(
    const Vector3f& rayStart,
    const Vector3f& rayDir,
    const Vector3f& mins,
    const Vector3f& maxs,
    float& t0,
    float& t1) {
    const float rcpDirX = (fabsf(rayDir.x) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.x)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpDirY = (fabsf(rayDir.y) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.y)
        : MATH_FLOAT_HUGE_NUMBER;
    const float rcpDirZ = (fabsf(rayDir.z) > MATH_FLOAT_SMALLEST_NON_DENORMAL)
        ? (1.0f / rayDir.z)
        : MATH_FLOAT_HUGE_NUMBER;

    const float sX = (mins.x - rayStart.x) * rcpDirX;
    const float sY = (mins.y - rayStart.y) * rcpDirY;
    const float sZ = (mins.z - rayStart.z) * rcpDirZ;

    const float tX = (maxs.x - rayStart.x) * rcpDirX;
    const float tY = (maxs.y - rayStart.y) * rcpDirY;
    const float tZ = (maxs.z - rayStart.z) * rcpDirZ;

    const float minX = std::min(sX, tX);
    const float minY = std::min(sY, tY);
    const float minZ = std::min(sZ, tZ);

    const float maxX = std::max(sX, tX);
    const float maxY = std::max(sY, tY);
    const float maxZ = std::max(sZ, tZ);

    t0 = std::max(minX, std::max(minY, minZ));
    t1 = std::min(maxX, std::min(maxY, maxZ));

    return (t0 <= t1);
}

namespace OVRFW {

float const VRMenuObject::TEXELS_PER_METER = 500.0f;
float const VRMenuObject::DEFAULT_TEXEL_SCALE = 1.0f / TEXELS_PER_METER;

const float VRMenuSurface::Z_BOUNDS = 0.05f;

//======================================================================================
// VRMenuSurfaceTexture

//==============================
// VRMenuSurfaceTexture::VRMenuSurfaceTexture::
VRMenuSurfaceTexture::VRMenuSurfaceTexture() : Type(SURFACE_TEXTURE_MAX), OwnsTexture(false) {}

//==============================
// VRMenuSurfaceTexture::LoadTexture
bool VRMenuSurfaceTexture::LoadTexture(
    OvrGuiSys& guiSys,
    eSurfaceTextureType const type,
    char const* imageName,
    bool const allowDefault) {
    Free();

    assert(type >= 0 && type < SURFACE_TEXTURE_MAX);

    Type = type;

    if (imageName != NULL && imageName[0] != '\0') {
#if defined(USE_TEXTURE_MANAGER)
        textureHandle_t const h =
            guiSys.GetTextureManager().LoadTexture(guiSys.GetFileSys(), imageName);
        Texture = guiSys.GetTextureManager().GetGlTexture(h);
#else
        std::vector<uint8_t> buffer;
        if (guiSys.GetFileSys().ReadFile(imageName, buffer)) {
            int w;
            int h;
            Texture = LoadTextureFromBuffer(
                imageName,
                MemBuffer(buffer, static_cast<int>(buffer.GetSize())),
                TextureFlags_t(TEXTUREFLAG_NO_DEFAULT),
                w,
                h);
        }
#endif
    }

    if (!Texture.IsValid() && allowDefault) {
#if defined(USE_TEXTURE_MANAGER)
        textureHandle_t const h =
            guiSys.GetTextureManager().LoadTexture("<default>", uiDefaultTgaData, uiDefaultTgaSize);
        Texture = guiSys.GetTextureManager().GetGlTexture(h);
#else
        int w;
        int h;
        Texture = LoadTextureFromBuffer(
            imageName, MemBuffer(uiDefaultTgaData, uiDefaultTgaSize), TextureFlags_t(), w, h);
#endif
        ALOGW(
            "VRMenuSurfaceTexture::CreateFromImage: failed to load image '%s' - default loaded instead!",
            imageName);
    }

    // if allocated via the texture manager we cannot "own" the texture -- since the texture manager
    // may have given the same handle to anyone else who asked.
#if !defined(USE_TEXTURE_MANAGER)
    OwnsTexture = true;
#endif
    return Texture.IsValid();
}

//==============================
// VRMenuSurfaceTexture::LoadTexture
void VRMenuSurfaceTexture::LoadTexture(
    eSurfaceTextureType const type,
    const GLuint texId,
    const int width,
    const int height) {
    Free();

    assert(type >= 0 && type < SURFACE_TEXTURE_MAX);

    Type = type;
    OwnsTexture = false;
    Texture = GlTexture(texId, width, height);
}

//==============================
// VRMenuSurfaceTexture::Free
void VRMenuSurfaceTexture::Free() {
    if (Texture.IsValid()) {
        if (OwnsTexture) {
            DeleteTexture(Texture);
        }
        Type = SURFACE_TEXTURE_MAX;
        OwnsTexture = false;
    }
}

//======================================================================================
// VRMenuSurfaceTris

//======================================================================================
// VRMenuSurface

#if 0
static void PrintBounds( const char * name, char const * prefix, Bounds3f const & bounds )
{
	OVR_LOG( "'%s' %s: min( %.2f, %.2f, %.2f ) - max( %.2f, %.2f, %.2f )",
		name, prefix,
		bounds.GetMins().x, bounds.GetMins().y, bounds.GetMins().z,
		bounds.GetMaxs().x, bounds.GetMaxs().y, bounds.GetMaxs().z );
}
#endif

//==============================
// VRMenuSurface::VRMenuSurface
VRMenuSurface::VRMenuSurface()
    : Color(1.0f)
      //, TextureDims( 0, 0 )
      //, Dims( 0.0f, 0.0f )
      //, Anchors( 0.0f, 0.0f, 1.0f
      ,
      Border(0.0f, 0.0f, 0.0f, 0.0f),
      ClipUVs(
          -1.0f,
          -1.0f,
          2.0f,
          2.0f) // if we clip exactly at the edges we get sharp, aliased edges
      ,
      Contents(CONTENT_SOLID),
      Visible(true),
      ProgramType(PROGRAM_MAX) {}

//==============================
// VRMenuSurface::~VRMenuSurface
VRMenuSurface::~VRMenuSurface() {
    Free();
}

//==============================
// VRMenuSurface::CreateImageGeometry
//
// This creates a quad for mapping the texture.
void VRMenuSurface::CreateImageGeometry(
    int const textureWidth,
    int const textureHeight,
    const Vector2f& dims,
    const Vector4f& border,
    const Vector4f& cropUV,
    ContentFlags_t const contents) {
    // assert( SurfaceDef.geo.vertexBuffer == 0 && SurfaceDef.geo.indexBuffer == 0 &&
    // SurfaceDef.geo.vertexArrayObject == 0 );

    TriangleIndex vertsX = 0;
    TriangleIndex vertsY = 0;
    float vertUVX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vertUVY[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float vertPosX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vertPosY[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // x components
    vertPosX[vertsX] = 0.0f;
    vertUVX[vertsX++] = 0.0f;

    if (border[BORDER_LEFT] > 0.0f) {
        vertPosX[vertsX] = border[BORDER_LEFT] / dims.x;
        vertUVX[vertsX++] = border[BORDER_LEFT] / (float)textureWidth;
    }

    if (border[BORDER_RIGHT] > 0.0f) {
        vertPosX[vertsX] = 1.0f - border[BORDER_RIGHT] / dims.x;
        vertUVX[vertsX++] = 1.0f - border[BORDER_RIGHT] / (float)textureWidth;
    }

    vertPosX[vertsX] = 1.0f;
    vertUVX[vertsX++] = 1.0f;

    // y components
    vertPosY[vertsY] = 0.0f;
    vertUVY[vertsY++] = 0.0f;

    if (border[BORDER_BOTTOM] > 0.0f) {
        vertPosY[vertsY] = border[BORDER_BOTTOM] / dims.y;
        vertUVY[vertsY++] = border[BORDER_BOTTOM] / (float)textureHeight;
    }

    if (border[BORDER_TOP] > 0.0f) {
        vertPosY[vertsY] = 1.0f - border[BORDER_TOP] / dims.y;
        vertUVY[vertsY++] = 1.0f - border[BORDER_TOP] / (float)textureHeight;
    }

    vertPosY[vertsY] = 1.0f;
    vertUVY[vertsY++] = 1.0f;

    for (int i = 0; i < vertsX; i++) {
        vertUVX[i] = (cropUV.z - cropUV.x) * vertUVX[i] + cropUV.x;
    }

    for (int i = 0; i < vertsY; i++) {
        vertUVY[i] = (cropUV.w - cropUV.y) * vertUVY[i] + cropUV.y;
    }

    // create the vertices
    const int vertexCount = vertsX * vertsY;
    const TriangleIndex horizontal = vertsX - 1;
    const TriangleIndex vertical = vertsY - 1;

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.uv1.resize(vertexCount);
    attribs.color.resize(vertexCount);

    Vector4f color(1.0f, 1.0f, 1.0f, 1.0f);

    for (int y = 0; y <= vertical; y++) {
        const float yPos =
            (-1 + vertPosY[y] * 2) * (dims.y * VRMenuObject::DEFAULT_TEXEL_SCALE * 0.5f);
        const float uvY = 1.0f - vertUVY[y];

        for (int x = 0; x <= horizontal; x++) {
            const int index = y * (horizontal + 1) + x;
            attribs.position[index].x =
                (-1 + vertPosX[x] * 2) * (dims.x * VRMenuObject::DEFAULT_TEXEL_SCALE * 0.5f);
            attribs.position[index].z = 0;
            attribs.position[index].y = yPos;
            attribs.uv0[index].x = vertUVX[x];
            attribs.uv0[index].y = uvY;
            attribs.uv1[index] = attribs.uv0[index];
            attribs.color[index] = color;
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6);

    // If this is to be used to draw a linear format texture, like
    // a surface texture, it is better for cache performance that
    // the triangles be drawn to follow the side to side linear order.
    int index = 0;
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    Tris.Init(attribs.position, indices, attribs.uv0, contents);

    if (SurfaceDef.geo.vertexBuffer == 0 && SurfaceDef.geo.indexBuffer == 0 &&
        SurfaceDef.geo.vertexArrayObject == 0) {
        SurfaceDef.geo.Create(attribs, indices);
    } else {
        SurfaceDef.geo.Update(attribs);
    }
}

//==============================
// VRMenuSurface::BuildDrawSurface
void VRMenuObject::BuildDrawSurface(
    OvrVRMenuMgr const& menuMgr,
    Matrix4f const& modelMatrix,
    const char* surfaceName,
    int const surfaceIndex,
    Vector4f const& color,
    Vector3f const& fadeDirection,
    Vector2f const& colorTableOffset,
    Vector4f const& clipUVs,
    Vector2f const& offsetUVs,
    bool const skipAdditivePass,
    VRMenuRenderFlags_t const& flags,
    Bounds3f const& localBounds,
    std::vector<ovrDrawSurface>& surfaceList) {
    int n;
    if (surfaceIndex < 0) {
        // this means we're only submitting an instanced text surface
        n = 1;
        surfaceList.resize(surfaceList.size() + 1);
    } else {
        // add one draw surface
        n = (flags & VRMenuRenderFlags_t(VRMENU_RENDER_SUBMIT_TEXT_SURFACE)) != 0 ? 2 : 1;
        surfaceList.resize(surfaceList.size() + n);

        Surfaces[surfaceIndex].BuildDrawSurface(
            menuMgr,
            modelMatrix,
            surfaceName,
            color,
            fadeDirection,
            colorTableOffset,
            clipUVs,
            offsetUVs,
            skipAdditivePass,
            flags,
            localBounds,
            surfaceList[surfaceList.size() - n]);
        n--;
    }

    if (n >= 1) {
        assert(TextSurface != nullptr);
        /// TextSurface->SurfaceDef.graphicsCommand.uniformValues[0][3] = color.w;
        surfaceList[surfaceList.size() - n].modelMatrix = TextSurface->ModelMatrix;
        surfaceList[surfaceList.size() - n].surface = &TextSurface->SurfaceDef;
    }
}

//==============================
// VRMenuSurface::BuildDrawSurface
// TODO: Ideally the materialDef only needs to be set up once unless it's been changed, but
// some menu items can have their surfaces changed on the fly (such as background-loaded thumbnails)
void VRMenuSurface::BuildDrawSurface(
    OvrVRMenuMgr const& menuMgr,
    Matrix4f const& modelMatrix,
    const char* surfaceName,
    Vector4f const& color,
    Vector3f const& fadeDirection,
    Vector2f const& colorTableOffset,
    Vector4f const& clipUVs,
    Vector2f const& offsetUVs,
    bool const skipAdditivePass,
    VRMenuRenderFlags_t const& flags,
    Bounds3f const& localBounds,
    ovrDrawSurface& outSurf) {
    outSurf.modelMatrix = modelMatrix;
#if defined(OVR_BUILD_DEBUG) // skip these allocations in release builds
    SurfaceDef.surfaceName = surfaceName;
#endif
    SurfaceDef.geo.localBounds = localBounds;
    outSurf.surface = &SurfaceDef;

    ovrGraphicsCommand& gc = SurfaceDef.graphicsCommand;

    GlProgram const* program = NULL;

    eGUIProgramType pt = ProgramType;
    if (skipAdditivePass) {
        if (pt == PROGRAM_DIFFUSE_PLUS_ADDITIVE || pt == PROGRAM_DIFFUSE_COMPOSITE) {
            pt = PROGRAM_DIFFUSE_ONLY; // this is used to not render the gaze-over hilights
        }
    }

    program = menuMgr.GetGUIGlProgram(pt);
    if (program == NULL) {
        assert(program != NULL);
        return;
    }

    gc.Program = *program;

    /// Update local parameters
    Color = color;
    FadeDirection = fadeDirection;
    ClipUVs = clipUVs;
    OffsetUVs = offsetUVs;
    ColorTableOffset = colorTableOffset;

    /// uniform binding - match the uniforms to what they were setup in VRMenuMgrLocal::Init
    int additiveIndex = IndexForTextureType(SURFACE_TEXTURE_ADDITIVE, 1);
    int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
    int diffuseADIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE_ALPHA_DISCARD, 1);
    int alphaIndex = IndexForTextureType(SURFACE_TEXTURE_ALPHA_MASK, 1);
    int diffuse2Index = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 2);
    int rampIndex = IndexForTextureType(SURFACE_TEXTURE_COLOR_RAMP, 1);
    int targetIndex = IndexForTextureType(SURFACE_TEXTURE_COLOR_RAMP_TARGET, 1);
    switch (pt) {
        case PROGRAM_DIFFUSE_ONLY:
            gc.Textures[0] = Textures[diffuseIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &OffsetUVs;
            gc.UniformData[3].Data = &gc.Textures[0];
            gc.UniformData[4].Data = &ClipUVs;
            break;

        case PROGRAM_ADDITIVE_ONLY:
            gc.Textures[0] = Textures[additiveIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &OffsetUVs;
            gc.UniformData[3].Data = &gc.Textures[0];
            break;

        case PROGRAM_DIFFUSE_ALPHA_DISCARD:
            gc.Textures[0] = Textures[diffuseADIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &OffsetUVs;
            gc.UniformData[3].Data = &gc.Textures[0];
            gc.UniformData[4].Data = &ClipUVs;
            break;

        case PROGRAM_DIFFUSE_PLUS_ADDITIVE:
            gc.Textures[0] = Textures[diffuseIndex].GetTexture();
            gc.Textures[1] = Textures[additiveIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &gc.Textures[0];
            gc.UniformData[3].Data = &gc.Textures[1];
            break;

        case PROGRAM_DIFFUSE_COMPOSITE:
            gc.Textures[0] = Textures[diffuseIndex].GetTexture();
            gc.Textures[1] = Textures[diffuse2Index].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &gc.Textures[0];
            gc.UniformData[3].Data = &gc.Textures[1];
            break;

        case PROGRAM_DIFFUSE_COLOR_RAMP:
            gc.Textures[0] = Textures[diffuseIndex].GetTexture();
            gc.Textures[1] = Textures[targetIndex].GetTexture();
            gc.Textures[2] = Textures[rampIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &OffsetUVs;
            gc.UniformData[3].Data = &gc.Textures[0];
            gc.UniformData[4].Data = &gc.Textures[1];
            gc.UniformData[5].Data = &gc.Textures[2];
            gc.UniformData[6].Data = &ColorTableOffset;
            break;

        case PROGRAM_DIFFUSE_COLOR_RAMP_TARGET:
            gc.Textures[0] = Textures[diffuseIndex].GetTexture();
            gc.Textures[1] = Textures[targetIndex].GetTexture();
            gc.Textures[2] = Textures[rampIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &gc.Textures[0];
            gc.UniformData[3].Data = &gc.Textures[1];
            gc.UniformData[4].Data = &gc.Textures[2];
            gc.UniformData[5].Data = &ColorTableOffset;
            break;

        case PROGRAM_ALPHA_DIFFUSE:
            gc.Textures[0] = Textures[alphaIndex].GetTexture();
            gc.Textures[1] = Textures[diffuseIndex].GetTexture();
            gc.UniformData[0].Data = &Color;
            gc.UniformData[1].Data = &FadeDirection;
            gc.UniformData[2].Data = &gc.Textures[0];
            gc.UniformData[3].Data = &gc.Textures[1];
            break;

        default:
            /// assert_WITH_TAG( !"Invalid gui program type", "VrMenu" );
            break;
    }

    // most programs use normal blending
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.depthEnable = (flags & VRMENU_RENDER_NO_DEPTH) != 0 ? false : true;
    gc.GpuState.depthMaskEnable = (flags & VRMENU_RENDER_NO_DEPTH_MASK) != 0 ? false : true;
    gc.GpuState.polygonOffsetEnable = (flags & VRMENU_RENDER_POLYGON_OFFSET) != 0 ? true : false;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = pt == PROGRAM_ADDITIVE_ONLY ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA;
    gc.GpuState.cullEnable = true;
}

//==============================
// VRMenuSurface::SetTextureSampling
//==============================
void VRMenuSurface::SetTextureSampling(eGUIProgramType const pt) {
    switch (pt) {
        case PROGRAM_DIFFUSE_ONLY:
        case PROGRAM_DIFFUSE_ALPHA_DISCARD: {
            int diffuseIndex = IndexForTextureType(
                pt == PROGRAM_DIFFUSE_ONLY ? SURFACE_TEXTURE_DIFFUSE
                                           : SURFACE_TEXTURE_DIFFUSE_ALPHA_DISCARD,
                1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind the texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_DIFFUSE_COMPOSITE: {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int diffuse2Index = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 2);
            /// assert_WITH_TAG( diffuse2Index >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind both textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuse2Index].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_ALPHA_DIFFUSE: {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            int alphaIndex = IndexForTextureType(SURFACE_TEXTURE_ALPHA_MASK, 1);
            /// assert_WITH_TAG( alphaIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind both textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[alphaIndex].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_ADDITIVE_ONLY: {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            int additiveIndex = IndexForTextureType(SURFACE_TEXTURE_ADDITIVE, 1);
            /// assert_WITH_TAG( additiveIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind the texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[additiveIndex].GetTexture().texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_DIFFUSE_PLUS_ADDITIVE: // has a diffuse and an additive
        {
            // glBlendFunc( GL_ONE, GL_ONE );
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int additiveIndex = IndexForTextureType(SURFACE_TEXTURE_ADDITIVE, 1);
            /// assert_WITH_TAG( additiveIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind both textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Textures[additiveIndex].GetTexture().texture);
            break;
        }
        case PROGRAM_DIFFUSE_COLOR_RAMP: // has a diffuse and color ramp, and color ramp target is
                                         // the diffuse
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int rampIndex = IndexForTextureType(SURFACE_TEXTURE_COLOR_RAMP, 1);
            /// assert_WITH_TAG( rampIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind both textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Textures[rampIndex].GetTexture().texture);
            // do not do any filtering on the "palette" texture
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_DIFFUSE_COLOR_RAMP_TARGET: // has diffuse, color ramp, and a separate color
                                                // ramp target
        {
            // ALOG( "Surface '%s' - PROGRAM_COLOR_RAMP_TARGET", SurfaceName );
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            int diffuseIndex = IndexForTextureType(SURFACE_TEXTURE_DIFFUSE, 1);
            /// assert_WITH_TAG( diffuseIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int rampIndex = IndexForTextureType(SURFACE_TEXTURE_COLOR_RAMP, 1);
            /// assert_WITH_TAG( rampIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            int targetIndex = IndexForTextureType(SURFACE_TEXTURE_COLOR_RAMP_TARGET, 1);
            /// assert_WITH_TAG( targetIndex >= 0, "VrMenu" );	// surface setup should have
            /// detected this!
            // bind both textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Textures[diffuseIndex].GetTexture().texture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Textures[targetIndex].GetTexture().texture);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, Textures[rampIndex].GetTexture().texture);
            // do not do any filtering on the "palette" texture
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        }
        case PROGRAM_MAX: {
            ALOGW("Unsupported texture map combination.");
            return;
        }
        default: {
            /// assert_WITH_TAG( !"Unhandled ProgramType", "Uhandled ProgramType" );
            return;
        }
    }
}

/// OVR_PERF_ACCUMULATOR( VerifyImageParms );
/// OVR_PERF_ACCUMULATOR( FindSurfaceForGeoSizing );
/// OVR_PERF_ACCUMULATOR( CreateImageGeometry );
/// OVR_PERF_ACCUMULATOR( SelectProgramType );
/// OVR_PERF_ACCUMULATOR( CreateFromSurfaceParms );

//==============================
// VRMenuSurface::CreateFromSurfaceParms
void VRMenuSurface::CreateFromSurfaceParms(OvrGuiSys& guiSys, VRMenuSurfaceParms const& parms) {
    /// OVR_PERF_TIMER( CreateFromSurfaceParms );

    Free();

    SurfaceName = parms.SurfaceName;

    {
        /// OVR_PERF_TIMER( VerifyImageParms );
        // verify the input parms have a valid image name and texture type
        bool isValid = false;
        for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
            if (!parms.ImageNames[i].empty() &&
                (parms.TextureTypes[i] >= SURFACE_TEXTURE_DIFFUSE &&
                 parms.TextureTypes[i] < SURFACE_TEXTURE_MAX)) {
                isValid = true;
                Textures[i].LoadTexture(
                    guiSys, parms.TextureTypes[i], parms.ImageNames[i].c_str(), true);
            } else if (
                (parms.ImageTexId[i] != 0) &&
                (parms.TextureTypes[i] >= SURFACE_TEXTURE_DIFFUSE &&
                 parms.TextureTypes[i] < SURFACE_TEXTURE_MAX)) {
                isValid = true;
                Textures[i].LoadTexture(
                    parms.TextureTypes[i],
                    parms.ImageTexId[i],
                    parms.ImageWidth[i],
                    parms.ImageHeight[i]);
            }
        }
        /// OVR_PERF_ACCUMULATE( VerifyImageParms );
        if (!isValid) {
            // OVR_LOG( "VRMenuSurfaceParms '%s' - no valid images - skipping",
            // parms.SurfaceName.c_str() );
            return;
        }
    }

    int surfaceIdx = -1;
    {
        /// OVR_PERF_TIMER( FindSurfaceForGeoSizing );
        // make sure we have a surface for sizing the geometry
        for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
            if (Textures[i].GetTexture().texture != 0) {
                surfaceIdx = i;
                break;
            }
        }
        /// OVR_PERF_ACCUMULATE( FindSurfaceForGeoSizing );
        if (surfaceIdx < 0) {
            // OVR_LOG( "VRMenuSurface::CreateFromImageParms - no suitable image for surface
            // creation" );
            return;
        }
    }

    TextureDims.x = Textures[surfaceIdx].GetWidth();
    TextureDims.y = Textures[surfaceIdx].GetHeight();

    if ((parms.Dims.x == 0) || (parms.Dims.y == 0)) {
        Dims.x = static_cast<float>(TextureDims.x);
        Dims.y = static_cast<float>(TextureDims.y);
    } else {
        Dims = parms.Dims;
    }

    Border = parms.Border;
    CropUV = parms.CropUV;
    OffsetUVs = parms.OffsetUVs;
    Anchors = parms.Anchors;
    Contents = parms.Contents;
    Color = parms.Color;

    {
        /// OVR_PERF_TIMER( CreateImageGeometry );
        CreateImageGeometry(TextureDims.x, TextureDims.y, Dims, Border, CropUV, Contents);
        /// OVR_PERF_ACCUMULATE( CreateImageGeometry );
    }

    {
        /// OVR_PERF_TIMER( SelectProgramType );
        // now, based on the combination of surfaces, determine the render prog to use
        if (HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_COLOR_RAMP, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_COLOR_RAMP_TARGET, 1)) {
            ProgramType = PROGRAM_DIFFUSE_COLOR_RAMP_TARGET;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 2)) {
            ProgramType = PROGRAM_DIFFUSE_ONLY;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE_ALPHA_DISCARD, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 2)) {
            ProgramType = PROGRAM_DIFFUSE_ALPHA_DISCARD;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_ADDITIVE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 2)) {
            ProgramType = PROGRAM_ADDITIVE_ONLY;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 2) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 1)) {
            ProgramType = PROGRAM_DIFFUSE_COMPOSITE;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_COLOR_RAMP, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 1)) {
            ProgramType = PROGRAM_DIFFUSE_COLOR_RAMP;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_ADDITIVE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 1)) {
            ProgramType = PROGRAM_DIFFUSE_PLUS_ADDITIVE;
        } else if (
            HasTexturesOfType(SURFACE_TEXTURE_ALPHA_MASK, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_DIFFUSE, 1) &&
            HasTexturesOfType(SURFACE_TEXTURE_MAX, 1)) {
            ProgramType = PROGRAM_ALPHA_DIFFUSE;
        } else {
            ALOGW("Invalid material combination -- either add a shader to support it or fix it.");
            ProgramType = PROGRAM_MAX;
        }
        /// OVR_PERF_ACCUMULATE( SelectProgramType );
    }

    SetTextureSampling(ProgramType);

    /// OVR_PERF_ACCUMULATE( CreateFromSurfaceParms );
}

//==============================
// VRMenuSurface::RegenerateSurfaceGeometry
void VRMenuSurface::RegenerateSurfaceGeometry() {
    CreateImageGeometry(TextureDims.x, TextureDims.y, Dims, Border, CropUV, Contents);
}

//==============================
// VRMenuSurface::
bool VRMenuSurface::HasTexturesOfType(eSurfaceTextureType const t, int const requiredCount) const {
    int count = 0;
    for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
        if (Textures[i].GetType() == t) {
            count++;
        }
    }
    return (requiredCount == count); // must be the exact same number
}

int VRMenuSurface::IndexForTextureType(eSurfaceTextureType const t, int const occurenceCount)
    const {
    int count = 0;
    for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
        if (Textures[i].GetType() == t) {
            count++;
            if (count == occurenceCount) {
                return i;
            }
        }
    }
    return -1;
}

//==============================
// VRMenuSurface::Free
void VRMenuSurface::Free() {
    for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
        Textures[i].Free();
    }
}

//==============================
// VRMenuSurface::IntersectRay
bool VRMenuSurface::IntersectRay(
    Vector3f const& start,
    Vector3f const& dir,
    Posef const& pose,
    Vector3f const& scale,
    ContentFlags_t const testContents,
    OvrCollisionResult& result) const {
    return Tris.IntersectRay(start, dir, pose, scale, testContents, result);
}

//==============================
// VRMenuSurface::IntersectRay
bool VRMenuSurface::IntersectRay(
    Vector3f const& localStart,
    Vector3f const& localDir,
    Vector3f const& scale,
    ContentFlags_t const testContents,
    OvrCollisionResult& result) const {
    return Tris.IntersectRay(localStart, localDir, scale, testContents, result);
}

//==============================
// VRMenuSurface::LoadTexture
void VRMenuSurface::LoadTexture(
    OvrGuiSys& guiSys,
    int const textureIndex,
    eSurfaceTextureType const type,
    char const* imageName) {
    if (textureIndex < 0 || textureIndex >= VRMENUSURFACE_IMAGE_MAX) {
        /// assert_WITH_TAG( textureIndex >= 0 && textureIndex < VRMENUSURFACE_IMAGE_MAX, "VrMenu"
        /// );
        return;
    }
    Textures[textureIndex].LoadTexture(guiSys, type, imageName, true);
}

//==============================
// VRMenuSurface::LoadTexture
void VRMenuSurface::LoadTexture(
    int const textureIndex,
    eSurfaceTextureType const type,
    const GLuint texId,
    const int width,
    const int height) {
    if (textureIndex < 0 || textureIndex >= VRMENUSURFACE_IMAGE_MAX) {
        /// assert_WITH_TAG( textureIndex >= 0 && textureIndex < VRMENUSURFACE_IMAGE_MAX, "VrMenu"
        /// );
        return;
    }
    Textures[textureIndex].LoadTexture(type, texId, width, height);
}

//==============================
// VRMenuSurface::GetAnchorOffsets
Vector2f VRMenuSurface::GetAnchorOffsets() const {
    return Vector2f(
        ((1.0f - Anchors.x) - 0.5f) * Dims.x *
            VRMenuObject::DEFAULT_TEXEL_SCALE, // inverted so that 0.0 is left-aligned
        (Anchors.y - 0.5f) * Dims.y * VRMenuObject::DEFAULT_TEXEL_SCALE);
}

void VRMenuSurface::SetOwnership(int const index, bool const isOwner) {
    Textures[index].SetOwnership(isOwner);
}

//======================================================================================
// VRMenuObject

//==================================
// VRMenuObject::VRMenuObject
VRMenuObject::VRMenuObject(VRMenuObjectParms const& parms, menuHandle_t const handle)
    : Type(parms.Type),
      Handle(handle),
      Id(parms.Id),
      Flags(parms.Flags),
      Name(parms.Name),
      Tag(parms.Tag),
      LocalPose(parms.LocalPose),
      LocalScale(parms.LocalScale),
      HilightPose(Quatf(), Vector3f(0.0f, 0.0f, 0.0f)),
      HilightScale(1.0f),
      WrapScale(1.0f),
      TextLocalPose(parms.TextLocalPose),
      TextLocalScale(parms.TextLocalScale),
      Text(parms.Text),
      CollisionPrimitive(NULL),
      Contents(parms.Contents),
      Color(parms.Color),
      TextColor(parms.TextColor),
      ColorTableOffset(0.0f),
      FontParms(parms.FontParms),
      Hilighted(false),
      Selected(false),
      TextDirty(true),
      MinsBoundsExpand(0.0f),
      MaxsBoundsExpand(0.0f),
      TextMetrics(),
      TextSurface(nullptr) {
    CullBounds.Clear();
}

//==================================
// VRMenuObject::~VRMenuObject
VRMenuObject::~VRMenuObject() {
    if (CollisionPrimitive != nullptr) {
        delete CollisionPrimitive;
        CollisionPrimitive = nullptr;
    }

    // all components must be dynamically allocated
    // this is critical
    for (int i = 0; i < static_cast<int>(Components.size()); ++i) {
        if (Components[i]) {
            delete Components[i];
        }
        Components[i] = nullptr;
    }
    Components.clear();
    Handle.Release();
    ParentHandle.Release();
    FreeTextSurface();
    Type = VRMENU_MAX;
}

/// OVR_PERF_ACCUMULATOR( VRMenuObjectInit );

//==================================
// VRMenuObject::Init
void VRMenuObject::Init(OvrGuiSys& guiSys, VRMenuObjectParms const& parms) {
    /// OVR_PERF_TIMER( VRMenuObjectInit );
    for (int i = 0; i < static_cast<int>(parms.SurfaceParms.size()); ++i) {
        int idx = AllocSurface();
        Surfaces[idx].CreateFromSurfaceParms(guiSys, parms.SurfaceParms[i]);
    }

    // bounds are nothing submitted for rendering
    CullBounds = Bounds3f(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    FontParms = parms.FontParms;
    for (int i = 0; i < static_cast<int>(parms.Components.size()); ++i) {
        AddComponent(parms.Components[i]);
    }

    if (parms.TexelCoords) {
        LocalPose.Translation = {
            LocalPose.Translation.x * DEFAULT_TEXEL_SCALE,
            LocalPose.Translation.y * DEFAULT_TEXEL_SCALE,
            LocalPose.Translation.z};
        TextLocalPose.Translation = {
            TextLocalPose.Translation.x * DEFAULT_TEXEL_SCALE,
            TextLocalPose.Translation.y * DEFAULT_TEXEL_SCALE,
            TextLocalPose.Translation.z};
        if (Surfaces.size() > 0) {
            Vector2f dims = Surfaces[0].GetDims();
            LocalScale = Vector3f(LocalScale.x / dims.x, LocalScale.y / dims.y, LocalScale.z);
        }
        FontParms.WrapWidth *= DEFAULT_TEXEL_SCALE;
    }
    Selected = parms.Selected;

    /// OVR_PERF_ACCUMULATE( VRMenuObjectInit );
}

//==================================
// VRMenuObject::FreeChildren
void VRMenuObject::FreeChildren(OvrVRMenuMgr& menuMgr) {
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        menuMgr.FreeObject(Children[i]);
    }
    Children.resize(0);
    // NOTE! bounds will be incorrect now until submitted for rendering
}

//==================================
// VRMenuObject::IsDescendant
bool VRMenuObject::IsDescendant(OvrVRMenuMgr& menuMgr, menuHandle_t const handle) const {
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        if (Children[i] == handle) {
            return true;
        }
    }

    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        VRMenuObject* child = menuMgr.ToObject(Children[i]);
        if (child != NULL) {
            bool r = child->IsDescendant(menuMgr, handle);
            if (r) {
                return true;
            }
        }
    }

    return false;
}

//==============================
// VRMenuObject::AddChild
void VRMenuObject::AddChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle) {
    Children.push_back(handle);

    VRMenuObject* child = menuMgr.ToObject(handle);
    if (child != NULL) {
        child->SetParentHandle(this->Handle);
    }
    // NOTE: bounds will be incorrect until submitted for rendering
}

//==============================
// VRMenuObject::RemoveChild
void VRMenuObject::RemoveChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle) {
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        if (Children[i] == handle) {
            Children.erase(Children.cbegin() + i);
            return;
        }
    }
}

//==============================
// VRMenuObject::FreeChild
void VRMenuObject::FreeChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle) {
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        menuHandle_t childHandle = Children[i];
        if (childHandle == handle) {
            Children.erase(Children.cbegin() + i);
            menuMgr.FreeObject(childHandle);
            return;
        }
    }
}

//==============================
// VRMenuObject::Frame
void VRMenuObject::Frame(OvrVRMenuMgr& menuMgr, Matrix4f const& viewMatrix) {
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        VRMenuObject* child = menuMgr.ToObject(Children[i]);
        if (child != NULL) {
            child->Frame(menuMgr, viewMatrix);
        }
    }
}

//==============================
// IntersectRayBounds
// Reports true if the hit was at or beyond start in the ray direction,
// or if the start point was inside of the bounds.
bool VRMenuObject::IntersectRayBounds(
    Vector3f const& start,
    Vector3f const& dir,
    Vector3f const& mins,
    Vector3f const& maxs,
    ContentFlags_t const testContents,
    float& t0,
    float& t1) const {
    if (!(testContents & GetContents())) {
        return false;
    }

    if (Bounds3f(mins, maxs).Contains(start, 0.1f)) {
        return true;
    }
    Intersect_RayBounds(start, dir, mins, maxs, t0, t1);
    return t0 >= 0.0f && t1 >= 0.0f && t1 >= t0;
}

//==============================
// VRMenuObject::IntersectRay
bool VRMenuObject::IntersectRay(
    Vector3f const& localStart,
    Vector3f const& localDir,
    Vector3f const& parentScale,
    Bounds3f const& bounds,
    float& bounds_t0,
    float& bounds_t1,
    ContentFlags_t const testContents,
    OvrCollisionResult& result) const {
    result = OvrCollisionResult();

    // bounds are already computed with scale applied
    if (!IntersectRayBounds(
            localStart,
            localDir,
            bounds.GetMins(),
            bounds.GetMaxs(),
            testContents,
            bounds_t0,
            bounds_t1)) {
        bounds_t0 = FLT_MAX;
        bounds_t1 = FLT_MAX;
        return false;
    }

    // if marked to check only the bounds, then we've hit the object
    if (Flags & VRMENUOBJECT_HIT_ONLY_BOUNDS) {
        result.t = bounds_t0;
        return true;
    }

    // vertices have not had the scale applied yet
    Vector3f const scale = GetLocalScale() * parentScale;

    // test vs. collision primitive
    if (CollisionPrimitive != NULL) {
        CollisionPrimitive->IntersectRay(localStart, localDir, scale, testContents, result);
    }

    // test vs. surfaces
    if (GetType() != VRMENU_CONTAINER && (Flags & VRMENUOBJECT_DONT_RENDER_SURFACE) == 0) {
        int numSurfaces = 0;
        for (const auto& surface : Surfaces) {
            if (surface.IsRenderable()) {
                numSurfaces++;

                OvrCollisionResult localResult;
                if (surface.IntersectRay(localStart, localDir, scale, testContents, localResult)) {
                    if (localResult.t < result.t) {
                        result = localResult;
                    }
                }
            }
        }
    }

    return result.TriIndex >= 0;
}

static void TransformByParentPose(
    Posef const& parentPose,
    Vector3f const& parentScale,
    Posef const& localPose,
    Vector3f const& localScale,
    Posef& outPose,
    Vector3f& outScale) {
    outPose.Translation = parentPose.Translation +
        (parentPose.Rotation * parentScale.EntrywiseMultiply(localPose.Translation));
    outPose.Rotation = parentPose.Rotation * localPose.Rotation;
    outScale = parentScale.EntrywiseMultiply(localScale);
}

//==============================
// VRMenuObject::HitTest_r
bool VRMenuObject::HitTest_r(
    OvrGuiSys const& guiSys,
    Posef const& parentPose,
    Vector3f const& parentScale,
    Vector3f const& rayStart,
    Vector3f const& rayDir,
    ContentFlags_t const testContents,
    HitTestResult& result) const {
    if (Flags & VRMENUOBJECT_DONT_RENDER) {
        return false;
    }

    if (Flags & VRMENUOBJECT_DONT_HIT_ALL) {
        return false;
    }

    // transform ray into local space
    Vector3f scale;
    Posef modelPose;
    TransformByParentPose(parentPose, parentScale, LocalPose, GetLocalScale(), modelPose, scale);

    Vector3f localStart = modelPose.Rotation.Inverted().Rotate(rayStart - modelPose.Translation);
    Vector3f localDir = modelPose.Rotation.Inverted().Rotate(rayDir).Normalized();
    /*
        LOG_WITH_TAG( "Spam", "Hit test vs '%s', start: (%.2f, %.2f, %.2f ) cull bounds( %.2f, %.2f,
       %.2f ) -> ( %.2f, %.2f, %.2f )", GetText().c_str(), localStart.x, localStart.y, localStart.z,
                CullBounds.b[0].x, CullBounds.b[0].y, CullBounds.b[0].z,
                CullBounds.b[1].x, CullBounds.b[1].y, CullBounds.b[1].z );
    */
    // test against cull bounds if we have children  ... otherwise cullBounds == localBounds
    if (Children.size() > 0) {
        if (CullBounds.IsInverted()) {
            ALOG("CullBounds are inverted!!");
            return false;
        }
        float cullT0;
        float cullT1;
        // any contents will hit cull bounds
        ContentFlags_t allContents(OVR::ALL_BITS);
        bool hitCullBounds = IntersectRayBounds(
            localStart,
            localDir,
            CullBounds.GetMins(),
            CullBounds.GetMaxs(),
            allContents,
            cullT0,
            cullT1);

        //        LOG_WITH_TAG( "Spam", "Cull hit = %s, t0 = %.2f t1 = %.2f", hitCullBounds ? "true"
        //        : "false", cullT0, cullT1 );

        if (!hitCullBounds) {
            return false;
        }
    }

    // test against self first, if not a container
    if (GetContents() & testContents) {
        if (Flags & VRMENUOBJECT_BOUND_ALL) {
            // local bounds are the union of surface bounds and text bounds
            Bounds3f localBounds = GetLocalBounds(guiSys.GetDefaultFont()) * parentScale;
            float t0;
            float t1;
            bool hit = IntersectRayBounds(
                localStart,
                localDir,
                localBounds.GetMins(),
                localBounds.GetMaxs(),
                testContents,
                t0,
                t1);
            if (hit) {
                result.HitHandle = Handle;
                result.t = t1;
                result.uv = Vector2f(0.0f); // unknown
            }
        } else {
            float selfT0;
            float selfT1;
            OvrCollisionResult cresult;
            Bounds3f const& localBounds = GetLocalBounds(guiSys.GetDefaultFont()) * parentScale;
            assert(!localBounds.IsInverted());

            bool hit = IntersectRay(
                localStart,
                localDir,
                parentScale,
                localBounds,
                selfT0,
                selfT1,
                testContents,
                cresult);
            if (hit) {
                // app->ShowInfoText( 0.0f, "tri: %i", (int)cresult.TriIndex );
                result = cresult;
                result.HitHandle = Handle;
            }

            // also check vs. the text bounds if there is any text
            if (!Text.empty() && GetType() != VRMENU_CONTAINER &&
                (Flags & VRMENUOBJECT_DONT_HIT_TEXT) == 0) {
                float textT0;
                float textT1;
                Bounds3f bounds = GetTextLocalBounds(guiSys.GetDefaultFont()) * parentScale;
                bool textHit = IntersectRayBounds(
                    localStart,
                    localDir,
                    bounds.GetMins(),
                    bounds.GetMaxs(),
                    testContents,
                    textT0,
                    textT1);
                if (textHit && textT1 < result.t) {
                    result.HitHandle = Handle;
                    result.t = textT1;
                    result.uv = Vector2f(0.0f); // unknown
                }
            }
        }
    }

    // test against children
    for (int i = 0; i < static_cast<int>(Children.size()); ++i) {
        VRMenuObject* child =
            static_cast<VRMenuObject*>(guiSys.GetVRMenuMgr().ToObject(Children[i]));
        if (child != NULL) {
            HitTestResult childResult;
            bool intersected = child->HitTest_r(
                guiSys, modelPose, scale, rayStart, rayDir, testContents, childResult);
            if (intersected && childResult.t < result.t) {
                result = childResult;
            }
        }
    }
    return result.HitHandle.IsValid();
}

//==============================
// VRMenuObject::HitTest
menuHandle_t VRMenuObject::HitTest(
    OvrGuiSys const& guiSys,
    Posef const& worldPose,
    Vector3f const& rayStart,
    Vector3f const& rayDir,
    ContentFlags_t const testContents,
    HitTestResult& result) const {
    HitTest_r(guiSys, worldPose, Vector3f(1.0f), rayStart, rayDir, testContents, result);

    return result.HitHandle;
}

//==============================
// VRMenuObject::GetLocalBounds
Bounds3f VRMenuObject::GetLocalBounds(BitmapFont const& font) const {
    Bounds3f bounds;
    bounds.Clear();
    Vector3f const localScale = GetLocalScale();
    for (const auto& surface : Surfaces) {
        Bounds3f const& surfaceBounds = surface.GetLocalBounds() * localScale;
        bounds = Bounds3f::Union(bounds, surfaceBounds);
    }

    if (CollisionPrimitive != NULL) {
        bounds = Bounds3f::Union(bounds, CollisionPrimitive->GetBounds());
    }

    // transform surface bounds by whatever the hilight pose is
    if (!bounds.IsInverted()) {
        bounds = Bounds3f::Transform(HilightPose, bounds);
    }

    // also union the text bounds, as long as we're not a container (containers don't render
    // anything)
    if (!Text.empty() && GetType() != VRMENU_CONTAINER) {
        bounds = Bounds3f::Union(bounds, GetTextLocalBounds(font));
    }

    // if no valid surface bounds, then the local bounds is the local translation
    if (bounds.IsInverted()) {
        bounds.AddPoint(LocalPose.Translation);
        bounds = Bounds3f::Transform(HilightPose, bounds);
    }

    // after everything is calculated, expand (or contract) the bounds some custom amount
    bounds = Bounds3f::Expand(bounds, MinsBoundsExpand, MaxsBoundsExpand);

    return bounds;
}

//==============================
// VRMenuObject::GetTextLocalBounds
Bounds3f VRMenuObject::GetTextLocalBounds(BitmapFont const& font) const {
    // NOTE: despite being 3 scalars, text scaling only uses the x component since
    // DrawText3D doesn't take separate x and y scales right now.
    Vector3f const localScale = GetLocalScale();
    Vector3f const textLocalScale = GetTextLocalScale();
    float scale = localScale.x * textLocalScale.x * FontParms.Scale * WrapScale;

    if (TextDirty) {
        TextDirty = false;

        // word-wrap the text if wrapping is specified
        if (FontParms.WrapWidth >= 0.0f && FontParms.MultiLine) {
            font.WordWrapText(
                Text,
                FontParms.WrapWidth * localScale.x * textLocalScale.x,
                localScale.x * textLocalScale.x * FontParms.Scale);
        }

        // also union the text bounds
        if (Text.empty()) {
            TextMetrics = textMetrics_t();
        } else {
            size_t len;

            int const MAX_LINES = 16;
            float lineWidths[MAX_LINES];
            int requestedLines = clamp<int>(FontParms.MaxLines, 1, 16);
            int numLines = 0;

            font.CalcTextMetrics(
                Text.c_str(),
                len,
                TextMetrics.w,
                TextMetrics.h,
                TextMetrics.ascent,
                TextMetrics.descent,
                TextMetrics.fontHeight,
                lineWidths,
                requestedLines,
                numLines);

            // for the time being if we exceed the number of lines we truncate the last few lines
            // and add a ... to indicate more text is there we'll do this until we support scrolling
            if (numLines > requestedLines) {
                font.TruncateText(Text, requestedLines);
            }

            if (FontParms.WrapWidth >= 0.0f &&
                TextMetrics.w * FontParms.Scale > FontParms.WrapWidth) {
                WrapScale = FontParms.WrapWidth / (TextMetrics.w * FontParms.Scale);
                scale = localScale.x * textLocalScale.x * FontParms.Scale * WrapScale;
            } else {
                WrapScale = 1.0f; // reset scale back to 1.0f in case it was shrunk down previously
            }
        }

        if (Flags & VRMenuObjectFlags_t(VRMENUOBJECT_INSTANCE_TEXT)) {
            FreeTextSurface();
            TextSurface = new ovrTextSurface();
            fontParms_t fp;
            fp.AlignHoriz = FontParms.AlignHoriz;
            fp.AlignVert = FontParms.AlignVert;
            fp.TrackRoll = FontParms.TrackRoll;
            fp.ColorCenter = FontParms.ColorCenter;
            fp.AlphaCenter = FontParms.AlphaCenter;
            TextSurface->SurfaceDef = font.TextSurface(
                Text.c_str(), scale, TextColor, FontParms.AlignHoriz, FontParms.AlignVert, &fp);
        }
    }

    // this seems overly complex because font characters are rendered so that their origin
    // is on their baseline and not on one of the corners of the glyph. Because of this
    // we must treat the initial ascent (amount the font goes above the first baseline) and
    // final descent (amount the font goes below the final baseline) independently from the
    // lines in between when centering.
    Bounds3f textBounds(
        Vector3f(0.0f, (TextMetrics.h - TextMetrics.ascent) * -1.0f, 0.0f) * scale,
        Vector3f(TextMetrics.w, TextMetrics.ascent, 0.0f) * scale);

    Vector3f trans = Vector3f::ZERO;
    switch (FontParms.AlignVert) {
        case VERTICAL_BASELINE:
            trans.y = 0.0f;
            break;

        case VERTICAL_CENTER: {
            trans.y = (TextMetrics.h * 0.5f) - TextMetrics.ascent;
            break;
        }

        case VERTICAL_CENTER_FIXEDHEIGHT: {
            trans.y = (TextMetrics.fontHeight * -0.5f);
            break;
        }

        case VERTICAL_TOP: {
            trans.y = TextMetrics.h - TextMetrics.ascent;
            break;
        }
    }

    switch (FontParms.AlignHoriz) {
        case HORIZONTAL_LEFT:
            trans.x = 0.0f;
            break;

        case HORIZONTAL_CENTER: {
            trans.x = TextMetrics.w * -0.5f;
            break;
        }
        case HORIZONTAL_RIGHT: {
            trans.x = -TextMetrics.w;
            break;
        }
    }

    textBounds.Translate(trans * scale);

    Bounds3f textLocalBounds = Bounds3f::Transform(GetTextLocalPose(), textBounds);
    // transform by hilightpose here since surfaces are transformed by it before unioning the bounds
    textLocalBounds = Bounds3f::Transform(HilightPose, textLocalBounds);

    return textLocalBounds;
}

//==============================
// VRMenuObject::CalcLocalBoundsForText
Bounds3f VRMenuObject::CalcLocalBoundsForText(BitmapFont const& font, std::string& text) const {
    if (text.empty()) {
        return Bounds3f();
    }

    // NOTE: despite being 3 scalars, text scaling only uses the x component since
    // DrawText3D doesn't take separate x and y scales right now.
    Vector3f const localScale = GetLocalScale();
    Vector3f const textLocalScale = GetTextLocalScale();

    float localWrapScale = 1.0f;
    float scale = localScale.x * textLocalScale.x * FontParms.Scale * localWrapScale;

    size_t len;
    int const MAX_LINES = 16;
    float lineWidths[MAX_LINES];
    int numLines = 0;
    int requestedLines = clamp<int>(FontParms.MaxLines, 1, 16);

    float w;
    float h;
    float ascent;
    float descent;
    float fontHeight;

    // word-wrap the text if wrapping is specified
    if (FontParms.WrapWidth >= 0.0f && FontParms.MultiLine) {
        font.WordWrapText(
            text,
            FontParms.WrapWidth * localScale.x * textLocalScale.x,
            localScale.x * textLocalScale.x * FontParms.Scale);
    }

    font.CalcTextMetrics(
        text.c_str(), len, w, h, ascent, descent, fontHeight, lineWidths, requestedLines, numLines);

    // for the time being if we exceed the number of lines we truncate the last few lines and add a
    // ... to indicate more text is there we'll do this until we support scrolling
    if (numLines > requestedLines) {
        font.TruncateText(text, requestedLines);
    }

    if (FontParms.WrapWidth >= 0.0f && w * FontParms.Scale > FontParms.WrapWidth) {
        localWrapScale = FontParms.WrapWidth / (w * FontParms.Scale);
        scale = localScale.x * textLocalScale.x * FontParms.Scale * localWrapScale;
    }

    // this seems overly complex because font characters are rendered so that their origin
    // is on their baseline and not on one of the corners of the glyph. Because of this
    // we must treat the initial ascent (amount the font goes above the first baseline) and
    // final descent (amount the font goes below the final baseline) independently from the
    // lines in between when centering.
    Bounds3f textBounds(
        Vector3f(0.0f, (h - ascent) * -1.0f, 0.0f) * scale, Vector3f(w, ascent, 0.0f) * scale);

    Vector3f trans = Vector3f::ZERO;
    switch (FontParms.AlignVert) {
        case VERTICAL_BASELINE:
            trans.y = 0.0f;
            break;

        case VERTICAL_CENTER: {
            trans.y = (TextMetrics.h * 0.5f) - TextMetrics.ascent;
            break;
        }

        case VERTICAL_CENTER_FIXEDHEIGHT: {
            trans.y = (TextMetrics.fontHeight * -0.5f);
            break;
        }

        case VERTICAL_TOP: {
            trans.y = TextMetrics.h - TextMetrics.ascent;
            break;
        }
    }

    switch (FontParms.AlignHoriz) {
        case HORIZONTAL_LEFT:
            trans.x = 0.0f;
            break;

        case HORIZONTAL_CENTER: {
            trans.x = TextMetrics.w * -0.5f;
            break;
        }
        case HORIZONTAL_RIGHT: {
            trans.x = -TextMetrics.w;
            break;
        }
    }

    textBounds.Translate(trans * scale);

    Bounds3f textLocalBounds = Bounds3f::Transform(GetTextLocalPose(), textBounds);
    // transform by hilightpose here since surfaces are transformed by it before unioning the bounds
    textLocalBounds = Bounds3f::Transform(HilightPose, textLocalBounds);

    return textLocalBounds;
}

//==============================
// VRMenuObject::AddComponent
void VRMenuObject::AddComponent(VRMenuComponent* component) {
    if (component == NULL) {
        return; // this is fine... makes submitting VRMenuComponentParms easier.
    }

    int componentIndex = GetComponentIndex(component);
    if (componentIndex >= 0) {
        // cannot add the same component twice!
        /// assert_WITH_TAG( componentIndex < 0, "VRMenu" );
        return;
    }
    Components.push_back(component);
}

//==============================
// VRMenuObject::DeleteComponent
void VRMenuObject::DeleteComponent(OvrGuiSys& guiSys, VRMenuComponent* component) {
#if defined(OVR_BUILD_DEBUG)
    int componentIndex = GetComponentIndex(component);
    assert(componentIndex >= 0);
#endif
    // stop the component from getting any events between now and the time it's freed.
    component->ClearEventFlags();

    // add the object and the component to a pending deletion list
    guiSys.GetVRMenuMgr().AddComponentToDeletionList(GetHandle(), component);
}

//==============================
// VRMenuObject::FreeComponents
void VRMenuObject::FreeComponents(ovrComponentList& list) {
    std::vector<VRMenuComponent*>& comps = list.GetComponents();
    for (int i = static_cast<int>(comps.size()) - 1; i >= 0; --i) {
        VRMenuComponent* component = comps[i];
        comps[i] = nullptr;

        for (int j = 0; j < static_cast<int>(Components.size()); ++j) {
            if (Components[j] == component) {
                Components.erase(Components.cbegin() + j);
                break;
            }
        }

        delete component;
    }
}

//==============================
// VRMenuObject::GetComponentIndex
int VRMenuObject::GetComponentIndex(VRMenuComponent* component) const {
    for (int i = 0; i < static_cast<int>(Components.size()); ++i) {
        if (Components[i] == component) {
            return i;
        }
    }
    return -1;
}

//==============================
// VRMenuObject::GetComponentById
VRMenuComponent* VRMenuObject::GetComponentById_Impl(int const id, const char* name) const {
    std::vector<VRMenuComponent*> const& comps = GetComponentList();
    for (int c = 0; c < static_cast<int>(comps.size()); ++c) {
        if (VRMenuComponent* comp = comps[c]) {
            if (comp->GetTypeId() == id) {
                if (name == NULL || !OVR::OVR_strcmp(comp->GetName(), name)) {
                    return comp;
                }
            }
        } else {
            assert(comp);
        }
    }

    return NULL;
}

//==============================
// VRMenuObject::GetComponentByTypeName
VRMenuComponent* VRMenuObject::GetComponentByTypeName_Impl(const char* typeName) const {
    std::vector<VRMenuComponent*> const& comps = GetComponentList();
    for (int c = 0; c < static_cast<int>(comps.size()); ++c) {
        if (VRMenuComponent* comp = comps[c]) {
            if (comp->GetTypeName() == typeName) {
                return comp;
            }
        } else {
            assert(comp);
        }
    }

    return NULL;
}

//==============================
// VRMenuObject::GetColorTableOffset
Vector2f const& VRMenuObject::GetColorTableOffset() const {
    return ColorTableOffset;
}

//==============================
// VRMenuObject::SetColorTableOffset
void VRMenuObject::SetColorTableOffset(Vector2f const& ofs) {
    ColorTableOffset = ofs;
}

//==============================
// VRMenuObject::GetColor
Vector4f const& VRMenuObject::GetColor() const {
    return Color;
}

//==============================
// VRMenuObject::SetColor
void VRMenuObject::SetColor(Vector4f const& c) {
    Color = c;
}

void VRMenuObject::SetVisible(bool visible) {
    if (visible) {
        Flags &= ~VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER);
    } else {
        Flags |= VRMenuObjectFlags_t(VRMENUOBJECT_DONT_RENDER);
    }
}

//==============================
// VRMenuObject::ChildForId
VRMenuObject* VRMenuObject::ChildForId(OvrVRMenuMgr const& menuMgr, VRMenuId_t const id) const {
    int n = NumChildren();
    for (int i = 0; i < n; ++i) {
        VRMenuObject const* child =
            static_cast<VRMenuObject*>(menuMgr.ToObject(GetChildHandleForIndex(i)));
        if (child != nullptr) {
            if (child->GetId() == id) {
                return const_cast<VRMenuObject*>(child);
            } else {
                VRMenuObject* c = child->ChildForId(menuMgr, id);
                if (c != nullptr) {
                    return c;
                }
            }
        }
    }
    return nullptr;
}

//==============================
// VRMenuObject::ChildHandleForId
menuHandle_t VRMenuObject::ChildHandleForId(OvrVRMenuMgr const& menuMgr, VRMenuId_t const id)
    const {
    int n = NumChildren();
    for (int i = 0; i < n; ++i) {
        menuHandle_t childHandle = GetChildHandleForIndex(i);
        VRMenuObject const* child = static_cast<VRMenuObject*>(menuMgr.ToObject(childHandle));
        if (child != nullptr) {
            if (child->GetId() == id) {
                return childHandle;
            } else {
                menuHandle_t handle = child->ChildHandleForId(menuMgr, id);
                if (handle.IsValid()) {
                    return handle;
                }
            }
        }
    }
    return menuHandle_t();
}

//==============================
// VRMenuObject::ChildHandleForName
menuHandle_t VRMenuObject::ChildHandleForName(OvrVRMenuMgr const& menuMgr, char const* name) const {
    if (name == NULL || name[0] == '\0') {
        return menuHandle_t();
    }

    int n = NumChildren();
    for (int i = 0; i < n; ++i) {
        VRMenuObject const* child =
            static_cast<VRMenuObject*>(menuMgr.ToObject(GetChildHandleForIndex(i)));
        if (child != NULL) {
            if (OVR::OVR_stricmp(child->GetName().c_str(), name) == 0) {
                return child->GetHandle();
            } else {
                menuHandle_t handle = child->ChildHandleForName(menuMgr, name);
                if (handle.IsValid()) {
                    return handle;
                }
            }
        }
    }
    return menuHandle_t();
}

//==============================
// VRMenuObject::ChildHandleForTag
menuHandle_t VRMenuObject::ChildHandleForTag(OvrVRMenuMgr const& menuMgr, char const* tag) const {
    if (tag == NULL || tag[0] == '\0') {
        return menuHandle_t();
    }

    int n = NumChildren();
    for (int i = 0; i < n; ++i) {
        VRMenuObject const* child =
            static_cast<VRMenuObject*>(menuMgr.ToObject(GetChildHandleForIndex(i)));
        if (child != NULL) {
            if (OVR::OVR_stricmp(child->GetTag().c_str(), tag) == 0) {
                return child->GetHandle();
            } else {
                menuHandle_t handle = child->ChildHandleForTag(menuMgr, tag);
                if (handle.IsValid()) {
                    return handle;
                }
            }
        }
    }
    return menuHandle_t();
}

//==============================
// VRMenuObject::GetLocalScale
Vector3f VRMenuObject::GetLocalScale() const {
    return Vector3f(
        LocalScale.x * HilightScale, LocalScale.y * HilightScale, LocalScale.z * HilightScale);
}

//==============================
// VRMenuObject::GetTextLocalScale
Vector3f VRMenuObject::GetTextLocalScale() const {
    return Vector3f(
        TextLocalScale.x * HilightScale,
        TextLocalScale.y * HilightScale,
        TextLocalScale.z * HilightScale);
}

//==============================
// VRMenuObject::SetSurfaceTexture
void VRMenuObject::SetSurfaceTexture(
    OvrGuiSys& guiSys,
    int const surfaceIndex,
    int const textureIndex,
    eSurfaceTextureType const type,
    char const* imageName) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture(guiSys, textureIndex, type, imageName);
}

//==============================
// VRMenuObject::SetSurfaceTexture
void VRMenuObject::SetSurfaceTexture(
    int const surfaceIndex,
    int const textureIndex,
    eSurfaceTextureType const type,
    GLuint const texId,
    int const width,
    int const height) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture(textureIndex, type, texId, width, height);
}

//==============================
// VRMenuObject::SetSurfaceTexture
void VRMenuObject::SetSurfaceTexture(
    int const surfaceIndex,
    int const textureIndex,
    eSurfaceTextureType const type,
    GlTexture const& texture) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture(
        textureIndex, type, texture.texture, texture.Width, texture.Height);
}

//==============================
// VRMenuObject::SetSurfaceTexture
void VRMenuObject::SetSurfaceTextureTakeOwnership(
    int const surfaceIndex,
    int const textureIndex,
    eSurfaceTextureType const type,
    GLuint const texId,
    int const width,
    int const height) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture(textureIndex, type, texId, width, height);
    Surfaces[surfaceIndex].SetOwnership(textureIndex, true);
}

//==============================
// VRMenuObject::SetSurfaceTextureTakeOwnership
void VRMenuObject::SetSurfaceTextureTakeOwnership(
    int const surfaceIndex,
    int const textureIndex,
    eSurfaceTextureType const type,
    GlTexture const& texture) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture(
        textureIndex, type, texture.texture, texture.Width, texture.Height);
    Surfaces[surfaceIndex].SetOwnership(textureIndex, true);
}

//==============================
// VRMenuObject::RegenerateSurfaceGeometry
void VRMenuObject::RegenerateSurfaceGeometry(
    int const surfaceIndex,
    const bool freeSurfaceGeometry) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }

    if (freeSurfaceGeometry) {
        Surfaces[surfaceIndex].Free();
    }

    Surfaces[surfaceIndex].RegenerateSurfaceGeometry();
}

//==============================
// VRMenuObject::GetSurfaceDims
Vector2f const& VRMenuObject::GetSurfaceDims(int const surfaceIndex) const {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return Vector2f::ZERO;
    }

    return Surfaces[surfaceIndex].GetDims();
}

//==============================
// VRMenuObject::SetSurfaceDims
void VRMenuObject::SetSurfaceDims(int const surfaceIndex, Vector2f const& dims) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }

    Surfaces[surfaceIndex].SetDims(dims);
}

//==============================
// VRMenuObject::GetSurfaceBorder
Vector4f const& VRMenuObject::GetSurfaceBorder(int const surfaceIndex) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return Vector4f::ZERO;
    }

    return Surfaces[surfaceIndex].GetBorder();
}

//==============================
// VRMenuObject::SetSurfaceBorder
void VRMenuObject::SetSurfaceBorder(int const surfaceIndex, Vector4f const& border) {
    if (surfaceIndex < 0 || surfaceIndex >= static_cast<int>(Surfaces.size())) {
        /// assert_WITH_TAG( surfaceIndex >= 0 && surfaceIndex < static_cast< int >( Surfaces.size()
        /// ), "VrMenu" );
        return;
    }

    Surfaces[surfaceIndex].SetBorder(border);
}

//==============================
// VRMenuObject::SetLocalBoundsExpand
void VRMenuObject::SetLocalBoundsExpand(Vector3f const mins, Vector3f const& maxs) {
    MinsBoundsExpand = mins;
    MaxsBoundsExpand = maxs;
}

//==============================
// VRMenuObject::SetCollisionPrimitive
void VRMenuObject::SetCollisionPrimitive(OvrCollisionPrimitive* c) {
    if (CollisionPrimitive != NULL) {
        delete CollisionPrimitive;
    }
    CollisionPrimitive = c;
}

//==============================
//  VRMenuObject::FindSurfaceWithTextureType
int VRMenuObject::FindSurfaceWithTextureType(eSurfaceTextureType const type, bool const singular)
    const {
    return FindSurfaceWithTextureType(type, singular, false);
}

//==============================
//  VRMenuObject::FindSurfaceWithTextureType
int VRMenuObject::FindSurfaceWithTextureType(
    eSurfaceTextureType const type,
    bool const singular,
    bool const visibleOnly) const {
    for (int i = 0; i < static_cast<int>(Surfaces.size()); ++i) {
        VRMenuSurface const& surf = Surfaces[i];
        if (visibleOnly && !surf.GetVisible()) {
            continue;
        }

        int numTextures = 0;
        bool hasType = false;
        // we have to look through the surface images because we don't know how many are valid
        for (int j = 0; j < VRMENUSURFACE_IMAGE_MAX; j++) {
            VRMenuSurfaceTexture const& texture = surf.GetTexture(j);
            if (texture.GetType() != SURFACE_TEXTURE_MAX) {
                numTextures++;
            }
            if (texture.GetType() == type) {
                hasType = true;
            }
        }
        if (hasType) {
            if (!singular || (singular && numTextures == 1)) {
                return i;
            }
        }
    }
    return -1;
}

//==============================
// VRMenuObject::SetSurfaceColor
void VRMenuObject::SetSurfaceColor(int const surfaceIndex, Vector4f const& color) {
    VRMenuSurface& surf = Surfaces[surfaceIndex];
    surf.SetColor(color);
}

//==============================
// VRMenuObject::GetSurfaceColor
Vector4f const& VRMenuObject::GetSurfaceColor(int const surfaceIndex) const {
    VRMenuSurface const& surf = Surfaces[surfaceIndex];
    return surf.GetColor();
}

//==============================
// VRMenuObject::SetSurfaceVisible
void VRMenuObject::SetSurfaceVisible(int const surfaceIndex, bool const v) {
    VRMenuSurface& surf = Surfaces[surfaceIndex];
    surf.SetVisible(v);
}

//==============================
// VRMenuObject::GetSurfaceVisible
bool VRMenuObject::GetSurfaceVisible(int const surfaceIndex) const {
    VRMenuSurface const& surf = Surfaces[surfaceIndex];
    return surf.GetVisible();
}

//==============================
// VRMenuObject::NumSurfaces
int VRMenuObject::NumSurfaces() const {
    return static_cast<int>(Surfaces.size());
}

//==============================
// VRMenuObject::AllocSurface
int VRMenuObject::AllocSurface() {
    int newIndex = static_cast<int>(Surfaces.size());
    Surfaces.emplace_back(VRMenuSurface());
    return newIndex;
}

//==============================
// VRMenuObject::CreateFromSurfaceParms
void VRMenuObject::CreateFromSurfaceParms(
    OvrGuiSys& guiSys,
    int const surfaceIndex,
    VRMenuSurfaceParms const& parms) {
    VRMenuSurface& surf = Surfaces[surfaceIndex];
    surf.CreateFromSurfaceParms(guiSys, parms);
}

//==============================
// VRMenuObject::SetText
void VRMenuObject::SetText(char const* text) {
    Text = text;
    TextDirty = true;
}

//==============================
// VRMenuObject::SetTextWordWrapped
void VRMenuObject::SetTextWordWrapped(
    char const* text,
    BitmapFont const& font,
    float const widthInMeters) {
    FontParms.WrapWidth = widthInMeters;
    SetText(text);
    font.WordWrapText(Text, widthInMeters, FontParms.Scale);
}

//==============================
// VRMenuObject::TransformByParent
void VRMenuObject::TransformByParent(
    Posef const& parentPose,
    Vector3f const& parentScale,
    Vector4f const& parentColor,
    Posef const& localPose,
    Vector3f const& localScale,
    Vector4f const& localColor,
    VRMenuObjectFlags_t const& objectFlags,
    Posef& outPose,
    Vector3f& outScale,
    Vector4f& outColor) {
    TransformByParentPose(parentPose, parentScale, localPose, localScale, outPose, outScale);
    outColor = (objectFlags & VRMENUOBJECT_DONT_MOD_PARENT_COLOR) ? localColor * parentColor.w
                                                                  : parentColor * localColor;
}

//==============================
// VRMenuObject::GetWorldTransform
void VRMenuObject::GetWorldTransform(
    OvrVRMenuMgr& menuMgr,
    Posef const& menuPose,
    Posef& outPose,
    Vector3f& outScale,
    Vector4f& outColor) const {
    std::vector<VRMenuObject const*> objects;
    objects.push_back(this);

    // collect all of the parents
    menuHandle_t parentHandle = ParentHandle;
    while (parentHandle.IsValid()) {
        VRMenuObject* obj = menuMgr.ToObject(parentHandle);
        objects.push_back(obj);
        parentHandle = obj->GetParentHandle();
    }

    // now go through them in parent -> child order to compute the transformation

    outPose = menuPose; // start as identity
    outScale = Vector3f(1.0f);
    outColor = Vector4f(1.0f);
    for (int i = static_cast<int>(objects.size()) - 1; i >= 0; --i) {
        VRMenuObject const* obj = objects[i];
        TransformByParent(
            outPose,
            outScale,
            outColor,
            obj->GetLocalPose(),
            obj->GetLocalScale(),
            obj->GetColor(),
            obj->GetFlags(),
            outPose,
            outScale,
            outColor);
    }
}

//==============================
// VRMenuObject::Recurse
void VRMenuObject::Recurse(
    OvrVRMenuMgr const& menuMgr,
    ovrRecursionFunctor& functor,
    VRMenuObject* obj) {
    functor.AtNode(obj);

    for (int i = 0; i < obj->NumChildren(); ++i) {
        VRMenuObject* child = menuMgr.ToObject(obj->GetChildHandleForIndex(i));
        if (child != nullptr) {
            child->Recurse(menuMgr, functor, child);
        }
    }
}

//==============================
// VRMenuObject::FreeTextSurface
void VRMenuObject::FreeTextSurface() const {
    if (TextSurface != nullptr) {
        TextSurface->SurfaceDef.geo.Free();
        delete TextSurface;
        TextSurface = nullptr;
    }
}

static void DumpText(char const* token, char const* text, int const size) {
    // DUMP THE ENITER BUFFER TO THE LOG TO CATCH MEMORY CORRUPTION
    OVR_LOG("VRGUI PARSE ERROR!");
    OVR_LOG("lex token: %s", token);
    OVR_LOG("lex buffer:");
    const int BLOCK_SIZE = 256;
    int blocks = size / BLOCK_SIZE;
    char const* bufferStart = text;
    for (int i = 0; i <= blocks; i++) {
        char const* cur = bufferStart + i * BLOCK_SIZE;
        int count = (i < blocks) ? BLOCK_SIZE : (size - (blocks * BLOCK_SIZE));
        char temp[BLOCK_SIZE + 1];
        memcpy(temp, cur, count);
        temp[count] = '\0';
        OVR_LOG("block %i: %s", i, temp);
    }
}

//==============================
// VRMenuObject::ParseItemParms
ovrParseResult VRMenuObject::ParseItemParms(
    ovrReflection& refl,
    ovrLocale const& locale,
    char const* fileName,
    std::vector<uint8_t> const& buffer,
    std::vector<VRMenuObjectParms const*>& itemParms) {
    ovrLexer lex(buffer, ":;|[],()/*\\#");

    char token[128];
    ovrLexer::ovrResult res = lex.NextToken(token, sizeof(token));
    if (res == ovrLexer::LEX_RESULT_EOF) {
        return ovrParseResult(res, "Error parsing reflection file '%s'.", fileName);
    }

    do {
        if (token[0] == '#') {
            // preprocessor token
            res = lex.NextToken(token, sizeof(token));
            if (res != ovrLexer::LEX_RESULT_OK) {
                DumpText(
                    token,
                    (const char*)(static_cast<uint8_t const*>(buffer.data())),
                    (int)buffer.size());
                return ovrParseResult(res, "Expected key word after #.");
            }
            if (OVR::OVR_strcmp(token, "pragma") == 0) {
                res = lex.NextToken(token, sizeof(token));
                if (res != ovrLexer::LEX_RESULT_OK) {
                    return ovrParseResult(res, "Expected pragma type.");
                }
                if (OVR::OVR_strcmp(token, "overload_float_default_value") == 0) {
                    res = lex.ExpectPunctuation("(", token, sizeof(token));
                    if (res != ovrLexer::LEX_RESULT_OK) {
                        return ovrParseResult(res, "Expected '('.");
                    }

                    // parse overload parameters
                    std::string scope;
                    std::string name;
                    for (;;) {
                        char nameToken[128];
                        res = lex.NextToken(nameToken, sizeof(nameToken));
                        if (res != ovrLexer::LEX_RESULT_OK) {
                            return ovrParseResult(res, "Expected identifier name.");
                        }
                        char puncToken[16];
                        res = lex.NextToken(puncToken, sizeof(puncToken));
                        if (res != ovrLexer::LEX_RESULT_OK) {
                            return ovrParseResult(res, "Expected ':'");
                        }
                        if (puncToken[0] == ',') {
                            name = nameToken;
                            break;
                        }
                        if (puncToken[0] != ':') {
                            return ovrParseResult(res, "Expected ':'");
                        }
                        res = lex.ExpectPunctuation(":", puncToken, sizeof(puncToken));
                        if (res != ovrLexer::LEX_RESULT_OK) {
                            return ovrParseResult(res, "Expected ':'");
                        }
                        if (!scope.empty()) {
                            scope += "::";
                        }
                        scope += nameToken;
                    }

                    float value;
                    res = lex.ParseFloat(value, 0.0f);
                    if (res != ovrLexer::LEX_RESULT_OK) {
                        return ovrParseResult(res, "Expected float value.");
                    }

                    res = lex.ExpectPunctuation(")", token, sizeof(token));
                    if (res != ovrLexer::LEX_RESULT_OK) {
                        return ovrParseResult(res, "Expected ')'.");
                    }

                    refl.AddOverload(new ovrReflectionOverload_FloatDefaultValue(
                        scope.c_str(), name.c_str(), value));
                }
            } else {
                // unknown pragmas are errors for now
                return ovrParseResult(res, "Unknown pragma %s", token);
            }
        } else if (OVR::OVR_strcmp(token, "itemParms") == 0) {
            std::vector<VRMenuObjectParms const*> parms;
            ovrTypeInfo const* typeInfo = refl.FindTypeInfo("std::vector< VRMenuObjectParms* >");
            if (typeInfo != nullptr) {
                ovrParseResult parseRes =
                    ParseArray(refl, locale, fileName, lex, typeInfo, &parms, 0);
                if (!parseRes) {
                    DeletePointerArray(parms);
                    return parseRes;
                }
            }

            itemParms.insert(itemParms.cend(), parms.cbegin(), parms.cend());
            parms.resize(0);

        } else {
            return ovrParseResult(res, "Unknown token '%s'", token);
        }

        res = lex.NextToken(token, sizeof(token));
        if (res == ovrLexer::LEX_RESULT_EOF) {
            break;
        } else if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(res, "Error parsing reflection file '%s'.", fileName);
        }
    } while (res == ovrLexer::LEX_RESULT_OK);

    return ovrParseResult();
}

} // namespace OVRFW
