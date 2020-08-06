/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#ifdef DIRECT3D11
#include "../IRenderer.h"
#include "../../OS/Interfaces/ILog.h"
#include <d3dcompiler.h>

#include "../../OS/Interfaces/IMemory.h"

static DescriptorType sD3D11_TO_DESCRIPTOR[] = {
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,    //D3D_SIT_CBUFFER
	DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_TBUFFER
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

void d3d11_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection)
{
	//Check to see if parameters are valid
	if (shaderCode == NULL)
	{
		LOGF(LogLevel::eERROR, "Parameter 'shaderCode' was NULL.");
		return;
	}
	if (shaderSize == 0)
	{
		LOGF(LogLevel::eERROR, "Parameter 'shaderSize' was 0.");
		return;
	}
	if (pOutReflection == NULL)
	{
		LOGF(LogLevel::eERROR, "Paramater 'pOutReflection' was NULL.");
		return;
	}

	//Run the D3D11 shader reflection on the compiled shader
	ID3D11ShaderReflection* d3d11reflection;
	D3DReflect(shaderCode, shaderSize, IID_PPV_ARGS(&d3d11reflection));

	//Allocate our internal shader reflection structure on the stack
	ShaderReflection reflection = {};    //initialize the struct to 0

	//Get a description of this shader
	D3D11_SHADER_DESC shaderDesc;
	d3d11reflection->GetDesc(&shaderDesc);

	//Get the number of bound resources
	reflection.mShaderResourceCount = shaderDesc.BoundResources;

	//Count string sizes of the bound resources for the name pool
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC bindDesc;
		d3d11reflection->GetResourceBindingDesc(i, &bindDesc);
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
			D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
			d3d11reflection->GetInputParameterDesc(i, &paramDesc);
			reflection.mNamePoolSize += (uint32_t)strlen(paramDesc.SemanticName) + 2;
		}
	}
	//Get the number of threads per group
	else if (shaderStage == SHADER_STAGE_COMP)
	{
		d3d11reflection->GetThreadGroupSize(
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
		ID3D11ShaderReflectionConstantBuffer* buffer = d3d11reflection->GetConstantBufferByIndex(i);

		D3D11_SHADER_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		//We only care about constant buffers
		if (bufferDesc.Type != D3D_CT_CBUFFER)
			continue;

		for (UINT v = 0; v < bufferDesc.Variables; ++v)
		{
			ID3D11ShaderReflectionVariable* variable = buffer->GetVariableByIndex(v);

			D3D11_SHADER_VARIABLE_DESC varDesc;
			variable->GetDesc(&varDesc);

			//Only count used variables
			if ((varDesc.uFlags | D3D_SVF_USED) != 0)
			{
				reflection.mNamePoolSize += (uint32_t)strlen(varDesc.Name) + 1;
				reflection.mVariableCount++;
			}
		}
	}

	//Allocate memory for the name pool
	if (reflection.mNamePoolSize)
		reflection.pNamePool = (char*)tf_calloc(reflection.mNamePoolSize, 1);
	char* pCurrentName = reflection.pNamePool;

	reflection.pVertexInputs = NULL;
	if (shaderStage == SHADER_STAGE_VERT && reflection.mVertexInputsCount > 0)
	{
		reflection.pVertexInputs = (VertexInput*)tf_malloc(sizeof(VertexInput) * reflection.mVertexInputsCount);

		for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
			d3d11reflection->GetInputParameterDesc(i, &paramDesc);

			//Get the length of the semantic name
			eastl::string inputNameWithIndex = paramDesc.SemanticName;
			bool hasParamIndex = paramDesc.SemanticIndex > 0 || inputNameWithIndex == "TEXCOORD";
			inputNameWithIndex += hasParamIndex ? eastl::to_string(paramDesc.SemanticIndex) : "";
			uint32_t len = (uint32_t)strlen(paramDesc.SemanticName) + (hasParamIndex ? 1 : 0);

			reflection.pVertexInputs[i].name = pCurrentName;
			reflection.pVertexInputs[i].name_size = len;
			reflection.pVertexInputs[i].size = (uint32_t)log2(paramDesc.Mask + 1) * sizeof(uint8_t[4]);

			//Copy over the name into the name pool
			memcpy(pCurrentName, inputNameWithIndex.c_str(), len);
			pCurrentName[len] = '\0';    //add a null terminator
			pCurrentName += len + 1;     //move the name pointer through the name pool
		}
	}

	reflection.pShaderResources = NULL;
	if (reflection.mShaderResourceCount > 0)
	{
		reflection.pShaderResources = (ShaderResource*)tf_malloc(sizeof(ShaderResource) * reflection.mShaderResourceCount);

		for (uint32_t i = 0; i < reflection.mShaderResourceCount; ++i)
		{
			D3D11_SHADER_INPUT_BIND_DESC bindDesc;
			d3d11reflection->GetResourceBindingDesc(i, &bindDesc);

			uint32_t len = (uint32_t)strlen(bindDesc.Name);

			reflection.pShaderResources[i].type = sD3D11_TO_DESCRIPTOR[bindDesc.Type];
			reflection.pShaderResources[i].set = 0U;    // not used in dx11
			reflection.pShaderResources[i].reg = bindDesc.BindPoint;
			reflection.pShaderResources[i].size = bindDesc.BindCount;
			reflection.pShaderResources[i].used_stages = shaderStage;
			reflection.pShaderResources[i].name = pCurrentName;
			reflection.pShaderResources[i].name_size = len;

			//Copy over the name
			memcpy(pCurrentName, bindDesc.Name, len);
			pCurrentName[len] = '\0';
			pCurrentName += len + 1;
		}
	}

	if (reflection.mVariableCount > 0)
	{
		reflection.pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * reflection.mVariableCount);

		UINT v = 0;
		for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
		{
			//Get the constant buffer
			ID3D11ShaderReflectionConstantBuffer* buffer = d3d11reflection->GetConstantBufferByIndex(i);

			//Get the constant buffer description
			D3D11_SHADER_BUFFER_DESC bufferDesc;
			buffer->GetDesc(&bufferDesc);

			//We only care about constant buffers
			if (bufferDesc.Type != D3D_CT_CBUFFER)
				continue;

			//Find the resource index for the constant buffer
			uint32_t resourceIndex = ~0u;
			for (UINT r = 0; r < shaderDesc.BoundResources; ++r)
			{
				D3D11_SHADER_INPUT_BIND_DESC inputDesc;
				d3d11reflection->GetResourceBindingDesc(r, &inputDesc);

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
				ID3D11ShaderReflectionVariable* variable = buffer->GetVariableByIndex(j);

				//Get the variable description
				D3D11_SHADER_VARIABLE_DESC varDesc;
				variable->GetDesc(&varDesc);

				//If the variable is used in the shader
				if ((varDesc.uFlags | D3D_SVF_USED) != 0)
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

	d3d11reflection->Release();

	//Copy the shader reflection data to the output variable
	*pOutReflection = reflection;
}

#endif
