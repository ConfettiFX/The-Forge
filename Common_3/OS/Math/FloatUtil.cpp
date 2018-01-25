/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include "FloatUtil.h"

float lerp(const float u, const float v, const float x) {
	return u + x * (v - u);
}

float cerp(const float u0, const float u1, const float u2, const float u3, float x) {
	float p = (u3 - u2) - (u0 - u1);
	float q = (u0 - u1) - p;
	float r = u2 - u0;
	return x * (x * (x * p + q) + r) + u1;
}

float sign(const float v) {
	return (v > 0) ? 1.0f : (v < 0) ? -1.0f : 0.0f;
}

float clamp(const float v, const float c0, const float c1) {
	return min(max(v, c0), c1);
}

float saturate(const float x)
{
	return clamp(x, 0, 1);
}

float sCurve(const float t) {
	return t * t * (3 - 2 * t);
}