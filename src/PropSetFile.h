// SciTE - Scintilla based Text Editor
/** @file PropSetFile.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2009 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef PROPSETFILE_H
#define PROPSETFILE_H

/**
 */

typedef std::map<std::string, std::string, std::less<>> mapss;

class ImportFilter {
public:
	std::set<std::string> excludes;
	std::set<std::string> includes;
	void SetFilter(const std::string &sExcludes, const std::string &sIncludes);
	bool IsValid(const std::string &name) const;
};

class PropSetFile {
	bool lowerKeys;
	std::string_view GetWildUsingStart(const PropSetFile &psStart, std::string_view keybase, std::string_view filename) const;
	static bool caseSensitiveFilenames;
	mapss props;
public:
	PropSetFile *superPS;
	explicit PropSetFile(bool lowerKeys_=false);

	void Set(std::string_view key, std::string_view val);
	void SetPath(std::string_view key, const FilePath &path);
	void SetLine(const char *keyVal, bool unescape);
	void Unset(std::string_view key);
	void Clear() noexcept;

	bool Exists(std::string_view key) const;
	[[nodiscard]] std::string_view Get(std::string_view key) const;
	std::string GetString(std::string_view key) const;
	std::string Evaluate(std::string_view key) const;
	std::string GetExpandedString(std::string_view key) const;
	std::string Expand(std::string_view withVars, int maxExpands=200) const;
	int GetInt(std::string_view key, int defaultValue=0) const;
	intptr_t GetInteger(std::string_view key, intptr_t defaultValue=0) const;
	long long GetLongLong(std::string_view key, long long defaultValue=0) const;

	enum class ReadLineState { active, excludedModule, conditionFalse };
	ReadLineState ReadLine(const std::string &lineBuffer, ReadLineState rls, const FilePath &directoryForImports, const ImportFilter &filter,
			       FilePathSet *imports, size_t depth);
	void ReadFromMemory(std::string_view data, const FilePath &directoryForImports, const ImportFilter &filter,
			    FilePathSet *imports, size_t depth);
	void Import(const FilePath &filename, const FilePath &directoryForImports, const ImportFilter &filter,
		    FilePathSet *imports, size_t depth);
	bool Read(const FilePath &filename, const FilePath &directoryForImports, const ImportFilter &filter,
		  FilePathSet *imports, size_t depth);
	std::string_view GetWild(std::string_view keybase, std::string_view filename) const;
	std::string GetNewExpandString(std::string_view keybase, std::string_view filename = "") const;
	bool GetFirst(const char *&key, const char *&val) const;
	bool GetNext(const char *&key, const char *&val) const;
	static void SetCaseSensitiveFilenames(bool caseSensitiveFilenames_) noexcept {
		caseSensitiveFilenames = caseSensitiveFilenames_;
	}
};

constexpr const char *extensionProperties = ".properties";
bool IsPropertiesFile(const FilePath &filename);

#endif
