
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_ENABLE_RECORDER

namespace gainput
{

namespace
{
	class RecordingListener : public InputListener
	{
	public:
		RecordingListener(InputManager& manager, InputRecorder& recorder) :
			manager_(manager),
			recorder_(recorder)
		{ }

		bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool /*oldValue*/, bool newValue)
		{
			if (!recorder_.IsDeviceToRecord(deviceId))
			{
				return true;
			}

			InputRecording* recording = recorder_.GetRecording();
			GAINPUT_ASSERT(recording);
			GAINPUT_ASSERT(manager_.GetTime() >= recorder_.GetStartTime());
			const uint64_t time = manager_.GetTime() - recorder_.GetStartTime();
			recording->AddChange(time, deviceId, deviceButton, newValue);

			return true;
		}

		bool OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float /*oldValue*/, float newValue)
		{
			if (!recorder_.IsDeviceToRecord(deviceId))
			{
				return true;
			}

			InputRecording* recording = recorder_.GetRecording();
			GAINPUT_ASSERT(recording);
			GAINPUT_ASSERT(manager_.GetTime() >= recorder_.GetStartTime());
			const uint64_t time = manager_.GetTime() - recorder_.GetStartTime();
			recording->AddChange(time, deviceId, deviceButton, newValue);

			return true;
		}

	private:
		InputManager& manager_;
		InputRecorder& recorder_;

	};
}


InputRecorder::InputRecorder(InputManager& manager) :
	manager_(manager),
	isRecording_(false),
	recordingListener_(0),
	recording_(0),
	startTime_(0),
	recordedDevices_(manager.GetAllocator())
{
}

InputRecorder::~InputRecorder()
{
	Stop();

	if (recording_)
	{
		manager_.GetAllocator().Delete(recording_);
	}
}

void
InputRecorder::Start()
{
	isRecording_ = true;

	if (recording_)
	{
		recording_->Clear();
	}
	else
	{
		recording_ = manager_.GetAllocator().New<InputRecording>(manager_.GetAllocator());
	}

	startTime_ = manager_.GetTime();

	recordingListener_ = manager_.GetAllocator().New<RecordingListener>(manager_, *this);
	recordingListenerId_ = manager_.AddListener(recordingListener_);

	// Record the initial state
	for (InputManager::iterator it = manager_.begin();
			it != manager_.end();
			++it)
	{
		InputDevice* device = it->second;
		const DeviceId deviceId = device->GetDeviceId();

		if (!IsDeviceToRecord(deviceId))
		{
			continue;
		}

		GAINPUT_ASSERT(device->GetInputState());
		for (DeviceButtonId buttonId = 0; buttonId < device->GetInputState()->GetButtonCount(); ++buttonId)
		{
			if (device->IsValidButtonId(buttonId))
			{
				if (device->GetButtonType(buttonId) == BT_BOOL)
				{
					recording_->AddChange(0, deviceId, buttonId, device->GetBool(buttonId));
				}
				else
				{
					recording_->AddChange(0, deviceId, buttonId, device->GetFloat(buttonId));
				}
			}
		}
	}
}

void
InputRecorder::Stop()
{
	if (!isRecording_)
	{
		return;
	}

	isRecording_ = false;
	manager_.RemoveListener(recordingListenerId_);
	manager_.GetAllocator().Delete(recordingListener_);
	recordingListener_ = 0;
}

}

#endif

