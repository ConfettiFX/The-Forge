/************************************************************************************

Filename    :   BitmapFont.h
Content     :   Bitmap font rendering.
Created     :   March 11, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include <vector>
#include <string>

#include "OVR_Math.h"
#include "SurfaceRender.h" // SurfaceDef

namespace OVRFW {

class ovrFileSys;
class BitmapFont;
class BitmapFontSurface;

enum HorizontalJustification { HORIZONTAL_LEFT, HORIZONTAL_CENTER, HORIZONTAL_RIGHT };

enum VerticalJustification {
    VERTICAL_BASELINE, // align text by baseline of first row
    VERTICAL_CENTER,
    VERTICAL_CENTER_FIXEDHEIGHT, // ignores ascenders/descenders
    VERTICAL_TOP
};

// To get a black outline on fonts, AlphaCenter should be < ( ColorCenter = 0.5 )
// To get non-outlined fonts, ColorCenter should be < ( AlphaCenter = 0.5 )
struct fontParms_t {
    fontParms_t()
        : AlignHoriz(HORIZONTAL_LEFT),
          AlignVert(VERTICAL_BASELINE),
          Billboard(false),
          TrackRoll(false),
          AlphaCenter(0.425f),
          ColorCenter(0.50f) {}

    HorizontalJustification
        AlignHoriz; // horizontal justification around the specified x coordinate
    VerticalJustification AlignVert; // vertical justification around the specified y coordinate
    bool Billboard; // true to always face the camera
    bool TrackRoll; // when billboarding, track with camera roll
    float AlphaCenter; // below this distance, alpha is 0, above this alpha is 1
    float ColorCenter; // below this distance, color is 0, above this color is 1
};

//==============================================================
// BitmapFont
class BitmapFont {
   public:
    static BitmapFont* Create();
    static void Free(BitmapFont*& font);

    virtual bool Load(ovrFileSys& fileSys, const char* uri) = 0;

    // Calculates the native (unscaled) width of the text string. Line endings are ignored.
    virtual float CalcTextWidth(char const* text) const = 0;
    // Calculates the native (unscaled) width of the text string. Each '\n' will start a new line
    // and will increase the height by FontInfo.FontHeight. For multi-line strings, lineWidths will
    // contain the width of each individual line of text and width will be the width of the widest
    // line of text.
    virtual void CalcTextMetrics(
        char const* text,
        size_t& len,
        float& width,
        float& height,
        float& ascent,
        float& descent,
        float& fontHeight,
        float* lineWidths,
        int const maxLines,
        int& numLines) const = 0;

    virtual void TruncateText(std::string& inOutText, int const maxLines) const = 0;

    // Word wraps passed in text based on the passed in width in meters.
    // Turns any pre-existing escape characters into spaces.
    virtual bool WordWrapText(
        std::string& inOutText,
        const float widthMeters,
        const float fontScale = 1.0f) const = 0;
    // Another version of WordWrapText which doesn't break in between strings that are listed in
    // wholeStrsList array Ex : "Gear VR", we don't want to break in between "Gear" & "VR" so we
    // need to pass "Gear VR" string in wholeStrsList
    virtual bool WordWrapText(
        std::string& inOutText,
        const float widthMeters,
        std::vector<std::string> wholeStrsList,
        const float fontScale = 1.0f) const = 0;

    // Get the last part of the string that will fit in the provided width. Returns an offset if the
    // entire string doesn't fit. The offset can be used to help with right justification. It is the
    // width of the part of the last character that would have fit.
    virtual float GetLastFitChars(
        std::string& inOutText,
        const float widthMeters,
        const float fontScale = 1.0f) const = 0;
    virtual float GetFirstFitChars(
        std::string& inOutText,
        const float widthMeters,
        const int numLines,
        const float fontScale = 1.0f) const = 0;

    // Returns a drawable surface with a newly allocated GlGeometry for the text,
    // allowing it to be sorted or transformed with more control than the global
    // BitmapFontSurface.
    //
    // Geometry is laid out on the Z = 0.0f plane, around the origin based on
    // the justification options, with faces oriented to be visible when viewing
    // down -Z.
    //
    // The SurfaceDef GlGeometry must be freed, but the GlProgram is shared by all
    // users of the font.
    virtual ovrSurfaceDef TextSurface(
        const char* text,
        float scale,
        const OVR::Vector4f& color,
        HorizontalJustification hjust,
        VerticalJustification vjust,
        fontParms_t const* fp = nullptr) const = 0;

    virtual OVR::Vector2f GetScaleFactor() const = 0;
    virtual void GetGlyphMetrics(
        const uint32_t charCode,
        float& width,
        float& height,
        float& advancex,
        float& advancey) const = 0;

   protected:
    virtual ~BitmapFont() {}
};

//==============================================================
// BitmapFontSurface
class BitmapFontSurface {
   public:
    static BitmapFontSurface* Create();
    static void Free(BitmapFontSurface*& fontSurface);

    virtual void Init(const int maxVertices) = 0;
    // Draw functions returns the amount to modify position by for the next draw call if you want to
    // render more lines and have them spaced normally. Will be along the vector up.
    virtual OVR::Vector3f DrawText3D(
        BitmapFont const& font,
        const fontParms_t& flags,
        const OVR::Vector3f& pos,
        OVR::Vector3f const& normal,
        OVR::Vector3f const& up,
        float const scale,
        OVR::Vector4f const& color,
        char const* text) = 0;
    virtual OVR::Vector3f DrawText3Df(
        BitmapFont const& font,
        const fontParms_t& flags,
        const OVR::Vector3f& pos,
        OVR::Vector3f const& normal,
        OVR::Vector3f const& up,
        float const scale,
        OVR::Vector4f const& color,
        char const* text,
        ...) = 0;

    virtual OVR::Vector3f DrawTextBillboarded3D(
        BitmapFont const& font,
        fontParms_t const& flags,
        OVR::Vector3f const& pos,
        float const scale,
        OVR::Vector4f const& color,
        char const* text) = 0;
    virtual OVR::Vector3f DrawTextBillboarded3Df(
        BitmapFont const& font,
        fontParms_t const& flags,
        OVR::Vector3f const& pos,
        float const scale,
        OVR::Vector4f const& color,
        char const* fmt,
        ...) = 0;

    virtual void Finish(OVR::Matrix4f const& viewMatrix) = 0;

    virtual void AppendSurfaceList(BitmapFont const& font, std::vector<ovrDrawSurface>& surfaceList)
        const = 0;

    virtual bool IsInitialized() const = 0;

    virtual void SetCullEnabled(const bool enabled) = 0;

   protected:
    virtual ~BitmapFontSurface() {}
};

} // namespace OVRFW
