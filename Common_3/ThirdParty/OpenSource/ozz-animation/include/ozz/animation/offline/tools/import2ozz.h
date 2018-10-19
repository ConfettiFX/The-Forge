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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_TOOLS_import2ozz_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_TOOLS_import2ozz_H_

#include "ozz/base/containers/vector.h"

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/raw_track.h"

namespace ozz {
namespace animation {

class Skeleton;

namespace offline {

// Defines ozz converter/importer interface.
// OzzImporter implements a command line tool to convert any source data format
// to ozz skeletons and animations. The tool exposes a set of global options
// through the command line, and a json configuration file to tune import
// settings. Reference json configuration is generated at
// src\animation\offline\tools\reference.json.
// To import a new source data format, one will implement the pure virtual
// functions of this interface. All the conversions end error processing are
// done by the tool.
class OzzImporter {
 public:
  // Function operator that must be called with main() arguments to start import
  // process.
  int operator()(int _argc, const char** _argv);

  // Loads source data file.
  // Returning false will report and error.
  virtual bool Load(const char* _filename) = 0;

  // Skeleton management.

  // Defines node types that should be considered as skeleton joints.
  struct NodeType {
    bool skeleton : 1;  // Uses skeleton nodes as skeleton joints.
    bool marker : 1;    // Uses marker nodes as skeleton joints.
    bool camera : 1;    // Uses camera nodes as skeleton joints.
    bool geometry : 1;  // Uses geometry nodes as skeleton joints.
    bool light : 1;     // Uses light nodes as skeleton joints.
    bool any : 1;  // Uses any node type as skeleton joints, including those
                   // listed above and any other.
  };

  // Import a skeleton from the source data file.
  // Returning false will report and error.
  virtual bool Import(ozz::animation::offline::RawSkeleton* _skeleton,
                      const NodeType& _types) = 0;

  // Animations management.

  // Gets the name of all the animations/clips/takes available from the source
  // data file.
  typedef ozz::Vector<ozz::String::Std>::Std AnimationNames;
  virtual AnimationNames GetAnimationNames() = 0;

  // Import animation "_animation_name" from the source data file.
  // The skeleton is provided such that implementation can look for its joints
  // animations.
  // Returning false will report and error.
  virtual bool Import(const char* _animation_name,
                      const ozz::animation::Skeleton& _skeleton,
                      float _sampling_rate, RawAnimation* _animation) = 0;

  // Tracks / properties management.

  // Defines properties, aka user-channel data: animations that aren't only
  // joint transforms.
  struct NodeProperty {
    ozz::String::Std name;
    enum Type { kFloat1 = 1, kFloat2 = 2, kFloat3 = 3, kFloat4 = 4 };
    Type type;
  };

  // Get all properties available for a node.
  typedef ozz::Vector<NodeProperty>::Std NodeProperties;
  virtual NodeProperties GetNodeProperties(const char* _node_name) = 0;

  // Imports a track of type 1, 2, 3 or 4 floats, for the triplet
  // _animation_name/_node_name/_track_name.
  // Returning false will report and error.
  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      RawFloatTrack* _track) = 0;
  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      RawFloat2Track* _track) = 0;
  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      RawFloat3Track* _track) = 0;
  virtual bool Import(const char* _animation_name, const char* _node_name,
                      const char* _track_name, float _sampling_rate,
                      RawFloat4Track* _track) = 0;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_TOOLS_import2ozz_H_
