set(GAINPUT_SRC
    # cmake-format: sortable
    lib/source/gainput/builtin/GainputInputDeviceBuiltIn.cpp
    lib/source/gainput/dev/GainputDev.cpp
    lib/source/gainput/dev/GainputMemoryStream.cpp
    lib/source/gainput/dev/GainputNetAddress.cpp
    lib/source/gainput/dev/GainputNetConnection.cpp
    lib/source/gainput/dev/GainputNetListener.cpp
    lib/source/gainput/gainput.cpp
    lib/source/gainput/GainputAllocator.cpp
    lib/source/gainput/GainputInputDeltaState.cpp
    lib/source/gainput/GainputInputDevice.cpp
    lib/source/gainput/GainputInputManager.cpp
    lib/source/gainput/GainputInputMap.cpp
    lib/source/gainput/GainputInputState.cpp
    lib/source/gainput/GainputMapFilters.cpp
    lib/source/gainput/gestures/GainputButtonStickGesture.cpp
    lib/source/gainput/gestures/GainputDoubleClickGesture.cpp
    lib/source/gainput/gestures/GainputHoldGesture.cpp
    lib/source/gainput/gestures/GainputPinchGesture.cpp
    lib/source/gainput/gestures/GainputRotateGesture.cpp
    lib/source/gainput/gestures/GainputSimultaneouslyDownGesture.cpp
    lib/source/gainput/gestures/GainputTapGesture.cpp
    lib/source/gainput/hid/GainputHIDWhitelist.cpp
    lib/source/gainput/hid/hidparsers/HIDParserPS4Controller.cpp
    lib/source/gainput/hid/hidparsers/HIDParserPS5Controller.cpp
    lib/source/gainput/hid/hidparsers/HIDParserSwitchController.cpp
    lib/source/gainput/keyboard/GainputInputDeviceKeyboard.cpp
    lib/source/gainput/mouse/GainputInputDeviceMouse.cpp
    lib/source/gainput/pad/GainputInputDevicePad.cpp
    lib/source/gainput/recorder/GainputInputPlayer.cpp
    lib/source/gainput/recorder/GainputInputRecorder.cpp
    lib/source/gainput/recorder/GainputInputRecording.cpp
    lib/source/gainput/touch/GainputInputDeviceTouch.cpp
)

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UDEV REQUIRED libudev)

    list(APPEND GAINPUT_SRC lib/source/gainput/hid/GainputHID.cpp lib/source/hidapi/linux/hid.c)
endif()

if(WIN32)
    find_library(XINPUT_LIBRARIES Xinput REQUIRED)

    list(APPEND GAINPUT_SRC lib/source/gainput/hid/GainputHID.cpp lib/source/hidapi/windows/hid.c)
endif()

if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(GAME_CONTROLLER_FRAMEWORK GameController)

    list(
        APPEND
        GAINPUT_SRC
        lib/source/gainput/apple/GainputInputDevicePadMac.cpp
        lib/source/gainput/apple/GainputInputDeviceMouseMac.mm
        lib/source/gainput/apple/GainputInputDevicePadAppleGCKit.mm
        lib/source/gainput/apple/GainputInputDeviceKeyboardMac.cpp
        lib/source/gainput/apple/GainputInputDeviceMouseMacRaw.mm
    )

    if(IOS)
        find_library(COREHAPTICS_FRAMEWORK CoreHaptics)

        list(APPEND GAINPUT_SRC lib/source/gainput/apple/GainputIos.mm lib/source/gainput/apple/GainputGCKit.mm)
    else()
        list(APPEND GAINPUT_SRC lib/source/gainput/hid/GainputHID.cpp lib/source/gainput/apple/GainputMac.mm lib/source/hidapi/mac/hid.c)
    endif()
    set(CXX_AS_OBJCXX_SRC
        # cmake-format: sortable
        lib/source/gainput/GainputInputManager.cpp lib/source/gainput/mouse/GainputInputDeviceMouse.cpp
        lib/source/gainput/pad/GainputInputDevicePad.cpp lib/source/gainput/touch/GainputInputDeviceTouch.cpp
    )
    set_source_files_properties(${CXX_AS_OBJCXX_SRC} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

if(ANDROID)
    find_library(ANDROID_LIBRARY android)

    list(APPEND GAINPUT_SRC lib/source/gainput/android/GainputInputDeviceGestureDetectorAndroid.cpp)
endif()

add_library(gainput STATIC ${GAINPUT_SRC})
target_link_libraries(
    gainput
    PUBLIC $<$<PLATFORM_ID:Linux>:${UDEV_LIBRARIES}> $<$<PLATFORM_ID:Windows>:${XINPUT_LIBRARIES}>
           $<$<PLATFORM_ID:Darwin>:${IOKIT_FRAMEWORK}> $<$<OR:$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:iOS>>:${GAME_CONTROLLER_FRAMEWORK}>
           $<$<PLATFORM_ID:iOS>:${COREHAPTICS_FRAMEWORK}> $<$<PLATFORM_ID:Android>:${ANDROID_LIBRARY}>
)
target_include_directories(
    gainput PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/source/ $<$<PLATFORM_ID:Android>:${ANDROID_NDK}/sources/android/native_app_glue/>
                   $<$<PLATFORM_ID:Android>:${CMAKE_CURRENT_SOURCE_DIR}/lib/include/>
)
