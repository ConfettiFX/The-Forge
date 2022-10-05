/*
 * Copyright (c) 2018-2022 The Forge Interactive Inc.
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

//Reads the gpu config and sets the preset level of all available gpu's
FORGE_API GPUPresetLevel getGPUPresetLevel(const char* vendorId, const char* modelId, const char* revId);

//Reads the graphics config and enables settings based on the available GPU settings
FORGE_API void setExtendedSettings(ExtendedSettings* pExtendedSettings, GPUSettings* pGpuSettings);

FORGE_API const char*    presetLevelToString(GPUPresetLevel preset);
FORGE_API GPUPresetLevel stringToPresetLevel(const char* presetLevel);