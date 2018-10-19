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

#ifndef OZZ_OZZ_OPTIONS_OPTIONS_H_
#define OZZ_OZZ_OPTIONS_OPTIONS_H_

// Implements a command line option processing utility. It helps with command
// line parsing by converting arguments to c++ objects of type bool, int, float
// or const char* c string.
// Unlike getogt(), program options can be scattered in the source files (a la
// google-gflags). Options are collected by a parser which then automatically
// generate the help/usage screen based on registered options.
//
// This library is made of two c++ file (.h/.cc) with no other dependency.
//
// To set an option from the command line, use the form --option=value for
// non-boolean options, and --option/--nooption for booleans.
// For example, "--var=46" will set "var" variable to 46. If "var" type is not
// compatible with the specified argument (in this case an integer, a float or a
// string), then the parser displays the help message and requires application
// to exit.
//
// Boolean options can be set using different syntax:
// - to set a boolean option to true: "--var", "--var=true", "--var=t",
//   "--var=yes", "--var=y", "--var=1".
// - to set a boolean option to false: "--novar", "--var=false", "--var=f",
//   "--var=no", "--var=n", "--var=0".
// Consistently using single-form --option/--nooption is recommended though.
//
// Specifying an option (in the command line) that has not been registered is
// an error, it will require the application to exit.
//
// As in getopt() and gflags, -- by itself terminates flags processing. So in:
// "foo -f1=1 -- -f2=2", f1 is considered but -f2 is not.
//
// Parsing is invoked through ozz::options::ParseCommandLine function, providing
// argc and argv arguments of the main function. This function also takes as
// argument two strings to specify the version and usage message.
//
// To declare/register a new option, use OZZ_OPTIONS_DECLARE_* like macros.
// Supported options types are bool, int, float and string (c string).
// OZZ_OPTIONS_DECLARE_* macros arguments allow to give the option a:
// - name, used in the code to read option value.
// - description, used by the help screen.
// - default value.
// - required flag, that specifies if the option is optional.
// So for example, in order to define a boolean "verbose" option, that is false
// by default and optional (ie: not required):
// OZZ_OPTIONS_DECLARE_BOOL(verbose, "Display verbose output", false, false);
// This option can then be referenced from the code using OPTIONS_verbose c++
// global variable, that implement an automatic cast operator to the option's
// type (bool in this case).
//
// The parser also integrates built-in options:
// --help displays the help screen, that is automatically generated based on the
// registered options.
// --version displays executable's version.

#include <cstddef>
#include <string>

namespace ozz {
namespace options {

// Eumerates options parsing results.
enum ParseResult {
  // Command line was parsed successfully.
  kSuccess,
  // Command line was parsed successfully, but an argument (like --help) was
  // specified and requires application to exit.
  kExitSuccess,
  // Command line parsing failed because of an invalid option or syntax. See
  // std::cout output for more details.
  kExitFailure,
};

// Parses all registered options using the command line specified with (_argc,
// _argv) arguments. Options are registered using OZZ_OPTIONS_DECLARE_* macros.
// Valid command line syntax is explained on top of this options.h file and
// displayed by the help/usage screen.
// ParseCommandLine expects that _argc >= 1 and _argv[0] is the executable path.
// _version and _usage are used to respectively set the executable version and
// usage string in case that the help/usage screen is displayed (--help).
// _version and _usage are not copied, ParseCommandLine caller is in charge of
// maintaining their allocation during application lifetime.
// See ParseResult for more details about returned values.
ParseResult ParseCommandLine(int _argc, const char* const* _argv,
                             const char* _version, const char* _usage);

// Get the executable path that was extracted from the last call to
// ParseCommandLine.
// If ParseCommandLine has never been called, then ParsedExecutablePath
// returns a default empty string.
std::string ParsedExecutablePath();

// Get the executable name that was extracted from the last call to
// ParseCommandLine.
// If ParseCommandLine has never been called, then ParsedExecutableName
// returns a default empty string.
const char* ParsedExecutableName();

// Get the executable usage that was extracted from the last call to
// ParseCommandLine.
// If ParseCommandLine has never been called, then ParsedExecutableUsage
// returns a default empty string.
const char* ParsedExecutableUsage();

#define OZZ_OPTIONS_DECLARE_BOOL(_name, _help, _default, _required)    \
  OZZ_OPTIONS_DECLARE_VARIABLE(ozz::options::BoolOption, _name, _help, \
                               _default, _required)
#define OZZ_OPTIONS_DECLARE_BOOL_FN(_name, _help, _default, _required, _fn) \
  OZZ_OPTIONS_DECLARE_VARIABLE_FN(ozz::options::BoolOption, _name, _help,   \
                                  _default, _required, _fn)

#define OZZ_OPTIONS_DECLARE_INT(_name, _help, _default, _required)    \
  OZZ_OPTIONS_DECLARE_VARIABLE(ozz::options::IntOption, _name, _help, \
                               _default, _required)
#define OZZ_OPTIONS_DECLARE_INT_FN(_name, _help, _default, _required, _fn) \
  OZZ_OPTIONS_DECLARE_VARIABLE_FN(ozz::options::IntOption, _name, _help,   \
                                  _default, _required, _fn)

#define OZZ_OPTIONS_DECLARE_FLOAT(_name, _help, _default, _required)    \
  OZZ_OPTIONS_DECLARE_VARIABLE(ozz::options::FloatOption, _name, _help, \
                               _default, _required)
#define OZZ_OPTIONS_DECLARE_FLOAT_FN(_name, _help, _default, _required, _fn) \
  OZZ_OPTIONS_DECLARE_VARIABLE_FN(ozz::options::FloatOption, _name, _help,   \
                                  _default, _required, _fn)

#define OZZ_OPTIONS_DECLARE_STRING(_name, _help, _default, _required)    \
  OZZ_OPTIONS_DECLARE_VARIABLE(ozz::options::StringOption, _name, _help, \
                               _default, _required)
#define OZZ_OPTIONS_DECLARE_STRING_FN(_name, _help, _default, _required, _fn) \
  OZZ_OPTIONS_DECLARE_VARIABLE_FN(ozz::options::StringOption, _name, _help,   \
                                  _default, _required, _fn)

#define OZZ_OPTIONS_DECLARE_VARIABLE(_type, _name, _help, _default, _required) \
  /* Instantiates a registrer for an option of type _type with name _name */   \
  static ozz::options::internal::Registrer<_type> OPTIONS_##_name(             \
      #_name, _help, _default, _required);
#define OZZ_OPTIONS_DECLARE_VARIABLE_FN(_type, _name, _help, _default,       \
                                        _required, _fn)                      \
  /* Instantiates a registrer for an option of type _type with name _name */ \
  static ozz::options::internal::Registrer<_type> OPTIONS_##_name(           \
      #_name, _help, _default, _required, _fn);

// Defines option interface.
class Option {
 public:
  // Returns option's name.
  const char* name() const { return name_; }

  // Returns help string.
  const char* help() const { return help_; }

  // A required option is satisfied if it was successfully parsed from the
  // command line. Non required option are always satisfied.
  bool statisfied() const { return parsed_ || !required_; }

  // Returns true if the option is required.
  bool required() const { return required_; }

  // Calls validation function if one is set.
  // Returns true if no function is set, or the function returns true.
  bool Validate(int _argc);

  // Parse the command line and set the option's value.
  // Returns true if argument parsing succeeds, false if argument doesn't match
  // or was already parsed (in case of an argument specified more than once).
  bool Parse(const char* _argv);

  // Restores default value.
  void RestoreDefault();

  // Outputs default value as a string.
  virtual std::string FormatDefault() const = 0;

  // Outputs type of value as a c string.
  virtual const char* FormatType() const = 0;

  // Implements the sorting operator.
  bool operator<(const Option& _option) const { return name_ < _option.name_; }

 protected:
  // Declares validation function prototype.
  // _option is the option to validate.
  // _argc the number of argument processed.
  // *_exit can be set to true to require application to exit. This flag is
  // relevant only if the function does not return false. Application will
  // exit anyway if false is returned.
  typedef bool (*ValidateFn)(const Option& _option, int _argc);

  // Construct an option.
  // _name and _help are set to an empty c string if NULL.
  Option(const char* _name, const char* _help, bool _required,
         ValidateFn _validate = NULL);

  // Destructor.
  virtual ~Option();

  // Parse the command line and set the option value.
  virtual bool ParseImpl(const char* _argv) = 0;

  // Restores default value typed implementation.
  virtual void RestoreDefaultImpl() = 0;

 private:
  // Option's name.
  const char* name_;

  // Option's help message.
  const char* help_;

  // Is this option required?
  bool required_;

  // Was this option successfully parsed from the command line.
  bool parsed_;

  // Validate function. NULL if no function is set.
  ValidateFn validate_;
};

// Defines a strongly typed option class
template <typename _Type>
class TypedOption : public Option {
 public:
  // Lets the type be known.
  typedef _Type Type;

  // Defines an option.
  TypedOption(const char* _name, const char* _help, _Type _default,
              bool _required, Option::ValidateFn _validate = NULL)
      : Option(_name, _help, _required, _validate),
        default_(_default),
        value_(_default) {}

  virtual ~TypedOption() {}

  // Implicit conversion to the option type.
  operator _Type() const { return value_; }

  // Explicit getter.
  const _Type& value() const { return value_; }

  // Get the default value.
  const _Type& default_value() const { return default_; }

 private:
  // Parse the command line and set the option value.
  virtual bool ParseImpl(const char* _argv);

  // Restores default value implementation.
  virtual void RestoreDefaultImpl() { value_ = default_; }

  // Outputs default value as a string.
  virtual std::string FormatDefault() const;

  // Outputs type of value as a string.
  virtual const char* FormatType() const;

  // Default option's value.
  _Type default_;

  // Current option's value.
  _Type value_;
};

// Declares all available option types.
typedef TypedOption<bool> BoolOption;
typedef TypedOption<int> IntOption;
typedef TypedOption<float> FloatOption;
typedef TypedOption<const char*> StringOption;

// Declares the option parser class.
// Option are registered by the parser and updated when command line arguments
// are parsed.
class Parser {
 public:
  // Construct a parser with only built-in options.
  Parser();

  // Destroys the parser. Options does not need to be unregistered.
  ~Parser();

  // Parses the command line against all registered options.
  // _argv arguments are parser until the end to the first "--" argument found.
  // Note that _argv arguments memory allocations must remains valid for the
  // life of parser, as some arguments like string options or executable path
  // and name will be pointed by the parser (ie: not copied).
  // See ParseResult for more details about returned values.
  ParseResult Parse(int _argc, const char* const* _argv);

  // Displays the help screen that is automatically built from all registered
  // options. Executable name is only available if ::Parse() was called with a
  // valid argv[0].
  void Help();

  // Registers a new option in this parser.
  // Registered options are updated when Parse() is called.
  // Returns true on success or false if:
  // - _option is not a valid option (ex: bad name...), or if an
  //   option with the same name already exists.
  // - _option si NULL.
  // - more than kMaxOptions were registered.
  bool RegisterOption(Option* _option);

  // Unregisters an option that was successfully registered using
  // ::RegisterOption().
  // Returns true if no more option is registered.
  bool UnregisterOption(Option* _option);

  // Get the maximum number of options that can be registered.
  // This excludes built-in options.
  int max_options() const;

  // Set executable usage string.
  // Note that _usage string will not be copied but rather pointed. This means
  // that _usage allocation must remain valid for all the parser's life.
  void set_usage(const char* _usage);

  // Get executable usage string.
  const char* usage() const;

  // Set executable version string.
  // Note that _usage string will not be copied but rather pointed. This means
  // that _usage allocation must remain valid for all the parser's life.
  void set_version(const char* _version);

  // Get executable version string.
  const char* version() const;

  // Returns the path of the executable that was extracted from argument 0.
  std::string executable_path() const;

  // Returns the name of the executable that was extracted from argument 0.
  const char* executable_name() const;

 private:
  // Get end of registered options array.
  Option** options_end() { return options_ + options_count_; }

  // Collection of registered options.
  Option* options_[32];

  // Number of registered options, including built-in options.
  int options_count_;

  // Number of built-in options.
  int builtin_options_count_;

  // The path of the executable, extracted from the first argument.
  const char* executable_path_begin_;
  const char* executable_path_end_;

  // The name of the executable, extracted from the first argument.
  const char* executable_name_;

  // Executable version set with ::set_version().
  const char* version_;

  // Executable usage set with ::set_usage().
  const char* usage_;

  // Built-in version option.
  BoolOption builtin_version_;

  // Built-in help option.
  BoolOption builtin_help_;
};

namespace internal {
// Automatically registers itself to the global parser.
template <typename _Option>
class Registrer : public _Option {
 public:
  Registrer(const char* _name, const char* _help,
            typename _Option::Type _default, bool _required,
            typename _Option::ValidateFn _fn = NULL);
  virtual ~Registrer();
};
}  // namespace internal
}  // namespace options
}  // namespace ozz
#endif  // OZZ_OZZ_OPTIONS_OPTIONS_H_
