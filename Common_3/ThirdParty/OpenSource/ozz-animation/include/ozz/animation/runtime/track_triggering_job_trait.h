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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_TRACK_TRIGGERING_JOB_TRAIT_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_TRACK_TRIGGERING_JOB_TRAIT_H_

// Defines iterator traits required to use TrackTriggeringJob::Iterator
// with stl algorithms.
// This is a separate file from "track_triggering_job.h" to prevent everyone
// from including stl file <iterator>.

#include "ozz/animation/runtime/track_triggering_job.h"

#include <iterator>

// Specializes std::iterator_traits.
namespace std {
template <>
struct iterator_traits<ozz::animation::TrackTriggeringJob::Iterator> {
  typedef ptrdiff_t difference_type;
  typedef ozz::animation::TrackTriggeringJob::Edge value_type;
  typedef const ozz::animation::TrackTriggeringJob::Edge* pointer;
  typedef const ozz::animation::TrackTriggeringJob::Edge& reference;
  typedef forward_iterator_tag iterator_category;
};
}  // namespace std
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_TRACK_TRIGGERING_JOB_TRAIT_H_
