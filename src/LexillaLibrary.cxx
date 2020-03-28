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
#include <string_view>
#include <vector>

#if !_WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include "ILexer.h"

#include "LexillaLibrary.h"

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

#if _WIN32

std::wstring WideStringFromUTF8(std::string_view sv) {
	const int sLength = static_cast<int>(sv.length());
	const int cchWide = ::MultiByteToWideChar(CP_UTF8, 0, sv.data(), sLength, nullptr, 0);
	std::wstring sWide(cchWide, 0);
	::MultiByteToWideChar(CP_UTF8, 0, sv.data(), sLength, &sWide[0], cchWide);
	return sWide;
}

#endif

std::string lastLoaded;
std::vector<CreateLexerFn> fnCLs;

Function FindSymbol(Module m, const char *symbol) noexcept {
#if _WIN32
	return ::GetProcAddress(m, symbol);
#else
	return dlsym(m, symbol);
#endif
}

}

bool LexillaLoad(std::string_view sharedLibraryPaths) {
	if (sharedLibraryPaths == lastLoaded) {
		return !fnCLs.empty();
	}

	std::string_view paths = sharedLibraryPaths;

	fnCLs.clear();
	while (!paths.empty()) {
		const size_t separator = paths.find_first_of(';');
		std::string path(paths.substr(0, separator));
		if (separator == std::string::npos) {
			paths.remove_prefix(paths.size());
		} else {
			paths.remove_prefix(separator + 1);
		}
		if (path.find('.') == std::string::npos) {
			// No '.' in path so add extension
			path.append(extensionSO);
		}
#if _WIN32
		// Convert from UTF-8 to wide characters
		std::wstring wsPath = WideStringFromUTF8(path);
		Module lexillaDL = ::LoadLibraryW(wsPath.c_str());
#else
		Module lexillaDL = dlopen(path.c_str(), RTLD_LAZY);
#endif
		if (lexillaDL) {
			CreateLexerFn fnCL = FunctionPointer<CreateLexerFn>(
				FindSymbol(lexillaDL, "CreateLexer"));
			if (fnCL) {
				fnCLs.push_back(fnCL);
			}
		}
	}
	lastLoaded = sharedLibraryPaths;

	return !fnCLs.empty();
}

Scintilla::ILexer5 *LexillaCreateLexer(std::string_view languageName) {
	std::string sLanguageName(languageName);	// Ensure NUL-termination
	for (CreateLexerFn fnCL : fnCLs) {
		Scintilla::ILexer5 *pLexer = fnCL(sLanguageName.c_str());
		if (pLexer) {
			return pLexer;
		}
	}
	return nullptr;
}
