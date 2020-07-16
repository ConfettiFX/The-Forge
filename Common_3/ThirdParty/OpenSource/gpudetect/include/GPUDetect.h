////////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
////////////////////////////////////////////////////////////////////////////////


#pragma once


#include <windows.h>
#include <stdint.h>

#include "DeviceId.h"


// forward decls
struct IDXGIAdapter;
struct ID3D11Device;


// Error return codes for GPUDetect
// These codes are set up so that (ERROR_CODE % GENERAL_ERROR_CODE) == 0 if
// ERROR_CODE is a subset of GENERAL_ERROR_CODE.
// e.g. GPUDETECT_ERROR_DXGI_LOAD % GPUDETECT_ERROR_DXGI_LOAD == 0
#define GPUDETECT_ERROR_GENERIC                 1

/// General DXGI Errors
#define GPUDETECT_ERROR_GENERAL_DXGI            2
#define GPUDETECT_ERROR_DXGI_LOAD               GPUDETECT_ERROR_GENERAL_DXGI * 3
#define GPUDETECT_ERROR_DXGI_ADAPTER_CREATION   GPUDETECT_ERROR_GENERAL_DXGI * 5
#define GPUDETECT_ERROR_DXGI_FACTORY_CREATION   GPUDETECT_ERROR_GENERAL_DXGI * 7
#define GPUDETECT_ERROR_DXGI_DEVICE_CREATION    GPUDETECT_ERROR_GENERAL_DXGI * 11
#define GPUDETECT_ERROR_DXGI_GET_ADAPTER_DESC   GPUDETECT_ERROR_GENERAL_DXGI * 13

/// DXGI Counter Errors
#define GPUDETECT_ERROR_GENERAL_DXGI_COUNTER    17
#define GPUDETECT_ERROR_DXGI_BAD_COUNTER        GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 19
#define GPUDETECT_ERROR_DXGI_COUNTER_CREATION   GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 23
#define GPUDETECT_ERROR_DXGI_COUNTER_GET_DATA   GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 29

/// Windows Registry Errors
#define GPUDETECT_ERROR_REG_GENERAL_FAILURE     31
#define GPUDETECT_ERROR_REG_NO_D3D_KEY          GPUDETECT_ERROR_REG_GENERAL_FAILURE * 37 // A DirectX key was not found in the registry in the expected location
#define GPUDETECT_ERROR_REG_MISSING_DRIVER_INFO GPUDETECT_ERROR_REG_GENERAL_FAILURE * 41 // Driver info is missing from the registry

/// Precondition Errors
#define GPUDETECT_ERROR_BAD_DATA                47

#define GPUDETECT_ERROR_NOT_SUPPORTED           55


namespace GPUDetect
{
	enum
	{
		INTEL_VENDOR_ID = 0x8086,
	};

	// Define settings to reflect Fidelity abstraction levels you need
	enum PresetLevel
	{
		NotCompatible,  // Found GPU is not compatible with the app
		Low,
		Medium,
		MediumPlus,
		High,
		Undefined  // No predefined setting found in cfg file.
				   // Use a default level for unknown video cards.
	};

	// The definitions for OS versions
	enum OSVersion
	{
		WIN_95_98_98SE_ME_NT = 4, // Windows 95/98/98SE/Me/NT
		WIN_2000, // Windows 2000
		WIN_2000_XP, // Windows 2000 or XP
		WIN_VISTA, // Windows Vista
		WIN_7, // Windows 7
		WIN_8, // Windows 8
		WIN_8_1, // Windows 8.1

		WIN_10 = 20, // Windows 10
		WIN_10_ANNIVERSARY, // Windows 10 Anniversary Update
		WIN_10_CREATORS, // Windows 10 Creators Update
		WIN_10_FALL_CREATORS, // Windows 10 Fall Creators Update
		WIN_10_APRIL_2018, // Windows 10 April 2018 Update
		WIN_10_OCTOBER_2018, // Windows 10 October 2018 Update
		WIN_10_MAY_2019 // Windows 10 May 2019 Update
	};

	// The definitions for DX versions in the driver number
	enum DXVersion
	{
		DX_6_X = 11,
		DX_7_X,
		DX_8_X,
		DX_9_X,
		DX_10_X,
		DX_11_0 = 17,
		DX_11_1,
		DX_12_0,
		DX_12_1
	};

	// An enum that identifies the generation of Graphics
	enum IntelGraphicsGeneration
	{
		INTEL_GFX_GEN_UNKNOWN = 0,
		INTEL_GFX_GEN6,
		INTEL_GFX_GEN7,
		INTEL_GFX_GEN7_5, // 7.5
		INTEL_GFX_GEN8,
		INTEL_GFX_GEN9,
		INTEL_GFX_GEN9_5, // 9.5
		INTEL_GFX_GEN10,
		INTEL_GFX_GEN11
	};

	struct GPUData
	{
		/////////////////////////
		// DX11 Extension Data //
		/////////////////////////

		/*******************************************************************************
		 * dxAdapterAvailability
		 *
		 *     Is true if Intel driver extension data is populated.
		 *     If this value is false, all other extension data will be null.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		bool dxAdapterAvailability;

		/*******************************************************************************
		 * vendorID
		 *
		 *     The vendorID of the GPU.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		unsigned int vendorID;

		/*******************************************************************************
		 * deviceID
		 *
		 *     The DeviceID of the GPU.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		unsigned int deviceID;

		/*******************************************************************************
		 * adapterLUID
		 *
		 *     The LUID of the d3d adapter.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		LUID adapterLUID;

		/*******************************************************************************
		 * isUMAArchitecture
		 *
		 *     Is true if the GPU uses a uniform memory access architecture.
		 *     On GPUs with a Unified Memory Architecture (UMA) like Intel integrated
		 *     GPUs, the CPU system memory is also used for graphics and there is no
		 *     dedicated VRAM.  Any memory reported as "dedicated" is really a small
		 *     pool of system memory reserved by the BIOS for internal use. All normal
		 *     application allocations (buffers, textures, etc.) are allocated from
		 *     general system "shared" memory.  For this reason, do not use the
		 *     dedicated memory size as an indication of UMA GPU capability (either
		 *     performance, nor memory capacity).
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		bool isUMAArchitecture;

		/*******************************************************************************
		 * videoMemory
		 *
		 *     The amount of Video memory in bytes.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		uint64_t videoMemory;

		/*******************************************************************************
		 * description
		 *
		 *     The driver-provided description of the GPU.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		WCHAR description[ _countof( DXGI_ADAPTER_DESC::Description ) ];

		/*******************************************************************************
		 * extensionVersion
		 *
		 *     Version number for D3D driver extensions.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		unsigned int extensionVersion;

		/*******************************************************************************
		 * intelExtensionAvailability
		 *
		 *     True if Intel driver extensions are available on the GPU.
		 *
		 *     This value is initialized by the InitExtensionInfo function.
		 *
		 ******************************************************************************/
		bool intelExtensionAvailability;

		/////////////////////////////////
		// DX11 Hardware Counters Data //
		/////////////////////////////////

		/*******************************************************************************
		 * counterAvailability
		 *
		 *     Returns true if Intel driver extensions are available to gather data from.
		 *     If this value is false, all other extension data will be null.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		bool counterAvailability;

		/*******************************************************************************
		 * maxFrequency
		 *
		 *     Returns the maximum frequency of the GPU in MHz.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		unsigned int maxFrequency;

		/*******************************************************************************
		 * minFrequency
		 *
		 *     Returns the minimum frequency of the GPU in MHz.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		unsigned int minFrequency;

		/// Advanced counter info

		/*******************************************************************************
		 * advancedCounterDataAvailability
		 *
		 *     Returns true if advanced counter data is available from this GPU.
		 *     Older Intel products only provide the maximum and minimum GPU frequency
		 *     via the hardware counters.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		bool advancedCounterDataAvailability;

		/*******************************************************************************
		 * architectureCounter
		 *
		 *     A counter that indicates which architecture the GPU uses.
		 *
		 *     This value is initialized by the InitCounterInfo function or the
		 *     InitExtensionInfo function.
		 *
		 ******************************************************************************/
		INTEL_GPU_ARCHITECTURE architectureCounter;

		/*******************************************************************************
		 * euCount
		 *
		 *     Returns the number of execution units (EUs) on the GPU.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		unsigned int euCount;

		/*******************************************************************************
		 * packageTDP
		 *
		 *     Returns the thermal design power (TDP) of the GPU in watts.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		unsigned int packageTDP;

		/*******************************************************************************
		 * maxFillRate
		 *
		 *     Returns the maximum fill rate of the GPU in pixels/clock.
		 *
		 *     This value is initialized by the InitCounterInfo function.
		 *
		 ******************************************************************************/
		unsigned int maxFillRate;

		///////////////////////
		// D3D Registry Data //
		///////////////////////

		/*******************************************************************************
		* dxDriverVersion
		*
		*     The version number for the adapter's driver. This is stored in a 4 part
		*     version number that should be displayed in the format: "0.1.2.4"
		*
		*     This value is initialized by the InitDxDriverVersion function.
		*
		******************************************************************************/
		unsigned int dxDriverVersion[ 4 ];

		/*******************************************************************************
		 * d3dRegistryDataAvailability
		 *
		 *     Is true if d3d registry data is populated. If this value is true and
		 *     vendorID == INTEL_VENDOR_ID, then the intelDriverInfo fields will be
		 *     populated. If this value is false, all other registry data data will be
		 *     null.
		 *
		 *     This value is initialized by the InitDxDriverVersion function.
		 *
		 ******************************************************************************/
		bool d3dRegistryDataAvailability;

		struct DriverVersionInfo
		{
			/*******************************************************************************
			 * osVersionID
			 *
			 *     The OS version ID for this device's driver. This is the first
			 *     section of the driver version number as illustrated by the 'X's below:
			 *     XX.00.000.0000
			 *     This can then be translated to a WDDM version using GetWDDMVersion().
			 *
			 *     This value is initialized by the InitDxDriverVersion function.
			 *
			 ******************************************************************************/
			unsigned int osVersionID;

			/*******************************************************************************
			 * directXVersionID
			 *
			 *     The DX version ID for this device's driver. This is the second
			 *     section of the driver version number as illustrated by the 'X's below:
			 *     00.XX.000.0000
			 *     This can then be translated to a DX version number using
			 *     GetDirectXVersion().
			 *
			 *     This value is initialized by the InitDxDriverVersion function.
			 *
			 ******************************************************************************/
			unsigned int directXVersionID;

			/*******************************************************************************
			 * driverReleaseRevision
			 *
			 *     The release revision for this device's driver. This is the third
			 *     section of the driver version number as illustrated by the 'X's below:
			 *     00.00.XXX.0000
			 *
			 *     This value is initialized by the InitDxDriverVersion function.
			 *
			 ******************************************************************************/
			unsigned int driverReleaseRevision;

			/*******************************************************************************
			 * buildNumber
			 *
			 *     The build number for this device's driver. This is the last
			 *     section of the driver version number as shown below:
			 *     00.00.000.XXXX
			 *
			 *     For example, for the driver version 12.34.56.7890, buildNumber
			 *     would be 7890.
			 *
			 *     This value is initialized by the InitDxDriverVersion function.
			 *
			 ******************************************************************************/
			unsigned int driverBuildNumber;
		} driverInfo;
	};

	/*******************************************************************************
	 * InitAll
	 *
	 *     Loads all available info from this GPUDetect into gpuData. Returns
	 *     EXIT_SUCCESS if no error was encountered, otherwise returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapterIndex
	 *         The index of the adapter to get the information from.
	 *
	 ******************************************************************************/
	int InitAll( GPUData* const gpuData, int adapterIndex );

	/*******************************************************************************
	 * InitAll
	 *
	 *     Loads all available info from this GPUDetect into gpuData. Returns
	 *     EXIT_SUCCESS if no error was encountered, otherwise returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapter
	 *         A pointer to the adapter to draw info from.
	 *
	 *	   device
	 *         A pointer to the device to draw info from.
	 *
	 ******************************************************************************/
	int InitAll( GPUData* const gpuData, IDXGIAdapter* adapter, ID3D11Device* device );

	/*******************************************************************************
	 * InitExtensionInfo
	 *
	 *     Loads available info from the DX11 extension interface. Returns
	 *     EXIT_SUCCESS if no error was encountered, otherwise returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapterIndex
	 *         The index of the adapter to get the information from.
	 *
	 ******************************************************************************/
	int InitExtensionInfo( GPUData* const gpuData, int adapterIndex );

	/*******************************************************************************
	 * InitExtensionInfo
	 *
	 *     Loads available info from the DX11 extension interface. Returns
	 *     EXIT_SUCCESS if no error was encountered, otherwise returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapter
	 *         A pointer to the adapter to draw info from.
	 *
	 *	   device
	 *         A pointer to the device to draw info from.
	 *
	 ******************************************************************************/
	int InitExtensionInfo( GPUData* const gpuData, IDXGIAdapter* adapter, ID3D11Device* device );

	/*******************************************************************************
	 * InitCounterInfo
	 *
	 *     Loads available info from DX11 hardware counters. Returns EXIT_SUCCESS
	 *     if no error was encountered, otherwise returns an error code. Requires
	 *     that InitExtensionInfo be run on gpuData before this is called.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapterIndex
	 *         The index of the adapter to get the information from.
	 *
	 ******************************************************************************/
	int InitCounterInfo( GPUData* const gpuData, int adapterIndex );

	/*******************************************************************************
	 * InitCounterInfo
	 *
	 *     Intel exposes additional information through the DX driver
	 *     that can be obtained querying a special counter. This loads the
	 *     available info. It returns EXIT_SUCCESS if no error was encountered,
	 *     otherwise returns an error code.
	 *
	 *      Requires that InitExtensionInfo be run on gpuData before this is called.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *	   device
	 *         A pointer to the device to draw info from.
	 *
	 ******************************************************************************/
	int InitCounterInfo( GPUData* const gpuData, ID3D11Device* device );

	/*******************************************************************************
	 * GetDefaultFidelityPreset
	 *
	 *     Function to find the default fidelity preset level, based on the type of
	 *     graphics adapter present.
	 *
	 *     The guidelines for graphics preset levels for Intel devices is a generic
	 *     one based on our observations with various contemporary games. You would
	 *     have to change it if your game already plays well on the older hardware
	 *     even at high settings.
	 *
	 *     Presets for Intel are expected in a file named "IntelGFX.cfg". This
	 *     method can also be easily modified to also read similar .cfg files
	 *     detailing presets for other manufacturers.
	 *
	 *     gpuData
	 *         The data for the GPU in question.
	 *
	 ******************************************************************************/
	PresetLevel GetDefaultFidelityPreset( const GPUData* const gpuData );

	/*******************************************************************************
	 * InitDxDriverVersion
	 *
	 *     Loads the DX driver version for the given adapter from the windows
	 *     registry. Returns EXIT_SUCCESS if no error was encountered, otherwise
	 *     returns an error code. Requires that InitExtensionInfo be run on gpuData
	 *     before this is called.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 ******************************************************************************/
	int InitDxDriverVersion( GPUData* const gpuData );

	/*******************************************************************************
	 * GetDriverVersionAsCString
	 *
	 *     Stores the driver version as a string in the 00.00.000.0000 format.
	 *
	 *     gpuData
	 *         The struct that contains the driver version.
	 *
	 *     outBuffer
	 *         Buffer to store the resulting c string.
	 *
	 *     outBufferSize
	 *         Size of buffer to store the resulting c string.
	 *
	 ******************************************************************************/
	void GetDriverVersionAsCString( const GPUData* const gpuData, char* const outBuffer, size_t outBufferSize );

	/*******************************************************************************
	 * GetWDDMVersion
	 *
	 *     Returns the Windows Display Driver Model (WDDM) number as derived from
	 *     the driver version if possible. Otherwise, returns -1.0f.
	 *
	 *     gpuData
	 *         The struct that contains the driver version.
	 *
	 ******************************************************************************/
	float GetWDDMVersion( const GPUData* const gpuData );

	/*******************************************************************************
	 * GetDirectXVersion
	 *
	 *     Returns the DX version number as derived from the driver version if
	 *     possible. Otherwise, returns -1.0f.
	 *
	 *     gpuData
	 *         The struct that contains the driver version.
	 *
	 ******************************************************************************/
	float GetDirectXVersion( const GPUData* const gpuData );

	/*******************************************************************************
	 * GetIntelGraphicsGeneration
	 *
	 *     Returns the generation of Intel(tm) Graphics, given the architecture.
	 *
	 *     architecture
	 *         The architecture to identify.
	 *
	 ******************************************************************************/
	IntelGraphicsGeneration GetIntelGraphicsGeneration( INTEL_GPU_ARCHITECTURE architecture );

	/*******************************************************************************
	 * GetIntelGraphicsGenerationAsCString
	 *
	 *     Returns the generation of Intel(tm) Graphics, given the architecture.
	 *
	 *     generation
	 *         The generation to identify.
	 *
	 *     outBuffer
	 *         Buffer to store the resulting c string.
	 *
	 *     outBufferSize
	 *         Size of buffer to store the resulting c string. Must be > 7 bytes!
	 *
	 ******************************************************************************/
	void GetIntelGraphicsGenerationAsCString( const IntelGraphicsGeneration genneration, char* const outBuffer, size_t outBufferSize  );

	/*******************************************************************************
	 * InitAdapter
	 *
	 *     Initializes a adapter of the given index. Returns EXIT_SUCCESS if no
	 *     error was encountered, otherwise returns an error code.
	 *
	 *     adapter
	 *         The address of the pointer that this function will point to
	 *         the created adapter.
	 *
	 *     adapterIndex
	 *         If adapterIndex >= 0 this will be used as as the index of the
	 *         adapter to create.
	 *
	 ******************************************************************************/
	int InitAdapter( IDXGIAdapter** adapter, int adapterIndex );

	/*******************************************************************************
	 * InitDevice
	 *
	 *     Initializes a DX11 device of the given index. Returns EXIT_SUCCESS if no
	 *     error was encountered, otherwise returns an error code.
	 *
	 *     adapter
	 *         The adapter to create the device from.
	 *
	 *     device
	 *         The address of the pointer that this function will point to the
	 *         created device.
	 *
	 ******************************************************************************/
	int InitDevice( IDXGIAdapter* adapter, ID3D11Device** device );

}
