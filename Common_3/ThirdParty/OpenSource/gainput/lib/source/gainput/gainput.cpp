
#include "../../include/gainput/gainput.h"

/**
\mainpage Gainput Documentation

Gainput is a C++ Open Source input library for games. It collects input data from different devices (like keyboards, mice or gamepads) and makes the inputs accessible through a unified interface. On top of that, inputs can be mapped to user-defined buttons and other advanced features are offered. Gainput aims to be a one-stop solution to acquire input from players for your game.

If there are any problems, please report them on <a href="https://github.com/jkuhlmann/gainput/issues" target="_blank">GitHub</a> or contact: <tt>gainput -a-t- johanneskuhlmann.de</tt>.

These pages are Gainput's API documentation. In order to download Gainput go to the <a href="http://gainput.johanneskuhlmann.de/" target="_blank">Gainput website</a>.

\section contents Contents
- \ref page_start
- \ref page_building
- \ref page_platforms
- \ref page_dependencies
- \ref page_faq

\section using Using Gainput
A minimal usage sample:

\code
#include <gainput/gainput.h>

// Define your user buttons somewhere global
enum Button
{
	ButtonConfirm
};

// Setting up Gainput
gainput::InputManager manager;
const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();

manager.SetDisplaySize(width, height);

gainput::InputMap map(manager);
map.MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);

while (game_running)
{
	// Call every frame
	manager.Update();

	// May have to call platform-specific event-handling functions here.

	// Check button state
	if (map.GetBoolWasDown(ButtonConfirm))
	{
		// Confirmed!
	}
}
\endcode


\section license License

Gainput is licensed under the MIT license:

\include "LICENSE"



\page page_start Getting Started
\tableofcontents
This page gives an overview on how to get Gainput into your game and use it for getting your players' input.


\section sect_obtaining Obtaining Gainput
Gainput can be obtained in source from <a href="https://github.com/jkuhlmann/gainput">GitHub</a>.

\section sect_building Building
Build Gainput as described on the \ref page_building page.


\section sect_integrating Integration Into Your Project
To begin with, your project should link to the dynamic or static version of the Gainput library. On Linux, the files are \c libgainput.so (dynamic library) and \c libgainputstatic.a (static library). On Windows, the filenames are \c gainput.lib (used with \c gainput.dll) and \c gainputstatic.lib. In case you decide to use the dynamic library, make sure to distribute the dynamic library together with your executable.

To have the API available, you have to include Gainput's main header file:

\code
#include <gainput/gainput.h>
\endcode

You should have the \c lib/include/ folder as an include folder in your project settings for this to work. The file includes most of Gainput's other header files so that you shouldn't need to include anything else.


\section sect_setup Setting up Gainput
Gainput's most important class is gainput::InputManager. You should create one that you use throughout your game. Create some input devices using gainput::InputManager::CreateDevice(). And then, during your game loop, call gainput::InputManager::Update() every frame.

Some platform-specific function calls may be necessary, like gainput::InputManager::HandleMessage().

On top of your gainput::InputManager, you should create at least one gainput::InputMap and map some device-specific buttons to your custom user buttons using gainput::InputMap::MapBool() or gainput::InputMap::MapFloat(). This will allow you to more conventienly provide alternative input methods or enable the player to change button mappings.


\section sect_using Using Gainput
After everything has been set up, use gainput::InputMap::GetBool() or gainput::InputMap::GetFloat() (and related functions) anywhere in your game to get player input.




\page page_building Building
Gainput is built using CMake which makes it easy to build the library.

Simply run these commands:
-# `mkdir build`
-# `cd build`
-# `cmake ..`
-# `make`

There are the regular CMake build configurations from which you choose by adding one these to the `cmake` call, for example:
- `-DCMAKE_BUILD_TYPE=Debug`
- `-DCMAKE_BUILD_TYPE=Release`

Building Gainput as shown above, will build a dynamic-link library, a static-link library, and all samples. The executables can be found in the \c build/ folder.


\section sect_defines Build Configuration Defines
There is a number of defines that determine what is included in the library and how it behaves. Normally, most of these are set by the build scripts or in gainput.h, but it may be necessary to set these when doing custom builds or modifying the build process. All defines must be set during compilation of the library itself.

Name | Description
-----|------------
\c GAINPUT_DEBUG | Enables debugging of the library itself, i.e. enables a lot of internal logs and checks.
\c GAINPUT_DEV | Enables the built-in development tool server that external tools or other Gainput instances can connect to.
\c GAINPUT_ENABLE_ALL_GESTURES | Enables all gestures. Note that there is also an individual define for each gesture (see gainput::InputGesture).
\c GAINPUT_ENABLE_RECORDER | Enables recording of inputs.
\c GAINPUT_LIB_BUILD | Should be set if Gainput is being built as a library.


\section sect_android_build Android NDK
In order to cross-compile for Android, the build has to be configured differently.

Naturally, the Android NDK must be installed. Make sure that \c ANDROID_NDK_PATH is set to the absolute path to your installation. And then follow these steps:

- Run `cmake -DCMAKE_TOOLCHAIN_FILE=../extern/cmake/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DANDROID_ABI="armeabi-v7a" -DANDROID_NATIVE_API_LEVEL=android-19 -DANDROID_STL="gnustl_static"`
- Build as above.

Executing these commands will also yield both a dynamic and static library in the \c build/ folder.


\page sample_fw Sample Framework
This framework makes it easier to provide succinct samples by taking care of the platform-dependent window creation/destruction.

In a proper project, everything that is handled by the sample framework should be handled for you by some other library or engine. The sample framework is not meant for production use.

\example ../../../samples/basic/basicsample_win.cpp
Shows the most basic initialization and usage of Gainput. It has separate implementations for all supported platforms.

\example ../../../samples/basic/basicsample_linux.cpp
Shows the most basic initialization and usage of Gainput. It has separate implementations for all supported platforms.

\example ../../../samples/basic/basicsample_android.cpp
Shows the most basic initialization and usage of Gainput. It has separate implementations for all supported platforms.

\example ../../../samples/dynamic/dynamicsample.cpp
Shows how to let the user dynamically change button mappings as well as how to load and save mappings. Uses the \ref sample_fw.

\example ../../../samples/gesture/gesturesample.cpp
Shows how to use input gestures. Also shows how to implement your own custom input device. Uses the \ref sample_fw.

\example ../../../samples/listener/listenersample.cpp
Shows how to use device button listeners as well as user button listeners. Uses the \ref sample_fw.

\example ../../../samples/recording/recordingsample.cpp
Shows how to record, play and serialize/deserialize input sequences. Uses the \ref sample_fw.

\example ../../../samples/sync/syncsample.cpp
Shows how to connect two Gainput instances to each other and send the device state over the network. Uses the \ref sample_fw. Works only in the \c dev build configuration.


\page page_platforms Platform Notes
\tableofcontents

\section platform_matrix Device Support Matrix

Platform \\ Device | Built-In | Keyboard | Mouse | Pad | Touch
-------------------|----------|----------|-------|-----|------
Android NDK | YES | YES | | | YES
iOS | YES | | | YES | YES
Linux | | STD  RAW | STD  RAW | YES | |
Mac OS X | | YES | YES | YES | |
Windows | | STD  RAW | STD  RAW | YES | |

What the entries mean:
- YES: This device is supported on this platform (basically, the same as STD).
- STD: This device is supported in standard variant on this platform (gainput::InputDevice::DV_STANDARD).
- RAW: This device is supported in raw variant on this platform (gainput::InputDevice::DV_RAW).


\section platform_android Android NDK
The keyboard support is limited to hardware keyboards, including, for example, hardware home buttons.

\section platform_linux Linux
Evdev is used for the raw input variants. Evdev has permission issues on some Linux distributions where the devices (\c /dev/input/event*) are only readable by root or a specific group. If a raw device's state is gainput::InputDevice::DS_UNAVAILABLE this may very well be the cause.

These gamepads have been tested and are explicitly supported:
- Microsoft X-Box 360 pad
- Sony PLAYSTATION(R)3 Controller

\section platform_osx Mac OS X
These gamepads have been tested and are explicitly supported:
- Microsoft X-Box 360 pad
- Sony PLAYSTATION(R)3 Controller

\section platform_windows Windows
The gamepad support is implemented using XINPUT which is Microsoft's most current API for such devices. However, that means that only Xbox 360 pads and compatible devices are supported.


\page page_dependencies Dependencies
Gainput has very few external dependencies in order to make it as self-contained as possible. Normally, the only extra piece of software you might have to install is CMake. Anything else should come with your IDE (or regular platform SDK).

\section sect_libs Libraries
Most importantly, Gainput does not depend on the STL or any other unnecessary helper libraries. Input is acquired using the following methods:

Android NDK: All input is acquired through the NDK. Native App Glue is used for most inputs.

iOS:
- CoreMotion framework for built-in device sensors
- GameController framework for gamepad inputs
- UIKit for touch inputs

Linux:
- the X11 message loop is used for keyboard and mouse
- the kernel's joystick API is used for pads

Mac OS X:
- AppKit for mouse
- IOKit's IOHIDManager for keyboard and gamepads

Windows:
- the Win32 message loop is used for keyboard and mouse
- XINPUT is used for gamepads

\section sect_building Building
Gainput is built using <a href="http://www.cmake.org/" target="_blank">CMake</a> 


\page page_faq FAQ

\tableofcontents

\section faq0 Why another library when input is included in most engines/libraries?
There are lots of other ways to acquire input, most are part of more complete engines or more comprehensive libraries. For one, Gainput is meant for those who are using something without input capabilities (for example, pure rendering engines) or those who are developing something themselves and want to skip input handling.

In the long run, Gainput aims to be better and offer more advanced features than built-in input solutions. That's the reason why more advanced features, like input recording/playback, remote syncing, gestures and external tool support, are already part of the library.


\page page_devprotocol Development Tool Protocol
If Gainput is built with \c GAINPUT_DEV defined, it features a server that external tools can connect to obtain information on devices, mappings and button states. The underlying protocol is TCP/IP and the default port 1211.

The following messages are defined:

\code
hello
{
	uint8_t cmd
	uint32_t protocolVersion
	uint32_t libVersion
}

device
{
	uint8_t cmd
	uint32_t deviceId
	string name
}

device button
{
	uint8_t cmd
	uint32_t deviceId
	uint32_t buttonId
	string name
	uint8_t type
}

map
{
	uint8_t cmd
	uint32_t mapId
	string name
}

remove map
{
	uint8_t cmd
	uint32_t mapId
}

user button
{
	uint8_t cmd
	uint32_t mapId
	uint32_t buttonId
	uint32_t deviceId
	uint32_t deviceButtonId
	float value
}

remove user button
{
	uint8_t cmd
	uint32_t mapId
	uint32_t buttonId
}

user button changed
{
	uint8_t cmd
	uint32_t mapId
	uint32_t buttonId
	uint8_t type
	uint8_t/float value
}

ping
{
	uint8_t cmd
}

get all infos
{
	uint8_t cmd
}

start device sync
{
	uint8_t cmd
	uint8_t deviceType
	uint8_t deviceIndex
}

set device button
{
	uint8_t cmd
	uint8_t deviceType
	uint8_t deviceIndex
	uint32_t deviceButtonId
	uint8_t/float value
}
\endcode

The message IDs (\c cmd) are defined in GainputDevProtocol.h.

Each message is prefaced with a \c uint8_t that specifies the message's length.

All integers are in network byte order.

Strings are represented like this:

\code
{
	uint8_t length
	char text[length] // without the trailing \0
}
\endcode

*/

namespace gainput
{

const char*
GetLibName()
{
	return "Gainput";
}

uint32_t
GetLibVersion()
{
	return ((1 << GAINPUT_VER_MAJOR_SHIFT) | (0) );
}

const char*
GetLibVersionString()
{
	return "1.0.0";
}

}

