/************************************************************************************

Filename    :   VRMenuComponent.h
Content     :   Menuing system for VR apps.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuObject.h"
#include "VRMenuEvent.h"
#include "SoundLimiter.h"
#include "FrameParams.h"
#include <string>

namespace OVRFW {

enum eMsgStatus {
    MSG_STATUS_CONSUMED, // message was consumed, don't pass to anything else
    MSG_STATUS_ALIVE // continue passing up
};

enum eVRMenuComponentFlags {
    VRMENU_COMPONENT_EVENT_HANDLER, // handles events
    VRMENU_COMPONENT_FRAME_UPDATE, // gets Frame updates
};

typedef OVR::BitFlagsT<eVRMenuComponentFlags> VRMenuComponentFlags_t;

class VRMenuEvent;

//==============================================================
// VRMenuComponent
//
// Base class for menu components
class VRMenuComponent {
   public:
    friend class VRMenuObject;

    static const int TYPE_ID = -1;
    static const char* TYPE_NAME;

    explicit VRMenuComponent(VRMenuEventFlags_t const& eventFlags) : EventFlags(eventFlags) {}
    virtual ~VRMenuComponent() {}

    bool HandlesEvent(VRMenuEventFlags_t const eventFlags) const {
        return (EventFlags & eventFlags) != 0;
    }

    // only called if the event's type flag is set in the component's EventFlags.
    eMsgStatus OnEvent(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

    VRMenuEventFlags_t GetEventFlags() const {
        return EventFlags;
    }

    virtual int GetTypeId() const {
        return TYPE_ID;
    }
    virtual const char* GetTypeName() const {
        return TYPE_NAME;
    }

    const char* GetName() const {
        return Name.c_str();
    }
    void SetName(char const* name) {
        Name = name;
    }

    virtual void SetEnabled(const bool /*enabled*/) {
        assert(false);
    }

   protected:
    void RemoveEventFlags(VRMenuEventFlags_t const& flags) {
        EventFlags &= ~flags;
    }
    void AddEventFlags(VRMenuEventFlags_t const& flags) {
        EventFlags |= flags;
    }
    void ClearEventFlags() {
        EventFlags &= ~EventFlags;
    }

   private:
    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event) = 0;

   private:
    VRMenuEventFlags_t EventFlags; // used to dispatch events to the correct handler
    std::string Name; // only needs to be set if the component will be searched by name
};

//==============================================================
// VRMenuComponent_OnFocusGained
class VRMenuComponent_OnFocusGained : public VRMenuComponent {
   public:
    VRMenuComponent_OnFocusGained()
        : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FOCUS_GAINED)) {}
};

//==============================================================
// VRMenuComponent_OnFocusLost
class VRMenuComponent_OnFocusLost : public VRMenuComponent {
   public:
    VRMenuComponent_OnFocusLost() : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FOCUS_LOST)) {}
};

//==============================================================
// VRMenuComponent_OnTouchDown
class VRMenuComponent_OnTouchDown : public VRMenuComponent {
   public:
    VRMenuComponent_OnTouchDown() : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_DOWN)) {}
};

//==============================================================
// VRMenuComponent_OnTouchUp
class VRMenuComponent_OnTouchUp : public VRMenuComponent {
   public:
    VRMenuComponent_OnTouchUp() : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_UP)) {}
};

//==============================================================
// VRMenuComponent_OnSubmitForRendering
class VRMenuComponent_OnSubmitForRendering : public VRMenuComponent {
   public:
    VRMenuComponent_OnSubmitForRendering()
        : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_SUBMIT_FOR_RENDERING)) {}
};

//==============================================================
// VRMenuComponent_OnRender
class VRMenuComponent_OnRender : public VRMenuComponent {
   public:
    VRMenuComponent_OnRender() : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_RENDER)) {}
};

//==============================================================
// VRMenuComponent_OnTouchRelative
class VRMenuComponent_OnTouchRelative : public VRMenuComponent {
   public:
    VRMenuComponent_OnTouchRelative()
        : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_RELATIVE)) {}
};

//==============================================================
// VRMenuComponent_OnTouchAbsolute
class VRMenuComponent_OnTouchAbsolute : public VRMenuComponent {
   public:
    VRMenuComponent_OnTouchAbsolute()
        : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_ABSOLUTE)) {}
};

} // namespace OVRFW
