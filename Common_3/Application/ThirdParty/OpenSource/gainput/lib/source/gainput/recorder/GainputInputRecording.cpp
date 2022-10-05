
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_ENABLE_RECORDER
#include "../dev/GainputStream.h"
#include "../dev/GainputMemoryStream.h"

const static unsigned SerializationVersion = 0x1;

namespace gainput
{

InputRecording::InputRecording(Allocator& allocator) :
	changes_(allocator),
	position_(0)
{
}

InputRecording::InputRecording(InputManager& manager, void* data, size_t size, Allocator& allocator) :
	changes_(allocator),
	position_(0)
{
	GAINPUT_ASSERT(data);
	GAINPUT_ASSERT(size > 0);

	Stream* stream = allocator.New<MemoryStream>(data, size, size);

	uint32_t t;
	uint8_t deviceType;
	uint8_t deviceIndex;
	stream->Read(t);
	GAINPUT_ASSERT(t == SerializationVersion);

	while (!stream->IsEof())
	{
		RecordedDeviceButtonChange change;

		stream->Read(t);
		change.time = t;
		stream->Read(deviceType);
		stream->Read(deviceIndex);
		change.deviceId = manager.FindDeviceId(InputDevice::DeviceType(deviceType), deviceIndex);
		GAINPUT_ASSERT(change.deviceId != InvalidDeviceId);

		stream->Read(t);
		change.buttonId = t;

		const InputDevice* device = manager.GetDevice(change.deviceId);
		GAINPUT_ASSERT(device);

		if (device->GetButtonType(change.buttonId) == BT_BOOL)
		{
			uint8_t value;
			stream->Read(value);
			change.b = (value != 0);
			GAINPUT_ASSERT(sizeof(float) >= sizeof(uint8_t));
			for (size_t i = 0; i < sizeof(float)-sizeof(uint8_t); ++i)
			{
				uint8_t t8;
				stream->Read(t8);
			}
		}
		else
		{
			float value;
			stream->Read(value);
			change.f = value;
		}

		changes_.push_back(change);
	}

	GAINPUT_ASSERT(stream->GetLeft() == 0);
	allocator.Delete(stream);
}

void
InputRecording::AddChange(uint64_t time, DeviceId deviceId, DeviceButtonId buttonId, bool value)
{
	RecordedDeviceButtonChange change;
	change.time = time;
	change.deviceId = deviceId;
	change.buttonId = buttonId;
	change.b = value;
	changes_.push_back(change);
}

void
InputRecording::AddChange(uint64_t time, DeviceId deviceId, DeviceButtonId buttonId, float value)
{
	RecordedDeviceButtonChange change;
	change.time = time;
	change.deviceId = deviceId;
	change.buttonId = buttonId;
	change.f = value;
	changes_.push_back(change);
}

void
InputRecording::Clear()
{
	changes_.clear();
}

bool
InputRecording::GetNextChange(uint64_t time, RecordedDeviceButtonChange& outChange)
{
	if (position_ >= changes_.size())
	{
		return false;
	}

	if (changes_[position_].time > time)
	{
		return false;
	}

	outChange = changes_[position_];
	++position_;

	return true;
}

uint64_t
InputRecording::GetDuration() const
{
	return changes_.empty() ? 0 : changes_[changes_.size() - 1].time;
}

size_t
InputRecording::GetSerializedSize() const
{
	return (
		sizeof(uint32_t) 
		+ changes_.size() * (sizeof(uint32_t) + 2*sizeof(uint8_t) + sizeof(uint32_t) + sizeof(float))
	);
}

void
InputRecording::GetSerialized(InputManager& manager, void* data) const
{
	Stream* stream = manager.GetAllocator().New<MemoryStream>(data, 0, GetSerializedSize());

	stream->Write(uint32_t(SerializationVersion));

	for (Array<RecordedDeviceButtonChange>::const_iterator it = changes_.begin();
			it != changes_.end();
			++it)
	{
		const InputDevice* device = manager.GetDevice(it->deviceId);
		GAINPUT_ASSERT(device);
		stream->Write(uint32_t(it->time));
		stream->Write(uint8_t(device->GetType()));
		stream->Write(uint8_t(device->GetIndex()));
		stream->Write(uint32_t(it->buttonId));
		if (device->GetButtonType(it->buttonId) == BT_BOOL)
		{
			stream->Write(uint8_t(it->b));
			GAINPUT_ASSERT(sizeof(float) >= sizeof(uint8_t));
			for (size_t i = 0; i < sizeof(float)-sizeof(uint8_t); ++i)
			{
				stream->Write(uint8_t(0));
			}
		}
		else
		{
			stream->Write(float(it->f));
		}
	}

	GAINPUT_ASSERT(stream->GetLeft() == 0);

	manager.GetAllocator().Delete(stream);
}

}

#endif

