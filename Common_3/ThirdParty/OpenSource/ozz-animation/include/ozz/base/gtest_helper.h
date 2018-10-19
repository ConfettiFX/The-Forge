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

#ifndef OZZ_OZZ_BASE_GTEST_HELPER_H_
#define OZZ_OZZ_BASE_GTEST_HELPER_H_

#include <sstream>

// EXPECT_ASSERTION expands to real death test if assertions are enabled.
// Parameters:
//   statement -  A statement that a macro such as EXPECT_DEATH would test
//                for program termination.
//   regex     -  A regex that a macro such as EXPECT_DEATH would use to test
//                the output of statement.
#ifdef NDEBUG
// Expands to nothing if asserts aren't enabled (ie: NDEBUG is defined)
#define EXPECT_ASSERTION(_statement, _regex) \
  do {                                       \
  } while (void(0), false);
#else  // NDEBUG
#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
namespace internal {
// Provides a hook during abort to ensure EXIT_FAILURE is returned.
inline int AbortHook(int, char*, int*) { exit(EXIT_FAILURE); }
}  // namespace internal
#define EXPECT_ASSERTION(_statement, _regex)                                   \
  do {                                                                         \
    /* During death tests executions:*/                                        \
    /* Disables popping message boxes during crt and stl assertions*/          \
    int old_mode = 0;                                                          \
    (void)old_mode;                                                            \
    _CRT_REPORT_HOOK old_hook = NULL;                                          \
    (void)old_hook;                                                            \
    if (testing::internal::GTEST_FLAG(internal_run_death_test).length() > 0) { \
      old_mode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);           \
      old_hook = _CrtSetReportHook(&internal::AbortHook);                      \
    }                                                                          \
    EXPECT_DEATH(_statement, _regex);                                          \
    if (testing::internal::GTEST_FLAG(internal_run_death_test).length() > 0) { \
      _CrtSetReportMode(_CRT_ASSERT, old_mode);                                \
      (void)_CrtSetReportHook(old_hook);                                       \
    }                                                                          \
                                                                               \
  } while (void(0), 0)
#else  // _WIN32
#define EXPECT_ASSERTION(_statement, _regex) EXPECT_DEATH(_statement, _regex)
#endif  // _WIN32
#endif  // NDEBUG

// EXPECT_EQ_LOG* executes _expression and compares its result with _eq.
// While executing _expression, EXPECT_EQ_LOG redirects _output (ex:
// std::clog) and then expects that the output matched the regular expression
// _re.
#define EXPECT_EQ_LOG(_expression, _eq, _output, _re)   \
  do {                                                  \
    internal::RedirectOuputTester tester(_output, _re); \
    EXPECT_EQ(_expression, _eq);                        \
                                                        \
  } while (void(0), 0)

// There are multiple declinations EXPECT_EQ_LOG*, to match with clog, cerr and
// cout outputs, and verbose level option.

// Specialises EXPECT_EQ_LOG* for verbose clog output type.
#define EXPECT_EQ_LOG_LOGV(_expression, _eq, _re) \
  EXPECT_EQ_LOG(_expression, _eq, std::clog,      \
                ozz::log::kVerbose <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_EQ_LOG* for standard clog output type.
#define EXPECT_EQ_LOG_LOG(_expression, _eq, _re) \
  EXPECT_EQ_LOG(_expression, _eq, std::clog,     \
                ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_EQ_LOG* for standard cout output type.
#define EXPECT_EQ_LOG_OUT(_expression, _eq, _re) \
  EXPECT_EQ_LOG(_expression, _eq, std::cout,     \
                ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_EQ_LOG* for standard cerr output type.
#define EXPECT_EQ_LOG_ERR(_expression, _eq, _re) \
  EXPECT_EQ_LOG(_expression, _eq, std::cerr,     \
                ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

// EXPECT_EQ_LOG* executes _expression while redirecting _output (ex:
// std::clog) and then expects that the output matched the regular expression
// _re.
#define EXPECT_LOG(_expression, _output, _re)           \
  do {                                                  \
    internal::RedirectOuputTester tester(_output, _re); \
    (_expression);                                      \
                                                        \
  } while (void(0), 0)

// There are multiple declinations EXPECT_LOG*, to match with clog, cerr and
// cout outputs, and verbose level option.

// Specialises EXPECT_LOG* for verbose clog output type.
#define EXPECT_LOG_LOGV(_expression, _re) \
  EXPECT_LOG(_expression, std::clog,      \
             ozz::log::kVerbose <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_LOG* for standard clog output type.
#define EXPECT_LOG_LOG(_expression, _re) \
  EXPECT_LOG(_expression, std::clog,     \
             ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_LOG* for standard cout output type.
#define EXPECT_LOG_OUT(_expression, _re) \
  EXPECT_LOG(_expression, std::cout,     \
             ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

// Specialises EXPECT_LOG* for standard cerr output type.
#define EXPECT_LOG_ERR(_expression, _re) \
  EXPECT_LOG(_expression, std::cerr,     \
             ozz::log::kStandard <= ozz::log::GetLevel() ? _re : NULL)

namespace internal {
class RedirectOuputTester {
 public:
  // Specify a NULL _regex to test an empty output
  RedirectOuputTester(std::ostream& _ostream, const char* _regex)
      : ostream_(_ostream), old_(_ostream.rdbuf()), regex_(_regex) {
    // Redirect ostream_ buffer.
    ostream_.rdbuf(redirect_.rdbuf());
  }
  ~RedirectOuputTester() {
    if (regex_) {
      EXPECT_TRUE(::testing::internal::RE::PartialMatch(redirect_.str().c_str(),
                                                        regex_));
    } else {
      EXPECT_EQ(redirect_.str().size(), 0u);
    }
    // Restore ostream_ buffer.
    ostream_.rdbuf(old_);
    // finally outputs everything temporary redirected.
    ostream_ << redirect_.str();
  }

 private:
  std::ostream& ostream_;
  std::streambuf* old_;
  const char* regex_;
  std::stringstream redirect_;
};
}  // namespace internal
#endif  // OZZ_OZZ_BASE_GTEST_HELPER_H_
