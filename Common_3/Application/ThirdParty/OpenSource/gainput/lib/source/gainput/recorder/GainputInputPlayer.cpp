
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_ENABLE_RECORDER

#include "../../../include/gainput/GainputInputDeltaState.h"
#include "../../../include/gainput/GainputHelpers.h"
#include "../../../include/gainput/GainputLog.h"


namespace gainput
{

InputPlayer::InputPlayer(InputManager& manager, InputRecording* recording) :
	manager_(manager),
	isPlaying_(false),
	recording_(recording),
	startTime_(0),
	devicesToReset_(manager.GetAllocator())
{
	playingModifierId_ = manager_.AddDeviceStateModifier(this);
}

InputPlayer::~InputPlayer()
{
	manager_.RemoveDeviceStateModifier(playingModifierId_);
}

void
InputPlayer::Update(InputDeltaState* delta)
{
	if (!isPlaying_)
	{
		return;
	}

	uint64_t now = manager_.GetTime();
	GAINPUT_ASSERT(now >= startTime_);
	now -= startTime_;

	RecordedDeviceButtonChange change;
	while (recording_->GetNextChange(now, change))
	{
		InputDevice* device = manager_.GetDevice(change.deviceId);

		GAINPUT_ASSERT(device);
		GAINPUT_ASSERT(device->IsValidButtonId(change.buttonId));
		GAINPUT_ASSERT(device->GetInputState());
		GAINPUT_ASSERT(device->GetPreviousInputState());

		if (!device->IsSynced())
		{
			device->SetSynced(true);
			devicesToReset_.push_back(change.deviceId);
		}

		if (device->GetButtonType(change.buttonId) == BT_BOOL)
		{
			HandleButton(*device, *device->GetInputState(), delta, change.buttonId, change.b);
		}
		else
		{
			HandleAxis(*device, *device->GetInputState(), delta, change.buttonId, change.f);
		}
	}

	if (now >= recording_->GetDuration())
	{
#ifdef GAINPUT_DEBUG
		GAINPUT_LOG("GAINPUT: Recording is over. Stopping playback.\n");
#endif
		Stop();
	}
}

void
InputPlayer::SetRecording(InputRecording* recording)
{
	GAINPUT_ASSERT(!isPlaying_);
	recording_ = recording;
}

void
InputPlayer::Start()
{
	GAINPUT_ASSERT(recording_);
	isPlaying_ = true;
	startTime_ = manager_.GetTime();
	recording_->Reset();
}

void
InputPlayer::Stop()
{
	isPlaying_ = false;

	for (Array<DeviceId>::const_iterator it = devicesToReset_.begin();
			it != devicesToReset_.end();
			++it)
	{
		InputDevice* device = manager_.GetDevice(*it);
		GAINPUT_ASSERT(device);
		device->SetSynced(false);
	}
	devicesToReset_.clear();
}

}

#endif

