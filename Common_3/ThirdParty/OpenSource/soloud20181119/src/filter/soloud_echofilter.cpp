/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

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

#include "soloud.h"
#include "soloud_echofilter.h"
#include "../../../../OS/Interfaces/IMemory.h"

namespace SoLoud
{
	EchoFilterInstance::EchoFilterInstance(EchoFilter *aParent)
	{
		mParent = aParent;
		mBuffer = 0;
		mBufferLength = 0;
		mOffset = 0;
		initParams(1);

	}

	void EchoFilterInstance::filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, double aTime)
	{
		updateParams(aTime);

		if (mBuffer == 0)
		{
			mBufferLength = (int)ceil(mParent->mDelay * aSamplerate);
			mBuffer = (float*)tf_calloc(mBufferLength * aChannels, sizeof(float));
			unsigned int i;
			for (i = 0; i < mBufferLength * aChannels; i++)
			{
				mBuffer[i] = 0;
			}
		}

		float decay = mParent->mDecay;
		unsigned int i, j;
		int prevofs = (mOffset + mBufferLength - 1) % mBufferLength;
		for (i = 0; i < aSamples; i++)
		{
			for (j = 0; j < aChannels; j++)
			{
				int chofs = j * mBufferLength;
				int bchofs = j * aSamples;
				
				mBuffer[mOffset + chofs] = mParent->mFilter * mBuffer[prevofs + chofs] + (1 - mParent->mFilter) * mBuffer[mOffset + chofs];
				
				float n = aBuffer[i + bchofs] + mBuffer[mOffset + chofs] * decay;
				mBuffer[mOffset + chofs] = n;

				aBuffer[i + bchofs] += (n - aBuffer[i + bchofs]) * mParam[0];
			}
			prevofs = mOffset;
			mOffset = (mOffset + 1) % mBufferLength;
		}
	}

	EchoFilterInstance::~EchoFilterInstance()
	{
		tf_free(mBuffer);
	}

	EchoFilter::EchoFilter()
	{
		mDelay = 0.3f;
		mDecay = 0.7f;
		mFilter = 0.0f;
	}

	result EchoFilter::setParams(float aDelay, float aDecay, float aFilter)
	{
		if (aDelay <= 0 || aDecay <= 0 || aFilter < 0 || aFilter >= 1.0f)
			return INVALID_PARAMETER;

		mDecay = aDecay;
		mDelay = aDelay;
		mFilter = aFilter;
		
		return 0;
	}


	FilterInstance *EchoFilter::createInstance()
	{
		return tf_new(EchoFilterInstance, this);
	}
}
