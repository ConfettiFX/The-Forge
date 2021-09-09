/************************************************************************************

Filename    :   VRMenuObject.h
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#pragma once

#include <vector>
#include <string>

#include "OVR_BitFlags.h"
#include "OVR_TypesafeNumber.h"
#include "OVR_Math.h"

#include "Render/Egl.h" // GLuint
#include "Render/BitmapFont.h" // HorizontalJustification & VerticalJustification
#include "Misc/Log.h"

#include "CollisionPrimitive.h"

#include "OVR_Lexer2.h" // ovrLexer

namespace OVRFW {

class App;
class OvrVRMenuMgr;
class OvrGuiSys;
class ovrReflection;
class ovrLocale;
class ovrParseResult;

//==============================
// DeletePointerArray
// helper function for cleaning up an array of pointers
template <typename T>
void DeletePointerArray(std::vector<T*>& a) {
    for (T* item : a) {
        delete item;
    }
    a.clear();
}

enum eGUIProgramType {
    PROGRAM_DIFFUSE_ONLY, // has a diffuse only
    PROGRAM_DIFFUSE_ALPHA_DISCARD, // discard 0-alpha fragments
    PROGRAM_ADDITIVE_ONLY,
    PROGRAM_DIFFUSE_PLUS_ADDITIVE, // has a diffuse and an additive
    PROGRAM_DIFFUSE_COLOR_RAMP, // has a diffuse and color ramp, and color ramp target is the
                                // diffuse
    PROGRAM_DIFFUSE_COLOR_RAMP_TARGET, // has diffuse, color ramp, and a separate color ramp target
    PROGRAM_DIFFUSE_COMPOSITE, // has two diffuse, with the second composited onto the first using
                               // it's alpha mask
    PROGRAM_ALPHA_DIFFUSE, // first texture is the alpha map, second texture is the d
    PROGRAM_MAX // some other combo not supported, or no texture maps at all
};

enum eVRMenuObjectType {
    VRMENU_CONTAINER, // a container for other controls
    VRMENU_STATIC, // non-interactable item, may have text
    VRMENU_BUTTON, // "clickable"
    VRMENU_MAX // invalid
};

enum eVRMenuSurfaceImage {
    VRMENUSURFACE_IMAGE_0,
    VRMENUSURFACE_IMAGE_1,
    VRMENUSURFACE_IMAGE_2,
    VRMENUSURFACE_IMAGE_MAX
};

enum eSurfaceTextureType {
    SURFACE_TEXTURE_DIFFUSE,
    SURFACE_TEXTURE_DIFFUSE_ALPHA_DISCARD, // discard 0-alpha fragments
    SURFACE_TEXTURE_ADDITIVE,
    SURFACE_TEXTURE_COLOR_RAMP,
    SURFACE_TEXTURE_COLOR_RAMP_TARGET,
    SURFACE_TEXTURE_ALPHA_MASK,
    SURFACE_TEXTURE_MAX // also use to indicate an uninitialized type
};

enum eVRMenuObjectFlags {
    VRMENUOBJECT_FLAG_NO_FOCUS_GAINED, // object will never get focus gained event
    VRMENUOBJECT_DONT_HIT_ALL, // do not collide with object in hit testing
    VRMENUOBJECT_DONT_HIT_TEXT, // do not collide with text in hit testing
    VRMENUOBJECT_HIT_ONLY_BOUNDS, // only collide against the bounds, not the collision surface
    VRMENUOBJECT_BOUND_ALL, // for hit testing, merge all bounds into a single AABB
    VRMENUOBJECT_FLAG_POLYGON_OFFSET, // render with polygon offset
    VRMENUOBJECT_FLAG_NO_DEPTH, // render without depth test
    VRMENUOBJECT_FLAG_NO_DEPTH_MASK, // render without writing to the z buffer
    VRMENUOBJECT_DONT_RENDER, // don't render this object nor its children
    VRMENUOBJECT_DONT_RENDER_SURFACE, // don't render this object's non text surfaces
    VRMENUOBJECT_DONT_RENDER_TEXT, // don't render this object's text (useful for naming an object
                                   // just for debugging)
    VRMENUOBJECT_NO_GAZE_HILIGHT, // when hit this object won't change the cursor hilight state
    VRMENUOBJECT_RENDER_HIERARCHY_ORDER, // this object and all its children will use this object's
                                         // position for depth sorting, and sort vs. each other by
                                         // submission index (i.e. parent's render first)
    VRMENUOBJECT_FLAG_BILLBOARD, // always face view plane normal
    VRMENUOBJECT_DONT_MOD_PARENT_COLOR, // don't apply the parent's color (but do apply alpha)
    VRMENUOBJECT_INSTANCE_TEXT, // text on this object will be rendered to its own surface for
                                // sorting (this has performance penalty so only use if if you have
                                // sorting issues with text).

    VRMENUOBJECT_MAX // not an actual flag, just used for counting the number of enums
};

typedef OVR::BitFlagsT<eVRMenuObjectFlags> VRMenuObjectFlags_t;

enum eVRMenuObjectInitFlags {
    VRMENUOBJECT_INIT_ALIGN_TO_VIEW, // align to the camera position on init
    VRMENUOBJECT_INIT_FORCE_POSITION // use the position in the parms instead of an auto-calculated
                                     // position
};

typedef OVR::BitFlagsT<eVRMenuObjectInitFlags> VRMenuObjectInitFlags_t;

enum eVRMenuId { INVALID_MENU_ID = INT_MIN };
typedef OVR::TypesafeNumberT<long long, eVRMenuId, INVALID_MENU_ID> VRMenuId_t;

// menu object handles
enum eMenuIdType { INVALID_MENU_OBJECT_ID = 0 };
typedef OVR::TypesafeNumberT<uint64_t, eMenuIdType, INVALID_MENU_OBJECT_ID> menuHandle_t;

// menu render flags
enum eVRMenuRenderFlags {
    VRMENU_RENDER_NO_DEPTH,
    VRMENU_RENDER_NO_FONT_OUTLINE,
    VRMENU_RENDER_POLYGON_OFFSET,
    VRMENU_RENDER_BILLBOARD,
    VRMENU_RENDER_NO_DEPTH_MASK,
    VRMENU_RENDER_SUBMIT_TEXT_SURFACE
};
typedef OVR::BitFlagsT<eVRMenuRenderFlags> VRMenuRenderFlags_t;

class VRMenuComponent;
class VRMenuComponent_OnFocusGained;
class VRMenuComponent_OnFocusLost;
class VRMenuComponent_OnDown;
class VRMenuComponent_OnUp;
class VRMenuComponent_OnSubmitForRendering;
class VRMenuComponent_OnRender;
class VRMenuComponent_OnTouchRelative;
class BitmapFont;
struct fontParms_t;

// border indices
enum eVRMenuBorder { BORDER_LEFT, BORDER_BOTTOM, BORDER_RIGHT, BORDER_TOP };

//==============================================================
// VRMenuSurfaceParms
class VRMenuSurfaceParms {
   public:
    VRMenuSurfaceParms()
        : SurfaceName(""),
          ImageNames(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(0.5f, 0.5f),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f) {
        InitSurfaceProperties();
    }

    explicit VRMenuSurfaceParms(char const* surfaceName)
        : SurfaceName(surfaceName != nullptr ? surfaceName : ""),
          ImageNames(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(0.5f, 0.5f),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f)

    {
        InitSurfaceProperties();
    }
    VRMenuSurfaceParms(
        char const* surfaceName,
        char const* imageName0,
        eSurfaceTextureType textureType0,
        char const* imageName1,
        eSurfaceTextureType textureType1,
        char const* imageName2,
        eSurfaceTextureType textureType2)
        : SurfaceName(surfaceName != nullptr ? surfaceName : ""),
          ImageNames(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(0.5f, 0.5f),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f)

    {
        InitSurfaceProperties();
        ImageNames[VRMENUSURFACE_IMAGE_0] = imageName0 != nullptr ? imageName0 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;

        ImageNames[VRMENUSURFACE_IMAGE_1] = imageName1 != nullptr ? imageName1 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;

        ImageNames[VRMENUSURFACE_IMAGE_2] = imageName2 != nullptr ? imageName2 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;
    }
    VRMenuSurfaceParms(
        char const* surfaceName,
        char const* imageName0,
        eSurfaceTextureType textureType0,
        char const* imageName1,
        eSurfaceTextureType textureType1,
        char const* imageName2,
        eSurfaceTextureType textureType2,
        OVR::Vector2f const& anchors)
        : SurfaceName(surfaceName != nullptr ? surfaceName : ""),
          ImageNames(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(anchors),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f)

    {
        InitSurfaceProperties();
        ImageNames[VRMENUSURFACE_IMAGE_0] = imageName0 != nullptr ? imageName0 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;

        ImageNames[VRMENUSURFACE_IMAGE_1] = imageName1 != nullptr ? imageName1 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;

        ImageNames[VRMENUSURFACE_IMAGE_2] = imageName2 != nullptr ? imageName2 : "";
        TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;
    }
    VRMenuSurfaceParms(
        char const* surfaceName,
        char const* imageNames[],
        eSurfaceTextureType textureTypes[])
        : SurfaceName(surfaceName),
          ImageNames(),
          TextureTypes(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(0.5f, 0.5f),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f)

    {
        InitSurfaceProperties();
        for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX && imageNames[i] != NULL; ++i) {
            ImageNames[i] = imageNames[i];
            TextureTypes[i] = textureTypes[i];
        }
    }
    VRMenuSurfaceParms(
        char const* surfaceName,
        GLuint imageTexId0,
        int width0,
        int height0,
        eSurfaceTextureType textureType0,
        GLuint imageTexId1,
        int width1,
        int height1,
        eSurfaceTextureType textureType1,
        GLuint imageTexId2,
        int width2,
        int height2,
        eSurfaceTextureType textureType2)
        : SurfaceName(surfaceName != nullptr ? surfaceName : ""),
          ImageNames(),
          Contents(CONTENT_SOLID),
          Color(1.0f),
          Anchors(0.5f, 0.5f),
          Border(0.0f, 0.0f, 0.0f, 0.0f),
          CropUV(0.0f),
          OffsetUVs(0.0f),
          Dims(0.0f, 0.0f)

    {
        InitSurfaceProperties();
        ImageTexId[VRMENUSURFACE_IMAGE_0] = imageTexId0;
        TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;
        ImageWidth[VRMENUSURFACE_IMAGE_0] = width0;
        ImageHeight[VRMENUSURFACE_IMAGE_0] = height0;

        ImageTexId[VRMENUSURFACE_IMAGE_1] = imageTexId1;
        TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;
        ImageWidth[VRMENUSURFACE_IMAGE_1] = width1;
        ImageHeight[VRMENUSURFACE_IMAGE_1] = height1;

        ImageTexId[VRMENUSURFACE_IMAGE_2] = imageTexId2;
        TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;
        ImageWidth[VRMENUSURFACE_IMAGE_2] = width2;
        ImageHeight[VRMENUSURFACE_IMAGE_2] = height2;
    }

    std::string SurfaceName; // for debugging only
    std::string ImageNames[VRMENUSURFACE_IMAGE_MAX];
    GLuint ImageTexId[VRMENUSURFACE_IMAGE_MAX];
    int ImageWidth[VRMENUSURFACE_IMAGE_MAX];
    int ImageHeight[VRMENUSURFACE_IMAGE_MAX];
    eSurfaceTextureType TextureTypes[VRMENUSURFACE_IMAGE_MAX];
    ContentFlags_t Contents;
    OVR::Vector4f Color;
    OVR::Vector2f Anchors;
    OVR::Vector4f Border; // if set to non-zero, sets the border on a sliced sprite
    OVR::Vector4f
        CropUV; // specifies the crop range of the texture in UV space, defaults to full texture
    OVR::Vector2f OffsetUVs; // offset UVs
    OVR::Vector2f Dims; // if set to zero, use texture size, non-zero sets dims to absolute size

   private:
    void InitSurfaceProperties() {
        for (int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i) {
            TextureTypes[i] = SURFACE_TEXTURE_MAX;
            ImageTexId[i] = 0;
            ImageWidth[i] = 0;
            ImageHeight[i] = 0;
        }

        CropUV = OVR::Vector4f(0.0f, 0.0f, 1.0f, 1.0f);
    }
};

//==============================================================
// VRMenuFontParms
class VRMenuFontParms {
   public:
    VRMenuFontParms()
        : AlignHoriz(HORIZONTAL_CENTER),
          AlignVert(VERTICAL_CENTER),
          Billboard(false),
          TrackRoll(false),
          Outline(false),
          ColorCenter(0.5f),
          AlphaCenter(0.5f),
          Scale(1.0f),
          WrapWidth(-1.0f),
          MaxLines(16),
          MultiLine(true) {}

    VRMenuFontParms(
        bool const centerHoriz,
        bool const centerVert,
        bool const billboard,
        bool const trackRoll,
        bool const outline,
        float const colorCenter,
        float const alphaCenter,
        float const scale)
        : AlignHoriz(centerHoriz ? HORIZONTAL_CENTER : HORIZONTAL_LEFT),
          AlignVert(centerVert ? VERTICAL_CENTER : VERTICAL_BASELINE),
          Billboard(billboard),
          TrackRoll(trackRoll),
          Outline(outline),
          ColorCenter(colorCenter),
          AlphaCenter(alphaCenter),
          Scale(scale),
          WrapWidth(-1.0f),
          MaxLines(16),
          MultiLine(true) {}

    VRMenuFontParms(
        bool const centerHoriz,
        bool const centerVert,
        bool const billboard,
        bool const trackRoll,
        bool const outline,
        float const colorCenter,
        float const alphaCenter,
        float const scale,
        float const wrapWidth,
        int const maxLines,
        bool const multiLine)
        : AlignHoriz(centerHoriz ? HORIZONTAL_CENTER : HORIZONTAL_LEFT),
          AlignVert(centerVert ? VERTICAL_CENTER : VERTICAL_BASELINE),
          Billboard(billboard),
          TrackRoll(trackRoll),
          Outline(outline),
          ColorCenter(colorCenter),
          AlphaCenter(alphaCenter),
          Scale(scale),
          WrapWidth(wrapWidth),
          MaxLines(maxLines),
          MultiLine(multiLine) {}

    VRMenuFontParms(
        bool const centerHoriz,
        bool const centerVert,
        bool const billboard,
        bool const trackRoll,
        bool const outline,
        float const scale)
        : AlignHoriz(centerHoriz ? HORIZONTAL_CENTER : HORIZONTAL_LEFT),
          AlignVert(centerVert ? VERTICAL_CENTER : VERTICAL_BASELINE),
          Billboard(billboard),
          TrackRoll(trackRoll),
          Outline(outline),
          ColorCenter(outline ? 0.5f : 0.0f),
          AlphaCenter(outline ? 0.425f : 0.5f),
          Scale(scale),
          WrapWidth(-1.0f),
          MaxLines(16),
          MultiLine(true) {}

    VRMenuFontParms(
        HorizontalJustification const alignHoriz,
        VerticalJustification const alignVert,
        bool const billboard,
        bool const trackRoll,
        bool const outline,
        float const colorCenter,
        float const alphaCenter,
        float const scale)
        : AlignHoriz(alignHoriz),
          AlignVert(alignVert),
          Billboard(billboard),
          TrackRoll(trackRoll),
          Outline(outline),
          ColorCenter(colorCenter),
          AlphaCenter(alphaCenter),
          Scale(scale),
          WrapWidth(-1.0f),
          MaxLines(16),
          MultiLine(true) {}

    VRMenuFontParms(
        HorizontalJustification const alignHoriz,
        VerticalJustification const alignVert,
        bool const billboard,
        bool const trackRoll,
        bool const outline,
        float const scale)
        : AlignHoriz(alignHoriz),
          AlignVert(alignVert),
          Billboard(billboard),
          TrackRoll(trackRoll),
          Outline(outline),
          ColorCenter(outline ? 0.5f : 0.0f),
          AlphaCenter(outline ? 0.425f : 0.5f),
          Scale(scale),
          WrapWidth(-1.0f),
          MaxLines(16),
          MultiLine(true) {}

    HorizontalJustification
        AlignHoriz; // horizontal justification around the specified x coordinate
    VerticalJustification AlignVert; // vertical justification around the specified y coordinate
    bool Billboard; // true to always face the camera
    bool TrackRoll; // when billboarding, track with camera roll
    bool Outline; // true if font should rended with an outline
    float ColorCenter; // center value for color -- used for outlining
    float AlphaCenter; // center value for alpha -- used for outlined or non-outlined
    float Scale; // scale factor for the text
    float WrapWidth; // wrap width in meters, < 0 means don't wrap
    int MaxLines; // maximum number of lines that this text area can display ( TODO: hook this up to
                  // scrolling )
    bool MultiLine; // used in conjunction with WrapWidth.  If set we only scale and don't wrap text
                    // ( single line )
};

//==============================================================
// VRMenumObjectParms
//
// Parms passed to the VRMenuObject factory to create a VRMenuObject
class VRMenuObjectParms {
   public:
    static VRMenuObjectParms* Create() {
        return new VRMenuObjectParms();
    }

    VRMenuObjectParms(
        eVRMenuObjectType const type,
        std::vector<VRMenuComponent*> const& components,
        VRMenuSurfaceParms const& surfaceParms,
        char const* text,
        OVR::Posef const& localPose,
        OVR::Vector3f const& localScale,
        VRMenuFontParms const& fontParms,
        VRMenuId_t const id,
        VRMenuObjectFlags_t const flags,
        VRMenuObjectInitFlags_t const initFlags)
        : Type(type),
          Flags(flags),
          InitFlags(initFlags),
          Components(components),
          SurfaceParms(),
          Text(text),
          LocalPose(localPose),
          LocalScale(localScale),
          TextLocalPose(OVR::Quatf(), OVR::Vector3f(0.0f)),
          TextLocalScale(1.0f),
          FontParms(fontParms),
          Color(1.0f),
          TextColor(1.0f),
          Id(id),
          Contents(CONTENT_SOLID),
          TexelCoords(false),
          Selected(false) {
        SurfaceParms.push_back(surfaceParms);
    }

    // same as above with additional text local parms
    VRMenuObjectParms(
        eVRMenuObjectType const type,
        std::vector<VRMenuComponent*> const& components,
        VRMenuSurfaceParms const& surfaceParms,
        char const* text,
        OVR::Posef const& localPose,
        OVR::Vector3f const& localScale,
        OVR::Posef const& textLocalPose,
        OVR::Vector3f const& textLocalScale,
        VRMenuFontParms const& fontParms,
        VRMenuId_t const id,
        VRMenuObjectFlags_t const flags,
        VRMenuObjectInitFlags_t const initFlags)
        : Type(type),
          Flags(flags),
          InitFlags(initFlags),
          Components(components),
          SurfaceParms(),
          Text(text),
          LocalPose(localPose),
          LocalScale(localScale),
          TextLocalPose(textLocalPose),
          TextLocalScale(textLocalScale),
          FontParms(fontParms),
          Color(1.0f),
          TextColor(1.0f),
          Id(id),
          Contents(CONTENT_SOLID),
          TexelCoords(false),
          Selected(false) {
        SurfaceParms.push_back(surfaceParms);
    }

    // takes an array of surface parms
    VRMenuObjectParms(
        eVRMenuObjectType const type,
        std::vector<VRMenuComponent*> const& components,
        std::vector<VRMenuSurfaceParms> const& surfaceParms,
        char const* text,
        OVR::Posef const& localPose,
        OVR::Vector3f const& localScale,
        OVR::Posef const& textLocalPose,
        OVR::Vector3f const& textLocalScale,
        VRMenuFontParms const& fontParms,
        VRMenuId_t const id,
        VRMenuObjectFlags_t const flags,
        VRMenuObjectInitFlags_t const initFlags)
        : Type(type),
          Flags(flags),
          InitFlags(initFlags),
          Components(components),
          SurfaceParms(surfaceParms),
          Text(text),
          LocalPose(localPose),
          LocalScale(localScale),
          TextLocalPose(textLocalPose),
          TextLocalScale(textLocalScale),
          FontParms(fontParms),
          Color(1.0f),
          TextColor(1.0f),
          Id(id),
          Contents(CONTENT_SOLID),
          TexelCoords(false),
          Selected(false) {}

    eVRMenuObjectType Type; // type of menu object
    VRMenuObjectFlags_t Flags; // bit flags
    VRMenuObjectInitFlags_t
        InitFlags; // intialization-only flags (not referenced beyond object initialization)
    std::vector<VRMenuComponent*> Components; // list of pointers to components
    std::vector<VRMenuSurfaceParms>
        SurfaceParms; // list of surface parameters for the object. Each parm will result in one
                      // surface, and surfaces will render in the same order as this list.
    std::string Text; // text to display on this object (if any)
    OVR::Posef LocalPose; // local-space position and orientation
    OVR::Vector3f LocalScale; // local-space scale
    OVR::Posef TextLocalPose; // offset of text, local to surface
    OVR::Vector3f TextLocalScale; // scale of text, local to surface
    VRMenuFontParms FontParms; // parameters for rendering the object's text
    OVR::Vector4f Color; // color modulation for surfaces
    OVR::Vector4f TextColor; // color of text
    VRMenuId_t Id; // user identifier, so the client using a menu can find a particular object
    VRMenuId_t ParentId; // id of object that should be made this object's parent.
    ContentFlags_t Contents; // collision contents for the menu object
    std::string Name; // for easier parenting
    std::string ParentName;
    std::string Tag;
    bool TexelCoords; // if true, pose translation units are in texels
    bool Selected; // true to start selected

   private:
    VRMenuObjectParms() // only VRMenuObjectParms::Create() can create a default object of this type
        : Type(VRMENU_MAX),
          Flags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER),
          InitFlags(VRMENUOBJECT_INIT_FORCE_POSITION),
          LocalPose(OVR::Quatf(), OVR::Vector3f(0.0f)),
          LocalScale(1.0f),
          TextLocalPose(OVR::Quatf(), OVR::Vector3f(0.0f)),
          TextLocalScale(1.0f),
          FontParms(),
          Color(1.0f),
          TextColor(1.0f),
          Contents(CONTENT_SOLID),
          TexelCoords(false),
          Selected(false) {}
};

//==============================================================
// HitTestResult
// returns the results of a hit test vs menu objects
class HitTestResult : public OvrCollisionResult {
   public:
    HitTestResult& operator=(OvrCollisionResult& rhs) {
        this->HitHandle.Release();
        this->RayStart = OVR::Vector3f::ZERO;
        this->RayDir = OVR::Vector3f::ZERO;
        this->t = rhs.t;
        this->uv = rhs.uv;
        return *this;
    }

    menuHandle_t HitHandle;
    OVR::Vector3f RayStart;
    OVR::Vector3f RayDir;
};

typedef std::int16_t guiIndex_t;

struct textMetrics_t {
    textMetrics_t() : w(0.0f), h(0.0f), ascent(0.0f), descent(0.0f), fontHeight(0.0f) {}

    float w;
    float h;
    float ascent;
    float descent;
    float fontHeight;
};

class App;

//==============================================================
// VRMenuSurfaceTexture
class VRMenuSurfaceTexture {
   public:
    VRMenuSurfaceTexture();

    bool LoadTexture(
        OvrGuiSys& guiSys,
        eSurfaceTextureType const type,
        char const* imageName,
        bool const allowDefault);
    void LoadTexture(
        eSurfaceTextureType const type,
        const GLuint texId,
        const int width,
        const int height);
    void Free();
    void SetOwnership(const bool isOwner) {
        OwnsTexture = isOwner;
    }

    GlTexture const& GetTexture() const {
        return Texture;
    }
    int GetWidth() const {
        return Texture.Width;
    }
    int GetHeight() const {
        return Texture.Height;
    }
    eSurfaceTextureType GetType() const {
        return Type;
    }

   private:
    GlTexture Texture;
    eSurfaceTextureType Type; // specifies how this image is used for rendering
    bool OwnsTexture; // if true, free texture on a reload or deconstruct
};

//==============================================================
// SubmittedMenuObject
class SubmittedMenuObject {
   public:
    SubmittedMenuObject()
        : SurfaceIndex(-1),
          DistanceIndex(-1),
          Scale(1.0f),
          Color(1.0f),
          ColorTableOffset(0.0f),
          SkipAdditivePass(false),
          Offsets(0.0f),
          ClipUVs(0.0f, 0.0f, 1.0f, 1.0f),
          OffsetUVs(0.0f, 0.0f),
          FadeDirection(0.0f) {}

    menuHandle_t Handle; // handle of the object
    int SurfaceIndex; // surface of the object
    int DistanceIndex; // use the position at this index to calc sort distance
    OVR::Posef Pose; // pose in model space
    OVR::Vector3f Scale; // scale of the object
    OVR::Vector4f Color; // color of the object
    OVR::Vector2f ColorTableOffset; // color table offset for color ramp fx
    bool SkipAdditivePass; // true to skip any additive multi-texture pass
    VRMenuRenderFlags_t Flags; // various flags
    OVR::Vector2f Offsets; // offsets based on anchors (width / height * anchor.x / .y)
    OVR::Vector4f ClipUVs; // x,y are min clip uvs, z,w are max clip uvs
    OVR::Vector2f OffsetUVs; // offset for UV
    OVR::Vector3f FadeDirection; // Fades vertices based on direction - default is zero vector which
                                 // indicates off
    std::string SurfaceName; // for debugging only
    OVR::Bounds3f LocalBounds; // local bounds
};

//==============================================================
// VRMenuSurface
class VRMenuSurface {
   public:
    // artificial bounds in Z, which is technically 0 for surfaces that are an image
    static const float Z_BOUNDS;

    VRMenuSurface();
    ~VRMenuSurface();

    void CreateFromSurfaceParms(OvrGuiSys& guiSys, VRMenuSurfaceParms const& parms);

    void Free();

    void RegenerateSurfaceGeometry();

    OVR::Bounds3f const& GetLocalBounds() const {
        return Tris.GetBounds();
    }

    bool IsRenderable() const {
        return (SurfaceDef.geo.vertexCount > 0 && SurfaceDef.geo.indexCount > 0) && Visible;
    }

    bool IntersectRay(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Posef const& pose,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const;
    // the ray should already be in local space
    bool IntersectRay(
        OVR::Vector3f const& localStart,
        OVR::Vector3f const& localDir,
        OVR::Vector3f const& scale,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const;
    void LoadTexture(
        OvrGuiSys& guiSys,
        int const textureIndex,
        eSurfaceTextureType const type,
        const char* imageName);
    void LoadTexture(
        int const textureIndex,
        eSurfaceTextureType const type,
        const GLuint texId,
        const int width,
        const int height);

    OVR::Vector4f const& GetColor() const {
        return Color;
    }
    void SetColor(OVR::Vector4f const& color) {
        Color = color;
    }

    OVR::Vector2f const& GetDims() const {
        return Dims;
    }
    void SetDims(OVR::Vector2f const& dims) {
        Dims = dims;
    } // requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

    OVR::Vector2f const& GetAnchors() const {
        return Anchors;
    }
    void SetAnchors(OVR::Vector2f const& a) {
        Anchors = a;
    }

    OVR::Vector2f GetAnchorOffsets() const;

    OVR::Vector4f const& GetBorder() const {
        return Border;
    }
    void SetBorder(OVR::Vector4f const& a) {
        Border = a;
    } // requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

    OVR::Vector4f const& GetClipUVs() const {
        return ClipUVs;
    }
    void SetClipUVs(OVR::Vector4f const& uvs) {
        ClipUVs = uvs;
    }

    OVR::Vector2f const& GetOffsetUVs() const {
        return OffsetUVs;
    }
    void SetOffsetUVs(OVR::Vector2f const& uvs) {
        OffsetUVs = uvs;
    }

    VRMenuSurfaceTexture const& GetTexture(int const index) const {
        return Textures[index];
    }
    void SetOwnership(int const index, bool const isOwner);

    bool GetVisible() const {
        return Visible;
    }
    void SetVisible(bool const v) {
        Visible = v;
    }

    std::string const& GetName() const {
        return SurfaceName;
    }

    void BuildDrawSurface(
        OvrVRMenuMgr const& menuMgr,
        OVR::Matrix4f const& modelMatrix,
        const char* surfaceName,
        OVR::Vector4f const& color,
        OVR::Vector3f const& fadeDirection,
        OVR::Vector2f const& colorTableOffset,
        OVR::Vector4f const& clipUVs,
        OVR::Vector2f const& offsetUVs,
        bool const skipAdditivePass,
        VRMenuRenderFlags_t const& flags,
        OVR::Bounds3f const& worldBounds,
        ovrDrawSurface& outSurf);

    OvrTriCollisionPrimitive const& GetTris() const {
        return Tris;
    }

   private:
    VRMenuSurfaceTexture Textures[VRMENUSURFACE_IMAGE_MAX];
    // GlGeometry						Geo;				// VBO for this surface
    OvrTriCollisionPrimitive Tris; // per-poly collision object
    OVR::Vector4f Color; // Color, modulated with object color
    OVR::Vector2i TextureDims; // texture width and height
    OVR::Vector2f Dims; // width and height
    OVR::Vector2f Anchors; // anchors
    OVR::Vector4f Border; // border size for sliced sprite
    OVR::Vector4f ClipUVs; // UV boundaries for fragment clipping
    OVR::Vector2f OffsetUVs; // UV offsets
    OVR::Vector4f CropUV; // Crop range in UV space
    OVR::Vector4f FadeDirection; // fade direction for some shaders
    OVR::Vector2f ColorTableOffset; // color table offset
    std::string SurfaceName; // name of the surface for debugging
    ContentFlags_t Contents;
    bool Visible; // must be true to render -- used to animate between different surfaces

    eGUIProgramType ProgramType;

    mutable ovrSurfaceDef SurfaceDef;

   private:
    void CreateImageGeometry(
        int const textureWidth,
        int const textureHeight,
        const OVR::Vector2f& dims,
        const OVR::Vector4f& border,
        const OVR::Vector4f& crop,
        ContentFlags_t const content);
    // This searches the loaded textures for the specified number of matching types. For instance,
    // ( SURFACE_TEXTURE_DIFFUSE, 2 ) would only return true if two of the textures were of type
    // SURFACE_TEXTURE_DIFFUSE.  This is used to determine, based on surface types, which shaders
    // to use to render the surface, independent of how the texture maps are ordered.
    bool HasTexturesOfType(eSurfaceTextureType const t, int const requiredCount) const;
    // Returns the index in Textures[] of the n-th occurence of type t.
    int IndexForTextureType(eSurfaceTextureType const t, int const occurenceCount) const;
    void SetTextureSampling(eGUIProgramType const pt);
};

//==============================================================
// ovrComponentList
class ovrComponentList {
   public:
    ovrComponentList() {} // std::vector<> requires a default constructor because ummm... yeah.

    ovrComponentList(menuHandle_t const ownerHandle) : OwnerHandle(ownerHandle) {}

    menuHandle_t GetOwnerHandle() const {
        return OwnerHandle;
    }

    void AddComponent(VRMenuComponent* component) {
        // never add a component twice
        for (auto* c : Components) {
            if (c == component) {
                return;
            }
        }
        Components.push_back(component);
    }

    std::vector<VRMenuComponent*>& GetComponents() {
        return Components;
    }

   private:
    menuHandle_t OwnerHandle; // object that owns the components
    std::vector<VRMenuComponent*> Components; // components to delete
};

//==============================================================
// VRMenuObject
// base class for all menu objects
class VRMenuObject {
   public:
    friend class VRMenuMgr;
    friend class VRMenuMgrLocal;

    class ovrRecursionFunctor {
       public:
        virtual ~ovrRecursionFunctor() {}

        virtual void AtNode(VRMenuObject* obj) = 0;
    };

    static float const TEXELS_PER_METER;
    static float const DEFAULT_TEXEL_SCALE;

    // Initialize the object after creation
    void Init(OvrGuiSys& guiSys, VRMenuObjectParms const& parms);

    // Frees all of this object's children
    void FreeChildren(OvrVRMenuMgr& menuMgr);

    // Adds a child to this menu object
    void AddChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle);
    // Removes a child from this menu object, but does not free the child object.
    void RemoveChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle);
    // Removes a child from tis menu object and frees it, along with any children it may have.
    void FreeChild(OvrVRMenuMgr& menuMgr, menuHandle_t const handle);
    // Returns true if the handle is in this menu's tree of children
    bool IsDescendant(OvrVRMenuMgr& menuMgr, menuHandle_t const handle) const;

    // Update this menu for a frame, including testing the gaze direction against the bounds
    // of this menu and all its children
    void Frame(OvrVRMenuMgr& menuMgr, OVR::Matrix4f const& viewMatrix);

    // Tests a ray (in object's local space) for intersection.
    bool IntersectRay(
        OVR::Vector3f const& localStart,
        OVR::Vector3f const& localDir,
        OVR::Vector3f const& parentScale,
        OVR::Bounds3f const& bounds,
        float& bounds_t0,
        float& bounds_t1,
        ContentFlags_t const testContents,
        OvrCollisionResult& result) const;

    // Test the ray against this object and all child objects, returning the first object that was
    // hit by the ray. The ray should be in parent-local space - for the current root menu this is
    // always world space.
    menuHandle_t HitTest(
        OvrGuiSys const& guiSys,
        OVR::Posef const& worldPose,
        OVR::Vector3f const& rayStart,
        OVR::Vector3f const& rayDir,
        ContentFlags_t const testContents,
        HitTestResult& result) const;

    //--------------------------------------------------------------
    // components
    //--------------------------------------------------------------
    void AddComponent(VRMenuComponent* component);
    void DeleteComponent(OvrGuiSys& guiSys, VRMenuComponent* component);

    std::vector<VRMenuComponent*> const& GetComponentList() const {
        return Components;
    }

    VRMenuComponent* GetComponentById_Impl(int const typeId, const char* name) const;
    VRMenuComponent* GetComponentByTypeName_Impl(const char* typeName) const;

    // TODO We might want to refactor these into a single GetComponent which internally manages
    // unique ids (using hashed class names for ex. ) Helper for getting component - returns NULL if
    // it fails. Required Component class to overload GetTypeId and define unique TYPE_ID
    template <typename ComponentType>
    ComponentType* GetComponentById() const {
        return static_cast<ComponentType*>(GetComponentById_Impl(ComponentType::TYPE_ID, NULL));
    }

    template <typename ComponentType>
    ComponentType* GetComponentByName(const char* name) const {
        return static_cast<ComponentType*>(GetComponentById_Impl(ComponentType::TYPE_ID, name));
    }

    // Helper for getting component - returns NULL if it fails. Required Component class to overload
    // GetTypeName and define unique TYPE_NAME
    template <typename ComponentType>
    ComponentType* GetComponentByTypeName() const {
        return static_cast<ComponentType*>(GetComponentByTypeName_Impl(ComponentType::TYPE_NAME));
    }

    //--------------------------------------------------------------
    // accessors
    //--------------------------------------------------------------
    eVRMenuObjectType GetType() const {
        return Type;
    }
    menuHandle_t GetHandle() const {
        return Handle;
    }
    menuHandle_t GetParentHandle() const {
        return ParentHandle;
    }
    void SetParentHandle(menuHandle_t const h) {
        ParentHandle = h;
    }

    VRMenuObjectFlags_t const& GetFlags() const {
        return Flags;
    }
    void SetFlags(VRMenuObjectFlags_t const& flags) {
        Flags = flags;
    }
    void AddFlags(VRMenuObjectFlags_t const& flags) {
        Flags |= flags;
    }
    void RemoveFlags(VRMenuObjectFlags_t const& flags) {
        Flags &= ~flags;
    }

    void ModifyFlags(bool const add, VRMenuObjectFlags_t const& flags) {
        if (add) {
            AddFlags(flags);
        } else {
            RemoveFlags(flags);
        }
    }

    std::string const& GetText() const {
        return Text;
    }
    void SetText(char const* text);
    void
    SetTextWordWrapped(char const* text, class BitmapFont const& font, float const widthInMeters);

    bool IsHilighted() const {
        return Hilighted;
    }
    void SetHilighted(bool const b) {
        Hilighted = b;
    }
    bool IsSelected() const {
        return Selected;
    }
    // NOTE: this will not send the VRMENU_EVENT_SELECTED or VRMENU_EVENT_DESELECTED event.
    // For that, use VRMenu::SetSelected()
    void SetSelected(bool const b) {
        Selected = b;
    }
    int NumChildren() const {
        return static_cast<int>(Children.size());
    }
    menuHandle_t GetChildHandleForIndex(int const index) const {
        return Children[index];
    }

    OVR::Posef const& GetLocalPose() const {
        return LocalPose;
    }
    void SetLocalPose(OVR::Posef const& pose) {
        LocalPose = pose;
    }
    OVR::Vector3f const& GetLocalPosition() const {
        return LocalPose.Translation;
    }
    void SetLocalPosition(OVR::Vector3f const& pos) {
        LocalPose.Translation = pos;
    }
    OVR::Quatf const& GetLocalRotation() const {
        return LocalPose.Rotation;
    }
    void SetLocalRotation(OVR::Quatf const& rot) {
        LocalPose.Rotation = rot;
    }
    OVR::Vector3f GetLocalScale() const;
    void SetLocalScale(OVR::Vector3f const& scale) {
        LocalScale = scale;
    }

    OVR::Posef const& GetHilightPose() const {
        return HilightPose;
    }
    void SetHilightPose(OVR::Posef const& pose) {
        HilightPose = pose;
    }
    float GetHilightScale() const {
        return HilightScale;
    }
    void SetHilightScale(float const s) {
        HilightScale = s;
    }

    void SetTextLocalPose(OVR::Posef const& pose) {
        TextLocalPose = pose;
    }
    OVR::Posef const& GetTextLocalPose() const {
        return TextLocalPose;
    }
    void SetTextLocalPosition(OVR::Vector3f const& pos) {
        TextLocalPose.Translation = pos;
    }
    OVR::Vector3f const& GetTextLocalPosition() const {
        return TextLocalPose.Translation;
    }
    void SetTextLocalRotation(OVR::Quatf const& rot) {
        TextLocalPose.Rotation = rot;
    }
    OVR::Quatf const& GetTextLocalRotation() const {
        return TextLocalPose.Rotation;
    }
    OVR::Vector3f GetTextLocalScale() const;
    float GetWrapScale() const {
        return WrapScale;
    }
    void SetTextLocalScale(OVR::Vector3f const& scale) {
        TextLocalScale = scale;
    }

    void SetLocalBoundsExpand(OVR::Vector3f const mins, OVR::Vector3f const& maxs);

    OVR::Bounds3f GetLocalBounds(BitmapFont const& font) const;
    OVR::Bounds3f GetTextLocalBounds(BitmapFont const& font) const;
    OVR::Bounds3f CalcLocalBoundsForText(BitmapFont const& font, std::string& text) const;

    OVR::Bounds3f const& GetCullBounds() const {
        return CullBounds;
    }
    void SetCullBounds(OVR::Bounds3f const& bounds) const {
        CullBounds = bounds;
    }

    OVR::Vector2f const& GetColorTableOffset() const;
    void SetColorTableOffset(OVR::Vector2f const& ofs);

    OVR::Vector4f const& GetColor() const;
    void SetColor(OVR::Vector4f const& c);

    OVR::Vector4f const& GetTextColor() const {
        return TextColor;
    }
    void SetTextColor(OVR::Vector4f const& c) {
        TextColor = c;
    }

    std::string const& GetName() const {
        return Name;
    }
    std::string const& GetTag() const {
        return Tag;
    }
    VRMenuId_t GetId() const {
        return Id;
    }
    VRMenuObject* ChildForId(OvrVRMenuMgr const& menuMgr, VRMenuId_t const id) const;
    menuHandle_t ChildHandleForId(OvrVRMenuMgr const& menuMgr, VRMenuId_t const id) const;
    menuHandle_t ChildHandleForName(OvrVRMenuMgr const& menuMgr, char const* name) const;
    menuHandle_t ChildHandleForTag(OvrVRMenuMgr const& menuMgr, char const* tag) const;

    void SetFontParms(VRMenuFontParms const& fontParms) {
        FontParms = fontParms;
    }
    VRMenuFontParms const& GetFontParms() const {
        return FontParms;
    }

    OVR::Vector3f const& GetFadeDirection() const {
        return FadeDirection;
    }
    void SetFadeDirection(OVR::Vector3f const& dir) {
        FadeDirection = dir;
    }

    void SetVisible(bool visible);

    // returns the index of the first surface with SURFACE_TEXTURE_ADDITIVE.
    // If singular is true, then the matching surface must have only one texture map and it must be
    // of that type.
    int FindSurfaceWithTextureType(eSurfaceTextureType const type, bool const singular) const;
    int FindSurfaceWithTextureType(
        eSurfaceTextureType const type,
        bool const singular,
        bool const visibleOnly) const;
    void SetSurfaceColor(int const surfaceIndex, OVR::Vector4f const& color);
    OVR::Vector4f const& GetSurfaceColor(int const surfaceIndex) const;
    void SetSurfaceVisible(int const surfaceIndex, bool const v);
    bool GetSurfaceVisible(int const surfaceIndex) const;
    int NumSurfaces() const;
    int AllocSurface();
    void CreateFromSurfaceParms(
        OvrGuiSys& guiSys,
        int const surfaceIndex,
        VRMenuSurfaceParms const& parms);

    void SetSurfaceTexture(
        OvrGuiSys& guiSys,
        int const surfaceIndex,
        int const textureIndex,
        eSurfaceTextureType const type,
        char const* imageName);

    void SetSurfaceTexture(
        int const surfaceIndex,
        int const textureIndex,
        eSurfaceTextureType const type,
        GLuint const texId,
        int const width,
        int const height);
    void SetSurfaceTexture(
        int const surfaceIndex,
        int const textureIndex,
        eSurfaceTextureType const type,
        class GlTexture const& texture);

    void SetSurfaceTextureTakeOwnership(
        int const surfaceIndex,
        int const textureIndex,
        eSurfaceTextureType const type,
        GLuint const texId,
        int const width,
        int const height);
    void SetSurfaceTextureTakeOwnership(
        int const surfaceIndex,
        int const textureIndex,
        eSurfaceTextureType const type,
        class GlTexture const& texture);

    void RegenerateSurfaceGeometry(int const surfaceIndex, const bool freeSurfaceGeometry);

    OVR::Vector2f const& GetSurfaceDims(int const surfaceIndex) const;
    void SetSurfaceDims(int const surfaceIndex, OVR::Vector2f const& dims);

    OVR::Vector4f const& GetSurfaceBorder(int const surfaceIndex);
    void SetSurfaceBorder(int const surfaceIndex, OVR::Vector4f const& border);

    //--------------------------------------------------------------
    // collision
    //--------------------------------------------------------------
    void SetCollisionPrimitive(OvrCollisionPrimitive* c);
    OvrCollisionPrimitive* GetCollisionPrimitive() {
        return CollisionPrimitive;
    }
    OvrCollisionPrimitive const* GetCollisionPrimitive() const {
        return CollisionPrimitive;
    }

    ContentFlags_t GetContents() const {
        return Contents;
    }
    void SetContents(ContentFlags_t const c) {
        Contents = c;
    }

    //--------------------------------------------------------------
    // surfaces (non-virtual)
    //--------------------------------------------------------------
    VRMenuSurface const& GetSurface(int const s) const {
        return Surfaces[s];
    }
    VRMenuSurface& GetSurface(int const s) {
        return Surfaces[s];
    }
    std::vector<VRMenuSurface> const& GetSurfaces() const {
        return Surfaces;
    }

    void BuildDrawSurface(
        OvrVRMenuMgr const& menuMgr,
        OVR::Matrix4f const& modelMatrix,
        const char* surfaceName,
        int const surfaceIndex,
        OVR::Vector4f const& color,
        OVR::Vector3f const& fadeDirection,
        OVR::Vector2f const& colorTableOffset,
        OVR::Vector4f const& clipUVs,
        OVR::Vector2f const& offsetUVs,
        bool const skipAdditivePass,
        VRMenuRenderFlags_t const& flags,
        OVR::Bounds3f const& worldBounds,
        std::vector<ovrDrawSurface>& surfaceList);

    //--------------------------------------------------------------
    // transformation (used by VRMenuMgr)
    //--------------------------------------------------------------
    void GetWorldTransform(
        OvrVRMenuMgr& menuMgr,
        OVR::Posef const& menuPose,
        OVR::Posef& outPose,
        OVR::Vector3f& outScale,
        OVR::Vector4f& outColor) const;

    static void TransformByParent(
        OVR::Posef const& parentPose,
        OVR::Vector3f const& parentScale,
        OVR::Vector4f const& parentColor,
        OVR::Posef const& localPose,
        OVR::Vector3f const& localScale,
        OVR::Vector4f const& localColor,
        VRMenuObjectFlags_t const& objectFlags,
        OVR::Posef& outPose,
        OVR::Vector3f& outScale,
        OVR::Vector4f& outColor);

    static void
    Recurse(OvrVRMenuMgr const& menuMgr, ovrRecursionFunctor& functor, VRMenuObject* obj);

    //--------------------------------------------------------------
    // reflection
    //--------------------------------------------------------------
    static ovrParseResult ParseItemParms(
        ovrReflection& refl,
        ovrLocale const& locale,
        char const* fileName,
        std::vector<uint8_t> const& buffer,
        std::vector<VRMenuObjectParms const*>& itemParms);

   private:
    eVRMenuObjectType Type; // type of this object
    menuHandle_t Handle; // handle of this object
    menuHandle_t ParentHandle; // handle of this object's parent
    VRMenuId_t Id; // opaque id that the creator of the menu can use to identify a menu object
    VRMenuObjectFlags_t Flags; // various bit flags
    std::string Name; // name of this object (can be empty)
    std::string Tag; // tags are like names but are always child-relative.
    OVR::Posef LocalPose; // local-space position and orientation
    OVR::Vector3f LocalScale; // local-space scale of this item
    OVR::Posef HilightPose; // additional pose applied when hilighted
    float HilightScale; // additional scale when hilighted
    mutable float WrapScale; // scale used when wrapping text fails
    OVR::Posef TextLocalPose; // local-space position and orientation of text, local to this node
                              // (i.e. after LocalPose / LocalScale are applied)
    OVR::Vector3f TextLocalScale; // local-space scale of the text at this node
    mutable std::string Text; // text to display on this object - this is mutable but only changes
                              // if word wrapping is required
    std::vector<menuHandle_t> Children; // array of direct children of this object
    std::vector<VRMenuComponent*> Components; // array of components on this object
    OvrCollisionPrimitive* CollisionPrimitive; // collision surface, if any
    ContentFlags_t Contents; // content flags for this object

    // may be cleaner to put texture and geometry in a separate surface structure
    std::vector<VRMenuSurface> Surfaces;
    OVR::Vector4f Color; // color modulation
    OVR::Vector4f TextColor; // color modulation for text
    OVR::Vector2f ColorTableOffset; // offset for color-ramp shader fx
    VRMenuFontParms FontParms; // parameters for text rendering
    OVR::Vector3f FadeDirection; // Fades vertices based on direction - default is zero vector which
                                 // indicates off

    bool Hilighted; // true if hilighted
    bool Selected; // true if selected
    mutable bool TextDirty; // if true, recalculate text bounds

    // cached state
    OVR::Vector3f MinsBoundsExpand; // amount to expand local bounds mins
    OVR::Vector3f MaxsBoundsExpand; // amount to expand local bounds maxs
    mutable OVR::Bounds3f
        CullBounds; // bounds of this object and all its children in the local space of its parent
    mutable textMetrics_t TextMetrics; // cached metrics for the text

    struct ovrTextSurface {
        ovrSurfaceDef SurfaceDef;
        OVR::Matrix4f ModelMatrix;
    };

    mutable ovrTextSurface* TextSurface;

   private:
    // only VRMenuMgrLocal static methods can construct and destruct a menu object.
    VRMenuObject(VRMenuObjectParms const& parms, menuHandle_t const handle);
    ~VRMenuObject();

    bool IntersectRayBounds(
        OVR::Vector3f const& start,
        OVR::Vector3f const& dir,
        OVR::Vector3f const& mins,
        OVR::Vector3f const& maxs,
        ContentFlags_t const testContents,
        float& t0,
        float& t1) const;

    // Test the ray against this object and all child objects, returning the first object that was
    // hit by the ray. The ray should be in parent-local space - for the current root menu this is
    // always world space.
    bool HitTest_r(
        OvrGuiSys const& guiSys,
        OVR::Posef const& parentPose,
        OVR::Vector3f const& parentScale,
        OVR::Vector3f const& rayStart,
        OVR::Vector3f const& rayDir,
        ContentFlags_t const testContents,
        HitTestResult& result) const;

    int GetComponentIndex(VRMenuComponent* component) const;

    void FreeTextSurface() const;

    // Called by VRMenuMgr to free deleted components.
    void FreeComponents(ovrComponentList& componentList);
};

} // namespace OVRFW
