/************************************************************************************

Filename    :   SoundLimiter.cpp
Content     :   Utility class for limiting how often sounds play.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#include "SoundLimiter.h"
#include "GuiSys.h"

#include "OVR_Types.h"
#include "OVR_TypesafeNumber.h"
#include "OVR_BitFlags.h"
#include "OVR_Math.h"
#include "OVR_Std.h"

#include "System.h"

namespace OVRFW {

//==============================
// ovrSoundLimiter::PlaySound
void ovrSoundLimiter::PlaySoundEffect(
    OvrGuiSys& guiSys,
    char const* soundName,
    double const limitSeconds) {
    double curTime = GetTimeInSeconds();
    double t = curTime - LastPlayTime;
    // LOG_WITH_TAG( "VrMenu", "PlaySoundEffect( '%s', %.2f ) - t == %.2f : %s", soundName,
    // limitSeconds, t, t >= limitSeconds ? "PLAYING" : "SKIPPING" );
    if (t >= limitSeconds) {
        guiSys.GetSoundEffectPlayer().Play(soundName);
        LastPlayTime = curTime;
    }
}

void ovrSoundLimiter::PlayMenuSound(
    OvrGuiSys& guiSys,
    char const* appendKey,
    char const* soundName,
    double const limitSeconds) {
    char overrideSound[1024];
    OVR::OVR_sprintf(overrideSound, 1024, "%s_%s", appendKey, soundName);

    if (guiSys.GetSoundEffectPlayer().Has(overrideSound)) {
        PlaySoundEffect(guiSys, overrideSound, limitSeconds);
    } else {
        PlaySoundEffect(guiSys, soundName, limitSeconds);
    }
}

} // namespace OVRFW
