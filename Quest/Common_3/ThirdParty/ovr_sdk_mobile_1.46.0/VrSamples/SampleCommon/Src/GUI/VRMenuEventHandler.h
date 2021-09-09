/************************************************************************************

Filename    :   VRMenuFrame.h
Content     :   Menu component for handling hit tests and dispatching events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include "VRMenuObject.h"
#include "VRMenuEvent.h"
#include "GazeCursor.h"
#include "SoundLimiter.h"
#include "FrameParams.h"

namespace OVRFW {

//==============================================================
// VRMenuEventHandler
class VRMenuEventHandler {
   public:
    VRMenuEventHandler();
    ~VRMenuEventHandler();

    void Frame(
        OvrGuiSys& guiSys,
        const ovrApplFrameIn& vrFrame,
        menuHandle_t const& rootHandle,
        OVR::Posef const& menuPose,
        OVR::Matrix4f const& traceMat,
        std::vector<VRMenuEvent>& events);

    void HandleEvents(
        OvrGuiSys& guiSys,
        const ovrApplFrameIn& vrFrame,
        menuHandle_t const rootHandle,
        std::vector<VRMenuEvent> const& events) const;

    void InitComponents(std::vector<VRMenuEvent>& events);
    void Opening(std::vector<VRMenuEvent>& events);
    void Opened(std::vector<VRMenuEvent>& events);
    void Closing(std::vector<VRMenuEvent>& events);
    void Closed(std::vector<VRMenuEvent>& events);

    menuHandle_t GetFocusedHandle() const {
        return FocusedHandle;
    }

   private:
    menuHandle_t FocusedHandle;

    ovrSoundLimiter GazeOverSoundLimiter;
    ovrSoundLimiter DownSoundLimiter;
    ovrSoundLimiter UpSoundLimiter;

   private:
    bool DispatchToComponents(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuEvent const& event,
        VRMenuObject* receiver) const;
    bool DispatchToPath(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuEvent const& event,
        std::vector<menuHandle_t> const& path,
        bool const log) const;
    bool BroadcastEvent(
        OvrGuiSys& guiSys,
        ovrApplFrameIn const& vrFrame,
        VRMenuEvent const& event,
        VRMenuObject* receiver) const;
};

} // namespace OVRFW
