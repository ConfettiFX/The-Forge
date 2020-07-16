////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////


#include "../include/DeviceId.h"


namespace GPUDetect
{

	char const* GetIntelGPUArchitectureString( INTEL_GPU_ARCHITECTURE arch )
	{
		switch( arch )
		{
		case IGFX_SANDYBRIDGE:   return "Sandy Bridge";
		case IGFX_IVYBRIDGE:     return "Ivy Bridge";
		case IGFX_HASWELL:       return "Haswell";
		case IGFX_VALLEYVIEW:    return "ValleyView";
		case IGFX_BROADWELL:     return "Broadwell";
		case IGFX_CHERRYVIEW:    return "Cherryview";
		case IGFX_SKYLAKE:       return "Skylake";
		case IGFX_KABYLAKE:      return "Kabylake";
		case IGFX_COFFEELAKE:    return "Coffeelake";
		case IGFX_WILLOWVIEW:    return "Willowview";
		case IGFX_BROXTON:       return "Broxton";
		case IGFX_GEMINILAKE:    return "Geminilake";
		case IGFX_CANNONLAKE:    return "Cannonlake";
		case IGFX_ICELAKE:       return "Icelake";
		case IGFX_ICELAKE_LP:    return "Icelake Low Power";
		case IGFX_LAKEFIELD:     return "Lakefield";

		// Architectures with no unique enum value, but that still can be determined from DeviceID
		case IGFX_WHISKEYLAKE:   return "Whiskeylake";

		case IGFX_UNKNOWN:
		case IGFX_MAX_PRODUCT:
		default:                 return "Unknown";
		}
	}

	INTEL_GPU_ARCHITECTURE GetIntelGPUArchitecture( unsigned int deviceId )
	{
		const unsigned int idhi = deviceId & 0xFF00;
		const unsigned int idlo = deviceId & 0x00FF;

		if( idhi == 0x0100 )
		{
			if( ( idlo & 0xFFF0 ) == 0x0050 || ( idlo & 0xFFF0 ) == 0x0060 )
			{
				return IGFX_IVYBRIDGE;
			}
			return IGFX_SANDYBRIDGE;
		}

		if( idhi == 0x0400 || idhi == 0x0A00 || idhi == 0x0D00 || idhi == 0x0C00 )
		{
			return IGFX_HASWELL;
		}

		if( idhi == 0x1600 || idhi == 0x0B00 )
		{
			return IGFX_BROADWELL;
		}

		if( idhi == 0x1900 || idhi == 0x0900 )
		{
			return IGFX_SKYLAKE;
		}

		if( idhi == 0x5900 )
		{
			return IGFX_KABYLAKE;
		}

		if( idhi == 0x3100 )
		{
			return IGFX_GEMINILAKE;;
		}

		if( idhi == 0x5A00 || idhi == 0x0A00 )
		{
			return IGFX_CANNONLAKE;
		}

		if( idhi == 0x3E00 )
		{
			if( idlo == 0x00A0 || idlo == 0x00A1 )
			{
				return IGFX_WHISKEYLAKE;
			}
			return IGFX_COFFEELAKE;
		}

		if ( idhi == 0x8A00 )
		{
			return IGFX_ICELAKE_LP;
		}

		return IGFX_UNKNOWN;
	}

}
