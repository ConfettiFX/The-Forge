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

#ifndef OZZ_OZZ_BASE_CONTAINERS_VECTOR_H_
#define OZZ_OZZ_BASE_CONTAINERS_VECTOR_H_

#include "../../../../../EASTL/vector.h"
#include "../platform.h"

namespace ozz {
// Redirects std::vector to ozz::vector in order to replace std default

template <class _Ty, class _Allocator = EASTLAllocatorType >
using vector = eastl::vector<_Ty, _Allocator>;

// Extends std::vector with two functions that gives access to the begin and the
// end of its array of elements.

// Returns the mutable begin of the array of elements, or nullptr if
// vector's empty.
template <class _Ty, class _Allocator>
inline _Ty* array_begin(eastl::vector<_Ty, _Allocator>& _vector) {
  return _vector.data();
}

// Returns the non-mutable begin of the array of elements, or nullptr if
// vector's empty.
template <class _Ty, class _Allocator>
inline const _Ty* array_begin(const eastl::vector<_Ty, _Allocator>& _vector) {
  return _vector.data();
}

// Returns the mutable end of the array of elements, or nullptr if
// vector's empty. Array end is one element past the last element of the
// array, it cannot be dereferenced.
template <class _Ty, class _Allocator>
inline _Ty* array_end(eastl::vector<_Ty, _Allocator>& _vector) {
  return _vector.data() + _vector.size();
}

// Returns the non-mutable end of the array of elements, or nullptr if
// vector's empty. Array end is one element past the last element of the
// array, it cannot be dereferenced.
template <class _Ty, class _Allocator>
inline const _Ty* array_end(const eastl::vector<_Ty, _Allocator>& _vector) {
  return _vector.data() + _vector.size();
}
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_VECTOR_H_
