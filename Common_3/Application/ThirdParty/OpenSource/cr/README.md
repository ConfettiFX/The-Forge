# cr.h

A single file header-only live reload solution for C, written in C++:

- simple public API, 3 functions to use only (and another to export);
- works and tested on Linux, MacOSX and Windows;
- automatic crash protection;
- automatic static state transfer;
- based on dynamic reloadable binary (.so/.dylib/.dll);
- support multiple plugins;
- MIT licensed;

### Build Status:

|Platform|Build Status|
|--------|------|
|Linux|[![Build Status](https://travis-ci.org/fungos/cr.svg?branch=master)](https://travis-ci.org/fungos/cr)|
|Windows|[![Build status](https://ci.appveyor.com/api/projects/status/jf0dq97w9b7b5ihi?svg=true)](https://ci.appveyor.com/project/fungos/cr)|

Note that the only file that matters is `cr.h`.

This file contains the documentation in markdown, the license, the implementation and the public api.
All other files in this repository are supporting files and can be safely ignored.

### Example

A (thin) host application executable will make use of `cr` to manage
live-reloading of the real application in the form of dynamic loadable binary, a host would be something like:

```c
#define CR_HOST // required in the host only and before including cr.h
#include "../cr.h"

int main(int argc, char *argv[]) {
    // the host application should initalize a plugin with a context, a plugin
    cr_plugin ctx;

    // the full path to the live-reloadable application
    cr_plugin_open(ctx, "c:/path/to/build/game.dll");

    // call the update function at any frequency matters to you, this will give
    // the real application a chance to run
    while (!cr_plugin_update(ctx)) {
        // do anything you need to do on host side (ie. windowing and input stuff?)
    }

    // at the end do not forget to cleanup the plugin context
    cr_plugin_close(ctx);
    return 0;
}
```

While the guest (real application), would be like:

```c
CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation) {
    assert(ctx);
    switch (operation) {
        case CR_LOAD:   return on_load(...); // loading back from a reload
        case CR_UNLOAD: return on_unload(...); // preparing to a new reload
        case CR_CLOSE: ...; // the plugin will close and not reload anymore
    }
    // CR_STEP
    return on_update(...);
}
```

### Changelog

#### 2020-01-09

- Deprecated `cr_plugin_load` in favor to `cr_plugin_open` for consistency with `cr_plugin_close`. See issue #49.
- Minor documentation improvements.

#### 2018-11-17

- Support to OSX finished, thanks to MESH Consultants Inc.
- Added a new possible failure `CR_BAD_IMAGE` in case the binary file is stil not ready even if its timestamp changed. This could happen if generating the file (compiler or copying) was slow.
- Windows: Fix issue with too long paths causing the PDB patch process to fail, causing the reload process to fail.
- **Possible breaking change:** Fix rollback flow. Before, during a rollback (for any reason) two versions were decremented one-shot so that the in following load, the version would bump again getting us effectively on the previous version, but in some cases not related to crashes this wasn't completely valid (see `CR_BAD_IMAGE`). Now the version is decremented one time in the crash handler and then another time during the rollback and then be bumped again. A rollback due an incomplete image will not incorrectly rollback two versions, it will continue at the same version retrying the load until the image is valid (copy or compiler finished writing to it). This may impact current uses of `cr` if the `version` info is used during `CR_UNLOAD` as it will now be a different value.

### Samples

Two simple samples can be found in the `samples` directory.

The first is one is a simple console application that demonstrate some basic static
states working between instances and basic crash handling tests. Print to output
is used to show what is happening.

The second one demonstrates how to live-reload an opengl application using
 [Dear ImGui](https://github.com/ocornut/imgui). Some state lives in the host
 side while most of the code is in the guest side.

 ![imgui sample](https://i.imgur.com/Nq6s0GP.gif)

#### Running Samples and Tests

The samples and tests uses the [fips build system](https://github.com/floooh/fips). It requires Python and CMake.

```
$ ./fips build            # will generate and build all artifacts
$ ./fips run crTest       # To run tests
$ ./fips run imgui_host   # To run imgui sample
# open a new console, then modify imgui_guest.cpp
$ ./fips make imgui_guest # to build and force imgui sample live reload
```

### Documentation

#### `int (*cr_main)(struct cr_plugin *ctx, enum cr_op operation)`

This is the function pointer to the dynamic loadable binary entry point function.

Arguments

- `ctx` pointer to a context that will be passed from `host` to the `guest` containing valuable information about the current loaded version, failure reason and user data. For more info see `cr_plugin`.
- `operation` which operation is being executed, see `cr_op`.

Return

- A negative value indicating an error, forcing a rollback to happen and failure
 being set to `CR_USER`. 0 or a positive value that will be passed to the
  `host` process.

#### `bool cr_plugin_open(cr_plugin &ctx, const char *fullpath)`

Loads and initialize the plugin.

Arguments

- `ctx` a context that will manage the plugin internal data and user data.
- `fullpath` full path with filename to the loadable binary for the plugin or
 `NULL`.

Return

- `true` in case of success, `false` otherwise.

#### `void cr_set_temporary_path(cr_plugin& ctx, const std::string &path)`

Sets temporary path to which temporary copies of plugin will be placed. Should be called
immediately after `cr_plugin_open()`. If `temporary` path is not set, temporary copies of
the file will be copied to the same directory where the original file is located.

Arguments

- `ctx` a context that will manage the plugin internal data and user data.
- `path` a full path to an existing directory which will be used for storing temporary plugin copies.

#### `int cr_plugin_update(cr_plugin &ctx, bool reloadCheck = true)`

This function will call the plugin `cr_main` function. It should be called as
 frequently as the core logic/application needs.

Arguments

- `ctx` the current plugin context data.
- `reloadCheck` optional: do a disk check (stat()) to see if the dynamic library needs a reload.

Return

- -1 if a failure happened during an update;
- -2 if a failure happened during a load or unload;
- anything else is returned directly from the plugin `cr_main`.

#### `void cr_plugin_close(cr_plugin &ctx)`

Cleanup internal states once the plugin is not required anymore.

Arguments

- `ctx` the current plugin context data.

#### `cr_op`

Enum indicating the kind of step that is being executed by the `host`:

- `CR_LOAD` A load caused by reload is being executed, can be used to restore any
 saved internal state. This does not happen when a plugin is loaded for the first
 time as there is no state to restore;
- `CR_STEP` An application update, this is the normal and most frequent operation;
- `CR_UNLOAD` An unload for reloading the plugin will be executed, giving the 
 application one chance to store any required data;
- `CR_CLOSE` Used when closing the plugin, This works like `CR_UNLOAD` but no `CR_LOAD` 
 should be expected afterwards;

#### `cr_plugin`

The plugin instance context struct.

- `p` opaque pointer for internal cr data;
- `userdata` may be used by the user to pass information between reloads;
- `version` incremetal number for each succeded reload, starting at 1 for the
 first load. **The version will change during a crash handling process**;
- `failure` used by the crash protection system, will hold the last failure error
 code that caused a rollback. See `cr_failure` for more info on possible values;

#### `cr_failure`

If a crash in the loadable binary happens, the crash handler will indicate the
 reason of the crash with one of these:

- `CR_NONE` No error;
- `CR_SEGFAULT` Segmentation fault. `SIGSEGV` on Linux/OSX or
 `EXCEPTION_ACCESS_VIOLATION` on Windows;
- `CR_ILLEGAL` In case of illegal instruction. `SIGILL` on Linux/OSX or
 `EXCEPTION_ILLEGAL_INSTRUCTION` on Windows;
- `CR_ABORT` Abort, `SIGBRT` on Linux/OSX, not used on Windows;
- `CR_MISALIGN` Bus error, `SIGBUS` on Linux/OSX or `EXCEPTION_DATATYPE_MISALIGNMENT`
 on Windows;
- `CR_BOUNDS` Is `EXCEPTION_ARRAY_BOUNDS_EXCEEDED`, Windows only;
- `CR_STACKOVERFLOW` Is `EXCEPTION_STACK_OVERFLOW`, Windows only;
- `CR_STATE_INVALIDATED` Static `CR_STATE` management safety failure;
- `CR_BAD_IMAGE` The plugin is not a valid image (i.e. the compiler may still
writing it);
- `CR_OTHER` Other signal, Linux only;
- `CR_USER` User error (for negative values returned from `cr_main`);

#### `CR_HOST` define

This define should be used before including the `cr.h` in the `host`, if `CR_HOST`
 is not defined, `cr.h` will work as a public API header file to be used in the
  `guest` implementation.

Optionally `CR_HOST` may also be defined to one of the following values as a way
 to configure the `safety` operation mode for automatic static state management
  (`CR_STATE`):

- `CR_SAFEST` Will validate address and size of the state data sections during
 reloads, if anything changes the load will rollback;
- `CR_SAFE` Will validate only the size of the state section, this mean that the
 address of the statics may change (and it is best to avoid holding any pointer
  to static stuff);
- `CR_UNSAFE` Will validate nothing but that the size of section fits, may not
 be necessarelly exact (growing is acceptable but shrinking isn't), this is the
 default behavior;
- `CR_DISABLE` Completely disable automatic static state management;

#### `CR_STATE` macro

Used to tag a global or local static variable to be saved and restored during a reload.

Usage

`static bool CR_STATE bInitialized = false;`

#### Overridable macros

You can define these macros before including cr.h in host (CR_HOST) to customize cr.h
 memory allocations and other behaviours:

- `CR_MAIN_FUNC`: changes 'cr_main' symbol to user-defined function name. default: #define CR_MAIN_FUNC "cr_main"
- `CR_ASSERT`: override assert. default: #define CA_ASSERT(e) assert(e)
- `CR_REALLOC`: override libc's realloc. default: #define CR_REALLOC(ptr, size) ::realloc(ptr, size)
- `CR_MALLOC`: override libc's malloc. default: #define CR_MALLOC(size) ::malloc(size)
- `CR_FREE`: override libc's free. default: #define CR_FREE(ptr) ::free(ptr)
- `CR_DEBUG`: outputs debug messages in CR_ERROR, CR_LOG and CR_TRACE
- `CR_ERROR`: logs debug messages to stderr. default (CR_DEBUG only): #define CR_ERROR(...) fprintf(stderr, __VA_ARGS__)
- `CR_LOG`: logs debug messages. default (CR_DEBUG only): #define CR_LOG(...) fprintf(stdout, __VA_ARGS__)
- `CR_TRACE`: prints function calls. default (CR_DEBUG only): #define CR_TRACE(...) fprintf(stdout, "CR_TRACE: %s\n", __FUNCTION__)

### FAQ / Troubleshooting

#### Q: Why?

A: Read about why I made this [here](https://fungos.github.io/blog/2017/11/20/cr.h-a-simple-c-hot-reload-header-only-library/).

#### Q: My application asserts/crash when freeing heap data allocated inside the dll, what is happening?

A: Make sure both your application host and your dll are using the dynamic
 run-time (/MD or /MDd) as any data allocated in the heap must be freed with
  the same allocator instance, by sharing the run-time between guest and
   host you will guarantee the same allocator is being used.

#### Q: Can we load multiple plugins at the same time?

A: Yes. This should work without issues on Windows. On Linux and OSX there may be
issues with crash handling

#### Q: You said this wouldn't lock my PDB, but it still locks! Why?

If you had to load the dll before `cr` for any reason, Visual Studio may still hold a lock to the PDB. You may be having [this issue](https://github.com/fungos/cr/issues/12) and the solution is [here](https://stackoverflow.com/questions/38427425/how-to-force-visual-studio-2015-to-unlock-pdb-file-after-freelibrary-call).

#### Q: Hot-reload is not working at all, what I'm doing wrong?

First, be sure that your build system is not interfering by somewhat still linking to your shared library. There are so many things that can go wrong and you need to be sure only `cr` will deal with your shared library. On linux, for more info on how to find what is happening, check [this issue](https://github.com/fungos/cr/issues/9).

#### Q: How much can I change things in the plugin without risking breaking everything?

`cr` is `C` reloader and dealing with C it assume simple things will mostly work.

The problem is how the linker will decide do rearrange things accordingly the amount of changes you do in the code. For incremental and localized changes I never had any issues, in general I hardly had any issues at all by writing normal C code. Now, when things start to become more complex and bordering C++, it becomes riskier. If you need do complex things, I suggest checking [RCCPP](https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus) and reading [this PDF](http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter15_Runtime_Compiled_C++_for_Rapid_AI_Development.pdf) and my original blog post about `cr` [here](https://fungos.github.io/blog/2017/11/20/cr.h-a-simple-c-hot-reload-header-only-library/).

With all these information you'll be able to decide which is better to your use case.

### `cr` Sponsors

![MESH](https://static1.squarespace.com/static/5a5f5f08aeb625edacf9327b/t/5a7b78aa8165f513404129a3/1534346581876/?format=150w)

#### [MESH Consultants Inc.](http://meshconsultants.ca/)
**For sponsoring the port of `cr` to the MacOSX.**

### Contributors

[Danny Grein](https://github.com/fungos)

[Rokas Kupstys](https://github.com/rokups)

[Noah Rinehart](https://github.com/noahrinehart)

[Niklas Lundberg](https://github.com/datgame)

[Sepehr Taghdisian](https://github.com/septag)

[Robert Gabriel Jakabosky](https://github.com/neopallium)

[@pixelherodev](https://github.com/pixelherodev)

### Contributing

We welcome *ALL* contributions, there is no minor things to contribute with, even one letter typo fixes are welcome.

The only things we require is to test thoroughly, maintain code style and keeping documentation up-to-date.

Also, accepting and agreeing to release any contribution under the same license.

----

### License

The MIT License (MIT)

Copyright (c) 2017 Danny Angelo Carminati Grein

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

----

### Source

<details>
<summary>View Source Code</summary>

```c
*/
#ifndef __CR_H__
#define __CR_H__

//
// Global OS specific defines/customizations
//
#if defined(_WIN32)
#define CR_WINDOWS
#define CR_PLUGIN(name) "" name ".dll"
#elif defined(__linux__)
#define CR_LINUX
#define CR_PLUGIN(name) "lib" name ".so"
#elif defined(__APPLE__)
#define CR_OSX
#define CR_PLUGIN(name) "lib" name ".dylib"
#else
#error "Unknown/unsupported platform, please open an issue if you think this \
platform should be supported."
#endif // CR_WINDOWS || CR_LINUX || CR_OSX

//
// Global compiler specific defines/customizations
//
#if defined(_MSC_VER)
#if defined(__cplusplus)
#define CR_EXPORT extern "C" __declspec(dllexport)
#define CR_IMPORT extern "C" __declspec(dllimport)
#else
#define CR_EXPORT __declspec(dllexport)
#define CR_IMPORT __declspec(dllimport)
#endif
#endif // defined(_MSC_VER)

#if defined(__GNUC__) // clang & gcc
#if defined(__cplusplus)
#define CR_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define CR_EXPORT __attribute__((visibility("default")))
#endif
#define CR_IMPORT
#endif // defined(__GNUC__)

// cr_mode defines how much we validate global state transfer between
// instances. The default is CR_UNSAFE, you can choose another mode by
// defining CR_HOST, ie.: #define CR_HOST CR_SAFEST
enum cr_mode {
    CR_SAFEST = 0, // validate address and size of the state section, if
                   // anything changes the load will rollback
    CR_SAFE = 1,   // validate only the size of the state section, this means
                   // that address is assumed to be safe if avoided keeping
                   // references to global/static states
    CR_UNSAFE = 2, // don't validate anything but that the size of the section
                   // fits, may not be identical though
    CR_DISABLE = 3 // completely disable the auto state transfer
};

// cr_op is passed into the guest process to indicate the current operation
// happening so the process can manage its internal data if it needs.
enum cr_op {
    CR_LOAD = 0,
    CR_STEP = 1,
    CR_UNLOAD = 2,
    CR_CLOSE = 3,
};

enum cr_failure {
    CR_NONE,     // No error
    CR_SEGFAULT, // SIGSEGV / EXCEPTION_ACCESS_VIOLATION
    CR_ILLEGAL,  // illegal instruction (SIGILL) / EXCEPTION_ILLEGAL_INSTRUCTION
    CR_ABORT,    // abort (SIGBRT)
    CR_MISALIGN, // bus error (SIGBUS) / EXCEPTION_DATATYPE_MISALIGNMENT
    CR_BOUNDS,   // EXCEPTION_ARRAY_BOUNDS_EXCEEDED
    CR_STACKOVERFLOW, // EXCEPTION_STACK_OVERFLOW
    CR_STATE_INVALIDATED, // one or more global data section changed and does
                          // not safely match basically a failure of
                          // cr_plugin_validate_sections
    CR_BAD_IMAGE, // The binary is not valid - compiler is still writing it
    CR_OTHER,    // Unknown or other signal,
    CR_USER = 0x100,
};

struct cr_plugin;

typedef int (*cr_plugin_main_func)(struct cr_plugin *ctx, enum cr_op operation);

// public interface for the plugin context, this has some user facing
// variables that may be used to manage reload feedback.
// - userdata may be used by the user to pass information between reloads
// - version is the reload counter (after loading the first instance it will
//   be 1, not 0)
// - failure is the (platform specific) last error code for any crash that may
//   happen to cause a rollback reload used by the crash protection system
struct cr_plugin {
    void *p;
    void *userdata;
    unsigned int version;
    enum cr_failure failure;
    unsigned int next_version;
    unsigned int last_working_version;
};

#ifndef CR_HOST

// Guest specific compiler defines/customizations
#if defined(_MSC_VER)
#pragma section(".state", read, write)
#define CR_STATE __declspec(allocate(".state"))
#endif // defined(_MSC_VER)

#if defined(CR_OSX)
#define CR_STATE __attribute__((used, section("__DATA,__state")))
#else
#if defined(__GNUC__) // clang & gcc
#define CR_STATE __attribute__((section(".state")))
#endif // defined(__GNUC__)
#endif

#else // #ifndef CR_HOST

// Overridable macros
#ifndef CR_LOG
#   ifdef CR_DEBUG
#       include <stdio.h>
#       define CR_LOG(...)     fprintf(stdout, __VA_ARGS__)
#   else
#       define CR_LOG(...)
#   endif
#endif

#ifndef CR_ERROR
#   ifdef CR_DEBUG
#       include <stdio.h>
#       define CR_ERROR(...)     fprintf(stderr, __VA_ARGS__)
#   else
#       define CR_ERROR(...)
#   endif
#endif

#ifndef CR_TRACE
#   ifdef CR_DEBUG
#       include <stdio.h>
#       define CR_TRACE        fprintf(stdout, "CR_TRACE: %s\n", __FUNCTION__);
#   else
#       define CR_TRACE
#   endif
#endif

#ifndef CR_MAIN_FUNC
#   define CR_MAIN_FUNC "cr_main"
#endif

#ifndef CR_ASSERT
#   include <assert.h>
#   define CR_ASSERT(e)             assert(e)
#endif

#ifndef CR_REALLOC
#   include <stdlib.h>
#   define CR_REALLOC(ptr, size)   ::realloc(ptr, size)
#endif

#ifndef CR_FREE
#   include <stdlib.h>
#   define CR_FREE(ptr)            ::free(ptr)
#endif

#ifndef CR_MALLOC
#   include <stdlib.h>
#   define CR_MALLOC(size)         ::malloc(size)
#endif

#if defined(_MSC_VER)
// we should probably push and pop this
#   pragma warning(disable:4003) // not enough actual parameters for macro 'identifier'
#endif

#define CR_DO_EXPAND(x) x##1337
#define CR_EXPAND(x) CR_DO_EXPAND(x)

#if CR_EXPAND(CR_HOST) == 1337
#define CR_OP_MODE CR_UNSAFE
#else
#define CR_OP_MODE CR_HOST
#endif

#include <algorithm>
#include <chrono>  // duration for sleep
#include <cstring> // memcpy
#include <string>
#include <thread> // this_thread::sleep_for

#if defined(CR_WINDOWS)
#define CR_PATH_SEPARATOR '\\'
#define CR_PATH_SEPARATOR_INVALID '/'
#else
#define CR_PATH_SEPARATOR '/'
#define CR_PATH_SEPARATOR_INVALID '\\'
#endif

static void cr_split_path(std::string path, std::string &parent_dir,
                          std::string &base_name, std::string &ext) {
    std::replace(path.begin(), path.end(), CR_PATH_SEPARATOR_INVALID,
                 CR_PATH_SEPARATOR);
    auto sep_pos = path.rfind(CR_PATH_SEPARATOR);
    auto dot_pos = path.rfind('.');

    if (sep_pos == std::string::npos) {
        parent_dir = "";
        if (dot_pos == std::string::npos) {
            ext = "";
            base_name = path;
        } else {
            ext = path.substr(dot_pos);
            base_name = path.substr(0, dot_pos);
        }
    } else {
        parent_dir = path.substr(0, sep_pos + 1);
        if (dot_pos == std::string::npos || sep_pos > dot_pos) {
            ext = "";
            base_name = path.substr(sep_pos + 1);
        } else {
            ext = path.substr(dot_pos);
            base_name = path.substr(sep_pos + 1, dot_pos - sep_pos - 1);
        }
    }
}

static std::string cr_version_path(const std::string &basepath,
                                   unsigned version,
                                   const std::string &temppath) {
    std::string folder, fname, ext;
    cr_split_path(basepath, folder, fname, ext);
    std::string ver = std::to_string(version);
#if defined(_MSC_VER)
    // When patching PDB file path in library file we will drop path and leave only file name.
    // Length of path is extra space for version number. Trim file name only if version number
    // length exceeds pdb folder path length. This is not relevant on other platforms.
    if (ver.size() > folder.size()) {
        fname = fname.substr(0, fname.size() - (ver.size() - folder.size()));
    }
#endif
    if (!temppath.empty()) {
        folder = temppath;
    }
    return folder + fname + ver + ext;
}

namespace cr_plugin_section_type {
enum e { state, bss, count };
}

namespace cr_plugin_section_version {
enum e { backup, current, count };
}

struct cr_plugin_section {
    cr_plugin_section_type::e type = {};
    intptr_t base = 0;
    char *ptr = 0;
    int64_t size = 0;
    void *data = nullptr;
};

struct cr_plugin_segment {
    char *ptr = 0;
    int64_t size = 0;
};

// keep track of some internal state about the plugin, should not be messed
// with by user
struct cr_internal {
    std::string fullname = {};
    std::string temppath = {};
    time_t timestamp = {};
    void *handle = nullptr;
    cr_plugin_main_func main = nullptr;
    cr_plugin_segment seg = {};
    cr_plugin_section data[cr_plugin_section_type::count]
                          [cr_plugin_section_version::count] = {};
    cr_mode mode = CR_SAFEST;
};

static bool cr_plugin_section_validate(cr_plugin &ctx,
                                       cr_plugin_section_type::e type,
                                       intptr_t vaddr, intptr_t ptr,
                                       int64_t size);
static void cr_plugin_sections_reload(cr_plugin &ctx,
                                      cr_plugin_section_version::e version);
static void cr_plugin_sections_store(cr_plugin &ctx);
static void cr_plugin_sections_backup(cr_plugin &ctx);
static void cr_plugin_reload(cr_plugin &ctx);
static int cr_plugin_unload(cr_plugin &ctx, bool rollback, bool close);
static bool cr_plugin_changed(cr_plugin &ctx);
static bool cr_plugin_rollback(cr_plugin &ctx);
static int cr_plugin_main(cr_plugin &ctx, cr_op operation);

void cr_set_temporary_path(cr_plugin &ctx, const std::string &path) {
    auto pimpl = (cr_internal *)ctx.p;
    pimpl->temppath = path;
}

#if defined(CR_WINDOWS)

// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#if defined(_MSC_VER)
#pragma comment(lib, "dbghelp.lib")
#endif
using so_handle = HMODULE;

#ifdef UNICODE
#   define CR_WINDOWS_ConvertPath(_newpath, _path)     std::wstring _newpath(cr_utf8_to_wstring(_path))

static std::wstring cr_utf8_to_wstring(const std::string &str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, 0, 0);
    wchar_t wpath_small[MAX_PATH];
    std::unique_ptr<wchar_t[]> wpath_big;
    wchar_t *wpath = wpath_small;
    if (wlen > _countof(wpath_small)) {
        wpath_big = std::unique_ptr<wchar_t[]>(new wchar_t[wlen]);
        wpath = wpath_big.get();
    }

    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wpath, wlen) != wlen) {
        return L"";
    }

    return wpath;
}
#else
#   define CR_WINDOWS_ConvertPath(_newpath, _path)     const std::string &_newpath = _path
#endif  // UNICODE

static time_t cr_last_write_time(const std::string &path) {
    CR_WINDOWS_ConvertPath(_path, path);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesEx(_path.c_str(), GetFileExInfoStandard, &fad)) {
        return -1;
    }

    if (fad.nFileSizeHigh == 0 && fad.nFileSizeLow == 0) {
        return -1;
    }

    LARGE_INTEGER time;
    time.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    time.LowPart = fad.ftLastWriteTime.dwLowDateTime;

    return static_cast<time_t>(time.QuadPart / 10000000 - 11644473600LL);
}

static bool cr_exists(const std::string &path) {
    CR_WINDOWS_ConvertPath(_path, path);
    return GetFileAttributes(_path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool cr_copy(const std::string &from, const std::string &to) {
    CR_WINDOWS_ConvertPath(_from, from);
    CR_WINDOWS_ConvertPath(_to, to);
    return CopyFile(_from.c_str(), _to.c_str(), FALSE) ? true : false;
}

static void cr_del(const std::string& path) {
    CR_WINDOWS_ConvertPath(_path, path);
    DeleteFile(_path.c_str());
}

// If using Microsoft Visual C/C++ compiler we need to do some workaround the
// fact that the compiled binary has a fullpath to the PDB hardcoded inside
// it. This causes a lot of headaches when trying compile while debugging as
// the referenced PDB will be locked by the debugger.
// To solve this problem, we patch the binary to rename the PDB to something
// we know will be unique to our in-flight instance, so when debugging it will
// lock this unique PDB and the compiler will be able to overwrite the
// original one.
#if defined(_MSC_VER)
#include <crtdbg.h>
#include <limits.h>
#include <stdio.h>
#include <tchar.h>

static std::string cr_replace_extension(const std::string &filepath,
                                        const std::string &ext) {
    std::string folder, filename, old_ext;
    cr_split_path(filepath, folder, filename, old_ext);
    return folder + filename + ext;
}

template <class T>
static T struct_cast(void *ptr, LONG offset = 0) {
    return reinterpret_cast<T>(reinterpret_cast<intptr_t>(ptr) + offset);
}

// RSDS Debug Information for PDB files
using DebugInfoSignature = DWORD;
#define CR_RSDS_SIGNATURE 'SDSR'
struct cr_rsds_hdr {
    DebugInfoSignature signature;
    GUID guid;
    long version;
    char filename[1];
};

static bool cr_pe_debugdir_rva(PIMAGE_OPTIONAL_HEADER optionalHeader,
                               DWORD &debugDirRva, DWORD &debugDirSize) {
    if (optionalHeader->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto optionalHeader64 =
            struct_cast<PIMAGE_OPTIONAL_HEADER64>(optionalHeader);
        debugDirRva =
            optionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG]
                .VirtualAddress;
        debugDirSize =
            optionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
    } else {
        auto optionalHeader32 =
            struct_cast<PIMAGE_OPTIONAL_HEADER32>(optionalHeader);
        debugDirRva =
            optionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG]
                .VirtualAddress;
        debugDirSize =
            optionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
    }

    if (debugDirRva == 0 && debugDirSize == 0) {
        return true;
    } else if (debugDirRva == 0 || debugDirSize == 0) {
        return false;
    }

    return true;
}

static bool cr_pe_fileoffset_rva(PIMAGE_NT_HEADERS ntHeaders, DWORD rva,
                                 DWORD &fileOffset) {
    bool found = false;
    auto *sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections;
         i++, sectionHeader++) {
        auto sectionSize = sectionHeader->Misc.VirtualSize;
        if ((rva >= sectionHeader->VirtualAddress) &&
            (rva < sectionHeader->VirtualAddress + sectionSize)) {
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    const int diff = static_cast<int>(sectionHeader->VirtualAddress -
                                sectionHeader->PointerToRawData);
    fileOffset = rva - diff;
    return true;
}

static char *cr_pdb_find(LPBYTE imageBase, PIMAGE_DEBUG_DIRECTORY debugDir) {
    CR_ASSERT(debugDir && imageBase);
    LPBYTE debugInfo = imageBase + debugDir->PointerToRawData;
    const auto debugInfoSize = debugDir->SizeOfData;
    if (debugInfo == 0 || debugInfoSize == 0) {
        return nullptr;
    }

    if (IsBadReadPtr(debugInfo, debugInfoSize)) {
        return nullptr;
    }

    if (debugInfoSize < sizeof(DebugInfoSignature)) {
        return nullptr;
    }

    if (debugDir->Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
        auto signature = *(DWORD *)debugInfo;
        if (signature == CR_RSDS_SIGNATURE) {
            auto *info = (cr_rsds_hdr *)(debugInfo);
            if (IsBadReadPtr(debugInfo, sizeof(cr_rsds_hdr))) {
                return nullptr;
            }

            if (IsBadStringPtrA((const char *)info->filename, UINT_MAX)) {
                return nullptr;
            }

            return info->filename;
        }
    }

    return nullptr;
}

static bool cr_pdb_replace(const std::string &filename, const std::string &pdbname,
                           std::string &orig_pdb) {
    CR_WINDOWS_ConvertPath(_filename, filename);

    HANDLE fp = nullptr;
    HANDLE filemap = nullptr;
    LPVOID mem = 0;
    bool result = false;
    do {
        fp = CreateFile(_filename.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if ((fp == INVALID_HANDLE_VALUE) || (fp == nullptr)) {
            break;
        }

        filemap = CreateFileMapping(fp, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        if (filemap == nullptr) {
            break;
        }

        mem = MapViewOfFile(filemap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (mem == nullptr) {
            break;
        }

        auto dosHeader = struct_cast<PIMAGE_DOS_HEADER>(mem);
        if (dosHeader == 0) {
            break;
        }

        if (IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER))) {
            break;
        }

        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            break;
        }

        auto ntHeaders =
            struct_cast<PIMAGE_NT_HEADERS>(dosHeader, dosHeader->e_lfanew);
        if (ntHeaders == 0) {
            break;
        }

        if (IsBadReadPtr(ntHeaders, sizeof(ntHeaders->Signature))) {
            break;
        }

        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            break;
        }

        if (IsBadReadPtr(&ntHeaders->FileHeader, sizeof(IMAGE_FILE_HEADER))) {
            break;
        }

        if (IsBadReadPtr(&ntHeaders->OptionalHeader,
                         ntHeaders->FileHeader.SizeOfOptionalHeader)) {
            break;
        }

        if (ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
            ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            break;
        }

        auto sectionHeaders = IMAGE_FIRST_SECTION(ntHeaders);
        if (IsBadReadPtr(sectionHeaders,
                         ntHeaders->FileHeader.NumberOfSections *
                             sizeof(IMAGE_SECTION_HEADER))) {
            break;
        }

        DWORD debugDirRva = 0;
        DWORD debugDirSize = 0;
        if (!cr_pe_debugdir_rva(&ntHeaders->OptionalHeader, debugDirRva,
                                debugDirSize)) {
            break;
        }

        if (debugDirRva == 0 || debugDirSize == 0) {
            break;
        }

        DWORD debugDirOffset = 0;
        if (!cr_pe_fileoffset_rva(ntHeaders, debugDirRva, debugDirOffset)) {
            break;
        }

        auto debugDir =
            struct_cast<PIMAGE_DEBUG_DIRECTORY>(mem, debugDirOffset);
        if (debugDir == 0) {
            break;
        }

        if (IsBadReadPtr(debugDir, debugDirSize)) {
            break;
        }

        if (debugDirSize < sizeof(IMAGE_DEBUG_DIRECTORY)) {
            break;
        }

        int numEntries = debugDirSize / sizeof(IMAGE_DEBUG_DIRECTORY);
        if (numEntries == 0) {
            break;
        }

        for (int i = 1; i <= numEntries; i++, debugDir++) {
            char *pdb = cr_pdb_find((LPBYTE)mem, debugDir);
            if (pdb) {
                auto len = strlen(pdb);
                if (len >= strlen(pdbname.c_str())) {
                    orig_pdb = pdb;
                    memcpy_s(pdb, len, pdbname.c_str(), pdbname.length());
                    pdb[pdbname.length()] = 0;
                    result = true;
                }
            }
        }
    } while (0);

    if (mem != nullptr) {
        UnmapViewOfFile(mem);
    }

    if (filemap != nullptr) {
        CloseHandle(filemap);
    }

    if ((fp != nullptr) && (fp != INVALID_HANDLE_VALUE)) {
        CloseHandle(fp);
    }

    return result;
}

bool static cr_pdb_process(const std::string &source,
                           const std::string &desination) {
    std::string folder, fname, ext, orig_pdb;
    cr_split_path(desination, folder, fname, ext);
    bool result = cr_pdb_replace(desination, fname + ".pdb", orig_pdb);
    result &= cr_copy(orig_pdb, cr_replace_extension(desination, ".pdb"));
    return result;
}
#endif // _MSC_VER

static void cr_pe_section_save(cr_plugin &ctx, cr_plugin_section_type::e type,
                               int64_t vaddr, int64_t base,
                               IMAGE_SECTION_HEADER &shdr) {
    const auto version = cr_plugin_section_version::current;
    auto p = (cr_internal *)ctx.p;
    auto data = &p->data[type][version];
    const size_t old_size = data->size;
    data->base = base;
    data->ptr = (char *)vaddr;
    data->size = shdr.SizeOfRawData;
    data->data = CR_REALLOC(data->data, shdr.SizeOfRawData);
    if (old_size < shdr.SizeOfRawData) {
        memset((char *)data->data + old_size, '\0',
               shdr.SizeOfRawData - old_size);
    }
}

static bool cr_plugin_validate_sections(cr_plugin &ctx, so_handle handle,
                                        const std::string &imagefile,
                                        bool rollback) {
    (void)imagefile;
    CR_ASSERT(handle);
    auto p = (cr_internal *)ctx.p;
    if (p->mode == CR_DISABLE) {
        return true;
    }
    auto ntHeaders = ImageNtHeader(handle);
    auto base = ntHeaders->OptionalHeader.ImageBase;
    auto sectionHeaders = (IMAGE_SECTION_HEADER *)(ntHeaders + 1);
    bool result = true;
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        auto sectionHeader = sectionHeaders[i];
        const int64_t size = sectionHeader.SizeOfRawData;
        if (!strcmp((const char *)sectionHeader.Name, ".state")) {
            if (ctx.version || rollback) {
                result &= cr_plugin_section_validate(
                    ctx, cr_plugin_section_type::state,
                    base + sectionHeader.VirtualAddress, base, size);
            }
            if (result) {
                auto sec = cr_plugin_section_type::state;
                cr_pe_section_save(ctx, sec,
                                   base + sectionHeader.VirtualAddress, base,
                                   sectionHeader);
            }
        } else if (!strcmp((const char *)sectionHeader.Name, ".bss")) {
            if (ctx.version || rollback) {
                result &= cr_plugin_section_validate(
                    ctx, cr_plugin_section_type::bss,
                    base + sectionHeader.VirtualAddress, base, size);
            }
            if (result) {
                auto sec = cr_plugin_section_type::bss;
                cr_pe_section_save(ctx, sec,
                                   base + sectionHeader.VirtualAddress, base,
                                   sectionHeader);
            }
        }
    }
    return result;
}

static void cr_so_unload(cr_plugin &ctx) {
    auto p = (cr_internal *)ctx.p;
    CR_ASSERT(p->handle);
    FreeLibrary((HMODULE)p->handle);
}

static so_handle cr_so_load(const std::string &filename) {
    CR_WINDOWS_ConvertPath(_filename, filename);
    auto new_dll = LoadLibrary(_filename.c_str());
    if (!new_dll) {
        CR_ERROR("Couldn't load plugin: %d\n", GetLastError());
    }
    return new_dll;
}

static cr_plugin_main_func cr_so_symbol(so_handle handle) {
    CR_ASSERT(handle);
    auto new_main = (cr_plugin_main_func)GetProcAddress(handle, CR_MAIN_FUNC);
    if (!new_main) {
        CR_ERROR("Couldn't find plugin entry point: %d\n",
                GetLastError());
    }
    return new_main;
}

static void cr_plat_init() {
}

static int cr_seh_filter(cr_plugin &ctx, unsigned long seh) {
    if (ctx.version == 1) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ctx.version = ctx.last_working_version;
    switch (seh) {
    case EXCEPTION_ACCESS_VIOLATION:
        ctx.failure = CR_SEGFAULT;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        ctx.failure = CR_ILLEGAL;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        ctx.failure = CR_MISALIGN;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        ctx.failure = CR_BOUNDS;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_STACK_OVERFLOW:
        ctx.failure = CR_STACKOVERFLOW;
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        break;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int cr_plugin_main(cr_plugin &ctx, cr_op operation) {
    auto p = (cr_internal *)ctx.p;
#ifndef __MINGW32__
    __try {
#endif
        if (p->main) {
            return p->main(&ctx, operation);
        }
#ifndef __MINGW32__
    } __except (cr_seh_filter(ctx, GetExceptionCode())) {
        return -1;
    }
#endif
    return -1;
}

#endif // CR_WINDOWS

#if defined(CR_LINUX) || defined(CR_OSX)

#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <unistd.h>

#if defined(CR_LINUX)
#   include <sys/sendfile.h>    // sendfile
#elif defined(CR_OSX)
#   include <copyfile.h>        // copyfile
#endif

using so_handle = void *;

static time_t cr_last_write_time(const std::string &path) {
    struct stat stats;
    if (stat(path.c_str(), &stats) == -1) {
        return -1;
    }

    if (stats.st_size == 0) {
        return -1;
    }

#if defined(CR_OSX)
    return stats.st_mtime;
#else
    return stats.st_mtim.tv_sec;
#endif
}

static bool cr_exists(const std::string &path) {
    struct stat stats {};
    return stat(path.c_str(), &stats) != -1;
}

static bool cr_copy(const std::string &from, const std::string &to) {
#if defined(CR_LINUX)
    // Reference: http://www.informit.com/articles/article.aspx?p=23618&seqNum=13
    int input, output;
    struct stat src_stat;
    if ((input = open(from.c_str(), O_RDONLY)) == -1) {
        return false;
    }
    fstat(input, &src_stat);

    if ((output = open(to.c_str(), O_WRONLY|O_CREAT, O_NOFOLLOW|src_stat.st_mode)) == -1) {
        close(input);
        return false;
    }

    int result = sendfile(output, input, NULL, src_stat.st_size);
    close(input);
    close(output);
    return result > -1;
#elif defined(CR_OSX)
    return copyfile(from.c_str(), to.c_str(), NULL, COPYFILE_ALL|COPYFILE_NOFOLLOW_DST) == 0;
#endif
}

static void cr_del(const std::string& path) {
    unlink(path.c_str());
}

// unix,internal
// a helper function to validate that an area of memory is empty
// this is used to validate that the data in the .bss haven't changed
// and that we are safe to discard it and uses the new one.
bool cr_is_empty(const void *const buf, int64_t len) {
    if (!buf || !len) {
        return true;
    }

    bool r = false;
    auto c = (const char *const)buf;
    for (int i = 0; i < len; ++i) {
        r |= c[i];
    }
    return !r;
}

#if defined(CR_LINUX)
#include <elf.h>
#include <link.h>

static size_t cr_file_size(const std::string &path) {
    struct stat stats;
    if (stat(path.c_str(), &stats) == -1) {
        return 0;
    }
    return static_cast<size_t>(stats.st_size);
}

// unix,internal
// save section information to be used during load/unload when copying
// around global state (from .bss and .state binary sections).
// vaddr = is the in memory loaded address of the segment-section
// base = is the in file section address
// shdr = the in file section header
template <class H>
void cr_elf_section_save(cr_plugin &ctx, cr_plugin_section_type::e type,
                         int64_t vaddr, int64_t base, H shdr) {
    const auto version = cr_plugin_section_version::current;
    auto p = (cr_internal *)ctx.p;
    auto data = &p->data[type][version];
    const size_t old_size = data->size;
    data->base = base;
    data->ptr = (char *)vaddr;
    data->size = shdr.sh_size;
    data->data = CR_REALLOC(data->data, shdr.sh_size);
    if (old_size < shdr.sh_size) {
        memset((char *)data->data + old_size, '\0', shdr.sh_size - old_size);
    }
}

// unix,internal
// validates that the sections being loaded are compatible with the previous
// one accordingly with desired `cr_mode` mode. If this is a first load, a
// validation is not necessary. At the same time it will initialize the
// section tracking information and alloc the required temporary space to use
// during unload.
template <class H>
bool cr_elf_validate_sections(cr_plugin &ctx, bool rollback, H shdr, int shnum,
                              const char *sh_strtab_p) {
    CR_ASSERT(sh_strtab_p);
    auto p = (cr_internal *)ctx.p;
    bool result = true;
    for (int i = 0; i < shnum; ++i) {
        const char *name = sh_strtab_p + shdr[i].sh_name;
        auto sectionHeader = shdr[i];
        const int64_t addr = sectionHeader.sh_addr;
        const int64_t size = sectionHeader.sh_size;
        const int64_t base = (intptr_t)p->seg.ptr + p->seg.size;
        if (!strcmp(name, ".state")) {
            const int64_t vaddr = base - size;
            auto sec = cr_plugin_section_type::state;
            if (ctx.version || rollback) {
                result &=
                    cr_plugin_section_validate(ctx, sec, vaddr, addr, size);
            }
            if (result) {
                cr_elf_section_save(ctx, sec, vaddr, addr, sectionHeader);
            }
        } else if (!strcmp(name, ".bss")) {
            // .bss goes past segment filesz, but it may be just padding
            const int64_t vaddr = base;
            auto sec = cr_plugin_section_type::bss;
            if (ctx.version || rollback) {
                // this is kinda hack to skip bss validation if our data is zero
                // this means we don't care scrapping it, and helps skipping
                // validating a .bss that serves only as padding in the segment.
                if (!cr_is_empty(p->data[sec][0].data, p->data[sec][0].size)) {
                    result &=
                        cr_plugin_section_validate(ctx, sec, vaddr, addr, size);
                }
            }
            if (result) {
                cr_elf_section_save(ctx, sec, vaddr, addr, sectionHeader);
            }
        }
    }
    return result;
}

struct cr_ld_data {
    cr_plugin *ctx = nullptr;
    int64_t data_segment_address = 0;
    int64_t data_segment_size = 0;
    const char *fullname = nullptr;
};

// Iterate over all loaded shared objects and then for each one, iterates
// over each segment.
// So we find our plugin by filename and try to find the segment that
// contains our data sections (.state and .bss) to find their virtual
// addresses.
// We search segments with type PT_LOAD (1), meaning it is a loadable
// segment (anything that really matters ie. .text, .data, .bss, etc...)
// The segment where the p_memsz is bigger than p_filesz is the segment
// that contains the section .bss (if there is one or there is padding).
// Also, the segment will have sensible p_flags value (PF_W for exemple).
//
// Some useful references:
// http://www.skyfree.org/linux/references/ELF_Format.pdf
// https://eli.thegreenplace.net/2011/08/25/load-time-relocation-of-shared-libraries/
static int cr_dl_header_handler(struct dl_phdr_info *info, size_t size,
                                void *data) {
    CR_ASSERT(info && data);
    auto p = (cr_ld_data *)data;
    auto ctx = p->ctx;
    if (strcasecmp(info->dlpi_name, p->fullname)) {
        return 0;
    }

    for (int i = 0; i < info->dlpi_phnum; i++) {
        auto phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        // assume the first writable segment is the one that contains our
        // sections this may not be true I imagine, but if this becomes an
        // issue we fix it by comparing against section addresses, but this
        // will require some rework on the code flow.
        if (phdr.p_flags & PF_W) {
            auto pimpl = (cr_internal *)ctx->p;
            pimpl->seg.ptr = (char *)(info->dlpi_addr + phdr.p_vaddr);
            pimpl->seg.size = phdr.p_filesz;
            break;
        }
    }
    return 0;
}

static bool cr_plugin_validate_sections(cr_plugin &ctx, so_handle handle,
                                        const std::string &imagefile,
                                        bool rollback) {
    CR_ASSERT(handle);
    cr_ld_data data;
    data.ctx = &ctx;
    auto pimpl = (cr_internal *)ctx.p;
    if (pimpl->mode == CR_DISABLE) {
        return true;
    }
    data.fullname = imagefile.c_str();
    dl_iterate_phdr(cr_dl_header_handler, (void *)&data);

    const auto len = cr_file_size(imagefile);
    char *p = nullptr;
    bool result = false;
    do {
        int fd = open(imagefile.c_str(), O_RDONLY);
        p = (char *)mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        auto ehdr32 = (Elf32_Ehdr *)p;
        if (ehdr32->e_ident[EI_MAG0] != ELFMAG0 ||
            ehdr32->e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr32->e_ident[EI_MAG2] != ELFMAG2 ||
            ehdr32->e_ident[EI_MAG3] != ELFMAG3) {
            break;
        }

        if (ehdr32->e_ident[EI_CLASS] == ELFCLASS32) {
            auto shdr = (Elf32_Shdr *)(p + ehdr32->e_shoff);
            auto sh_strtab = &shdr[ehdr32->e_shstrndx];
            const char *const sh_strtab_p = p + sh_strtab->sh_offset;
            result = cr_elf_validate_sections(ctx, rollback, shdr,
                                              ehdr32->e_shnum, sh_strtab_p);
        } else {
            auto ehdr64 = (Elf64_Ehdr *)p;
            auto shdr = (Elf64_Shdr *)(p + ehdr64->e_shoff);
            auto sh_strtab = &shdr[ehdr64->e_shstrndx];
            const char *const sh_strtab_p = p + sh_strtab->sh_offset;
            result = cr_elf_validate_sections(ctx, rollback, shdr,
                                              ehdr64->e_shnum, sh_strtab_p);
        }
    } while (0);

    if (p) {
        munmap(p, len);
    }

    if (!result) {
        ctx.failure = CR_STATE_INVALIDATED;
    }

    return result;
}

#elif defined(CR_OSX)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <stdlib.h>     // realpath
#include <limits.h>     // PATH_MAX

#if __LP64__
typedef struct mach_header_64 macho_hdr;
#define CR_MH_MAGIC MH_MAGIC_64
#else
typedef struct mach_header macho_hdr;
#define CR_MH_MAGIC MH_MAGIC
#endif

// osx,internal
// save section information to be used during load/unload when copying
// around global state (from .bss and .state binary sections).
// vaddr = is the in memory loaded address of the segment-section
void cr_macho_section_save(cr_plugin &ctx, cr_plugin_section_type::e type,
                           intptr_t addr, size_t size) {
    const auto version = cr_plugin_section_version::current;
    auto p = (cr_internal *)ctx.p;
    auto data = &p->data[type][version];
    const size_t old_size = data->size;
    data->base = 0;
    data->ptr = (char *)addr;
    data->size = size;
    data->data = CR_REALLOC(data->data, size);
    if (old_size < size) {
        memset((char *)data->data + old_size, '\0', size - old_size);
    }
}

// Iterate over all loaded shared objects and then for each one to find
// our plugin by filename. Then knowing its image index we can get our
// data sections (__state and __bss) and calculate their virtual
// addresses.
//
// Some useful references:
// man 3 dyld
static bool cr_plugin_validate_sections(cr_plugin &ctx, so_handle handle,
                                        const std::string &imagefile,
                                        bool rollback) {
    bool result = true;
    auto pimpl = (cr_internal *)ctx.p;
    if (pimpl->mode == CR_DISABLE) {
        return result;
    }
    CR_TRACE

    // resolve absolute path of the image, because _dyld_get_image_name returns abs path
    char imageAbsPath[PATH_MAX+1];
    if (!::realpath(imagefile.c_str(), imageAbsPath)) {
        CR_ASSERT(0 && "resolving absolute path for plugin failed");
        return false;
    }

    const int count = (int)_dyld_image_count();
    for (int i = 0; i < count; i++) {
        const char *name = _dyld_get_image_name(i);

        if (strcasecmp(name, imageAbsPath)) {
            // match loaded image filename
            continue;
        }

        const auto hdr = _dyld_get_image_header(i);
        if (hdr->filetype != MH_DYLIB) {
            // assure it is a valid dylib
            continue;
        }

        intptr_t vaddr = _dyld_get_image_vmaddr_slide(i);
        (void)vaddr;
        //auto cmd_stride = sizeof(struct mach_header);
        if (hdr->magic != CR_MH_MAGIC) {
            // check for conforming mach-o header
            continue;
        }

        auto validate_and_save = [&](cr_plugin_section_type::e sec,
            intptr_t addr, unsigned long size) {
            if (addr != 0 && size != 0) {
                if (ctx.version || rollback) {
                    result &= cr_plugin_section_validate(ctx, sec, addr, 0, size);
                }
                if (result) {
                    cr_macho_section_save(ctx, sec, addr, size);
                }
            }
        };

        auto mhdr = (macho_hdr *)hdr;
        unsigned long size = 0;
        auto ptr = (intptr_t)getsectiondata(mhdr, SEG_DATA, "__bss", &size);
        validate_and_save(cr_plugin_section_type::bss, ptr, (size_t)size);
        if (result) {
            ptr = (intptr_t)getsectiondata(mhdr, SEG_DATA, "__state", &size);
            validate_and_save(cr_plugin_section_type::state, ptr, (size_t)size);
        }
        break;
    }

    return result;
}

#endif

static void cr_so_unload(cr_plugin &ctx) {
    CR_ASSERT(ctx.p);
    auto p = (cr_internal *)ctx.p;
    CR_ASSERT(p->handle);

    const int r = dlclose(p->handle);
    if (r) {
        CR_ERROR("Error closing plugin: %d\n", r);
    }

    p->handle = nullptr;
    p->main = nullptr;
}

static so_handle cr_so_load(const std::string &new_file) {
    dlerror();
    auto new_dll = dlopen(new_file.c_str(), RTLD_NOW);
    if (!new_dll) {
        CR_ERROR("Couldn't load plugin: %s\n", dlerror());
    }
    return new_dll;
}

static cr_plugin_main_func cr_so_symbol(so_handle handle) {
    CR_ASSERT(handle);
    dlerror();
    auto new_main = (cr_plugin_main_func)dlsym(handle, CR_MAIN_FUNC);
    if (!new_main) {
        CR_ERROR("Couldn't find plugin entry point: %s\n", dlerror());
    }
    return new_main;
}

sigjmp_buf env;

static void cr_signal_handler(int sig, siginfo_t *si, void *uap) {
    CR_TRACE
    (void)uap;
    CR_ASSERT(si);
    siglongjmp(env, sig);
}

static void cr_plat_init() {
    CR_TRACE
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = cr_signal_handler;
#if defined(CR_LINUX)
    sa.sa_restorer = nullptr;
#endif

    if (sigaction(SIGILL, &sa, nullptr) == -1) {
        CR_ERROR("Failed to setup SIGILL handler\n");
    }
    if (sigaction(SIGBUS, &sa, nullptr) == -1) {
        CR_ERROR("Failed to setup SIGBUS handler\n");
    }
    if (sigaction(SIGSEGV, &sa, nullptr) == -1) {
        CR_ERROR("Failed to setup SIGSEGV handler\n");
    }
    if (sigaction(SIGABRT, &sa, nullptr) == -1) {
        CR_ERROR("Failed to setup SIGABRT handler\n");
    }
}

static cr_failure cr_signal_to_failure(int sig) {
    switch (sig) {
    case 0:
        return CR_NONE;
    case SIGILL:
        return CR_ILLEGAL;
    case SIGBUS:
        return CR_MISALIGN;
    case SIGSEGV:
        return CR_SEGFAULT;
    case SIGABRT:
        return CR_ABORT;
    }
    return static_cast<cr_failure>(CR_OTHER + sig);
}

static int cr_plugin_main(cr_plugin &ctx, cr_op operation) {
    if (int sig = sigsetjmp(env, 1)) {
        ctx.version = ctx.last_working_version;
        ctx.failure = cr_signal_to_failure(sig);
        CR_LOG("1 FAILURE: %d (CR: %d)\n", sig, ctx.failure);
        return -1;
    } else {
        auto p = (cr_internal *)ctx.p;
        CR_ASSERT(p);
        if (p->main) {
            return p->main(&ctx, operation);
        }
    }

    return -1;
}

#endif // CR_LINUX || CR_OSX

static bool cr_plugin_load_internal(cr_plugin &ctx, bool rollback) {
    CR_TRACE
    auto p = (cr_internal *)ctx.p;
    const auto file = p->fullname;
    if (cr_exists(file) || rollback) {
        const auto old_file = cr_version_path(file, ctx.version, p->temppath);
        CR_LOG("unload '%s' with rollback: %d\n", old_file.c_str(), rollback);
        int r = cr_plugin_unload(ctx, rollback, false);
        if (r < 0) {
            return false;
        }

        auto new_version = rollback ? ctx.version : ctx.next_version;
        auto new_file = cr_version_path(file, new_version, p->temppath);
        if (rollback) {
            // Don't rollback to this version again, if it crashes.
            ctx.last_working_version = ctx.version > 0 ? ctx.version - 1 : 0;
        } else {
            // Save current version for rollback.
            ctx.last_working_version = ctx.version;
            cr_copy(file, new_file);

            // Update `next_version` for use by the next reload.
            ctx.next_version = new_version + 1;

#if defined(_MSC_VER)
            if (!cr_pdb_process(file, new_file)) {
                CR_ERROR("Couldn't process PDB, debugging may be "
                         "affected and/or reload may fail\n");
            }
#endif // defined(_MSC_VER)
        }

        auto new_dll = cr_so_load(new_file);
        if (!new_dll) {
            ctx.failure = CR_BAD_IMAGE;
            return false;
        }

        if (!cr_plugin_validate_sections(ctx, new_dll, new_file, rollback)) {
            return false;
        }

        if (rollback) {
            cr_plugin_sections_reload(ctx, cr_plugin_section_version::backup);
        } else if (ctx.version) {
            cr_plugin_sections_reload(ctx, cr_plugin_section_version::current);
        }

        auto new_main = cr_so_symbol(new_dll);
        if (!new_main) {
            return false;
        }

        auto p2 = (cr_internal *)ctx.p;
        p2->handle = new_dll;
        p2->main = new_main;
        if (ctx.failure != CR_BAD_IMAGE) {
            p2->timestamp = cr_last_write_time(file);
        }
        ctx.version = new_version;
        CR_LOG("loaded: %s (version: %d)\n", new_file.c_str(), ctx.version);
    } else {
        CR_ERROR("Error loading plugin.\n");
        return false;
    }
    return true;
}

static bool cr_plugin_section_validate(cr_plugin &ctx,
                                       cr_plugin_section_type::e type,
                                       intptr_t ptr, intptr_t base,
                                       int64_t size) {
    CR_TRACE
    (void)ptr;
    auto p = (cr_internal *)ctx.p;
    switch (p->mode) {
    case CR_SAFE:
        return (p->data[type][0].size == size);
    case CR_UNSAFE:
        return (p->data[type][0].size <= size);
    case CR_DISABLE:
        return true;
    default:
        break;
    }
    // CR_SAFEST
    return (p->data[type][0].base == base && p->data[type][0].size == size);
}

// internal
static void cr_plugin_sections_backup(cr_plugin &ctx) {
    auto p = (cr_internal *)ctx.p;
    if (p->mode == CR_DISABLE) {
        return;
    }
    CR_TRACE

    for (int i = 0; i < cr_plugin_section_type::count; ++i) {
        auto cur = &p->data[i][cr_plugin_section_version::current];
        if (cur->ptr) {
            auto bkp = &p->data[i][cr_plugin_section_version::backup];
            bkp->data = CR_REALLOC(bkp->data, cur->size);
            bkp->ptr = cur->ptr;
            bkp->size = cur->size;
            bkp->base = cur->base;

            if (bkp->data) {
                std::memcpy(bkp->data, cur->data, bkp->size);
            }
        }
    }
}

// internal
// Before unloading iterate over possible global static state and keeps an
// internal copy to be used in next version load and a backup copy as a known
// valid state checkpoint. This is mostly due that a new load may want to
// modify the state and if anything bad happens we are sure to have a valid
// and compatible copy of the state for the previous version of the plugin.
static void cr_plugin_sections_store(cr_plugin &ctx) {
    auto p = (cr_internal *)ctx.p;
    if (p->mode == CR_DISABLE) {
        return;
    }
    CR_TRACE

    auto version = cr_plugin_section_version::current;
    for (int i = 0; i < cr_plugin_section_type::count; ++i) {
        if (p->data[i][version].ptr && p->data[i][version].data) {
            const char *ptr = p->data[i][version].ptr;
            const int64_t len = p->data[i][version].size;
            std::memcpy(p->data[i][version].data, ptr, len);
        }
    }

    cr_plugin_sections_backup(ctx);
}

// internal
// After a load happens reload the global state from previous version from our
// internal copy created during the unload step.
static void cr_plugin_sections_reload(cr_plugin &ctx,
                                      cr_plugin_section_version::e version) {
    CR_ASSERT(version < cr_plugin_section_version::count);
    auto p = (cr_internal *)ctx.p;
    if (p->mode == CR_DISABLE) {
        return;
    }
    CR_TRACE

    for (int i = 0; i < cr_plugin_section_type::count; ++i) {
        if (p->data[i][version].data) {
            const int64_t len = p->data[i][version].size;
            // restore backup into the current section address as it may
            // change due aslr and backup address may be invalid
            const auto current = cr_plugin_section_version::current;
            auto dest = (void *)p->data[i][current].ptr;
            if (dest) {
                std::memcpy(dest, p->data[i][version].data, len);
            }
        }
    }
}

// internal
// Cleanup and frees any temporary memory used to keep global static data
// between sessions, used during shutdown.
static void cr_so_sections_free(cr_plugin &ctx) {
    CR_TRACE
    auto p = (cr_internal *)ctx.p;
    for (int i = 0; i < cr_plugin_section_type::count; ++i) {
        for (int v = 0; v < cr_plugin_section_version::count; ++v) {
            if (p->data[i][v].data) {
                CR_FREE(p->data[i][v].data);
            }
            p->data[i][v].data = nullptr;
        }
    }
}

static bool cr_plugin_changed(cr_plugin &ctx) {
    auto p = (cr_internal *)ctx.p;
    const auto src = cr_last_write_time(p->fullname);
    const auto cur = p->timestamp;
    return src > cur;
}

// internal
// Unload current running plugin, if it is not a rollback it will trigger a
// last update with `cr_op::CR_UNLOAD` (that may crash and cause another
// rollback, etc.) storing global static states to use with next load. If the
// unload is due a rollback, no `cr_op::CR_UNLOAD` is called neither any state
// is saved, giving opportunity to the previous version to continue with valid
// previous state.
static int cr_plugin_unload(cr_plugin &ctx, bool rollback, bool close) {
    CR_TRACE
    auto p = (cr_internal *)ctx.p;
    int r = 0;
    if (p->handle) {
        if (!rollback) {
            r = cr_plugin_main(ctx, close ? CR_CLOSE : CR_UNLOAD);
            // Don't store state if unload crashed.  Rollback will use backup.
            if (r < 0) {
                CR_LOG("4 FAILURE: %d\n", r);
            } else {
                cr_plugin_sections_store(ctx);
            }
        }
        cr_so_unload(ctx);
        p->handle = nullptr;
        p->main = nullptr;
    }
    return r;
}

// internal
// Force a version rollback, causing a partial-unload and a load with the
// previous version, also triggering an update with `cr_op::CR_LOAD` that
// in turn may also cause more rollbacks.
static bool cr_plugin_rollback(cr_plugin &ctx) {
    CR_TRACE
    auto loaded = cr_plugin_load_internal(ctx, true);
    if (loaded) {
        loaded = cr_plugin_main(ctx, CR_LOAD) >= 0;
        if (loaded) {
            ctx.failure = CR_NONE;
        }
    }
    return loaded;
}

// internal
// Checks if a rollback or a reload is needed, do the unload/loading and call
// update one time with `cr_op::CR_LOAD`. Note that this may fail due to crash
// handling during this first update, effectivelly rollbacking if possible and
// causing a consecutive `CR_LOAD` with the previous version.
static void cr_plugin_reload(cr_plugin &ctx) {
    if (cr_plugin_changed(ctx)) {
        CR_TRACE
        if (!cr_plugin_load_internal(ctx, false)) {
            return;
        }
        int r = cr_plugin_main(ctx, CR_LOAD);
        if (r < 0 && !ctx.failure) {
            CR_LOG("2 FAILURE: %d\n", r);
            ctx.failure = CR_USER;
        }
    }
}

// This is basically the plugin `main` function, should be called as
// frequently as your core logic/application needs. -1 and -2 are the only
// possible return values from cr meaning a fatal error (causes rollback),
// other return values are returned directly from `cr_main`.
extern "C" int cr_plugin_update(cr_plugin &ctx, bool reloadCheck = true) {
    if (ctx.failure) {
        CR_LOG("1 ROLLBACK version was %d\n", ctx.version);
        cr_plugin_rollback(ctx);
        CR_LOG("1 ROLLBACK version is now %d\n", ctx.version);
    } else {
        if (reloadCheck) {
            cr_plugin_reload(ctx);
        }
    }

    // -2 to differentiate from crash handling code path, meaning the crash
    // happened probably during load or unload and not update
    if (ctx.failure) {
        CR_LOG("3 FAILURE: -2\n");
        return -2;
    }

    int r = cr_plugin_main(ctx, CR_STEP);
    if (r < 0 && !ctx.failure) {
        CR_LOG("4 FAILURE: CR_USER\n");
        ctx.failure = CR_USER;
    }
    return r;
}

// Loads a plugin from the specified full path (or current directory if NULL).
extern "C" bool cr_plugin_open(cr_plugin &ctx, const char *fullpath) {
    CR_TRACE
    CR_ASSERT(fullpath);
    if (!cr_exists(fullpath)) {
        return false;
    }
    auto p = new(CR_MALLOC(sizeof(cr_internal))) cr_internal;
    p->mode = CR_OP_MODE;
    p->fullname = fullpath;
    ctx.p = p;
    ctx.next_version = 1;
    ctx.last_working_version = 0;
    ctx.version = 0;
    ctx.failure = CR_NONE;
    cr_plat_init();
    return true;
}

// 20200109 [DEPRECATED] Use `cr_plugin_open` instead.
extern "C" bool cr_plugin_load(cr_plugin &ctx, const char *fullpath) {
    return cr_plugin_open(ctx, fullpath);
}

// Call to cleanup internal state once the plugin is not required anymore.
extern "C" void cr_plugin_close(cr_plugin &ctx) {
    CR_TRACE
    const bool rollback = false;
    const bool close = true;
    cr_plugin_unload(ctx, rollback, close);
    cr_so_sections_free(ctx);
    auto p = (cr_internal *)ctx.p;

    // delete backups
    const auto file = p->fullname;
    for (unsigned int i = 0; i < ctx.version; i++) {
        cr_del(cr_version_path(file, i, p->temppath));
#if defined(_MSC_VER)
        cr_del(cr_replace_extension(cr_version_path(file, i, p->temppath), ".pdb"));
#endif
    }

    p->~cr_internal();
    CR_FREE(p);
    ctx.p = nullptr;
    ctx.version = 0;
}

#endif // #ifndef CR_HOST

#endif // __CR_H__
// clang-format off
/*
```

</details>
