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

#include "../../../include/ozz/animation/offline/track_optimizer.h"


#include <cstddef>

// Internal include file
#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.
#include "../src/animation/offline/decimate.h"

#include "../../../include/ozz/base/maths/math_ex.h"

#include "../../../include/ozz/animation/offline/raw_track.h"

// Needs runtime track to access TrackPolicy.
#include "../../../include/ozz/animation/runtime/track.h"

namespace ozz {
namespace animation {
namespace offline {

// Setup default values (favoring quality).
TrackOptimizer::TrackOptimizer() : tolerance(1e-3f) {  // 1 mm.
}

namespace {

template <typename _KeyFrame>
struct Adapter {
  typedef typename _KeyFrame::ValueType ValueType;
  typedef typename animation::internal::TrackPolicy<ValueType> Policy;

  Adapter() {}

  bool Decimable(const _KeyFrame& _key) const {
    // RawTrackInterpolation::kStep keyframes aren't optimized, as steps can't
    // be interpolated.
    return _key.interpolation != RawTrackInterpolation::kStep;
  }

  _KeyFrame Lerp(const _KeyFrame& _left, const _KeyFrame& _right,
                 const _KeyFrame& _ref) const {
    ASSERT(Decimable(_ref));
    const float alpha =
        (_ref.ratio - _left.ratio) / (_right.ratio - _left.ratio);
    ASSERT(alpha >= 0.f && alpha <= 1.f);
    const _KeyFrame key = {_ref.interpolation, _ref.ratio,
                           Policy::Lerp(_left.value, _right.value, alpha)};
    return key;
  }

  float Distance(const _KeyFrame& _a, const _KeyFrame& _b) const {
    return Policy::Distance(_a.value, _b.value);
  }
};

template <typename _Track>
inline bool Optimize(float _tolerance, const _Track& _input, _Track* _output) {
  if (!_output) {
    return false;
  }
  // Reset output animation to default.
  *_output = _Track();

  // Validate animation.
  if (!_input.Validate()) {
    return false;
  }

  // Copy name
  _output->name = _input.name;

  // Optimizes.
  const Adapter<typename _Track::Keyframe> adapter;
  Decimate(_input.keyframes, adapter, _tolerance, &_output->keyframes);

  // Output animation is always valid though.
  return _output->Validate();
}
}  // namespace

bool TrackOptimizer::operator()(const RawFloatTrack& _input,
                                RawFloatTrack* _output) const {
  return Optimize(tolerance, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat2Track& _input,
                                RawFloat2Track* _output) const {
  return Optimize(tolerance, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat3Track& _input,
                                RawFloat3Track* _output) const {
  return Optimize(tolerance, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat4Track& _input,
                                RawFloat4Track* _output) const {
  return Optimize(tolerance, _input, _output);
}
bool TrackOptimizer::operator()(const RawQuaternionTrack& _input,
                                RawQuaternionTrack* _output) const {
  return Optimize(1.f - std::cos(.5f * tolerance), _input, _output);
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
