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

#include "../../../include/ozz/animation/offline/animation_builder.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

#include "../../../include/ozz/animation/offline/raw_animation.h"
#include "../../../include/ozz/animation/runtime/animation.h"
#include "../../../include/ozz/base/containers/vector.h"
#include "../../../include/ozz/base/maths/math_constant.h"
#include "../../../include/ozz/base/memory/allocator.h"

// Internal include file
#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.
#include "../src/animation/runtime/animation_keyframe.h"

// TheForge BEGIN
static uint16_t FloatToHalfTemp(float _f) {
  const uint32_t f32infty = 255 << 23;
  const uint32_t f16infty = 31 << 23;
  const union {
    uint32_t u;
    float f;

  } magic = {15 << 23};
  const uint32_t sign_mask = 0x80000000u;
  const uint32_t round_mask = ~0x00000fffu;

  const union {
    float f;
    uint32_t u;

  } f = {_f};
  const uint32_t sign = f.u & sign_mask;
  const uint32_t f_nosign = f.u & ~sign_mask;

  if (f_nosign >= f32infty) {  // Inf or NaN (all exponent bits set)
    // NaN->qNaN and Inf->Inf
    const uint32_t result =
        ((f_nosign > f32infty) ? 0x7e00 : 0x7c00) | (sign >> 16);
    return static_cast<uint16_t>(result);

  } else {  // (De)normalized number or zero
    const union {
      uint32_t u;
      float f;

    } rounded = {f_nosign & round_mask};
    const union {
      float f;
      uint32_t u;

    } exp = {rounded.f * magic.f};
    const uint32_t re_rounded = exp.u - round_mask;
    // Clamp to signed infinity if overflowed
    const uint32_t result =
        ((re_rounded > f16infty ? f16infty : re_rounded) >> 13) | (sign >> 16);
    return static_cast<uint16_t>(result);
  }
}
// TheForge END

namespace ozz {
namespace animation {
namespace offline {
namespace {

struct SortingTranslationKey {
  uint16_t track;
  float prev_key_time;
  RawAnimation::TranslationKey key;
};

struct SortingRotationKey {
  uint16_t track;
  float prev_key_time;
  RawAnimation::RotationKey key;
};

struct SortingScaleKey {
  uint16_t track;
  float prev_key_time;
  RawAnimation::ScaleKey key;
};

// Keyframe sorting. Stores first by time and then track number.
template <typename _Key>
bool SortingKeyLess(const _Key& _left, const _Key& _right) {
  const float time_diff = _left.prev_key_time - _right.prev_key_time;
  return time_diff < 0.f || (time_diff == 0.f && _left.track < _right.track);
}

template <typename _SrcKey, typename _DestTrack>
void PushBackIdentityKey(uint16_t _track, float _time, _DestTrack** _dest) {
  typedef _DestTrack DestKey;
  float prev_time = -1.f;
  if (arrlen(*_dest) != 0 && arrlast(*_dest).track == _track) {
    prev_time = arrlast(*_dest).key.time;
  }
  const DestKey key = {_track, prev_time, {_time, _SrcKey::identity()}};
  arrpush(*_dest, key);
}

// Copies a track from a RawAnimation to an Animation.
// Also fixes up the front (t = 0) and back keys (t = duration).
template <typename _SrcTrack, typename _DestTrack>
void CopyRaw(const _SrcTrack* _src, uint16_t _track, float _duration,
             _DestTrack** _dest) {
  typedef _SrcTrack SrcKey;
  typedef _DestTrack DestKey;

  if ((uint32_t)arrlen(_src) == 0) {  // Adds 2 new keys.
    PushBackIdentityKey<SrcKey, _DestTrack>(_track, 0.f, _dest);
    PushBackIdentityKey<SrcKey, _DestTrack>(_track, _duration, _dest);
  } else if ((uint32_t)arrlen(_src) == 1) {  // Adds 1 new key.
    const SrcKey& raw_key = _src[0];
    ASSERT(raw_key.time >= 0 && raw_key.time <= _duration);
    const DestKey first = {_track, -1.f, {0.f, raw_key.value}};
    arrpush(*_dest, first);
    const DestKey last = {_track, 0.f, {_duration, raw_key.value}};
    arrpush(*_dest, last);
  } else {  // Copies all keys, and fixes up first and last keys.
    float prev_time = -1.f;
    if (_src[0].time != 0.f) {  // Needs a key at t = 0.f.
      const DestKey first = {_track, prev_time, {0.f, _src[0].value}};
      arrpush(*_dest, first);
      prev_time = 0.f;
    }
    for (size_t k = 0; k < (uint32_t)arrlen(_src); ++k) {  // Copies all keys.
      const SrcKey& raw_key = _src[k];
      ASSERT(raw_key.time >= 0 && raw_key.time <= _duration);
      const DestKey key = {_track, prev_time, {raw_key.time, raw_key.value}};
      arrpush(*_dest, key);
      prev_time = raw_key.time;
    }
    if (arrlast(_src).time - _duration != 0.f) {  // Needs a key at t = _duration.
      const DestKey last = {_track, prev_time, {_duration, arrlast(_src).value}};
      arrpush(*_dest, last);
    }
  }
  ASSERT(_dest[0]->key.time == 0.f &&
         arrlast(*_dest).key.time - _duration == 0.f);
}

template <typename _SortingKey>
void CopyToAnimation(_SortingKey* _src,
                     ozz::span<Float3Key>* _dest, float _inv_duration) {
  const size_t src_count = (uint32_t)arrlen(_src);
  if (!src_count) {
    return;
  }

  // Sort animation keys to favor cache coherency.
  std::sort(array_begin(_src), array_end(_src), &SortingKeyLess<_SortingKey>);

  // Fills output.
  const _SortingKey* src = _src;
  for (size_t i = 0; i < src_count; ++i) {
    Float3Key& key = (*_dest)[i];
    key.ratio = src[i].key.time * _inv_duration;
    key.track = src[i].track;

    key.value[0] = FloatToHalfTemp(src[i].key.value.getX());
    key.value[1] = FloatToHalfTemp(src[i].key.value.getY());
    key.value[2] = FloatToHalfTemp(src[i].key.value.getZ());
  }
}

// Compares float absolute values.
bool LessAbs(float _left, float _right) {
  return abs(_left) < abs(_right);
}

// Compresses quaternion to ozz::animation::RotationKey format.
// The 3 smallest components of the quaternion are quantized to 16 bits
// integers, while the largest is recomputed thanks to quaternion normalization
// property (x^2+y^2+z^2+w^2 = 1). Because the 3 components are the 3 smallest,
// their value cannot be greater than sqrt(2)/2. Thus quantization quality is
// improved by pre-multiplying each componenent by sqrt(2).
void CompressQuat(const Quat& _src, ozz::animation::QuaternionKey* _dest) {
  // Finds the largest quaternion component.
  const float quat[4] = {_src.getX(), _src.getY(), _src.getZ(), _src.getW()};
  const ptrdiff_t largest = std::max_element(quat, quat + 4, LessAbs) - quat;
  ASSERT(largest <= 3);
  _dest->largest = largest & 0x3;

  // Stores the sign of the largest component.
  _dest->sign = quat[largest] < 0.f;

  // Quantize the 3 smallest components on 16 bits signed integers.
  const float kFloat2Int = 32767.f * math::kSqrt2;
  const int32_t kMapping[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
  const int32_t* map = kMapping[largest];
  const int32_t a = static_cast<int32_t>(floor(quat[map[0]] * kFloat2Int + .5f));
  const int32_t b = static_cast<int32_t>(floor(quat[map[1]] * kFloat2Int + .5f));
  const int32_t c = static_cast<int32_t>(floor(quat[map[2]] * kFloat2Int + .5f));
  _dest->value[0] = clamp(-32767, a, 32767) & 0xffff;
  _dest->value[1] = clamp(-32767, b, 32767) & 0xffff;
  _dest->value[2] = clamp(-32767, c, 32767) & 0xffff;
}

// Specialize for rotations in order to normalize quaternions.
// Consecutive opposite quaternions are also fixed up in order to avoid checking
// for the smallest path during the NLerp runtime algorithm.
void CopyToAnimation(SortingRotationKey* _src,
                     ozz::span<QuaternionKey>* _dest, float _inv_duration) {
  const size_t src_count = (uint32_t)arrlen(_src);
  if (!src_count) {
    return;
  }

  // Normalize quaternions.
  // Also fixes-up successive opposite quaternions that would fail to take the
  // shortest path during the normalized-lerp.
  // Note that keys are still sorted per-track at that point, which allows this
  // algorithm to process all consecutive keys.
  size_t track = std::numeric_limits<size_t>::max();
  const Quat identity = Quat::identity();
  SortingRotationKey* src = _src;
  for (size_t i = 0; i < src_count; ++i) {
    Quat src_key = identity;
    if (norm(src[i].key.value) != 0.f) {
      src_key = normalize(src[i].key.value);
    }

    if (track != src[i].track) {   // First key of the track.
      if (src_key.getW() < 0.f) {  // .w eq to a dot with identity quaternion.
        src_key = -src_key;        // Q an -Q are the same rotation.
      }
    } else {  // Still on the same track: so fixes-up quaternion.
      const Quat prev_key = src[i - 1].key.value;

      const float dot =
          src_key.getX() * prev_key.getX() + src_key.getY() * prev_key.getY() +
          src_key.getZ() * prev_key.getZ() + src_key.getW() * prev_key.getW();
      if (dot < 0.f) {
        src_key = -src_key;  // Q an -Q are the same rotation.
      }
    }
    // Stores fixed-up quaternion.
    src[i].key.value = src_key;
    track = src[i].track;
  }

  // Sort.
  std::sort(array_begin(_src), array_end(_src),
            &SortingKeyLess<SortingRotationKey>);

  // Fills rotation keys output.
  for (size_t i = 0; i < src_count; ++i) {
    const SortingRotationKey& skey = src[i];
    QuaternionKey& dkey = (*_dest)[i];
    dkey.ratio = skey.key.time * _inv_duration;
    dkey.track = skey.track;

    // Compress quaternion to destination container.
    CompressQuat(skey.key.value, &dkey);
  }
}
}  // namespace

// Ensures _input's validity and allocates _animation.
// An animation needs to have at least two key frames per joint, the first at
// t = 0 and the last at t = duration. If at least one of those keys are not
// in the RawAnimation then the builder creates it.
bool AnimationBuilder::Build(const RawAnimation& _input, Animation* animation) {
  // Tests _raw_animation validity.
  if (!_input.Validate()) {
    return false;
  }

  // Sets duration.
  const float duration = _input.duration;
  const float inv_duration = 1.f / _input.duration;
  animation->duration_ = duration;
  // A _duration == 0 would create some division by 0 during sampling.
  // Also we need at least to keys with different times, which cannot be done
  // if duration is 0.
  ASSERT(duration > 0.f);  // This case is handled by Validate().

  // Sets tracks count. Can be safely casted to uint16_t as number of tracks as
  // already been validated.
  const uint16_t num_tracks = static_cast<uint16_t>(_input.num_tracks());
  animation->num_tracks_ = num_tracks;
  const uint16_t num_soa_tracks = Align(num_tracks, 4);

  // Declares and preallocates tracks to sort.
  size_t translations = 0, rotations = 0, scales = 0;
  for (uint16_t i = 0; i < num_tracks; ++i) {
    const RawAnimation::JointTrack& raw_track = _input.tracks[i];
    translations += (uint32_t)arrlen(raw_track.translations) + 2;  // +2 because worst case
    rotations += (uint32_t)arrlen(raw_track.rotations) + 2;        // needs to add the
    scales += (uint32_t)arrlen(raw_track.scales) + 2;              // first and last keys.
  }
  SortingTranslationKey* sorting_translations = nullptr;
  arrsetcap(sorting_translations, translations);
  SortingRotationKey* sorting_rotations = nullptr;
  arrsetcap(sorting_rotations, rotations);
  SortingScaleKey* sorting_scales = nullptr;
  arrsetcap(sorting_scales, scales);

  // Filters RawAnimation keys and copies them to the output sorting structure.
  uint16_t i = 0;
  for (; i < num_tracks; ++i) {
    const RawAnimation::JointTrack& raw_track = _input.tracks[i];
    CopyRaw(raw_track.translations, i, duration, &sorting_translations);
    CopyRaw(raw_track.rotations, i, duration, &sorting_rotations);
    CopyRaw(raw_track.scales, i, duration, &sorting_scales);
  }

  // Add enough identity keys to match soa requirements.
  for (; i < num_soa_tracks; ++i) {
    typedef RawAnimation::TranslationKey SrcTKey;
    PushBackIdentityKey<SrcTKey>(i, 0.f, &sorting_translations);
    PushBackIdentityKey<SrcTKey>(i, duration, &sorting_translations);

    typedef RawAnimation::RotationKey SrcRKey;
    PushBackIdentityKey<SrcRKey>(i, 0.f, &sorting_rotations);
    PushBackIdentityKey<SrcRKey>(i, duration, &sorting_rotations);

    typedef RawAnimation::ScaleKey SrcSKey;
    PushBackIdentityKey<SrcSKey>(i, 0.f, &sorting_scales);
    PushBackIdentityKey<SrcSKey>(i, duration, &sorting_scales);
  }

  // Allocate animation members.
  animation->Allocate(blength(&_input.name), (uint32_t)arrlen(sorting_translations),
                      (uint32_t)arrlen(sorting_rotations), (uint32_t)arrlen(sorting_scales));

  // Copy sorted keys to final animation.
  CopyToAnimation(sorting_translations, &animation->translations_,
                  inv_duration);
  CopyToAnimation(sorting_rotations, &animation->rotations_, inv_duration);
  CopyToAnimation(sorting_scales, &animation->scales_, inv_duration);

  // Copy animation's name.
  if (animation->name_) {
    strcpy(animation->name_, (const char*)_input.name.data);
  }

  arrfree(sorting_translations);
  arrfree(sorting_rotations);
  arrfree(sorting_scales);

  return true;  // Success.
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
