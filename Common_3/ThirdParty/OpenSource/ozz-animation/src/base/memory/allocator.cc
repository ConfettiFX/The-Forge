//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
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


//CONFFX_BEGIN

#include "../../../include/ozz/base/memory/allocator.h"

#include <memory.h>

#include <atomic>
#include <cassert>
//#include <cstdlib>

#include "../../../include/ozz/base/maths/math_ex.h"

#include "../../../../OS/Interfaces/IMemory.h"
//CONFFX_END


namespace ozz {
namespace memory {

namespace {
struct Header {
  void* unaligned;
  size_t size;
};
}  // namespace

// Implements the basic heap allocator.
// Will trace allocation count and assert in case of a memory leak.
class HeapAllocator : public Allocator {
 public:
  HeapAllocator() { allocation_count_.store(0); }
  ~HeapAllocator() {
    assert(allocation_count_.load() == 0 && "Memory leak detected");
  }

 protected:
//CONFFX_BEGIN

  void* Allocate(size_t _size, size_t _alignment) {
	  // Allocates enough memory to store the header + required alignment space.
	  const size_t to_allocate = _size + sizeof(Header) + _alignment - 1;
	  char* unaligned = reinterpret_cast<char*>(tf_malloc(to_allocate));

	  if (!unaligned) {
		  return nullptr;
	  }
	  char* aligned = ozz::math::Align(unaligned + sizeof(Header), _alignment);
	  assert(aligned + _size <= unaligned + to_allocate);  // Don't overrun.
	  // Set the header
	  Header* header = reinterpret_cast<Header*>(aligned - sizeof(Header));
	  assert(reinterpret_cast<char*>(header) >= unaligned);
	  header->unaligned = unaligned;
	  header->size = _size;
	  // Allocation's succeeded.
	  return aligned;
  }
//CONFFX_END

//CONFFX_BEGIN
  void Deallocate(void* _block) {
	  if (_block) {
		  Header* header = reinterpret_cast<Header*>(
			  reinterpret_cast<char*>(_block) - sizeof(Header));
		  tf_free(header->unaligned);

		  // Deallocation completed.
	  }
  }
//CONFFX_END


 private:
  // Internal allocation count used to track memory leaks.
  // Should equals 0 at destruction time.
  std::atomic_int allocation_count_;
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
