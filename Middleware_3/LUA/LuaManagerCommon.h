#pragma once

#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

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
	virtual eastl::string GetStringArg(int argIdx) = 0;
	virtual void          GetStringArrayArg(int argIdx, eastl::vector<const char*>& outResult) = 0;

	virtual void PushResultNumber(double d) = 0;
	virtual void PushResultInteger(int i) = 0;
	virtual void PushResultString(const char* s) = 0;
};

struct ILuaFunctionWrap
{
	ILuaFunctionWrap(const char* functionName): functionName(functionName){};
	virtual ~ILuaFunctionWrap() {}
	virtual int ExecuteFunction(ILuaStateWrap* luaState) { return 0; };

	eastl::string functionName;
};

template <class T>
struct LuaFunctionWrap: public ILuaFunctionWrap
{
	LuaFunctionWrap(T& function, const char* functionName): ILuaFunctionWrap(functionName), m_function(function) {}
	virtual ~LuaFunctionWrap(){};
	virtual int ExecuteFunction(ILuaStateWrap* luaState) override { return m_function(luaState); }

	T m_function;
};

class IScriptCallbackWrap
{
	public:
	virtual ~IScriptCallbackWrap() {}
	virtual void ExecuteCallback(ScriptState resultState) = 0;
};

template <class T>
class ScriptCallbackWrap: public IScriptCallbackWrap
{
	public:
	ScriptCallbackWrap(T& callback): m_callback(callback) {}
	virtual ~ScriptCallbackWrap(){};
	virtual void ExecuteCallback(ScriptState resultState) override { m_callback(resultState); }

	private:
	T m_callback;
};
