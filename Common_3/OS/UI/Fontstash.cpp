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

#include "Fontstash.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"

#include "UIRenderer.h"
#include "../../Renderer/IRenderer.h"
#include "../Image/Image.h"

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
// include Fontstash (should be after MemoryTracking so that it also detects memory free/remove in fontstash)
#define FONTSTASH_IMPLEMENTATION
#include "../../ThirdParty/OpenSource/Fontstash/src/fontstash.h"

#include "../Interfaces/IMemoryManager.h"

class _Impl_FontStash
{
public:
	_Impl_FontStash() 
	{
		tex = NULL; 
		width = 0;
		height = 0; 
		renderer = NULL;
		fontStashContext = NULL;
	}

	~_Impl_FontStash() 
	{
		// unload fontstash context
		fonsDeleteInternal(fontStashContext);

		img.Destroy ();

		// unload font buffers
		for(unsigned int i=0; i<(uint32_t)fontBuffers.size(); i++)
			conf_free(fontBuffers[i]);
	}

	void init(UIRenderer* _renderer, int width_, int height_)
	{
		// set renderer
		renderer = _renderer;

		// create image
		img.Create(ImageFormat::R8, width_, height_, 1, 1, 1);
		
		// init state objects
		//if ((textSamplerState = renderer->addSamplerState(NEAREST, CLAMP, CLAMP, CLAMP)) == SS_NONE) return;
		//if ((textBlendState = renderer->addBlendState(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, SRC_ALPHA, ONE_MINUS_SRC_ALPHA)) == BS_NONE) return;
		//if ((textDepthTest = renderer->addDepthState(false, false)) == DS_NONE) return;
		//if ((textRasterizerState = renderer->addRasterizerState(CULL_NONE, 0, 0.0f, SOLID, false, true)) == RS_NONE) return;

		// create FONS context
		FONSparams params;
		memset(&params, 0, sizeof(params));
		params.width = width_;
		params.height = height_;
		params.flags = (unsigned char)FONS_ZERO_TOPLEFT;
		params.renderCreate = fonsImplementationGenerateTexture;
		params.renderResize = fonsImplementationResizeTexture;
		params.renderUpdate = fonsImplementationModifyTexture;
		params.renderDraw = fonsImplementationRenderText; 
		params.renderDelete = fonsImplementationRemoveTexture;
		params.userPtr = this;

		fontStashContext = fonsCreateInternal(&params);
	}

	static int fonsImplementationGenerateTexture(void* userPtr, int width, int height);
	static int fonsImplementationResizeTexture(void* userPtr, int width, int height);
	static void fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data);
	static void fonsImplementationRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
	static void fonsImplementationRemoveTexture(void* userPtr);

	FONScontext* fontStashContext;

	Image img;
	Texture* tex;

	int width, height;
	UIRenderer* renderer;

	tinystl::vector<void*> fontBuffers;
};


Fontstash::Fontstash(UIRenderer* renderer, int width, int height)
{
	impl = conf_placement_new<_Impl_FontStash>(conf_calloc(1, sizeof(_Impl_FontStash)));
	impl->init(renderer, width, height);
}

Fontstash::~Fontstash()
{
	impl->~_Impl_FontStash();
	conf_free(impl);
}

int Fontstash::defineFont(const char* identification, const char* filename)
{
	FONScontext* fs=impl->fontStashContext;

	File file = File();
	file.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_Textures);
	unsigned bytes = file.GetSize();
	void* buffer = conf_malloc(bytes);
	file.Read(buffer, bytes);

	// add buffer to font buffers for cleanup
	impl->fontBuffers.emplace_back(buffer);

	return fonsAddFontMem(fs, identification, (unsigned char*)buffer, (int)bytes, 0);
}

int Fontstash::defineFont(const char* identification, const char* filename, FSRoot root)
{
	FONScontext* fs=impl->fontStashContext;

	File file = {};
	file.Open(filename, FileMode::FM_ReadBinary, root);
	unsigned bytes = file.GetSize();
	void* buffer = conf_malloc(bytes);
	file.Read(buffer, bytes);

	// add buffer to font buffers for cleanup
	impl->fontBuffers.emplace_back(buffer);

	return fonsAddFontMem(fs, identification, (unsigned char*)buffer, (int)bytes, 0);
}

int Fontstash::getFontID(const char* identification)
{
	FONScontext* fs=impl->fontStashContext;
	return fonsGetFontByName(fs, identification);
}

void Fontstash::drawText(const char* message, float x, float y, int fontID, unsigned int color/*=0xffffffff*/, float size/*=16.0f*/, float spacing/*=3.0f*/, float blur/*=0.0f*/)
{
	FONScontext* fs=impl->fontStashContext;
	fonsSetSize(fs, size);
	fonsSetFont(fs, fontID);
	fonsSetColor(fs, color);
	fonsSetSpacing(fs, spacing);
	fonsSetBlur(fs, blur);
	fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
	fonsDrawText(fs, x,y, message,NULL);
}

float Fontstash::measureText(float* out_bounds, const char* message, float x, float y, int fontID, unsigned int color/*=0xffffffff*/, float size/*=16.0f*/, float spacing/*=3.0f*/, float blur/*=0.0f*/)
{
	return measureText(out_bounds, message, (int)strlen(message), x, y, fontID, color, size, spacing, blur);
}

float Fontstash::measureText(float* out_bounds, const char* message, int messageLength, float x, float y, int fontID, unsigned int color/*=0xffffffff*/, float size/*=16.0f*/, float spacing/*=0.0f*/, float blur/*=0.0f*/)
{
	if(out_bounds == nullptr)
		return 0;

	FONScontext* fs=impl->fontStashContext;
	fonsSetSize(fs, size);
	fonsSetFont(fs, fontID);
	fonsSetColor(fs, color);
	fonsSetSpacing(fs, spacing);
	fonsSetBlur(fs, blur);
	fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
	return fonsTextBounds(fs, x, y, message, message+messageLength, out_bounds);

}

// --  FONS renderer implementation --
int _Impl_FontStash::fonsImplementationGenerateTexture(void* userPtr, int width, int height)
{
	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;
	ctx->width = width;
	ctx->height = height;

	Texture* oldTex = ctx->tex;


	// R8 mode
	//addTexture2d(ctx->renderer, width, height, SampleCount::SAMPLE_COUNT_1, ctx->img.getFormat(), ctx->img.GetMipMapCount(), NULL, false, TextureUsage::TEXTURE_USAGE_SAMPLED_IMAGE, &ctx->tex);
	ctx->tex = ctx->renderer->addTexture(&ctx->img, 0);
	
	// Create may be called multiple times, delete existing texture.
	// NOTE: deleting the texture afterwards seems to fix a driver bug on Intel where it would reuse the old contents of ctx->img, causing the texture not to update.
	if (oldTex)
	{
		ctx->renderer->removeTexture(oldTex);
		//removeTexture(ctx->renderer, oldTex);
	}

	return 1;
}

int _Impl_FontStash::fonsImplementationResizeTexture(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return fonsImplementationGenerateTexture(userPtr, width, height);
}

void _Impl_FontStash::fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data)
{
	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;

	// TODO: Update the GPU texture instead of changing the CPU texture and rebuilding it on GPU.
	ctx->img.loadFromMemoryXY(data + rect[0] + rect[1]*ctx->width, rect[0], rect[1], rect[2], rect[3], ctx->width);

	fonsImplementationGenerateTexture(userPtr, ctx->width, ctx->height);		// rebuild texture
}

void _Impl_FontStash::fonsImplementationRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	//make it static so that we don't need to create texVertex Arr every time
	static TexVertex vtx[FONS_VERTEX_COUNT];
	ASSERT(nverts <= (sizeof(vtx)/sizeof(vtx[0])));

	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;
	if (ctx->tex == NULL) return;

	// build vertices
	for(int impl=0; impl<nverts; impl++)
	{
		vtx[impl].position.setX(verts[impl*2+0]);
		vtx[impl].position.setY(verts[impl*2+1]);
		vtx[impl].texCoord.setX(tcoords[impl*2+0]);
		vtx[impl].texCoord.setY(tcoords[impl*2+1]);
	}

	// extract color
	ubyte* colorByte = (ubyte*)colors;
	float4 color;
	for(int i=0; i<4; i++)
		color[i] = ((float)colorByte[i])/255.0f;

	ctx->renderer->drawTexturedR8AsAlpha(PrimitiveTopology::PRIMITIVE_TOPO_TRI_LIST, vtx, nverts, ctx->tex, &color);
}

void _Impl_FontStash::fonsImplementationRemoveTexture(void* userPtr)
{
	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;
	if (ctx->tex)
	{
		ctx->renderer->removeTexture(ctx->tex);
		//removeTexture(ctx->renderer, ctx->tex);
		ctx->tex = NULL;
	}
}
