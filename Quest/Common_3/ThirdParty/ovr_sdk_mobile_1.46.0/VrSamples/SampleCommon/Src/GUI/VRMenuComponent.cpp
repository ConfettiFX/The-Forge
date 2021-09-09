/************************************************************************************

Filename    :   VRMenuComponent.h
Content     :   Menuing system for VR apps.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VRMenuComponent.h"
#include "VRMenuMgr.h"

namespace OVRFW {

const char* VRMenuComponent::TYPE_NAME = "";

//==============================
// VRMenuComponent::OnEvent
eMsgStatus VRMenuComponent::OnEvent(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    assert(self != NULL);

    //-------------------
    // do any pre work that every event handler must do
    //-------------------

    // LOG_WITH_TAG( "VrMenu", "OnEvent '%s' for '%s'", VRMenuEventTypeNames[event.EventType],
    // self->GetText().ToCStr() );

    // call the overloaded implementation
    eMsgStatus status = OnEvent_Impl(guiSys, vrFrame, self, event);

    //-------------------
    // do any post work that every event handle must do
    //-------------------

    // When new items are added to a menu, the menu sends VRMENU_EVENT_INIT again so that those
    // components will be initialized.  In order to prevent components from getting initialized
    // a second time, components clear their VRMENU_EVENT_INIT flag after each init event.
    if (event.EventType == VRMENU_EVENT_INIT) {
        EventFlags &= ~VRMenuEventFlags_t(VRMENU_EVENT_INIT);
    }

    return status;
}

} // namespace OVRFW
