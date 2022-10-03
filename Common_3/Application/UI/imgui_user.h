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



#pragma once
#include "../ThirdParty/OpenSource/imgui/imgui.h"

namespace ImGui {
// Returns 0 if no change
// Returns 1 if it got changed via the UI or the default textbox
// Returns 2,3,4,5 if a textbox was requested by double clik or crtl+click but wasn't spawned.
// The return value is based on which component asked for a text box since this is a multi-component widget.
/* Paramters:
	* label -> Name above widgets
	* value_p -> float value(s) to modify
	* components -> Number of values in value_p
	* minv -> min float value. If minv == maxv then no limits are applied
	* max -> max float value. Same as above.
	* format -> format to print the float value as. (Mainly used to control number of decimal points)
	* minimumHitRadius -> Determines what's the minimum hit radius for the knob to start rotating and change the value. 
	* windowWidthRatio -> [0,FLT_MAX] value that determines the final size of the all the knobs on the same line with respect to the containing window.
	                        Value of 1.0f with component = 1 --> Size of 1 knob takes full width  .. with 2 knobs and value of 1 --> each knob is half width and so on.
	* doubleTapForText -> If true then double clicking on knob will open a text box
	* spawnText -> 
	*     If true and doubleTapForText is true then the widget spawn a textu box instead of the knob widget.
	*     If false and doubleTapForText is true, then function will return an int between 2 and 5 to determine which component of the float value needs to be modified BUT the text box won't be spawned. It's up to the client to spawn it.
	 * framePaddingScale -> Multipler to control extra padding on left side of first knob
	**/
int KnobFloatN(
	const char* label, float* value_p, const int& components, const float& step = 1.0f, const float& minv = 0.f, const float& maxv = 0.f,
	const char* format = "%.1f", const float& minimumHitRadius = 0.5f, float windowWidthRatio = 1.0f, bool doubleTapForText = true,
	bool spawnText = true, float framePaddingScale = 3.0f);
}    // namespace ImGui
