/************************************************************************************

Filename    :   OVR_Locale.h
Content     :   Header file for string localization interface.
Created     :   April 6, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/
#pragma once

#include <stdint.h>
#include <string>

#include "OVR_FileSys.h"
#include "JniUtils.h"

namespace OVRFW {

class ovrFileSys;

class ovrLocale {
   public:
    static char const* LOCALIZED_KEY_PREFIX;
    static size_t const LOCALIZED_KEY_PREFIX_LEN;

    //----------------------------------------------------------
    // static methods
    //----------------------------------------------------------
    // creates a locale object for the system's current locale.
    static ovrLocale*
    Create(JNIEnv& jni, jobject activity, char const* name, ovrFileSys* fileSys = nullptr);

    // frees the local object
    static void Destroy(ovrLocale*& localePtr);

    // Takes a UTF8 string and returns an identifier that can be used as an Android string id.
    static std::string MakeStringIdFromUTF8(char const* str);

    // Takes an ANSI string and returns an identifier that can be used as an Android string id.
    static std::string MakeStringIdFromANSI(char const* str);

    // Localization : Returns xliff formatted string
    // These are set to const char * to make sure that's all that's passed in - we support up to 9,
    // add more functions as needed
    static std::string GetXliffFormattedString(const std::string& inXliffStr, const char* arg1);
    static std::string
    GetXliffFormattedString(const std::string& inXliffStr, const char* arg1, const char* arg2);
    static std::string GetXliffFormattedString(
        const std::string& inXliffStr,
        const char* arg1,
        const char* arg2,
        const char* arg3);

    static std::string ToString(char const* fmt, float const f);
    static std::string ToString(char const* fmt, int const i);

    //----------------------------------------------------------
    // public virtual interface methods
    //----------------------------------------------------------
    virtual ~ovrLocale() {}

    virtual char const* GetName() const = 0;
    virtual char const* GetLanguageCode() const = 0;

    // returns true if this locale is the system's default locale (on Android this
    // means the string resources are loaded from res/values/ vs. res/values-*/
    virtual bool IsSystemDefaultLocale() const = 0;

    virtual bool LoadStringsFromAndroidFormatXMLFile(ovrFileSys& fileSys, char const* fileName) = 0;

    // takes a file name, a buffer and a size in bytes. The buffer must already have
    // been loaded. The name is only an identifier used for error reporting.
    virtual bool AddStringsFromAndroidFormatXMLBuffer(
        char const* name,
        char const* buffer,
        size_t const size) = 0;

    // returns the localized string associated with the passed key. Returns false if the
    // key was not found. If the key was not found, out will be set to the defaultStr.
    virtual bool GetLocalizedString(char const* key, char const* defaultStr, std::string& out)
        const = 0;

    // Takes a string with potentially multiple "@string/*" keys and outputs the string to the out
    // buffer with the keys replaced by the localized text.
    virtual void ReplaceLocalizedText(char const* inText, char* out, size_t const outSize)
        const = 0;
};

} // namespace OVRFW
