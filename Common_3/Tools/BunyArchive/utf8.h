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

#include <inttypes.h>
#include <stddef.h>

static inline uint32_t utf8_charlen(uint8_t c)
{
    if (c < 0x80)
        return 1;
    if ((c & 0xe0) == 0xc0)
        return 2;
    if ((c & 0xf0) == 0xe0)
        return 3;
    if ((c & 0xf8) == 0xf0 && (c <= 0xf4))
        return 4;
    // invalid UTF8
    return 0;
}

static inline uint32_t utf8_valid(const uint8_t* c)
{
    uint32_t clen = utf8_charlen(*c);
    switch (clen)
    {
    // fall through
    case 4:
        if ((c[3] & 0xc0) != 0x80)
            return 0;
    // fall through
    case 3:
        if ((c[2] & 0xc0) != 0x80)
            return 0;
    // fall through
    case 2:
        if ((c[1] & 0xc0) != 0x80)
            return 0;
    // fall through
    case 1:
        return clen;
    case 0:
        return 0;
    }
    return clen;
}

static inline const uint8_t* utf8_to_utf32(const uint8_t* c, uint32_t* out)
{
    switch (utf8_valid(c))
    {
    case 0:
        *out = 0;
        return c + 1;
    case 1:
        *out = *c;
        return c + 1;
    case 2:
        *out = (uint32_t)((c[0] & 0x1f) << 6) | //
               (uint32_t)(c[1] & 0x3f);
        return c + 2;
    case 3:
        *out = (uint32_t)((c[0] & 0x0f) << 12) | (uint32_t)((c[1] & 0x3f) << 6) | //
               (uint32_t)(c[2] & 0x3f);
        return c + 3;
    case 4:
        *out = (uint32_t)((c[0] & 0x07) << 18) | (uint32_t)((c[1] & 0x3f) << 12) | (uint32_t)((c[2] & 0x3f) << 6) | //
               (uint32_t)(c[3] & 0x3f);
        return c + 4;
    }
    *out = 0;
    return c + 1;
}

static inline size_t utf8_strlen(const char* str)
{
    const uint8_t* cur = (const uint8_t*)str;

    size_t   i = 0;
    uint32_t c = 1;
    while (c && ++i)
        cur = utf8_to_utf32(cur, &c);
    return i;
}
