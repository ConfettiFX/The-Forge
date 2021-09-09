/************************************************************************************

Filename    :   ActionComponents.h
Content     :   Misc. VRMenu Components to handle actions
Created     :   September 12, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "ActionComponents.h"
#include "Misc/Log.h"

namespace OVRFW {

//==============================
// OvrButton_OnUp::OnEvent_Impl
eMsgStatus OvrButton_OnUp::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    assert(event.EventType == VRMENU_EVENT_TOUCH_UP);
    ALOG("Button id %lli clicked", ButtonId.Get());
    Menu->OnItemEvent(guiSys, vrFrame, ButtonId, event);
    return MSG_STATUS_CONSUMED;
}

} // namespace OVRFW
