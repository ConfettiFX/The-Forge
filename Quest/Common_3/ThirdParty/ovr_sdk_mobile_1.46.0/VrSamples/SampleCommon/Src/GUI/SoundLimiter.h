/************************************************************************************

Filename    :   SoundLimiter.cpp
Content     :   Utility class for limiting how often sounds play.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

namespace OVRFW {

class OvrGuiSys;

//==============================================================
// ovrSoundLimiter
class ovrSoundLimiter {
   public:
    ovrSoundLimiter() : LastPlayTime(0.0) {}

    void PlaySoundEffect(OvrGuiSys& guiSys, char const* soundName, double const limitSeconds);
    // Checks if menu specific sounds exists before playing the default vrappframework sound passed
    // in.
    void PlayMenuSound(
        OvrGuiSys& guiSys,
        char const* menuName,
        char const* soundName,
        double const limitSeconds);

   private:
    double LastPlayTime;
};

} // namespace OVRFW
