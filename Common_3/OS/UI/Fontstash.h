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

#pragma once

#include "../Interfaces/IFileSystem.h"
#include "../Math/MathTypes.h"

class Fontstash 
{
public:
	Fontstash(class UIRenderer* renderer, int width, int height);
	~Fontstash();

	//! Makes a font available to the font stash.
	//! - Fonts can not be undefined in a FontStash due to its dynamic nature (once packed into an atlas, they cannot be unpacked, unless it is fully rebuilt)
	//! - Defined fonts will automatically be unloaded when the Fontstash is destroyed.
	//! - When it is paramount to be able to unload individual fonts, use multiple fontstashes.
	int defineFont(const char* identification, const char* filename);
	int defineFont(const char* identification, const char* filename, FSRoot root);

	//! Find a font by user defined identification.
	int getFontID(const char* identification);

	//! Draw text.
	void drawText(const char* message, float x, float y, int fontID, unsigned int color=0xffffffff, float size=16.0f, float spacing=0.0f, float blur=0.0f);

	//! Measure text boundaries. Results will be written to out_bounds (x,y,x2,y2).
	float measureText(float* out_bounds, const char* message, float x, float y, int fontID, unsigned int color=0xffffffff, float size=16.0f, float spacing=0.0f, float blur=0.0f);
	float measureText(float* out_bounds, const char* message, int messageLength, float x, float y, int fontID, unsigned int color=0xffffffff, float size=16.0f, float spacing=0.0f, float blur=0.0f);
protected:
	class _Impl_FontStash* impl;
};
