/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
void loadProfilerUI(UIApp* uiApp, int32_t width, int32_t height) {}
void unloadProfilerUI() {}
void cmdDrawProfilerUI() {}
float2 cmdDrawGpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, ProfileToken nProfileToken, const TextDrawDesc* pDrawDesc) {}
float2 cmdDrawCpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, const TextDrawDesc* pDrawDesc) {}
void toggleProfilerUI() {}
#else

#include "ProfilerBase.h"
#include "GpuProfiler.h"

#define MAX_DETAILED_TIMERS_DRAW 2048
#define MAX_TIME_STR_LEN 32
#define MAX_TITLE_STR_LEN 128
#define FRAME_HISTORY_LEN 128
#define CRITICAL_COLOR_THRESHOLD 0.5f
#define WARNING_COLOR_THRESHOLD 0.3f
#define MAX_TOOLTIP_STR_LEN 512
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
	eastl::string mThreadName;
};

struct ProfileDetailedModeFrame
{
	float mFrameTime;
	eastl::vector<ProfileDetailedModeTime> mTimers;
};

struct ProfileDetailedModeTooltip
{
	ProfileDetailedModeTooltip() {}
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
eastl::string gFrameTimerTitle = "FrameTimer";
eastl::string gGPUTimerTitle[PROFILE_MAX_GROUPS];
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
char gTooltipData[MAX_TOOLTIP_STR_LEN] = { '0' };

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

eastl::string profileUtilTrimFloatString(const eastl::string& numericString, uint32_t precision = 2u)
{
	eastl::string result;
	result.assign(numericString, 0, numericString.find('.') + 1 + precision);
	return result;
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

		gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget("", float2(x, startHeightPixels), float2(x, lineHeight), color, false)));
		x += interLineDistance;
	}
}

void profileGetDetailedModeFrameTimeBetweenTicks(int64_t nTicks, int64_t nTicksEnd, int32_t nLogIndex, uint32_t* nFrameBegin, uint32_t* nFrameEnd)
{
	Profile& S = *ProfileGet();
	ASSERT(nLogIndex < 0 || S.Pool[nLogIndex]);

	bool bGpu = S.Pool[nLogIndex]->nGpu != 0;
	uint32_t nPut = tfrg_atomic32_load_relaxed(&S.Pool[nLogIndex]->nPut);

	uint32_t nBegin = S.nFrameCurrent;

	for (uint32_t i = 0; i < PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY; ++i)
	{
		uint32_t nFrame = (S.nFrameCurrent + PROFILE_MAX_FRAME_HISTORY - i) % PROFILE_MAX_FRAME_HISTORY;

		if (nLogIndex >= 0)
		{
			uint32_t nCurrStart = S.Frames[nBegin].nLogStart[nLogIndex];
			uint32_t nPrevStart = S.Frames[nFrame].nLogStart[nLogIndex];
			bool bOverflow = (nPrevStart <= nCurrStart) ? (nPut >= nPrevStart && nPut < nCurrStart) : (nPut < nCurrStart || nPut >= nPrevStart);
			if (bOverflow)
				break;
		}

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
						timerLog.mThreadName.append(pLog->ThreadName);
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
			eastl::string tooltipData;
			const ProfileDetailedModeTime& timer = gDetailedModeTooltips[i].mTimer;
			const ProfileTimerInfo& timerInfo = S.TimerInfo[timer.mTimerInfoIndex];

			tooltipData.append(timerInfo.pName);
			tooltipData.append("\n------------------------------\n");

			tooltipData.append("Group Name: ");
			tooltipData.append(S.GroupInfo[timerInfo.nGroupIndex].pName);
			tooltipData.append("\n");

			tooltipData.append("Thread: ");
			tooltipData.append(timer.mThreadName);
			tooltipData.append("\n");

			tooltipData.append("Frame Number: ");
			tooltipData.append(eastl::to_string(timer.mFrameNum));
			tooltipData.append("\n");

			tooltipData.append("Start Time(ms): ");
			tooltipData.append(eastl::to_string(timer.mCurrFrameTime + timer.mStartTime));
			tooltipData.append("\n");

			tooltipData.append("End Time(ms)  : ");
			tooltipData.append(eastl::to_string(timer.mCurrFrameTime + timer.mEndTime));
			tooltipData.append("\n");

			tooltipData.append("Total Time(ms): ");
			tooltipData.append(eastl::to_string(timer.mEndTime - timer.mStartTime));
			tooltipData.append("\n");


			strcpy(gTooltipData, tooltipData.c_str());
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
		pWidgetGuiComponent->RemoveWidget(gDetailedModeTooltips[i].mWidget);
	}

	// Remove all other widgets.
	for (uint32_t i = 0; i < gDetailedModeWidgets.size(); ++i)
	{
		pWidgetGuiComponent->RemoveWidget(gDetailedModeWidgets[i]);
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
			gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(FilledRectWidget(eastl::string(timerInfo.pName), pos, scale, color)));

			// Add data to draw a tooltip for this timer.
			gDetailedModeTooltips.push_back(ProfileDetailedModeTooltip(timer, pWidgetGuiComponent->AddWidget(DrawTextWidget(timerInfo.pName, pos + float2(scale.x, 0.f), 0xFFFFFFFF))));
			frameHeight = max(frameHeight, height * 1.2f);
			++timerCount;
		}
		frameTime += frameToDraw.mFrameTime;
	}

	// Add timeline.
	for (uint32_t i = 0; i < (uint32_t)ceilf(frameTime); ++i)
	{
		gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawTextWidget(eastl::to_string(i), float2(startWidthPixels + i * msToPixels, startHeightPixels - gCurrWindowSize.y * 0.03f), 0xFFFFFFFF)));
	}

	// Backgroud vertical lines.
	profileDrawDetailedModeGrid(startHeightPixels, startWidthPixels, msToPixels / 2.f, (uint32_t)ceilf(frameTime * 2.f), frameHeight);
	// Horizontal Separator lines. 
	gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget("TopSeparator", float2(startWidthPixels, startHeightPixels), float2(ceilf(frameTime) * msToPixels, startHeightPixels), 0x64646464, true)));
	gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget("BottomSeparator", float2(startWidthPixels, frameHeight), float2(ceilf(frameTime) * msToPixels, frameHeight), 0x64646464, true)));
	// Tooltip widget.
	gDetailedModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawTooltipWidget("Tooltips", &gShowTooltip, gTooltipData)));
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
		pWidgetGuiComponent->RemoveWidget(gPlotModeWidgets[i]);
	}

	gPlotModeWidgets.clear();

	float referenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
	const float baseWidthOffset = gCurrWindowSize.x * 0.02f;
	const float timelineCount = min(10.f, referenceTime);
	const float timeHeightStart = gCurrWindowSize.y * 0.2f;
	const float timeHeightEnd = timeHeightStart + gCurrWindowSize.y * 0.4f;
	const float timeHeight = timeHeightEnd - timeHeightStart;

	// Draw Reference Separator.
	gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget("PlotTimelineSeparator", float2(baseWidthOffset, timeHeightStart), float2(baseWidthOffset, timeHeightEnd), 0x64646464, false)));
	gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget("PlotTimelineSeparator2", float2(gCurrWindowSize.x - baseWidthOffset, timeHeightStart), float2(gCurrWindowSize.x - baseWidthOffset, timeHeightEnd), 0x64646464, false)));

	// Draw Reference times and reference lines.
	for (uint32_t i = 0; i <= (uint32_t)timelineCount; ++i)
	{
		float percentHeight = (float)i / timelineCount;
		const float xStart = baseWidthOffset;
		const float xEnd = gCurrWindowSize.x - baseWidthOffset;
		float y = timeHeightStart + percentHeight * timeHeight;

		gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawLineWidget(eastl::to_string(y), float2(xStart, y), float2(xEnd, y), 0x32323232, false)));
		gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(DrawTextWidget(profileUtilTrimFloatString(eastl::to_string((1.f - percentHeight) * referenceTime), 1) + eastl::string("ms"), float2(baseWidthOffset, y), 0xFFFFFFFF)));
	}

	// Add some space after the graph drawing.
	gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(CursorLocationWidget("", float2(baseWidthOffset, timeHeightEnd * 1.05f))));

	// Draw Timer Infos.
	for (uint32_t i = 0; i < (uint32_t)gPlotModeData.size(); ++i)
	{
		ProfileTimerInfo& timerInfo = S.TimerInfo[gPlotModeData[i].mTimerInfo];
		uint32_t color = (timerInfo.nColor | 0xFF000000);
		gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(OneLineCheckboxWidget(timerInfo.pName, &gPlotModeData[i].mEnabled, color)));

		// Only 6 timer names in one line to not overflow horizontally.
		if ((i + 1) % 10 != 0)
		{
			gPlotModeWidgets.push_back(pWidgetGuiComponent->AddWidget(HorizontalSpaceWidget()));
		}
	}
}

// Timer mode functions.
void profileDrawTimerMode(Profile& S)
{
	// Create the table header.
	eastl::vector<IWidget*> header;
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Group/Timer", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Time", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Average Time", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Max Time", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Min Time", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Call Average", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Call Count", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Exclusive Time", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Exclusive Average", gLilacColor));
	header.push_back(tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(ColorLabelWidget)), "Exclusive Max Time", gLilacColor));
	gWidgetTable.push_back(header);

	// Add the header coloumn.
	pWidgetGuiComponent->AddWidget(ColumnWidget("Header", header));

	pWidgetGuiComponent->AddWidget(SeparatorWidget());

	// Add other coloumn data.
	for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
	{
		pWidgetGuiComponent->AddWidget(ColorLabelWidget(S.GroupInfo[groupIndex].pName, gFernGreenColor));
		pWidgetGuiComponent->AddWidget(SeparatorWidget());

		// Timers are not 1-1 with the groups so search entire list(not very large) every time.
		for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
		{
			if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
			{
				eastl::vector<IWidget*> timerCol;
				timerCol.push_back(tf_placement_new<LabelWidget>(tf_calloc(1, sizeof(LabelWidget)), S.TimerInfo[timerIndex].pName));
				eastl::vector<char*> timeRowData;
				eastl::vector<float4*> timeColorData;

				// There are 9 time categories in the header above.
				for (uint32_t i = 0; i < 9; ++i)
				{
					char* timeResult = (char*)tf_calloc(MAX_TIME_STR_LEN, sizeof(char));
					strcpy(timeResult, "-");
					float4* timeColor = (float4*)tf_calloc(1, sizeof(float4));
					*timeColor = gNormalColor;
					timerCol.push_back(tf_placement_new<DynamicTextWidget>(tf_calloc(1, sizeof(DynamicTextWidget)), "", timeResult, MAX_TIME_STR_LEN, timeColor));
					timeRowData.push_back(timeResult);
					timeColorData.push_back(timeColor);
				}

				// Store for dynamic text update.
				gTimerData.push_back(timeRowData);
				gTimerColorData.push_back(timeColorData);

				// Add the time data to the column.
				pWidgetGuiComponent->AddWidget(ColumnWidget(S.TimerInfo[timerIndex].pName, timerCol));
				gWidgetTable.push_back(timerCol);
			}
		}

		pWidgetGuiComponent->AddWidget(SeparatorWidget());
	}
}

void profileResetTimerModeData(uint32_t tableLocation)
{
	eastl::vector<char*>& timeCol = gTimerData[tableLocation];
	eastl::vector<float4*>& timeColor = gTimerColorData[tableLocation];
	strcpy(timeCol[0], "-");
	strcpy(timeCol[1], "-");
	strcpy(timeCol[2], "-");
	strcpy(timeCol[3], "-");
	strcpy(timeCol[4], "-");
	strcpy(timeCol[5], "-");
	strcpy(timeCol[6], "-");
	strcpy(timeCol[7], "-");
	strcpy(timeCol[8], "-");
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
	strcpy(timeCol[0], profileUtilTrimFloatString(eastl::to_string(fTime)).c_str());
	strcpy(timeCol[1], profileUtilTrimFloatString(eastl::to_string(fAverage)).c_str());
	strcpy(timeCol[2], profileUtilTrimFloatString(eastl::to_string(fMax)).c_str());
	strcpy(timeCol[3], profileUtilTrimFloatString(eastl::to_string(fMin)).c_str());
	strcpy(timeCol[4], profileUtilTrimFloatString(eastl::to_string(fCallAverage)).c_str());
	strcpy(timeCol[5], profileUtilTrimFloatString(eastl::to_string(fCallCount)).c_str());

	if (!bGpu) // No exclusive times for GPU
	{
		strcpy(timeCol[6], profileUtilTrimFloatString(eastl::to_string(fFrameMsExclusive)).c_str());
		strcpy(timeCol[7], profileUtilTrimFloatString(eastl::to_string(fAverageExclusive)).c_str());
		strcpy(timeCol[8], profileUtilTrimFloatString(eastl::to_string(fMaxExclusive)).c_str());
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

void unloadProfilerUI()
{
	if (pWidgetGuiComponent)
	{
		pWidgetGuiComponent->RemoveAllWidgets();
	}

	// Free any allocated widget memory.
	for (uint32_t i = 0; i < gWidgetTable.size(); ++i)
	{
		for (uint32_t j = 0; j < gWidgetTable[i].size(); ++j)
		{
			tf_delete(gWidgetTable[i][j]);
		}
	}

	// Free any allocated timer data mem.
	for (uint32_t i = 0; i < gTimerData.size(); ++i)
	{
		for (uint32_t j = 0; j < gTimerData[i].size(); ++j)
		{
			tf_delete(gTimerData[i][j]);
			tf_delete(gTimerColorData[i][j]);
		}
	}

	// Free any allocated graph data mem.
	for (uint32_t i = 0; i < gPlotModeData.size(); ++i)
	{
		tf_delete(gPlotModeData[i].mTimeData);
	}

	gWidgetTable.set_capacity(0);
	gTimerData.set_capacity(0);
	gTimerColorData.set_capacity(0);
	gPlotModeData.set_capacity(0);
	gDetailedModeTooltips.set_capacity(0);
	gDetailedModeWidgets.set_capacity(0);
	gDetailedModeDump.set_capacity(0);
	gPlotModeWidgets.set_capacity(0);
	gFrameTimerTitle.set_capacity(0);
	for (uint32_t i = 0; i < PROFILE_MAX_GROUPS; ++i)
	{
		gGPUTimerTitle[i].set_capacity(0);
	}
	gUnloaded = true;
}

void exitProfilerUI()
{
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
	float fAverage = fsToMs * (S.Aggregate[nTimerIndex].nTicks / nAggregateFrames);
	eastl::string printableString = pRoot->mName + eastl::string(" - ") + eastl::to_string(fAverage) + eastl::string(" ms");
	pAppUIRef->pImpl->pFontStash->drawText(
		pCmd, printableString.c_str(), pos.x, pos.y, pDrawDesc->mFontID, pDrawDesc->mFontColor,
		pDrawDesc->mFontSize, pDrawDesc->mFontSpacing, pDrawDesc->mFontBlur);

	float2 textSizePx = pAppUIRef->MeasureText(printableString.c_str(), *pDrawDesc);
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

	float2 totalTextSizePx = pAppUIRef->MeasureText(titleStr, *pDesc);
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
	eastl::string txt = eastl::string().sprintf("CPU %f ms", getCpuAvgFrameTime());
	pAppUIRef->DrawText(pCmd, screenCoordsInPx, txt.c_str(), pDesc);

	float2 totalTextSizePx = pAppUIRef->MeasureText(txt.c_str(), *pDesc);
	return totalTextSizePx;
}
/// Draws the top menu and draws the selected timer mode.
void profileLoadWidgetUI(Profile& S)
{
	// Reset
	unloadProfilerUI();

	eastl::vector<IWidget*> topMenu;
	topMenu.push_back(tf_placement_new<DropdownWidget>(tf_calloc(1, sizeof(DropdownWidget)), "Select Profile Mode", (uint32_t*)&gProfileMode, pProfileModesNames, gProfileModesValues, PROFILE_MODE_MAX));
	topMenu.push_back(tf_placement_new<DropdownWidget>(tf_calloc(1, sizeof(DropdownWidget)), "Aggregate Frames", (uint32_t*)&gAggregateFrames, pAggregateFramesNames, gAggregateFramesValues, PROFILE_AGGREGATE_FRAMES_MAX));
	topMenu.push_back(tf_placement_new<DropdownWidget>(tf_calloc(1, sizeof(DropdownWidget)), "Reference Times", (uint32_t*)&gReferenceTime, pReferenceTimesNames, gReferenceTimesValues, PROFILE_REFTIME_MAX));
	topMenu.back()->pOnEdited = ProfileCallbkReferenceTimeUpdated;

	// Don't dump frames to disk in detailed mode.
	if (gProfileMode == PROFILE_MODE_DETAILED)
	{
		gDumpFramesNow = true;
		topMenu.push_back(tf_placement_new<DropdownWidget>(tf_calloc(1, sizeof(DropdownWidget)), "Dump Frames Detailed View", (uint32_t*)&gDumpFramesDetailedMode, pDumpFramesDetailedViewNames, gDumpFramesDetailedValues, PROFILE_DUMPFRAME_MAX));
		topMenu.back()->pOnEdited = profileCallbkDumpFrames;
	}
	else
	{
		topMenu.push_back(tf_placement_new<DropdownWidget>(tf_calloc(1, sizeof(DropdownWidget)), "Dump Frames To File", (uint32_t*)&gDumpFramesToFile, pDumpFramesToFileNames, gDumpFramesToFileValues, PROFILE_DUMPFILE_MAX));
		topMenu.back()->pOnEdited = profileCallbkDumpFramesToFile;
	}

	topMenu.push_back(tf_placement_new<CheckboxWidget>(tf_calloc(1, sizeof(CheckboxWidget)), "Profiler Paused", &gProfilerPaused));
	topMenu.back()->pOnEdited = profileCallbkPauseProfiler;
	pWidgetGuiComponent->AddWidget(ColumnWidget("topmenu", topMenu));
	gWidgetTable.push_back(topMenu);
	pWidgetGuiComponent->AddWidget(SeparatorWidget());

	if (gProfileMode == PROFILE_MODE_TIMER)
	{
		pWidgetGuiComponent->AddWidget(PlotLinesWidget("", gFrameTimeData, FRAME_HISTORY_LEN, &gMinPlotReferenceTime, &S.fReferenceTime, &gHistogramSize, &gFrameTimerTitle));
		for (uint32_t i = 0; i < S.nGroupCount; ++i)
		{
			if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
			{
				pWidgetGuiComponent->AddWidget(PlotLinesWidget("", gGPUFrameTimeData[i], FRAME_HISTORY_LEN, &gMinPlotReferenceTime, &S.fReferenceTime, &gHistogramSize, &gGPUTimerTitle[i]));
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
					pWidgetGuiComponent->AddWidget(DrawCurveWidget("Curves", graphTimer.mTimeData, FRAME_HISTORY_LEN, 1.f, color));
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

	gCurrWindowSize = float2(pWidgetGuiComponent->mCurrentWindowRect.getZ(), pWidgetGuiComponent->mCurrentWindowRect.getW());
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
		gFrameTimerTitle = eastl::string("Frame: Time[") + (eastl::to_string(gFrameTime))
			+ eastl::string("ms] Avg[") + eastl::to_string(fToMs * (S.nFlipAggregateDisplay / nAggregateFrames))
			+ eastl::string("ms] Min[") + eastl::to_string(fToMs * S.nFlipMinDisplay)
			+ eastl::string("ms] Max[") + eastl::to_string(fToMs * S.nFlipMaxDisplay)
			+ eastl::string("ms]");
		for (uint32_t i = 0; i < S.nGroupCount; ++i)
		{
			if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
			{
				gGPUFrameTimeData[i][frameNum] = gGPUFrameTime[i];
				gGPUTimerTitle[i] = eastl::string(S.GroupInfo[i].pName) + eastl::string(": Time[") + (eastl::to_string(gGPUFrameTime[i]))
					+ eastl::string("ms]");
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

void loadProfilerUI(UIApp* uiApp, int32_t width, int32_t height)
{
	// Remove previous GUI component.
	if (pWidgetGuiComponent)
	{
		pAppUIRef->RemoveGuiComponent(pWidgetGuiComponent);
	}

	pAppUIRef = uiApp;

	GuiDesc guiDesc = {};
	guiDesc.mStartSize = profileUtilCalcWindowSize(width, height);
	guiDesc.mStartPosition = vec2(PROFILER_WINDOW_X, PROFILER_WINDOW_Y);
	pWidgetGuiComponent = pAppUIRef->AddGuiComponent(" Micro Profiler ", &guiDesc);

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

	pMenuGuiComponent = pAppUIRef->AddGuiComponent("Micro Profiler", &guiMenuDesc);
	pMenuGuiComponent->AddWidget(CheckboxWidget("Toggle Profiler", &gProfilerWidgetUIEnabled));
	pMenuGuiComponent->AddWidget(SeparatorWidget());
	ButtonWidget dumpButtonWidget("Dump Profile");
	dumpButtonWidget.pOnDeactivatedAfterEdit = profileCallbkDumpFramesToFile;
	pMenuGuiComponent->AddWidget(dumpButtonWidget);
	pMenuGuiComponent->AddWidget(SeparatorWidget());
	pMenuGuiComponent->AddWidget(SliderFloatWidget("Transparency", &gGuiTransparency, 0.0f, 0.9f, 0.01f, "%.2f"));
}

void cmdDrawProfilerUI()
{
	if (!pAppUIRef)
		return;
	PROFILER_SET_CPU_SCOPE("MicroProfilerUI", "WidgetsUI", 0x737373);

	ASSERT(pAppUIRef); // Must be initialized through loadProfilerUI
	pMenuGuiComponent->mAlpha = 1.0f - gGuiTransparency;
	pAppUIRef->Gui(pMenuGuiComponent);
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
		pAppUIRef->Gui(pWidgetGuiComponent);

	} // profiler enabled.

}

#endif // PROFILE_ENABLED
