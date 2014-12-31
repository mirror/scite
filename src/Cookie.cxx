// SciTE - Scintilla based Text Editor
/** @file Cookie.cxx
 ** Examine start of files for coding cookies and type information.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "Scintilla.h"

#include "GUI.h"

#include "StringHelpers.h"
#include "Cookie.h"

std::string ExtractLine(const char *buf, size_t length) {
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
	return std::string(buf, endl);
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

static UniMode CookieValue(const std::string &s) {
	size_t posCoding = s.find(codingCookie);
	if (posCoding != std::string::npos) {
		posCoding += strlen(codingCookie);
		if ((s[posCoding] == ':') || (s[posCoding] == '=')) {
			posCoding++;
			if ((s[posCoding] == '\"') || (s[posCoding] == '\'')) {
				posCoding++;
			}
			while ((posCoding < s.length()) &&
			        (isSpaceChar(s[posCoding]))) {
				posCoding++;
			}
			size_t endCoding = posCoding;
			while ((endCoding < s.length()) &&
			        (isEncodingChar(s[endCoding]))) {
				endCoding++;
			}
			std::string code(s, posCoding, endCoding-posCoding);
			LowerCaseAZ(code);
			if (code == "utf-8") {
				return uniCookie;
			}
		}
	}
	return uni8Bit;
}

UniMode CodingCookieValue(const char *buf, size_t length) {
	std::string l1 = ExtractLine(buf, length);
	UniMode unicodeMode = CookieValue(l1);
	if (unicodeMode == uni8Bit) {
		std::string l2 = ExtractLine(buf + l1.length(), length - l1.length());
		unicodeMode = CookieValue(l2);
	}
	return unicodeMode;
}

