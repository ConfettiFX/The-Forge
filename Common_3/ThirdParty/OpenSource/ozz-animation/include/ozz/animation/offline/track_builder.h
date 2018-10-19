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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_

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
class TrackBuilder {
 public:
  // Creates a Track based on _raw_track and *this builder
  // parameters.
  // The returned instance will then need to be deleted using the default
  // allocator Delete() function.
  // See Raw*Track::Validate() for more details about failure reasons.
  FloatTrack* operator()(const RawFloatTrack& _input) const;
  Float2Track* operator()(const RawFloat2Track& _input) const;
  Float3Track* operator()(const RawFloat3Track& _input) const;
  Float4Track* operator()(const RawFloat4Track& _input) const;
  QuaternionTrack* operator()(const RawQuaternionTrack& _input) const;

 private:
  template <typename _RawTrack, typename _Track>
  _Track* Build(const _RawTrack& _input) const;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_TRACK_BUILDER_H_
