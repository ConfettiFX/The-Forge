/************************************************************************************

Filename    :   VRMenu.h
Content     :   Class that implements the basic framework for a VR menu, holds all the
                components for a single menu, and updates the VR menu event handler to
                process menu events for a single menu.
Created     :   June 30, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include "OVR_BitFlags.h"
#include "OVR_Std.h"

#include "FrameParams.h"
#include "Misc/Log.h"

#include "VRMenuObject.h"
#include "SoundLimiter.h"
#include "GazeCursor.h"

namespace OVRFW {

class BitmapFont;
class BitmapFontSurface;
class VRMenuEventHandler;
class OvrGuiSys;

enum eVRMenuFlags {
    // initially place the menu in front of the user's view on the horizon plane but do not move to
    // follow the user's gaze.
    VRMENU_FLAG_PLACE_ON_HORIZON,
    // place the menu directly in front of the user's view on the horizon plane each frame. The user
    // will sill be able to gaze track vertically.
    VRMENU_FLAG_TRACK_GAZE_HORIZONTAL,
    // place the menu directly in front of the user's view each frame -- this means gaze tracking
    // won't be available since the user can't look at different parts of the menu.
    VRMENU_FLAG_TRACK_GAZE,
    // If set, just consume the back key but do nothing with it (for warning menu that must be
    // accepted)
    VRMENU_FLAG_BACK_KEY_DOESNT_EXIT,
    // If set, a short-press of the back key will exit the app when in this menu
    VRMENU_FLAG_BACK_KEY_EXITS_APP,
    // If set, return false so short press is passed to app
    VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP
};

typedef OVR::BitFlagsT<eVRMenuFlags, int> VRMenuFlags_t;

//==============================================================
// VRMenu
class VRMenu {
   public:
    friend class OvrGuiSysLocal;

    enum eMenuState {
        MENUSTATE_OPENING,
        MENUSTATE_OPEN,
        MENUSTATE_CLOSING,
        MENUSTATE_CLOSED,
        MENUSTATE_MAX
    };

    static char const* MenuStateNames[MENUSTATE_MAX];

    static VRMenuId_t GetRootId();

    static VRMenu* Create(char const* menuName);

    bool InitFromReflectionData(
        OvrGuiSys& guiSys,
        ovrFileSys& fileSys,
        ovrReflection& refl,
        ovrLocale const& locale,
        char const* fileNames[],
        float const menuDistance,
        VRMenuFlags_t const& flags);

    void Init(
        OvrGuiSys& guiSys,
        float const menuDistance,
        VRMenuFlags_t const& flags,
        std::vector<VRMenuComponent*> comps = std::vector<VRMenuComponent*>());
    void InitWithItems(
        OvrGuiSys& guiSys,
        float const menuDistance,
        VRMenuFlags_t const& flags,
        std::vector<VRMenuObjectParms const*>& itemParms);

    void AddItems(
        OvrGuiSys& guiSys,
        std::vector<VRMenuObjectParms const*>& itemParms,
        menuHandle_t parentHandle,
        bool const recenter);
    void Shutdown(OvrGuiSys& guiSys);
    void Frame(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        OVR::Matrix4f const& viewMatrix,
        OVR::Matrix4f const& traceMat);

    bool OnKeyEvent(OvrGuiSys& guiSys, int const keyCode, const int action);

    void Open(OvrGuiSys& guiSys);
    void Close(OvrGuiSys& guiSys, bool const instant = false);

    bool IsOpen() const {
        return CurMenuState == MENUSTATE_OPEN;
    }
    bool IsOpenOrOpening() const {
        return CurMenuState == MENUSTATE_OPEN || CurMenuState == MENUSTATE_OPENING ||
            NextMenuState == MENUSTATE_OPEN || NextMenuState == MENUSTATE_OPENING;
    }

    bool IsClosed() const {
        return CurMenuState == MENUSTATE_CLOSED;
    }
    bool IsClosedOrClosing() const {
        return CurMenuState == MENUSTATE_CLOSED || CurMenuState == MENUSTATE_CLOSING ||
            NextMenuState == MENUSTATE_CLOSED || NextMenuState == MENUSTATE_CLOSING;
    }

    void OnItemEvent(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuId_t const itemId,
        class VRMenuEvent const& event);

    virtual void OnJobCompleted(OvrGuiSys&, class ovrJob&) {}

    // Clients can query the current menu state but to change it they must use
    // SetNextMenuState() and allow the menu to switch states when it can.
    eMenuState GetCurMenuState() const {
        return CurMenuState;
    }

    // Returns the next menu state.
    eMenuState GetNextMenuState() const {
        return NextMenuState;
    }
    // Sets the next menu state.  The menu will switch to that state at the next
    // opportunity.
    void SetNextMenuState(eMenuState const s) {
        NextMenuState = s;
    }

    menuHandle_t GetRootHandle() const {
        return RootHandle;
    }
    menuHandle_t GetFocusedHandle() const;
    OVR::Posef const& GetMenuPose() const {
        return MenuPose;
    }
    void SetMenuPose(OVR::Posef const& pose) {
        MenuPose = pose;
    }

    menuHandle_t HandleForName(OvrVRMenuMgr const& menuMgr, char const* name) const;
    menuHandle_t HandleForId(OvrVRMenuMgr const& menuMgr, VRMenuId_t const id) const;

    VRMenuObject* ObjectForName(OvrGuiSys const& guiSys, char const* name) const;
    VRMenuObject* ObjectForId(OvrGuiSys const& guiSys, VRMenuId_t const id) const;

    VRMenuId_t IdForName(OvrGuiSys const& guiSys, char const* name) const;

    char const* GetName() const {
        return Name.c_str();
    }
    bool IsMenu(char const* menuName) const {
        return OVR::OVR_stricmp(Name.c_str(), menuName) == 0;
    }

    // Use an arbitrary view matrix. This is used when updating the menus and passing the current
    // matrix
    void RepositionMenu(OVR::Matrix4f const& viewMatrix);

    // Reset the MenuPose orientation - for now we assume identity orientation as the basis for all
    // menus
    void ResetMenuOrientation(OVR::Matrix4f const& viewMatrix);

    VRMenuFlags_t const& GetFlags() const {
        return Flags;
    }
    void SetFlags(VRMenuFlags_t const& flags) {
        Flags = flags;
    }

    OVR::Posef CalcMenuPosition(OVR::Matrix4f const& viewMatrix) const;

    OVR::Posef CalcMenuPositionOnHorizon(OVR::Matrix4f const& viewMatrix) const;

    float GetMenuDistance() const {
        return MenuDistance;
    }
    void SetMenuDistance(float const& menuDistance) {
        MenuDistance = menuDistance;
    }

    // Set the selected state of an object AND send an event for selection
    void SetSelected(VRMenuObject* obj, bool const selected);
    void SetSelected(OvrGuiSys& guiSys, VRMenuId_t const itemId, bool const selected);

   protected:
    // only derived classes can instance a VRMenu
    VRMenu(char const* name);
    // only GuiSysLocal can free a VRMenu
    virtual ~VRMenu();

   private:
    menuHandle_t
        RootHandle; // handle to the menu's root item (to which all other items must be attached)

    eMenuState CurMenuState; // the current menu state
    eMenuState NextMenuState; // the state the menu should move to next

    OVR::Posef MenuPose; // world-space position and orientation of this menu's root item

    ovrSoundLimiter OpenSoundLimiter; // prevents the menu open sound from playing too often
    ovrSoundLimiter CloseSoundLimiter; // prevents the menu close sound from playing too often

    VRMenuEventHandler* EventHandler;
    std::vector<VRMenuEvent> PendingEvents; // events pending since the last frame

    std::string Name; // name of the menu

    VRMenuFlags_t Flags; // various flags that dictate menu behavior
    float MenuDistance; // distance from eyes
    bool IsInitialized; // true if Init was called
    bool ComponentsInitialized; // true if init message has been sent to components
    bool PostInitialized; // true if post init was run

   private:
    // return true to continue with normal initialization (adding items) or false to skip.
    virtual bool Init_Impl(
        OvrGuiSys& guiSys,
        float const menuDistance,
        VRMenuFlags_t const& flags,
        std::vector<VRMenuObjectParms const*>& itemParms);
    // called once from Frame() after all child items have received the init event.
    virtual void PostInit_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame);
    virtual void Shutdown_Impl(OvrGuiSys& guiSys);
    virtual void Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame);
    // return true if the key was consumed.
    virtual bool OnKeyEvent_Impl(OvrGuiSys& guiSys, int const keyCode, const int action);
    virtual void Open_Impl(OvrGuiSys& guiSys);
    virtual void Close_Impl(OvrGuiSys& guiSys);
    virtual void OnItemEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuId_t const itemId,
        class VRMenuEvent const& event);
    virtual void ResetMenuOrientation_Impl(OVR::Matrix4f const& viewMatrix);

    // return true when finished opening/closing - allowing derived menus to animate etc. during
    // open/close
    virtual bool IsFinishedOpening() const {
        return true;
    }
    virtual bool IsFinishedClosing() const {
        return true;
    }
};

} // namespace OVRFW
