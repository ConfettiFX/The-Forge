/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#include <string.h>
#include "soloud.h"
#include "soloud_robotizefilter.h"
#include "../../../../OS/Interfaces/IMemory.h"


namespace SoLoud
{
	RobotizeFilterInstance::RobotizeFilterInstance(RobotizeFilter *aParent)
	{
		mParent = aParent;
		initParams(1);
		mParam[WET] = 1.0;
	}

	void RobotizeFilterInstance::fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels)
	{
		unsigned int i;
		for (i = 0; i < aSamples; i++)
		{
			aFFTBuffer[i*2] = 0;
		}
	}

	RobotizeFilter::RobotizeFilter()
	{
	}

	FilterInstance *RobotizeFilter::createInstance()
	{
		return tf_new(RobotizeFilterInstance, this);
	}
}
