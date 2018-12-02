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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_H_

//CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive_traits.h"
//CONFFX_END

namespace ozz {
namespace animation {
namespace offline {

// Offline animation type.
// This animation type is not intended to be used in run time. It is used to
// define the offline animation object that can be converted to the runtime
// animation using the AnimationBuilder.
// This animation structure exposes tracks of keyframes. Keyframes are defined
// with a time and a value which can either be a translation (3 float x, y, z),
// a rotation (a quaternion) or scale coefficient (3 floats x, y, z). Tracks are
// defined as a set of three different std::vectors (translation, rotation and
// scales). Animation structure is then a vector of tracks, along with a
// duration value.
// Finally the RawAnimation structure exposes Validate() function to check that
// it is valid, meaning that all the following rules are respected:
//  1. Animation duration is greater than 0.
//  2. Keyframes' time are sorted in a strict ascending order.
//  3. Keyframes' time are all within [0,animation duration] range.
// Animations that would fail this validation will fail to be converted by the
// AnimationBuilder.
struct RawAnimation {
  // Constructs a valid RawAnimation with a 1s default duration.
  RawAnimation();

  // Deallocates raw animation.
  ~RawAnimation();

  // Tests for *this validity.
  // Returns true if animation data (duration, tracks) is valid:
  //  1. Animation duration is greater than 0.
  //  2. Keyframes' time are sorted in a strict ascending order.
  //  3. Keyframes' time are all within [0,animation duration] range.
  bool Validate() const;

  // Defines a raw translation key frame.
  struct TranslationKey {
    // Key frame time.
    float time;

    // Key frame value.
    typedef Vector3 Value; //CONFFX_BEGIN
    Value value;

    // Provides identity transformation for a translation key.
    static Vector3 identity() { return Vector3(0.f, 0.f, 0.f); }  //CONFFX_BEGIN
  };

  // Defines a raw rotation key frame.
  struct RotationKey {
    // Key frame time.
    float time;

    // Key frame value.
    typedef Quat Value; //CONFFX_BEGIN
    Quat value;

    // Provides identity transformation for a rotation key.
    static Quat identity() { return Quat::identity(); } //CONFFX_BEGIN
  };

  // Defines a raw scaling key frame.
  struct ScaleKey {
    // Key frame time.
    float time;

    // Key frame value.
    typedef Vector3 Value;  // CONFFX_BEGIN
    Vector3 value;

    // Provides identity transformation for a scale key.
    static Vector3 identity() { return Vector3(1.f, 1.f, 1.f);
    }  // CONFFX_BEGIN
  };

  // Defines a track of key frames for a bone, including translation, rotation
  // and scale.
  struct JointTrack {
    typedef ozz::Vector<TranslationKey>::Std Translations;
    Translations translations;
    typedef ozz::Vector<RotationKey>::Std Rotations;
    Rotations rotations;
    typedef ozz::Vector<ScaleKey>::Std Scales;
    Scales scales;
  };

  // Returns the number of tracks of this animation.
  int num_tracks() const { return static_cast<int>(tracks.size()); }

  // Stores per joint JointTrack, ie: per joint animation key-frames.
  // tracks_.size() gives the number of animated joints.
  ozz::Vector<JointTrack>::Std tracks;

  // The duration of the animation. All the keys of a valid RawAnimation are in
  // the range [0,duration].
  float duration;

  // Name of the animation.
  ozz::String::Std name;
};
}  // namespace offline
}  // namespace animation
namespace io {
OZZ_IO_TYPE_VERSION(2, animation::offline::RawAnimation)
OZZ_IO_TYPE_TAG("ozz-raw_animation", animation::offline::RawAnimation)

// Should not be called directly but through io::Archive << and >> operators.
template <>
struct Extern<animation::offline::RawAnimation> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawAnimation* _animations,
                   size_t _count);
  static void Load(IArchive& _archive,
                   animation::offline::RawAnimation* _animations, size_t _count,
                   uint32_t _version);
};
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_H_
