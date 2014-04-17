// SciTE - Scintilla based Text Editor
/** @file Cookie.cxx
 ** Examine start of files for coding cookies and type information.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>

#include "SString.h"
#include "Cookie.h"

SString ExtractLine(const char *buf, size_t length) {
	unsigned int endl = 0;
	if (length > 0) {
		while ((endl < length) && (buf[endl] != '\r') && (buf[endl] != '\n')) {
			endl++;
		}
		if (((endl + 1) < length) && (buf[endl] == '\r') && (buf[endl+1] == '\n')) {
			endl++;
		}
		if (endl < length) {
			endl++;
		}
	}
	return SString(buf, 0, endl);
}

static const char codingCookie[] = "coding";

static bool isEncodingChar(char ch) {
	return (ch == '_') || (ch == '-') || (ch == '.') ||
	       (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
	       (ch >= '0' && ch <= '9');
}

static bool isSpaceChar(char ch) {
	return (ch == ' ') || (ch == '\t');
}

static UniMode CookieValue(const SString &s) {
	int posCoding = s.search(codingCookie);
	if (posCoding >= 0) {
		posCoding += static_cast<int>(strlen(codingCookie));
		if ((s[posCoding] == ':') || (s[posCoding] == '=')) {
			posCoding++;
			if ((s[posCoding] == '\"') || (s[posCoding] == '\'')) {
				posCoding++;
			}
			while ((posCoding < static_cast<int>(s.length())) &&
			        (isSpaceChar(s[posCoding]))) {
				posCoding++;
			}
			size_t endCoding = static_cast<size_t>(posCoding);
			while ((endCoding < s.length()) &&
			        (isEncodingChar(s[endCoding]))) {
				endCoding++;
			}
			SString code(s.c_str(), posCoding, endCoding);
			code.lowercase();
			if (code == "utf-8") {
				return uniCookie;
			}
		}
	}
	return uni8Bit;
}

UniMode CodingCookieValue(const char *buf, size_t length) {
	SString l1 = ExtractLine(buf, length);
	UniMode unicodeMode = CookieValue(l1);
	if (unicodeMode == uni8Bit) {
		SString l2 = ExtractLine(buf + l1.length(), length - l1.length());
		unicodeMode = CookieValue(l2);
	}
	return unicodeMode;
}

