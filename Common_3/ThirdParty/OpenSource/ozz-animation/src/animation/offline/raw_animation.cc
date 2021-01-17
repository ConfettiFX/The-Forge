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

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/skeleton.h"

namespace ozz {
namespace animation {
namespace offline {

RawAnimation::RawAnimation() : duration(1.f) {}

namespace {

// Implements key frames' time range and ordering checks.
// See AnimationBuilder::Create for more details.
template <typename _Key>
static bool ValidateTrack(const typename ozz::vector<_Key>& _track,
                          float _duration) {
  float previous_time = -1.f;
  for (size_t k = 0; k < _track.size(); ++k) {
    const float frame_time = _track[k].time;
    // Tests frame's time is in range [0:duration].
    if (frame_time < 0.f || frame_time > _duration) {
      return false;
    }
    // Tests that frames are sorted.
    if (frame_time <= previous_time) {
      return false;
    }
    previous_time = frame_time;
  }
  return true;  // Validated.
}
}  // namespace
bool RawAnimation::JointTrack::Validate(float _duration) const {
  return ValidateTrack<TranslationKey>(translations, _duration) &&
         ValidateTrack<RotationKey>(rotations, _duration) &&
         ValidateTrack<ScaleKey>(scales, _duration);
}

bool RawAnimation::Validate() const {
  if (duration <= 0.f) {  // Tests duration is valid.
    return false;
  }
  if (tracks.size() > Skeleton::kMaxJoints) {  // Tests number of tracks.
    return false;
  }
  // Ensures that all key frames' time are valid, ie: in a strict ascending
  // order and within range [0:duration].
  bool valid = true;
  for (size_t i = 0; valid && i < tracks.size(); ++i) {
    valid = tracks[i].Validate(duration);
  }
  return valid;  // *this is valid.
}

size_t RawAnimation::size() const {
  size_t size = sizeof(*this);

  // Accumulates keyframes size.
  const size_t tracks_count = tracks.size();
  for (size_t i = 0; i < tracks_count; ++i) {
    size += tracks[i].translations.size() * sizeof(TranslationKey);
    size += tracks[i].rotations.size() * sizeof(RotationKey);
    size += tracks[i].scales.size() * sizeof(ScaleKey);
  }

  // Accumulates tracks.
  size += tracks_count * sizeof(JointTrack);
  size += name.size();

  return size;
}

}  // namespace offline
}  // namespace animation
}  // namespace ozz
