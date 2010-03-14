// SciTE - Scintilla based Text Editor
/** @file StyleWriter.cxx
 ** Simple buffered interface to the text and styles of a document held by Scintilla.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "Scintilla.h"
#include "GUI.h"
#include "StyleWriter.h"

bool TextReader::InternalIsLeadByte(char ch) const {
	return GUI::IsDBCSLeadByte(codePage, ch);
}

void TextReader::Fill(int position) {
	if (lenDoc == -1)
		lenDoc = sw.Send(SCI_GETTEXTLENGTH, 0, 0);
	startPos = position - slopSize;
	if (startPos + bufferSize > lenDoc)
		startPos = lenDoc - bufferSize;
	if (startPos < 0)
		startPos = 0;
	endPos = startPos + bufferSize;
	if (endPos > lenDoc)
		endPos = lenDoc;

	Sci_TextRange tr = {{startPos, endPos}, buf};
	sw.SendPointer(SCI_GETTEXTRANGE, 0, &tr);
}

bool TextReader::Match(int pos, const char *s) {
	for (int i=0; *s; i++) {
		if (*s != SafeGetCharAt(pos+i))
			return false;
		s++;
	}
	return true;
}

char TextReader::StyleAt(int position) {
	return static_cast<char>(sw.Send(
		SCI_GETSTYLEAT, position, 0));
}

int TextReader::GetLine(int position) {
	return sw.Send(SCI_LINEFROMPOSITION, position, 0);
}

int TextReader::LineStart(int line) {
	return sw.Send(SCI_POSITIONFROMLINE, line, 0);
}

int TextReader::LevelAt(int line) {
	return sw.Send(SCI_GETFOLDLEVEL, line, 0);
}

int TextReader::Length() {
	if (lenDoc == -1)
		lenDoc = sw.Send(SCI_GETTEXTLENGTH, 0, 0);
	return lenDoc;
}

int TextReader::GetLineState(int line) {
	return sw.Send(SCI_GETLINESTATE, line);
}

int StyleWriter::SetLineState(int line, int state) {
	return sw.Send(SCI_SETLINESTATE, line, state);
}

void StyleWriter::StartAt(unsigned int start, char chMask) {
	sw.Send(SCI_STARTSTYLING, start, chMask);
}

void StyleWriter::StartSegment(unsigned int pos) {
	startSeg = pos;
}

void StyleWriter::ColourTo(unsigned int pos, int chAttr) {
	// Only perform styling if non empty range
	if (pos != startSeg - 1) {
		if (validLen + (pos - startSeg + 1) >= bufferSize)
			Flush();
		if (validLen + (pos - startSeg + 1) >= bufferSize) {
			// Too big for buffer so send directly
			sw.Send(SCI_SETSTYLING, pos - startSeg + 1, chAttr);
		} else {
			for (unsigned int i = startSeg; i <= pos; i++) {
				styleBuf[validLen++] = static_cast<char>(chAttr);
			}
		}
	}
	startSeg = pos+1;
}

void StyleWriter::SetLevel(int line, int level) {
	sw.Send(SCI_SETFOLDLEVEL, line, level);
}

void StyleWriter::Flush() {
	startPos = extremePosition;
	lenDoc = -1;
	if (validLen > 0) {
		sw.SendPointer(SCI_SETSTYLINGEX, validLen, styleBuf);
		validLen = 0;
	}
}