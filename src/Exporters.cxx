// SciTE - Scintilla based Text Editor
/** @file Exporters.cxx
 ** Manage input and output with the system.
 **/ 
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>   	// For time_t

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>

#endif

#if PLAT_WIN 
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

#define RTF_HEADEROPEN "{\\rtf1\\ansi\\deff0\\deftab720"
#define RTF_FONTDEFOPEN "{\\fonttbl"
#define RTF_FONTDEF "{\\f%d\\fnil\\fcharset0 %s;}"
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

int GetHexChar(char ch) { // 'H'
	return ch > '9' ? (ch | 0x20) - 'a' + 10 : ch - '0';
}

int GetHexByte(const char *hexbyte) { // "HH"
	return (GetHexChar(*hexbyte) << 4) | GetHexChar(hexbyte[1]);
}

int GetRTFHighlight(const char *rgb) { // "#RRGGBB"
	static int highlights[][3] = {
	                                 { 0x00, 0x00, 0x00 },        // highlight1  0;0;0       black
	                                 { 0x00, 0x00, 0xFF },        // highlight2  0;0;255     blue
	                                 { 0x00, 0xFF, 0xFF },        // highlight3  0;255;255   cyan
	                                 { 0x00, 0xFF, 0x00 },        // highlight4  0;255;0     green
	                                 { 0xFF, 0x00, 0xFF },        // highlight5  255;0;255   violet
	                                 { 0xFF, 0x00, 0x00 },        // highlight6  255;0;0     red
	                                 { 0xFF, 0xFF, 0x00 },        // highlight7  255;255;0   yellow
	                                 { 0xFF, 0xFF, 0xFF },        // highlight8  255;255;255 white
	                                 { 0x00, 0x00, 0x80 },        // highlight9  0;0;128     dark blue
	                                 { 0x00, 0x80, 0x80 },        // highlight10 0;128;128   dark cyan
	                                 { 0x00, 0x80, 0x00 },        // highlight11 0;128;0     dark green
	                                 { 0x80, 0x00, 0x80 },        // highlight12 128;0;128   dark violet
	                                 { 0x80, 0x00, 0x00 },        // highlight13 128;0;0     brown
	                                 { 0x80, 0x80, 0x00 },        // highlight14 128;128;0   khaki
	                                 { 0x80, 0x80, 0x80 },        // highlight15 128;128;128 dark grey
	                                 { 0xC0, 0xC0, 0xC0 },        // highlight16 192;192;192 grey
	                             };
	int maxdelta = 3 * 255 + 1, delta, index = -1;
	int r = GetHexByte (rgb + 1), g = GetHexByte (rgb + 3), b = GetHexByte (rgb + 5);
	for (unsigned int i = 0; i < sizeof(highlights) / sizeof(*highlights); i++) {
		delta = abs (r - *highlights[i]) +
		        abs (g - highlights[i][1]) +
		        abs (b - highlights[i][2]);
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
	if (lastOffset != currentOffset ||        // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETFONTFACE);
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
	if (lastOffset != currentOffset ||        // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETFONTSIZE);
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
	if (lastOffset != currentOffset ||        // change
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
	if (lastOffset != currentOffset ||        // change
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
	int tabSize = props.GetInt("export.rtf.tabsize", props.GetInt("tabsize"));
	int wysiwyg = props.GetInt("export.rtf.wysiwyg", 1);
	SString fontFace = props.GetExpanded("export.rtf.font.face");
	int fontSize = props.GetInt("export.rtf.font.size", 10 << 1);
	int tabs = props.GetInt("export.rtf.tabs", 0);
	if (tabSize == 0)
		tabSize = 4;
	if (!fontFace.length())
		fontFace = RTF_FONTFACE;
	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		char styles[STYLE_DEFAULT + 1][MAX_STYLEDEF];
		char fonts[STYLE_DEFAULT + 1][MAX_FONTDEF];
		char colors[STYLE_DEFAULT + 1][MAX_COLORDEF];
		char lastStyle[MAX_STYLEDEF], deltaStyle[MAX_STYLEDEF];
		int fontCount = 1, colorCount = 1, i;
		fputs(RTF_HEADEROPEN RTF_FONTDEFOPEN, fp);
		strncpy(*fonts, fontFace.c_str(), MAX_FONTDEF);
		fprintf(fp, RTF_FONTDEF, 0, fontFace.c_str());
		strncpy(*colors, "#000000", MAX_COLORDEF);
		for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
			char key[200];
			sprintf(key, "style.*.%0d", istyle);
			char *valdef = StringDup(props.GetExpanded(key).c_str());
			sprintf(key, "style.%s.%0d", language.c_str(), istyle);
			char *val = StringDup(props.GetExpanded(key).c_str());
			SString family;
			SString fore;
			SString back;
			bool italics = false;
			bool bold = false;
			int size = 0;
			if ((valdef && *valdef) || (val && *val)) {
				if (valdef && *valdef) {
					char *opt = valdef;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (val && *val) {
					char *opt = val;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (wysiwyg && family.length()) {
					for (i = 0; i < fontCount; i++)
						if (EqualCaseInsensitive(family.c_str(), fonts[i]))
							break;
					if (i >= fontCount) {
						strncpy(fonts[fontCount++], family.c_str(), MAX_FONTDEF);
						fprintf(fp, RTF_FONTDEF, i, family.c_str());
					}
					sprintf(lastStyle, RTF_SETFONTFACE "%d", i);
				} else
					strcpy(lastStyle, RTF_SETFONTFACE "0");
				sprintf(lastStyle + strlen(lastStyle), RTF_SETFONTSIZE "%d",
				        wysiwyg && size ? size << 1 : fontSize);
				if (fore.length()) {
					for (i = 0; i < colorCount; i++)
						if (EqualCaseInsensitive(fore.c_str(), colors[i]))
							break;
					if (i >= colorCount)
						strncpy(colors[colorCount++], fore.c_str(), MAX_COLORDEF);
					sprintf(lastStyle + strlen(lastStyle), RTF_SETCOLOR "%d", i);
				} else
					strcat(lastStyle, RTF_SETCOLOR "0");
				sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d",
				        back.length() ? GetRTFHighlight(back.c_str()) : 0);
				strcat(lastStyle, bold ? RTF_BOLD_ON : RTF_BOLD_OFF);
				strcat(lastStyle, italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
				strncpy(styles[istyle], lastStyle, MAX_STYLEDEF);
			} else
				sprintf(styles[istyle], RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
				        RTF_SETCOLOR "0" RTF_SETBACKGROUND "0"
				        RTF_BOLD_OFF RTF_ITALIC_OFF, fontSize);
			if (val)
				delete []val;
			if (valdef)
				delete []valdef;
		}
		fputs(RTF_FONTDEFCLOSE RTF_COLORDEFOPEN, fp);
		for (i = 0; i < colorCount; i++) {
			fprintf(fp, RTF_COLORDEF, GetHexByte(colors[i] + 1),
			        GetHexByte(colors[i] + 3), GetHexByte(colors[i] + 5));
		}
		fprintf(fp, RTF_COLORDEFCLOSE RTF_HEADERCLOSE RTF_BODYOPEN RTF_SETFONTFACE "0"
		        RTF_SETFONTSIZE "%d" RTF_SETCOLOR "0 ", fontSize);
		sprintf(lastStyle, RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
		        RTF_SETCOLOR "0" RTF_SETBACKGROUND "0"
		        RTF_BOLD_OFF RTF_ITALIC_OFF, fontSize);
		bool prevCR = false;
		int styleCurrent = -1;
		WindowAccessor acc(wEditor.GetID(), props);
		for (i = start; i < end; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
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
				fputs("\\", fp);
			else if (ch == '\t') {
				if (tabs)
					fputs(RTF_TAB, fp);
				else
					for (int itab = 0; itab < tabSize; itab++)
						fputc(' ', fp);
			} else if (ch == '\n') {
				if (!prevCR)
					fputs(RTF_EOLN, fp);
			} else if (ch == '\r')
				fputs(RTF_EOLN, fp);
			else
				fputc(ch, fp);
			prevCR = ch == '\r';
		}
		fputs(RTF_BODYCLOSE, fp);
		fclose(fp);
	} else {
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}
}

void SciTEBase::SaveToHTML(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;
	int wysiwyg = props.GetInt("export.html.wysiwyg", 1);
	int tabs = props.GetInt("export.html.tabs", 0);
	int folding = props.GetInt("export.html.folding", 0);
	int onlyStylesUsed = props.GetInt("export.html.styleused",0);
	int titleFullPath = props.GetInt("export.html.title.fullpath",0);

	int lengthDoc = LengthDocument();
	WindowAccessor acc(wEditor.GetID(), props);

	bool styleIsUsed[STYLE_MAX+1];
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
		SString colour;
		for (int istyle = 0; istyle <= STYLE_MAX; istyle++) {
			if ((istyle > STYLE_DEFAULT) && (istyle <= STYLE_LASTPREDEFINED))
				continue;
			if (styleIsUsed[istyle]) {
				char key[200];
				sprintf(key, "style.*.%0d", istyle);
				char *valdef = StringDup(props.GetExpanded(key).c_str());
				sprintf(key, "style.%s.%0d", language.c_str(), istyle);
				char *val = StringDup(props.GetExpanded(key).c_str());
				SString family;
				SString fore;
				SString back;
				bool italics = false;
				bool bold = false;
				int size = 0;
				if ((valdef && *valdef) || (val && *val)) {
					if (istyle == STYLE_DEFAULT)
						fprintf(fp, "span {\n");
					else
						fprintf(fp, ".S%0d {\n", istyle);
					if (valdef && *valdef) {
						char *opt = valdef;
						while (opt) {
							char *cpComma = strchr(opt, ',');
							if (cpComma)
								*cpComma = '\0';
							char *colon = strchr(opt, ':');
							if (colon)
								*colon++ = '\0';
							if (0 == strcmp(opt, "italics"))
								italics = true;
							if (0 == strcmp(opt, "notitalics"))
								italics = false;
							if (0 == strcmp(opt, "bold"))
								bold = true;
							if (0 == strcmp(opt, "notbold"))
								bold = false;
							if (0 == strcmp(opt, "font"))
								family = colon;
							if (0 == strcmp(opt, "fore"))
								fore = colon;
							if (0 == strcmp(opt, "back"))
								back = colon;
							if (0 == strcmp(opt, "size"))
								size = atoi(colon);
							if (cpComma)
								opt = cpComma + 1;
							else
								opt = 0;
						}
					}
					if (val && *val) {
						char *opt = val;
						while (opt) {
							char *cpComma = strchr(opt, ',');
							if (cpComma)
								*cpComma = '\0';
							char *colon = strchr(opt, ':');
							if (colon)
								*colon++ = '\0';
							if (0 == strcmp(opt, "italics"))
								italics = true;
							if (0 == strcmp(opt, "notitalics"))
								italics = false;
							if (0 == strcmp(opt, "bold"))
								bold = true;
							if (0 == strcmp(opt, "notbold"))
								bold = false;
							if (0 == strcmp(opt, "font"))
								family = colon;
							if (0 == strcmp(opt, "fore"))
								fore = colon;
							if (0 == strcmp(opt, "back"))
								back = colon;
							if (0 == strcmp(opt, "size"))
								size = atoi(colon);
							if (cpComma)
								opt = cpComma + 1;
							else
								opt = 0;
						}
					}
					if (italics)
						fprintf(fp, "\tfont-style: italic;\n");
					if (bold)
						fprintf(fp, "\tfont-weight: bold;\n");
					if (wysiwyg && family.length())
						fprintf(fp, "\tfont-family: %s;\n", family.c_str());
					if (fore.length())
						fprintf(fp, "\tcolor: %s;\n", fore.c_str());
					if (back.length())
						fprintf(fp, "\tbackground: %s;\n", back.c_str());
					if (wysiwyg && size)
						fprintf(fp, "\tfont-size: %0dpt;\n", size);
					fprintf(fp, "}\n");
				}
				if (val)
					delete []val;
				if (valdef)
					delete []valdef;
			}
		}
		fputs("</style>\n", fp);
		fputs("</head>\n", fp);
		fputs("<body>\n", fp);

		int line = acc.GetLine(0);
		int level = (acc.LevelAt(line) & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE;
		int newLevel;
		int styleCurrent = acc.StyleAt(0);
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
				fputs("&nbsp;&nbsp;", fp);
			}
		}

		fprintf(fp, "<span class=\"S%d\">", styleCurrent);

		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (style != styleCurrent) {
				fputs("</span>", fp);
				fprintf(fp, "<span class=\"S%0d\">", style);
				styleCurrent = style;
			}
			if (ch == ' ') {
				if (wysiwyg) {
					if (acc[i + 1] != ' ' || i + 1 >= lengthDoc) {
						// Single space, kept as is
						fputc(' ', fp);
					} else {
						while (i < lengthDoc && acc[i] == ' ') {
							fputs("&nbsp;", fp);
							i++;
						}
						i--; // the last one will be done by the loop
					}
				} else {
					fputc(' ', fp);
				}
			} else if (ch == '\t') {
				if (wysiwyg) {
					for (int itab = 0; itab < tabSize; itab++)
						fputs("&nbsp;", fp);
				} else {
					if (tabs) {
						fputc(ch, fp);
					} else {
						for (int itab = 0; itab < tabSize; itab++)
							fputc(' ', fp);
					}
				}
			} else if ((ch == '\r') || (ch == '\n')) {
				if (ch == '\r' && acc[i+1] == '\n') {
					i++;
				}
				if (wysiwyg) {
					fputs("<br />", fp);
				}

				fputs("</span>", fp);
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
						fputs("&nbsp;&nbsp;", fp);
					level = newLevel;
				} else {
					fputc('\n', fp);
				}

				fprintf(fp, "<span class=\"S%0d\">", styleCurrent); // we know it's the correct next style
			} else if (ch == '<') {
				fputs("&lt;", fp);
			} else if (ch == '>') {
				fputs("&gt;", fp);
			} else if (ch == '&') {
				fputs("&amp;", fp);
			} else {
				fputc(ch, fp);
			}
		}

		fputs("</span>", fp);

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
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}
}

/*
	PDF Exporter...
	Contributed by Ahmad M. Zawawi <zeus_go64@hotmail.com>
	Status: Alpha
	Known Problems:
		doesnt support background colours for now
		doesnt support most styles
		output not fully optimized
		not Object Oriented :-(
*/
//void SciTEBase::SaveToPDF(const char *saveName) {
void SciTEBase::SaveToPDF(const char *) {
#ifdef CODE_MADE_TO_WORK

	SendEditor(SCI_COLOURISE, 0, -1);

	// read the tabsize, wsysiwyg and 'expand tabs' flag...
	// P.S. currently those are not currently used in the code...
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)	{
		tabSize = 4;
	}
	//int wysiwyg = props.GetInt("export.pdf.wysiwyg", 1);
	//int tabs = props.GetInt("export.pdf.tabs", 0);

	// check that we have content
	if (!LengthDocument()) {
		// no content to export, issue an error message
		char msg[200];
		strcpy(msg, "Nothing to export as PDF");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}

	FILE *fp = fopen(saveName, "wt");
	if (!fp) {
		// couldnt open the file for saving, issue an error message
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}

	char *PDFColours[STYLE_DEFAULT];

	// initialize that array...
	for (int j = 0; j < STYLE_DEFAULT; j++) {
		PDFColours[j] = NULL;
	}

	// collect all styles available for that 'language'
	// or the default style if no language is available...
	for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
		char key[200];
		sprintf(key, "style.*.%0d", istyle);
		char *valdef = StringDup(props.GetExpanded(key).c_str());
		sprintf(key, "style.%s.%0d", language.c_str(), istyle);
		char *val = StringDup(props.GetExpanded(key).c_str());
		SString family;
		SString fore;
		SString back;
		if ((valdef && *valdef) || (val && *val)) {
			if (valdef && *valdef) {
				char *opt = valdef;
				while (opt) {
					char *cpComma = strchr(opt, ',');
					if (cpComma)
						*cpComma = '\0';
					char *colon = strchr(opt, ':');
					if (colon)
						*colon++ = '\0';
				if (0 == strcmp(opt, "font")) { family = colon; }
					if (0 == strcmp(opt, "fore")) { fore = colon; }
					if (0 == strcmp(opt, "back")) { back = colon; }
					if (cpComma)
						opt = cpComma + 1;
					else
						opt = 0;
				}
			}
			if (val && *val) {
				char *opt = val;
				while (opt) {
					char *cpComma = strchr(opt, ',');
					if (cpComma)
						*cpComma = '\0';
					char *colon = strchr(opt, ':');
					if (colon)
						*colon++ = '\0';
				if (0 == strcmp(opt, "font")) { family = colon; }
					if (0 == strcmp(opt, "fore")) { fore = colon; }
					if (0 == strcmp(opt, "back")) { back = colon; }
					if (cpComma)
						opt = cpComma + 1;
					else
						opt = 0;
				}
			}

			if (fore.length()) {
				int red, green, blue;
				char buffer[20];
				// decompose it using simple sscanf...

				// red component...
				buffer[0] = fore[1];
				buffer[1] = fore[2];
				buffer[2] = '\0';
				sscanf(buffer, "%x", &red);

				// green component...
				buffer[0] = fore[3];
				buffer[1] = fore[4];
				buffer[2] = '\0';
				sscanf(buffer, "%x", &green);

				// blue component...
				buffer[0] = fore[5];
				buffer[1] = fore[6];
				buffer[2] = '\0';
				sscanf(buffer, "%x", &blue);

				// at last, we got the PDF colour!!!
				sprintf(buffer, "%3.2f %3.2f %3.2f", (red / 256.0f), (green / 256.0f), (blue / 256.0f) );
				PDFColours[istyle] = StringDup(buffer);
			}
		}
		if (val) { delete []val; }
		if (valdef) { delete []valdef; }
	}

	// the thing that identifies a PDF 1.3 file...
	fputs("%PDF-1.3\n", fp);

	int pageObjNumber = 100;	// it starts at object #100, it should fix this one to be more generic
	const int pageHeight = 60;		// for now this is fixed... i fix it once i have fonts & styles implemented...

	// do here all the writing
	int lengthDoc = LengthDocument();
	bool prevCR = false;
	int numLines = 0;
	WindowAccessor acc(wEditor.GetID(), props);

	int eolMode = SendEditor( SCI_GETEOLMODE, 0 );

	//
	bool firstTime = true;
	SString textObj = "";
	SString stream = "";
	int styleCurrent = 0;
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
			// which im gonna add in the next edition of the Save2PDF feature
			if ( !firstTime ) {
				// close the opened text object if there are any...
				if (!prevCR) {
					stream += ") Tj\n";
				} else {
					stream += ")\n";
				}

				// close last text object
				stream += "ET\n";

				// patch in the stream size (minus 1 since the newline is not counted... go figure)
				char *buffer = new char[textObj.size() + 1];
				sprintf(buffer, textObj.c_str(), stream.size() - 1);
				textObj = buffer;
				delete []buffer;

				// concatenate stream within the text object
				textObj += stream;
				textObj += "endstream\n";
				textObj += "endobj\n";

				// write the actual object to the PDF
				fputs( textObj.c_str(), fp );

			}
			firstTime = false;

			// open a text object
			char buffer[20];
			sprintf( buffer, "%d 0 obj\n", textObjNumber);
			textObj = buffer;
			textObj += "<< /Length %d >>\n"; // we should patch the length here correctly...
			textObj += "stream\n";

			// new stream ;-)
			stream = "BT\n";

			stream += "%% draw text string using current graphics state\n";
			stream += "/F1 12 Tf\n";
			stream += "1 0 0 1 20 750 Tm\n";

			// a new page should take the previous style information...
			// this is a glitch in the PDF spec... it is not persisted over multiple pages...
			// check that everything is okay... just to be 100% safe...
			int style = acc.StyleAt(i);
			if ( (style < STYLE_DEFAULT) && PDFColours[style]) {
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
		if ( style != styleCurrent) {
			// check that everything is okay... just to be 100% safe...
			if ( (style < STYLE_DEFAULT) && PDFColours[style]) {
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
			for (int itab = 0; itab < tabSize; itab++) {
				stream += " ";
			}
		} else if (ch == 0x0A) {
			// close and begin a newline...
			if (!prevCR) {
				stream += ") Tj\n";
				stream += "0 -12 TD\n";
				stream += "(";
			}
			// increment the number of lines if it is CRLF or LF
			if ( SC_EOL_CR != eolMode ) { numLines++; }

		} else if ( ch == 0x0D ) {
			stream += ") Tj\n";
			stream += "0 -12 TD\n";
			stream += "(";
			// increment the number of lines if it is in CR mode
			if ( SC_EOL_CR == eolMode ) { numLines++; }

		} else if ( (ch == ')') || (ch == '(') || (ch == '\\') ) {
			// you should escape those characters for PDF 1.2+
			char buffer[10];
			sprintf(buffer, "\\%c", ch);
			stream += buffer;
		} else {
			// write the character normally...
			stream += ch;
		}
		prevCR = (ch == '\r');
	}

	// close the opened text object if there are any...
	if (!prevCR) {
		stream += ") Tj\n";
	}

	// close last text object if and only if there is one open...
	if ( lengthDoc > 0) {
		// close last text object
		stream += "ET\n";

		// patch in the stream size (minus 1 since the newline is not counted... go figure)
		char *buffer = new char[textObj.size() + 1 + 200];  // 200 by Neil as this is quite indeterminate
		sprintf(buffer, textObj.c_str(), stream.size() - 1);
		textObj = buffer;
		delete buffer;

		// concatenate stream within the text object
		textObj += stream;
		textObj += "endstream\n";
		textObj += "endobj\n";

		// write the actual object to the PDF
		fputs( textObj.c_str(), fp );
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
		fputs("<< /Type /Page\n", fp);
		fputs("/Parent 3 0 R\n", fp);
		fputs("/MediaBox [ 0 0 612 792 ]\n", fp);

		// we need to patch in the corresponding page text object!
		int textObjNumber = (k + 1);
		sprintf( buffer, "/Contents %d 0 R\n", textObjNumber);
		fputs(buffer, fp);

		fputs("/Resources << /ProcSet 6 0 R\n", fp);
		fputs("/Font << /F1 7 0 R >>\n", fp);
		fputs(">>\n", fp);
		fputs(">>\n", fp);
		fputs("endobj\n", fp);

		// add this to the list of page number references...
		sprintf( buffer, "%d 0 R ", k);
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
	fputs("% catalog object\n", fp);
	fputs("1 0 obj\n", fp);
	fputs("<< /Type /Catalog\n", fp);
	fputs("/Outlines 2 0 R\n", fp);
	fputs("/Pages 3 0 R\n", fp);
	fputs(">>\n", fp);
	fputs("endobj\n", fp);

	// create an empty outline object
	fputs("2 0 obj\n", fp);
	fputs("<< /Type /Outlines\n", fp);
	fputs("/Count 0\n", fp);
	fputs(">>\n", fp);
	fputs("endobj\n", fp);

	//
	fputs("6 0 obj\n", fp);
	fputs("[ /PDF /Text ]\n", fp);
	fputs("endobj\n", fp);

	//
	fputs("7 0 obj\n", fp);
	fputs("<< /Type /Font\n", fp);
	fputs("/Subtype /Type1\n", fp);
	fputs("/Name /F1\n", fp);
	fputs("/BaseFont /Helvetica\n", fp);
	fputs("/Encoding /MacRomanEncoding\n", fp);
	fputs(">>\n", fp);
	fputs("endobj\n", fp);

	// end the file, with the trailer
	fputs("xref\n", fp);
	fputs("0 8\n", fp);
	fputs("0000000000 65535 f\n", fp);
	fputs("0000000009 00000 n\n", fp);
	fputs("0000000074 00000 n\n", fp);
	fputs("0000000120 00000 n\n", fp);
	fputs("0000000179 00000 n\n", fp);
	fputs("0000000364 00000 n\n", fp);
	fputs("0000000466 00000 n\n", fp);
	fputs("0000000496 00000 n\n", fp);
	fputs("trailer\n", fp);
	fputs("<< /Size 8\n", fp);
	fputs("/Root 1 0 R\n", fp);
	fputs(">>\n", fp);
	fputs("startxref\n", fp);
	fputs("0\n", fp);
	fputs("%%EOF\n", fp);

	// and close the PDF file
	fclose(fp);
#endif
}
