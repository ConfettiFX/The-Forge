/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#pragma once

#include "../GraphicsConfig.h"

#if defined(GLES)
struct WindowHandle;
typedef void* GLContext;
typedef void* GLConfig;
typedef void* GLSurface;

bool initGL(GLConfig* pOutConfig);

void removeGL(GLConfig* pConfig);

bool initGLContext(GLConfig config, GLContext* pOutContext, GLContext sharedContext = nullptr);

void removeGLContext(GLContext* pContext);

bool addGLSurface(GLContext context, GLConfig config, const WindowHandle* pWindowHandle, GLSurface* pOutSurface);

void removeGLSurface(GLContext context, GLConfig config, GLSurface* pSurface);

bool swapGLBuffers(GLSurface surface);

void setGLSwapInterval(bool enableVsync);

void getGLSurfaceSize(unsigned int* width, unsigned int* height);

void* getExtensionsFunction(const char* pExtFunctionName);

#endif