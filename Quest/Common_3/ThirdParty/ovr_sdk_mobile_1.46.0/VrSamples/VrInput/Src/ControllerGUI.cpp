/************************************************************************************

Filename    :   ControllerGUI.cpp
Content     :
Created     :   3/8/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrInput.h"
#include "ControllerGUI.h"

#include <android/keycodes.h>

namespace OVRFW {

const char* ovrControllerGUI::MENU_NAME = "controllerGUI";

ovrControllerGUI* ovrControllerGUI::Create(ovrVrInput& vrControllerApp) {
    char const* menuFiles[] = {"apk:///assets/controllergui.txt", nullptr};

    ovrControllerGUI* menu = new ovrControllerGUI(vrControllerApp);
    if (!menu->InitFromReflectionData(
            vrControllerApp.GetGuiSys(),
            vrControllerApp.GetGuiSys().GetFileSys(),
            vrControllerApp.GetGuiSys().GetReflection(),
            vrControllerApp.GetLocale(),
            menuFiles,
            2.0f,
            VRMenuFlags_t(VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP))) {
        delete menu;
        return nullptr;
    }
    return menu;
}

void ovrControllerGUI::OnItemEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuId_t const itemId,
    VRMenuEvent const& event) {}

bool ovrControllerGUI::OnKeyEvent_Impl(
    OvrGuiSys& guiSys,
    int const keyCode,
    const int repeatCount) {
    return (keyCode == AKEYCODE_BACK);
}

void ovrControllerGUI::PostInit_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) {}

void ovrControllerGUI::Open_Impl(OvrGuiSys& guiSys) {}

void ovrControllerGUI::Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) {
    OVR_UNUSED(VrInputApp);
}

} // namespace OVRFW
