Gainput [![Build Status](https://travis-ci.org/jkuhlmann/gainput.png?branch=master)](https://travis-ci.org/jkuhlmann/gainput) [![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)
=======

Gainput is the awesome C++ input library for your game:

- handles your input needs from low-level device reading to high-level mapping of user-defined buttons
- well-documented, clean, lightweight, and easy to use
- a unified interface on all supported platforms: **Android NDK, iOS/tvOS, Linux, macOS, Windows**
- supported devices: keyboard, mouse, gamepad, multi-touch, device built-in sensors
- [Open Source (MIT license)](https://github.com/jkuhlmann/gainput/blob/master/LICENSE)
- [complete list of features](#features)
- [API documentation](http://gainput.johanneskuhlmann.de/api/)


Usage
-----

```cpp
#include <gainput/gainput.h>

enum Button
{
	ButtonConfirm
};

gainput::InputManager manager;
manager.SetDisplaySize(displayWidth, displayHeight);
const gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();
const gainput::DeviceId padId = manager.CreateDevice<gainput::InputDevicePad>();
const gainput::DeviceId touchId = manager.CreateDevice<gainput::InputDeviceTouch>();

gainput::InputMap map(manager);
map.MapBool(ButtonConfirm, keyboardId, gainput::KeyReturn);
map.MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
map.MapBool(ButtonConfirm, padId, gainput::PadButtonA);
map.MapBool(ButtonConfirm, touchId, gainput::Touch0Down);

while (running)
{
	manager.Update();

	// May need some platform-specific message handling here

	if (map.GetBoolWasDown(ButtonConfirm))
	{
		// Confirmed!
	}
}
```


Features
--------

- Offers a **unified interface** on all supported platforms. (Some minor changes are necessary to setup the library.)
- Provides a low-level and high-level interface: Query the state of input devices buttons directly or map device buttons to a user button. That way it's easy to support alternative inputs or change the **input mappings** around later.
- Supports **recording and playback** of input sequences.
- Features a **network server** to obtain information on devices and mappings from.
- Two Gainput instances can **sync device states over the network**. It's also possible to receive **multi-touch inputs from a smartphone**'s regular browser.
- Completely written in portable **C++**.
- **No STL** is used. **No exceptions** are thrown. **No RTTI** is used. **No C++11**, and **no boost**.
- **No weird external dependencies** are used. Relies on the existing platform SDKs.
- **Easily set up and built** using your favorite IDE/build tool.
- **Listeners** can be installed both for devices buttons as well as user buttons. That way you are notified when a button state changes.
- **Gestures** allow for more complex input patterns to be detected, for example double-clicking, pinch/rotation gestures, or holding several buttons simultaneously.
- An **external allocator** can be supplied to the library so that all memory management is done the way you want it.
- Supports **raw input** on Linux and Windows.
- Gamepad rumbling is supported where available.
- It's easy to check for all pressed buttons so that offering a way to the players to remap their buttons is easy to implement. Similarly it's easy to save and load mappings.
- Possibly unnecessary features, like gestures or the network server, are easily disabled.
- **Dead zones** can be set up for any float-value button.
- **State changes**, i.e. if a button is newly down or just released, can be checked for.


Building
--------

By default, Gainput is built using [CMake](http://www.cmake.org/).

1. Run `mkdir build`
1. Run `cmake ..`
1. Run `make`
1. The library can be found in `lib/`, the executables in `samples/`.


Contributing
------------

Everyone is welcome to contribute to the library. If you find any problems, you can submit them using [GitHub's issue system](https://github.com/jkuhlmann/gainput/issues). If you want to contribute code, you should fork the project and then send a pull request.


Dependencies
------------

Gainput has a minimal number of external dependencies to make it as self-contained as possible. It uses the platforms' default ways of getting inputs and doesn't use the STL.


Testing
-------

Generally, testing should be done by building and running Gainput on all supported platforms. The samples in the `samples/` folder should be used in order to determine if the library is functional.

The unit tests in the `test/` folder are built by the normal CMake build. The executable can be found in the `test/` folder. All build configurations and unit tests are built and run by Travis CI whenever something is pushed into the repository.


Alternatives
------------

- [OIS](https://github.com/wgois/Object-oriented-Input-System--OIS-)
- [SDL](http://www.libsdl.org/)

