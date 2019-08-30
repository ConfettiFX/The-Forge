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

#include "ozz/animation/runtime/track_sampling_job.h"
#include "ozz/animation/runtime/track.h"
#include "ozz/base/maths/math_ex.h"

#include <cassert>

#include "../../EASTL/algorithm.h"

namespace ozz {
namespace animation {
namespace internal {

template <typename _Track>
TrackSamplingJob<_Track>::TrackSamplingJob()
    : ratio(0.f), track(NULL), result(NULL) {}

template <typename _Track>
bool TrackSamplingJob<_Track>::Validate() const {
  bool success = true;
  success &= result != NULL;
  success &= track != NULL;
  return success;
}

template <typename _Track>
bool TrackSamplingJob<_Track>::Run() const {
  if (!Validate()) {
    return false;
  }

  // Clamps ratio in range [0,1].
  const float clamped_ratio = math::Clamp(0.f, ratio, 1.f);

  // Search keyframes to interpolate.
  const Range<const float> ratios = track->ratios();
  const Range<const ValueType> values = track->values();
  assert(ratios.count() == values.count() &&
         track->steps().count() * 8 >= values.count());

  // Default track returns identity.
  if (ratios.count() == 0) {
    *result = internal::TrackPolicy<ValueType>::identity();
    return true;
  }

  // Search for the first key frame with a ratio value greater than input ratio.
  // Our ratio is between this one and the previous one.
  const float* ptk1 = eastl::upper_bound(ratios.begin, ratios.end, clamped_ratio);

  // Deduce keys indices.
  const size_t id1 = ptk1 - ratios.begin;
  const size_t id0 = id1 - 1;

  const bool id0step = (track->steps()[id0 / 8] & (1 << (id0 & 7))) != 0;
  if (id0step || ptk1 == ratios.end) {
    *result = values[id0];
  } else {
    // Lerp relevant keys.
    const float tk0 = ratios[id0];
    const float tk1 = ratios[id1];
    assert(clamped_ratio >= tk0 && clamped_ratio < tk1 && tk0 != tk1);
    const float alpha = (clamped_ratio - tk0) / (tk1 - tk0);
    const ValueType& vk0 = values[id0];
    const ValueType& vk1 = values[id1];
    *result = internal::TrackPolicy<ValueType>::Lerp(vk0, vk1, alpha);
  }
  return true;
}

// Explicitly instantiate supported tracks.
template struct TrackSamplingJob<FloatTrack>;
template struct TrackSamplingJob<Float2Track>;
template struct TrackSamplingJob<Float3Track>;
template struct TrackSamplingJob<Float4Track>;
template struct TrackSamplingJob<QuaternionTrack>;
}  // namespace internal
}  // namespace animation
}  // namespace ozz
