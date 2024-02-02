/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

// Support external config file override
#if defined(EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH)
#include EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH
#elif defined(EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH_NO_STRING)
// When invoking clanng from FastBuild the EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH define doesn't get expanded to a string,
// quotes are removed, that's why we add this variation of the macro that turns the define back into a valid string
#define TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY2(x) #x
#define TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY(x)  TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY2(x)

#include TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY(EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH_NO_STRING)

#undef TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY
#undef TF_EXTERNAL_ASSET_PIPELINE_CONFIG_STRINGIFY2
#else

// Define these for the asset pipeline to add the proper defines to implement the functions of these libraries.
// Don't define them if the AssetPipeline is linked against some other library that already implements these functions (it would generate
// linker errors).
#define ENABLE_ASSET_PIPELINE_CGLTF_WRITE_IMPLEMENTATION
#define ENABLE_ASSET_PIPELINE_CGLTF_IMPLEMENTATION
#define ENABLE_ASSET_PIPELINE_TINYDDS_IMPLEMENTATION
#define ENABLE_ASSET_PIPELINE_TINYKTX_IMPLEMENTATION

#endif
