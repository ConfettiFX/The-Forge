/************************************************************************************

Filename    :   VRMenuEvent.cpp
Content     :   Event class for menu events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "VRMenuEvent.h"

namespace OVRFW {

char const* VRMenuEvent::EventTypeNames[] = {
    "VRMENU_EVENT_FOCUS_GAINED",
    "VRMENU_EVENT_FOCUS_LOST",
    "VRMENU_EVENT_TOUCH_DOWN",
    "VRMENU_EVENT_TOUCH_UP",
    "VRMENU_EVENT_TOUCH_RELATIVE",
    "VRMENU_EVENT_TOUCH_ABSOLUTE",
    "VRMENU_EVENT_SWIPE_FORWARD",
    "VRMENU_EVENT_SWIPE_BACK",
    "VRMENU_EVENT_SWIPE_UP",
    "VRMENU_EVENT_SWIPE_DOWN",
    "VRMENU_EVENT_FRAME_UPDATE",
    "VRMENU_EVENT_SUBMIT_FOR_RENDERING",
    "VRMENU_EVENT_RENDER",
    "VRMENU_EVENT_OPENING",
    "VRMENU_EVENT_OPENED",
    "VRMENU_EVENT_CLOSING",
    "VRMENU_EVENT_CLOSED",
    "VRMENU_EVENT_INIT",
    "VRMENU_EVENT_SELECTED",
    "VRMENU_EVENT_DESELECTED",
    "VRMENU_EVENT_UPDATE_OBJECT",
    "VRMENU_EVENT_SWIPE_COMPLETE",
    "VRMENU_EVENT_ITEM_ACTION_COMPLETE"};

} // namespace OVRFW
