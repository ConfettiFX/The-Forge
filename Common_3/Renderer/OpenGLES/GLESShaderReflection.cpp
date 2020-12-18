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

#if defined(GLES)

#include "../IRenderer.h"

#include "../../ThirdParty/OpenSource/OpenGL/GLES2/gl2.h"

#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Interfaces/IMemory.h"

inline const char* util_get_enum_string(GLenum value)
{
	switch (value)
	{
		// Errors
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		// Framebuffer status
	case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
	case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";

	default: return "GL_UNKNOWN_ERROR";
	}
}

#define CHECK_GLRESULT(exp)                                                            \
{                                                                                      \
	exp;                                                                               \
	GLenum glRes = glGetError();                                                       \
	if (glRes != GL_NO_ERROR)                                                          \
	{                                                                                  \
		LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes));  \
		ASSERT(false);                                                                 \
	}                                                                                  \
}

#define CHECK_GL_RETURN_RESULT(var, exp)									          \
{                                                                                     \
	var = exp;                                                                        \
	GLenum glRes = glGetError();                                                      \
	if (glRes != GL_NO_ERROR)                                                         \
	{                                                                                 \
		LOGF(eERROR, "%s: FAILED with Error: %s", #exp, util_get_enum_string(glRes)); \
		ASSERT(false);                                                                \
	}                                                                                 \
}

static DescriptorType convert_gl_type(GLenum type)
{
	switch (type)
	{
		case GL_FLOAT:
		case GL_FLOAT_VEC2:
		case GL_FLOAT_VEC3:
		case GL_FLOAT_VEC4:
		case GL_INT:
		case GL_INT_VEC2:
		case GL_INT_VEC3:
		case GL_INT_VEC4:
		case GL_BOOL:
		case GL_BOOL_VEC2:
		case GL_BOOL_VEC3:
		case GL_BOOL_VEC4:
		case GL_FLOAT_MAT2:
		case GL_FLOAT_MAT3:
		case GL_FLOAT_MAT4:
			return DESCRIPTOR_TYPE_BUFFER;
		case GL_SAMPLER_2D:
			return DESCRIPTOR_TYPE_TEXTURE;
		case GL_SAMPLER_CUBE:
			return DESCRIPTOR_TYPE_TEXTURE_CUBE;			
		default: ASSERT(false && "Unknown GL type"); return DESCRIPTOR_TYPE_UNDEFINED;
	}
}

// Remove '[#n]' from uniform name and return '#n'
int util_extract_array_index(char* uniformName)
{
	if (char* block = strrchr(uniformName, '['))
	{
		block[0] = '\0';
		++block;
		block[strlen(block) - 1] = '\0';
		int arrayIndex = atoi(block);
		return arrayIndex;
	}

	return 0;
}

uint32_t util_get_parent_index(char* uniformName, ShaderReflection* pReflection)
{
	for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
	{
		if (strcmp(uniformName, pReflection->pShaderResources[i].name) == 0)
			return i;
	}
	return 0;
}

ShaderResource* util_get_parent_resource(char* uniformName, ShaderReflection* pReflection)
{
	for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
	{
		if (strcmp(uniformName, pReflection->pShaderResources[i].name) == 0)
			return &pReflection->pShaderResources[i];
	}
	return nullptr;
}

const uint32_t gMaxStructs = 32;
struct ShaderStruct
{
	char* pName;
	uint32_t mVariableCount;
	uint32_t mUsageCount;
	char* ppVariables[gMaxStructs] = {};
	char* ppStructNames[gMaxStructs] = {};
};

struct ShaderStuctContainer
{
	uint32_t mStructCount;
	ShaderStruct pShaderStructs[gMaxStructs];
};

uint32_t util_get_struct_offset(const char* pUniformName, const char* pVariableName, const ShaderStuctContainer* pShaderStructContainer)
{
	for (uint32_t i = 0; i < pShaderStructContainer->mStructCount; ++i)
	{
		const ShaderStruct* pStruct = &pShaderStructContainer->pShaderStructs[i];
		{
			for (uint32_t u = 0; u < pStruct->mUsageCount; ++u)
			{
				if (strcmp(pStruct->ppStructNames[u], pUniformName) != 0)
					continue;

				for (uint32_t v = 0; v < pStruct->mVariableCount; ++v)
				{
					if (strcmp(pStruct->ppVariables[v], pVariableName) != 0)
						continue;

					return v;
				}
			}
		}
	}

	LOGF(LogLevel::eERROR, "Could not find variable offset for \"%s\" in \"%s\"", pVariableName, pUniformName);
	return 0;
}

void gles_freeShaderStructs(ShaderStuctContainer* pShaderStructContainer)
{
	for (uint32_t i = 0; i < pShaderStructContainer->mStructCount; ++i)
	{
		ShaderStruct* pStruct = &pShaderStructContainer->pShaderStructs[i];
		tf_free(pStruct->pName);

		for (uint32_t u = 0; u < pStruct->mUsageCount; ++u)
		{
			tf_free(pStruct->ppStructNames[u]);
		}

		for (uint32_t v = 0; v < pStruct->mVariableCount; ++v)
		{
			tf_free(pStruct->ppVariables[v]);
		}
	}
}

// Extract shader struct layout and usage from shader code
void gles_extractShaderStructs(const char* shaderCode, ShaderStuctContainer* pStructContainer)
{
	static const char* structChars = "struct";
	static uint32_t structLen = strlen(structChars);
	static const char* seps = "\t\n\v\f\r ";

	// Find all structs in shader code
	const char* pStruct = strstr(shaderCode, structChars);
	while(pStruct)
	{
		if (pStructContainer->mStructCount == gMaxStructs)
		{
			LOGF(LogLevel::eWARNING, "Maximum struct storage reached {%u}", gMaxStructs);
			break;
		}

		ShaderStruct& shStruct = pStructContainer->pShaderStructs[pStructContainer->mStructCount++];
		shStruct.mVariableCount = 0;
		shStruct.mUsageCount = 0;
		pStruct += structLen + 1;

		const char* pStructContent = strchr(pStruct, '{');
		ASSERT(pStructContent);
		const char* pStructEnd = strchr(pStructContent, '}');
		int contentLen = strlen(pStructContent) - strlen(pStructEnd);

		// Get struct name
		int nameLen = strlen(pStruct) - strlen(pStructContent);
		while (nameLen > 0 && strchr(seps, pStruct[nameLen -1]) != nullptr) // remove seperators
		{
			--nameLen;
		}
		shStruct.pName = (char*)tf_calloc(nameLen + 1, 1);
		strncpy(shStruct.pName, pStruct, nameLen);
		shStruct.pName[nameLen] = '\0';

		// Find struct usages
		const char* pStructUsage = pStructEnd;
		while ((pStructUsage = strstr(pStructUsage, shStruct.pName)))
		{
			if (shStruct.mUsageCount == gMaxStructs)
			{
				LOGF(LogLevel::eWARNING, "Maximum struct usage storage reached {%u} for \"%s\"", gMaxStructs, shStruct.pName);
				break;
			}

			pStructUsage += nameLen + 1;
			int length = strcspn(pStructUsage, ";");
			shStruct.ppStructNames[shStruct.mUsageCount] = (char*)tf_calloc(length + 1, 1);
			char* pStructName = shStruct.ppStructNames[shStruct.mUsageCount++];
			strncpy(pStructName, pStructUsage, length);
			pStructName[length] = '\0';
			pStructUsage += length;

			if (strchr(pStructName, '['))  // Remove array specifer 
			{
				strchr(pStructName, '[')[0] = '\0';
			}
		}

		// Find struct variables
		const char* pVariableEnd;
		while ((pVariableEnd = strchr(pStructContent, ';')))
		{
			if (shStruct.mVariableCount == gMaxStructs)
			{
				LOGF(LogLevel::eWARNING, "Maximum struct variable storage reached {%u} for \"%s\"", gMaxStructs, shStruct.pName);
				break;
			}

			// Check found location is within bounds of the struct content
			int difLen = strlen(pStructContent) - strlen(pVariableEnd);
			if (contentLen <= difLen)
				break;

			int variableLen = difLen;
			while (difLen >= 0 &&  ' ' != pStructContent[difLen])
			{
				--difLen;
			}
			++difLen; // include space

			pStructContent += difLen;
			contentLen -= difLen;
			variableLen -= difLen;
			shStruct.ppVariables[shStruct.mVariableCount] = (char*)tf_calloc(variableLen + 1, 1);
			char* pVariable = shStruct.ppVariables[shStruct.mVariableCount++];
			strncpy(pVariable, pStructContent, variableLen );
			pVariable[variableLen] = '\0';

			if (strchr(pVariable, '[')) // Remove array specifer 
			{
				strchr(pVariable, '[')[0] = '\0';
			}

			pStructContent += variableLen + 1;
			contentLen = contentLen - variableLen - 1;
		};

		pStruct = strstr(pStruct, structChars);
	}
}

void gles_createShaderReflection(Shader* pProgram, ShaderReflection* pOutReflection, const BinaryShaderDesc* pDesc)
{
	ASSERT(pProgram);
	const GLuint glProgram = pProgram->mProgram;
	if (glProgram == GL_NONE)
	{
		LOGF(LogLevel::eERROR, "Create Shader Refection failed. Invalid GL program!");
		return;
	}

	if (pOutReflection == NULL)
	{
		LOGF(LogLevel::eERROR, "Create Shader Refection failed. Invalid reflection output!");
		return;
	}
	ShaderStuctContainer shaderStructContainer;
	shaderStructContainer.mStructCount = 0;
	gles_extractShaderStructs((const char*)pDesc->mVert.pByteCode, &shaderStructContainer);
	gles_extractShaderStructs((const char*)pDesc->mFrag.pByteCode, &shaderStructContainer);

	ShaderReflection reflection = {};
	reflection.pVertexInputs = NULL;
	reflection.pShaderResources = NULL;
	reflection.pVariables = NULL;
	reflection.mShaderStage = SHADER_STAGE_VERT;

	GLint activeAttributeMaxLength = 0;
	GLint activeUniformMaxLength = 0;
	GLint nUniforms = 0; // Uniforms are divided between variables and shader resources
	CHECK_GLRESULT(glGetProgramiv(glProgram, GL_ACTIVE_ATTRIBUTES, (GLint*)&reflection.mVertexInputsCount));
	CHECK_GLRESULT(glGetProgramiv(glProgram, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &activeAttributeMaxLength));

	CHECK_GLRESULT(glGetProgramiv(glProgram, GL_ACTIVE_UNIFORMS, &nUniforms));
	CHECK_GLRESULT(glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &activeUniformMaxLength));

	// Retrieve vertexInputs and name size (name later added to namePool)
	if (reflection.mVertexInputsCount > 0)
	{
		reflection.pVertexInputs = (VertexInput*)tf_malloc(sizeof(VertexInput) * reflection.mVertexInputsCount);
		for (GLint i = 0; i < reflection.mVertexInputsCount; ++i)
		{
			VertexInput& vertexInput = reflection.pVertexInputs[i];
			char* attrName = (char*)tf_calloc(activeAttributeMaxLength, sizeof(char));
			GLenum attrType = 0;
			CHECK_GLRESULT(glGetActiveAttrib(glProgram, i, activeAttributeMaxLength, (GLsizei*)&vertexInput.name_size, (GLint*)&vertexInput.size, &attrType, attrName));
			LOGF(LogLevel::eDEBUG, "Program %u, Attribute %d, size %d, type %u, name \"%s\"", glProgram, i, vertexInput.size, attrType, attrName);
			reflection.mNamePoolSize += vertexInput.name_size + 1;
			tf_free(attrName);
		}
	}

	// Retrieve variable and shaderResource counts and name length
	if (nUniforms > 0)
	{
		char* shaderResourceName = (char*)tf_calloc(activeUniformMaxLength, 1); 
		for (GLint i = 0; i < nUniforms; ++i)
		{
			char* uniformName = (char*)tf_calloc(activeUniformMaxLength, 1);
			GLenum uniformType = 0;
			GLsizei nameSize;
			GLint size;
			CHECK_GLRESULT(glGetActiveUniform(glProgram, i, activeUniformMaxLength, &nameSize, (GLint*)&size, &uniformType, uniformName));
			GLint location;
			CHECK_GL_RETURN_RESULT(location, glGetUniformLocation(glProgram, uniformName));
			LOGF(LogLevel::eDEBUG, "Program %u, Uniform %d, location: %d, size %d, type %u, name \"%s\"", glProgram, i, location, size, uniformType, uniformName);

			// Check if part of shader resource
			char* splitName;
			if ((splitName = strrchr(uniformName, '.')))
			{
				char* variableName = (char*)tf_calloc(activeUniformMaxLength, 1);
				strcpy(variableName, uniformName);
				
				splitName[0] = '\0';
				++splitName;

				int arrayIndex = util_extract_array_index(uniformName);
				if (arrayIndex > 0)
				{
					tf_free(variableName);
					continue;
				}

				reflection.mNamePoolSize += strlen(variableName) + 1;
				++reflection.mVariableCount;

				// New shader resource
				if (strcmp(uniformName, shaderResourceName) != 0)
				{
					strncpy(shaderResourceName, uniformName, strlen(uniformName) + 1);
					reflection.mNamePoolSize += strlen(uniformName) + 1;
					++reflection.mShaderResourceCount;
				}
				tf_free(variableName);
			}
			else
			{
				util_extract_array_index(uniformName);
				++reflection.mShaderResourceCount;
				reflection.mNamePoolSize += strlen(uniformName) + 1;
			}
			tf_free(uniformName);
		}
		tf_free(shaderResourceName);
	}

	// Allocate resources
	if (reflection.mVariableCount > 0)
	{
		reflection.pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * reflection.mVariableCount);
	}

	if (reflection.mShaderResourceCount > 0)
	{
		reflection.pShaderResources = (ShaderResource*)tf_malloc(sizeof(ShaderResource) * reflection.mShaderResourceCount);
	}

	//Allocate memory for the name pool
	reflection.pNamePool = (char*)tf_calloc(reflection.mNamePoolSize, 1);
	char* pCurrentName = reflection.pNamePool;

	// Attach vertexInput names to namePool
	for (GLint i = 0; i < reflection.mVertexInputsCount; ++i)
	{
		VertexInput& vertexInput = reflection.pVertexInputs[i];
		GLint attrSize = 0;
		GLenum attrType = 0;
		CHECK_GLRESULT(glGetActiveAttrib(glProgram, i, activeAttributeMaxLength, NULL, &attrSize, &attrType, pCurrentName));
		vertexInput.name = pCurrentName;
		pCurrentName += vertexInput.name_size + 1;
	}

	// Fill shader resources
	char* shaderResourceName = (char*)tf_calloc(activeUniformMaxLength, 1);
	GLint uniformOffset = 0;
	for (GLint i = 0; i < reflection.mShaderResourceCount; ++i)
	{
		ShaderResource& shaderResource = reflection.pShaderResources[i];

		char* uniformName = (char*)tf_calloc(activeUniformMaxLength, 1);
		for (GLint j = uniformOffset; j < nUniforms; ++j)
		{
			GLenum uniformType = 0;
			GLsizei nameSize;
			GLint size;
			CHECK_GLRESULT(glGetActiveUniform(glProgram, j, activeUniformMaxLength, &nameSize, (GLint*)&size, &uniformType, uniformName));
			GLint location;
			CHECK_GL_RETURN_RESULT(location, glGetUniformLocation(glProgram, uniformName));
			char* splitName;
			if ((splitName = strrchr(uniformName, '.')))
			{
				splitName[0] = '\0';

				int arrayIndex = util_extract_array_index(uniformName);
				if (arrayIndex > 0)
				{
					reflection.pShaderResources[i - 1].size = arrayIndex + 1;
					++uniformOffset;
					continue;
				}

				if (strcmp(uniformName, shaderResourceName) != 0)
				{
					strncpy(shaderResourceName, uniformName, strlen(uniformName) + 1);

					shaderResource.type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					// Not available for GLSL 100 used it for glType
					shaderResource.set = GL_NONE; 
					shaderResource.reg = location;
					shaderResource.size = size;
					//shaderResource.used_stages = stage;

					shaderResource.name_size = strlen(uniformName);
					shaderResource.name = pCurrentName;

					// Copy name
					strncpy(pCurrentName, uniformName, shaderResource.name_size + 1);
					pCurrentName += shaderResource.name_size + 1;
					++uniformOffset;
					break;
				}
				else
				{
					++uniformOffset;
					continue;
				}
			}
			else
			{
				util_extract_array_index(uniformName);

				shaderResource.type = convert_gl_type(uniformType);
				// Not available for GLSL 100 used it for glType
				shaderResource.set = uniformType;
				shaderResource.reg = location;
				shaderResource.size = size;
				//shaderResource.used_stages = stage;

				shaderResource.name_size = strlen(uniformName);
				shaderResource.name = pCurrentName;

				// Copy name
				strncpy(pCurrentName, uniformName, shaderResource.name_size + 1);
				pCurrentName += shaderResource.name_size + 1;
				++uniformOffset;
				break;
			}
		}
		tf_free(uniformName);
	}

	memset(shaderResourceName, 0, activeUniformMaxLength);

	uint32_t variableParentIndex = 0;
	uniformOffset = 0;

	// Fill shader variables
	for (GLint i = 0; i < reflection.mVariableCount; ++i)
	{
		ShaderVariable& shaderVariable = reflection.pVariables[i];

		char* uniformName = (char*)tf_calloc(activeUniformMaxLength, 1);
		for (GLint j = uniformOffset; j < nUniforms; ++j)
		{
			GLenum uniformType = 0;
			GLsizei nameSize;
			GLint size;
			CHECK_GLRESULT(glGetActiveUniform(glProgram, j, activeUniformMaxLength, &nameSize, (GLint*)&size, &uniformType, uniformName));
			GLint location;
			CHECK_GL_RETURN_RESULT(location, glGetUniformLocation(glProgram, uniformName));
			char* splitName;
			if ((splitName = strrchr(uniformName, '.')))
			{
				char* variableName = (char*)tf_calloc(activeUniformMaxLength, 1);
				strcpy(variableName, uniformName);

				splitName[0] = '\0';
				++splitName;

				int arrayIndex = util_extract_array_index(uniformName);
				if (arrayIndex > 0)
				{
					++uniformOffset;
					tf_free(variableName);
					continue;
				}

				ShaderResource* pResource = util_get_parent_resource(uniformName, &reflection);
				if (!pResource)
				{
					LOGF(LogLevel::eERROR, "Shader resource {%s} not found for shader variable {%s}", uniformName, variableName);
					tf_free(variableName);
					continue;
				}

				util_extract_array_index(splitName);
				shaderVariable.parent_index = util_get_parent_index(uniformName, &reflection);
				shaderVariable.offset = util_get_struct_offset(uniformName, splitName, &shaderStructContainer);// location - pResource->reg; // GL Uniform Buffer Object offset
				shaderVariable.size = size;
				shaderVariable.type = uniformType;
				shaderVariable.name = pCurrentName;
				shaderVariable.name_size = strlen(variableName);

				// Copy name
				strncpy(pCurrentName, variableName, shaderVariable.name_size + 1);
				pCurrentName += shaderVariable.name_size + 1;
				++uniformOffset;
				tf_free(variableName);
				break;
			}
			else
			{
				++uniformOffset;
				++variableParentIndex;
			}
		}
		tf_free(uniformName);
	}
	tf_free(shaderResourceName);

	gles_freeShaderStructs(&shaderStructContainer);

	//Copy the shader reflection data to the output variable
	*pOutReflection = reflection;
}
#endif    // GLES