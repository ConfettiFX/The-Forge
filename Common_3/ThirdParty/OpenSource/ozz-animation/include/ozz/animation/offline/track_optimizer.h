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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_TRACK_OPTIMIZER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_TRACK_OPTIMIZER_H_

namespace ozz {
namespace animation {
namespace offline {

// Forward declare offline track types.
struct RawFloatTrack;
struct RawFloat2Track;
struct RawFloat3Track;
struct RawFloat4Track;
struct RawQuaternionTrack;

// TrackOptimizer is responsible for optimizing an offline raw track instance.
// Optimization is a keyframe reduction process. Redundant and interpolable
// keyframes (within a tolerance value) are removed from the track. Default
// optimization tolerances are set in order to favor quality over runtime
// performances and memory footprint.
class TrackOptimizer {
 public:
  // Initializes the optimizer with default tolerances (favoring quality).
  TrackOptimizer();

  // Optimizes _input using *this parameters.
  // Returns true on success and fills _output track with the optimized
  // version of _input track.
  // *_output must be a valid Raw*Track instance.
  // Returns false on failure and resets _output to an empty track.
  // See Raw*Track::Validate() for more details about failure reasons.
  bool operator()(const RawFloatTrack& _input, RawFloatTrack* _output) const;
  bool operator()(const RawFloat2Track& _input, RawFloat2Track* _output) const;
  bool operator()(const RawFloat3Track& _input, RawFloat3Track* _output) const;
  bool operator()(const RawFloat4Track& _input, RawFloat4Track* _output) const;
  bool operator()(const RawQuaternionTrack& _input,
                  RawQuaternionTrack* _output) const;

  // Optimization tolerance.
  float tolerance;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_TRACK_OPTIMIZER_H_
