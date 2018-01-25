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

// vurtun UI forward declarations
struct nk_input;
struct nk_style;
struct nk_command_queue;
struct nk_command_buffer;
struct nk_user_font;
struct nk_config;
struct nk_panel;
struct nk_context;

// framework forward declarations
class UIRenderer;
class _Impl_NuklearGUIDriver;
struct Texture;

struct KeyboardCharEventData;
struct KeyboardButtonEventData;
struct MouseMoveEventData;
struct MouseButtonEventData;
struct MouseWheelEventData;

struct TextDrawDesc;

class NuklearGUIDriver
{
public:
	NuklearGUIDriver();
	~NuklearGUIDriver();

	void load(UIRenderer* renderer, int fontID, float fontSize, Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400);
	void setWindowRect(int x, int y, int width, int height);
	nk_context* getContext();
	void processInput();
	void clear(nk_command_buffer* q=0);
	void draw(nk_command_buffer* q=0, Texture* renderTarget = 0);
	void setFontCalibration(float offset, float heightScale);

	int addFont(int fontID, float size);
	nk_user_font* getFont(int index);
	void popFont();

	nk_input* getUIInput();
	nk_command_buffer* getUICommandQueue();
	nk_style* getUIConfig();

	void onChar(const KeyboardCharEventData* data);
	void onKey(const KeyboardButtonEventData* data);
	void onJoystick(int button, bool down);
	void onMouseMove(const MouseMoveEventData* data);
	void onMouseClick(const MouseButtonEventData* data);
	void onMouseScroll(const MouseWheelEventData* data);
protected:

	_Impl_NuklearGUIDriver* impl;
};
