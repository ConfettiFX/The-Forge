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

#include "LuaManagerImpl.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Application/Interfaces/ICameraController.h"
#include "../../Utilities/Interfaces/IFileSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

const char LuaManagerImpl::className[] = "LuaManager";
bool       LuaManagerImpl::m_registered = false;

void LogError(lua_State* lstate, const char* msg)
{
    // retrieve line number for error info
    lua_Debug ar;
    lua_getstack(lstate, 1, &ar);
    lua_getinfo(lstate, "nSl", &ar);
    int line = ar.currentline;

    LOGF(eERROR, "%s\n\tLine: %d", msg, line);
}

int LuaSleep(lua_State* lstate)
{
    luaL_checkinteger(lstate, 1);
    int sleepTime = (int)lua_tointeger(lstate, 1);
    threadSleep(sleepTime);
    return 0;
}

luaL_Reg conffLib[] = { { "sleep", LuaSleep }, { NULL, NULL } };

int luaopen_conffLib(lua_State* L)
{
    luaL_newlib(L, conffLib);
    return 1;
}

Luna<LuaManagerImpl>::FunctionType LuaManagerImpl::methods[] = { { NULL, NULL } };

Luna<LuaManagerImpl>::PropertyType LuaManagerImpl::properties[] = { { NULL, NULL } };

LuaManagerImpl::LuaManagerImpl(lua_State* L):
    m_UpdatableScriptLuaState(nullptr), m_SyncLuaState(nullptr), m_AsyncLuaStates(), m_AsyncLuaStatesMutex(), m_AddAsyncScriptMutex(),
    m_Functions(nullptr), m_UpdateFunctionNameBuf(), m_UpdatableScriptExitBuf(), m_UpdatableScriptFile(nullptr), m_AsyncScriptsCounter(0)
{
    UNREF_PARAM(L);
    memset(m_AsyncLuaStates, 0, MAX_LUA_WORKERS * sizeof(lua_State*));
}

LuaManagerImpl::LuaManagerImpl():
    m_UpdatableScriptLuaState(nullptr), m_SyncLuaState(nullptr), m_AsyncLuaStates(), m_UpdatableScriptFile(nullptr),
    m_AsyncScriptsCounter(0)
{
    Register();

    for (uint32_t i = 0; i < MAX_LUA_WORKERS; ++i)
        initMutex(&m_AsyncLuaStatesMutex[i]);
    initMutex(&m_AddAsyncScriptMutex);
}

LuaManagerImpl::~LuaManagerImpl()
{
    DestroyLuaState(m_SyncLuaState);
    m_SyncLuaState = nullptr;

    if (m_UpdatableScriptLuaState != nullptr)
    {
        if (m_UpdatableScriptExitName.slen != 0)
            ExitScript(m_UpdatableScriptLuaState, (const char*)m_UpdatableScriptExitName.data);
        DestroyLuaState(m_UpdatableScriptLuaState);
        m_UpdatableScriptLuaState = nullptr;
    }

    for (int i = 0; i < MAX_LUA_WORKERS; ++i)
    {
        DestroyLuaState(m_AsyncLuaStates[i]);
        m_AsyncLuaStates[i] = nullptr;
    }

    for (ptrdiff_t i = 0; i < arrlen(m_Functions); ++i)
    {
        m_Functions[i]->~ILuaFunctionWrap();
        tf_free(m_Functions[i]);
    }
    arrfree(m_Functions);

    for (uint32_t i = 0; i < MAX_LUA_WORKERS; ++i)
        destroyMutex(&m_AsyncLuaStatesMutex[i]);
    destroyMutex(&m_AddAsyncScriptMutex);

    m_registered = false;
    bdestroy(&m_UpdatableScriptExitName);
    bdestroy(&m_UpdateFunctionName);
}

void LuaManagerImpl::DestroyLuaState(lua_State* state)
{
    if (state)
    {
        // remove self from lua state to not be deleted by garbage collector
        lua_getglobal(state, "loader");
        lua_pushnil(state);
        lua_setmetatable(state, -2);

        lua_close(state);
    }
}

// Message handler used to run all chunks
static int msghandler(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL)
    {                                            /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
            return 1;                            /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    LOGF(eERROR, "Script error: %s\n", msg);
#ifdef AUTOMATED_TESTING
    ASSERTFAIL("Lua script error encountered during automated testing");
#endif
    return 1; /* return the traceback */
}

const char* luaReaderFunction(lua_State* L, void* ud, size_t* sz)
{
    UNREF_PARAM(L);
    FileStream*  pHandle = (FileStream*)ud;
    const size_t size = 1024;
    static char  buffer[size];
    //*sz = read_file((void*)buffer, size, pHandle);
    *sz = fsReadFromStream(pHandle, buffer, size);
    return buffer;
}

bool RunScriptFile(const char* scriptFile, lua_State* L)
{
    // int loadfile_error = luaL_loadfile(L, scriptFile);
    lua_Reader reader = luaReaderFunction;

    FileStream fh = {};

    if (!fsOpenStreamFromPath(RD_SCRIPTS, scriptFile, FM_READ, &fh))
    {
#ifdef AUTOMATED_TESTING
        ASSERTFAIL("Can't load script %s\n", scriptFile);
#endif
        return false;
    }

    int loadfile_error = lua_load(L, reader, &fh, NULL, NULL);
    fsCloseStream(&fh);
    if (loadfile_error != 0)
    {
        LOGF(eERROR, "Can't load script %s\n", scriptFile);
        return false;
    }

    int status;
    int narg = 0;
    int nres = 0;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base);              /* put it under function and args */
    status = lua_pcall(L, narg, nres, base);
    // signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base); /* remove message handler from the stack */
    return status == 0;
}

void LuaManagerImpl::ExitScript(lua_State* state, const char* exitFunctionName)
{
    int narg = 0;
    int nres = 0;
    int base = lua_gettop(state) - narg; /* function index */

    lua_getglobal(state, exitFunctionName);
    if (lua_isfunction(state, -1))
    {
        lua_pcall(state, narg, nres, base);
    }
}

bool LuaManagerImpl::SetUpdatableScript(const char* scriptFile, const char* updateFunctionName, const char* exitFunctionName)
{
    if (m_UpdatableScriptLuaState != nullptr)
    {
        if (m_UpdatableScriptExitName.slen != 0)
            ExitScript(m_UpdatableScriptLuaState, (const char*)m_UpdatableScriptExitName.data);
        DestroyLuaState(m_UpdatableScriptLuaState);
    }

    m_UpdatableScriptLuaState = CreateLuaState();
    RegisterLuaManagerForLuaState(m_UpdatableScriptLuaState);
    RegisterFunctionsForState(m_UpdatableScriptLuaState);

    bassigncstr(&m_UpdateFunctionName, updateFunctionName);
    ASSERT(!bownsdata(&m_UpdateFunctionName));

    bassigncstr(&m_UpdatableScriptExitName, exitFunctionName);
    ASSERT(!bownsdata(&m_UpdatableScriptExitName));

    m_UpdatableScriptFile = scriptFile;

    // int loadfile_error = luaL_loadfile(m_UpdatableScriptLuaState, m_UpdatableScriptName.c_str());
    lua_Reader reader = luaReaderFunction;
    // int loadfile_error = lua_load(m_UpdatableScriptLuaState, reader, open_file(scriptFile, "rb"), NULL, NULL);

    FileStream fHandle = {};
    if (!fsOpenStreamFromPath(RD_SCRIPTS, scriptFile, FM_READ, &fHandle))
    {
        return false;
    }

    int loadfile_error = lua_load(m_UpdatableScriptLuaState, reader, &fHandle, NULL, NULL);
    fsCloseStream(&fHandle);

    if (loadfile_error != 0)
    {
        LOGF(eERROR, "Can't load script %s\n", scriptFile);
        return false;
    }
    int narg = 0;
    int nres = 0;
    int base = lua_gettop(m_UpdatableScriptLuaState) - narg;  /* function index */
    lua_pushcfunction(m_UpdatableScriptLuaState, msghandler); /* push message handler */
    lua_insert(m_UpdatableScriptLuaState, base);              /* put it under function and args */
    int status = lua_pcall(m_UpdatableScriptLuaState, narg, nres, base);
    return status == 0;
}

bool LuaManagerImpl::ReloadUpdatableScript()
{
    ASSERT(m_UpdatableScriptFile);
    return SetUpdatableScript(m_UpdatableScriptFile, (const char*)m_UpdateFunctionName.data, (const char*)m_UpdatableScriptExitName.data);
}

void LuaManagerImpl::RegisterFunctionsForState(lua_State* state)
{
    for (ptrdiff_t i = 0; i < arrlen(m_Functions); ++i)
    {
        Luna<LuaManagerImpl>::RegisterMethod(state, (const char*)m_Functions[i]->functionName.data, (int)i);
    }
}

bool LuaManagerImpl::Update(float deltaTime, const char* updateFunctionName)
{
    int narg = 1; // we are going to push "deltaTime"
    int nres = 0;
    int base = lua_gettop(m_UpdatableScriptLuaState) - narg; /* function index */

    // push function to stack
    if (updateFunctionName != nullptr)
        lua_getglobal(m_UpdatableScriptLuaState, updateFunctionName); /* function to be called */
    else
    {
        ASSERT(m_UpdateFunctionName.slen != 0);
        lua_getglobal(m_UpdatableScriptLuaState, (const char*)m_UpdateFunctionName.data);
    }
    if (lua_isfunction(m_UpdatableScriptLuaState, -1))
    {
        lua_pushnumber(m_UpdatableScriptLuaState, deltaTime);
        int status = lua_pcall(m_UpdatableScriptLuaState, narg, nres, base);
        return status == 0;
    }
    return false;
}

bool LuaManagerImpl::RunScript(const char* scriptFile)
{
    bool status = RunScriptFile(scriptFile, m_SyncLuaState);
    return !status; // Compatibility: If the script fails, RunScript (this function) returns true
}

void AsyncScriptExecute(void* pData)
{
    ASSERT(pData != nullptr);
    ScriptTaskInfo* info = (ScriptTaskInfo*)pData;
    MutexLock       lock(*(info->mutex));
    int             status = RunScriptFile(info->scriptFile, info->luaState);
    if (info->callback)
    {
        info->callback(status == 0 ? FINISHED_OK : FINISHED_ERROR);
    }
    if (info->callbackLambda)
    {
        info->callbackLambda->ExecuteCallback(status == 0 ? FINISHED_OK : FINISHED_ERROR);
        info->callbackLambda->~IScriptCallbackWrap();
        tf_free(info->callbackLambda);
    }
    info->~ScriptTaskInfo();        // call destructors of non-trivial members of the struct
    memset(info, 0, sizeof(*info)); // set zeros just in case if someone will try to use released chunk of memory
    tf_free(info);
}

void LuaManagerImpl::AddAsyncScript(const char* scriptFile, IScriptCallbackWrap* callbackLambda)
{
    ScriptTaskInfo* info = (ScriptTaskInfo*)tf_calloc(1, sizeof(ScriptTaskInfo));
    info->scriptFile = scriptFile;
    info->callback = nullptr;

    info->callbackLambda = callbackLambda;

    // ThreadDesc* pItem = (ThreadDesc*)tf_calloc(1, sizeof(*pItem));

    // pItem->pFunc = AsyncScriptExecute;
    // pItem->pData = info;

    {
        MutexLock lock(m_AddAsyncScriptMutex);
        info->luaState = m_AsyncLuaStates[m_AsyncScriptsCounter % MAX_LUA_WORKERS];
        info->mutex = &m_AsyncLuaStatesMutex[m_AsyncScriptsCounter % MAX_LUA_WORKERS];
        ++m_AsyncScriptsCounter;

        // for now we run scripts in synchronous way
        // m_ThreadPool->AddWorkItem(pItem);
        AsyncScriptExecute(info);
    }
}

void LuaManagerImpl::AddAsyncScript(const char* scriptFile, ScriptDoneCallback callback)
{
    ScriptTaskInfo* info = (ScriptTaskInfo*)tf_calloc(1, sizeof(ScriptTaskInfo));
    info->scriptFile = scriptFile;
    info->callback = callback;
    info->callbackLambda = nullptr;
    // ThreadDesc* pItem = (ThreadDesc*)tf_calloc(1, sizeof(*pItem));

    // pItem->pFunc = AsyncScriptExecute;
    // pItem->pData = info;

    {
        MutexLock lock(m_AddAsyncScriptMutex);
        info->luaState = m_AsyncLuaStates[m_AsyncScriptsCounter % MAX_LUA_WORKERS];
        info->mutex = &m_AsyncLuaStatesMutex[m_AsyncScriptsCounter % MAX_LUA_WORKERS];
        ++m_AsyncScriptsCounter;
        // m_ThreadPool->AddWorkItem(pItem);
        AsyncScriptExecute(info);
    }
}

void LuaManagerImpl::AddAsyncScript(const char* scriptFile)
{
    ScriptDoneCallback cb = nullptr;
    AddAsyncScript(scriptFile, cb);
}

void LuaManagerImpl::SetFunction(ILuaFunctionWrap* wrap)
{
    // 1. Check if function is already registered
    // Since this shouldn't be called often then just
    // use string compare. We can implement more fast search if needed
    for (ptrdiff_t i = 0; i < arrlen(m_Functions); ++i)
    {
        if (biseq(&m_Functions[i]->functionName, &wrap->functionName))
        {
            m_Functions[i]->~ILuaFunctionWrap();
            tf_free(m_Functions[i]);
            m_Functions[i] = wrap;
            return;
        }
    }
    // 2.
    arrpush(m_Functions, wrap);
    int         id = (int)(arrlen(m_Functions) - 1);
    const char* pName = (const char*)m_Functions[id]->functionName.data; //-V1004

    Luna<LuaManagerImpl>::RegisterMethod(m_SyncLuaState, pName, id);
    // m_UpdatableScriptLuaState is created in LuaManagerImpl::SetUpdatableScript() so it may not exist here.
    // When LuaManagerImpl::SetUpdatableScript() is invoked all these functions will be registered in new state.
    if (m_UpdatableScriptLuaState != nullptr)
        Luna<LuaManagerImpl>::RegisterMethod(m_UpdatableScriptLuaState, pName, id);
    for (int i = 0; i < MAX_LUA_WORKERS; ++i)
    {
        MutexLock lock(m_AsyncLuaStatesMutex[i]);
        Luna<LuaManagerImpl>::RegisterMethod(m_AsyncLuaStates[i], pName, id);
    }
}

// allocate and free function. Used in lua_newstate and in lua_close
static void* tf_l_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize; /* not used */
    if (nsize == 0)
    {
        tf_free(ptr);
        return NULL;
    }
    else
        return tf_realloc(ptr, nsize);
}

static int l_panic(lua_State* L)
{
    lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));
    return 0; /* return to Lua to abort */
}

void LuaManagerImpl::Register()
{
    // Only one instance of this class is allowed
    ASSERT(m_registered == false);

    // create Lua states
    m_SyncLuaState = CreateLuaState();
    RegisterLuaManagerForLuaState(m_SyncLuaState);

    for (int i = 0; i < MAX_LUA_WORKERS; ++i)
    {
        m_AsyncLuaStates[i] = CreateLuaState();
        RegisterLuaManagerForLuaState(m_AsyncLuaStates[i]);
    }

    m_registered = true;
}

lua_State* LuaManagerImpl::CreateLuaState()
{
    lua_State* lstate = lua_newstate(tf_l_alloc, NULL);
    if (lstate)
        lua_atpanic(lstate, &l_panic);
    luaL_openlibs(lstate);
    luaopen_debug(lstate);
    return lstate;
}

void LuaManagerImpl::RegisterLuaManagerForLuaState(lua_State* state)
{
    Luna<LuaManagerImpl>::Register(state);

    luaL_requiref(state, "conffLib", luaopen_conffLib, 1);
    lua_pop(state, 1); /* remove lib */

    Luna<LuaManagerImpl>::push(state, this);
    lua_setglobal(state, "loader");
}

int LuaManagerImpl::FunctionDispatch(int functionIndex, lua_State* state)
{
    LuaStateWrap stateWrap;
    stateWrap.luaState = state;
    ASSERT(arrlen(m_Functions) > functionIndex);
    if (arrlen(m_Functions) > functionIndex)
    {
        return m_Functions[functionIndex]->ExecuteFunction(&stateWrap);
    }
    return 0;
}

int LuaStateWrap::GetArgumentsCount() { return Luna<LuaManagerImpl>::GetArgCount(luaState); }

double LuaStateWrap::GetNumberArg(int argIdx) { return lua_tonumber(luaState, argIdx); }

long long int LuaStateWrap::GetIntegerArg(int argIdx) { return lua_tointeger(luaState, argIdx); }

void LuaStateWrap::GetStringArg(int argIdx, const char** result) { *result = lua_tostring(luaState, argIdx); }

void LuaStateWrap::GetStringArrayArg(int argIdx, const char** outResult, int* size)
{
    ASSERT(size != NULL);
    int arraySize = (int)luaL_len(luaState, argIdx); // get array size
    if (outResult == NULL)
    {
        *size = arraySize;
        return;
    }

    arraySize = arraySize <= *size ? arraySize : *size;

    for (int i = 1; i <= arraySize; ++i)
    {
        lua_geti(luaState, 1, i);
        outResult[i - 1] = lua_tostring(luaState, -1);
        lua_pop(luaState, 1);
    }
}

void LuaStateWrap::PushResultNumber(double d) { lua_pushnumber(luaState, d); }

void LuaStateWrap::PushResultInteger(int i) { lua_pushinteger(luaState, i); }

void LuaStateWrap::PushResultString(const char* s) { lua_pushstring(luaState, s); }
