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

#include "NuklearGUIDriver.h"

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IUIManager.h"
#include "../../ThirdParty/OpenSource/NuklearUI/nuklear.h"
#include "Fontstash.h"
#include "UIRenderer.h"
#include "../../Renderer/IRenderer.h"

#include "../Interfaces/IMemoryManager.h" //NOTE: this should be the last include in a .cpp

struct InputInstruction
{
	enum Type
	{
		ITYPE_CHAR,
		ITYPE_KEY,
		ITYPE_JOYSTICK,
		ITYPE_MOUSEMOVE,
		ITYPE_MOUSECLICK,
		ITYPE_MOUSESCROLL,
	};
	Type type;
	union {
		struct { int mousex; int mousey; int mousebutton; bool mousedown; };
		struct { int scrollx; int scrolly; int scrollamount; };
		struct { int key; bool keydown; };
		struct { unsigned int charUnicode; };
		struct { int joystickbutton; bool joystickdown; };
	};
};

class _Impl_NuklearGUIDriver
{
public:
	struct nk_command_buffer queue;
	struct nk_cursor cursor;
	struct nk_context context;

	void* memory;

	UIRenderer* renderer;
	Fontstash* fontstash;
	int fontID;
	
	float width;
	float height;

	InputInstruction inputInstructions[2048];
	int inputInstructionCount;

	float fontCalibrationOffset;

	struct Font
	{
		_Impl_NuklearGUIDriver* driver;
		struct nk_user_font userFont;
		int fontID;
	};
	Font fontStack[128];
	int currentFontStackPos;
};


// font size callback
static float font_get_width(nk_handle handle, float h, const char *text, int len)
{
	float width;
	float bounds[4];
	_Impl_NuklearGUIDriver::Font* font = (_Impl_NuklearGUIDriver::Font*)handle.ptr;
	width = font->driver->fontstash->measureText(bounds, text, (int)len, 0.0f, 0.0f, font->fontID, 0xFFFFFFFF, h, 0.0f);
	return width;
}

NuklearGUIDriver::NuklearGUIDriver()
{
	impl = conf_placement_new<_Impl_NuklearGUIDriver>(conf_calloc(1, sizeof(_Impl_NuklearGUIDriver)));
	impl->inputInstructionCount = 0;
	impl->fontCalibrationOffset = 0.0f;
	impl->currentFontStackPos = 0;
}

NuklearGUIDriver::~NuklearGUIDriver()
{
	impl->~_Impl_NuklearGUIDriver();
	conf_free(impl);
}

void NuklearGUIDriver::setFontCalibration(float offset, float fontsize)
{
	impl->fontCalibrationOffset = offset;
	impl->fontStack[impl->currentFontStackPos].userFont.height = fontsize;
}

int NuklearGUIDriver::addFont(int fontID, float size)
{
	impl->fontStack[impl->currentFontStackPos].driver = impl;
	impl->fontStack[impl->currentFontStackPos].fontID = fontID;
	impl->fontStack[impl->currentFontStackPos].userFont.userdata.ptr = impl->fontStack + impl->currentFontStackPos;
	impl->fontStack[impl->currentFontStackPos].userFont.height = size;
	impl->fontStack[impl->currentFontStackPos].userFont.width = font_get_width;
	return (impl->currentFontStackPos)++;
}

nk_user_font* NuklearGUIDriver::getFont(int index)
{
	return &impl->fontStack[index].userFont;
}

void NuklearGUIDriver::popFont()
{
	if (impl->currentFontStackPos > 1)
	{
		--(impl->currentFontStackPos);
	}
}

nk_input* NuklearGUIDriver::getUIInput()
{
	return &impl->context.input;
}

nk_command_buffer* NuklearGUIDriver::getUICommandQueue()
{
	struct nk_command_buffer *canvas;
	canvas = nk_window_get_canvas(&impl->context);
	return canvas;
}

nk_style* NuklearGUIDriver::getUIConfig()
{
	return &impl->context.style;
}

void NuklearGUIDriver::load(UIRenderer* renderer, int fontID, float fontSize,Texture* cursorTexture, float uiwidth, float uiheight)
{
	// init renderer/font
	impl->renderer = renderer;
	impl->fontstash = renderer->getFontstash(fontID);
	impl->fontID = impl->fontstash->getFontID("default");

	// init UI (input)
	memset(&impl->context.input, 0, sizeof impl->context.input);

	// init UI (font)	
	impl->fontStack[impl->currentFontStackPos].driver = impl;
	impl->fontStack[impl->currentFontStackPos].fontID = impl->fontID;
	impl->fontStack[impl->currentFontStackPos].userFont.userdata.ptr = impl->fontStack + impl->currentFontStackPos;
	impl->fontStack[impl->currentFontStackPos].userFont.height = fontSize;
	impl->fontStack[impl->currentFontStackPos].userFont.width = font_get_width;
	++(impl->currentFontStackPos);

	// init UI (command queue & config)
	// Use nk_window_get_canvas 
	// nk_buffer_init_fixed(&impl->queue, impl->memoryScratchBuffer, sizeof(impl->memoryScratchBuffer));
	nk_init_default(&impl->context, &impl->fontStack[0].userFont);
	nk_style_default(&impl->context);

	if (cursorTexture != NULL)
	{
		// init Cursor Texture
		impl->cursor.img.handle.ptr = cursorTexture;
		impl->cursor.img.h = 1;
		impl->cursor.img.w = 1;
		impl->cursor.img.region[0] = 0;
		impl->cursor.img.region[1] = 0;
		impl->cursor.img.region[2] = 1;
		impl->cursor.img.region[3] = 1;
		impl->cursor.offset.x = 0;
		impl->cursor.offset.y = 0;
		impl->cursor.size.x = 32;
		impl->cursor.size.y = 32;

		for (nk_flags i = 0; i != NK_CURSOR_COUNT; i++)
		{
			nk_style_load_cursor(&impl->context, nk_style_cursor(i), &impl->cursor);
		}

	}
	nk_style_set_font(&impl->context, &impl->fontStack[0].userFont);

	// Height width
	impl->width = uiwidth;
	impl->height = uiheight;
}

nk_context* NuklearGUIDriver::getContext()
{
	return &impl->context;
}

void NuklearGUIDriver::setWindowRect(int x, int y, int width, int height)
{
	impl->context.current->bounds.x = float(x);
	impl->context.current->bounds.y = float(y);
	impl->context.current->bounds.w = float(width);
	impl->context.current->bounds.h = float(height);
}

void NuklearGUIDriver::onChar(const KeyboardCharEventData* data)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_CHAR;
	is.charUnicode = data->unicode;

	return;
}

void NuklearGUIDriver::onKey(const KeyboardButtonEventData* data)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_KEY;
	is.key = data->key;
	is.keydown = data->pressed;

	return;
}

void NuklearGUIDriver::onJoystick(int button, bool down)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_JOYSTICK;
	is.joystickbutton = button;
	is.joystickdown = down;

	return;
}

void NuklearGUIDriver::onMouseMove(const MouseMoveEventData* data)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_MOUSEMOVE;
	is.mousex = data->x;
	is.mousey = data->y;

	return;
}

void NuklearGUIDriver::onMouseClick(const MouseButtonEventData* data)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_MOUSECLICK;
	is.mousex = data->x;
	is.mousey = data->y;
	is.mousebutton = data->button;
	is.mousedown = data->pressed;
	return;
}


void NuklearGUIDriver::onMouseScroll(const MouseWheelEventData* data)
{
	if (impl->inputInstructionCount >= sizeof(impl->inputInstructions) / sizeof(impl->inputInstructions[0])) return;

	InputInstruction& is = impl->inputInstructions[impl->inputInstructionCount++];
	is.type = InputInstruction::ITYPE_MOUSESCROLL;
	is.scrollx = data->x;
	is.scrolly = data->y;
	is.scrollamount = data->scroll;
	return;
}

void NuklearGUIDriver::processInput()
{
#if !defined(TARGET_IOS)
#if !defined(_DURANGO)
	const static int KeyIndex[] =
	{
		0,
		KEY_SHIFT,
		KEY_CTRL,
		KEY_DELETE,
		KEY_ENTER,
		KEY_TAB,
		KEY_BACKSPACE,
		0,//NK_KEY_COPY,
		0,//NK_KEY_CUT,
		0,//NK_KEY_PASTE,
		KEY_UP,
		KEY_DOWN,
		KEY_LEFT,
		KEY_RIGHT,
	};
#endif
	nk_input_begin(&impl->context);

	for (int i = 0; i < impl->inputInstructionCount; i++)
	{
		InputInstruction& is = impl->inputInstructions[i];
		switch (is.type)
		{
		case InputInstruction::ITYPE_MOUSEMOVE:
			nk_input_motion(&impl->context, is.mousex, is.mousey);
			break;
		case InputInstruction::ITYPE_MOUSECLICK:
			if (is.mousebutton != MOUSE_LEFT)
				break;
			nk_input_button(&impl->context, NK_BUTTON_LEFT, is.mousex, is.mousey, is.mousedown ? nk_true : nk_false);
			break;
		case InputInstruction::ITYPE_MOUSESCROLL:
			nk_input_scroll(&impl->context, nk_vec2(0.0f, float(is.scrollamount)));
			break;
		case InputInstruction::ITYPE_CHAR:
		{
			if(is.charUnicode >= 32)
				nk_input_unicode(&impl->context, is.charUnicode);
		}
		break;
#if !defined(_DURANGO)
		case InputInstruction::ITYPE_KEY:
		{
			for (uint i = 0; i < 14; ++i)
			{
				if (KeyIndex[i] == is.key && KeyIndex[i] != 0)
				{
					nk_input_key(&impl->context, nk_keys(i), is.keydown);
					break;
				}
			}
		}
		break;
#endif
		case InputInstruction::ITYPE_JOYSTICK:
			break;
		default:
			break;
		}
	}

	nk_input_end(&impl->context);

	// reset instruction counter
	impl->inputInstructionCount = 0;
#else
//	assert(false && "Unsupported on target iOS");
#endif
}

void NuklearGUIDriver::clear(nk_command_buffer* q/*=0*/)
{
	nk_clear(&impl->context);
	if (q == 0)
		return;

	//nk_command_buffer_reset(q);
}

//void NuklearGUIDriver::push_draw_cursor_command(nk_panel_layout * layout)
//{
//	//if (impl->cursor.handle.id == TEXTURE_NONE)
//	//{
//	//	return;
//	//}
//
//	//struct nk_rect bounds;
//	//bounds.x = impl->context.input.mouse.pos.x;
//	//bounds.y = impl->context.input.mouse.pos.y;
//	//// 16 will be the default cursor size
//	//bounds.w = max(0.f, min(16.f, (impl->width - (float)impl->context.input.mouse.pos.x)));
//	//bounds.h = max(0.f, min(16.f, (impl->height - (float)impl->context.input.mouse.pos.y)));
//	//nk_image(&impl->context, &impl->cursor);
//	return;
//}

void NuklearGUIDriver::draw(nk_command_buffer* q/*=0*/, Texture* renderTarget/* = -1*/)
{
  UNREF_PARAM(q);
  UNREF_PARAM(renderTarget);
	static const int CircleEdgeCount = 10;
	
	const struct nk_command *cmd;
	/* iterate over and execute each draw command except the text */
	nk_foreach(cmd, &impl->context)
	{
		switch (cmd->type)
		{
		case NK_COMMAND_NOP:
			break;
		case NK_COMMAND_SCISSOR:
		{
			const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;
			RectDesc scissorRect;
			scissorRect.left = s->x;
			scissorRect.right = s->x + s->w;
			scissorRect.top = s->y;
			scissorRect.bottom = s->y + s->h;
			impl->renderer->setScissor(&scissorRect);
			break;
		}
		case NK_COMMAND_LINE:
		{
			const struct nk_command_line *l = (const struct nk_command_line*)cmd;
			const float lineOffset = float(l->line_thickness) / 2;
			// thick line support
			const vec2 begin = vec2(l->begin.x, l->begin.y);
			const vec2 end = vec2(l->end.x, l->end.y);
			const vec2 normal = normalize(end - begin);
			const vec2 binormal = vec2(normal.getY(), -normal.getX()) * lineOffset;
			float2 vertices[] = 
			{
				v2ToF2(begin + binormal), v2ToF2(end + binormal),
				v2ToF2(begin - binormal), v2ToF2(end - binormal)
			};
			float4 color = float4((float)l->color.r / 255.0f, (float)l->color.g / 255.0f, (float)l->color.b / 255.0f, (float)l->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, 4, &color);
			break;
		}
		case NK_COMMAND_RECT:
		{
			const struct nk_command_rect *r = (const struct nk_command_rect*)cmd;
			float lineOffset = float(r->line_thickness);
			// thick line support
			float2 vertices[] = 
			{ 
				// top-left
				float2(r->x - lineOffset, r->y - lineOffset), float2(r->x + lineOffset, r->y + lineOffset),
				
				// top-right
				float2(r->x + r->w + lineOffset, r->y - lineOffset), float2(r->x + r->w - lineOffset, r->y + lineOffset),

				// bottom-right
				float2(r->x + r->w + lineOffset, r->y + r->h + lineOffset), float2(r->x + r->w - lineOffset, r->y + r->h - lineOffset),

				// bottom-left
				float2(r->x - lineOffset, r->y + r->h + lineOffset), float2(r->x + lineOffset, r->y + r->h - lineOffset),

				// top-left
				float2(r->x - lineOffset, r->y - lineOffset), float2(r->x + lineOffset, r->y + lineOffset),
			};
			float4 color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, 10, &color);
			break;
		}
		case NK_COMMAND_RECT_FILLED:
		{
			const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled*)cmd;
			float2 vertices[] = { MAKEQUAD(r->x, r->y, r->x + r->w, r->y + r->h, 0.0f) };
			float4 color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, 4, &color);
			break;
		}
		case NK_COMMAND_CIRCLE:
		{
			const struct nk_command_circle *r = (const struct nk_command_circle*)cmd;
			// thick line support
			const float lineOffset = float(r->line_thickness) / 2;
			const float hw = (float)r->w / 2.0f;
			const float hh = (float)r->h / 2.0f;
			const float2 center = float2(r->x + hw, r->y + hh);

			float2 vertices[(CircleEdgeCount + 1) * 2];
			float t = 0;
			for (uint i = 0; i < CircleEdgeCount + 1; ++i)
			{
				const float dt = (2 * PI) / CircleEdgeCount;
				vertices[i * 2 + 0] = center + float2(cosf(t) * (hw + lineOffset), sinf(t) * (hh + lineOffset));
				vertices[i * 2 + 1] = center + float2(cosf(t) * (hw - lineOffset), sinf(t) * (hh - lineOffset));
				t += dt;
			}

			float4 color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, (CircleEdgeCount + 1) * 2, &color);
			break;
		}
		case NK_COMMAND_CIRCLE_FILLED:
		{
			const struct nk_command_circle_filled *r = (const struct nk_command_circle_filled*)cmd;
			const float hw = (float)r->w / 2.0f;
			const float hh = (float)r->h / 2.0f;
			const float2 center = float2(r->x + hw, r->y + hh);
			
			float2 vertices[(CircleEdgeCount) * 2 + 1];
			float t = 0;
			for (uint i = 0; i < CircleEdgeCount; ++i)
			{
				const float dt = (2 * PI) / CircleEdgeCount;
				vertices[i * 2 + 0] = center + float2(cosf(t) * hw, sinf(t) * hh);
				vertices[i * 2 + 1] = center;
				t += dt;
			}
			// set last point on circle
			vertices[CircleEdgeCount * 2] = vertices[0];

			float4 color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, CircleEdgeCount * 2 + 1, &color);
			break;
		}
		case NK_COMMAND_TRIANGLE:
		{
			//const struct nk_command_triangle *r = (const struct nk_command_triangle*)cmd;
			//vec2 vertices[] = { vec2(r->a.x, r->a.y), vec2(r->b.x, r->b.y), vec2(r->c.x, r->c.y) };
			//vec4 color[] = { vec4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f) };
			//impl->renderer->drawPlain(PRIM_LINE_LOOP, vertices, 3, impl->bstAlphaBlend, impl->dstNone, color, impl->rstScissorNoCull);
			break;
		}
		case NK_COMMAND_TRIANGLE_FILLED:
		{
			const struct nk_command_triangle_filled *r = (const struct nk_command_triangle_filled*)cmd;
			float2 vertices[] = { float2(r->a.x, r->a.y), float2(r->b.x, r->b.y), float2(r->c.x, r->c.y) };
			float4 color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_LIST, vertices, 3, &color);
			break;
		}
		case NK_COMMAND_IMAGE:
		{
			const struct nk_command_image *r = (const struct nk_command_image*)cmd;

			float2 RegionTopLeft(float2((float)r->x, (float)r->y));
			float2 RegionBottonLeft(RegionTopLeft + float2(0, r->h));
			float2 RegionTopRight(RegionTopLeft + float2(r->w, 0));
			float2 RegionBottonRight(RegionTopLeft + float2(r->w, r->h));

			float2 texCoord[4] = {
				float2(float(r->img.region[0] + r->img.region[3]) / r->img.w, float(r->img.region[1] + r->img.region[2]) / r->img.h),
				float2(float(r->img.region[0] + r->img.region[3]) / r->img.w, float(r->img.region[1]) / r->img.h),
				float2(float(r->img.region[0]) / r->img.w, float(r->img.region[1] + r->img.region[2]) / r->img.h),
				float2(float(r->img.region[0]) / r->img.w, float(r->img.region[1]) / r->img.h)
			};

			TexVertex vertices[4] = { TexVertex(RegionBottonRight, texCoord[0]),
																TexVertex(RegionTopRight, texCoord[1]),
																TexVertex(RegionBottonLeft, texCoord[2]),
																TexVertex(RegionTopLeft, texCoord[3]) };

			float4 color = float4((float)r->col.r / 255.0f, (float)r->col.g / 255.0f, (float)r->col.b / 255.0f, (float)r->col.a / 255.0f);
			impl->renderer->drawTextured(PRIMITIVE_TOPO_TRI_STRIP, vertices, 4, (Texture*)r->img.handle.ptr, &color);

			break;
		}
		case NK_COMMAND_TEXT:
		{
			const struct nk_command_text *r = (const struct nk_command_text*)cmd;
			float2 vertices[] = { MAKEQUAD(r->x, r->y + impl->fontCalibrationOffset, r->x + r->w, r->y + r->h + impl->fontCalibrationOffset, 0.0f) };
			float4 color = float4((float)r->background.r / 255.0f, (float)r->background.g / 255.0f, (float)r->background.b / 255.0f, (float)r->background.a / 255.0f);

			impl->renderer->drawPlain(PRIMITIVE_TOPO_TRI_STRIP, vertices, 4, &color);
			_Impl_NuklearGUIDriver::Font* font = (_Impl_NuklearGUIDriver::Font*)r->font->userdata.ptr;
			impl->fontstash->drawText(r->string, r->x, r->y + impl->fontCalibrationOffset, font->fontID, *(unsigned int*)&r->foreground, r->font->height, 0.0f, 0.0f);
			
			break;
		}
		default:
			break;
		}
	}
}
