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

#include "ozz/animation/offline/track_builder.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

#include "ozz/base/memory/allocator.h"

#include "ozz/animation/offline/raw_track.h"

#include "ozz/animation/runtime/track.h"

namespace ozz {
namespace animation {
namespace offline {

namespace {

template <typename _RawTrack>
void PatchBeginEndKeys(const _RawTrack& _input,
                       typename _RawTrack::Keyframes* keyframes) {
  if (_input.keyframes.empty()) {
    const typename _RawTrack::ValueType default_value =
        animation::internal::TrackPolicy<
            typename _RawTrack::ValueType>::identity();

    const typename _RawTrack::Keyframe begin = {RawTrackInterpolation::kLinear,
                                                0.f, default_value};
    keyframes->push_back(begin);
    const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                              1.f, default_value};
    keyframes->push_back(end);
  } else if (_input.keyframes.size() == 1) {
    const typename _RawTrack::Keyframe& src_key = _input.keyframes.front();
    const typename _RawTrack::Keyframe begin = {RawTrackInterpolation::kLinear,
                                                0.f, src_key.value};
    keyframes->push_back(begin);
    const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                              1.f, src_key.value};
    keyframes->push_back(end);
  } else {
    // Copy all source data.
    // Push an initial and last keys if they don't exist.
    if (_input.keyframes.front().ratio != 0.f) {
      const typename _RawTrack::Keyframe& src_key = _input.keyframes.front();
      const typename _RawTrack::Keyframe begin = {
          RawTrackInterpolation::kLinear, 0.f, src_key.value};
      keyframes->push_back(begin);
    }
    for (size_t i = 0; i < _input.keyframes.size(); ++i) {
      keyframes->push_back(_input.keyframes[i]);
    }
    if (_input.keyframes.back().ratio != 1.f) {
      const typename _RawTrack::Keyframe& src_key = _input.keyframes.back();
      const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                                1.f, src_key.value};
      keyframes->push_back(end);
    }
  }
}

template <typename _Keyframes>
void Fixup(_Keyframes* _keyframes) {
  // Nothing to do by default.
  (void)_keyframes;
}
}  // namespace

// Ensures _input's validity and allocates _animation.
// An animation needs to have at least two key frames per joint, the first at
// t = 0 and the last at t = 1. If at least one of those keys are not
// in the RawAnimation then the builder creates it.
template <typename _RawTrack, typename _Track>
_Track* TrackBuilder::Build(const _RawTrack& _input) const {
  // Tests _raw_animation validity.
  if (!_input.Validate()) {
    return NULL;
  }

  // Everything is fine, allocates and fills the animation.
  // Nothing can fail now.
  _Track* track = memory::default_allocator()->New<_Track>();

  // Copy data to temporary prepared data structure
  typename _RawTrack::Keyframes keyframes;
  // Guessing a worst size to avoid realloc.
  const size_t worst_size =
      _input.keyframes.size() * 2 +  // * 2 in case all keys are kStep
      2;                             // + 2 for first and last keys
  keyframes.reserve(worst_size);

  // Ensure there's a key frame at the start and end of the track (required for
  // sampling).
  PatchBeginEndKeys(_input, &keyframes);

  // Fixup values, ex: successive opposite quaternions that would fail to take
  // the shortest path during the normalized-lerp.
  Fixup(&keyframes);

  // Allocates output track.
  const size_t name_len = _input.name.size();
  track->Allocate(keyframes.size(), _input.name.size());

  // Copy all keys to output.
  assert(keyframes.size() == track->ratios_.count() &&
         keyframes.size() == track->values_.count() &&
         keyframes.size() <= track->steps_.count() * 8);
  memset(track->steps_.begin, 0, track->steps_.size());
  for (size_t i = 0; i < keyframes.size(); ++i) {
    const typename _RawTrack::Keyframe& src_key = keyframes[i];
    track->ratios_[i] = src_key.ratio;
    track->values_[i] = src_key.value;
    track->steps_[i / 8] |=
        (src_key.interpolation == RawTrackInterpolation::kStep) << (i & 7);
  }

  // Copy track's name.
  if (name_len) {
    strcpy(track->name_, _input.name.c_str());
  }

  return track;  // Success.
}

FloatTrack* TrackBuilder::operator()(const RawFloatTrack& _input) const {
  return Build<RawFloatTrack, FloatTrack>(_input);
}
Float2Track* TrackBuilder::operator()(const RawFloat2Track& _input) const {
  return Build<RawFloat2Track, Float2Track>(_input);
}
Float3Track* TrackBuilder::operator()(const RawFloat3Track& _input) const {
  return Build<RawFloat3Track, Float3Track>(_input);
}
Float4Track* TrackBuilder::operator()(const RawFloat4Track& _input) const {
  return Build<RawFloat4Track, Float4Track>(_input);
}

namespace {
// Fixes-up successive opposite quaternions that would fail to take the shortest
// path during the lerp.
//CONFFX_BEGIN
template <>
void Fixup<RawQuaternionTrack::Keyframes>(
    RawQuaternionTrack::Keyframes* _keyframes) {
  assert(_keyframes->size() >= 2);

  const Quat identity = Quat::identity();
  for (size_t i = 0; i < _keyframes->size(); ++i) {
    RawQuaternionTrack::ValueType& src_key = _keyframes->at(i).value;

    // Normalizes input quaternion.
    if (norm(src_key) != 0.f) 
	{
      src_key = normalize(src_key);
    } 
	else 
	{
      src_key = identity;
    }

    // Ensures quaternions are all on the same hemisphere.
    if (i == 0) {
      if (src_key.getW() < 0.f) {
        src_key = -src_key;  // Q an -Q are the same rotation.
      }
    } else {
      RawQuaternionTrack::ValueType& prev_key = _keyframes->at(i - 1).value;
      const float dot = src_key.getX() * prev_key.getX() + src_key.getY() * prev_key.getY() +
                        src_key.getZ() * prev_key.getZ() + src_key.getW() * prev_key.getW();
      if (dot < 0.f) {
        src_key = -src_key;  // Q an -Q are the same rotation.
      }
    }
  }
}
//CONFFX_END
}  // namespace

QuaternionTrack* TrackBuilder::operator()(
    const RawQuaternionTrack& _input) const {
  return Build<RawQuaternionTrack, QuaternionTrack>(_input);
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
