/************************************************************************************

Filename    :   Reflection.cpp
Content     :   Functions and declarations for introspection and reflection of C++ objects.
Created     :   11/16/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Reflection.h"
#include "ReflectionData.h"

#include "Misc/Log.h"
#include "Locale/OVR_Locale.h"

#include "OVR_TypesafeNumber.h"

#include <alloca.h>
#include <cstdlib> // for strtoll

namespace OVRFW {

//==============================================================================================
// Parsing
//==============================================================================================

template <typename Type>
bool EnumForName(ovrEnumInfo const* enumInfos, char const* const name, Type& out) {
    int enumMax = INT_MIN;
    for (int i = 0; enumInfos[i].Name != NULL; ++i) {
        if (OVR::OVR_strcmp(enumInfos[i].Name, name) == 0) {
            out = static_cast<Type>(enumInfos[i].Value);
            return true;
        }
        if (enumInfos[i].Value > enumMax) {
            enumMax = enumInfos[i].Value;
        }
    }
    out = static_cast<Type>(enumMax);
    return false;
}

ovrParseResult ExpectPunctuation(const char* name, ovrLexer& lex, const char* expected) {
    const int MAX_TOKEN = 128;
    char token[MAX_TOKEN];
    ovrLexer::ovrResult res = lex.ExpectPunctuation(expected, token, MAX_TOKEN);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(
            ovrLexer::LEX_RESULT_UNEXPECTED_TOKEN,
            "Error parsing '%s': Expected one of '%s', got '%s'",
            name,
            expected,
            token);
    }
    return ovrParseResult();
}

ovrParseResult ParseBool(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    bool& out = *static_cast<bool*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
    if (res == ovrLexer::LEX_RESULT_OK) {
        if (!OVR::OVR_strcmp(token, "false") || !OVR::OVR_strcmp(token, "0")) {
            out = false;
            return ovrParseResult();
        } else if (!OVR::OVR_strcmp(token, "true") || !OVR::OVR_strcmp(token, "1")) {
            out = true;
            return ovrParseResult();
        }
    }

    return ovrParseResult(ovrLexer::LEX_RESULT_UNEXPECTED_TOKEN, "Error parsing '%s'", name);
}

ovrParseResult ParseInt(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    int& out = *static_cast<int*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.ParseInt(out, 0);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected int, got '%s'", name, token);
    }
    return ovrParseResult();
}

ovrParseResult ParseFloat(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    float& out = *static_cast<float*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.ParseFloat(out, 0.0f);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected float, got '%s'", name, token);
    }
    return ovrParseResult();
}

ovrParseResult ParseDouble(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    double& out = *static_cast<double*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.ParseDouble(out, 0.0);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected double, got '%s'", name, token);
    }
    return ovrParseResult();
}

ovrParseResult ParseEnum(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* atomicInfo,
    void* outPtr,
    size_t const /*arraySize*/) {
    int& out = *static_cast<int*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected enum, got '%s'", name, token);
    }

    if (!EnumForName(atomicInfo->EnumInfos, token, out)) {
        return ovrParseResult(
            ovrLexer::LEX_RESULT_UNEXPECTED_TOKEN,
            "Error parsing '%s': expected enum, got '%s'",
            name,
            token);
    }
    return ovrParseResult();
}

ovrParseResult ParseTypesafeNumber_int(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    enum eTempEnum { INVALID_TEMP_ENUM = 0 };
    typedef OVR::TypesafeNumberT<int, eTempEnum, INVALID_TEMP_ENUM> TempTypesafeNumber;

    TempTypesafeNumber& out = *static_cast<TempTypesafeNumber*>(outPtr);

    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    int value;
    ovrLexer::ovrResult res = lex.ParseInt(value, 0);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected int, got '%s'", name, token);
    }
    out.Set(value);

    return ovrParseResult();
}

ovrParseResult ParseTypesafeNumber_long_long(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    enum eTempEnum { INVALID_TEMP_ENUM = 0 };
    typedef OVR::TypesafeNumberT<long long, eTempEnum, INVALID_TEMP_ENUM> TempTypesafeNumber;

    TempTypesafeNumber& out = *static_cast<TempTypesafeNumber*>(outPtr);

    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected long long, got '%s'", name, token);
    }
    long long value = strtoll(token, nullptr, 10);
    out.Set(value);
    return ovrParseResult();
}

ovrParseResult ParseBitFlags(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* atomicInfo,
    void* outPtr,
    size_t const /*arraySize*/) {
    int& out = *static_cast<int*>(outPtr);
    size_t const MAX_TOKEN = 128;
    char token[MAX_TOKEN];

    out = 0;

    ovrLexer::ovrResult res;
    for (;;) {
        res = lex.NextToken(token, MAX_TOKEN);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(res, "Error parsing '%s': expected enum, got '%s'", name, token);
        }

        int e;
        bool ok = EnumForName(atomicInfo->EnumInfos, token, e);
        if (!ok) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_UNEXPECTED_TOKEN,
                "Error parsing '%s': exepected enum, got '%s'",
                name,
                token);
        }
        out |= (1 << e);

        res = lex.PeekToken(token, MAX_TOKEN);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_UNEXPECTED_TOKEN,
                "Error parsing '%s': expected '|' or ';', got '%s'",
                name,
                token);
        } else if (!OVR::OVR_strcmp(token, ";")) {
            return ovrParseResult();
        } else if (!OVR::OVR_strcmp(token, "|")) {
            // consume the OR operator
            lex.NextToken(token, MAX_TOKEN);
        }
    }
}

ovrParseResult ParseString(
    ovrReflection& /*refl*/,
    ovrLocale const& locale,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* /*atomicInfo*/,
    void* outPtr,
    size_t const /*arraySize*/) {
    std::string& out = *static_cast<std::string*>(outPtr);
    size_t const MAX_TOKEN = 1024;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
    if (res != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(res, "Error parsing '%s': expected string, got '%s'", name, token);
    }

    // we find the start of the string because it may be preceeded by a format specifier (~~w0,
    // ~~RRGGBBAA, etc.)
    char const* keyPtr = strstr(token, "@string/");
    if (keyPtr != nullptr) {
        intptr_t const keyIndex = keyPtr - token;
        std::string temp;
        locale.GetLocalizedString(keyPtr, keyPtr, temp);
        token[keyIndex] = '\0';
        out += token;
        out += temp;
    } else {
        out = token;
    }
    return ovrParseResult();
}

ovrParseResult ParseIntVector(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* atomicInfo,
    void* outPtr,
    size_t const /*arraySize*/) {
    int* out = static_cast<int*>(outPtr);
    size_t const MAX_TOKEN = 1024;
    char token[MAX_TOKEN];

    ovrParseResult parseRes = ExpectPunctuation(name, lex, "(");
    if (!parseRes) {
        return parseRes;
    }

    const int maxElements = static_cast<int>(atomicInfo->Size / sizeof(int));
    for (int i = 0; i < 4; ++i) {
        if (i >= maxElements) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR, "Error parsing '%s': too many vector elements", name);
        }

        ovrLexer::ovrResult res = lex.ParseInt(out[i], 0);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(res, "Error parsing '%s': expected int", name);
        }

        res = lex.ExpectPunctuation(",)", token, MAX_TOKEN);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(
                res, "Error parsing '%s': expected ',' or '}', got '%s", name, token);
        }
        if (!OVR::OVR_strcmp(token, ")")) {
            break; // end of vector
        }
    }

    return ovrParseResult();
}

ovrParseResult ParseFloatVector(
    ovrReflection& /*refl*/,
    ovrLocale const& /*locale*/,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* atomicInfo,
    void* outPtr,
    size_t const /*arraySize*/) {
    float* out = static_cast<float*>(outPtr);
    size_t const MAX_TOKEN = 1024;
    char token[MAX_TOKEN];

    ovrParseResult parseRes = ExpectPunctuation(name, lex, "(");
    if (!parseRes) {
        return parseRes;
    }

    const int maxElements = static_cast<int>(atomicInfo->Size / sizeof(float));
    for (int i = 0; i < 4; ++i) {
        if (i >= maxElements) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR, "Error parsing '%s': too many vector elements", name);
        }

        ovrLexer::ovrResult res = lex.ParseFloat(out[i], 0.0f);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(res, "Error parsing '%s': expected float", name);
        }

        res = lex.ExpectPunctuation(",)", token, MAX_TOKEN);
        if (res != ovrLexer::LEX_RESULT_OK) {
            return ovrParseResult(
                res, "Error parsing '%s': expected ',' or '}', got '%s", name, token);
        }
        if (!OVR::OVR_strcmp(token, ")")) {
            break; // end of vector
        }
    }

    return ovrParseResult();
}

ovrParseResult::ovrParseResult(ovrLexer::ovrResult const result, char const* fmt, ...)
    : Result(result), Error() {
    char buffer[4096];
    va_list argPtr;
    va_start(argPtr, fmt);
    OVR::OVR_vsprintf(buffer, sizeof(buffer), fmt, argPtr);
    va_end(argPtr);

#if defined(OVR_BUILD_DEBUG)
    if (result != ovrLexer::LEX_RESULT_OK) {
        ALOG("ovrParseResult Error = %s ", &buffer[0]);
    }
#endif

    Error = buffer;
}

static bool IsInteger(char const* token) {
    size_t const len = OVR::OVR_strlen(token);
    for (size_t i = 0; i < len; ++i) {
        if ((i == 0 && token[i] == '-') || (token[i] >= '0' && token[i] <= '9')) {
            continue;
        }
        return false;
    }
    return true;
}

ovrParseResult ParseArray(
    ovrReflection& refl,
    ovrLocale const& locale,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* arrayTypeInfo,
    void* arrayPtr,
    size_t const arraySize) {
    const int MAX_TOKEN = 1024;
    char token[MAX_TOKEN];

    // next token must be either the size of the array or an opening brace
    ovrLexer::ovrResult result = lex.NextToken(token, MAX_TOKEN);
    if (result != ovrLexer::LEX_RESULT_OK) {
        return ovrParseResult(result, "Error parsing '%s'", name);
    }

    int count;
    if (!OVR::OVR_strcmp(token, "{")) {
        // a count of 0 for dynamic arrays means grow as items are added
        count = static_cast<int>(
            (arrayTypeInfo->ArrayType == ovrArrayType::OVR_POINTER ||
             arrayTypeInfo->ArrayType == ovrArrayType::OVR_OBJECT)
                ? 0
                : arraySize);
    } else {
        if (arrayTypeInfo->ArrayType != ovrArrayType::OVR_POINTER &&
            arrayTypeInfo->ArrayType != ovrArrayType::OVR_OBJECT) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR,
                "Error parsing '%s': size of array should not be specified for non-dynamic arrays.",
                name);
        }

        if (!IsInteger(token)) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR, "Error parsing '%s': expected integer", name);
        }
        assert(arrayTypeInfo->ResizeArrayFn != nullptr);
        count = strtol(token, nullptr, 10);
        if (count <= 0) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR,
                "Error parsing '%s': invalid array size %i",
                name,
                count);
        }
        arrayTypeInfo->ResizeArrayFn(arrayPtr, count);

        ovrParseResult parseRes = ExpectPunctuation(name, lex, "{");
        if (!parseRes) {
            return parseRes;
        }
    }

    // in an array, each entry is a type name
    for (int index = 0;; ++index) {
        ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
        if (res == ovrLexer::LEX_RESULT_EOF) {
            return ovrParseResult();
        }
        if (res) {
            return ovrParseResult(res, "Error %d parsing '%s'", name);
        }

        if (!OVR::OVR_strcmp(token, "}")) {
            return ovrParseResult();
        }

        if (index >= count) {
            if (count == 0) {
                // resize the dynamic array
                arrayTypeInfo->ResizeArrayFn(arrayPtr, index + 1);
            } else {
                assert(index < count);
                continue;
            }
        }

        const ovrTypeInfo* elementTypeInfo = refl.FindTypeInfo(token);
        if (elementTypeInfo == nullptr) {
            return ovrParseResult(
                ovrLexer::LEX_RESULT_ERROR,
                "Error %d parsing '%s': Unknown type '%s'",
                name,
                token);
        }

        if (arrayTypeInfo->ArrayType == ovrArrayType::C_OBJECT ||
            arrayTypeInfo->ArrayType == ovrArrayType::C_POINTER) {
            ovrParseResult parseRes = ExpectPunctuation(name, lex, "[");
            if (!parseRes) {
                return parseRes;
            }

            int idx = 0;
            res = lex.ParseInt(idx, 0);
            if (res) {
                return ovrParseResult(
                    res, "Error parsing '%s': expected array index, got '%s'", name, token);
            }

            parseRes = ExpectPunctuation(name, lex, "]");
            if (!parseRes) {
                return parseRes;
            }

            if (idx != index) {
                return ovrParseResult(
                    ovrLexer::LEX_RESULT_ERROR,
                    "Error parsing '%s': expected index %d, got %d",
                    name,
                    index,
                    idx);
            }
        }

        // if the array is not an array of pointers, do a placement new on the stack to avoid heap
        // fragmentation
        void* placementBuffer = nullptr;
        if (arrayTypeInfo->ArrayType != ovrArrayType::OVR_POINTER &&
            arrayTypeInfo->ArrayType != ovrArrayType::C_POINTER) {
            placementBuffer = alloca(elementTypeInfo->Size);
        }
        void* elementPtr = elementTypeInfo->CreateFn(placementBuffer);

        if (elementTypeInfo->MemberInfo != nullptr) {
            ovrParseResult parseRes =
                ParseObject(refl, locale, name, lex, elementTypeInfo, elementPtr, 0);
            if (!parseRes) {
                return parseRes;
            }
        } else {
            assert(elementTypeInfo->ParseFn != nullptr);

            ovrParseResult parseRes = ExpectPunctuation(name, lex, "=");
            if (!parseRes) {
                return parseRes;
            }

            parseRes =
                elementTypeInfo->ParseFn(refl, locale, name, lex, elementTypeInfo, elementPtr, 0);
            if (!parseRes) {
                return parseRes;
            }

            parseRes = ExpectPunctuation(name, lex, ";");
            if (!parseRes) {
                return parseRes;
            }
        }

        // copy to the array
        arrayTypeInfo->SetArrayElementFn(arrayPtr, index, elementPtr);
    }
}

void BuildScope(ovrReflection& refl, ovrTypeInfo const* typeInfo, std::string& scope) {
    ovrTypeInfo const* parentTypeInfo = refl.FindTypeInfo(typeInfo->ParentTypeName);
    if (parentTypeInfo != nullptr) {
        BuildScope(refl, parentTypeInfo, scope);
    }
    if (!scope.empty()) {
        scope += "::";
    }
    scope += typeInfo->TypeName;
}

ovrReflectionOverload const* ovrReflection::FindOverload(char const* scope) const {
    for (ovrReflectionOverload const* o : Overloads) {
        if (OVR::OVR_strcmp(o->GetScope(), scope) == 0) {
            return o;
        }
    }
    return nullptr;
}

ovrParseResult ParseObject(
    ovrReflection& refl,
    ovrLocale const& locale,
    const char* name,
    ovrLexer& lex,
    ovrTypeInfo const* objectTypeInfo,
    void* objPtr,
    const size_t /*arraySize*/) {
    std::string scope;
    BuildScope(refl, objectTypeInfo, scope);
    ovrReflectionOverload const* o = refl.FindOverload(scope.c_str());
    if (o != nullptr && o->OverloadsMemberVar()) {
        ovrMemberInfo const* overloadedMemberVar =
            refl.FindMemberReflectionInfo(objectTypeInfo->MemberInfo, o->GetName());
        if (overloadedMemberVar != nullptr) {
            uint8_t* memberPtr = static_cast<uint8_t*>(objPtr) + overloadedMemberVar->Offset;
            switch (o->GetType()) {
                case ovrReflectionOverload::OVERLOAD_FLOAT_DEFAULT_VALUE:
                    *reinterpret_cast<float*>(memberPtr) =
                        static_cast<ovrReflectionOverload_FloatDefaultValue const*>(o)->GetValue();
                    break;
                default:
                    assert(false); // unhandled overload type
                    break;
            }
        }
    }

    const int MAX_TOKEN = 1024;
    char token[MAX_TOKEN];

    ovrLexer::ovrResult result = lex.ExpectPunctuation("{", token, MAX_TOKEN);
    if (result) {
        return ovrParseResult(result, "Error parsing '%s': Expected '{', got '%s'", name, token);
    }

    // in an object, each entry is a member variable name
    for (;;) {
        ovrLexer::ovrResult res = lex.NextToken(token, MAX_TOKEN);
        if (res == ovrLexer::LEX_RESULT_EOF) {
            return ovrParseResult();
        }
        if (res) {
            return ovrParseResult(res, "Error %d parsing '%s'", name);
        }

        if (!OVR::OVR_strcmp(token, "}")) {
            break;
        }

        ovrMemberInfo const* memberInfo =
            refl.FindMemberReflectionInfoRecursive(objectTypeInfo, token);
        if (memberInfo == nullptr) {
            assert(memberInfo != nullptr);
            return ovrParseResult(res, "Error parsing '%s': Unknown member '%s", name, token);
        }

        void* memberPtr = static_cast<char*>(objPtr) + memberInfo->Offset;

        ovrTypeInfo const* memberTypeInfo = refl.FindTypeInfo(memberInfo->TypeName);
        if (memberTypeInfo == nullptr) {
            assert(memberTypeInfo != nullptr);
            return ovrParseResult(
                res, "Error parsing '%s': Unknown type '%s'", name, memberInfo->TypeName);
        }

        if (memberTypeInfo->ParseFn != nullptr) // if we have a special-case parse function, use it
        {
            assert(memberTypeInfo->ParseFn != nullptr);

            if (memberInfo->Operator != ovrTypeOperator::ARRAY) {
                ovrParseResult parseRes = ExpectPunctuation(name, lex, "=");
                if (!parseRes) {
                    return parseRes;
                }
            }

            ovrParseResult parseRes = memberTypeInfo->ParseFn(
                refl, locale, name, lex, memberTypeInfo, memberPtr, memberInfo->ArraySize);
            if (!parseRes) {
                return parseRes;
            }

            if (memberInfo->Operator != ovrTypeOperator::ARRAY) {
                parseRes = ExpectPunctuation(name, lex, ";");
                if (!parseRes) {
                    return parseRes;
                }
            }
        } else // otherwise, this must be an object
        {
            assert(memberTypeInfo->MemberInfo != nullptr);

            ovrParseResult parseRes =
                ParseObject(refl, locale, name, lex, memberTypeInfo, memberPtr, 0);
            if (!parseRes) {
                return parseRes;
            }
        }
    }

    return ovrParseResult();
}

//=============================================================================================
// ovrReflection
//=============================================================================================

ovrReflection* ovrReflection::Create() {
    ovrReflection* r = new ovrReflection();
    if (r != nullptr) {
        r->Init();
    }
    return r;
}

void ovrReflection::Destroy(ovrReflection*& r) {
    if (r != nullptr) {
        r->Shutdown();
    }
    delete r;
    r = nullptr;
}

void ovrReflection::Init() {
    AddTypeInfoList(TypeInfoList);
}

void ovrReflection::Shutdown() {
    for (int i = 0; i < static_cast<int>(Overloads.size()); ++i) {
        delete Overloads[i];
        Overloads[i] = nullptr;
    }
    Overloads.clear();
}

void ovrReflection::AddTypeInfoList(ovrTypeInfo const* list) {
    TypeInfoLists.push_back(list);
}

ovrMemberInfo const* ovrReflection::FindMemberReflectionInfoRecursive(
    ovrTypeInfo const* objectTypeInfo,
    const char* memberName) {
    ovrMemberInfo const* arrayOfMemberType = objectTypeInfo->MemberInfo;
    for (int i = 0; arrayOfMemberType[i].MemberName != nullptr; ++i) {
        if (!OVR::OVR_strcmp(arrayOfMemberType[i].MemberName, memberName)) {
            return &arrayOfMemberType[i];
        }
    }

    if (objectTypeInfo->ParentTypeName == nullptr) {
        return nullptr;
    }
    const ovrTypeInfo* parentTypeInfo = FindTypeInfo(objectTypeInfo->ParentTypeName);
    if (parentTypeInfo == nullptr) {
        return nullptr;
    }

    return FindMemberReflectionInfoRecursive(parentTypeInfo, memberName);
}

ovrMemberInfo const* ovrReflection::FindMemberReflectionInfo(
    ovrMemberInfo const* arrayOfMemberType,
    const char* memberName) {
    for (int i = 0; arrayOfMemberType[i].MemberName != nullptr; ++i) {
        if (!OVR::OVR_strcmp(arrayOfMemberType[i].MemberName, memberName)) {
            return &arrayOfMemberType[i];
        }
    }
    return nullptr;
}

ovrTypeInfo const* ovrReflection::FindTypeInfo(char const* typeName) {
    assert(TypeInfoLists.size() > 0);
    if (typeName == nullptr || typeName[0] == '\0') {
        return nullptr;
    }

    for (int i = 0; i < static_cast<int>(TypeInfoLists.size()); ++i) {
        // ALOG( "FindTypeInfo searching for %s ...", typeName );
        ovrTypeInfo const* ti = StaticFindTypeInfo(TypeInfoLists[i], typeName);
        if (ti != nullptr) {
            return ti;
        }
    }
    ALOG("FindTypeInfo for '%s' could not be found! ERROR", typeName);
    assert(false);
    return nullptr;
}

ovrTypeInfo const* ovrReflection::StaticFindTypeInfo(
    ovrTypeInfo const* list,
    char const* typeName) {
    if (typeName == nullptr || typeName[0] == '\0') {
        return nullptr;
    }

    for (int i = 0; list[i].TypeName != nullptr; ++i) {
        if (!OVR::OVR_strcmp(list[i].TypeName, typeName)) {
            return &list[i];
        }
    }
    return nullptr;
}

} // namespace OVRFW
