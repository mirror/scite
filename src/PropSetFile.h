// SciTE - Scintilla based Text Editor
/** @file PropSetFile.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class PropSetFile : public PropSet {
	bool lowerKeys;
public:
	PropSetFile(bool lowerKeys_=false);
	~PropSetFile();
	bool ReadLine(const char *data, bool ifIsTrue, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	void ReadFromMemory(const char *data, int len, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	bool Read(FilePath filename, FilePath directoryForImports, FilePath imports[] = 0, int sizeImports = 0);
	void SetInteger(const char *key, sptr_t i);
};
