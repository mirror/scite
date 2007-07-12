// SciTE - Scintilla based Text Editor
/** @file StringList.h
 ** Definition of class holding a list of strings.
 **/
// Copyright 1998-2005 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class StringList {
public:
	// Each word contains at least one character - a empty word acts as sentinel at the end.
	char **words;
	char **wordsNoCase;
	char *list;
	int len;
	bool onlyLineEnds;	///< Delimited by any white space or only line ends
	bool sorted;
	bool sortedNoCase;
	int starts[256];
	StringList(bool onlyLineEnds_ = false) :
		words(0), wordsNoCase(0), list(0), len(0), onlyLineEnds(onlyLineEnds_),
		sorted(false), sortedNoCase(false) {}
	~StringList() { Clear(); }
	operator bool() { return len ? true : false; }
	char *operator[](int ind) { return words[ind]; }
	void Clear();
	void Set(const char *s);
	char *Allocate(int size);
	void SetFromAllocated();
	bool InList(const char *s);
	//bool InListAbbreviated(const char *s, const char marker);
	const char *GetNearestWord(const char *wordStart, int searchLen,
		bool ignoreCase = false, SString wordCharacters="", int wordIndex = -1);
	char *GetNearestWords(const char *wordStart, int searchLen,
		bool ignoreCase=false, char otherSeparator='\0', bool exactLen=false);
};

