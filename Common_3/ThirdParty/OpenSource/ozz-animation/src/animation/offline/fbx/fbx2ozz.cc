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

#include "animation/offline/fbx/fbx2ozz.h"

#include "ozz/base/log.h"

int main(int _argc, const char** _argv) {
  Fbx2OzzImporter converter;
  return converter(_argc, _argv);
}

Fbx2OzzImporter::Fbx2OzzImporter()
    : settings_(fbx_manager_), scene_loader_(NULL) {}

Fbx2OzzImporter::~Fbx2OzzImporter() {
  ozz::memory::default_allocator()->Delete(scene_loader_);
}

bool Fbx2OzzImporter::Load(const char* _filename) {
  ozz::memory::default_allocator()->Delete(scene_loader_);
  scene_loader_ = ozz::memory::default_allocator()
                      ->New<ozz::animation::offline::fbx::FbxSceneLoader>(
                          _filename, "", fbx_manager_, settings_);

  if (!scene_loader_->scene()) {
    ozz::log::Err() << "Failed to import file " << _filename << "."
                    << std::endl;
    ozz::memory::default_allocator()->Delete(scene_loader_);
    scene_loader_ = NULL;
    return false;
  }
  return true;
}
