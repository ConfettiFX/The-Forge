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

#include "mat2.h"
#include <math.h>

mat2 operator + (const mat2 &m, const mat2 &n) {
	return mat2(m.mCol0 + n.mCol0, m.mCol1 + n.mCol1);
}

mat2 operator - (const mat2 &m, const mat2 &n) {
	return mat2(m.mCol0 - n.mCol0, m.mCol1 - n.mCol1);
}

mat2 operator - (const mat2 &m) {
	return mat2(-m.mCol0, -m.mCol1);
}

/*
	 [q,		r			]
	 [s,		t			]
[a,b][a*q+b*s,	a*r+b*t		]
[c,d][c*q+d*s,	c*r+b*t		]
*/

mat2 operator * (const mat2 &m, const mat2 &n) {
	return mat2(m.getCol(0)*n.getCol(0).getX() + m.getCol(1)*n.getCol(0).getY(),
		m.getCol(0)*n.getCol(1).getX() + m.getCol(1)*n.getCol(1).getY());
}

vec2 operator * (const mat2 &m, const vec2 &v) {
	return vec2(m.mCol0*v.getX() + m.mCol1*v.getY());
}

mat2 operator * (const mat2 &m, const float x) {
	return mat2(m.mCol0 * x, m.mCol1 * x);
}

float det(const mat2 &m) {
	return (m.mCol0.getX() * m.mCol1.getY() - m.mCol1.getX() * m.mCol0.getY());
}

mat2 operator ! (const mat2 &m) {
	float invDet = 1.0f / det(m);

	return mat2(
		m.mCol1.getY(), -m.mCol1.getX(),
		-m.mCol0.getY(), m.mCol0.getX()) * invDet;
}

mat2 identity2() {
	return mat2(1, 0, 0, 1);
}

mat2 rotate(const float angle) {
	float cosA = cosf(angle), sinA = sinf(angle);

	return mat2(cosA, sinA, -sinA, cosA);
}

const vec2 mat2::getCol(int col) const
{
	return *(&mCol0 + col);
}

float mat2::getElem(int col, int row) const
{
	return getCol(col)[row];
}