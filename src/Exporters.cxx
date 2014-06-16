// SciTE - Scintilla based Text Editor
/** @file Exporters.cxx
 ** Export the current document to various markup languages.
 **/
// Copyright 1998-2006 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>

#if defined(__unix__)

#include <unistd.h>

#if defined(GTK)
#include <gtk/gtk.h>
#endif

#else

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
#include <windows.h>
#include <commctrl.h>

// For chdir
#ifdef _MSC_VER
#include <direct.h>
#endif

#endif

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"

#include "SString.h"
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

	// Read the default settings
	char key[200];
	sprintf(key, "style.*.%0d", STYLE_DEFAULT);
	char *valdefDefault = StringDup(props.GetExpanded(key).c_str());
	sprintf(key, "style.%s.%0d", language.c_str(), STYLE_DEFAULT);
	char *valDefault = StringDup(props.GetExpanded(key).c_str());

	StyleDefinition defaultStyle(valdefDefault);
	defaultStyle.ParseStyleDefinition(valDefault);

	delete []valDefault;
	delete []valdefDefault;

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
		delete []val;
		delete []valdef;
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


//---------- Save to HTML ----------

void SciTEBase::SaveToHTML(FilePath saveName) {
	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;
	int wysiwyg = props.GetInt("export.html.wysiwyg", 1);
	int tabs = props.GetInt("export.html.tabs", 0);
	int folding = props.GetInt("export.html.folding", 0);
	int onlyStylesUsed = props.GetInt("export.html.styleused", 0);
	int titleFullPath = props.GetInt("export.html.title.fullpath", 0);

	int lengthDoc = LengthDocument();
	TextReader acc(wEditor);

	bool styleIsUsed[STYLE_MAX + 1];
	if (onlyStylesUsed) {
		int i;
		for (i = 0; i <= STYLE_MAX; i++) {
			styleIsUsed[i] = false;
		}
		// check the used styles
		for (i = 0; i < lengthDoc; i++) {
			styleIsUsed[acc.StyleAt(i) & 0x7F] = true;
		}
	} else {
		for (int i = 0; i <= STYLE_MAX; i++) {
			styleIsUsed[i] = true;
		}
	}
	styleIsUsed[STYLE_DEFAULT] = true;

	FILE *fp = saveName.Open(GUI_TEXT("wt"));
	if (fp) {
		fputs("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n", fp);
		fputs("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n", fp);
		fputs("<head>\n", fp);
		if (titleFullPath)
			fprintf(fp, "<title>%s</title>\n",
			        static_cast<const char *>(filePath.AsUTF8().c_str()));
		else
			fprintf(fp, "<title>%s</title>\n",
			        static_cast<const char *>(filePath.Name().AsUTF8().c_str()));
		// Probably not used by robots, but making a little advertisement for those looking
		// at the source code doesn't hurt...
		fputs("<meta name=\"Generator\" content=\"SciTE - www.Scintilla.org\" />\n", fp);
		if (codePage == SC_CP_UTF8)
			fputs("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n", fp);

		if (folding) {
			fputs("<script language=\"JavaScript\" type=\"text/javascript\">\n"
			      "<!--\n"
			      "function symbol(id, sym) {\n"
			      " if (id.textContent==undefined) {\n"
			      " id.innerText=sym; } else {\n"
			      " id.textContent=sym; }\n"
			      "}\n"
			      "function toggle(id) {\n"
			      "var thislayer=document.getElementById('ln'+id);\n"
			      "id-=1;\n"
			      "var togline=document.getElementById('hd'+id);\n"
			      "var togsym=document.getElementById('bt'+id);\n"
			      "if (thislayer.style.display == 'none') {\n"
			      " thislayer.style.display='block';\n"
			      " togline.style.textDecoration='none';\n"
			      " symbol(togsym,'- ');\n"
			      "} else {\n"
			      " thislayer.style.display='none';\n"
			      " togline.style.textDecoration='underline';\n"
			      " symbol(togsym,'+ ');\n"
			      "}\n"
			      "}\n"
			      "//-->\n"
			      "</script>\n", fp);
		}

		fputs("<style type=\"text/css\">\n", fp);

		std::string bgColour;
		char key[200];
		sprintf(key, "style.*.%0d", STYLE_DEFAULT);
		char *valdef = StringDup(props.GetExpanded(key).c_str());
		sprintf(key, "style.%s.%0d", language.c_str(), STYLE_DEFAULT);
		char *val = StringDup(props.GetExpanded(key).c_str());

		StyleDefinition sddef(valdef);
		sddef.ParseStyleDefinition(val);
		if (sddef.back.length()) {
			bgColour = sddef.back;
		}
		delete []val;
		delete []valdef;

		SString sval = props.GetExpanded("font.monospace");
		StyleDefinition sdmono(sval.c_str());

		for (int istyle = 0; istyle <= STYLE_MAX; istyle++) {
			if ((istyle > STYLE_DEFAULT) && (istyle <= STYLE_LASTPREDEFINED))
				continue;
			if (styleIsUsed[istyle]) {
				sprintf(key, "style.*.%0d", istyle);
				valdef = StringDup(props.GetExpanded(key).c_str());
				sprintf(key, "style.%s.%0d", language.c_str(), istyle);
				val = StringDup(props.GetExpanded(key).c_str());

				StyleDefinition sd(valdef);
				sd.ParseStyleDefinition(val);

				if (CurrentBuffer()->useMonoFont && sd.font.length() && sdmono.font.length()) {
					sd.font = sdmono.font;
					sd.size = sdmono.size;
					sd.italics = sdmono.italics;
					sd.weight = sdmono.weight;
				}

				if (sd.specified != StyleDefinition::sdNone) {
					if (istyle == STYLE_DEFAULT) {
						fprintf(fp, "span {\n");
					} else {
						fprintf(fp, ".S%0d {\n", istyle);
					}
					if (sd.italics) {
						fprintf(fp, "\tfont-style: italic;\n");
					}
					if (sd.IsBold()) {
						fprintf(fp, "\tfont-weight: bold;\n");
					}
					if (wysiwyg && sd.font.length()) {
						fprintf(fp, "\tfont-family: '%s';\n", sd.font.c_str());
					}
					if (sd.fore.length()) {
						fprintf(fp, "\tcolor: %s;\n", sd.fore.c_str());
					} else if (istyle == STYLE_DEFAULT) {
						fprintf(fp, "\tcolor: #000000;\n");
					}
					if ((sd.specified & StyleDefinition::sdBack) && sd.back.length()) {
						if (istyle != STYLE_DEFAULT && bgColour != sd.back) {
							fprintf(fp, "\tbackground: %s;\n", sd.back.c_str());
							fprintf(fp, "\ttext-decoration: inherit;\n");
						}
					}
					if (wysiwyg && sd.size) {
						fprintf(fp, "\tfont-size: %0dpt;\n", sd.size);
					}
					fprintf(fp, "}\n");
				} else {
					styleIsUsed[istyle] = false;	// No definition, it uses default style (32)
				}

				delete []val;
				delete []valdef;
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
		bool inFoldSpan = false;
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
				fprintf(fp, "<span id=\"hd%d\" onclick=\"toggle('%d')\">", line, line + 1);
				fprintf(fp, "<span id=\"bt%d\">- </span>", line);
				inFoldSpan = true;
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
				if (inFoldSpan) {
					fputs("</span>", fp);
					inFoldSpan = false;
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

					if (lvl & SC_FOLDLEVELHEADERFLAG) {
						fprintf(fp, "<span id=\"hd%d\" onclick=\"toggle('%d')\">", line, line + 1);
						fprintf(fp, "<span id=\"bt%d\">- </span>", line);
						inFoldSpan = true;
					} else
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
		GUI::gui_string msg = LocaliseMessage(
		            "Could not save file \"^0\".", filePath.AsInternal());
		WindowMessageBox(wSciTE, msg);
	}
}


//---------- Save to PDF ----------

/*
	PDF Exporter. Status: Beta
	Contributed by Ahmad M. Zawawi <zeus_go64@hotmail.com>
	Modifications by Darren Schroeder Feb 22, 2003; Philippe Lhoste 2003-10
	Overhauled by Kein-Hong Man 2003-11

	This exporter is meant to be small and simple; users are expected to
	use other methods for heavy-duty formatting. PDF elements marked with
	"PDF1.4Ref" states where in the PDF 1.4 Reference Spec (the PDF file of
	which is freely available from Adobe) the particular element can be found.

	Possible TODOs that will probably not be implemented: full styling,
	optimization, font substitution, compression, character set encoding.
*/
#define PDF_TAB_DEFAULT		8
#define PDF_FONT_DEFAULT	1	// Helvetica
#define PDF_FONTSIZE_DEFAULT	10
#define PDF_SPACING_DEFAULT	1.2
#define PDF_HEIGHT_DEFAULT	792	// Letter
#define PDF_WIDTH_DEFAULT	612
#define PDF_MARGIN_DEFAULT	72	// 1.0"
#define PDF_ENCODING		"WinAnsiEncoding"

struct PDFStyle {
	char fore[24];
	int font;
};

static const char *PDFfontNames[] = {
            "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
            "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
            "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic"
        };

// ascender, descender aligns font origin point with page
static short PDFfontAscenders[] =  { 629, 718, 699 };
static short PDFfontDescenders[] = { 157, 207, 217 };
static short PDFfontWidths[] =     { 600,   0,   0 };

inline void getPDFRGB(char* pdfcolour, const char* stylecolour) {
	// grab colour components (max string length produced = 18)
	for (int i = 1; i < 6; i += 2) {
		char val[20];
		// 3 decimal places for enough dynamic range
		int c = (IntFromHexByte(stylecolour + i) * 1000 + 127) / 255;
		if (c == 0 || c == 1000) {	// optimise
			sprintf(val, "%d ", c / 1000);
		} else {
			sprintf(val, "0.%03d ", c);
		}
		strcat(pdfcolour, val);
	}
}

void SciTEBase::SaveToPDF(FilePath saveName) {
	// This class conveniently handles the tracking of PDF objects
	// so that the cross-reference table can be built (PDF1.4Ref(p39))
	// All writes to fp passes through a PDFObjectTracker object.
	class PDFObjectTracker {
	private:
		FILE *fp;
		long *offsetList, tableSize;
		// Private so PDFObjectTracker objects can not be copied
		PDFObjectTracker(const PDFObjectTracker &) {}
	public:
		int index;
		PDFObjectTracker(FILE *fp_) {
			fp = fp_;
			tableSize = 100;
			offsetList = new long[tableSize];
			index = 1;
		}
		~PDFObjectTracker() {
			delete []offsetList;
		}
		void write(const char *objectData) {
			size_t length = strlen(objectData);
			// note binary write used, open with "wb"
			fwrite(objectData, sizeof(char), length, fp);
		}
		void write(int objectData) {
			char val[20];
			sprintf(val, "%d", objectData);
			write(val);
		}
		// returns object number assigned to the supplied data
		int add(const char *objectData) {
			// resize xref offset table if too small
			if (index > tableSize) {
				long newSize = tableSize * 2;
				long *newList = new long[newSize];
				for (int i = 0; i < tableSize; i++) {
					newList[i] = offsetList[i];
				}
				delete []offsetList;
				offsetList = newList;
				tableSize = newSize;
			}
			// save offset, then format and write object
			offsetList[index - 1] = ftell(fp);
			write(index);
			write(" 0 obj\n");
			write(objectData);
			write("endobj\n");
			return index++;
		}
		// builds xref table, returns file offset of xref table
		long xref() {
			char val[32];
			// xref start index and number of entries
			long xrefStart = ftell(fp);
			write("xref\n0 ");
			write(index);
			// a xref entry *must* be 20 bytes long (PDF1.4Ref(p64))
			// so extra space added; also the first entry is special
			write("\n0000000000 65535 f \n");
			for (int i = 0; i < index - 1; i++) {
				sprintf(val, "%010ld 00000 n \n", offsetList[i]);
				write(val);
			}
			return xrefStart;
		}
	};

	// Object to manage line and page rendering. Apart from startPDF, endPDF
	// everything goes in via add() and nextLine() so that line formatting
	// and pagination can be done properly.
	class PDFRender {
	private:
		bool pageStarted;
		bool firstLine;
		int pageCount;
		int pageContentStart;
		double xPos, yPos;	// position tracking for line wrapping
		SString pageData;	// holds PDF stream contents
		SString segment;	// character data
		char *segStyle;		// style of segment
		bool justWhiteSpace;
		int styleCurrent, stylePrev;
		double leading;
		char *buffer;
		// Private so PDFRender objects can not be copied
		PDFRender(const PDFRender &) {}
	public:
		PDFObjectTracker *oT;
		PDFStyle *style;
		int fontSize;		// properties supplied by user
		int fontSet;
		long pageWidth, pageHeight;
		GUI::Rectangle pageMargin;
		//
		PDFRender() {
			pageStarted = false;
			firstLine = false;
			pageCount = 0;
			pageContentStart = 0;
			xPos = 0.0;
			yPos = 0.0;
			justWhiteSpace = true;
			styleCurrent = STYLE_DEFAULT;
			stylePrev = STYLE_DEFAULT;
			leading = PDF_FONTSIZE_DEFAULT * PDF_SPACING_DEFAULT;
			oT = NULL;
			style = NULL;
			fontSize = 0;
			fontSet = PDF_FONT_DEFAULT;
			pageWidth = 100;
			pageHeight = 100;
			buffer = new char[250];
			segStyle = new char[100];
		}
		~PDFRender() {
			delete []style;
			delete []buffer;
			delete []segStyle;
		}
		//
		double fontToPoints(int thousandths) const {
			return (double)fontSize * thousandths / 1000.0;
		}
		void setStyle(char *buff, int style_) {
			int styleNext = style_;
			if (style_ == -1) { styleNext = styleCurrent; }
			*buff = '\0';
			if (styleNext != styleCurrent || style_ == -1) {
				if (style[styleCurrent].font != style[styleNext].font
				        || style_ == -1) {
					sprintf(buff, "/F%d %d Tf ",
					        style[styleNext].font + 1, fontSize);
				}
				if (strcmp(style[styleCurrent].fore, style[styleNext].fore) != 0
				        || style_ == -1) {
					strcat(buff, style[styleNext].fore);
					strcat(buff, "rg ");
				}
			}
		}
		//
		void startPDF() {
			if (fontSize <= 0) {
				fontSize = PDF_FONTSIZE_DEFAULT;
			}
			// leading is the term for distance between lines
			leading = fontSize * PDF_SPACING_DEFAULT;
			// sanity check for page size and margins
			int pageWidthMin = (int)leading + pageMargin.left + pageMargin.right;
			if (pageWidth < pageWidthMin) {
				pageWidth = pageWidthMin;
			}
			int pageHeightMin = (int)leading + pageMargin.top + pageMargin.bottom;
			if (pageHeight < pageHeightMin) {
				pageHeight = pageHeightMin;
			}
			// start to write PDF file here (PDF1.4Ref(p63))
			// ASCII>127 characters to indicate binary-possible stream
			oT->write("%PDF-1.3\n%\xc7\xec\x8f\xa2\n");
			styleCurrent = STYLE_DEFAULT;

			// build objects for font resources; note that font objects are
			// *expected* to start from index 1 since they are the first objects
			// to be inserted (PDF1.4Ref(p317))
			for (int i = 0; i < 4; i++) {
				sprintf(buffer, "<</Type/Font/Subtype/Type1"
				        "/Name/F%d/BaseFont/%s/Encoding/"
				        PDF_ENCODING
				        ">>\n", i + 1,
				        PDFfontNames[fontSet * 4 + i]);
				oT->add(buffer);
			}
			pageContentStart = oT->index;
		}
		void endPDF() {
			if (pageStarted) {	// flush buffers
				endPage();
			}
			// refer to all used or unused fonts for simplicity
			int resourceRef = oT->add(
			            "<</ProcSet[/PDF/Text]\n"
			            "/Font<</F1 1 0 R/F2 2 0 R/F3 3 0 R"
			            "/F4 4 0 R>> >>\n");
			// create all the page objects (PDF1.4Ref(p88))
			// forward reference pages object; calculate its object number
			int pageObjectStart = oT->index;
			int pagesRef = pageObjectStart + pageCount;
			for (int i = 0; i < pageCount; i++) {
				sprintf(buffer, "<</Type/Page/Parent %d 0 R\n"
				        "/MediaBox[ 0 0 %ld %ld"
				        "]\n/Contents %d 0 R\n"
				        "/Resources %d 0 R\n>>\n",
				        pagesRef, pageWidth, pageHeight,
				        pageContentStart + i, resourceRef);
				oT->add(buffer);
			}
			// create page tree object (PDF1.4Ref(p86))
			pageData = "<</Type/Pages/Kids[\n";
			for (int j = 0; j < pageCount; j++) {
				sprintf(buffer, "%d 0 R\n", pageObjectStart + j);
				pageData += buffer;
			}
			sprintf(buffer, "]/Count %d\n>>\n", pageCount);
			pageData += buffer;
			oT->add(pageData.c_str());
			// create catalog object (PDF1.4Ref(p83))
			sprintf(buffer, "<</Type/Catalog/Pages %d 0 R >>\n", pagesRef);
			int catalogRef = oT->add(buffer);
			// append the cross reference table (PDF1.4Ref(p64))
			long xref = oT->xref();
			// end the file with the trailer (PDF1.4Ref(p67))
			sprintf(buffer, "trailer\n<< /Size %d /Root %d 0 R\n>>"
			        "\nstartxref\n%ld\n%%%%EOF\n",
			        oT->index, catalogRef, xref);
			oT->write(buffer);
		}
		void add(char ch, int style_) {
			if (!pageStarted) {
				startPage();
			}
			// get glyph width (TODO future non-monospace handling)
			double glyphWidth = fontToPoints(PDFfontWidths[fontSet]);
			xPos += glyphWidth;
			// if cannot fit into a line, flush, wrap to next line
			if (xPos > pageWidth - pageMargin.right) {
				nextLine();
				xPos += glyphWidth;
			}
			// if different style, then change to style
			if (style_ != styleCurrent) {
				flushSegment();
				// output code (if needed) for new style
				setStyle(segStyle, style_);
				stylePrev = styleCurrent;
				styleCurrent = style_;
			}
			// escape these characters
			if (ch == ')' || ch == '(' || ch == '\\') {
				segment += '\\';
			}
			if (ch != ' ') { justWhiteSpace = false; }
			segment += ch;	// add to segment data
		}
		void flushSegment() {
			if (segment.length() > 0) {
				if (justWhiteSpace) {	// optimise
					styleCurrent = stylePrev;
				} else {
					pageData += segStyle;
				}
				pageData += "(";
				pageData += segment;
				pageData += ")Tj\n";
			}
			segment.clear();
			*segStyle = '\0';
			justWhiteSpace = true;
		}
		void startPage() {
			pageStarted = true;
			firstLine = true;
			pageCount++;
			double fontAscender = fontToPoints(PDFfontAscenders[fontSet]);
			yPos = pageHeight - pageMargin.top - fontAscender;
			// start a new page
			sprintf(buffer, "BT 1 0 0 1 %d %d Tm\n",
			        pageMargin.left, (int)yPos);
			// force setting of initial font, colour
			setStyle(segStyle, -1);
			strcat(buffer, segStyle);
			pageData = buffer;
			xPos = pageMargin.left;
			segment.clear();
			flushSegment();
		}
		void endPage() {
			pageStarted = false;
			flushSegment();
			// build actual text object; +3 is for "ET\n"
			// PDF1.4Ref(p38) EOL marker preceding endstream not counted
			char *textObj = new char[pageData.length() + 100];
			// concatenate stream within the text object
			sprintf(textObj, "<</Length %d>>\nstream\n%s"
			        "ET\nendstream\n",
			        static_cast<int>(pageData.length() - 1 + 3),
			        pageData.c_str());
			oT->add(textObj);
			delete []textObj;
		}
		void nextLine() {
			if (!pageStarted) {
				startPage();
			}
			xPos = pageMargin.left;
			flushSegment();
			// PDF follows cartesian coords, subtract -> down
			yPos -= leading;
			double fontDescender = fontToPoints(PDFfontDescenders[fontSet]);
			if (yPos < pageMargin.bottom + fontDescender) {
				endPage();
				startPage();
				return;
			}
			if (firstLine) {
				// avoid breakage due to locale setting
				int f = (int)(leading * 10 + 0.5);
				sprintf(buffer, "0 -%d.%d TD\n", f / 10, f % 10);
				firstLine = false;
			} else {
				sprintf(buffer, "T*\n");
			}
			pageData += buffer;
		}
	};
	PDFRender pr;

	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);
	// read exporter flags
	int tabSize = props.GetInt("tabsize", PDF_TAB_DEFAULT);
	if (tabSize < 0) {
		tabSize = PDF_TAB_DEFAULT;
	}
	// read magnification value to add to default screen font size
	pr.fontSize = props.GetInt("export.pdf.magnification");
	// set font family according to face name
	SString propItem = props.GetExpanded("export.pdf.font");
	pr.fontSet = PDF_FONT_DEFAULT;
	if (propItem.length()) {
		if (propItem == "Courier")
			pr.fontSet = 0;
		else if (propItem == "Helvetica")
			pr.fontSet = 1;
		else if (propItem == "Times")
			pr.fontSet = 2;
	}
	// page size: width, height
	propItem = props.GetExpanded("export.pdf.pagesize");
	char *buffer = new char[200];
	char *ps = StringDup(propItem.c_str());
	const char *next = GetNextPropItem(ps, buffer, 32);
	if (0 >= (pr.pageWidth = atol(buffer))) {
		pr.pageWidth = PDF_WIDTH_DEFAULT;
	}
	GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageHeight = atol(buffer))) {
		pr.pageHeight = PDF_HEIGHT_DEFAULT;
	}
	delete []ps;
	// page margins: left, right, top, bottom
	propItem = props.GetExpanded("export.pdf.margins");
	ps = StringDup(propItem.c_str());
	next = GetNextPropItem(ps, buffer, 32);
	if (0 >= (pr.pageMargin.left = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.left = PDF_MARGIN_DEFAULT;
	}
	next = GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.right = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.right = PDF_MARGIN_DEFAULT;
	}
	next = GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.top = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.top = PDF_MARGIN_DEFAULT;
	}
	GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.bottom = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.bottom = PDF_MARGIN_DEFAULT;
	}
	delete []ps;

	// collect all styles available for that 'language'
	// or the default style if no language is available...
	pr.style = new PDFStyle[STYLE_MAX + 1];
	for (int i = 0; i <= STYLE_MAX; i++) {	// get keys
		pr.style[i].font = 0;
		pr.style[i].fore[0] = '\0';

		sprintf(buffer, "style.*.%0d", i);
		char *valdef = StringDup(props.GetExpanded(buffer).c_str());
		sprintf(buffer, "style.%s.%0d", language.c_str(), i);
		char *val = StringDup(props.GetExpanded(buffer).c_str());

		StyleDefinition sd(valdef);
		sd.ParseStyleDefinition(val);

		if (sd.specified != StyleDefinition::sdNone) {
			if (sd.italics) { pr.style[i].font |= 2; }
			if (sd.IsBold()) { pr.style[i].font |= 1; }
			if (sd.fore.length()) {
				getPDFRGB(pr.style[i].fore, sd.fore.c_str());
			} else if (i == STYLE_DEFAULT) {
				strcpy(pr.style[i].fore, "0 0 0 ");
			}
			// grab font size from default style
			if (i == STYLE_DEFAULT) {
				if (sd.size > 0)
					pr.fontSize += sd.size;
				else
					pr.fontSize = PDF_FONTSIZE_DEFAULT;
			}
		}
		delete []val;
		delete []valdef;
	}
	// patch in default foregrounds
	for (int j = 0; j <= STYLE_MAX; j++) {
		if (pr.style[j].fore[0] == '\0') {
			strcpy(pr.style[j].fore, pr.style[STYLE_DEFAULT].fore);
		}
	}
	delete []buffer;

	FILE *fp = saveName.Open(GUI_TEXT("wb"));
	if (!fp) {
		// couldn't open the file for saving, issue an error message
		GUI::gui_string msg = LocaliseMessage("Could not save file '^0'.", filePath.AsInternal());
		WindowMessageBox(wSciTE, msg);
		return;
	}
	// initialise PDF rendering
	PDFObjectTracker ot(fp);
	pr.oT = &ot;
	pr.startPDF();

	// do here all the writing
	int lengthDoc = LengthDocument();
	TextReader acc(wEditor);

	if (!lengthDoc) {	// enable zero length docs
		pr.nextLine();
	} else {
		int lineIndex = 0;
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (ch == '\t') {
				// expand tabs
				int ts = tabSize - (lineIndex % tabSize);
				lineIndex += ts;
				for (; ts; ts--) {	// add ts count of spaces
					pr.add(' ', style);	// add spaces
				}
			} else if (ch == '\r' || ch == '\n') {
				if (ch == '\r' && acc[i + 1] == '\n') {
					i++;
				}
				// close and begin a newline...
				pr.nextLine();
				lineIndex = 0;
			} else {
				// write the character normally...
				pr.add(ch, style);
				lineIndex++;
			}
		}
	}
	// write required stuff and close the PDF file
	pr.endPDF();
	fclose(fp);
}


//---------- Save to TeX ----------

static char* getTexRGB(char* texcolor, const char* stylecolor) {
	//texcolor[rgb]{0,0.5,0}{....}
	double rf = IntFromHexByte(stylecolor + 1) / 256.0;
	double gf = IntFromHexByte(stylecolor + 3) / 256.0;
	double bf = IntFromHexByte(stylecolor + 5) / 256.0;
	// avoid breakage due to locale setting
	int r = (int)(rf * 10 + 0.5);
	int g = (int)(gf * 10 + 0.5);
	int b = (int)(bf * 10 + 0.5);
	sprintf(texcolor, "%d.%d, %d.%d, %d.%d", r / 10, r % 10, g / 10, g % 10, b / 10, b % 10);
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
	if (style.IsBold()) {
		fputs("\\textbf{", fp);
		closing_brackets++;
	}
	if (style.fore.length()) {
		fprintf(fp, "\\textcolor[rgb]{%s}{", getTexRGB(rgb, style.fore.c_str()) );
		closing_brackets++;
	}
	if (style.back.length()) {
		fprintf(fp, "\\colorbox[rgb]{%s}{", getTexRGB( rgb, style.back.c_str()) );
		closing_brackets++;
	}
	fputs("#1", fp);
	for (int i = 0; i <= closing_brackets; i++) {
		fputc( '}', fp );
	}
	fputc('\n', fp);
}

void SciTEBase::SaveToTEX(FilePath saveName) {
	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;

	int lengthDoc = LengthDocument();
	TextReader acc(wEditor);
	bool styleIsUsed[STYLE_MAX + 1];

	int titleFullPath = props.GetInt("export.tex.title.fullpath", 0);

	int i;
	for (i = 0; i <= STYLE_MAX; i++) {
		styleIsUsed[i] = false;
	}
	for (i = 0; i < lengthDoc; i++) {	// check the used styles
		styleIsUsed[acc.StyleAt(i) & 0X7f] = true;
	}
	styleIsUsed[STYLE_DEFAULT] = true;

	FILE *fp = saveName.Open(GUI_TEXT("wt"));
	if (fp) {
		fputs("\\documentclass[a4paper]{article}\n"
		      "\\usepackage[a4paper,margin=2cm]{geometry}\n"
		      "\\usepackage[T1]{fontenc}\n"
		      "\\usepackage{color}\n"
		      "\\usepackage{alltt}\n"
 		      "\\usepackage{times}\n"
 		      "\\setlength{\\fboxsep}{0pt}\n", fp);

		for (i = 0; i < STYLE_MAX; i++) {      // get keys
			if (styleIsUsed[i]) {
				char key[200];
				sprintf(key, "style.*.%0d", i);
				char *valdef = StringDup(props.GetExpanded(key).c_str());
				sprintf(key, "style.%s.%0d", language.c_str(), i);
				char *val = StringDup(props.GetExpanded(key).c_str());

				StyleDefinition sd(valdef); //check default properties
				sd.ParseStyleDefinition(val); //check language properties

				defineTexStyle(sd, fp, i); // writeout style macroses
				delete []val;
				delete []valdef;
			}
		}

		fputs("\\begin{document}\n\n", fp);
		fprintf(fp, "Source File: %s\n\n\\noindent\n\\small{\n",
		        static_cast<const char *>(titleFullPath ? filePath.AsUTF8().c_str() : filePath.Name().AsUTF8().c_str()));

		int styleCurrent = acc.StyleAt(0);

		fprintf(fp, "\\scite%s{", texStyle(styleCurrent));

		int lineIdx = 0;

		for (i = 0; i < lengthDoc; i++) { //here process each character of the document
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (style != styleCurrent) { //new style?
				fprintf(fp, "}\\scite%s{", texStyle(style) );
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
		fputs("}\n} %end small\n\n\\end{document}\n", fp); //close last empty style macros and document too
		fclose(fp);
	} else {
		GUI::gui_string msg = LocaliseMessage(
		            "Could not save file \"^0\".", filePath.AsInternal());
		WindowMessageBox(wSciTE, msg);
	}
}


//---------- Save to XML ----------

void SciTEBase::SaveToXML(FilePath saveName) {

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

	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);

	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0) {
		tabSize = 4;
	}

	int lengthDoc = LengthDocument();

	TextReader acc(wEditor);

	FILE *fp = saveName.Open(GUI_TEXT("wt"));

	if (fp) {

		bool collapseSpaces = (props.GetInt("export.xml.collapse.spaces", 1) == 1);
		bool collapseLines  = (props.GetInt("export.xml.collapse.lines", 1) == 1);

		fprintf(fp, "<?xml version='1.0' encoding='%s'?>\n", (codePage == SC_CP_UTF8) ? "utf-8" : "ascii");

		fputs("<document xmlns='http://www.scintila.org/scite.rng'", fp);
		fprintf(fp, " filename='%s'",
		        static_cast<const char *>(filePath.Name().AsUTF8().c_str()));
		fprintf(fp, " type='%s'", "unknown");
		fprintf(fp, " version='%s'", "1.0");
		fputs(">\n", fp);

		fputs("<data comment='This element is reserved for future usage.'/>\n", fp);

		fputs("<text>\n", fp);

		int styleCurrent = -1; // acc.StyleAt(0);
		int lineNumber = 1;
		int lineIndex = 0;
		bool styleDone = false;
		bool lineDone = false;
		bool charDone = false;
		int styleNew = -1;
		int spaceLen = 0;
		int emptyLines = 0;

		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style != styleCurrent) {
				styleCurrent = style;
				styleNew = style;
			}
			if (ch == ' ') {
				spaceLen++;
			} else if (ch == '\t') {
				int ts = tabSize - (lineIndex % tabSize);
				lineIndex += ts - 1;
				spaceLen += ts;
			} else if (ch == '\f') {
				// ignore this animal
			} else if (ch == '\r' || ch == '\n') {
				if (ch == '\r' && acc[i + 1] == '\n') {
					i++;
				}
				if (styleDone) {
					fputs("</t>", fp);
					styleDone = false;
				}
				lineIndex = -1;
				if (lineDone) {
					fputs("</line>\n", fp);
					lineDone = false;
				} else if (collapseLines) {
					emptyLines++;
				} else {
					fprintf(fp, "<line n='%d'/>\n", lineNumber);
				}
				charDone = false;
				lineNumber++;
				styleCurrent = -1; // acc.StyleAt(i + 1);
			} else {
				if (collapseLines && (emptyLines > 0)) {
					fputs("<line/>\n", fp);
				}
				emptyLines = 0;
				if (! lineDone) {
					fprintf(fp, "<line n='%d'>", lineNumber);
					lineDone = true;
				}
				if (styleNew >= 0) {
					if (styleDone) { fputs("</t>", fp); }
				}
				if (! collapseSpaces) {
					while (spaceLen > 0) {
						fputs("<s/>", fp);
						spaceLen--;
					}
				} else if (spaceLen == 1) {
					fputs("<s/>", fp);
					spaceLen = 0;
				} else if (spaceLen > 1) {
					fprintf(fp, "<s n='%d'/>", spaceLen);
					spaceLen = 0;
				}
				if (styleNew >= 0) {
					fprintf(fp, "<t n='%d'>", style);
					styleNew = -1;
					styleDone = true;
				}
				switch (ch) {
				case '>' :
					fputs("<g/>", fp);
					break;
				case '<' :
					fputs("<l/>", fp);
					break;
				case '&' :
					fputs("<a/>", fp);
					break;
				case '#' :
					fputs("<h/>", fp);
					break;
				default  :
					fputc(ch, fp);
				}
				charDone = true;
			}
			lineIndex++;
		}
		if (styleDone) {
			fputs("</t>", fp);
		}
		if (lineDone) {
			fputs("</line>\n", fp);
		}
		if (charDone) {
			// no last empty line: fprintf(fp, "<line n='%d'/>", lineNumber);
		}

		fputs("</text>\n", fp);
		fputs("</document>\n", fp);

		fclose(fp);
	} else {
		GUI::gui_string msg = LocaliseMessage("Could not save file \"^0\".", filePath.AsInternal());
		WindowMessageBox(wSciTE, msg);
	}
}
