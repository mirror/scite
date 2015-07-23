// SciTE - Scintilla based Text Editor
/** @file ExportRTF.cxx
 ** Export the current document to RTF.
 **/
// Copyright 1998-2006 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"

#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "StyleDefinition.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"


//---------- Save to RTF ----------

#define RTF_HEADEROPEN "{\\rtf1\\ansi\\deff0\\deftab720"
#define RTF_FONTDEFOPEN "{\\fonttbl"
#define RTF_FONTDEFCLOSE "}"
#define RTF_COLORDEFOPEN "{\\colortbl"
#define RTF_COLORDEFCLOSE "}"
#define RTF_HEADERCLOSE "\n"
#define RTF_BODYOPEN ""
#define RTF_BODYCLOSE "}"

#define RTF_SETFONTFACE "\\f"
#define RTF_SETFONTSIZE "\\fs"
#define RTF_SETCOLOR "\\cf"
#define RTF_SETBACKGROUND "\\highlight"
#define RTF_BOLD_ON "\\b"
#define RTF_BOLD_OFF "\\b0"
#define RTF_ITALIC_ON "\\i"
#define RTF_ITALIC_OFF "\\i0"
#define RTF_UNDERLINE_ON "\\ul"
#define RTF_UNDERLINE_OFF "\\ulnone"
#define RTF_STRIKE_ON "\\i"
#define RTF_STRIKE_OFF "\\strike0"

#define RTF_EOLN "\\par\n"
#define RTF_TAB "\\tab "

#define MAX_STYLEDEF 128
#define MAX_FONTDEF 64
#define MAX_COLORDEF 8
#define RTF_FONTFACE "Courier New"
#define RTF_COLOR "#000000"

// extract the next RTF control word from *style
void GetRTFNextControl(char **style, char *control) {
	ptrdiff_t len;
	char *pos = *style;
	*control = '\0';
	if ('\0' == *pos) return;
	pos++; // implicit skip over leading '\'
	while ('\0' != *pos && '\\' != *pos) { pos++; }
	len = pos - *style;
	memcpy(control, *style, len);
	*(control + len) = '\0';
	*style = pos;
}

// extracts control words that are different between two styles
void GetRTFStyleChange(char *delta, char *last, char *current) { // \f0\fs20\cf0\highlight0\b0\i0
	char lastControl[MAX_STYLEDEF], currentControl[MAX_STYLEDEF];
	char *lastPos = last;
	char *currentPos = current;
	*delta = '\0';
	// font face, size, color, background, bold, italic
	for (int i = 0; i < 6; i++) {
		GetRTFNextControl(&lastPos, lastControl);
		GetRTFNextControl(&currentPos, currentControl);
		if (strcmp(lastControl, currentControl)) {	// changed
			strcat(delta, currentControl);
		}
	}
	if ('\0' != *delta) { strcat(delta, " "); }
	strcpy(last, current);
}

void SciTEBase::SaveToStreamRTF(std::ostream &os, int start, int end) {
	int lengthDoc = LengthDocument();
	if (end < 0)
		end = lengthDoc;
	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);

	StyleDefinition defaultStyle = StyleDefinitionFor(STYLE_DEFAULT);

	int tabSize = props.GetInt("export.rtf.tabsize", props.GetInt("tabsize"));
	int wysiwyg = props.GetInt("export.rtf.wysiwyg", 1);
	std::string fontFace = props.GetExpandedString("export.rtf.font.face");
	if (fontFace.length()) {
		defaultStyle.font = fontFace;
	} else if (defaultStyle.font.length() == 0) {
		defaultStyle.font = RTF_FONTFACE;
	}
	int fontSize = props.GetInt("export.rtf.font.size", 0);
	if (fontSize > 0) {
		defaultStyle.size = fontSize << 1;
	} else if (defaultStyle.size == 0) {
		defaultStyle.size = 10 << 1;
	} else {
		defaultStyle.size <<= 1;
	}
	unsigned int characterset = props.GetInt("character.set", SC_CHARSET_DEFAULT);
	int tabs = props.GetInt("export.rtf.tabs", 0);
	if (tabSize == 0)
		tabSize = 4;

	char styles[STYLE_MAX + 1][MAX_STYLEDEF];
	char fonts[STYLE_MAX + 1][MAX_FONTDEF];
	char colors[STYLE_MAX + 1][MAX_COLORDEF];
	char lastStyle[MAX_STYLEDEF], deltaStyle[MAX_STYLEDEF];
	int fontCount = 1, colorCount = 2, i;
	os << RTF_HEADEROPEN << RTF_FONTDEFOPEN;
	StringCopy(fonts[0], defaultStyle.font.c_str());
	os << "{\\f" << 0 << "\\fnil\\fcharset" << characterset << " " << defaultStyle.font.c_str() << ";}";
	StringCopy(colors[0], defaultStyle.fore.c_str());
	StringCopy(colors[1], defaultStyle.back.c_str());

	for (int istyle = 0; istyle <= STYLE_MAX; istyle++) {

		StyleDefinition sd = StyleDefinitionFor(istyle);

		if (sd.specified != StyleDefinition::sdNone) {
			if (wysiwyg && sd.font.length()) {
				for (i = 0; i < fontCount; i++)
					if (EqualCaseInsensitive(sd.font.c_str(), fonts[i]))
						break;
				if (i >= fontCount) {
					StringCopy(fonts[fontCount++], sd.font.c_str());
					os << "{\\f" << i << "\\fnil\\fcharset" << characterset << " " << sd.font.c_str() << ";}";
				}
				sprintf(lastStyle, RTF_SETFONTFACE "%d", i);
			} else {
				strcpy(lastStyle, RTF_SETFONTFACE "0");
			}

			sprintf(lastStyle + strlen(lastStyle), RTF_SETFONTSIZE "%d",
				wysiwyg && sd.size ? sd.size << 1 : defaultStyle.size);

			if (sd.specified & StyleDefinition::sdFore) {
				for (i = 0; i < colorCount; i++)
					if (EqualCaseInsensitive(sd.fore.c_str(), colors[i]))
						break;
				if (i >= colorCount)
					StringCopy(colors[colorCount++], sd.fore.c_str());
				sprintf(lastStyle + strlen(lastStyle), RTF_SETCOLOR "%d", i);
			} else {
				strcat(lastStyle, RTF_SETCOLOR "0");	// Default fore
			}

			// PL: highlights doesn't seems to follow a distinct table, at least with WordPad and Word 97
			// Perhaps it is different for Word 6?
//				sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d",
//				        sd.back.length() ? GetRTFHighlight(sd.back.c_str()) : 0);
			if (sd.specified & StyleDefinition::sdBack) {
				for (i = 0; i < colorCount; i++)
					if (EqualCaseInsensitive(sd.back.c_str(), colors[i]))
						break;
				if (i >= colorCount)
					StringCopy(colors[colorCount++], sd.back.c_str());
				sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d", i);
			} else {
				strcat(lastStyle, RTF_SETBACKGROUND "1");	// Default back
			}
			if (sd.specified & StyleDefinition::sdWeight) {
				strcat(lastStyle, sd.IsBold() ? RTF_BOLD_ON : RTF_BOLD_OFF);
			} else {
				strcat(lastStyle, defaultStyle.IsBold() ? RTF_BOLD_ON : RTF_BOLD_OFF);
			}
			if (sd.specified & StyleDefinition::sdItalics) {
				strcat(lastStyle, sd.italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
			} else {
				strcat(lastStyle, defaultStyle.italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
			}
			StringCopy(styles[istyle], lastStyle);
		} else {
			sprintf(styles[istyle], RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
				RTF_SETCOLOR "0" RTF_SETBACKGROUND "1"
				RTF_BOLD_OFF RTF_ITALIC_OFF, defaultStyle.size);
		}
	}
	os << RTF_FONTDEFCLOSE RTF_COLORDEFOPEN;
	for (i = 0; i < colorCount; i++) {
		os << "\\red" << IntFromHexByte(colors[i] + 1) << "\\green" << IntFromHexByte(colors[i] + 3) <<
			"\\blue" << IntFromHexByte(colors[i] + 5) << ";";
	}
	os << RTF_COLORDEFCLOSE RTF_HEADERCLOSE RTF_BODYOPEN RTF_SETFONTFACE "0"
		RTF_SETFONTSIZE << defaultStyle.size << RTF_SETCOLOR "0 ";
	sprintf(lastStyle, RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
		RTF_SETCOLOR "0" RTF_SETBACKGROUND "1"
		RTF_BOLD_OFF RTF_ITALIC_OFF, defaultStyle.size);
	bool prevCR = false;
	int styleCurrent = -1;
	TextReader acc(wEditor);
	int column = 0;
	for (i = start; i < end; i++) {
		char ch = acc[i];
		int style = acc.StyleAt(i);
		if (style > STYLE_MAX)
			style = 0;
		if (style != styleCurrent) {
			GetRTFStyleChange(deltaStyle, lastStyle, styles[style]);
			if (*deltaStyle)
				os << deltaStyle;
			styleCurrent = style;
		}
		if (ch == '{')
			os << "\\{";
		else if (ch == '}')
			os << "\\}";
		else if (ch == '\\')
			os << "\\\\";
		else if (ch == '\t') {
			if (tabs) {
				os << RTF_TAB;
			} else {
				int ts = tabSize - (column % tabSize);
				for (int itab = 0; itab < ts; itab++) {
					os << ' ';
				}
				column += ts - 1;
			}
		} else if (ch == '\n') {
			if (!prevCR) {
				os << RTF_EOLN;
				column = -1;
			}
		} else if (ch == '\r') {
			os << RTF_EOLN;
			column = -1;
		} else
			os << ch;
		column++;
		prevCR = ch == '\r';
	}
	os << RTF_BODYCLOSE;
}

void SciTEBase::SaveToRTF(FilePath saveName, int start, int end) {
	FILE *fp = saveName.Open(GUI_TEXT("wt"));
	if (fp) {
		std::ostringstream oss;
		SaveToStreamRTF(oss, start, end);
		std::string rtf = oss.str();
		fwrite(rtf.c_str(), 1, rtf.length(), fp);
		fclose(fp);
	} else {
		GUI::gui_string msg = LocaliseMessage("Could not save file '^0'.", filePath.AsInternal());
		WindowMessageBox(wSciTE, msg);
	}
}

