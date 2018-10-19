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

#include "ozz/animation/offline/tools/import2ozz.h"

#include <cstdlib>
#include <cstring>

#include "animation/offline/tools/import2ozz_anim.h"
#include "animation/offline/tools/import2ozz_config.h"
#include "animation/offline/tools/import2ozz_skel.h"

#include "ozz/base/log.h"

#include "ozz/base/io/stream.h"

#include "ozz/options/options.h"

#include <json/json.h>

// Declares command line options.
OZZ_OPTIONS_DECLARE_STRING(file, "Specifies input file", "", true)

static bool ValidateEndianness(const ozz::options::Option& _option,
                               int /*_argc*/) {
  const ozz::options::StringOption& option =
      static_cast<const ozz::options::StringOption&>(_option);
  bool valid = std::strcmp(option.value(), "native") == 0 ||
               std::strcmp(option.value(), "little") == 0 ||
               std::strcmp(option.value(), "big") == 0;
  if (!valid) {
    ozz::log::Err() << "Invalid endianness option \"" << option << "\""
                    << std::endl;
  }
  return valid;
}

OZZ_OPTIONS_DECLARE_STRING_FN(
    endian,
    "Selects output endianness mode. Can be \"native\" (same as current "
    "platform), \"little\" or \"big\".",
    "native", false, &ValidateEndianness)

ozz::Endianness InitializeEndianness() {
  // Initializes output endianness from options.
  ozz::Endianness endianness = ozz::GetNativeEndianness();
  if (std::strcmp(OPTIONS_endian, "little") == 0) {
    endianness = ozz::kLittleEndian;
  } else if (std::strcmp(OPTIONS_endian, "big") == 0) {
    endianness = ozz::kBigEndian;
  }
  ozz::log::LogV() << (endianness == ozz::kLittleEndian ? "Little" : "Big")
                   << " endian output binary format selected." << std::endl;
  return endianness;
}

static bool ValidateLogLevel(const ozz::options::Option& _option,
                             int /*_argc*/) {
  const ozz::options::StringOption& option =
      static_cast<const ozz::options::StringOption&>(_option);
  bool valid = std::strcmp(option.value(), "verbose") == 0 ||
               std::strcmp(option.value(), "standard") == 0 ||
               std::strcmp(option.value(), "silent") == 0;
  if (!valid) {
    ozz::log::Err() << "Invalid log level option \"" << option << "\""
                    << std::endl;
  }
  return valid;
}

OZZ_OPTIONS_DECLARE_STRING_FN(
    log_level,
    "Selects log level. Can be \"silent\", \"standard\" or \"verbose\".",
    "standard", false, &ValidateLogLevel)

void InitializeLogLevel() {
  ozz::log::Level log_level = ozz::log::GetLevel();
  if (std::strcmp(OPTIONS_log_level, "silent") == 0) {
    log_level = ozz::log::kSilent;
  } else if (std::strcmp(OPTIONS_log_level, "standard") == 0) {
    log_level = ozz::log::kStandard;
  } else if (std::strcmp(OPTIONS_log_level, "verbose") == 0) {
    log_level = ozz::log::kVerbose;
  }
  ozz::log::SetLevel(log_level);
  ozz::log::LogV() << "Verbose log level activated." << std::endl;
}

namespace ozz {
namespace animation {
namespace offline {

int OzzImporter::operator()(int _argc, const char** _argv) {
  // Parses arguments.
  ozz::options::ParseResult parse_result = ozz::options::ParseCommandLine(
      _argc, _argv, "2.0",
      "Imports skeleton and animations from a file and converts it to ozz "
      "binary raw or runtime data format.");
  if (parse_result != ozz::options::kSuccess) {
    return parse_result == ozz::options::kExitSuccess ? EXIT_SUCCESS
                                                      : EXIT_FAILURE;
  }

  // Initialize general executable options.
  InitializeLogLevel();
  const ozz::Endianness endianness = InitializeEndianness();

  Json::Value config;
  if (!ProcessConfiguration(&config)) {
    // Specific error message are reported during configuration processing.
    return EXIT_FAILURE;
  }

  // Ensures file to import actually exist.
  if (!ozz::io::File::Exist(OPTIONS_file)) {
    ozz::log::Err() << "File \"" << OPTIONS_file << "\" doesn't exist."
                    << std::endl;
    return EXIT_FAILURE;
  }

  // Imports animations from the document.
  ozz::log::Log() << "Importing file \"" << OPTIONS_file << "\"" << std::endl;
  if (!Load(OPTIONS_file)) {
    ozz::log::Err() << "Failed to import file \"" << OPTIONS_file << "\"."
                    << std::endl;
    return EXIT_FAILURE;
  }

  // Handles skeleton import processing
  if (!ImportSkeleton(config, this, endianness)) {
    return EXIT_FAILURE;
  }

  // Handles animations import processing
  if (!ImportAnimations(config, this, endianness)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
