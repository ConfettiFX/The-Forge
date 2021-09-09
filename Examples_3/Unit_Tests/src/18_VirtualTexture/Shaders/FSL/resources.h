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

#ifndef RESOURCES_H
#define RESOURCES_H

CBUFFER(VTBufferInfo, UPDATE_FREQ_PER_FRAME, b3, binding = 3)
{
	DATA(uint, TotalPageCount, None);
	DATA(uint, CurrentFrameOffset, None);
};

RES(RWBuffer(uint), VTReadbackBuffer, UPDATE_FREQ_PER_FRAME, u1, binding = 1);
RES(RWBuffer(uint), VTVisBuffer, UPDATE_FREQ_PER_FRAME, u2, binding = 2);

#define VT_CURRENT_FRAME_OFFSET()     (Get(CurrentFrameOffset))
#define VT_TOTAL_PAGE_COUNT()         (Get(TotalPageCount))

#if 0
uint vtMakeSafeIndex(uint index)
{
	return (index >= VT_TOTAL_PAGE_COUNT()) ? 0x00FFFFFF : index;
}
#else
#define vtMakeSafeIndex(index) (index)
#endif

#define VT_ALIVE_PAGE_COUNT()         (VTReadbackBuffer[VT_CURRENT_FRAME_OFFSET() + 0])
#define VT_REMOVE_PAGE_COUNT()        (VTReadbackBuffer[VT_CURRENT_FRAME_OFFSET() + 1])
#define VT_ALIVE_PAGES(index)         (VTReadbackBuffer[VT_CURRENT_FRAME_OFFSET() + 2 + vtMakeSafeIndex(index)])
#define VT_REMOVE_PAGES(index)        (VTReadbackBuffer[VT_CURRENT_FRAME_OFFSET() + 2 + VT_TOTAL_PAGE_COUNT() + vtMakeSafeIndex(index)])
#define VT_VISIBLE_PAGES(index)       (VTVisBuffer[(index) * 2])
#define VT_PREV_VISIBLE_PAGES(index)  (VTVisBuffer[(index) * 2 + 1])

#endif
