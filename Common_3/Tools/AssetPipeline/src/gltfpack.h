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

#pragma once

#include "../../../ThirdParty/OpenSource/meshoptimizer/tools/cgltf.h"
#include "../../../ThirdParty/OpenSource/meshoptimizer/tools/fast_obj.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

struct Attr
{
	float f[4];
};

struct Stream
{
	cgltf_attribute_type type;
	int index;
	int target; // 0 = base mesh, 1+ = morph target

	eastl::vector<Attr> data;
};

struct Mesh
{
	cgltf_node* node;

	cgltf_material* material;
	cgltf_skin* skin;

	unsigned int materialID;

	eastl::vector<Stream> streams;
	eastl::vector<unsigned int> indices;

	size_t targets;
	eastl::vector<float> weights;
};

struct Settings
{
	int pos_bits;
	int tex_bits;
	int nrm_bits;
	bool nrm_unit;

	int anim_freq;
	bool anim_const;

	bool compress;
	bool verbose;
};

struct QuantizationParams
{
	float pos_offset[3];
	float pos_scale;
	int pos_bits;

	float uv_offset[2];
	float uv_scale[2];
	int uv_bits;
};

struct StreamFormat
{
	cgltf_type type;
	cgltf_component_type component_type;
	bool normalized;
	size_t stride;
};

struct Model
{
	cgltf_data * data = NULL;
	eastl::vector<Mesh> mMeshArray;
	vec3 CenterPosition;
};

int gltfpack(int argc, const char** argv);

const char* getError(cgltf_result result);

void transformPosition(float* ptr, const float* transform);

void transformNormal(float* ptr, const float* transform);

void transformMesh(Mesh& mesh, const cgltf_node* node);

void parseMeshesGltf(cgltf_data* data, eastl::vector<Mesh>& meshes);

void reindexMesh(Mesh& mesh);

void stripifyMesh(Mesh& mesh);

void simplifyMesh(Mesh& mesh, float threshold = .5f);

void optimizeMesh(Mesh& mesh);

void quantizeMaterial(cgltf_material* meshMaterial);

void quantizeMesh(Mesh& mesh, Settings settings, QuantizationParams qp);

QuantizationParams prepareQuantization(const eastl::vector<Mesh>& meshes, const Settings& settings);