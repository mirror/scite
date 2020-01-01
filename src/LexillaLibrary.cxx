// SciTE - Scintilla based Text Editor
/** @file LexillaLibrary.cxx
 ** Interface to loadable lexers.
 ** Maintains a list of lexer library paths and CreateLexer functions.
 ** If list changes then load all the lexer libraries and find the functions.
 ** When asked to create a lexer, call each function until one succeeds.
 **/
// Copyright 2019 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstring>

#include <string>
#include <vector>
#include <chrono>

#if !_WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include "ILexer.h"

#include "GUI.h"

#include "FilePath.h"

namespace {

#if _WIN32
#define EXT_LEXER_DECL __stdcall
typedef FARPROC Function;
typedef HMODULE Module;
constexpr const char *extensionSO = ".dll"; 
#else
#define EXT_LEXER_DECL
typedef void *Function;
typedef void *Module;
#if defined(__APPLE__)
constexpr const char *extensionSO = ".dylib";
#else
constexpr const char *extensionSO = ".so";
#endif
#endif

typedef Scintilla::ILexer5 *(EXT_LEXER_DECL *CreateLexerFn)(const char *name);

/// Generic function to convert from a Function(void* or FARPROC) to a function pointer.
/// This avoids undefined and conditionally defined behaviour.
template<typename T>
T FunctionPointer(Function function) noexcept {
	static_assert(sizeof(T) == sizeof(function));
	T fp;
	memcpy(&fp, &function, sizeof(T));
	return fp;
}

FilePathSet lastLoaded;
std::vector<CreateLexerFn> fnCLs;

Function FindSymbol(Module m, const char *symbol) noexcept {
#if _WIN32
	return ::GetProcAddress(m, symbol);
#else
	return dlsym(m, symbol);
#endif
}

}

bool LexillaLoad(FilePathSet sharedLibraries) {
	if (lastLoaded != sharedLibraries) {
		fnCLs.clear();
		for (FilePath sharedLibrary : sharedLibraries) {
			if (!sharedLibrary.Extension().IsSet()) {
				std::string slu = sharedLibrary.AsUTF8();
				slu.append(extensionSO);
				sharedLibrary.Set(GUI::StringFromUTF8(slu));
			}
#if _WIN32
			Module lexillaDL = ::LoadLibraryW(sharedLibrary.AsInternal());
#else
			Module lexillaDL = dlopen(sharedLibrary.AsInternal(), RTLD_LAZY);
#endif
			if (lexillaDL) {
				CreateLexerFn fnCL = FunctionPointer<CreateLexerFn>(
					FindSymbol(lexillaDL, "CreateLexer"));
				if (fnCL) {
					fnCLs.push_back(fnCL);
				}
			}
		}
		lastLoaded = sharedLibraries;
	}

	return !fnCLs.empty();
}

Scintilla::ILexer5 *LexillaCreateLexer(const std::string &languageName) {
	for (CreateLexerFn fnCL : fnCLs) {
		Scintilla::ILexer5 *pLexer = fnCL(languageName.c_str());
		if (pLexer) {
			return pLexer;
		}
	}
	return nullptr;
}
