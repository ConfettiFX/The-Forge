/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include "../../Application/Config.h"

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"

#define MAX_FUNCTION_NAME_LENGTH 128

enum ScriptState
{
    FINISHED_OK,
    FINISHED_ERROR,
};

typedef void (*ScriptDoneCallback)(ScriptState state);

struct ILuaStateWrap
{
    virtual int           GetArgumentsCount() = 0;
    virtual double        GetNumberArg(int argIdx) = 0;
    virtual long long int GetIntegerArg(int argIdx) = 0;
    virtual void          GetStringArg(int argIdx, const char** result) = 0;
    /*
     * if outResult is NULL
     *   number of elements in the array is returned in size
     * otherwise fills outResult with up to *size elements
     *
     * Note: Internally pops lua state up to *size times
     */
    virtual void          GetStringArrayArg(int argIdx, const char** outResult, int* size) = 0;

    virtual void PushResultNumber(double d) = 0;
    virtual void PushResultInteger(int i) = 0;
    virtual void PushResultString(const char* s) = 0;
};

struct ILuaFunctionWrap
{
    ILuaFunctionWrap(const char* functionName)
    {
        bassigncstr(&this->functionName, functionName);
        ASSERT(!bownsdata(&this->functionName));
    };
    virtual ~ILuaFunctionWrap() {}
    virtual int ExecuteFunction(ILuaStateWrap* luaState)
    {
        UNREF_PARAM(luaState);
        return 0;
    };

    unsigned char fnNameBuf[MAX_FUNCTION_NAME_LENGTH] = {};
    bstring       functionName = bemptyfromarr(fnNameBuf);
};

template<class T>
struct LuaFunctionWrap: public ILuaFunctionWrap
{
    LuaFunctionWrap(T& function, const char* functionName): ILuaFunctionWrap(functionName), m_function(function) {}
    virtual ~LuaFunctionWrap() = default;
    virtual int ExecuteFunction(ILuaStateWrap* luaState) override { return m_function(luaState); }

    T m_function;
};

class IScriptCallbackWrap
{
public:
    virtual ~IScriptCallbackWrap() {}
    virtual void ExecuteCallback(ScriptState resultState) = 0;
};

template<class T>
class ScriptCallbackWrap: public IScriptCallbackWrap
{
public:
    ScriptCallbackWrap(T& callback): m_callback(callback) {}
    virtual ~ScriptCallbackWrap() = default;
    virtual void ExecuteCallback(ScriptState resultState) override { m_callback(resultState); }

private:
    T m_callback;
};
