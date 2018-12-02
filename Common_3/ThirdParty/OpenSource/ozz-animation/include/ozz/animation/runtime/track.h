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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_TRACK_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_TRACK_H_

#include "ozz/base/io/archive_traits.h"
#include "ozz/base/platform.h"

//CONFFX_BEGIN
#include "../../../../../../../../Common_3/OS/Math/MathTypes.h"
//CONFFX_END

namespace ozz {
namespace animation {

// Forward declares the TrackBuilder, used to instantiate a Track.
namespace offline {
class TrackBuilder;
}

namespace internal {
// Runtime user-channel track internal implementation.
// The runtime track data structure exists for 1 to 4 float types (FloatTrack,
// ..., Float4Track) and quaternions (QuaternionTrack). See RawTrack for more
// details on track content. The runtime track data structure is optimized for
// the processing of ozz::animation::TrackSamplingJob and
// ozz::animation::TrackTriggeringJob. Keyframe ratios, values and interpolation
// mode are all store as separate buffers in order to access the cache
// coherently. Ratios are usually accessed/read alone from the jobs that all
// start by looking up the keyframes to interpolate indeed.
template <typename _ValueType>
class Track {
 public:
  typedef _ValueType ValueType;

  Track();
  ~Track();

  // Keyframe accessors.
  Range<const float> ratios() const { return ratios_; }
  Range<const _ValueType> values() const { return values_; }
  Range<const uint8_t> steps() const { return steps_; }

  // Get the estimated track's size in bytes.
  size_t size() const;

  // Get track name.
  const char* name() const { return name_ ? name_ : ""; }

  // Serialization functions.
  // Should not be called directly but through io::Archive << and >> operators.
  void Save(ozz::io::OArchive& _archive) const;
  void Load(ozz::io::IArchive& _archive, uint32_t _version);

 private:
  // Disables copy and assignation.
  Track(Track const&);
  void operator=(Track const&);

  // TrackBuilder class is allowed to allocate a Track.
  friend class offline::TrackBuilder;

  // Internal destruction function.
  void Allocate(size_t _keys_count, size_t _name_len);
  void Deallocate();

  // Keyframe ratios (0 is the beginning of the track, 1 is the end).
  Range<float> ratios_;

  // Keyframe values.
  Range<_ValueType> values_;

  // Keyframe modes (1 bit per key): 1 for step, 0 for linear.
  Range<uint8_t> steps_;

  // Track name.
  char* name_;
};

// Definition of operations policies per track value type.
template <typename _ValueType>
struct TrackPolicy {
  inline static _ValueType Lerp(const _ValueType& _a, const _ValueType& _b,
                                float _alpha) {
    return lerp(_alpha, _a, _b);  // CONFFX_BEGIN
  }
  inline static _ValueType identity() { return _ValueType(0.f); }
};
// Specialization for quaternions policy.
//CONFFX_BEGIN
template <>
inline Quat TrackPolicy<Quat>::Lerp(
    const Quat& _a, const Quat& _b, float _alpha) {
  // Uses NLerp to favor speed. This same function is used when optimizing the
  // curve (key frame reduction), so "constant speed" interpolation can still be
  // approximated with a lower tolerance value if it matters.
  return normalize(lerp(_alpha, _a, _b));
}
template <>
inline Quat TrackPolicy<Quat>::identity() {
  return Quat::identity();
}
//CONFFX_END
}  // namespace internal

// Runtime track data structure instantiation.
class FloatTrack : public internal::Track<float> {};
//CONFFX_BEGIN
class Float2Track : public internal::Track<Vector2> {};
class Float3Track : public internal::Track<Vector3> {};
class Float4Track : public internal::Track<Vector4> {};
class QuaternionTrack : public internal::Track<Quat> {};
//CONFFX_END
}  // namespace animation
namespace io {
OZZ_IO_TYPE_VERSION(1, animation::FloatTrack)
OZZ_IO_TYPE_TAG("ozz-float_track", animation::FloatTrack)
OZZ_IO_TYPE_VERSION(1, animation::Float2Track)
OZZ_IO_TYPE_TAG("ozz-float2_track", animation::Float2Track)
OZZ_IO_TYPE_VERSION(1, animation::Float3Track)
OZZ_IO_TYPE_TAG("ozz-float3_track", animation::Float3Track)
OZZ_IO_TYPE_VERSION(1, animation::Float4Track)
OZZ_IO_TYPE_TAG("ozz-float4_track", animation::Float4Track)
OZZ_IO_TYPE_VERSION(1, animation::QuaternionTrack)
OZZ_IO_TYPE_TAG("ozz-quat_track", animation::QuaternionTrack)
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_TRACK_H_
