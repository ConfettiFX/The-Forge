//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "../../../include/ozz/base/memory/allocator.h"

#include <memory.h>

#include <atomic>

#include <cstdlib>

#include "../../../include/ozz/base/maths/math_ex.h"

#include "../../../../../../../../Utilities/Interfaces/IMemory.h"

namespace ozz {
namespace memory {

// Implements the basic heap allocator.
class HeapAllocator : public Allocator {
 protected:
  void* Allocate(size_t _size, size_t _alignment)
  {
      return tf_memalign(_alignment, _size);
  }

  void Deallocate(void* _block)
  {
      tf_free(_block);
  }
};

namespace {
// Instantiates the default heap allocator->
HeapAllocator g_heap_allocator;

// Instantiates the default heap allocator pointer.
Allocator* g_default_allocator = &g_heap_allocator;
}  // namespace

// Implements default allocator accessor.
Allocator* default_allocator() { return g_default_allocator; }

// Implements default allocator setter.
Allocator* SetDefaulAllocator(Allocator* _allocator) {
  Allocator* previous = g_default_allocator;
  g_default_allocator = _allocator;
  return previous;
}
}  // namespace memory
}  // namespace ozz
