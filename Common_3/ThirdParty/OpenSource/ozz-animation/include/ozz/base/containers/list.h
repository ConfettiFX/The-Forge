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

#ifndef OZZ_OZZ_BASE_CONTAINERS_LIST_H_
#define OZZ_OZZ_BASE_CONTAINERS_LIST_H_

#ifdef _MSC_VER
#pragma warning(push)
// Removes constant conditional expression warning.
#pragma warning(disable : 4127)
#endif  // _MSC_VER

#include <list>

#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER

#include "ozz/base/containers/std_allocator.h"
#include "../../../../../../OpenSource/EASTL/list.h"

namespace ozz {
// Redirects std::list to ozz::List in order to replace std default allocator by
// ozz::StdAllocator.
template <class _Ty, class _Allocator = ozz::StdAllocator<_Ty> >
struct List {
  typedef eastl::list<_Ty, _Allocator> Std;
};
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_LIST_H_
