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

#include "ozz/animation/offline/additive_animation_builder.h"

#include <cassert>
#include <cstddef>

#include "ozz/animation/offline/raw_animation.h"
//#include "ozz/base/maths/transform.h"

namespace ozz {
namespace animation {
namespace offline {

namespace {
template <typename _RawTrack, typename _RefType, typename _MakeDelta>
void MakeDelta(const _RawTrack& _src, const _RefType& reference,
               const _MakeDelta& _make_delta, _RawTrack* _dest) {
  _dest->reserve(_src.size());

  // Early out if no key.
  if (_src.empty()) {
    return;
  }

  // Copy animation keys.
  for (size_t i = 0; i < _src.size(); ++i) {
    const typename _RawTrack::value_type delta = {
        _src[i].time, _make_delta(reference, _src[i].value)};
    _dest->push_back(delta);
  }
}

// CONFFX_BEGIN
Vector3 MakeDeltaTranslation(const Vector3& _reference,
                                  const Vector3& _value) {
  return _value - _reference;
}

Quat MakeDeltaRotation(const Quat& _reference,
                                   const Quat& _value) {
  return _value * conj(_reference);
}

Vector3 MakeDeltaScale(const Vector3& _reference,
                            const Vector3& _value) {
  return divPerElem(_value, _reference);
}
//CONFFX_END
}  // namespace

// Setup default values (favoring quality).
AdditiveAnimationBuilder::AdditiveAnimationBuilder() {}

bool AdditiveAnimationBuilder::operator()(const RawAnimation& _input,
                                          RawAnimation* _output) const {
  if (!_output) {
    return false;
  }
  // Reset output animation to default.
  *_output = RawAnimation();

  // Validate animation.
  if (!_input.Validate()) {
    return false;
  }

  // Rebuilds output animation.
  _output->duration = _input.duration;
  _output->tracks.resize(_input.tracks.size());

  for (size_t i = 0; i < _input.tracks.size(); ++i) {
    const RawAnimation::JointTrack& track_in = _input.tracks[i];
    RawAnimation::JointTrack& track_out = _output->tracks[i];

    const RawAnimation::JointTrack::Translations& translations =
        track_in.translations;
    const Vector3 ref_translation =
        translations.size() > 0 ? translations[0].value : Vector3(0.f, 0.f, 0.f);

    const RawAnimation::JointTrack::Rotations& rotations = track_in.rotations;
    const Quat ref_rotation = rotations.size() > 0
                                              ? rotations[0].value
                                              : Quat::identity();

    const RawAnimation::JointTrack::Scales& scales = track_in.scales;
    const Vector3 ref_scale =
        scales.size() > 0 ? scales[0].value : Vector3(1.f, 1.f, 1.f);

    MakeDelta(translations, ref_translation, MakeDeltaTranslation,
              &track_out.translations);
    MakeDelta(rotations, ref_rotation, MakeDeltaRotation, &track_out.rotations);
    MakeDelta(scales, ref_scale, MakeDeltaScale, &track_out.scales);
  }

  // Output animation is always valid though.
  return _output->Validate();
}

bool AdditiveAnimationBuilder::operator()(
    const RawAnimation& _input,
    const span<AffineTransform>& _reference_pose,
    RawAnimation* _output) const {
  if (!_output) {
    return false;
  }

  // Reset output animation to default.
  *_output = RawAnimation();

  // Validate animation.
  if (!_input.Validate()) {
    return false;
  }

  // The reference pose must have at least the same number of
  // tracks as the raw animation.
  if (_input.num_tracks() > static_cast<int>(_reference_pose.size())) {
    return false;
  }

  // Rebuilds output animation.
  _output->duration = _input.duration;
  _output->tracks.resize(_input.tracks.size());

  for (size_t i = 0; i < _input.tracks.size(); ++i) {
    MakeDelta(_input.tracks[i].translations, _reference_pose[i].translation,
              MakeDeltaTranslation, &_output->tracks[i].translations);
    MakeDelta(_input.tracks[i].rotations, _reference_pose[i].rotation,
              MakeDeltaRotation, &_output->tracks[i].rotations);
    MakeDelta(_input.tracks[i].scales, _reference_pose[i].scale, MakeDeltaScale,
              &_output->tracks[i].scales);
  }

  // Output animation is always valid though.
  return _output->Validate();
}

}  // namespace offline
}  // namespace animation
}  // namespace ozz
