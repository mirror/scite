// SciTE - Scintilla based Text Editor
/** @file StringList.h
 ** Definition of class holding a list of strings.
 **/
// Copyright 1998-2005 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class StringList {
	// Text pointed into by words and wordsNoCase
	std::string listText;
	// Each word contains at least one character.
	std::vector<char *> words;
	std::vector<char *> wordsNoCase;
	bool onlyLineEnds;	///< Delimited by any white space or only line ends
	bool sorted;
	bool sortedNoCase;
	void SetFromListText();
	void SortIfNeeded(bool ignoreCase);
public:
	explicit StringList(bool onlyLineEnds_ = false) :
		words(0), wordsNoCase(0), onlyLineEnds(onlyLineEnds_),
		sorted(false), sortedNoCase(false) {}
	size_t Length() const noexcept { return words.size(); }
	operator bool() const noexcept { return !words.empty(); }
	char *operator[](size_t ind) { return words[ind]; }
	void Clear() noexcept;
	void Set(const char *s);
	void Set(const std::vector<char> &data);
	std::string GetNearestWord(const char *wordStart, size_t searchLen,
				   bool ignoreCase, const std::string &wordCharacters, int wordIndex);
	std::string GetNearestWords(const char *wordStart, size_t searchLen,
				    bool ignoreCase, char otherSeparator='\0', bool exactLen=false);
};
