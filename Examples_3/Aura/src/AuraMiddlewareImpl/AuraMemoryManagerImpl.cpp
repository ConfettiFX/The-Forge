/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#include "../../../../../Custom-Middleware/Aura/Interfaces/IAuraMemoryManager.h"
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

namespace aura {
void* alloc(size_t size) { return tf_malloc(size); }

void dealloc(void* ptr) { tf_free(ptr); }
}    // namespace aura