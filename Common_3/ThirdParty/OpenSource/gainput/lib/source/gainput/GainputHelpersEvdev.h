
#ifndef GAINPUTHELPERSEVDEV_H_
#define GAINPUTHELPERSEVDEV_H_

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/uinput.h>

#include "../../include/gainput/GainputLog.h"

namespace gainput
{
namespace
{

static const unsigned EvdevDeviceCount = 11;
static const char* EvdevDeviceIds[EvdevDeviceCount] =
{
	"/dev/input/mice",
	"/dev/input/event0",
	"/dev/input/event1",
	"/dev/input/event2",
	"/dev/input/event3",
	"/dev/input/event4",
	"/dev/input/event5",
	"/dev/input/event6",
	"/dev/input/event7",
	"/dev/input/event8",
	"/dev/input/event9",
};

typedef long BitType;

#define GAINPUT_BITCOUNT	(sizeof(BitType)*8)
#define GAINPUT_BITS(n)		((n/GAINPUT_BITCOUNT)+1)

bool IsBitSet(const BitType* bits, unsigned bit)
{
	return bool(bits[bit/GAINPUT_BITCOUNT] & (1ul << (bit % GAINPUT_BITCOUNT)));
}

bool HasEventType(const BitType* bits, unsigned type)
{
	return IsBitSet(bits, type);
}

bool HasEventCode(const BitType* bits, unsigned code)
{
	return IsBitSet(bits, code);
}


class EvdevDevice
{
public:
	EvdevDevice(int fd) :
		valid_(false)
	{
		int rc;

		memset(name_, 0, sizeof(name_));
		rc = ioctl(fd, EVIOCGNAME(sizeof(name_) - 1), name_);
		if (rc < 0)
			return;

		GAINPUT_LOG("EVDEV Device name: %s\n", name_);

		rc = ioctl(fd, EVIOCGBIT(0, sizeof(bits_)), bits_);
		if (rc < 0)
			return;

		rc = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits_)), keyBits_);
		if (rc < 0)
			return;

		valid_ = true;
	}

	bool IsValid() const
	{
		return valid_;
	}

	InputDevice::DeviceType GetDeviceType() const
	{
		if (HasEventType(bits_, EV_REL)
			&& HasEventType(bits_, EV_KEY)
			&& HasEventCode(keyBits_, BTN_LEFT))
		{
			GAINPUT_LOG("EVDEV Detected as mouse\n");
			return InputDevice::DT_MOUSE;
		}
		else if (HasEventType(bits_, EV_KEY)
			&& HasEventCode(keyBits_, KEY_A)
			&& HasEventCode(keyBits_, KEY_Q))
		{
			GAINPUT_LOG("EVDEV Detected as keyboard\n");
			return InputDevice::DT_KEYBOARD;
		}
		else if (HasEventType(bits_, EV_ABS)
			&& HasEventType(bits_, EV_KEY)
			&& (HasEventCode(keyBits_, BTN_GAMEPAD)
				|| HasEventCode(keyBits_, BTN_JOYSTICK))
			)
		{
			GAINPUT_LOG("EVDEV Detected as pad\n");
			return InputDevice::DT_PAD;
		}

		GAINPUT_LOG("EVDEV Unknown device type\n");
		return InputDevice::DT_COUNT;
	}

private:
	bool valid_;
	char name_[255];
	BitType bits_[GAINPUT_BITS(EV_CNT)];
	BitType keyBits_[GAINPUT_BITS(KEY_CNT)];
};


}
}

#endif

