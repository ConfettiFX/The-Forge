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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_

#include "../../../../include/ozz/animation/offline/export.h"
#include "../../../../include/ozz/animation/offline/raw_animation.h"

#include "../../../../include/ozz/base/span.h"

namespace ozz {
namespace animation {
namespace offline {

// Translation interpolation method.
OZZ_ANIMOFFLINE_DLL Vector3 LerpTranslation(const Vector3& _a,
                                                 const Vector3& _b,
                             float _alpha);

// Rotation interpolation method.
OZZ_ANIMOFFLINE_DLL Quat LerpRotation(const Quat& _a,
                              const Quat& _b, float _alpha);

// Scale interpolation method.
OZZ_ANIMOFFLINE_DLL Vector3 LerpScale(const Vector3& _a,
                                           const Vector3& _b,
                       float _alpha);

// Samples a RawAnimation track. This function shall be used for offline
// purpose. Use ozz::animation::Animation and ozz::animation::SamplingJob for
// runtime purpose.
// Returns false if track is invalid.
OZZ_ANIMOFFLINE_DLL bool SampleTrack(const RawAnimation::JointTrack& _track,
                                     float _time,
                 AffineTransform* _transform);

// Samples a RawAnimation. This function shall be used for offline
// purpose. Use ozz::animation::Animation and ozz::animation::SamplingJob for
// runtime purpose.
// _animation must be valid.
// Returns false output range is too small or animation is invalid.
OZZ_ANIMOFFLINE_DLL bool SampleAnimation(
    const RawAnimation& _animation, float _time,
                     const span<AffineTransform>& _transforms);

// Implement fixed rate keyframe time iteration. This utility purpose is to
// ensure that sampling goes strictly from 0 to duration, and that period
// between consecutive time samples have a fixed period.
// This sounds trivial, but floating point error could occur if keyframe time
// was accumulated for a long duration.
class OZZ_ANIMOFFLINE_DLL FixedRateSamplingTime {
 public:
  FixedRateSamplingTime(float _duration, float _frequency);

  float time(size_t _key) const {
    ASSERT(_key < num_keys_);
    return min(_key * period_, duration_);
  }

  size_t num_keys() const { return num_keys_; }

 private:
  float duration_;
  float period_;
  size_t num_keys_;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_
