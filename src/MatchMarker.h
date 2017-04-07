// SciTE - Scintilla based Text Editor
/** @file MatchMarker.h
 ** Mark all the matches of a string.
 **/
// Copyright 1998-2014 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

struct LineRange {
	int lineStart;
	int lineEnd;
	LineRange(int lineStart_, int lineEnd_) : lineStart(lineStart_), lineEnd(lineEnd_) {}
};

std::vector<LineRange> LinesBreak(GUI::ScintillaWindow *pSci);

class MatchMarker {
	GUI::ScintillaWindow *pSci;
	std::string textMatch;
	int styleMatch;
	int flagsMatch;
	int indicator;
	int bookMark;
	std::vector<LineRange> lineRanges;
public:
	MatchMarker();
	~MatchMarker();
	void StartMatch(GUI::ScintillaWindow *pSci_,
		const std::string &textMatch_, int flagsMatch_, int styleMatch_,
		int indicator_, int bookMark_);
	bool Complete() const;
	void Continue();
	void Stop();
};
