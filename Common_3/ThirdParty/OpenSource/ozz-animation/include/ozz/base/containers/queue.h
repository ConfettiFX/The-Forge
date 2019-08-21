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

#ifndef OZZ_OZZ_BASE_CONTAINERS_QUEUE_H_
#define OZZ_OZZ_BASE_CONTAINERS_QUEUE_H_

#include <queue>

#include "deque.h"
#include "../../EASTL/deque.h"
#include "../../EASTL/queue.h"
#include "../../EASTL/priority_queue.h"

namespace ozz {
// Redirects std::queue to ozz::Queue in order to replace std default allocator
// by ozz::StdAllocator.
template <class _Ty, class _Container = typename ozz::Deque<_Ty>::Std>
struct Queue {
  typedef eastl::queue<_Ty, _Container> Std;
};

// Redirects std::priority_queue to ozz::PriorityQueue in order to replace std
// default allocator by ozz::StdAllocator.
template <class _Ty, class _Container = typename ozz::Deque<_Ty>::Std,
          class _Pred = eastl::less<typename _Container::value_type> >
struct PriorityQueue {
  typedef eastl::priority_queue<_Ty, _Container, _Pred> Std;
};
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_QUEUE_H_
