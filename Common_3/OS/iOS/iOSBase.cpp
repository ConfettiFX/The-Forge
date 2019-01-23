/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#ifdef __APPLE__

#include <ctime>

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IPlatformEvents.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/ITimeManager.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IMemoryManager.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static bool                         gAppRunning;
static WindowsDesc*                 gCurrentWindow = nullptr;
static tinystl::vector<MonitorDesc> gMonitors;
static int                          gCurrentTouchEvent = 0;

static tinystl::vector<MonitorDesc> monitors;

// Update the state of the keys based on state previous frame
void updateTouchEvent(int numTaps) { gCurrentTouchEvent = numTaps; }

int getTouchEvent()
{
	int prevTouchEvent = gCurrentTouchEvent;
	gCurrentTouchEvent = 0;
	return prevTouchEvent;
}

bool isRunning() { return gAppRunning; }

void requestShutDown() { gAppRunning = false; }

void openWindow(const char* app_name, WindowsDesc* winDesc) { gCurrentWindow = winDesc; }

// Function needed to access the current window descriptor from the GameViewController.
WindowsDesc* getCurrentWindow() { return gCurrentWindow; }

void getRecommendedResolution(RectDesc* rect)
{
	// TODO: Get recommeneded resolution based on the device.
	*rect = RectDesc{ 0, 0, 1334, 750 };
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes) {}

/************************************************************************/
// Time Related Functions
/************************************************************************/

unsigned getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds

	ms += s * 1000;

	return (unsigned int)ms;
}

long long getUSec()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

#endif
