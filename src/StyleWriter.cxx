// SciTE - Scintilla based Text Editor
/** @file StyleWriter.cxx
 ** Simple buffered interface to the text and styles of a document held by Scintilla.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstdint>

#include <string>

#include "ScintillaTypes.h"
#include "ScintillaCall.h"

#include "GUI.h"
#include "ScintillaWindow.h"
#include "StyleWriter.h"

namespace SA = Scintilla::API;

TextReader::TextReader(SA::ScintillaCall &sw_) :
	startPos(extremePosition),
	endPos(0),
	codePage(0),
	sw(sw_),
	lenDoc(-1) {
	buf[0] = 0;
}

bool TextReader::InternalIsLeadByte(char ch) const {
	return GUI::IsDBCSLeadByte(codePage, ch);
}

void TextReader::Fill(SA::Position position) {
	if (lenDoc == -1)
		lenDoc = sw.Length();
	startPos = position - slopSize;
	if (startPos + bufferSize > lenDoc)
		startPos = lenDoc - bufferSize;
	if (startPos < 0)
		startPos = 0;
	endPos = startPos + bufferSize;
	if (endPos > lenDoc)
		endPos = lenDoc;
	sw.SetTargetRange(startPos, endPos);
	sw.TargetText(buf);
}

bool TextReader::Match(SA::Position pos, const char *s) {
	for (int i=0; *s; i++) {
		if (*s != SafeGetCharAt(pos+i))
			return false;
		s++;
	}
	return true;
}

int TextReader::StyleAt(SA::Position position) {
	return static_cast<unsigned char>(sw.StyleAt(position));
}

SA::Line TextReader::GetLine(SA::Position position) {
	return sw.LineFromPosition(position);
}

SA::Position TextReader::LineStart(SA::Line line) {
	return sw.LineStart(line);
}

SA::FoldLevel TextReader::LevelAt(SA::Line line) {
	return sw.FoldLevel(line);
}

SA::Position TextReader::Length() {
	if (lenDoc == -1)
		lenDoc = sw.Length();
	return lenDoc;
}

int TextReader::GetLineState(SA::Line line) {
	return sw.LineState(line);
}

StyleWriter::StyleWriter(SA::ScintillaCall &sw_) :
	TextReader(sw_),
	validLen(0),
	startSeg(0) {
	styleBuf[0] = 0;
}

void StyleWriter::SetLineState(SA::Line line, int state) {
	sw.SetLineState(line, state);
}

void StyleWriter::StartAt(SA::Position start, char chMask) {
	sw.StartStyling(start, chMask);
}

void StyleWriter::StartSegment(SA::Position pos) {
	startSeg = pos;
}

void StyleWriter::ColourTo(SA::Position pos, int chAttr) {
	// Only perform styling if non empty range
	if (pos != startSeg - 1) {
		if (validLen + (pos - startSeg + 1) >= bufferSize)
			Flush();
		if (validLen + (pos - startSeg + 1) >= bufferSize) {
			// Too big for buffer so send directly
			sw.SetStyling(pos - startSeg + 1, chAttr);
		} else {
			for (SA::Position i = startSeg; i <= pos; i++) {
				styleBuf[validLen++] = static_cast<char>(chAttr);
			}
		}
	}
	startSeg = pos+1;
}

void StyleWriter::SetLevel(SA::Line line, SA::FoldLevel level) {
	sw.SetFoldLevel(line, level);
}

void StyleWriter::Flush() {
	startPos = extremePosition;
	lenDoc = -1;
	if (validLen > 0) {
		sw.SetStylingEx(validLen, styleBuf);
		validLen = 0;
	}
}
