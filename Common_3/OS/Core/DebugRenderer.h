#pragma once

#include "../../OS/Math/MathTypes.h"
#include "../Interfaces/IFileSystem.h"

typedef struct Renderer Renderer;
typedef struct Cmd Cmd;
typedef struct Texture Texture;
typedef struct GpuProfiler GpuProfiler;
typedef struct TextDrawDesc TextDrawDesc;
typedef struct GpuProfileDrawDesc GpuProfileDrawDesc;

void initDebugRendererInterface(Renderer* pRenderer, const char* pDebugFontPath, FSRoot root);
void removeDebugRendererInterface();

uint32_t addDebugFont(const char* pFontPath, FSRoot root);

void drawDebugText(Cmd* pCmd, float x, float y, const char* pText, const TextDrawDesc* pDrawDesc);

//Use this if you need textRendering in WorldSpace
void drawDebugText(Cmd* pCmd, const mat4& mProjView ,const mat4& mWorldMat,const char* pText, const TextDrawDesc* pDrawDesc);
void drawDebugGpuProfile(Cmd* pCmd, float x, float y, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc);
void drawDebugTexture(Cmd* pCmd, float x, float y, float w, float h, Texture* pTexture, float r, float g, float b);