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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_RAW_TRACK_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_RAW_TRACK_H_

// CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"

#include "ozz/base/io/archive_traits.h"
//CONFFX_END

namespace ozz {
namespace animation {
namespace offline {

// Interpolation mode.
struct RawTrackInterpolation {
  enum Value {
    kStep,    // All values following this key, up to the next key, are equal.
    kLinear,  // All value between this key and the next are linearly
              // interpolated.
  };
};

// Keyframe data structure.
template <typename _ValueType>
struct RawTrackKeyframe {
  typedef _ValueType ValueType;
  RawTrackInterpolation::Value interpolation;
  float ratio;
  ValueType value;
};

namespace internal {

// Offline user-channel animation track type implementation.
// This offline track data structure is meant to be used for user-channel
// tracks, aka animation of variables that aren't joint transformation. It is
// available for tracks of 1 to 4 floats (RawFloatTrack, RawFloat2Track, ...,
// RawFloat4Track) and quaternions (RawQuaternionTrack). Quaternions differ from
// float4 because of the specific interpolation and comparison treatment they
// require. As all other Raw data types, they are not intended to be used in run
// time. They are used to define the offline track object that can be converted
// to the runtime one using the a ozz::animation::offline::TrackBuilder. This
// animation structure exposes a single sequence of keyframes. Keyframes are
// defined with a ratio, a value and an interpolation mode:
// - Ratio: A track has no duration, so it uses ratios between 0 (beginning of
// the track) and 1 (the end), instead of times. This allows to avoid any
// discrepancy between the durations of tracks and the animation they match
// with.
// - Value: The animated value (float, ... float4, quaternion).
// - Interpolation mode (`ozz::animation::offline::RawTrackInterpolation`):
// Defines how value is interpolated with the next key. Track structure is then
// a sorted vector of keyframes. RawTrack structure exposes a Validate()
// function to check that all the following rules are respected:
// 1. Keyframes' ratios are sorted in a strict ascending order.
// 2. Keyframes' ratios are all within [0,1] range.
// RawTrack that would fail this validation will fail to be converted by
// the RawTrackBuilder.
template <typename _ValueType>
struct RawTrack {
  typedef _ValueType ValueType;
  typedef RawTrackKeyframe<ValueType> Keyframe;

  // Constructs a valid RawFloatTrack.
  RawTrack();

  // Deallocates track.
  ~RawTrack();

  // Validates that all the following rules are respected:
  //  1. Keyframes' ratios are sorted in a strict ascending order.
  //  2. Keyframes' ratios are all within [0,1] range.
  bool Validate() const;

  // Uses intrusive serialization option, as a way to factorize code.
  // Version and Tag should still be defined for each specialization.
  void Save(io::OArchive& _archive) const;
  void Load(io::IArchive& _archive, uint32_t _version);

  // Sequence of keyframes, expected to be sorted.
  typedef typename ozz::Vector<Keyframe>::Std Keyframes;
  Keyframes keyframes;

  // Name of the track.
  String::Std name;
};
}  // namespace internal

// Offline user-channel animation track type instantiation.
//CONFFX_BEGIN
struct RawFloatTrack : public internal::RawTrack<float> {};
struct RawFloat2Track : public internal::RawTrack<Vector2> {};
struct RawFloat3Track : public internal::RawTrack<Vector3> {};
struct RawFloat4Track : public internal::RawTrack<Vector4> {};
struct RawQuaternionTrack : public internal::RawTrack<Quat> {};
//CONFFX_END
}  // namespace offline
}  // namespace animation

namespace io {
OZZ_IO_TYPE_VERSION(1, animation::offline::RawFloatTrack)
OZZ_IO_TYPE_TAG("ozz-raw_float_track", animation::offline::RawFloatTrack)
OZZ_IO_TYPE_VERSION(1, animation::offline::RawFloat2Track)
OZZ_IO_TYPE_TAG("ozz-raw_float2_track", animation::offline::RawFloat2Track)
OZZ_IO_TYPE_VERSION(1, animation::offline::RawFloat3Track)
OZZ_IO_TYPE_TAG("ozz-raw_float3_track", animation::offline::RawFloat3Track)
OZZ_IO_TYPE_VERSION(1, animation::offline::RawFloat4Track)
OZZ_IO_TYPE_TAG("ozz-raw_float4_track", animation::offline::RawFloat4Track)
OZZ_IO_TYPE_VERSION(1, animation::offline::RawQuaternionTrack)
OZZ_IO_TYPE_TAG("ozz-raw_quat_track", animation::offline::RawQuaternionTrack)
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_RAW_TRACK_H_
