/************************************************************************************

Filename    :   OVR_Uri.cpp
Content     :   URI parser.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "OVR_Uri.h"
#include <cctype>
#include "Misc/Log.h"
#include "OVR_UTF8Util.h"
#include <assert.h>

// If enabled, allow URI's missing a scheme to be considered valid. This is valuable
// if we want to allow for old paths to work by defaulting the scheme.
#define TOLERATE_MISSING_SCHEME
// If enabled, allow URI's that have a host but no path to be considered valid.
// #define TOLERATE_HOST_ONLY

#if defined(OVR_BUILD_DEBUG)
#define URI_LOG(_fmt_, ...) ALOG(_fmt_, __VA_ARGS__)
#else
#define URI_LOG(_fmt_, ...)       \
    if (!InUnitTest) {            \
        ALOG(_fmt_, __VA_ARGS__); \
    }
#endif

namespace OVRFW {

bool ovrUri::InUnitTest = false;

//==============================================================
// UTF8Decoder
// allows decoding of a UTF string, peeking ahead without changing the current decode
// location and "backing up" up to two code points if we want to decode characters again.
class UTF8Decoder {
   public:
    explicit UTF8Decoder(char const* source)
        : Source(source), Cur(source), Prev(NULL), PrevPrev(NULL) {}

    explicit UTF8Decoder(UTF8Decoder const& other)
        : Source(other.Source), Cur(other.Cur), Prev(other.Prev), PrevPrev(other.PrevPrev) {}

    UTF8Decoder& operator=(UTF8Decoder const& rhs) {
        if (this != &rhs) {
            Source = rhs.Source;
            Cur = rhs.Cur;
            Prev = rhs.Prev;
            PrevPrev = rhs.PrevPrev;
        }
        return *this;
    }

    uint32_t DecodeNext() {
        PrevPrev = Prev;
        Prev = Cur;
        return UTF8Util::DecodeNextChar(&Cur);
    }

    uint32_t PeekNext() {
        char const* temp = Cur;
        return UTF8Util::DecodeNextChar(&temp);
    }

    uint32_t PeekNextNext() {
        char const* temp = Cur;
        UTF8Util::DecodeNextChar(&temp);
        return UTF8Util::DecodeNextChar(&temp);
    }

    void Backup() {
        if (Prev == NULL) {
            assert(Prev != NULL);
            return;
        }
        Cur = Prev;
        Prev = PrevPrev;
        PrevPrev = NULL;
    }

    char const* GetCur() const {
        return Cur;
    }
    void Reset() {
        Cur = Source;
        Prev = NULL;
        PrevPrev = NULL;
    }

   private:
    char const* Source; // the string we're decoding
    char const* Cur; // the current decode point
    char const* Prev; // the previous decode point
    char const* PrevPrev; // the previos, previous decode point
};

//==============================
// ovrUri::ParseScheme
bool ovrUri::ParseScheme(char const* uri, char* outScheme, size_t const outSchemeSize) {
    int port = 0;
    return ParseUri(
        uri, outScheme, outSchemeSize, NULL, 0, NULL, 0, NULL, 0, port, NULL, 0, NULL, 0, NULL, 0);
}

//==============================
// EncodeCharToBuffer
// only returns false if the buffer overflows -- NULL out pointers are just skipped
static bool
EncodeCharToBuffer(uint32_t const ch, char* out, size_t const outSize, ptrdiff_t& outOffset) {
    if (out == NULL) {
        // if the output buffer is null, just skip
        return true;
    }

    assert(outSize > 1);

    // test to see if the character encoding will overflow the buffer
    char encodeSizeBuff[4];
    ptrdiff_t encodeSize = 0;
    UTF8Util::EncodeChar(encodeSizeBuff, &encodeSize, ch);
    if (static_cast<size_t>(outOffset + encodeSize) >= outSize) {
        // output buffer is full
        out[outOffset] = '\0';
        return false;
    }

    // just re-encode into the real buffer
    UTF8Util::EncodeChar(out, &outOffset, ch);
    return true;
}

static bool IsLegalHostCharacter(uint32_t const ch) {
    return ch != ' ';
}

//==============================
// ovrUri::ParseUri
bool ovrUri::ParseUri(
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
    size_t const outQuerySize,
    char* outFragment,
    size_t const outFragmentSize) {
    // verify all parameters are either NULL or large enough to hold a meaningful path
    if (outScheme != NULL) {
        if (outSchemeSize < 2) {
            assert(outSchemeSize > 1);
            return false;
        }
        outScheme[0] = '\0';
    }
    if (outHost != NULL) {
        if (outHostSize < 2) {
            assert(outHostSize > 1);
            return false;
        }
        outHost[0] = '\0';
    }
    if (outUsername != NULL) {
        if (outUsernameSize < 2) {
            assert(outUsernameSize > 1);
            return false;
        }
        outUsername[0] = '\0';
    }
    if (outPassword != NULL) {
        if (outPasswordSize < 2) {
            assert(outPasswordSize > 1);
            return false;
        }
        outPassword[0] = '\0';
    }
    if (outPath != NULL) {
        if (outPathSize < 2) {
            assert(outPathSize > 1);
            return false;
        }
        outPath[0] = '\0';
    }
    if (outQuery != NULL) {
        if (outQuerySize < 2) {
            assert(outQuerySize > 1);
            return false;
        }
        outQuery[0] = '\0';
    }
    if (outFragment != NULL) {
        if (outFragmentSize < 2) {
            assert(outFragmentSize > 1);
            return false;
        }
        outFragment[0] = '\0';
    }
    outPort = 0;

    // encode UTF8 to outputs as we go
    UTF8Decoder decoder(uri);
    uint32_t ch = 0;

    // Parse the scheme. The scheme is everything up to the first colon
    {
        ptrdiff_t schemeOffset = 0;

        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == '\0') {
                // if we get here then we didn't have a colon and there's no scheme
                if (outScheme != NULL) {
                    outScheme[0] = '\0';
                }
#if defined(TOLERATE_MISSING_SCHEME) // tolerant to missing scheme, but not clear if that's
                                     // desirable
                // try parsing as a host
                decoder.Reset();
                break;
#else // error on scheme only
                LOG("Uri '%s': scheme only!", uri);
                return false;
#endif
            } else if (ch == ':') {
                // 0-terminate
                if (outScheme != NULL) {
                    outScheme[schemeOffset] = '\0';
                    // scheme must at least 1 character in length and start with a letter
                    if (schemeOffset < 1) {
                        // assert( schemeOffset >= 1 );
                        ALOG("Uri '%s' has an empty scheme!", uri);
                        return false;
                    }
                }
                // a valid scheme must be followed by //
                if (decoder.PeekNext() != '/' || decoder.PeekNextNext() != '/') {
                    // assert( false );
                    ALOG(
                        "Uri '%s': valid scheme must be followed by a host separator ( '//' ).",
                        uri);
                    return false;
                }
                // done with scheme
                break;
            }

            if (schemeOffset == 0) {
                if (ch == '/') {
#if defined(TOLERATE_MISSING_SCHEME) // tolerant to missing scheme, but not clear if that's
                                     // desirable
                    // this may be the start of a path or host
                    // go back to the start of the string and continue parsing as if it were a host
                    decoder.Reset();
                    break;
#else // error on missing scheme
                    LOG("Uri '%s': missing scheme", uri);
                    return false;
#endif
                } else if (!isalpha(ch)) {
                    // assert( schemeOffset > 0 || isalpha( ch ) );
                    ALOG("Uri '%s': scheme does not start with a letter.", uri);
                    if (outScheme != NULL) {
                        outScheme[0] = '\0';
                    }
                    return false;
                }
            }

            if (!EncodeCharToBuffer(ch, outScheme, outSchemeSize, schemeOffset)) {
                ALOG("Uri '%s': scheme buffer overflow!", uri);
                return false;
            }
        }
    }

    // the scheme must be followed by // if there is a host
    if (decoder.PeekNext() == '/' && decoder.PeekNextNext() == '/') {
        // skip the two forward slashes
        decoder.DecodeNext();
        decoder.DecodeNext();

        UTF8Decoder hostStartDecoder(decoder);

        bool parsedUsername = false;
        bool usernameOverflowed = false;
        bool passwordOverflowed = false;

        ptrdiff_t usernameOffset = 0;
        ptrdiff_t passwordOffset = 0;

        // scan for username / password followed by @
        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == '\0' || ch == '/') {
                // we reached the end of the host section or the uri without finding a username /
                // password reset to the start of the host and continue trying to parse a host
                decoder = hostStartDecoder;
                if (outUsername != NULL) {
                    outUsername[0] = '\0';
                }
                if (outPassword != NULL) {
                    outPassword[0] = '\0';
                }
                break;
            }
            if (ch == ':') {
                if (parsedUsername) {
                    ALOG("Uri '%s': Malformed authority!", uri);
                    return false;
                }
                // start parsing the password now
                parsedUsername = true;
                if (outUsername != NULL) {
                    outUsername[usernameOffset] = '\0';
                }
                continue;
            } else if (ch == '@') {
                // continue to parse the host
                if (usernameOverflowed) {
                    ALOG("Uri '%s': username buffer overflow!", uri);
                }
                if (passwordOverflowed) {
                    ALOG("Uri '%s': password buffer overflow!", uri);
                }
                if (outPassword != NULL) {
                    outPassword[passwordOffset] = '\0';
                }
                break;
            }

            if (!parsedUsername) {
                if (!usernameOverflowed &&
                    !EncodeCharToBuffer(ch, outUsername, outUsernameSize, usernameOffset)) {
                    // don't consider this an error until we're sure that we're parsing a username
                    // and not a host
                    usernameOverflowed = true;
                    break;
                }
            } else {
                if (!passwordOverflowed &&
                    !EncodeCharToBuffer(ch, outPassword, outPasswordSize, passwordOffset)) {
                    // don't consider this an error until we're sure that we're parsing a password
                    // and not a host
                    passwordOverflowed = true;
                    break;
                }
            }
        }

        // There is a host so parse the host. The host is everything up to the next forward slash (
        // or colon if there is a port ) The host does not include the //
        ptrdiff_t hostOffset = 0;

        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == ':') {
                // parse out the port
                ptrdiff_t portOffset = 0;
                char portString[128];
                portString[0] = '\0';
                for (;;) {
                    ch = decoder.DecodeNext();
                    if (ch == '/' || ch == '\0') {
                        // got to the end if the Uri or the start of the path
                        portString[portOffset] = '\0';
                        outPort = atoi(portString);
                        if (ch == '\0') {
                            // finished parsing the Uri
                            if (outPath != NULL) {
                                // assert( outPath != NULL && ch != '\0' );
                                ALOG(
                                    "Uri '%s': found host:port without path when a path was expected!",
                                    uri);
                                return false;
                            } else {
                                ALOG("Uri '%s': parsed host:port without a path!", uri);
                            }
                            return true;
                        }
                        // continue parsing
                        break;
                    } else if (!isdigit(ch)) {
                        ALOG("Uri '%s' has a non-numeric port!", uri);
                        // assert( isdigit( ch ) );
                        return false;
                    }

                    if (!EncodeCharToBuffer(ch, &portString[0], sizeof(portString), portOffset)) {
                        ALOG("Uri '%s': port buffer overflow!", uri);
                        return false;
                    }
                }
            }

            // check for the end of the host or the end of the string -- we may have just parsed the
            // port at this point
            if (ch == '/' || ch == '\0') {
                // end of the host
                if (outHost != NULL) {
                    outHost[hostOffset] = '\0';
                }
                if (ch == '\0') {
#if defined(TOLERATE_HOST_ONLY)
                    // return hostOffset > 0 ? true : false;
                    // it's technically ok for a URI to only specify the host.
                    // It's not enough information by itself to find a resource, but we do not know
                    // if the client is going to compose a URI in parts outside of this.
                    return true;
#else
                    if (hostOffset < 1) {
                        ALOG("Uri '%s': missing host!", uri);
                        // assert( hostOffset > 0 );
                        return false;
                    }
#endif
                }
                // un-parse the forward slash because it must be part of the path
                decoder.Backup();
                break; // reached a '/'
            }

            if (!IsLegalHostCharacter(ch)) {
                ALOG("Uri '%s': illegal character 0x%x in host.", uri, ch);
                if (outHost != NULL) {
                    outHost[0] = '\0';
                }
                return false;
            }
            if (!EncodeCharToBuffer(ch, outHost, outHostSize, hostOffset)) {
                ALOG("Uri '%s': host buffer overflow!", uri);
                return false;
            }
        }
    }

    // parse the path as everything up to the first query indicator (?) or the end of the string
    {
        ptrdiff_t pathOffset = 0;

        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == '\0' || ch == '?') {
                // zero-terminate
                if (outPath != NULL) {
                    outPath[pathOffset] = '\0';
                    if (pathOffset == 1 && outPath[0] == '/') {
                        ALOG("Uri '%s': empty path!", uri);
                        return false;
                    }
                }
                if (ch == '\0') {
                    return true;
                }
                break; // reached the start of the query
            }

            if (!EncodeCharToBuffer(ch, outPath, outPathSize, pathOffset)) {
                ALOG("Uri '%s': path buffer overflow", uri);
                return false;
            }
        }
    }

    // parse the query up to the first fragment indicator (#) or the end of the string
    {
        intptr_t queryOffset = 0;

        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == '\0' || ch == '#') {
                // zero-terminate
                if (outQuery != NULL) {
                    outQuery[queryOffset] = '\0';
                }
                if (ch == '\0') {
                    return true;
                }
                break; // reached the start of the fragment
            }

            if (!EncodeCharToBuffer(ch, outQuery, outQuerySize, queryOffset)) {
                ALOG("Uri '%s': query buffer overflow", uri);
                return false;
            }
        }
    }

    // parse the fragment to the end of the string
    {
        intptr_t fragmentOffset = 0;

        for (;;) {
            ch = decoder.DecodeNext();
            if (ch == '\0') {
                // zero-terminate
                if (outFragment != NULL) {
                    outFragment[fragmentOffset] = '\0';
                }
                return true;
            }

            if (!EncodeCharToBuffer(ch, outFragment, outFragmentSize, fragmentOffset)) {
                ALOG("Uri '%s': fragment buffer overflow", uri);
                return false;
            }
        }
    }
}

bool ovrUri::IsValidUri(char const* uri) {
    char scheme[128];
    char username[128];
    char password[128];
    char host[256];
    int port;
    char path[1024];
    char query[1024];
    char fragment[1024];

    bool const valid = ovrUri::ParseUri(
        uri,
        scheme,
        sizeof(scheme),
        username,
        sizeof(username),
        password,
        sizeof(password),
        host,
        sizeof(host),
        port,
        path,
        sizeof(path),
        query,
        sizeof(query),
        fragment,
        sizeof(fragment));
    return valid;
}

static void LogResult(char const* name, char const* value) {
#if defined(OVR_BUILD_DEBUG)
    ALOG("%s: %s", name, value != NULL ? value : "<NULL>");
#endif
}

static void LogResult(char const* name, int const value) {
#if defined(OVR_BUILD_DEBUG)
    ALOG("%s: %i", name, value);
#endif
}

static void ReportTest(
    char const* testName,
    char const* uri,
    bool const isValid,
    bool const success,
    char const* scheme,
    char const* username,
    char const* password,
    char const* host,
    int const port,
    char const* path,
    char const* query,
    char const* fragment) {
    char const* failMsg = NULL;
    if (isValid) {
        failMsg = !success ? "FAILED TO PARSE VALID URI" : NULL;
    } else if (!isValid) {
        failMsg = success ? "INCORRECTLY PARSED INVALID URI" : NULL;
    }

    if (failMsg != NULL) {
        ALOG("Test %s", testName);
        ALOG("URI: %s", uri);
        ALOG("%s", failMsg);
        LogResult("scheme", scheme);
        LogResult("username", username);
        LogResult("password", password);
        LogResult("host", host);
        LogResult("port", port);
        LogResult("path", path);
        LogResult("query", query);
        LogResult("fragment", fragment);
        assert(failMsg == NULL);
    }
}

static void Test(char const* testName, char const* uri, bool const isValid) {
    char scheme[128];
    char username[128];
    char password[128];
    char host[256];
    int port;
    char path[1024];
    char query[1024];
    char fragment[1024];

    bool const success = ovrUri::ParseUri(
        uri,
        scheme,
        sizeof(scheme),
        username,
        sizeof(username),
        password,
        sizeof(password),
        host,
        sizeof(host),
        port,
        path,
        sizeof(path),
        query,
        sizeof(query),
        fragment,
        sizeof(fragment));
    ReportTest(
        testName,
        uri,
        isValid,
        success,
        scheme,
        username,
        password,
        host,
        port,
        path,
        query,
        fragment);
}

void ovrUri::DoUnitTest() {
    InUnitTest = true; // don't show debug info in release

    Test("1", "file:///sdcard/oculus/360Photos/pic.jpg", true);
    Test("2", "http://puzz.s3.amazonaws.com/2008/07/galerija_equirectangular.jpg", true);
    Test("3", "apk:///assets/default.jpg", true);
    Test("4", "http://puzz.s3.amazonaws.com:8008/2008/07/galerija_equirectangular.jpg", true);
#if defined(TOLERATE_MISSING_SCHEME)
    bool const missingSchemeOk = true;
#else
    bool const missingSchemeOk = false;
#endif
    Test("Path without Scheme", "/sdcard/oculus/360Photos/pic.jpg", missingSchemeOk);
    Test("Path without Scheme 2", "sdcard/oculus/360Photos/pic.jpg", missingSchemeOk);
    Test("Malformed scheme", ":///sdcard/oculus/360Photos/pic.jpg", false);
    Test("6", "No scheme colon", missingSchemeOk);
    Test("7", "file:No host double slash", false);
    Test("8", "file:/No host double slash", false);
    Test("File scheme with illegal spaces in host", "file://No path slash", false);
    Test("http scheme with a host but no path", "http://www.oculus.com", true);
    Test("http scheme only", "http:", false);
    Test("http scheme missing host", "http://", false);
    Test("http scheme with empty host and empty path", "http:///", false);

    InUnitTest = false;
}

} // namespace OVRFW
