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

#ifndef OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_
#define OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_

#include <cstddef>
#include <new>
#include <utility>

#include "../../../../include/ozz/base/platform.h"
#include "../../../../include/ozz/base/span.h"

namespace ozz {
namespace memory {

// Forwards declare Allocator class.
class Allocator;

// Defines the default allocator accessor.
OZZ_BASE_DLL Allocator* default_allocator();

// Set the default allocator, used for all dynamic allocation inside ozz.
// Returns current memory allocator, such that in can be restored if needed.
OZZ_BASE_DLL Allocator* SetDefaulAllocator(Allocator* _allocator);

// Defines an abstract allocator class.
// Implements helper methods to allocate/deallocate POD typed objects instead of
// raw memory.
// Implements New and Delete function to allocate C++ objects, as a replacement
// of new and delete operators.
class OZZ_BASE_DLL Allocator {
 public:
  // Default virtual destructor.
  virtual ~Allocator() {}

  // Next functions are the pure virtual functions that must be implemented by
  // allocator concrete classes.

  // Allocates _size bytes on the specified _alignment boundaries.
  // Allocate function conforms with standard malloc function specifications.
  virtual void* Allocate(size_t _size, size_t _alignment) = 0;

  // Frees a block that was allocated with Allocate or Reallocate.
  // Argument _block can be NULL.
  // Deallocate function conforms with standard free function specifications.
  virtual void Deallocate(void* _block) = 0;

  template <typename T>
  ozz::span<T> AllocateRange(size_t num)
  {
      T* alloc = (T*)Allocate(sizeof(T) * num, alignof(T));
      return ozz::span<T>(alloc, num);
  }

  template <typename T>
  void Deallocate(ozz::span<T> s)
  {
      Deallocate(s.data());
  }
};
}  // namespace memory

// ozz replacement for c++ operator new with, used to allocate with an
// ozz::memory::Allocator. Delete must be used to deallocate such object.
// It can be used for constructor with no argument:
// Type* object = New<Type>();
// or any number of argument:
// Type* object = New<Type>(1,2,3,4);
template <typename _Ty, typename... _Args>
_Ty* New(_Args&&... _args) {
  void* alloc =
      memory::default_allocator()->Allocate(sizeof(_Ty), alignof(_Ty));
  return new (alloc) _Ty(std::forward<_Args>(_args)...);
}

template <typename _Ty>
void Delete(_Ty* _object) {
  if (_object) {
    // Prevents from false "unreferenced parameter" warning when _Ty has no
    // explicit destructor.
    (void)_object;
    _object->~_Ty();
    memory::default_allocator()->Deallocate(_object);
  }
}

}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_
