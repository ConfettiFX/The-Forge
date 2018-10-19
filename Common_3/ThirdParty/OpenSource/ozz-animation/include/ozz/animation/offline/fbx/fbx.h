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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_FBX_FBX_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_FBX_FBX_H_

#include <fbxsdk.h>

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/transform.h"

namespace ozz {
namespace math {
struct Transform;
}  // namespace math
namespace animation {
namespace offline {
namespace fbx {

// Manages FbxManager instance.
class FbxManagerInstance {
 public:
  // Instantiates FbxManager.
  FbxManagerInstance();

  // Destroys FbxManager.
  ~FbxManagerInstance();

  // Cast operator to get internal FbxManager instance.
  operator FbxManager*() const { return fbx_manager_; }

 private:
  FbxManager* fbx_manager_;
};

// Default io settings used to import a scene.
class FbxDefaultIOSettings {
 public:
  // Instantiates default settings.
  explicit FbxDefaultIOSettings(const FbxManagerInstance& _manager);

  // De-instantiates default settings.
  virtual ~FbxDefaultIOSettings();

  // Get FbxIOSettings instance.
  FbxIOSettings* settings() const { return io_settings_; }

  // Cast operator to get internal FbxIOSettings instance.
  operator FbxIOSettings*() const { return io_settings_; }

 private:
  FbxIOSettings* io_settings_;
};

// Io settings used to import an animation from a scene.
class FbxAnimationIOSettings : public FbxDefaultIOSettings {
 public:
  FbxAnimationIOSettings(const FbxManagerInstance& _manager);
};

// Io settings used to import a skeleton from a scene.
class FbxSkeletonIOSettings : public FbxDefaultIOSettings {
 public:
  FbxSkeletonIOSettings(const FbxManagerInstance& _manager);
};

// Implements axis system and unit system conversion helper, from any Fbx system
// to ozz system (FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd,
// FbxAxisSystem::eRightHanded, meter).
// While Fbx sdk FbxAxisSystem::ConvertScene and FbxSystem::ConvertScene only
// affect scene root, this class functions can be used to bake nodes, vertices,
// animations transformations...
class FbxSystemConverter {
 public:
  // Initialize converter with fbx scene systems.
  FbxSystemConverter(const FbxAxisSystem& _from_axis,
                     const FbxSystemUnit& _from_unit);

  // Converts a fbx matrix to an ozz Float4x4 matrix, in ozz axis and unit
  // systems, using _m' = C * _m * (C-1) operation.
  math::Float4x4 ConvertMatrix(const FbxAMatrix& _m) const;

  // Converts fbx matrix to an ozz transform, in ozz axis and unit systems,
  // using _m' = C * _m * (C-1) operation.
  // Can return false if matrix isn't affine.
  bool ConvertTransform(const FbxAMatrix& _m,
                        math::Transform* _transform) const;

  // Converts fbx FbxVector4 point to an ozz Float3, in ozz axis and unit
  // systems, using _p' = C * _p operation.
  math::Float3 ConvertPoint(const FbxVector4& _p) const;

  // Converts fbx FbxVector4 normal to an ozz Float3, in ozz axis and unit
  // systems, using _p' = ((C-1)-T) * _p operation. Normals are converted
  // using the inverse transpose matrix to support non-uniform scale
  // transformations.
  math::Float3 ConvertNormal(const FbxVector4& _p) const;

 private:
  // The matrix used to convert from "from" axis/unit to ozz coordinate system
  // base.
  math::Float4x4 convert_;

  // The inverse of convert_ matrix.
  math::Float4x4 inverse_convert_;

  // The transpose inverse of convert_ matrix.
  math::Float4x4 inverse_transpose_convert_;
};

// Loads a scene from a Fbx file.
class FbxSceneLoader {
 public:
  // Loads the scene that can then be obtained with scene() function.
  FbxSceneLoader(const char* _filename, const char* _password,
                 const FbxManagerInstance& _manager,
                 const FbxDefaultIOSettings& _io_settings);

  FbxSceneLoader(FbxStream* _stream, const char* _password,
                 const FbxManagerInstance& _manager,
                 const FbxDefaultIOSettings& _io_settings);

  ~FbxSceneLoader();

  // Returns a valid scene if fbx import was successful, NULL otherwise.
  FbxScene* scene() const { return scene_; }

  // Returns a valid converter if fbx import was successful, NULL otherwise.
  FbxSystemConverter* converter() { return converter_; }

 private:
  void ImportScene(FbxImporter* _importer, const bool _initialized,
                   const char* _password, const FbxManagerInstance& _manager,
                   const FbxDefaultIOSettings& _io_settings);

  // Scene instance that was loaded from the file.
  FbxScene* scene_;

  // Axis and unit conversion helper.
  FbxSystemConverter* converter_;
};
}  // namespace fbx
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_FBX_FBX_H_
