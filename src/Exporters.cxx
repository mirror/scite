// SciTE - Scintilla based Text Editor
/** @file Exporters.cxx
 ** Export the current document to various markup languages.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>
#include <gtk/gtk.h>

#endif

#if PLAT_WIN

#define _WIN32_WINNT  0x0400
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
#endif
#include <commctrl.h>

// For chdir
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif

#endif

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "WindowAccessor.h"
#include "Scintilla.h"
#include "Extender.h"
#include "SciTEBase.h"


//---------- Save to RTF ----------

#define RTF_HEADEROPEN "{\\rtf1\\ansi\\deff0\\deftab720"
#define RTF_FONTDEFOPEN "{\\fonttbl"
#define RTF_FONTDEF "{\\f%d\\fnil\\fcharset%u %s;}"
#define RTF_FONTDEFCLOSE "}"
#define RTF_COLORDEFOPEN "{\\colortbl"
#define RTF_COLORDEF "\\red%d\\green%d\\blue%d;"
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

// PL: if my change below is kept, this function is to be removed.
int GetRTFHighlight(const char *rgb) { // "#RRGGBB"
	static int highlights[][3] = {
	                                 { 0x00, 0x00, 0x00 },          // highlight1  0;0;0       black
	                                 { 0x00, 0x00, 0xFF },          // highlight2  0;0;255     blue
	                                 { 0x00, 0xFF, 0xFF },          // highlight3  0;255;255   cyan
	                                 { 0x00, 0xFF, 0x00 },          // highlight4  0;255;0     green
	                                 { 0xFF, 0x00, 0xFF },          // highlight5  255;0;255   violet
	                                 { 0xFF, 0x00, 0x00 },          // highlight6  255;0;0     red
	                                 { 0xFF, 0xFF, 0x00 },          // highlight7  255;255;0   yellow
	                                 { 0xFF, 0xFF, 0xFF },          // highlight8  255;255;255 white
	                                 { 0x00, 0x00, 0x80 },          // highlight9  0;0;128     dark blue
	                                 { 0x00, 0x80, 0x80 },          // highlight10 0;128;128   dark cyan
	                                 { 0x00, 0x80, 0x00 },          // highlight11 0;128;0     dark green
	                                 { 0x80, 0x00, 0x80 },          // highlight12 128;0;128   dark violet
	                                 { 0x80, 0x00, 0x00 },          // highlight13 128;0;0     brown
	                                 { 0x80, 0x80, 0x00 },          // highlight14 128;128;0   khaki
	                                 { 0x80, 0x80, 0x80 },          // highlight15 128;128;128 dark grey
	                                 { 0xC0, 0xC0, 0xC0 },          // highlight16 192;192;192 grey
	                             };
	int maxdelta = 3 * 255 + 1, delta, index = -1;
	int r = IntFromHexByte(rgb + 1), g = IntFromHexByte(rgb + 3), b = IntFromHexByte(rgb + 5);
	for (unsigned int i = 0; i < sizeof(highlights) / sizeof(*highlights); i++) {
		delta = abs(r - highlights[i][0]) +
		        abs(g - highlights[i][1]) +
		        abs(b - highlights[i][2]);
		if (delta < maxdelta) {
			maxdelta = delta;
			index = i;
		}
	}
	return index + 1;
}

void GetRTFStyleChange(char *delta, char *last, const char *current) { // \f0\fs20\cf0\highlight0\b0\i0
	int lastLen = strlen(last), offset = 2, lastOffset, currentOffset, len;
	*delta = '\0';
	// font face
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||          // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove(last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat(delta, RTF_SETFONTFACE);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 3;
	// size
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||          // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove(last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat(delta, RTF_SETFONTSIZE);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 3;
	// color
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||          // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETCOLOR);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 10;
	// background
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||          // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETBACKGROUND);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 2;
	// bold
	if (last[offset] != current[offset]) {
		if (current[offset] == '\\') { // turn on
			memmove (last + offset, last + offset + 1, lastLen-- - offset);
			strcat (delta, RTF_BOLD_ON);
			offset += 2;
		} else { // turn off
			memmove (last + offset + 1, last + offset, ++lastLen - offset);
			last[offset] = '0';
			strcat (delta, RTF_BOLD_OFF);
			offset += 3;
		}
	} else
		offset += current[offset] == '\\' ? 2 : 3;
	// italic
	if (last[offset] != current[offset]) {
		if (current[offset] == '\\') { // turn on
			memmove (last + offset, last + offset + 1, lastLen-- - offset);
			strcat (delta, RTF_ITALIC_ON);
		} else { // turn off
			memmove (last + offset + 1, last + offset, ++lastLen - offset);
			last[offset] = '0';
			strcat (delta, RTF_ITALIC_OFF);
		}
	}
	if (*delta) {
		lastOffset = strlen(delta);
		delta[lastOffset] = ' ';
		delta[lastOffset + 1] = '\0';
	}
}

void SciTEBase::SaveToRTF(const char *saveName, int start, int end) {
	int lengthDoc = LengthDocument();
	if (end < 0)
		end = lengthDoc;
	SendEditor(SCI_COLOURISE, 0, -1);

	// Read the default settings
	char key[200];
	sprintf(key, "style.*.%0d", STYLE_DEFAULT);
	char *valdef = StringDup(props.GetExpanded(key).c_str());
	sprintf(key, "style.%s.%0d", language.c_str(), STYLE_DEFAULT);
	char *val = StringDup(props.GetExpanded(key).c_str());

	StyleDefinition defaultStyle(valdef);
	defaultStyle.ParseStyleDefinition(val);

	if (val) delete []val;
	if (valdef) delete []valdef;

	int tabSize = props.GetInt("export.rtf.tabsize", props.GetInt("tabsize"));
	int wysiwyg = props.GetInt("export.rtf.wysiwyg", 1);
	SString fontFace = props.GetExpanded("export.rtf.font.face");
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

	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		char styles[STYLE_DEFAULT + 1][MAX_STYLEDEF];
		char fonts[STYLE_DEFAULT + 1][MAX_FONTDEF];
		char colors[STYLE_DEFAULT + 1][MAX_COLORDEF];
		char lastStyle[MAX_STYLEDEF], deltaStyle[MAX_STYLEDEF];
		int fontCount = 1, colorCount = 2, i;
		fputs(RTF_HEADEROPEN RTF_FONTDEFOPEN, fp);
		strncpy(fonts[0], defaultStyle.font.c_str(), MAX_FONTDEF);
		fprintf(fp, RTF_FONTDEF, 0, characterset, defaultStyle.font.c_str());
		strncpy(colors[0], defaultStyle.rawFore.c_str(), MAX_COLORDEF);
		strncpy(colors[1], defaultStyle.rawBack.c_str(), MAX_COLORDEF);

		for (int istyle = 0; istyle < STYLE_DEFAULT; istyle++) {
			sprintf(key, "style.*.%0d", istyle);
			char *valdef = StringDup(props.GetExpanded(key).c_str());
			sprintf(key, "style.%s.%0d", language.c_str(), istyle);
			char *val = StringDup(props.GetExpanded(key).c_str());

			StyleDefinition sd(valdef);
			sd.ParseStyleDefinition(val);

			if (sd.specified != StyleDefinition::sdNone) {
				if (wysiwyg && sd.font.length()) {
					for (i = 0; i < fontCount; i++)
						if (EqualCaseInsensitive(sd.font.c_str(), fonts[i]))
							break;
					if (i >= fontCount) {
						strncpy(fonts[fontCount++], sd.font.c_str(), MAX_FONTDEF);
						fprintf(fp, RTF_FONTDEF, i, characterset, sd.font.c_str());
					}
					sprintf(lastStyle, RTF_SETFONTFACE "%d", i);
				} else {
					strcpy(lastStyle, RTF_SETFONTFACE "0");
				}

				sprintf(lastStyle + strlen(lastStyle), RTF_SETFONTSIZE "%d",
				        wysiwyg && sd.size ? sd.size << 1 : defaultStyle.size);

				if (sd.specified & StyleDefinition::sdFore) {
					for (i = 0; i < colorCount; i++)
						if (EqualCaseInsensitive(sd.rawFore.c_str(), colors[i]))
							break;
					if (i >= colorCount)
						strncpy(colors[colorCount++], sd.rawFore.c_str(), MAX_COLORDEF);
					sprintf(lastStyle + strlen(lastStyle), RTF_SETCOLOR "%d", i);
				} else {
					strcat(lastStyle, RTF_SETCOLOR "0");	// Default fore
				}

				// PL: highlights doesn't seems to follow a distinct table, at least with WordPad and Word 97
				// Perhaps it is different for Word 6?
//				sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d",
//				        sd.rawBack.length() ? GetRTFHighlight(sd.rawBack.c_str()) : 0);
				if (sd.specified & StyleDefinition::sdBack) {
					for (i = 0; i < colorCount; i++)
						if (EqualCaseInsensitive(sd.rawBack.c_str(), colors[i]))
							break;
					if (i >= colorCount)
						strncpy(colors[colorCount++], sd.rawBack.c_str(), MAX_COLORDEF);
					sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d", i);
				} else {
					strcat(lastStyle, RTF_SETBACKGROUND "1");	// Default back
				}
				if (sd.specified & StyleDefinition::sdBold) {
					strcat(lastStyle, sd.bold ? RTF_BOLD_ON : RTF_BOLD_OFF);
				} else {
					strcat(lastStyle, defaultStyle.bold ? RTF_BOLD_ON : RTF_BOLD_OFF);
				}
				if (sd.specified & StyleDefinition::sdItalics) {
					strcat(lastStyle, sd.italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
				} else {
					strcat(lastStyle, defaultStyle.italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
				}
				strncpy(styles[istyle], lastStyle, MAX_STYLEDEF);
			} else {
				sprintf(styles[istyle], RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
				        RTF_SETCOLOR "0" RTF_SETBACKGROUND "1"
				        RTF_BOLD_OFF RTF_ITALIC_OFF, defaultStyle.size);
			}
			if (val)
				delete []val;
			if (valdef)
				delete []valdef;
		}
		fputs(RTF_FONTDEFCLOSE RTF_COLORDEFOPEN, fp);
		for (i = 0; i < colorCount; i++) {
			fprintf(fp, RTF_COLORDEF, IntFromHexByte(colors[i] + 1),
			        IntFromHexByte(colors[i] + 3), IntFromHexByte(colors[i] + 5));
		}
		fprintf(fp, RTF_COLORDEFCLOSE RTF_HEADERCLOSE RTF_BODYOPEN RTF_SETFONTFACE "0"
		        RTF_SETFONTSIZE "%d" RTF_SETCOLOR "0 ", defaultStyle.size);
		sprintf(lastStyle, RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
		        RTF_SETCOLOR "0" RTF_SETBACKGROUND "1"
		        RTF_BOLD_OFF RTF_ITALIC_OFF, defaultStyle.size);
		bool prevCR = false;
		int styleCurrent = -1;
		WindowAccessor acc(wEditor.GetID(), props);
		int column = 0;
		for (i = start; i < end; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style > STYLE_DEFAULT)
				style = 0;
			if (style != styleCurrent) {
				GetRTFStyleChange(deltaStyle, lastStyle, styles[style]);
				if (*deltaStyle)
					fputs(deltaStyle, fp);
				styleCurrent = style;
			}
			if (ch == '{')
				fputs("\\{", fp);
			else if (ch == '}')
				fputs("\\}", fp);
			else if (ch == '\\')
				fputs("\\\\", fp);
			else if (ch == '\t') {
				if (tabs) {
					fputs(RTF_TAB, fp);
				} else {
					int ts = tabSize - (column % tabSize);
					for (int itab = 0; itab < ts; itab++) {
						fputc(' ', fp);
					}
					column += ts - 1;
				}
			} else if (ch == '\n') {
				if (!prevCR) {
					fputs(RTF_EOLN, fp);
					column = -1;
				}
			} else if (ch == '\r') {
				fputs(RTF_EOLN, fp);
				column = -1;
			}
			else
				fputc(ch, fp);
			column++;
			prevCR = ch == '\r';
		}
		fputs(RTF_BODYCLOSE, fp);
		fclose(fp);
	} else {
		SString msg = LocaliseMessage("Could not save file '^0'.", fullPath);
		WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
	}
}


//---------- Save to HTML ----------

void SciTEBase::SaveToHTML(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;
	int wysiwyg = props.GetInt("export.html.wysiwyg", 1);
	int tabs = props.GetInt("export.html.tabs", 0);
	int folding = props.GetInt("export.html.folding", 0);
	int onlyStylesUsed = props.GetInt("export.html.styleused", 0);
	int titleFullPath = props.GetInt("export.html.title.fullpath", 0);

	int lengthDoc = LengthDocument();
	WindowAccessor acc(wEditor.GetID(), props);

	bool styleIsUsed[STYLE_MAX + 1];
	if (onlyStylesUsed) {
		int i;
		for (i = 0; i <= STYLE_MAX; i++) {
			styleIsUsed[i] = false;
		}
		// check the used styles
		for (i = 0; i < lengthDoc; i++) {
			styleIsUsed[acc.StyleAt(i)] = true;
		}
	} else {
		for (int i = 0; i <= STYLE_MAX; i++) {
			styleIsUsed[i] = true;
		}
	}
	styleIsUsed[STYLE_DEFAULT] = true;

	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		fputs("<!DOCTYPE html  PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"DTD/xhtml1-strict.dtd\">\n", fp);
		fputs("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n", fp);
		fputs("<head>\n", fp);
		if (titleFullPath)
			fprintf(fp, "<title>%s</title>\n", fullPath);
		else
			fprintf(fp, "<title>%s</title>\n", fileName);
		// Probably not used by robots, but making a little advertisement for those looking
		// at the source code doesn't hurt...
		fputs("<meta name=\"GENERATOR\" content=\"SciTE - www.Scintilla.org\" />\n", fp);

		if (folding) {
			fputs("<script language=\"JavaScript\" type=\"text/javascript\">\n"
			      "<!--\n"
			      "function toggle(thisid) {\n"
			      "var thislayer=document.getElementById(thisid);\n"
			      "if (thislayer.style.display == 'none') {\n"
			      " thislayer.style.display='block';\n"
			      "} else {\n"
			      " thislayer.style.display='none';\n"
			      "}\n"
			      "}\n"
			      "//-->\n"
			      "</script>\n", fp);
		}

		fputs("<style type=\"text/css\">\n", fp);

		SString bgColour;
		for (int istyle = 0; istyle <= STYLE_MAX; istyle++) {
			if ((istyle > STYLE_DEFAULT) && (istyle <= STYLE_LASTPREDEFINED))
				continue;
			if (styleIsUsed[istyle]) {
				char key[200];
				sprintf(key, "style.*.%0d", istyle);
				char *valdef = StringDup(props.GetExpanded(key).c_str());
				sprintf(key, "style.%s.%0d", language.c_str(), istyle);
				char *val = StringDup(props.GetExpanded(key).c_str());

				StyleDefinition sd(valdef);
				sd.ParseStyleDefinition(val);

				if (sd.specified != StyleDefinition::sdNone) {
					if (istyle == STYLE_DEFAULT) {
						fprintf(fp, "span {\n");
					} else {
						fprintf(fp, ".S%0d {\n", istyle);
					}
					if (sd.italics) {
						fprintf(fp, "\tfont-style: italic;\n");
					}
					if (sd.bold) {
						fprintf(fp, "\tfont-weight: bold;\n");
					}
					if (wysiwyg && sd.font.length()) {
						fprintf(fp, "\tfont-family: '%s';\n", useMonoFont ? "monospace" : sd.font.c_str());
					}
					if (sd.rawFore.length()) {
						fprintf(fp, "\tcolor: %s;\n", sd.rawFore.c_str());
					} else if (istyle == STYLE_DEFAULT) {
						fprintf(fp, "\tcolor: #000000;\n");
					}
					if (sd.rawBack.length()) {
						fprintf(fp, "\tbackground: %s;\n", sd.rawBack.c_str());
						if (istyle == STYLE_DEFAULT)
							bgColour = sd.rawBack;
					}
					if (wysiwyg && sd.size) {
						fprintf(fp, "\tfont-size: %0dpt;\n", sd.size);
					}
					fprintf(fp, "}\n");
				} else {
					styleIsUsed[istyle] = false;	// No definition, it uses default style (32)
				}

				if (val) {
					delete []val;
				}
				if (valdef) {
					delete []valdef;
				}
			}
		}
		fputs("</style>\n", fp);
		fputs("</head>\n", fp);
		if (bgColour.length() > 0)
			fprintf(fp, "<body bgcolor=\"%s\">\n", bgColour.c_str());
		else
			fputs("<body>\n", fp);

		int line = acc.GetLine(0);
		int level = (acc.LevelAt(line) & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE;
		int newLevel;
		int styleCurrent = acc.StyleAt(0);
		bool inStyleSpan = false;
		// Global span for default attributes
		if (wysiwyg) {
			fputs("<span>", fp);
		} else {
			fputs("<pre>", fp);
		}

		if (folding) {
			int lvl = acc.LevelAt(0);
			level = (lvl & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE;

			if (lvl & SC_FOLDLEVELHEADERFLAG) {
				fprintf(fp, "<span onclick=\"toggle('ln%d')\">-</span> ", line + 1);
			} else {
				fputs("&nbsp; ", fp);
			}
		}

		if (styleIsUsed[styleCurrent]) {
			fprintf(fp, "<span class=\"S%0d\">", styleCurrent);
			inStyleSpan = true;
		}
		// Else, this style has no definition (beside default one):
		// no span for it, except the global one

		int column = 0;
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (style != styleCurrent) {
				if (inStyleSpan) {
					fputs("</span>", fp);
					inStyleSpan = false;
				}
				if (ch != '\r' && ch != '\n') {	// No need of a span for the EOL
					if (styleIsUsed[style]) {
						fprintf(fp, "<span class=\"S%0d\">", style);
						inStyleSpan = true;
					}
					styleCurrent = style;
				}
			}
			if (ch == ' ') {
				if (wysiwyg) {
					char prevCh = '\0';
					if (column == 0) {	// At start of line, must put a &nbsp; because regular space will be collapsed
						prevCh = ' ';
					}
					while (i < lengthDoc && acc[i] == ' ') {
						if (prevCh != ' ') {
							fputc(' ', fp);
						} else {
							fputs("&nbsp;", fp);
						}
						prevCh = acc[i];
						i++;
						column++;
					}
					i--; // the last incrementation will be done by the for loop
				} else {
					fputc(' ', fp);
					column++;
				}
			} else if (ch == '\t') {
				int ts = tabSize - (column % tabSize);
				if (wysiwyg) {
					for (int itab = 0; itab < ts; itab++) {
						if (itab % 2) {
							fputc(' ', fp);
						} else {
							fputs("&nbsp;", fp);
						}
					}
					column += ts;
				} else {
					if (tabs) {
						fputc(ch, fp);
						column++;
					} else {
						for (int itab = 0; itab < ts; itab++) {
							fputc(' ', fp);
						}
						column += ts;
					}
				}
			} else if (ch == '\r' || ch == '\n') {
				if (inStyleSpan) {
					fputs("</span>", fp);
					inStyleSpan = false;
				}
				if (ch == '\r' && acc[i + 1] == '\n') {
					i++;	// CR+LF line ending, skip the "extra" EOL char
				}
				column = 0;
				if (wysiwyg) {
					fputs("<br />", fp);
				}

				styleCurrent = acc.StyleAt(i + 1);
				if (folding) {
					line = acc.GetLine(i + 1);

					int lvl = acc.LevelAt(line);
					newLevel = (lvl & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE;

					if (newLevel < level)
						fprintf(fp, "</span>");
					fputc('\n', fp); // here to get clean code
					if (newLevel > level)
						fprintf(fp, "<span id=\"ln%d\">", line);

					if (lvl & SC_FOLDLEVELHEADERFLAG)
						fprintf(fp, "<span onclick=\"toggle('ln%d')\">-</span> ", line + 1);
					else
						fputs("&nbsp; ", fp);
					level = newLevel;
				} else {
					fputc('\n', fp);
				}

				if (styleIsUsed[styleCurrent] && acc[i + 1] != '\r' && acc[i + 1] != '\n') {
					// We know it's the correct next style,
					// but no (empty) span for an empty line
					fprintf(fp, "<span class=\"S%0d\">", styleCurrent);
					inStyleSpan = true;
				}
			} else {
				switch (ch) {
				case '<':
					fputs("&lt;", fp);
					break;
				case '>':
					fputs("&gt;", fp);
					break;
				case '&':
					fputs("&amp;", fp);
					break;
				default:
					fputc(ch, fp);
				}
				column++;
			}
		}

		if (inStyleSpan) {
			fputs("</span>", fp);
		}

		if (folding) {
			while (level > 0) {
				fprintf(fp, "</span>");
				level--;
			}
		}

		if (!wysiwyg) {
			fputs("</pre>", fp);
		} else {
			fputs("</span>", fp);
		}

		fputs("\n</body>\n</html>\n", fp);
		fclose(fp);
	} else {
		SString msg = LocaliseMessage(
		                  "Could not save file \"^0\".", fullPath);
		WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
	}
}


//---------- Save to PDF ----------

/*
	PDF Exporter...
	Contributed by Ahmad M. Zawawi <zeus_go64@hotmail.com>
	Modifications by Darren Schroeder Feb 22, 2003
	Status: Alpha
	Known Problems:
		doesn't support background colours for now
		doesn't support most styles
		output not fully optimized
		not Object Oriented :-(
*/
void SciTEBase::SaveToPDF(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);

	// read the tabsize, wsysiwyg and 'expand tabs' flag...
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)	{
		tabSize = 4;
	}
	// P.S. currently those are not currently used in the code...
	//int wysiwyg = props.GetInt("export.pdf.wysiwyg", 1);
	//int tabs = props.GetInt("export.pdf.tabs", 0);

	// check that we have content
	if (!LengthDocument()) {
		// no content to export, issue an error message
		char msg[200];
		strcpy(msg, "Nothing to export as PDF");	// Should be localized...
		WindowMessageBox(wSciTE, msg, MB_OK);
		return;
	}

	FILE *fp = fopen(saveName, "wt");
	if (!fp) {
		SString msg = LocaliseMessage("Could not save file \"^0\".", fullPath) ;
		WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING) ;
		return;
	}

	char *PDFColours[STYLE_DEFAULT + 1];
	int defaultSize = 9;
	int j;

	// initialize that array...
	for (j = 0; j <= STYLE_DEFAULT; j++) {
		PDFColours[j] = NULL;
	}

	// collect all styles available for that 'language'
	// or the default style if no language is available
	// (or an attribute isn't defined for this language/style)...
	for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
		char key[200];
		sprintf(key, "style.*.%0d", istyle);
		char *valdef = StringDup(props.GetExpanded(key).c_str());
		sprintf(key, "style.%s.%0d", language.c_str(), istyle);
		char *val = StringDup(props.GetExpanded(key).c_str());

		StyleDefinition sd(valdef);
		sd.ParseStyleDefinition(val);

		if (sd.specified != StyleDefinition::sdNone) {
			if (sd.rawFore.length()) {
				int red, green, blue;
				char buffer[30];

				red = IntFromHexByte(sd.rawFore.c_str() + 1);
				green = IntFromHexByte(sd.rawFore.c_str() + 3);
				blue = IntFromHexByte(sd.rawFore.c_str() + 5);

				// at last, we got the PDF colour!!!
				sprintf(buffer, "%3.2f %3.2f %3.2f", (red / 256.0f), (green / 256.0f), (blue / 256.0f) );
				PDFColours[istyle] = StringDup(buffer);
			}
			if (istyle == STYLE_DEFAULT && sd.size > 0) {
				defaultSize = sd.size;
			}
		}

		if (val) {
			delete []val;
		}
		if (valdef) {
			delete []valdef;
		}
	}
	// If not defined, default colour is black
	if (PDFColours[STYLE_DEFAULT] == NULL) {
		PDFColours[STYLE_DEFAULT] = StringDup("0.00 0.00 0.00");
	}
	// If one colour isn't defined, it takes the default colour
	for (j = 0; j < STYLE_DEFAULT; j++) {
		if (PDFColours[j] == NULL) {
			PDFColours[j] = StringDup(PDFColours[STYLE_DEFAULT]);
		}
	}

	// the thing that identifies a PDF 1.3 file...
	fputs("%PDF-1.3\n", fp);

	int pageObjNumber = 100;	// it starts at object #100, it should fix this one to be more generic
//	const int pageHeight = 60;	// for now this is fixed... i fix it once i have fonts & styles implemented...
	// PL: I replaced the fixed height of '12' by the size found in the default style.
	// Not perfect, but better as it is smaller (for me)...
	// So I compute an empirical page height based on the previous values.
	// A more precise method should be used here.
	const int pageHeight = 60 * 12 / (defaultSize + 1);

	// do here all the writing
	int lengthDoc = LengthDocument();
	int numLines = 0;
	WindowAccessor acc(wEditor.GetID(), props);

	//
	bool firstTime = true;
	SString textObj = "";
	SString stream = "";
	int styleCurrent = 0;
	int column = 0;
	for (int i = 0; i < lengthDoc; i++) {
		char ch = acc[i];

		// a new page is needed whenever the number of lines exceeds that of page height or
		// the first time it is created...
		int newPageNeeded = (numLines > pageHeight) || firstTime;
		if ( newPageNeeded ) {
			// create a new text object using (pageObjNumber + 1)
			int textObjNumber = (pageObjNumber + 1);

			// if not the first text object created, we should close the previous
			// all of this wouldnt have happened if i used OOP
			// which i'm gonna add in the next edition of the Save2PDF feature
			if ( !firstTime ) {
				// close the opened text object if there are any...
				stream += ") Tj\n";

				// close last text object
				stream += "ET\n";

				// patch in the stream size (minus 1 since the newline is not counted... go figure)
				char *buffer = new char[textObj.size() + 1 + 200]; // Copied Neil's [2/22/2003 21:04]
				sprintf(buffer, textObj.c_str(), stream.length() - 1); // Length instead of size [2/22/2003 21:08]
				textObj = buffer;
				delete [] buffer;

				// concatenate stream within the text object
				textObj += stream;
				textObj += "endstream\n";
				textObj += "endobj\n";

				// write the actual object to the PDF
				fputs( textObj.c_str(), fp );

				// reinitialize the stream [2/22/2003 20:39]
				stream.clear();
			}
			firstTime = false;

			// open a text object
			char buffer[20];
			sprintf(buffer, "%d 0 obj\n", textObjNumber);
			textObj = buffer;
			textObj += "<< /Length %d >>\n"; // we should patch the length here correctly...
			textObj += "stream\n";

			// new stream ;-)
			stream = "BT\n";

			stream += "%% draw text string using current graphics state\n";
			sprintf(buffer, "/F1 %d Tf\n", defaultSize);	// Size of the default style
			stream += buffer;
			stream += "1 0 0 1 20 750 Tm\n";

			// a new page should take the previous style information...
			// this is a glitch in the PDF spec... it is not persisted over multiple pages...
			int style = acc.StyleAt(i);
			if (style <= STYLE_DEFAULT) {
				stream += PDFColours[style];
				stream += " rg\n";
				stream += PDFColours[style];
				stream += " RG\n";
				styleCurrent = style;
			}

			// start the first text string with that text object..
			stream += "(";

			// update the page numbers
			pageObjNumber += 2;	// since (pageObjNumber + 1) is reserved for the textObject for that page...

			// make numLines equal to zero...
			numLines = 0;
		}

		// apply only new styles...
		int style = acc.StyleAt(i);
		if (style != styleCurrent) {
			if (style <= STYLE_DEFAULT) {
				stream += ") Tj\n";
				stream += PDFColours[style];
				stream += " rg\n";
				stream += PDFColours[style];
				stream += " RG\n";
				stream += "(";
				styleCurrent = style;
			}
		}

		if (ch == '\t') {
			// expand tabs into equals 'tabsize' spaces...
			int ts = tabSize - (column % tabSize);
			for (int itab = 0; itab < ts; itab++) {
				stream += " ";
			}
			column += ts;
		} else if (ch == '\r' || ch == '\n') {
			if (ch == '\r' && acc[i + 1] == '\n') {
				i++;
			}
			// close and begin a newline...
			char buffer[20];
			stream += ") Tj\n";
			sprintf(buffer, "0 -%d TD\n", defaultSize+1);	// Size of the default style
			stream += buffer;	// We should compute these strings only once
			stream += "(";
			column = 0;
			numLines++;
		} else if ((ch == ')') || (ch == '(') || (ch == '\\')) {
			// you should escape those characters for PDF 1.2+
			char buffer[10];
			sprintf(buffer, "\\%c", ch);
			stream += buffer;
			column++;
		} else {
			// write the character normally...
			stream += ch;
			column++;
		}
	}
	// Clean up
	for (j = 0; j <= STYLE_DEFAULT; j++) {
		delete [] PDFColours[j];
	}

	// close the opened text object if there are any...
	stream += ") Tj\n";

	// close last text object if and only if there is one open...
	if (lengthDoc > 0) {
		// close last text object
		stream += "ET\n";

		// patch in the stream size (minus 1 since the newline is not counted... go figure)
		char *buffer = new char[textObj.size() + 1 + 200];  // 200 by Neil as this is quite indeterminate
		sprintf(buffer, textObj.c_str(), stream.length() - 1);  // Length instead of size [2/22/2003 21:08]
		textObj = buffer;
		delete [] buffer;

		// concatenate stream within the text object
		textObj += stream;
		textObj += "endstream\n";
		textObj += "endobj\n";

		// write the actual object to the PDF
		fputs( textObj.c_str(), fp );

		// empty the stream [2/22/2003 20:56]
		stream.clear();
	}

	// now create all the page objects...
	SString pageRefs = "";
	int pageCount = 0;
	for (int k = 100; k < pageObjNumber; k += 2 ) {
		//
		char buffer[20];
		sprintf( buffer, "%% page number %d\n", (k - 99));
		fputs(buffer, fp);

		sprintf( buffer, "%d 0 obj\n", k );
		fputs(buffer, fp);
		fputs("<< /Type /Page\n"
		      "/Parent 3 0 R\n"
		      "/MediaBox [ 0 0 612 792 ]\n", fp);

		// we need to patch in the corresponding page text object!
		int textObjNumber = (k + 1);
		sprintf( buffer, "/Contents %d 0 R\n", textObjNumber);
		fputs(buffer, fp);

		fputs("/Resources << /ProcSet 6 0 R\n"
		      "/Font << /F1 7 0 R >>\n"
		      ">>\n"
		      ">>\n"
		      "endobj\n", fp);

		// add this to the list of page number references...
		sprintf(buffer, "%d 0 R ", k);
		pageRefs += buffer;

		// increment the page count...
		pageCount++;
	}

	//
	fputs("3 0 obj\n", fp);
	fputs("<< /Type /Pages\n", fp);

	// new scope...
	{
		SString buff = "";
		buff += "/Kids [";
		buff += pageRefs;
		buff += "]\n";
		fputs(buff.c_str(), fp);		// need to patch in all the page objects
	}

	{
		char buffer[20];
		sprintf(buffer, "/Count %d\n", pageCount);
		fputs(buffer, fp);		// we need to patch also the number of page objects created
	}

	fputs(">>\n", fp);
	fputs("endobj\n", fp);

	// create catalog object
	fputs("% catalog object\n"
	      "1 0 obj\n"
	      "<< /Type /Catalog\n"
	      "/Outlines 2 0 R\n"
	      "/Pages 3 0 R\n"
	      ">>\n"
	      "endobj\n", fp);

	// create an empty outline object
	fputs("2 0 obj\n"
	      "<< /Type /Outlines\n"
	      "/Count 0\n"
	      ">>\n"
	      "endobj\n", fp);

	//
	fputs("6 0 obj\n"
	      "[ /PDF /Text ]\n"
	      "endobj\n", fp);

	//
	fputs("7 0 obj\n"
	      "<< /Type /Font\n"
	      "/Subtype /Type1\n"
	      "/Name /F1\n"
	      "/BaseFont /Helvetica\n"
	      "/Encoding /MacRomanEncoding\n"
	      ">>\n"
	      "endobj\n", fp);

	// end the file, with the trailer
	fputs("xref\n"
	      "0 8\n"
	      "0000000000 65535 f\n"
	      "0000000009 00000 n\n"
	      "0000000074 00000 n\n"
	      "0000000120 00000 n\n"
	      "0000000179 00000 n\n"
	      "0000000364 00000 n\n"
	      "0000000466 00000 n\n"
	      "0000000496 00000 n\n"
	      "trailer\n"
	      "<< /Size 8\n"
	      "/Root 1 0 R\n"
	      ">>\n"
	      "startxref\n"
	      "0\n"
	      "%%EOF\n", fp);

	// and close the PDF file
	fclose(fp);
}


//---------- Save to TeX ----------

static char* getTexRGB(char* texcolor, const char* stylecolor) {
	//texcolor[rgb]{0,0.5,0}{....}
	float r = IntFromHexByte(stylecolor + 1) / 256.0;
	float g = IntFromHexByte(stylecolor + 3) / 256.0;
	float b = IntFromHexByte(stylecolor + 5) / 256.0;
	sprintf(texcolor, "%.1f, %.1f, %.1f", r, g, b);
	return texcolor;
}

#define CHARZ ('z' - 'b')
static char* texStyle(int style) {
	static char buf[10];
	int i = 0;
	do {
		buf[i++] = static_cast<char>('a' + (style % CHARZ));
		style /= CHARZ;
	} while ( style > 0 );
	buf[i] = 0;
	return buf;
}

static void defineTexStyle(StyleDefinition &style, FILE* fp, int istyle) {
	int closing_brackets = 2;
	char rgb[200];
	fprintf(fp, "\\newcommand{\\scite%s}[1]{\\noindent{\\ttfamily{", texStyle(istyle));
	if (style.italics) {
		fputs("\\textit{", fp);
		closing_brackets++;
	}
	if (style.bold) {
		fputs("\\textbf{", fp);
		closing_brackets++;
	}
	if (style.rawFore.length()) {
		fprintf(fp, "\\textcolor[rgb]{%s}{", getTexRGB(rgb, style.rawFore.c_str()) );
		closing_brackets++;
	}
	if (style.rawBack.length()) {
		fprintf(fp, "\\colorbox[rgb]{%s}{", getTexRGB( rgb, style.rawBack.c_str()) );
		closing_brackets++;
	}
	fputs("#1", fp);
	for (int i = 0; i <= closing_brackets; i++) {
		fputc( '}', fp );
	}
	fputc('\n', fp);
}

void SciTEBase::SaveToTEX(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;

	char key[200];
	int lengthDoc = LengthDocument();
	WindowAccessor acc(wEditor.GetID(), props);
	bool styleIsUsed[STYLE_MAX + 1];

	int titleFullPath = props.GetInt("export.tex.title.fullpath", 0);

	int i;
	for (i = 0; i <= STYLE_MAX; i++) {
		styleIsUsed[i] = false;
	}
	for (i = 0; i < lengthDoc; i++) {	// check the used styles
		styleIsUsed[acc.StyleAt(i)] = true;
	}
	styleIsUsed[STYLE_DEFAULT] = true;

	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		fputs("\\documentclass[a4paper]{article}\n"
		      "\\usepackage[a4paper,margin=2cm]{geometry}\n"
		      "\\usepackage[T1]{fontenc}\n"
		      "\\usepackage{color}\n"
		      "\\usepackage{alltt}\n"
		      "\\usepackage{times}\n", fp);

		for (i = 0; i < STYLE_MAX; i++) {      // get keys
			if (styleIsUsed[i]) {
				sprintf(key, "style.*.%0d", i);
				char *valdef = StringDup(props.GetExpanded(key).c_str());
				sprintf(key, "style.%s.%0d", language.c_str(), i);
				char *val = StringDup(props.GetExpanded(key).c_str());

				StyleDefinition sd(valdef); //check default properties
				sd.ParseStyleDefinition(val); //check language properties

				if (sd.specified != StyleDefinition::sdNone) {
					defineTexStyle(sd, fp, i); // writeout style macroses
				} // Else we should use STYLE_DEFAULT
				if (val)
					delete []val;
				if (valdef)
					delete []valdef;
			}
		}

		fputs("\\begin{document}\n\n", fp);
		fprintf(fp, "Source File: %s\n\n\\noindent\n\\tiny{\n", titleFullPath ? fullPath : fileName);

		int styleCurrent = acc.StyleAt(0);

		fprintf(fp, "\\scite%s{", texStyle(styleCurrent));

		int lineIdx = 0;

		for (i = 0; i < lengthDoc; i++) { //here process each character of the document
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (style != styleCurrent) { //new style?
				fprintf(fp, "}\n\\scite%s{", texStyle(style) );
				styleCurrent = style;
			}

			switch ( ch ) { //write out current character.
			case '\t': {
					int ts = tabSize - (lineIdx % tabSize);
					lineIdx += ts - 1;
					fprintf(fp, "\\hspace*{%dem}", ts);
					break;
				}
			case '\\':
				fputs("{\\textbackslash}", fp);
				break;
			case '>':
			case '<':
			case '@':
				fprintf(fp, "$%c$", ch);
				break;
			case '{':
			case '}':
			case '^':
			case '_':
			case '&':
			case '$':
			case '#':
			case '%':
			case '~':
				fprintf(fp, "\\%c", ch);
				break;
			case '\r':
			case '\n':
				lineIdx = -1;	// Because incremented below
				if (ch == '\r' && acc[i + 1] == '\n')
					i++;	// Skip the LF
				styleCurrent = acc.StyleAt(i + 1);
				fprintf(fp, "} \\\\\n\\scite%s{", texStyle(styleCurrent) );
				break;
			case ' ':
				if (acc[i + 1] == ' ') {
					fputs("{\\hspace*{1em}}", fp);
				} else {
					fputc(' ', fp);
				}
				break;
			default:
				fputc(ch, fp);
			}
			lineIdx++;
		}
		fputs("}\n} %end tiny\n\n\\end{document}\n", fp); //close last empty style macros and document too
		fclose(fp);
	} else {
		SString msg = LocaliseMessage(
		                  "Could not save file \"^0\".", fullPath);
		WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
	}
}


//---------- Save to XML ----------

void SciTEBase::SaveToXML(const char *saveName) {

    // Author: Hans Hagen / PRAGMA ADE / www.pragma-ade.com
    // Version: 1.0 / august 18, 2003
    // Remark: for a suitable style, see ConTeXt (future) distributions

    // The idea is that one can use whole files, or ranges of lines in manuals
    // and alike. Since ConTeXt can handle XML files, it's quite convenient to
    // use this format instead of raw TeX, although the output would not look
    // much different in structure.

    // We don't put style definitions in here since the main document will in
    // most cases determine the look and feel. This way we have full control over
    // the layout. The type attribute will hold the current lexer value.

    // <document>            : the whole thing
    // <data>                : reserved for metadata
    // <text>                : the main bodyof text
    // <line n-'number'>     : a line of text

    // <t n='number'>...<t/> : tag
    // <s n='number'/>       : space
    // <g/>                  : >
    // <l/>                  : <
    // <a/>                  : &
    // <h/>                  : #

    // We don't use entities, but empty elements for special characters
    // but will eventually use utf-8 (once i know how to get them out).

	SendEditor(SCI_COLOURISE, 0, -1) ;

	int tabSize = props.GetInt("tabsize") ;
	if (tabSize == 0) {
		tabSize = 4 ;
    }

	int lengthDoc = LengthDocument() ;

	WindowAccessor acc(wEditor.GetID(), props) ;

	FILE *fp = fopen(saveName, "wt");

	if (fp) {

        bool collapseSpaces = (props.GetInt("export.xml.collapse.spaces", 1) == 1) ;
        bool collapseLines  = (props.GetInt("export.xml.collapse.lines", 1) == 1) ;

		fputs("<?xml version='1.0' encoding='ascii'?>\n", fp) ;

		fputs("<document xmlns='http://www.scintila.org/scite.rng'", fp) ;
        fprintf(fp, " filename='%s'",fileName) ;
		fprintf(fp, " type='%s'", "unknown") ;
		fprintf(fp, " version='%s'", "1.0") ;
        fputs(">\n", fp) ;

        fputs("<data comment='This element is reserved for future usage.'/>\n", fp) ;

		fputs("<text>\n", fp) ;

		int styleCurrent = -1 ; // acc.StyleAt(0) ;
        int lineNumber = 1 ;
		int lineIndex = 0 ;
        bool styleDone = false ;
        bool lineDone = false ;
        bool charDone = false ;
        int styleNew = -1 ;
        int spaceLen = 0 ;
        int emptyLines = 0 ;

		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i] ;
			int style = acc.StyleAt(i) ;
			if (style != styleCurrent) {
				styleCurrent = style ;
                styleNew = style ;
			}
            if (ch == ' ') {
                spaceLen++ ;
            } else if (ch == '\t') {
                int ts = tabSize - (lineIndex % tabSize) ;
                lineIndex += ts - 1 ;
                spaceLen += ts ;
            } else if (ch == '\f') {
                // ignore this animal
            } else if (ch == '\r' || ch == '\n') {
				if (ch == '\r' && acc[i + 1] == '\n') {
					i++;
				}
                if (styleDone) {
                    fputs("</t>", fp) ;
                    styleDone = false ;
                }
                lineIndex = -1 ;
                if (lineDone) {
                    fputs("</line>\n", fp) ;
                    lineDone = false ;
                } else if (collapseLines) {
                    emptyLines++ ;
                } else {
                    fprintf(fp, "<line n='%d'/>\n", lineNumber) ;
                }
                charDone = false ;
                lineNumber++ ;
                styleCurrent = -1 ; // acc.StyleAt(i + 1) ;
            } else {
                if (collapseLines && (emptyLines > 0)) {
                    fputs("<line/>\n", fp) ;
                }
                emptyLines = 0 ;
                if (! lineDone) {
                    fprintf(fp, "<line n='%d'>", lineNumber) ;
                    lineDone = true ;
                }
                if (styleNew >= 0) {
                    if (styleDone) { fputs("</t>", fp) ; }
                }
                if (! collapseSpaces) {
                    while (spaceLen > 0) {
                        fputs("<s/>", fp) ;
                        spaceLen-- ;
                    }
                } else if (spaceLen == 1) {
                    fputs("<s/>", fp) ;
                    spaceLen = 0 ;
                } else if (spaceLen > 1) {
                    fprintf(fp, "<s n='%d'/>", spaceLen) ;
                    spaceLen = 0 ;
                }
                if (styleNew >= 0) {
                    fprintf(fp, "<t n='%d'>", style) ;
                    styleNew = -1 ;
                    styleDone = true ;
                }
                switch (ch) {
				case '>' :
					fputs("<g/>", fp) ;
					break ;
				case '<' :
					fputs("<l/>", fp) ;
					break ;
				case '&' :
					fputs("<a/>", fp) ;
					break ;
				case '#' :
					fputs("<h/>", fp) ;
					break ;
				default  :
					fputc(ch, fp) ;
                }
                charDone = true ;
            }
			lineIndex++ ;
		}
        if (styleDone) {
            fputs("</t>", fp) ;
        }
        if (lineDone) {
            fputs("</line>\n", fp) ;
        }
		if (charDone) {
            // no last empty line: fprintf(fp, "<line n='%d'/>", lineNumber) ;
        }

		fputs("</text>\n", fp) ;
		fputs("</document>\n", fp) ;

		fclose(fp) ;
	} else {
		SString msg = LocaliseMessage("Could not save file \"^0\".", fullPath) ;
		WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING) ;
	}
}
