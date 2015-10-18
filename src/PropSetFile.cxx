// SciTE - Scintilla based Text Editor
/** @file PropSetFile.cxx
 ** Property set implementation.
 **/
// Copyright 1998-2007 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <locale.h>
#include <assert.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>

#if defined(GTK)

#include <unistd.h>

#endif

#include "Scintilla.h"

#include "GUI.h"

#include "StringHelpers.h"
#include "FilePath.h"
#include "PropSetFile.h"

// The comparison and case changing functions here assume ASCII
// or extended ASCII such as the normal Windows code page.

inline bool IsASpace(unsigned int ch) {
    return (ch == ' ') || ((ch >= 0x09) && (ch <= 0x0d));
}

static std::set<std::string> FilterFromString(std::string values) {
	std::vector<std::string> vsFilter = StringSplit(values, ' ');
	std::set<std::string> fs;
	for (std::vector<std::string>::const_iterator it=vsFilter.begin(); it != vsFilter.end(); ++it) {
		if (!it->empty())
			fs.insert(*it);
	}
	return fs;
}

void ImportFilter::SetFilter(std::string sExcludes, std::string sIncludes) {
	excludes = FilterFromString(sExcludes);
	includes = FilterFromString(sIncludes);
}

bool ImportFilter::IsValid(std::string name) const {
	if (!includes.empty()) {
		return includes.count(name) > 0;
	} else {
		return excludes.count(name) == 0;
	}
}

bool PropSetFile::caseSensitiveFilenames = false;

PropSetFile::PropSetFile(bool lowerKeys_) : lowerKeys(lowerKeys_), superPS(0) {
}

PropSetFile::PropSetFile(const PropSetFile &copy) : lowerKeys(copy.lowerKeys), props(copy.props), superPS(copy.superPS) {
}

PropSetFile::~PropSetFile() {
	superPS = 0;
	Clear();
}

PropSetFile &PropSetFile::operator=(const PropSetFile &assign) {
	if (this != &assign) {
		lowerKeys = assign.lowerKeys;
		superPS = assign.superPS;
		props = assign.props;
	}
	return *this;
}

void PropSetFile::Set(const char *key, const char *val, ptrdiff_t lenKey, ptrdiff_t lenVal) {
	if (!*key)	// Empty keys are not supported
		return;
	if (lenKey == -1)
		lenKey = strlen(key);
	if (lenVal == -1)
		lenVal = strlen(val);
	props[std::string(key, lenKey)] = std::string(val, lenVal);
}

void PropSetFile::Set(const char *keyVal) {
	while (IsASpace(*keyVal))
		keyVal++;
	const char *endVal = keyVal;
	while (*endVal && (*endVal != '\n'))
		endVal++;
	const char *eqAt = strchr(keyVal, '=');
	if (eqAt) {
		const char *pKeyEnd = eqAt - 1;
		while ((pKeyEnd >= keyVal) && IsASpace(*pKeyEnd)) {
			--pKeyEnd;
		}
		const ptrdiff_t lenVal = endVal - eqAt - 1;
		const ptrdiff_t lenKey = pKeyEnd - keyVal + 1;
		Set(keyVal, eqAt + 1, lenKey, lenVal);
	} else if (*keyVal) {	// No '=' so assume '=1'
		Set(keyVal, "1", endVal-keyVal, 1);
	}
}

void PropSetFile::Unset(const char *key, int lenKey) {
	if (!*key)	// Empty keys are not supported
		return;
	if (lenKey == -1)
		lenKey = static_cast<int>(strlen(key));
	mapss::iterator keyPos = props.find(std::string(key, lenKey));
	if (keyPos != props.end())
		props.erase(keyPos);
}

bool PropSetFile::Exists(const char *key) const {
	mapss::const_iterator keyPos = props.find(std::string(key));
	if (keyPos != props.end()) {
		return true;
	} else {
		if (superPS) {
			// Failed here, so try in base property set
			return superPS->Exists(key);
		} else {
			return false;
		}
	}
}

std::string PropSetFile::GetString(const char *key) const {
	const std::string sKey(key);
	const PropSetFile *psf = this;
	while (psf) {
		mapss::const_iterator keyPos = psf->props.find(sKey);
		if (keyPos != psf->props.end()) {
			return keyPos->second;
		}
		// Failed here, so try in base property set
		psf = psf->superPS;
	}
	return "";
}

static std::string ShellEscape(const char *toEscape) {
	std::string str(toEscape);
	for (int i = static_cast<int>(str.length()-1); i >= 0; --i) {
		switch (str[i]) {
		case ' ':
		case '|':
		case '&':
		case ',':
		case '`':
		case '"':
		case ';':
		case ':':
		case '!':
		case '^':
		case '$':
		case '{':
		case '}':
		case '(':
		case ')':
		case '[':
		case ']':
		case '=':
		case '<':
		case '>':
		case '\\':
		case '\'':
			str.insert(i, "\\");
			break;
		default:
			break;
		}
	}
	return str;
}

std::string PropSetFile::Evaluate(const char *key) const {
	if (strchr(key, ' ')) {
		if (isprefix(key, "escape ")) {
			std::string val = GetString(key+7);
			std::string escaped = ShellEscape(val.c_str());
			return escaped;
		} else if (isprefix(key, "star ")) {
			const std::string sKeybase(key + 5);
			// Create set of variables with values
			mapss values;
			// For this property set and all base sets
			for (const PropSetFile *psf = this; psf; psf = psf->superPS) {
				mapss::const_iterator it = psf->props.lower_bound(sKeybase);
				while ((it != psf->props.end()) && (it->first.find(sKeybase) == 0)) {
					mapss::iterator itDestination = values.find(it->first);
					if (itDestination == values.end()) {
						// Not present so add
						values[it->first] = it->second;
					}
					++it;
				}
			}
			// Concatenate all variables
			std::string combination;
			for (mapss::const_iterator itV = values.begin(); itV != values.end(); ++itV) {
				combination += itV->second;
			}
			return combination;
		} else if (isprefix(key, "scale ")) {
			const int scaleFactor = GetInt("ScaleFactor", 100);
			const char *val = key + 6;
			if (scaleFactor == 100) {
				return val;
			} else {
				const int value = atoi(val);
				return StdStringFromInteger(value * scaleFactor / 100);
			}
		}
	} else {
		return GetString(key);
	}
	return "";
}

// There is some inconsistency between GetExpanded("foo") and Expand("$(foo)").
// A solution is to keep a stack of variables that have been expanded, so that
// recursive expansions can be skipped.  For now I'll just use the C++ stack
// for that, through a recursive function and a simple chain of pointers.

struct VarChain {
	VarChain(const char*var_=NULL, const VarChain *link_=NULL): var(var_), link(link_) {}

	bool contains(const char *testVar) const {
		return (var && (0 == strcmp(var, testVar)))
			|| (link && link->contains(testVar));
	}

	const char *var;
	const VarChain *link;
};

static int ExpandAllInPlace(const PropSetFile &props, std::string &withVars, int maxExpands, const VarChain &blankVars = VarChain()) {
	size_t varStart = withVars.find("$(");
	while ((varStart != std::string::npos) && (maxExpands > 0)) {
		size_t varEnd = withVars.find(")", varStart+2);
		if (varEnd == std::string::npos) {
			break;
		}

		// For consistency, when we see '$(ab$(cde))', expand the inner variable first,
		// regardless whether there is actually a degenerate variable named 'ab$(cde'.
		size_t innerVarStart = withVars.find("$(", varStart+2);
		while ((innerVarStart != std::string::npos) && (innerVarStart < varEnd)) {
			varStart = innerVarStart;
			innerVarStart = withVars.find("$(", varStart+2);
		}

		std::string var(withVars.c_str(), varStart + 2, varEnd - (varStart + 2));
		std::string val = props.Evaluate(var.c_str());

		if (blankVars.contains(var.c_str())) {
			val.clear(); // treat blankVar as an empty string (e.g. to block self-reference)
		}

		if (--maxExpands >= 0) {
			maxExpands = ExpandAllInPlace(props, val, maxExpands, VarChain(var.c_str(), &blankVars));
		}

		withVars.erase(varStart, varEnd-varStart+1);
		withVars.insert(varStart, val);

		varStart = withVars.find("$(");
	}

	return maxExpands;
}

std::string PropSetFile::GetExpandedString(const char *key) const {
	std::string val = GetString(key);
	ExpandAllInPlace(*this, val, 200, VarChain(key));
	return val;
}

std::string PropSetFile::Expand(const std::string &withVars, int maxExpands) const {
	std::string val = withVars;
	ExpandAllInPlace(*this, val, maxExpands);
	return val;
}

int PropSetFile::GetInt(const char *key, int defaultValue) const {
	std::string val = GetExpandedString(key);
	if (val.length())
		return atoi(val.c_str());
	return defaultValue;
}

void PropSetFile::Clear() {
	props.clear();
}

/**
 * Get a line of input. If end of line escaped with '\\' then continue reading.
 */
static bool GetFullLine(const char *&fpc, size_t &lenData, char *s, size_t len) {
	bool continuation = true;
	s[0] = '\0';
	while ((len > 1) && lenData > 0) {
		char ch = *fpc;
		fpc++;
		lenData--;
		if ((ch == '\r') || (ch == '\n')) {
			if (!continuation) {
				if ((lenData > 0) && (ch == '\r') && ((*fpc) == '\n')) {
					// munch the second half of a crlf
					fpc++;
					lenData--;
				}
				*s = '\0';
				return true;
			}
		} else if ((ch == '\\') && (lenData > 0) && ((*fpc == '\r') || (*fpc == '\n'))) {
			continuation = true;
			if ((lenData > 1) && (((*fpc == '\r') && (*(fpc+1) == '\r')) || ((*fpc == '\n') && (*(fpc+1) == '\n'))))
				continuation = false;
			else if ((lenData > 2) && ((*fpc == '\r') && (*(fpc+1) == '\n') && (*(fpc+2) == '\n' || *(fpc+2) == '\r')))
				continuation = false;
		} else {
			continuation = false;
			*s++ = ch;
			*s = '\0';
			len--;
		}
	}
	return false;
}

static bool IsSpaceOrTab(char ch) {
	return (ch == ' ') || (ch == '\t');
}

static bool IsCommentLine(const char *line) {
	while (IsSpaceOrTab(*line)) ++line;
	return (*line == '#');
}

static bool GenericPropertiesFile(const FilePath &filename) {
	std::string name = filename.BaseName().AsUTF8();
	if (name == "abbrev" || name == "Embedded")
		return true;
	return name.find("SciTE") != std::string::npos;
}

void PropSetFile::Import(FilePath filename, FilePath directoryForImports, const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth) {
	if (depth > 20)	// Possibly recursive import so give up to avoid crash
		return;
	if (Read(filename, directoryForImports, filter, imports, depth)) {
		if (imports && (std::find(imports->begin(),imports->end(), filename) == imports->end())) {
			imports->push_back(filename);
		}
	}
}

PropSetFile::ReadLineState PropSetFile::ReadLine(const char *lineBuffer, ReadLineState rls, FilePath directoryForImports,
	const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth) {
	//UnSlash(lineBuffer);
	if ((rls == rlConditionFalse) && (!IsSpaceOrTab(lineBuffer[0])))    // If clause ends with first non-indented line
		rls = rlActive;
	if (isprefix(lineBuffer, "module ")) {
		std::string module = lineBuffer + strlen("module") + 1;
		if (module.empty() || filter.IsValid(module)) {
			rls = rlActive;
		} else {
			rls = rlExcludedModule;
		}
	}
	if (rls != rlActive) {
		return rls;
	}
	if (isprefix(lineBuffer, "if ")) {
		const char *expr = lineBuffer + strlen("if") + 1;
		rls = (GetInt(expr) != 0) ? rlActive : rlConditionFalse;
	} else if (isprefix(lineBuffer, "import ") && directoryForImports.IsSet()) {
		std::string importName(lineBuffer + strlen("import") + 1);
		if (importName == "*") {
			// Import all .properties files in this directory except for system properties
			FilePathSet directories;
			FilePathSet files;
			directoryForImports.List(directories, files);
			for (size_t i = 0; i < files.size(); i ++) {
				FilePath fpFile = files[i];
				if (IsPropertiesFile(fpFile) &&
					!GenericPropertiesFile(fpFile) &&
					filter.IsValid(fpFile.BaseName().AsUTF8())) {
					FilePath importPath(directoryForImports, fpFile);
					Import(importPath, directoryForImports, filter, imports, depth+1);
				}
			}
		} else if (filter.IsValid(importName)) {
			importName += ".properties";
			FilePath importPath(directoryForImports, FilePath(GUI::StringFromUTF8(importName)));
			Import(importPath, directoryForImports, filter, imports, depth+1);
		}
	} else if (!IsCommentLine(lineBuffer)) {
		Set(lineBuffer);
	}
	return rls;
}

void PropSetFile::ReadFromMemory(const char *data, size_t len, FilePath directoryForImports,
                                 const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth) {
	const char *pd = data;
	std::vector<char> lineBuffer(len+1);	// +1 for NUL
	ReadLineState rls = rlActive;
	while (len > 0) {
		GetFullLine(pd, len, &lineBuffer[0], lineBuffer.size());
		if (lowerKeys) {
			for (int i=0; lineBuffer[i] && (lineBuffer[i] != '='); i++) {
				if ((lineBuffer[i] >= 'A') && (lineBuffer[i] <= 'Z')) {
					lineBuffer[i] = static_cast<char>(lineBuffer[i] - 'A' + 'a');
				}
			}
		}
		rls = ReadLine(&lineBuffer[0], rls, directoryForImports, filter, imports, depth);
	}
}

bool PropSetFile::Read(FilePath filename, FilePath directoryForImports,
                       const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth) {
	std::vector<char> propsData = filename.Read();
	size_t lenFile = propsData.size();
	if (lenFile > 0) {
		const char *data = &propsData[0];
		if ((lenFile >= 3) && (memcmp(data, "\xef\xbb\xbf", 3) == 0)) {
			data += 3;
			lenFile -= 3;
		}
		ReadFromMemory(data, lenFile, directoryForImports, filter, imports, depth);
		return true;
	}
	return false;
}

void PropSetFile::SetInteger(const char *key, int i) {
	char tmp[32];
	sprintf(tmp, "%d", static_cast<int>(i));
	Set(key, tmp);
}

static bool StringEqual(const char *a, const char *b, size_t len, bool caseSensitive) {
	if (caseSensitive) {
		for (size_t i = 0; i < len; i++) {
			if (a[i] != b[i])
				return false;
		}
	} else {
		for (size_t i = 0; i < len; i++) {
			if (MakeUpperCase(a[i]) != MakeUpperCase(b[i]))
				return false;
		}
	}
	return true;
}

// Match file names to patterns allowing for a '*' suffix or prefix.
static bool MatchWild(const char *pattern, size_t lenPattern, const char *fileName, bool caseSensitive) {
	size_t lenFileName = strlen(fileName);
	if (lenFileName == lenPattern) {
		if (StringEqual(pattern, fileName, lenFileName, caseSensitive)) {
			return true;
		}
	}
	if (lenFileName >= lenPattern-1) {
		if (pattern[0] == '*') {
			// Matching suffixes
			return StringEqual(pattern+1, fileName + lenFileName - (lenPattern-1), lenPattern-1, caseSensitive);
		} else if (pattern[lenPattern-1] == '*') {
			// Matching prefixes
			return StringEqual(pattern, fileName, lenPattern-1, caseSensitive);
		}
	}
	return false;
}

static bool startswith(const std::string &s, const char *keybase) {
	return isprefix(s.c_str(), keybase);
}

std::string PropSetFile::GetWildUsingStart(const PropSetFile &psStart, const char *keybase, const char *filename) {
	const std::string sKeybase(keybase);
	const size_t lenKeybase = strlen(keybase);
	const PropSetFile *psf = this;
	while (psf) {
		mapss::const_iterator it = psf->props.lower_bound(sKeybase);
		while ((it != psf->props.end()) && startswith(it->first, keybase)) {
			const char *orgkeyfile = it->first.c_str() + lenKeybase;
			const char *keyptr = NULL;
			std::string key;

			if (strncmp(orgkeyfile, "$(", 2) == 0) {
				const char *cpendvar = strchr(orgkeyfile, ')');
				if (cpendvar) {
					std::string var(orgkeyfile, 2, cpendvar - orgkeyfile - 2);
					key = psStart.GetExpandedString(var.c_str());
					keyptr = key.c_str();
				}
			}
			const char *keyfile = keyptr;

			if (keyfile == NULL)
				keyfile = orgkeyfile;

			for (;;) {
				const char *del = strchr(keyfile, ';');
				if (del == NULL)
					del = keyfile + strlen(keyfile);
				if (MatchWild(keyfile, del - keyfile, filename, caseSensitiveFilenames)) {
					return it->second;
				}
				if (*del == '\0')
					break;
				keyfile = del + 1;
			}

			if (0 == strcmp(it->first.c_str(), keybase)) {
				return it->second;
			}
			++it;
		}
		// Failed here, so try in base property set
		psf = psf->superPS;
	}
	return "";
}

std::string PropSetFile::GetWild(const char *keybase, const char *filename) {
	return GetWildUsingStart(*this, keybase, filename);
}

// GetNewExpandString does not use Expand as it has to use GetWild with the filename for each
// variable reference found.

std::string PropSetFile::GetNewExpandString(const char *keybase, const char *filename) {
	std::string withVars = GetWild(keybase, filename);
	size_t varStart = withVars.find("$(");
	int maxExpands = 1000;	// Avoid infinite expansion of recursive definitions
	while ((varStart != std::string::npos) && (maxExpands > 0)) {
		size_t varEnd = withVars.find(")", varStart+2);
		if (varEnd == std::string::npos) {
			break;
		}
		std::string var(withVars, varStart + 2, varEnd - varStart - 2);	// Subtract the $(
		std::string val = GetWild(var.c_str(), filename);
		if (var == keybase)
			val.clear(); // Self-references evaluate to empty string
		withVars.replace(varStart, varEnd - varStart + 1, val);
		varStart = withVars.find("$(");
		maxExpands--;
	}
	return withVars;
}

/**
 * Initiate enumeration.
 */
bool PropSetFile::GetFirst(const char *&key, const char *&val) {
	mapss::iterator it = props.begin();
	if (it != props.end()) {
		key = it->first.c_str();
		val = it->second.c_str();
		return true;
	} else {
		return false;
	}
}

/**
 * Continue enumeration.
 */
bool PropSetFile::GetNext(const char *&key, const char *&val) {
	mapss::iterator it = props.find(key);
	if (it != props.end()) {
		++it;
		if (it != props.end()) {
			key = it->first.c_str();
			val = it->second.c_str();
			return true;
		}
	}
	return false;
}

bool IsPropertiesFile(const FilePath &filename) {
	FilePath ext = filename.Extension();
	if (EqualCaseInsensitive(ext.AsUTF8().c_str(), PROPERTIES_EXTENSION + 1))
		return true;
	return false;
}
