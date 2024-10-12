/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This is the main interface to the Android Game Controller library, also known
   as Paddleboat.

   See the documentation at
   https://developer.android.com/games/sdk/game-controller for more information
   on using this library in a native Android game.
 */

/**
 * @defgroup paddleboat Game Controller main interface
 * The main interface to use the Game Controller library.
 * @{
 */

#ifndef PADDLEBOAT_H
#define PADDLEBOAT_H

#include <android/input.h>
#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/gamesdk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PADDLEBOAT_MAJOR_VERSION 2
#define PADDLEBOAT_MINOR_VERSION 0
#define PADDLEBOAT_BUGFIX_VERSION 0
#define PADDLEBOAT_PACKED_VERSION                            \
    ANDROID_GAMESDK_PACKED_VERSION(PADDLEBOAT_MAJOR_VERSION, \
                                   PADDLEBOAT_MINOR_VERSION, \
                                   PADDLEBOAT_BUGFIX_VERSION)

/**
 * @brief Maximum number of simultaneously connected controllers.
 */
#define PADDLEBOAT_MAX_CONTROLLERS 8

/**
 * @brief The maximum number of characters, including the terminating
 * character, allowed in a string table entry
 */
#define PADDLEBOAT_STRING_TABLE_ENTRY_MAX_SIZE 64

/**
 * @brief The expected value in the `fileIdentifier` field of the
 * `Paddleboat_Controller_Mapping_File_Header` for a valid
 * mapping file.
 */
#define PADDLEBOAT_MAPPING_FILE_IDENTIFIER 0xadd1eb0a

/**
 * @brief Paddleboat error code results.
 */
enum Paddleboat_ErrorCode : int32_t {
    /**
     * @brief No error. Function call was successful.
     */
    PADDLEBOAT_NO_ERROR = 0,
    /**
     * @brief ::Paddleboat_init was called a second time without a call to
     * ::Paddleboat_destroy in between.
     */
    PADDLEBOAT_ERROR_ALREADY_INITIALIZED = -2000,
    /**
     * @brief Paddleboat was not successfully initialized. Either
     * ::Paddleboat_init was not called or returned an error.
     */
    PADDLEBOAT_ERROR_NOT_INITIALIZED = -2001,
    /**
     * @brief Paddleboat could not be successfully initialized. Instantiation
     * of the GameControllerManager class failed.
     */
    PADDLEBOAT_ERROR_INIT_GCM_FAILURE = -2002,
    /**
     * @brief Invalid controller index specified. Valid index range is from 0
     * to PADDLEBOAT_MAX_CONTROLLERS - 1
     */
    PADDLEBOAT_ERROR_INVALID_CONTROLLER_INDEX = -2003,
    /**
     * @brief No controller is connected at the specified controller index.
     */
    PADDLEBOAT_ERROR_NO_CONTROLLER = -2004,
    /**
     * @brief No virtual or physical mouse device is connected.
     */
    PADDLEBOAT_ERROR_NO_MOUSE = -2005,
    /**
     * @brief The feature is not supported by the specified controller.
     * Example: Calling ::Paddleboat_setControllerVibrationData on a controller
     * that does not have the `PADDLEBOAT_CONTROLLER_FLAG_VIBRATION` bit
     * set in `Paddleboat_Controller_Info.controllerFlags`.
     */
    PADDLEBOAT_ERROR_FEATURE_NOT_SUPPORTED = -2006,
    /**
     * @brief An invalid parameter was specified. This usually means NULL or
     * nullptr was passed in a parameter that requires a valid pointer.
     */
    PADDLEBOAT_ERROR_INVALID_PARAMETER = -2007,
    /**
     * @brief Invalid controller mapping data was provided. The data in the
     * provided buffer does not match the expected mapping data format.
     */
    PADDLEBOAT_INVALID_MAPPING_DATA = -2008,
    /**
     * @brief Incompatible controller mapping data was provided. The data in
     * the provided buffer is from an incompatible version of the mapping data format.
     */
    PADDLEBOAT_INCOMPATIBLE_MAPPING_DATA = -2009,
    /**
     * @brief A file I/O error occurred when trying to read mapping data from the
     * file descriptor passed to ::Paddleboat_addControllerRemapDataFromFd
     */
    PADDLEBOAT_ERROR_FILE_IO = -2010
};

/**
 * @brief Paddleboat controller buttons defined as bitmask values.
 * AND against `Paddleboat_Controller_Data.buttonsDown` to check for button
 * status.
 */
enum Paddleboat_Buttons : uint32_t {
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` direction pad
     * up button.
     */
    PADDLEBOAT_BUTTON_DPAD_UP = (1U << 0),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` direction pad
     * left button.
     */
    PADDLEBOAT_BUTTON_DPAD_LEFT = (1U << 1),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` direction pad
     * down button.
     */
    PADDLEBOAT_BUTTON_DPAD_DOWN = (1U << 2),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` direction pad
     * right button.
     */
    PADDLEBOAT_BUTTON_DPAD_RIGHT = (1U << 3),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` A button.
     */
    PADDLEBOAT_BUTTON_A = (1U << 4),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` B button.
     */
    PADDLEBOAT_BUTTON_B = (1U << 5),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` X button.
     */
    PADDLEBOAT_BUTTON_X = (1U << 6),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` Y button.
     */
    PADDLEBOAT_BUTTON_Y = (1U << 7),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` L1 trigger
     * button.
     */
    PADDLEBOAT_BUTTON_L1 = (1U << 8),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` L2 trigger
     * button.
     */
    PADDLEBOAT_BUTTON_L2 = (1U << 9),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` L3 thumbstick
     * button.
     */
    PADDLEBOAT_BUTTON_L3 = (1U << 10),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` R1 trigger
     * button.
     */
    PADDLEBOAT_BUTTON_R1 = (1U << 11),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` R2 trigger
     * button.
     */
    PADDLEBOAT_BUTTON_R2 = (1U << 12),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` R3 thumbstick
     * button.
     */
    PADDLEBOAT_BUTTON_R3 = (1U << 13),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` Select
     * button.
     */
    PADDLEBOAT_BUTTON_SELECT = (1U << 14),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` Start button.
     */
    PADDLEBOAT_BUTTON_START = (1U << 15),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` System
     * button.
     */
    PADDLEBOAT_BUTTON_SYSTEM = (1U << 16),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` Touchpad
     * pressed.
     */
    PADDLEBOAT_BUTTON_TOUCHPAD = (1U << 17),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` AUX1 button.
     */
    PADDLEBOAT_BUTTON_AUX1 = (1U << 18),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` AUX2 button.
     */
    PADDLEBOAT_BUTTON_AUX2 = (1U << 19),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` AUX3 button.
     */
    PADDLEBOAT_BUTTON_AUX3 = (1U << 20),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Data.buttonsDown` AUX4 button.
     */
    PADDLEBOAT_BUTTON_AUX4 = (1U << 21),
    /**
     * @brief Count of defined controller buttons.
     */
    PADDLEBOAT_BUTTON_COUNT = 22
};

/**
 * @brief Paddleboat controller device feature flags as bitmask values
 * AND against `Paddleboat_Controller_Info.controllerFlags` to determine feature
 * availability.
 */
enum Paddleboat_Controller_Flags : uint32_t {
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device wasn't found in the internal
     * controller database and a generic button and axis mapping
     * profile is being used
     */
    PADDLEBOAT_CONTROLLER_FLAG_GENERIC_PROFILE = (0x0000000010),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports reporting accelerometer
     * motion axis data
     */
    PADDLEBOAT_CONTROLLER_FLAG_ACCELEROMETER = (0x00400000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports reporting gyroscope
     * motion axis data
     */
    PADDLEBOAT_CONTROLLER_FLAG_GYROSCOPE = (0x00800000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports setting a player number
     * light using the ::Paddleboat_setControllerLight function
     * with the `PADDLEBOAT_LIGHT_PLAYER_NUMBER` light type
     */
    PADDLEBOAT_CONTROLLER_FLAG_LIGHT_PLAYER = (0x01000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports setting a RGB light
     * color using the ::Paddleboat_setControllerLight function
     * with the `PADDLEBOAT_LIGHT_RGB` light type
     */
    PADDLEBOAT_CONTROLLER_FLAG_LIGHT_RGB = (0x02000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports reporting the battery
     * status information in the controller data structure
     */
    PADDLEBOAT_CONTROLLER_FLAG_BATTERY = (0x04000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports vibration effects
     * using the ::Paddleboat_setControllerVibrationData function
     */
    PADDLEBOAT_CONTROLLER_FLAG_VIBRATION = (0x08000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device supports both left and right
     * vibration motor data for vibration effects
     */
    PADDLEBOAT_CONTROLLER_FLAG_VIBRATION_DUAL_MOTOR = (0x10000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device has a touchpad that will register
     * the `PADDLEBOAT_BUTTON_TOUCHPAD` button when pressed
     */
    PADDLEBOAT_CONTROLLER_FLAG_TOUCHPAD = (0x20000000),
    /**
     * @brief Bitmask for `Paddleboat_Controller_Info.controllerFlags`
     * If set, this controller device can simulate a virtual mouse and
     * will report coordinates in `Paddleboat_Controller_Data.virtualPointer`
     */
    PADDLEBOAT_CONTROLLER_FLAG_VIRTUAL_MOUSE = (0x40000000)
};

/**
 * @brief Bitmask values to use with ::Paddleboat_getIntegratedMotionSensorFlags
 * and ::Paddleboat_setMotionDataCallbackWithIntegratedFlags
 * Bitmask values represent integrated sensor types on the main device instead
 * of a controller device.
 */
enum Paddleboat_Integrated_Motion_Sensor_Flags : uint32_t {
    /**
     * @brief Bitmask for ::Paddleboat_getIntegratedMotionSensorFlags
     * No present integrated motion sensors.
     */
    PADDLEBOAT_INTEGRATED_SENSOR_NONE = 0,
    /**
     * @brief Bitmask for ::Paddleboat_getIntegratedMotionSensorFlags
     * If set, the main device supports reporting accelerometer
     * motion axis data
     */
    PADDLEBOAT_INTEGRATED_SENSOR_ACCELEROMETER = (0x00000001),
    /**
     * @brief Bitmask for ::Paddleboat_getIntegratedMotionSensorFlags
     * If set, the main device supports reporting gyroscope
     * motion axis data
     */
    PADDLEBOAT_INTEGRATED_SENSOR_GYROSCOPE = (0x00000002),
};

/**
 * @brief Bitmask flags to use with ::Paddleboat_getIntegratedMotionSensorFlags
 * and ::Paddleboat_setMotionDataCallbackWithIntegratedFlags
 * Flag values represent integrated sensor types on the main device instead
 * of a controller device.
 */
 enum Paddleboat_Motion_Data_Callback_Sensor_Index : uint32_t {
    /**
     * @brief Value passed in the `controllerIndex` parameter of the
     * `Paddleboat_MotionDataCallback` if integrated sensor data
     * reporting is active and the motion data event came from
     * an integrated sensor, rather than a controller sensor.
     * This value will only be passed to the motion data callback
     * if integrated sensor data has been requested using
     * ::Paddleboat_setMotionDataCallbackWithIntegratedFlags
     */
    PADDLEBOAT_INTEGRATED_SENSOR_INDEX = (0x40000000)
};

/**
 * @brief Paddleboat mouse buttons as bitmask values
 * AND against `Paddleboat_Mouse_Data.buttonsDown` to determine button status.
 */
enum Paddleboat_Mouse_Buttons : uint32_t {
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` left mouse button.
     */
    PADDLEBOAT_MOUSE_BUTTON_LEFT = (1U << 0),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` right mouse
     * button.
     */
    PADDLEBOAT_MOUSE_BUTTON_RIGHT = (1U << 1),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` middle mouse
     * button.
     */
    PADDLEBOAT_MOUSE_BUTTON_MIDDLE = (1U << 2),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` back mouse button.
     */
    PADDLEBOAT_MOUSE_BUTTON_BACK = (1U << 3),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` forward mouse
     * button.
     */
    PADDLEBOAT_MOUSE_BUTTON_FORWARD = (1U << 4),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` mouse button 6.
     */
    PADDLEBOAT_MOUSE_BUTTON_6 = (1U << 5),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` mouse button 7.
     */
    PADDLEBOAT_MOUSE_BUTTON_7 = (1U << 6),
    /**
     * @brief Bitmask for `Paddleboat_Mouse_Data.buttonsDown` mouse button 8.
     */
    PADDLEBOAT_MOUSE_BUTTON_8 = (1U << 7)
};

//* Axis order: LStickX, LStickY, RStickX, RStickY, L1, L2, R1, R2, HatX, HatY\n

/**
 * @brief Paddleboat axis mapping table axis order.
 */
enum Paddleboat_Mapping_Axis : uint32_t {
    /**
     * @brief Paddleboat internal mapping index for left thumbstick X axis. */
    PADDLEBOAT_MAPPING_AXIS_LEFTSTICK_X = 0,
    /**
     * @brief Paddleboat internal mapping index for left thumbstick Y axis. */
    PADDLEBOAT_MAPPING_AXIS_LEFTSTICK_Y,
    /**
     * @brief Paddleboat internal mapping index for right thumbstick X axis. */
    PADDLEBOAT_MAPPING_AXIS_RIGHTSTICK_X,
    /**
     * @brief Paddleboat internal mapping index for right thumbstick Y axis. */
    PADDLEBOAT_MAPPING_AXIS_RIGHTSTICK_Y,
    /**
     * @brief Paddleboat internal mapping index for L1 trigger axis. */
    PADDLEBOAT_MAPPING_AXIS_L1,
    /**
     * @brief Paddleboat internal mapping index for L2 trigger axis. */
    PADDLEBOAT_MAPPING_AXIS_L2,
    /**
     * @brief Paddleboat internal mapping index for R1 trigger axis. */
    PADDLEBOAT_MAPPING_AXIS_R1,
    /**
     * @brief Paddleboat internal mapping index for R2 trigger axis. */
    PADDLEBOAT_MAPPING_AXIS_R2,
    /**
     * @brief Paddleboat internal mapping index for HatX trigger axis.
     * This is usually the dpad left/right. */
    PADDLEBOAT_MAPPING_AXIS_HATX,
    /**
     * @brief Paddleboat internal mapping index for HatY trigger axis.
     * This is usually the dpad up/down. */
    PADDLEBOAT_MAPPING_AXIS_HATY,
    /**
     * @brief Number of axis used for controller mapping configuration. */
    PADDLEBOAT_MAPPING_AXIS_COUNT = 10
};

/**
 * @brief Special constants to specify an axis or axis button mapping is ignored
 * by the controller.
 */
enum Paddleboat_Ignored_Axis : uint32_t {
    /**
     * @brief Constant that signifies an axis in the
     * `Paddleboat_Controller_Mapping_Data.axisPositiveButtonMapping` array
     * and/or `Paddleboat_Controller_Mapping_Data.axisNegativeButtonMapping`
     * array does not have a mapping to a button
     */
    PADDLEBOAT_AXIS_BUTTON_IGNORED = 0xFE,
    /**
     * @brief Constant that signifies an axis in the
     * `Paddleboat_Controller_Mapping_Data.axisMapping` array is unused by
     * the controller
     */
    PADDLEBOAT_AXIS_IGNORED = 0xFFFE
};

/**
 * @brief Special constant to specify a button is ignored by the controller.
 */
enum Paddleboat_Ignored_Buttons : uint32_t {
    /**
     * @brief Constant that signifies a button in the
     * `Paddleboat_Controller_Mapping_Data.buttonMapping` array
     * does not map to a button on the controller
     */
    PADDLEBOAT_BUTTON_IGNORED = 0xFFFE
};

/**
 * @brief Battery status of a controller
 */
enum Paddleboat_BatteryStatus : uint32_t {
    PADDLEBOAT_CONTROLLER_BATTERY_UNKNOWN = 0,  ///< Battery status is unknown
    PADDLEBOAT_CONTROLLER_BATTERY_CHARGING =
        1,  ///< Controller battery is charging
    PADDLEBOAT_CONTROLLER_BATTERY_DISCHARGING =
        2,  ///< Controller battery is discharging
    PADDLEBOAT_CONTROLLER_BATTERY_NOT_CHARGING =
        3,  ///< Controller battery is not charging
    PADDLEBOAT_CONTROLLER_BATTERY_FULL =
        4  ///< Controller battery is completely charged
};

/**
 * @brief Current status of a controller (at a specified controller index)
 */
enum Paddleboat_ControllerStatus : uint32_t {
    PADDLEBOAT_CONTROLLER_INACTIVE = 0,  ///< No controller is connected
    PADDLEBOAT_CONTROLLER_ACTIVE = 1,    ///< Controller is connected and active
    PADDLEBOAT_CONTROLLER_JUST_CONNECTED =
        2,  ///< Controller has just connected,
            ///< only seen in a controller status callback
    PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED =
        3  ///< Controller has just disconnected,
           ///< only seen in a controller status callback
};

/**
 * @brief The button layout and iconography of the controller buttons
 */
enum Paddleboat_ControllerButtonLayout : uint32_t {
    //!  Y \n
    //! X B\n
    //!  A 
    PADDLEBOAT_CONTROLLER_LAYOUT_STANDARD = 0,
    //!  △ \n
    //! □ ○\n
    //!  x \n
    //! x = A, ○ = B, □ = X, △ = Y
    PADDLEBOAT_CONTROLLER_LAYOUT_SHAPES = 1,
    //!  X \n
    //! Y A\n
    //!  B 
    PADDLEBOAT_CONTROLLER_LAYOUT_REVERSE = 2,
    //! X Y R1 L1\n
    //! A B R2 L2
    PADDLEBOAT_CONTROLLER_LAYOUT_ARCADE_STICK = 3,
    //! Mask value, AND with
    //! `Paddleboat_Controller_Info.controllerFlags`
    //! to get the `Paddleboat_ControllerButtonLayout` value
    PADDLEBOAT_CONTROLLER_LAYOUT_MASK = 3
};

/**
 * @brief The type of light being specified by a call to
 * ::Paddleboat_setControllerLight
 */
enum Paddleboat_LightType : uint32_t {
    PADDLEBOAT_LIGHT_PLAYER_NUMBER = 0,  ///< Light is a player index,
                                         ///< `lightData` is the player number
    PADDLEBOAT_LIGHT_RGB = 1             ///< Light is a color light,
                              ///< `lightData` is a ARGB (8888) light value.
};

/**
 * @brief The type of motion data being reported in a Paddleboat_Motion_Data
 * structure
 */
enum Paddleboat_Motion_Type : uint32_t {
    PADDLEBOAT_MOTION_ACCELEROMETER = 0,  ///< Accelerometer motion data
    PADDLEBOAT_MOTION_GYROSCOPE = 1       ///< Gyroscope motion data
};

/**
 * @brief The status of the mouse device
 */
enum Paddleboat_MouseStatus : uint32_t {
    PADDLEBOAT_MOUSE_NONE = 0,  ///< No mouse device is connected
    PADDLEBOAT_MOUSE_CONTROLLER_EMULATED =
        1,                     ///< A virtual mouse is connected
                               ///< The virtual mouse is being simulated
                               ///< by a game controller
    PADDLEBOAT_MOUSE_PHYSICAL  ///< A physical mouse or trackpad device is
                               ///< connected
};

/**
 * @brief The addition mode to use when passing new controller mapping data
 * to ::Paddleboat_addControllerRemapData
 */
enum Paddleboat_Remap_Addition_Mode : uint32_t {
    PADDLEBOAT_REMAP_ADD_MODE_DEFAULT =
        0,  ///< If a vendorId/productId controller entry in the
            ///< new remap table exists in the current database,
            ///< and the min/max API ranges overlap, replace the
            ///< existing entry, otherwise add a new entry.
            ///< Always adds a new entry if the vendorId/productId
            ///< does not exist in the current database. These
            ///< changes only persist for the current session.
    PADDLEBOAT_REMAP_ADD_MODE_REPLACE_ALL  ///< The current controller database
                                           ///< will be erased and entirely
                                           ///< replaced by the new remap table.
                                           ///< This change only persists for
                                           ///< the current session.
};

/**
 * @brief A structure that describes the current battery state of a controller.
 * This structure will only be populated if a controller has
 * `PADDLEBOAT_CONTROLLER_FLAG_BATTERY` set in
 * `Paddleboat_Controller_Info.controllerFlags`
 */
typedef struct Paddleboat_Controller_Battery {
    Paddleboat_BatteryStatus
        batteryStatus;  /** @brief The current status of the battery */
    float batteryLevel; /** @brief The current charge level of the battery, from
                           0.0 to 1.0 */
} Paddleboat_Controller_Battery;

/**
 * @brief A structure that contains virtual pointer position data.
 * X and Y coordinates are pixel based and range from 0,0 to window
 * width,height.
 */
typedef struct Paddleboat_Controller_Pointer {
    /** @brief X pointer position in window space pixel coordinates */
    float pointerX;
    /** @brief Y pointer position in window space pixel coordinates */
    float pointerY;
} Paddleboat_Controller_Pointer;

/**
 * @brief A structure that contains X and Y axis data for an analog thumbstick.
 * Axis ranges from -1.0 to 1.0.
 */
typedef struct Paddleboat_Controller_Thumbstick {
    /** @brief X axis data for the thumbstick */
    float stickX;
    /** @brief X axis data for the thumbstick */
    float stickY;
} Paddleboat_Controller_Thumbstick;

/**
 * @brief A structure that contains axis precision data for a thumbstick in the
 * X and Y axis. Value ranges from 0.0 to 1.0. Flat is the extent of a center
 * flat (deadzone) area of a thumbstick axis Fuzz is the error tolerance
 * (deviation) of a thumbstick axis
 */
typedef struct Paddleboat_Controller_Thumbstick_Precision {
    /** @brief X axis flat value for the thumbstick */
    float stickFlatX;
    /** @brief Y axis flat value for the thumbstick */
    float stickFlatY;
    /** @brief X axis fuzz value for the thumbstick */
    float stickFuzzX;
    /** @brief Y axis fuzz value for the thumbstick */
    float stickFuzzY;
} Paddleboat_Controller_Thumbstick_Precision;

/**
 * @brief A structure that contains the current data
 * for a controller's inputs and sensors.
 */
typedef struct Paddleboat_Controller_Data {
    /** @brief Timestamp of most recent controller data update, timestamp is
     * microseconds elapsed since clock epoch. */
    uint64_t timestamp;
    /** @brief Bit-per-button bitfield array */
    uint32_t buttonsDown;
    /** @brief Left analog thumbstick axis data */
    Paddleboat_Controller_Thumbstick leftStick;
    /** @brief Right analog thumbstick axis data */
    Paddleboat_Controller_Thumbstick rightStick;
    /** @brief L1 trigger axis data. Axis range is 0.0 to 1.0. */
    float triggerL1;
    /** @brief L2 trigger axis data. Axis range is 0.0 to 1.0. */
    float triggerL2;
    /** @brief R1 trigger axis data. Axis range is 0.0 to 1.0. */
    float triggerR1;
    /** @brief R2 trigger axis data. Axis range is 0.0 to 1.0. */
    float triggerR2;
    /**
     * @brief Virtual pointer pixel coordinates in window space.
     * If `Paddleboat_Controller_Info.controllerFlags` has the
     * `PADDLEBOAT_CONTROLLER_FLAG_VIRTUAL_MOUSE` bit set, pointer coordinates
     * are valid. If this bit is not set, pointer coordinates will always be
     * 0,0.
     */
    Paddleboat_Controller_Pointer virtualPointer;
    /**
     * @brief Battery status. This structure will only be populated if the
     * controller has `PADDLEBOAT_CONTROLLER_FLAG_BATTERY` set in
     * `Paddleboat_Controller_Info.controllerFlags`
     */
    Paddleboat_Controller_Battery battery;
} Paddleboat_Controller_Data;

/**
 * @brief A structure that contains information
 * about a particular controller device. Several fields
 * are populated by the value of the corresponding fields from InputDevice.
 */
typedef struct Paddleboat_Controller_Info {
    /** @brief Controller feature flag bits */
    uint32_t controllerFlags;
    /** @brief Controller number, maps to InputDevice.getControllerNumber() */
    int32_t controllerNumber;
    /** @brief Vendor ID, maps to InputDevice.getVendorId() */
    int32_t vendorId;
    /** @brief Product ID, maps to InputDevice.getProductId() */
    int32_t productId;
    /** @brief Device ID, maps to InputDevice.getId() */
    int32_t deviceId;
    /** @brief the flat and fuzz precision values of the left thumbstick */
    Paddleboat_Controller_Thumbstick_Precision leftStickPrecision;
    /** @brief the flat and fuzz precision values of the right thumbstick */
    Paddleboat_Controller_Thumbstick_Precision rightStickPrecision;
} Paddleboat_Controller_Info;

/**
 * @brief A structure that contains motion data reported by a controller.
 */
typedef struct Paddleboat_Motion_Data {
    /** @brief Timestamp of when the motion data event occurred, timestamp is
     * nanoseconds elapsed since clock epoch. */
    uint64_t timestamp;
    /** @brief The type of motion event data */
    Paddleboat_Motion_Type motionType;
    /** @brief Motion X axis data. */
    float motionX;
    /** @brief Motion Y axis data. */
    float motionY;
    /** @brief Motion Z axis data. */
    float motionZ;
} Paddleboat_Motion_Data;

/**
 * @brief A structure that contains input data for the mouse device.
 */
typedef struct Paddleboat_Mouse_Data {
    /** @brief Timestamp of most recent mouse data update, timestamp is
     * microseconds elapsed since clock epoch. */
    uint64_t timestamp;
    /** @brief Bit-per-button bitfield array of mouse button status. */
    uint32_t buttonsDown;
    /** @brief Number of horizontal mouse wheel movements since previous
     * read of mouse data. Can be positive or negative depending
     * on direction of scrolling.
     */
    int32_t mouseScrollDeltaH;
    /** @brief Number of vertical mouse wheel movements since previous
     * read of mouse data. Can be positive or negative depending
     * on direction of scrolling.
     */
    int32_t mouseScrollDeltaV;
    /** @brief Current mouse X coordinates in window space pixels. */
    float mouseX;
    /** @brief Current mouse Y coordinates in window space pixels. */
    float mouseY;
} Paddleboat_Mouse_Data;

/**
 * @brief A structure that describes the parameters of a vibration effect.
 */
typedef struct Paddleboat_Vibration_Data {
    /** @brief Duration to vibrate the left motor in milliseconds. */
    int32_t durationLeft;
    /** @brief Duration to vibrate the right motor in milliseconds. */
    int32_t durationRight;
    /** @brief Intensity of vibration of left motor, valid range is 0.0 to 1.0.
     */
    float intensityLeft;
    /** @brief Intensity of vibration of right motor, valid range is 0.0 to 1.0.
     */
    float intensityRight;
} Paddleboat_Vibration_Data;

/**
 * @brief A structure that describes the button and axis mappings
 * for a specified controller device running on a specified range of Android API
 * levels.\n See `Paddleboat_Mapping_Axis` for axis order. Hat axis should be
 * mapped to dpad buttons.
 * @deprecated Use the `Paddleboat_Controller_Mapping_File_Header` in combination
 * with the ::Paddleboat_addControllerRemapDataFromFileBuffer function instead.
 */
typedef struct Paddleboat_Controller_Mapping_Data {
    /** @brief Minimum API level required for this entry */
    int16_t minimumEffectiveApiLevel;
    /** @brief Maximum API level required for this entry, 0 = no max */
    int16_t maximumEffectiveApiLevel;
    /** @brief VendorID of the controller device for this entry */
    int32_t vendorId;
    /** @brief ProductID of the controller device for this entry */
    int32_t productId;
    /** @brief Flag bits, will be ORed with
     * `Paddleboat_Controller_Info.controllerFlags`
     */
    int32_t flags;
    /** @brief AMOTION_EVENT_AXIS value for
     * the corresponding Paddleboat control axis, or PADDLEBOAT_AXIS_IGNORED if
     * unsupported.
     */
    uint16_t axisMapping[PADDLEBOAT_MAPPING_AXIS_COUNT];
    /** @brief Button to set on
     * positive axis value, PADDLEBOAT_AXIS_BUTTON_IGNORED if none.
     */
    uint8_t axisPositiveButtonMapping[PADDLEBOAT_MAPPING_AXIS_COUNT];
    /** @brief Button to set on
     * negative axis value, PADDLEBOAT_AXIS_BUTTON_IGNORED if none.
     */
    uint8_t axisNegativeButtonMapping[PADDLEBOAT_MAPPING_AXIS_COUNT];
    /** @brief AKEYCODE_ value corresponding
     * with the corresponding Paddleboat button.
     * PADDLEBOAT_BUTTON_IGNORED if unsupported.
     */
    uint16_t buttonMapping[PADDLEBOAT_BUTTON_COUNT];
} Paddleboat_Controller_Mapping_Data;

/**
 * @brief Signature of a function that can be passed to
 * ::Paddleboat_setControllerStatusCallback to receive information about
 controller
 * connections and disconnections.

 * @param controllerIndex Index of the controller that has registered a status
 change,
 * will range from 0 to PADDLEBOAT_MAX_CONTROLLERS - 1.
 * @param controllerStatus New status of the controller.
 * @param userData The value of the userData parameter passed
 * to ::Paddleboat_setControllerStatusCallback
 *
 * Function will be called on the same thread that calls ::Paddleboat_update.
 */
typedef void (*Paddleboat_ControllerStatusCallback)(
    const int32_t controllerIndex,
    const Paddleboat_ControllerStatus controllerStatus, void *userData);

/**
 * @brief Signature of a function that can be passed to
 * ::Paddleboat_setMouseStatusCallback to receive information about mouse
 * device status changes.
 * @param mouseStatus Current status of the mouse.
 * @param userData The value of the userData parameter passed
 * to ::Paddleboat_setMouseStatusCallback
 *
 * Function will be called on the same thread that calls ::Paddleboat_update.
 */
typedef void (*Paddleboat_MouseStatusCallback)(
    const Paddleboat_MouseStatus mouseStatus, void *userData);

/**
 * @brief Signature of a function that can be passed to
 * ::Paddleboat_setMotionDataCallback to receive information about motion data
 events
 * sent by connected controllers

 * @param controllerIndex Index of the controller reporting the motion event,
 * will range from 0 to PADDLEBOAT_MAX_CONTROLLERS - 1. If integrated motion
 * sensor reporting was enabled, this value will equal
 * `PADDLEBOAT_INTEGRATED_SENSOR_INDEX` if the motion event came from
 * the integrated sensors on the main device, instead of a controller
 * @param motionData The motion data. Pointer is only valid until the callback
 returns.
 * @param userData The value of the userData parameter passed
 * to ::Paddleboat_setMotionDataCallback
 *
 */
typedef void (*Paddleboat_MotionDataCallback)(
    const int32_t controllerIndex, const Paddleboat_Motion_Data *motionData,
    void *userData);

/**
 * @brief Signature of a function that can be passed to
 * ::Paddleboat_setPhysicalKeyboardStatusCallback to receive information about
 * physical keyboard connection status changes.
 * @param physicalKeyboardStatus Whether a physical keyboard is currently connected.
 * @param userData The value of the userData parameter passed
 * to ::Paddleboat_setPhysicalKeyboardStatusCallback
 *
 * Function will be called on the same thread that calls ::Paddleboat_update.
 */
typedef void (*Paddleboat_PhysicalKeyboardStatusCallback)(
        const bool physicalKeyboardStatus, void *userData);

/**
 * @brief Initialize Paddleboat, constructing internal resources via JNI. This
 * may be called after calling ::Paddleboat_destroy to reinitialize the library.
 * @param env The JNIEnv attached to the thread calling the function.
 * @param jcontext A Context derived object used by the game. This can be an
 * Activity derived class.
 * @return `PADDLEBOAT_NO_ERROR` if successful, otherwise an error code relating
 * to initialization failure.
 * @see Paddleboat_destroy
 */
Paddleboat_ErrorCode Paddleboat_init(JNIEnv *env, jobject jcontext);

/**
 * @brief Check if Paddleboat was successfully initialized.
 * @return false if the initialization failed or was not called.
 */
bool Paddleboat_isInitialized();

/**
 * @brief Destroy resources that Paddleboat has created.
 * @param env The JNIEnv attached to the thread calling the function.
 * @see Paddleboat_init
 */
void Paddleboat_destroy(JNIEnv *env);

/**
 * @brief Inform Paddleboat that a stop event was sent to the application.
 * @param env The JNIEnv attached to the thread calling the function.
 */
void Paddleboat_onStop(JNIEnv *env);

/**
 * @brief Inform Paddleboat that a start event was sent to the application.
 * @param env The JNIEnv attached to the thread calling the function.
 */
void Paddleboat_onStart(JNIEnv *env);

/**
 * @brief Process an input event to see if it is from a device being
 * managed by Paddleboat.
 * @param event the input event received by the application.
 * @return 0 if the event was ignored, 1 if the event was processed/consumed by
 * Paddleboat.
 */
int32_t Paddleboat_processInputEvent(const AInputEvent *event);

/**
 * @brief Process a GameActivityKeyEvent input event to see if it is from a
 * device being managed by Paddleboat. At least once per game frame, the
 * game should iterate through reported GameActivityKeyEvents and pass them to
 * ::Paddleboat_processGameActivityKeyInputEvent for evaluation.
 * @param event the GameActivityKeyEvent input event received by the
 * application.
 * @param eventSize the size of the GameActivityKeyEvent struct being passed in
 * bytes.
 * @return 0 if the event was ignored, 1 if the event was processed/consumed by
 * Paddleboat.
 */
int32_t Paddleboat_processGameActivityKeyInputEvent(const void *event,
                                                    const size_t eventSize);

/**
 * @brief Process a GameActivityMotionEvent input event to see if it is from a
 * device being managed by Paddleboat. At least once per game frame, the
 * game should iterate through reported GameActivityMotionEvents and pass them
 * to
 * ::Paddleboat_processGameActivityMotionInputEvent for evaluation.
 * @param event the GameActivityMotionEvent input event received by the
 * application.
 * @param eventSize the size of the GameActivityMotionEvent struct being passed
 * in bytes.
 * @return 0 if the event was ignored, 1 if the event was processed/consumed by
 * Paddleboat.
 */
int32_t Paddleboat_processGameActivityMotionInputEvent(const void *event,
                                                       const size_t eventSize);

/**
 * @brief Retrieve the active axis ids being used by connected devices. This can
 * be used to determine what axis values to provide to
 * GameActivityPointerInfo_enableAxis when GameActivity is being used.
 * @return A bitmask of the active axis ids that have been used by connected
 * devices during the current application session.
 */
uint64_t Paddleboat_getActiveAxisMask();

/**
 * @brief Get whether Paddleboat consumes AKEYCODE_BACK key events from devices
 * being managed by Paddleboat. The default at initialization is true.
 * @return If true, Paddleboat will consume AKEYCODE_BACK key events, if false
 * it will pass them through.
 */
bool Paddleboat_getBackButtonConsumed();

/**
 * @brief Get availability information for motion data sensors integrated
 * directly on the main device, instead of attached to a controller.
 * @return The bitmask of integrated motion data sensors.
 */

Paddleboat_Integrated_Motion_Sensor_Flags Paddleboat_getIntegratedMotionSensorFlags();

/**
 * @brief Set whether Paddleboat consumes AKEYCODE_BACK key events from devices
 * being managed by Paddleboat. The default at initialization is true. This can
 * be set to false to allow exiting the application from a back button press
 * when the application is in an appropriate state (i.e. the title screen).
 * @param consumeBackButton If true, Paddleboat will consume AKEYCODE_BACK key
 * events, if false it will pass them through.
 */
void Paddleboat_setBackButtonConsumed(bool consumeBackButton);

/**
 * @brief Set a callback to be called whenever a controller managed by
 * Paddleboat changes status. This is used to inform of controller connections
 * and disconnections.
 * @param statusCallback function pointer to the controllers status change
 * callback, passing NULL or nullptr will remove any currently registered
 * callback.
 * @param userData optional pointer (may be NULL or nullptr) to user data that
 * will be passed as a parameter to the status callback. A reference to this
 * pointer will be retained internally until changed by a future call to
 * ::Paddleboat_setControllerStatusCallback
 */
void Paddleboat_setControllerStatusCallback(
    Paddleboat_ControllerStatusCallback statusCallback, void *userData);

/**
 * @brief Set a callback which is called whenever a controller managed by
 * Paddleboat reports a motion data event.
 * @param motionDataCallback function pointer to the motion data callback,
 * passing NULL or nullptr will remove any currently registered callback.
 * @param userData optional pointer (may be NULL or nullptr) to user data
 * that will be passed as a parameter to the status callback. A reference
 * to this pointer will be retained internally until changed by a future
 * call to ::Paddleboat_setMotionDataCallback or
 * ::Paddleboat_setMotionDataCallbackWithIntegratedFlags
 */
void Paddleboat_setMotionDataCallback(
    Paddleboat_MotionDataCallback motionDataCallback, void *userData);

/**
 * @brief Set a callback which is called whenever a controller managed by
 * Paddleboat reports a motion data event.
 * @param motionDataCallback function pointer to the motion data callback,
 * passing NULL or nullptr will remove any currently registered callback.
 * @param integratedSensorFlags specifies if integrated device sensor data
 * will be reported in the motion data callback. If a sensor flag bit is
 * set, and the main device has that sensor, the motion data will be
 * reported in the motion data callback.
 * The ::Paddleboat_getIntegratedMotionSensorFlags function can be used
 * to determine availability of integrated sensors.
 * @param userData optional pointer (may be NULL or nullptr) to user data
 * that will be passed as a parameter to the status callback. A reference
 * to this pointer will be retained internally until changed by a future
 * call to ::Paddleboat_setMotionDataCallback
 * @return `PADDLEBOAT_NO_ERROR` if the callback was successfully registered,
 * otherwise an error code. Attempting to register integrated sensor reporting
 * if the specified sensor is not present will result in a
 * `PADDLEBOAT_ERROR_FEATURE_NOT_SUPPORTED` error code.
 */
Paddleboat_ErrorCode Paddleboat_setMotionDataCallbackWithIntegratedFlags(
    Paddleboat_MotionDataCallback motionDataCallback,
    Paddleboat_Integrated_Motion_Sensor_Flags integratedSensorFlags,
    void *userData);

/**
 * @brief Set a callback to be called when the mouse status changes. This is
 * used to inform of physical or virual mouse device connections and
 * disconnections.
 * @param statusCallback function pointer to the controllers status change
 * callback, passing NULL or nullptr will remove any currently registered
 * callback.
 * @param userData optional pointer (may be NULL or nullptr) to user data that
 * will be passed as a parameter to the status callback. A reference to this
 * pointer will be retained internally until changed by a future call to
 * ::Paddleboat_setMouseStatusCallback
 */
void Paddleboat_setMouseStatusCallback(
    Paddleboat_MouseStatusCallback statusCallback, void *userData);

/**
 * @brief Set a callback to be called when the physical keyboard connection.
 * status changes. This is used to inform of connections or disconnections
 * of a physical keyboard to the primary device.
 * @param statusCallback function pointer to the keyboard status change
 * callback, passing NULL or nullptr will remove any currently registered
 * callback.
 * @param userData optional pointer (may be NULL or nullptr) to user data that
 * will be passed as a parameter to the status callback. A reference to this
 * pointer will be retained internally until changed by a future call to
 * ::Paddleboat_setPhysicalKeyboardStatusCallback
 */
void Paddleboat_setPhysicalKeyboardStatusCallback(
        Paddleboat_PhysicalKeyboardStatusCallback statusCallback,
        void *userData);

/**
 * @brief Retrieve the current controller data from the controller with the
 * specified index.
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1
 * @param[out] controllerData a pointer to the controller data struct to
 * populate.
 * @return `PADDLEBOAT_NO_ERROR` if data was successfully read.
 */
Paddleboat_ErrorCode Paddleboat_getControllerData(
    const int32_t controllerIndex, Paddleboat_Controller_Data *controllerData);

/**
 * @brief Retrieve the current controller device info from the controller with
 * the specified index.
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1
 * @param[out] controllerInfo a pointer to the controller device info struct to
 * populate.
 * @return true if the data was read, false if there was no connected controller
 * at the specified index.
 */
Paddleboat_ErrorCode Paddleboat_getControllerInfo(
    const int32_t controllerIndex, Paddleboat_Controller_Info *controllerInfo);

/**
 * @brief Retrieve the current controller name from the controller with the
 * specified index. This name is retrieved from InputDevice.getName().
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1
 * @param bufferSize The capacity in bytes of the string buffer passed in
 * controllerName
 * @param[out] controllerName A pointer to a buffer that will be populated with
 * the name string. The name string is a C string in UTF-8 format. If the length
 * of the string is greater than bufferSize the string will be truncated to fit.
 * @return `PADDLEBOAT_NO_ERROR` if data was successfully read.
 */
Paddleboat_ErrorCode Paddleboat_getControllerName(const int32_t controllerIndex,
                                                  const size_t bufferSize,
                                                  char *controllerName);
/**
 * @brief Retrieve the current controller device info from the controller with
 * the specified index.
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1.
 * @return Paddleboat_ControllerStatus enum value of the current controller
 * status of the specified controller index.
 */
Paddleboat_ControllerStatus Paddleboat_getControllerStatus(
    const int32_t controllerIndex);

/**
 * @brief Configures a light on the controller with the specified index.
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1
 * @param lightType Specifies the type of light on the controller to configure.
 * @param lightData Light configuration data. For player index lights, this is
 * a number indicating the player index (usually between 1 and 4). For RGB
 * lights, this is a 8888 ARGB value.
 * @param env The JNIEnv attached to the thread calling the function.
 * @return `PADDLEBOAT_NO_ERROR` if successful, otherwise an error code.
 */
Paddleboat_ErrorCode Paddleboat_setControllerLight(
    const int32_t controllerIndex, const Paddleboat_LightType lightType,
    const uint32_t lightData, JNIEnv *env);

/**
 * @brief Set vibration data for the controller with the specified index.
 * @param controllerIndex The index of the controller to read from, must be
 * between 0 and PADDLEBOAT_MAX_CONTROLLERS - 1.
 * @param vibrationData The intensity and duration data for the vibration
 * effect. Valid intensity range is from 0.0 to 1.0. Intensity of 0.0 will turn
 * off vibration if it is active. Duration is specified in milliseconds. The
 * pointer passed in vibrationData is not retained and does not need to persist
 * after function return.
 * @param env The JNIEnv attached to the thread calling the function.
 * @return true if the vibration data was set, false if there was no connected
 * controller or the connected controller does not support vibration.
 */
Paddleboat_ErrorCode Paddleboat_setControllerVibrationData(
    const int32_t controllerIndex,
    const Paddleboat_Vibration_Data *vibrationData, JNIEnv *env);
/**
 * @brief Retrieve the current mouse data.
 * @param[out] mouseData pointer to the mouse data struct to populate.
 * @return true if the data was read, false if there was no connected mouse
 * device.
 */
Paddleboat_ErrorCode Paddleboat_getMouseData(Paddleboat_Mouse_Data *mouseData);

/**
 * @brief Retrieve the current controller device info from the controller with
 * the specified index.
 * @return Paddleboat_MouseStatus enum value of the current mouse status.
 */
Paddleboat_MouseStatus Paddleboat_getMouseStatus();

/**
 * @brief Retrieve the physical keyboard connection status for the device.
 * @return Whether a physical keyboard is currently connected, as boolean.
 */
bool Paddleboat_getPhysicalKeyboardStatus();

/**
 * @brief Add new controller remap information to the internal remapping table.
 * Used to specify custom controller information or override default mapping
 * for a given controller.
 * @param addMode The addition mode for the new data. See the
 * `Paddleboat_Remap_Addition_Mode` enum for details on each mode.
 * @param remapTableEntryCount the number of remap elements in the mappingData
 * array.
 * @param mappingData An array of controller mapping structs to be added to the
 * internal remapping table. The pointer passed in mappingData is not retained
 * and does not need to persist after function return.
 * @deprecated Use ::Paddleboat_addControllerRemapDataFromFd or
 * ::Paddleboat_addControllerRemapDataFromFileBuffer instead.
 */
void Paddleboat_addControllerRemapData(
    const Paddleboat_Remap_Addition_Mode addMode,
    const int32_t remapTableEntryCount,
    const Paddleboat_Controller_Mapping_Data *mappingData);

/**
 * @brief Add new controller remap information to the internal remapping table.
 * Used to specify custom controller information or override default mapping
 * for a given controller. For more information on controller mapping, see the
 * documentation at:
 * https://developer.android.com/games/sdk/game-controller/custom-mapping
 * @param addMode The addition mode for the new data. See the
 * `Paddleboat_Remap_Addition_Mode` enum for details on each mode.
 * @param mappingFileDescriptor A file descriptor returned by a call to `open`.
 * Paddleboat does not call 'close' on the file descriptor before returning, closing
 * the file is the responsibility of the caller.
 * @return `PADDLEBOAT_NO_ERROR` if successful, otherwise an error code.
 */
Paddleboat_ErrorCode Paddleboat_addControllerRemapDataFromFd(
    const Paddleboat_Remap_Addition_Mode addMode,
    const int mappingFileDescriptor);

/**
 * @brief Add new controller remap information to the internal remapping table.
 * Used to specify custom controller information or override default mapping
 * for a given controller. For more information on controller mapping, see the
 * documentation at:
 * https://developer.android.com/games/sdk/game-controller/custom-mapping
 * @param addMode The addition mode for the new data. See the
 * `Paddleboat_Remap_Addition_Mode` enum for details on each mode.
 * @param mappingFileBuffer A pointer to a buffer containing a PaddleboatMappingTool
 * file compatible with this version of the Paddleboat library. The
 * beginning of the file is a `Paddleboat_Controller_Mapping_File_Header`.
 * @param mappingFileBufferSize the size of the file in bytes passed in
 * `mappingFileHeader`.
 * @return `PADDLEBOAT_NO_ERROR` if successful, otherwise an error code.
 */
Paddleboat_ErrorCode Paddleboat_addControllerRemapDataFromFileBuffer(
    const Paddleboat_Remap_Addition_Mode addMode,
    const void *mappingFileBuffer,
    const size_t mappingFileBufferSize);

/**
 * @brief Retrieve the current table of controller remap entries.
 * @param destRemapTableEntryCount the number of
 * `Paddleboat_Controller_Mapping_Data` entries in the array passed in the
 * mappingData parameter. Paddleboat will not copy more than this number of
 * entries out of the internal array to avoid overflowing the buffer.
 * @param[out] mappingData pointer to an array of
 * `Paddleboat_Controller_Mapping_Data` structures. this should contain at least
 * destRemapTableEntryCount elements. Passing nullptr is valid, and can be used
 * to get the number of elements in the internal remap table.
 * @return The number of elements in the internal remap table.
 * @deprecated The number of elements returned will always be zero.
 */
int32_t Paddleboat_getControllerRemapTableData(
    const int32_t destRemapTableEntryCount,
    Paddleboat_Controller_Mapping_Data *mappingData);

/**
 * @brief Updates internal Paddleboat status and processes pending
 * connection/disconnections. Paddleboat_update is responsible for triggering
 * any registered controller or mouse status callbacks, those callbacks will
 * fire on the same thread that called Paddleboat_update. This function should
 * be called once per game frame. It is recommended to call Paddleboat_update
 * before sending any new input events or reading controller inputs for a
 * particular game frame.
 * @param env The JNIEnv attached to the thread calling the function.
 */
void Paddleboat_update(JNIEnv *env);

/**
 * @brief An function that returns the last keycode seen in a key event
 * coming from a controller owned by Paddleboat. Useful for debugging unknown
 * buttons in new devices in the sample app
 * @return keycode from last controller key event.
 */
int32_t Paddleboat_getLastKeycode();

#ifdef __cplusplus
}
#endif

#endif
/** @} */
