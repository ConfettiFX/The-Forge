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

#include "GainputInputDeviceGestureDetectorAndroid.h"

 //--------------------------------------------------------------------------------
 // gestureDetector.cpp
 //--------------------------------------------------------------------------------
namespace ndk_helper
{
	//--------------------------------------------------------------------------------
	// TapDetector
	//--------------------------------------------------------------------------------
	GESTURE_STATE TapDetector::Detect(const AInputEvent* motionEvent)
	{
		const size_t pntrCnt = AMotionEvent_getPointerCount(motionEvent);

		if (pntrCnt < 1)
			return GESTURE_STATE_NONE;

		for (size_t i = 0; i < gainput::TouchPointCount; ++i)
		{
			mLastFingerIndexProcessed[i] = false;
		}

		GESTURE_STATE ret = GESTURE_STATE_NONE;

		const int32_t action = AMotionEvent_getAction(motionEvent);
		const uint32_t flags = action & AMOTION_EVENT_ACTION_MASK;
		const int32_t iIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		
		switch (flags)
		{
		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_POINTER_DOWN:
		{
			// We need to find a free slot first
			size_t pointerIdArrIndex = -1;
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (-1 == mPointerIds[i])
				{
					pointerIdArrIndex = i;
					break;
				}
			}

			// No free slot?  Then we exceeded max supported taps... so just return
			if (-1 == pointerIdArrIndex)
				return GESTURE_STATE_NONE;

			mPointerIds[pointerIdArrIndex] = AMotionEvent_getPointerId(motionEvent, iIndex);
			mDownX[pointerIdArrIndex] = AMotionEvent_getX(motionEvent, iIndex);
			mDownY[pointerIdArrIndex] = AMotionEvent_getY(motionEvent, iIndex);
			mFingerStates[pointerIdArrIndex] = GESTURE_STATE_START;
			mLastFingerIndexProcessed[pointerIdArrIndex] = true;
			ret = GESTURE_STATE_ACTION;
		}
		break;

		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP:
		{
			const int32_t upPntrId = AMotionEvent_getPointerId(motionEvent, iIndex);

			// Got to find if we're tracking it
			size_t pointerIdArrIndex = -1;
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (upPntrId == mPointerIds[i])
				{
					pointerIdArrIndex = i;
					break;
				}
			}

			// Not tracking it?  just return
			if (-1 == pointerIdArrIndex)
				return GESTURE_STATE_NONE;

			if (mFingerStates[pointerIdArrIndex] != GESTURE_STATE_CANCELED) // make sure it wasn't already canceled
			{
				const float x = AMotionEvent_getX(motionEvent, iIndex) - mDownX[pointerIdArrIndex];
				const float y = AMotionEvent_getY(motionEvent, iIndex) - mDownY[pointerIdArrIndex];
				if (x * x + y * y < TOUCH_SLOP * TOUCH_SLOP * mDpFactor)
				{
					mFingerStates[pointerIdArrIndex] = GESTURE_STATE_END;
					ret = GESTURE_STATE_ACTION;
					mLastFingerIndexProcessed[pointerIdArrIndex] = true;
				}
				else
				{
					mFingerStates[pointerIdArrIndex] = GESTURE_STATE_CANCELED;
					ret = GESTURE_STATE_ACTION;
					mLastFingerIndexProcessed[pointerIdArrIndex] = true;
				}
			}

			// We're done tracking... reset id for this finger so we can redetect down actions
			mPointerIds[pointerIdArrIndex] = -1;
		}
		break;

		case AMOTION_EVENT_ACTION_CANCEL:
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (mPointerIds[i]!= -1)
				{
					mFingerStates[i] = GESTURE_STATE_CANCELED;
					mPointerIds[i] = -1;
					mLastFingerIndexProcessed[i] = true;
				}
			}
			
			ret = GESTURE_STATE_ACTION;
			
			break;
		}

		return ret;
	}

	//--------------------------------------------------------------------------------
	// DragDetector
	//--------------------------------------------------------------------------------
	GESTURE_STATE DragDetector::Detect(const AInputEvent* motionEvent)
	{
		const size_t pntrCnt = AMotionEvent_getPointerCount(motionEvent);

		if (pntrCnt < 1)
			return false;

		for (size_t i = 0; i < gainput::TouchPointCount; ++i)
		{
			mLastFingerIndexProcessed[i] = false;
		}

		GESTURE_STATE ret = GESTURE_STATE_NONE;

		const int32_t action = AMotionEvent_getAction(motionEvent);
		const uint32_t flags = action & AMOTION_EVENT_ACTION_MASK;
		const int32_t iIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

		switch (flags)
		{
		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_POINTER_DOWN:
		{
			// We need to find a free slot first
			size_t pointerIdArrIndex = -1;
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (-1 == mPointerIds[i])
				{
					pointerIdArrIndex = i;
					break;
				}
			}

			// No free slot?  Then we exceeded max supported taps... so just return
			if (-1 == pointerIdArrIndex)
				return GESTURE_STATE_NONE;

			mPointerIds[pointerIdArrIndex] = AMotionEvent_getPointerId(motionEvent, iIndex);
			mStartX[pointerIdArrIndex] = mCurX[pointerIdArrIndex] = AMotionEvent_getX(motionEvent, iIndex);
			mStartY[pointerIdArrIndex] = mCurY[pointerIdArrIndex] = AMotionEvent_getY(motionEvent, iIndex);
			mFingerStates[pointerIdArrIndex] = GESTURE_STATE_START;
			mLastFingerIndexProcessed[pointerIdArrIndex] = true;
			ret = GESTURE_STATE_ACTION;
		}
		break;

		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP:
		{
			const int32_t upPntrId = AMotionEvent_getPointerId(motionEvent, iIndex);

			// Got to find if we're tracking it
			size_t pointerIdArrIndex = -1;
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (upPntrId == mPointerIds[i])
				{
					pointerIdArrIndex = i;
					break;
				}
			}

			// Not tracking it?  just return
			if (-1 == pointerIdArrIndex)
				return GESTURE_STATE_NONE;

			if (mFingerStates[pointerIdArrIndex] != GESTURE_STATE_CANCELED) // make sure it wasn't already canceled
			{
				mFingerStates[pointerIdArrIndex] = GESTURE_STATE_END;
				ret = GESTURE_STATE_ACTION;
				mLastFingerIndexProcessed[pointerIdArrIndex] = true;
			}

			// We're done tracking... reset id for this finger so we can redetect down actions
			mPointerIds[pointerIdArrIndex] = -1;
		}
		break;

		case AMOTION_EVENT_ACTION_CANCEL:
			for (size_t i = 0; i < gainput::TouchPointCount; ++i)
			{
				if (mPointerIds[i] != -1)
				{
					mFingerStates[i] = GESTURE_STATE_CANCELED;
					mPointerIds[i] = -1;
					mLastFingerIndexProcessed[i] = true;
				}
			}

			ret = GESTURE_STATE_ACTION;

			break;

		case AMOTION_EVENT_ACTION_MOVE:
		{
			// Got to find if we're tracking them
			// Note: for this event, Android will always report index 0... but multiple fingers might have moved 
			//		 so we need to iterate through all fingers and find which have moved
			bool fingerMoveChanged = false;
			for (uint32_t i = 0; i < static_cast<uint32_t>(pntrCnt); ++i)
			{
				const int32_t pntrId = AMotionEvent_getPointerId(motionEvent, i);

				for (uint32_t j = 0; j < gainput::TouchPointCount; ++j)
				{
					if (mPointerIds[j] == pntrId)
					{
						float const x = AMotionEvent_getX(motionEvent, i);
						float const y = AMotionEvent_getY(motionEvent, i);

						if (x != AMotionEvent_getHistoricalX(motionEvent, i, 0) ||
							y != AMotionEvent_getHistoricalY(motionEvent, i, 0))
						{
							mCurX[j] = x;
							mCurY[j] = y;

							mFingerStates[j] = GESTURE_STATE_MOVE;
							ret = GESTURE_STATE_ACTION;
							mLastFingerIndexProcessed[j] = true;
							fingerMoveChanged = true;

							break;
						}
					}
				}
			}

			if (!fingerMoveChanged)
				return GESTURE_STATE_NONE;
		}
		break;
		}

		return ret;
	}

}   //namespace ndkHelper

