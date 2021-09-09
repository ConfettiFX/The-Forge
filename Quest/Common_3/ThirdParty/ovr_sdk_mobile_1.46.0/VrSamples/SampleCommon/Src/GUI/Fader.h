/************************************************************************************

Filename    :   Fader.h
Content     :   Utility classes for animation based on alpha values
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#pragma once

namespace OVRFW {

//==============================================================
// Fader
// Fades a value between 0 and 1
class Fader {
   public:
    enum eFadeState {
        FADE_NONE, // only the state when the alpha is either 1 or 0
        FADE_PAUSED, // value may be in the middle
        FADE_IN,
        FADE_OUT,
        FADE_MAX
    };

    Fader(float const startAlpha);

    void Update(float const fadeRate, double const deltaSeconds);

    float GetFadeAlpha() const {
        return FadeAlpha;
    }
    eFadeState GetFadeState() const {
        return FadeState;
    }

    void StartFadeIn();
    void StartFadeOut();
    void PauseFade();
    void UnPause();
    void Reset();
    void ForceFinish();
    void SetFadeAlpha(float const fa) {
        FadeAlpha = fa;
    }

    char const* GetFadeStateName(eFadeState const state) const;

    bool IsFadingInOrFadedIn() const;

   private:
    eFadeState FadeState;
    eFadeState PrePauseState;
    float StartAlpha;
    float FadeAlpha;

   private:
    bool IsFadingInOrFadedIn(eFadeState const state) const;
};

//==============================================================
// SineFader
// Maps fade alpha to a sine curve
class SineFader : public Fader {
   public:
    SineFader(float const startAlpha);

    float GetFinalAlpha() const;
};

} // namespace OVRFW
