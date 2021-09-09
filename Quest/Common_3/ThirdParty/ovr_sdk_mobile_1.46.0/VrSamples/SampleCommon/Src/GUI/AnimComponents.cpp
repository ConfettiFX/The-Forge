/************************************************************************************

Filename    :   SurfaceAnim_Component.cpp
Content     :   A reusable component for animating VR menu object surfaces.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "AnimComponents.h"

#include "GuiSys.h"
#include "VRMenuObject.h"
#include "VRMenuMgr.h"
#include "System.h"

#include "OVR_Math.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

//================================
// OvrAnimComponent::OvrAnimComponent
OvrAnimComponent::OvrAnimComponent(float const framesPerSecond, bool const looping)
    : VRMenuComponent(VRMenuEventFlags_t()),
      BaseTime(0.0),
      BaseFrame(0),
      CurFrame(0),
      FramesPerSecond(framesPerSecond),
      AnimState(ANIMSTATE_PAUSED),
      Looping(looping),
      ForceVisibilityUpdate(false),
      FractionalFrame(0.0f),
      FloatFrame(0.0) {}

//================================
// OvrAnimComponent::OvrAnimComponent
OvrAnimComponent::OvrAnimComponent() : OvrAnimComponent(30.0f, true) {}

//================================
// OvrAnimComponent::Frame
eMsgStatus OvrAnimComponent::Frame(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    // only recalculate the current frame if playing
    if (AnimState == ANIMSTATE_PLAYING) {
        double timePassed = vrFrame.PredictedDisplayTime - BaseTime;
        FloatFrame = timePassed * FramesPerSecond;
        int totalFrames = (int)floor(FloatFrame);
        FractionalFrame = static_cast<float>(FloatFrame - static_cast<double>(totalFrames));
        int numFrames = GetNumFrames(self);
        int frame = BaseFrame + totalFrames;
        CurFrame = !Looping ? clamp<int>(frame, 0, numFrames - 1) : frame % numFrames;
        SetFrameVisibilities(guiSys, vrFrame, self);
    } else if (ForceVisibilityUpdate) {
        SetFrameVisibilities(guiSys, vrFrame, self);
        ForceVisibilityUpdate = false;
        // don't update any more
        RemoveEventFlags(VRMENU_EVENT_FRAME_UPDATE);
    }

    return MSG_STATUS_ALIVE;
}

//================================
// OvrAnimComponent::SetFrame
void OvrAnimComponent::SetFrame(VRMenuObject* self, int const frameNum) {
    assert(self != NULL);
    CurFrame = clamp<int>(frameNum, 0, GetNumFrames(self) - 1);
    // we must reset the base frame and the current time so that the frame calculation
    // remains correct if we're playing.  If we're not playing, this will cause the
    // next Play() to start from this frame.
    BaseFrame = frameNum;
    BaseTime = GetTimeInSeconds();
    ForceVisibilityUpdate = true; // make sure visibilities are set next frame update

    // make sure we run one frame
    AddEventFlags(VRMENU_EVENT_FRAME_UPDATE);
}

//================================
// OvrAnimComponent::Play
void OvrAnimComponent::Play() {
    AnimState = ANIMSTATE_PLAYING;
    BaseTime = GetTimeInSeconds();
    // on a play we offset the base frame to the current frame so a resume from pause doesn't
    // restart
    BaseFrame = CurFrame;

    AddEventFlags(VRMENU_EVENT_FRAME_UPDATE);
}

//================================
// OvrAnimComponent::Pause
void OvrAnimComponent::Pause() {
    AnimState = ANIMSTATE_PAUSED;
    RemoveEventFlags(VRMENU_EVENT_FRAME_UPDATE);
}

eMsgStatus OvrAnimComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame(guiSys, vrFrame, self, event);
        default:
            assert(!"Event flags mismatch!"); // the constructor is specifying a flag that's not
                                              // handled
            return MSG_STATUS_ALIVE;
    }
}

//==============================================================================================
// OvrSurfaceAnimComponent
//==============================================================================================

const char* OvrSurfaceAnimComponent::TYPE_NAME = "OvrSurfaceAnimComponent";

OvrSurfaceAnimComponent* OvrSurfaceAnimComponent::Create(void* placementBuffer) {
    if (placementBuffer != NULL) {
        return new (placementBuffer) OvrSurfaceAnimComponent;
    }
    return new OvrSurfaceAnimComponent();
}

//================================
// OvrSurfaceAnimComponent::OvrSurfaceAnimComponent
OvrSurfaceAnimComponent::OvrSurfaceAnimComponent(
    float const framesPerSecond,
    bool const looping,
    int const surfacesPerFrame)
    : OvrAnimComponent(framesPerSecond, looping), SurfacesPerFrame(surfacesPerFrame) {}

//================================
// OvrSurfaceAnimComponent::OvrSurfaceAnimComponent
OvrSurfaceAnimComponent::OvrSurfaceAnimComponent() : OvrSurfaceAnimComponent(30.0f, true, 1) {}

//================================
// OvrSurfaceAnimComponent::SetFrameVisibilities
void OvrSurfaceAnimComponent::SetFrameVisibilities(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self) const {
    int minIndex = GetCurFrame() * SurfacesPerFrame;
    int maxIndex = (GetCurFrame() + 1) * SurfacesPerFrame;
    for (int i = 0; i < self->NumSurfaces(); ++i) {
        self->SetSurfaceVisible(i, i >= minIndex && i < maxIndex);
    }
}

//================================
// OvrSurfaceAnimComponent::NumFrames
int OvrSurfaceAnimComponent::GetNumFrames(VRMenuObject* self) const {
    return self->NumSurfaces() / SurfacesPerFrame;
}

//==============================================================
// OvrChildrenAnimComponent
//
const char* OvrTrailsAnimComponent::TYPE_NAME = "OvrChildrenAnimComponent";

OvrTrailsAnimComponent::OvrTrailsAnimComponent(
    float const framesPerSecond,
    bool const looping,
    int const numFrames,
    int const numFramesAhead,
    int const numFramesBehind)
    : OvrAnimComponent(framesPerSecond, looping),
      NumFrames(numFrames),
      FramesAhead(numFramesAhead),
      FramesBehind(numFramesBehind) {}

float OvrTrailsAnimComponent::GetAlphaForFrame(const int frame) const {
    const int currentFrame = GetCurFrame();
    if (frame == currentFrame)
        return 1.0f;

    const float alpha = GetFractionalFrame();
    const float aheadFactor = 1.0f / FramesAhead;
    for (int ahead = 1; ahead <= FramesAhead; ++ahead) {
        if (frame == (currentFrame + ahead)) {
            return (alpha * aheadFactor) + (aheadFactor * (FramesAhead - ahead));
        }
    }

    const float invAlpha = 1.0f - alpha;
    const float behindFactor = 1.0f / FramesBehind;
    for (int behind = 1; behind < FramesBehind; ++behind) {
        if (frame == (currentFrame - behind)) {
            return (invAlpha * behindFactor) + (behindFactor * (FramesBehind - behind));
        }
    }

    return 0.0f;
}

void OvrTrailsAnimComponent::SetFrameVisibilities(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self) const {
    //	LOG( "FracFrame: %f", GetFractionalFrame() );
    for (int i = 0; i < self->NumChildren(); ++i) {
        menuHandle_t childHandle = self->GetChildHandleForIndex(i);
        if (VRMenuObject* childObject = guiSys.GetVRMenuMgr().ToObject(childHandle)) {
            Vector4f color = childObject->GetColor();
            color.w = GetAlphaForFrame(i);
            childObject->SetColor(color);
        }
    }
}

int OvrTrailsAnimComponent::GetNumFrames(VRMenuObject* self) const {
    return NumFrames;
}

} // namespace OVRFW
