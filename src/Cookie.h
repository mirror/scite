// SciTE - Scintilla based Text Editor
/** @file Cookie.h
 ** Examine start of files for coding cookies and type information.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

// Related to Utf8_16::encodingType but with additional values at end
enum UniMode {
    uni8Bit = 0, uni16BE = 1, uni16LE = 2, uniUTF8 = 3,
    uniCookie = 4
};

SString ExtractLine(const char *buf, size_t length);
UniMode CodingCookieValue(const char *buf, size_t length);
