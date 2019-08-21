#pragma once

#include "../../../OS/Interfaces/IProfiler.h"

typedef uint64_t ProfileToken;
typedef uint16_t ProfileGroupId;


#if 0 == PROFILE_ENABLED 
#define ProfileMouseButton(foo, bar) do{}while(0)
#define ProfileMousePosition(foo, bar, z) do{}while(0)
#define ProfileModKey(key) do{}while(0)
#define ProfileDraw(cmd, width, height) do{}while(0)
#define ProfileIsDrawing() 0
#define ProfileToggleDisplayMode() do{}while(0)
#define ProfileSetDisplayMode(f) do{}while(0)
#else

#ifndef PROFILE_DRAWCURSOR
#define PROFILE_DRAWCURSOR 0
#endif

#ifndef PROFILE_DETAILED_BAR_NAMES
#define PROFILE_DETAILED_BAR_NAMES 1
#endif

#ifndef PROFILE_TEXT_WIDTH
#define PROFILE_TEXT_WIDTH static_cast<uint32_t>(7 * getDpiScale().x)
#endif

#ifndef PROFILE_TEXT_HEIGHT
#define PROFILE_TEXT_HEIGHT static_cast<uint32_t>(16 * getDpiScale().y)
#endif

#ifndef PROFILE_MIN_WIDTH_TO_DISPLAY_WHOLE_MENU
#define PROFILE_MIN_WIDTH_TO_DISPLAY_WHOLE_MENU 667 * getDpiScale().x
#endif

#ifndef PROFILE_DETAILED_BAR_HEIGHT
#define PROFILE_DETAILED_BAR_HEIGHT 12
#endif

#ifndef PROFILE_DETAILED_CONTEXT_SWITCH_HEIGHT
#define PROFILE_DETAILED_CONTEXT_SWITCH_HEIGHT 7
#endif

#ifndef PROFILE_GRAPH_WIDTH
#define PROFILE_GRAPH_WIDTH 256
#endif

#ifndef PROFILE_GRAPH_HEIGHT
#define PROFILE_GRAPH_HEIGHT 256
#endif

#ifndef PROFILE_BORDER_SIZE 
#define PROFILE_BORDER_SIZE 1
#endif

#ifndef PROFILE_HELP_LEFT
#define PROFILE_HELP_LEFT "Left-Click"
#endif

#ifndef PROFILE_HELP_RIGHT
#define PROFILE_HELP_RIGHT "Right-Click"
#endif

#ifndef PROFILE_HELP_MOD
#define PROFILE_HELP_MOD "Mod"
#endif

#ifndef PROFILE_BAR_WIDTH
#define PROFILE_BAR_WIDTH 100
#endif

#ifndef PROFILE_CUSTOM_MAX
#define PROFILE_CUSTOM_MAX 8 
#endif

#ifndef PROFILE_CUSTOM_MAX_TIMERS
#define PROFILE_CUSTOM_MAX_TIMERS 64
#endif

#ifndef PROFILE_CUSTOM_PADDING
#define PROFILE_CUSTOM_PADDING 12
#endif

#define PROFILE_FRAME_HISTORY_HEIGHT 50
#define PROFILE_FRAME_HISTORY_WIDTH 7
#define PROFILE_FRAME_HISTORY_COLOR_CPU 0xffff7f27 //255 127 39
#define PROFILE_FRAME_HISTORY_COLOR_GPU 0xff37a0ee //55 160 238
#define PROFILE_FRAME_HISTORY_COLOR_HIGHTLIGHT 0x7733bb44
#define PROFILE_FRAME_COLOR_HIGHTLIGHT 0x20009900
#define PROFILE_FRAME_COLOR_HIGHTLIGHT_GPU 0x20996600
#define PROFILE_NUM_FRAMES (PROFILE_MAX_FRAME_HISTORY - (PROFILE_GPU_FRAME_DELAY+1))

#define PROFILE_TOOLTIP_MAX_STRINGS (32 + PROFILE_MAX_GROUPS*2)
#define PROFILE_TOOLTIP_STRING_BUFFER_SIZE (4*1024)
#define PROFILE_TOOLTIP_MAX_LOCKED 3

#define PROFILE_COUNTER_INDENT 4
#define PROFILE_COUNTER_WIDTH 100

enum
{
	PROFILE_CUSTOM_BARS = 0x1,
	PROFILE_CUSTOM_BAR_SOURCE_MAX = 0x2,
	PROFILE_CUSTOM_BAR_SOURCE_AVG = 0,
	PROFILE_CUSTOM_STACK = 0x4,
	PROFILE_CUSTOM_STACK_SOURCE_MAX = 0x8,
	PROFILE_CUSTOM_STACK_SOURCE_AVG = 0,
};

enum ProfileBoxType
{
	ProfileBoxTypeBar,
	ProfileBoxTypeFlat,
};

struct Cmd;

void ProfileDraw(Cmd* pCmd); //! call if drawing microprofilers
bool ProfileIsDrawing();
void ProfileToggleGraph(ProfileToken nToken);
bool ProfileDrawGraph(uint32_t nScreenWidth, uint32_t nScreenHeight);
void ProfileToggleDisplayMode(); //switch between off, bars, detailed
void ProfileSetDisplayMode(int); //switch between off, bars, detailed
void ProfileClearGraph();
void ProfileMousePosition(uint32_t nX, uint32_t nY, int nWheelDelta);
void ProfileModKey(uint32_t nKeyState);
void ProfileMouseButton(uint32_t nLeft, uint32_t nRight);
void ProfileDrawLineVertical(int nX, int nTop, int nBottom, uint32_t nColor);
void ProfileDrawLineHorizontal(int nLeft, int nRight, int nY, uint32_t nColor);
void ProfileLoadPreset(const char* pSuffix);
void ProfileSavePreset(const char* pSuffix);

void ProfileDrawText(int nX, int nY, uint32_t nColor, const char* pText, uint32_t nNumCharacters);
void ProfileDrawBox(int nX, int nY, int nX1, int nY1, uint32_t nColor, ProfileBoxType = ProfileBoxTypeFlat);
void ProfileDrawLine2D(uint32_t nVertices, float* pVertices, uint32_t nColor);
void ProfileDumpTimers();

//void ProfileInitUI();

void ProfileCustomGroupToggle(const char* pCustomName);
void ProfileCustomGroupEnable(const char* pCustomName);
void ProfileCustomGroupEnable(uint32_t nIndex);
void ProfileCustomGroupDisable();
void ProfileCustomGroup(const char* pCustomName, uint32_t nMaxTimers, uint32_t nAggregateFlip, float fReferenceTime, uint32_t nFlags);
void ProfileCustomGroupAddTimer(const char* pCustomName, const char* pGroup, const char* pTimer);

bool ProfileIsDetailed();

#endif
