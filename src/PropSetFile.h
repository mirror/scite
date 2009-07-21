// SciTE - Scintilla based Text Editor
/** @file PropSetFile.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2009 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

/**
 */

typedef std::map<std::string, std::string> mapss;

class PropSetFile : public PropertyGet {
	bool lowerKeys;
	SString GetWildUsingStart(const PropSetFile &psStart, const char *keybase, const char *filename);
	static bool caseSensitiveFilenames;
	mapss props;
	std::string enumnext;
public:
	PropSetFile *superPS;
	PropSetFile(bool lowerKeys_=false);
	virtual ~PropSetFile();
	void Set(const char *key, const char *val, int lenKey=-1, int lenVal=-1);
	void Set(const char *keyVal);
	void Unset(const char *key, int lenKey=-1);
	void SetMultiple(const char *s);
	SString Get(const char *key) const;
	SString GetExpanded(const char *key) const;
	SString Expand(const char *withVars, int maxExpands=100) const;
	int GetInt(const char *key, int defaultValue=0) const;
	void Clear();
	char *ToString() const;	// Caller must delete[] the return value

	bool ReadLine(const char *data, bool ifIsTrue, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	void ReadFromMemory(const char *data, int len, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	bool Read(FilePath filename, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	void SetInteger(const char *key, int i);
	SString GetWild(const char *keybase, const char *filename);
	SString GetNewExpand(const char *keybase, const char *filename="");
	bool GetFirst(const char *&key, const char *&val);
	bool GetNext(const char *&key, const char *&val);
	static void SetCaseSensitiveFilenames(bool caseSensitiveFilenames_) {
		caseSensitiveFilenames = caseSensitiveFilenames_;
	}

private:
	// copy-value semantics not implemented
	PropSetFile(const PropSetFile &copy);
	void operator=(const PropSetFile &assign);
};
