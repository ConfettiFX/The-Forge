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

#include "SpirvTools.h"

#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)    // warning C4996: 'std::move_backward::_Unchecked_iterators::_Deprecate'
#endif

#include "../../ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.hpp"

// helper functions
void ReflectBoundResources(
   spirv_cross::Compiler* pCompiler,
   const spirv_cross::SmallVector<spirv_cross::Resource>& allResources,
   const std::unordered_set<uint32_t>& usedResouces,
   SPIRV_Resource* resources,
   uint32_t* current_resource,
   SPIRV_Resource_Type spriv_type)
{
   for(size_t i = 0; i < allResources.size(); ++i)
   {
	  spirv_cross::Resource const& input = allResources[i];
	  SPIRV_Resource& resource = resources[(*current_resource)++];

	  resource.SPIRV_code.id = input.id;
	  resource.SPIRV_code.type_id = input.type_id;
	  resource.SPIRV_code.base_type_id = input.base_type_id;

	  resource.type = spriv_type;

	  resource.is_used = (usedResouces.count(resource.SPIRV_code.id) != 0);

	  resource.set = pCompiler->get_decoration(resource.SPIRV_code.id, spv::DecorationDescriptorSet);
	  resource.binding = pCompiler->get_decoration(resource.SPIRV_code.id, spv::DecorationBinding);

	  spirv_cross::SPIRType type = pCompiler->get_type(resource.SPIRV_code.type_id);

	  // Special case for textureBuffer / imageBuffer
	  // textureBuffer is considered as separate image  with dimension buffer in SpirV but they require a buffer descriptor of type uniform texel buffer
	  // imageBuffer is considered as storage image with dimension buffer in SpirV but they require a buffer descriptor of type storage texel buffer
	  if (type.image.dim == spv::Dim::DimBuffer)
	  {
		  if (spriv_type == SPIRV_TYPE_IMAGES)
			resource.type = SPIRV_TYPE_UNIFORM_TEXEL_BUFFERS;
		  else if (spriv_type == SPIRV_TYPE_STORAGE_IMAGES)
			  resource.type = SPIRV_TYPE_STORAGE_TEXEL_BUFFERS;
	  }

	  // Set the texture dimensions
	  switch (type.image.dim)
	  {
	  case spv::DimBuffer:
		  resource.dim = SPIRV_DIM_BUFFER;
		  break;
	  case spv::Dim1D:
		  resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE1DARRAY : SPIRV_DIM_TEXTURE1D;
		  break;
	  case spv::Dim2D:
		  if (type.image.ms)
			resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE2DMSARRAY : SPIRV_DIM_TEXTURE2DMS;
		  else
			resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE2DARRAY : SPIRV_DIM_TEXTURE2D;
		  break;
	  case spv::Dim3D:
		  resource.dim = SPIRV_DIM_TEXTURE3D;
		  break;
	  case spv::DimCube:
		  resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURECUBEARRAY : SPIRV_DIM_TEXTURECUBE;
		  break;
	  default:
		  resource.dim = SPIRV_DIM_UNDEFINED;
		  break;
	  }

	  //if(spriv_type != SPIRV_TYPE_UNIFORM_BUFFERS)
	  {
		 if(type.array.size())
			resource.size = type.array[0];
		 else
			resource.size = 1;
	  }
	  //else
	  //{
	  //   resource.size = (uint32_t)pCompiler->get_declared_struct_size(type);
	  //}

	  // Use the instance name if there is one
	  std::string name = pCompiler->get_name(resource.SPIRV_code.id);
	  if (name.empty())
		  name = input.name;

	  resource.name_size = (uint32_t)name.size();
	  resource.name = new char[resource.name_size + 1];
	  // name is a const char * but we just allocated it so it is fine to modify it now
	  memcpy((char*)resource.name, name.data(), resource.name_size);
	  ((char*)resource.name)[resource.name_size] = 0;
   }
}


void CreateCrossCompiler(uint32_t const* SpirvBinary, uint32_t BinarySize, CrossCompiler* outCompiler)
{
   if(outCompiler == NULL)
   {
	  return; // error code here
   }
   // build the compiler
   spirv_cross::Compiler* compiler = new spirv_cross::Compiler(SpirvBinary, BinarySize);

   outCompiler->pCompiler = compiler;

   outCompiler->pShaderResouces = NULL;
   outCompiler->ShaderResourceCount = 0;

   outCompiler->pUniformVariables = NULL;
   outCompiler->UniformVariablesCount = 0;
}

void DestroyCrossCompiler(CrossCompiler* pCompiler)
{
   if(pCompiler == NULL)
   {
	  return; // error code here
   }

   spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

   delete compiler;

   for(uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
   {
	  delete[] pCompiler->pShaderResouces[i].name;
   }

   delete[] pCompiler->pShaderResouces;

   for(uint32_t i = 0; i < pCompiler->UniformVariablesCount; ++i)
   {
	  delete[] pCompiler->pUniformVariables[i].name;
   }

   delete[] pCompiler->pUniformVariables;
   delete[] pCompiler->pEntryPoint;

   pCompiler->pCompiler = NULL;
   pCompiler->pShaderResouces = NULL;
   pCompiler->ShaderResourceCount = 0;
   pCompiler->pUniformVariables = NULL;
   pCompiler->UniformVariablesCount = 0;
}

void ReflectEntryPoint(CrossCompiler* pCompiler)
{
	if (pCompiler == NULL)
	{
		return; // error code here
	}

	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	std::string entryPoint = compiler->get_entry_points_and_stages()[0].name;

	pCompiler->EntryPointSize = (uint32_t)entryPoint.size();

	pCompiler->pEntryPoint = new char[pCompiler->EntryPointSize + 1];
	memcpy(pCompiler->pEntryPoint, entryPoint.c_str(), pCompiler->EntryPointSize * sizeof(char));
	pCompiler->pEntryPoint[pCompiler->EntryPointSize] = 0;
}

void ReflectShaderResources(CrossCompiler* pCompiler)
{
   if(pCompiler == NULL)
   {
	  return; // error code here
   }
   spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

   // 1. get all shader resources
   spirv_cross::ShaderResources allResources;
   std::unordered_set<uint32_t> usedResouces;

   allResources = compiler->get_shader_resources();
   usedResouces = compiler->get_active_interface_variables();
   // 2. Count number of resources and allocate array

   uint32_t resource_count = 0;
   // resources we want to reflect
   resource_count += (uint32_t)allResources.stage_inputs.size();			// inputs
   resource_count += (uint32_t)allResources.stage_outputs.size();		    // outputs
   resource_count += (uint32_t)allResources.uniform_buffers.size();		    // const buffers
   resource_count += (uint32_t)allResources.storage_buffers.size();		    // buffers
   resource_count += (uint32_t)allResources.separate_images.size();		    // textures
   resource_count += (uint32_t)allResources.storage_images.size();		    // uav textures
   resource_count += (uint32_t)allResources.separate_samplers.size();	    // samplers
   resource_count += (uint32_t)allResources.push_constant_buffers.size();   // push constants
   resource_count += (uint32_t)allResources.subpass_inputs.size();          // input attachments
   resource_count += (uint32_t)allResources.acceleration_structures.size(); // raytracing structures

   // these we dont care about right
   // subpass_inputs - we are not going to use this   TODO: warn when found
   // sampled_images - we wont be using these		 TODO: warn when found
   // atomic_counters - not useable in Vulkan		 TODO: warn when found

   // allocate array for resources
   SPIRV_Resource* resources = new SPIRV_Resource[resource_count];

   uint32_t current_resource = 0;

   // 3. start by reflecting the shader inputs
   for(size_t i = 0; i < allResources.stage_inputs.size(); ++i)
   {
	  spirv_cross::Resource const& input = allResources.stage_inputs[i];
	  SPIRV_Resource& resource = resources[current_resource++];


	  resource.SPIRV_code.id = input.id;
	  resource.SPIRV_code.type_id = input.type_id;
	  resource.SPIRV_code.base_type_id = input.base_type_id;

	  resource.type = SPIRV_TYPE_STAGE_INPUTS;

	  resource.is_used = (usedResouces.count(resource.SPIRV_code.id) != 0);

	  resource.set = uint32_t(-1); // stage inputs dont have sets
	  resource.binding = compiler->get_decoration(resource.SPIRV_code.id, spv::DecorationLocation); // location is the binding point for inputs

	  spirv_cross::SPIRType type = compiler->get_type(resource.SPIRV_code.type_id);
	  // bit width * vecsize = size
	  resource.size = (type.width / 8) * type.vecsize;

	  resource.name_size = (uint32_t)input.name.size();
	  resource.name = new char[resource.name_size + 1];
	  // name is a const char * but we just allocated it so it is fine to modify it now
	  memcpy((char*)resource.name, input.name.data(), resource.name_size);
	  ((char*)resource.name)[resource.name_size] = 0;
   }

   // 4. reflect output
   for(size_t i = 0; i < allResources.stage_outputs.size(); ++i)
   {
	  spirv_cross::Resource const& input = allResources.stage_outputs[i];
	  SPIRV_Resource& resource = resources[current_resource++];


	  resource.SPIRV_code.id = input.id;
	  resource.SPIRV_code.type_id = input.type_id;
	  resource.SPIRV_code.base_type_id = input.base_type_id;

	  resource.type = SPIRV_TYPE_STAGE_OUTPUTS;

	  resource.is_used = (usedResouces.count(resource.SPIRV_code.id) != 0);

	  resource.set = uint32_t(-1); // stage outputs dont have sets
	  resource.binding = compiler->get_decoration(resource.SPIRV_code.id, spv::DecorationLocation); // location is the binding point for inputs

	  spirv_cross::SPIRType type = compiler->get_type(resource.SPIRV_code.type_id);
	  // bit width * vecsize = size
	  resource.size = (type.width / 8) * type.vecsize;

	  resource.name_size = (uint32_t)input.name.size();
	  resource.name = new char[resource.name_size + 1];
	  // name is a const char * but we just allocated it so it is fine to modify it now
	  memcpy((char*)resource.name, input.name.data(), resource.name_size);
	  ((char*)resource.name)[resource.name_size] = 0;
   }

   // 5. reflect the 'normal' resources
   ReflectBoundResources(compiler, allResources.uniform_buffers, usedResouces, resources, &current_resource, SPIRV_TYPE_UNIFORM_BUFFERS);
   ReflectBoundResources(compiler, allResources.storage_buffers, usedResouces, resources, &current_resource, SPIRV_TYPE_STORAGE_BUFFERS);
   ReflectBoundResources(compiler, allResources.storage_images, usedResouces, resources, &current_resource, SPIRV_TYPE_STORAGE_IMAGES);
   ReflectBoundResources(compiler, allResources.separate_images, usedResouces, resources, &current_resource, SPIRV_TYPE_IMAGES);
   ReflectBoundResources(compiler, allResources.separate_samplers, usedResouces, resources, &current_resource, SPIRV_TYPE_SAMPLERS);
   ReflectBoundResources(compiler, allResources.subpass_inputs, usedResouces, resources, &current_resource, SPIRV_TYPE_SUBPASS_INPUTS);
   ReflectBoundResources(compiler, allResources.subpass_inputs, usedResouces, resources, &current_resource, SPIRV_TYPE_SUBPASS_INPUTS);
   ReflectBoundResources(compiler, allResources.acceleration_structures, usedResouces, resources, &current_resource, SPIRV_TYPE_ACCELERATION_STRUCTURES);

   // 6. reflect push buffers
   for(size_t i = 0; i < allResources.push_constant_buffers.size(); ++i)
   {
	  spirv_cross::Resource const& input = allResources.push_constant_buffers[i];
	  SPIRV_Resource& resource = resources[current_resource++];


	  resource.SPIRV_code.id = input.id;
	  resource.SPIRV_code.type_id = input.type_id;
	  resource.SPIRV_code.base_type_id = input.base_type_id;

	  resource.type = SPIRV_TYPE_PUSH_CONSTANT;

	  resource.is_used = (usedResouces.count(resource.SPIRV_code.id) != 0);

	  resource.set = uint32_t(-1); // push consts dont have sets
	  resource.binding = uint32_t(-1); // push consts dont have bindings

	  spirv_cross::SPIRType type = compiler->get_type(resource.SPIRV_code.type_id);
	  resource.size = (uint32_t)compiler->get_declared_struct_size(type);

	  resource.name_size = (uint32_t)input.name.size();
	  resource.name = new char[resource.name_size + 1];
	  resource.dim = SPIRV_DIM_UNDEFINED;
	  // name is a const char * but we just allocated it so it is fine to modify it now
	  memcpy((char*)resource.name, input.name.data(), resource.name_size);
	  ((char*)resource.name)[resource.name_size] = 0;
   }

   pCompiler->pShaderResouces = resources;
   pCompiler->ShaderResourceCount = resource_count;
}

void ReflectShaderVariables(CrossCompiler* pCompiler)
{
   if(pCompiler == NULL)
   {
	  return; // error code here
   }
   if(pCompiler->ShaderResourceCount == 0)
   {
	  return; // error code here
   }
   spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

   // 1. count number of variables we have
   uint32_t variable_count = 0;

   for(uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
   {
	  SPIRV_Resource& resource = pCompiler->pShaderResouces[i];

	  if(resource.type == SPIRV_TYPE_UNIFORM_BUFFERS || resource.type == SPIRV_TYPE_PUSH_CONSTANT)
	  {
		 spirv_cross::SPIRType type = compiler->get_type(resource.SPIRV_code.type_id);
		 variable_count += (uint32_t)type.member_types.size();
	  }
   }

   // 2. allocate memory
   SPIRV_Variable* variables = new SPIRV_Variable[variable_count];
   uint32_t current_variable = 0;

   // 3. reflect
   for(uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
   {
	  SPIRV_Resource& resource = pCompiler->pShaderResouces[i];

	  if(resource.type == SPIRV_TYPE_UNIFORM_BUFFERS || resource.type == SPIRV_TYPE_PUSH_CONSTANT)
	  {
		 uint32_t start_of_block = current_variable;

		 spirv_cross::SPIRType type = compiler->get_type(resource.SPIRV_code.type_id);
		 for(uint32_t j = 0; j < (uint32_t)type.member_types.size(); ++j)
		 {
			SPIRV_Variable& variable = variables[current_variable++];

			variable.SPIRV_type_id = type.member_types[j];

			variable.parent_SPIRV_code = resource.SPIRV_code;
			variable.parent_index = i;

			variable.is_used = false;

			variable.size = (uint32_t)compiler->get_declared_struct_member_size(type, j);
			variable.offset = compiler->get_member_decoration(resource.SPIRV_code.base_type_id, j, spv::DecorationOffset);

			std::string member_name = compiler->get_member_name(resource.SPIRV_code.base_type_id, j);
			variable.name_size = (uint32_t)member_name.size();
			variable.name = new char[variable.name_size + 1];
			// name is a const char * but we just allocated it so it is fine to modify it now
			memcpy((char*)variable.name, member_name.data(), variable.name_size);
			((char*)variable.name)[variable.name_size] = 0;
		 }

		 spirv_cross::SmallVector<spirv_cross::BufferRange> range = compiler->get_active_buffer_ranges(resource.SPIRV_code.id);

		 for(uint32_t j = 0; j < (uint32_t)range.size(); ++j)
		 {
			variables[start_of_block + range[j].index].is_used = true;
		 }
	  }
   }

   pCompiler->pUniformVariables = variables;
   pCompiler->UniformVariablesCount = variable_count;
}

void ReflectComputeShaderWorkGroupSize(CrossCompiler* pCompiler, uint32_t* pSizeX, uint32_t* pSizeY, uint32_t* pSizeZ)
{
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	spirv_cross::SPIREntryPoint* pEntryPoint = &compiler->get_entry_point(pCompiler->pEntryPoint, compiler->get_execution_model());

	*pSizeX = pEntryPoint->workgroup_size.x;
	*pSizeY = pEntryPoint->workgroup_size.y;
	*pSizeZ = pEntryPoint->workgroup_size.z;
}

void ReflectHullShaderControlPoint(CrossCompiler* pCompiler, uint32_t* pSizeX)
{
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	spirv_cross::SPIREntryPoint* pEntryPoint = &compiler->get_entry_point(pCompiler->pEntryPoint, compiler->get_execution_model());

	*pSizeX = pEntryPoint->output_vertices;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
