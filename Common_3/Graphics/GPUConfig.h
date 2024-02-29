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

#include "../Application/Config.h"

#include "Interfaces/IGraphics.h"

// read gpu.cfg and store all its content in specific structures
FORGE_API void initGPUSettings(ExtendedSettings* pExtendedSettings);

// free all specific gpu.cfg structures
FORGE_API void exitGPUSettings();

// set default value, samplerAnisotropySupported, graphicsQueueSupported, primitiveID
FORGE_API void setDefaultGPUSettings(GPUSettings* pGpuSettings);

// selects best gpu depending on the gpu comparison rules stored in gpu.cfg
FORGE_API uint32_t util_select_best_gpu(GPUSettings* availableSettings, uint32_t gpuCount);

// reads the gpu data and sets the preset level of all available gpu's
FORGE_API GPUPresetLevel getDefaultPresetLevel();
FORGE_API GPUPresetLevel getGPUPresetLevel(const char* vendorName, const char* modelName);
FORGE_API GPUPresetLevel getGPUPresetLevel(const char* vendorName, const char* modelName, uint32_t modelId);
FORGE_API GPUPresetLevel getGPUPresetLevel(const char* vendorName, const char* modelName, uint32_t modelId, uint32_t revId);

// apply the configuration's rules to GPUSettings
FORGE_API void applyConfigurationSettings(GPUSettings* pGpuSettings, GPUCapBits* pCapBits);

// apply the user extended configuration rules to the ExtendedSetting
FORGE_API void applyExtendedSettings(ExtendedSettings* pExtendedSettings, const GPUSettings* pGpuSettings);

// return if the the GPUSettings validate the current driver rejection rules
FORGE_API bool checkDriverRejectionSettings(const GPUSettings* pGpuSettings);

// ------ utilities ------
FORGE_API const char*    presetLevelToString(GPUPresetLevel preset);
FORGE_API GPUPresetLevel stringToPresetLevel(const char* presetLevel);
FORGE_API bool           gpuVendorEquals(uint32_t vendorId, const char* vendorName);
FORGE_API const char*    getGPUVendorName(uint32_t modelId);
FORGE_API uint32_t       getGPUVendorID(const char*);
