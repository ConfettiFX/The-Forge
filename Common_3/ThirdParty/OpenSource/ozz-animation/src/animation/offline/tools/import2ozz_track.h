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

#ifndef OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_TRACK_H_
#define OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_TRACK_H_

#include "ozz/base/endianness.h"
#include "ozz/base/platform.h"

#include "animation/offline/tools/import2ozz_config.h"
#include "ozz/animation/offline/tools/import2ozz.h"

namespace Json {
class Value;
}

namespace ozz {
namespace animation {
class Skeleton;
namespace offline {

class OzzImporter;
bool ProcessTracks(OzzImporter& _importer, const char* _animation_name,
                   const Skeleton& _skeleton, const Json::Value& _config,
                   const ozz::Endianness _endianness);

// Property type enum to config string conversions.
struct PropertyTypeConfig
    : JsonEnum<PropertyTypeConfig, OzzImporter::NodeProperty::Type> {
  static EnumNames GetNames();
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_TRACK_H_
