/************************************************************************************

Filename    :   OvrGuiSys.cpp
Content     :   Manager for native GUIs.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "GuiSys.h"

#include "Render/GlProgram.h"
#include "Render/GlTexture.h"
#include "Render/GlGeometry.h"
#include "Render/TextureManager.h"

#include "Misc/Log.h"

#include "FrameParams.h"

#include "VRMenu.h"
#include "VRMenuMgr.h"
#include "VRMenuComponent.h"
#include "SoundLimiter.h"
#include "VRMenuEventHandler.h"
#include "DefaultComponent.h"

#include "JniUtils.h"
#include "OVR_JSON.h"

#include "Reflection.h"
#include "ReflectionData.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

inline Vector3f GetViewMatrixForward(Matrix4f const& m) {
    return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]).Normalized();
}

inline Vector3f GetViewMatrixUp(Matrix4f const& m) {
    return Vector3f(-m.M[1][0], -m.M[1][1], -m.M[1][2]).Normalized();
}

inline Vector3f GetViewMatrixLeft(Matrix4f const& m) {
    return Vector3f(-m.M[0][0], -m.M[0][1], -m.M[0][2]).Normalized();
}

inline Vector3f GetViewMatrixPosition(Matrix4f const& m) {
    return m.Inverted().GetTranslation();
}

namespace OVRFW {

class OvrPointTracker {
   public:
    static const int DEFAULT_FRAME_RATE = 60;

    OvrPointTracker(float const rate = 0.1f)
        : LastFrameTime(0.0), Rate(rate), CurPosition(0.0f), FirstFrame(true) {}

    void Update(double const curFrameTime, Vector3f const& newPos) {
        double frameDelta = curFrameTime - LastFrameTime;
        LastFrameTime = curFrameTime;
        float const rateScale =
            static_cast<float>(frameDelta / (1.0 / static_cast<double>(DEFAULT_FRAME_RATE)));
        float const rate = Rate * rateScale;
        if (FirstFrame) {
            CurPosition = newPos;
        } else {
            Vector3f delta = (newPos - CurPosition) * rate;
            if (delta.Length() < 0.001f) {
                // don't allow a denormal to propagate from multiplications of very small numbers
                delta = Vector3f(0.0f);
            }
            CurPosition += delta;
        }
        FirstFrame = false;
    }

    void Reset() {
        FirstFrame = true;
    }
    void SetRate(float const r) {
        Rate = r;
    }

    Vector3f const& GetCurPosition() const {
        return CurPosition;
    }

   private:
    double LastFrameTime;
    float Rate;
    Vector3f CurPosition;
    bool FirstFrame;
};

#define IMPL_CONSOLE_FUNC_BOOL(var_name) \
    ovrLexer lex(parms);                 \
    int v;                               \
    lex.ParseInt(v, 1);                  \
    var_name = v != 0;                   \
    ALOG(#var_name "( '%s' ) = %i", parms, var_name)

//==============================================================
// ovrInfoText
class ovrInfoText {
   public:
    std::string Text; // informative text to show in front of the view
    Vector4f Color; // color of info text
    Vector3f Offset; // offset from center of screen in view space
    long long EndFrame; // time to stop showing text
    OvrPointTracker PointTracker; // smoothly tracks to text ideal location
    OvrPointTracker FPSPointTracker; // smoothly tracks to ideal FPS text location
};

//==============================================================
// OvrGuiSysLocal
class OvrGuiSysLocal : public OvrGuiSys {
   public:
    OvrGuiSysLocal(const void* context);
    virtual ~OvrGuiSysLocal();

    virtual void Init(
        ovrFileSys* fileSysArg,
        OvrGuiSys::SoundEffectPlayer& soundEffectPlayer,
        char const* fontName,
        OvrDebugLines* debugLines) override;
    // Init with a custom font surface for larger-than-normal amounts of text.
    virtual void Init(
        ovrFileSys* fileSysArg,
        OvrGuiSys::SoundEffectPlayer& soundEffectPlayer,
        char const* fontName,
        BitmapFontSurface* fontSurface,
        OvrDebugLines* debugLines) override;

    virtual void Shutdown() override;

    virtual void Frame(ovrApplFrameIn const& vrFrame, Matrix4f const& viewMatrix) override;
    virtual void Frame(
        ovrApplFrameIn const& vrFrame,
        Matrix4f const& viewMatrix,
        Matrix4f const& traceMat) override;

    virtual void AppendSurfaceList(
        Matrix4f const& centerViewMatrix,
        std::vector<ovrDrawSurface>* surfaceList) const override;

    virtual bool OnKeyEvent(int const keyCode, const int action) override;

    virtual void ResetMenuOrientations(Matrix4f const& viewMatrix) override;

    virtual HitTestResult TestRayIntersection(const Vector3f& start, const Vector3f& dir)
        const override;

    virtual void AddMenu(VRMenu* menu) override;
    virtual VRMenu* GetMenu(char const* menuName) const override;
    virtual std::vector<std::string> GetAllMenuNames() const override;
    virtual void DestroyMenu(VRMenu* menu) override;

    virtual void OpenMenu(char const* name) override;

    virtual void CloseMenu(char const* menuName, bool const closeInstantly) override;
    virtual void CloseMenu(VRMenu* menu, bool const closeInstantly) override;

    virtual bool IsMenuActive(char const* menuName) const override;
    virtual bool IsAnyMenuActive() const override;
    virtual bool IsAnyMenuOpen() const override;

    virtual void ShowInfoText(float const duration, const char* fmt, ...) override;
    virtual void ShowInfoText(
        float const duration,
        Vector3f const& offset,
        Vector4f const& color,
        const char* fmt,
        ...) override;

    virtual const void* GetContext() override {
        return Context;
    }
    virtual ovrFileSys& GetFileSys() override {
        return *FileSys;
    }
    virtual OvrVRMenuMgr& GetVRMenuMgr() override {
        return *MenuMgr;
    }
    virtual OvrVRMenuMgr const& GetVRMenuMgr() const override {
        return *MenuMgr;
    };
    virtual OvrGazeCursor& GetGazeCursor() override {
        return *GazeCursor;
    }
    virtual BitmapFont& GetDefaultFont() override {
        return *DefaultFont;
    }
    virtual BitmapFont const& GetDefaultFont() const override {
        return *DefaultFont;
    }
    virtual BitmapFontSurface& GetDefaultFontSurface() override {
        return *DefaultFontSurface;
    }
    virtual OvrDebugLines& GetDebugLines() override {
        return *DebugLines;
    }
    virtual SoundEffectPlayer& GetSoundEffectPlayer() override {
        return *SoundEffectPlayer;
    }
    virtual ovrTextureManager& GetTextureManager() override {
        return *TextureManager;
    } // app's texture manager should always initialize before guisys

    virtual ovrReflection& GetReflection() override {
        return *Reflection;
    }
    virtual ovrReflection const& GetReflection() const override {
        return *Reflection;
    }

   private:
    const void* Context;
    ovrFileSys* FileSys;
    OvrVRMenuMgr* MenuMgr;
    OvrGazeCursor* GazeCursor;
    BitmapFont* DefaultFont;
    BitmapFontSurface* DefaultFontSurface;
    OvrDebugLines* DebugLines;
    OvrGuiSys::SoundEffectPlayer* SoundEffectPlayer;
    ovrReflection* Reflection;
    ovrTextureManager* TextureManager;

    std::vector<VRMenu*> Menus;
    std::vector<VRMenu*> ActiveMenus;

    ovrInfoText InfoText;
    long long LastVrFrameNumber;

    int RecenterCount;

    bool IsInitialized;

    static bool SkipFrame;
    static bool SkipRender;
    static bool SkipSubmit;
    static bool SkipFont;
    static bool SkipCursor;

   private:
    int FindMenuIndex(char const* menuName) const;
    int FindMenuIndex(VRMenu const* menu) const;
    int FindActiveMenuIndex(VRMenu const* menu) const;
    int FindActiveMenuIndex(char const* menuName) const;
    virtual void MakeActive(VRMenu* menu) override;
    void MakeInactive(VRMenu* menu);

    std::vector<VRMenuComponent*> GetDefaultComponents();

    static void GUISkipFrame(void* appPtr, char const* parms) {
        IMPL_CONSOLE_FUNC_BOOL(SkipFrame);
    }
    static void GUISkipRender(void* appPtr, char const* parms) {
        IMPL_CONSOLE_FUNC_BOOL(SkipRender);
    }
    static void GUISkipSubmit(void* appPtr, char const* parms) {
        IMPL_CONSOLE_FUNC_BOOL(SkipSubmit);
    }
    static void GUISkipFont(void* appPtr, char const* parms) {
        IMPL_CONSOLE_FUNC_BOOL(SkipFont);
    }
    static void GUISkipCursor(void* appPtr, char const* parms) {
        IMPL_CONSOLE_FUNC_BOOL(SkipCursor);
    }
};

Vector4f const OvrGuiSys::BUTTON_DEFAULT_TEXT_COLOR(0.098f, 0.6f, 0.96f, 1.0f);
Vector4f const OvrGuiSys::BUTTON_HILIGHT_TEXT_COLOR(1.0f);

//==============================
// OvrGuiSys::Create
OvrGuiSys* OvrGuiSys::Create(const void* context) {
    return new OvrGuiSysLocal(context);
}

//==============================
// OvrGuiSys::Destroy
void OvrGuiSys::Destroy(OvrGuiSys*& guiSys) {
    if (guiSys != nullptr) {
        guiSys->Shutdown();
        delete guiSys;
        guiSys = nullptr;
    }
}

bool OvrGuiSysLocal::SkipFrame = false;
bool OvrGuiSysLocal::SkipRender = false;
bool OvrGuiSysLocal::SkipSubmit = false;
bool OvrGuiSysLocal::SkipFont = false;
bool OvrGuiSysLocal::SkipCursor = false;

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::OvrGuiSysLocal(const void* context)
    : Context(context),
      FileSys(nullptr),
      MenuMgr(nullptr),
      GazeCursor(nullptr),
      DefaultFont(nullptr),
      DefaultFontSurface(nullptr),
      DebugLines(nullptr),
      SoundEffectPlayer(nullptr),
      Reflection(nullptr),
      TextureManager(nullptr),
      LastVrFrameNumber(0),
      RecenterCount(0),
      IsInitialized(false) {}

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::~OvrGuiSysLocal() {
    assert(IsInitialized == false); // Shutdown should already have been called
}

//==============================
// OvrGuiSysLocal::Init
void OvrGuiSysLocal::Init(
    ovrFileSys* fileSysArg,
    OvrGuiSys::SoundEffectPlayer& soundEffectPlayer,
    char const* fontName,
    BitmapFontSurface* fontSurface,
    OvrDebugLines* debugLines) {
    ALOG("OvrGuiSysLocal::Init");

    this->FileSys = fileSysArg;
    Reflection = ovrReflection::Create();

    SoundEffectPlayer = &soundEffectPlayer;
    DebugLines = debugLines;

    MenuMgr = OvrVRMenuMgr::Create(*this);
    MenuMgr->Init(*this);

    GazeCursor = OvrGazeCursor::Create(*(this->FileSys));

    DefaultFont = BitmapFont::Create();

    assert(fontSurface->IsInitialized()); // if you pass a font surface in, you must initialized it
                                          // before calling OvrGuiSysLocal::Init()
    DefaultFontSurface = fontSurface;

    // choose a package to load the font from.
    // select the System Activities package first
    ALOG("GuiSys::Init - fontName is '%s'", fontName);

    if (OVR::OVR_strncmp(fontName, "apk:", 4) == 0) // if full apk path specified use that
    {
        if (!DefaultFont->Load(*(this->FileSys), fontName)) {
            // we can't just do a fatal error here because the /lang/ host is supposed to be System
            // Activities one case of the font failing to load is because System Activities is
            // missing entirely. Instead, we
            /// app->ShowDependencyError();
        }
    } else {
        char fontUri[1024];
        OVR::OVR_sprintf(fontUri, sizeof(fontUri), "apk://font/res/raw/%s", fontName);
        if (!DefaultFont->Load(*(this->FileSys), fontUri)) {
            // we can't just do a fatal error here because the /lang/ host is supposed to be System
            // Activities one case of the font failing to load is because System Activities is
            // missing entirely. Instead, we
            /// app->ShowDependencyError();
        }
    }

    TextureManager = ovrTextureManager::Create();

    IsInitialized = true;

    /*
        app->RegisterConsoleFunction( "GUISkipFrame", OvrGuiSysLocal::GUISkipFrame );
        app->RegisterConsoleFunction( "GUISkipRender", OvrGuiSysLocal::GUISkipRender );
        app->RegisterConsoleFunction( "GUISkipSubmit", OvrGuiSysLocal::GUISkipSubmit );
        app->RegisterConsoleFunction( "GUISkipFont", OvrGuiSysLocal::GUISkipFont );
        app->RegisterConsoleFunction( "GUISkipCursor", OvrGuiSysLocal::GUISkipCursor );
    */
}

//==============================
// OvrGuiSysLocal::Init
void OvrGuiSysLocal::Init(
    ovrFileSys* fileSysArg,
    OvrGuiSys::SoundEffectPlayer& soundEffectPlayer,
    char const* fontName,
    OvrDebugLines* debugLines) {
    BitmapFontSurface* fontSurface = BitmapFontSurface::Create();
    fontSurface->Init(8192);
    Init(fileSysArg, soundEffectPlayer, fontName, fontSurface, debugLines);
}

//==============================
// OvrGuiSysLocal::Shutdown
void OvrGuiSysLocal::Shutdown() {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    IsInitialized = false;

    // pointers in this list will always be in Menus list, too, so just clear it
    for (int i = 0; i < static_cast<int>(ActiveMenus.size()); ++i) {
        ActiveMenus[i] = nullptr;
    }
    ActiveMenus.clear();

    // We need to make sure we delete any child menus here -- it's not enough to just delete them
    // in the destructor of the parent, because they'll be left in the menu list since the
    // destructor has no way to call GuiSys->DestroyMenu() for them.
    for (int i = 0; i < static_cast<int>(Menus.size()); ++i) {
        VRMenu* menu = Menus[i];
        Menus[i] = nullptr;
        menu->Shutdown(*this);
        delete menu;
    }
    Menus.clear();

    BitmapFontSurface::Free(DefaultFontSurface);
    BitmapFont::Free(DefaultFont);
    OvrGazeCursor::Destroy(GazeCursor);
    OvrVRMenuMgr::Destroy(MenuMgr);
    ovrReflection::Destroy(Reflection);
    ovrTextureManager::Destroy(TextureManager);

    DebugLines = nullptr;
    SoundEffectPlayer = nullptr;
}

//==============================
// OvrGuiSysLocal::RepositionMenus
// Reposition any open menus
void OvrGuiSysLocal::ResetMenuOrientations(Matrix4f const& centerViewMatrix) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    for (int i = 0; i < static_cast<int>(Menus.size()); ++i) {
        if (VRMenu* menu = Menus.at(i)) {
            ALOG("ResetMenuOrientation -> '%s'", menu->GetName());
            menu->ResetMenuOrientation(centerViewMatrix);
        }
    }
}

//==============================
// OvrGuiSysLocal::AddMenu
void OvrGuiSysLocal::AddMenu(VRMenu* menu) {
    if (menu == nullptr) {
        ALOGW("Attempted to add null menu!");
        return;
    }
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    int menuIndex = FindMenuIndex(menu->GetName());
    if (menuIndex >= 0) {
        ALOGW("Duplicate menu name '%s'", menu->GetName());
        assert(menuIndex < 0);
    }
    Menus.push_back(menu);
}

//==============================
// OvrGuiSysLocal::GetMenu
VRMenu* OvrGuiSysLocal::GetMenu(char const* menuName) const {
    int menuIndex = FindMenuIndex(menuName);
    if (menuIndex >= 0) {
        return Menus[menuIndex];
    }
    return nullptr;
}

//==============================
// OvrGuiSysLocal::GetAllMenuNames
std::vector<std::string> OvrGuiSysLocal::GetAllMenuNames() const {
    std::vector<std::string> allMenuNames;
    for (int i = 0; i < static_cast<int>(Menus.size()); ++i) {
        allMenuNames.push_back(std::string(Menus[i]->GetName()));
    }
    return allMenuNames;
}

//==============================
// OvrGuiSysLocal::DestroyMenu
void OvrGuiSysLocal::DestroyMenu(VRMenu* menu) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    if (menu == nullptr) {
        return;
    }

    MakeInactive(menu);

    menu->Shutdown(*this);
    delete menu;

    int idx = FindMenuIndex(menu);
    if (idx >= 0) {
        Menus.erase(Menus.cbegin() + idx);
    }
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex(char const* menuName) const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return -1;
    }

    for (int i = 0; i < static_cast<int>(Menus.size()); ++i) {
        if (OVR::OVR_stricmp(Menus[i]->GetName(), menuName) == 0) {
            return i;
        }
    }
    return -1;
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex(VRMenu const* menu) const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return -1;
    }

    for (int i = 0; i < static_cast<int>(Menus.size()); ++i) {
        if (Menus[i] == menu) {
            return i;
        }
    }
    return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex(VRMenu const* menu) const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return -1;
    }

    for (int i = 0; i < static_cast<int>(ActiveMenus.size()); ++i) {
        if (ActiveMenus[i] == menu) {
            return i;
        }
    }
    return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex(char const* menuName) const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return -1;
    }

    for (int i = 0; i < static_cast<int>(ActiveMenus.size()); ++i) {
        if (OVR::OVR_stricmp(ActiveMenus[i]->GetName(), menuName) == 0) {
            return i;
        }
    }
    return -1;
}

//==============================
// OvrGuiSysLocal::MakeActive
void OvrGuiSysLocal::MakeActive(VRMenu* menu) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    int idx = FindActiveMenuIndex(menu);
    if (idx < 0) {
        ActiveMenus.push_back(menu);
    }
}

//==============================
// OvrGuiSysLocal::MakeInactive
void OvrGuiSysLocal::MakeInactive(VRMenu* menu) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    int idx = FindActiveMenuIndex(menu);
    if (idx >= 0) {
        ActiveMenus.erase(ActiveMenus.cbegin() + idx);
    }
}

//==============================
// OvrGuiSysLocal::OpenMenu
void OvrGuiSysLocal::OpenMenu(char const* menuName) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    int menuIndex = FindMenuIndex(menuName);
    if (menuIndex < 0) {
        ALOGW("No menu named '%s'", menuName);
        assert(menuIndex >= 0 && menuIndex < static_cast<int>(Menus.size()));
        return;
    }
    VRMenu* menu = Menus[menuIndex];
    assert(menu != nullptr);

    menu->Open(*this);
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu(VRMenu* menu, bool const closeInstantly) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    assert(menu != nullptr);

    menu->Close(*this, closeInstantly);
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu(char const* menuName, bool const closeInstantly) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return;
    }

    int menuIndex = FindMenuIndex(menuName);
    if (menuIndex < 0) {
        ALOGW("No menu named '%s'", menuName);
        assert(menuIndex >= 0 && menuIndex < static_cast<int>(Menus.size()));
        return;
    }
    VRMenu* menu = Menus[menuIndex];
    CloseMenu(menu, closeInstantly);
}

//==============================
// OvrGuiSysLocal::IsMenuActive
bool OvrGuiSysLocal::IsMenuActive(char const* menuName) const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return false;
    }

    int idx = FindActiveMenuIndex(menuName);
    return idx >= 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuActive() const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return false;
    }

    return ActiveMenus.size() > 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuOpen() const {
    if (!IsInitialized) {
        assert(IsInitialized);
        return false;
    }

    for (int i = 0; i < static_cast<int>(ActiveMenus.size()); ++i) {
        if (ActiveMenus[i]->IsOpenOrOpening()) {
            return true;
        }
    }
    return false;
}

//==============================
// OvrGuiSysLocal::Frame
void OvrGuiSysLocal::Frame(ovrApplFrameIn const& vrFrame, Matrix4f const& centerViewMatrix) {
    Matrix4f traceMat(centerViewMatrix.Inverted());

    Frame(vrFrame, centerViewMatrix, traceMat);
}

//==============================
// OvrGuiSysLocal::Frame
void OvrGuiSysLocal::Frame(
    const ovrApplFrameIn& vrFrame,
    Matrix4f const& centerViewMatrix,
    Matrix4f const& traceMat) {
    /// OVR_PERF_TIMER( OvrGuiSys_Frame );

    if (!IsInitialized || SkipFrame) {
        assert(IsInitialized);
        return;
    }

    Matrix4f lastViewMatrix(vrFrame.HeadPose);

    const int currentRecenterCount = vrFrame.RecenterCount;
    if (currentRecenterCount != RecenterCount) {
        /// TODO - validate this
        // ALOG( "OvrGuiSysLocal::Frame - reorienting menus" );
        /// app->RecenterLastViewMatrix();
        ResetMenuOrientations(lastViewMatrix);
        RecenterCount = currentRecenterCount;
    }

    // draw info text
    if (InfoText.EndFrame >= LastVrFrameNumber) {
        Vector3f viewPos = GetViewMatrixPosition(lastViewMatrix);
        Vector3f viewFwd = GetViewMatrixForward(lastViewMatrix);
        Vector3f viewUp(0.0f, 1.0f, 0.0f);
        Vector3f viewLeft = viewUp.Cross(viewFwd);
        Vector3f newPos = viewPos + viewFwd * InfoText.Offset.z + viewUp * InfoText.Offset.y +
            viewLeft * InfoText.Offset.x;
        InfoText.PointTracker.Update(vrFrame.PredictedDisplayTime, newPos);

        fontParms_t fp;
        fp.AlignHoriz = HORIZONTAL_CENTER;
        fp.AlignVert = VERTICAL_CENTER;
        fp.Billboard = true;
        fp.TrackRoll = false;
        DefaultFontSurface->DrawTextBillboarded3Df(
            *DefaultFont,
            fp,
            InfoText.PointTracker.GetCurPosition(),
            1.0f,
            InfoText.Color,
            InfoText.Text.c_str());
    }

    {
        /// OVR_PERF_TIMER( OvrGuiSys_Frame_Menus_Frame );
        // go backwards through the list so we can use unordered remove when a menu finishes closing
        for (int i = static_cast<int>(ActiveMenus.size()) - 1; i >= 0; --i) {
            VRMenu* curMenu = ActiveMenus[i];
            assert(curMenu != nullptr);

            curMenu->Frame(*this, vrFrame, centerViewMatrix, traceMat);

            if (curMenu->GetCurMenuState() == VRMenu::MENUSTATE_CLOSED) {
                // remove from the active list
                ActiveMenus[i] = ActiveMenus.back();
                ActiveMenus.pop_back();
                continue;
            }
        }
    }

    {
        /// OVR_PERF_TIMER( OvrGuiSys_GazeCursor_Frame );
        GazeCursor->Frame(centerViewMatrix, traceMat, vrFrame.DeltaSeconds);
    }

    {
        /// OVR_PERF_TIMER( OvrGuiSys_Frame_Font_Finish );
        DefaultFontSurface->Finish(centerViewMatrix);
    }

    {
        /// OVR_PERF_TIMER( OvrGuiSys_Frame_MenuMgr_Finish );
        MenuMgr->Finish(centerViewMatrix);
    }

    LastVrFrameNumber = vrFrame.FrameIndex;
}

//==============================
// OvrGuiSysLocal::AppendSurfaceList
void OvrGuiSysLocal::AppendSurfaceList(
    Matrix4f const& centerViewMatrix,
    std::vector<ovrDrawSurface>* surfaceList) const {
    if (!IsInitialized || SkipRender) {
        assert(IsInitialized);
        return;
    }

    if (!SkipSubmit) {
        MenuMgr->AppendSurfaceList(centerViewMatrix, *surfaceList);
    }

    if (!SkipFont) {
        DefaultFontSurface->AppendSurfaceList(*DefaultFont, *surfaceList);
    }

    if (!SkipCursor) {
        GazeCursor->AppendSurfaceList(*surfaceList);
    }
}

//==============================
// OvrGuiSysLocal::OnKeyEvent
bool OvrGuiSysLocal::OnKeyEvent(int const keyCode, const int action) {
    if (!IsInitialized) {
        assert(IsInitialized);
        return false;
    }
    for (int i = 0; i < static_cast<int>(ActiveMenus.size()); ++i) {
        VRMenu* curMenu = ActiveMenus[i];
        if (curMenu && curMenu->OnKeyEvent(*this, keyCode, action)) {
            ALOG("VRMenu '%s' consumed key event", curMenu->GetName());
            return true;
        }
    }
    // we ignore other keys in the app menu for now
    return false;
}

void OvrGuiSysLocal::ShowInfoText(float const duration, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    InfoText.Text = buffer;
    InfoText.Color = Vector4f(1.0f);
    InfoText.Offset = Vector3f(0.0f, 0.0f, 1.5f);
    InfoText.PointTracker.Reset();
    InfoText.EndFrame = LastVrFrameNumber + (long long)(duration * 60.0f) + 1;
}

void OvrGuiSysLocal::ShowInfoText(
    float const duration,
    Vector3f const& offset,
    Vector4f const& color,
    const char* fmt,
    ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    InfoText.Text = buffer;
    InfoText.Color = color;
    if (offset != InfoText.Offset || InfoText.EndFrame < LastVrFrameNumber) {
        InfoText.PointTracker.Reset();
    }
    InfoText.Offset = offset;
    InfoText.EndFrame = LastVrFrameNumber + (long long)(duration * 60.0f) + 1;
}

bool OvrGuiSys::ovrDummySoundEffectPlayer::Has(const char* name) const {
    ALOG("ovrDummySoundEffectPlayer::Has( %s )", name);
    return false;
}

void OvrGuiSys::ovrDummySoundEffectPlayer::Play(const char* name) {
    ALOG("ovrDummySoundEffectPlayer::Play( %s )", name);
}

void OvrGuiSys::ovrDummySoundEffectPlayer::Stop(const char* name) {
    ALOG("ovrDummySoundEffectPlayer::Stop( %s )", name);
}

void OvrGuiSys::ovrDummySoundEffectPlayer::LoadSoundAsset(const char* name) {
    ALOG("ovrDummySoundEffectPlayer::LoadSoundAsset( %s )", name);
}

//==============================
// OvrGuiSysLocal::TestRayIntersection
HitTestResult OvrGuiSysLocal::TestRayIntersection(const Vector3f& start, const Vector3f& dir)
    const {
    HitTestResult result;

    for (int i = static_cast<int>(ActiveMenus.size()) - 1; i >= 0; --i) {
        VRMenu* curMenu = ActiveMenus[i];
        if (curMenu == nullptr) {
            continue;
        }
        VRMenuObject* root = GetVRMenuMgr().ToObject(curMenu->GetRootHandle());
        if (root == nullptr) {
            continue;
        }

        HitTestResult r;
        menuHandle_t hitHandle = root->HitTest(
            *this, curMenu->GetMenuPose(), start, dir, ContentFlags_t(CONTENT_SOLID), r);
        if (hitHandle.IsValid() && r.t < result.t) {
            result = r;
            result.RayStart = start;
            result.RayDir = dir;
        }
    }
    return result;
}

} // namespace OVRFW
