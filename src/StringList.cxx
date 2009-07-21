// SciTE - Scintilla based Text Editor
/** @file StringList.cxx
 ** IMplementation of class holding a list of strings.
 **/
// Copyright 1998-2005 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <map>

#include "SString.h"
#include "StringList.h"
#include "PropSet.h"

static inline bool IsASpace(unsigned int ch) {
    return (ch == ' ') || ((ch >= 0x09) && (ch <= 0x0d));
}

static inline char MakeUpperCase(char ch) {
	if (ch < 'a' || ch > 'z')
		return ch;
	else
		return static_cast<char>(ch - 'a' + 'A');
}

static int CompareNCaseInsensitive(const char *a, const char *b, size_t len) {
	while (*a && *b && len) {
		if (*a != *b) {
			char upperA = MakeUpperCase(*a);
			char upperB = MakeUpperCase(*b);
			if (upperA != upperB)
				return upperA - upperB;
		}
		a++;
		b++;
		len--;
	}
	if (len == 0)
		return 0;
	else
		// Either *a or *b is nul
		return *a - *b;
}

/**
 * Creates an array that points into each word in the string and puts \0 terminators
 * after each word.
 */
static char **ArrayFromStringList(char *StringList, int *len, bool onlyLineEnds = false) {
	int prev = '\n';
	int words = 0;
	// For rapid determination of whether a character is a separator, build
	// a look up table.
	bool wordSeparator[256];
	for (int i=0;i<256; i++) {
		wordSeparator[i] = false;
	}
	wordSeparator['\r'] = true;
	wordSeparator['\n'] = true;
	if (!onlyLineEnds) {
		wordSeparator[' '] = true;
		wordSeparator['\t'] = true;
	}
	for (int j = 0; StringList[j]; j++) {
		int curr = static_cast<unsigned char>(StringList[j]);
		if (!wordSeparator[curr] && wordSeparator[prev])
			words++;
		prev = curr;
	}
	char **keywords = new char *[words + 1];
	words = 0;
	prev = '\0';
	size_t slen = strlen(StringList);
	for (size_t k = 0; k < slen; k++) {
		if (!wordSeparator[static_cast<unsigned char>(StringList[k])]) {
			if (!prev) {
				keywords[words] = &StringList[k];
				words++;
			}
		} else {
			StringList[k] = '\0';
		}
		prev = StringList[k];
	}
	keywords[words] = &StringList[slen];
	*len = words;
	return keywords;
}

void StringList::Clear() {
	if (words) {
		delete []list;
		delete []words;
		delete []wordsNoCase;
	}
	words = 0;
	wordsNoCase = 0;
	list = 0;
	len = 0;
	sorted = false;
	sortedNoCase = false;
}

void StringList::Set(const char *s) {
	list = StringDup(s);
	sorted = false;
	sortedNoCase = false;
	words = ArrayFromStringList(list, &len, onlyLineEnds);
	wordsNoCase = new char * [len + 1];
	memcpy(wordsNoCase, words, (len + 1) * sizeof (*words));
}

char *StringList::Allocate(int size) {
	list = new char[size + 1];
	list[size] = '\0';
	return list;
}

void StringList::SetFromAllocated() {
	sorted = false;
	sortedNoCase = false;
	words = ArrayFromStringList(list, &len, onlyLineEnds);
	wordsNoCase = new char * [len + 1];
	memcpy(wordsNoCase, words, (len + 1) * sizeof (*words));
}

extern "C" int slCmpString(const void *a1, const void *a2) {
	// Can't work out the correct incantation to use modern casts here
	return strcmp(*(char**)(a1), *(char**)(a2));
}

extern "C" int slCmpStringNoCase(const void *a1, const void *a2) {
	// Can't work out the correct incantation to use modern casts here
	return CompareNoCase(*(char**)(a1), *(char**)(a2));
}

static void SortStringList(char **words, unsigned int len) {
	qsort(reinterpret_cast<void*>(words), len, sizeof(*words),
	      slCmpString);
}

static void SortStringListNoCase(char **wordsNoCase, unsigned int len) {
	qsort(reinterpret_cast<void*>(wordsNoCase), len, sizeof(*wordsNoCase),
	      slCmpStringNoCase);
}

bool StringList::InList(const char *s) {
	if (0 == words)
		return false;
	if (!sorted) {
		sorted = true;
		SortStringList(words, len);
		for (unsigned int k = 0; k < (sizeof(starts) / sizeof(starts[0])); k++)
			starts[k] = -1;
		for (int l = len - 1; l >= 0; l--) {
			unsigned char indexChar = words[l][0];
			starts[indexChar] = l;
		}
	}
	unsigned char firstChar = s[0];
	int j = starts[firstChar];
	if (j >= 0) {
		while ((unsigned char)words[j][0] == firstChar) {
			if (s[1] == words[j][1]) {
				const char *a = words[j] + 1;
				const char *b = s + 1;
				while (*a && *a == *b) {
					a++;
					b++;
				}
				if (!*a && !*b)
					return true;
			}
			j++;
		}
	}
	j = starts['^'];
	if (j >= 0) {
		while (words[j][0] == '^') {
			const char *a = words[j] + 1;
			const char *b = s;
			while (*a && *a == *b) {
				a++;
				b++;
			}
			if (!*a)
				return true;
			j++;
		}
	}
	return false;
}

/**
 * Returns an element (complete) of the StringList array which has
 * the same beginning as the passed string.
 * The length of the word to compare is passed too.
 * Letter case can be ignored or preserved (default).
 */
const char *StringList::GetNearestWord(const char *wordStart, int searchLen, bool ignoreCase /*= false*/, SString wordCharacters /*='/0' */, int wordIndex /*= -1 */) {
	int start = 0; // lower bound of the api array block to search
	int end = len - 1; // upper bound of the api array block to search
	int pivot; // index of api array element just being compared
	int cond; // comparison result (in the sense of strcmp() result)
	const char *word; // api array element just being compared

	if (0 == words)
		return NULL;
	if (ignoreCase) {
		if (!sortedNoCase) {
			sortedNoCase = true;
			SortStringListNoCase(wordsNoCase, len);
		}
		while (start <= end) { // binary searching loop
			pivot = (start + end) >> 1;
			word = wordsNoCase[pivot];
			cond = CompareNCaseInsensitive(wordStart, word, searchLen);
			if (!cond) {
				// find first word
				start = pivot;
				while (start > 0 && !CompareNCaseInsensitive(wordStart, wordsNoCase[start-1], searchLen)) {
					start--;
				}
				// find last word
				end = pivot;
				while (end < len-1 && !CompareNCaseInsensitive(wordStart, wordsNoCase[end+1], searchLen)) {
					end++;
				}

				// Finds first word in a series of equal words
				for (pivot = start; pivot <= end; pivot++) {
					word = wordsNoCase[pivot];
					if (!wordCharacters.contains(word[searchLen])) {
						if (wordIndex <= 0) // Checks if a specific index was requested
							return word; // result must not be freed with free()
						wordIndex--;
					}
				}
				return NULL;
			}
			else if (cond > 0)
				start = pivot + 1;
			else if (cond < 0)
				end = pivot - 1;
		}
	} else { // preserve the letter case
		if (!sorted) {
			sorted = true;
			SortStringList(words, len);
		}
		while (start <= end) { // binary searching loop
			pivot = (start + end) >> 1;
			word = words[pivot];
			cond = strncmp(wordStart, word, searchLen);
			if (!cond) {
				// find first word
				start = pivot;
				while (start > 0 && !strncmp(wordStart, words[start-1], searchLen)) {
					start--;
				}
				// find last word
				end = pivot;
				while (end < len-1 && !strncmp(wordStart, words[end+1], searchLen)) {
					end++;
				}

				// Finds first word in a series of equal words
				pivot = start;
				while (pivot <= end) {
					word = words[pivot];
					if (!wordCharacters.contains(word[searchLen])) {
						if (wordIndex <= 0) // Checks if a specific index was requested
							return word; // result must not be freed with free()
						wordIndex--;
					}
					pivot++;
				}
				return NULL;
			}
			else if (cond > 0)
				start = pivot + 1;
			else if (cond < 0)
				end = pivot - 1;
		}
	}
	return NULL;
}

/**
 * Find the length of a 'word' which is actually an identifier in a string
 * which looks like "identifier(..." or "identifier" and where
 * there may be extra spaces after the identifier that should not be
 * counted in the length.
 */
static unsigned int LengthWord(const char *word, char otherSeparator) {
	const char *endWord = 0;
	// Find an otherSeparator
	if (otherSeparator)
		endWord = strchr(word, otherSeparator);
	// Find a '('. If that fails go to the end of the string.
	if (!endWord)
		endWord = strchr(word, '(');
	if (!endWord)
		endWord = word + strlen(word);
	// Last case always succeeds so endWord != 0

	// Drop any space characters.
	if (endWord > word) {
		endWord--;	// Back from the '(', otherSeparator, or '\0'
		// Move backwards over any spaces
		while ((endWord > word) && (IsASpace(*endWord))) {
			endWord--;
		}
	}
	return endWord - word;
}

/**
 * Returns elements (first words of them) of the StringList array which have
 * the same beginning as the passed string.
 * The length of the word to compare is passed too.
 * Letter case can be ignored or preserved (default).
 * If there are more words meeting the condition they are returned all of
 * them in the ascending order separated with spaces.
 *
 * NOTE: returned buffer has to be freed with delete[].
 */
char *StringList::GetNearestWords(
    const char *wordStart,
    int searchLen,
    bool ignoreCase /*= false*/,
    char otherSeparator /*= '\0'*/,
    bool exactLen /*=false*/) {
	unsigned int wordlen; // length of the word part (before the '(' brace) of the api array element
	SString wordsNear;
	wordsNear.setsizegrowth(1000);
	int start = 0; // lower bound of the api array block to search
	int end = len - 1; // upper bound of the api array block to search
	int pivot; // index of api array element just being compared
	int cond; // comparison result (in the sense of strcmp() result)

	if (0 == words)
		return NULL;
	if (ignoreCase) {
		if (!sortedNoCase) {
			sortedNoCase = true;
			SortStringListNoCase(wordsNoCase, len);
		}
		while (start <= end) { // Binary searching loop
			pivot = (start + end) / 2;
			cond = CompareNCaseInsensitive(wordStart, wordsNoCase[pivot], searchLen);
			if (!cond) {
				// Find first match
				while ((pivot > start) &&
					(0 == CompareNCaseInsensitive(wordStart,
						wordsNoCase[pivot-1], searchLen))) {
					--pivot;
				}
				// Grab each match
				while ((pivot <= end) &&
					(0 == CompareNCaseInsensitive(wordStart,
						wordsNoCase[pivot], searchLen))) {
					wordlen = LengthWord(wordsNoCase[pivot], otherSeparator) + 1;
					++pivot;
					if (exactLen && wordlen != LengthWord(wordStart, otherSeparator) + 1)
						continue;
					wordsNear.append(wordsNoCase[pivot-1], wordlen, ' ');
				}
				return wordsNear.detach();
			} else if (cond < 0) {
				end = pivot - 1;
			} else if (cond > 0) {
				start = pivot + 1;
			}
		}
	} else {	// Preserve the letter case
		if (!sorted) {
			sorted = true;
			SortStringList(words, len);
		}
		while (start <= end) { // Binary searching loop
			pivot = (start + end) / 2;
			cond = strncmp(wordStart, words[pivot], searchLen);
			if (!cond) {
				// Find first match
				while ((pivot > start) &&
					(0 == strncmp(wordStart,
						words[pivot-1], searchLen))) {
					--pivot;
				}
				// Grab each match
				while ((pivot <= end) &&
					(0 == strncmp(wordStart,
						words[pivot], searchLen))) {
					wordlen = LengthWord(words[pivot], otherSeparator) + 1;
					++pivot;
					if (exactLen && wordlen != LengthWord(wordStart, otherSeparator) + 1)
						continue;
					wordsNear.append(words[pivot-1], wordlen, ' ');
				}
				return wordsNear.detach();
			} else if (cond < 0) {
				end = pivot - 1;
			} else if (cond > 0) {
				start = pivot + 1;
			}
		}
	}
	return NULL;
}

#ifdef _MSC_VER
// Unreferenced inline functions are OK
#pragma warning(disable: 4514)
#endif
