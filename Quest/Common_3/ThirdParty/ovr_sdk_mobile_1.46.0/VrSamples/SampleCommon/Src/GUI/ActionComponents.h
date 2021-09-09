/************************************************************************************

Filename    :   ActionComponents.h
Content     :   Misc. VRMenu Components to handle actions
Created     :   September 12, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#pragma once

#include "VRMenuComponent.h"
#include "VRMenu.h"

namespace OVRFW {

class VRMenu;

//==============================================================
// OvrButton_OnUp
// This is a generic component that forwards a touch up to a menu (normally its owner)
class OvrButton_OnUp : public VRMenuComponent_OnTouchUp {
   public:
    static const int TYPE_ID = 1010;

    OvrButton_OnUp(VRMenu* menu, VRMenuId_t const buttonId)
        : VRMenuComponent_OnTouchUp(), Menu(menu), ButtonId(buttonId) {}

    void SetID(VRMenuId_t newButtonId) {
        ButtonId = newButtonId;
    }

    virtual int GetTypeId() const {
        return TYPE_ID;
    }

   private:
    virtual eMsgStatus OnEvent_Impl(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuObject* self,
        VRMenuEvent const& event);

   private:
    VRMenu* Menu; // menu that holds the button
    VRMenuId_t ButtonId; // id of the button this control handles
};

} // namespace OVRFW
