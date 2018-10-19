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

#ifndef OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_
#define OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_

#include <cstddef>
#include <new>

#include "../platform.h"

namespace ozz {
namespace memory {

// Forwards declare Allocator class.
class Allocator;

// Declares a default alignment value.
static const size_t kDefaultAlignment = 16;

// Defines the default allocator accessor.
Allocator* default_allocator();

// Set the default allocator, used for all dynamic allocation inside ozz.
// Returns current memory allocator, such that in can be restored if needed.
Allocator* SetDefaulAllocator(Allocator* _allocator);

// Defines an abstract allocator class.
// Implements helper methods to allocate/deallocate POD typed objects instead of
// raw memory.
// Implements New and Delete function to allocate C++ objects, as a replacement
// of new and delete operators.
class Allocator {
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

  // Changes the size of a block that was allocated with Allocate.
  // Argument _block can be NULL.
  // Reallocate function conforms with standard realloc function specifications.
  virtual void* Reallocate(void* _block, size_t _size, size_t _alignment) = 0;

  // Next functions are helper functions used to provide typed and ranged
  // allocations.

  // Allocates an array of _count objects of type _Ty. Alignment is
  // automatically deduced from _Ty type.
  // Allocate function conforms with standard malloc function specifications.
  template <typename _Ty>
  _Ty* Allocate(size_t _count) {
    return reinterpret_cast<_Ty*>(
        Allocate(_count * sizeof(_Ty), OZZ_ALIGN_OF(_Ty)));
  }

  // Allocates a range of _count objects of type _Ty. Alignment is
  // automatically deduced from _Ty type.
  // AllocateRange function conforms with standard malloc function
  // specifications.
  template <typename _Ty>
  Range<_Ty> AllocateRange(size_t _count) {
    _Ty* alloc = reinterpret_cast<_Ty*>(
        Allocate(_count * sizeof(_Ty), OZZ_ALIGN_OF(_Ty)));
    return Range<_Ty>(alloc, alloc ? _count : 0);
  }

  // Frees a block that was allocated with Allocate, AllocateRange or Reallocate
  // functions of *this allocator.
  // Argument _range can be an empty (NULL) range.
  // Deallocate function conforms with standard free function specifications.
  template <typename _Ty>
  void Deallocate(Range<_Ty>& _range) {
    Deallocate(_range.begin);
    _range = Range<_Ty>();
  }

  // Changes the size of a block that was allocated with Allocate,
  // AllocateRange and Reallocate of *this allocator.
  // Reallocate function conforms with standard realloc function specifications.
  template <typename _Ty>
  _Ty* Reallocate(_Ty* _block, size_t _count) {
    return reinterpret_cast<_Ty*>(
        Reallocate(_block, _count * sizeof(_Ty), OZZ_ALIGN_OF(_Ty)));
  }

  // Changes the size of a range that was allocated with Allocate,
  // AllocateRange and Reallocate of *this allocator.
  template <typename _Ty>
  void Reallocate(Range<_Ty>& _range, size_t _count) {
    _Ty* alloc = reinterpret_cast<_Ty*>(
        Reallocate(_range.begin, _count * sizeof(_Ty), OZZ_ALIGN_OF(_Ty)));
    _range = Range<_Ty>(alloc, alloc ? _count : 0);
  }

  // Replaces operator new with no argument.
  // New function conforms with standard operator new specifications.
  template <typename _Ty>
  _Ty* New() {
    return new (Allocate<_Ty>(1)) _Ty;
  }

  // Replaces operator new with one argument.
  // New function conforms with standard operator new specifications.
  template <typename _Ty, typename _Arg0>
  _Ty* New(const _Arg0& _arg0) {
    return new (Allocate<_Ty>(1)) _Ty(_arg0);
  }

  // Replaces operator new with two arguments.
  // New function conforms with standard operator new specifications.
  template <typename _Ty, typename _Arg0, typename _Arg1>
  _Ty* New(const _Arg0& _arg0, const _Arg1& _arg1) {
    return new (Allocate<_Ty>(1)) _Ty(_arg0, _arg1);
  }

  // Replaces operator new with three arguments.
  // New function conforms with standard operator new specifications.
  template <typename _Ty, typename _Arg0, typename _Arg1, typename _Arg2>
  _Ty* New(const _Arg0& _arg0, const _Arg1& _arg1, const _Arg2& _arg2) {
    return new (Allocate<_Ty>(1)) _Ty(_arg0, _arg1, _arg2);
  }

  // Replaces operator new with four arguments.
  // New function conforms with standard operator new specifications.
  template <typename _Ty, typename _Arg0, typename _Arg1, typename _Arg2,
            typename _Arg3>
  _Ty* New(const _Arg0& _arg0, const _Arg1& _arg1, const _Arg2& _arg2,
           const _Arg3& _arg3) {
    return new (Allocate<_Ty>(1)) _Ty(_arg0, _arg1, _arg2, _arg3);
  }

  // Replaces operator delete for objects allocated using one of the New
  // functions ot *this allocator.
  // Delete function conforms with standard operator delete specifications.
  template <typename _Ty>
  void Delete(_Ty* _object) {
    if (_object) {
      _object->~_Ty();
      Deallocate(_object);
    }
  }
};
}  // namespace memory
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MEMORY_ALLOCATOR_H_
