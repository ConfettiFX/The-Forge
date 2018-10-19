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

#ifndef OZZ_ANIMATION_OFFLINE_FBX_FBX2OZZ_H_
#define OZZ_ANIMATION_OFFLINE_FBX_FBX2OZZ_H_

#include "ozz/animation/offline/tools/import2ozz.h"

#include "ozz/animation/offline/fbx/fbx.h"

// fbx2ozz is a command line tool that converts an animation imported from a
// fbx document to ozz runtime format.
//
// fbx2ozz extracts animated joints from a fbx document. Only the animated
// joints whose names match those of the ozz runtime skeleton given as argument
// are selected. Keyframes are then optimized, based on command line settings,
// and serialized as a runtime animation to an ozz binary archive.
//
// Use fbx2ozz integrated help command (fbx2ozz --help) for more details
// about available arguments.

class Fbx2OzzImporter : public ozz::animation::offline::OzzImporter {
 public:
  Fbx2OzzImporter();
  ~Fbx2OzzImporter();

 private:
  virtual bool Load(const char* _filename);

  // Skeleton management
  virtual bool Import(ozz::animation::offline::RawSkeleton* _skeleton,
                      const NodeType& _types);

  // Animation management
  virtual AnimationNames GetAnimationNames();

  virtual bool Import(const char* _animation_name,
                      const ozz::animation::Skeleton& _skeleton,
                      float _sampling_rate,
                      ozz::animation::offline::RawAnimation* _animation);

  // Track management
  virtual NodeProperties GetNodeProperties(const char* _node_name);

  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      ozz::animation::offline::RawFloatTrack* _track);

  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      ozz::animation::offline::RawFloat2Track* _track);

  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      ozz::animation::offline::RawFloat3Track* _track);

  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      ozz::animation::offline::RawFloat4Track* _track);

  // Fbx internal helpers
  ozz::animation::offline::fbx::FbxManagerInstance fbx_manager_;
  ozz::animation::offline::fbx::FbxAnimationIOSettings settings_;
  ozz::animation::offline::fbx::FbxSceneLoader* scene_loader_;
};
#endif  // OZZ_ANIMATION_OFFLINE_FBX_FBX2OZZ_H_
