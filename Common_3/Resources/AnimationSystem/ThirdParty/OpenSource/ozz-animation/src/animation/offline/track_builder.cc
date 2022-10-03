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

#include "../../../include/ozz/animation/offline/track_builder.h"


#include <cmath>
#include <cstring>
#include <limits>

#include "../../../include/ozz/base/memory/allocator.h"

#include "../../../include/ozz/animation/offline/raw_track.h"

#include "../../../include/ozz/animation/runtime/track.h"
#include "../../../include/ozz/base/maths/simd_math_archive.h"

namespace ozz {
namespace animation {
namespace offline {

namespace {

template <typename _RawTrack>
void PatchBeginEndKeys(const _RawTrack& _input,
                       typename _RawTrack::Keyframes* keyframes) {
  if (arrlen(_input.keyframes) == 0) {
    const typename _RawTrack::ValueType default_value =
        animation::internal::TrackPolicy<
            typename _RawTrack::ValueType>::identity();

    const typename _RawTrack::Keyframe begin = {RawTrackInterpolation::kLinear,
                                                0.f, default_value};
    arrpush(*keyframes, begin);
    const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                              1.f, default_value};
    arrpush(*keyframes, end);
  } else if ((uint32_t)arrlen(_input.keyframes) == 1) {
    const typename _RawTrack::Keyframe& src_key = _input.keyframes[0];
    const typename _RawTrack::Keyframe begin = {RawTrackInterpolation::kLinear,
                                                0.f, src_key.value};
    arrpush(*keyframes, begin);
    const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                              1.f, src_key.value};
    arrpush(*keyframes, end);
  } else {
    // Copy all source data.
    // Push an initial and last keys if they don't exist.
    if (_input.keyframes[0].ratio != 0.f) {
      const typename _RawTrack::Keyframe& src_key = _input.keyframes[0];
      const typename _RawTrack::Keyframe begin = {
          RawTrackInterpolation::kLinear, 0.f, src_key.value};
      arrpush(*keyframes, begin);
    }
    for (size_t i = 0; i < (uint32_t)arrlen(_input.keyframes); ++i) {
      arrpush(*keyframes, _input.keyframes[i]);
    }
    if (arrlast(_input.keyframes).ratio != 1.f) {
      const typename _RawTrack::Keyframe& src_key = arrlast(_input.keyframes);
      const typename _RawTrack::Keyframe end = {RawTrackInterpolation::kLinear,
                                                1.f, src_key.value};
      arrpush(*keyframes, end);
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
bool TrackBuilder::Build(const _RawTrack& _input, _Track* track) const
{
  // Tests _raw_animation validity.
  if (!_input.Validate()) {
    return false;
  }

  // Copy data to temporary prepared data structure
  typename _RawTrack::Keyframes keyframes = nullptr;
  // Guessing a worst size to avoid realloc.
  const size_t worst_size =
      (uint32_t)arrlen(_input.keyframes) * 2 +  // * 2 in case all keys are kStep
      2;                             // + 2 for first and last keys
  arrsetcap(keyframes, worst_size);

  // Ensure there's a key frame at the start and end of the track (required for
  // sampling).
  PatchBeginEndKeys(_input, &keyframes);

  // Fixup values, ex: successive opposite quaternions that would fail to take
  // the shortest path during the normalized-lerp.
  Fixup(&keyframes);

  // Allocates output track.
  const size_t name_len = blength(&_input.name);
  track->Allocate((uint32_t)arrlen(keyframes), name_len);

  // Copy all keys to output.
  ASSERT((uint32_t)arrlen(keyframes) == track->ratios_.size() &&
         (uint32_t)arrlen(keyframes) == track->values_.size() &&
         (uint32_t)arrlen(keyframes) <= track->steps_.size() * 8);
  memset(track->steps_.data(), 0, track->steps_.size_bytes());
  for (size_t i = 0; i < (uint32_t)arrlen(keyframes); ++i) {
    const typename _RawTrack::Keyframe& src_key = keyframes[i];
    track->ratios_[i] = src_key.ratio;
    track->values_[i] = src_key.value;
    track->steps_[i / 8] |=
        (src_key.interpolation == RawTrackInterpolation::kStep) << (i & 7);
  }

  // Copy track's name.
  if (name_len) {
    strcpy(track->name_, (const char*)_input.name.data);
  }

  arrfree(keyframes);

  return true;  // Success.
}

bool TrackBuilder::operator()(const RawFloatTrack& _input, FloatTrack* pOut)
{
  return Build<RawFloatTrack, FloatTrack>(_input, pOut);
}
bool TrackBuilder::operator()(const RawFloat2Track& _input, Float2Track* pOut)
{
  return Build<RawFloat2Track, Float2Track>(_input, pOut);
}
bool TrackBuilder::operator()(const RawFloat3Track& _input, Float3Track* pOut)
{
  return Build<RawFloat3Track, Float3Track>(_input, pOut);
}
bool TrackBuilder::operator()(const RawFloat4Track& _input, Float4Track* pOut)
{
  return Build<RawFloat4Track, Float4Track>(_input, pOut);
}

namespace {
// Fixes-up successive opposite quaternions that would fail to take the shortest
// path during the lerp.
template <>
void Fixup<RawQuaternionTrack::Keyframes>(
    RawQuaternionTrack::Keyframes* _keyframes) {
  ASSERT((uint32_t)arrlen(*_keyframes) >= 2);

  const Quat identity = Quat::identity();
  for (size_t i = 0; i < (uint32_t)arrlen(*_keyframes); ++i) {
    RawQuaternionTrack::ValueType& src_key = (*_keyframes)[i].value;

    // Normalizes input quaternion.
    if (dot(src_key, src_key) == 0.f)
        src_key = identity;
    else
        src_key = normalize(src_key);

    // Ensures quaternions are all on the same hemisphere.
    if (i == 0) {
      if (src_key.getW() < 0.f) {
        src_key = -src_key;  // Q an -Q are the same rotation.
      }
    } else {
      RawQuaternionTrack::ValueType& prev_key = (*_keyframes)[i - 1].value;
      const float dot = src_key.getX() * prev_key.getX() + src_key.getY() * prev_key.getY() +
                        src_key.getZ() * prev_key.getZ() + src_key.getW() * prev_key.getW();
      if (dot < 0.f) {
        src_key = -src_key;  // Q an -Q are the same rotation.
      }
    }
  }
}
}  // namespace

bool TrackBuilder::operator()(const RawQuaternionTrack& _input, QuaternionTrack* pOut ) 
{
  return Build<RawQuaternionTrack, QuaternionTrack>(_input, pOut);
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
