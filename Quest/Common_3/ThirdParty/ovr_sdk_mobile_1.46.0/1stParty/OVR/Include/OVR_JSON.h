/************************************************************************************

PublicHeader:   None
Filename    :   OVR_JSON.h
Content     :   JSON format reader and writer
Created     :   April 9, 2013
Author      :   Brant Lewis
Notes       :
  The code is a derivative of the cJSON library written by Dave Gamble and subject
  to the following permissive copyright.

  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/
#ifndef OVR_JSON_h
#define OVR_JSON_h

#include <memory>
#include <vector>
#include <string>
#include <list>
#include <fstream>

#include "OVR_Types.h"
#include "OVR_Math.h"
#include "OVR_Std.h"
#include "OVR_LogUtils.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

namespace OVR {

// JSONItemType describes the type of JSON item, specifying the type of
// data that can be obtained from it.
enum JSONItemType {
    JSON_None = 0,
    JSON_Null = 1,
    JSON_Bool = 2,
    JSON_Number = 3,
    JSON_String = 4,
    JSON_Array = 5,
    JSON_Object = 6
};

//-----------------------------------------------------------------------------
// Create a new copy of a string
inline char* JSON_strdup(const char* str) {
    size_t len = OVR_strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (!copy)
        return 0;
    memcpy(copy, str, len);
    return copy;
}

//-----------------------------------------------------------------------------
// Render the number from the given item into a string.
inline char* PrintNumber(double d) {
    char* str;
    int valueint = (int)d;

    if (fabs(((double)valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
        str = (char*)malloc(21); // 2^64+1 can be represented in 21 chars.
        if (str)
            OVR_sprintf(str, 21, "%d", valueint);
    } else {
        str = (char*)malloc(64); // This is a nice tradeoff.
        if (str) {
            // The JSON Standard, section 7.8.3, specifies that decimals are always expressed with
            // '.' and not some locale-specific decimal such as ',' or ' '. However, since we are
            // using the C standard library below to write a floating point number, we need to make
            // sure that it's writing a '.' and not something else. We can't change the locale (even
            // temporarily) here, as it will affect the whole process by default. That are
            // compiler-specific ways to change this per-thread, but below we implement the simple
            // solution of simply fixing the decimal after the string was written.

            if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60)
                OVR_sprintf(str, 64, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
                OVR_sprintf(str, 64, "%e", d);
            else
                OVR_sprintf(str, 64, "%f", d);
        }
    }
    return str;
}

//-----------------------------------------------------------------------------
// Parse the input text into an un-escaped cstring, and populate item.
static constexpr unsigned char firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

//-----------------------------------------------------------------------------
// Helper to assign error sting and return 0.
inline const char* AssignError(const char** perror, const char* errorMessage) {
    if (perror)
        *perror = errorMessage;
    return nullptr;
}

//-----------------------------------------------------------------------------
// Parses a hex string up to the specified number of digits.
// Returns the first character after the string.
inline const char* ParseHex(unsigned* val, unsigned digits, const char* str) {
    *val = 0;

    for (unsigned digitCount = 0; digitCount < digits; digitCount++, str++) {
        unsigned v = *str;

        if ((v >= '0') && (v <= '9'))
            v -= '0';
        else if ((v >= 'a') && (v <= 'f'))
            v = 10 + v - 'a';
        else if ((v >= 'A') && (v <= 'F'))
            v = 10 + v - 'A';
        else
            break;

        *val = *val * 16 + v;
    }

    return str;
}

//-----------------------------------------------------------------------------
// Render the string provided to an escaped version that can be printed.
inline char* PrintString(const char* str) {
    const char* ptr;
    char *ptr2, *out;
    int len = 0;
    unsigned char token;

    if (!str)
        return JSON_strdup("");
    ptr = str;

    token = *ptr;
    while (token && ++len) {
        if (strchr("\"\\\b\f\n\r\t", token))
            len++;
        else if (token < 32)
            len += 5;
        ptr++;
        token = *ptr;
    }

    int buff_size = len + 3;
    out = (char*)malloc(buff_size);
    if (!out)
        return 0;

    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';

    while (*ptr) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            *ptr2++ = '\\';
            switch (token = *ptr++) {
                case '\\':
                    *ptr2++ = '\\';
                    break;
                case '\"':
                    *ptr2++ = '\"';
                    break;
                case '\b':
                    *ptr2++ = 'b';
                    break;
                case '\f':
                    *ptr2++ = 'f';
                    break;
                case '\n':
                    *ptr2++ = 'n';
                    break;
                case '\r':
                    *ptr2++ = 'r';
                    break;
                case '\t':
                    *ptr2++ = 't';
                    break;
                default:
                    OVR_sprintf(ptr2, buff_size - (ptr2 - out), "u%04x", token);
                    ptr2 += 5;
                    break; // Escape and print.
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2++ = '\0';
    return out;
}

//-----------------------------------------------------------------------------
// Utility to jump whitespace and cr/lf
static const char* skip(const char* in) {
    while (in && *in && (unsigned char)*in <= ' ')
        in++;
    return in;
}

//-----------------------------------------------------------------------------
// ***** JSON

// JSON object represents a JSON node that can be either a root of the JSON tree
// or a child item. Every node has a type that describes what it is.
// New JSON trees are typically loaded with JSON::Load or created with JSON::Parse.

class JSON {
   public:
    std::list<std::shared_ptr<JSON>> Children;
    JSONItemType Type; // Type of this JSON node.
    std::string Name; // Name part of the {Name, Value} pair in a parent object.
    std::string Value;
    double dValue;

   public:
    JSON(JSONItemType itemType = JSON_Object) : Type(itemType), dValue(0.0) {}
    ~JSON() {}

    // *** Creation of NEW JSON objects

    static std::shared_ptr<JSON> CreateObject() {
        return std::make_shared<JSON>(JSON_Object);
    }
    static std::shared_ptr<JSON> CreateNull() {
        return std::make_shared<JSON>(JSON_Null);
    }
    static std::shared_ptr<JSON> CreateArray() {
        return std::make_shared<JSON>(JSON_Array);
    }
    static std::shared_ptr<JSON> CreateBool(bool b) {
        return createHelper(JSON_Bool, b ? 1.0 : 0.0);
    }
    static std::shared_ptr<JSON> CreateNumber(double num) {
        return createHelper(JSON_Number, num);
    }
    static std::shared_ptr<JSON> CreateString(const char* s) {
        return createHelper(JSON_String, 0.0, s);
    }

    // Creates a new JSON object from parsing the given string.
    // Returns a null pointer and fills in *perror in case of parse error.
    static std::shared_ptr<JSON> Parse(const char* buff, const char** perror = nullptr) {
        const char* end = nullptr;
        std::shared_ptr<JSON> json = std::make_shared<JSON>();

        if (json == nullptr) {
            AssignError(perror, "Error: Failed to allocate memory");
            return nullptr;
        }

        end = json->parseValue(skip(buff), perror);
        if (!end) {
            return nullptr;
        } // parse failure. ep is set.

        return json;
    }

    // Loads and parses a JSON object from a file.
    // Returns a null pointer and fills in *perror in case of parse error.
    static std::shared_ptr<JSON> Load(const char* path, const char** perror = nullptr) {
        std::ifstream is;
        is.open(path, std::ifstream::in);
        if (!is.is_open()) {
#if defined(OVR_OS_ANDROID)
            OVR_LOG("JSON::Load failed to open %s", path);
#endif
            AssignError(perror, "Failed to open file");
            return nullptr;
        }

        // get size
        is.seekg(0, is.end);
        int len = static_cast<int>(is.tellg());
        is.seekg(0, is.beg);

        // allocate buffer
        std::vector<uint8_t> buff;
        buff.resize(len + 1);

        // read all file
        is.read((char*)buff.data(), len);
        if (!is) {
#if defined(OVR_OS_ANDROID)
            OVR_LOG("JSON::Load failed to read %s", path);
#endif
            AssignError(perror, "Failed to read file");
            return nullptr;
        }

        // close
        is.close();

        // Ensure the result is null-terminated since Parse() expects null-terminated input.
        buff[len] = '\0';

        std::shared_ptr<JSON> json = JSON::Parse((char*)buff.data(), perror);

#if defined(OVR_OS_ANDROID)
#if defined(OVR_BUILD_DEBUG)
        OVR_LOG(
            "JSON::Load finished reading %s - length = %d ptr = %p",
            path,
            (int)buff.size(),
            json.get());
#endif
#endif

        return json;
    }

    // Saves a JSON object to a file.
    bool Save(const char* path) const {
        std::ofstream os;
        os.open(path, std::ios::out | std::ios::trunc);
        if (!os.is_open()) {
            return false;
        }

        char* text = PrintValue(0, true);
        if (text) {
            intptr_t len = OVR_strlen(text);
            OVR_ASSERT(len <= (intptr_t)(int)len);

            os.write((char*)text, (int)len);
            // save stream state
            bool writeComplete = !!os;
            os.close();

            free(text);
            return writeComplete;
        } else {
            return false;
        }
    }
    // Child item access functions
    void AddItem(const char* string, std::shared_ptr<JSON> item) {
        if (!item)
            return;
        item->Name = string;
        Children.push_back(item);
    }
    void AddBoolItem(const char* name, bool b) {
        AddItem(name, CreateBool(b));
    }
    void AddNumberItem(const char* name, double n) {
        AddItem(name, CreateNumber(n));
    }
    void AddStringItem(const char* name, const char* s) {
        AddItem(name, CreateString(s));
    }

    // *** Object Member Access

    // These provide access to child items of the list.
    bool HasItems() const {
        return Children.empty();
    }
    // Returns first/last child item, or null if child list is empty.
    std::shared_ptr<JSON> GetFirstItem() {
        return (!Children.empty()) ? Children.front() : nullptr;
    }
    const std::shared_ptr<JSON> GetFirstItem() const {
        return (!Children.empty()) ? Children.front() : nullptr;
    }
    std::shared_ptr<JSON> GetLastItem() {
        return (!Children.empty()) ? Children.back() : nullptr;
    }
    const std::shared_ptr<JSON> GetLastItem() const {
        return (!Children.empty()) ? Children.back() : nullptr;
    }

    // Counts the number of items in the object; these methods are inefficient.
    unsigned GetItemCount() const {
        return static_cast<unsigned>(Children.size());
    }
    std::shared_ptr<JSON> GetItemByIndex(unsigned index) {
        for (const std::shared_ptr<JSON>& child : Children) {
            if (index-- == 0) {
                return child;
            }
        }
        return nullptr;
    }
    const std::shared_ptr<JSON> GetItemByIndex(unsigned index) const {
        for (const std::shared_ptr<JSON>& child : Children) {
            if (index-- == 0) {
                return child;
            }
        }
        return nullptr;
    }
    std::shared_ptr<JSON> GetItemByName(const char* name) {
        for (const std::shared_ptr<JSON>& child : Children) {
            if (OVR_strcmp(child->Name.c_str(), name) == 0) {
                return child;
            }
        }
        return nullptr;
    }
    const std::shared_ptr<JSON> GetItemByName(const char* name) const {
        for (const std::shared_ptr<JSON>& child : Children) {
            if (OVR_strcmp(child->Name.c_str(), name) == 0) {
                return child;
            }
        }
        return nullptr;
    }
    void ReplaceNodeWith(const char* name, const std::shared_ptr<JSON> newNode) {
        for (auto it = Children.begin(); it != Children.end(); ++it) {
            if (OVR_strcmp((*it)->Name.c_str(), name) == 0) {
                *it = newNode;
                return;
            }
        }
    }

    /*
        // Returns next item in a list of children; 0 if no more items exist.
        JSON*           GetNextItem(JSON* item)				{ return (item->pNext == nullptr) ?
       nullptr : item->pNext; } const JSON*     GetNextItem(const JSON* item) const	{ return
       (item->pNext == nullptr) ? nullptr : item->pNext; } JSON*           GetPrevItem(JSON* item)
       { return (item->pPrev == nullptr) ? nullptr : item->pPrev; } const JSON* GetPrevItem(const
       JSON* item) const	{ return (item->pPrev == nullptr) ? nullptr : item->pPrev; }
    */

    // Value access with range checking where possible.
    // Using the JsonReader class is recommended instead of using these.
    bool GetBoolValue() const {
        OVR_ASSERT((Type == JSON_Number) || (Type == JSON_Bool));
        OVR_ASSERT(dValue == 0.0 || dValue == 1.0); // if this hits, value is out of range
        return (dValue != 0.0);
    }
    int32_t GetInt32Value() const {
        OVR_ASSERT(Type == JSON_Number);
        OVR_ASSERT(dValue >= INT_MIN && dValue <= INT_MAX); // if this hits, value is out of range
        return (int32_t)dValue;
    }
    int64_t GetInt64Value() const {
        OVR_ASSERT(Type == JSON_Number);
        OVR_ASSERT(
            dValue >= -9007199254740992LL &&
            dValue <= 9007199254740992LL); // 2^53 - if this hits, value is out of range
        return (int64_t)dValue;
    }
    float GetFloatValue() const {
        OVR_ASSERT(Type == JSON_Number);
        OVR_ASSERT(dValue >= -FLT_MAX && dValue <= FLT_MAX); // too large to represent as a float
        OVR_ASSERT(
            dValue == 0 || dValue <= -FLT_MIN ||
            dValue >= FLT_MIN); // if the number is too small to be represented as a float
        return (float)dValue;
    }
    double GetDoubleValue() const {
        OVR_ASSERT(Type == JSON_Number);
        return dValue;
    }
    const std::string& GetStringValue() const {
        OVR_ASSERT(
            Type == JSON_String || Type == JSON_Null); // May be JSON_Null if the value od a string
                                                       // field was actually the word "null"
        return Value;
    }

    // *** Array Element Access

    // Add new elements to the end of array.
    void AddArrayElement(std::shared_ptr<JSON> item) {
        if (!item) {
            return;
        }

        Children.push_back(item);
    }
    void AddArrayBool(bool b) {
        AddArrayElement(CreateBool(b));
    }
    void AddArrayNumber(double n) {
        AddArrayElement(CreateNumber(n));
    }
    void AddArrayString(const char* s) {
        AddArrayElement(CreateString(s));
    }

    // Accessed array elements; currently inefficient.
    int GetArraySize() const {
        if (Type == JSON_Array) {
            return GetItemCount();
        } else
            return 0;
    }
    double GetArrayNumber(int index) const {
        if (Type == JSON_Array) {
            const std::shared_ptr<JSON> number = GetItemByIndex(index);
            return number ? number->dValue : 0.0;
        } else {
            return 0;
        }
    }
    const char* GetArrayString(int index) const {
        if (Type == JSON_Array) {
            const std::shared_ptr<JSON> number = GetItemByIndex(index);
            return number ? number->Value.c_str() : nullptr;
        } else {
            return nullptr;
        }
    }

    // Return text value of JSON. Use free when done with return value
    char* PrintValue(int depth, bool fmt) const {
        char* out = nullptr;

        switch (Type) {
            case JSON_Null:
                out = JSON_strdup("null");
                break;
            case JSON_Bool:
                if (dValue == 0)
                    out = JSON_strdup("false");
                else
                    out = JSON_strdup("true");
                break;
            case JSON_Number:
                out = PrintNumber(dValue);
                break;
            case JSON_String:
                out = PrintString(Value.c_str());
                break;
            case JSON_Array:
                out = PrintArray(depth, fmt);
                break;
            case JSON_Object:
                out = PrintObject(depth, fmt);
                break;
            case JSON_None: {
                OVR_ASSERT(false);
#if defined(OVR_OS_ANDROID)
                OVR_LOG("JSON::PrintValue - Bad JSON type.");
#endif
                break;
            }
        }
        return out;
    }

   protected:
    static std::shared_ptr<JSON>
    createHelper(JSONItemType itemType, double dval, const char* strVal = nullptr) {
        std::shared_ptr<JSON> item = std::make_shared<JSON>(itemType);
        if (item) {
            item->dValue = dval;
            if (strVal)
                item->Value = strVal;
        }
        return item;
    }

    // JSON Parsing helper functions.
    const char* parseValue(const char* buff, const char** perror) {
        if (perror)
            *perror = 0;

        if (!buff)
            return nullptr; // Fail on null.

        if (!strncmp(buff, "null", 4)) {
            Type = JSON_Null;
            return buff + 4;
        }
        if (!strncmp(buff, "false", 5)) {
            Type = JSON_Bool;
            Value = "false";
            dValue = 0.0;
            return buff + 5;
        }
        if (!strncmp(buff, "true", 4)) {
            Type = JSON_Bool;
            Value = "true";
            dValue = 1.0;
            return buff + 4;
        }
        if (*buff == '\"') {
            return parseString(buff, perror);
        }
        if (*buff == '-' || (*buff >= '0' && *buff <= '9')) {
            return parseNumber(buff);
        }
        if (*buff == '[') {
            return parseArray(buff, perror);
        }
        if (*buff == '{') {
            return parseObject(buff, perror);
        }

        return AssignError(perror, (std::string("Syntax Error: Invalid syntax: ") + buff).c_str());
    }
    const char* parseNumber(const char* num) {
        const char* num_start = num;
        double n = 0, sign = 1, scale = 0;
        int subscale = 0, signsubscale = 1;

        // Could use sscanf for this?
        if (*num == '-') {
            sign = -1, num++; // Has sign?
        }
        if (*num == '0') {
            num++; // is zero
        }

        if (*num >= '1' && *num <= '9') {
            do {
                n = (n * 10.0) + (*num++ - '0');
            } while (*num >= '0' && *num <= '9'); // Number?
        }

        if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
            num++;
            do {
                n = (n * 10.0) + (*num++ - '0');
                scale--;
            } while (*num >= '0' && *num <= '9'); // Fractional part?
        }

        if (*num == 'e' || *num == 'E') // Exponent?
        {
            num++;
            if (*num == '+') {
                num++;
            } else if (*num == '-') {
                signsubscale = -1;
                num++; // With sign?
            }

            while (*num >= '0' && *num <= '9') {
                subscale = (subscale * 10) + (*num++ - '0'); // Number?
            }
        }

        // Number = +/- number.fraction * 10^+/- exponent
        n = sign * n * pow(10.0, (scale + subscale * signsubscale));

        // Assign parsed value.
        Type = JSON_Number;
        dValue = n;
        Value.assign(num_start, num - num_start);

        return num;
    }
    const char* parseArray(const char* buff, const char** perror) {
        std::shared_ptr<JSON> child;
        if (*buff != '[') {
            return AssignError(perror, "Syntax Error: Missing opening bracket");
        }

        Type = JSON_Array;
        buff = skip(buff + 1);

        if (*buff == ']')
            return buff + 1; // empty array.

        child = std::make_shared<JSON>();
        if (!child)
            return nullptr; // memory fail
        Children.push_back(child);

        buff = skip(child->parseValue(skip(buff), perror)); // skip any spacing, get the buff.
        if (!buff)
            return 0;

        while (*buff == ',') {
            std::shared_ptr<JSON> new_item = std::make_shared<JSON>();
            if (!new_item)
                return AssignError(perror, "Error: Failed to allocate memory");

            Children.push_back(new_item);

            buff = skip(new_item->parseValue(skip(buff + 1), perror));
            if (!buff)
                return AssignError(perror, "Error: Failed to allocate memory");
        }

        if (*buff == ']')
            return buff + 1; // end of array

        return AssignError(perror, "Syntax Error: Missing ending bracket");
    }
    const char* parseObject(const char* buff, const char** perror) {
        if (*buff != '{') {
            return AssignError(perror, "Syntax Error: Missing opening brace");
        }

        Type = JSON_Object;
        buff = skip(buff + 1);
        if (*buff == '}')
            return buff + 1; // empty array.

        std::shared_ptr<JSON> child = std::make_shared<JSON>();
        Children.push_back(child);

        buff = skip(child->parseString(skip(buff), perror));
        if (!buff)
            return 0;
        child->Name = child->Value;
        child->Value.clear();

        if (*buff != ':') {
            return AssignError(perror, "Syntax Error: Missing colon");
        }

        buff = skip(child->parseValue(skip(buff + 1), perror)); // skip any spacing, get the value.
        if (!buff)
            return 0;

        while (*buff == ',') {
            child = std::make_shared<JSON>();
            if (!child)
                return 0; // memory fail

            Children.push_back(child);

            buff = skip(child->parseString(skip(buff + 1), perror));
            if (!buff)
                return 0;

            child->Name = child->Value;
            child->Value.clear();

            if (*buff != ':') {
                return AssignError(perror, "Syntax Error: Missing colon");
            } // fail!

            // Skip any spacing, get the value.
            buff = skip(child->parseValue(skip(buff + 1), perror));
            if (!buff)
                return 0;
        }

        if (*buff == '}')
            return buff + 1; // end of array

        return AssignError(perror, "Syntax Error: Missing closing brace");
    }
    const char* parseString(const char* str, const char** perror) {
        const char* ptr = str + 1;
        const char* p;
        char* ptr2;
        char* out;
        int len = 0;
        unsigned uc, uc2;

        if (*str != '\"') {
            return AssignError(perror, "Syntax Error: Missing quote");
        }

        while (*ptr != '\"' && *ptr && ++len) {
            if (*ptr++ == '\\') {
                if (*ptr) {
                    ptr++; // Skip escaped quotes.
                } else {
                    // we reached the end of the file too soon, stop
                }
            }
        }

        // This is how long we need for the string, roughly.
        out = (char*)malloc(len + 1);
        if (!out)
            return 0;

        ptr = str + 1;
        ptr2 = out;

        while (*ptr != '\"' && *ptr) {
            if (*ptr != '\\') {
                *ptr2++ = *ptr++;
            } else {
                ptr++;
                switch (*ptr) {
                    case 'b':
                        *ptr2++ = '\b';
                        break;
                    case 'f':
                        *ptr2++ = '\f';
                        break;
                    case 'n':
                        *ptr2++ = '\n';
                        break;
                    case 'r':
                        *ptr2++ = '\r';
                        break;
                    case 't':
                        *ptr2++ = '\t';
                        break;

                    // Transcode utf16 to utf8.
                    case 'u':

                        // Get the unicode char.
                        p = ParseHex(&uc, 4, ptr + 1);
                        if (ptr != p)
                            ptr = p - 1;

                        if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
                            break; // Check for invalid.

                        // UTF16 surrogate pairs.
                        if (uc >= 0xD800 && uc <= 0xDBFF) {
                            if (ptr[1] != '\\' || ptr[2] != 'u')
                                break; // Missing second-half of surrogate.

                            p = ParseHex(&uc2, 4, ptr + 3);
                            if (ptr != p)
                                ptr = p - 1;

                            if (uc2 < 0xDC00 || uc2 > 0xDFFF)
                                break; // Invalid second-half of surrogate.

                            uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                        }

                        len = 4;

                        if (uc < 0x80)
                            len = 1;
                        else if (uc < 0x800)
                            len = 2;
                        else if (uc < 0x10000)
                            len = 3;

                        ptr2 += len;

                        switch (len) {
                            case 4:
                                *--ptr2 = static_cast<char>((uc | 0x80) & 0xBF);
                                uc >>= 6;
                                // no break, fall through
                            case 3:
                                *--ptr2 = static_cast<char>((uc | 0x80) & 0xBF);
                                uc >>= 6;
                                // no break
                            case 2:
                                *--ptr2 = static_cast<char>((uc | 0x80) & 0xBF);
                                uc >>= 6;
                                // no break
                            case 1:
                                *--ptr2 = (char)(uc | firstByteMark[len]);
                                // no break
                        }
                        ptr2 += len;
                        break;

                    default:
                        if (*ptr) {
                            *ptr2++ = *ptr;
                        }
                        break;
                }
                if (*ptr) {
                    ptr++;
                }
            }
        }

        *ptr2 = 0;
        if (*ptr == '\"')
            ptr++;

        // Make a copy of the string
        Value = out;
        free(out);
        Type = JSON_String;

        return ptr;
    }

    char* PrintObject(int depth, bool fmt) const {
        char **entries = 0, **names = 0;
        char* out = 0;
        char *ptr, *ret, *str;
        intptr_t len = 7, i = 0, j;
        bool fail = false;

        // Count the number of entries.
        int numentries = GetItemCount();

        // Explicitly handle empty object case
        if (numentries == 0) {
            out = (char*)malloc(fmt ? depth + 4 : 4);
            if (!out)
                return 0;
            ptr = out;
            *ptr++ = '{';

            if (fmt) {
                *ptr++ = '\n';
                for (i = 0; i < depth - 1; i++)
                    *ptr++ = '\t';
            }
            *ptr++ = '}';
            *ptr++ = '\0';
            return out;
        }
        // Allocate space for the names and the objects
        entries = (char**)malloc(numentries * sizeof(char*));
        if (!entries)
            return 0;
        names = (char**)malloc(numentries * sizeof(char*));

        if (!names) {
            free(entries);
            return 0;
        }
        memset(entries, 0, sizeof(char*) * numentries);
        memset(names, 0, sizeof(char*) * numentries);

        // Collect all the results into our arrays:
        depth++;
        if (fmt)
            len += depth;

        for (const auto& child : Children) {
            names[i] = str = PrintString(child->Name.c_str());
            entries[i++] = ret = child->PrintValue(depth, fmt);

            if (str && ret) {
                len += OVR_strlen(ret) + OVR_strlen(str) + 2 + (fmt ? 2 + depth : 0);
            } else {
                fail = true;
                break;
            }
        }

        // Try to allocate the output string
        if (!fail)
            out = (char*)malloc(len);
        if (!out)
            fail = true;

        // Handle failure
        if (fail) {
            for (i = 0; i < numentries; i++) {
                if (names[i])
                    free(names[i]);

                if (entries[i])
                    free(entries[i]);
            }

            free(names);
            free(entries);
            return 0;
        }

        // Compose the output:
        *out = '{';
        ptr = out + 1;
        if (fmt) {
            *ptr++ = '\n';
        }
        *ptr = 0;

        for (i = 0; i < numentries; i++) {
            if (fmt) {
                for (j = 0; j < depth; j++) {
                    *ptr++ = '\t';
                }
            }
            OVR_strcpy(ptr, len - (ptr - out), names[i]);
            ptr += OVR_strlen(names[i]);
            *ptr++ = ':';

            if (fmt) {
                *ptr++ = '\t';
            }

            OVR_strcpy(ptr, len - (ptr - out), entries[i]);
            ptr += OVR_strlen(entries[i]);

            if (i != numentries - 1) {
                *ptr++ = ',';
            }

            if (fmt) {
                *ptr++ = '\n';
            }
            *ptr = 0;

            free(names[i]);
            free(entries[i]);
        }

        free(names);
        free(entries);

        if (fmt) {
            for (i = 0; i < depth - 1; i++) {
                *ptr++ = '\t';
            }
        }
        *ptr++ = '}';
        *ptr++ = '\0';

        return out;
    }

    char* PrintArray(int depth, bool fmt) const {
        char** entries;
        char *out = 0, *ptr, *ret;
        intptr_t len = 5;

        bool fail = false;

        // How many entries in the array?
        int numentries = GetItemCount();
        if (!numentries) {
            out = (char*)malloc(3);
            if (out)
                OVR_strcpy(out, 3, "[]");
            return out;
        }
        // Allocate an array to hold the values for each
        entries = (char**)malloc(numentries * sizeof(char*));
        if (!entries)
            return 0;
        memset(entries, 0, numentries * sizeof(char*));

        //// Retrieve all the results:
        int entry = 0;
        for (std::shared_ptr<JSON> child : Children) {
            if (entry >= numentries)
                break;

            ret = child->PrintValue(depth + 1, fmt);
            entries[entry] = ret;
            if (ret)
                len += OVR_strlen(ret) + 2 + (fmt ? 1 : 0);
            else {
                fail = true;
                break;
            }
            ++entry;
        }

        // If we didn't fail, try to malloc the output string
        if (!fail)
            out = (char*)malloc(len);
        // If that fails, we fail.
        if (!out)
            fail = true;

        // Handle failure.
        if (fail) {
            for (int i = 0; i < numentries; i++) {
                if (entries[i])
                    free(entries[i]);
            }
            free(entries);
            return 0;
        }

        // Compose the output array.
        *out = '[';
        ptr = out + 1;
        *ptr = '\0';
        for (int i = 0; i < numentries; i++) {
            OVR_strcpy(ptr, len - (ptr - out), entries[i]);
            ptr += OVR_strlen(entries[i]);
            if (i != numentries - 1) {
                *ptr++ = ',';
                if (fmt)
                    *ptr++ = ' ';
                *ptr = '\0';
            }
            free(entries[i]);
        }
        free(entries);
        *ptr++ = ']';
        *ptr++ = '\0';
        return out;
    }

    friend class JsonReader;
};

//-----------------------------------------------------------------------------
// ***** JsonReader

// Fast JSON reader. This reads one JSON node at a time.
//
// When used appropriately this class should result in easy to read, const
// correct code with maximum variable localization. Reading the JSON data with
// this class is not only fast but also safe in that variables or objects are
// either skipped or default initialized if the JSON file contains different
// data than expected.
//
// To be completely safe and to support backward / forward compatibility the
// JSON object names will have to be verified. This class will try to verify
// the object names with minimal effort. If the children of a node are read
// in the order they appear in the JSON file then using this class results in
// only one string comparison per child. Only if the children are read out
// of order, all children may have to be iterated to match a child name.
// This should, however, only happen when the code that writes out the JSON
// file has been changed without updating the code that reads back the data.
// Either way this class will do the right thing as long as the JSON tree is
// considered const and is not changed underneath this class.
//
// This is an example of how this class can be used to load a simplified indexed
// triangle model:
//
//	std::shared_ptr<JSON> json = JSON::Load( "filename.json" );
//	const JsonReader model( json );
//	if ( model.IsObject() )
//	{
//		const JsonReader vertices( model.GetChildByName( "vertices" ) );
//		if ( vertices.IsArray() )
//		{
//			while ( !vertices.IsEndOfArray() )
//			{
//				const JsonReader vertex( vertices.GetNextArrayElement() );
//				if ( vertex.IsObject() )
//				{
//					const float x = vertex.GetChildFloatByName( "x", 0.0f );
//					const float y = vertex.GetChildFloatByName( "y", 0.0f );
//					const float z = vertex.GetChildFloatByName( "z", 0.0f );
//				}
//			}
//		}
//		const JsonReader indices( model.GetChildByName( "indices" ) );
//		if ( indices.IsArray() )
//		{
//			while ( !indices.IsEndOfArray() )
//			{
//				const int index = indices.GetNextArrayInt32( 0 );
//			}
//		}
//	}
//  // shared_ptr will free resources when it goes out of scope
//

class JsonReader {
   public:
    JsonReader(const std::shared_ptr<JSON> json) : Parent(json) {
        if (Parent) {
            Child = Parent->Children.begin();
        }
    }

    JsonReader(std::list<std::shared_ptr<JSON>>::iterator it) : JsonReader(*it) {}

    const std::shared_ptr<JSON> AsParent() const {
        return Parent;
    }

    bool IsValid() const {
        return Parent != nullptr;
    }
    bool IsObject() const {
        return Parent != nullptr && Parent->Type == JSON_Object;
    }
    bool IsArray() const {
        return Parent != nullptr && Parent->Type == JSON_Array;
    }
    bool IsEndOfArray() const {
        OVR_ASSERT(Parent != nullptr);
        return (Child == Parent->Children.end());
    }

    std::list<std::shared_ptr<JSON>>::iterator GetFirstChild() const {
        return Parent->Children.begin();
    }
    std::list<std::shared_ptr<JSON>>::iterator GetNextChild(
        std::list<std::shared_ptr<JSON>>::iterator& child) const {
        auto childClone = child;
        ++childClone;
        return childClone;
    }

    const std::shared_ptr<JSON> GetChildByName(const char* childName) const {
        assert(IsObject());

        // Check if the the cached child pointer is valid.
        if (Child != Parent->Children.end()) {
            if (OVR_strcmp((*Child)->Name.c_str(), childName) == 0) {
                const std::shared_ptr<JSON> c = *Child;
                ++Child; // Cache the next child.
                return c;
            }
        }
        // Itereate over all children.
        for (auto c = Parent->Children.begin(); c != Parent->Children.end(); ++c) {
            if (OVR_strcmp((*c)->Name.c_str(), childName) == 0) {
                Child = c; // Cache the next child.
                return *c;
            }
        }
        return 0;
    }
    bool GetChildBoolByName(const char* childName, const bool defaultValue = false) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return (c != nullptr) ? c->GetBoolValue() : defaultValue;
    }
    int32_t GetChildInt32ByName(const char* childName, const int32_t defaultValue = 0) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return (c != nullptr) ? c->GetInt32Value() : defaultValue;
    }
    int64_t GetChildInt64ByName(const char* childName, const int64_t defaultValue = 0) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return (c != nullptr) ? c->GetInt64Value() : defaultValue;
    }
    float GetChildFloatByName(const char* childName, const float defaultValue = 0.0f) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return (c != nullptr) ? c->GetFloatValue() : defaultValue;
    }
    double GetChildDoubleByName(const char* childName, const double defaultValue = 0.0) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return (c != nullptr) ? c->GetDoubleValue() : defaultValue;
    }
    const std::string GetChildStringByName(
        const char* childName,
        const std::string& defaultValue = std::string("")) const {
        const std::shared_ptr<JSON> c = GetChildByName(childName);
        return std::string(
            (c != nullptr && c->Type != JSON_Null) ? c->GetStringValue() : defaultValue);
    }

    const std::shared_ptr<JSON> GetNextArrayElement() const {
        assert(IsArray());

        // Check if the the cached child pointer is valid.
        if (Child != Parent->Children.end()) {
            const std::shared_ptr<JSON> c = *Child;
            ++Child; // Cache the next child.
            return c;
        }
        return nullptr;
    }

    bool GetNextArrayBool(const bool defaultValue = false) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return (c != nullptr) ? c->GetBoolValue() : defaultValue;
    }
    int32_t GetNextArrayInt32(const int32_t defaultValue = 0) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return (c != nullptr) ? c->GetInt32Value() : defaultValue;
    }
    int64_t GetNextArrayInt64(const int64_t defaultValue = 0) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return (c != nullptr) ? c->GetInt64Value() : defaultValue;
    }
    float GetNextArrayFloat(const float defaultValue = 0.0f) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return (c != nullptr) ? c->GetFloatValue() : defaultValue;
    }
    double GetNextArrayDouble(const double defaultValue = 0.0) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return (c != nullptr) ? c->GetDoubleValue() : defaultValue;
    }
    const std::string GetNextArrayString(const std::string& defaultValue = std::string("")) const {
        const std::shared_ptr<JSON> c = GetNextArrayElement();
        return std::string((c != nullptr) ? c->GetStringValue() : defaultValue);
    }

   private:
    std::shared_ptr<JSON> Parent;
    mutable std::list<std::shared_ptr<JSON>>::iterator Child; // cached child pointer (iterator)
};

} // namespace OVR

#endif // OVR_JSON_h
