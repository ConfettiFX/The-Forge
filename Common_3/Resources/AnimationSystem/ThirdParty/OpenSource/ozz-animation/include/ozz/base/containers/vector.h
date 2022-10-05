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


#include "../../../../../../../../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

namespace ozz {
  
// Returns the mutable begin of the array of elements, or NULL if
// vector's empty.
template <class _Ty>
inline _Ty* array_begin(_Ty* _array) {
  return _array;
}

// Returns the mutable end of the array of elements, or NULL if
// vector's empty. Array end is one element past the last element of the
// array, it cannot be dereferenced.
template <class _Ty>
inline _Ty* array_end(_Ty* _array) {
  return _array + arrlen(_array);
}

}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_VECTOR_H_
