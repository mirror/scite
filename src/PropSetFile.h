// SciTE - Scintilla based Text Editor
/** @file PropSetFile.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2009 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

/**
 */

typedef std::map<std::string, std::string> mapss;

class ImportFilter {
public:
	std::set<std::string> excludes;
	std::set<std::string> includes;
	void SetFilter(std::string sExcludes, std::string sIncludes);
	bool IsValid(std::string name) const;
};

class PropSetFile {
	bool lowerKeys;
	SString GetWildUsingStart(const PropSetFile &psStart, const char *keybase, const char *filename);
	static bool caseSensitiveFilenames;
	mapss props;
public:
	PropSetFile *superPS;
	PropSetFile(bool lowerKeys_=false);
	PropSetFile(const PropSetFile &copy);
	virtual ~PropSetFile();
	PropSetFile &operator=(const PropSetFile &assign);
	void Set(const char *key, const char *val, ptrdiff_t lenKey=-1, ptrdiff_t lenVal=-1);
	void Set(const char *keyVal);
	void Unset(const char *key, int lenKey=-1);
	bool Exists(const char *key) const;
	std::string GetString(const char *key) const;
	SString Get(const char *key) const;
	SString Evaluate(const char *key) const;
	SString GetExpanded(const char *key) const;
	SString Expand(const char *withVars, int maxExpands=100) const;
	int GetInt(const char *key, int defaultValue=0) const;
	void Clear();

	bool ReadLine(const char *data, bool ifIsTrue, FilePath directoryForImports, const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth);
	void ReadFromMemory(const char *data, size_t len, FilePath directoryForImports, const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth);
	void Import(FilePath filename, FilePath directoryForImports, const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth);
	bool Read(FilePath filename, FilePath directoryForImports, const ImportFilter &filter, std::vector<FilePath> *imports, size_t depth);
	void SetInteger(const char *key, int i);
	SString GetWild(const char *keybase, const char *filename);
	SString GetNewExpand(const char *keybase, const char *filename="");
	bool GetFirst(const char *&key, const char *&val);
	bool GetNext(const char *&key, const char *&val);
	static void SetCaseSensitiveFilenames(bool caseSensitiveFilenames_) {
		caseSensitiveFilenames = caseSensitiveFilenames_;
	}
};

#define PROPERTIES_EXTENSION	".properties"
bool IsPropertiesFile(const FilePath &filename);
