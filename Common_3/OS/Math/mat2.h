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

#ifndef _CFX_MAT2_
#define _CFX_MAT2_

#include "FloatUtil.h"

//----------------------------------------------------------------------------
// mat2
// Bullet doesn't have a mat2 class, perhaps we should submit it to them.
//----------------------------------------------------------------------------
struct mat2 {
	vec2 mCol0;
	vec2 mCol1;

	mat2() {}
	mat2(const vec2 &col0, const vec2 &col1) {
		mCol0 = col0;
		mCol1 = col1;
	}
	mat2(const float m00, const float m01,
		const float m10, const float m11) {
		mCol0 = vec2(m00, m10);
		mCol1 = vec2(m01, m11);
	}

	const vec2 getCol(int col) const;
	float getElem(int col, int row) const;
};

mat2 operator + (const mat2 &m, const mat2 &n);
mat2 operator - (const mat2 &m, const mat2 &n);
mat2 operator - (const mat2 &m);

mat2 operator * (const mat2 &m, const mat2 &n);
vec2 operator * (const mat2 &m, const vec2 &v);
mat2 operator * (const mat2 &m, const float x);

float det(const mat2 &m);
mat2 operator ! (const mat2 &m);

mat2 identity2();
mat2 rotate(const float angle);

#endif