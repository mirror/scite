// SciTE - Scintilla based Text Editor
/** @file MatchMarker.cxx
 ** Mark all the matches of a string.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <string>
#include <vector>

#include "Scintilla.h"

#include "GUI.h"

#include "MatchMarker.h"

std::vector<LineRange> LinesBreak(GUI::ScintillaWindow *pSci) {
	std::vector<LineRange> lineRanges;
	if (pSci) {
		const int lineEnd = pSci->Call(SCI_GETLINECOUNT);
		const int lineStartVisible = pSci->Call(SCI_GETFIRSTVISIBLELINE);
		const int docLineStartVisible = pSci->Call(SCI_DOCLINEFROMVISIBLE, lineStartVisible);
		const int linesOnScreen = pSci->Call(SCI_LINESONSCREEN);
		const int surround = 40;
		LineRange rangePriority(docLineStartVisible - surround, docLineStartVisible + linesOnScreen + surround);
		if (rangePriority.lineStart < 0)
			rangePriority.lineStart = 0;
		if (rangePriority.lineEnd > lineEnd)
			rangePriority.lineEnd = lineEnd;
		lineRanges.push_back(rangePriority);
		if (rangePriority.lineEnd < lineEnd)
			lineRanges.emplace_back(rangePriority.lineEnd, lineEnd);
		if (rangePriority.lineStart > 0)
			lineRanges.emplace_back(0, rangePriority.lineStart);
	}
	return lineRanges;
}

MatchMarker::MatchMarker() :
	pSci(0), styleMatch(-1), flagsMatch(0), indicator(0), bookMark(-1) {
}

MatchMarker::~MatchMarker() {
}

void MatchMarker::StartMatch(GUI::ScintillaWindow *pSci_,
	const std::string &textMatch_, int flagsMatch_, int styleMatch_,
	int indicator_, int bookMark_) {
	lineRanges.clear();
	pSci = pSci_;
	textMatch = textMatch_;
	flagsMatch = flagsMatch_;
	styleMatch = styleMatch_;
	indicator = indicator_;
	bookMark = bookMark_;
	lineRanges = LinesBreak(pSci);
	// Perform the initial marking immediately to avoid flashing
	Continue();
}

bool MatchMarker::Complete() const {
	return lineRanges.empty();
}

void MatchMarker::Continue() {
	const int segment = 200;

	// Remove old indicators if any exist.
	pSci->Call(SCI_SETINDICATORCURRENT, indicator);

	const LineRange rangeSearch = lineRanges[0];
	int lineEndSegment = rangeSearch.lineStart + segment;
	if (lineEndSegment > rangeSearch.lineEnd)
		lineEndSegment = rangeSearch.lineEnd;

	pSci->Call(SCI_SETSEARCHFLAGS, flagsMatch);
	const int positionStart = pSci->Call(SCI_POSITIONFROMLINE, rangeSearch.lineStart);
	const int positionEnd = pSci->Call(SCI_POSITIONFROMLINE, lineEndSegment);
	pSci->Call(SCI_SETTARGETSTART, positionStart);
	pSci->Call(SCI_SETTARGETEND, positionEnd);
	pSci->Call(SCI_INDICATORCLEARRANGE, positionStart, positionEnd - positionStart);

	//Monitor the amount of time took by the search.
	GUI::ElapsedTime searchElapsedTime;

	// Find the first occurrence of word.
	int posFound = pSci->CallString(
		SCI_SEARCHINTARGET, textMatch.length(), textMatch.c_str());
	while (posFound != INVALID_POSITION) {
		// Limit the search duration to 250 ms. Avoid to freeze editor for huge lines.
		if (searchElapsedTime.Duration() > 0.25) {
			// Clear all indicators because timer has expired.
			pSci->Call(SCI_INDICATORCLEARRANGE, 0, pSci->Call(SCI_GETLENGTH));
			lineRanges.clear();
			break;
		}
		int posEndFound = pSci->Call(SCI_GETTARGETEND);

		if ((styleMatch < 0) || (styleMatch == pSci->Call(SCI_GETSTYLEAT, posFound))) {
			pSci->Call(SCI_INDICATORFILLRANGE, posFound, posEndFound - posFound);
			if (bookMark >= 0) {
				pSci->Call(SCI_MARKERADD,
					pSci->Call(SCI_LINEFROMPOSITION, posFound), bookMark);
			}
		}
		if (posEndFound == posFound) {
			// Empty matches are possible for regex
			posEndFound = pSci->Call(SCI_POSITIONAFTER, posEndFound);
		}
		// Try to find next occurrence of word.
		pSci->Call(SCI_SETTARGETSTART, posEndFound);
		pSci->Call(SCI_SETTARGETEND, positionEnd);
		posFound = pSci->CallString(
			SCI_SEARCHINTARGET, textMatch.length(), textMatch.c_str());
	}

	// Retire searched lines
	if (!lineRanges.empty()) {
		// Check in case of re-entrance
		if (lineEndSegment >= rangeSearch.lineEnd) {
			lineRanges.erase(lineRanges.begin());
		} else {
			lineRanges[0].lineStart = lineEndSegment;
		}
	}
}

void MatchMarker::Stop() {
	pSci = NULL;
	lineRanges.clear();
}
