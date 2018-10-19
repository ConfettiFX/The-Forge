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

#include "animation/offline/tools/import2ozz_config.h"

#include <fstream>
#include <sstream>

#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/track_optimizer.h"

#include "ozz/base/containers/string.h"
#include "ozz/base/log.h"

#include "ozz/options/options.h"

#include <json/json.h>

bool ValidateExclusiveConfigOption(const ozz::options::Option& _option,
                                   int _argc);
OZZ_OPTIONS_DECLARE_STRING_FN(
    config, "Specifies input configuration string in json format", "", false,
    &ValidateExclusiveConfigOption)
OZZ_OPTIONS_DECLARE_STRING_FN(
    config_file, "Specifies input configuration file in json format", "", false,
    &ValidateExclusiveConfigOption)

// Validate exclusive config options.
bool ValidateExclusiveConfigOption(const ozz::options::Option& _option,
                                   int _argc) {
  (void)_option;
  (void)_argc;
  bool not_exclusive =
      OPTIONS_config_file.value()[0] != 0 && OPTIONS_config.value()[0] != 0;
  if (not_exclusive) {
    ozz::log::Err() << "--config and --config_file are exclusive options."
                    << std::endl;
  }
  return !not_exclusive;
}

OZZ_OPTIONS_DECLARE_STRING(
    config_dump_reference,
    "Dumps reference json configuration to specified file.", "", false)

namespace {

template <typename _Type>
struct ToJsonType;

template <>
struct ToJsonType<int> {
  static Json::ValueType type() { return Json::intValue; }
};
template <>
struct ToJsonType<unsigned int> {
  static Json::ValueType type() { return Json::uintValue; }
};
template <>
struct ToJsonType<float> {
  static Json::ValueType type() { return Json::realValue; }
};
template <>
struct ToJsonType<const char*> {
  static Json::ValueType type() { return Json::stringValue; }
};
template <>
struct ToJsonType<bool> {
  static Json::ValueType type() { return Json::booleanValue; }
};

const char* JsonTypeToString(Json::ValueType _type) {
  switch (_type) {
    case Json::nullValue:
      return "null";
    case Json::intValue:
      return "integer";
    case Json::uintValue:
      return "unsigned integer";
    case Json::realValue:
      return "float";
    case Json::stringValue:
      return "UTF-8 string";
    case Json::booleanValue:
      return "boolean";
    case Json::arrayValue:
      return "array";
    case Json::objectValue:
      return "object";
    default:
      assert(false && "unknown json type");
      return "unknown";
  }
}

bool IsCompatibleType(Json::ValueType _type, Json::ValueType _expected) {
  switch (_expected) {
    case Json::nullValue:
      return _type == Json::nullValue;
    case Json::intValue:
      return _type == Json::intValue || _type == Json::uintValue;
    case Json::uintValue:
      return _type == Json::uintValue;
    case Json::realValue:
      return _type == Json::realValue || _type == Json::intValue ||
             _type == Json::uintValue;
    case Json::stringValue:
      return _type == Json::stringValue;
    case Json::booleanValue:
      return _type == Json::booleanValue;
    case Json::arrayValue:
      return _type == Json::arrayValue;
    case Json::objectValue:
      return _type == Json::objectValue;
    default:
      assert(false && "unknown json type");
      return false;
  }
}

bool MakeDefaultArray(Json::Value& _parent, const char* _name,
                      const char* _comment, bool _empty) {
  // Check if exists first.
  const bool existed = _parent.isMember(_name);
  // Create in any case
  Json::Value* member = &_parent[_name];
  if (!existed) {
    member->resize(_empty ? 0 : 1);
    assert(member->isArray());
  }

  // Pushes comment if there's not one already.
  if (*_comment != 0 && !member->hasComment(Json::commentBefore)) {
    member->setComment(std::string("//  ") + _comment, Json::commentBefore);
  }

  return existed;
}

bool MakeDefaultObject(Json::Value& _parent, const char* _name,
                       const char* _comment) {
  // Check if exists first.
  const bool existed = _parent.isMember(_name);
  // Create in any case
  Json::Value* member = &_parent[_name];
  if (!existed) {
    *member = Json::Value(Json::objectValue);
    assert(member->isObject());
  }

  // Pushes comment if there's not one already.
  if (*_comment != 0 && !member->hasComment(Json::commentBefore)) {
    member->setComment(std::string("//  ") + _comment, Json::commentBefore);
  }

  return existed;
}

template <typename _Type>
bool MakeDefault(Json::Value& _parent, const char* _name, _Type _value,
                 const char* _comment) {
  // Check if exists first.
  const bool existed = _parent.isMember(_name);
  // Create in any case
  Json::Value* member = &_parent[_name];
  if (!existed) {
    *member = _value;
    assert(IsCompatibleType(member->type(), ToJsonType<_Type>::type()));
  }

  // Pushes comment if there's not one already.
  if (*_comment != 0 && !member->hasComment(Json::commentAfterOnSameLine)) {
    member->setComment(std::string("//  ") + _comment,
                       Json::commentAfterOnSameLine);
  }

  return existed;
}

bool SanitizeSkeletonJointTypes(Json::Value& _root, bool _all_options) {
  (void)_all_options;
  MakeDefault(_root, "skeleton", true,
              "Uses skeleton nodes as skeleton joints.");
  MakeDefault(_root, "marker", false, "Uses marker nodes as skeleton joints.");
  MakeDefault(_root, "camera", false, "Uses camera nodes as skeleton joints.");
  MakeDefault(_root, "geometry", false,
              "Uses geometry nodes as skeleton joints.");
  MakeDefault(_root, "light", false, "Uses light nodes as skeleton joints.");
  MakeDefault(_root, "null", false, "Uses null nodes as skeleton joints.");
  MakeDefault(_root, "any", false,
              "Uses any node type as skeleton joints, including those listed "
              "above and any other.");

  return true;
}

bool SanitizeSkeletonImport(Json::Value& _root, bool _all_options) {
  (void)_all_options;
  MakeDefault(
      _root, "enable", true,
      "Imports (from source data file) and writes skeleton output file.");
  MakeDefault(_root, "raw", false, "Outputs raw skeleton.");
  MakeDefaultObject(
      _root, "types",
      "Define nodes types that should be considered as skeleton joints.");
  SanitizeSkeletonJointTypes(_root["types"], _all_options);
  return true;
}

bool SanitizeSkeleton(Json::Value& _root, bool _all_options) {
  MakeDefault(_root, "filename", "skeleton.ozz",
              "Specifies skeleton input/output filename. The file will be "
              "outputted if import is true. It will also be used as an input "
              "reference during animations import.");
  MakeDefaultObject(_root, "import", "Define skeleton import settings.");
  SanitizeSkeletonImport(_root["import"], _all_options);

  return true;
}

bool SanitizeOptimizationTolerances(Json::Value& _root) {
  MakeDefault(
      _root, "translation",
      ozz::animation::offline::AnimationOptimizer().translation_tolerance,
      "Translation optimization tolerance, defined as the distance between two "
      "translation values in meters.");

  MakeDefault(_root, "rotation",
              ozz::animation::offline::AnimationOptimizer().rotation_tolerance,
              "Rotation optimization tolerance, ie: the angle between two "
              "rotation values in radian.");

  MakeDefault(_root, "scale",
              ozz::animation::offline::AnimationOptimizer().scale_tolerance,
              "Scale optimization tolerance, ie: the norm of the difference of "
              "two scales.");

  MakeDefault(
      _root, "hierarchical",
      ozz::animation::offline::AnimationOptimizer().hierarchical_tolerance,
      "Hierarchical translation optimization tolerance, ie: the maximum error "
      "(distance) that an optimization on a joint is allowed to generate on "
      "its whole child hierarchy.");

  return true;
}

bool SanitizeTrackImport(Json::Value& _root) {
  MakeDefault(_root, "filename", "*.ozz",
              "Specifies track output filename(s). Use a \'*\' character "
              "to specify part(s) of the filename that should be replaced by "
              "the track (aka \"joint_name-property_name\") name.");
  MakeDefault(_root, "joint_name", "*",
              "Name of the joint that contains the property to import. "
              "Wildcard characters '*' and '?' are supported.");
  MakeDefault(_root, "property_name", "*",
              "Name of the property to import. Wildcard characters '*' and '?' "
              "are supported.");
  MakeDefault(_root, "type", 1,
              "Type of the property, aka the number of floating point "
              "components. 1 to 4 components are supported.");
  const int components = _root["type"].asInt();
  if (components < 1 || components > 4) {
    ozz::log::Err() << "Invalid value \"" << components
                    << "\" for import track type property. 1 to 4 components "
                       "are supported."
                    << std::endl;
    return false;
  }
  MakeDefault(_root, "raw", false, "Outputs raw track.");
  MakeDefault(_root, "optimize", true, "Activates keyframes optimization.");
  MakeDefault(_root, "optimization_tolerance",
              ozz::animation::offline::TrackOptimizer().tolerance,
              "Optimization tolerance");

  return true;
}
/*
bool SanitizeTrackMotion(Json::Value& _root) {
  MakeDefault(_root, "joint_name", "",
              "Name of the joint that contains the property to import. "
              "Wildcard characters '*' and '?' are supported.");
  MakeDefault(_root, "output", "*.ozz",
              "Specifies track output file(s). Use a \'*\' character to "
              "specify part(s) of the filename that should be replaced by the "
              "joint_name.");
  MakeDefault(_root, "optimization_tolerance",
              ozz::animation::offline::TrackOptimizer().tolerance,
              "Optimization tolerance");
  return true;
}*/

bool SanitizeTrack(Json::Value& _root, bool _all_options) {
  MakeDefaultArray(_root, "properties", "Properties to import.", !_all_options);
  Json::Value& imports = _root["properties"];
  for (Json::ArrayIndex i = 0; i < imports.size(); ++i) {
    if (!SanitizeTrackImport(imports[i])) {
      return false;
    }
  }
  /*
    MakeDefaultArray(_root, "motions", "Motions tracks to generate.",
                     !_all_options);
    Json::Value& motions = _root["motions"];
    for (Json::ArrayIndex i = 0; i < motions.size(); ++i) {
      if (!SanitizeTrackMotion(motions[i])) {
        return false;
      }
    }*/
  return true;
}

bool SanitizeAnimation(Json::Value& _root, bool _all_options) {
  MakeDefault(_root, "clip", "*",
              "Specifies clip name (take) of the animation to import from the "
              "source file. Wildcard characters \'*\' and \'?\' are supported");

  MakeDefault(_root, "filename", "*.ozz",
              "Specifies animation output filename. Use a \'*\' character to "
              "specify part(s) of the filename that should be replaced by the "
              "clip name.");

  MakeDefault(_root, "raw", false, "Outputs raw animation.");

  MakeDefault(
      _root, "additive", false,
      "Creates a delta animation that can be used for additive blending.");

  MakeDefault(_root, "sampling_rate", 0.f,
              "Selects animation sampling rate in hertz. Set a value <= 0 to "
              "use imported scene default frame rate.");

  MakeDefault(_root, "optimize", true,
              "Activates keyframes reduction optimization.");

  MakeDefaultObject(_root, "optimization_tolerances",
                    "Optimization tolerances.");
  SanitizeOptimizationTolerances(_root["optimization_tolerances"]);

  MakeDefaultArray(_root, "tracks", "Tracks to build.", !_all_options);
  Json::Value& tracks = _root["tracks"];
  for (Json::ArrayIndex i = 0; i < tracks.size(); ++i) {
    if (!SanitizeTrack(tracks[i], _all_options)) {
      return false;
    }
  }

  return true;
}  // namespace

bool SanitizeRoot(Json::Value& _root, bool _all_options) {
  // Skeleton
  MakeDefaultObject(_root, "skeleton", "Skeleton to import");
  SanitizeSkeleton(_root["skeleton"], _all_options);

  // Animations.
  // Forces array creation as it's expected for the defaultconfiguration.
  MakeDefaultArray(_root, "animations", "Animations to import.", false);
  Json::Value& animations = _root["animations"];
  for (Json::ArrayIndex i = 0; i < animations.size(); ++i) {
    if (!SanitizeAnimation(animations[i], _all_options)) {
      return false;
    }
  }

  return true;
}

bool RecursiveCheck(const Json::Value& _root, const Json::Value& _expected,
                    ozz::String::Std _name) {
  if (!IsCompatibleType(_root.type(), _expected.type())) {
    // It's a failure to have a wrong member type.
    ozz::log::Err() << "Invalid type \"" << JsonTypeToString(_root.type())
                    << "\" for json member \"" << _name << "\". \""
                    << JsonTypeToString(_expected.type()) << "\" expected."
                    << std::endl;
    return false;
  }

  if (_root.isArray()) {
    assert(_expected.isArray());
    for (Json::ArrayIndex i = 0; i < _root.size(); ++i) {
      std::ostringstream istr;
      istr << "[" << i << "]";
      if (!RecursiveCheck(_root[i], _expected[0], _name + istr.str().c_str())) {
        return false;
      }
    }
  } else if (_root.isObject()) {
    assert(_expected.isObject());
    for (Json::Value::iterator it = _root.begin(); it != _root.end(); it++) {
      const std::string& name = it.name();
      if (!_expected.isMember(name)) {
        ozz::log::Err() << "Invalid json member \""
                        << _name + "." + name.c_str() << "\"." << std::endl;
        return false;
      }
      const Json::Value& expected_member = _expected[name];
      if (!RecursiveCheck(*it, expected_member, _name + "." + name.c_str())) {
        return false;
      }
    }
  }
  return true;
}

std::string ToString(const Json::Value& _value) {
  // Format configuration
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "  ";
  builder["precision"] = 4;
  return Json::writeString(builder, _value);
}

bool DumpConfig(const char* _path, const Json::Value& _config) {
  if (_path[0] != 0) {
    ozz::log::LogV() << "Opens config file to dump: " << _path << std::endl;
    std::ofstream file(_path);
    if (!file.is_open()) {
      ozz::log::Err() << "Failed to open config file to dump: \"" << _path
                      << "\"" << std::endl;
      return false;
    }
    const std::string& document = ToString(_config);
    file << document;
  }
  return true;
}
}  // namespace

bool ProcessConfiguration(Json::Value* _config) {
  if (!_config) {
    return false;
  }

  // Use {} as a default config, otherwise take the one specified as argument.
  std::string config_string = "{}";
  // Takes config from program options.
  if (OPTIONS_config.value()[0] != 0) {
    config_string = OPTIONS_config.value();
  } else if (OPTIONS_config_file.value()[0] != 0) {
    ozz::log::LogV() << "Opens config file: \"" << OPTIONS_config_file << "\"."
                     << std::endl;

    std::ifstream file(OPTIONS_config_file.value());
    if (!file.is_open()) {
      ozz::log::Err() << "Failed to open config file: \"" << OPTIONS_config_file
                      << "\"." << std::endl;
      return false;
    }
    config_string.assign(std::istreambuf_iterator<char>(file),
                         std::istreambuf_iterator<char>());
  } else {
    ozz::log::Log() << "No configuration provided, using default configuration."
                    << std::endl;
  }

  Json::Reader json_builder;
  if (!json_builder.parse(config_string, *_config, true)) {
    ozz::log::Err() << "Error while parsing configuration string: "
                    << json_builder.getFormattedErrorMessages() << std::endl;
    return false;
  }

  // Build the reference config to compare it with provided one and detect
  // unexpected members.
  Json::Value ref_config;
  if (!SanitizeRoot(ref_config, true)) {
    assert(false && "Failed to sanitized default configuration.");
  }

  // All format errors are reported within that function
  if (!RecursiveCheck(*_config, ref_config, "root")) {
    return false;
  }

  // Sanitized provided config.
  if (!SanitizeRoot(*_config, false)) {
    return false;
  }

  // Dumps the config to LogV now it's sanitized.
  if (ozz::log::GetLevel() >= ozz::log::kVerbose) {
    const std::string& document = ToString(*_config);
    ozz::log::LogV() << "Sanitized configuration:" << std::endl
                     << document << std::endl;
  }

  // Dumps reference config to file.
  if (!DumpConfig(OPTIONS_config_dump_reference.value(), ref_config)) {
    return false;
  }

  return true;
}
