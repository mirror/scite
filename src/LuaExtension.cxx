// SciTE - Scintilla based Text Editor
// LuaExtension.cxx - Lua scripting extension
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "Scintilla.h"
#include "Accessor.h"
#include "Extender.h"
#include "LuaExtension.h"

#include "SString.h"
#include "SciTEKeys.h"
#include "IFaceTable.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "Platform.h"

#if PLAT_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _MSC_VER
// MSVC looks deeper into the code than other compilers, sees that 
// lua_error calls longjmp, and complains about unreachable code.
#pragma warning(disable: 4702)
#endif

#else

#include <limits.h>
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#else
#define MAX_PATH 260
#endif

#endif


// A note on naming conventions:
// I've gone back and forth on this a bit, trying different styles.
// It isn't easy to get something that feels consistent, considering
// that the Lua API uses lower case, underscore-separated words and
// Scintilla of course uses mixed case with no underscores.

// What I've settled on is that functions that require you to think
// about the Lua stack are likely to be mixed with Lua API functions,
// so these should using a naming convention similar to Lua itself.
// Functions that don't manipulate Lua at a low level should follow
// the normal SciTE convention.  There is some grey area of course,
// and for these I just make a judgement call


static ExtensionAPI *host = 0;
static lua_State *luaState = 0;
static bool luaDisabled = false;

static char *startupScript = NULL;

LuaExtension::LuaExtension() {}

LuaExtension::~LuaExtension() {}

LuaExtension &LuaExtension::Instance() {
	static LuaExtension singleton;
	return singleton;
}

// I want to be able to get at easily.
static lua_CFunction lualib_loadstring = 0;
static lua_CFunction lualib_string_find = 0;

static int cf_getglobal_wrapper(lua_State *L) {
	if (lua_gettop(L) == 1) {
		lua_gettable(L, LUA_GLOBALSINDEX);
		return 1;
	}
	return 0;
}

static bool safe_getglobal(lua_State *L) {
	lua_pushcfunction(L, cf_getglobal_wrapper);
	lua_insert(L, -2);
	if (0==lua_pcall(L, 1, 1, 0)) {
		return true;
	} else {
		lua_pop(L, 1);
		lua_pushnil(L);
		return false;
	}
}


// Find the pane (self object) at the given index (usually 1), and if
// it is not found there, put the default pane at that index, so that
// the other arguments will be shifted.


static ExtensionAPI::Pane CheckPaneSelfArg(lua_State *L) {
	ExtensionAPI::Pane p = ExtensionAPI::paneEditor;

	bool found = false;

	if (lua_isuserdata(L, 1)) {
		void *pdest = lua_unboxpointer(L, 1);
		if (pdest == reinterpret_cast<void *>(ExtensionAPI::paneEditor)) {
			p = ExtensionAPI::paneEditor;
			found = true;
		} else if (pdest == reinterpret_cast<void *>(ExtensionAPI::paneOutput)) {
			p = ExtensionAPI::paneOutput;
			found = true;
		}
	}

	if (!found) {
		lua_pushstring(L, "Lua: Self argument is missing in pane method.  Use ':' rather than '.'");
		lua_error(L);
	}

	return p;
}

static int cf_pane_textrange(lua_State *L) {
	ExtensionAPI::Pane p = CheckPaneSelfArg(L);

	if (lua_gettop(L) >= 3) {
		int cpMin = static_cast<int>(luaL_checknumber(L, 2));
		int cpMax = static_cast<int>(luaL_checknumber(L, 3));

		char *range = host->Range(p, cpMin, cpMax);
		if (range) {
			lua_pushstring(L, range);
			delete []range;
			return 1;
		}
	} else {
		lua_pushstring(L, "Lua: not enough arguments for <pane>:textrange");
		lua_error(L);
	}

	return 0;
}

static int cf_pane_insert(lua_State *L) {
	ExtensionAPI::Pane p = CheckPaneSelfArg(L);
	int pos = luaL_checkint(L, 2);
	const char *s = luaL_checkstring(L, 3);
	if (s) {
		host->Insert(p, pos, s);
	}
	return 0;
}

static int cf_pane_remove(lua_State *L) {
	ExtensionAPI::Pane p = CheckPaneSelfArg(L);
	if (lua_gettop(L) >= 3) {
		int cpMin = static_cast<int>(luaL_checknumber(L, 2));
		int cpMax = static_cast<int>(luaL_checknumber(L, 3));
		host->Remove(p, cpMin, cpMax);
	}
	return 0;
}

static int cf_pane_findtext(lua_State *L) {
    ExtensionAPI::Pane p = CheckPaneSelfArg(L);

	int nArgs = lua_gettop(L);

    const char *t = luaL_checkstring(L, 2);
	bool hasError = (!t);

	if (!hasError) {
		TextToFind ft = {{0, 0}, 0, {0, 0}};

		ft.lpstrText = const_cast<char*>(t);

		int flags = (nArgs > 2) ? luaL_checkint(L, 3) : 0;
		hasError = (flags == 0 && lua_gettop(L) > nArgs);

		if (!hasError) {
			if (nArgs > 3) {
				ft.chrg.cpMin = static_cast<int>(luaL_checkint(L,4));
				hasError = (lua_gettop(L) > nArgs);
			}
		}

		if (!hasError) {
			if (nArgs > 4) {
				ft.chrg.cpMax = static_cast<int>(luaL_checkint(L,5));
				hasError = (lua_gettop(L) > nArgs);
			} else {
				ft.chrg.cpMax = host->Send(p, SCI_GETLENGTH, 0, 0);
			}
		}

		if (!hasError) {
			int result = host->Send(p, SCI_FINDTEXT, reinterpret_cast<uptr_t&>(flags), reinterpret_cast<sptr_t>(&ft));
			if (result >= 0) {
				lua_pushnumber(L, ft.chrgText.cpMin);
				lua_pushnumber(L, ft.chrgText.cpMax);
				return 2;
			} else {
				lua_pushnumber(L, -1);
				return 1;
			}
		}
	}
	
	if (hasError) {
		lua_pushstring(L, "Lua: invalid arguments for <pane>:findtext");
		lua_error(L);
	}

	return 0;
}

static int cf_global_trace(lua_State *L) {
	const char *s = lua_tostring(L,1);
	if (s) {
		host->Trace(s);
	}
	return 0;
}

static int cf_props_metatable_index(lua_State *L) {
	int selfArg = lua_isuserdata(L, 1) ? 1 : 0;
	
	if (lua_isstring(L, selfArg + 1)) {
		char *value = host->Property(lua_tostring(L, selfArg + 1));
		if (value) {
			lua_pushstring(L, value);
			delete []value;
			return 1;
		} else {
			lua_pushstring(L, "");
			return 1;
		}
	} else {
		lua_pushstring(L, "Lua: string argument required for property access");
		lua_error(L);
	}
	return 0;
}

static int cf_props_metatable_newindex(lua_State *L) {
	int selfArg = lua_isuserdata(L, 1) ? 1 : 0;

	const char *key = lua_isstring(L, selfArg + 1) ? lua_tostring(L, selfArg + 1) : 0;
	const char *val = luaL_checkstring(L, selfArg + 2);
	
	if (key && *key && val) {
		host->SetProperty(key, val);
	} else {
		lua_pushstring(L, "Lua: property name and value must be strings");
		lua_error(L);
	}
	return 0;
}

static int cf_global_alert(lua_State *L) {
	if (host) {
		SString msg = lua_tostring(L, 1);
		if (msg.substitute("\n", "\n> ")) {
			msg.substitute("\r\n", "\n");
		}
		msg.insert(0, "> ");
		msg.append("\n");
		
		host->Trace(msg.c_str());
	}
	return 0;
}


static int cf_global_dostring(lua_State *L) {
	if (host) {
		if (lualib_loadstring) {
			lua_pushcfunction(L, lualib_loadstring);
			lua_insert(L, 1);
			lua_call(L, lua_gettop(L)-1, 2);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_call(L, 0, 0);
			} else {
				lua_remove(L, -2);
				lua_error(L);
			}
		} else {
			lua_pushstring(L, "Lua internal error: missing required library function loadstring");
			lua_error(L);
		}
	}
	return 0;
}

static void call_alert(lua_State *L, const char *s = NULL) {
	lua_pushstring(L, "_ALERT");
	if (!safe_getglobal(L) || !lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		lua_pushcfunction(L, cf_global_alert);
	}
	if (s) {
		lua_pushstring(L, s);
	} else {
		lua_insert(L, -2);
	}
	lua_pcall(L, 1, 0, 0);
}


static bool call_function(lua_State *L, int nargs, bool ignoreFunctionReturnValue=false) {
	bool handled = false;

	if (L) {
		int result = lua_pcall(L, nargs, ignoreFunctionReturnValue ? 0 : 1, 0);
		if (0 == result) {
			if (ignoreFunctionReturnValue) {
				handled = true;
			} else {
				handled = (0 != lua_toboolean(L, -1));
				lua_pop(L, 1);
			}
		} else if (result == LUA_ERRRUN) {
			call_alert(L); // use pushed error message
		} else {
			lua_pop(L, 1);
			if (result == LUA_ERRMEM) {
				host->Trace("> Lua: memory allocation error\n");
			} else if (result == LUA_ERRERR) {
				host->Trace("> Lua: error function failed\n");
			} else {
				host->Trace("> Lua: unexpected error\n");
			}
		}
	}
	return handled;
}

static bool CallNamedFunction(char *name) {
	bool handled = false;
	if (luaState) {
		lua_pushstring(luaState, name);
		if (safe_getglobal(luaState) && lua_isfunction(luaState, -1)) {
			handled = call_function(luaState, 0);
		}
	}
	return handled;
}

static bool CallNamedFunction(const char *name, const char *arg) {
	bool handled = false;
	if (luaState) {
		lua_pushstring(luaState, name);
		if (safe_getglobal(luaState) && lua_isfunction(luaState, -1)) {
			lua_pushstring(luaState, arg);
			handled = call_function(luaState, 1);
		}
	}
	return handled;
}


inline bool IFaceTypeIsScriptable(IFaceType t, int index) {
	return t < iface_stringresult || (index==1 && t == iface_stringresult);
}

inline bool IFaceTypeIsNumeric(IFaceType t) {
	return (t > iface_void && t < iface_string);
}

inline bool IFaceFunctionIsScriptable(const IFaceFunction &f) {
	return IFaceTypeIsScriptable(f.paramType[0], 0) && IFaceTypeIsScriptable(f.paramType[1], 1);
}


static int cf_pane_iface_function(lua_State *L) {
	int funcidx = lua_upvalueindex(1);

	if (lua_islightuserdata(L, funcidx)) {
		IFaceFunction *func = reinterpret_cast<IFaceFunction *>(lua_touserdata(L, funcidx));

		ExtensionAPI::Pane p = CheckPaneSelfArg(L);

		int params[2] = {0,0};

		int arg = 2;

		char *stringResult = 0;
		bool needStringResult = false;
		
		int loopParamCount = 2;

		if (func->paramType[0] == iface_length && func->paramType[1] == iface_string) {
			params[0] = static_cast<int>(lua_strlen(L, arg));
			params[1] = reinterpret_cast<int>(params[0] ? lua_tostring(L, arg) : "");
			loopParamCount = 0;
		} else if (func->paramType[1] == iface_stringresult) {
			needStringResult = true;
			// The buffer will be allocated later, so it won't leak if Lua does
			// a longjmp in response to a bad arg.
			if (func->paramType[0] == iface_length) {
				loopParamCount = 0;
			} else {
				loopParamCount = 1;
			}
		}

		for (int i=0; i<loopParamCount; ++i) {
			if (func->paramType[i] == iface_string) {
				const char *s = lua_tostring(L, arg++);
				params[i] = reinterpret_cast<int>(s ? s : "");
			} else if (func->paramType[i] == iface_keymod) {
				const char *pszKeymod = lua_tostring(L, arg++);
				if (pszKeymod)
					params[i] = SciTEKeys::ParseKeyCode(pszKeymod);
				if (params[i] == 0) {
					lua_pushstring(L, "Lua: invalid argument where keymod expression expected");
					lua_error(L);
					return 0;
				}
			} else if (IFaceTypeIsNumeric(func->paramType[i])) {
				params[i] = static_cast<int>(luaL_checknumber(L, arg++));
			}
		}

		
		if (needStringResult) {
			int stringResultLen = host->Send(p, func->value, params[0], 0);
			if (stringResultLen > 0) {
				// not all string result methods are guaranteed to add a null terminator
				stringResult = new char[stringResultLen+1];
				if (stringResult) {
					stringResult[stringResultLen]='\0';
					params[1] = reinterpret_cast<int>(stringResult);
				} else {
					lua_pushstring(L, "Lua: string result buffer allocation failed");
					lua_error(L);
					return 0;
				}
			} else {
				// Is this an error?  Are there any cases where it's not an error,
				// and where the right thing to do is just return a blank string?
				return 0;
			}
			if (func->paramType[0] == iface_length) {
				params[0] = stringResultLen;
			}
		}
		
		// Now figure out what to do with the param types and return type.
		// - stringresult gets inserted at the start of return tuple.
		// - numeric return type gets returned to lua as a number (following the stringresult)
		// - other return types e.g. void get dropped.

		int result = host->Send(p, func->value, reinterpret_cast<uptr_t&>(params[0]), reinterpret_cast<sptr_t&>(params[1]));

		int resultCount = 0;

		if (stringResult) {
			lua_pushstring(L, stringResult);
			delete[] stringResult;
			resultCount++;
		}

		if (IFaceTypeIsNumeric(func->returnType)) {
			lua_pushnumber(L, result);
			resultCount++;
		}

		return resultCount;
	} else {
		lua_pushstring(L, "Lua: internal error - bad upvalue in iface function closure");
		lua_error(L);
	}

	return 0;
}

static bool push_iface_function(lua_State *L, const char *name, int closureStackIndex=0) {
	int i = IFaceTable::FindFunction(name);
	if (i >= 0) {
		if (IFaceFunctionIsScriptable(IFaceTable::functions[i])) {
			int closureCount = 1;
			if (closureStackIndex && lua_type(L, closureStackIndex) != LUA_TNONE) {
				lua_pushvalue(L, closureStackIndex);
				++closureCount;
			}
			lua_pushlightuserdata(L, const_cast<IFaceFunction*>(IFaceTable::functions+i));
			
			lua_pushcclosure(L, cf_pane_iface_function, closureCount);
			return true;
		} else {
			return false;
		}
	}
	return false;
}

struct NamedFunction {
	char *name;
	lua_CFunction func;
};

static NamedFunction paneFunctions[] = {
	{"findtext", cf_pane_findtext},  // FindText is not exposed due to complex argument type.
	{"textrange", cf_pane_textrange}, // GetTextRange is not exposed, so this is still needed.
	{"remove", cf_pane_remove}, // this requires 2 requests - SetSel followed by Clear.  Drop it?
	{"insert", cf_pane_insert},    // identical to InsertText.  Drop it?
	{NULL,NULL}
};

static bool push_named_function(lua_State *L, NamedFunction *namedFunctions, const char *name, int closureStackIndex=0) {
	for (int i=0; namedFunctions[i].name; ++i) {
		if (0==strcmp(name, namedFunctions[i].name)) {
			if (closureStackIndex && lua_type(L, closureStackIndex) != LUA_TNONE) {
				lua_pushvalue(L, closureStackIndex);
				lua_pushcclosure(L, namedFunctions[i].func, 1);
			} else {
				lua_pushcfunction(L, namedFunctions[i].func);
			}
			return true;
		}
	}
	return false;
}


static int cf_pane_metatable_index(lua_State *L) {
	if (lua_isstring(L, 2)) {
		const char *name = lua_tostring(L, 2);
		return (push_named_function(L, paneFunctions, name) || 
			push_iface_function(L, name)
		);
	}

	return 0;
}


static int cf_global_metatable_index(lua_State *L) {
	if (lua_isstring(L, 2)) {
		const char *name = lua_tostring(L, 2);
		int i = IFaceTable::FindConstant(name);
		if (i >= 0) {
			lua_pushnumber(L, IFaceTable::constants[i].value);
			return 1;
		} else {
			i = IFaceTable::FindFunctionByConstantName(name);
			if (i >= 0) {
				lua_pushnumber(L, IFaceTable::functions[i].value);
				return 1;
			}
		}
	}

	return 0;
}



static int LuaPanicFunction(lua_State *L) {
	if (L == luaState) {
		lua_close(luaState);
		luaState = NULL;
		luaDisabled = true;
	}
	host->Trace("\n> Lua: error occurred in unprotected call.  This is very bad.\n");

	return 1;
}




// Don't initialise Lua in LuaExtension::Initialise.  Wait and initialise Lua the
// first time Lua is used, e.g. when a Load event is called with an argument that
// appears to be the name of a Lua script.  This just-in-time initialisation logic
// does add a little extra complexity but not a lot.  It's probably worth it,
// since it means a user who is having trouble with Lua can just refrain from
// using it.

static char *CheckStartupScript() {
	if (startupScript)
		delete[] startupScript;

	startupScript = host->Property("ext.lua.startup.script");

	if (startupScript && startupScript[0] == '\0') {
		delete[] startupScript;
		startupScript = NULL;
	}

	return startupScript;
}

static inline int CheckResetMode() {
	int resetMode = 0;
	char *pszResetMode = host->Property("ext.lua.reset");
	if (pszResetMode) {
		resetMode = atoi(pszResetMode);
		delete [] pszResetMode;
	}
	return resetMode;
}


static bool InitGlobalScope(bool checkProperties) {
	int resetMode = checkProperties ? CheckResetMode() : 0;
	
	if (luaState && resetMode==2) {
		lua_close(luaState);
		luaState = 0;
	}
	
	if (luaState) {
		lua_newtable(luaState);
		if ((resetMode==0) && lua_getmetatable(luaState, LUA_GLOBALSINDEX)) {
			lua_setmetatable(luaState, -2);
			lua_replace(luaState, LUA_GLOBALSINDEX);
			return true;
		}
		lua_replace(luaState, LUA_GLOBALSINDEX);
	} else if (!luaDisabled) {
		luaState = lua_open();
		if (!luaState) {
			luaDisabled = true;
			host->Trace("> Lua: scripting engine failed to initalise\n");
			return false;
		}
		lua_atpanic(luaState, LuaPanicFunction);
	} else {
		return false;
	}

	// ...register standard libraries
	luaopen_base(luaState);
	luaopen_string(luaState);
	luaopen_table(luaState);
	luaopen_math(luaState);
	luaopen_io(luaState);
	luaopen_debug(luaState);
	
	// Now, there are some static functions that I want to have easy
	// access to later, so I'll just grab those now
	
	lua_pushstring(luaState, "loadstring");
	if (safe_getglobal(luaState)) {
		lualib_loadstring = lua_tocfunction(luaState, -1);
		lua_pop(luaState,1);
	}
	
	lua_pushstring(luaState, "string");
	if (safe_getglobal(luaState)) {
		lua_pushstring(luaState, "find");
		lua_gettable(luaState, -2);
		lualib_string_find = lua_tocfunction(luaState, -1);
		lua_pop(luaState, 2);
	}

	// ...and our own globals.

	lua_register(luaState, "_ALERT", cf_global_alert); //default may be overridden
	lua_register(luaState, "alert", cf_global_alert);
	lua_register(luaState, "dostring", cf_global_dostring);
	lua_register(luaState, "trace", cf_global_trace);

	// props object with metatable
	lua_pushstring(luaState, "props");
	lua_boxpointer(luaState, NULL);
	
	lua_newtable(luaState);
	lua_pushstring(luaState, "__index");
	lua_pushcfunction(luaState, cf_props_metatable_index);
	lua_settable(luaState, -3);
	lua_pushstring(luaState, "__newindex");
	lua_pushcfunction(luaState, cf_props_metatable_newindex);
	lua_settable(luaState, -3);

	lua_setmetatable(luaState, -2);
	lua_settable(luaState, LUA_GLOBALSINDEX);


	// Metatable for pane objects
	lua_newtable(luaState);
	lua_pushstring(luaState, "__index");
	lua_pushcfunction(luaState, cf_pane_metatable_index);
	lua_settable(luaState, -3);

	// Use lua_boxpointer and lua_unboxpointer rather than
	// lightuserdata, so the result will be a real userdata
	// that can have a metatable.

	lua_pushstring(luaState, "editor");
	lua_boxpointer(luaState, reinterpret_cast<void *>(ExtensionAPI::paneEditor));
	lua_pushvalue(luaState, -3);
	lua_setmetatable(luaState,-2);
	lua_settable(luaState, LUA_GLOBALSINDEX);

	lua_pushstring(luaState, "output");
	lua_boxpointer(luaState, reinterpret_cast<void *>(ExtensionAPI::paneOutput));
	lua_pushvalue(luaState, -3);
	lua_setmetatable(luaState, -2);
	lua_settable(luaState, LUA_GLOBALSINDEX);

	// Squirrel the metatable away?  No.
	lua_pop(luaState, 1);

	// Metatable for global namespace, to publish iface constants
	lua_newtable(luaState);
	lua_pushstring(luaState, "__index");
	lua_pushcfunction(luaState, cf_global_metatable_index);
	lua_settable(luaState, -3);
	lua_setmetatable(luaState, LUA_GLOBALSINDEX);
	
	if (checkProperties && resetMode != 0) {
		CheckStartupScript();
	}
	
	if (startupScript) {
		int status = lua_dofile(luaState, startupScript);
		
		if (status != 0) {
			host->Trace("> Lua: error occurred in startup script\n");
			// reset, or leave things in a possibly invalid state?
		}
	}
	
	// So that startup doesn't need to be re-run each time Load is called:
	
	// Create a new global namespace, with the "current" global namespace
	// (modified by startup script) as the index metamethod.  This in turn
	// will cascade to the iface constants and functions when needed.
	
	// This is our new metatable
	lua_newtable(luaState);
	lua_pushstring(luaState, "__index");
	lua_pushvalue(luaState, LUA_GLOBALSINDEX);
	lua_settable(luaState, -3);
	
	// And here's our new blank namespace (with all globals meta-accessible).
	lua_newtable(luaState);
	lua_replace(luaState, LUA_GLOBALSINDEX);
	lua_setmetatable(luaState, LUA_GLOBALSINDEX);
	
	return true;
}



bool LuaExtension::Initialise(ExtensionAPI *host_) {
	host = host_;

	if (CheckStartupScript()) {
		InitGlobalScope(false);
	}

	return false;
}

bool LuaExtension::Finalise() {
	if (luaState) {
		lua_close(luaState);
	}

	luaState = NULL;
	host = NULL;
	
	// The rest don't strictly need to be cleared since they
	// are never accessed except when luaState and host are set
	
	if (startupScript) {
		delete [] startupScript;
		startupScript = NULL;
	}
	
	lualib_loadstring = 0;
	lualib_string_find = 0; 
	
	return false;
}

bool LuaExtension::Clear() {
	if (luaState) {
		InitGlobalScope(true);
	} else if (CheckResetMode() && CheckStartupScript()) {
		InitGlobalScope(false);
	}

	return false;
}

bool LuaExtension::Load(const char *filename) {
	bool loaded = false;

	if (!luaDisabled) {
		int sl = strlen(filename);
		if (sl >= 4 && strcmp(filename+sl-4, ".lua")==0) {
			if (luaState || InitGlobalScope(false)) {
				int status = lua_dofile(luaState, filename);

				if (status != 0) {
					host->Trace("> Lua: error loading extension script\n");
				}
				loaded = true;
			}
		}
	}
	return loaded;
}

bool LuaExtension::OnExecute(const char *s) {
	bool handled = false;

	if (luaState || InitGlobalScope(false)) {
		// May as well use Lua's pattern matcher to parse the command.
		// Scintilla's RESearch was the other option.

		int stackBase = lua_gettop(luaState);
		
		if (lualib_string_find) {
			lua_pushcfunction(luaState, lualib_string_find);
			lua_pushstring(luaState, s);
			lua_pushstring(luaState, "^%s*([%a_][%a%d_]*)%s*(.-)%s*$");
			
			int status = lua_pcall(luaState, 2, 4, 0);
			if (status==0) {
				lua_insert(luaState, stackBase+1);
	
				if (safe_getglobal(luaState)) {
					if (lua_isfunction(luaState, -1)) {
						// Try calling it and, even if it fails, short-circuit Filerx
						
						handled = true;
						
						lua_insert(luaState, stackBase+1);
						
						lua_settop(luaState, stackBase+2);
						
						if (!call_function(luaState, 1, true)) {
							host->Trace("> Lua: error occurred while processing command\n");
						}
					}
				} else {
					host->Trace("> Lua: error checking global scope for command\n");
				}
			}
		}
		lua_settop(luaState, stackBase);
	}
	
	return handled;
}

bool LuaExtension::OnOpen(const char *filename) {
	return CallNamedFunction("OnOpen", filename);
}

bool LuaExtension::OnSwitchFile(const char *filename) {
	return CallNamedFunction("OnSwitchFile", filename);
}

bool LuaExtension::OnBeforeSave(const char *filename) {
	return CallNamedFunction("OnBeforeSave", filename);
}

bool LuaExtension::OnSave(const char *filename) {
	return CallNamedFunction("OnSave", filename);
}

bool LuaExtension::OnChar(char ch) {
	char chs[2] = {ch, '\0'};
	return CallNamedFunction("OnChar", chs);
}

bool LuaExtension::OnSavePointReached() {
	return CallNamedFunction("OnSavePointReached");
}

bool LuaExtension::OnSavePointLeft() {
	return CallNamedFunction("OnSavePointLeft");
}

bool LuaExtension::OnDoubleClick() {
	return CallNamedFunction("OnDoubleClick");
}

bool LuaExtension::OnMarginClick() {
	return CallNamedFunction("OnMarginClick");
}


#ifdef _MSC_VER
// Unreferenced inline functions are OK
#pragma warning(disable: 4514)
#endif 
