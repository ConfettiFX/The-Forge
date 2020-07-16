/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2018 Intel Corporation
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
/////////////////////////////////////////////////////////////////////////////////////////////


#pragma once


namespace GPUDetect
{
	enum INTEL_GPU_ARCHITECTURE
	{
		IGFX_UNKNOWN = 0x00,
		IGFX_SANDYBRIDGE = 0x0c,
		IGFX_IVYBRIDGE,
		IGFX_HASWELL,
		IGFX_VALLEYVIEW,
		IGFX_BROADWELL,
		IGFX_CHERRYVIEW,
		IGFX_SKYLAKE,
		IGFX_KABYLAKE,
		IGFX_COFFEELAKE,
		IGFX_WILLOWVIEW,
		IGFX_BROXTON,
		IGFX_GEMINILAKE,
		IGFX_CANNONLAKE = 0x18,
		IGFX_ICELAKE = 0x1c,
		IGFX_ICELAKE_LP,
		IGFX_LAKEFIELD,

		IGFX_MAX_PRODUCT,

		// Architectures with no enum value
		IGFX_WHISKEYLAKE
	};

	/*******************************************************************************
	 * getIntelGPUArchitecture
	 *
	 *      Returns the architecture of an Intel GPU by parsing the device id.  It
	 *      assumes that it is indeed an Intel GPU device ID (i.e., that VendorID
	 *      was INTEL_VENDOR_ID).
	 *
	 *      You cannot generally compare device IDs to compare architectures; for
	 *      example, a newer architecture may have an lower deviceID.
	 *
	 ******************************************************************************/
	INTEL_GPU_ARCHITECTURE GetIntelGPUArchitecture( unsigned int deviceId );

	/*******************************************************************************
	 * getIntelGPUArchitectureString
	 *
	 *     Convert A INTEL_GPU_ARCHITECTURE to a string.
	 *
	 ******************************************************************************/
	char const* GetIntelGPUArchitectureString( INTEL_GPU_ARCHITECTURE arch );
}
