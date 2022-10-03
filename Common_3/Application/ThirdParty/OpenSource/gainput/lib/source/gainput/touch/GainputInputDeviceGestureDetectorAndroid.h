/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 //--------------------------------------------------------------------------------
 // gestureDetector.h
 //--------------------------------------------------------------------------------
#ifndef GESTUREDETECTOR_H_
#define GESTUREDETECTOR_H_

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window_jni.h>

#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputInputDevice.h"
#include "GainputTouchInfo.h"

#include "../../../../../../../../Utilities/Interfaces/ILog.h"
#include "../../../../../../../../Utilities/Math/MathTypes.h"



namespace ndk_helper
{
	//--------------------------------------------------------------------------------
	// Constants
	//--------------------------------------------------------------------------------
	const int32_t TOUCH_SLOP = 24;	

	enum
	{
		GESTURE_STATE_NONE = 0,
		GESTURE_STATE_START = 1,
		GESTURE_STATE_MOVE = 2,
		GESTURE_STATE_END = 4,
		GESTURE_STATE_ACTION = (GESTURE_STATE_START | GESTURE_STATE_END),
		GESTURE_STATE_CANCELED = GESTURE_STATE_ACTION + 1,
	};
	typedef int32_t GESTURE_STATE;

	/******************************************************************
	 * Base class of Gesture Detectors
	 * GestureDetectors handles input events and detect gestures
	 * Note that different detectors may detect gestures with an event at
	 * same time. The caller needs to manage gesture priority accordingly
	 *
	 */
	class GestureDetector
	{
	public:
		GestureDetector()
		{
			mDpFactor = 1.f;
		}

		virtual ~GestureDetector() {}

		virtual void SetConfiguration(AConfiguration* config)
		{
			mDpFactor = 160.f / AConfiguration_getDensity(config);
		}

		virtual GESTURE_STATE Detect(const AInputEvent* motionEvent) = 0;

	protected:
		float mDpFactor;
	};

	/******************************************************************
	 * Tap gesture detector
	 * Returns GESTURE_STATE_ACTION when a tap gesture is detected
	 *
	 */
	class TapDetector : public GestureDetector
	{
	public:
		TapDetector()
		{
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				mFingerStates[i] = GESTURE_STATE_NONE;
				mPointerIds[i] = -1;
				mLastFingerIndexProcessed[i] = false;
			}
		}
		virtual ~TapDetector()
		{
		}

		// Returns GESTURE_STATE_NONE if no finger taps detected, otherwise returns GESTURE_STATE_ACTION if a finger state has changed
		// If GESTURE_STATE_ACTION is returned, caller must check mFingerStates for each finger states
		virtual GESTURE_STATE Detect(const AInputEvent* motionEvent);

		float mDownX[gainput::TouchPointCount];
		float mDownY[gainput::TouchPointCount];
		GESTURE_STATE mFingerStates[gainput::TouchPointCount];

		bool mLastFingerIndexProcessed[gainput::TouchPointCount];

	private:
		int32_t mPointerIds[gainput::TouchPointCount];
	};

	/******************************************************************
	 * Drag gesture detector
	 * Returns drag gesture state when a drag-tap gesture is detected
	 *
	 */
	class DragDetector : public GestureDetector
	{
	public:
		DragDetector()
		{
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				mFingerStates[i] = GESTURE_STATE_NONE;
				mPointerIds[i] = -1;
				mLastFingerIndexProcessed[i] = false;
			}
		}
		virtual ~DragDetector()
		{
		}

		// Returns GESTURE_STATE_NONE if no finger taps detected, otherwise returns GESTURE_STATE_ACTION if a finger state has changed
		// If GESTURE_STATE_ACTION is returned, caller must check mFingerStates for each finger states
		virtual GESTURE_STATE Detect(const AInputEvent* motionEvent);

		float mStartX[gainput::TouchPointCount];
		float mStartY[gainput::TouchPointCount];
		float mCurX[gainput::TouchPointCount];
		float mCurY[gainput::TouchPointCount];
		GESTURE_STATE mFingerStates[gainput::TouchPointCount];

		bool mLastFingerIndexProcessed[gainput::TouchPointCount];

	private:
		int32_t mPointerIds[gainput::TouchPointCount];
	};

}   //namespace ndkHelper
#endif /* GESTUREDETECTOR_H_ */
