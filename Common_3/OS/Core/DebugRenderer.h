#pragma once

#include "../../OS/Math/MathTypes.h"
#include "../Interfaces/IFileSystem.h"

typedef struct Renderer Renderer;
typedef struct Cmd Cmd;
typedef struct Texture Texture;
typedef struct GpuProfiler GpuProfiler;

typedef struct DebugTextDrawDesc
{
	DebugTextDrawDesc(uint32_t fontID = 0, uint32_t color = 0xffffffff, float size = 15.0f, float spacing = 0.0f, float fontBlur = 0.0f) :
		mFontID(fontID), mFontColor(color), mFontSize(size), mFontSpacing(spacing), mFontBlur(fontBlur) {}

	uint32_t mFontID;
	uint32_t mFontColor;
	float mFontSize;
	float mFontSpacing;
	float mFontBlur;
} DebugTextDrawDesc;

typedef struct GpuProfileDrawDesc
{
	float mChildIndent = 25.0f;
	float mHeightOffset = 25.0f;
	DebugTextDrawDesc mDrawDesc = DebugTextDrawDesc(0, 0xFF00CCAA, 15);
} GpuProfileDrawDesc;

void initDebugRendererInterface(Renderer* pRenderer, const char* pDebugFontPath, FSRoot root);
void removeDebugRendererInterface();

uint32_t addDebugFont(const char* pFontPath, FSRoot root);

void drawDebugText(Cmd* pCmd, float x, float y, const char* pText, const DebugTextDrawDesc* pDrawDesc);

//Use this if you need textRendering in WorldSpace 
void drawDebugText(Cmd* pCmd, const mat4& mProjView ,const mat4& mWorldMat,const char* pText, const DebugTextDrawDesc* pDrawDesc);
void drawDebugGpuProfile(Cmd* pCmd, float x, float y, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc);
void drawDebugTexture(Cmd* pCmd, float x, float y, float w, float h, Texture* pTexture, float r, float g, float b);