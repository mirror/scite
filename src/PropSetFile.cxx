// SciTE - Scintilla based Text Editor
/** @file PropSetFile.cxx
 ** Property set implementation.
 **/
// Copyright 1998-2007 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <stdexcept>
#include <tuple>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <memory>
#include <chrono>
#include <sstream>

#include <fcntl.h>

#if defined(GTK)

#include <unistd.h>

#endif

#include "GUI.h"

#include "StringHelpers.h"
#include "FilePath.h"
#include "PathMatch.h"
#include "PropSetFile.h"
#include "EditorConfig.h"

// The comparison and case changing functions here assume ASCII
// or extended ASCII such as the normal Windows code page.

void ImportFilter::SetFilter(const std::string &sExcludes, const std::string &sIncludes) {
	excludes = SetFromString(sExcludes, ' ');
	includes = SetFromString(sIncludes, ' ');
}

bool ImportFilter::IsValid(const std::string &name) const {
	if (!includes.empty()) {
		return includes.count(name) > 0;
	} else {
		return excludes.count(name) == 0;
	}
}

bool PropSetFile::caseSensitiveFilenames = false;

PropSetFile::PropSetFile(bool lowerKeys_) : lowerKeys(lowerKeys_), superPS(nullptr) {
}

void PropSetFile::Set(std::string_view key, std::string_view val) {
	if (key.empty())	// Empty keys are not supported
		return;
	props[std::string(key)] = std::string(val);
}

void PropSetFile::SetPath(std::string_view key, const FilePath &path) {
	Set(key, path.AsUTF8());
}

void PropSetFile::SetLine(const char *keyVal, bool unescape) {
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
		const std::string_view key(keyVal, lenKey);
		const std::string_view value(eqAt + 1, lenVal);
		if (unescape && (key.find("\\") != std::string_view::npos)) {
			const std::string keyUnescaped = UnicodeUnEscape(key);
			Set(keyUnescaped, value);
		} else {
			Set(key, value);
		}
	} else if (*keyVal) {	// No '=' so assume '=1'
		Set(keyVal, "1");
	}
}

void PropSetFile::Unset(std::string_view key) {
	if (key.empty())	// Empty keys are not supported
		return;
	const mapss::iterator keyPos = props.find(key);
	if (keyPos != props.end())
		props.erase(keyPos);
}

void PropSetFile::Clear() noexcept {
	props.clear();
}

bool PropSetFile::Exists(std::string_view key) const {
	const mapss::const_iterator keyPos = props.find(key);
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

std::string_view PropSetFile::Get(std::string_view key) const {
	const PropSetFile *psf = this;
	while (psf) {
		const mapss::const_iterator keyPos = psf->props.find(key);
		if (keyPos != psf->props.end()) {
			return keyPos->second;
		}
		// Failed here, so try in base property set
		psf = psf->superPS;
	}
	return "";
}

std::string PropSetFile::GetString(std::string_view key) const {
	return std::string(Get(key));
}

namespace {

void ShellEscape(std::string &str) {
	for (ptrdiff_t i = str.length() - 1; i >= 0; --i) {
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
}

}

std::string PropSetFile::Evaluate(std::string_view key) const {
	if (key.find(' ') != std::string_view::npos) {
		if (StartsWith(key, "escape ")) {
			std::string val(Get(key.substr(7)));
			ShellEscape(val);
			return val;
		} else if (StartsWith(key, "= ")) {
			const std::string sExpressions(key.substr(2));
			std::vector<std::string> parts = StringSplit(sExpressions, ';');
			if (parts.size() > 1) {
				bool equal = true;
				for (size_t part = 1; part < parts.size(); part++) {
					if (parts[part] != parts[0]) {
						equal = false;
					}
				}
				return equal ? "1" : "0";
			}
		} else if (StartsWith(key, "star ")) {
			const std::string sKeybase(key.substr(5));
			// Create set of variables with values
			mapss values;
			// For this property set and all base sets
			for (const PropSetFile *psf = this; psf; psf = psf->superPS) {
				mapss::const_iterator it = psf->props.lower_bound(sKeybase);
				while ((it != psf->props.end()) && (it->first.find(sKeybase) == 0)) {
					const mapss::iterator itDestination = values.find(it->first);
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
		} else if (StartsWith(key, "scale ")) {
			const int scaleFactor = GetInt("ScaleFactor", 100);
			std::string val(key.substr(6));
			if (scaleFactor == 100) {
				return val;
			} else {
				const int value = IntegerFromString(val, 1);
				return StdStringFromInteger(value * scaleFactor / 100);
			}
		}
	} else {
		return std::string(Get(key));
	}
	return "";
}

// There is some inconsistency between GetExpanded("foo") and Expand("$(foo)").
// A solution is to keep a stack of variables that have been expanded, so that
// recursive expansions can be skipped.  For now I'll just use the C++ stack
// for that, through a recursive function and a simple chain of pointers.

struct VarChain {
	VarChain() = default;
	explicit VarChain(std::string_view var_, const VarChain *link_=nullptr) noexcept : var(var_), link(link_) {}

	bool contains(std::string_view testVar) const {
		return (var == testVar)
		       || (link && link->contains(testVar));
	}

	const std::string_view var;
	const VarChain *link=nullptr;
};

static int ExpandAllInPlace(const PropSetFile &props, std::string &withVars, int maxExpands, const VarChain &blankVars = VarChain()) {
	size_t varStart = withVars.find("$(");
	while ((varStart != std::string::npos) && (maxExpands > 0)) {
		const size_t varEnd = withVars.find(')', varStart+2);
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

		std::string var(withVars, varStart + 2, varEnd - (varStart + 2));
		std::string val = props.Evaluate(var);

		if (blankVars.contains(var)) {
			val.clear(); // treat blankVar as an empty string (e.g. to block self-reference)
		}

		if (--maxExpands >= 0) {
			maxExpands = ExpandAllInPlace(props, val, maxExpands, VarChain(var, &blankVars));
		}

		withVars.erase(varStart, varEnd-varStart+1);
		withVars.insert(varStart, val);

		varStart = withVars.find("$(");
	}

	return maxExpands;
}

std::string PropSetFile::GetExpandedString(std::string_view key) const {
	std::string val(Get(key));
	ExpandAllInPlace(*this, val, 200, VarChain(key));
	return val;
}

std::string PropSetFile::Expand(std::string_view withVars, int maxExpands) const {
	std::string val(withVars);
	ExpandAllInPlace(*this, val, maxExpands);
	return val;
}

int PropSetFile::GetInt(std::string_view key, int defaultValue) const {
	return IntegerFromString(GetExpandedString(key), defaultValue);
}

intptr_t PropSetFile::GetInteger(std::string_view key, intptr_t defaultValue) const {
	return IntPtrFromString(GetExpandedString(key), defaultValue);
}

long long PropSetFile::GetLongLong(std::string_view key, long long defaultValue) const {
	return LongLongFromString(GetExpandedString(key), defaultValue);
}

namespace {

/**
 * Get a line of input. If end of line escaped with '\\' then continue reading.
 */
void GetFullLine(std::string_view &data, std::string &lineBuffer) {
	lineBuffer.clear();
	while (!data.empty()) {
		const char ch = data[0];
		data.remove_prefix(1);
		if ((ch == '\r') || (ch == '\n')) {
			if ((data.length() > 0) && (ch == '\r') && (data[0] == '\n')) {
				// munch the second half of a crlf
				data.remove_prefix(1);
			}
			return;
		} else if ((ch == '\\') && (data.length() > 0) && ((data[0] == '\r') || (data[0] == '\n'))) {
			const char next = data[0];
			data.remove_prefix(1);
			if ((data.length() > 0) && (next == '\r') && (data[0] == '\n')) {
				data.remove_prefix(1);
			}
		} else {
			lineBuffer.push_back(ch);
		}
	}
}

bool IsCommentLine(std::string_view line) noexcept {
	while (!line.empty() && IsSpaceOrTab(line.front()))
		line.remove_prefix(1);
	return StartsWith(line, "#");
}

bool GenericPropertiesFile(const FilePath &filename) {
	std::string name = filename.BaseName().AsUTF8();
	if (name == "abbrev" || name == "Embedded")
		return true;
	return name.find("SciTE") != std::string::npos;
}

}

void PropSetFile::Import(const FilePath &filename, const FilePath &directoryForImports, const ImportFilter &filter,
			 FilePathSet *imports, size_t depth) {
	if (depth > 20)	// Possibly recursive import so give up to avoid crash
		return;
	if (Read(filename, directoryForImports, filter, imports, depth)) {
		if (imports && (std::find(imports->begin(), imports->end(), filename) == imports->end())) {
			imports->push_back(filename);
		}
	}
}

PropSetFile::ReadLineState PropSetFile::ReadLine(const std::string &lineBuffer, ReadLineState rls, const FilePath &directoryForImports,
		const ImportFilter &filter, FilePathSet *imports, size_t depth) {
	if ((rls == ReadLineState::conditionFalse) && (!IsSpaceOrTab(lineBuffer[0])))    // If clause ends with first non-indented line
		rls = ReadLineState::active;
	if (StartsWith(lineBuffer, "module ")) {
		std::string module = lineBuffer.substr(strlen("module") + 1);
		if (module.empty() || filter.IsValid(module)) {
			rls = ReadLineState::active;
		} else {
			rls = ReadLineState::excludedModule;
		}
		return rls;
	}
	if (rls != ReadLineState::active) {
		return rls;
	}
	if (StartsWith(lineBuffer, "if ")) {
		std::string_view expr = lineBuffer;
		expr.remove_prefix(strlen("if") + 1);
		std::string value = Expand(expr);
		if (value == "0" || value == "") {
			rls = ReadLineState::conditionFalse;
		} else if (value == "1") {
			rls = ReadLineState::active;
		} else {
			rls = (GetInt(value) != 0) ? ReadLineState::active : ReadLineState::conditionFalse;
		}
	} else if (superPS && StartsWith(lineBuffer, "match ")) {
		// Don't match on stand-alone property sets like localiser
		const std::string pattern = lineBuffer.substr(strlen("match") + 1);
		const std::string relPath(Get("RelativePath"));
		const bool matches = PathMatch(pattern, relPath);
		rls = matches ? ReadLineState::active : ReadLineState::conditionFalse;
	} else if (StartsWith(lineBuffer, "import ")) {
		if (directoryForImports.IsSet()) {
			std::string importName = lineBuffer.substr(strlen("import") + 1);
			if (importName == "*") {
				// Import all .properties files in this directory except for system properties
				FilePathSet directories;
				FilePathSet files;
				directoryForImports.List(directories, files);
				for (const FilePath &fpFile : files) {
					if (IsPropertiesFile(fpFile) &&
							!GenericPropertiesFile(fpFile) &&
							filter.IsValid(fpFile.BaseName().AsUTF8())) {
						FilePath importPath(directoryForImports, fpFile);
						Import(importPath, directoryForImports, filter, imports, depth + 1);
					}
				}
			} else if (filter.IsValid(importName)) {
				importName += ".properties";
				FilePath importPath(directoryForImports, FilePath(GUI::StringFromUTF8(importName)));
				Import(importPath, directoryForImports, filter, imports, depth + 1);
			}
		}
	} else if (!IsCommentLine(lineBuffer)) {
		SetLine(lineBuffer.c_str(), true);
	}
	return rls;
}

void PropSetFile::ReadFromMemory(std::string_view data, const FilePath &directoryForImports,
				 const ImportFilter &filter, FilePathSet *imports, size_t depth) {
	std::string lineBuffer;
	ReadLineState rls = ReadLineState::active;
	while (!data.empty()) {
		GetFullLine(data, lineBuffer);
		if (lowerKeys) {
			for (char &ch : lineBuffer) {
				if (ch == '=')
					break;
				ch = MakeLowerCase(ch);
			}
		}
		rls = ReadLine(lineBuffer, rls, directoryForImports, filter, imports, depth);
	}
}

bool PropSetFile::Read(const FilePath &filename, const FilePath &directoryForImports,
		       const ImportFilter &filter, FilePathSet *imports, size_t depth) {
	const std::string propsData = filename.Read();
	if (!propsData.empty()) {
		std::string_view data(propsData);
		const std::string_view svUtf8BOM(UTF8BOM);
		if (StartsWith(data, svUtf8BOM)) {
			data.remove_prefix(svUtf8BOM.length());
		}
		ReadFromMemory(data, directoryForImports, filter, imports, depth);
		return true;
	}
	return false;
}

namespace {

bool StringEqual(std::string_view a, std::string_view b, bool caseSensitive) noexcept {
	if (caseSensitive) {
		return a == b;
	} else {
		return EqualCaseInsensitive(a, b);
	}
	return true;
}

// Match file names to patterns allowing for '*' and '?'.

bool MatchWild(std::string_view pattern, std::string_view text, bool caseSensitive) {
	if (StringEqual(pattern, text, caseSensitive)) {
		return true;
	} else if (pattern.empty()) {
		return false;
	} else if (pattern.front() == '*') {
		pattern.remove_prefix(1);
		if (pattern.empty()) {
			return true;
		}
		while (!text.empty()) {
			if (MatchWild(pattern, text, caseSensitive)) {
				return true;
			}
			text.remove_prefix(1);
		}
	} else if (text.empty()) {
		return false;
	} else if (pattern.front() == '?') {
		pattern.remove_prefix(1);
		text.remove_prefix(1);
		return MatchWild(pattern, text, caseSensitive);
	} else if (caseSensitive && pattern.front() == text.front()) {
		pattern.remove_prefix(1);
		text.remove_prefix(1);
		return MatchWild(pattern, text, caseSensitive);
	} else if (!caseSensitive && MakeUpperCase(pattern.front()) == MakeUpperCase(text.front())) {
		pattern.remove_prefix(1);
		text.remove_prefix(1);
		return MatchWild(pattern, text, caseSensitive);
	}
	return false;
}

bool MatchWildSet(std::string_view patternSet, std::string_view text, bool caseSensitive) {
	while (!patternSet.empty()) {
		const size_t sepPos = patternSet.find_first_of(';');
		const std::string_view pattern = patternSet.substr(0, sepPos);
		if (MatchWild(pattern, text, caseSensitive)) {
			return true;
		}
		// Move to next
		patternSet = (sepPos == std::string_view::npos) ? "" : patternSet.substr(sepPos + 1);
	}
	return false;
}

}

std::string_view PropSetFile::GetWildUsingStart(const PropSetFile &psStart, std::string_view keybase, std::string_view filename) const {
	const PropSetFile *psf = this;
	while (psf) {
		mapss::const_iterator it = psf->props.lower_bound(keybase);
		while ((it != psf->props.end()) && StartsWith(it->first, keybase)) {
			if (it->first == keybase) {
				return it->second;
			}
			const std::string_view first = it->first;
			const std::string_view orgkeyfile = first.substr(keybase.length());
			std::string key;	// keyFile may point into key so key lifetime must cover keyFile
			std::string_view keyFile = orgkeyfile;

			if (StartsWith(orgkeyfile, "$(")) {
				// $(X) is a variable so extract X and find its value
				const size_t endVar = orgkeyfile.find_first_of(')');
				if (endVar != std::string_view::npos) {
					const std::string_view var = orgkeyfile.substr(2, endVar-2);
					key = psStart.GetExpandedString(var);
					keyFile = key;
				}
			}

			if (MatchWildSet(keyFile, filename, caseSensitiveFilenames)) {
				return it->second;
			}

			++it;
		}
		// Failed here, so try in base property set
		psf = psf->superPS;
	}
	return "";
}

std::string_view PropSetFile::GetWild(std::string_view keybase, std::string_view filename) const {
	return GetWildUsingStart(*this, keybase, filename);
}

// GetNewExpandString does not use Expand as it has to use GetWild with the filename for each
// variable reference found.

std::string PropSetFile::GetNewExpandString(std::string_view keybase, std::string_view filename) const {
	std::string withVars(GetWild(keybase, filename));
	size_t varStart = withVars.find("$(");
	int maxExpands = 1000;	// Avoid infinite expansion of recursive definitions
	while ((varStart != std::string::npos) && (maxExpands > 0)) {
		const size_t varEnd = withVars.find(')', varStart+2);
		if (varEnd == std::string::npos) {
			break;
		}
		const std::string_view var = std::string_view(withVars).substr(
			varStart + 2, varEnd - varStart - 2);	// Subtract the $(
		std::string val;
		if (var != keybase)	// Self-references evaluate to empty string
			val = GetWild(var, filename);
		withVars.replace(varStart, varEnd - varStart + 1, val);
		varStart = withVars.find("$(");
		maxExpands--;
	}
	return withVars;
}

/**
 * Initiate enumeration.
 */
bool PropSetFile::GetFirst(const char *&key, const char *&val) const {
	const mapss::const_iterator it = props.begin();
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
bool PropSetFile::GetNext(const char *&key, const char *&val) const {
	mapss::const_iterator it = props.find(key);
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
	const FilePath ext = filename.Extension();
	return EqualCaseInsensitive(ext.AsUTF8(), extensionProperties + 1);
}
