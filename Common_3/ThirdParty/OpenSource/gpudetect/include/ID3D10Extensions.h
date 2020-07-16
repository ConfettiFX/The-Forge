//--------------------------------------------------------------------------------------
// Copyright 2011,2012,2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------
#pragma once

namespace ID3D10
{

	/*****************************************************************************\
	CONST: EXTENSION_INTERFACE_VERSION
	PURPOSE: Version of this header file
	\*****************************************************************************/
	const UINT EXTENSION_INTERFACE_VERSION_1_0 = 0x00010000;
	const UINT EXTENSION_INTERFACE_VERSION = EXTENSION_INTERFACE_VERSION_1_0;

	/*****************************************************************************\
	CONST: CAPS_EXTENSION_KEY
	PURPOSE: KEY to pass to UMD
	\*****************************************************************************/
	const char CAPS_EXTENSION_KEY[ 16 ] = {
		'I','N','T','C',
		'E','X','T','N',
		'C','A','P','S',
		'F','U','N','C' };

	/*****************************************************************************\
	TYPEDEF: PFND3D10UMDEXT_CHECKEXTENSIONSUPPORT
	PURPOSE: Function pointer for shader flag extensions
	\*****************************************************************************/
	typedef BOOL( APIENTRY* PFND3D10UMDEXT_CHECKEXTENSIONSUPPORT )( UINT );

	/*****************************************************************************\
	STRUCT: EXTENSION_BASE
	PURPOSE: Base data structure for extension initialization data
	\*****************************************************************************/
	struct EXTENSION_BASE
	{
		// Input:
		char    Key[ 16 ];                // CAPS_EXTENSION_KEY
		UINT    ApplicationVersion;     // EXTENSION_INTERFACE_VERSION
	};

	/*****************************************************************************\
	STRUCT: CAPS_EXTENSION_1_0
	PURPOSE: Caps data structure
	\*****************************************************************************/
	struct CAPS_EXTENSION_1_0 : EXTENSION_BASE
	{
		// Output:
		UINT    DriverVersion;          // EXTENSION_INTERFACE_VERSION
		UINT    DriverBuildNumber;      // BUILD_NUMBER
	};

	typedef CAPS_EXTENSION_1_0  CAPS_EXTENSION;

#ifndef D3D10_UMD
	/*****************************************************************************\
	FUNCTION: GetExtensionCaps
	PURPOSE: Gets extension caps table from Intel graphics driver
	\*****************************************************************************/
	inline HRESULT GetExtensionCaps(
		ID3D11Device* pd3dDevice,
		CAPS_EXTENSION* pCaps )
	{
		D3D11_BUFFER_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.ByteWidth = sizeof( CAPS_EXTENSION );
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = pCaps;
		initData.SysMemPitch = sizeof( CAPS_EXTENSION );
		initData.SysMemSlicePitch = 0;

		ZeroMemory( pCaps, sizeof( CAPS_EXTENSION ) );
		memcpy( pCaps->Key, CAPS_EXTENSION_KEY,
				sizeof( pCaps->Key ) );
		pCaps->ApplicationVersion = EXTENSION_INTERFACE_VERSION;

		ID3D11Buffer* pBuffer = NULL;
		HRESULT result = pd3dDevice->CreateBuffer(
			&desc,
			&initData,
			&pBuffer );

		if( pBuffer )
			pBuffer->Release();

		if( S_OK == result )
		{
			result = ( pCaps->ApplicationVersion <= pCaps->DriverVersion ) ? S_OK : S_FALSE;
		}
		return result;
	};
#endif

	/*****************************************************************************\
	CONST: RESOURCE_EXTENSION_KEY
	PURPOSE: KEY to pass to UMD
	\*****************************************************************************/
	const char RESOURCE_EXTENSION_KEY[ 16 ] = {
		'I','N','T','C',
		'E','X','T','N',
		'R','E','S','O',
		'U','R','C','E' };

	/*****************************************************************************\
	ENUM: RESOURCE_EXTENSION_TYPE
	PURPOSE: Enumeration of supported resource extensions
	\*****************************************************************************/
	enum RESOURCE_EXTENSION_TYPE
	{
		RESOURCE_EXTENSION_RESERVED = 0,

		// Version 1_0
		RESOURCE_EXTENSION_DIRECT_ACCESS = 1,
	};

	/*****************************************************************************\
	ENUM: RESOURCE_EXTENSION_FLAGS
	PURPOSE: Enumeration for extra information
	\*****************************************************************************/
	enum RESOURCE_EXTENSION_FLAGS
	{
		RESOURCE_EXTENSION_DIRECT_ACCESS_LINEAR_ALLOCATION = 0x1,
	};

	/*****************************************************************************\
	STRUCT: RESOURCE_EXTENSION_1_0
	PURPOSE: Resource extension interface structure
	\*****************************************************************************/
	struct RESOURCE_EXTENSION_1_0 : EXTENSION_BASE
	{
		// Input:

		// Enumeration of the extension
		UINT    Type;       // RESOURCE_EXTENSION_TYPE

		// Extension data
		UINT    Data[ 16 ];
	};

	typedef RESOURCE_EXTENSION_1_0  RESOURCE_EXTENSION;

	/*****************************************************************************\
	STRUCT: RESOURCE_DIRECT_ACCESS_MAP_DATA
	PURPOSE: Direct Access Resource extension Map structure
	\*****************************************************************************/
	struct RESOURCE_DIRECT_ACCESS_MAP_DATA
	{
		void*   pBaseAddress;
		UINT    XOffset;
		UINT    YOffset;

		UINT    TileFormat;
		UINT    Pitch;
		UINT    Size;
	};

#ifndef D3D10_UMD
	/*****************************************************************************\
	FUNCTION: SetResouceExtension
	PURPOSE: Resource extension interface
	\*****************************************************************************/
	inline HRESULT SetResouceExtension(
		ID3D11Device* pd3dDevice,
		const RESOURCE_EXTENSION* pExtnDesc )
	{
		D3D11_BUFFER_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.ByteWidth = sizeof( RESOURCE_EXTENSION );
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory( &initData, sizeof( initData ) );
		initData.pSysMem = pExtnDesc;
		initData.SysMemPitch = sizeof( RESOURCE_EXTENSION );
		initData.SysMemSlicePitch = 0;

		ID3D11Buffer* pBuffer = NULL;
		HRESULT result = pd3dDevice->CreateBuffer(
			&desc,
			&initData,
			&pBuffer );

		if( pBuffer )
			pBuffer->Release();

		return result;
	}

	/*****************************************************************************\
	FUNCTION: SetDirectAccessResouceExtension
	PURPOSE: Direct Access Resource extension interface
	\*****************************************************************************/
	inline HRESULT SetDirectAccessResouceExtension(
		ID3D11Device* pd3dDevice,
		const UINT flags )
	{
		RESOURCE_EXTENSION extnDesc;
		ZeroMemory( &extnDesc, sizeof( extnDesc ) );
		memcpy( &extnDesc.Key[ 0 ], RESOURCE_EXTENSION_KEY,
				sizeof( extnDesc.Key ) );
		extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
		extnDesc.Type = RESOURCE_EXTENSION_DIRECT_ACCESS;
		extnDesc.Data[ 0 ] = flags;

		return SetResouceExtension( pd3dDevice, &extnDesc );
	}
#endif

	/*****************************************************************************\
	CONST: STATE_EXTENSION_KEY
	PURPOSE: KEY to pass to UMD
	\*****************************************************************************/
	const char STATE_EXTENSION_KEY[ 16 ] = {
		'I','N','T','C',
		'E','X','T','N',
		'S','T','A','T',
		'E','O','B','J' };

	/*****************************************************************************\
	ENUM: STATE_EXTENSION_TYPE
	PURPOSE: Enumeration of supported state extensions
	\*****************************************************************************/
	enum STATE_EXTENSION_TYPE
	{
		STATE_EXTENSION_RESERVED = 0,

		// Version 1_0
	};

	/*****************************************************************************\
	STRUCT: STATE_EXTENSION_1_0
	PURPOSE: UMD extension interface structure
	\*****************************************************************************/
	struct STATE_EXTENSION_1_0 : EXTENSION_BASE
	{
		// Input:

		// Enumeration of the extension
		UINT    Type;       // STATE_EXTENSION_TYPE

		// Extension data
		UINT    Data[ 16 ];
	};

	typedef STATE_EXTENSION_1_0 STATE_EXTENSION;

#ifndef D3D10_UMD
	/*****************************************************************************\
	FUNCTION: SetStateExtension
	PURPOSE: State extension interface
	\*****************************************************************************/
	inline HRESULT SetStateExtension(
		ID3D11Device* pd3dDevice,
		const STATE_EXTENSION* pExtnDesc )
	{
		D3D11_BUFFER_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.ByteWidth = sizeof( STATE_EXTENSION );
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory( &initData, sizeof( initData ) );
		initData.pSysMem = pExtnDesc;
		initData.SysMemPitch = sizeof( STATE_EXTENSION );
		initData.SysMemSlicePitch = 0;

		ID3D11Buffer* pBuffer = NULL;
		HRESULT result = pd3dDevice->CreateBuffer(
			&desc,
			&initData,
			&pBuffer );

		if( pBuffer )
			pBuffer->Release();

		return result;
	}
#endif

	/*****************************************************************************\
	ENUM: SHADER_EXTENSION_TYPE
	PURPOSE: Enumeration of supported shader extensions
	\*****************************************************************************/
	enum SHADER_EXTENSION_TYPE
	{
		SHADER_EXTENSION_RESERVED = 0,

		// Version 1_0
		SHADER_EXTENSION_PIXEL_SHADER_ORDERING = 1,
	};

} // namespace ID3D10
