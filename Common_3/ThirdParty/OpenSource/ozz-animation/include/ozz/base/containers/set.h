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

#ifndef OZZ_OZZ_BASE_CONTAINERS_SET_H_
#define OZZ_OZZ_BASE_CONTAINERS_SET_H_

#include <set>

#include "../../EASTL/set.h"

namespace ozz {
// Redirects std::set to ozz::Set in order to replace std default allocator by
// ozz::StdAllocator.
template <class _Key, class _Pred = eastl::less<_Key>,
          class _Allocator = EASTLAllocatorType<_Key> >
struct Set {
  typedef eastl::set<_Key, _Pred, _Allocator> Std;
};

// Redirects std::multiset to ozz::MultiSet in order to replace std default
// allocator by ozz::StdAllocator.
template <class _Key, class _Pred = eastl::less<_Key>,
          class _Allocator = EASTLAllocatorType<_Key> >
struct MultiSet {
  typedef eastl::multiset<_Key, _Pred, _Allocator> Std;
};
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_SET_H_
