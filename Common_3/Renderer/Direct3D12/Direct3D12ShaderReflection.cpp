/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#ifdef DIRECT3D12
#include "../IRenderer.h"
#include "../../OS/Interfaces/ILogManager.h"

#ifdef _DURANGO
#include "..\..\..\Xbox\CommonXBOXOne_3\OS\XBoxPrivateHeaders.h"
#else
#include <d3dcompiler.h>
#include "../../../Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/dxcapi.use.h"
extern dxc::DxcDllSupport gDxcDllHelper;
#endif

#include "../../OS/Interfaces/IMemoryManager.h"

static DescriptorType sD3D12_TO_DESCRIPTOR[] = {
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,    //D3D_SIT_CBUFFER
	DESCRIPTOR_TYPE_UNDEFINED,         //D3D_SIT_TBUFFER
	DESCRIPTOR_TYPE_TEXTURE,           //D3D_SIT_TEXTURE
	DESCRIPTOR_TYPE_SAMPLER,           //D3D_SIT_SAMPLER
	DESCRIPTOR_TYPE_RW_TEXTURE,        //D3D_SIT_UAV_RWTYPED
	DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_STRUCTURED
	DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_RWSTRUCTURED
	DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_BYTEADDRESS
	DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_RWBYTEADDRESS
	DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_APPEND_STRUCTURED
	DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_CONSUME_STRUCTURED
	DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER
};

void d3d12_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection)
{
	//Check to see if parameters are valid
	if (shaderCode == NULL)
	{
		LOGERROR("Parameter 'shaderCode' was NULL.");
		return;
	}
	if (shaderSize == 0)
	{
		LOGERROR("Parameter 'shaderSize' was 0.");
		return;
	}
	if (pOutReflection == NULL)
	{
		LOGERROR("Paramater 'pOutReflection' was NULL.");
		return;
	}

	//Run the D3D12 shader reflection on the compiled shader
	ID3D12ShaderReflection* d3d12reflection = NULL;
	D3DReflect(shaderCode, shaderSize, IID_PPV_ARGS(&d3d12reflection));
#ifndef _DURANGO
	if (!d3d12reflection)
	{
		IDxcLibrary* pLibrary = NULL;
		gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary);
		IDxcBlobEncoding* pBlob = NULL;
		pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shaderCode, (UINT32)shaderSize, 0, &pBlob);
#define DXIL_FOURCC(ch0, ch1, ch2, ch3) \
	((uint32_t)(uint8_t)(ch0) | (uint32_t)(uint8_t)(ch1) << 8 | (uint32_t)(uint8_t)(ch2) << 16 | (uint32_t)(uint8_t)(ch3) << 24)

		IDxcContainerReflection* pReflection;
		UINT32                   shaderIdx;
		gDxcDllHelper.CreateInstance(CLSID_DxcContainerReflection, &pReflection);
		pReflection->Load(pBlob);
		(pReflection->FindFirstPartKind(DXIL_FOURCC('D', 'X', 'I', 'L'), &shaderIdx));
		(pReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void**)&d3d12reflection));

		pBlob->Release();
		pLibrary->Release();
		pReflection->Release();
	}
#endif

	//Allocate our internal shader reflection structure on the stack
	ShaderReflection reflection = {};    //initialize the struct to 0

	//Get a description of this shader
	D3D12_SHADER_DESC shaderDesc;
	d3d12reflection->GetDesc(&shaderDesc);

	//Get the number of bound resources
	reflection.mShaderResourceCount = shaderDesc.BoundResources;

	//Count string sizes of the bound resources for the name pool
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		d3d12reflection->GetResourceBindingDesc(i, &bindDesc);
		reflection.mNamePoolSize += (uint32_t)strlen(bindDesc.Name) + 1;
	}

	//Get the number of input parameters
	reflection.mVertexInputsCount = 0;

	if (shaderStage == SHADER_STAGE_VERT)
	{
		reflection.mVertexInputsCount = shaderDesc.InputParameters;

		//Count the string sizes of the vertex inputs for the name pool
		for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
			d3d12reflection->GetInputParameterDesc(i, &paramDesc);
			reflection.mNamePoolSize += (uint32_t)strlen(paramDesc.SemanticName) + 1;
		}
	}
	//Get the number of threads per group
	else if (shaderStage == SHADER_STAGE_COMP)
	{
		d3d12reflection->GetThreadGroupSize(
			&reflection.mNumThreadsPerGroup[0], &reflection.mNumThreadsPerGroup[1], &reflection.mNumThreadsPerGroup[2]);
	}
	//Get the number of cnotrol point
	else if (shaderStage == SHADER_STAGE_TESC)
	{
		reflection.mNumControlPoint = shaderDesc.cControlPoints;
	}

	//Count the number of variables and add to the size of the string pool
	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		ID3D12ShaderReflectionConstantBuffer* buffer = d3d12reflection->GetConstantBufferByIndex(i);

		D3D12_SHADER_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		//We only care about constant buffers
		if (bufferDesc.Type != D3D_CT_CBUFFER)
			continue;

		for (UINT v = 0; v < bufferDesc.Variables; ++v)
		{
			ID3D12ShaderReflectionVariable* variable = buffer->GetVariableByIndex(v);

			D3D12_SHADER_VARIABLE_DESC varDesc;
			variable->GetDesc(&varDesc);

			//Only count used variables
			if (varDesc.uFlags | D3D_SVF_USED)
			{
				reflection.mNamePoolSize += (uint32_t)strlen(varDesc.Name) + 1;
				reflection.mVariableCount++;
			}
		}
	}

	//Allocate memory for the name pool
	if (reflection.mNamePoolSize)
		reflection.pNamePool = (char*)conf_calloc(reflection.mNamePoolSize, 1);
	char* pCurrentName = reflection.pNamePool;

	reflection.pVertexInputs = NULL;
	if (shaderStage == SHADER_STAGE_VERT && reflection.mVertexInputsCount > 0)
	{
		reflection.pVertexInputs = (VertexInput*)conf_malloc(sizeof(VertexInput) * reflection.mVertexInputsCount);

		for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
			d3d12reflection->GetInputParameterDesc(i, &paramDesc);

			//Get the length of the semantic name
			uint32_t len = (uint32_t)strlen(paramDesc.SemanticName);

			reflection.pVertexInputs[i].name = pCurrentName;
			reflection.pVertexInputs[i].name_size = len;
			reflection.pVertexInputs[i].size = (uint32_t)log2(paramDesc.Mask + 1) * sizeof(uint8_t[4]);

			//Copy over the name into the name pool
			memcpy(pCurrentName, paramDesc.SemanticName, len);
			pCurrentName[len] = '\0';    //add a null terminator
			pCurrentName += len + 1;     //move the name pointer through the name pool
		}
	}

	reflection.pShaderResources = NULL;
	if (reflection.mShaderResourceCount > 0)
	{
		reflection.pShaderResources = (ShaderResource*)conf_malloc(sizeof(ShaderResource) * reflection.mShaderResourceCount);

		for (uint32_t i = 0; i < reflection.mShaderResourceCount; ++i)
		{
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			d3d12reflection->GetResourceBindingDesc(i, &bindDesc);

			uint32_t len = (uint32_t)strlen(bindDesc.Name);

			reflection.pShaderResources[i].type = sD3D12_TO_DESCRIPTOR[bindDesc.Type];
			reflection.pShaderResources[i].set = bindDesc.Space;
			reflection.pShaderResources[i].reg = bindDesc.BindPoint;
			reflection.pShaderResources[i].size = bindDesc.BindCount;
			reflection.pShaderResources[i].used_stages = shaderStage;
			reflection.pShaderResources[i].name = pCurrentName;
			reflection.pShaderResources[i].name_size = len;

			// RWTyped is considered as DESCRIPTOR_TYPE_TEXTURE by default so we handle the case for RWBuffer here
			if (bindDesc.Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED && bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER)
			{
				reflection.pShaderResources[i].type = DESCRIPTOR_TYPE_RW_BUFFER;
			}
			// Buffer<> is considered as DESCRIPTOR_TYPE_TEXTURE by default so we handle the case for Buffer<> here
			if (bindDesc.Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE && bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER)
			{
				reflection.pShaderResources[i].type = DESCRIPTOR_TYPE_BUFFER;
			}

			//Copy over the name
			memcpy(pCurrentName, bindDesc.Name, len);
			pCurrentName[len] = '\0';
			pCurrentName += len + 1;
		}
	}

	if (reflection.mVariableCount > 0)
	{
		reflection.pVariables = (ShaderVariable*)conf_malloc(sizeof(ShaderVariable) * reflection.mVariableCount);

		UINT v = 0;
		for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
		{
			//Get the constant buffer
			ID3D12ShaderReflectionConstantBuffer* buffer = d3d12reflection->GetConstantBufferByIndex(i);

			//Get the constant buffer description
			D3D12_SHADER_BUFFER_DESC bufferDesc;
			buffer->GetDesc(&bufferDesc);

			//We only care about constant buffers
			if (bufferDesc.Type != D3D_CT_CBUFFER)
				continue;

			//Find the resource index for the constant buffer
			uint32_t resourceIndex = ~0u;
			for (UINT r = 0; r < shaderDesc.BoundResources; ++r)
			{
				D3D12_SHADER_INPUT_BIND_DESC inputDesc;
				d3d12reflection->GetResourceBindingDesc(r, &inputDesc);

				if (inputDesc.Type == D3D_SIT_CBUFFER && strcmp(inputDesc.Name, bufferDesc.Name) == 0)
				{
					resourceIndex = r;
					break;
				}
			}
			ASSERT(resourceIndex != ~0u);

			//Go through all the variables in the constant buffer
			for (UINT j = 0; j < bufferDesc.Variables; ++j)
			{
				//Get the variable
				ID3D12ShaderReflectionVariable* variable = buffer->GetVariableByIndex(j);

				//Get the variable description
				D3D12_SHADER_VARIABLE_DESC varDesc;
				variable->GetDesc(&varDesc);

				//If the variable is used in the shader
				if (varDesc.uFlags | D3D_SVF_USED)
				{
					uint32_t len = (uint32_t)strlen(varDesc.Name);

					reflection.pVariables[v].parent_index = resourceIndex;
					reflection.pVariables[v].offset = varDesc.StartOffset;
					reflection.pVariables[v].size = varDesc.Size;
					reflection.pVariables[v].name = pCurrentName;
					reflection.pVariables[v].name_size = len;

					//Copy over the name
					memcpy(pCurrentName, varDesc.Name, len);
					pCurrentName[len] = '\0';
					pCurrentName += len + 1;

					++v;
				}
			}
		}
	}

	reflection.mShaderStage = shaderStage;

	d3d12reflection->Release();

	//Copy the shader reflection data to the output variable
	*pOutReflection = reflection;
}

#endif
