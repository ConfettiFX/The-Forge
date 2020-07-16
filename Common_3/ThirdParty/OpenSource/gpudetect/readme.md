# GPU Detect

## Overview
This short sample demonstrates a way to detect the primary graphics hardware present in a system, and to initialize a game's default fidelity presets based on the found graphics device. The code and accompanying data is meant to be used as a guideline, and should be adapted to the game's specific needs. The sample is specific to Microsoft Windows although some of the information will be applicable on other operating systems.

The API can be used to obtain a default quality setting for the device from the data provided in the vendor specific configuration files. Additionally, the Vendor ID and Device ID of the primary graphics device can be queried. The API works by using the DXGI interface. If the Vendor ID is recognized, the code then looks for a configuration file specific to that vendor. In the example, the configuration file is used to list the expected performance levels (Low / Medium / High) of all relevant devices from that vendor. If the device is not recognized the API returns the lowest quality level as a safe fall-back.

This sample is intended for customers developing graphical applications who wish to target Intel(tm) graphics devices.
## File List
*	DeviceId.h -> Header file for device ID code.
*	DeviceId.cpp -> Implementation of functions to convert the device ID into more useful information.
*	GPUDetect.h -> Header file for GPU detection code.
*	GPUDetect.cpp -> Implementation of functions to obtain information about graphics devices.
*	IntelGfx.cfg -> Sample configuration file with list of known Intel GPU devices, their device IDs, and example expected graphics performance levels with regards to the calling game / application.
*	TestMain.cpp -> Simple console based test utility that calls the above functions, and displays the result.

## Configuration File
The configuration file is an example of what could be done using information from GPU Detect. It has one line each for a known GPU device. The Vendor ID is repeated for clarity here. In this example file, each graphics device has been categorized as Low, Medium, or High based on the applications tested performance on each of these parts.
```
;
; Intel Graphics Preset Levels
;
; Format: 
; VendorIDHex, DeviceIDHex, Out of the Box Settings ; Commented name of cards
;
0x8086, 0x1612, Medium; Intel(R) HD Graphics 5600
0x8086, 0x1616, Medium; Intel(R) HD Graphics 5500
0x8086, 0x161E, Low; Intel(R) HD Graphics 5300
0x8086, 0x1622, High; Intel(R) Iris Pro Graphics 6200
```

## Building
This project requires the latest Windows SDK.

## Links
*	[Intel(tm) Graphics Developer's Guides](https://software.intel.com/en-us/articles/intel-hd-graphics-developers-guides) - For more information on developing for Intel(tm) graphics.
*	[Intel(tm) Developer Zone Games & Graphics Forum](https://software.intel.com/en-us/forums/developing-games-and-graphics-on-intel) - Forum for answers on software issues.
*	[Intel(tm) GameDev Developer Zone](https://software.intel.com/en-us/gamedev) - Articles and samples related to developing games
*	[Intel(tm) Graphics Performance Analyzer](https://software.intel.com/en-us/gpa) -Must have tool for any graphics developer.
