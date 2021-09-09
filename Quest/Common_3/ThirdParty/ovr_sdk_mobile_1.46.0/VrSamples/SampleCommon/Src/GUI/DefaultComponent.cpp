/************************************************************************************

Filename    :   DefaultComponent.h
Content     :   A default menu component that handles basic actions most menu items need.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "DefaultComponent.h"

#include "GuiSys.h"

#include "AnimComponents.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

//==============================
//  OvrDefaultComponent::
OvrDefaultComponent::OvrDefaultComponent(
    Vector3f const& hilightOffset,
    float const hilightScale,
    float const fadeDuration,
    float const fadeDelay,
    Vector4f const& textNormalColor,
    Vector4f const& textHilightColor,
    bool const useSurfaceAnimator,
    const bool noHilight)
    : VRMenuComponent(
          VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_DOWN) | VRMENU_EVENT_TOUCH_UP |
          VRMENU_EVENT_FOCUS_GAINED | VRMENU_EVENT_FOCUS_LOST | VRMENU_EVENT_INIT),
      HilightFader(0.0f),
      StartFadeInTime(-1.0),
      StartFadeOutTime(-1.0),
      HilightOffset(hilightOffset),
      HilightScale(hilightScale),
      FadeDuration(fadeDuration),
      FadeDelay(fadeDelay),
      TextNormalColor(textNormalColor),
      TextHilightColor(textHilightColor),
      SuppressText(false),
      UseSurfaceAnimator(useSurfaceAnimator),
      NoHilight(noHilight) {}

//==============================
//  OvrDefaultComponent::OnEvent_Impl
eMsgStatus OvrDefaultComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_INIT:
            // run one frame to set the object initial state
            return Frame(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_FOCUS_GAINED:
            return FocusGained(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_FOCUS_LOST:
            return FocusLost(guiSys, vrFrame, self, event);
        case VRMENU_EVENT_TOUCH_DOWN:
            // DownSoundLimiter.PlaySoundEffect( guiSys, "", 0.1 );
            return MSG_STATUS_ALIVE;
        case VRMENU_EVENT_TOUCH_UP:
            UpSoundLimiter.PlaySoundEffect(guiSys, "sv_panel_touch_up", 0.1);
            return MSG_STATUS_ALIVE;
        default:
            assert(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  OvrDefaultComponent::Frame
eMsgStatus OvrDefaultComponent::Frame(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    double t = vrFrame.PredictedDisplayTime;
    if (StartFadeInTime >= 0.0 && t >= StartFadeInTime) {
        HilightFader.StartFadeIn();
        StartFadeInTime = -1.0;
    } else if (StartFadeOutTime >= 0.0 && t > StartFadeOutTime) {
        HilightFader.StartFadeOut();
        StartFadeOutTime = -1.0;
    }

    float const fadeRate = 1.0f / FadeDuration;
    HilightFader.Update(fadeRate, vrFrame.DeltaSeconds);
    if (HilightFader.GetFadeState() == Fader::FADE_NONE && StartFadeInTime <= 0.0 &&
        StartFadeOutTime <= 0.0) {
        RemoveEventFlags(VRMENU_EVENT_FRAME_UPDATE);
    }

    float const hilightAlpha = HilightFader.GetFinalAlpha();
    Vector3f offset = HilightOffset * hilightAlpha;
    self->SetHilightPose(Posef(Quatf(), offset));

    if (!NoHilight) {
        if (UseSurfaceAnimator) {
            OvrSurfaceAnimComponent* sac = self->GetComponentById<OvrSurfaceAnimComponent>();
            if (sac != NULL) {
                sac->SetFrame(self, hilightAlpha >= 0.5f ? 1 : 0);
            }
        } else {
            // set the highlight alpha on the first visible additive surface. If an animation
            // component is also running, some surfaces may be set to invisible.
            int additiveSurfIndex =
                self->FindSurfaceWithTextureType(SURFACE_TEXTURE_ADDITIVE, true, true);
            if (additiveSurfIndex >= 0) {
                Vector4f surfColor = self->GetSurfaceColor(additiveSurfIndex);
                surfColor.w = hilightAlpha;
                self->SetSurfaceColor(additiveSurfIndex, surfColor);
            }
        }
    }

    float const scale = ((HilightScale - 1.0f) * hilightAlpha) + 1.0f;
    self->SetHilightScale(scale);

    if (SuppressText) {
        self->SetTextColor(Vector4f(0.0f));
    } else {
        Vector4f colorDelta = TextHilightColor - TextNormalColor;
        Vector4f curColor = TextNormalColor + (colorDelta * hilightAlpha);
        self->SetTextColor(curColor);
    }

    return MSG_STATUS_ALIVE;
}

//==============================
//  OvrDefaultComponent::FocusGained
eMsgStatus OvrDefaultComponent::FocusGained(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    // set the hilight flag
    self->SetHilighted(true);
    GazeOverSoundLimiter.PlaySoundEffect(guiSys, "sv_focusgained", 0.1);

    StartFadeOutTime = -1.0;
    StartFadeInTime = FadeDelay + vrFrame.PredictedDisplayTime;

    AddEventFlags(VRMENU_EVENT_FRAME_UPDATE);

    return MSG_STATUS_ALIVE;
}

//==============================
//  OvrDefaultComponent::FocusLost
eMsgStatus OvrDefaultComponent::FocusLost(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    // clear the hilight flag
    self->SetHilighted(false);

    StartFadeInTime = -1.0;
    StartFadeOutTime = FadeDelay + vrFrame.PredictedDisplayTime;

    AddEventFlags(VRMENU_EVENT_FRAME_UPDATE);

    return MSG_STATUS_ALIVE;
}

const char* OvrSurfaceToggleComponent::TYPE_NAME = "OvrSurfaceToggleComponent";

//==============================
//  OvrSurfaceToggleComponent::OnEvent_Impl
eMsgStatus OvrSurfaceToggleComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame(guiSys, vrFrame, self, event);
        default:
            assert(!"Event flags mismatch!");
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  OvrSurfaceToggleComponent::FocusLost
eMsgStatus OvrSurfaceToggleComponent::Frame(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    const int numSurfaces = self->NumSurfaces();
    for (int i = 0; i < numSurfaces; ++i) {
        self->SetSurfaceVisible(i, false);
    }

    if (self->IsHilighted()) {
        self->SetSurfaceVisible(GroupIndex + 1, true);
    } else {
        self->SetSurfaceVisible(GroupIndex, true);
    }
    return MSG_STATUS_ALIVE;
}

} // namespace OVRFW
