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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_RAW_SKELETON_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_RAW_SKELETON_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive_traits.h"
//CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"
//CONFFX_END

namespace ozz {
namespace animation {
namespace offline {

// Off-line skeleton type.
// This skeleton type is not intended to be used in run time. It is used to
// define the offline skeleton object that can be converted to the runtime
// skeleton using the SkeletonBuilder. This skeleton structure exposes joints'
// hierarchy. A joint is defined with a name, a transformation (its bind pose),
// and its children. Children are exposed as a public std::vector of joints.
// This same type is used for skeleton roots, also exposed from the public API.
// The public API exposed through std:vector's of joints can be used freely with
// the only restriction that the total number of joints does not exceed
// Skeleton::kMaxJoints.
struct RawSkeleton {
  // Construct an empty skeleton.
  RawSkeleton();

  // The destructor is responsible for deleting the roots and their hierarchy.
  ~RawSkeleton();

  // Offline skeleton joint type.
  struct Joint {
    // Type of the list of children joints.
    typedef ozz::Vector<Joint>::Std Children;

    // Children joints.
    Children children;

    // The name of the joint.
    ozz::String::Std name;

    // Joint bind pose transformation in local space.
    AffineTransform transform; //CONFFX_BEGIN
  };

  // Tests for *this validity.
  // Returns true on success or false on failure if the number of joints exceeds
  // ozz::Skeleton::kMaxJoints.
  bool Validate() const;

  // Returns the number of joints of *this animation.
  // This function is not constant time as it iterates the hierarchy of joints
  // and counts them.
  int num_joints() const;

  // Applies a specified functor to each joint in a depth-first order.
  // _Fct is of type void(const Joint& _current, const Joint* _parent) where the
  // first argument is the child of the second argument. _parent is null if the
  // _current joint is the root.
  template <typename _Fct>
  _Fct IterateJointsDF(_Fct _fct) const {
    IterHierarchyDF(roots, NULL, _fct);
    return _fct;
  }

  // Applies a specified functor to each joint in a breadth-first order.
  // _Fct is of type void(const Joint& _current, const Joint* _parent) where the
  // first argument is the child of the second argument. _parent is null if the
  // _current joint is the root.
  template <typename _Fct>
  _Fct IterateJointsBF(_Fct _fct) const {
    IterHierarchyBF(roots, NULL, _fct);
    return _fct;
  }

  // Declares the skeleton's roots. Can be empty if the skeleton has no joint.
  Joint::Children roots;

 private:
  // Internal function used to iterate through joint hierarchy depth-first.
  template <typename _Fct>
  static void IterHierarchyDF(const RawSkeleton::Joint::Children& _children,
                              const RawSkeleton::Joint* _parent, _Fct& _fct) {
    for (size_t i = 0; i < _children.size(); ++i) {
      const RawSkeleton::Joint& current = _children[i];
      _fct(current, _parent);
      IterHierarchyDF(current.children, &current, _fct);
    }
  }

  // Internal function used to iterate through joint hierarchy breadth-first.
  template <typename _Fct>
  static void IterHierarchyBF(const RawSkeleton::Joint::Children& _children,
                              const RawSkeleton::Joint* _parent, _Fct& _fct) {
    for (size_t i = 0; i < _children.size(); ++i) {
      const RawSkeleton::Joint& current = _children[i];
      _fct(current, _parent);
    }
    for (size_t i = 0; i < _children.size(); ++i) {
      const RawSkeleton::Joint& current = _children[i];
      IterHierarchyBF(current.children, &current, _fct);
    }
  }
};
}  // namespace offline
}  // namespace animation
namespace io {
OZZ_IO_TYPE_VERSION(1, animation::offline::RawSkeleton)
OZZ_IO_TYPE_TAG("ozz-raw_skeleton", animation::offline::RawSkeleton)

// Should not be called directly but through io::Archive << and >> operators.
template <>
struct Extern<animation::offline::RawSkeleton> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawSkeleton* _skeletons,
                   size_t _count);
  static void Load(IArchive& _archive,
                   animation::offline::RawSkeleton* _skeletons, size_t _count,
                   uint32_t _version);
};
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_RAW_SKELETON_H_
