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

#ifndef OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_CONFIG_H_
#define OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_CONFIG_H_

#include "ozz/base/platform.h"

#include <json/json-forwards.h>

namespace ozz {
namespace animation {
namespace offline {

// Get the sanitized (all members are set, with the right types) configuration.
bool ProcessConfiguration(Json::Value* _config);

// Internal function used to compare enum names.
bool CompareName(const char* _a, const char* _b);

// Struct allowing inheriting class to provide enum names.
template <typename _Type, typename _Enum>
struct JsonEnum {
  // Struct allowing inheriting class to provide enum names.
  struct EnumNames {
    size_t count;
    const char** names;
  };

  static bool GetEnumFromName(const char* _name, _Enum* _enum) {
    const EnumNames enums = _Type::GetNames();
    for (size_t i = 0; i < enums.count; ++i) {
      if (CompareName(enums.names[i], _name)) {
        *_enum = static_cast<_Enum>(i);
        return true;
      }
    }
    return false;
  }

  static const char* GetEnumName(_Enum _enum) {
    const EnumNames enums = _Type::GetNames();
    assert(static_cast<size_t>(_enum) < enums.count);
    return enums.names[_enum];
  }

  static bool IsValidEnumName(const char* _name) {
    const EnumNames enums = _Type::GetNames();
    bool valid = false;
    for (size_t i = 0; !valid && i < enums.count; ++i) {
      valid = CompareName(enums.names[i], _name);
    }
    return valid;
  }
};

}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_ANIMATION_OFFLINE_TOOLS_IMPORT2OZZ_CONFIG_H_
