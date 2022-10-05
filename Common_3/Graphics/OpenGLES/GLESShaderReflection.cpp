/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../GraphicsConfig.h"

#if defined(GLES)

#include "../Interfaces/IGraphics.h"

#include "../ThirdParty/OpenSource/OpenGL/GLES2/gl2.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IMemory.h"

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

void gles_createShaderReflection(Shader* pProgram, ShaderReflection* pOutReflection, const BinaryShaderDesc* pDesc)
{
	ASSERT(pProgram);
	const GLuint glProgram = pProgram->mGLES.mProgram;
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
		GLchar* attrName = (GLchar*)tf_calloc(activeAttributeMaxLength, sizeof(GLchar));
		for (GLint i = 0; i < reflection.mVertexInputsCount; ++i)
		{
			VertexInput& vertexInput = reflection.pVertexInputs[i];
			memset(attrName, 0, activeAttributeMaxLength * sizeof(GLchar));
			GLenum attrType = 0;
			CHECK_GLRESULT(glGetActiveAttrib(glProgram, i, activeAttributeMaxLength, (GLsizei*)&vertexInput.name_size, (GLint*)&vertexInput.size, &attrType, attrName));
			LOGF(LogLevel::eDEBUG, "Program %u, Attribute %d, size %d, type %u, name \"%s\"", glProgram, i, vertexInput.size, attrType, attrName);
			reflection.mNamePoolSize += vertexInput.name_size + 1;
		}
		tf_free(attrName);
	}

	// Retrieve variable and shaderResource counts and name length
	if (nUniforms > 0)
	{
		char* shaderResourceName = (char*)tf_calloc(activeUniformMaxLength, 1); 
		GLchar* uniformName = (GLchar*)tf_calloc(activeUniformMaxLength, sizeof(GLchar));
		for (GLint i = 0; i < nUniforms; ++i)
		{
			memset(uniformName, 0, activeUniformMaxLength * sizeof(GLchar));
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
		}
		tf_free(uniformName);
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
				shaderVariable.offset = location - pResource->reg; // GL Uniform Buffer Object offset
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


	//Copy the shader reflection data to the output variable
	*pOutReflection = reflection;
}
#endif    // GLES