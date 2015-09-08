// SciTE - Scintilla based Text Editor
/** @file ExportHTML.cxx
 ** Export the current document to HTML.
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
			styleIsUsed[acc.StyleAt(i)] = true;
		}
	} else {
		for (int i = 0; i <= STYLE_MAX; i++) {
			styleIsUsed[i] = true;
		}
	}
	styleIsUsed[STYLE_DEFAULT] = true;

	FILE *fp = saveName.Open(GUI_TEXT("wt"));
	bool failedWrite = fp == NULL;
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

		StyleDefinition sddef = StyleDefinitionFor(STYLE_DEFAULT);

		if (sddef.back.length()) {
			bgColour = sddef.back;
		}

		std::string sval = props.GetExpandedString("font.monospace");
		StyleDefinition sdmono(sval.c_str());

		for (int istyle = 0; istyle <= STYLE_MAX; istyle++) {
			if ((istyle > STYLE_DEFAULT) && (istyle <= STYLE_LASTPREDEFINED))
				continue;
			if (styleIsUsed[istyle]) {

				StyleDefinition sd = StyleDefinitionFor(istyle);

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
		if (fclose(fp) != 0) {
			failedWrite = true;
		}
	}
	if (failedWrite) {
		FailedSaveMessageBox(saveName);
	}
}

