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


#include <dxgi.h>
#include <d3d11.h>
#ifdef _WIN32_WINNT_WIN10
#include <d3d11_3.h>
#endif
#include "../include/ID3D10Extensions.h"
#include <winreg.h>
#include <tchar.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "../include/GPUDetect.h"


#define MAX_KEY_LENGTH 255

// These should only be needed for reading data from the counter
struct IntelDeviceInfo1
{
	DWORD GPUMaxFreq;
	DWORD GPUMinFreq;
};
static_assert( sizeof(IntelDeviceInfo1) == 8, "struct size mismatch" );

struct IntelDeviceInfo2 : public IntelDeviceInfo1
{
	DWORD GPUArchitecture;   // INTEL_GPU_ARCHITECTURE
	DWORD EUCount;
	DWORD PackageTDP;
	DWORD MaxFillRate;
};
static_assert( sizeof(IntelDeviceInfo2) == 24, "struct size mismatch" );


namespace GPUDetect
{

// Returns RETURN_SUCCESS if successfully initialized
int GetIntelDeviceInfo( IntelDeviceInfo2* deviceInfo, ID3D11Device* device );


int InitAll( GPUData* const gpuData, int adapterIndex )
{
	if( gpuData == nullptr || adapterIndex < 0 )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	IDXGIAdapter* adapter = nullptr;
	int returnCode = InitAdapter( &adapter, adapterIndex );
	if( returnCode != EXIT_SUCCESS )
	{
		return returnCode;
	}

	ID3D11Device* device = nullptr;
	returnCode = InitDevice( adapter, &device );
	if( returnCode != EXIT_SUCCESS )
	{
		adapter->Release();
		return returnCode;
	}

	returnCode = InitAll( gpuData, adapter, device );

	adapter->Release();
	device->Release();
	return returnCode;
}

int InitAll( GPUData* const gpuData, IDXGIAdapter* adapter, ID3D11Device* device )
{
	int returnCode = InitExtensionInfo( gpuData, adapter, device );
	if( returnCode != EXIT_SUCCESS )
	{
		return returnCode;
	}

	returnCode = InitDxDriverVersion( gpuData );
	if( returnCode != EXIT_SUCCESS )
	{
		return returnCode;
	}

	returnCode = InitCounterInfo( gpuData, device );

	return returnCode;
}

int InitExtensionInfo( GPUData* const gpuData, int adapterIndex )
{
	if( gpuData == nullptr || adapterIndex < 0 )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	IDXGIAdapter* adapter = nullptr;
	int returnCode = InitAdapter( &adapter, adapterIndex );
	if( returnCode != EXIT_SUCCESS )
	{
		return returnCode;
	}

	ID3D11Device* device = nullptr;
	returnCode = InitDevice( adapter, &device );
	if( returnCode != EXIT_SUCCESS )
	{
		adapter->Release();
		return returnCode;
	}

	returnCode = InitExtensionInfo( gpuData, adapter, device );

	adapter->Release();
	device->Release();
	return returnCode;
}

int InitExtensionInfo( GPUData* const gpuData, IDXGIAdapter* adapter, ID3D11Device* device )
{
	if ( gpuData == nullptr || adapter == nullptr || device == nullptr )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	// basic DXGI information
	DXGI_ADAPTER_DESC AdapterDesc = {};
	if( FAILED( adapter->GetDesc( &AdapterDesc ) ) )
	{
		return GPUDETECT_ERROR_DXGI_ADAPTER_CREATION;
	}

	gpuData->dxAdapterAvailability = true;

	gpuData->vendorID = AdapterDesc.VendorId;
	gpuData->deviceID = AdapterDesc.DeviceId;
	gpuData->adapterLUID = AdapterDesc.AdapterLuid;

	wcscpy_s( gpuData->description, _countof( GPUData::description ), AdapterDesc.Description );

	if( AdapterDesc.VendorId == INTEL_VENDOR_ID && AdapterDesc.DedicatedVideoMemory <= 512 * 1024 * 1024 )
	{
		gpuData->isUMAArchitecture = true;
	}

#ifdef _WIN32_WINNT_WIN10
	ID3D11Device3* pDevice3 = nullptr;
	if( SUCCEEDED( device->QueryInterface( __uuidof( ID3D11Device3 ), (void**) &pDevice3 ) ) )
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS2 FeatureData = {};
		if( SUCCEEDED( pDevice3->CheckFeatureSupport( D3D11_FEATURE_D3D11_OPTIONS2, &FeatureData, sizeof( FeatureData ) ) ) )
		{
			gpuData->isUMAArchitecture = FeatureData.UnifiedMemoryArchitecture == TRUE;
		}
		pDevice3->Release();
	}
#endif // _WIN32_WINNT_WIN10

	if( gpuData->isUMAArchitecture )
	{
		gpuData->videoMemory = AdapterDesc.SharedSystemMemory;
	}
	else
	{
		gpuData->videoMemory = AdapterDesc.DedicatedVideoMemory;
	}

	// Intel specific information
	if( AdapterDesc.VendorId == INTEL_VENDOR_ID )
	{
		gpuData->architectureCounter = GetIntelGPUArchitecture(gpuData->deviceID);

		//if (AdapterDesc.VendorId == INTEL_VENDOR_ID
		ID3D10::CAPS_EXTENSION intelExtCaps = {};
		if (S_OK == GetExtensionCaps(device, &intelExtCaps))
		{
			gpuData->extensionVersion = intelExtCaps.DriverVersion;
			gpuData->intelExtensionAvailability = (gpuData->extensionVersion >= ID3D10::EXTENSION_INTERFACE_VERSION_1_0);
		}
	}

	return EXIT_SUCCESS;
}

int InitCounterInfo( GPUData* const gpuData, int adapterIndex )
{
	if( gpuData == nullptr || adapterIndex < 0 )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	IDXGIAdapter* adapter = nullptr;
	int returnCode = InitAdapter( &adapter, adapterIndex );
	if( returnCode != EXIT_SUCCESS )
	{
		return returnCode;
	}

	ID3D11Device* device = nullptr;
	returnCode = InitDevice( adapter, &device );
	if( returnCode != EXIT_SUCCESS )
	{
		adapter->Release();
		return returnCode;
	}

	returnCode = InitCounterInfo( gpuData, device );

	adapter->Release();
	return returnCode;
}

int InitCounterInfo( GPUData* const gpuData, ID3D11Device* device )
{
	if( gpuData == nullptr || device == nullptr )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	//
	// In DirectX, Intel exposes additional information through the driver that can be obtained
	// querying a special DX counter
	//
	gpuData->counterAvailability = gpuData->vendorID == INTEL_VENDOR_ID;
	if( !gpuData->counterAvailability )
	{
		return GPUDETECT_ERROR_NOT_SUPPORTED;
	}

	IntelDeviceInfo2 info = {};
	const int deviceInfoReturnCode = GetIntelDeviceInfo( &info, device );
	if (deviceInfoReturnCode != EXIT_SUCCESS)
	{
		return deviceInfoReturnCode;
	}
	else
	{
		gpuData->maxFrequency = info.GPUMaxFreq;
		gpuData->minFrequency = info.GPUMinFreq;

		//
		// Older versions of the IntelDeviceInfo query only return
		// GPUMaxFreq and GPUMinFreq, all other members will be zero.
		//
		if (info.GPUArchitecture != IGFX_UNKNOWN)
		{
			gpuData->advancedCounterDataAvailability = true;
			gpuData->architectureCounter = (INTEL_GPU_ARCHITECTURE)info.GPUArchitecture;
			gpuData->euCount = info.EUCount;
			gpuData->packageTDP = info.PackageTDP;
			gpuData->maxFillRate = info.MaxFillRate;
		}
	}

	return EXIT_SUCCESS;
}

PresetLevel GetDefaultFidelityPreset( const GPUData* const gpuData )
{
	// Return if prerequisite info is not met
	if( !gpuData->dxAdapterAvailability )
	{
		return PresetLevel::Undefined;
	}

	//
	// Look for a config file that qualifies devices from any vendor
	// The code here looks for a file with one line per recognized graphics
	// device in the following format:
	//
	// VendorIDHex, DeviceIDHex, CapabilityEnum      ;Commented name of card
	//

	const char* cfgFileName = nullptr;

	switch( gpuData->vendorID )
	{
	case INTEL_VENDOR_ID:
		cfgFileName = "IntelGfx.cfg";
		break;

		// Add other cases in this fashion to allow for additional cfg files
		//case SOME_VENDOR_ID:
		//    cfgFileName =  "OtherBrandGfx.cfg";
		//    break;

	default:
		return PresetLevel::Undefined;;
	}

	PresetLevel presets = Undefined;

	FILE* fp = nullptr;
	fopen_s( &fp, cfgFileName, "r" );

	if( fp )
	{
		char line[ 100 ];

		//
		// read one line at a time till EOF
		//
		while( fgets( line, _countof( line ), fp ) )
		{
			//
			// Parse and remove the comment part of any line
			//
			unsigned int i = 0;
			for( ; i < _countof( line ) - 1 && line[ i ] && line[ i ] != ';'; i++ )
			{}
			line[ i ] = '\0';

			//
			// Try to extract GPUVendorId, GPUDeviceId and recommended Default Preset Level
			//
			char* context = nullptr;
			const char* const szVendorId = strtok_s( line, ",\n", &context );
			const char* const szDeviceId = strtok_s( nullptr, ",\n", &context );
			const char* const szPresetLevel = strtok_s( nullptr, ",\n", &context );

			if( ( szVendorId == nullptr ) ||
				( szDeviceId == nullptr ) ||
				( szPresetLevel == nullptr ) )
			{
				continue;  // blank or improper line in cfg file - skip to next line
			}

			unsigned int vId = 0;
			int rv = sscanf_s( szVendorId, "%x", &vId );
			assert( rv == 1 );

			unsigned int dId = 0;
			rv = sscanf_s( szDeviceId, "%x", &dId );
			assert( rv == 1 );

			//
			// If current graphics device is found in the cfg file, use the
			// pre-configured default Graphics Presets setting.
			//
			if( ( vId == gpuData->vendorID ) && ( dId == gpuData->deviceID ) )
			{
				char s[ 10 ] = {};
				sscanf_s( szPresetLevel, "%s", s, (unsigned int) _countof( s ) );

				if( !_stricmp( s, "Low" ) )
					presets = Low;
				else if( !_stricmp( s, "Medium" ) )
					presets = Medium;
				else if( !_stricmp( s, "Medium+" ) )
					presets = MediumPlus;
				else if( !_stricmp( s, "High" ) )
					presets = High;
				else
					presets = NotCompatible;

				break;
			}
		}

		fclose( fp );
	}
	else
	{
		printf( "Error: %s not found! Fallback to default presets.\n", cfgFileName );
	}

	//
	// If the current graphics device was not listed in any of the config
	// files, or if config file not found, use Low settings as default.
	// This should be changed to reflect the desired behavior for unknown
	// graphics devices.
	//
	if( presets == Undefined )
	{
		presets = Low;
	}

	return presets;
}

int InitDxDriverVersion( GPUData* const gpuData )
{
	if( gpuData == nullptr || !( gpuData->dxAdapterAvailability == true ) )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	if( gpuData->adapterLUID.HighPart == 0 && gpuData->adapterLUID.LowPart == 0 )
	{
		// This should not happen with an active/current adapter.
		// But the registry can contain old leftover driver entries with LUID == 0.
		return GPUDETECT_ERROR_BAD_DATA;
	}

	// Fetch registry data
	HKEY dxKeyHandle = nullptr;
	DWORD numOfAdapters = 0;

	LSTATUS returnCode = ::RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\DirectX"), 0, KEY_READ, &dxKeyHandle );

	if( returnCode != ERROR_SUCCESS )
	{
		return GPUDETECT_ERROR_REG_NO_D3D_KEY;
	}

	// Find all subkeys

	DWORD subKeyMaxLength = 0;

	returnCode = ::RegQueryInfoKey(
		dxKeyHandle,
		nullptr,
		nullptr,
		nullptr,
		&numOfAdapters,
		&subKeyMaxLength,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	if( returnCode != ERROR_SUCCESS )
	{
		return GPUDETECT_ERROR_REG_GENERAL_FAILURE;
	}

	subKeyMaxLength += 1; // include the null character

	uint64_t driverVersionRaw = 0;

	bool foundSubkey = false;
	TCHAR* subKeyName = new TCHAR[subKeyMaxLength];

	for( DWORD i = 0; i < numOfAdapters; ++i )
	{
		DWORD subKeyLength = subKeyMaxLength;

		returnCode = ::RegEnumKeyEx(
			dxKeyHandle,
			i,
			subKeyName,
			&subKeyLength,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		if( returnCode == ERROR_SUCCESS )
		{
			LUID adapterLUID = {};
			DWORD qwordSize = sizeof( uint64_t );

			returnCode = ::RegGetValue(
				dxKeyHandle,
				subKeyName,
				_T("AdapterLuid"),
				RRF_RT_QWORD,
				nullptr,
				&adapterLUID,
				&qwordSize
			);

			if( returnCode == ERROR_SUCCESS // If we were able to retrieve the registry values
				&& adapterLUID.HighPart == gpuData->adapterLUID.HighPart && adapterLUID.LowPart == gpuData->adapterLUID.LowPart ) // and if the vendor ID and device ID match
			{
				// We have our registry key! Let's get the driver version num now

				returnCode = ::RegGetValue(
					dxKeyHandle,
					subKeyName,
					_T("DriverVersion"),
					RRF_RT_QWORD,
					nullptr,
					&driverVersionRaw,
					&qwordSize
				);

				if( returnCode == ERROR_SUCCESS )
				{
					foundSubkey = true;
					break;
				}
			}
		}
	}

	returnCode = ::RegCloseKey( dxKeyHandle );
	assert( returnCode == ERROR_SUCCESS );
	delete[] subKeyName;

	if( !foundSubkey )
	{
		return GPUDETECT_ERROR_REG_MISSING_DRIVER_INFO;
	}

	// Now that we have our driver version as a DWORD, let's process that into something readable
	gpuData->dxDriverVersion[ 0 ] = (unsigned int) ( ( driverVersionRaw & 0xFFFF000000000000 ) >> 16 * 3 );
	gpuData->dxDriverVersion[ 1 ] = (unsigned int) ( ( driverVersionRaw & 0x0000FFFF00000000 ) >> 16 * 2 );
	gpuData->dxDriverVersion[ 2 ] = (unsigned int) ( ( driverVersionRaw & 0x00000000FFFF0000 ) >> 16 * 1 );
	gpuData->dxDriverVersion[ 3 ] = (unsigned int) ( ( driverVersionRaw & 0x000000000000FFFF ) );

	gpuData->driverInfo.osVersionID = gpuData->dxDriverVersion[0];
	gpuData->driverInfo.directXVersionID = gpuData->dxDriverVersion[1];
	gpuData->driverInfo.driverReleaseRevision = gpuData->dxDriverVersion[2];
	gpuData->driverInfo.driverBuildNumber = gpuData->dxDriverVersion[3];

	gpuData->d3dRegistryDataAvailability = true;

	return EXIT_SUCCESS;
}

void GetDriverVersionAsCString( const GPUData* const gpuData, char* const outBuffer, size_t outBufferSize )
{
	// let's assume 4 digits max per segment
	const size_t kMaxBufferSize = (4 * 4) + 3;
	if( gpuData != nullptr && outBuffer != nullptr && outBufferSize <= kMaxBufferSize)
	{
		sprintf_s( outBuffer, outBufferSize, "%u.%u.%u.%u", gpuData->dxDriverVersion[ 0 ], gpuData->dxDriverVersion[ 1 ], gpuData->dxDriverVersion[ 2 ], gpuData->dxDriverVersion[ 3 ] );
	}
}

float GetWDDMVersion( const GPUData* const gpuData )
{
	if( gpuData == nullptr || !gpuData->d3dRegistryDataAvailability )
	{
		return -1.0f;
	}

	switch( gpuData->driverInfo.osVersionID )
	{
	// Most of the time, just shift the decimal to the left
	default:                   return (float)gpuData->driverInfo.osVersionID / 10;

	// Versions that can't be derived by shifting the decimal
	case OSVersion::WIN_VISTA: return 1.0f;
	case OSVersion::WIN_7:     return 1.1f;
	case OSVersion::WIN_8:     return 1.2f;
	case OSVersion::WIN_8_1:   return 1.3f;

	// OS IDs that come before WDDM
	case 6:
	case 5:
	case 4:                    return -1.0f;
	}
}

float GetDirectXVersion( const GPUData* const gpuData )
{
	if ( gpuData == nullptr || !gpuData->d3dRegistryDataAvailability )
	{
		return -1.0f;
	}

	switch( gpuData->driverInfo.directXVersionID )
	{
	case DXVersion::DX_12_1: return 12.1f;
	case DXVersion::DX_12_0: return 12.0f;
	case DXVersion::DX_11_1: return 11.1f;
	case DXVersion::DX_11_0: return 11.0f;
	case DXVersion::DX_10_X: return 10.f;
	case DXVersion::DX_9_X:  return 9.f;
	case DXVersion::DX_8_X:  return 8.f;
	case DXVersion::DX_7_X:  return 7.f;
	case DXVersion::DX_6_X:  return 6.f;

	default: return -1.0f;
	}
}

GPUDetect::IntelGraphicsGeneration GPUDetect::GetIntelGraphicsGeneration( INTEL_GPU_ARCHITECTURE architecture )
{
	switch( architecture )
	{
		case IGFX_SANDYBRIDGE:
		case IGFX_IVYBRIDGE:
			return INTEL_GFX_GEN7;

		case IGFX_HASWELL:
			return INTEL_GFX_GEN7_5;

		case IGFX_BROADWELL:
		case IGFX_CHERRYVIEW:
			return INTEL_GFX_GEN8;

		case IGFX_SKYLAKE:
			return INTEL_GFX_GEN9;

		case IGFX_GEMINILAKE:
		case IGFX_KABYLAKE:
		case IGFX_WHISKEYLAKE:
		case IGFX_COFFEELAKE:
			return INTEL_GFX_GEN9_5;

		case IGFX_CANNONLAKE:
			return INTEL_GFX_GEN10;

		case IGFX_LAKEFIELD:
		case IGFX_ICELAKE:
		case IGFX_ICELAKE_LP:
			return INTEL_GFX_GEN11;

		default:
			return INTEL_GFX_GEN_UNKNOWN;
	}
}

void GPUDetect::GetIntelGraphicsGenerationAsCString( const IntelGraphicsGeneration generation, char* const outBuffer, size_t outBufferSize )
{
#ifdef GPUDETECT_CHECK_PRECONDITIONS
	// Check preconditions
	if ( outBuffer == nullptr || outBufferSize < 7 || outBufferSize > RSIZE_MAX )
	{
		return;
	}
#endif

	switch( generation )
	{
	default:
	case INTEL_GFX_GEN_UNKNOWN: strcpy_s( outBuffer, outBufferSize, "Unkown" ); break;

	case INTEL_GFX_GEN6:        strcpy_s( outBuffer, outBufferSize, "Gen6"   ); break;
	case INTEL_GFX_GEN7:        strcpy_s( outBuffer, outBufferSize, "Gen7"   ); break;
	case INTEL_GFX_GEN7_5:      strcpy_s( outBuffer, outBufferSize, "Gen7.5" ); break;
	case INTEL_GFX_GEN8:        strcpy_s( outBuffer, outBufferSize, "Gen8"   ); break;
	case INTEL_GFX_GEN9:        strcpy_s( outBuffer, outBufferSize, "Gen9"   ); break;
	case INTEL_GFX_GEN9_5:      strcpy_s( outBuffer, outBufferSize, "Gen9.5" ); break;
	case INTEL_GFX_GEN10:       strcpy_s( outBuffer, outBufferSize, "Gen10"  ); break;
	case INTEL_GFX_GEN11:       strcpy_s( outBuffer, outBufferSize, "Gen11"  ); break;
	}
}

int InitAdapter( IDXGIAdapter** adapter, int adapterIndex )
{
	if( adapter == nullptr || adapterIndex < 0 )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	//
	// We are relying on DXGI (supported on Windows Vista and later) to query
	// the adapter, so fail if it is not available.
	//
	// DXGIFactory1 is required by Windows Store Apps so try that first.
	//
	const HMODULE hDXGI = ::LoadLibrary( _T("dxgi.dll") );
	if( hDXGI == nullptr )
	{
		return GPUDETECT_ERROR_DXGI_LOAD;
	}

	typedef HRESULT( WINAPI*LPCREATEDXGIFACTORY )( REFIID riid, void** ppFactory );

	LPCREATEDXGIFACTORY pCreateDXGIFactory = (LPCREATEDXGIFACTORY) ::GetProcAddress( hDXGI, "CreateDXGIFactory1" );
	if( pCreateDXGIFactory == nullptr )
	{
		pCreateDXGIFactory = (LPCREATEDXGIFACTORY) ::GetProcAddress( hDXGI, "CreateDXGIFactory" );
		if( pCreateDXGIFactory == nullptr )
		{
			::FreeLibrary( hDXGI );
			return GPUDETECT_ERROR_DXGI_FACTORY_CREATION;
		}
	}

	//
	// We have the CreateDXGIFactory function so use it to actually create the factory and enumerate
	// through the adapters. Here, we are specifically looking for the Intel gfx adapter.
	//
	IDXGIFactory* pFactory = nullptr;
	if( FAILED( pCreateDXGIFactory( __uuidof( IDXGIFactory ), (void**) ( &pFactory ) ) ) )
	{
		::FreeLibrary( hDXGI );
		return GPUDETECT_ERROR_DXGI_FACTORY_CREATION;
	}

	if( FAILED( pFactory->EnumAdapters( adapterIndex, (IDXGIAdapter**) adapter ) ) )
	{
		pFactory->Release();
		::FreeLibrary( hDXGI );
		return GPUDETECT_ERROR_DXGI_ADAPTER_CREATION;
	}

	pFactory->Release();
	::FreeLibrary( hDXGI );
	return EXIT_SUCCESS;
}

int InitDevice( IDXGIAdapter* adapter, ID3D11Device** device )
{
	if ( device == nullptr )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}

	if( FAILED( ::D3D11CreateDevice( adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, device, nullptr, nullptr ) ) )
	{
		return GPUDETECT_ERROR_DXGI_DEVICE_CREATION;
	}

	return EXIT_SUCCESS;
}

int GetIntelDeviceInfo( IntelDeviceInfo2* deviceInfo, ID3D11Device* device )
{
	assert( deviceInfo != nullptr );
	assert( device != nullptr );

	//
	// Grab the device context from the device.
	//
	ID3D11DeviceContext* deviceContext = nullptr;
	device->GetImmediateContext( &deviceContext );

	//
	// Query the device to find the number of device dependent counters.
	//
	D3D11_COUNTER_INFO counterInfo = {};
	device->CheckCounterInfo( &counterInfo );
	if( counterInfo.LastDeviceDependentCounter == 0 )
	{
		deviceContext->Release();
		return GPUDETECT_ERROR_DXGI_BAD_COUNTER;
	}

	//
	// Search for the "Intel Device Information" counter and, if found, parse
	// it's description to determine the supported version.
	//
	D3D11_COUNTER_DESC counterDesc = {};
	int intelDeviceInfoVersion = 0;
	bool intelDeviceInfo = false;

	for( int i = D3D11_COUNTER_DEVICE_DEPENDENT_0; i <= counterInfo.LastDeviceDependentCounter; ++i )
	{
		counterDesc.Counter = static_cast<D3D11_COUNTER>( i );

		D3D11_COUNTER_TYPE counterType = {};
		UINT uiSlotsRequired = 0;
		UINT uiNameLength = 0;
		UINT uiUnitsLength = 0;
		UINT uiDescLength = 0;

		if( FAILED( device->CheckCounter( &counterDesc, &counterType, &uiSlotsRequired, nullptr, &uiNameLength, nullptr, &uiUnitsLength, nullptr, &uiDescLength ) ) )
		{
			continue;
		}

		LPSTR sName = new char[ uiNameLength ];
		LPSTR sUnits = new char[ uiUnitsLength ];
		LPSTR sDesc = new char[ uiDescLength ];

		intelDeviceInfo =
			SUCCEEDED( device->CheckCounter( &counterDesc, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength ) ) &&
			( strcmp( sName, "Intel Device Information" ) == 0 );

		if( intelDeviceInfo )
		{
			sscanf_s( sDesc, "Version %d", &intelDeviceInfoVersion );
		}

		delete[] sName;
		delete[] sUnits;
		delete[] sDesc;

		if( intelDeviceInfo )
		{
			break;
		}
	}

	//
	// Create the information counter, and query it to get the data. GetData()
	// returns a pointer to the data, not the actual data.
	//
	ID3D11Counter* counter = nullptr;
	if( !intelDeviceInfo || FAILED( device->CreateCounter( &counterDesc, &counter ) ) )
	{
		deviceContext->Release();
		return GPUDETECT_ERROR_DXGI_COUNTER_CREATION;
	}

	deviceContext->Begin( counter );
	deviceContext->End( counter );

	uintptr_t dataAddress = 0;
	if( deviceContext->GetData( counter, reinterpret_cast<void*>(&dataAddress), sizeof( dataAddress ), 0 ) != S_OK )
	{
		counter->Release();
		deviceContext->Release();
		return GPUDETECT_ERROR_DXGI_COUNTER_GET_DATA;
	}

	//
	// Copy the information into the user's structure
	//
	assert( intelDeviceInfoVersion == 1 || intelDeviceInfoVersion == 2 );
	const size_t infoSize = intelDeviceInfoVersion == 1
		? sizeof( IntelDeviceInfo1 )
		: sizeof( IntelDeviceInfo2 );
	assert( infoSize <= sizeof( *deviceInfo ) );
	memset( deviceInfo, 0, sizeof( *deviceInfo ) );
	memcpy( deviceInfo, reinterpret_cast<const void*>( dataAddress ), infoSize );

	counter->Release();
	deviceContext->Release();
	return EXIT_SUCCESS;
}

}
