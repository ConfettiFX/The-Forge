/************************************************************************************

Filename    :   BitmapFont.cpp
Content     :   Monospaced bitmap font rendering intended for debugging only.
Created     :   March 11, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

// TODO:
// - add support for multiple fonts per surface using texture arrays (store texture in 3rd texture
// coord)
// - in-world text really should sort with all other transparent surfaces
//

#include "BitmapFont.h"

#include <errno.h>
#include <math.h>
#include <sys/stat.h>

#include "OVR_UTF8Util.h"
#include "OVR_JSON.h"
#include "OVR_Math.h"

#include "Misc/Log.h"

#include "Egl.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"

#include "PackageFiles.h"
#include "OVR_FileSys.h"
#include "OVR_Uri.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

inline Vector3f GetViewMatrixUp(Matrix4f const& m) {
    return Vector3f(-m.M[1][0], -m.M[1][1], -m.M[1][2]).Normalized();
}

inline std::string ExtractFile(const std::string& s) {
    const int l = static_cast<int>(s.length());
    if (l == 0) {
        return std::string("");
    }

    int end = l;
    if (s[l - 1] == '/') { // directory ends in a slash
        end = l - 1;
    }

    int start;
    for (start = end - 1; start > -1 && s[start] != '/'; start--)
        ;
    start++;

    return std::string(&s[start], end - start);
}

inline void StripFilename(char const* inPath, char* outPath, size_t const outPathSize) {
    const char WIN_PATH_SEPARATOR = '\\';
    const char NIX_PATH_SEPARATOR = '/';
    const char URI_PATH_SEPARATOR = '/';

    assert(inPath != NULL);
    if (inPath[0] == '\0') {
        OVR::OVR_strcpy(outPath, outPathSize, inPath);
        return;
    }

    intptr_t inOfs = OVR::OVR_strlen(inPath);
    for (;;) {
        uint32_t ch;
        if (!OVRFW::UTF8Util::DecodePrevChar(inPath, inOfs, ch)) {
            // invalid UTF-8 encoding
            assert(false);
            break;
        }
        if (ch == WIN_PATH_SEPARATOR || ch == NIX_PATH_SEPARATOR || ch == URI_PATH_SEPARATOR) {
            OVR::OVR_strncpy(outPath, outPathSize, inPath, inOfs + 1);
            return;
        }
    }
    // never hit a path separator so copy the entire thing
    OVR::OVR_strcpy(outPath, outPathSize, inPath);
}

static uint32_t DecodeNextChar(char const* p, intptr_t& offset) {
    char const* t = p + offset;
    uint32_t ch = OVRFW::UTF8Util::DecodeNextChar(&t);
    offset = t - p;
    return ch;
}

static bool EncodeChar(char* p, size_t const& maxOffset, intptr_t& offset, uint32_t ch) {
    // test for buffer overflow by encoding to a temp buffer and seeing how far the offset moved
    char temp[6];
    intptr_t tempOfs = 0;
    OVRFW::UTF8Util::EncodeChar(temp, &tempOfs, ch);
    if (static_cast<intptr_t>(maxOffset) - offset <= tempOfs) {
        // just encode a null byte at the current offset
        assert(false);
        p[offset] = '\0';
        offset++;
        return false;
    }
    OVRFW::UTF8Util::EncodeChar(p, &offset, ch);
    return true;
}

bool AppendUriPath(
    char const* inPath,
    char const* appendPath,
    char* outPath,
    size_t const outPathSize) {
    const char WIN_PATH_SEPARATOR = '\\';
    const char NIX_PATH_SEPARATOR = '/';
    const char URI_PATH_SEPARATOR = '/';

    if (inPath == NULL || outPath == NULL || appendPath == NULL || outPathSize < 2) {
        assert(inPath != NULL && outPath != NULL && appendPath != NULL && outPathSize > 1);
        return false;
    }
    intptr_t inOfs = 0;
    intptr_t outOfs = 0;
    uint32_t lastCh = 0xffffffff;
    uint32_t ch = 0xffffffff;
    for (;;) {
        lastCh = ch;
        ch = DecodeNextChar(inPath, inOfs);
        if (ch == '\0') {
            break;
        }
        if (!EncodeChar(outPath, outPathSize, outOfs, ch)) {
            return false;
        }
    }

    // ensure there's always a path separator after inPath
    if (lastCh != WIN_PATH_SEPARATOR && lastCh != NIX_PATH_SEPARATOR &&
        lastCh != URI_PATH_SEPARATOR) {
        // emit a separator
        if (!EncodeChar(outPath, outPathSize, outOfs, URI_PATH_SEPARATOR)) {
            return false; // buffer overflow
        }
    }

    // skip past any path separators at the start of append path
    intptr_t appendOfs = 0;
    char const* appendPathStart = &appendPath[0];
    for (;;) {
        ch = DecodeNextChar(appendPath, appendOfs);
        if (ch != WIN_PATH_SEPARATOR && ch != NIX_PATH_SEPARATOR && ch != URI_PATH_SEPARATOR) {
            break;
        }
        appendPathStart = appendPath + appendOfs;
    }

    appendOfs = 0;
    for (;;) {
        ch = DecodeNextChar(appendPathStart, appendOfs);
        if (!EncodeChar(outPath, outPathSize, outOfs, ch)) {
            return false;
        }
        if (ch == 0) {
            return true;
        }
    }
}

namespace OVRFW {

static char const* FontSingleTextureVertexShaderSrc = R"glsl(
	attribute vec4 Position;
	attribute vec2 TexCoord;
	attribute vec4 VertexColor;
	attribute vec4 FontParms;
	varying highp vec2 oTexCoord;
	varying lowp vec4 oColor;
	varying vec4 oFontParms;
	void main()
	{
	    gl_Position = TransformVertex( Position );
	    oTexCoord = TexCoord;
	    oColor = VertexColor;
	    oFontParms = FontParms;
	}
)glsl";

// Use derivatives to make the faded color and alpha boundaries a
// consistent thickness regardless of font scale.
static char const* SDFFontFragmentShaderSrc = R"glsl(
	uniform sampler2D Texture0;
	varying highp vec2 oTexCoord;
	varying lowp vec4 oColor;
	varying mediump vec4 oFontParms;
	void main()
	{
	    mediump float distance = texture2D( Texture0, oTexCoord ).r;
	    mediump float ds = oFontParms.z * 255.0;
		 mediump float dd = fwidth( oTexCoord.x ) * oFontParms.w * 10.0 * ds;
	    mediump float ALPHA_MIN = oFontParms.x - dd;
	    mediump float ALPHA_MAX = oFontParms.x + dd;
	    mediump float COLOR_MIN = oFontParms.y - dd;
	    mediump float COLOR_MAX = oFontParms.y + dd;
		 gl_FragColor.xyz = ( oColor * ( clamp( distance, COLOR_MIN, COLOR_MAX ) - COLOR_MIN ) / ( COLOR_MAX - COLOR_MIN ) ).xyz;
		 gl_FragColor.w = oColor.w * ( clamp( distance, ALPHA_MIN, ALPHA_MAX ) - ALPHA_MIN ) / ( ALPHA_MAX - ALPHA_MIN );
	}
)glsl";

class FontGlyphType {
   public:
    FontGlyphType()
        : CharCode(0),
          X(0.0f),
          Y(0.0f),
          Width(0.0f),
          Height(0.0f),
          AdvanceX(0.0f),
          AdvanceY(0.0f),
          BearingX(0.0f),
          BearingY(0.0f) {}

    int32_t CharCode;
    float X;
    float Y;
    float Width;
    float Height;
    float AdvanceX;
    float AdvanceY;
    float BearingX;
    float BearingY;
};

class ovrFontWeight {
   public:
    ovrFontWeight(float const alphaCenterOffset = 0.0f, float const colorCenterOffset = 0.0f)
        : AlphaCenterOffset(alphaCenterOffset), ColorCenterOffset(colorCenterOffset) {}

    float AlphaCenterOffset;
    float ColorCenterOffset;
};

class FontInfoType {
   public:
    static const int FNT_FILE_VERSION;

    // This is used to scale the UVs to world units that work with the current scale values used
    // throughout the native code. Unfortunately the original code didn't account for the image size
    // before factoring in the user scale, so this keeps everything the same.
    static const float DEFAULT_SCALE_FACTOR;

    FontInfoType()
        : NaturalWidth(0.0f),
          NaturalHeight(0.0f),
          HorizontalPad(0),
          VerticalPad(0),
          FontHeight(0),
          ScaleFactorX(1.0f),
          ScaleFactorY(1.0f),
          TweakScale(1.0f),
          CenterOffset(0.0f),
          MaxAscent(0.0f),
          MaxDescent(0.0f),
          EdgeWidth(32.0f) {}

    bool Load(ovrFileSys& fileSys, char const* uri);
    bool Save(char const* filename);

    FontGlyphType const& GlyphForCharCode(uint32_t const charCode) const;
    ovrFontWeight GetFontWeight(int const index) const;

    std::string FontName; // name of the font (not necessarily the file name)
    std::string CommandLine; // command line used to generate this font
    std::string ImageFileName; // the file name of the font image
    float NaturalWidth; // width of the font image before downsampling to SDF
    float NaturalHeight; // height of the font image before downsampling to SDF
    float HorizontalPad; // horizontal padding for all glyphs
    float VerticalPad; // vertical padding for all glyphs
    float FontHeight; // vertical distance between two baselines (i.e. two lines of text)
    float ScaleFactorX; // x-axis scale factor
    float ScaleFactorY; // y-axis scale factor
    float TweakScale; // additional scale factor used to tweak the size of other-language fonts
    float CenterOffset; // +/- value applied to "center" distance in the signed distance field.
                        // Range [-1,1]. A negative offset will make the font appear bolder.
    float MaxAscent; // maximum ascent of any character
    float MaxDescent; // maximum descent of any character
    float EdgeWidth; // adjust the edge falloff. Helps with fonts that have smaller glyph sizes in
                     // the texture (CJK)
    std::vector<FontGlyphType> Glyphs; // info about each glyph in the font
    std::vector<int32_t>
        CharCodeMap; // index by character code to get the index of a glyph for the character
    std::vector<ovrFontWeight> FontWeights;

   private:
    bool LoadFromBuffer(void const* buffer, size_t const bufferSize);
};

const int FontInfoType::FNT_FILE_VERSION =
    1; // initial version storing pixel locations and scaling post/load to fix some precision loss
// for now, we're not going to increment this so that we're less likely to have dependency issues
// with loading the font from Home const int FontInfoType::FNT_FILE_VERSION = 2;		// added
// TweakScale for manual adjustment of other-language fonts
const float FontInfoType::DEFAULT_SCALE_FACTOR = 512.0f;

class BitmapFontLocal : public BitmapFont {
   public:
    BitmapFontLocal() : FontTexture(), ImageWidth(0), ImageHeight(0) {}
    ~BitmapFontLocal() {
        FreeTexture(FontTexture);
        GlProgram::Free(FontProgram);
    }

    virtual bool Load(ovrFileSys& fileSys, const char* uri);

    // Calculates the native (unscaled) width of the text string. Line endings are ignored.
    virtual float CalcTextWidth(char const* text) const;

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
        int& numLines) const;

    virtual void TruncateText(std::string& inOutText, int const maxLines) const;

    bool WordWrapText(std::string& inOutText, const float widthMeters, const float fontScale = 1.0f)
        const;
    bool WordWrapText(
        std::string& inOutText,
        const float widthMeters,
        std::vector<std::string> wholeStrsList,
        const float fontScale = 1.0f) const;
    float GetLastFitChars(
        std::string& inOutText,
        const float widthMeters,
        const float fontScale = 1.0f) const;
    float GetFirstFitChars(
        std::string& inOutText,
        const float widthMeters,
        const int numLines,
        const float fontScale = 1.0f) const;

    // Returns a newly allocated GlGeometry for the text, allowing it to be sorted
    // or transformed with more control than the global BitmapFontSurface.
    //
    // The GlGeometry must be freed, but the GlProgram is shared by all users of the font.
    virtual ovrSurfaceDef TextSurface(
        const char* text,
        float scale,
        const Vector4f& color,
        HorizontalJustification hjust,
        VerticalJustification vjust,
        fontParms_t const* fontParms = nullptr) const;

    FontGlyphType const& GlyphForCharCode(uint32_t const charCode) const {
        return FontInfo.GlyphForCharCode(charCode);
    }
    virtual Vector2f GetScaleFactor() const {
        return Vector2f(FontInfo.ScaleFactorX, FontInfo.ScaleFactorY);
    }
    virtual void GetGlyphMetrics(
        const uint32_t charCode,
        float& width,
        float& height,
        float& advancex,
        float& advancey) const;

    FontInfoType const& GetFontInfo() const {
        return FontInfo;
    }
    const GlProgram& GetFontProgram() const {
        return FontProgram;
    }
    int GetImageWidth() const {
        return ImageWidth;
    }
    int GetImageHeight() const {
        return ImageHeight;
    }
    const GlTexture& GetFontTexture() const {
        return FontTexture;
    }

   private:
    FontInfoType FontInfo;
    GlTexture FontTexture;
    int ImageWidth;
    int ImageHeight;

    GlProgram FontProgram;

   private:
    bool LoadImage(ovrFileSys& fileSys, char const* uri);
    bool LoadImageFromBuffer(
        char const* imageName,
        std::vector<unsigned char>& buffer,
        bool const isASTC);
    bool LoadFontInfo(char const* glyphFileName);
    bool LoadFontInfoFromBuffer(unsigned char const* buffer, size_t const bufferSize);
};

// We cast BitmapFont to BitmapFontLocal internally so that we do not have to expose
// a lot of BitmapFontLocal methods in the BitmapFont interface just so BitmapFontSurfaceLocal
// can use them. This problem comes up because BitmapFontSurface specifies the interface as
// taking BitmapFont as a parameter, not BitmapFontLocal. This is safe right now because
// we know that BitmapFont cannot be instantiated, nor is there any class derived from it other
// than BitmapFontLocal.
static BitmapFontLocal const& AsLocal(BitmapFont const& font) {
    return *static_cast<BitmapFontLocal const*>(&font);
}

struct fontVertex_t {
    fontVertex_t() : xyz(0.0f), s(0.0f), t(0.0f), rgba(), fontParms() {}

    Vector3f xyz;
    float s;
    float t;
    std::uint8_t rgba[4];
    std::uint8_t fontParms[4];
};

typedef unsigned short fontIndex_t;

//==============================
// ftoi
#if defined(OVR_CPU_X86_64)
#include <xmmintrin.h>
inline int ftoi(float const f) {
    return _mm_cvtt_ss2si(_mm_set_ss(f));
}
#elif defined(OVR_CPU_x86)
inline int ftoi(float const f) {
    int i;
    __asm
    {
		fld f
		fistp i
    }
    return i;
}
#else
inline int ftoi(float const f) {
    return (int)f;
}
#endif

//==============================
// ColorToABGR
int32_t ColorToABGR(Vector4f const& color) {
    // format is ABGR
    return (ftoi(color.w * 255.0f) << 24) | (ftoi(color.z * 255.0f) << 16) |
        (ftoi(color.y * 255.0f) << 8) | ftoi(color.x * 255.0f);
}

static bool IsHexDigit(char const ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

static bool DigitToHex(char const ch, uint32_t& hex) {
    if (ch >= 'A' && ch <= 'F') {
        hex = ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
        hex = ch - 'a' + 10;
    } else if (ch >= '0' && ch <= '9') {
        hex = ch - '0';
    } else {
        hex = 0;
        return false;
    }
    return true;
}

static bool CheckForColorEscape(char const** buffer, uint32_t& color) {
    // Must be exactly 10 chars "~~XXXXXXXX" where X's are ABGR in hex
    OVR_ASSERT(buffer != nullptr && *buffer != nullptr);

    char const* ptr = *buffer;
    if (ptr[0] != '~') {
        return false;
    }
    if (ptr[1] != '~') {
        return false;
    }
    color = 0;
    for (int i = 2; i < 10; ++i) {
        uint32_t hex;
        if (!DigitToHex(ptr[i], hex)) {
            return false;
        }

        color |= hex;
        color <<= 4;
    }
    *buffer += 10;
    return true;
}

static bool CheckForFormatEscape(char const** buffer, uint32_t& color, uint32_t& weight) {
    OVR_ASSERT(buffer != nullptr && *buffer != nullptr);
    char const* ptr = *buffer;
    if (ptr[0] == '\0' || ptr[1] == '\0' || ptr[2] == '\0') {
        return false;
    } else if (ptr[0] != '~' || ptr[1] != '~') {
        return false;
    }
    // if the character after ~~ is a hex digit, this is a color
    else if (IsHexDigit(ptr[2])) {
        return CheckForColorEscape(buffer, color);
    } else if (ptr[2] == 'w' || ptr[2] == 'W') {
        // a single-digit weight command should follow
        if (!DigitToHex(ptr[3], weight)) {
            return false;
        }
        *buffer += 4;
        return true;
    }
    return false; // if we got here we don't have a valid character after ~~ so return false.
}

// The vertices in a vertex block are in local space and pre-scaled.  They are transformed into
// world space and stuffed into the VBO before rendering (once the current MVP is known).
// The vertices can be pivoted around the Pivot point to face the camera, then an additional
// rotation applied.
class VertexBlockType {
   public:
    VertexBlockType()
        : Font(NULL),
          Verts(NULL),
          NumVerts(0),
          Pivot(0.0f),
          Rotation(),
          Billboard(true),
          TrackRoll(false) {}

    VertexBlockType(VertexBlockType const& other)
        : Font(NULL),
          Verts(NULL),
          NumVerts(0),
          Pivot(0.0f),
          Rotation(),
          Billboard(true),
          TrackRoll(false) {
        Copy(other);
    }

    VertexBlockType& operator=(VertexBlockType const& other) {
        Copy(other);
        return *this;
    }

    void Copy(VertexBlockType const& other) {
        if (&other == this) {
            return;
        }
        delete[] Verts;
        Font = other.Font;
        Verts = other.Verts;
        NumVerts = other.NumVerts;
        Pivot = other.Pivot;
        Rotation = other.Rotation;
        Billboard = other.Billboard;
        TrackRoll = other.TrackRoll;

        other.Font = NULL;
        other.Verts = NULL;
        other.NumVerts = 0;
    }

    VertexBlockType(
        BitmapFont const& font,
        int const numVerts,
        Vector3f const& pivot,
        Quatf const& rot,
        bool const billboard,
        bool const trackRoll)
        : Font(&font),
          NumVerts(numVerts),
          Pivot(pivot),
          Rotation(rot),
          Billboard(billboard),
          TrackRoll(trackRoll) {
        Verts = new fontVertex_t[numVerts];
    }

    ~VertexBlockType() {
        Free();
    }

    void Free() {
        Font = NULL;
        delete[] Verts;
        Verts = NULL;
        NumVerts = 0;
    }

    mutable BitmapFont const* Font; // the font used to render text into this vertex block
    mutable fontVertex_t* Verts; // the vertices
    mutable int NumVerts; // the number of vertices in the block
    Vector3f Pivot; // postion this vertex block can be rotated around
    Quatf Rotation; // additional rotation to apply
    bool Billboard; // true to always face the camera
    bool TrackRoll; // if true, when billboarded, roll with the camera
};

// Sets up VB and VAO for font drawing
GlGeometry FontGeometry(int maxQuads, Bounds3f& localBounds) {
    GlGeometry Geo;

    Geo.indexCount = maxQuads * 6;
    Geo.vertexCount = maxQuads * 4;

    Geo.localBounds = localBounds;

    // font VAO
    glGenVertexArrays(1, &Geo.vertexArrayObject);
    glBindVertexArray(Geo.vertexArrayObject);

    // vertex buffer
    const int vertexByteCount = Geo.vertexCount * sizeof(fontVertex_t);
    glGenBuffers(1, &Geo.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Geo.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexByteCount, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_POSITION); // x, y and z
    glVertexAttribPointer(
        VERTEX_ATTRIBUTE_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(fontVertex_t), (void*)0);

    glEnableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_UV0); // s and t
    glVertexAttribPointer(
        VERTEX_ATTRIBUTE_LOCATION_UV0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(fontVertex_t),
        (void*)offsetof(fontVertex_t, s));

    glEnableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_COLOR); // color
    glVertexAttribPointer(
        VERTEX_ATTRIBUTE_LOCATION_COLOR,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(fontVertex_t),
        (void*)offsetof(fontVertex_t, rgba));

    glDisableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_UV1);

    glEnableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS); // outline parms
    glVertexAttribPointer(
        VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(fontVertex_t),
        (void*)offsetof(fontVertex_t, fontParms));

    fontIndex_t* indices = new fontIndex_t[Geo.indexCount];
    const int indexByteCount = Geo.indexCount * sizeof(fontIndex_t);

    // indices never change
    fontIndex_t v = 0;
    for (int i = 0; i < maxQuads; i++) {
        indices[i * 6 + 0] = v + 2;
        indices[i * 6 + 1] = v + 1;
        indices[i * 6 + 2] = v + 0;
        indices[i * 6 + 3] = v + 3;
        indices[i * 6 + 4] = v + 2;
        indices[i * 6 + 5] = v + 0;
        v += 4;
    }

    glGenBuffers(1, &Geo.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Geo.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexByteCount, (void*)indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    delete[] indices;

    return Geo;
}

struct ovrFormat {
    ovrFormat(uint32_t const color) : Color(color), Weight(0xffffffff), LastWeight(0xffffffff) {}

    uint32_t Color;
    uint32_t Weight;
    uint32_t LastWeight;
};

static void UpdateFormat(
    FontInfoType const& fontInfo,
    fontParms_t const& fontParms,
    char const** buffer,
    ovrFormat& format,
    uint8_t vertexParms[4]) {
    while (CheckForFormatEscape(buffer, format.Color, format.Weight))
        ;
    if (format.Weight != format.LastWeight && format.Weight != 0xffffffff) {
        ovrFontWeight const& w = fontInfo.GetFontWeight(format.Weight);
        vertexParms[1] = (uint8_t)(
            clamp<float>(
                fontParms.ColorCenter + fontInfo.CenterOffset + w.ColorCenterOffset, 0.0f, 1.0f) *
            255);
        vertexParms[0] = (uint8_t)(
            clamp<float>(
                fontParms.AlphaCenter + fontInfo.CenterOffset + w.AlphaCenterOffset, 0.0f, 1.0f) *
            255);
    }
}

//==============================
// DrawText3D
VertexBlockType DrawTextToVertexBlock(
    BitmapFont const& font,
    fontParms_t const& fontParms,
    Vector3f const& pos,
    Vector3f const& normal,
    Vector3f const& up,
    float scale,
    Vector4f const& color,
    char const* text,
    Vector3f* toNextLine = nullptr) {
    if (toNextLine) {
        *toNextLine = Vector3f::ZERO;
    }
    if (text == NULL || text[0] == '\0') {
#if defined(OVR_BUILD_DEBUG)
        ALOG("DrawTextToVertexBlock: null or empty text!");
#endif
        return VertexBlockType(); // nothing to do here, move along
    }

    // TODO: multiple line support -- we would need to calculate the horizontal width
    // for each string ending in \n
    size_t len;
    float width;
    float height;
    float ascent;
    float descent;
    float fontHeight;
    int const MAX_LINES = 128;
    float lineWidths[MAX_LINES];
    int numLines;
    AsLocal(font).CalcTextMetrics(
        text, len, width, height, ascent, descent, fontHeight, lineWidths, MAX_LINES, numLines);

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "BitmapFontSurfaceLocal::DrawText3D( \"%s\" %s %s ) : width = %.2f, height = %.2f,
/// numLines = %i, fh = %.2f", 			text, 			( fontParms.AlignVert == VERTICAL_CENTER ) ?
/// "cv" : ( ( fontParms.AlignVert == VERTICAL_CENTER_FIXEDHEIGHT ) ? "cvfh" : "tv" ),
/// 			( fontParms.AlignHoriz == HORIZONTAL_CENTER ) ? "ch" : ( ( fontParms.AlignHoriz ==
/// HORIZONTAL_LEFT ) ? "lh" : "rh" ),
///			width, height, numLines, AsLocal( font ).GetFontInfo().FontHeight );
#endif

    if (len == 0) {
#if defined(OVR_BUILD_DEBUG)
        ALOG("DrawTextToVertexBlock: zero-length text after metrics!");
#endif
        return VertexBlockType();
    }

    if (!normal.IsNormalized()) {
        ALOG(
            "DrawTextToVertexBlock: normal = ( %g, %g, %g ), text = '%s'",
            normal.x,
            normal.y,
            normal.z,
            text);
        OVR_ASSERT_WITH_TAG(normal.IsNormalized(), "BitmapFont");
    }
    if (!up.IsNormalized()) {
        ALOG("DrawTextToVertexBlock: up = ( %g, %g, %g ), text = '%s'", up.x, up.y, up.z, text);
        OVR_ASSERT_WITH_TAG(up.IsNormalized(), "BitmapFont");
    }

    const FontInfoType& fontInfo = AsLocal(font).GetFontInfo();

    float imageWidth = (float)AsLocal(font).GetImageWidth();
    float const xScale = AsLocal(font).GetFontInfo().ScaleFactorX * scale;
    float const yScale = AsLocal(font).GetFontInfo().ScaleFactorY * scale;

    // allocate a vertex block
    const int numVerts = 4 * static_cast<int>(len);
    VertexBlockType vb(font, numVerts, pos, Quatf(), fontParms.Billboard, fontParms.TrackRoll);

    Vector3f const right = up.Cross(normal);
    Vector3f const r = (fontParms.Billboard) ? Vector3f(1.0f, 0.0f, 0.0f) : right;
    Vector3f const u = (fontParms.Billboard) ? Vector3f(0.0f, 1.0f, 0.0f) : up;

    Vector3f curPos(0.0f);
    switch (fontParms.AlignVert) {
        case VERTICAL_BASELINE:
            break;

        case VERTICAL_CENTER: {
            float const vofs = (height * 0.5f) - ascent;
            curPos += u * (vofs * scale);
            break;
        }
        case VERTICAL_CENTER_FIXEDHEIGHT: {
            // for fixed height, we must adjust single-line text by the max ascent because fonts
            // are rendered according to their baseline. For multiline text, the first line
            // contributes max ascent only while the other lines are adjusted by font height.
            float const ma = AsLocal(font).GetFontInfo().MaxAscent;
            float const md = AsLocal(font).GetFontInfo().MaxDescent;
            float const fh = AsLocal(font).GetFontInfo().FontHeight;
            float const adjust = (ma - md) * 0.5f;
            float const vofs = (fh * (numLines - 1) * 0.5f) - adjust;
            curPos += u * (vofs * yScale);
            break;
        }
        case VERTICAL_TOP: {
            float const vofs = height - ascent;
            curPos += u * (vofs * scale);
            break;
        }
    }

    Vector3f basePos = curPos;
    switch (fontParms.AlignHoriz) {
        case HORIZONTAL_LEFT:
            break;

        case HORIZONTAL_CENTER: {
            curPos -= r * (lineWidths[0] * 0.5f * scale);
            break;
        }
        case HORIZONTAL_RIGHT: {
            curPos -= r * (lineWidths[0] * scale);
            break;
        }
    }

    Vector3f lineInc = u * (fontInfo.FontHeight * yScale);
    float const distanceScale = imageWidth / FontInfoType::DEFAULT_SCALE_FACTOR;
    float const imageScaleFactor = 1024.0f / imageWidth;

    float const weightOffset = 0.0f;
    float const edgeWidth = fontInfo.EdgeWidth * imageScaleFactor;

    uint8_t vertexParms[4] = {
        (uint8_t)(
            clamp<float>(fontParms.AlphaCenter + fontInfo.CenterOffset + weightOffset, 0.0f, 1.0f) *
            255),
        (uint8_t)(
            clamp<float>(fontParms.ColorCenter + fontInfo.CenterOffset + weightOffset, 0.0f, 1.0f) *
            255),
        //(uint8_t)( 0 ),
        (uint8_t)(clamp<float>(distanceScale, 1.0f, 255.0f)),
        (uint8_t)(clamp<float>(edgeWidth / 16.0f, 0.0f, 1.0f) * 255.0f)};

    ovrFormat format(ColorToABGR(color));

    int curLine = 0;
    fontVertex_t* v = vb.Verts;
    char const* p = text;
    size_t i = 0;

    UpdateFormat(fontInfo, fontParms, &p, format, vertexParms);
    uint32_t charCode = UTF8Util::DecodeNextChar(&p);

    for (; charCode != '\0'; i++, charCode = UTF8Util::DecodeNextChar(&p)) {
        OVR_ASSERT(i < len);
        if (charCode == '\n' && curLine < numLines && curLine < MAX_LINES) {
            // move to next line
            curLine++;
            basePos -= lineInc;
            if (toNextLine) {
                *toNextLine -= lineInc;
            }
            curPos = basePos;
            switch (fontParms.AlignHoriz) {
                case HORIZONTAL_LEFT:
                    break;

                case HORIZONTAL_CENTER: {
                    curPos -= r * (lineWidths[curLine] * 0.5f * scale);
                    break;
                }
                case HORIZONTAL_RIGHT: {
                    curPos -= r * (lineWidths[curLine] * scale);
                    break;
                }
            }
        }

        FontGlyphType const& g = AsLocal(font).GlyphForCharCode(charCode);

        float s0 = g.X;
        float t0 = g.Y;
        float s1 = (g.X + g.Width);
        float t1 = (g.Y + g.Height);

        float bearingX = g.BearingX * xScale;
        float bearingY = g.BearingY * yScale;

        float rw = (g.Width + g.BearingX) * xScale;
        float rh = (g.Height - g.BearingY) * yScale;

        // lower left
        v[i * 4 + 0].xyz = curPos + (r * bearingX) - (u * rh);
        v[i * 4 + 0].s = s0;
        v[i * 4 + 0].t = t1;
        *(std::uint32_t*)(&v[i * 4 + 0].rgba[0]) = format.Color;
        *(std::uint32_t*)(&v[i * 4 + 0].fontParms[0]) = *(std::uint32_t*)(&vertexParms[0]);
        // upper left
        v[i * 4 + 1].xyz = curPos + (r * bearingX) + (u * bearingY);
        v[i * 4 + 1].s = s0;
        v[i * 4 + 1].t = t0;
        *(std::uint32_t*)(&v[i * 4 + 1].rgba[0]) = format.Color;
        *(std::uint32_t*)(&v[i * 4 + 1].fontParms[0]) = *(std::uint32_t*)(&vertexParms[0]);
        // upper right
        v[i * 4 + 2].xyz = curPos + (r * rw) + (u * bearingY);
        v[i * 4 + 2].s = s1;
        v[i * 4 + 2].t = t0;
        *(std::uint32_t*)(&v[i * 4 + 2].rgba[0]) = format.Color;
        *(std::uint32_t*)(&v[i * 4 + 2].fontParms[0]) = *(std::uint32_t*)(&vertexParms[0]);
        // lower right
        v[i * 4 + 3].xyz = curPos + (r * rw) - (u * rh);
        v[i * 4 + 3].s = s1;
        v[i * 4 + 3].t = t1;
        *(std::uint32_t*)(&v[i * 4 + 3].rgba[0]) = format.Color;
        *(std::uint32_t*)(&v[i * 4 + 3].fontParms[0]) = *(std::uint32_t*)(&vertexParms[0]);
        // advance to start of next char
        curPos += r * (g.AdvanceX * xScale);

        UpdateFormat(fontInfo, fontParms, &p, format, vertexParms);
    }

    if (toNextLine) {
        *toNextLine -= lineInc;
    }

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "DrawTextToVertexBlock: drawn %d vertices lineInc = ", vb.NumVerts );
#endif

    return vb;
}

ovrFontWeight FontInfoType::GetFontWeight(const int index) const {
    if (index < 0 || static_cast<const size_t>(index) >= FontWeights.size()) {
        return ovrFontWeight();
    }
    return FontWeights[index];
}

ovrSurfaceDef BitmapFontLocal::TextSurface(
    const char* text,
    float scale,
    const Vector4f& color,
    HorizontalJustification hjust,
    VerticalJustification vjust,
    fontParms_t const* fontParms) const {
    fontParms_t fp;
    if (fontParms != nullptr) {
        fp = *fontParms;
    } else {
        fp.AlignHoriz = hjust;
        fp.AlignVert = vjust;
    }
    VertexBlockType vb = DrawTextToVertexBlock(
        *this,
        fp,
        Vector3f(0.0f), // origin
        Vector3f(0.0f, 0.0f, 1.0f), // normal
        Vector3f(0.0f, 1.0f, 0.0f), // up
        scale,
        color,
        text);

    Bounds3f blockBounds(Bounds3f::Init);
    for (int i = 0; i < vb.NumVerts; i++) {
        blockBounds.AddPoint(vb.Verts[i].xyz);
    }

    ovrSurfaceDef s;
    s.geo = FontGeometry(vb.NumVerts / 4, blockBounds);

    glBindVertexArray(s.geo.vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, s.geo.vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vb.NumVerts * sizeof(fontVertex_t), (void*)vb.Verts);
    glBindVertexArray(0);

    vb.Free();

    // for now we set up both the gpu state, program, and uniformdata.

    // Special blend mode to also work over underlay layers
    s.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE_SEPARATE;
    s.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
    s.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    s.graphicsCommand.GpuState.blendSrcAlpha = GL_ONE;
    s.graphicsCommand.GpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
    s.graphicsCommand.GpuState.depthMaskEnable = false;

    s.graphicsCommand.Program = FontProgram;
    s.graphicsCommand.UniformData[0].Data = (void*)&FontTexture;

    s.surfaceName = text;
    return s;
}

//==================================================================================================
// BitmapFontSurfaceLocal
//
class BitmapFontSurfaceLocal : public BitmapFontSurface {
   public:
    BitmapFontSurfaceLocal();
    virtual ~BitmapFontSurfaceLocal();

    virtual void Init(const int maxVertices);
    void Free();

    // add text to the VBO that will render in a 2D pass.
    virtual Vector3f DrawText3D(
        BitmapFont const& font,
        const fontParms_t& flags,
        const Vector3f& pos,
        Vector3f const& normal,
        Vector3f const& up,
        float const scale,
        Vector4f const& color,
        char const* text);
    virtual Vector3f DrawText3Df(
        BitmapFont const& font,
        const fontParms_t& flags,
        const Vector3f& pos,
        Vector3f const& normal,
        Vector3f const& up,
        float const scale,
        Vector4f const& color,
        char const* text,
        ...);

    virtual Vector3f DrawTextBillboarded3D(
        BitmapFont const& font,
        fontParms_t const& flags,
        Vector3f const& pos,
        float const scale,
        Vector4f const& color,
        char const* text);
    virtual Vector3f DrawTextBillboarded3Df(
        BitmapFont const& font,
        fontParms_t const& flags,
        Vector3f const& pos,
        float const scale,
        Vector4f const& color,
        char const* fmt,
        ...);

    // transform the billboarded font strings
    virtual void Finish(Matrix4f const& viewMatrix);

    // add the VBO to the surface render list
    virtual void AppendSurfaceList(BitmapFont const& font, std::vector<ovrDrawSurface>& surfaceList)
        const;

    virtual bool IsInitialized() const {
        return Initialized;
    }

    virtual void SetCullEnabled(const bool enabled);

   private:
    // This limitation may not exist anymore now that ModelMatrix is no longer a member.
    BitmapFontSurfaceLocal& operator=(BitmapFontSurfaceLocal const& rhs);

    mutable ovrSurfaceDef FontSurfaceDef;

    fontVertex_t* Vertices; // vertices that are written to the VBO
    int MaxVertices;
    int MaxIndices;
    int CurVertex; // reset every Render()
    int CurIndex; // reset every Render()
    bool Initialized;

    std::vector<VertexBlockType>
        VertexBlocks; // each pointer in the array points to an allocated block ov
};

//==================================================================================================
// FontInfoType
//==================================================================================================

//==============================
// FontInfoType::Load
bool FontInfoType::Load(ovrFileSys& fileSys, char const* uri) {
    std::vector<uint8_t> buffer;
    if (!fileSys.ReadFile(uri, buffer)) {
        ALOG("FontInfoType::Load ReadFile FAIL uri = '%s' ", uri);
        return false;
    }
    ALOG("FontInfoType::Load ReadFile OK buffer size = '%d' ", (int)buffer.size());

    bool result = LoadFromBuffer(buffer.data(), buffer.size());
    // enable the block below to rewrite the font
    /*
        if ( result )
        {
            Save( "c:/temp/" );
        }
    */

    ALOG("FontInfoType::Load ReadFile DONE result = '%d' ", (int)result);
    return result;
}

//==============================
// FontInfoType::LoadFromBuffer
bool FontInfoType::LoadFromBuffer(void const* buffer, size_t const bufferSize) {
    char const* errorMsg = NULL;
    std::shared_ptr<OVR::JSON> jsonRoot =
        OVR::JSON::Parse(reinterpret_cast<char const*>(buffer), &errorMsg);
    if (jsonRoot == NULL) {
        OVR_WARN("OVR::JSON Error: %s", (errorMsg != NULL) ? errorMsg : "<NULL>");
        ALOG(
            "FontInfoType::LoadFromBuffer FAIL OVR::JSON ERROR = '%s' ",
            (errorMsg != NULL) ? errorMsg : "<NULL>");
        return false;
    }

    int32_t maxCharCode = -1;
    // currently we're only supporting the first unicode plane up to 65k. If we were to support
    // other planes we could conceivably end up with a very sparse 1,114,111 byte type for mapping
    // character codes to glyphs and if that's the case we may just want to use a hash, or use a
    // combination of tables for the first 65K and hashes for the other, less-frequently-used
    // characters.
    static const int MAX_GLYPHS = 0xffff;

    // load the glyphs
    const OVR::JsonReader jsonGlyphs(jsonRoot);
    if (!jsonGlyphs.IsObject()) {
        ALOG("FontInfoType::LoadFromBuffer FAIL ==> jsonGlyphs.IsObject() = FALSE ");
        return false;
    }

    // OVR::JSON doesn't have ints so cast from float to an int
    int Version = static_cast<int>(jsonGlyphs.GetChildFloatByName("Version"));
    if (Version != FNT_FILE_VERSION) {
        ALOG("FontInfoType::LoadFromBuffer FAIL ==> Version != FNT_FILE_VERSION ");
        return false;
    }

    FontName = jsonGlyphs.GetChildStringByName("FontName");
    CommandLine = jsonGlyphs.GetChildStringByName("CommandLine");
    ImageFileName = jsonGlyphs.GetChildStringByName("ImageFileName");
    const int numGlyphs = jsonGlyphs.GetChildInt32ByName("NumGlyphs");
    if (numGlyphs < 0 || numGlyphs > MAX_GLYPHS) {
        OVR_ASSERT(numGlyphs > 0 && numGlyphs <= MAX_GLYPHS);
        ALOG("FontInfoType::LoadFromBuffer FAIL ==> numGlyphs < 0 || numGlyphs > MAX_GLYPHS ");
        return false;
    }

    NaturalWidth = jsonGlyphs.GetChildFloatByName("NaturalWidth");
    NaturalHeight = jsonGlyphs.GetChildFloatByName("NaturalHeight");

    // we scale everything after loading integer values from the OVR::JSON file because the OVR
    // OVR::JSON writer loses precision on floats
    float nwScale = 1.0f / NaturalWidth;
    float nhScale = 1.0f / NaturalHeight;

    HorizontalPad = jsonGlyphs.GetChildFloatByName("HorizontalPad") * nwScale;
    VerticalPad = jsonGlyphs.GetChildFloatByName("VerticalPad") * nhScale;
    FontHeight = jsonGlyphs.GetChildFloatByName("FontHeight") * nhScale;
    CenterOffset = jsonGlyphs.GetChildFloatByName("CenterOffset");
    TweakScale = jsonGlyphs.GetChildFloatByName("TweakScale", 1.0f);
    EdgeWidth = jsonGlyphs.GetChildFloatByName("EdgeWidth", 32.0f);

#if defined(OVR_BUILD_DEBUG)
    ALOG("FontName = %s", FontName.c_str());
    ALOG("CommandLine = %s", CommandLine.c_str());
    ALOG("HorizontalPad = %.4f", HorizontalPad);
    ALOG("VerticalPad = %.4f", VerticalPad);
    ALOG("FontHeight = %.4f", FontHeight);
    ALOG("CenterOffset = %.4f", CenterOffset);
    ALOG("TweakScale = %.4f", TweakScale);
    ALOG("EdgeWidth = %.4f", EdgeWidth);
    ALOG("ImageFileName = %s", ImageFileName.c_str());
    ALOG("Loading %i glyphs.", numGlyphs);
#endif

    /// HACK: this is hard-coded until we do not have a dependcy on reading the font from Home
    /// TODO: I believe this can now be removed.
    if (OVR::OVR_stricmp(FontName.c_str(), "korean.fnt") == 0) {
        TweakScale = 0.75f;
        CenterOffset = -0.02f;
    }
    /// HACK: end hack

    const OVR::JsonReader jsonWeightArray(jsonGlyphs.GetChildByName("Weights"));
    if (jsonWeightArray.IsValid()) {
#if defined(OVR_BUILD_DEBUG)
        ALOG("jsonWeightArray valid");
#endif

        for (int i = 0; !jsonWeightArray.IsEndOfArray(); ++i) {
            const OVR::JsonReader jsonWeight(jsonWeightArray.GetNextArrayElement());
            if (jsonWeight.IsObject()) {
                ovrFontWeight w;
                w.AlphaCenterOffset = jsonWeight.GetChildFloatByName("AlphaCenterOffset");
                w.ColorCenterOffset = jsonWeight.GetChildFloatByName("ColorCenterOffset");
                FontWeights.push_back(w);
#if defined(OVR_BUILD_DEBUG)
///				ALOG( "FontWeights[%d] --> AlphaCenterOffset=%.3f ColorCenterOffset=%.3f", i,
/// w.AlphaCenterOffset, w.ColorCenterOffset );
#endif
            }
        }
    }

#if defined(OVR_BUILD_DEBUG)
    ALOG("jsonWeightArray DONE");
#endif

    Glyphs.resize(numGlyphs);
    const OVR::JsonReader jsonGlyphArray(jsonGlyphs.GetChildByName("Glyphs"));

    double oWidth = 0.0;
    double oHeight = 0.0;

    if (jsonGlyphArray.IsArray()) {
        for (int i = 0; i < static_cast<int>(Glyphs.size()) && !jsonGlyphArray.IsEndOfArray();
             i++) {
            const OVR::JsonReader jsonGlyph(jsonGlyphArray.GetNextArrayElement());
            if (jsonGlyph.IsObject()) {
                FontGlyphType& g = Glyphs[i];
                g.CharCode = jsonGlyph.GetChildInt32ByName("CharCode");
                g.X = jsonGlyph.GetChildFloatByName("X");
                g.Y = jsonGlyph.GetChildFloatByName("Y");
                g.Width = jsonGlyph.GetChildFloatByName("Width");
                g.Height = jsonGlyph.GetChildFloatByName("Height");
                g.AdvanceX = jsonGlyph.GetChildFloatByName("AdvanceX");
                g.AdvanceY = jsonGlyph.GetChildFloatByName("AdvanceY");
                g.BearingX = jsonGlyph.GetChildFloatByName("BearingX");
                g.BearingY = jsonGlyph.GetChildFloatByName("BearingY");

                if (g.CharCode == 'O') {
                    oWidth = g.Width;
                    oHeight = g.Height;
                }

                g.X *= nwScale;
                g.Y *= nhScale;
                g.Width *= nwScale;
                g.Height *= nhScale;
                g.AdvanceX *= nwScale;
                g.AdvanceY *= nhScale;
                g.BearingX *= nwScale;
                g.BearingY *= nhScale;

                float const ascent = g.BearingY;
                float const descent = g.Height - g.BearingY;
                if (ascent > MaxAscent) {
                    MaxAscent = ascent;
                }
                if (descent > MaxDescent) {
                    MaxDescent = descent;
                }

#if defined(OVR_BUILD_DEBUG)
///				ALOG( "Glyphs[%d] --> X=%.3f Y=%.3f CharCode=%d", i, g.X, g.Y, g.CharCode );
#endif

                maxCharCode = std::max<int32_t>(maxCharCode, g.CharCode);
            }
        }
    }

#if defined(OVR_BUILD_DEBUG)
    ALOG("jsonGlyphArray DONE maxCharCode =%d", maxCharCode);
#endif

    float const DEFAULT_TEXT_SCALE = 0.0025f;

    double const NATURAL_WIDTH_SCALE = NaturalWidth / 4096.0;
    double const NATURAL_HEIGHT_SCALE = NaturalHeight / 3820.0;
    double const DEFAULT_O_WIDTH = 325.0;
    double const DEFAULT_O_HEIGHT = 322.0;
    double const OLD_WIDTH_FACTOR = 1.04240608;
    float const widthScaleFactor =
        static_cast<float>(DEFAULT_O_WIDTH / oWidth * OLD_WIDTH_FACTOR * NATURAL_WIDTH_SCALE);
    float const heightScaleFactor =
        static_cast<float>(DEFAULT_O_HEIGHT / oHeight * OLD_WIDTH_FACTOR * NATURAL_HEIGHT_SCALE);

    ScaleFactorX = DEFAULT_SCALE_FACTOR * DEFAULT_TEXT_SCALE * widthScaleFactor * TweakScale;
    ScaleFactorY = DEFAULT_SCALE_FACTOR * DEFAULT_TEXT_SCALE * heightScaleFactor * TweakScale;

    // This is not intended for wide or ucf character sets -- depending on the size range of
    // character codes lookups may need to be changed to use a hash.
    if (maxCharCode >= MAX_GLYPHS) {
        OVR_ASSERT(maxCharCode <= MAX_GLYPHS);
        maxCharCode = MAX_GLYPHS;
    }

    // resize the array to the maximum glyph value
    CharCodeMap.resize(maxCharCode + 1);

    // init to empty value
    CharCodeMap.assign(CharCodeMap.size(), -1);

    for (int i = 0; i < static_cast<int>(Glyphs.size()); ++i) {
        FontGlyphType const& g = Glyphs[i];
        CharCodeMap[g.CharCode] = i;
    }

    ALOG("FontInfoType load SUCCESS");
    return true;
}

class ovrGlyphSort {
   public:
    void SortGlyphIndicesByCharacterCode(
        std::vector<FontGlyphType> const& glyphs,
        std::vector<int>& glyphIndices) {
        Glyphs = &glyphs;
        qsort(glyphIndices.data(), glyphIndices.size(), sizeof(int), CompareByCharacterCode);
        Glyphs = nullptr;
    }

   private:
    static int CompareByCharacterCode(const void* a, const void* b) {
        int const aIndex = *(int*)a;
        int const bIndex = *(int*)b;
        FontGlyphType const& glyphA = (*Glyphs)[aIndex];
        FontGlyphType const& glyphB = (*Glyphs)[bIndex];
        if (glyphA.CharCode < glyphB.CharCode) {
            return -1;
        } else if (glyphA.CharCode > glyphB.CharCode) {
            return 1;
        }
        return 0;
    }

    static std::vector<FontGlyphType> const* Glyphs;
};

std::vector<FontGlyphType> const* ovrGlyphSort::Glyphs = nullptr;

bool FontInfoType::Save(char const* path) {
    std::shared_ptr<OVR::JSON> joFont = OVR::JSON::CreateObject();
    joFont->AddStringItem("FontName", FontName.c_str());
    joFont->AddStringItem("CommandLine", CommandLine.c_str());
    joFont->AddNumberItem("Version", FNT_FILE_VERSION);
    joFont->AddStringItem("ImageFileName", ImageFileName.c_str());
    joFont->AddNumberItem("NaturalWidth", NaturalWidth);
    joFont->AddNumberItem("NaturalHeight", NaturalHeight);
    joFont->AddNumberItem("HorizontalPad", HorizontalPad);
    joFont->AddNumberItem("VerticalPad", VerticalPad);
    joFont->AddNumberItem("FontHeight", FontHeight);
    joFont->AddNumberItem("CenterOffset", CenterOffset);
    joFont->AddNumberItem("TweakScale", TweakScale);
    joFont->AddNumberItem("EdgeWidth", EdgeWidth);
    joFont->AddNumberItem("NumGlyphs", Glyphs.size());

    std::vector<int> glyphIndices;
    glyphIndices.resize(Glyphs.size());
    for (int i = 0; i < static_cast<int>(Glyphs.size()); ++i) {
        glyphIndices[i] = i;
    }

    ovrGlyphSort sort;
    sort.SortGlyphIndicesByCharacterCode(Glyphs, glyphIndices);

    std::shared_ptr<OVR::JSON> joGlyphsArray = OVR::JSON::CreateArray();
    joFont->AddItem("Glyphs", joGlyphsArray);
    // add all glyphs
    for (const int& index : glyphIndices) {
        FontGlyphType const& g = Glyphs[index];

        std::shared_ptr<OVR::JSON> joGlyph = OVR::JSON::CreateObject();
        joGlyph->AddNumberItem("CharCode", g.CharCode);
        joGlyph->AddNumberItem(
            "X", g.X); // now in natural units because the OVR::JSON writer loses precision
        joGlyph->AddNumberItem(
            "Y", g.Y); // now in natural units because the OVR::JSON writer loses precision
        joGlyph->AddNumberItem("Width", g.Width);
        joGlyph->AddNumberItem("Height", g.Height);
        joGlyph->AddNumberItem("AdvanceX", g.AdvanceX);
        joGlyph->AddNumberItem("AdvanceY", g.AdvanceY);
        joGlyph->AddNumberItem("BearingX", g.BearingX);
        joGlyph->AddNumberItem("BearingY", g.BearingY);
        joGlyphsArray->AddArrayElement(joGlyph);
    }

    char filePath[1024];
    AppendUriPath(path, FontName.c_str(), filePath, sizeof(filePath));
    ALOG("Writing font file '%s'\n...", filePath);
    joFont->Save(filePath);

    return true;
}

//==============================
// FontInfoType::GlyphForCharCode
FontGlyphType const& FontInfoType::GlyphForCharCode(uint32_t const charCode) const {
    auto lookupGlyph = [this](uint32_t const ch) {
        return ch >= CharCodeMap.size() ? -1 : CharCodeMap[ch];
    };

    int glyphIndex =
        lookupGlyph(charCode); // charCode >= CharCodeMap.size() ? -1 : CharCodeMap[charCode];
    if (glyphIndex < 0 || glyphIndex >= static_cast<int>(Glyphs.size())) {
#if defined(OVR_BUILD_DEBUG)
        OVR_WARN(
            "FontInfoType::GlyphForCharCode FAILED TO FIND GLYPH FOR CHARACTER! charCode %u => %i [mapsize=%zu] [glyphsize=%i]",
            charCode,
            glyphIndex,
            CharCodeMap.size(),
            static_cast<int>(Glyphs.size()));
#endif

        switch (charCode) {
            case '*': {
                static FontGlyphType emptyGlyph;
                return emptyGlyph;
            }
            // Some fonts don't include special punctuation marks but translators are apt to use
            // absolutely every obscure character there is. Alternatively this could be done when
            // the text is loaded from the string resource, but at that point we do not necessarily
            // know if the font contains the special characters.
            case 0x201C: // left double quote
            case 0x201D: // right double quote
            {
                return GlyphForCharCode('\"');
            }
            case 0x2013: // English Dash
            case 0x2014: // Em Dash
            {
                return GlyphForCharCode('-');
            }
            default: {
                // if we have a glyph for "replacement character" U+FFFD, use that, otherwise use
                // "black diamond" U+25C6. If that doesn't exists, use "halfwidth black square"
                // U+FFED, and if that doesn't exist, use the asterisk.
                glyphIndex = lookupGlyph(0xFFFD);
                if (glyphIndex < 0 || glyphIndex >= static_cast<int>(Glyphs.size())) {
                    glyphIndex = lookupGlyph(0x25C6);
                    if (glyphIndex < 0 || glyphIndex >= static_cast<int>(Glyphs.size())) {
                        glyphIndex = lookupGlyph(0xFFED);
                        if (glyphIndex < 0 || glyphIndex >= static_cast<int>(Glyphs.size())) {
#if 0 // enable to make unknown characters obvious
							static const char unknownGlyphs[] = { '!', '@', '#', '$', '%', '^', '&', '*', '(', ')' };
							static int curGlyph = 0;
							curGlyph++;
							curGlyph = curGlyph % sizeof( unknownGlyphs );
							return GlyphForCharCode( unknownGlyphs[curGlyph] );
#else
                            return GlyphForCharCode('*');
#endif
                        }
                    }
                }
            }
        }
    }

    OVR_ASSERT(glyphIndex >= 0 && glyphIndex < static_cast<int>(Glyphs.size()));
    return Glyphs[glyphIndex];
}

//==================================================================================================
// BitmapFontLocal
//==================================================================================================

static bool ExtensionMatches(char const* fileName, char const* ext) {
    if (fileName == NULL || ext == NULL) {
        return false;
    }
    size_t extLen = OVR::OVR_strlen(ext);
    size_t fileNameLen = OVR::OVR_strlen(fileName);
    if (extLen > fileNameLen) {
        return false;
    }
    return OVR::OVR_stricmp(fileName + fileNameLen - extLen, ext) == 0;
}

//==============================
// BitmapFontLocal::Load
bool BitmapFontLocal::Load(ovrFileSys& fileSys, char const* uri) {
    char scheme[128];
    char host[128];
    int port;
    char path[1024];

    ALOG("Load Uri = %s", uri);

    if (!ovrUri::ParseUri(
            uri,
            scheme,
            sizeof(scheme),
            NULL,
            0,
            NULL,
            0,
            host,
            sizeof(host),
            port,
            path,
            sizeof(path),
            NULL,
            0,
            NULL,
            0)) {
        ALOG("ParseUri FAILED Uri = %s", uri);
        return false;
    }

    if (!FontInfo.Load(fileSys, uri)) {
        ALOG("FontInfo.Load FAILED Uri = %s", uri);
        return false;
    }

    char imagePath[1024];
    {
        ALOG("FontInfo file Uri = %s", uri);
        ALOG("FontInfo file path = %s", path);
        std::string imageBaseName = ExtractFile(FontInfo.ImageFileName);
        ALOG("image base name = %s", imageBaseName.c_str());

        StripFilename(path, imagePath, sizeof(imagePath));
        ALOG("FontInfo base path = %s", imagePath);

        AppendUriPath(imagePath, imageBaseName.c_str(), imagePath, sizeof(imagePath));
        ALOG("imagePath = %s", imagePath);
    }

    char imageUri[1024];
    OVR::OVR_sprintf(imageUri, sizeof(imageUri), "%s://%s%s", scheme, host, imagePath);
    ALOG("imageUri = %s", imageUri);
    if (!LoadImage(fileSys, imageUri)) {
        ALOG("BitmapFont image load FAILED: imageUri = '%s' uri = '%s'", imageUri, uri);
        return false;
    }

    // create the shaders for font rendering if not already created
    if (FontProgram.VertexShader == 0 || FontProgram.FragmentShader == 0) {
        static ovrProgramParm fontUniformParms[] = {
            {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        };
        FontProgram = GlProgram::Build(
            FontSingleTextureVertexShaderSrc,
            SDFFontFragmentShaderSrc,
            fontUniformParms,
            sizeof(fontUniformParms) / sizeof(ovrProgramParm));
    }

#if defined(OVR_BUILD_DEBUG)
    ALOG("BitmapFont for uri = %s load SUCCESS", uri);
#endif

    return true;
}

//==============================
// BitmapFontLocal::Load
bool BitmapFontLocal::LoadImage(ovrFileSys& fileSys, char const* uri) {
    std::vector<uint8_t> imageBuffer;
    if (!fileSys.ReadFile(uri, imageBuffer)) {
        return false;
    }
    bool success = LoadImageFromBuffer(uri, imageBuffer, ExtensionMatches(uri, ".astc"));
    if (!success) {
        ALOG("BitmapFontLocal::LoadImage: failed to load image '%s'", uri);
    }
    return success;
}

//==============================
// BitmapFontLocal::LoadImageFromBuffer
bool BitmapFontLocal::LoadImageFromBuffer(
    char const* imageName,
    std::vector<unsigned char>& buffer,
    bool const isASTC) {
    DeleteTexture(FontTexture);

    if (isASTC) {
        FontTexture = LoadASTCTextureFromMemory(buffer.data(), buffer.size(), 1, false);
    } else {
        FontTexture = LoadTextureFromBuffer(
            imageName, buffer, TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), ImageWidth, ImageHeight);
    }
    if (FontTexture.IsValid() == false) {
        OVR_WARN("BitmapFontLocal::Load: failed to load '%s'", imageName);
        return false;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, FontTexture.texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ALOG("BitmapFontLocal::LoadImageFromBuffer: success");
    return true;
}

//==============================
// BitmapFontLocal::GetGlyphMetrics
void BitmapFontLocal::GetGlyphMetrics(
    const uint32_t charCode,
    float& width,
    float& height,
    float& advanceX,
    float& advanceY) const {
    FontGlyphType const& glyph = GlyphForCharCode(charCode);
    width = glyph.Width;
    height = glyph.Height;
    advanceX = glyph.AdvanceX;
    advanceY = glyph.AdvanceY;
}

//==============================
// BitmapFontLocal::WordWrapText
bool BitmapFontLocal::WordWrapText(
    std::string& inOutText,
    const float widthMeters,
    const float fontScale) const {
    return WordWrapText(inOutText, widthMeters, std::vector<std::string>(), fontScale);
}

//==============================
// BitmapFontLocal::WordWrapText
bool BitmapFontLocal::WordWrapText(
    std::string& inOutText,
    const float widthMeters,
    std::vector<std::string> wholeStrsList,
    const float fontScale) const {
    char const* source = inOutText.c_str();
    if (*source == '\0') {
        // ALOG( "Tried to word-wrap NULL text!" );
        return false;
    }

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "Word-wrapping '%s' ... ", source );
#endif

    // we will change characters in the new string and can potentially add line breaks after some
    // characters so it may grow larger than the original string. While determining the length of
    // the string, find any characters that may potentially have a line break added after them and
    // increase the string length by 1 for each.
    auto IsPostLineBreakChar = [](uint32_t const ch) {
        // array of characters after which we can add line breaks.
        uint32_t const postLineBreakChars[] = {
            ',',
            '.',
            ':',
            ';',
            '>',
            '!',
            '?',
            ')',
            ']',
            '-',
            '=',
            '+',
            '*',
            '\\',
            '/',
            0x3002, // Chinese 'full-stop
            '\0' // list terminator
        };

        for (int i = 0; postLineBreakChars[i] != 0; ++i) {
            if (ch == postLineBreakChars[i]) {
                return true;
            }
        }
        return false;
    };

    size_t lengthInBytes = 0;
    char const* cur = source;
    for (;;) {
        char const* prev = cur;
        uint32_t const charCode = UTF8Util::DecodeNextChar(&cur);
        ptrdiff_t const numBytes = cur - prev;

        if (charCode == '\0') {
            break;
        }

        lengthInBytes += static_cast<int>(numBytes);

        if (IsPostLineBreakChar(charCode)) {
            ++lengthInBytes; // add one byte for '\n'
        }
    }

    // we now have the maximum possible length of the string with line-breaks.
    char* dest = new char[lengthInBytes + 1];
    dest[lengthInBytes] = '\0';
    intptr_t destOffset = 0;

    cur = source; // reset
    intptr_t lastLineBreakOfs = -1;
    intptr_t lastPostLineBreakOfs = -1;

    float const xScale = FontInfo.ScaleFactorX * fontScale;
    double lineWidthAtLastBreak = 0.0;
    double lineWidth = 0.0;
    bool isCJK = false;

    while (*cur != '\0') {
        // skip over formatting
        uint32_t color;
        uint32_t weight;
        char const* p = cur;
        while (CheckForFormatEscape(&p, color, weight)) {
        }
        ptrdiff_t numFormatChars = p - cur;
        if (numFormatChars > 0) {
            // copy the formatting to the destination
            memcpy(dest + destOffset, cur, numFormatChars);
            cur += numFormatChars;
            destOffset += numFormatChars;
        }

        double lastLineWidth = lineWidth;
        intptr_t lastDestOffset = destOffset;

        char const* pre = cur;
        uint32_t charCode = UTF8Util::DecodeNextChar(&cur);
        if (charCode == '\0') {
            UTF8Util::EncodeChar(dest, &destOffset, '\0');
            break;
        }
        intptr_t const charCodeSize = cur - pre;
        // determine if the string is Chinese, Japanese or Korean.
        isCJK |= (charCode >= 0x4E00 && charCode <= 0x9FAF) ||
            (charCode >= 0x3000 && charCode <= 0x30FF);

        // replace tabs with a space
        if (charCode == '\t') {
            charCode = ' ';
        }

        if (charCode == ' ') {
            lastLineBreakOfs = destOffset;
            lineWidthAtLastBreak = lineWidth; // line width *before* the space
        } else if (charCode == '\\') {
            // replace verbatim  and "\r" with explicit breaks
            char const* temp = cur;
            uint32_t nextCode = UTF8Util::DecodeNextChar(&temp);
            if (nextCode == 'r' || nextCode == 'n') {
                lineWidth = 0.0;
                lineWidthAtLastBreak = 0.0;
                cur = temp; // skip the next character
                // output a linefeed
                UTF8Util::EncodeChar(dest, &destOffset, '\n');
                continue;
            }
        } else if (charCode == '\r' || charCode == '\n') {
            lineWidth = 0.0;
            lineWidthAtLastBreak = 0.0;
            // output a linefeed
            UTF8Util::EncodeChar(dest, &destOffset, '\n');
            continue;
        }

        FontGlyphType const& g = GlyphForCharCode(charCode);
        lineWidth += g.AdvanceX * xScale;

        if (lineWidth >= widthMeters) {
            if (charCode == ' ') {
                // just turn the emitted character into a linefeed
                charCode = '\n';
            } else {
                if (lastLineBreakOfs < 0 && lastPostLineBreakOfs < 0) {
                    // we're unable to wrap based on punctuation or whitespace, so we must break at
                    // some arbitrary character. This is fine for Chinese, but not for western
                    // languages. It's also relatively rare for western languages since they use
                    // spaces between each word.
                    /*if ( !isCJK )
                    {
                        return false;
                    }*/
                    // just break at the last character that didn't exceed the line width
                    lastPostLineBreakOfs = lastDestOffset;
                    lineWidthAtLastBreak = lastLineWidth;
                }

                intptr_t bestBreakOfs = lastLineBreakOfs > lastPostLineBreakOfs
                    ? lastLineBreakOfs
                    : lastPostLineBreakOfs;

                char* destBreakPoint = dest + bestBreakOfs;

                bool const overwrite = bestBreakOfs != lastPostLineBreakOfs;
                if (overwrite) {
                    // we overwrote with a single byte, but we could have overwritten a multi-byte
                    // unicode character so we may need to shift the entire destination string down
                    // by (sizeof overwritten char) - 1 bytes. decode the character at the bestBreak
                    // point to determine how many bytes we need to skip
                    char const* afterDestBreakPoint = destBreakPoint;
                    uint32_t const overwrittenCode = UTF8Util::DecodeNextChar(&afterDestBreakPoint);
                    OVR_UNUSED(overwrittenCode);

                    // encode the line feed over the last break point.
                    UTF8Util::EncodeChar(dest, &bestBreakOfs, '\n');

                    // determine if we need to shift the dest text down
                    ptrdiff_t numBytesToSkip = afterDestBreakPoint - destBreakPoint;
                    if (numBytesToSkip > 1) {
                        memcpy(
                            dest + bestBreakOfs + 1,
                            dest + bestBreakOfs + numBytesToSkip,
                            lengthInBytes - bestBreakOfs);
                        destOffset -= numBytesToSkip - 1;
                    }
                } else {
                    // move the buffer contents after the insertion point to make room for 1 byte
                    for (intptr_t ofs = destOffset; ofs > bestBreakOfs; --ofs) {
                        dest[ofs] = dest[ofs - 1];
                    }
                    // encode the line feed after the last break point
                    UTF8Util::EncodeChar(dest, &bestBreakOfs, '\n');
                    destOffset++;
                }
            }

            lastLineBreakOfs = -1;
            lastPostLineBreakOfs = -1;

            // subtract the width after the last whitespace so that we don't lose any accumulated
            // width.
            lineWidth -= lineWidthAtLastBreak;
        }

        if (IsPostLineBreakChar(charCode)) {
            lastPostLineBreakOfs = destOffset + charCodeSize;
            lineWidthAtLastBreak = lineWidth; // line width *after* the break char
        }

        // output the current character to the destination buffer
        UTF8Util::EncodeChar(dest, &destOffset, charCode);

        // in CJK cases we cannot be absolutely certain of the number of breaks until we've scanned
        // everything and computed the width (because we may break at any character). In that case,
        // we may need to expand the buffer.
        if (destOffset + 6 >
            static_cast<intptr_t>(lengthInBytes)) // 6 bytes is max UTF-8 encoding size
        {
            size_t newLen = (lengthInBytes * 3) / 2;
            char* newDest = new char[newLen + 1];
            memcpy(newDest, dest, destOffset);
            newDest[newLen] = '\0';
            delete[] dest;
            dest = newDest;
            lengthInBytes = newLen;
        }
    }

    dest[destOffset] = '\0';
    inOutText = dest;
    delete[] dest;

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "Word-wrapping '%s' -> '%s' DONE", source, inOutText.c_str() );
#endif

    return true;
}

float BitmapFontLocal::GetFirstFitChars(
    std::string& inOutText,
    const float widthMeters,
    const int numLines,
    const float fontScale) const {
    if (inOutText.empty()) {
        return 0;
    }

    float const xScale = FontInfo.ScaleFactorX * fontScale;
    float lineWidth = 0.0f;
    int remainingLines = numLines;

    for (int32_t pos = 0; pos < static_cast<int32_t>(inOutText.length()); ++pos) {
        uint32_t charCode = UTF8Util::GetCharAt(pos, inOutText.c_str());
        if (charCode == '\n') {
            remainingLines--;
            if (remainingLines == 0) {
                inOutText = inOutText.substr(0, pos);
                return widthMeters - lineWidth;
            }
        } else if (charCode != '\0') {
            FontGlyphType const& glyph = GlyphForCharCode(charCode);
            lineWidth += glyph.AdvanceX * xScale;
            if (lineWidth > widthMeters) {
                inOutText =
                    inOutText.substr(0, pos - 1); // -1 to not include current char that didn't fit.
                return widthMeters - (lineWidth - glyph.AdvanceX * xScale);
            }
        }
    }
    // Entire text fits, leave inOutText unchanged
    return 0;
}

float BitmapFontLocal::GetLastFitChars(
    std::string& inOutText,
    const float widthMeters,
    const float fontScale) const {
    if (inOutText.empty()) {
        return 0;
    }

    float const xScale = FontInfo.ScaleFactorX * fontScale;
    float lineWidth = 0.0f;

    for (int32_t pos = static_cast<int32_t>(inOutText.length()) - 1; pos >= 0; --pos) {
        uint32_t charCode = UTF8Util::GetCharAt(pos, inOutText.c_str());
        FontGlyphType const& glyph = GlyphForCharCode(charCode);
        lineWidth += glyph.AdvanceX * xScale;
        if (lineWidth > widthMeters) {
            inOutText = inOutText.substr(
                pos + 1, inOutText.length()); // +1 to not include current char that didn't fit.
            return widthMeters - (lineWidth - glyph.AdvanceX * xScale);
        }
    }
    // Entire text fits, leave inOutText unchanged
    return 0;
}

//==============================
// BitmapFontLocal::CalcTextWidth
float BitmapFontLocal::CalcTextWidth(char const* text) const {
    float width = 0.0f;
    char const* p = text;
    uint32_t color;
    uint32_t weight;
    while (CheckForFormatEscape(&p, color, weight))
        ;
    for (uint32_t charCode = UTF8Util::DecodeNextChar(&p); charCode != '\0';
         charCode = UTF8Util::DecodeNextChar(&p)) {
        if (charCode == '\r' || charCode == '\n') {
            continue; // skip line endings
        }

        FontGlyphType const& g = GlyphForCharCode(charCode);
        width += g.AdvanceX * FontInfo.ScaleFactorX;
        while (CheckForFormatEscape(&p, color, weight))
            ;
    }

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "CalcTextWidth '%s' = %.3f ", text, width );
#endif

    return width;
}

//==============================
// BitmapFontLocal::CalcTextMetrics
void BitmapFontLocal::CalcTextMetrics(
    char const* text,
    size_t& len,
    float& width,
    float& height,
    float& firstAscent,
    float& lastDescent,
    float& fontHeight,
    float* lineWidths,
    int const maxLines,
    int& numLines) const {
    len = 0;
    numLines = 0;
    width = 0.0f;
    height = 0.0f;

    if (lineWidths == NULL || maxLines <= 0) {
        return;
    }
    if (text == NULL || text[0] == '\0') {
        return;
    }

    float maxLineAscent = 0.0f;
    float maxLineDescent = 0.0f;
    firstAscent = 0.0f;
    lastDescent = 0.0f;
    fontHeight = FontInfo.FontHeight * FontInfo.ScaleFactorY;
    numLines = 0;
    int charsOnLine = 0;
    lineWidths[0] = 0.0f;
    uint32_t color;
    uint32_t weight;

    char const* p = text;
    for (;; len++) {
        while (CheckForFormatEscape(&p, color, weight))
            ;
        uint32_t charCode = UTF8Util::DecodeNextChar(&p);
        if (charCode == '\r') {
            continue; // skip carriage returns
        }
        if (charCode == '\n' || charCode == '\0') {
            // keep track of the widest line, which will be the width of the entire text block
            if (numLines < maxLines && lineWidths[numLines] > width) {
                width = lineWidths[numLines];
            }

            firstAscent = (numLines == 0) ? maxLineAscent : firstAscent;
            lastDescent = (charsOnLine > 0) ? maxLineDescent : lastDescent;
            charsOnLine = 0;

            numLines++;
            if (numLines < maxLines) {
                // if we're not out of array space, advance and zero the width
                lineWidths[numLines] = 0.0f;
                maxLineAscent = 0.0f;
                maxLineDescent = 0.0f;
            }

            if (charCode == '\0') {
                break;
            }
            continue;
        }

        charsOnLine++;

        FontGlyphType const& g = GlyphForCharCode(charCode);

        if (numLines < maxLines) {
            lineWidths[numLines] += g.AdvanceX * FontInfo.ScaleFactorX;
        }

        if (numLines == 0) {
            if (g.BearingY > maxLineAscent) {
                maxLineAscent = g.BearingY;
            }
        } else {
            // all lines after the first line are full height
            maxLineAscent = FontInfo.FontHeight;
        }
        float descent = g.Height - g.BearingY;
        if (descent > maxLineDescent) {
            maxLineDescent = descent;
        }
    }

    OVR_ASSERT(numLines >= 1);

    firstAscent *= FontInfo.ScaleFactorY;
    lastDescent *= FontInfo.ScaleFactorY;
    height = firstAscent;

    if (numLines < maxLines) {
        height += (numLines - 1) * FontInfo.FontHeight * FontInfo.ScaleFactorY;
    } else {
        height += (maxLines - 1) * FontInfo.FontHeight * FontInfo.ScaleFactorY;
    }

    height += lastDescent;

    // OVR_ASSERT( numLines <= maxLines );
}

//==============================
// BitmapFontLocal::TruncateText
void BitmapFontLocal::TruncateText(std::string& inOutText, int const maxLines) const {
    char const* p = inOutText.c_str();

    if (p == nullptr || p[0] == '\0') {
        return;
    }

    uint32_t color;
    uint32_t weight;

    int lineCount = 0;
    size_t len = 0;
    for (;; len++) {
        while (CheckForFormatEscape(&p, color, weight))
            ;
        uint32_t charCode = UTF8Util::DecodeNextChar(&p);
        if (charCode == '\n') {
            lineCount++;
            if (lineCount == maxLines - 1) {
                inOutText = inOutText.substr(0, len + 1);
                inOutText += "...";
                break;
            }
        }

        if (charCode == '\0') {
            break;
        }
    }

#if defined(OVR_BUILD_DEBUG)
///	ALOG( "TruncateText '%s' -> '%s' ", p, inOutText.c_str() );
#endif
}

//==================================================================================================
// BitmapFontSurfaceLocal
//==================================================================================================

//==============================
// BitmapFontSurfaceLocal::BitmapFontSurface
BitmapFontSurfaceLocal::BitmapFontSurfaceLocal()
    : Vertices(NULL),
      MaxVertices(0),
      MaxIndices(0),
      CurVertex(0),
      CurIndex(0),
      Initialized(false) {}

//==============================
// BitmapFontSurfaceLocal::~BitmapFontSurfaceLocal
BitmapFontSurfaceLocal::~BitmapFontSurfaceLocal() {
    FontSurfaceDef.geo.Free();
    delete[] Vertices;
    Vertices = NULL;
}

//==============================
// BitmapFontSurfaceLocal::Init
// Initializes the surface VBO
void BitmapFontSurfaceLocal::Init(const int maxVertices) {
    OVR_ASSERT(
        FontSurfaceDef.geo.vertexBuffer == 0 && FontSurfaceDef.geo.indexBuffer == 0 &&
        FontSurfaceDef.geo.vertexArrayObject == 0);
    OVR_ASSERT(Vertices == NULL);
    if (Vertices != NULL) {
        delete[] Vertices;
        Vertices = NULL;
    }
    OVR_ASSERT(maxVertices % 4 == 0);

    MaxVertices = maxVertices;
    MaxIndices = (maxVertices / 4) * 6;

    Vertices = new fontVertex_t[maxVertices];

    CurVertex = 0;
    CurIndex = 0;

    Bounds3f localBounds(Bounds3f::Init);
    FontSurfaceDef.geo = FontGeometry(MaxVertices / 4, localBounds);
    FontSurfaceDef.geo.indexCount = 0; // if there's anything to render this will be modified

    FontSurfaceDef.surfaceName = "font";

    // FontSurfaceDef.graphicsCommand.GpuState.blendMode = GL_FUNC_ADD;
    FontSurfaceDef.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
    FontSurfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

#if 0
	FontSurfaceDef.graphicsCommand.GpuState.blendSrcAlpha = GL_ONE;
	FontSurfaceDef.graphicsCommand.GpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
	FontSurfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE_SEPARATE;
#else
    FontSurfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
#endif

    FontSurfaceDef.graphicsCommand.GpuState.frontFace = GL_CCW;

    FontSurfaceDef.graphicsCommand.GpuState.depthEnable = true;
    FontSurfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
    FontSurfaceDef.graphicsCommand.GpuState.polygonOffsetEnable = false;
    FontSurfaceDef.graphicsCommand.GpuState.cullEnable = true;

    Initialized = true;

    ALOG("BitmapFontSurfaceLocal::Init: success");
}

//==============================
// BitmapFontSurfaceLocal::DrawText3D
Vector3f BitmapFontSurfaceLocal::DrawText3D(
    BitmapFont const& font,
    fontParms_t const& parms,
    Vector3f const& pos,
    Vector3f const& normal,
    Vector3f const& up,
    float scale,
    Vector4f const& color,
    char const* text) {
#if defined(OVR_BUILD_DEBUG)
///	ALOG( "DrawText3D -> '%s'", text == NULL ? "<null>" : text );
#endif

    if (text == NULL || text[0] == '\0') {
        return Vector3f::ZERO; // nothing to do here, move along
    }
    Vector3f toNextLine;
    VertexBlockType vb =
        DrawTextToVertexBlock(font, parms, pos, normal, up, scale, color, text, &toNextLine);

    // add the new vertex block to the array of vertex blocks
    VertexBlocks.push_back(vb);

    return toNextLine;
}

//==============================
// BitmapFontSurfaceLocal::DrawText3Df
Vector3f BitmapFontSurfaceLocal::DrawText3Df(
    BitmapFont const& font,
    fontParms_t const& parms,
    Vector3f const& pos,
    Vector3f const& normal,
    Vector3f const& up,
    float const scale,
    Vector4f const& color,
    char const* fmt,
    ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    OVR::OVR_vsprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return DrawText3D(font, parms, pos, normal, up, scale, color, buffer);
}

//==============================
// BitmapFontSurfaceLocal::DrawTextBillboarded3D
Vector3f BitmapFontSurfaceLocal::DrawTextBillboarded3D(
    BitmapFont const& font,
    fontParms_t const& parms,
    Vector3f const& pos,
    float const scale,
    Vector4f const& color,
    char const* text) {
    fontParms_t billboardParms = parms;
    billboardParms.Billboard = true;
    return DrawText3D(
        font,
        billboardParms,
        pos,
        Vector3f(1.0f, 0.0f, 0.0f),
        Vector3f(0.0f, -1.0f, 0.0f),
        scale,
        color,
        text);
}

//==============================
// BitmapFontSurfaceLocal::DrawTextBillboarded3Df
Vector3f BitmapFontSurfaceLocal::DrawTextBillboarded3Df(
    BitmapFont const& font,
    fontParms_t const& parms,
    Vector3f const& pos,
    float const scale,
    Vector4f const& color,
    char const* fmt,
    ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    OVR::OVR_vsprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return DrawTextBillboarded3D(font, parms, pos, scale, color, buffer);
}

//==============================================================
// vbSort_t
// small structure that is used to sort vertex blocks by their distance to the camera
//==============================================================
struct vbSort_t {
    int VertexBlockIndex;
    float DistanceSquared;
};

//==============================
// VertexBlockSortFn
// sort function for vertex blocks
int VertexBlockSortFn(void const* a, void const* b) {
    return ftoi(((vbSort_t const*)a)->DistanceSquared - ((vbSort_t const*)b)->DistanceSquared);
}

//==============================
// BitmapFontSurfaceLocal::Finish
// transform all vertex blocks into the vertices array so they're ready to be uploaded to the VBO
// We don't have to do this for each eye because the billboarded surfaces are sorted / aligned
// based on the center view matrix's view direction.
void BitmapFontSurfaceLocal::Finish(Matrix4f const& viewMatrix) {
    // SPAM( "BitmapFontSurfaceLocal::Finish" );

    FontSurfaceDef.geo.localBounds.Clear();

    Matrix4f invViewMatrix = viewMatrix.Inverted(); // if the view is never scaled or sheared we
                                                    // could use Transposed() here instead
    Vector3f viewPos = invViewMatrix.GetTranslation();
    Vector3f viewUp = GetViewMatrixUp(viewMatrix);

    // sort vertex blocks indices based on distance to pivot
    int const MAX_VERTEX_BLOCKS = 256;
    vbSort_t vbSort[MAX_VERTEX_BLOCKS];
    int const n = VertexBlocks.size();
    for (int i = 0; i < n; ++i) {
        vbSort[i].VertexBlockIndex = i;
        VertexBlockType& vb = VertexBlocks[i];
        vbSort[i].DistanceSquared = (vb.Pivot - viewPos).LengthSq();
    }

    qsort(vbSort, n, sizeof(vbSort[0]), VertexBlockSortFn);

    // transform the vertex blocks into the vertices array
    CurIndex = 0;
    CurVertex = 0;

    // TODO:
    // To add multiple-font-per-surface support, we need to add a 3rd component to s and t,
    // then get the font for each vertex block, and set the texture index on each vertex in
    // the third texture coordinate.
    for (int i = 0; i < static_cast<int>(VertexBlocks.size()); ++i) {
        VertexBlockType& vb = VertexBlocks[vbSort[i].VertexBlockIndex];
        Matrix4f transform;
        if (vb.Billboard) {
            if (vb.TrackRoll) {
                transform = invViewMatrix;
            } else {
                Vector3f textNormal = viewPos - vb.Pivot;
                float const len = textNormal.Length();
                if (len < MATH_FLOAT_SMALLEST_NON_DENORMAL) {
                    vb.Free();
                    continue;
                }
                textNormal *= 1.0f / len;
                transform = Matrix4f::CreateFromBasisVectors(textNormal, viewUp * -1.0f);
            }
            transform.SetTranslation(vb.Pivot);
        } else {
            transform.SetIdentity();
            transform.SetTranslation(vb.Pivot);
        }

        for (int j = 0; j < vb.NumVerts; j++) {
            fontVertex_t const& v = vb.Verts[j];
            Vector3f const position = transform.Transform(v.xyz);
            Vertices[CurVertex].xyz = position;
            Vertices[CurVertex].s = v.s;
            Vertices[CurVertex].t = v.t;
            *(std::uint32_t*)(&Vertices[CurVertex].rgba[0]) = *(std::uint32_t*)(&v.rgba[0]);
            *(std::uint32_t*)(&Vertices[CurVertex].fontParms[0]) =
                *(std::uint32_t*)(&v.fontParms[0]);
            CurVertex++;

            FontSurfaceDef.geo.localBounds.AddPoint(position);
        }
        CurIndex += (vb.NumVerts / 2) * 3;
        // free this vertex block
        vb.Free();
    }
    // remove all elements from the vertex block (but don't free the memory since it's likely to be
    // needed on the next frame.
    VertexBlocks.clear();

    glBindVertexArray(FontSurfaceDef.geo.vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, FontSurfaceDef.geo.vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, CurVertex * sizeof(fontVertex_t), (void*)Vertices);
    glBindVertexArray(0);
    FontSurfaceDef.geo.indexCount = CurIndex;
}

//==============================
// BitmapFontSurfaceLocal::AppendSurfaceList
void BitmapFontSurfaceLocal::AppendSurfaceList(
    BitmapFont const& font,
    std::vector<ovrDrawSurface>& surfaceList) const {
    if (FontSurfaceDef.geo.indexCount == 0) {
        return;
    }

    ovrDrawSurface drawSurf;

    FontSurfaceDef.graphicsCommand.Program = AsLocal(font).GetFontProgram();
    FontSurfaceDef.graphicsCommand.UniformData[0].Data = (void*)&AsLocal(font).GetFontTexture();

    drawSurf.surface = &FontSurfaceDef;

    surfaceList.push_back(drawSurf);
}

void BitmapFontSurfaceLocal::SetCullEnabled(const bool enabled) {
    FontSurfaceDef.graphicsCommand.GpuState.cullEnable = enabled;
}

//==============================
// BitmapFont::Create
BitmapFont* BitmapFont::Create() {
    return new BitmapFontLocal;
}
//==============================
// BitmapFont::Free
void BitmapFont::Free(BitmapFont*& font) {
    if (font != NULL) {
        delete font;
        font = NULL;
    }
}

//==============================
// BitmapFontSurface::Create
BitmapFontSurface* BitmapFontSurface::Create() {
    return new BitmapFontSurfaceLocal();
}

//==============================
// BitmapFontSurface::Free
void BitmapFontSurface::Free(BitmapFontSurface*& fontSurface) {
    if (fontSurface != NULL) {
        delete fontSurface;
        fontSurface = NULL;
    }
}

} // namespace OVRFW
