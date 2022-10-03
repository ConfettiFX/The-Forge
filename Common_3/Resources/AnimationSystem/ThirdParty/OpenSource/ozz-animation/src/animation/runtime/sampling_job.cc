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

#include "../../../include/ozz/animation/runtime/sampling_job.h"



#include "../../../include/ozz/animation/runtime/animation.h"
#include "../../../include/ozz/base/maths/math_constant.h"
#include "../../../include/ozz/base/maths/math_ex.h"
#include "../../../include/ozz/base/memory/allocator.h"

// Internal include file
#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.
#include "../../animation/runtime/animation_keyframe.h"

namespace ozz {
namespace animation {

namespace internal {
struct InterpSoaFloat3 {
  Vector4 ratio[2];
  SoaFloat3 value[2];
};
struct InterpSoaQuaternion {
  Vector4 ratio[2];
  SoaQuaternion value[2];
};
}  // namespace internal

bool SamplingJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for NULL pointers.
  if (!animation || !context) {
    return false;
  }
  valid &= !output.empty();

  const int32_t num_soa_tracks = animation->num_soa_tracks();

  // Tests context size.
  valid &= context->max_soa_tracks() >= num_soa_tracks;

  return valid;
}

namespace {
// Loops through the sorted key frames and update context structure.
template <typename _Key>
void UpdateCacheCursor(float _ratio, int32_t _num_soa_tracks,
                       const ozz::span<const _Key>& _keys, int32_t* _cursor,
                       int32_t* _cache, uint8_t* _outdated) {
  ASSERT(_num_soa_tracks >= 1);
  const int32_t num_tracks = _num_soa_tracks * 4;
  ASSERT(_keys.begin() + num_tracks * 2 <= _keys.end());

  const _Key* cursor = NULL;
  if (!*_cursor) {
    // Initializes interpolated entries with the first 2 sets of key frames.
    // The sorting algorithm ensures that the first 2 key frames of a track
    // are consecutive.
    for (int32_t i = 0; i < _num_soa_tracks; ++i) {
      const int32_t in_index0 = i * 4;                   // * soa size
      const int32_t in_index1 = in_index0 + num_tracks;  // 2nd row.
      const int32_t out_index = i * 4 * 2;
      _cache[out_index + 0] = in_index0 + 0;
      _cache[out_index + 1] = in_index1 + 0;
      _cache[out_index + 2] = in_index0 + 1;
      _cache[out_index + 3] = in_index1 + 1;
      _cache[out_index + 4] = in_index0 + 2;
      _cache[out_index + 5] = in_index1 + 2;
      _cache[out_index + 6] = in_index0 + 3;
      _cache[out_index + 7] = in_index1 + 3;
    }
    cursor = _keys.begin() + num_tracks * 2;  // New cursor position.

    // All entries are outdated. It cares to only flag valid soa entries as
    // this is the exit condition of other algorithms.
    const int32_t num_outdated_flags = (_num_soa_tracks + 7) / 8;
    for (int32_t i = 0; i < num_outdated_flags - 1; ++i) {
      _outdated[i] = 0xff;
    }
    _outdated[num_outdated_flags - 1] =
        0xff >> (num_outdated_flags * 8 - _num_soa_tracks);
  } else {
    cursor = _keys.begin() + *_cursor;  // Might be == end()
    ASSERT(cursor >= _keys.begin() + num_tracks * 2 && cursor <= _keys.end());
  }

  // Search for the keys that matches _ratio.
  // Iterates while the context is not updated with left and right keys required
  // for interpolation at time ratio _ratio, for all tracks. Thanks to the
  // keyframe sorting, the loop can end as soon as it finds a key greater that
  // _ratio. It will mean that all the keys lower than _ratio have been
  // processed, meaning all context entries are up to date.
  while (cursor < _keys.end() &&
         _keys[_cache[cursor->track * 2 + 1]].ratio <= _ratio) {
    // Flag this soa entry as outdated.
    _outdated[cursor->track / 32] |= (1 << ((cursor->track & 0x1f) / 4));
    // Updates context.
    const int32_t base = cursor->track * 2;
    _cache[base] = _cache[base + 1];
    _cache[base + 1] = static_cast<int32_t>(cursor - _keys.begin());
    // Process next key.
    ++cursor;
  }
  ASSERT(cursor <= _keys.end());

  // Updates cursor output.
  *_cursor = static_cast<int32_t>(cursor - _keys.begin());
}

template <typename _Key, typename _InterpKey, typename _Decompress>
void UpdateInterpKeyframes(int32_t _num_soa_tracks,
                           const ozz::span<const _Key>& _keys,
                           const int32_t* _interp, uint8_t* _outdated,
                           _InterpKey* _interp_keys,
                           const _Decompress& _decompress) {
  const int32_t num_outdated_flags = (_num_soa_tracks + 7) / 8;
  for (int32_t j = 0; j < num_outdated_flags; ++j) {
    uint8_t outdated = _outdated[j];
    _outdated[j] = 0;  // Reset outdated entries as all will be processed.
    for (int32_t i = j * 8; outdated; ++i, outdated >>= 1) {
      if (!(outdated & 1)) {
        continue;
      }
      const int32_t base = i * 4 * 2;  // * soa size * 2 keys

      // Decompress left side keyframes and store them in soa structures.
      const _Key& k00 = _keys[_interp[base + 0]];
      const _Key& k10 = _keys[_interp[base + 2]];
      const _Key& k20 = _keys[_interp[base + 4]];
      const _Key& k30 = _keys[_interp[base + 6]];
      _interp_keys[i].ratio[0] = Vector4(k00.ratio, k10.ratio, k20.ratio, k30.ratio);
      _decompress(k00, k10, k20, k30, &_interp_keys[i].value[0]);

      // Decompress right side keyframes and store them in soa structures.
      const _Key& k01 = _keys[_interp[base + 1]];
      const _Key& k11 = _keys[_interp[base + 3]];
      const _Key& k21 = _keys[_interp[base + 5]];
      const _Key& k31 = _keys[_interp[base + 7]];
      _interp_keys[i].ratio[1] = Vector4(k01.ratio, k11.ratio, k21.ratio, k31.ratio);
      _decompress(k01, k11, k21, k31, &_interp_keys[i].value[1]);
    }
  }
}

inline void DecompressFloat3(const Float3Key& _k0, const Float3Key& _k1,
                             const Float3Key& _k2, const Float3Key& _k3,
                             SoaFloat3* _soa_float3) {
  _soa_float3->x = halfToFloat(vector4int::Load(_k0.value[0], _k1.value[0], _k2.value[0], _k3.value[0]));
  _soa_float3->y = halfToFloat(vector4int::Load(_k0.value[1], _k1.value[1], _k2.value[1], _k3.value[1]));
  _soa_float3->z = halfToFloat(vector4int::Load(_k0.value[2], _k1.value[2], _k2.value[2], _k3.value[2]));
}

// Defines a mapping table that defines components assignation in the output
// quaternion.
constexpr int32_t kCpntMapping[4][4] = {
    {0, 0, 1, 2}, {0, 0, 1, 2}, {0, 1, 0, 2}, {0, 1, 2, 0}};

void DecompressQuaternion(const QuaternionKey& _k0, const QuaternionKey& _k1,
                          const QuaternionKey& _k2, const QuaternionKey& _k3,
                          SoaQuaternion* _quaternion) {
  // Selects proper mapping for each key.
  const int32_t* m0 = kCpntMapping[_k0.largest];
  const int32_t* m1 = kCpntMapping[_k1.largest];
  const int32_t* m2 = kCpntMapping[_k2.largest];
  const int32_t* m3 = kCpntMapping[_k3.largest];

  // Prepares an array of input values, according to the mapping required to
  // restore quaternion largest component.
  alignas(16) int32_t cmp_keys[4][4] = {
      {_k0.value[m0[0]], _k1.value[m1[0]], _k2.value[m2[0]], _k3.value[m3[0]]},
      {_k0.value[m0[1]], _k1.value[m1[1]], _k2.value[m2[1]], _k3.value[m3[1]]},
      {_k0.value[m0[2]], _k1.value[m1[2]], _k2.value[m2[2]], _k3.value[m3[2]]},
      {_k0.value[m0[3]], _k1.value[m1[3]], _k2.value[m2[3]], _k3.value[m3[3]]},
  };

  // Resets largest component to 0. Overwritting here avoids 16 branchings
  // above.
  cmp_keys[_k0.largest][0] = 0;
  cmp_keys[_k1.largest][1] = 0;
  cmp_keys[_k2.largest][2] = 0;
  cmp_keys[_k3.largest][3] = 0;

  // Rebuilds quaternion from quantized values.
  const Vector4 kInt2Float = Vector4(1.f / (32767.f * math::kSqrt2));
  Vector4 cpnt[4] = {
      mulPerElem(kInt2Float,
          Vector4::fromVector4Int(vector4int::LoadPtr(cmp_keys[0]))),
      mulPerElem(kInt2Float,
          Vector4::fromVector4Int(vector4int::LoadPtr(cmp_keys[1]))),
      mulPerElem(kInt2Float,
          Vector4::fromVector4Int(vector4int::LoadPtr(cmp_keys[2]))),
      mulPerElem(kInt2Float,
          Vector4::fromVector4Int(vector4int::LoadPtr(cmp_keys[3]))),
  };

  // Get back length of 4th component. Favors performance over accuracy by using
  // x * RSqrtEst(x) instead of Sqrt(x).
  // ww0 cannot be 0 because we 're recomputing the largest component.
  const Vector4 dot = mulPerElem(cpnt[0], cpnt[0]) + mulPerElem(cpnt[1], cpnt[1]) +
      mulPerElem(cpnt[2], cpnt[2]) + mulPerElem(cpnt[3], cpnt[3]);
  const Vector4 ww0 = max(Vector4(1e-16f), Vector4::one() - dot);
  const Vector4 w0 = mulPerElem(ww0, rSqrtEst(ww0));
  // Re-applies 4th component' s sign.
  const Vector4Int sign = ShiftL(
      vector4int::Load(_k0.sign, _k1.sign, _k2.sign, _k3.sign), 31);
  const Vector4 restored = orPerElem(w0, sign);

  // Re-injects the largest component inside the SoA structure.
  cpnt[_k0.largest] = orPerElem(
      cpnt[_k0.largest], andPerElem(restored, vector4int::mask_f000()));
  cpnt[_k1.largest] = orPerElem(
      cpnt[_k1.largest], andPerElem(restored, vector4int::mask_0f00()));
  cpnt[_k2.largest] = orPerElem(
      cpnt[_k2.largest], andPerElem(restored, vector4int::mask_00f0()));
  cpnt[_k3.largest] = orPerElem(
      cpnt[_k3.largest], andPerElem(restored, vector4int::mask_000f()));

  // Stores result.
  _quaternion->x = cpnt[0];
  _quaternion->y = cpnt[1];
  _quaternion->z = cpnt[2];
  _quaternion->w = cpnt[3];
}

void Interpolates(float _anim_ratio, int32_t _num_soa_tracks,
                  const internal::InterpSoaFloat3* _translations,
                  const internal::InterpSoaQuaternion* _rotations,
                  const internal::InterpSoaFloat3* _scales,
                  SoaTransform* _output) {
  const Vector4 anim_ratio = Vector4(_anim_ratio);
  for (int32_t i = 0; i < _num_soa_tracks; ++i) {
    // Prepares interpolation coefficients.
    const Vector4 interp_t_ratio =
        mulPerElem((anim_ratio - _translations[i].ratio[0]),
        rcpEst(_translations[i].ratio[1] - _translations[i].ratio[0]));
    const Vector4 interp_r_ratio =
        mulPerElem((anim_ratio - _rotations[i].ratio[0]),
        rcpEst(_rotations[i].ratio[1] - _rotations[i].ratio[0]));
    const Vector4 interp_s_ratio =
        mulPerElem((anim_ratio - _scales[i].ratio[0]),
        rcpEst(_scales[i].ratio[1] - _scales[i].ratio[0]));

    // Processes interpolations.
    // The lerp of the rotation uses the shortest path, because opposed
    // quaternions were negated during animation build stage (AnimationBuilder).
    _output[i].translation = Lerp(_translations[i].value[0],
                                  _translations[i].value[1], interp_t_ratio);
    _output[i].rotation = NLerpEst(_rotations[i].value[0],
                                   _rotations[i].value[1], interp_r_ratio);
    _output[i].scale =
        Lerp(_scales[i].value[0], _scales[i].value[1], interp_s_ratio);
  }
}
}  // namespace

SamplingJob::SamplingJob() : ratio(0.f), animation(NULL), context(NULL) {}

bool SamplingJob::Run() const {
  if (!Validate()) {
    return false;
  }

  const int32_t num_soa_tracks = animation->num_soa_tracks();
  if (num_soa_tracks == 0) {  // Early out if animation contains no joint.
    return true;
  }

  // Clamps ratio in range [0,duration].
  const float anim_ratio = math::Clamp(0.f, ratio, 1.f);

  // Step the context to this potentially new animation and ratio.
  ASSERT(context->max_soa_tracks() >= num_soa_tracks);
  context->Step(*animation, anim_ratio);

  // Fetch key frames from the animation to the context at r = anim_ratio.
  // Then updates outdated soa hot values.
  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->translations(),
                    &context->translation_cursor_, context->translation_keys_,
                    context->outdated_translations_);
  UpdateInterpKeyframes(num_soa_tracks, animation->translations(),
                        context->translation_keys_,
                        context->outdated_translations_,
                        context->soa_translations_, &DecompressFloat3);

  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->rotations(),
                    &context->rotation_cursor_, context->rotation_keys_,
                    context->outdated_rotations_);
  UpdateInterpKeyframes(num_soa_tracks, animation->rotations(),
                        context->rotation_keys_, context->outdated_rotations_,
                        context->soa_rotations_, &DecompressQuaternion);

  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->scales(),
                    &context->scale_cursor_, context->scale_keys_,
                    context->outdated_scales_);
  UpdateInterpKeyframes(num_soa_tracks, animation->scales(),
                        context->scale_keys_, context->outdated_scales_,
                        context->soa_scales_, &DecompressFloat3);

  // only interp as much as we have output for.
  const int32_t num_soa_interp_tracks = math::Min(static_cast< int32_t >(output.size()), num_soa_tracks);

  // Interpolates soa hot data.
  Interpolates(anim_ratio, num_soa_interp_tracks, context->soa_translations_,
               context->soa_rotations_, context->soa_scales_, output.begin());

  return true;
}

SamplingJob::Context::Context()
    : max_soa_tracks_(0),
      soa_translations_(
          NULL) {  // soa_translations_ is the allocation pointer.
  Invalidate();
}

SamplingJob::Context::Context(int32_t _max_tracks)
    : max_soa_tracks_(0),
      soa_translations_(
          NULL) {  // soa_translations_ is the allocation pointer.
  Resize(_max_tracks);
}

SamplingJob::Context::~Context() {
  // Deallocates everything at once.
  memory::default_allocator()->Deallocate(soa_translations_);
}

void SamplingJob::Context::Resize(int32_t _max_tracks) {
  using internal::InterpSoaFloat3;
  using internal::InterpSoaQuaternion;

  // Reset existing data.
  Invalidate();
  memory::default_allocator()->Deallocate(soa_translations_);

  // Updates maximum supported soa tracks.
  max_soa_tracks_ = (_max_tracks + 3) / 4;

  // Allocate all context data at once in a single allocation.
  // Alignment is guaranteed because memory is dispatch from the highest
  // alignment requirement (Soa data: SimdFloat4) to the lowest (outdated
  // flag: uint8_t).

  // Computes allocation size.
  const size_t max_tracks = max_soa_tracks_ * 4;
  const size_t num_outdated = (max_soa_tracks_ + 7) / 8;
  const size_t size =
      sizeof(InterpSoaFloat3) * max_soa_tracks_ +
      sizeof(InterpSoaQuaternion) * max_soa_tracks_ +
      sizeof(InterpSoaFloat3) * max_soa_tracks_ +
      sizeof(int32_t) * max_tracks * 2 * 3 +  // 2 keys * (trans + rot + scale).
      sizeof(uint8_t) * 3 * num_outdated;

  // Allocates all at once.
  memory::Allocator* allocator = memory::default_allocator();
  char* alloc_begin = reinterpret_cast<char*>(
      allocator->Allocate(size, alignof(InterpSoaFloat3)));
  char* alloc_cursor = alloc_begin;

  // Distributes buffer memory while ensuring proper alignment (serves larger
  // alignment values first).
  static_assert(alignof(InterpSoaFloat3) >= alignof(InterpSoaQuaternion) &&
                    alignof(InterpSoaQuaternion) >= alignof(InterpSoaFloat3) &&
                    alignof(InterpSoaFloat3) >= alignof(int32_t) &&
                    alignof(int32_t) >= alignof(uint8_t),
                "Must serve larger alignment values first)");

  soa_translations_ = reinterpret_cast<InterpSoaFloat3*>(alloc_cursor);
  ASSERT(IsAligned(soa_translations_, alignof(InterpSoaFloat3)));
  alloc_cursor += sizeof(InterpSoaFloat3) * max_soa_tracks_;
  soa_rotations_ = reinterpret_cast<InterpSoaQuaternion*>(alloc_cursor);
  ASSERT(IsAligned(soa_rotations_, alignof(InterpSoaQuaternion)));
  alloc_cursor += sizeof(InterpSoaQuaternion) * max_soa_tracks_;
  soa_scales_ = reinterpret_cast<InterpSoaFloat3*>(alloc_cursor);
  ASSERT(IsAligned(soa_scales_, alignof(InterpSoaFloat3)));
  alloc_cursor += sizeof(InterpSoaFloat3) * max_soa_tracks_;

  translation_keys_ = reinterpret_cast<int32_t*>(alloc_cursor);
  ASSERT(IsAligned(translation_keys_, alignof(int32_t)));
  alloc_cursor += sizeof(int32_t) * max_tracks * 2;
  rotation_keys_ = reinterpret_cast<int32_t*>(alloc_cursor);
  alloc_cursor += sizeof(int32_t) * max_tracks * 2;
  scale_keys_ = reinterpret_cast<int32_t*>(alloc_cursor);
  alloc_cursor += sizeof(int32_t) * max_tracks * 2;

  outdated_translations_ = reinterpret_cast<uint8_t*>(alloc_cursor);
  ASSERT(IsAligned(outdated_translations_, alignof(uint8_t)));
  alloc_cursor += sizeof(uint8_t) * num_outdated;
  outdated_rotations_ = reinterpret_cast<uint8_t*>(alloc_cursor);
  alloc_cursor += sizeof(uint8_t) * num_outdated;
  outdated_scales_ = reinterpret_cast<uint8_t*>(alloc_cursor);
  alloc_cursor += sizeof(uint8_t) * num_outdated;

  ASSERT(alloc_cursor == alloc_begin + size);
}

void SamplingJob::Context::Step(const Animation& _animation, float _ratio) {
  // The context is invalidated if animation has changed or if it is being
  // rewind.
  if (animation_ != &_animation || _ratio < ratio_) {
    animation_ = &_animation;
    translation_cursor_ = 0;
    rotation_cursor_ = 0;
    scale_cursor_ = 0;
  }
  ratio_ = _ratio;
}

void SamplingJob::Context::Invalidate() {
  animation_ = NULL;
  ratio_ = 0.f;
  translation_cursor_ = 0;
  rotation_cursor_ = 0;
  scale_cursor_ = 0;
}
}  // namespace animation
}  // namespace ozz
