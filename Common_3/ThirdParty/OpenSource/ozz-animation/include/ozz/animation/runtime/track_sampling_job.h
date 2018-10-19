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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_TRACK_SAMPLING_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_TRACK_SAMPLING_JOB_H_

#include "ozz/animation/runtime/track.h"

namespace ozz {
namespace animation {

namespace internal {

// TrackSamplingJob internal implementation. See *TrackSamplingJob for more
// details.
template <typename _Track>
struct TrackSamplingJob {
  typedef typename _Track::ValueType ValueType;

  TrackSamplingJob();

  // Validates all parameters.
  bool Validate() const;

  // Validates and executes sampling.
  bool Run() const;

  // Ratio used to sample track, clamped in range [0,1] before job execution. 0
  // is the beginning of the track, 1 is the end. This is a ratio rather than a
  // ratio because tracks have no duration.
  float ratio;

  // Track to sample.
  const _Track* track;

  // Job output.
  typename _Track::ValueType* result;
};
}  // namespace internal

// Track sampling job implementation. Track sampling allows to query a track
// value for a specified ratio. This is a ratio rather than a time because
// tracks have no duration.
struct FloatTrackSamplingJob : public internal::TrackSamplingJob<FloatTrack> {};
struct Float2TrackSamplingJob : public internal::TrackSamplingJob<Float2Track> {
};
struct Float3TrackSamplingJob : public internal::TrackSamplingJob<Float3Track> {
};
struct Float4TrackSamplingJob : public internal::TrackSamplingJob<Float4Track> {
};
struct QuaternionTrackSamplingJob
    : public internal::TrackSamplingJob<QuaternionTrack> {};

}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_TRACK_SAMPLING_JOB_H_
