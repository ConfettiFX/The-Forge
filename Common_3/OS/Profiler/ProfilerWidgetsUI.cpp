/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include "../../../Middleware_3/UI/AppUI.h"
#include  "../../Renderer/IRenderer.h"
#include "../Interfaces/IProfiler.h"

#if 0 == PROFILE_ENABLED
void initProfilerUI(UIApp* uiApp, int32_t width, int32_t height) {}
void exitProfilerUI() {}
void cmdDrawProfilerUI() {}
float2 cmdDrawGpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, ProfileToken nProfileToken, const TextDrawDesc* pDrawDesc) {}
float2 cmdDrawCpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, const TextDrawDesc* pDrawDesc) {}
void toggleProfilerUI() {}
#else

#include "ProfilerBase.h"
#include "GpuProfiler.h"

#define MAX_DETAILED_TIMERS_DRAW 2048
#define MAX_TIME_STR_LEN 32
#define MAX_TITLE_STR_LEN 256
#define FRAME_HISTORY_LEN 128
#define CRITICAL_COLOR_THRESHOLD 0.5f
#define WARNING_COLOR_THRESHOLD 0.3f
#define MAX_TOOLTIP_STR_LEN 256
#define PROFILER_WINDOW_X 0.f
#define PROFILER_WINDOW_Y 0.f

// Must be initialized with application's ui before drawing.
UIApp* pAppUIRef = 0;
GuiComponent* pWidgetGuiComponent = 0;
GuiComponent* pMenuGuiComponent = 0;
float gGuiTransparency = 0.1f;

bool gProfilerWidgetUIEnabled = false;
// To re-alloc all dynamic data when screen resize unloads it.
bool gUnloaded = true;
float2 gCurrWindowSize;

GpuProfiler* getGpuProfiler(ProfileToken nProfileToken);

typedef struct GpuProfileDrawDesc
{
	float        mChildIndent = 25.0f;
	float        mHeightOffset = 5.0f;
} GpuProfileDrawDesc;

static GpuProfileDrawDesc gDefaultGpuProfileDrawDesc = {};
static TextDrawDesc       gDefaultTextDrawDesc = TextDrawDesc(0, 0xff00ffff, 15);

struct  ProfileDetailedModeTime
{
	uint32_t mTimerInfoIndex;
	float mStartTime;
	float mEndTime;
	uint32_t mFrameNum;
	float mCurrFrameTime;
	char mThreadName[64];
};

struct ProfileDetailedModeFrame
{
	float mFrameTime;
	eastl::vector<ProfileDetailedModeTime> mTimers;
};

struct ProfileDetailedModeTooltip
{
	ProfileDetailedModeTooltip() {} //-V730
	ProfileDetailedModeTooltip(const ProfileDetailedModeTime& detailedLogTimer, IWidget* widget) :
		mTimer(detailedLogTimer),
		mWidget(widget) {}

	ProfileDetailedModeTime mTimer;
	IWidget* mWidget;
};

enum ProfileModes
{
	PROFILE_MODE_TIMER,
	PROFILE_MODE_DETAILED,
	PROFILE_MODE_PLOT,
	PROFILE_MODE_MAX
};

enum ProfileAggregateFrames
{
	PROFILE_AGGREGATE_NUM_INF,
	PROFILE_AGGREGATE_NUM_10,
	PROFILE_AGGREGATE_NUM_20,
	PROFILE_AGGREGATE_NUM_30,
	PROFILE_AGGREGATE_NUM_60,
	PROFILE_AGGREGATE_NUM_120,
	PROFILE_AGGREGATE_FRAMES_MAX
};

enum ProfileDumpFramesFile
{
	PROFILE_DUMPFILE_NUM_32,
	PROFILE_DUMPFILE_NUM_64,
	PROFILE_DUMPFILE_NUM_128,
	PROFILE_DUMPFILE_NUM_256,
	PROFILE_DUMPFILE_NUM_512,
	PROFILE_DUMPFILE_MAX
};

enum ProfileDumpFramesDetailedMode
{
	PROFILE_DUMPFRAME_NUM_2,
	PROFILE_DUMPFRAME_NUM_4,
	PROFILE_DUMPFRAME_NUM_8,
	PROFILE_DUMPFRAME_NUM_16,
	PROFILE_DUMPFRAME_MAX
};

enum ProfileReferenceTimes
{
	PROFILE_REFTIME_MS_1,
	PROFILE_REFTIME_MS_2,
	PROFILE_REFTIME_MS_5,
	PROFILE_REFTIME_MS_10,
	PROFILE_REFTIME_MS_15,
	PROFILE_REFTIME_MS_20,
	PROFILE_REFTIME_MS_33,
	PROFILE_REFTIME_MS_66,
	PROFILE_REFTIME_MS_100,
	PROFILE_REFTIME_MS_250,
	PROFILE_REFTIME_MS_500,
	PROFILE_REFTIME_MS_1000,
	PROFILE_REFTIME_MAX
};

const char* pProfileModesNames[] = {
	"Timer",
	"Detailed",
	"Plot"
};

const char* pAggregateFramesNames[] =
{
	"Infinite",
	"10",
	"20",
	"30",
	"60",
	"120"
};

const char* pReferenceTimesNames[] =
{
	"1ms",
	"2ms",
	"5ms",
	"10ms",
	"15ms",
	"20ms",
	"33ms",
	"66ms",
	"100ms",
	"250ms",
	"500ms",
	"1000ms"
};

const char* pDumpFramesToFileNames[] =
{
	"32",
	"64",
	"128",
	"256",
	"512"
};

const char* pDumpFramesDetailedViewNames[] =
{
	"2",
	"4",
	"8",
	"16",
};

const uint32_t gProfileModesValues[] = {
	PROFILE_MODE_TIMER,
	PROFILE_MODE_DETAILED,
	PROFILE_MODE_PLOT,
};

const uint32_t gAggregateFramesValues[] = {
	PROFILE_AGGREGATE_NUM_INF,
	PROFILE_AGGREGATE_NUM_10,
	PROFILE_AGGREGATE_NUM_20,
	PROFILE_AGGREGATE_NUM_30,
	PROFILE_AGGREGATE_NUM_60,
	PROFILE_AGGREGATE_NUM_120
};

const uint32_t gReferenceTimesValues[] = {
	PROFILE_REFTIME_MS_1,
	PROFILE_REFTIME_MS_2,
	PROFILE_REFTIME_MS_5,
	PROFILE_REFTIME_MS_10,
	PROFILE_REFTIME_MS_15,
	PROFILE_REFTIME_MS_20,
	PROFILE_REFTIME_MS_33,
	PROFILE_REFTIME_MS_66,
	PROFILE_REFTIME_MS_100,
	PROFILE_REFTIME_MS_250,
	PROFILE_REFTIME_MS_500,
	PROFILE_REFTIME_MS_1000
};

const uint32_t gDumpFramesToFileValues[] = {
	PROFILE_DUMPFILE_NUM_32,
	PROFILE_DUMPFILE_NUM_64,
	PROFILE_DUMPFILE_NUM_128,
	PROFILE_DUMPFILE_NUM_256,
	PROFILE_DUMPFILE_NUM_512
};

const uint32_t gDumpFramesDetailedValues[] = {
	PROFILE_DUMPFRAME_NUM_2,
	PROFILE_DUMPFRAME_NUM_4,
	PROFILE_DUMPFRAME_NUM_8,
	PROFILE_DUMPFRAME_NUM_16,
};


// Common top-menu data.
ProfileModes gProfileMode = PROFILE_MODE_TIMER;
ProfileModes gPrevProfileMode = PROFILE_MODE_TIMER;
ProfileAggregateFrames gAggregateFrames = PROFILE_AGGREGATE_NUM_60;
ProfileReferenceTimes gReferenceTime = PROFILE_REFTIME_MS_15;
ProfileDumpFramesFile gDumpFramesToFile = PROFILE_DUMPFILE_NUM_32;
ProfileDumpFramesDetailedMode gDumpFramesDetailedMode = PROFILE_DUMPFRAME_NUM_4;
bool gProfilerPaused = false;
float gMinPlotReferenceTime = 0.f;
float gFrameTime = 0.f;
float gFrameTimeData[FRAME_HISTORY_LEN] = { 0.f };
float gGPUFrameTime[PROFILE_MAX_GROUPS];
float gGPUFrameTimeData[PROFILE_MAX_GROUPS][FRAME_HISTORY_LEN] = { { 0.0f } };
char gFrameTimerTitle[MAX_TITLE_STR_LEN] = "FrameTimer";
char gGPUTimerTitle[PROFILE_MAX_GROUPS][MAX_TITLE_STR_LEN]{};
float2 gHistogramSize;

// Timer mode data.
uint32_t gTotalGroups = 0;
uint32_t gTotalTimers = 0;
eastl::vector<eastl::vector<IWidget*>> gWidgetTable;
eastl::vector<eastl::vector<char*>> gTimerData;
eastl::vector<eastl::vector<float4*>> gTimerColorData;

// Timer mode color coding.
float4 gCriticalColor = float4(1.f, 0.f, 0.f, 1.f);
float4 gWarningColor = float4(1.f, 1.f, 0.f, 1.f);
float4 gNormalColor = float4(1.f, 1.f, 1.f, 1.f);
float4 gFernGreenColor = float4(0.31f, 0.87f, 0.26f, 1.0f);
float4 gLilacColor = float4(0.f, 1.f, 1.f, 1.0f);

// Plot mode data.
struct PlotModeData
{
	float2* mTimeData;
	uint32_t mTimerInfo;
	bool mEnabled;
};

eastl::vector<PlotModeData> gPlotModeData;
eastl::vector<IWidget*> gPlotModeWidgets;
bool gUpdatePlotModeGUI = false;

// Detailed mode data.
eastl::vector<ProfileDetailedModeFrame> gDetailedModeDump;
eastl::vector<ProfileDetailedModeTooltip> gDetailedModeTooltips;
eastl::vector<IWidget*> gDetailedModeWidgets;
bool gDumpFramesNow = true;
bool gShowTooltip = true;
char gTooltipData[MAX_TOOLTIP_STR_LEN] = "0";

// Utility functions.
float profileUtilRoundFloatByPrecision(const float& number, float precision = 2.f)
{
	return roundf(number * powf(10.f, precision)) / powf(10.f, precision);
}

float profileUtilReferenceTimeFromEnum(const ProfileReferenceTimes& referenceTime)
{
	switch (referenceTime)
	{
	case PROFILE_REFTIME_MS_1:
		return 1.f;
	case PROFILE_REFTIME_MS_2:
		return 2.f;
	case PROFILE_REFTIME_MS_5:
		return 5.f;
	case PROFILE_REFTIME_MS_10:
		return 10.f;
	case PROFILE_REFTIME_MS_15:
		return 15.f;
	case PROFILE_REFTIME_MS_20:
		return 20.f;
	case PROFILE_REFTIME_MS_33:
		return 33.f;
	case PROFILE_REFTIME_MS_66:
		return 66.f;
	case PROFILE_REFTIME_MS_100:
		return 100.f;
	case PROFILE_REFTIME_MS_250:
		return 250.f;
	case PROFILE_REFTIME_MS_500:
		return 500.f;
	case PROFILE_REFTIME_MS_1000:
		return 1000.f;
	default:
		return 15.f;
	}
}

uint32_t profileUtilAggregateFramesFromEnum(const ProfileAggregateFrames& aggregateFrames)
{
	switch (aggregateFrames)
	{
	case PROFILE_AGGREGATE_NUM_INF:
		return 0;
	case PROFILE_AGGREGATE_NUM_10:
		return 10;
	case PROFILE_AGGREGATE_NUM_20:
		return 20;
	case PROFILE_AGGREGATE_NUM_30:
		return 30;
	case PROFILE_AGGREGATE_NUM_60:
		return 60;
	case PROFILE_AGGREGATE_NUM_120:
		return 120;
	default:
		return 60;
	}
}

uint32_t profileUtilDumpFramesFromFileEnum(const ProfileDumpFramesFile& dumpFrames)
{
	switch (dumpFrames)
	{
	case PROFILE_DUMPFILE_NUM_32:
		return 32;
	case  PROFILE_DUMPFILE_NUM_64:
		return 64;
	case PROFILE_DUMPFILE_NUM_128:
		return 128;
	case PROFILE_DUMPFILE_NUM_256:
		return 256;
	case PROFILE_DUMPFILE_NUM_512:
		return 512;
	default:
		return 32;
	}
}

uint32_t profileUtilDumpFramesDetailedModeEnum(const ProfileDumpFramesDetailedMode& dumpFrames)
{
	switch (dumpFrames)
	{
	case PROFILE_DUMPFRAME_NUM_2:
		return 2;
	case PROFILE_DUMPFRAME_NUM_4:
		return 4;
	case PROFILE_DUMPFRAME_NUM_8:
		return 8;
	case PROFILE_DUMPFRAME_NUM_16:
		return 16;
	default:
		return 8;
	}
}

void profileUtilTrimFloatString(char* numericString, char* result, uint32_t precision = 2u)
{
	char* delim = strchr(numericString, '.');
	ASSERT(delim);
	size_t delimPos = delim - numericString;
	strncpy(result, numericString, delimPos + 1 + precision);
}

int profileUtilGroupIndexFromName(Profile& S, const char* groupName)
{
	for (uint32_t i = 0; i < S.nGroupCount; ++i)
	{
		if (strcmp(S.GroupInfo[i].pName, groupName) == 0)
		{
			return S.GroupInfo[i].nGroupIndex;
		}
	}
	return 0;
}

vec2 profileUtilCalcWindowSize(int32_t width, int32_t height)
{
	return vec2((float)(width >> 1), (float)(height >> 1));
}

// Callback functions.
void profileCallbkDumpFramesToFile()
{
	ASSERT(pAppUIRef);
	dumpProfileData(pAppUIRef->pImpl->pRenderer, pAppUIRef->pImpl->pRenderer->pName, profileUtilDumpFramesFromFileEnum(gDumpFramesToFile));
}

void profileCallbkDumpFrames()
{
	// Dump fresh frames to detailed mode and clear any old data.
	gDumpFramesNow = true;
	gDetailedModeDump.clear();
}

void profileCallbkPauseProfiler()
{
	ProfileTogglePause();
}

void ProfileCallbkReferenceTimeUpdated()
{
	if (gProfileMode == PROFILE_MODE_PLOT)
	{
		gUpdatePlotModeGUI = true;
	}
}

// Detailed mode functions.
void profileDrawDetailedModeGrid(float startHeightPixels, float startWidthPixels, float interLineDistance, uint32_t totalLines, float lineHeight)
{
	float x = startWidthPixels;

	for (uint32_t i = 0; i < totalLines; ++i)
	{
		// Alternate between dark lines as whole numbers and light lines decimal mid.
		uint32_t color = 0x64646464;

		if (i % 2 != 0)
		{
			color = 0x32323232;
		}

		DrawLineWidget lineWidget;
		lineWidget.mPos1 = float2(x, startHeightPixels);
		lineWidget.mPos2 = float2(x, lineHeight);
		lineWidget.mColor = color;
		lineWidget.mAddItem = false;
		gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "", &lineWidget, WIDGET_TYPE_DRAW_LINE));
		addWidgetLua(gDetailedModeWidgets.back());
		x += interLineDistance;
	}
}

void profileGetDetailedModeFrameTimeBetweenTicks(int64_t nTicks, int64_t nTicksEnd, int32_t nLogIndex, uint32_t* nFrameBegin, uint32_t* nFrameEnd)
{
	Profile& S = *ProfileGet();
	ASSERT(nLogIndex >= 0 && S.Pool[nLogIndex]);

	bool bGpu = S.Pool[nLogIndex]->nGpu != 0;
	uint32_t nPut = tfrg_atomic32_load_relaxed(&S.Pool[nLogIndex]->nPut);

	uint32_t nBegin = S.nFrameCurrent;

	for (uint32_t i = 0; i < PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY; ++i)
	{
		uint32_t nFrame = (S.nFrameCurrent + PROFILE_MAX_FRAME_HISTORY - i) % PROFILE_MAX_FRAME_HISTORY;
		uint32_t nCurrStart = S.Frames[nBegin].nLogStart[nLogIndex];
		uint32_t nPrevStart = S.Frames[nFrame].nLogStart[nLogIndex];
		bool bOverflow = (nPrevStart <= nCurrStart) ? (nPut >= nPrevStart && nPut < nCurrStart) : (nPut < nCurrStart || nPut >= nPrevStart);
		if (bOverflow)
			break;

		nBegin = nFrame;
		if ((bGpu ? S.Frames[nBegin].nFrameStartGpu[nLogIndex] : S.Frames[nBegin].nFrameStartCpu) <= nTicks)
			break;
	}

	uint32_t nEnd = nBegin;

	while (nEnd != S.nFrameCurrent)
	{
		nEnd = (nEnd + 1) % PROFILE_MAX_FRAME_HISTORY;
		if ((bGpu ? S.Frames[nEnd].nFrameStartGpu[nLogIndex] : S.Frames[nEnd].nFrameStartCpu) >= nTicksEnd)
			break;
	}

	*nFrameBegin = nBegin;
	*nFrameEnd = nEnd;
}

/// Get data for detailed mode UI from the microprofiler.
void profileUpdateDetailedModeData(Profile& S)
{
	int64_t nTicksPerSecondCpu = ProfileTicksPerSecondCpu();
	float fToMsCpu = ProfileTickToMsMultiplier(nTicksPerSecondCpu);
	float fDetailedRange = profileUtilReferenceTimeFromEnum(gReferenceTime);
	int64_t nBaseTicksCpu = S.Frames[S.nFrameCurrent].nFrameStartCpu;
	int64_t nBaseTicksEndCpu = nBaseTicksCpu + ProfileMsToTick(fDetailedRange, nTicksPerSecondCpu);

	uint64_t nActiveGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;
	ProfileDetailedModeFrame frameLog;
	frameLog.mFrameTime = fDetailedRange;

	// Go over each active thread to profile.
	for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
	{
		ProfileThreadLog* pLog = S.Pool[i];
		if (!pLog)
			continue;

		bool bGpu = pLog->nGpu != 0;
		float fToMs = bGpu ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(pLog->nGpuToken)) : fToMsCpu;
		int64_t nBaseTicks = bGpu ? S.Frames[S.nFrameCurrent].nFrameStartGpu[i] : nBaseTicksCpu;
		int64_t nBaseTicksEnd = bGpu ? nBaseTicks + ProfileMsToTick(fDetailedRange, (int64_t)getGpuProfileTicksPerSecond(pLog->nGpuToken)) : nBaseTicksEndCpu;
		int64_t nGapTime = 0;
		uint32_t nLogFrameBegin, nLogFrameEnd;
		profileGetDetailedModeFrameTimeBetweenTicks(nBaseTicks - nGapTime, nBaseTicksEnd + nGapTime, i, &nLogFrameBegin, &nLogFrameEnd);

		uint32_t nGet = S.Frames[nLogFrameBegin].nLogStart[i];
		uint32_t nPut = nLogFrameEnd == S.nFrameCurrent ? tfrg_atomic32_load_relaxed(&pLog->nPut) : S.Frames[nLogFrameEnd].nLogStart[i];
		if (nPut == nGet)
			continue;

		uint32_t nRange[2][2] = { {0, 0}, {0, 0} };
		ProfileGetRange(nPut, nGet, nRange);

		uint32_t nStack[PROFILE_STACK_MAX];
		uint32_t nStackPos = 0;

		{
			uint32_t nStart = nRange[0][0];
			uint32_t nEnd = nRange[0][1];

			for (uint32_t k = nStart; k < nEnd; ++k)
			{
				ProfileLogEntry* pEntry = pLog->Log + k;
				uint64_t nType = ProfileLogType(*pEntry);

				if (P_LOG_ENTER == nType)
				{
					ASSERT(nStackPos < PROFILE_STACK_MAX);
					nStack[nStackPos++] = k;
				}
				else if (P_LOG_LEAVE == nType)
				{
					if (0 == nStackPos)
					{
						continue;
					}

					ProfileLogEntry* pEntryEnter = pLog->Log + nStack[nStackPos - 1];

					// Make sure the entry timer points belong to the same event index.
					if (ProfileLogTimerIndex(*pEntryEnter) != ProfileLogTimerIndex(*pEntry))
					{
						continue;
					}

					int64_t nTickStart = ProfileLogGetTick(*pEntryEnter);
					int64_t nTickEnd = ProfileLogGetTick(*pEntry);
					uint64_t nTimerIndex = ProfileLogTimerIndex(*pEntry);

					// Make sure the timer is in the groups being processed.
					if (!(nActiveGroup & (1ull << S.TimerInfo[nTimerIndex].nGroupIndex)))
					{
						nStackPos--;
						continue;
					}

					float fMsStart = fToMs * ProfileLogTickDifference(nBaseTicks, nTickStart);
					float fMsEnd = fToMs * ProfileLogTickDifference(nBaseTicks, nTickEnd);
					// Add relevant profile data for the frame to be drawn in UI.
					if (fMsEnd < fDetailedRange && fMsStart > 0 && abs(fMsStart - fMsEnd) > 0.0001f)
					{
						ProfileDetailedModeTime timerLog;
						timerLog.mTimerInfoIndex = (uint32_t)nTimerIndex;
						timerLog.mStartTime = fMsStart;
						timerLog.mEndTime = fMsEnd;
						strncpy(timerLog.mThreadName, pLog->ThreadName, 64);
						timerLog.mFrameNum = (uint32_t)gDetailedModeDump.size() + 1;
						frameLog.mTimers.push_back(timerLog);
					}

					nStackPos--;

					if (0 == nStackPos && ProfileLogTickDifference(nTickEnd, nBaseTicksEnd) < 0)
					{
						break;
					}
				}
			}
		}
	}

	// Push detailed dump logs.
	gDetailedModeDump.push_back(frameLog);
}

/// Get data to display as a tooltip in detailed mode. To enable tooltop just hover over any text in detailed mode.
void profileUpdateDetailedModeTooltip(Profile& S)
{
	gShowTooltip = false;

	for (uint32_t i = 0; i < gDetailedModeTooltips.size(); ++i)
	{
		// Need tooltip.
		if (gDetailedModeTooltips[i].mWidget->mHovered)
		{
			// Update tooltip data for this frame.
			memset(gTooltipData, 0, MAX_TOOLTIP_STR_LEN);
			const ProfileDetailedModeTime& timer = gDetailedModeTooltips[i].mTimer;
			const ProfileTimerInfo& timerInfo = S.TimerInfo[timer.mTimerInfoIndex];

			strcat(gTooltipData, timerInfo.pName);
			strcat(gTooltipData, "\n------------------------------\n");

			strcat(gTooltipData, "Group Name: ");
			strcat(gTooltipData, S.GroupInfo[timerInfo.nGroupIndex].pName);
			strcat(gTooltipData, "\n");

			strcat(gTooltipData, "Thread: ");
			strcat(gTooltipData, timer.mThreadName);
			strcat(gTooltipData, "\n");

			char buffer[30]{};

			sprintf(buffer, "%u", timer.mFrameNum);
			strcat(gTooltipData, "Frame Number: ");
			strcat(gTooltipData, buffer);
			strcat(gTooltipData, "\n");

			sprintf(buffer, "%f", timer.mCurrFrameTime + timer.mStartTime);
			strcat(gTooltipData, "Start Time(ms): ");
			strcat(gTooltipData, buffer);
			strcat(gTooltipData, "\n");

			sprintf(buffer, "%f", timer.mCurrFrameTime + timer.mEndTime);
			strcat(gTooltipData, "End Time(ms)  : ");
			strcat(gTooltipData, buffer);
			strcat(gTooltipData, "\n");

			sprintf(buffer, "%f", timer.mEndTime - timer.mStartTime);
			strcat(gTooltipData, "Total Time(ms): ");
			strcat(gTooltipData, buffer);
			strcat(gTooltipData, "\n");

			gShowTooltip = true;

		}
	}
}

/// Main functionality to draw the horizontal UI for detailed mode.
void profileDrawDetailedMode(Profile& S)
{
	// Remove any tooltip widgets.
	for (uint32_t i = 0; i < gDetailedModeTooltips.size(); ++i)
	{
		removeGuiWidget(pWidgetGuiComponent, gDetailedModeTooltips[i].mWidget);
	}

	// Remove all other widgets.
	for (uint32_t i = 0; i < gDetailedModeWidgets.size(); ++i)
	{
		removeGuiWidget(pWidgetGuiComponent, gDetailedModeWidgets[i]);
	}

	gDetailedModeTooltips.clear();
	gDetailedModeWidgets.clear();

	const float startHeightPixels = gCurrWindowSize.y * 0.2f;
	const float startWidthPixels = gCurrWindowSize.x * 0.02f;
	const float interTimerHeight = gCurrWindowSize.y * 0.01f;
	const float msToPixels = gCurrWindowSize.y * 0.1f;
	const float timerHeight = gCurrWindowSize.y * 0.025f;
	float frameTime = 0.0f;
	float frameHeight = 0.f;
	uint32_t timerCount = 0;
	// Draw all frames in the dump into a timeline.
	for (uint32_t frameIndex = 0; frameIndex < gDetailedModeDump.size(); ++frameIndex)
	{
		ProfileDetailedModeFrame& frameToDraw = gDetailedModeDump[frameIndex];
		if (timerCount > MAX_DETAILED_TIMERS_DRAW)
		{
			Log::Write(LogLevel::eWARNING, __FILE__, __LINE__, "Reached maximum amount of drawable detailed timers");
			break;
		}

		// Draw all timers as rectangles on the timeline.
		for (uint32_t i = 0; i < (uint32_t)frameToDraw.mTimers.size(); ++i)
		{
			if (timerCount > MAX_DETAILED_TIMERS_DRAW)
				break;
			ProfileDetailedModeTime& timer = frameToDraw.mTimers[i];
			timer.mCurrFrameTime = frameTime;
			ProfileTimerInfo& timerInfo = S.TimerInfo[timer.mTimerInfoIndex];
			// timer.mFrameNum = frameIndex + 1;
			float height = startHeightPixels + gCurrWindowSize.y * 0.035f + (interTimerHeight + timerHeight) * timer.mTimerInfoIndex;

			// Overall start and end times.
			float startTime = frameTime + timer.mStartTime;
			float endTime = frameTime + timer.mEndTime;

			float2 pos = float2(startWidthPixels + startTime * msToPixels, height);
			float2 scale = float2(max((endTime - startTime), 0.05f) * msToPixels, timerHeight);
			uint32_t color = (timerInfo.nColor | 0x7D000000);

			// Draw the timer bar and text.
			FilledRectWidget rectWidget;
			rectWidget.mPos = pos;
			rectWidget.mScale = scale;
			rectWidget.mColor = color;
			gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, timerInfo.pName, &rectWidget, WIDGET_TYPE_FILLED_RECT));
			addWidgetLua(gDetailedModeWidgets.back());

			// Add data to draw a tooltip for this timer.
			DrawTextWidget textWidget;
			textWidget.mPos = pos + float2(scale.x, 0.f);
			textWidget.mColor = 0xFFFFFFFF;
			gDetailedModeTooltips.push_back(ProfileDetailedModeTooltip(timer, addGuiWidget(pWidgetGuiComponent, timerInfo.pName, &textWidget, WIDGET_TYPE_DRAW_TEXT)));
			addWidgetLua(gDetailedModeTooltips.back().mWidget);
			frameHeight = max(frameHeight, height * 1.2f);
			++timerCount;
		}
		frameTime += frameToDraw.mFrameTime;
	}

	// Add timeline.
	for (uint32_t i = 0; i < (uint32_t)ceilf(frameTime); ++i)
	{
		char indexStr[10];
		sprintf(indexStr, "%u", i);

		DrawTextWidget textWidget;
		textWidget.mPos = float2(startWidthPixels + i * msToPixels, startHeightPixels - gCurrWindowSize.y * 0.03f);
		textWidget.mColor = 0xFFFFFFFF;
		gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, indexStr, &textWidget, WIDGET_TYPE_DRAW_TEXT));
		addWidgetLua(gDetailedModeWidgets.back());
	}

	// Backgroud vertical lines.
	profileDrawDetailedModeGrid(startHeightPixels, startWidthPixels, msToPixels / 2.f, (uint32_t)ceilf(frameTime * 2.f), frameHeight);
	// Horizontal Separator lines. 

	DrawLineWidget topLine;
	topLine.mPos1 = float2(startWidthPixels, startHeightPixels);
	topLine.mPos2 = float2(ceilf(frameTime) * msToPixels, startHeightPixels);
	topLine.mColor = 0x64646464;
	topLine.mAddItem = true;
	gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "TopSeparator", &topLine, WIDGET_TYPE_DRAW_LINE));
	addWidgetLua(gDetailedModeWidgets.back());

	DrawLineWidget bottomLine;
	bottomLine.mPos1 = float2(startWidthPixels, frameHeight);
	bottomLine.mPos2 = float2(ceilf(frameTime) * msToPixels, frameHeight);
	bottomLine.mColor = 0x64646464;
	bottomLine.mAddItem = true;
	gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "BottomSeparator", &bottomLine, WIDGET_TYPE_DRAW_LINE));
	addWidgetLua(gDetailedModeWidgets.back());
	// Tooltip widget.

	DrawTooltipWidget tooltip;
	tooltip.mShowTooltip = &gShowTooltip;
	tooltip.mText = gTooltipData;
	gDetailedModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "Tooltips", &tooltip, WIDGET_TYPE_DRAW_TOOLTIP));
	addWidgetLua(gDetailedModeWidgets.back());
}

// Plot mode functions.
/// Get data for plot mode UI.
void profileUpdatePlotModeData(Profile& S, int frameNum)
{
	const float timeHeightStart = gCurrWindowSize.y * 0.2f;
	const float timeHeightEnd = timeHeightStart + gCurrWindowSize.y * 0.4f;
	const float timeHeight = timeHeightEnd - timeHeightStart;
	const float widthOffsetLeft = gCurrWindowSize.x * 0.02f;;
	const float widthOffsetRight = 0.f;

	// Go over all timers and store frame's data into plotData if enabled.
	uint32_t timerLocation = 0;
	for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
	{
		float fToMs = ProfileTickToMsMultiplier(S.GroupInfo[groupIndex].Type == ProfileTokenTypeGpu ? getGpuProfileTicksPerSecond(S.GroupInfo[groupIndex].nGpuProfileToken) : ProfileTicksPerSecondCpu());
		for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
		{
			if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
			{
				float referenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
				float fTime = profileUtilRoundFloatByPrecision(fToMs * S.Frame[timerIndex].nTicks);
				float percentY = clamp(fTime / referenceTime, 0.f, 1.f);
				float x = (float)frameNum / FRAME_HISTORY_LEN * gCurrWindowSize.x;
				if (gPlotModeData[timerLocation].mEnabled)
				{
					gPlotModeData[timerLocation].mTimeData[frameNum] = float2(clamp(x, widthOffsetLeft, gCurrWindowSize.x - widthOffsetRight), timeHeightEnd - percentY * timeHeight);
				}
				else
				{
					// Don't render this value as the timer is not enabled.
					gPlotModeData[timerLocation].mTimeData[frameNum] = float2(FLT_MAX, FLT_MAX);
				}
				timerLocation++;
			}
		}
	}
}

void profileDrawPlotMode(Profile& S)
{
	// Remove any previous widgets.
	for (uint32_t i = 0; i < gPlotModeWidgets.size(); ++i)
	{
		removeGuiWidget(pWidgetGuiComponent, gPlotModeWidgets[i]);
	}

	gPlotModeWidgets.clear();

	float referenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
	const float baseWidthOffset = gCurrWindowSize.x * 0.02f;
	const float timelineCount = min(10.f, referenceTime);
	const float timeHeightStart = gCurrWindowSize.y * 0.2f;
	const float timeHeightEnd = timeHeightStart + gCurrWindowSize.y * 0.4f;
	const float timeHeight = timeHeightEnd - timeHeightStart;

	// Draw Reference Separator.
	DrawLineWidget lineSeparator;
	lineSeparator.mPos1 = float2(baseWidthOffset, timeHeightStart);
	lineSeparator.mPos2 = float2(baseWidthOffset, timeHeightEnd);
	lineSeparator.mColor = 0x64646464;
	lineSeparator.mAddItem = false;
	gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "PlotTimelineSeparator", &lineSeparator, WIDGET_TYPE_DRAW_LINE));
	addWidgetLua(gPlotModeWidgets.back());

	lineSeparator.mPos1 = float2(gCurrWindowSize.x - baseWidthOffset, timeHeightStart);
	lineSeparator.mPos2 = float2(gCurrWindowSize.x - baseWidthOffset, timeHeightEnd);
	gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "PlotTimelineSeparator2", &lineSeparator, WIDGET_TYPE_DRAW_LINE));
	addWidgetLua(gPlotModeWidgets.back());

	// Draw Reference times and reference lines.
	for (uint32_t i = 0; i <= (uint32_t)timelineCount; ++i)
	{
		float percentHeight = (float)i / timelineCount;
		const float xStart = baseWidthOffset;
		const float xEnd = gCurrWindowSize.x - baseWidthOffset;
		float y = timeHeightStart + percentHeight * timeHeight;

		char floatStr[30];
		sprintf(floatStr, "%f", (1.f - percentHeight) * referenceTime);

		char resultStr[30]{};
		profileUtilTrimFloatString(floatStr, resultStr, 1);
		strcat(resultStr, "ms");

		char yStr[30];
		sprintf(yStr, "%f", y);

		lineSeparator.mPos1 = float2(xStart, y);
		lineSeparator.mPos2 = float2(xEnd, y);
		lineSeparator.mColor = 0x32323232;
		gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, yStr, &lineSeparator, WIDGET_TYPE_DRAW_LINE));
		addWidgetLua(gPlotModeWidgets.back());

		DrawTextWidget text;
		text.mPos = float2(baseWidthOffset, y);
		text.mColor = 0xFFFFFFFF;
		gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, resultStr, &text, WIDGET_TYPE_DRAW_TEXT));
		addWidgetLua(gPlotModeWidgets.back());
	}

	// Add some space after the graph drawing.
	CursorLocationWidget cursor;
	cursor.mLocation = float2(baseWidthOffset, timeHeightEnd * 1.05f);
	gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "", &cursor, WIDGET_TYPE_CURSOR_LOCATION));
	addWidgetLua(gPlotModeWidgets.back());

	// Draw Timer Infos.
	for (uint32_t i = 0; i < (uint32_t)gPlotModeData.size(); ++i)
	{
		ProfileTimerInfo& timerInfo = S.TimerInfo[gPlotModeData[i].mTimerInfo];
		uint32_t color = (timerInfo.nColor | 0xFF000000);

		OneLineCheckboxWidget oneLineCheckbox;
		oneLineCheckbox.pData = &gPlotModeData[i].mEnabled;
		oneLineCheckbox.mColor = color;
		gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, timerInfo.pName, &oneLineCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));
		addWidgetLua(gPlotModeWidgets.back());

		// Only 6 timer names in one line to not overflow horizontally.
		if ((i + 1) % 10 != 0)
		{
			HorizontalSpaceWidget space;
			gPlotModeWidgets.push_back(addGuiWidget(pWidgetGuiComponent, "", &space, WIDGET_TYPE_HORIZONTAL_SPACE));
			addWidgetLua(gPlotModeWidgets.back());
		}
	}
}

// Timer mode functions.
void profileDrawTimerMode(Profile& S)
{
	const char* headerNames[10] =
	{
		"Group/Timer",
		"Time",
		"Average Time",
		"Max Time",
		"Min Time",
		"Call Average",
		"Call Count",
		"Exclusive Time",
		"Exclusive Average",
		"Exclusive Max Time"
	};

	// Create the table header.
	eastl::vector<IWidget*> header;

	for (uint32_t i = 0; i < 10; ++i)
	{
		IWidget* pLabel = (IWidget*)tf_calloc(1, sizeof(IWidget));
		pLabel->mType = WIDGET_TYPE_COLOR_LABEL;
		strcpy(pLabel->mLabel, headerNames[i]);

		ColorLabelWidget* pColorLabel = (ColorLabelWidget*)tf_calloc(1, sizeof(ColorLabelWidget));
		pColorLabel->mColor = gLilacColor;
		pLabel->pWidget = pColorLabel;

		header.push_back(pLabel);
	}
	gWidgetTable.push_back(header);

	// Add the header coloumn.
	ColumnWidget headerColumn;
	headerColumn.mNumColumns = (uint32_t)header.size();
	headerColumn.mPerColumnWidgets = header;
	addWidgetLua(addGuiWidget(pWidgetGuiComponent, "Header", &headerColumn, WIDGET_TYPE_COLUMN));

	SeparatorWidget separator;
	addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

	// Add other coloumn data.
	for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
	{
		ColorLabelWidget colorLabel;
		colorLabel.mColor = gFernGreenColor;
		addWidgetLua(addGuiWidget(pWidgetGuiComponent, S.GroupInfo[groupIndex].pName, &colorLabel, WIDGET_TYPE_COLOR_LABEL));
		addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

		// Timers are not 1-1 with the groups so search entire list(not very large) every time.
		for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
		{
			if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
			{
				eastl::vector<IWidget*> timerCol;

				IWidget* pLabel = (IWidget*)tf_calloc(1, sizeof(IWidget));
				pLabel->mType = WIDGET_TYPE_LABEL;
				strcpy(pLabel->mLabel, S.TimerInfo[timerIndex].pName);
				pLabel->pWidget = (LabelWidget*)tf_calloc(1, sizeof(LabelWidget));
				timerCol.push_back(pLabel);

				eastl::vector<char*> timeRowData;
				eastl::vector<float4*> timeColorData;

				// There are 9 time categories in the header above.
				for (uint32_t i = 0; i < 9; ++i)
				{
					char* timeResult = (char*)tf_calloc(MAX_TIME_STR_LEN, sizeof(char));
					sprintf(timeResult, "-");
					float4* timeColor = (float4*)tf_calloc(1, sizeof(float4));
					*timeColor = gNormalColor;

					IWidget* pDynamicTextBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
					pDynamicTextBase->mType = WIDGET_TYPE_DYNAMIC_TEXT;

					DynamicTextWidget* pDynamicText = (DynamicTextWidget*)tf_calloc(1, sizeof(DynamicTextWidget));
					pDynamicText->pData = timeResult;
					pDynamicText->mLength = MAX_TIME_STR_LEN;
					pDynamicText->pColor = timeColor;
					pDynamicTextBase->pWidget = pDynamicText;

					timerCol.push_back(pDynamicTextBase);

					timeRowData.push_back(timeResult);
					timeColorData.push_back(timeColor);
				}

				// Store for dynamic text update.
				gTimerData.push_back(timeRowData);
				gTimerColorData.push_back(timeColorData);

				// Add the time data to the column.
				ColumnWidget timerColWidget;
				timerColWidget.mNumColumns = (uint32_t)timerCol.size();
				timerColWidget.mPerColumnWidgets = timerCol;
				addWidgetLua(addGuiWidget(pWidgetGuiComponent, S.TimerInfo[timerIndex].pName, &timerColWidget, WIDGET_TYPE_COLUMN));
				gWidgetTable.push_back(timerCol);
			}
		}

		addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));
	}
}

void profileResetTimerModeData(uint32_t tableLocation)
{
	eastl::vector<char*>& timeCol = gTimerData[tableLocation];
	eastl::vector<float4*>& timeColor = gTimerColorData[tableLocation];
	sprintf(timeCol[0], "-");
	sprintf(timeCol[1], "-");
	sprintf(timeCol[2], "-");
	sprintf(timeCol[3], "-");
	sprintf(timeCol[4], "-");
	sprintf(timeCol[5], "-");
	sprintf(timeCol[6], "-");
	sprintf(timeCol[7], "-");
	sprintf(timeCol[8], "-");
	*timeColor[0] = gNormalColor;
	*timeColor[1] = gNormalColor;
	*timeColor[2] = gNormalColor;
	*timeColor[3] = gNormalColor;
	*timeColor[4] = gNormalColor;
	*timeColor[5] = gNormalColor;
	*timeColor[6] = gNormalColor;
	*timeColor[7] = gNormalColor;
	*timeColor[8] = gNormalColor;
}

/// Get data for timer mode functionality.
void profileUpdateTimerModeData(Profile& S, uint32_t groupIndex, uint32_t timerIndex, uint32_t tableLocation)
{
	if (!S.Aggregate[timerIndex].nCount)
	{
		profileResetTimerModeData(tableLocation);
		return;
	}
	bool bGpu = S.GroupInfo[groupIndex].Type == ProfileTokenTypeGpu;
	float fToMs = ProfileTickToMsMultiplier(bGpu ? getGpuProfileTicksPerSecond(S.GroupInfo[groupIndex].nGpuProfileToken) : ProfileTicksPerSecondCpu());

	uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
	uint32_t nAggregateCount = S.Aggregate[timerIndex].nCount ? S.Aggregate[timerIndex].nCount : 1;

	// Data to append to the horizontal list of times.
	float fTime = profileUtilRoundFloatByPrecision(fToMs * S.Frame[timerIndex].nTicks);
	float fAverage = profileUtilRoundFloatByPrecision(fToMs * (S.Aggregate[timerIndex].nTicks / nAggregateFrames));
	float fCallAverage = profileUtilRoundFloatByPrecision(fToMs * (S.Aggregate[timerIndex].nTicks / nAggregateCount));
	float fMax = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMax[timerIndex]));
	float fMin = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMin[timerIndex]));
	uint32_t fCallCount = S.Frame[timerIndex].nCount;
	float fFrameMsExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.FrameExclusive[timerIndex]));
	float fAverageExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateExclusive[timerIndex] / nAggregateFrames));
	float fMaxExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMaxExclusive[timerIndex]));

	eastl::vector<char*>& timeCol = gTimerData[tableLocation];
	eastl::vector<float4*> timeColor = gTimerColorData[tableLocation];

	// Fill the timer data buffers with selected timer's data.
	char floatStr[30];
	memset(timeCol[0], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", fTime);
	profileUtilTrimFloatString(floatStr, timeCol[0]);

	memset(timeCol[1], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", fAverage);
	profileUtilTrimFloatString(floatStr, timeCol[1]);

	memset(timeCol[2], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", fMax);
	profileUtilTrimFloatString(floatStr, timeCol[2]);

	memset(timeCol[3], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", fMin);
	profileUtilTrimFloatString(floatStr, timeCol[3]);

	memset(timeCol[4], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", fCallAverage);
	profileUtilTrimFloatString(floatStr, timeCol[4]);

	memset(timeCol[5], 0, MAX_TIME_STR_LEN);
	sprintf(floatStr, "%f", (float)fCallCount);
	profileUtilTrimFloatString(floatStr, timeCol[5]);

	if (!bGpu) // No exclusive times for GPU
	{
		memset(timeCol[6], 0, MAX_TIME_STR_LEN);
		sprintf(floatStr, "%f", fFrameMsExclusive);
		profileUtilTrimFloatString(floatStr, timeCol[6]);

		memset(timeCol[7], 0, MAX_TIME_STR_LEN);
		sprintf(floatStr, "%f", fAverageExclusive);
		profileUtilTrimFloatString(floatStr, timeCol[7]);

		memset(timeCol[8], 0, MAX_TIME_STR_LEN);
		sprintf(floatStr, "%f", fMaxExclusive);
		profileUtilTrimFloatString(floatStr, timeCol[8]);
	}

	// Also add color coding to the times relative to the current selected reference time.
	float criticalTime = CRITICAL_COLOR_THRESHOLD * profileUtilReferenceTimeFromEnum(gReferenceTime);
	float warnTime = WARNING_COLOR_THRESHOLD * profileUtilReferenceTimeFromEnum(gReferenceTime);
	const uint32_t criticalCount = 10;
	const uint32_t warnCount = 5;

	// Change colors to warning/criticals and back to normal.
	fTime > warnTime ? *timeColor[0] = gWarningColor : *timeColor[0] = gNormalColor;
	fAverage > warnTime ? *timeColor[1] = gWarningColor : *timeColor[1] = gNormalColor;
	fMax > warnTime ? *timeColor[2] = gWarningColor : *timeColor[2] = gNormalColor;
	fMin > warnTime ? *timeColor[3] = gWarningColor : *timeColor[3] = gNormalColor;
	fCallAverage > warnTime ? *timeColor[4] = gWarningColor : *timeColor[4] = gNormalColor;
	fCallCount > warnCount ? *timeColor[5] = gWarningColor : *timeColor[5] = gNormalColor;
	fFrameMsExclusive > warnTime ? *timeColor[6] = gWarningColor : *timeColor[6] = gNormalColor;
	fAverageExclusive > warnTime ? *timeColor[7] = gWarningColor : *timeColor[7] = gNormalColor;
	fMaxExclusive > warnTime ? *timeColor[8] = gWarningColor : *timeColor[8] = gNormalColor;

	fTime > criticalTime ? *timeColor[0] = gCriticalColor : float4(0.0f);
	fAverage > criticalTime ? *timeColor[1] = gCriticalColor : float4(0.0f);
	fMax > criticalTime ? *timeColor[2] = gCriticalColor : float4(0.0f);
	fMin > criticalTime ? *timeColor[3] = gCriticalColor : float4(0.0f);
	fCallAverage > criticalTime ? *timeColor[4] = gCriticalColor : float4(0.0f);
	fCallCount > criticalCount ? *timeColor[5] = gCriticalColor : float4(0.0f);
	fFrameMsExclusive > criticalTime ? *timeColor[6] = gCriticalColor : float4(0.0f);
	fAverageExclusive > criticalTime ? *timeColor[7] = gCriticalColor : float4(0.0f);
	fMaxExclusive > criticalTime ? *timeColor[8] = gCriticalColor : float4(0.0f);
}

void resetProfilerUI()
{
	if (pWidgetGuiComponent)
	{
		removeGuiAllWidgets(pWidgetGuiComponent);
	}

	// Free any allocated widget memory.
	for (uint32_t i = 0; i < gWidgetTable.size(); ++i)
	{
		for (uint32_t j = 0; j < gWidgetTable[i].size(); ++j)
		{
			destroyWidget(gWidgetTable[i][j], true);
		}
	}
	gWidgetTable.clear();

	// Free any allocated timer data mem.
	for (uint32_t i = 0; i < gTimerData.size(); ++i)
	{
		for (uint32_t j = 0; j < gTimerData[i].size(); ++j)
		{
			tf_delete(gTimerData[i][j]);
			tf_delete(gTimerColorData[i][j]);
		}
	}
	gTimerData.clear();
	gTimerColorData.clear();

	// Free any allocated graph data mem.
	for (uint32_t i = 0; i < gPlotModeData.size(); ++i)
	{
		tf_free(gPlotModeData[i].mTimeData);
	}
	gPlotModeData.clear();
	gDetailedModeTooltips.clear();
	gDetailedModeWidgets.clear();
	gDetailedModeDump.clear();
	gPlotModeWidgets.clear();
	memset(gFrameTimerTitle, 0, MAX_TITLE_STR_LEN);
}

void exitProfilerUI()
{
	resetProfilerUI();

	gWidgetTable.set_capacity(0);
	gTimerData.set_capacity(0);
	gTimerColorData.set_capacity(0);
	gPlotModeData.set_capacity(0);
	gDetailedModeTooltips.set_capacity(0);
	gDetailedModeWidgets.set_capacity(0);
	gDetailedModeDump.set_capacity(0);
	gPlotModeWidgets.set_capacity(0);
	memset(gFrameTimerTitle, 0, MAX_TITLE_STR_LEN);
	for (uint32_t i = 0; i < PROFILE_MAX_GROUPS; ++i)
	{
		memset(gGPUTimerTitle[i], 0, MAX_TITLE_STR_LEN);
	}
	pAppUIRef = 0;
	pWidgetGuiComponent = 0;
	pMenuGuiComponent = 0;
	gTotalGroups = 0;
	gTotalTimers = 0;
	gUnloaded = true;
}

void drawGpuProfileRecursive(Cmd* pCmd, const GpuProfiler* pGpuProfiler, const TextDrawDesc* pDrawDesc, float2& origin, const uint32_t index, float2& curTotalTxtSizePx)
{
	GpuTimer* pRoot = &pGpuProfiler->pGpuTimerPool[index];
	ASSERT(pRoot);
	uint32_t nTimerIndex = ProfileGetTimerIndex(pRoot->mMicroProfileToken);
	Profile& S = *ProfileGet();
	if (!S.Aggregate[nTimerIndex].nCount)
		return;

	const GpuProfileDrawDesc* pGpuDrawDesc = &gDefaultGpuProfileDrawDesc;
	float2 pos(origin.x + pGpuDrawDesc->mChildIndent * pRoot->mDepth, origin.y);
	uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
	float fsToMs = ProfileTickToMsMultiplier((uint64_t)pGpuProfiler->mGpuTimeStampFrequency);
	float fAverage = fsToMs * (float)(S.Aggregate[nTimerIndex].nTicks / nAggregateFrames);

	char printableString[128]{};
	strcpy(printableString, pRoot->mName);
	strcat(printableString, " - ");
  
	char buffer[30];
	sprintf(buffer, "%f", fAverage);
	strcat(printableString, buffer);

	sprintf(buffer, " ms");
	strcat(printableString, buffer);

	pAppUIRef->pImpl->pFontStash->drawText(
		pCmd, printableString, pos.x, pos.y, pDrawDesc->mFontID, pDrawDesc->mFontColor,
		pDrawDesc->mFontSize, pDrawDesc->mFontSpacing, pDrawDesc->mFontBlur);

	float2 textSizePx = measureAppUIText(pAppUIRef, printableString, pDrawDesc);
	origin.y += textSizePx.y + pGpuDrawDesc->mHeightOffset;
	textSizePx.x += pGpuDrawDesc->mChildIndent * pRoot->mDepth;
	curTotalTxtSizePx.x = max(textSizePx.x, curTotalTxtSizePx.x);
	curTotalTxtSizePx.y += textSizePx.y + pGpuDrawDesc->mHeightOffset;

	// Metal only supports gpu timers on command buffer boundaries so all timers other than root will be zero
#ifdef METAL
	return;
#else
	for (uint32_t i = index + 1; i < pGpuProfiler->mCurrentPoolIndex; ++i)
	{
		if (pGpuProfiler->pGpuTimerPool[i].pParent == pRoot)
		{
			drawGpuProfileRecursive(pCmd, pGpuProfiler, pDrawDesc, origin, i, curTotalTxtSizePx);
		}
	}
#endif
}

float2 cmdDrawGpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, ProfileToken nProfileToken, const TextDrawDesc* pDrawDesc)
{
	GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
	if (!pGpuProfiler || !pAppUIRef)
	{
		return float2(0.f, 0.f);
	}
	ASSERT(pAppUIRef); // Must be initialized through loadProfilerUI   
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	float2                    pos = screenCoordsInPx;
	const char titleStr[] = "-----GPU Times-----";

	pAppUIRef->pImpl->pFontStash->drawText(
		pCmd, titleStr, pos.x, pos.y, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize,
		pDesc->mFontSpacing, pDesc->mFontBlur);

	float2 totalTextSizePx = measureAppUIText(pAppUIRef, titleStr, pDesc);
	pos.y += totalTextSizePx.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
	drawGpuProfileRecursive(pCmd, pGpuProfiler, pDesc, pos, 0, totalTextSizePx);

	return totalTextSizePx;
}

float2 cmdDrawCpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, const TextDrawDesc* pDrawDesc)
{
	if (!pAppUIRef)
		return float2(0.f, 0.f);

	ASSERT(pAppUIRef); // Must be initialized through loadProfilerUI
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	char txt[30] = {};
	sprintf(txt, "CPU %f ms", getCpuAvgFrameTime());
	drawAppUIText(pAppUIRef, pCmd, &screenCoordsInPx, txt, pDesc);

	float2 totalTextSizePx = measureAppUIText(pAppUIRef, txt, pDesc);
	return totalTextSizePx;
}
/// Draws the top menu and draws the selected timer mode.
void profileLoadWidgetUI(Profile& S)
{
	resetProfilerUI();

	eastl::vector<IWidget*> topMenu;

	// Profile mode
	IWidget* pProfileModeBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
	pProfileModeBase->mType = WIDGET_TYPE_DROPDOWN;
	strcpy(pProfileModeBase->mLabel, "Select Profile Mode");
	DropdownWidget* pProfileMode = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));
	pProfileMode->pData = (uint32_t*)&gProfileMode;
	pProfileModeBase->pWidget = pProfileMode;

	for (uint32_t i = 0; i < PROFILE_MODE_MAX; ++i)
	{
		char* profileModeName = (char*)tf_calloc(strlen(pProfileModesNames[i]) + 1, sizeof(char));
		strcpy(profileModeName, pProfileModesNames[i]);
		pProfileMode->mNames.push_back(profileModeName);

		pProfileMode->mValues.push_back(gProfileModesValues[i]);
	}
	topMenu.push_back(pProfileModeBase);

	// Aggregate frames
	IWidget* pAggregateBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
	pAggregateBase->mType = WIDGET_TYPE_DROPDOWN;
	strcpy(pAggregateBase->mLabel, "Aggregate Frames");
	DropdownWidget* pAggregate = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));
	pAggregate->pData = (uint32_t*)&gAggregateFrames;
	pAggregateBase->pWidget = pAggregate;

	for (uint32_t i = 0; i < PROFILE_AGGREGATE_FRAMES_MAX; ++i)
	{
		char* aggregateName = (char*)tf_calloc(strlen(pAggregateFramesNames[i]) + 1, sizeof(char));
		strcpy(aggregateName, pAggregateFramesNames[i]);
		pAggregate->mNames.push_back(aggregateName);

		pAggregate->mValues.push_back(gAggregateFramesValues[i]);
	}
	topMenu.push_back(pAggregateBase);

	// Reference times
	IWidget* pReferenceBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
	pReferenceBase->mType = WIDGET_TYPE_DROPDOWN;
	strcpy(pReferenceBase->mLabel, "Reference Times");
	DropdownWidget* pReference = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));
	pReference->pData = (uint32_t*)&gReferenceTime;
	pReferenceBase->pWidget = pReference;

	for (uint32_t i = 0; i < PROFILE_REFTIME_MAX; ++i)
	{
		char* referenceName = (char*)tf_calloc(strlen(pReferenceTimesNames[i]) + 1, sizeof(char));
		strcpy(referenceName, pReferenceTimesNames[i]);
		pReference->mNames.push_back(referenceName);

		pReference->mValues.push_back(gReferenceTimesValues[i]);
	}
	topMenu.push_back(pReferenceBase);
	topMenu.back()->pOnEdited = ProfileCallbkReferenceTimeUpdated;

	// Don't dump frames to disk in detailed mode.
	if (gProfileMode == PROFILE_MODE_DETAILED)
	{
		IWidget* pDetailedBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
		pDetailedBase->mType = WIDGET_TYPE_DROPDOWN;
		strcpy(pDetailedBase->mLabel, "Dump Frames Detailed View");
		DropdownWidget* pDetailed = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));
		pDetailed->pData = (uint32_t*)&gDumpFramesDetailedMode;
		pDetailedBase->pWidget = pDetailed;

		for (uint32_t i = 0; i < PROFILE_DUMPFRAME_MAX; ++i)
		{
			char* detailedName = (char*)tf_calloc(strlen(pDumpFramesDetailedViewNames[i]) + 1, sizeof(char));
			strcpy(detailedName, pDumpFramesDetailedViewNames[i]);
			pDetailed->mNames.push_back(detailedName);

			pDetailed->mValues.push_back(gDumpFramesDetailedValues[i]);
		}
		topMenu.push_back(pDetailedBase);
		topMenu.back()->pOnEdited = profileCallbkDumpFrames;

		gDumpFramesNow = true;
	}
	else
	{
		IWidget* pDumpBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
		pDumpBase->mType = WIDGET_TYPE_DROPDOWN;
		strcpy(pDumpBase->mLabel, "Dump Frames Detailed View");
		DropdownWidget* pDump = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));
		pDump->pData = (uint32_t*)&gDumpFramesToFile;
		pDumpBase->pWidget = pDump;

		for (uint32_t i = 0; i < PROFILE_DUMPFILE_MAX; ++i)
		{
			char* dumpName = (char*)tf_calloc(strlen(pDumpFramesToFileNames[i]) + 1, sizeof(char));
			strcpy(dumpName, pDumpFramesToFileNames[i]);
			pDump->mNames.push_back(dumpName);

			pDump->mValues.push_back(gDumpFramesToFileValues[i]);
		}
		topMenu.push_back(pDumpBase);
		topMenu.back()->pOnEdited = profileCallbkDumpFramesToFile;
	}

	IWidget* pCheckboxBase = (IWidget*)tf_calloc(1, sizeof(IWidget));
	pCheckboxBase->mType = WIDGET_TYPE_CHECKBOX;
	strcpy(pCheckboxBase->mLabel, "Profiler Paused");
	CheckboxWidget* pCheckbox = (CheckboxWidget*)tf_calloc(1, sizeof(CheckboxWidget));
	pCheckbox->pData = &gProfilerPaused;
	pCheckboxBase->pWidget = pCheckbox;

	topMenu.push_back(pCheckboxBase);
	topMenu.back()->pOnEdited = profileCallbkPauseProfiler;

	ColumnWidget topMenuCol;
	topMenuCol.mNumColumns = (uint32_t)topMenu.size();
	topMenuCol.mPerColumnWidgets = topMenu;

	addWidgetLua(addGuiWidget(pWidgetGuiComponent, "topmenu", &topMenuCol, WIDGET_TYPE_COLUMN));
	gWidgetTable.push_back(topMenu);

	SeparatorWidget separator;
	addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

	if (gProfileMode == PROFILE_MODE_TIMER)
	{
		PlotLinesWidget plotLines;
		plotLines.mValues = gFrameTimeData;
		plotLines.mNumValues = FRAME_HISTORY_LEN;
		plotLines.mScaleMin = &gMinPlotReferenceTime;
		plotLines.mScaleMax = &S.fReferenceTime;
		plotLines.mPlotScale = &gHistogramSize;
		plotLines.mTitle = gFrameTimerTitle;

		addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &plotLines, WIDGET_TYPE_PLOT_LINES));
		for (uint32_t i = 0; i < S.nGroupCount; ++i)
		{
			if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
			{
				plotLines.mValues = gGPUFrameTimeData[i];
				plotLines.mNumValues = FRAME_HISTORY_LEN;
				plotLines.mScaleMin = &gMinPlotReferenceTime;
				plotLines.mScaleMax = &S.fReferenceTime;
				plotLines.mPlotScale = &gHistogramSize;
				plotLines.mTitle = gGPUTimerTitle[i];

				addWidgetLua(addGuiWidget(pWidgetGuiComponent, "", &plotLines, WIDGET_TYPE_PLOT_LINES));
			}
		}

		profileDrawTimerMode(S);
	}
	else if (gProfileMode == PROFILE_MODE_DETAILED)
	{
		profileDrawDetailedMode(S);
	}
	else if (gProfileMode == PROFILE_MODE_PLOT)
	{
		// Allocate plot data for all timers.
		for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
		{
			for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
			{
				if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
				{
					PlotModeData graphTimer;
					graphTimer.mTimerInfo = timerIndex;
					graphTimer.mTimeData = (float2*)tf_malloc(FRAME_HISTORY_LEN * sizeof(float2));
					graphTimer.mEnabled = false;
					uint32_t color = (S.TimerInfo[timerIndex].nColor | 0x7D000000);

					DrawCurveWidget curve;
					curve.mPos = graphTimer.mTimeData;
					curve.mNumPoints = FRAME_HISTORY_LEN;
					curve.mThickness = 1.f;
					curve.mColor = color;
					addWidgetLua(addGuiWidget(pWidgetGuiComponent, "Curves", &curve, WIDGET_TYPE_DRAW_CURVE));
					gPlotModeData.push_back(graphTimer);
				}
			}
		}

		profileDrawPlotMode(S);
	}
}

/// Checks for a change in profile mode so we don't have to call profileDrawWidgetUI every frame (expensive).
void profileUpdateProfileMode(Profile& S)
{
	if (gPrevProfileMode != gProfileMode)
	{
		profileLoadWidgetUI(S);
		gPrevProfileMode = gProfileMode;
	}
}

/// Get data for the header histograms and updates the plot mode data. Called every frame.
void profileUpdateWidgetUI(Profile& S)
{
	float fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
	uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;

	// Check windowSize change.
	float2 windowSize = float2(pWidgetGuiComponent->mCurrentWindowRect.getZ(), pWidgetGuiComponent->mCurrentWindowRect.getW());
	if (windowSize.x != gCurrWindowSize.x || windowSize.y != gCurrWindowSize.y)
	{
		gCurrWindowSize = windowSize;
		gUpdatePlotModeGUI = true;
	}

	gHistogramSize = float2(gCurrWindowSize.x, 0.1f * gCurrWindowSize.y);
	gFrameTime = fToMs * (S.nFlipTicks);

	// Currently take gpu ticks per second from first Gpu group found
	for (uint32_t i = 0; i < S.nGroupCount; ++i)
	{
		if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
		{
			gGPUFrameTime[i] = getGpuProfileTime(S.GroupInfo[i].nGpuProfileToken);
		}
	}

	S.fReferenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
	S.nAggregateFlip = profileUtilAggregateFramesFromEnum(gAggregateFrames);

	static int frameNum = 0;
	frameNum = (frameNum + 1) % FRAME_HISTORY_LEN;

	// Add the data to histogram array.
	if (!gProfilerPaused)
	{
		gFrameTimeData[frameNum] = gFrameTime;

		sprintf(gFrameTimerTitle, "Frame: Time[");

		char buffer[30];

		sprintf(buffer, "%f", gFrameTime);
		strcat(gFrameTimerTitle, buffer);
		strcat(gFrameTimerTitle, "ms] Avg[");

		sprintf(buffer, "%f", fToMs * (S.nFlipAggregateDisplay / nAggregateFrames));
		strcat(gFrameTimerTitle, buffer);
		strcat(gFrameTimerTitle, "ms] Min[");

		sprintf(buffer, "%f", fToMs * S.nFlipMinDisplay);
		strcat(gFrameTimerTitle, buffer);
		strcat(gFrameTimerTitle, "ms] Max[");

		sprintf(buffer, "%f", fToMs * S.nFlipMaxDisplay);
		strcat(gFrameTimerTitle, buffer);
		strcat(gFrameTimerTitle, "ms]");

		for (uint32_t i = 0; i < S.nGroupCount; ++i)
		{
			if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
			{
				gGPUFrameTimeData[i][frameNum] = gGPUFrameTime[i];

				memset(gGPUTimerTitle[i], 0, MAX_TITLE_STR_LEN);
				strcpy(gGPUTimerTitle[i], S.GroupInfo[i].pName);
				strcat(gGPUTimerTitle[i], ": Time[");

				char buffer[30];
				sprintf(buffer, "%f", gGPUFrameTime[i]);
				strcat(gGPUTimerTitle[i], buffer);

				strcat(gGPUTimerTitle[i], "ms]");
			}
		}

		static int frameNumPlot = 0;
		if (gProfileMode == PROFILE_MODE_PLOT)
		{
			profileUpdatePlotModeData(S, frameNumPlot);
			frameNumPlot = (frameNumPlot + 1) % FRAME_HISTORY_LEN;
		}
		else
		{
			frameNumPlot = 0;
		}
	}
}

void toggleProfilerUI()
{
	gProfilerWidgetUIEnabled = (gProfilerWidgetUIEnabled == true) ? false : true;
}

void initProfilerUI(UIApp* uiApp, int32_t width, int32_t height)
{
	// Remove previous GUI component.
	if (pWidgetGuiComponent)
	{
		removeAppUIGuiComponent(pAppUIRef, pWidgetGuiComponent);
	}

	pAppUIRef = uiApp;

	GuiDesc guiDesc = {};
	guiDesc.mStartSize = profileUtilCalcWindowSize(width, height);
	guiDesc.mStartPosition = vec2(PROFILER_WINDOW_X, PROFILER_WINDOW_Y);
	pWidgetGuiComponent = addAppUIGuiComponent(pAppUIRef, " Micro Profiler ", &guiDesc);

	// Disable auto resize and enable manual re-size for the profiler window with scrollbars.
	pWidgetGuiComponent->mFlags |= GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR;
	pWidgetGuiComponent->mFlags |= GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR;
	pWidgetGuiComponent->mFlags &= (~GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE);
	pWidgetGuiComponent->mFlags &= (~GUI_COMPONENT_FLAGS_NO_RESIZE);

	gUnloaded = true;

	// We won't be drawing the default profiler UI anymore.
	Profile& S = *ProfileGet();
	S.nDisplay = P_DRAW_OFF;

	GuiDesc guiMenuDesc = {};
	float   dpiScale = getDpiScale().x;

	guiMenuDesc.mStartPosition = vec2(width - 300.0f * dpiScale, height * 0.5f);

	pMenuGuiComponent = addAppUIGuiComponent(pAppUIRef, "Micro Profiler", &guiMenuDesc);

	CheckboxWidget checkbox;
	checkbox.pData = &gProfilerWidgetUIEnabled;
	addWidgetLua(addGuiWidget(pMenuGuiComponent, "Toggle Profiler", &checkbox, WIDGET_TYPE_CHECKBOX));

	SeparatorWidget separator;
	addWidgetLua(addGuiWidget(pMenuGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

	ButtonWidget dumpButtonWidget;
	IWidget* pDumpButton = addGuiWidget(pMenuGuiComponent, "Dump Profile", &dumpButtonWidget, WIDGET_TYPE_BUTTON);
	pDumpButton->pOnDeactivatedAfterEdit = profileCallbkDumpFramesToFile;
	addWidgetLua(pDumpButton);

	addWidgetLua(addGuiWidget(pMenuGuiComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

	SliderFloatWidget sliderFloat;
	sliderFloat.pData = &gGuiTransparency;
	sliderFloat.mMin = 0.f;
	sliderFloat.mMax = 0.9f;
	sliderFloat.mStep = 0.01f;
	memset(sliderFloat.mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(sliderFloat.mFormat, "%.2f");
	addWidgetLua(addGuiWidget(pMenuGuiComponent, "Transparency", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));
}

void cmdDrawProfilerUI()
{
	if (!pAppUIRef)
		return;
	PROFILER_SET_CPU_SCOPE("MicroProfilerUI", "WidgetsUI", 0x737373);

	ASSERT(pAppUIRef); // Must be initialized through loadProfilerUI
	pMenuGuiComponent->mAlpha = 1.0f - gGuiTransparency;
	appUIGui(pAppUIRef, pMenuGuiComponent);
	Profile& S = *ProfileGet();

	// Profiler enabled.
	if (gProfilerWidgetUIEnabled)
	{
		// New groups or timers were found. Create the widget table.
		if (S.nGroupCount != gTotalGroups || S.nTotalTimers != gTotalTimers || gUnloaded)
		{
			profileLoadWidgetUI(S);
			gTotalGroups = S.nGroupCount;
			gTotalTimers = S.nTotalTimers;
			gUnloaded = false;
		}

		// Select profile mode to display.
		profileUpdateProfileMode(S);
		// Update frame history, etc.
		profileUpdateWidgetUI(S);

		// Update timer mode buffers.
		if (gProfileMode == PROFILE_MODE_TIMER)
		{
			uint32_t timerTableLocation = 0;
			for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
			{
				// Timers are not 1-1 with the groups so search entire list every time.
				for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
				{
					if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
					{
						profileUpdateTimerModeData(S, groupIndex, timerIndex, timerTableLocation++);
					}
				}
			}
		}

		// Accumulate frames for detailed view.
		if (gProfileMode == PROFILE_MODE_DETAILED)
		{
			if (gDumpFramesNow)
			{
				// Add this frame data.
				profileUpdateDetailedModeData(S);
				// Finish dumping required number of frames and display in GUI.
				if (gDetailedModeDump.size() >= profileUtilDumpFramesDetailedModeEnum(gDumpFramesDetailedMode))
				{
					gDumpFramesNow = false;
					profileDrawDetailedMode(S);
				}
			}

			// Update mouse hover tooltip for detailed mode.
			profileUpdateDetailedModeTooltip(S);
		}

		// Update plot mode GUI.
		if (gProfileMode == PROFILE_MODE_PLOT)
		{
			// Recreate Plot mode GUI if reference settings are changed.
			if (gUpdatePlotModeGUI)
			{
				profileDrawPlotMode(S);
				gUpdatePlotModeGUI = false;
			}
		}

		pWidgetGuiComponent->mAlpha = 1.0f - gGuiTransparency;
		appUIGui(pAppUIRef, pWidgetGuiComponent);

	} // profiler enabled.

}

#endif // PROFILE_ENABLED
