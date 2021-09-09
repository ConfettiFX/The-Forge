/************************************************************************************

Filename    :   OVR_Uri.h
Content     :   URI parser.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include <stddef.h> // for size_t
#include <stdint.h>

namespace OVRFW {

//==============================================================
// ovrUri
//
// Parses URIs for validity and returns component pieces. The URI is considered valid
// even if it is only a partial URI. It is up to the caller to determine if it has
// enough of a URI to locate a resource. This is because we only want to validate
// structure with this class. This leaves a client of this class able to compose URIs
// from multiple sources and still able to use this class for validation of each
// piece.
//
// If a scheme and host are missing, the URI is parsed as if it were a path only.
// Clients of this class can then decide how to handle the path for a default case.
//
// Paths following a host name: file://windows_machine_name/c:/temp/readme.txt
// will always have the leading path separator at the beginning. In the above case
// the parse will result in:
// scheme: file
// host: windows_machine_name
// path: /c:/temp/readme.txt
//
// Which seems incorrect for the drive letter case. But is correct according to parse
// examples here: https://en.wikipedia.org/wiki/URI_scheme where the path does include
// the first slash after the host section.
// file://windows_machine_name/temp/readme.txt results in:
// scheme: file
// host: windows_machine_name
// path: /temp/readme.txt
// Clients need to take this into account when appending the path to what they consider
// to be the host root. On a Windows machine, the host root is unclear without the drive
// letter specification.
//
// Note that the scheme never includes the trailing colon and the host never includes
// the leading double forward slashes.
// Note spaces in host names are considered invalid.
// Spaces in paths are currently allowed but really these should have to be escaped.
//
// Features:
// - supports UTF-8
// - supports authority (username and password)
// - supports queries
// - supports fragments
//
// TODO: support escaped characters (%20), etc.

class ovrUri {
   public:
    static const int MAX_URI_SIZE = 1024;

    // if any of the out pointers are NULL, that part of the Uri will not be returned
    static bool ParseUri(
        char const* uri,
        char* outScheme,
        size_t const outSchemeSize,
        char* outUsername,
        size_t const outUsernameSize,
        char* outPassword,
        size_t const outPasswordSize,
        char* outHost,
        size_t const outHostSize,
        int& outPort,
        char* outPath,
        size_t const outPathSize,
        char* outQuery,
        size_t outQuerySize,
        char* outFragment,
        size_t outFragmentSize);

    // Parses out just the scheme. This will fail if the URI is not properly formed (i.e. if it's
    // only a scheme without a path.
    static bool ParseScheme(char const* uri, char* outScheme, size_t const outSchemeSize);

    static bool IsValidUri(char const* uri);

    static void DoUnitTest();

    static bool InUnitTest;
};

} // namespace OVRFW
