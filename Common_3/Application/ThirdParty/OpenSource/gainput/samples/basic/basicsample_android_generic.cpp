
#include <gainput/gainput.h>

#if defined(GAINPUT_PLATFORM_ANDROID)

#include <jni.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "gainput", __VA_ARGS__))

// Define your user buttons
enum Button
{
	ButtonMenu,
	ButtonConfirm,
	MouseX,
	MouseY
};

gainput::InputManager* manager;
gainput::InputMap* map;
gainput::DeviceId mouseId;
gainput::DeviceId keyboardId;
gainput::DeviceId padId;
gainput::DeviceId touchId;


extern "C" {
JNIEXPORT void JNICALL
Java_com_example_gainput_gainput_BasicActivity_nativeOnCreate(JNIEnv * /*env*/, jobject /*thiz*/)
{
	manager = new gainput::InputManager;
	mouseId = manager->CreateDevice<gainput::InputDeviceMouse>();
	keyboardId = manager->CreateDevice<gainput::InputDeviceKeyboard>();
	padId = manager->CreateDevice<gainput::InputDevicePad>();
	touchId = manager->CreateDevice<gainput::InputDeviceTouch>();

	map = new gainput::InputMap(*manager);
	map->MapBool(ButtonMenu, keyboardId, gainput::KeyBack);
	map->MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
	map->MapBool(ButtonConfirm, padId, gainput::PadButtonA);
	map->MapBool(ButtonConfirm, touchId, gainput::Touch0Down);

	map->MapFloat(MouseX, mouseId, gainput::MouseAxisX);
	map->MapFloat(MouseY, mouseId, gainput::MouseAxisY);
	map->MapFloat(MouseX, touchId, gainput::Touch0X);
	map->MapFloat(MouseY, touchId, gainput::Touch0Y);
}

JNIEXPORT void JNICALL
Java_com_example_gainput_gainput_BasicActivity_nativeOnUpdate(JNIEnv * /*env*/, jobject /*thiz*/)
{
	manager->Update();

	if (map->GetBoolWasDown(ButtonMenu))
	{
		LOGI("Open Menu!!");
	}
	if (map->GetBoolWasDown(ButtonConfirm))
	{
		LOGI("Confirmed!!");
	}
	if (map->GetBool(ButtonConfirm))
	{
		LOGI("LM down");
	}

	if (map->GetFloatDelta(MouseX) != 0.0f || map->GetFloatDelta(MouseY) != 0.0f)
	{
		LOGI("Mouse: %f, %f", map->GetFloat(MouseX), map->GetFloat(MouseY));
	}
}

}

#endif

