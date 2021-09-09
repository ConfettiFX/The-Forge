/************************************************************************************

Filename    :   OvrMenuMgr.cpp
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "VRMenuMgr.h"

#include "Render/DebugLines.h"
#include "Render/BitmapFont.h"
#include "Misc/Log.h"

#include "VRMenuObject.h"
#include "GuiSys.h"

#include "OVR_Lexer2.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

// diffuse-only programs
static const char* GUIDiffuseOnlyVertexShaderSrc = R"glsl(
uniform lowp vec4 UniformColor;
uniform lowp vec4 UniformFadeDirection;
uniform lowp vec2 UniformUVOffset;

attribute vec4 Position;
attribute vec2 TexCoord;
attribute vec4 VertexColor;

varying highp vec2 oTexCoord;
varying lowp vec4 oColor;

void main()
{
    gl_Position = TransformVertex( Position );
    oTexCoord = vec2( TexCoord.x + UniformUVOffset.x, TexCoord.y + UniformUVOffset.y );
    oColor = UniformColor * VertexColor;
	// Fade out vertices if direction is positive
    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )
	{
        if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }
    }
}
)glsl";

static const char* GUIDiffuseOnlyFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform highp vec4 ClipUVs;

varying highp vec2 oTexCoord;
varying lowp vec4 oColor;

void main()
{
    if ( oTexCoord.x < ClipUVs.x || oTexCoord.y < ClipUVs.y || oTexCoord.x > ClipUVs.z || oTexCoord.y > ClipUVs.w )
    {
        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    }
    else
    {
        gl_FragColor = oColor * texture2D( Texture0, oTexCoord );
    }
}
)glsl";

static const char* GUIDiffuseAlphaDiscardFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform highp vec4 ClipUVs;

varying highp vec2 oTexCoord;
varying lowp vec4 oColor;

void main()
{
    if ( oTexCoord.x < ClipUVs.x || oTexCoord.y < ClipUVs.y || oTexCoord.x > ClipUVs.z || oTexCoord.y > ClipUVs.w )
    {
        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    }
    else
    {
        lowp vec4 texColor = texture2D( Texture0, oTexCoord );
        if ( texColor.w < 0.01 ) discard;
        gl_FragColor = oColor * texColor;
    }
}
)glsl";

// diffuse color ramped programs
static const char* GUIColorRampFragmentSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform mediump vec2 ColorTableOffset;

varying highp vec2 oTexCoord;
varying lowp vec4 oColor;

void main()
{
    lowp vec4 texel = texture2D( Texture0, oTexCoord );
    lowp vec2 colorIndex = vec2( ColorTableOffset.x + texel.x, ColorTableOffset.y );
    lowp vec4 remappedColor = texture2D( Texture1, colorIndex.xy );
    gl_FragColor = oColor * vec4( remappedColor.xyz, texel.a );
}
)glsl";

// diffuse + color ramped + target programs
static const char* GUIDiffuseColorRampTargetVertexShaderSrc = R"glsl(
uniform lowp vec4 UniformColor;
uniform lowp vec4 UniformFadeDirection;

attribute vec4 Position;
attribute vec2 TexCoord;
attribute vec2 TexCoord1;
attribute vec4 VertexColor;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    gl_Position = TransformVertex( Position );
    oTexCoord = TexCoord;
    oTexCoord1 = TexCoord1;
    oColor = UniformColor * VertexColor;
	// Fade out vertices if direction is positive
    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )
	{
       if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }
    }
}
)glsl";

static const char* GUIColorRampTargetFragmentSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;	// color ramp target
uniform sampler2D Texture2;	// color ramp
uniform mediump vec2 ColorTableOffset;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    mediump vec4 lookup = texture2D( Texture1, oTexCoord1 );
    mediump vec2 colorIndex = vec2( ColorTableOffset.x + lookup.x, ColorTableOffset.y );
    mediump vec4 remappedColor = texture2D( Texture2, colorIndex.xy );
    mediump vec4 texel = texture2D( Texture0, oTexCoord );
    mediump vec3 blended = ( texel.xyz * ( 1.0 - lookup.a ) ) + ( remappedColor.xyz * lookup.a );
    gl_FragColor = oColor * vec4( blended.xyz, texel.a );
}
)glsl";

// diffuse + additive programs
static const char* GUITwoTextureColorModulatedShaderSrc = R"glsl(
uniform lowp vec4 UniformColor;
uniform lowp vec4 UniformFadeDirection;

attribute vec4 Position;
attribute vec2 TexCoord;
attribute vec2 TexCoord1;
attribute vec4 VertexColor;
attribute vec4 Parms;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    gl_Position = TransformVertex( Position );
    oTexCoord = TexCoord;
    oTexCoord1 = TexCoord1;
    oColor = UniformColor * VertexColor;
    // Fade out vertices if direction is positive
    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )
    {
        if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }
    }
}
)glsl";

static const char* GUIDiffusePlusAdditiveFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    lowp vec4 diffuseTexel = texture2D( Texture0, oTexCoord );
    lowp vec4 additiveTexel = texture2D( Texture1, oTexCoord1 );
    lowp vec4 additiveModulated = vec4( additiveTexel.xyz * additiveTexel.a, 0.0 );
    gl_FragColor = min( diffuseTexel + additiveModulated, 1.0 ) * oColor;
}
)glsl";

// diffuse + diffuse program
// the alpha for the second diffuse is used to composite the color to the first diffuse and
// the alpha of the first diffuse is used to composite to the fragment.
static const char* GUIDiffuseCompositeFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    lowp vec4 diffuse1Texel = texture2D( Texture0, oTexCoord );
    lowp vec4 diffuse2Texel = texture2D( Texture1, oTexCoord1 );
    gl_FragColor = vec4( diffuse1Texel.xyz * ( 1.0 - diffuse2Texel.a ) + diffuse2Texel.xyz * diffuse2Texel.a, diffuse1Texel.a ) * oColor;
}
)glsl";

// alpha map + diffuse program
// the alpha channel of first texture is to composite to the fragment
static const char* GUIAlphaDiffuseFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;

varying highp vec2 oTexCoord;
varying highp vec2 oTexCoord1;
varying lowp vec4 oColor;

void main()
{
    lowp vec4 diffuse1Texel = texture2D( Texture0, oTexCoord );
    lowp vec4 diffuse2Texel = texture2D( Texture1, oTexCoord1 );
    gl_FragColor = vec4( diffuse2Texel.xyz, diffuse1Texel.a ) * oColor;
}
)glsl";

//==================================
// ComposeHandle
menuHandle_t ComposeHandle(int const index, std::uint32_t const id) {
    std::uint64_t handle = (((std::uint64_t)id) << 32ULL) | (std::uint64_t)index;
    return menuHandle_t(handle);
}

//==================================
// DecomposeHandle
void DecomposeHandle(menuHandle_t const handle, int& index, std::uint32_t& id) {
    index = (int)(handle.Get() & 0xFFFFFFFF);
    id = (std::uint32_t)(handle.Get() >> 32ULL);
}

//==================================
// HandleComponentsAreValid
static bool HandleComponentsAreValid(int const index, std::uint32_t const id) {
    if (id == INVALID_MENU_OBJECT_ID) {
        return false;
    }
    if (index < 0) {
        return false;
    }
    return true;
}

//==============================================================
// SurfSort
class SurfSort {
   public:
    int64_t Key;

    bool operator<(SurfSort const& other) const {
        return Key - other.Key > 0; // inverted because we want to render furthest-to-closest
    }
};

//==============================================================
// VRMenuMgrLocal
class VRMenuMgrLocal : public OvrVRMenuMgr {
   public:
    static int const MAX_SUBMITTED = 256;

    VRMenuMgrLocal(OvrGuiSys& guiSys);
    virtual ~VRMenuMgrLocal();

    // Initialize the VRMenu system
    virtual void Init(OvrGuiSys& guiSys);
    // Shutdown the VRMenu syatem
    virtual void Shutdown();

    // creates a new menu object
    virtual menuHandle_t CreateObject(VRMenuObjectParms const& parms);
    // Frees a menu object.  If the object is a child of a parent object, this will
    // also remove the child from the parent.
    virtual void FreeObject(menuHandle_t const handle);
    // Returns true if the handle is valid.
    virtual bool IsValid(menuHandle_t const handle) const;
    // Return the object for a menu handle or NULL if the object does not exist or the
    // handle is invalid;
    virtual VRMenuObject* ToObject(menuHandle_t const handle) const;

    // Submits the specified menu object to be renderered
    virtual void SubmitForRendering(
        OvrGuiSys& guiSys,
        Matrix4f const& centerViewMatrix,
        menuHandle_t const handle,
        Posef const& worldPose,
        VRMenuRenderFlags_t const& flags);

    // Call once per frame before rendering to sort surfaces.
    virtual void Finish(Matrix4f const& viewMatrix);

    virtual void AppendSurfaceList(
        Matrix4f const& centerViewMatrix,
        std::vector<ovrDrawSurface>& surfaceList);

    virtual GlProgram const* GetGUIGlProgram(eGUIProgramType const programType) const;

    static VRMenuMgrLocal& ToLocal(OvrVRMenuMgr& menuMgr) {
        return *(VRMenuMgrLocal*)&menuMgr;
    }

   private:
    //--------------------------------------------------------------
    // private methods
    //--------------------------------------------------------------

    // make a private assignment operator to prevent warning C4512: assignment operator could not be
    // generated
    VRMenuMgrLocal& operator=(const VRMenuMgrLocal&);

    void AddComponentToDeletionList(menuHandle_t const ownerHandle, VRMenuComponent* component);
    void ExecutePendingComponentDeletions();

    void CondenseList();
    void SubmitForRenderingRecursive(
        OvrGuiSys& guiSys,
        Matrix4f const& centerViewMatrix,
        VRMenuRenderFlags_t const& flags,
        VRMenuObject const* obj,
        Posef const& parentModelPose,
        Vector4f const& parentColor,
        Vector3f const& parentScale,
        Bounds3f& cullBounds,
        SubmittedMenuObject* submitted,
        int const maxIndices,
        int& curIndex,
        int const distanceIndex) const;

    //--------------------------------------------------------------
    // private members
    //--------------------------------------------------------------
    OvrGuiSys& GuiSys; // reference to the GUI sys that owns this menu manager
    std::uint32_t CurrentId; // ever-incrementing object ID (well... up to 4 billion or so :)
    std::vector<VRMenuObject*> ObjectList; // list of all menu objects
    std::vector<int> FreeList; // list of free slots in the array

    std::vector<ovrComponentList>
        PendingDeletions; // list of components (and owning objects) that are pending deletion

    bool Initialized; // true if Init has been called

    SubmittedMenuObject Submitted[MAX_SUBMITTED]; // all objects that have been submitted for
                                                  // rendering on the current frame
    std::vector<SurfSort>
        SortKeys; // sort key consisting of distance from view and submission index
    int NumSubmitted; // number of currently submitted menu objects
    mutable int NumToRender; // number of submitted objects to render

    GlProgram GUIProgramDiffuseOnly; // has a diffuse only
    GlProgram GUIProgramDiffuseAlphaDiscard; // diffuse, but discard fragments with 0 alpha
    GlProgram GUIProgramDiffusePlusAdditive; // has a diffuse and an additive
    GlProgram GUIProgramDiffuseComposite; // has a two diffuse maps
    GlProgram GUIProgramDiffuseColorRamp; // has a diffuse and color ramp, and color ramp target is
                                          // the diffuse
    GlProgram GUIProgramDiffuseColorRampTarget; // has diffuse, color ramp, and a separate color
                                                // ramp target
    GlProgram GUIProgramAlphaDiffuse; // alpha map + diffuse map

    static bool ShowCollision; // show collision bounds only
    static bool ShowDebugBounds; // true to show the menu items' debug bounds. This is static so
                                 // that the console command will turn on bounds for all activities.
    static bool
        ShowDebugHierarchy; // true to show the menu items' hierarchy. This is static so that the
                            // console command will turn on bounds for all activities.
    static bool ShowDebugNormals;
    static bool ShowPoses;
    static bool ShowStats; // show stats like number of draw calls
    static bool ShowWrapWidths;

    static void DebugCollision(void* appPtr, const char* cmdLine);
    static void DebugMenuBounds(void* appPtr, const char* cmdLine);
    static void DebugMenuHierarchy(void* appPtr, const char* cmdLine);
    static void DebugMenuPoses(void* appPtr, const char* cmdLine);
    static void DebugShowStats(void* appPtr, const char* cmdLine);
    static void DebugWordWrap(void* appPtr, const char* cmdLine);
};

bool VRMenuMgrLocal::ShowCollision = false;
bool VRMenuMgrLocal::ShowDebugBounds = false;
bool VRMenuMgrLocal::ShowDebugHierarchy = false;
bool VRMenuMgrLocal::ShowDebugNormals = false;
bool VRMenuMgrLocal::ShowPoses = false;
bool VRMenuMgrLocal::ShowStats = false;
bool VRMenuMgrLocal::ShowWrapWidths = false;

void VRMenuMgrLocal::DebugCollision(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowCollision = show != 0;
    ALOG("DebugCollision( '%s' ): show = %i", parms, show);
}

void VRMenuMgrLocal::DebugMenuBounds(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowDebugBounds = show != 0;
    ALOG("DebugMenuBounds( '%s' ): show = %i", parms, show);
}

void VRMenuMgrLocal::DebugMenuHierarchy(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowDebugHierarchy = show != 0;
    ALOG("DebugMenuHierarchy( '%s' ): show = %i", parms, show);
}

void VRMenuMgrLocal::DebugMenuPoses(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowPoses = show != 0;
    ALOG("DebugMenuPoses( '%s' ): show = %i", parms, show);
}

void VRMenuMgrLocal::DebugShowStats(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowStats = show != 0;
    ALOG("ShowStats( '%s' ): show = %i", parms, show);
}

void VRMenuMgrLocal::DebugWordWrap(void* appPtr, const char* parms) {
    ovrLexer lex(parms);
    int show;
    lex.ParseInt(show, 0);
    ShowWrapWidths = show != 0;
    ALOG("ShowWrapWidths( '%s' ): show = %i", parms, show);
}

//==================================
// VRMenuMgrLocal::VRMenuMgrLocal
VRMenuMgrLocal::VRMenuMgrLocal(OvrGuiSys& guiSys)
    : GuiSys(guiSys), CurrentId(0), Initialized(false), NumSubmitted(0), NumToRender(0) {}

//==================================
// VRMenuMgrLocal::~VRMenuMgrLocal
VRMenuMgrLocal::~VRMenuMgrLocal() {}

//==================================
// VRMenuMgrLocal::Init
//
// Initialize the VRMenu system
void VRMenuMgrLocal::Init(OvrGuiSys& guiSys) {
    ALOG("VRMenuMgrLocal::Init");
    if (Initialized) {
        return;
    }

    // diffuse only
    if (GUIProgramDiffuseOnly.VertexShader == 0 || GUIProgramDiffuseOnly.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformUVOffset", OVRFW::ovrProgramParmType::FLOAT_VECTOR2},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"ClipUVs", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffuseOnly ");
        GUIProgramDiffuseOnly = OVRFW::GlProgram::Build(
            GUIDiffuseOnlyVertexShaderSrc,
            GUIDiffuseOnlyFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }
    // diffuse alpha discard only
    if (GUIProgramDiffuseAlphaDiscard.VertexShader == 0 ||
        GUIProgramDiffuseAlphaDiscard.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformUVOffset", OVRFW::ovrProgramParmType::FLOAT_VECTOR2},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"ClipUVs", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffuseAlphaDiscard ");
        GUIProgramDiffuseAlphaDiscard = OVRFW::GlProgram::Build(
            GUIDiffuseOnlyVertexShaderSrc,
            GUIDiffuseAlphaDiscardFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }
    // diffuse + additive
    if (GUIProgramDiffusePlusAdditive.VertexShader == 0 ||
        GUIProgramDiffusePlusAdditive.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffusePlusAdditive ");
        GUIProgramDiffusePlusAdditive = OVRFW::GlProgram::Build(
            GUITwoTextureColorModulatedShaderSrc,
            GUIDiffusePlusAdditiveFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }
    // diffuse + diffuse
    if (GUIProgramDiffuseComposite.VertexShader == 0 ||
        GUIProgramDiffuseComposite.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffuseComposite ");
        GUIProgramDiffuseComposite = OVRFW::GlProgram::Build(
            GUITwoTextureColorModulatedShaderSrc,
            GUIDiffuseCompositeFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }
    // diffuse color ramped
    if (GUIProgramDiffuseColorRamp.VertexShader == 0 ||
        GUIProgramDiffuseColorRamp.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformUVOffset", OVRFW::ovrProgramParmType::FLOAT_VECTOR2},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"ColorTableOffset", OVRFW::ovrProgramParmType::FLOAT_VECTOR2},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffuseColorRamp ");
        GUIProgramDiffuseColorRamp = OVRFW::GlProgram::Build(
            GUIDiffuseOnlyVertexShaderSrc, GUIColorRampFragmentSrc, uniformParms, uniformCount);
    }
    // diffuse, color ramp, and a specific target for the color ramp
    if (GUIProgramDiffuseColorRampTarget.VertexShader == 0 ||
        GUIProgramDiffuseColorRampTarget.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture2", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"ColorTableOffset", OVRFW::ovrProgramParmType::FLOAT_VECTOR2},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramDiffuseColorRampTarget ");
        GUIProgramDiffuseColorRampTarget = OVRFW::GlProgram::Build(
            GUIDiffuseColorRampTargetVertexShaderSrc,
            GUIColorRampTargetFragmentSrc,
            uniformParms,
            uniformCount);
    }
    if (GUIProgramAlphaDiffuse.VertexShader == 0 || GUIProgramAlphaDiffuse.FragmentShader == 0) {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"UniformFadeDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ALOG("VRMenuMgrLocal::Init - compiling ... GUIProgramAlphaDiffuse ");
        GUIProgramAlphaDiffuse = OVRFW::GlProgram::Build(
            GUITwoTextureColorModulatedShaderSrc,
            GUIAlphaDiffuseFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }

    Initialized = true;
}

//==================================
// VRMenuMgrLocal::Shutdown
//
// Shutdown the VRMenu syatem
void VRMenuMgrLocal::Shutdown() {
    if (!Initialized) {
        return;
    }

    GlProgram::Free(GUIProgramDiffuseOnly);
    GlProgram::Free(GUIProgramDiffuseAlphaDiscard);
    GlProgram::Free(GUIProgramDiffusePlusAdditive);
    GlProgram::Free(GUIProgramDiffuseComposite);
    GlProgram::Free(GUIProgramDiffuseColorRamp);
    GlProgram::Free(GUIProgramDiffuseColorRampTarget);
    GlProgram::Free(GUIProgramAlphaDiffuse);

    Initialized = false;
}

//==================================
// VRMenuMgrLocal::CreateObject
// creates a new menu object
menuHandle_t VRMenuMgrLocal::CreateObject(VRMenuObjectParms const& parms) {
    /// OVR_PERF_TIMER( CreatObject );

    if (!Initialized) {
        ALOGW("VRMenuMgrLocal::CreateObject - manager has not been initialized!");
        return menuHandle_t();
    }

    // validate parameters
    if (parms.Type >= VRMENU_MAX) {
        ALOGW("VRMenuMgrLocal::CreateObject - Invalid menu object type: %i", parms.Type);
        return menuHandle_t();
    }

    // create the handle first so we can enforce setting it be requiring it to be passed to the
    // constructor
    int index = -1;
    if (FreeList.size() > 0) {
        index = FreeList.back();
        FreeList.pop_back();
    } else {
        index = static_cast<int>(ObjectList.size());
    }

    std::uint32_t id = ++CurrentId;
    menuHandle_t handle = ComposeHandle(index, id);
    // ALOG( "VRMenuMgrLocal::CreateObject - handle is %llu", handle.Get() );

    VRMenuObject* obj = new VRMenuObject(parms, handle);
    if (obj == NULL) {
        ALOGW("VRMenuMgrLocal::CreateObject - failed to allocate menu object!");
        assert(
            obj != NULL); // this would be bad -- but we're likely just going to explode elsewhere
        return menuHandle_t();
    }

    obj->Init(GuiSys, parms);

    if (index == static_cast<int>(ObjectList.size())) {
        // we have to grow the array
        ObjectList.push_back(obj);
    } else {
        // insert in existing slot
        assert(ObjectList[index] == NULL);
        ObjectList[index] = obj;
    }

    return handle;
}

//==================================
// VRMenuMgrLocal::FreeObject
// Frees a menu object.  If the object is a child of a parent object, this will
// also remove the child from the parent.
void VRMenuMgrLocal::FreeObject(menuHandle_t const handle) {
    int index;
    std::uint32_t id;
    DecomposeHandle(handle, index, id);
    if (!HandleComponentsAreValid(index, id)) {
        return;
    }
    if (ObjectList[index] == NULL) {
        // already freed
        return;
    }

    VRMenuObject* obj = ObjectList[index];
    // remove this object from its parent's child list
    if (obj->GetParentHandle().IsValid()) {
        VRMenuObject* parentObj = ToObject(obj->GetParentHandle());
        if (parentObj != NULL) {
            parentObj->RemoveChild(*this, handle);
        }
    }

    // free all of this object's children
    obj->FreeChildren(*this);

    delete obj;

    // empty the slot
    ObjectList[index] = NULL;
    // add the index to the free list
    FreeList.push_back(index);

    CondenseList();
}

//==================================
// VRMenuMgrLocal::CondenseList
// keeps the free list from growing too large when items are removed
void VRMenuMgrLocal::CondenseList() {
    // we can only condense the array if we have a significant number of items at the end of the
    // array buffer that are empty (because we cannot move an existing object around without
    // changing its handle, too, which would invalidate any existing references to it). This is the
    // difference between the current size and the array capacity.
    int const MIN_FREE = 64; // very arbitray number
    if (ObjectList.capacity() - ObjectList.size() < MIN_FREE) {
        return;
    }

    // shrink to current size
    ObjectList.resize(ObjectList.size());

    // create a new free list of just indices < the new size
    std::vector<int> newFreeList;
    for (int i = 0; i < static_cast<int>(FreeList.size()); ++i) {
        if (FreeList[i] <= static_cast<int>(ObjectList.size())) {
            newFreeList.push_back(FreeList[i]);
        }
    }
    FreeList = newFreeList;
}

//==================================
// VRMenuMgrLocal::IsValid
bool VRMenuMgrLocal::IsValid(menuHandle_t const handle) const {
    int index;
    std::uint32_t id;
    DecomposeHandle(handle, index, id);
    return HandleComponentsAreValid(index, id);
}

//==================================
// VRMenuMgrLocal::ToObject
// Return the object for a menu handle.
VRMenuObject* VRMenuMgrLocal::ToObject(menuHandle_t const handle) const {
    int index;
    std::uint32_t id;
    DecomposeHandle(handle, index, id);
    if (id == INVALID_MENU_OBJECT_ID) {
        return NULL;
    }
    if (!HandleComponentsAreValid(index, id)) {
        ALOGW("VRMenuMgrLocal::ToObject - invalid handle.");
        return NULL;
    }
    if (index >= static_cast<int>(ObjectList.size())) {
        ALOGW("VRMenuMgrLocal::ToObject - index out of range.");
        return NULL;
    }
    VRMenuObject* object = ObjectList[index];
    if (object == NULL) {
        ALOGW("VRMenuMgrLocal::ToObject - slot empty.");
        return NULL; // this can happen if someone is holding onto the handle of an object that's
                     // been freed
    }
    if (object->GetHandle() != handle) {
        // if the handle of the object in the slot does not match, then the object the handle refers
        // to was deleted and a new object is in the slot
        ALOGW("VRMenuMgrLocal::ToObject - slot mismatch.");
        return NULL;
    }
    return object;
}

/*
static void LogBounds( const char * name, char const * prefix, Bounds3f const & bounds )
{
    ALOG_WITH_TAG( "Spam", "'%s' %s: min( %.2f, %.2f, %.2f ) - max( %.2f, %.2f, %.2f )",
        name, prefix,
        bounds.GetMins().x, bounds.GetMins().y, bounds.GetMins().z,
        bounds.GetMaxs().x, bounds.GetMaxs().y, bounds.GetMaxs().z );
}
*/

/// OVR_PERF_ACCUMULATOR( SubmitForRenderingRecursive_DrawText3D );
/// OVR_PERF_ACCUMULATOR( SubmitForRenderingRecursive_submit );

//==============================
// VRMenuMgrLocal::SubmitForRenderingRecursive
void VRMenuMgrLocal::SubmitForRenderingRecursive(
    OvrGuiSys& guiSys,
    Matrix4f const& centerViewMatrix,
    VRMenuRenderFlags_t const& flags,
    VRMenuObject const* obj,
    Posef const& parentModelPose,
    Vector4f const& parentColor,
    Vector3f const& parentScale,
    Bounds3f& cullBounds,
    SubmittedMenuObject* submitted,
    int const maxIndices,
    int& curIndex,
    int const distanceIndex) const {
    if (curIndex >= maxIndices) {
        // If this happens we're probably not correctly clearing the submitted surfaces each frame
        // OR we've got a LOT of surfaces.
        ALOG("maxIndices = %i, curIndex = %i", maxIndices, curIndex);
        /// assert_WITH_TAG( curIndex < maxIndices, "VrMenu" );
        return;
    }

    // check if this object is hidden
    VRMenuObjectFlags_t const oFlags = obj->GetFlags();
    if (oFlags & VRMENUOBJECT_DONT_RENDER) {
        return;
    }

    Posef const& localPose = obj->GetLocalPose();
    Vector3f const& localScale = obj->GetLocalScale();
    Vector4f const& localColor = obj->GetColor();
    Posef curModelPose;
    Vector4f curColor;
    Vector3f scale;

    VRMenuObject::TransformByParent(
        parentModelPose,
        parentScale,
        parentColor,
        localPose,
        localScale,
        localColor,
        oFlags,
        curModelPose,
        scale,
        curColor);

    assert(obj != NULL);

    cullBounds = obj->GetLocalBounds(guiSys.GetDefaultFont()) * parentScale;

    int submissionIndex = -1;
    if (obj->GetType() != VRMENU_CONTAINER) // containers never render, but their children may
    {
        Posef const& hilightPose = obj->GetHilightPose();
        Posef itemPose(
            curModelPose.Rotation * hilightPose.Rotation,
            curModelPose.Translation +
                (curModelPose.Rotation * parentScale.EntrywiseMultiply(hilightPose.Translation)));
        Matrix4f poseMat(itemPose.Rotation);
        curModelPose = itemPose; // so children like the slider bar caret use our hilight offset and
                                 // don't end up clipping behind us!
        VRMenuRenderFlags_t rFlags = flags;
        if (oFlags & VRMENUOBJECT_FLAG_POLYGON_OFFSET) {
            rFlags |= VRMENU_RENDER_POLYGON_OFFSET;
        }
        if (oFlags & VRMENUOBJECT_FLAG_NO_DEPTH) {
            rFlags |= VRMENU_RENDER_NO_DEPTH;
        }
        if (oFlags & VRMENUOBJECT_FLAG_NO_DEPTH_MASK) {
            rFlags |= VRMENU_RENDER_NO_DEPTH_MASK;
        }
        if (oFlags & VRMENUOBJECT_INSTANCE_TEXT) {
            rFlags |= VRMENU_RENDER_SUBMIT_TEXT_SURFACE;
        }

        if (oFlags & VRMENUOBJECT_FLAG_BILLBOARD) {
            Matrix4f invViewMatrix = centerViewMatrix.Transposed();
            itemPose.Rotation = Quatf(invViewMatrix);
        }

        if (ShowPoses) {
            Matrix4f const itemPoseMat(itemPose);
            guiSys.GetDebugLines().AddLine(
                itemPose.Translation,
                itemPose.Translation + itemPoseMat.GetXBasis() * 0.05f,
                Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
                Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
                0,
                false);
            guiSys.GetDebugLines().AddLine(
                itemPose.Translation,
                itemPose.Translation + itemPoseMat.GetYBasis() * 0.05f,
                Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
                Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
                0,
                false);
            guiSys.GetDebugLines().AddLine(
                itemPose.Translation,
                itemPose.Translation + itemPoseMat.GetZBasis() * 0.05f,
                Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
                Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
                0,
                false);
        }

        if ((oFlags & VRMENUOBJECT_DONT_RENDER_SURFACE) == 0) {
            // the menu object may have zero or more renderable surfaces (if 0, it may draw only
            // text)
            /// OVR_PERF_ACCUMULATE( SubmitForRenderingRecursive_submit );
            submissionIndex = curIndex;
            std::vector<VRMenuSurface> const& surfaces = obj->GetSurfaces();
            for (int i = 0; i < static_cast<int>(surfaces.size()); ++i) {
                VRMenuSurface const& surf = surfaces[i];
                if (surf.IsRenderable()) {
                    SubmittedMenuObject& sub = submitted[curIndex];
                    sub.SurfaceIndex = i;
                    sub.DistanceIndex = distanceIndex >= 0 ? distanceIndex : curIndex;
                    sub.Pose = itemPose;
                    sub.Scale = scale;
                    sub.Flags = rFlags;
                    sub.ColorTableOffset = obj->GetColorTableOffset();
                    sub.SkipAdditivePass = !obj->IsHilighted();
                    sub.Handle = obj->GetHandle();
                    // modulate surface color with parent's current color
                    sub.Color = surf.GetColor() * curColor;
                    sub.Offsets = surf.GetAnchorOffsets();
                    sub.FadeDirection = obj->GetFadeDirection();
                    sub.ClipUVs = surf.GetClipUVs();
                    sub.OffsetUVs = surf.GetOffsetUVs();
#if defined(OVR_BUILD_DEBUG)
                    sub.SurfaceName = surf.GetName();
#endif
                    sub.LocalBounds = cullBounds;
                    curIndex++;
                }
            }
            /// OVR_PERF_TIMER_STOP( SubmitForRenderingRecursive_submit );
        }

        std::string const& text = obj->GetText();
        if ((oFlags & VRMENUOBJECT_DONT_RENDER_TEXT) == 0 && text.length() > 0) {
            Posef const& textLocalPose = obj->GetTextLocalPose();
            Posef curTextPose;
            curTextPose.Translation =
                itemPose.Translation + (itemPose.Rotation * textLocalPose.Translation * scale);
            curTextPose.Rotation = itemPose.Rotation * textLocalPose.Rotation;

            Matrix4f textMat(curTextPose);
            Vector3f textUp = textMat.GetYBasis();
            Vector3f textNormal = textMat.GetZBasis();
            Vector3f position = curTextPose.Translation +
                textNormal * 0.001f; // this is simply to prevent z-fighting right now
            Vector3f textScale = scale * obj->GetTextLocalScale() * obj->GetWrapScale();

            Vector4f textColor = obj->GetTextColor();
            // Apply parent's alpha influence
            textColor.w *= parentColor.w;
            VRMenuFontParms const& fp = obj->GetFontParms();
            fontParms_t fontParms;
            fontParms.AlignHoriz = fp.AlignHoriz;
            fontParms.AlignVert = fp.AlignVert;
            fontParms.Billboard = fp.Billboard;
            fontParms.TrackRoll = fp.TrackRoll;
            fontParms.ColorCenter = fp.ColorCenter;
            fontParms.AlphaCenter = fp.AlphaCenter;

            // We could re-create the text surface here every frame to handle all cases where any
            // font parameters change, but that would be needlessly expensive.
            if (rFlags & VRMENU_RENDER_SUBMIT_TEXT_SURFACE) {
                assert(obj->TextSurface != nullptr);
                Matrix4f scaleMatrix;
                scaleMatrix.M[0][0] = parentScale.x;
                scaleMatrix.M[1][1] = parentScale.y;
                scaleMatrix.M[2][2] = parentScale.z;
                obj->TextSurface->ModelMatrix = scaleMatrix * Matrix4f(curTextPose.Rotation);
                obj->TextSurface->ModelMatrix.SetTranslation(curTextPose.Translation);

                // if we didn't submit anything but we have an instanced text surface, submit an
                // invalid surface so that the text surface will be added to the surface list in
                // BuildDrawSurface
                if (curIndex - submissionIndex == 0) {
                    SubmittedMenuObject& sub = submitted[curIndex];
                    sub.SurfaceIndex = -1;
                    sub.Pose = itemPose;
                    sub.Scale = scale;
                    sub.Flags = rFlags;
                    sub.Handle = obj->GetHandle();
                    sub.Color = parentColor;
                    sub.LocalBounds = cullBounds;
                    curIndex++;
                }
            } else {
                /// OVR_PERF_ACCUMULATE( SubmitForRenderingRecursive_DrawText3D );
                guiSys.GetDefaultFontSurface().DrawText3D(
                    guiSys.GetDefaultFont(),
                    fontParms,
                    position,
                    textNormal,
                    textUp,
                    textScale.x * fp.Scale,
                    textColor,
                    text.c_str());
            }

            if (ShowWrapWidths) {
                // this shows a ruler for the wrap width when rendering text
                Vector3f yofs(0.0f, 0.05f, 0.0f);
                Vector3f xofs(fp.WrapWidth * 0.5f, 0.0f, 0.0f);

                guiSys.GetDebugLines().AddLine(
                    position - xofs - yofs,
                    position - xofs + yofs,
                    Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
                    Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
                    0,
                    false);

                guiSys.GetDebugLines().AddLine(
                    position + xofs - yofs,
                    position + xofs + yofs,
                    Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
                    Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
                    0,
                    false);

                guiSys.GetDebugLines().AddLine(
                    position - xofs,
                    position + xofs,
                    Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
                    Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
                    0,
                    false);
            }
        }
        // ALOG_WITH_TAG( "Spam", "AddPoint for '%s'", text.c_str() );
        // GetDebugLines().AddPoint( curModelPose.Position, 0.05f, 1, true );
    }

    // submit all children
    if (obj->Children.size() > 0) {
        // If this object has the render hierarchy order flag, then it and all its children should
        // be depth sorted based on this object's distance + the inverse of the submission index.
        // (inverted because we want a higher submission index to render after a lower submission
        // index)
        int di = distanceIndex;
        if (di < 0 && (oFlags & VRMenuObjectFlags_t(VRMENUOBJECT_RENDER_HIERARCHY_ORDER))) {
            di = submissionIndex;
        }

        for (int i = 0; i < static_cast<int>(obj->Children.size()); ++i) {
            menuHandle_t childHandle = obj->Children[i];
            VRMenuObject const* child = static_cast<VRMenuObject const*>(ToObject(childHandle));
            if (child == NULL) {
                continue;
            }

            Bounds3f childCullBounds;
            SubmitForRenderingRecursive(
                guiSys,
                centerViewMatrix,
                flags,
                child,
                curModelPose,
                curColor,
                scale,
                childCullBounds,
                submitted,
                maxIndices,
                curIndex,
                di);

            Posef pose = child->GetLocalPose();
            pose.Translation = pose.Translation * scale;
            childCullBounds = Bounds3f::Transform(pose, childCullBounds);
            cullBounds = Bounds3f::Union(cullBounds, childCullBounds);
        }
    }

    obj->SetCullBounds(cullBounds);

    // VRMenuId_t debugId( 297 );
    if (ShowCollision) {
        OvrCollisionPrimitive const* cp = obj->GetCollisionPrimitive();
        if (cp != NULL) {
            cp->DebugRender(guiSys.GetDebugLines(), curModelPose, scale, ShowDebugNormals);
        }
        // if ( obj->GetId() == debugId )
        {
            std::vector<VRMenuSurface> const& surfaces = obj->GetSurfaces();
            for (int i = 0; i < static_cast<int>(surfaces.size()); ++i) {
                VRMenuSurface const& surf = surfaces[i];
                surf.GetTris().DebugRender(
                    guiSys.GetDebugLines(), curModelPose, scale, ShowDebugNormals);
            }
        }
    }
    if (ShowDebugBounds) //&& obj->GetId() == debugId )
    {
        {
            // for debug drawing, put the cull bounds in world space
            // LogBounds( obj->GetText().c_str(), "Transformed CullBounds", myCullBounds );
            guiSys.GetDebugLines().AddBounds(
                curModelPose, cullBounds, Vector4f(0.0f, 1.0f, 1.0f, 1.0f));
        }
        {
            Bounds3f localBounds = obj->GetLocalBounds(guiSys.GetDefaultFont()) * parentScale;
            // LogBounds( obj->GetText().c_str(), "localBounds", localBounds );
            guiSys.GetDebugLines().AddBounds(
                curModelPose, localBounds, Vector4f(1.0f, 0.0f, 0.0f, 1.0f));
            Bounds3f textLocalBounds = obj->GetTextLocalBounds(guiSys.GetDefaultFont());
            Posef hilightPose = obj->GetHilightPose();
            textLocalBounds = Bounds3f::Transform(
                Posef(hilightPose.Rotation, hilightPose.Translation * scale), textLocalBounds);
            guiSys.GetDebugLines().AddBounds(
                curModelPose, textLocalBounds * parentScale, Vector4f(1.0f, 1.0f, 0.0f, 1.0f));
        }
    }

    // draw the hierarchy
    if (ShowDebugHierarchy) {
        fontParms_t fp;
        fp.AlignHoriz = HORIZONTAL_CENTER;
        fp.AlignVert = VERTICAL_CENTER;
        fp.Billboard = true;
#if 0
		VRMenuObject const * parent = ToObject( obj->GetParentHandle() );
		if ( parent != NULL )
		{
			Vector3f itemUp = curModelPose.Rotation * Vector3f( 0.0f, 1.0f, 0.0f );
			Vector3f itemNormal = curModelPose.Rotation * Vector3f( 0.0f, 0.0f, 1.0f );
			fontSurface.DrawTextBillboarded3D( font, fp, curModelPose.Translation, itemNormal, itemUp,
					0.5f, Vector4f( 1.0f, 0.0f, 1.0f, 1.0f ), obj->GetSurfaces()[0] ); //parent->GetText().c_str() );
		}
#endif
        guiSys.GetDebugLines().AddLine(
            parentModelPose.Translation,
            curModelPose.Translation,
            Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
            Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
            5,
            false);
        if (obj->GetSurfaces().size() > 0) {
            guiSys.GetDefaultFontSurface().DrawTextBillboarded3D(
                guiSys.GetDefaultFont(),
                fp,
                curModelPose.Translation,
                0.5f,
                Vector4f(0.8f, 0.8f, 0.8f, 1.0f),
                obj->GetSurfaces()[0].GetName().c_str());
        }
    }
}

//==============================
// VRMenuMgrLocal::SubmitForRendering
// Submits the specified menu object and it's children
void VRMenuMgrLocal::SubmitForRendering(
    OvrGuiSys& guiSys,
    Matrix4f const& centerViewMatrix,
    menuHandle_t const handle,
    Posef const& worldPose,
    VRMenuRenderFlags_t const& flags) {
    // ALOG( "VRMenuMgrLocal::SubmitForRendering" );
    if (NumSubmitted >= MAX_SUBMITTED) {
        ALOGW("Too many menu objects submitted!");
        return;
    }
    VRMenuObject* obj = static_cast<VRMenuObject*>(ToObject(handle));
    if (obj == NULL) {
        return;
    }

    Bounds3f cullBounds;
    SubmitForRenderingRecursive(
        guiSys,
        centerViewMatrix,
        flags,
        obj,
        worldPose,
        Vector4f(1.0f),
        Vector3f(1.0f),
        cullBounds,
        Submitted,
        MAX_SUBMITTED,
        NumSubmitted,
        -1);

    /// OVR_PERF_REPORT( SubmitForRenderingRecursive_submit );
    /// OVR_PERF_REPORT( SubmitForRenderingRecursive_DrawText3D );
}

//==============================
// VRMenuMgrLocal::Finish
void VRMenuMgrLocal::Finish(Matrix4f const& viewMatrix) {
    // free any deleted component objects
    ExecutePendingComponentDeletions();

    if (NumSubmitted == 0) {
        NumToRender = 0;
        return;
    }

    Matrix4f invViewMatrix = viewMatrix.Inverted(); // if the view is never scaled or sheared we
                                                    // could use Transposed() here instead
    Vector3f viewPos = invViewMatrix.GetTranslation();

    // sort surfaces
    SortKeys.resize(NumSubmitted);
    for (int i = 0; i < NumSubmitted; ++i) {
        // The sort key is a combination of the distance squared, reinterpreted as an integer, and
        // the submission index. This sorts on distance while still allowing submission order to
        // contribute in the equal case. The DistanceIndex is used to force a submitted object to
        // use some other object's distance instead of its own, allowing a group of objects to sort
        // against all other object's based on a single distance. Objects uising the same
        // DistanceIndex will then be sorted against each other based only on their submission
        // index.
        float const distSq =
            (Submitted[Submitted[i].DistanceIndex].Pose.Translation - viewPos).LengthSq();
        int64_t sortKey = *reinterpret_cast<unsigned const*>(&distSq);
        SortKeys[i].Key = (sortKey << 32ULL) |
            (NumSubmitted -
             i); // invert i because we want items submitted sooner to be considered "further away"
    }

    std::sort(SortKeys.begin(), SortKeys.end());

    NumToRender = NumSubmitted;
    NumSubmitted = 0;
}

//==============================
// VRMenuMgrLocal::AppendSurfaceList
void VRMenuMgrLocal::AppendSurfaceList(
    Matrix4f const& centerViewMatrix,
    std::vector<ovrDrawSurface>& surfaceList) {
    if (NumToRender == 0) {
        return;
    }

    for (int i = 0; i < NumToRender; ++i) {
        int idx = abs(static_cast<int>(SortKeys[i].Key & 0xFFFFFFFF) - NumToRender);
        SubmittedMenuObject const& cur = Submitted[idx];

        VRMenuObject* obj = static_cast<VRMenuObject*>(ToObject(cur.Handle));
        if (obj != NULL) {
            Vector3f translation(
                cur.Pose.Translation.x + cur.Offsets.x,
                cur.Pose.Translation.y + cur.Offsets.y,
                cur.Pose.Translation.z);

            Matrix4f transform(cur.Pose.Rotation);
            if (cur.Flags & VRMENU_RENDER_BILLBOARD) {
                Matrix4f invViewMatrix = centerViewMatrix.Inverted();
                Vector3f viewPos = invViewMatrix.GetTranslation();

                Vector3f normal = viewPos - cur.Pose.Translation;
                Vector3f up(0.0f, 1.0f, 0.0f);
                float length = normal.Length();
                if (length > MATH_FLOAT_SMALLEST_NON_DENORMAL) {
                    normal.Normalize();
                    if (normal.Dot(up) > MATH_FLOAT_SMALLEST_NON_DENORMAL) {
                        transform =
                            Matrix4f::CreateFromBasisVectors(normal, Vector3f(0.0f, 1.0f, 0.0f));
                    }
                }
            }

            Matrix4f scaleMatrix;
            scaleMatrix.M[0][0] = cur.Scale.x;
            scaleMatrix.M[1][1] = cur.Scale.y;
            scaleMatrix.M[2][2] = cur.Scale.z;

            transform *= scaleMatrix;
            transform.SetTranslation(translation);

            // TODO: do we need to use SubmittedMenuObject at all now that we can use
            // ovrSurfaceDef? We still need to sort for now but ideally SurfaceRenderer
            // would sort all surfaces before rendering.

            obj->BuildDrawSurface(
                *this,
                transform,
                cur.SurfaceName.c_str(),
                cur.SurfaceIndex,
                cur.Color,
                cur.FadeDirection,
                cur.ColorTableOffset,
                cur.ClipUVs,
                cur.OffsetUVs,
                cur.SkipAdditivePass,
                cur.Flags,
                cur.LocalBounds,
                surfaceList);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);

    if (ShowStats) {
        ALOG("VRMenuMgr: submitted %i surfaces", NumToRender);
    }
}

//==============================
// VRMenuMgrLocal::GetGUIGlProgram
GlProgram const* VRMenuMgrLocal::GetGUIGlProgram(eGUIProgramType const programType) const {
    switch (programType) {
        case PROGRAM_DIFFUSE_ONLY:
            return &GUIProgramDiffuseOnly;
        case PROGRAM_DIFFUSE_ALPHA_DISCARD:
            return &GUIProgramDiffuseAlphaDiscard;
        case PROGRAM_ADDITIVE_ONLY:
            return &GUIProgramDiffuseOnly;
        case PROGRAM_DIFFUSE_PLUS_ADDITIVE:
            return &GUIProgramDiffusePlusAdditive;
        case PROGRAM_DIFFUSE_COMPOSITE:
            return &GUIProgramDiffuseComposite;
        case PROGRAM_DIFFUSE_COLOR_RAMP:
            return &GUIProgramDiffuseColorRamp;
        case PROGRAM_DIFFUSE_COLOR_RAMP_TARGET:
            return &GUIProgramDiffuseColorRampTarget;
        case PROGRAM_ALPHA_DIFFUSE:
            return &GUIProgramAlphaDiffuse;
        default:
            /// assert_WITH_TAG( !"Invalid gui program type", "VrMenu" );
            break;
    }
    return NULL;
}

//==============================
// VRMenuMgrLocal::AddComponentToDeletionList
void VRMenuMgrLocal::AddComponentToDeletionList(
    menuHandle_t const ownerHandle,
    VRMenuComponent* component) {
    int index = -1;
    for (int i = 0; i < static_cast<int>(PendingDeletions.size()); ++i) {
        if (PendingDeletions[i].GetOwnerHandle() == ownerHandle) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        index = static_cast<int>(PendingDeletions.size());
        PendingDeletions.push_back(ovrComponentList(ownerHandle));
    }

    PendingDeletions[index].AddComponent(component);
}

//==============================
// VRMenuMgrLocal::ExecutePendingComponentDeletions
void VRMenuMgrLocal::ExecutePendingComponentDeletions() {
    for (int i = static_cast<int>(PendingDeletions.size()) - 1; i >= 0; --i) {
        ovrComponentList& list = PendingDeletions[i];
        VRMenuObject* object = ToObject(list.GetOwnerHandle());
        if (object != nullptr) {
            object->FreeComponents(list);
        }
    }
    PendingDeletions.clear();
}

//==============================
// OvrVRMenuMgr::Create
OvrVRMenuMgr* OvrVRMenuMgr::Create(OvrGuiSys& guiSys) {
    VRMenuMgrLocal* mgr = new VRMenuMgrLocal(guiSys);
    return mgr;
}

//==============================
// OvrVRMenuMgr::Free
void OvrVRMenuMgr::Destroy(OvrVRMenuMgr*& mgr) {
    if (mgr != NULL) {
        mgr->Shutdown();
        delete mgr;
        mgr = NULL;
    }
}

} // namespace OVRFW
