
#ifndef GAINPUTINPUTDEVICETOUCHANDROID_H_
#define GAINPUTINPUTDEVICETOUCHANDROID_H_

#include <android/native_activity.h>
#include <gainput/GainputHelpers.h>

#include "GainputInputDeviceTouchImpl.h"
#include "GainputTouchInfo.h"
#include "GainputInputDeviceGestureDetectorAndroid.h"

namespace gainput
{

	class InputDeviceTouchImplAndroid : public InputDeviceTouchImpl
	{
	public:
		InputDeviceTouchImplAndroid(InputManager& manager, InputDevice& device, InputState& state, InputState& previousState) :
			manager_(manager),
			device_(device),
			state_(&state),
			nextState_(manager.GetAllocator(), TouchPointCount*TouchDataElems),
			delta_(0),
			gestureTypeToID()
		{
		}

		~InputDeviceTouchImplAndroid()
		{
			gestureTypeToID.clear();
		}

		void AddGestureMapping(const uint32_t gestureID, const gainput::GestureConfig &config)
		{
			AConfiguration* conf = (AConfiguration*)manager_.GetWindowsInstance();

			if (gestureTypeToID.find(config.mType) == gestureTypeToID.end())
			{
				switch ((GestureType)config.mType)
				{
				case GestureType::GestureTap:
					tap_detector_.SetConfiguration(conf);
					gestureTypeToID[config.mType] = gestureID;
					break;
				case GestureType::GesturePan:
					drag_detector_.SetConfiguration(conf);
					gestureTypeToID[config.mType] = gestureID;
					break;
				default:
					break;
				}
			}
		}

		InputDevice::DeviceVariant GetVariant() const
		{
			return InputDevice::DV_STANDARD;
		}

		void Update(InputDeltaState* delta)
		{
			delta_ = delta;
			*state_ = nextState_;
		}

		InputDevice::DeviceState GetState() const { return InputDevice::DS_OK; }

		gainput::GesturePhase GetGesturePhase(ndk_helper::GESTURE_STATE state)
		{
			switch (state)
			{
			case ndk_helper::GESTURE_STATE_MOVE:
			case ndk_helper::GESTURE_STATE_ACTION:
				return gainput::GesturePhase::GesturePhaseUpdated;
			case ndk_helper::GESTURE_STATE_END:
				return gainput::GesturePhase::GesturePhaseEnded;
			case ndk_helper::GESTURE_STATE_START:
				return gainput::GesturePhase::GesturePhaseStarted;
			case ndk_helper::GESTURE_STATE_CANCELED:
				return gainput::GesturePhase::GesturePhaseCanceled;
			default:
				return gainput::GesturePhase::GesturePhaseCanceled;
				break;
			}
		}

		int32_t HandleInput(AInputEvent* event)
		{
			GAINPUT_ASSERT(state_);
			GAINPUT_ASSERT(event);

			if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION)
			{
				return 0;
			}

			ndk_helper::GESTURE_STATE tapState = ndk_helper::GESTURE_STATE_NONE;
			ndk_helper::GESTURE_STATE dragState = ndk_helper::GESTURE_STATE_NONE;

			if (gestureTypeToID.find(GestureType::GestureTap) != gestureTypeToID.end())
				tapState = tap_detector_.Detect(event);
			if (gestureTypeToID.find(GestureType::GesturePan) != gestureTypeToID.end())
				dragState = drag_detector_.Detect(event);


			if (tapState != ndk_helper::GESTURE_STATE_NONE)
			{
				GAINPUT_LOG("Single tap gesture");

				for (size_t i = 0; i < gainput::TouchPointCount; ++i)
				{
					if (tap_detector_.mLastFingerIndexProcessed[i])
					{
						gainput::GestureChange gestureData = {};
						gestureData.phase = GetGesturePhase(tap_detector_.mFingerStates[i]);
						gestureData.type = gainput::GestureTap;
						gestureData.position[0] = tap_detector_.mDownX[i];
						gestureData.position[1] = tap_detector_.mDownY[i];
						gestureData.fingerIndex = i;
						HandleGesture(gestureTypeToID[gainput::GestureTap], gestureData);
					}
				}
			}

			if (dragState != ndk_helper::GESTURE_STATE_NONE)
			{
				GAINPUT_LOG("Drag gesture");

				for (size_t i = 0; i < gainput::TouchPointCount; ++i)
				{
					if (drag_detector_.mLastFingerIndexProcessed[i])
					{
						gainput::GestureChange gestureData = {};
						gestureData.phase = GetGesturePhase(drag_detector_.mFingerStates[i]);
						gestureData.type = gainput::GesturePan;
						gestureData.position[0] = drag_detector_.mCurX[i];
						gestureData.position[1] = drag_detector_.mCurY[i];
						gestureData.scale = 1.0f;
						gestureData.velocity = 1.0f;
						gestureData.translation[0] = gestureData.position[0] - drag_detector_.mStartX[i];
						gestureData.translation[1] = gestureData.position[1] - drag_detector_.mStartY[i];
						gestureData.fingerIndex = i;
						HandleGesture(gestureTypeToID[gainput::GesturePan], gestureData);
					}
				}
			}

			for (uint32_t i = 0; i < AMotionEvent_getPointerCount(event) && i < TouchPointCount; ++i)
			{
				GAINPUT_ASSERT(i < TouchPointCount);
				const float x = AMotionEvent_getX(event, i);
				const float y = AMotionEvent_getY(event, i);
				HandleFloat(Touch0X + i * TouchDataElems, x);
				HandleFloat(Touch0Y + i * TouchDataElems, y);
				const int motionAction = AMotionEvent_getAction(event);
				const bool down = (motionAction == AMOTION_EVENT_ACTION_DOWN || motionAction == AMOTION_EVENT_ACTION_MOVE);
				HandleBool(Touch0Down + i * TouchDataElems, down);
				HandleFloat(Touch0Pressure + i * TouchDataElems, AMotionEvent_getPressure(event, i));
#ifdef GAINPUT_DEBUG
				const int32_t w = manager_.GetDisplayWidth();
				const int32_t h = manager_.GetDisplayHeight();
				GAINPUT_LOG("Touch %i) x: %f, y: %f, w: %i, h: %i, action: %d\n", i, x, y, w, h, motionAction);
#endif
			}

			return 1;
		}

		InputState* GetNextInputState()
		{
			return &nextState_;
		}

	private:
		InputManager& manager_;
		InputDevice& device_;
		InputState* state_;
		InputState nextState_;
		InputDeltaState* delta_;
		gainput::HashMap<gainput::GestureType, uint32_t> gestureTypeToID;

		ndk_helper::TapDetector tap_detector_;
		ndk_helper::DragDetector drag_detector_;


		void HandleBool(DeviceButtonId buttonId, bool value)
		{
			HandleButton(device_, nextState_, delta_, buttonId, value);
		}

		void HandleFloat(DeviceButtonId buttonId, float value)
		{
			HandleAxis(device_, nextState_, delta_, buttonId, value);
		}

		void HandleGesture(DeviceButtonId buttonId, gainput::GestureChange& gesture)
		{
			if (delta_)
				delta_->AddChange(device_.GetDeviceId(), buttonId, gesture);
		}
	};

}

#endif

