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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_

#include "../../../../include/ozz/animation/offline/export.h"

namespace ozz {
namespace animation {

// Forward declares the runtime tracks type.
class FloatTrack;
class Float2Track;
class Float3Track;
class Float4Track;
class QuaternionTrack;

namespace offline {

// Forward declares the offline tracks type.
struct RawFloatTrack;
struct RawFloat2Track;
struct RawFloat3Track;
struct RawFloat4Track;
struct RawQuaternionTrack;

// Defines the class responsible of building runtime track instances from
// offline tracks.The input raw track is first validated. Runtime conversion of
// a validated raw track cannot fail. Note that no optimization is performed on
// the data at all.
class OZZ_ANIMOFFLINE_DLL TrackBuilder {
 public:
  // Creates a Track based on _raw_track and *this builder parameters.
  // Returns a track instance on success, an empty unique_ptr on failure. See
  // Raw*Track::Validate() for more details about failure reasons.
  // The track is returned as an unique_ptr as ownership is given back to the
  // caller.
  bool operator()(const RawFloatTrack& _input, FloatTrack* pOut);
  bool operator()(const RawFloat2Track& _input, Float2Track* pOut);
  bool operator()(const RawFloat3Track& _input, Float3Track* pOut);
  bool operator()(const RawFloat4Track& _input, Float4Track* pOut);
  bool operator()(const RawQuaternionTrack& _input, QuaternionTrack* pOut);

 private:
  template <typename _RawTrack, typename _Track>
  bool Build(const _RawTrack& _input, _Track* pOut) const;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_
