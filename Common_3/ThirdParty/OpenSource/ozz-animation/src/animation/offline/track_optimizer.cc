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

#include "ozz/animation/offline/track_optimizer.h"

#include "ozz/base/maths/math_ex.h"

#include "ozz/animation/offline/raw_track.h"

// Needs runtime track to access TrackPolicy.
#include "ozz/animation/runtime/track.h"

namespace ozz {
namespace animation {
namespace offline {

// Setup default values (favoring quality).
TrackOptimizer::TrackOptimizer() : tolerance(1e-3f) {  // 1 mm.
}

namespace {

bool Compare(float _left, float _right, float _tolerance) {
  return fabs(_left - _right) <= _tolerance;
}

//CONFFX_BEGIN
// Returns true if the angle between _a and _b is less than _tolerance.
bool Compare(const Quat& _a, const Quat& _b, float _tolerance) {
  // Computes w component of a-1 * b.
  const float diff_w = _a.getX() * _b.getX() + _a.getY() * _b.getY() + _a.getZ() * _b.getZ() + _a.getW() * _b.getW();
  // Converts w back to an angle.
  const float angle = 2.f * acos(min(abs(diff_w), 1.f));
  return abs(angle) <= _tolerance;
}

// Returns true if the distance between _a and _b is less than _tolerance.
bool Compare(const Vector4& _a, const Vector4& _b, float _tolerance) {
  const Vector4 diff = _a - _b;
  return dot(diff, diff) <= _tolerance * _tolerance;
}
bool Compare(const Vector3& _a, const Vector3& _b, float _tolerance) {
  const Vector3 diff = _a - _b;
  return (dot(diff, diff) <= _tolerance * _tolerance);
}
bool Compare(const Vector2& _a, const Vector2& _b, float _tolerance) {
  const Vector2 diff = _a - _b;
  return dot(diff, diff) <= _tolerance * _tolerance;
}
//CONFFX_END

// Copy _src keys to _dest but except the ones that can be interpolated.
template <typename _Keyframes>
void Filter(const _Keyframes& _src, float _tolerance, _Keyframes* _dest) {
  typedef typename _Keyframes::value_type Keyframe;
  typedef typename Keyframe::ValueType ValueType;

  _dest->reserve(_src.size());

  // Only copies the key that cannot be interpolated from the others.
  size_t last_src_pushed = 0;  // Index (in src) of the last pushed key.
  for (size_t i = 0; i < _src.size(); ++i) {
    const Keyframe& current = _src[i];

    // First and last keys are always pushed.
    // RawTrackInterpolation::kStep keyframes aren't optimized, as steps can't
    // be interpolated.
    if (i == 0 || current.interpolation == RawTrackInterpolation::kStep) {
      _dest->push_back(_src[i]);
      last_src_pushed = i;
    } else if (i == _src.size() - 1) {
      // Don't push the last value if it's the same as last_src_pushed.
      const Keyframe& left = _src[last_src_pushed];
      if (!Compare(left.value, current.value, _tolerance)) {
        _dest->push_back(current);
        last_src_pushed = i;
      }
    } else {
      // Only inserts i key if keys in range ]last_src_pushed,i] cannot be
      // interpolated from keys last_src_pushed and i + 1.
      const Keyframe& left = _src[last_src_pushed];
      const Keyframe& right = _src[i + 1];
      for (size_t j = last_src_pushed + 1; j <= i; ++j) {
        const Keyframe& test = _src[j];
        const float alpha =
            (test.ratio - left.ratio) / (right.ratio - left.ratio);
        assert(alpha >= 0.f && alpha <= 1.f);
        const ValueType lerped =
            animation::internal::TrackPolicy<ValueType>::Lerp(
                left.value, right.value, alpha);
        if (!Compare(lerped, test.value, _tolerance)) {
          _dest->push_back(current);
          last_src_pushed = i;
          break;
        }
      }
    }
  }
  assert(_dest->size() <= _src.size());
}

template <typename _Track>
bool Optimize(const TrackOptimizer& _optimizer, const _Track& _input,
              _Track* _output) {
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
  Filter(_input.keyframes, _optimizer.tolerance, &_output->keyframes);

  // Output animation is always valid though.
  return _output->Validate();
}
}  // namespace

bool TrackOptimizer::operator()(const RawFloatTrack& _input,
                                RawFloatTrack* _output) const {
  return Optimize(*this, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat2Track& _input,
                                RawFloat2Track* _output) const {
  return Optimize(*this, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat3Track& _input,
                                RawFloat3Track* _output) const {
  return Optimize(*this, _input, _output);
}
bool TrackOptimizer::operator()(const RawFloat4Track& _input,
                                RawFloat4Track* _output) const {
  return Optimize(*this, _input, _output);
}
bool TrackOptimizer::operator()(const RawQuaternionTrack& _input,
                                RawQuaternionTrack* _output) const {
  return Optimize(*this, _input, _output);
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
