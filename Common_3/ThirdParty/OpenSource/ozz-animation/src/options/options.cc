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

#include "ozz/options/options.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <new>
#include <sstream>

namespace ozz {
namespace options {

namespace internal {

namespace {

// Declares and instantiates a global object that aims to delete the global
// BaseRegistrer parser. This global BaseRegistrer parser is allocated when
// options are being registered or when ParseCommandLine is called.
static class GlobalRegistrer {
 public:
  ~GlobalRegistrer() {
    if (parser_) {
      parser_->~Parser();
      parser_ = NULL;
    }
  }

  static Parser* Construct() {
    if (!parser_) {
      parser_ = new (placeholder_) Parser;
    }
    return parser_;
  }

  static Parser* parser() { return parser_; }

 private:
  // Take advantage of the default initialization (to 0) of file-scope level
  // variables to ensure that registration does not depend on the initialization
  // order of any other variable.
  static Parser* parser_;

  // The global parser is allocated from the static section in order to avoid
  // any heap allocation before entering the main (and after exiting it also).
  // Uses an array of pointers (void*) in order to ensure a native minimum
  // alignment.
  static void* placeholder_[];
} g_global_registrer;

// Default initialization to 0 only. See parser_ member document above.
Parser* GlobalRegistrer::parser_;

// Compute placeholder_ array size and performs default initialization only.
// See placeholder_ member document above.
static const size_t kParserPlaceholerSize =
    (sizeof(Parser) + sizeof(void*) - 1) / sizeof(void*);
void* GlobalRegistrer::placeholder_[kParserPlaceholerSize];
}  // namespace

// Implements Registrer constructor and destructor.
template <typename _Option>
Registrer<_Option>::Registrer(const char* _name, const char* _help,
                              typename _Option::Type _default, bool _required,
                              typename _Option::ValidateFn _fn)
    : _Option(_name, _help, _default, _required, _fn) {
  if (!internal::g_global_registrer.Construct()->RegisterOption(this)) {
    std::cerr << "Failed to register option " << _name << std::endl;
  }
}

template <typename _Option>
Registrer<_Option>::~Registrer() {
  Parser* parser = internal::g_global_registrer.parser();
  if (parser) {
    parser->UnregisterOption(this);
  }
}

// Explicit instantiation of all supported types of Registrer.
template class Registrer<TypedOption<bool> >;
template class Registrer<TypedOption<int> >;
template class Registrer<TypedOption<float> >;
template class Registrer<TypedOption<const char*> >;
}  // namespace internal

// Construct the parser if no option is registered.
// This local parser will be deleted automatically. This allows to query the
// executable path and name, as well as the usage and version strings even if
// no option is registered.
ParseResult ParseCommandLine(int _argc, const char* const* _argv,
                             const char* _version, const char* _usage) {
  // Need to instantiate a local parser if no option is registered.
  Parser* parser = internal::g_global_registrer.Construct();

  // Set usage and version strings and parse command line.
  parser->set_usage(_usage);
  parser->set_version(_version);
  return parser->Parse(_argc, _argv);
}

// A NULL parser means that no option is registered and that ParseCommandLine
// has not been called.
std::string ParsedExecutablePath() {
  Parser* parser = internal::g_global_registrer.parser();
  if (!parser) {
    return std::string();
  }
  return parser->executable_path();
}

// See Parser::ExecutableName().
const char* ParsedExecutableName() {
  Parser* parser = internal::g_global_registrer.parser();
  if (!parser) {
    return "";
  }
  return parser->executable_name();
}

// See Parser::Usage().
const char* ParsedExecutableUsage() {
  Parser* parser = internal::g_global_registrer.parser();
  if (!parser) {
    return "";
  }
  return parser->usage();
}

namespace {

// Portable case insensitive string comparison functions.
int StrICmp(const char* _left, const char* _right) {
  int l, r;
  do {
    l = static_cast<unsigned char>(std::tolower(*(_left++)));
    r = static_cast<unsigned char>(std::tolower(*(_right++)));
  } while (l && (l == r));
  return l - r;
}

int StrNICmp(const char* _left, const char* _right, size_t _count) {
  if (_count) {
    int l, r;
    do {
      l = static_cast<unsigned char>(std::tolower(*(_left++)));
      r = static_cast<unsigned char>(std::tolower(*(_right++)));
    } while (--_count && l && (l == r));
    return l - r;
  }
  return 0;
}

// Returns the first character after _option, or NULL if option has not been
// found.
const char* ParseOption(const char* _argv, const char* _prefix,
                        const char* _option) {
  const size_t prefix_len = std::strlen(_prefix);
  const size_t option_len = std::strlen(_option);

  // All options start with --.
  if (StrNICmp(_argv, _prefix, prefix_len) != 0) {
    return NULL;
  }
  _argv += prefix_len;
  if (!StrNICmp(_argv, _option, option_len)) {
    return _argv + option_len;
  }

  return NULL;
}

bool Parse(const char* _argv, const char* _option, bool* _value) {
  assert(_value && _option && _argv);

  const char* option_end = ParseOption(_argv, "--", _option);
  if (!option_end) {
    // Test for the explicit option of form --no*.
    option_end = ParseOption(_argv, "--no", _option);
    if (option_end && *option_end == '\0') {
      *_value = false;
      return true;
    }
    return false;
  }

  // Option _option was found, now checks for a valid value.
  if (*option_end == '\0') {
    // Implicit boolean options have no trailing characters.
    *_value = true;
    return true;
  } else if (*option_end == '=') {
    // Explicit options values are set after the '=' character.
    // Using StrICmp function ensures an exact match, ie no trailing characters.
    for (++option_end; std::isspace(*option_end);
         ++option_end) {  // Trims spaces.
    }
    const char* true_options[] = {"yes", "true", "1", "t", "y"};
    for (size_t i = 0; i < sizeof(true_options) / sizeof(true_options[0]);
         i++) {
      if (!StrICmp(option_end, true_options[i])) {
        *_value = true;
        return true;
      }
    }
    const char* false_options[] = {"no", "false", "0", "f", "n"};
    for (size_t i = 0; i < sizeof(false_options) / sizeof(false_options[0]);
         ++i) {
      if (!StrICmp(option_end, false_options[i])) {
        *_value = false;
        return true;
      }
    }
  }
  return false;  // Option was not found or is invalid.
}

bool Parse(const char* _argv, const char* _option, float* _value) {
  assert(_value && _option && _argv);

  const char* option_end = ParseOption(_argv, "--", _option);
  if (option_end && *option_end == '=') {
    for (++option_end; std::isspace(*option_end);
         ++option_end) {  // Trims spaces.
    }
    char* found;
    double double_value = std::strtod(option_end, &found);
    // No trailing characters are allowed.
    if (found != option_end && *found == '\0') {
      *_value = static_cast<float>(double_value);
      return true;
    }
  }
  return false;  // Option was not found or is invalid.
}

bool Parse(const char* _argv, const char* _option, int* _value) {
  assert(_value && _option && _argv);

  const char* option_end = ParseOption(_argv, "--", _option);
  if (option_end && *option_end == '=') {
    for (++option_end; std::isspace(*option_end);
         ++option_end) {  // Trims spaces.
    }
    char* found;
    long long_value = std::strtol(option_end, &found, 10);
    // No trailing characters are allowed. If base is 0,
    if (found != option_end && *found == '\0') {
      *_value = static_cast<int>(long_value);
      return true;
    }
  }
  return false;  // Option was not found or is invalid.
}

bool Parse(const char* _argv, const char* _option, const char** _value) {
  assert(_value && _option && _argv);

  const char* option_end = ParseOption(_argv, "--", _option);
  if (option_end && *option_end == '=') {
    for (++option_end; std::isspace(*option_end);
         ++option_end) {  // Trims spaces.
    }
    *_value = option_end;
    return true;
  }
  return false;  // Option was not found or is invalid.
}

// Format option type using template specialization.
template <typename _Type>
const char* FormatOptionType();

// Specialization of FormatOptionType for all supported types.
template <>
const char* FormatOptionType<bool>() {
  return "bool";
}
template <>
const char* FormatOptionType<float>() {
  return "float";
}
template <>
const char* FormatOptionType<int>() {
  return "int";
}
template <>
const char* FormatOptionType<const char*>() {
  return "string";
}

// Validate exclusive options.
bool ValidateExclusiveOption(const Option& _option, int _argc) {
  if (static_cast<const BoolOption&>(_option).value() && _argc != 1) {
    std::cout << "Option \"" << _option.name()
              << "\" is an exclusive option. "
                 "It must not be used with any other option."
              << std::endl;
    // This is an exclusive flag.
    return false;
  }
  return true;
}
}  // namespace

Option::Option(const char* _name, const char* _help, bool _required,
               ValidateFn _validate)
    : name_(_name ? _name : ""),
      help_(_help ? _help : "..."),
      required_(_required),
      parsed_(false),
      validate_(_validate) {}

Option::~Option() {}

bool Option::Validate(int _argc) {
  if (validate_) {
    return (*validate_)(*this, _argc);
  }
  return true;
}

bool Option::Parse(const char* _argv) {
  if (ParseImpl(_argv) && !parsed_) {  // Fails if argument's already specified.
    parsed_ = true;
    return true;
  }
  return false;
}

void Option::RestoreDefault() {
  parsed_ = false;
  RestoreDefaultImpl();  // Restores actual value.
}

template <typename _Type>
bool TypedOption<_Type>::ParseImpl(const char* _argv) {
  return ozz::options::Parse(_argv, name(), &value_);
}

template <typename _Type>
std::string TypedOption<_Type>::FormatDefault() const {
  std::stringstream str;
  str << "\"" << std::boolalpha << default_ << "\"";
  return str.str();
}

template <typename _Type>
const char* TypedOption<_Type>::FormatType() const {
  return ozz::options::FormatOptionType<_Type>();
}

// Explicit instantiation of all supported types of TypedOption.
template class TypedOption<bool>;
template class TypedOption<int>;
template class TypedOption<float>;
template class TypedOption<const char*>;

Parser::Parser()
    : options_count_(0),
      builtin_options_count_(0),
      executable_path_begin_(""),
      executable_path_end_(executable_path_begin_ + 1),
      executable_name_(""),
      version_(NULL),
      usage_(NULL),
      builtin_version_("version", "Displays application version", false, false,
                       &ValidateExclusiveOption),
      builtin_help_("help", "Displays help", false, false,
                    &ValidateExclusiveOption) {
  // Set default values.
  set_version(NULL);
  set_usage(NULL);
  // Registers built-in options.
  RegisterOption(&builtin_version_);
  RegisterOption(&builtin_help_);
  builtin_options_count_ = options_count_;
}

Parser::~Parser() {
  UnregisterOption(&builtin_version_);
  UnregisterOption(&builtin_help_);
}

ParseResult Parser::Parse(int _argc, const char* const* _argv) {
  if (_argc < 1 || !_argv) {
    return kExitFailure;
  }

  // Select the left most '/' or '\\' separator.
  const char* path_end =
      std::max(std::strrchr(_argv[0], '/'), strrchr(_argv[0], '\\'));
  if (path_end) {
    ++path_end;  // Includes the last separator.
    executable_path_begin_ = _argv[0];
    executable_path_end_ = path_end;
    executable_name_ = path_end;
  } else {
    executable_path_begin_ = "";
    executable_path_end_ = executable_path_begin_ + 1;
    executable_name_ = _argv[0];
  }

  // The first argument is skipped because it is the program path.
  ++_argv;
  --_argc;

  // Hides all arguments after a "--" argument, substitutes argc to argc_trunc.
  int argc_trunc = 0;
  for (; argc_trunc < _argc && std::strcmp(_argv[argc_trunc], "--") != 0;
       ++argc_trunc) {
  }

  // Restores built-in options to their default value in case parsing in done
  // multiple times.
  for (int i = 0; i < options_count_; ++i) {
    options_[i]->RestoreDefault();
  }

  // Iterates all arguments and all options.
  ParseResult result = kSuccess;
  for (int i = 0; i < argc_trunc; ++i) {
    const char* argv = _argv[i];

    // Empty arguments aren't consider invalid.
    if (*argv == 0) {
      continue;
    }

    int j = 0;
    for (; j < options_count_; ++j) {
      if (options_[j]->Parse(argv)) {
        break;  // Also breaks if argument is duplicated.
      }
    }
    // An invalid (or duplicated) command line argument is a fatal failure.
    if (j == options_count_) {
      std::cout << "Invalid command line argument:\"" << argv << "\"."
                << std::endl;
      result = kExitFailure;
      break;
    }
  }

  // Validate build-in options first.
  // They need to be validated and tested first, as they have priority over
  // others, even required once.
  if (!builtin_help_.Validate(argc_trunc) ||
      !builtin_version_.Validate(argc_trunc)) {
    result = kExitFailure;
  }

  // Display built-in help.
  if (result == kSuccess && builtin_help_) {
    Help();
    result = kExitSuccess;
  }

  // Display built-in version.
  if (result == kSuccess && builtin_version_) {
    std::cout << "version " << version() << std::endl;
    result = kExitSuccess;
  }

  // Ensures all required options were specified in the command line.
  if (result == kSuccess) {
    for (int i = 0; i < options_count_; ++i) {
      if (!options_[i]->statisfied()) {
        std::cout << "Required option \"" << options_[i]->name()
                  << "\" is not specified." << std::endl;
        result = kExitFailure;
        break;
      }
    }
  }

  // Validates all options.
  if (result == kSuccess) {
    for (int i = 0; i < options_count_; ++i) {
      if (!options_[i]->Validate(argc_trunc)) {
        result = kExitFailure;
        break;
      }
    }
  }

  // Also displays help if an error occurred.
  if (result == kExitFailure) {
    Help();
  }

  return result;
}

void Parser::Help() {
  std::cout << std::endl;
  std::cout << executable_name() << " version " << version() << std::endl;
  std::cout << usage() << std::endl;
  std::cout << std::endl;

  // Displays usage line.
  std::cout << "Usage:" << std::endl;
  std::cout << executable_name();
  for (int i = 0; i < options_count_; ++i) {
    const Option& option = *options_[i];
    std::cout << ' ';
    if (option.required()) {
      std::cout << '[';
    }
    std::cout << "--" << option.name();
    if (option.required()) {
      std::cout << ']';
    }
  }
  std::cout << std::endl;

  // Displays option details.
  std::cout << "\nWhere:" << std::endl;
  for (int i = 0; i < options_count_; ++i) {
    const Option& option = *options_[i];
    const std::string option_str =
        std::string(" --") + option.name() + "=<" + option.FormatType() + ">";
    std::cout << std::setiosflags(std::ios::left) << std::setw(28) << option_str
              << std::resetiosflags(std::ios::left) << option.help()
              << "(default is " << option.FormatDefault() << ")" << std::endl;
  }

  const char* how_to =
      "\n"
      "Syntax:\n"
      "To set an option from the command line use the form --option=value for\n"
      "non-boolean options, and --option/--nooption for booleans.\n"
      "For example, \"foo --var=46\" will set \"var\" variable to 46.\n"
      "If \"var\" type is not compatible with the specified argument (in this\n"
      "case not an integer, a float or a string), then this help message\n"
      "is displayed and application exits.\n"
      "\n"
      "Boolean options can be set using different syntax:\n"
      "- to set a boolean option to true: \"--var\", \"--var=true\", "
      "\"--var=t\","
      "  \"--var=yes\", \"--var=y\", \"--var=1\".\n"
      "- to set a boolean option to false: \"--novar\", \"--var=false\","
      "   \"--var=f\", \"--var=no\", \"--var=n\", \"--var=0\".\n"
      "Consistently using single-form --option/--nooption is recommended "
      "though.";
  std::cout << how_to << std::endl;
}

namespace {
// Sort required options first, and then based on their names.
bool sort_options(Option* _left, Option* _right) {
  return (_left->required() && !_right->required()) ||
         (_left->required() == _right->required() &&
          std::strcmp(_left->name(), _right->name()) < 0);
}
}  // namespace

bool Parser::RegisterOption(Option* _option) {
  if (!_option) {
    return false;
  }
  if (options_count_ == sizeof(options_) / sizeof(options_[0])) {
    return false;
  }

  // Tests for duplicate options.
  if (std::count(options_, options_end(), _option) != 0) {
    return false;
  }

  // Empty (or NULL) names aren't allowed.
  if (_option->name()[0] == '\0') {
    std::cerr << "Empty (or NULL) names aren't allowed." << std::endl;
    return false;
  }

  // Test for duplicate options' name.
  for (int i = 0; i < options_count_; ++i) {
    if (StrICmp(options_[i]->name(), _option->name()) == 0) {
      std::cerr << "Option name:\"" << _option->name()
                << "\" already registered." << std::endl;
      return false;
    }
  }

  // Adds the option and maintains lexical order.
  options_[options_count_++] = _option;
  std::inplace_merge(options_, options_end() - 1, options_end(), &sort_options);

  return true;
}

bool Parser::UnregisterOption(Option* _option) {
  if (!_option) {
    return false;
  }

  // Finds and removes _option from the collection.
  Option** it = std::remove(options_, options_end(), _option);
  if (it != options_end()) {
    return --options_count_ == builtin_options_count_;
  }
  return false;
}

int Parser::max_options() const {
  return sizeof(options_) / sizeof(options_[0]) - builtin_options_count_;
}

void Parser::set_usage(const char* _usage) {
  usage_ = _usage ? _usage : "unspecified usage";
}

void Parser::set_version(const char* _version) {
  version_ = _version ? _version : "unspecified version";
}

const char* Parser::usage() const { return usage_; }

const char* Parser::version() const { return version_; }

std::string Parser::executable_path() const {
  return std::string(executable_path_begin_, executable_path_end_);
}

const char* Parser::executable_name() const { return executable_name_; }
}  // namespace options
}  // namespace ozz
