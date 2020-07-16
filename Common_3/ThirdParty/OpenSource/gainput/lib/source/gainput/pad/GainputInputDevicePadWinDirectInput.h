
#ifndef GAINPUTINPUTDEVICEDIRECTINPUTPADWIN_H_
#define GAINPUTINPUTDEVICEDIRECTINPUTPADWIN_H_

#include <dinput.h>
#include <dbt.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment(lib,"Hid.lib")

#include "../../../../../../../../Common_3/OS/Interfaces/ILog.h"
#include "GainputControllerDb.h"

const GUID FAR USB_DEVICE = { 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00,0xC0, 0x4F, 0xB9, 0x51, 0xED };
static HDEVNOTIFY hDeviceNotify;
/*DirectInput implementation : methods/helper functionalities derived from SDL(Thank You SDL)*/
namespace gainput
{
#define O_DIJOFS_X            offsetof(DIJOYSTATE, lX)
#define O_DIJOFS_Y            offsetof(DIJOYSTATE, lY)
#define O_DIJOFS_Z            offsetof(DIJOYSTATE, lZ)
#define O_DIJOFS_RX           offsetof(DIJOYSTATE, lRx)
#define O_DIJOFS_RY           offsetof(DIJOYSTATE, lRy)
#define O_DIJOFS_RZ           offsetof(DIJOYSTATE, lRz)
#define O_DIJOFS_SLIDER(n)   (offsetof(DIJOYSTATE, rglSlider) + \
                                                        (n) * sizeof(LONG))



#define HAT_CENTERED    0x00
#define HAT_UP          0x01
#define HAT_RIGHT       0x02
#define HAT_DOWN        0x04
#define HAT_LEFT		0x08
#define HAT_RIGHTUP     (HAT_RIGHT|HAT_UP)
#define HAT_RIGHTDOWN   (HAT_RIGHT|HAT_DOWN)
#define HAT_LEFTUP      (HAT_LEFT|HAT_UP)
#define HAT_LEFTDOWN    (HAT_LEFT|HAT_DOWN)

#define MAX_INPUTS  256     /* each joystick can have up to 256 inputs */
#define INPUT_QSIZE 32

#define MAX_DIRECT_INPUT 10
#define SONY_USB_VID        0x054C

#define LIL_ENDIAN  1234
#define BIG_ENDIAN  4321

#define HARDWARE_BUS_USB        0x03
#define HARDWARE_BUS_BLUETOOTH  0x05


#ifdef __linux__
#include <endian.h>
#define BYTEORDER  __BYTE_ORDER
#else /* __linux__ */
#if defined(__hppa__) || \
		defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
		(defined(__MIPS__) && defined(__MISPEB__)) || \
		defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
		defined(__sparc__)
#define BYTEORDER   BIG_ENDIAN
#else
#define BYTEORDER   LIL_ENDIAN
#endif
#endif /* __linux__ */

	/**
	 *  \name Swap to native
	 *  Byteswap item from the specified endianness to the native endianness.
	 */
	 /* @{ */
#if BYTEORDER == LIL_ENDIAN
#define SwapLE16(X) (X)
#define SwapLE32(X) (X)
#define SwapLE64(X) (X)
#define SwapFloatLE(X)  (X)
#define SwapBE16(X) Swap16(X)
#define SwapBE32(X) Swap32(X)
#define SwapBE64(X) Swap64(X)
#define SwapFloatBE(X)  SwapFloat(X)
#else
#define SwapLE16(X) Swap16(X)
#define SwapLE32(X) Swap32(X)
#define SwapLE64(X) Swap64(X)
#define SwapFloatLE(X)  SwapFloat(X)
#define SwapBE16(X) (X)
#define SwapBE32(X) (X)
#define SwapBE64(X) (X)
#define SwapFloatBE(X)  (X)
#endif
/* @} *//* Swap to native */

//eg:a:b1,b:b2,y:b3,x:b0,start:b9,guide:b12,back:b8,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftshoulder:b4,rightshoulder:b5,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b6,righttrigger:b7"
//for mapping
#define A "a"
#define B "b"
#define X "x"
#define Y "y"
#define L1 "leftshoulder"
#define R1 "rightshoulder"
#define L3 "leftstick"
#define R3 "rightstick"
#define DN "dpup"
#define DS "dpdown"
#define DE "dpright"
#define DW "dpleft"
#define leftx "leftx"
#define rightx "rightx"
#define lefty "lefty"
#define righty "righty"
#define lefttrigger "lefttrigger"
#define righttrigger "righttrigger"
#define start "start"
#define back "back"
#define guide "guide"

	const int HAT_VALS[] =
	{
		HAT_UP,
		HAT_UP | HAT_RIGHT,
		HAT_RIGHT,
		HAT_DOWN | HAT_RIGHT,
		HAT_DOWN,
		HAT_DOWN | HAT_LEFT,
		HAT_LEFT,
		HAT_UP | HAT_LEFT
	};

	typedef enum
	{
		CONTROLLER_BUTTON_INVALID = -1,
		CONTROLLER_BUTTON_A,
		CONTROLLER_BUTTON_B,
		CONTROLLER_BUTTON_X,
		CONTROLLER_BUTTON_Y,
		CONTROLLER_BUTTON_BACK,
		CONTROLLER_BUTTON_GUIDE,
		CONTROLLER_BUTTON_START,
		CONTROLLER_BUTTON_LEFTSTICK,
		CONTROLLER_BUTTON_RIGHTSTICK,
		CONTROLLER_BUTTON_LEFTSHOULDER,
		CONTROLLER_BUTTON_RIGHTSHOULDER,
		CONTROLLER_BUTTON_DPAD_UP,
		CONTROLLER_BUTTON_DPAD_DOWN,
		CONTROLLER_BUTTON_DPAD_LEFT,
		CONTROLLER_BUTTON_DPAD_RIGHT,
		CONTROLLER_BUTTON_MAX,
	} GameControllerButton;

	typedef enum
	{
		CONTROLLER_AXIS_INVALID =-1,
		CONTROLLER_LEFT_X,
		CONTROLLER_LEFT_Y,
		CONTROLLER_RIGHT_X,
		CONTROLLER_RIGHT_Y,
		CONTROLLER_LEFT_TRIGGER,
		CONTROLLER_RIGHT_TRIGGER,
		CONTROLLER_MAX_AXIS
	}GameControllerAxis;

	typedef enum
	{
		CONTROLLER_HAT_INVALID =-1,
		CONTROLLER_HAT_UP,
		CONTROLLER_HAT_RIGHT,
		CONTROLLER_HAT_DOWN,
		CONTROLLER_HAT_LEFT,
		CONTROLLER_HAT_LEFT_UP,
		CONTROLLER_HAT_LEFT_DOWN,
		CONTROLLER_HAT_RIGHT_UP,
		CONTROLLER_HAT_RIGHT_DOWN,
		CONTROLLER_HAT_MAX
	}GameControllerHat;

	static PRAWINPUTDEVICELIST RawDevList = NULL;
	static UINT RawDevListCount = 0;
	const float MaximumAxisValue = 32767.0f;
	const float MinimumAxisValue = 32768.0f;

	enum class Type
	{
		BUTTON,
		AXIS,
		HAT
	};

	typedef struct input_t
	{
		/*DirectInput offset for this input type: */
		DWORD ofs;
		/*Button, axis or hat: */
		Type type;
		/*input offset: */
		UINT8 num;
	} input_t;

	typedef struct {
		UINT8 data[16];
	}JoystickGUID;


	typedef struct joystick_hwdata
	{
		JoystickGUID guid;
		UINT16 vendor;
		UINT16 product;
		UINT32 rumble_expiration;
		LPDIRECTINPUTDEVICE8 InputDevice;
		DIDEVCAPS Capabilities;
		bool buffered;
		input_t Inputs[MAX_INPUTS];
		int NumInputs;
		int NumSliders;
		bool ff_initialized;
		DIEFFECT *ffeffect;
		LPDIRECTINPUTEFFECT ffeffect_ref;

		bool bXInputDevice; /* TRUE if this device supports using the xinput API rather than DirectInput */
		bool bXInputHaptic; /* Supports force feedback via XInput. */
	}joystick_hwdata;

	/* The joystick structure */
	typedef struct JoystickAxisInfo
	{
		INT16 initial_value;       /* Initial axis state */
		INT16 value;               /* Current axis state */
		INT16 zero;                /* Zero point on the axis (-32768 for triggers) */
		bool has_initial_value; /* Whether we've seen a value on the axis yet */
		bool sent_initial_value; /* Whether we've sent the initial axis value */
	} JoystickAxisInfo;

	typedef struct ControllerMapping
	{
		int buttons[CONTROLLER_BUTTON_MAX];
		int axis[CONTROLLER_MAX_AXIS];
		int hat[CONTROLLER_HAT_MAX];
		char control_mapping[512];
		ControllerMapping()
		{
			for (int i = 0; i < CONTROLLER_BUTTON_MAX; ++i)
			{
				buttons[i] = (GameControllerButton)(CONTROLLER_BUTTON_A + i);
			}

			for (int i = 0; i < CONTROLLER_MAX_AXIS; ++i)
			{
				axis[i] = (GameControllerAxis)(CONTROLLER_LEFT_X + i);
			}

			hat[CONTROLLER_HAT_UP]		= HAT_UP;
			hat[CONTROLLER_HAT_DOWN]	= HAT_DOWN;
			hat[CONTROLLER_HAT_RIGHT]	= HAT_RIGHT;
			hat[CONTROLLER_HAT_LEFT]	= HAT_LEFT;

			hat[CONTROLLER_HAT_LEFT_UP] = hat[CONTROLLER_HAT_LEFT] | hat[CONTROLLER_HAT_UP];
			hat[CONTROLLER_HAT_LEFT_DOWN] = hat[CONTROLLER_HAT_LEFT] | hat[CONTROLLER_HAT_DOWN];
			hat[CONTROLLER_HAT_RIGHT_UP] = hat[CONTROLLER_HAT_RIGHT] | hat[CONTROLLER_HAT_UP];
			hat[CONTROLLER_HAT_RIGHT_DOWN] = hat[CONTROLLER_HAT_RIGHT] | hat[CONTROLLER_HAT_DOWN];
			//default ath moment
		}

		void SetDefaultMapping()
		{
			static const char* mapping = "a:b1,b:b2,y:b3,x:b0,start:b9,guide:b12,back:b8,leftstick:b10,rightstick:b11,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a5,lefttrigger:b6,righttrigger:b7";
			SetFromString(mapping);
		}

		void SetFromString(const char* mapping)
		{
			//callen
			char  _mapping[512];
			int len = 0;
			while (*mapping !='\0')
			{
				_mapping[len++] = *mapping;
				mapping++;
			}
			memcpy(control_mapping, _mapping, len);
			control_mapping[len] = '\0';

			char *main;
			char *ptr = strtok_s(_mapping, ",", &main);
			while (ptr != NULL)
			{
				SetMapping(ptr);
				ptr = strtok_s(NULL, ",", &main);
			}

			hat[CONTROLLER_HAT_LEFT_UP] = hat[CONTROLLER_HAT_LEFT] | hat[CONTROLLER_HAT_UP];
			hat[CONTROLLER_HAT_LEFT_DOWN] = hat[CONTROLLER_HAT_LEFT] | hat[CONTROLLER_HAT_DOWN];
			hat[CONTROLLER_HAT_RIGHT_UP] = hat[CONTROLLER_HAT_RIGHT] | hat[CONTROLLER_HAT_UP];
			hat[CONTROLLER_HAT_RIGHT_DOWN] = hat[CONTROLLER_HAT_RIGHT] | hat[CONTROLLER_HAT_DOWN];
		}

		void SetMapping(char* mapping)
		{
			char *main;
			char *name = strtok_s(mapping, ":", &main);
			char* originalId = strtok_s(NULL, ":", &main);
			int index = 0;
			GameControllerButton button = GetButtonFromString(name);
			if (button != CONTROLLER_BUTTON_INVALID)
			{
				sscanf(originalId, "b%d", &index);
				buttons[button] = index;
			}

			//NEED TO CHECK THIS
			GameControllerAxis currentaxis = GetAxisFromString(name);
			if (currentaxis != CONTROLLER_BUTTON_INVALID)
			{
				sscanf(originalId, "a%d", &index);
				axis[currentaxis] = index;
			}

			GameControllerHat ht = GetHatFromString(name);
			if (ht != CONTROLLER_HAT_INVALID)
			{
				sscanf(originalId, "h0.%d", &index);
				hat[ht] = index;
			}
		}

		int operator[](GameControllerButton index)
		{
			return buttons[index];
		}

		int operator[](GameControllerAxis index)
		{
			return axis[index];
		}

		GameControllerAxis GetAxisFromString(const char* axis)
		{
			if (_strcmpi(leftx, axis) == 0)
			{
				return CONTROLLER_LEFT_X;
			}

			if (_strcmpi(rightx, axis) == 0)
			{
				return CONTROLLER_RIGHT_X;
			}

			if (_strcmpi(lefty, axis) == 0)
			{
				return CONTROLLER_LEFT_Y;
			}

			if (_strcmpi(righty, axis) == 0)
			{
				return CONTROLLER_RIGHT_Y;
			}

			if(_strcmpi(lefttrigger, axis) == 0 )
			{
				return CONTROLLER_LEFT_TRIGGER;
			}

			if(_strcmpi(righttrigger, axis) == 0 )
			{
				return CONTROLLER_RIGHT_TRIGGER;
			}

			return CONTROLLER_AXIS_INVALID;
		}

		GameControllerButton GetButtonFromString(const char* button)
		{
			if (_strcmpi(A, button) == 0)
			{
				return CONTROLLER_BUTTON_A;
			}

			if (_strcmpi(B, button) == 0)
			{
				return CONTROLLER_BUTTON_B;
			}

			if (_strcmpi(X, button) == 0)
			{
				return CONTROLLER_BUTTON_X;
			}

			if (_strcmpi(Y, button) == 0)
			{
				return CONTROLLER_BUTTON_Y;
			}

			if (_strcmpi(L1, button) == 0)
			{
				return CONTROLLER_BUTTON_LEFTSHOULDER;
			}

			if (_strcmpi(R1, button) == 0)
			{
				return CONTROLLER_BUTTON_RIGHTSHOULDER;
			}

			if (_strcmpi(L3, button) == 0)
			{
				return CONTROLLER_BUTTON_LEFTSTICK;
			}

			if (_strcmpi(R3, button) == 0)
			{
				return CONTROLLER_BUTTON_RIGHTSTICK;
			}

			if (_strcmpi(start, button) == 0)
			{
				return CONTROLLER_BUTTON_START;
			}

			if(_strcmpi(back , button)==0)
			{
				return CONTROLLER_BUTTON_BACK;
			}

			if(_strcmpi(guide  , button)==0)
			{
				return CONTROLLER_BUTTON_GUIDE;
			}

			return CONTROLLER_BUTTON_INVALID;
		}

		GameControllerHat GetHatFromString(const char* hat)
		{
			if (_strcmpi(DN, hat) == 0)
			{
				return CONTROLLER_HAT_UP;
			}

			if (_strcmpi(DS, hat) == 0)
			{
				return CONTROLLER_HAT_DOWN;
			}

			if (_strcmpi(DE, hat) == 0)
			{
				return CONTROLLER_HAT_RIGHT;
			}

			if (_strcmpi(DW, hat) == 0)
			{
				return CONTROLLER_HAT_LEFT;
			}
			return CONTROLLER_HAT_INVALID;
		}

	}ControllMapping;


	typedef struct GamePad
	{
		INT32 instance_id;			/* Device instance, monotonically increasing from 0 */
		char name[256];             /* Joystick name - system dependent */
		GUID guid;					/* Joystick guid */

		int naxes;                  /* Number of axis controls on the joystick */
		JoystickAxisInfo axes[10];

		int nhats;                    /* Number of hats on the joystick */
		UINT8 hats[4];                /* Current hat states */

		int nbuttons;               /* Number of buttons on the joystick */
		UINT8 buttons[32];             /* Current button states */

		bool attached;
		bool is_game_controller;
		bool delayed_guide_button; /* TRUE if this device has the guide button event delayed */
		bool force_recentering; /* TRUE if this device needs to have its state reset to 0 */

		joystick_hwdata hwdata;     /* Driver dependent information */
		ControllMapping mapping;

		void Clean()
		{
			naxes = 0;
			nbuttons = 0;
			attached;
			is_game_controller = false;
			delayed_guide_button=false;
			force_recentering = false;
			memset(axes, 0, sizeof(axes));
			memset(buttons, 0, sizeof(buttons));
			memset(hats, 0, sizeof(hats));

			memset(hwdata.Inputs,0,sizeof(hwdata.Inputs));
			hwdata.NumInputs = 0;
			hwdata.NumSliders = 0;

			memset(name, 0, sizeof(name));
			hwdata = {};
			mapping = {};
		}
	}GamePad;

	typedef struct ControllerFeedback
	{
		uint8_t vibration_left;
		uint8_t vibration_right;
		uint8_t r;			// led color
		uint8_t g;			// led color
		uint8_t b;			// led color
		uint32_t duration_ms;
		ControllerFeedback()
		{
			vibration_left = 0;
			vibration_right = 0;
			r = 0;
			g = 0;
			b = 0;
			duration_ms = 0;
		}
	}ControllerFeedback;

	typedef struct GamePadInfo
	{
		bool					created;
		bool					assignedHid;
		int						padIndex;
		char					name[1024];
		GamePad					gamepad;
		LPDIRECTINPUTDEVICE8	interfacePtr;
		LPDIRECTINPUTDEVICE8	gamepadDevicePtr;
		HANDLE					rawInputDeviceHandle;
		HANDLE					hidDevice;
	}GamePadInfo;

	typedef struct DirectInputGamePads
	{
		int				xinputCountConnected;
		void *handle;
		GamePadInfo		gamePadInfos[MAX_DIRECT_INPUT];
		LPDIRECTINPUT8	dinput;
		int				directInputCountConnected;
	}DirectInputGamePads;


	typedef struct LinearAllocator
	{
	public:

		inline size_t Get_capacity() const
		{
			return capacity;
		}

		inline void Reserve(size_t newCapacity)
		{
			capacity = newCapacity;
			buffer = (uint8_t*)malloc(capacity);
		}

		inline uint8_t* Allocate(size_t size)
		{
			if (offset + size <= capacity)
			{
				uint8_t* ret = &buffer[offset];
				offset += size;
				return ret;
			}
			return nullptr;
		}

		inline void Free(size_t size)
		{
			if (offset >= size)
			{
				DLOGF(LogLevel::eERROR, "offset >= size");
				return;
			}
			offset -= size;
		}

		inline void Reset()
		{
			offset = 0;
		}

		~LinearAllocator()
		{
			free(buffer);
		}
	private:
		uint8_t* buffer;
		size_t capacity = 0;
		size_t offset = 0;
	}LinearAllocator;


	static bool IsXInputDevice(const GUID* pGuidProductFromDirectInput)
	{
		static GUID IID_ValveStreamingGamepad = { MAKELONG(0x28DE, 0x11FF), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_X360WiredGamepad = { MAKELONG(0x045E, 0x02A1), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_X360WirelessGamepad = { MAKELONG(0x045E, 0x028E), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneWiredGamepad = { MAKELONG(0x045E, 0x02FF), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneWirelessGamepad = { MAKELONG(0x045E, 0x02DD), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneNewWirelessGamepad = { MAKELONG(0x045E, 0x02D1), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneSWirelessGamepad = { MAKELONG(0x045E, 0x02EA), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneSBluetoothGamepad = { MAKELONG(0x045E, 0x02E0), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
		static GUID IID_XOneEliteWirelessGamepad = { MAKELONG(0x045E, 0x02E3), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };

		static const GUID *s_XInputProductGUID[] =
		{
			&IID_ValveStreamingGamepad,
			&IID_X360WiredGamepad,         /* Microsoft's wired X360 controller for Windows. */
			&IID_X360WirelessGamepad,      /* Microsoft's wireless X360 controller for Windows. */
			&IID_XOneWiredGamepad,         /* Microsoft's wired Xbox One controller for Windows. */
			&IID_XOneWirelessGamepad,      /* Microsoft's wireless Xbox One controller for Windows. */
			&IID_XOneNewWirelessGamepad,   /* Microsoft's updated wireless Xbox One controller (w/ 3.5 mm jack) for Windows. */
			&IID_XOneSWirelessGamepad,     /* Microsoft's wireless Xbox One S controller for Windows. */
			&IID_XOneSBluetoothGamepad,    /* Microsoft's Bluetooth Xbox One S controller for Windows. */
			&IID_XOneEliteWirelessGamepad  /* Microsoft's wireless Xbox One Elite controller for Windows. */
		};

		size_t iDevice;
		UINT i;

		/* Check for well known XInput device GUIDs */
		/* This lets us skip RAWINPUT for popular devices. Also, we need to do this for the Valve Streaming Gamepad because it's virtualized and doesn't show up in the device list. */
		for (iDevice = 0; iDevice < 9; ++iDevice)
		{
			if (memcmp((void*)pGuidProductFromDirectInput, s_XInputProductGUID[iDevice], sizeof(GUID)) == 0)
			{
				return true;
			}
		}

		if (RawDevList == NULL)
		{
			if ((GetRawInputDeviceList(NULL, &RawDevListCount, sizeof(RAWINPUTDEVICELIST)) == -1) || (!RawDevListCount))
			{
				return false;
			}

			RawDevList = (PRAWINPUTDEVICELIST)malloc(sizeof(RAWINPUTDEVICELIST) * RawDevListCount);
			if (RawDevList == NULL)
			{
				return false;
			}

			if (GetRawInputDeviceList(RawDevList, &RawDevListCount, sizeof(RAWINPUTDEVICELIST)) == -1)
			{
				free(RawDevList);
				RawDevList = NULL;
				return false;
			}
		}

		for (i = 0; i < RawDevListCount; i++)
		{
			RID_DEVICE_INFO rdi;
			char devName[128];
			UINT rdiSize = sizeof(rdi);
			UINT nameSize = sizeof(devName);

			rdi.cbSize = sizeof(rdi);
			if ((RawDevList[i].dwType == RIM_TYPEHID) &&
				(GetRawInputDeviceInfoA(RawDevList[i].hDevice, RIDI_DEVICEINFO, &rdi, &rdiSize) != ((UINT)-1)) &&
				(MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) == ((LONG)pGuidProductFromDirectInput->Data1)) &&
				(GetRawInputDeviceInfoA(RawDevList[i].hDevice, RIDI_DEVICENAME, devName, &nameSize) != ((UINT)-1)) &&
				(strstr(devName, "IG_") != NULL))
			{
				return true;
			}
		}
		return false;
	}

	static BOOL CALLBACK EnumDevObjectsCallback(LPCDIDEVICEOBJECTINSTANCE dev, LPVOID pvRef)
	{
		GamePadInfo *gamepadInfo = (GamePadInfo *)pvRef;
		HRESULT result;
		input_t *in = &gamepadInfo->gamepad.hwdata.Inputs[gamepadInfo->gamepad.hwdata.NumInputs];

		if (dev->dwType & DIDFT_BUTTON)
		{
			in->type = Type::BUTTON;
			in->num = gamepadInfo->gamepad.nbuttons;
			in->ofs = DIJOFS_BUTTON(in->num);
			gamepadInfo->gamepad.nbuttons++;
		}
		else if (dev->dwType & DIDFT_POV)
		{
			in->type = Type::HAT;
			in->num = gamepadInfo->gamepad.nhats;
			in->ofs = DIJOFS_POV(in->num);
			gamepadInfo->gamepad.nhats++;
		}
		else if (dev->dwType & DIDFT_AXIS)
		{
			DIPROPRANGE diprg;
			DIPROPDWORD dilong;

			in->type = Type::AXIS;
			in->num = gamepadInfo->gamepad.naxes;
			if (!memcmp(&dev->guidType, &GUID_XAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_X;
			else if (!memcmp(&dev->guidType, &GUID_YAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_Y;
			else if (!memcmp(&dev->guidType, &GUID_ZAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_Z;
			else if (!memcmp(&dev->guidType, &GUID_RxAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_RX;
			else if (!memcmp(&dev->guidType, &GUID_RyAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_RY;
			else if (!memcmp(&dev->guidType, &GUID_RzAxis, sizeof(dev->guidType)))
				in->ofs = DIJOFS_RZ;
			else if (!memcmp(&dev->guidType, &GUID_Slider, sizeof(dev->guidType)))
			{
				in->ofs = DIJOFS_SLIDER(gamepadInfo->gamepad.hwdata.NumSliders);
				++gamepadInfo->gamepad.hwdata.NumSliders;
			}
			else
			{
				return DIENUM_CONTINUE; /* not an axis we can grok */
			}

			diprg.diph.dwSize = sizeof(DIPROPRANGE);
			diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			diprg.diph.dwObj = dev->dwType;
			diprg.diph.dwHow = DIPH_BYID;
			diprg.lMin = (LONG)-MinimumAxisValue;
			diprg.lMax = (LONG)MaximumAxisValue;

			result = IDirectInputDevice8_SetProperty(gamepadInfo->interfacePtr, DIPROP_RANGE, &diprg.diph);
			if (FAILED(result))
			{
				return DIENUM_CONTINUE;     /* don't use this axis */
			}

			/* Set dead zone to 0. */
			dilong.diph.dwSize = sizeof(dilong);
			dilong.diph.dwHeaderSize = sizeof(dilong.diph);
			dilong.diph.dwObj = dev->dwType;
			dilong.diph.dwHow = DIPH_BYID;
			dilong.dwData = 0;
			result = IDirectInputDevice8_SetProperty(gamepadInfo->interfacePtr, DIPROP_DEADZONE, &dilong.diph);
			if (FAILED(result))
			{
				return DIENUM_CONTINUE;     /* don't use this axis */
			}

			gamepadInfo->gamepad.naxes++;
		}
		else
		{
			/* not supported at this time */
			return DIENUM_CONTINUE;
		}

		gamepadInfo->gamepad.hwdata.NumInputs++;

		if (gamepadInfo->gamepad.hwdata.NumInputs == MAX_INPUTS)
		{
			return DIENUM_STOP;
		}

		return DIENUM_CONTINUE;
		return true;
	}

	/* Sort using the data offset into the DInput struct.
	 * This gives a reasonable ordering for the inputs.
	 */
	static int
		SortDevFunc(const void *a, const void *b)
	{
		const input_t *inputA = (const input_t*)a;
		const input_t *inputB = (const input_t*)b;

		if (inputA->ofs < inputB->ofs)
			return -1;
		if (inputA->ofs > inputB->ofs)
			return 1;
		return 0;
	}

	/* Sort the input objects and recalculate the indices for each input. */
	static void
		SortDevObjects(GamePad *joystick)
	{
		input_t *inputs = joystick->hwdata.Inputs;
		int nButtons = 0;
		int nHats = 0;
		int nAxis = 0;
		int n;

		qsort(inputs, joystick->hwdata.NumInputs, sizeof(input_t), SortDevFunc);

		for (n = 0; n < joystick->hwdata.NumInputs; n++) {
			switch (inputs[n].type) {
			case Type::BUTTON:
				inputs[n].num = nButtons;
				nButtons++;
				break;

			case Type::HAT:
				inputs[n].num = nHats;
				nHats++;
				break;

			case Type::AXIS:
				inputs[n].num = nAxis;
				nAxis++;
				break;
			}
		}
	}

	static void CreateGamePad(LPVOID pvRef)
	{
		DirectInputGamePads *directInutGamePads = (DirectInputGamePads *)pvRef;
		GamePadInfo& gamepadInfo = directInutGamePads->gamePadInfos[directInutGamePads->directInputCountConnected];
		HRESULT result = directInutGamePads->dinput->CreateDevice(gamepadInfo.gamepad.guid, &gamepadInfo.gamepadDevicePtr, NULL);

		/*get interface to gamepad*/
		result = IDirectInputDevice8_QueryInterface(gamepadInfo.gamepadDevicePtr, IID_IDirectInputDevice8, (LPVOID *)&gamepadInfo.interfacePtr);
		result = IDirectInputDevice8_Release(gamepadInfo.gamepadDevicePtr);
		/*set copperation level for shared access*/
		result = IDirectInputDevice8_SetCooperativeLevel(gamepadInfo.interfacePtr, (HWND)directInutGamePads->handle, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
		/*Set data format*/
		result = IDirectInputDevice8_SetDataFormat(gamepadInfo.interfacePtr, &c_dfDIJoystick);

		//get device info

		/* What buttons and axes does it have? */
		result = IDirectInputDevice8_EnumObjects(gamepadInfo.interfacePtr, EnumDevObjectsCallback, &gamepadInfo, DIDFT_BUTTON | DIDFT_AXIS | DIDFT_POV);

		/* Reorder the input objects. Some devices do not report the X axis as
		 the first axis, for example. */
		SortDevObjects(&gamepadInfo.gamepad);

		DIPROPDWORD dipdw;
		memset(&(dipdw), 0, sizeof((dipdw)));

		dipdw.diph.dwSize = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj = 0;
		dipdw.diph.dwHow = DIPH_DEVICE;
		dipdw.dwData = INPUT_QSIZE;

		/* Set the buffer size */
		result = IDirectInputDevice8_SetProperty(gamepadInfo.interfacePtr, DIPROP_BUFFERSIZE, &dipdw.diph);
		gamepadInfo.gamepad.hwdata.buffered = result == DI_POLLEDDEVICE ? false : true;

		//acquire
		result = IDirectInputDevice8_Acquire(gamepadInfo.interfacePtr);
		gamepadInfo.created = result != S_OK;
	}

	static void JoystickGetGUIDString( JoystickGUID guid, char *pszGUID, int cbGUID )
	{
		static const char k_rgchHexToASCII[] = "0123456789abcdef";
		int i;

		if( (pszGUID == NULL) || (cbGUID <= 0) ) {
			return;
		}

		for( i = 0; i < sizeof( guid.data ) && i < (cbGUID - 1) / 2; i++ ) {
			/* each input byte writes 2 ascii chars, and might write a null byte. */
			/* If we don't have room for next input byte, stop */
			unsigned char c = guid.data[i];

			*pszGUID++ = k_rgchHexToASCII[c >> 4];
			*pszGUID++ = k_rgchHexToASCII[c & 0x0F];
		}
		*pszGUID = '\0';
	}

	static void GetInfo(const DIDEVICEINSTANCE * pdidInstance,LPVOID pvRef)
	{
		DirectInputGamePads *directInutGamePads= (DirectInputGamePads *)pvRef;
		GamePad &gamepad = directInutGamePads->gamePadInfos[directInutGamePads->directInputCountConnected].gamepad;
		gamepad.guid = pdidInstance->guidInstance;
		// copy product name into global
		strcpy(gamepad.name, (char *)pdidInstance->tszProductName);
		memset(gamepad.hwdata.guid.data, 0, sizeof(gamepad.hwdata.guid));
		gamepad.hwdata.bXInputDevice = false;

		UINT16 *guid16;
		UINT16 vendor = 0;
		UINT16 product = 0;
		UINT16 version = 0;

		guid16 = (UINT16 *)gamepad.hwdata.guid.data;
		if (memcmp(&pdidInstance->guidProduct.Data4[2], "PIDVID", 6) == 0)
		{
			vendor = (UINT16)LOWORD(pdidInstance->guidProduct.Data1);
			product = (UINT16)HIWORD(pdidInstance->guidProduct.Data1);
			version = 0;

			*guid16++ = SwapLE16(HARDWARE_BUS_USB);
			*guid16++ = 0;
			*guid16++ = SwapLE16(vendor);
			*guid16++ = 0;
			*guid16++ = SwapLE16(product);
			*guid16++ = 0;
			*guid16++ = SwapLE16(version);
			*guid16++ = 0;
		}
		else {
			*guid16++ = SwapLE16(HARDWARE_BUS_BLUETOOTH);
			*guid16++ = 0;
			//strlcpy((char*)guid16, gamepad.name, sizeof(gamepad.hwdata.guid) - 4);
		}
		gamepad.hwdata.vendor = vendor;
		gamepad.hwdata.product = product;
	}

	static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE * pdidInstance, LPVOID pvRef)
	{
		DirectInputGamePads *directInutGamePads = (DirectInputGamePads *)pvRef;
		if (IsXInputDevice(&pdidInstance->guidProduct))
		{
			directInutGamePads->xinputCountConnected++;
			//handled by xinput
			return DIENUM_CONTINUE;
		}

		GetInfo(pdidInstance, pvRef);
		CreateGamePad(pvRef);
		directInutGamePads->gamePadInfos[directInutGamePads->directInputCountConnected].gamepad.instance_id = directInutGamePads->directInputCountConnected++;
		return DIENUM_CONTINUE;
	}

	/*
	* grab the guid string from a mapping string
	*/
	static bool GetControllerGUIDFromMappingString( const char *pMapping, char* guid, int guidLen )
	{
		const char *pFirstComma = strchr( pMapping, ',' );
		if( pFirstComma )
		{
			if( guidLen < (pFirstComma - pMapping + 1) )
				return false;

			memcpy( guid, pMapping, pFirstComma - pMapping );
			guid[pFirstComma - pMapping] = '\0';

			/* Convert old style GUIDs to the new style in 2.0.5 */
			if( strlen( guid ) == 32 && memcmp( &guid[20], "504944564944", 12 ) == 0 )
			{
				memcpy( &guid[20], "000000000000", 12 );
				memcpy( &guid[16], &guid[4], 4 );
				memcpy( &guid[8], &guid[0], 4 );
				memcpy( &guid[0], "03000000", 8 );
			}
			return true;
		}
		return false;
	}

	/*
	 * grab the button mapping string from a mapping string
	 */
	static char *GetControllerMappingFromMappingString(const char *pMapping)
	{
		const char *pFirstComma, *pSecondComma;

		pFirstComma = strchr(pMapping, ',');
		if (!pFirstComma)
			return NULL;

		pSecondComma = strchr(pFirstComma + 1, ',');
		if (!pSecondComma)
			return NULL;

		return strdup(pSecondComma + 1); /* mapping is everything after the 3rd comma */
	}

	static const char* GetMappingStringForGUID(JoystickGUID guid)
	{
		char guidStr[33];
		char dbGuidStr[33];
		JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));

		int i = 0;
		const char* mappingString = s_ControllerMappings[i];

		while (mappingString)
		{
			GetControllerGUIDFromMappingString(mappingString, dbGuidStr, sizeof(dbGuidStr));
			if (strncmp(guidStr, dbGuidStr, sizeof(guidStr)) == 0)
				return mappingString;
			++i;
			mappingString = s_ControllerMappings[i];
		}

		return NULL;
	}

	/*
	* grab the name string from a mapping string
	*/
	static bool GetControllerNameFromMappingString(const char *pMapping, char* name, int nameLen)
	{
		const char *pFirstComma, *pSecondComma;

		pFirstComma = strchr(pMapping, ',');
		if (!pFirstComma)
			return false;

		pSecondComma = strchr(pFirstComma + 1, ',');
		if (!pSecondComma)
			return false;

		if (nameLen < (pSecondComma - pFirstComma))
			return false;

		memcpy(name, pFirstComma + 1, pSecondComma - pFirstComma);
		name[pSecondComma - pFirstComma - 1] = 0;
		return true;
	}

	class DirectInputInitializer;

	class DirectInputInitializer
	{
		DirectInputGamePads gamePads;
		int gamePadsInUse;

	public:

		static DirectInputInitializer* GetInstance(void* hwnd = NULL)
		{
			static DirectInputInitializer* instance = NULL;
			if (!instance)
			{
				static DirectInputInitializer s_instance;
				if (s_instance.Init(hwnd))
				{
					instance = &s_instance;
				}
			}
			return instance;
		}

		void XInputClean()
		{
			if (gamePads.xinputCountConnected)
			{
				gamePads.xinputCountConnected--;
			}
		}

		void DInputClean()
		{
			for (int i = 0; i < gamePads.directInputCountConnected; ++i)
			{
				if (gamePads.gamePadInfos[i].interfacePtr)
				{
					gamePads.gamePadInfos[i].interfacePtr->Release();
					gamePads.gamePadInfos[i].interfacePtr = NULL;
					gamePads.gamePadInfos[i].gamepadDevicePtr = NULL;
				}
				if (gamePads.gamePadInfos[i].hidDevice)
				{
					CloseHandle(gamePads.gamePadInfos[i].hidDevice);
					gamePads.gamePadInfos[i].hidDevice = NULL;
				}
				memset(gamePads.gamePadInfos[i].name, 0, sizeof(gamePads.gamePadInfos[i].gamepad.name));
				gamePads.gamePadInfos[i].created = false;
				gamePads.gamePadInfos[i].gamepad.Clean();
				gamePads.gamePadInfos[i].assignedHid = false;
				gamePads.gamePadInfos[i].rawInputDeviceHandle = NULL;
			}
			gamePadsInUse = 0;
			gamePads.directInputCountConnected = 0;
			IDirectInput8_EnumDevices(gamePads.dinput, DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, &gamePads, DIEDFL_ATTACHEDONLY);
		}

		void OnDeviceAdd()
		{
			DInputClean();
		}

		bool Init(void* _hwnd)
		{
			gamePads.handle = _hwnd;
			gamePadsInUse = 0;

			DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
			ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));

			NotificationFilter.dbcc_size = sizeof(NotificationFilter);
			NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			NotificationFilter.dbcc_reserved = 0;
			NotificationFilter.dbcc_classguid = USB_DEVICE;
			HDEVNOTIFY hDevNotify = RegisterDeviceNotification(_hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);


			HINSTANCE instance = GetModuleHandle(NULL);
			//create instance
			HRESULT re = CoCreateInstance(CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectInput8, (LPVOID*)&gamePads.dinput);
			if (FAILED(re))
			{
				LOGF(LogLevel::eERROR, "CoCreateInstance failed for gamePad %08x", re);
				return false;
			}
			if (!gamePads.dinput)
			{
				LOGF(LogLevel::eERROR, "gamePads.dinput is null");
				return false;
			}
			//initialize
			IDirectInput8_Initialize(gamePads.dinput, instance, DIRECTINPUT_VERSION);
			//enum devices
			IDirectInput8_EnumDevices(gamePads.dinput, DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, &gamePads, DIEDFL_ATTACHEDONLY);

			//Registering RawInput as well only for Effects(in future use only RawInput) : workaround
			RAWINPUTDEVICE Rid[2] = {};

			// Register gamepad:
			Rid[0].usUsagePage = 0x01;
			Rid[0].usUsage = 0x05;
			Rid[0].dwFlags = 0;
			Rid[0].hwndTarget = 0;

			// Register joystick:
			Rid[1].usUsagePage = 0x01;
			Rid[1].usUsage = 0x04;
			Rid[1].dwFlags = 0;
			Rid[1].hwndTarget = 0;

			RegisterRawInputDevices(Rid, ARRAYSIZE(Rid), sizeof(Rid[0]));
			return true;
			//
		}

		bool GetDirectInputGamePad(int padIndex, GamePadInfo& gamePadinfo)
		{
			if (padIndex < gamePads.xinputCountConnected)
			{
				//xinput already connected
				//preference given to xinput on windows
				return false;
			}
			if ((gamePads.directInputCountConnected - gamePadsInUse))
			{
				gamePadinfo = gamePads.gamePadInfos[gamePadsInUse];
				gamePadinfo.padIndex = gamePadsInUse++;

				const char* mapping = GetMappingStringForGUID(gamePadinfo.gamepad.hwdata.guid);
				if (mapping)
				{
					char* controllerMapping = GetControllerMappingFromMappingString(mapping);
					gamePadinfo.gamepad.mapping.SetFromString(controllerMapping);
					GetControllerNameFromMappingString(mapping, gamePadinfo.gamepad.name, sizeof(gamePadinfo.gamepad.name));
				}
				else
				{
					gamePadinfo.gamepad.mapping.SetDefaultMapping();
				}
				return true;
			}
			return false;
		}

		static unsigned char nibble(char c)
		{
			if ((c >= '0') && (c <= '9')) {
				return (unsigned char)(c - '0');
			}

			if ((c >= 'A') && (c <= 'F')) {
				return (unsigned char)(c - 'A' + 0x0a);
			}

			if ((c >= 'a') && (c <= 'f')) {
				return (unsigned char)(c - 'a' + 0x0a);
			}
			return 0;
		}


		JoystickGUID ParseStringToGUID(char* guidStr)
		{
			JoystickGUID guid;
			char name[32];
			memcpy(name, guidStr, 32);
			name[31] = '\0';
			size_t len = strlen(guidStr);
			if (len == 32 && memcmp(&guidStr[20], "504944564944", 12) == 0)
			{
				guidStr[20];
				memcpy(&name[20], "000000000000", 12);
				memcpy(&name[16], &name[4], 4);
				memcpy(&name[8], &name[0], 4);
				memcpy(&name[0], "03000000", 8);
			}

			int maxoutputbytes = sizeof(guid);
			UINT8 *p;
			size_t i;

			len = (len) & ~0x1;

			memset(&guid, 0x00, sizeof(guid));

			p = (UINT8 *)&guid;
			for (i = 0; (i < len) && ((p - (UINT8*)&guid) < maxoutputbytes); i += 2, p++) {
				*p = (nibble(name[i]) << 4) | nibble(name[i + 1]);
			}
			return guid;
		}

		~DirectInputInitializer()
		{
			free(RawDevList);
			RawDevList = NULL;
		}
	};

	class GainputInputDirectInputPadWin
	{
		GamePadInfo gamepadInfo;
		GamePad gamepad;
		int padIndex;
		int countIndx;
		uint8_t* input_messages[1024];
		LinearAllocator allocator;
	public:
		bool created;
		GainputInputDirectInputPadWin()
		{
			gamepadInfo.gamepad = { 0 };
			gamepadInfo.gamepadDevicePtr = NULL;
			gamepadInfo.interfacePtr = NULL;
			gamepadInfo.hidDevice = NULL;
			created = false;
			padIndex = 0;
			countIndx = 0;
			allocator.Reserve(1024 * 1024);
		}

		void Init(int index, void* handle)
		{
			DirectInputInitializer*instance = DirectInputInitializer::GetInstance(handle);
			if (instance && instance->GetDirectInputGamePad(index, gamepadInfo))
			{
				gamepad = gamepadInfo.gamepad;
				created = true;
				padIndex = index;
			}
		}

		void ParseMessage(void* lparam)
		{
			if (!created)
				return;
			UINT size = 0;
			UINT result;
			result = GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
			if (result)
				return;

			uint8_t* input = (uint8_t*)allocator.Allocate(size);
			if (input != nullptr)
			{
				result = GetRawInputData((HRAWINPUT)lparam, RID_INPUT, input, &size, sizeof(RAWINPUTHEADER));
				if (result == size)
				{
					const RAWINPUT& raw = *(PRAWINPUT)input;

					if (raw.header.dwType == RIM_TYPEHID)
					{
						if (!gamepadInfo.assignedHid)
						{
							gamepadInfo.assignedHid = true;
							RID_DEVICE_INFO info;
							info.cbSize = sizeof(RID_DEVICE_INFO);
							UINT bufferSize = sizeof(RID_DEVICE_INFO);
							UINT result = GetRawInputDeviceInfo(raw.header.hDevice, RIDI_DEVICEINFO, &info, &bufferSize);

							result = GetRawInputDeviceInfo(raw.header.hDevice, RIDI_DEVICENAME, NULL, &bufferSize);
							if (result != 0)
							{
								DLOGF(LogLevel::eERROR, "result!=0");
								return;
							}
							result = GetRawInputDeviceInfo(raw.header.hDevice, RIDI_DEVICENAME, (void*)gamepadInfo.name, &bufferSize);
							gamepadInfo.name[bufferSize] = '\0';
							gamepadInfo.rawInputDeviceHandle = raw.header.hDevice;
							if (result == -1)
							{
								DLOGF(LogLevel::eERROR, "result==-1");
							}
						}
					}
				}
			}
		}

		void OnDeviceRemove(int index, void* handle)
		{
			DirectInputInitializer*instance = DirectInputInitializer::GetInstance();
			if (index == padIndex)
			{
				if (instance)
				{
					if (created)
					{
						instance->DInputClean();
					}
					else
					{
						//xinput disconnected??
						instance->XInputClean();
					}
					created = false;
					//check if there are any devices connected and assign it to the current index
					if (instance->GetDirectInputGamePad(index, gamepadInfo))
					{
						gamepad = gamepadInfo.gamepad;
						created = true;
					}
				}
			}
		}

		void OnDeviceAdd(int index, void* hwnd)
		{
			DirectInputInitializer*instance = DirectInputInitializer::GetInstance();
			if (instance)
			{
				instance->OnDeviceAdd();
				if (instance->GetDirectInputGamePad(index, gamepadInfo))
				{
					gamepad = gamepadInfo.gamepad;
					created = true;
				}
			}
		}

		~GainputInputDirectInputPadWin()
		{
			if (created)
			{
				HRESULT re;
				if (gamepadInfo.interfacePtr)
				{
					re = gamepadInfo.interfacePtr->Release();
					gamepadInfo.interfacePtr = NULL;
				}
			}
		}
		char* GetDeviceName()
		{
			return gamepad.name;
		}

		int8_t TranslatePOV(DWORD value)
		{
			if (LOWORD(value) == 0xFFFF)
				return HAT_CENTERED;
			/* Round the value up: */
			value += 4500 / 2;
			value %= 36000;
			value /= 4500;

			if (value >= 8)
				return HAT_CENTERED;        /* shouldn't happen */

			return HAT_VALS[value];
		}

		void EvaluateJoystickAxis(GamePad& joystick, UINT8 axis, INT16 value)
		{
			/* Make sure we're not getting garbage or duplicate events */
			if (axis >= joystick.naxes)
			{
				return;
			}

			if (!joystick.axes[axis].has_initial_value)
			{
				joystick.axes[axis].initial_value = value;
				joystick.axes[axis].value = value;
				joystick.axes[axis].zero = value;
				joystick.axes[axis].has_initial_value = true;
			}

			if (value == joystick.axes[axis].value)
			{
				return;
			}

			if (!joystick.axes[axis].sent_initial_value)
			{
				const int MAX_ALLOWED_JITTER = (int)MaximumAxisValue / 100;
				if (abs(value - joystick.axes[axis].value) <= MAX_ALLOWED_JITTER)
				{
					return;
				}
				joystick.axes[axis].sent_initial_value = true;
				joystick.axes[axis].value = value; /* Just so we pass the check above */
				EvaluateJoystickAxis(joystick, axis, joystick.axes[axis].initial_value);
			}

			/* ignore events if we don't have keyboard focus, except for button
			 * release. */
			 /*TODO*/

			/* Update internal joystick state */
			joystick.axes[axis].value = value;
		}

		void EvaluateJoystickButton(GamePad& joystick, UINT8 button, UINT8 state)
		{
			/* Make sure we're not getting garbage*/
			if (button >= joystick.nbuttons)
			{
				return;
			}

			if (state == joystick.buttons[button])
			{
				return;
			}

			/* ignore events if we don't have keyboard focus, except for button
			 * release. */
			 /*TODO*/
			  /* Update internal joystick state */
			joystick.buttons[button] = state;
		}

		void EvaluateJoystickHat(GamePad& joystick, UINT8 hat, UINT8 value)
		{
			/* Make sure we're not getting garbage or duplicate events */
			if (hat >= joystick.nhats)
			{
				return;
			}
			if (value == joystick.hats[hat])
			{
				return;
			}

			/* ignore events if we don't have keyboard focus, except for button
			 * release. */
			 /*TODO*/

			/* Update internal joystick state */
			joystick.hats[hat] = value;
		}

		const float& max(const float& a, const float& b)
		{
			return (a < b) ? b : a;
		}

		const float& min(const float& a, const float& b)
		{
			return (a < b) ? b : a;
		}

		bool SetControllerFeedback(const ControllerFeedback& data)
		{
			if (!gamepadInfo.hidDevice)
			{
				gamepadInfo.hidDevice = CreateFile(
					gamepadInfo.name,
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
					OPEN_EXISTING, 0, NULL
				);

			}
			if (gamepadInfo.hidDevice == INVALID_HANDLE_VALUE)
			{
				DLOGF(LogLevel::eERROR, "gamepadInfo.hidDevice == INVALID_HANDLE_VALUE");
				return false;
			}
			//assert(gamepadInfo.hidDevice != INVALID_HANDLE_VALUE);
			uint8_t buf[32] = {};
			buf[0] = 0x05;
			buf[1] = 0xFF;
			buf[4] = data.vibration_right;
			buf[5] = data.vibration_left;
			buf[6] = data.r;
			buf[7] = data.g;
			buf[8] = data.b;
			DWORD bytes_written;
			BOOL result = WriteFile(gamepadInfo.hidDevice, buf, sizeof(buf), &bytes_written, NULL);
			if (result != TRUE)
			{
				DLOGF(LogLevel::eERROR, "result != TRUE");
				return false;
			}
			if (bytes_written != ARRAYSIZE(buf))
			{
				DLOGF(LogLevel::eERROR, "bytes_written != ARRAYSIZE(buf)");
				return false;
			}
			return result;
		}


		void Update(InputDeltaState* delta, InputState& state_, InputDevice& device_)
		{
			if (!created)
				return;

			countIndx = 0;
			allocator.Reset();

			if (gamepad.hwdata.buffered)
			{
				DInputBuffered(delta, state_, device_);
			}
			else
			{
				DInputPoll(delta, state_, device_);
			}

			HandleButton(device_, state_, delta, PadButtonA, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_A]]) != 0);
			HandleButton(device_, state_, delta, PadButtonB, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_B]]) != 0);
			HandleButton(device_, state_, delta, PadButtonX, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_X]]) != 0);
			HandleButton(device_, state_, delta, PadButtonY, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_Y]]) != 0);
			HandleButton(device_, state_, delta, PadButtonStart, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_START]]) != 0);
			HandleButton(device_, state_, delta, PadButtonSelect, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_BACK]]) != 0);
			HandleButton(device_, state_, delta, PadButtonL3, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_LEFTSTICK]]) != 0);
			HandleButton(device_, state_, delta, PadButtonR3, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_RIGHTSTICK]]) != 0);
			HandleButton(device_, state_, delta, PadButtonL1, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_LEFTSHOULDER]]) != 0);
			HandleButton(device_, state_, delta, PadButtonR1, (gamepad.buttons[gamepad.mapping[CONTROLLER_BUTTON_RIGHTSHOULDER]]) != 0);
			HandleButton(device_, state_, delta, PadButtonL2, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_LEFT_TRIGGER]].value) > 0.27);
			HandleButton(device_, state_, delta, PadButtonR2, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_RIGHT_TRIGGER]].value) > 0.27);

			if (gamepad.nbuttons >= 13)
				HandleButton(device_, state_, delta, PadButton17, gamepad.buttons[13] != 0);

			HandleAxis(device_, state_, delta, PadButtonAxis4, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_LEFT_TRIGGER]].value));
			HandleAxis(device_, state_, delta, PadButtonAxis5, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_RIGHT_TRIGGER]].value));
			HandleAxis(device_, state_, delta, PadButtonLeftStickX, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_LEFT_X]].value));
			HandleAxis(device_, state_, delta, PadButtonLeftStickY, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_LEFT_Y]].value * -1L));//its inverted??
			HandleAxis(device_, state_, delta, PadButtonRightStickX, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_RIGHT_X]].value));
			HandleAxis(device_, state_, delta, PadButtonRightStickY, GetAxisValue(gamepad.axes[gamepad.mapping[CONTROLLER_RIGHT_Y]].value * -1L));//its inverted??
		}

		void DInputBuffered(InputDeltaState* delta, InputState& state_, InputDevice& device_)
		{
			if (!gamepadInfo.interfacePtr)
			{
				ASSERT(gamepadInfo.interfacePtr && "GamePad Interface Ptr is null");
				return;
			}

			EvaluateJoystickHat(gamepad, 0, HAT_CENTERED);

			int i;
			HRESULT result;
			DWORD numevents;
			DIDEVICEOBJECTDATA evtbuf[INPUT_QSIZE];

			numevents = INPUT_QSIZE;
			result = IDirectInputDevice8_GetDeviceData(gamepadInfo.interfacePtr, sizeof(DIDEVICEOBJECTDATA), evtbuf, &numevents, 0);

			int num = 0;
			for (i = 0; i < (int)numevents; ++i)
			{
				for (int j = 0; j < gamepad.hwdata.NumInputs; ++j)
				{
					const input_t *in = &gamepad.hwdata.Inputs[j];

					if (evtbuf[i].dwOfs != in->ofs)
						continue;

					switch (in->type)
					{
					case Type::AXIS:
					{
						EvaluateJoystickAxis(gamepad, in->num, (INT16)evtbuf[i].dwData);

					}
					break;
					case Type::BUTTON:
						EvaluateJoystickButton(gamepad, in->num, (UINT8)(evtbuf[i].dwData ? 1 : 0));
						break;
					case Type::HAT:
					{
						UINT8 pos = TranslatePOV(evtbuf[i].dwData);
						EvaluateJoystickHat(gamepad, in->num, pos);
						num = in->num;
						HandleButtonsForHat(delta, state_, device_, num);
					}
					break;
					}
				}
			}

		}

		void DInputPoll(InputDeltaState* delta, InputState& state_, InputDevice& device_)
		{
			if (!gamepadInfo.interfacePtr)
			{
				ASSERT(gamepadInfo.interfacePtr && "GamePad Interface Ptr is null");
				return;
			}

			DIJOYSTATE state = { 0 };
			HRESULT result;
			int i;

			result = IDirectInputDevice8_GetDeviceState(gamepadInfo.interfacePtr, sizeof(DIJOYSTATE), &state);

			if (result != DI_OK)
			{
				return;
			}

			/* Set each known axis, button and POV. */
			for (i = 0; i < gamepad.hwdata.NumInputs; ++i)
			{
				const input_t *in = &gamepad.hwdata.Inputs[i];

				switch (in->type)
				{
				case Type::AXIS:
					switch (in->ofs)
					{
					case O_DIJOFS_X:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lX);
						break;
					case O_DIJOFS_Y:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lY);
						break;
					case O_DIJOFS_Z:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lZ);
						break;
					case O_DIJOFS_RX:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lRx);
						break;
					case O_DIJOFS_RY:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lRy);
						break;
					case O_DIJOFS_RZ:
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.lRz);
						break;
					case O_DIJOFS_SLIDER(0):
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.rglSlider[0]);
						break;
					case O_DIJOFS_SLIDER(1):
						EvaluateJoystickAxis(gamepad, in->num, (INT16)state.rglSlider[1]);
						break;
					}
					break;

				case Type::BUTTON:
					EvaluateJoystickButton(gamepad, in->num, (UINT8)(state.rgbButtons[in->ofs - DIJOFS_BUTTON0] ? 1 : 0));
					break;
				case Type::HAT:
				{
					UINT8 pos = TranslatePOV(state.rgdwPOV[in->ofs - DIJOFS_POV(0)]);
					EvaluateJoystickHat(gamepad, in->num, pos);
					//HandleButtonsForHat(delta, state_, device_, in->num);
					break;
				}
				}
			}
		}


		void HandleButtonsForHat(InputDeltaState* delta, InputState& state_, InputDevice& device_, int num)
		{
			if (num >= gamepad.nhats)
				return;

			HandleButton(device_, state_, delta, PadButtonUp, (gamepad.hats[num] & gamepad.mapping.hat[CONTROLLER_HAT_UP]));
			HandleButton(device_, state_, delta, PadButtonDown, (gamepad.hats[num] & gamepad.mapping.hat[CONTROLLER_HAT_DOWN]));
			HandleButton(device_, state_, delta, PadButtonLeft, (gamepad.hats[num] & gamepad.mapping.hat[CONTROLLER_HAT_LEFT]));
			HandleButton(device_, state_, delta, PadButtonRight, (gamepad.hats[num] & gamepad.mapping.hat[CONTROLLER_HAT_RIGHT]));
		}
		static float GetAxisValue(SHORT value)
		{
			if (value < 0)
			{
				return float(value) / MinimumAxisValue;
			}
			else
			{
				return float(value) / MaximumAxisValue;
			}
		}

		static float GetAxisValue(LONG value)
		{
			if (value < 0)
			{
				return float(value) / MinimumAxisValue;
			}
			else
			{
				return float(value) / MaximumAxisValue;
			}
		}
	};
}

#endif