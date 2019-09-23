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

#if PROFILE_ENABLED

class UIApp;

/// Turn profiler display on/off.
void toggleWidgetProfilerUI();
/// Init profiler widget ui.
void initWidgetProfilerUI(UIApp* uiApp, int32_t width, int32_t height);
/// Use widgets to draw the profiler. Needs init or this will crash.
void drawWidgetProfilerUI();
/// Delete and reset any profiler widget ui data.
void unloadWidgetProfilerUI();

#endif
