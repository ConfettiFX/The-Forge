package de.johanneskuhlmann.gainput;

import android.content.Context;
import android.hardware.input.InputManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.util.HashMap;
import java.util.Map;

public class Gainput implements InputManager.InputDeviceListener
{

	// Must have same order/values as the respective enum in GainputInputDevicePad.h
	enum PadButton
	{
		PadButtonLeftStickX,
		PadButtonLeftStickY,
		PadButtonRightStickX,
		PadButtonRightStickY,
		PadButtonAxis4, // L2/Left trigger
		PadButtonAxis5, // R2/Right trigger
		PadButtonAxis6,
		PadButtonAxis7,
		PadButtonAxis8,
		PadButtonAxis9,
		PadButtonAxis10,
		PadButtonAxis11,
		PadButtonAxis12,
		PadButtonAxis13,
		PadButtonAxis14,
		PadButtonAxis15,
		PadButtonAxis16,
		PadButtonAxis17,
		PadButtonAxis18,
		PadButtonAxis19,
		PadButtonAxis20,
		PadButtonAxis21,
		PadButtonAxis22,
		PadButtonAxis23,
		PadButtonAxis24,
		PadButtonAxis25,
		PadButtonAxis26,
		PadButtonAxis27,
		PadButtonAxis28,
		PadButtonAxis29,
		PadButtonAxis30,
		PadButtonAxis31,
		PadButtonAccelerationX,
		PadButtonAccelerationY,
		PadButtonAccelerationZ,
		PadButtonGravityX,
		PadButtonGravityY,
		PadButtonGravityZ,
		PadButtonGyroscopeX,
		PadButtonGyroscopeY,
		PadButtonGyroscopeZ,
		PadButtonMagneticFieldX,
		PadButtonMagneticFieldY,
		PadButtonMagneticFieldZ,
		PadButtonStart,
		PadButtonSelect,
		PadButtonLeft,
		PadButtonRight,
		PadButtonUp,
		PadButtonDown,
		PadButtonA, // Cross
		PadButtonB, // Circle
		PadButtonX, // Square
		PadButtonY, // Triangle
		PadButtonL1,
		PadButtonR1,
		PadButtonL2,
		PadButtonR2,
		PadButtonL3, // Left thumb
		PadButtonR3, // Right thumb
		PadButtonHome, // PS button
		PadButton17,
		PadButton18,
		PadButton19,
		PadButton20,
		PadButton21,
		PadButton22,
		PadButton23,
		PadButton24,
		PadButton25,
		PadButton26,
		PadButton27,
		PadButton28,
		PadButton29,
		PadButton30,
		PadButton31,
		PadButtonMax_
	}

	// Must have same order/values as the respective enum in GainputInputDevice.h
	enum DeviceType
	{
		DT_MOUSE,		///< A mouse/cursor input device featuring one pointer.
		DT_KEYBOARD,		///< A keyboard input device.
		DT_PAD,			///< A joypad/gamepad input device.
		DT_TOUCH,		///< A touch-sensitive input device supporting multiple simultaneous pointers.
		DT_BUILTIN,		///< Any controls directly built into the device that also contains the screen.
		DT_REMOTE,		///< A generic networked input device.
		DT_GESTURE,		///< A gesture input device, building on top of other input devices.
		DT_CUSTOM,		///< A custom, user-created input device.
		DT_COUNT		///< The count of input device types.
	}

	public static native void nativeOnInputBool(int deviceType, int deviceIndex, int buttonId, boolean value);
	public static native void nativeOnInputFloat(int deviceType, int deviceIndex, int buttonId, float value);
	public static native void nativeOnDeviceChanged(int deviceId, boolean available);

	public float viewWidth = 1.0f;
	public float viewHeight = 1.0f;

	private float getRealX(float x)
	{
		return (x/viewWidth);
	}

	private float getRealY(float y)
	{
		return (y/viewHeight);
	}

	public Gainput(Context context)
	{
		inputManager_ = (InputManager) context.getSystemService(Context.INPUT_SERVICE);

		if (inputManager_ != null)
		{
			inputManager_.registerInputDeviceListener(this, null);
		}

		int[] deviceIds = InputDevice.getDeviceIds();
		for (int deviceId : deviceIds)
		{
			InputDevice dev = InputDevice.getDevice(deviceId);
			int sources = dev.getSources();
			if (((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
					|| ((sources & InputDevice.SOURCE_JOYSTICK)
					== InputDevice.SOURCE_JOYSTICK))
			{
				onInputDeviceAdded(deviceId);
			}
		}
	}

	@Override
	protected void finalize()
	{
		if (inputManager_ != null)
		{
			inputManager_.unregisterInputDeviceListener(this);
		}
	}

	private final InputManager inputManager_;
	private Map<Integer, Integer> deviceIdMappings_ = new HashMap<Integer, Integer>();

	private int translateDeviceIdToIndex(int deviceId)
	{
		Integer index = deviceIdMappings_.get(deviceId);
		if (index != null)
		{
			return index;
		}

		// Find the lowest non-used index.
		for (int i = 0; i < 1000; ++i)
		{
			if (!deviceIdMappings_.containsValue(i))
			{
				deviceIdMappings_.put(deviceId, i);
				return i;
			}
		}
		return 0;
	}

	@Override
	public void onInputDeviceAdded(int deviceId)
	{
		nativeOnDeviceChanged(translateDeviceIdToIndex(deviceId), true);
	}

	@Override
	public void onInputDeviceChanged(int deviceId)
	{
	}

	@Override
	public void onInputDeviceRemoved(int deviceId)
	{
		int oldDeviceId = translateDeviceIdToIndex(deviceId);
		deviceIdMappings_.remove(deviceId);
		nativeOnDeviceChanged(oldDeviceId, false);
	}

	private void handleAxis(int deviceId, PadButton button, float value)
	{
		boolean isButton = false;
		if (button == PadButton.PadButtonLeft
				|| button == PadButton.PadButtonUp)
		{
			if (value < -0.5f)
				value = -1.0f;
			else
				value = 0.0f;
			isButton = true;
		}
		else if (button == PadButton.PadButtonRight
				|| button == PadButton.PadButtonDown)
		{
			if (value > 0.5f)
				value = 1.0f;
			else
				value = 0.0f;
			isButton = true;
		}

		if (isButton)
		{
			nativeOnInputBool(DeviceType.DT_PAD.ordinal(), deviceId, button.ordinal(), value != 0.0f);
		}
		else
		{
			nativeOnInputFloat(DeviceType.DT_PAD.ordinal(), deviceId, button.ordinal(), value);
		}
	}

	public boolean handleMotionEvent(MotionEvent event)
	{
		int source = event.getSource();
		if ((source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
				|| (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
		{
			int deviceId = translateDeviceIdToIndex(event.getDeviceId());
			handleAxis(deviceId, PadButton.PadButtonLeftStickX, event.getAxisValue(MotionEvent.AXIS_X));
			handleAxis(deviceId, PadButton.PadButtonLeftStickY, -event.getAxisValue(MotionEvent.AXIS_Y));
			handleAxis(deviceId, PadButton.PadButtonRightStickX, event.getAxisValue(MotionEvent.AXIS_Z));
			handleAxis(deviceId, PadButton.PadButtonRightStickY, -event.getAxisValue(MotionEvent.AXIS_RZ));
			handleAxis(deviceId, PadButton.PadButtonAxis4, event.getAxisValue(MotionEvent.AXIS_LTRIGGER));
			handleAxis(deviceId, PadButton.PadButtonAxis5, event.getAxisValue(MotionEvent.AXIS_RTRIGGER));
			handleAxis(deviceId, PadButton.PadButtonLeft, event.getAxisValue(MotionEvent.AXIS_HAT_X));
			handleAxis(deviceId, PadButton.PadButtonRight, event.getAxisValue(MotionEvent.AXIS_HAT_X));
			handleAxis(deviceId, PadButton.PadButtonUp, event.getAxisValue(MotionEvent.AXIS_HAT_Y));
			handleAxis(deviceId, PadButton.PadButtonDown, event.getAxisValue(MotionEvent.AXIS_HAT_Y));
			return true;
		}
		return false;
	}

	private float getButtonState(KeyEvent event)
	{
		if (event.getAction() == KeyEvent.ACTION_DOWN)
			return 1.0f;
		return 0.0f;
	}

	private void handleButton(int deviceId, PadButton button, KeyEvent event)
	{
		float state = getButtonState(event);
		nativeOnInputBool(DeviceType.DT_PAD.ordinal(), deviceId, button.ordinal(), state != 0.0f);
	}

	public boolean handleKeyEvent(KeyEvent event)
	{
		int keyCode = event.getKeyCode();
		int source = event.getSource();
		if ((source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
				|| (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
		{
			int deviceId = translateDeviceIdToIndex(event.getDeviceId());
			if (keyCode == KeyEvent.KEYCODE_DPAD_UP)
				handleButton(deviceId, PadButton.PadButtonUp, event);
			else if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN)
				handleButton(deviceId, PadButton.PadButtonDown, event);
			else if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT)
				handleButton(deviceId, PadButton.PadButtonLeft, event);
			else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT)
				handleButton(deviceId, PadButton.PadButtonRight, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_A)
				handleButton(deviceId, PadButton.PadButtonA, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_B)
				handleButton(deviceId, PadButton.PadButtonB, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_X)
				handleButton(deviceId, PadButton.PadButtonX, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_Y)
				handleButton(deviceId, PadButton.PadButtonY, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_L1)
				handleButton(deviceId, PadButton.PadButtonL1, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_R1)
				handleButton(deviceId, PadButton.PadButtonR1, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_THUMBL)
				handleButton(deviceId, PadButton.PadButtonL3, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_THUMBR)
				handleButton(deviceId, PadButton.PadButtonR3, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT)
				handleButton(deviceId, PadButton.PadButtonSelect, event);
			else if (keyCode == KeyEvent.KEYCODE_BUTTON_START)
				handleButton(deviceId, PadButton.PadButtonStart, event);
			else if (keyCode == KeyEvent.KEYCODE_HOME)
				handleButton(deviceId, PadButton.PadButtonHome, event);
			return true;
		}
		else if ((source & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD)
		{
			boolean down = event.getAction() == KeyEvent.ACTION_DOWN;
			nativeOnInputBool(DeviceType.DT_KEYBOARD.ordinal(), 0, event.getKeyCode(), down);

			if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP))
			{
				return false;
			}
			else
			{
				return true;
			}
		}
		return false;

	}

	public boolean handleTouchEvent(MotionEvent event)
	{
		try
		{
			final int action = event.getAction() & MotionEvent.ACTION_MASK;
			final int numberOfPointers = event.getPointerCount();

			switch (action)
			{
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN:
				{
					for (int i = 0; i < numberOfPointers; ++i)
					{
						final int pointerId = event.getPointerId(i);
						final float x_move = getRealX(event.getX(i));
						final float y_move = getRealY(event.getY(i));
						nativeOnInputBool(DeviceType.DT_TOUCH.ordinal(), 0, 0 + 4 * pointerId, true);
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 1 + 4 * pointerId, x_move);
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 2 + 4 * pointerId, y_move);
					}
					break;
				}
				case MotionEvent.ACTION_MOVE:
				{
					for (int i = 0; i < numberOfPointers; ++i)
					{
						final int pointerId = event.getPointerId(i);
						final float x_move = getRealX(event.getX(i));
						final float y_move = getRealY(event.getY(i));
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 1 + 4 * pointerId, x_move);
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 2 + 4 * pointerId, y_move);
					}
					break;
				}
				case MotionEvent.ACTION_POINTER_UP:
				case MotionEvent.ACTION_UP:
				{
					for (int i = 0; i < numberOfPointers; ++i)
					{
						final int pointerId = event.getPointerId(i);
						final float x_move = getRealX(event.getX(i));
						final float y_move = getRealY(event.getY(i));
						nativeOnInputBool(DeviceType.DT_TOUCH.ordinal(), 0, 0 + 4 * pointerId, false);
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 1 + 4 * pointerId, x_move);
						nativeOnInputFloat(DeviceType.DT_TOUCH.ordinal(), 0, 2 + 4 * pointerId, y_move);
					}
					break;
				}
			}
			return true;
		}
		catch (final Exception ex)
		{
			ex.printStackTrace();
			return false;
		}
	}
}
