// SciTE - Scintilla based Text Editor
/** @file StringHelpers.cxx
 ** Implementation of widely useful string functions.
 **/
// Copyright 2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <string.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "Scintilla.h"

#include "GUI.h"
#include "StringHelpers.h"

bool StartsWith(GUI::gui_string const &s, GUI::gui_string const &start) {
	return (s.size() >= start.size()) &&
		(std::equal(s.begin(), s.begin() + start.size(), start.begin()));
}

bool EndsWith(GUI::gui_string const &s, GUI::gui_string const &end) {
	return (s.size() >= end.size()) &&
		(std::equal(s.begin() + s.size() - end.size(), s.end(), end.begin()));
}

int Substitute(GUI::gui_string &s, const GUI::gui_string &sFind, const GUI::gui_string &sReplace) {
	int c = 0;
	size_t lenFind = sFind.size();
	size_t lenReplace = sReplace.size();
	size_t posFound = s.find(sFind);
	while (posFound != GUI::gui_string::npos) {
		s.replace(posFound, lenFind, sReplace);
		posFound = s.find(sFind, posFound + lenReplace);
		c++;
	}
	return c;
}

std::string StdStringFromInteger(int i) {
	char number[32];
	sprintf(number, "%0d", i);
	return std::string(number);
}

void LowerCaseAZ(char *s) {
	while (*s) {
		if (*s >= 'A' && *s <= 'Z') {
			*s = static_cast<char>(*s - 'A' + 'a');
		}
		s++;
	}
}

/**
 * Convert a string into C string literal form using \a, \b, \f, \n, \r, \t, \v, and \ooo.
 * The return value is a newly allocated character array containing the result.
 * 4 bytes are allocated for each byte of the input because that is the maximum
 * expansion needed when all of the input needs to be output using the octal form.
 * The return value should be deleted with delete[].
 */
char *Slash(const char *s, bool quoteQuotes) {
	char *oRet = new char[strlen(s) * 4 + 1];
	char *o = oRet;
	while (*s) {
		if (*s == '\a') {
			*o++ = '\\';
			*o++ = 'a';
		} else if (*s == '\b') {
			*o++ = '\\';
			*o++ = 'b';
		} else if (*s == '\f') {
			*o++ = '\\';
			*o++ = 'f';
		} else if (*s == '\n') {
			*o++ = '\\';
			*o++ = 'n';
		} else if (*s == '\r') {
			*o++ = '\\';
			*o++ = 'r';
		} else if (*s == '\t') {
			*o++ = '\\';
			*o++ = 't';
		} else if (*s == '\v') {
			*o++ = '\\';
			*o++ = 'v';
		} else if (*s == '\\') {
			*o++ = '\\';
			*o++ = '\\';
		} else if (quoteQuotes && (*s == '\'')) {
			*o++ = '\\';
			*o++ = '\'';
		} else if (quoteQuotes && (*s == '\"')) {
			*o++ = '\\';
			*o++ = '\"';
		} else if (isascii(*s) && (*s < ' ')) {
			*o++ = '\\';
			*o++ = static_cast<char>((*s >> 6) + '0');
			*o++ = static_cast<char>((*s >> 3) + '0');
			*o++ = static_cast<char>((*s & 0x7) + '0');
		} else {
			*o++ = *s;
		}
		s++;
	}
	*o = '\0';
	return oRet;
}

/**
 * Is the character an octal digit?
 */
static bool IsOctalDigit(char ch) {
	return ch >= '0' && ch <= '7';
}

/**
 * If the character is an hexa digit, get its value.
 */
static int GetHexaDigit(char ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'A' && ch <= 'F') {
		return ch - 'A' + 10;
	}
	if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	}
	return -1;
}

/**
 * Convert C style \a, \b, \f, \n, \r, \t, \v, \ooo and \xhh into their indicated characters.
 */
unsigned int UnSlash(char *s) {
	char *sStart = s;
	char *o = s;

	while (*s) {
		if (*s == '\\') {
			s++;
			if (*s == 'a') {
				*o = '\a';
			} else if (*s == 'b') {
				*o = '\b';
			} else if (*s == 'f') {
				*o = '\f';
			} else if (*s == 'n') {
				*o = '\n';
			} else if (*s == 'r') {
				*o = '\r';
			} else if (*s == 't') {
				*o = '\t';
			} else if (*s == 'v') {
				*o = '\v';
			} else if (IsOctalDigit(*s)) {
				int val = *s - '0';
				if (IsOctalDigit(*(s + 1))) {
					s++;
					val *= 8;
					val += *s - '0';
					if (IsOctalDigit(*(s + 1))) {
						s++;
						val *= 8;
						val += *s - '0';
					}
				}
				*o = static_cast<char>(val);
			} else if (*s == 'x') {
				s++;
				int val = 0;
				int ghd = GetHexaDigit(*s);
				if (ghd >= 0) {
					s++;
					val = ghd;
					ghd = GetHexaDigit(*s);
					if (ghd >= 0) {
						s++;
						val *= 16;
						val += ghd;
					}
				}
				*o = static_cast<char>(val);
			} else {
				*o = *s;
			}
		} else {
			*o = *s;
		}
		o++;
		if (*s) {
			s++;
		}
	}
	*o = '\0';
	return static_cast<unsigned int>(o - sStart);
}

/**
 * Convert C style \0oo into their indicated characters.
 * This is used to get control characters into the regular expresion engine.
 */
unsigned int UnSlashLowOctal(char *s) {
	char *sStart = s;
	char *o = s;
	while (*s) {
		if ((s[0] == '\\') && (s[1] == '0') && IsOctalDigit(s[2]) && IsOctalDigit(s[3])) {
			*o = static_cast<char>(8 * (s[2] - '0') + (s[3] - '0'));
			s += 3;
		} else {
			*o = *s;
		}
		o++;
		if (*s)
			s++;
	}
	*o = '\0';
	return static_cast<unsigned int>(o - sStart);
}
