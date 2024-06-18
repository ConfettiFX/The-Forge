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

#include "../../Application/Config.h"

// Don't compile stbtt unless we need it for UI or fonts
#if defined ENABLE_FORGE_UI || defined ENABLE_FORGE_FONTS

#include "../../Utilities/Interfaces/ILog.h"
// Previous implementation defined all stb math functions as imgui math functions
// However, imgui math functions expand to standard c math functions, so no need to define unless we want custom math
// Also we must check for implementation defines in case the user already has an implementation in the _same_ compilation unit (e.g. unity
// builds)

#ifndef STB_RECT_PACK_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_ASSERT(x) ASSERT(x)
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_rectpack.h"
#endif

#include "../../Utilities/Interfaces/IMemory.h"

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert(x) ASSERT(x)

static void* stbtt_alloc_func(size_t size, void* user_data)
{
    UNREF_PARAM(user_data);
    return tf_malloc(size);
}

static void stbtt_dealloc_func(void* ptr, void* user_data)
{
    UNREF_PARAM(user_data);
    tf_free(ptr);
}

#define STBTT_malloc(x, u) stbtt_alloc_func(x, u)
#define STBTT_free(x, u)   stbtt_dealloc_func(x, u)

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_truetype.h"
#endif

#endif // UI OR FONTS