/************************************************************************************

Filename    :   ControllerGUI.h
Content     :
Created     :   3/8/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "GUI/VRMenu.h"
#include "Appl.h"

namespace OVRFW {

class ovrVrInput;

class ovrControllerGUI : public VRMenu {
   public:
    static char const* MENU_NAME;

    virtual ~ovrControllerGUI() {}

    static ovrControllerGUI* Create(ovrVrInput& vrControllerApp);

   private:
    ovrVrInput& VrInputApp;

   private:
    ovrControllerGUI(ovrVrInput& vrControllerApp)
        : VRMenu(MENU_NAME), VrInputApp(vrControllerApp) {}

    ovrControllerGUI operator=(ovrControllerGUI&) = delete;

    virtual void OnItemEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuId_t const itemId,
        VRMenuEvent const& event) override;

    virtual bool OnKeyEvent_Impl(OvrGuiSys& guiSys, int const keyCode, const int repeatCount)
        override;

    virtual void PostInit_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) override;

    virtual void Open_Impl(OvrGuiSys& guiSys) override;

    virtual void Frame_Impl(OvrGuiSys& guiSys, ovrApplFrameIn const& vrFrame) override;
};

} // namespace OVRFW
