/************************************************************************************

Filename    :   Fader.cpp
Content     :   Utility classes for animation based on alpha values
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "Fader.h"

#include "OVR_Math.h"
#include "Misc/Log.h"

#include <assert.h>

namespace OVRFW {

//======================================================================================
// Fader
//======================================================================================

//==============================
// Fader::Fader
Fader::Fader(float const startAlpha)
    : FadeState(FADE_NONE),
      PrePauseState(FADE_NONE),
      StartAlpha(startAlpha),
      FadeAlpha(startAlpha) {}

//==============================
// Fader::Update
void Fader::Update(float const fadeRate, double const deltaSeconds) {
    if (FadeState > FADE_PAUSED && deltaSeconds > 0.0f) {
        float const fadeDelta =
            static_cast<float>(fadeRate * deltaSeconds) * (FadeState == FADE_IN ? 1.0f : -1.0f);
        FadeAlpha += fadeDelta;
        assert(fabs(fadeDelta) > MATH_FLOAT_SMALLEST_NON_DENORMAL);
        if (fabs(fadeDelta) < MATH_FLOAT_SMALLEST_NON_DENORMAL) {
            ALOG("Fader::Update fabs( fadeDelta ) < MATH_FLOAT_SMALLEST_NON_DENORMAL !!!!");
        }
        if (FadeAlpha < MATH_FLOAT_SMALLEST_NON_DENORMAL) {
            FadeAlpha = 0.0f;
            FadeState = FADE_NONE;
            // ALOG( "FadeState = FADE_NONE" );
        } else if (FadeAlpha >= 1.0f - MATH_FLOAT_SMALLEST_NON_DENORMAL) {
            FadeAlpha = 1.0f;
            FadeState = FADE_NONE;
            // ALOG( "FadeState = FADE_NONE" );
        }
        // ALOG( "fadeState = %s, fadeDelta = %.4f, fadeAlpha = %.4f", GetFadeStateName( FadeState
        // ), fadeDelta, FadeAlpha );
    }
}

//==============================
// Fader::StartFadeIn
void Fader::StartFadeIn() {
    // ALOG( "StartFadeIn" );
    FadeState = FADE_IN;
}

//==============================
// Fader::StartFadeOut
void Fader::StartFadeOut() {
    // ALOG( "StartFadeOut" );
    FadeState = FADE_OUT;
}

//==============================
// Fader::PauseFade
void Fader::PauseFade() {
    // ALOG( "PauseFade" );
    PrePauseState = FadeState;
    FadeState = FADE_PAUSED;
}

//==============================
// Fader::UnPause
void Fader::UnPause() {
    FadeState = PrePauseState;
}

//==============================
// Fader::GetFadeStateName
char const* Fader::GetFadeStateName(eFadeState const state) const {
    char const* fadeStateNames[FADE_MAX] = {"FADE_NONE", "FADE_PAUSED", "FADE_IN", "FADE_OUT"};
    return fadeStateNames[state];
}

//==============================
// Fader::Reset
void Fader::Reset() {
    FadeAlpha = StartAlpha;
}

//==============================
// Fader::Reset
void Fader::ForceFinish() {
    FadeAlpha = FadeState == FADE_IN ? 1.0f : 0.0f;
    FadeState = FADE_NONE;
}

//==============================
// Fader::IsFadingInOrFadedIn
bool Fader::IsFadingInOrFadedIn() const {
    if (FadeState == FADE_PAUSED) {
        return IsFadingInOrFadedIn(PrePauseState);
    }
    return IsFadingInOrFadedIn(FadeState);
}

//==============================
// Fader::IsFadingInOrFadedIn
bool Fader::IsFadingInOrFadedIn(eFadeState const state) const {
    switch (FadeState) {
        case FADE_IN:
            return true;
        case FADE_OUT:
            return false;
        case FADE_NONE:
            return FadeAlpha >= 1.0f;
        default:
            assert(false); // this should never be called with state FADE_PAUSE
            return false;
    }
}

//======================================================================================
// SineFader
//======================================================================================

//==============================
// SineFader::SineFader
SineFader::SineFader(float const startAlpha) : Fader(startAlpha) {}

//==============================
// SineFader::GetFinalAlpha
float SineFader::GetFinalAlpha() const {
    // NOTE: pausing will still re-calculate the
    if (GetFadeState() == FADE_NONE) {
        return GetFadeAlpha(); // already clamped
    }
    // map to sine wave
    float radians = (1.0f - GetFadeAlpha()) * MATH_FLOAT_PI; // range 0 to pi
    return (cosf(radians) + 1.0f) * 0.5f; // range 0 to 1
}

} // namespace OVRFW
