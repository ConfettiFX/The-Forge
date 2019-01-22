#pragma once

#include "../../OS/Math/MathTypes.h"
#include "../Interfaces/IFileSystem.h"

typedef struct Renderer           Renderer;
typedef struct Cmd                Cmd;
typedef struct Texture            Texture;
typedef struct GpuProfiler        GpuProfiler;
typedef struct TextDrawDesc       TextDrawDesc;
typedef struct GpuProfileDrawDesc GpuProfileDrawDesc;

void initDebugRendererInterface(Renderer* pRenderer, const char* pDebugFontPath, FSRoot root);
void removeDebugRendererInterface();

// adds font to DebugRenderer's Fontstash.
//
// Note that UIApp also has its own Fontstash container and its own DrawText() function.
//
uint32_t addDebugFont(const char* pFontPath, FSRoot root);

// draws the text using the font defined in DebugRenderer's Fontstash.
//
// This function is intended for debugging purposes. If the App wants to render
// text, it should be handled through the UIApp class and its interface instead of this one.
//
void drawDebugText(Cmd* pCmd, float x, float y, const char* pText, const TextDrawDesc* pDrawDesc);

void drawDebugGpuProfile(Cmd* pCmd, float x, float y, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc);
void drawDebugTexture(Cmd* pCmd, float x, float y, float w, float h, Texture* pTexture, float r, float g, float b);