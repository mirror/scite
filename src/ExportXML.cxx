// SciTE - Scintilla based Text Editor
/** @file ExportXML.cxx
 ** Export the current document to XML.
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
	bool failedWrite = fp == NULL;

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

		if (fclose(fp) != 0) {
			failedWrite = true;
		}
	}
	if (failedWrite) {
		FailedSaveMessageBox(saveName);
	}
}
