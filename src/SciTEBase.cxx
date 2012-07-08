// SciTE - Scintilla based Text Editor
/** @file SciTEBase.cxx
 ** Platform independent base class of editor.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#if defined(__unix__)

#include <unistd.h>

#if defined(GTK)
#include <gtk/gtk.h>
#endif

#else

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
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

#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __DMC__
#include <dir.h>
#endif

#endif

#include "Scintilla.h"
#include "SciLexer.h"
#include "ILexer.h"

#include "GUI.h"
#include "SString.h"
#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"

#include "Cookie.h"
#include "Worker.h"
#include "FileWorker.h"
#include "SciTEBase.h"

Searcher::Searcher() {
	wholeWord = false;
	matchCase = false;
	regExp = false;
	unSlash = false;
	wrapFind = true;
	reverseFind = false;

	replacing = false;
	havefound = false;
	findInStyle = false;
	findStyle = 0;
	closeFind = true;

	focusOnReplace = false;
}

// The find and replace dialogs and strips often manipulate boolean
// flags based on dialog control IDs and menu IDs.
bool &Searcher::FlagFromCmd(int cmd) {
	static bool notFound;
	switch (cmd) {
		case IDWHOLEWORD:
		case IDM_WHOLEWORD:
			return wholeWord;
		case IDMATCHCASE:
		case IDM_MATCHCASE:
			return matchCase;
		case IDREGEXP:
		case IDM_REGEXP:
			return regExp;
		case IDUNSLASH:
		case IDM_UNSLASH:
			return unSlash;
		case IDWRAP:
		case IDM_WRAPAROUND:
			return wrapFind;
		case IDDIRECTIONUP:
		case IDM_DIRECTIONUP:
			return reverseFind;
	}
	return notFound;
}

SciTEBase::SciTEBase(Extension *ext) : apis(true), extender(ext) {
	codePage = 0;
	characterSet = 0;
	language = "java";
	lexLanguage = SCLEX_CPP;
	lexLPeg = -1;
	functionDefinition = 0;
	indentOpening = true;
	indentClosing = true;
	indentMaintain = false;
	statementLookback = 10;
	preprocessorSymbol = '\0';

	tbVisible = false;
	sbVisible = false;
	tabVisible = false;
	tabHideOne = false;
	tabMultiLine = false;
	sbNum = 1;
	visHeightTools = 0;
	visHeightTab = 0;
	visHeightStatus = 0;
	visHeightEditor = 1;
	heightBar = 7;
	dialogsOnScreen = 0;
	topMost = false;
	wrap = false;
	wrapOutput = false;
	wrapStyle = SC_WRAP_WORD;
	isReadOnly = false;
	openFilesHere = false;
	fullScreen = false;

	heightOutput = 0;
	heightOutputStartDrag = 0;
	previousHeightOutput = 0;

	allowMenuActions = true;
	scrollOutput = 1;
	returnOutputToCommand = true;

	ptStartDrag.x = 0;
	ptStartDrag.y = 0;
	capturedMouse = false;
	firstPropertiesRead = true;
	localiser.read = false;
	splitVertical = false;
	bufferedDraw = true;
	twoPhaseDraw = true;
	bracesCheck = true;
	bracesSloppy = false;
	bracesStyle = 0;
	braceCount = 0;

	indentationWSVisible = true;
	indentExamine = SC_IV_LOOKBOTH;
	autoCompleteIgnoreCase = false;
	callTipUseEscapes = false;
	callTipIgnoreCase = false;
	autoCCausedByOnlyOne = false;
	startCalltipWord = 0;
	currentCallTip = 0;
	maxCallTips = 1;
	currentCallTipWord = "";
	lastPosCallTip = 0;

	margin = false;
	marginWidth = marginWidthDefault;
	foldMargin = true;
	foldMarginWidth = foldMarginWidthDefault;
	lineNumbers = false;
	lineNumbersWidth = lineNumbersWidthDefault;
	lineNumbersExpand = false;

	abbrevInsert[0] = '\0';

	languageMenu = 0;
	languageItems = 0;

	shortCutItemList = 0;
	shortCutItems = 0;

	macrosEnabled = false;
	recording = false;

	propsBase.superPS = &propsEmbed;
	propsUser.superPS = &propsBase;
	propsDirectory.superPS = &propsUser;
	propsLocal.superPS = &propsDirectory;
	propsDiscovered.superPS = &propsLocal;
	props.superPS = &propsDiscovered;

	propsStatus.superPS = &props;

	needReadProperties = false;
	quitting = false;

	timerMask = 0;
	delayBeforeAutoSave = 0;
}

SciTEBase::~SciTEBase() {
	TimerEnd(timerAutoSave);
	if (extender)
		extender->Finalise();
	delete []languageMenu;
	delete []shortCutItemList;
	popup.Destroy();
}

void SciTEBase::WorkerCommand(int cmd, Worker *pWorker) {
	switch (cmd) {
	case WORK_FILEREAD:
		TextRead(static_cast<FileLoader *>(pWorker));
		UpdateProgress(pWorker);
		break;
	case WORK_FILEWRITTEN:
		TextWritten(static_cast<FileStorer *>(pWorker));
		UpdateProgress(pWorker);
		break;
	case WORK_FILEPROGRESS:
 		UpdateProgress(pWorker);
		break;
	}
}

int SciTEBase::CallFocused(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	if (wOutput.HasFocus())
		return wOutput.Call(msg, wParam, lParam);
	else
		return wEditor.Call(msg, wParam, lParam);
}

sptr_t SciTEBase::CallPane(int destination, unsigned int msg, uptr_t wParam, sptr_t lParam) {
	if (destination == IDM_SRCWIN)
		return wEditor.Call(msg, wParam, lParam);
	else if (destination == IDM_RUNWIN)
		return wOutput.Call(msg, wParam, lParam);
	else
		return CallFocused(msg, wParam, lParam);
}

void SciTEBase::CallChildren(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	wEditor.Call(msg, wParam, lParam);
	wOutput.Call(msg, wParam, lParam);
}

SString SciTEBase::GetTranslationToAbout(const char * const propname, bool retainIfNotFound) {
#if !defined(GTK)
	return SString(GUI::UTF8FromString(localiser.Text(propname, retainIfNotFound)).c_str());
#else
	// On GTK+, localiser.Text always converts to UTF-8.
	return SString(localiser.Text(propname, retainIfNotFound).c_str());
#endif
}

void SciTEBase::ViewWhitespace(bool view) {
	if (view && indentationWSVisible)
		wEditor.Call(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
	else if (view)
		wEditor.Call(SCI_SETVIEWWS, SCWS_VISIBLEAFTERINDENT);
	else
		wEditor.Call(SCI_SETVIEWWS, SCWS_INVISIBLE);
}

StyleAndWords SciTEBase::GetStyleAndWords(const char *base) {
	StyleAndWords sw;
	SString fileNameForExtension = ExtensionFileName();
	SString sAndW = props.GetNewExpand(base, fileNameForExtension.c_str());
	sw.styleNumber = sAndW.value();
	const char *space = strchr(sAndW.c_str(), ' ');
	if (space)
		sw.words = space + 1;
	return sw;
}

void SciTEBase::AssignKey(int key, int mods, int cmd) {
	wEditor.Call(SCI_ASSIGNCMDKEY,
	        LongFromTwoShorts(static_cast<short>(key),
	                static_cast<short>(mods)), cmd);
}

/**
 * Override the language of the current file with the one indicated by @a cmdID.
 * Mostly used to set a language on a file of unknown extension.
 */
void SciTEBase::SetOverrideLanguage(int cmdID) {
	RecentFile rf = GetFilePosition();
	EnsureRangeVisible(wEditor, 0, wEditor.Call(SCI_GETLENGTH), false);
	// Zero all the style bytes
	wEditor.Call(SCI_CLEARDOCUMENTSTYLE);

	CurrentBuffer()->overrideExtension = "x.";
	CurrentBuffer()->overrideExtension += languageMenu[cmdID].extension;
	ReadProperties();
	SetIndentSettings();
	wEditor.Call(SCI_COLOURISE, 0, -1);
	Redraw();
	DisplayAround(rf);
}

int SciTEBase::LengthDocument() {
	return wEditor.Call(SCI_GETLENGTH);
}

int SciTEBase::GetCaretInLine() {
	int caret = wEditor.Call(SCI_GETCURRENTPOS);
	int line = wEditor.Call(SCI_LINEFROMPOSITION, caret);
	int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, line);
	return caret - lineStart;
}

void SciTEBase::GetLine(char *text, int sizeText, int line) {
	if (line < 0)
		line = GetCurrentLineNumber();
	int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, line);
	int lineEnd = wEditor.Call(SCI_GETLINEENDPOSITION, line);
	int lineMax = lineStart + sizeText - 1;
	if (lineEnd > lineMax)
		lineEnd = lineMax;
	GetRange(wEditor, lineStart, lineEnd, text);
	text[lineEnd - lineStart] = '\0';
}

SString SciTEBase::GetLine(int line) {
	int len;
	// Get needed buffer size
	if (line < 0) {
		len = wEditor.Call(SCI_GETCURLINE, 0, 0);
	} else {
		len = wEditor.Call(SCI_GETLINE, line, 0);
	}
	// Allocate buffer
	SBuffer text(len);
	// And get the line
	if (line < 0) {
		wEditor.CallString(SCI_GETCURLINE, len, text.ptr());
	} else {
		wEditor.CallString(SCI_GETLINE, line, text.ptr());
	}
	return SString(text);
}

void SciTEBase::GetRange(GUI::ScintillaWindow &win, int start, int end, char *text) {
	Sci_TextRange tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	win.SendPointer(SCI_GETTEXTRANGE, 0, &tr);
}

/**
 * Check if the given line is a preprocessor condition line.
 * @return The kind of preprocessor condition (enum values).
 */
int SciTEBase::IsLinePreprocessorCondition(char *line) {
	char *currChar = line;
	char word[32];

	if (!currChar) {
		return false;
	}
	while (isspacechar(*currChar) && *currChar) {
		currChar++;
	}
	if (preprocessorSymbol && (*currChar == preprocessorSymbol)) {
		currChar++;
		while (isspacechar(*currChar) && *currChar) {
			currChar++;
		}
		size_t i = 0;
		while (!isspacechar(*currChar) && *currChar && (i < (sizeof(word) - 1))) {
			word[i++] = *currChar++;
		}
		word[i] = '\0';
		if (preprocCondStart.InList(word)) {
			return ppcStart;
		}
		if (preprocCondMiddle.InList(word)) {
			return ppcMiddle;
		}
		if (preprocCondEnd.InList(word)) {
			return ppcEnd;
		}
	}
	return noPPC;
}

/**
 * Search a matching preprocessor condition line.
 * @return @c true if the end condition are meet.
 * Also set curLine to the line where one of these conditions is mmet.
 */
bool SciTEBase::FindMatchingPreprocessorCondition(
    int &curLine,   		///< Number of the line where to start the search
    int direction,   		///< Direction of search: 1 = forward, -1 = backward
    int condEnd1,   		///< First status of line for which the search is OK
    int condEnd2) {		///< Second one

	bool isInside = false;
	char line[800];	// No need for full line
	int status, level = 0;
	int maxLines = wEditor.Call(SCI_GETLINECOUNT) - 1;

	while (curLine < maxLines && curLine > 0 && !isInside) {
		curLine += direction;	// Increment or decrement
		GetLine(line, sizeof(line), curLine);
		status = IsLinePreprocessorCondition(line);

		if ((direction == 1 && status == ppcStart) || (direction == -1 && status == ppcEnd)) {
			level++;
		} else if (level > 0 && ((direction == 1 && status == ppcEnd) || (direction == -1 && status == ppcStart))) {
			level--;
		} else if (level == 0 && (status == condEnd1 || status == condEnd2)) {
			isInside = true;
		}
	}

	return isInside;
}

/**
 * Find if there is a preprocessor condition after or before the caret position,
 * @return @c true if inside a preprocessor condition.
 */
bool SciTEBase::FindMatchingPreprocCondPosition(
    bool isForward,   		///< @c true if search forward
    int &mppcAtCaret,   	///< Matching preproc. cond.: current position of caret
    int &mppcMatch) {		///< Matching preproc. cond.: matching position

	bool isInside = false;
	int curLine;
	char line[800];	// Probably no need to get more characters, even if the line is longer, unless very strange layout...
	int status;

	// Get current line
	curLine = wEditor.Call(SCI_LINEFROMPOSITION, mppcAtCaret);
	GetLine(line, sizeof(line), curLine);
	status = IsLinePreprocessorCondition(line);

	switch (status) {
	case ppcStart:
		if (isForward) {
			isInside = FindMatchingPreprocessorCondition(curLine, 1, ppcMiddle, ppcEnd);
		} else {
			mppcMatch = mppcAtCaret;
			return true;
		}
		break;
	case ppcMiddle:
		if (isForward) {
			isInside = FindMatchingPreprocessorCondition(curLine, 1, ppcMiddle, ppcEnd);
		} else {
			isInside = FindMatchingPreprocessorCondition(curLine, -1, ppcStart, ppcMiddle);
		}
		break;
	case ppcEnd:
		if (isForward) {
			mppcMatch = mppcAtCaret;
			return true;
		} else {
			isInside = FindMatchingPreprocessorCondition(curLine, -1, ppcStart, ppcMiddle);
		}
		break;
	default:   	// Should be noPPC

		if (isForward) {
			isInside = FindMatchingPreprocessorCondition(curLine, 1, ppcMiddle, ppcEnd);
		} else {
			isInside = FindMatchingPreprocessorCondition(curLine, -1, ppcStart, ppcMiddle);
		}
		break;
	}

	if (isInside) {
		mppcMatch = wEditor.Call(SCI_POSITIONFROMLINE, curLine);
	}
	return isInside;
}

static bool IsBrace(char ch) {
	return ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == '{' || ch == '}';
}

/**
 * Find if there is a brace next to the caret, checking before caret first, then
 * after caret. If brace found also find its matching brace.
 * @return @c true if inside a bracket pair.
 */
bool SciTEBase::FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy) {
	int maskStyle = (1 << wEditor.Call(SCI_GETSTYLEBITSNEEDED)) - 1;
	bool isInside = false;
	GUI::ScintillaWindow &win = editor ? wEditor : wOutput;

	int mainSel = win.Call(SCI_GETMAINSELECTION, 0, 0);
	if (win.Send(SCI_GETSELECTIONNCARETVIRTUALSPACE, mainSel, 0) > 0)
		return false;

	int bracesStyleCheck = editor ? bracesStyle : 0;
	int caretPos = win.Call(SCI_GETCURRENTPOS, 0, 0);
	braceAtCaret = -1;
	braceOpposite = -1;
	char charBefore = '\0';
	char styleBefore = '\0';
	int lengthDoc = win.Call(SCI_GETLENGTH, 0, 0);
	TextReader acc(win);
	if ((lengthDoc > 0) && (caretPos > 0)) {
		// Check to ensure not matching brace that is part of a multibyte character
		if (win.Send(SCI_POSITIONBEFORE, caretPos) == (caretPos - 1)) {
			charBefore = acc[caretPos - 1];
			styleBefore = static_cast<char>(acc.StyleAt(caretPos - 1) & maskStyle);
		}
	}
	// Priority goes to character before caret
	if (charBefore && IsBrace(charBefore) &&
	        ((styleBefore == bracesStyleCheck) || (!bracesStyle))) {
		braceAtCaret = caretPos - 1;
	}
	bool colonMode = false;
	if ((lexLanguage == SCLEX_PYTHON) &&
	        (':' == charBefore) && (SCE_P_OPERATOR == styleBefore)) {
		braceAtCaret = caretPos - 1;
		colonMode = true;
	}
	bool isAfter = true;
	if (lengthDoc > 0 && sloppy && (braceAtCaret < 0) && (caretPos < lengthDoc)) {
		// No brace found so check other side
		// Check to ensure not matching brace that is part of a multibyte character
		if (win.Send(SCI_POSITIONAFTER, caretPos) == (caretPos + 1)) {
			char charAfter = acc[caretPos];
			char styleAfter = static_cast<char>(acc.StyleAt(caretPos) & maskStyle);
			if (charAfter && IsBrace(charAfter) && ((styleAfter == bracesStyleCheck) || (!bracesStyle))) {
				braceAtCaret = caretPos;
				isAfter = false;
			}
			if ((lexLanguage == SCLEX_PYTHON) &&
			        (':' == charAfter) && (SCE_P_OPERATOR == styleAfter)) {
				braceAtCaret = caretPos;
				colonMode = true;
			}
		}
	}
	if (braceAtCaret >= 0) {
		if (colonMode) {
			int lineStart = win.Call(SCI_LINEFROMPOSITION, braceAtCaret);
			int lineMaxSubord = win.Call(SCI_GETLASTCHILD, lineStart, -1);
			braceOpposite = win.Call(SCI_GETLINEENDPOSITION, lineMaxSubord);
		} else {
			braceOpposite = win.Call(SCI_BRACEMATCH, braceAtCaret, 0);
		}
		if (braceOpposite > braceAtCaret) {
			isInside = isAfter;
		} else {
			isInside = !isAfter;
		}
	}
	return isInside;
}

void SciTEBase::BraceMatch(bool editor) {
	if (!bracesCheck)
		return;
	int braceAtCaret = -1;
	int braceOpposite = -1;
	FindMatchingBracePosition(editor, braceAtCaret, braceOpposite, bracesSloppy);
	GUI::ScintillaWindow &win = editor ? wEditor : wOutput;
	if ((braceAtCaret != -1) && (braceOpposite == -1)) {
		win.Send(SCI_BRACEBADLIGHT, braceAtCaret, 0);
		wEditor.Call(SCI_SETHIGHLIGHTGUIDE, 0);
	} else {
		char chBrace = 0;
		if (braceAtCaret >= 0)
			chBrace = static_cast<char>(win.Send(
			            SCI_GETCHARAT, braceAtCaret, 0));
		win.Send(SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
		int columnAtCaret = win.Call(SCI_GETCOLUMN, braceAtCaret, 0);
		int columnOpposite = win.Call(SCI_GETCOLUMN, braceOpposite, 0);
		if (chBrace == ':') {
			int lineStart = win.Call(SCI_LINEFROMPOSITION, braceAtCaret);
			int indentPos = win.Call(SCI_GETLINEINDENTPOSITION, lineStart, 0);
			int indentPosNext = win.Call(SCI_GETLINEINDENTPOSITION, lineStart + 1, 0);
			columnAtCaret = win.Call(SCI_GETCOLUMN, indentPos, 0);
			int columnAtCaretNext = win.Call(SCI_GETCOLUMN, indentPosNext, 0);
			int indentSize = win.Call(SCI_GETINDENT);
			if (columnAtCaretNext - indentSize > 1)
				columnAtCaret = columnAtCaretNext - indentSize;
			if (columnOpposite == 0)	// If the final line of the structure is empty
				columnOpposite = columnAtCaret;
		} else {
			if (win.Send(SCI_LINEFROMPOSITION, braceAtCaret) == win.Send(SCI_LINEFROMPOSITION, braceOpposite)) {
				// Avoid attempting to draw a highlight guide
				columnAtCaret = 0;
				columnOpposite = 0;
			}
		}

		if (props.GetInt("highlight.indentation.guides"))
			win.Send(SCI_SETHIGHLIGHTGUIDE, Minimum(columnAtCaret, columnOpposite), 0);
	}
}

void SciTEBase::SetWindowName() {
	if (filePath.IsUntitled()) {
		windowName = localiser.Text("Untitled");
		windowName.insert(0, GUI_TEXT("("));
		windowName += GUI_TEXT(")");
	} else if (props.GetInt("title.full.path") == 2) {
		windowName = FileNameExt().AsInternal();
		windowName += GUI_TEXT(" ");
		windowName += localiser.Text("in");
		windowName += GUI_TEXT(" ");
		windowName += filePath.Directory().AsInternal();
	} else if (props.GetInt("title.full.path") == 1) {
		windowName = filePath.AsInternal();
	} else {
		windowName = FileNameExt().AsInternal();
	}
	if (CurrentBuffer()->isDirty)
		windowName += GUI_TEXT(" * ");
	else
		windowName += GUI_TEXT(" - ");
	windowName += appName;

	if (buffers.length > 1 && props.GetInt("title.show.buffers")) {
		windowName += GUI_TEXT(" [");
		windowName += GUI::StringFromInteger(buffers.Current() + 1);
		windowName += GUI_TEXT(" ");
		windowName += localiser.Text("of");
		windowName += GUI_TEXT(" ");
		windowName += GUI::StringFromInteger(buffers.length);
		windowName += GUI_TEXT("]");
	}

	wSciTE.SetTitle(windowName.c_str());
}

Sci_CharacterRange SciTEBase::GetSelection() {
	Sci_CharacterRange crange;
	crange.cpMin = wEditor.Call(SCI_GETSELECTIONSTART);
	crange.cpMax = wEditor.Call(SCI_GETSELECTIONEND);
	return crange;
}

SelectedRange SciTEBase::GetSelectedRange() {
	return SelectedRange(wEditor.Call(SCI_GETCURRENTPOS), wEditor.Call(SCI_GETANCHOR));
}

void SciTEBase::SetSelection(int anchor, int currentPos) {
	wEditor.Call(SCI_SETSEL, anchor, currentPos);
}

SString SciTEBase::GetCTag() {
	int lengthDoc, selStart, selEnd;
	int mustStop = 0;
	char c;
	GUI::ScintillaWindow &wCurrent = wOutput.HasFocus() ? wOutput : wEditor;

	lengthDoc = wCurrent.Call(SCI_GETLENGTH);
	selStart = selEnd = wCurrent.Call(SCI_GETSELECTIONEND);
	TextReader acc(wCurrent);
	while (!mustStop) {
		if (selStart < lengthDoc - 1) {
			selStart++;
			c = acc[selStart];
			if (c == '\r' || c == '\n') {
				mustStop = -1;
			} else if (c == '\t' && ((acc[selStart + 1] == '/' && acc[selStart + 2] == '^') || isdigit(acc[selStart + 1]))) {
				mustStop = 1;
			}
		} else {
			mustStop = -1;
		}
	}
	if (mustStop == 1 && (acc[selStart + 1] == '/' && acc[selStart + 2] == '^')) {	// Found
		selEnd = selStart += 3;
		mustStop = 0;
		while (!mustStop) {
			if (selEnd < lengthDoc - 1) {
				selEnd++;
				c = acc[selEnd];
				if (c == '\r' || c == '\n') {
					mustStop = -1;
				} else if (c == '$' && acc[selEnd + 1] == '/') {
					mustStop = 1;	// Found!
				}

			} else {
				mustStop = -1;
			}
		}
	} else if (mustStop == 1 && isdigit(acc[selStart + 1])) {
		// a Tag can be referenced by line Number also
		selEnd = selStart += 1;
		while (isdigit(acc[selEnd]) && (selEnd < lengthDoc)) {
			selEnd++;
		}
	}

	if (selStart < selEnd) {
		return GetRange(wCurrent, selStart, selEnd);
	} else {
		return SString();
	}
}

// Default characters that can appear in a word
bool SciTEBase::iswordcharforsel(char ch) {
	return !strchr("\t\n\r !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", ch);
}

// Accept slightly more characters than for a word
// Doesn't accept all valid characters, as they are rarely used in source filenames...
// Accept path separators '/' and '\', extension separator '.', and ':', MS drive unit
// separator, and also used for separating the line number for grep. Same for '(' and ')' for cl.
// Accept '?' and '%' which are used in URL.
bool SciTEBase::isfilenamecharforsel(char ch) {
	return !strchr("\t\n\r \"$'*,;<>[]^`{|}", ch);
}

bool SciTEBase::islexerwordcharforsel(char ch) {
	// If there are no word.characters defined for the current file, fall back on the original function
	if (wordCharacters.length())
		return wordCharacters.contains(ch);
	else
		return iswordcharforsel(ch);
}

void SciTEBase::HighlightCurrentWord(bool highlight) {
	if (!currentWordHighlight.isEnabled)
		return;
	GUI::ScintillaWindow &wCurrent = wOutput.HasFocus() ? wOutput : wEditor;
	// Remove old indicators if any exist.
	wCurrent.Call(SCI_SETINDICATORCURRENT, indicatorHightlightCurrentWord);
	int lenDoc = wCurrent.Call(SCI_GETLENGTH);
	wCurrent.Call(SCI_INDICATORCLEARRANGE, 0, lenDoc);
	if (!highlight)
		return;
	// Get start & end selection.
	int selStart = wCurrent.Call(SCI_GETSELECTIONSTART);
	int selEnd = wCurrent.Call(SCI_GETSELECTIONEND);
	bool noUserSelection = selStart == selEnd;
	SString wordToFind = RangeExtendAndGrab(wCurrent, selStart, selEnd,
	        &SciTEBase::islexerwordcharforsel);
	if (wordToFind.length() == 0 || wordToFind.contains('\n') || wordToFind.contains('\r'))
		return; // No highlight when no selection or multi-lines selection.
	if (noUserSelection && currentWordHighlight.statesOfDelay == currentWordHighlight.noDelay) {
		// Manage delay before highlight when no user selection but there is word at the caret.
		currentWordHighlight.statesOfDelay = currentWordHighlight.delay;
		// Reset timer
		currentWordHighlight.elapsedTimes.Duration(true);
		return;
	}
	// Get style of the current word to highlight only word with same style.
	int selectedStyle = wCurrent.Call(SCI_GETSTYLEAT, selStart);

	// Manage word with DBCS.
	wordToFind = EncodeString(wordToFind);

	// Case sensitive & whole word only.
	wCurrent.Call(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE | SCFIND_WHOLEWORD);
	wCurrent.Call(SCI_SETTARGETSTART, 0);
	wCurrent.Call(SCI_SETTARGETEND, lenDoc);
	// Find the first occurrence of word.
	int indexOf = wCurrent.CallString(SCI_SEARCHINTARGET,
	        wordToFind.length(), wordToFind.c_str());
	while (indexOf != -1 && indexOf < lenDoc) {
		if (!currentWordHighlight.isOnlyWithSameStyle || selectedStyle ==
		        wCurrent.Call(SCI_GETSTYLEAT, indexOf)) {
			wCurrent.Call(SCI_INDICATORFILLRANGE, indexOf, wordToFind.length());
		}
		// Try to find next occurrence of word.
		wCurrent.Call(SCI_SETTARGETSTART, indexOf + wordToFind.length() + 1);
		wCurrent.Call(SCI_SETTARGETEND, lenDoc);
		indexOf = wCurrent.CallString(SCI_SEARCHINTARGET, wordToFind.length(),
		        wordToFind.c_str());
	}
}

SString SciTEBase::GetRange(GUI::ScintillaWindow &win, int selStart, int selEnd) {
	SBuffer sel(selEnd - selStart);
	Sci_TextRange tr;
	tr.chrg.cpMin = selStart;
	tr.chrg.cpMax = selEnd;
	tr.lpstrText = sel.ptr();
	win.SendPointer(SCI_GETTEXTRANGE, 0, &tr);
	return SString(sel);
}

SString SciTEBase::GetRangeInUIEncoding(GUI::ScintillaWindow &win, int selStart, int selEnd) {
	return GetRange(win, selStart, selEnd);
}

SString SciTEBase::GetLine(GUI::ScintillaWindow &win, int line) {
	int lineStart = win.Call(SCI_POSITIONFROMLINE, line);
	int lineEnd = win.Call(SCI_GETLINEENDPOSITION, line);
	if ((lineStart < 0) || (lineEnd < 0))
		return SString();
	return GetRange(win, lineStart, lineEnd);
}

SString SciTEBase::RangeExtendAndGrab(
    GUI::ScintillaWindow &wCurrent,
    int &selStart,
    int &selEnd,
    bool (SciTEBase::*ischarforsel)(char ch),	///< Function returning @c true if the given char. is part of the selection.
    bool stripEol /*=true*/) {

	if (selStart == selEnd && ischarforsel) {
		// Empty range and have a function to extend it
		int lengthDoc = wCurrent.Call(SCI_GETLENGTH);
		TextReader acc(wCurrent);
		// Try and find a word at the caret
		// On the left...
		while ((selStart > 0) && ((this->*ischarforsel)(acc[selStart - 1]))) {
			selStart--;
		}
		// and on the right
		while ((selEnd < lengthDoc) && ((this->*ischarforsel)(acc[selEnd]))) {
			selEnd++;
		}
	}
	SString selected;
	if (selStart != selEnd) {
		selected = GetRangeInUIEncoding(wCurrent, selStart, selEnd);
	}
	if (stripEol) {
		// Change whole line selected but normally end of line characters not wanted.
		// Remove possible terminating \r, \n, or \r\n.
		size_t sellen = selected.length();
		if (sellen >= 2 && (selected[sellen - 2] == '\r' && selected[sellen - 1] == '\n')) {
			selected.remove(sellen - 2, 0);
		} else if (sellen >= 1 && (selected[sellen - 1] == '\r' || selected[sellen - 1] == '\n')) {
			selected.remove(sellen - 1, 0);
		}
	}

	return selected;
}

/**
 * If there is selected text, either in the editor or the output pane,
 * put the selection in the @a sel buffer, up to @a len characters.
 * Otherwise, try and select characters around the caret, as long as they are OK
 * for the @a ischarforsel function.
 * Remove the last two character controls from the result, as they are likely
 * to be CR and/or LF.
 */
SString SciTEBase::SelectionExtend(
    bool (SciTEBase::*ischarforsel)(char ch),	///< Function returning @c true if the given char. is part of the selection.
    bool stripEol /*=true*/) {

	GUI::ScintillaWindow &wCurrent = wOutput.HasFocus() ? wOutput : wEditor;

	int selStart = wCurrent.Call(SCI_GETSELECTIONSTART);
	int selEnd = wCurrent.Call(SCI_GETSELECTIONEND);
	return RangeExtendAndGrab(wCurrent, selStart, selEnd, ischarforsel, stripEol);
}

void SciTEBase::FindWordAtCaret(int &start, int &end) {

	GUI::ScintillaWindow &wCurrent = wOutput.HasFocus() ? wOutput : wEditor;

	start = wCurrent.Call(SCI_GETSELECTIONSTART);
	end = wCurrent.Call(SCI_GETSELECTIONEND);
	// Call just to update start & end
	RangeExtendAndGrab(wCurrent, start, end, &SciTEBase::iswordcharforsel, false);
}

bool SciTEBase::SelectWordAtCaret() {
	int selStart = 0;
	int selEnd = 0;
	FindWordAtCaret(selStart, selEnd);
	SetSelection(selStart, selEnd);
	return selStart != selEnd;
}

SString SciTEBase::SelectionWord(bool stripEol /*=true*/) {
	return SelectionExtend(&SciTEBase::islexerwordcharforsel, stripEol);
}

SString SciTEBase::SelectionFilename() {
	return SelectionExtend(&SciTEBase::isfilenamecharforsel);
}

void SciTEBase::SelectionIntoProperties() {
	SString currentSelection = SelectionExtend(0, false);
	props.Set("CurrentSelection", currentSelection.c_str());

	SString word = SelectionWord();
	props.Set("CurrentWord", word.c_str());

	int selStart = CallFocused(SCI_GETSELECTIONSTART);
	int selEnd = CallFocused(SCI_GETSELECTIONEND);
	props.SetInteger("SelectionStartLine", CallFocused(SCI_LINEFROMPOSITION, selStart) + 1);
	props.SetInteger("SelectionStartColumn", CallFocused(SCI_GETCOLUMN, selStart) + 1);
	props.SetInteger("SelectionEndLine", CallFocused(SCI_LINEFROMPOSITION, selEnd) + 1);
	props.SetInteger("SelectionEndColumn", CallFocused(SCI_GETCOLUMN, selEnd) + 1);
}

void SciTEBase::SelectionIntoFind(bool stripEol /*=true*/) {
	SString sel = SelectionWord(stripEol);
	if (sel.length() && !sel.contains('\r') && !sel.contains('\n')) {
		// The selection does not include a new line, so is likely to be
		// the expression to search...
		findWhat = sel;
		if (unSlash) {
			char *slashedFind = Slash(findWhat.c_str(), false);
			if (slashedFind) {
				findWhat = slashedFind;
				delete []slashedFind;
			}
		}
	}
	// else findWhat remains the same as last time.
}

SString SciTEBase::EncodeString(const SString &s) {
	return SString(s);
}

static int UnSlashAsNeeded(SString &s, bool escapes, bool regularExpression) {
	if (escapes) {
		char *sUnslashed = StringDup(s.c_str(), s.length());
		size_t len;
		if (regularExpression) {
			// For regular expressions, the only escape sequences allowed start with \0
			// Other sequences, like \t, are handled by the RE engine.
			len = UnSlashLowOctal(sUnslashed);
		} else {
			// C style escapes allowed
			len = UnSlash(sUnslashed);
		}
		s = sUnslashed;
		delete []sUnslashed;
		return static_cast<int>(len);
	} else {
		return static_cast<int>(s.length());
	}
}

void SciTEBase::RemoveFindMarks() {
	if (CurrentBuffer()->findMarks != Buffer::fmNone) {
		wEditor.Call(SCI_SETINDICATORCURRENT, indicatorMatch);
		wEditor.Call(SCI_INDICATORCLEARRANGE, 0, LengthDocument());
		CurrentBuffer()->findMarks = Buffer::fmNone;
	}
	wEditor.Call(SCI_ANNOTATIONCLEARALL);
}

int SciTEBase::MarkAll() {
	int posCurrent = wEditor.Call(SCI_GETCURRENTPOS);
	int marked = 0;
	int posFirstFound = FindNext(false, false);

	SString findMark = props.Get("find.mark");
	if (findMark.length()) {
		wEditor.Call(SCI_SETINDICATORCURRENT, indicatorMatch);
		RemoveFindMarks();
		CurrentBuffer()->findMarks = Buffer::fmMarked;
	}
	if (posFirstFound != -1) {
		int posEndFound;
		int posFound = posFirstFound;
		do {
			marked++;
			int line = wEditor.Call(SCI_LINEFROMPOSITION, posFound);
			BookmarkAdd(line);
			if (findMark.length()) {
				wEditor.Call(SCI_INDICATORFILLRANGE, posFound, wEditor.Call(SCI_GETTARGETEND) - posFound);
			}
			posFound = FindNext(false, false);
			posEndFound = wEditor.Call(SCI_GETTARGETEND);
			// Since start position may be within a match, terminate when match includes initial position
		} while ((posFound != -1) && !((posFound <= posFirstFound) && (posFirstFound <= posEndFound)));
	}
	wEditor.Call(SCI_SETCURRENTPOS, posCurrent);
	return marked;
}

int SciTEBase::IncrementSearchMode() {
	FindIncrement();
	return 0;
}

int SciTEBase::FindInTarget(const char *findWhatText, int lenFind, int startPosition, int endPosition) {
	wEditor.Call(SCI_SETTARGETSTART, startPosition);
	wEditor.Call(SCI_SETTARGETEND, endPosition);
	int posFind = wEditor.CallString(SCI_SEARCHINTARGET, lenFind, findWhatText);
	while (findInStyle && posFind != -1 && findStyle != wEditor.Call(SCI_GETSTYLEAT, posFind)) {
		if (startPosition < endPosition) {
			wEditor.Call(SCI_SETTARGETSTART, posFind + 1);
			wEditor.Call(SCI_SETTARGETEND, endPosition);
		} else {
			wEditor.Call(SCI_SETTARGETSTART, startPosition);
			wEditor.Call(SCI_SETTARGETEND, posFind + 1);
		}
		posFind = wEditor.CallString(SCI_SEARCHINTARGET, lenFind, findWhatText);
	}
	return posFind;
}

void SciTEBase::SetFind(const char *sFind) {
	findWhat = sFind;
	memFinds.Insert(findWhat.c_str());
	props.Set("find.what", findWhat.c_str());
}

bool SciTEBase::FindHasText() const {
	return findWhat[0];
}

void SciTEBase::SetReplace(const char *sReplace) {
	replaceWhat = sReplace;
	memReplaces.Insert(replaceWhat.c_str());
}

void SciTEBase::MoveBack(int distance) {
	Sci_CharacterRange cr = GetSelection();
	int caret = static_cast<int>(cr.cpMin) - distance;
	SetSelection(caret, caret);
}

void SciTEBase::ScrollEditorIfNeeded() {
	GUI::Point ptCaret;
	int caret = wEditor.Call(SCI_GETCURRENTPOS);
	ptCaret.x = wEditor.Call(SCI_POINTXFROMPOSITION, 0, caret);
	ptCaret.y = wEditor.Call(SCI_POINTYFROMPOSITION, 0, caret);
	ptCaret.y += wEditor.Call(SCI_TEXTHEIGHT, 0, 0) - 1;

	GUI::Rectangle rcEditor = wEditor.GetClientPosition();
	if (!rcEditor.Contains(ptCaret))
		wEditor.Call(SCI_SCROLLCARET);
}

int SciTEBase::FindNext(bool reverseDirection, bool showWarnings, bool allowRegExp) {
	if (findWhat.length() == 0) {
		Find();
		return -1;
	}
	SString findTarget = EncodeString(findWhat);
	int lenFind = UnSlashAsNeeded(findTarget, unSlash, regExp);
	if (lenFind == 0)
		return -1;

	Sci_CharacterRange cr = GetSelection();
	int startPosition = static_cast<int>(cr.cpMax);
	int endPosition = LengthDocument();
	if (reverseDirection) {
		startPosition = static_cast<int>(cr.cpMin);
		endPosition = 0;
	}

	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) |
	        (matchCase ? SCFIND_MATCHCASE : 0) |
	        ((allowRegExp && regExp) ? SCFIND_REGEXP : 0) |
	        (props.GetInt("find.replace.regexp.posix") ? SCFIND_POSIX : 0);

	wEditor.Call(SCI_SETSEARCHFLAGS, flags);
	int posFind = FindInTarget(findTarget.c_str(), lenFind, startPosition, endPosition);
	if (posFind == -1 && wrapFind) {
		// Failed to find in indicated direction
		// so search from the beginning (forward) or from the end (reverse)
		// unless wrapFind is false
		if (reverseDirection) {
			startPosition = LengthDocument();
			endPosition = 0;
		} else {
			startPosition = 0;
			endPosition = LengthDocument();
		}
		posFind = FindInTarget(findTarget.c_str(), lenFind, startPosition, endPosition);
		WarnUser(warnFindWrapped);
	}
	if (posFind == -1) {
		havefound = false;
		if (showWarnings) {
			WarnUser(warnNotFound);
			FindMessageBox("Can not find the string '^0'.",
			        &findWhat);
		}
	} else {
		havefound = true;
		int start = wEditor.Call(SCI_GETTARGETSTART);
		int end = wEditor.Call(SCI_GETTARGETEND);
		EnsureRangeVisible(wEditor, start, end);
		SetSelection(start, end);
		if (!replacing && closeFind) {
			DestroyFindReplace();
		}
	}
	return posFind;
}

void SciTEBase::HideMatch() {
}

void SciTEBase::ReplaceOnce() {
	if (!FindHasText())
		return;

	if (!havefound) {
		Sci_CharacterRange crange = GetSelection();
		SetSelection(static_cast<int>(crange.cpMin), static_cast<int>(crange.cpMin));
		FindNext(false);
	}

	if (havefound) {
		SString replaceTarget = EncodeString(replaceWhat);
		int replaceLen = UnSlashAsNeeded(replaceTarget, unSlash, regExp);
		Sci_CharacterRange cr = GetSelection();
		wEditor.Call(SCI_SETTARGETSTART, cr.cpMin);
		wEditor.Call(SCI_SETTARGETEND, cr.cpMax);
		int lenReplaced = replaceLen;
		if (regExp)
			lenReplaced = wEditor.CallString(SCI_REPLACETARGETRE, replaceLen, replaceTarget.c_str());
		else	// Allow \0 in replacement
			wEditor.CallString(SCI_REPLACETARGET, replaceLen, replaceTarget.c_str());
		SetSelection(static_cast<int>(cr.cpMin) + lenReplaced, static_cast<int>(cr.cpMin));
		havefound = false;
	}

	FindNext(false);
}

int SciTEBase::DoReplaceAll(bool inSelection) {
	SString findTarget = EncodeString(findWhat);
	int findLen = UnSlashAsNeeded(findTarget, unSlash, regExp);
	if (findLen == 0) {
		return -1;
	}

	Sci_CharacterRange cr = GetSelection();
	int startPosition = static_cast<int>(cr.cpMin);
	int endPosition = static_cast<int>(cr.cpMax);
	int countSelections = wEditor.Call(SCI_GETSELECTIONS);
	if (inSelection) {
		int selType = wEditor.Call(SCI_GETSELECTIONMODE);
		if (selType == SC_SEL_LINES) {
			// Take care to replace in whole lines
			int startLine = wEditor.Call(SCI_LINEFROMPOSITION, startPosition);
			startPosition = wEditor.Call(SCI_POSITIONFROMLINE, startLine);
			int endLine = wEditor.Call(SCI_LINEFROMPOSITION, endPosition);
			endPosition = wEditor.Call(SCI_POSITIONFROMLINE, endLine + 1);
		} else {
			for (int i=0; i<countSelections; i++) {
				startPosition = Minimum(startPosition, wEditor.Call(SCI_GETSELECTIONNSTART, i));
				endPosition = Maximum(endPosition, wEditor.Call(SCI_GETSELECTIONNEND, i));
			}
		}
		if (startPosition == endPosition) {
			return -2;
		}
	} else {
		endPosition = LengthDocument();
		if (wrapFind) {
			// Whole document
			startPosition = 0;
		}
		// If not wrapFind, replace all only from caret to end of document
	}

	SString replaceTarget = EncodeString(replaceWhat);
	int replaceLen = UnSlashAsNeeded(replaceTarget, unSlash, regExp);
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) |
	        (matchCase ? SCFIND_MATCHCASE : 0) |
	        (regExp ? SCFIND_REGEXP : 0) |
	        (props.GetInt("find.replace.regexp.posix") ? SCFIND_POSIX : 0);
	wEditor.Call(SCI_SETSEARCHFLAGS, flags);
	int posFind = FindInTarget(findTarget.c_str(), findLen, startPosition, endPosition);
	if ((findLen == 1) && regExp && (findTarget[0] == '^')) {
		// Special case for replace all start of line so it hits the first line
		posFind = startPosition;
		wEditor.Call(SCI_SETTARGETSTART, startPosition);
		wEditor.Call(SCI_SETTARGETEND, startPosition);
	}
	if ((posFind != -1) && (posFind <= endPosition)) {
		int lastMatch = posFind;
		int replacements = 0;
		wEditor.Call(SCI_BEGINUNDOACTION);
		// Replacement loop
		while (posFind != -1) {
			int lenTarget = wEditor.Call(SCI_GETTARGETEND) - wEditor.Call(SCI_GETTARGETSTART);
			if (inSelection && countSelections > 1) {
				// We must check that the found target is entirely inside a selection
				bool insideASelection = false;
				for (int i=0; i<countSelections && !insideASelection; i++) {
					int startPos= wEditor.Call(SCI_GETSELECTIONNSTART, i);
					int endPos = wEditor.Call(SCI_GETSELECTIONNEND, i);
					if (posFind >= startPos && posFind + lenTarget <= endPos)
						insideASelection = true;
				}
				if (!insideASelection) {
					// Found target is totally or partly outside the selections
					lastMatch = posFind + 1;
					if (lastMatch >= endPosition) {
						// Run off the end of the document/selection with an empty match
						posFind = -1;
					} else {
						posFind = FindInTarget(findTarget.c_str(), findLen, lastMatch, endPosition);
					}
					continue;	// No replacement
				}
			}
			int movepastEOL = 0;
			if (lenTarget <= 0) {
				char chNext = static_cast<char>(wEditor.Call(SCI_GETCHARAT, wEditor.Call(SCI_GETTARGETEND)));
				if (chNext == '\r' || chNext == '\n') {
					movepastEOL = 1;
				}
			}
			int lenReplaced = replaceLen;
			if (regExp) {
				lenReplaced = wEditor.CallString(SCI_REPLACETARGETRE, replaceLen, replaceTarget.c_str());
			} else {
				wEditor.CallString(SCI_REPLACETARGET, replaceLen, replaceTarget.c_str());
			}
			// Modify for change caused by replacement
			endPosition += lenReplaced - lenTarget;
			// For the special cases of start of line and end of line
			// something better could be done but there are too many special cases
			lastMatch = posFind + lenReplaced + movepastEOL;
			if (lenTarget == 0) {
				lastMatch = wEditor.Call(SCI_POSITIONAFTER, lastMatch);
			}
			if (lastMatch >= endPosition) {
				// Run off the end of the document/selection with an empty match
				posFind = -1;
			} else {
				posFind = FindInTarget(findTarget.c_str(), findLen, lastMatch, endPosition);
			}
			replacements++;
		}
		if (inSelection) {
			if (countSelections == 1)
				SetSelection(startPosition, endPosition);
		} else {
			SetSelection(lastMatch, lastMatch);
		}
		wEditor.Call(SCI_ENDUNDOACTION);
		return replacements;
	}
	return 0;
}

int SciTEBase::ReplaceAll(bool inSelection) {
	int replacements = DoReplaceAll(inSelection);
	props.SetInteger("Replacements", (replacements > 0 ? replacements : 0));
	UpdateStatusBar(false);
	if (replacements == -1) {
		FindMessageBox(
		    inSelection ?
		    "Find string must not be empty for 'Replace in Selection' command." :
		    "Find string must not be empty for 'Replace All' command.");
	} else if (replacements == -2) {
		FindMessageBox(
		    "Selection must not be empty for 'Replace in Selection' command.");
	} else if (replacements == 0) {
		FindMessageBox(
		    "No replacements because string '^0' was not present.", &findWhat);
	}
	return replacements;
}

int SciTEBase::ReplaceInBuffers() {
	int currentBuffer = buffers.Current();
	int replacements = 0;
	for (int i = 0; i < buffers.length; i++) {
		SetDocumentAt(i);
		replacements += DoReplaceAll(false);
		if (i == 0 && replacements < 0) {
			FindMessageBox(
			    "Find string must not be empty for 'Replace in Buffers' command.");
			break;
		}
	}
	SetDocumentAt(currentBuffer);
	props.SetInteger("Replacements", replacements);
	UpdateStatusBar(false);
	if (replacements == 0) {
		FindMessageBox(
		    "No replacements because string '^0' was not present.", &findWhat);
	}
	return replacements;
}

void SciTEBase::UIClosed() {
}

void SciTEBase::UIHasFocus() {
}

void SciTEBase::OutputAppendString(const char *s, int len) {
	if (len == -1)
		len = static_cast<int>(strlen(s));
	wOutput.Call(SCI_APPENDTEXT, len, reinterpret_cast<sptr_t>(s));
	if (scrollOutput) {
		int line = wOutput.Call(SCI_GETLINECOUNT, 0, 0);
		int lineStart = wOutput.Call(SCI_POSITIONFROMLINE, line);
		wOutput.Call(SCI_GOTOPOS, lineStart);
	}
}

void SciTEBase::OutputAppendStringSynchronised(const char *s, int len) {
	if (len == -1)
		len = static_cast<int>(strlen(s));
	wOutput.Send(SCI_APPENDTEXT, len, reinterpret_cast<sptr_t>(s));
	if (scrollOutput) {
		sptr_t line = wOutput.Send(SCI_GETLINECOUNT);
		sptr_t lineStart = wOutput.Send(SCI_POSITIONFROMLINE, line);
		wOutput.Send(SCI_GOTOPOS, lineStart);
	}
}

void SciTEBase::MakeOutputVisible() {
	if (heightOutput <= 0) {
		ToggleOutputVisible();
	}
}

void SciTEBase::Execute() {
	props.Set("CurrentMessage", "");
	dirNameForExecute = FilePath();
	bool displayParameterDialog = false;
	int ic;
	parameterisedCommand = "";
	for (ic = 0; ic < jobQueue.commandMax; ic++) {
		if (jobQueue.jobQueue[ic].command[0] == '*') {
			displayParameterDialog = true;
			jobQueue.jobQueue[ic].command.remove(0, 1);
			parameterisedCommand = jobQueue.jobQueue[ic].command;
		}
		if (jobQueue.jobQueue[ic].directory.IsSet()) {
			dirNameForExecute = jobQueue.jobQueue[ic].directory;
		}
	}
	if (displayParameterDialog) {
		if (!ParametersDialog(true)) {
			jobQueue.ClearJobs();
			return;
		}
	} else {
		ParamGrab();
	}
	for (ic = 0; ic < jobQueue.commandMax; ic++) {
		jobQueue.jobQueue[ic].command = props.Expand(jobQueue.jobQueue[ic].command.c_str());
	}

	if (jobQueue.ClearBeforeExecute()) {
		wOutput.Send(SCI_CLEARALL);
	}

	wOutput.Call(SCI_MARKERDELETEALL, static_cast<uptr_t>(-1));
	wEditor.Call(SCI_MARKERDELETEALL, 0);
	// Ensure the output pane is visible
	if (jobQueue.ShowOutputPane()) {
		MakeOutputVisible();
	}

	jobQueue.cancelFlag = 0L;
	if (jobQueue.HasCommandToRun()) {
		jobQueue.SetExecuting(true);
	}
	CheckMenus();
	dirNameAtExecute = filePath.Directory();
}

void SciTEBase::ToggleOutputVisible() {
	if (heightOutput > 0) {
		heightOutput = NormaliseSplit(0);
		WindowSetFocus(wEditor);
	} else {
		if (previousHeightOutput < 20) {
			if (splitVertical)
				heightOutput = NormaliseSplit(300);
			else
				heightOutput = NormaliseSplit(100);
			previousHeightOutput = heightOutput;
		} else {
			heightOutput = NormaliseSplit(previousHeightOutput);
		}
	}
	SizeSubWindows();
	Redraw();
}

void SciTEBase::BookmarkAdd(int lineno) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	if (!BookmarkPresent(lineno))
		wEditor.Call(SCI_MARKERADD, lineno, markerBookmark);
}

void SciTEBase::BookmarkDelete(int lineno) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	if (BookmarkPresent(lineno))
		wEditor.Call(SCI_MARKERDELETE, lineno, markerBookmark);
}

bool SciTEBase::BookmarkPresent(int lineno) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	int state = wEditor.Call(SCI_MARKERGET, lineno);
	return state & (1 << markerBookmark);
}

void SciTEBase::BookmarkToggle(int lineno) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	if (BookmarkPresent(lineno)) {
		BookmarkDelete(lineno);
	} else {
		BookmarkAdd(lineno);
	}
}

void SciTEBase::BookmarkNext(bool forwardScan, bool select) {
	int lineno = GetCurrentLineNumber();
	int sci_marker = SCI_MARKERNEXT;
	int lineStart = lineno + 1;	//Scan starting from next line
	int lineRetry = 0;				//If not found, try from the beginning
	int anchor = wEditor.Call(SCI_GETANCHOR);
	if (!forwardScan) {
		lineStart = lineno - 1;		//Scan starting from previous line
		lineRetry = wEditor.Call(SCI_GETLINECOUNT, 0, 0L);	//If not found, try from the end
		sci_marker = SCI_MARKERPREVIOUS;
	}
	int nextLine = wEditor.Call(sci_marker, lineStart, 1 << markerBookmark);
	if (nextLine < 0)
		nextLine = wEditor.Call(sci_marker, lineRetry, 1 << markerBookmark);
	if (nextLine < 0 || nextLine == lineno)	// No bookmark (of the given type) or only one, and already on it
		WarnUser(warnNoOtherBookmark);
	else {
		GotoLineEnsureVisible(nextLine);
		if (select) {
			wEditor.Call(SCI_SETANCHOR, anchor);
		}
	}
}

GUI::Rectangle SciTEBase::GetClientRectangle() {
	return wContent.GetClientPosition();
}

void SciTEBase::Redraw() {
	wSciTE.InvalidateAll();
	wEditor.InvalidateAll();
	wOutput.InvalidateAll();
}

char *SciTEBase::GetNearestWords(const char *wordStart, size_t searchLen,
		const char *separators, bool ignoreCase /*=false*/, bool exactLen /*=false*/) {
	char *words = 0;
	while (!words && *separators) {
		words = apis.GetNearestWords(wordStart, searchLen, ignoreCase, *separators, exactLen);
		separators++;
	}
	return words;
}

void SciTEBase::FillFunctionDefinition(int pos /*= -1*/) {
	if (pos > 0) {
		lastPosCallTip = pos;
	}
	if (apis) {
		char *words = GetNearestWords(currentCallTipWord.c_str(), currentCallTipWord.length(),
			calltipParametersStart.c_str(), callTipIgnoreCase, true);
		if (!words)
			return;
		// Counts how many call tips
		const char *spacePos = strchr(words, ' ');
		maxCallTips = 1;
		while (spacePos) {
			maxCallTips++;
			spacePos = strchr(spacePos + 1, ' ');
		}
		delete []words;

		// Should get current api definition
		const char *word = apis.GetNearestWord(currentCallTipWord.c_str(), currentCallTipWord.length(),
		        callTipIgnoreCase, calltipWordCharacters, currentCallTip);
		if (word) {
			functionDefinition = word;
			if (maxCallTips > 1) {
				functionDefinition.insert(0, "\001");
			}

			if (calltipEndDefinition != "") {
				int posEndDef = functionDefinition.search(calltipEndDefinition.c_str());
				if (maxCallTips > 1) {
					if ((posEndDef > 1) &&
					        ((posEndDef + calltipEndDefinition.length()) < functionDefinition.length())) {
						functionDefinition.insert(posEndDef + calltipEndDefinition.length(), "\n\002");
					} else {
						functionDefinition.append("\n\002");
					}
				} else {
					if ((posEndDef > 1) &&
					        ((posEndDef + calltipEndDefinition.length()) < functionDefinition.length())) {
						functionDefinition.insert(posEndDef + calltipEndDefinition.length(), "\n");
					}
				}
			} else if (maxCallTips > 1) {
				functionDefinition.insert(1, "\002");
			}

			SString definitionForDisplay;
			if (callTipUseEscapes) {
				char *sUnslashed = StringDup(functionDefinition.c_str());
				UnSlash(sUnslashed);
				definitionForDisplay = sUnslashed;
				delete []sUnslashed;
			} else {
				definitionForDisplay = functionDefinition;
			}

			wEditor.CallString(SCI_CALLTIPSHOW, lastPosCallTip - currentCallTipWord.length(), definitionForDisplay.c_str());
			ContinueCallTip();
		}
	}
}

bool SciTEBase::StartCallTip() {
	currentCallTip = 0;
	currentCallTipWord = "";
	SString line = GetLine();
	int current = GetCaretInLine();
	int pos = wEditor.Call(SCI_GETCURRENTPOS);
	int braces;
	do {
		braces = 0;
		while (current > 0 && (braces || !calltipParametersStart.contains(line[current - 1]))) {
			if (calltipParametersStart.contains(line[current - 1]))
				braces--;
			else if (calltipParametersEnd.contains(line[current - 1]))
				braces++;
			current--;
			pos--;
		}
		if (current > 0) {
			current--;
			pos--;
		} else
			break;
		while (current > 0 && isspacechar(line[current - 1])) {
			current--;
			pos--;
		}
	} while (current > 0 && !calltipWordCharacters.contains(line[current - 1]));
	if (current <= 0)
		return true;

	startCalltipWord = current - 1;
	while (startCalltipWord > 0 &&
	        calltipWordCharacters.contains(line[startCalltipWord - 1])) {
		startCalltipWord--;
	}

	line.change(current, '\0');
	currentCallTipWord = line.c_str() + startCalltipWord;
	functionDefinition = "";
	FillFunctionDefinition(pos);
	return true;
}

void SciTEBase::ContinueCallTip() {
	SString line = GetLine();
	int current = GetCaretInLine();

	int braces = 0;
	int commas = 0;
	for (int i = startCalltipWord; i < current; i++) {
		if (calltipParametersStart.contains(line[i]))
			braces++;
		else if (calltipParametersEnd.contains(line[i]) && braces > 0)
			braces--;
		else if (braces == 1 && calltipParametersSeparators.contains(line[i]))
			commas++;
	}

	int startHighlight = 0;
	while (functionDefinition[startHighlight] && !calltipParametersStart.contains(functionDefinition[startHighlight]))
		startHighlight++;
	if (calltipParametersStart.contains(functionDefinition[startHighlight]))
		startHighlight++;
	while (functionDefinition[startHighlight] && commas > 0) {
		if (calltipParametersSeparators.contains(functionDefinition[startHighlight]))
			commas--;
		// If it reached the end of the argument list it means that the user typed in more
		// arguments than the ones listed in the calltip
		if (calltipParametersEnd.contains(functionDefinition[startHighlight]))
			commas = 0;
		else
			startHighlight++;
	}
	if (calltipParametersSeparators.contains(functionDefinition[startHighlight]))
		startHighlight++;
	int endHighlight = startHighlight;
	while (functionDefinition[endHighlight] && !calltipParametersSeparators.contains(functionDefinition[endHighlight]) && !calltipParametersEnd.contains(functionDefinition[endHighlight]))
		endHighlight++;
	if (callTipUseEscapes) {
		char *sUnslashed = StringDup(functionDefinition.substr(0, startHighlight + 1).c_str());
		int unslashedStartHighlight = UnSlash(sUnslashed) - 1;
		delete []sUnslashed;

		sUnslashed = StringDup(functionDefinition.substr(startHighlight, endHighlight - startHighlight + 1).c_str());
		int unslashedEndHighlight = unslashedStartHighlight + UnSlash(sUnslashed) - 1;
		delete []sUnslashed;

		startHighlight = unslashedStartHighlight;
		endHighlight = unslashedEndHighlight;
	}

	wEditor.Call(SCI_CALLTIPSETHLT, startHighlight, endHighlight);
}

void SciTEBase::EliminateDuplicateWords(char *words) {
	std::set<std::string> wordSet;
	std::vector<char> wordsOut(strlen(words) + 1);
	char *wordsWrite = &wordsOut[0];

	char *wordCurrent = words;
	while (*wordCurrent) {
		char *afterWord = strchr(wordCurrent, ' ');
		if (!afterWord)
			afterWord = wordCurrent + strlen(wordCurrent);
		std::string word(wordCurrent, afterWord);
		if (wordSet.count(word) == 0) {
			wordSet.insert(word);
			if (wordsWrite != &wordsOut[0])
				*wordsWrite++ = ' ';
			strcpy(wordsWrite, word.c_str());
			wordsWrite += word.length();
		}
		wordCurrent = afterWord;
		if (*wordCurrent)
			wordCurrent++;
	}

	*wordsWrite = '\0';
	strcpy(words, &wordsOut[0]);
}

bool SciTEBase::StartAutoComplete() {
	SString line = GetLine();
	int current = GetCaretInLine();

	int startword = current;

	while ((startword > 0) &&
	        (calltipWordCharacters.contains(line[startword - 1]) ||
	         autoCompleteStartCharacters.contains(line[startword - 1]))) {
		startword--;
	}

	SString root = line.substr(startword, current - startword);
	if (apis) {
		char *words = GetNearestWords(root.c_str(), root.length(),
			calltipParametersStart.c_str(), autoCompleteIgnoreCase);
		if (words) {
			EliminateDuplicateWords(words);
			wEditor.Call(SCI_AUTOCSETSEPARATOR, ' ');
			wEditor.CallString(SCI_AUTOCSHOW, root.length(), words);
			delete []words;
		}
	}
	return true;
}

bool SciTEBase::StartAutoCompleteWord(bool onlyOneWord) {
	SString line = GetLine();
	int current = GetCaretInLine();

	int startword = current;
	// Autocompletion of pure numbers is mostly an annoyance
	bool allNumber = true;
	while (startword > 0 && wordCharacters.contains(line[startword - 1])) {
		startword--;
		if (line[startword] < '0' || line[startword] > '9') {
			allNumber = false;
		}
	}
	if (startword == current || allNumber)
		return true;
	SString root = line.substr(startword, current - startword);
	int doclen = LengthDocument();
	Sci_TextToFind ft = {{0, 0}, 0, {0, 0}};
	ft.lpstrText = const_cast<char *>(root.c_str());
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = doclen;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	const int flags = SCFIND_WORDSTART | (autoCompleteIgnoreCase ? 0 : SCFIND_MATCHCASE);
	int posCurrentWord = wEditor.Call(SCI_GETCURRENTPOS) - static_cast<int>(root.length());
	unsigned int minWordLength = 0;
	unsigned int nwords = 0;

	// wordsNear contains a list of words separated by single spaces and with a space
	// at the start and end. This makes it easy to search for words.
	SString wordsNear;
	wordsNear.setsizegrowth(1000);
	wordsNear.append("\n");

	int posFind = wEditor.CallString(SCI_FINDTEXT, flags, reinterpret_cast<char *>(&ft));
	TextReader acc(wEditor);
	while (posFind >= 0 && posFind < doclen) {	// search all the document
		int wordEnd = posFind + static_cast<int>(root.length());
		if (posFind != posCurrentWord) {
			while (wordCharacters.contains(acc.SafeGetCharAt(wordEnd)))
				wordEnd++;
			unsigned int wordLength = wordEnd - posFind;
			if (wordLength > root.length()) {
				SString word = GetRange(wEditor, posFind, wordEnd);
				word.insert(0, "\n");
				word.append("\n");
				if (!wordsNear.contains(word.c_str())) {	// add a new entry
					wordsNear += word.c_str() + 1;
					if (minWordLength < wordLength)
						minWordLength = wordLength;

					nwords++;
					if (onlyOneWord && nwords > 1) {
						return true;
					}
				}
			}
		}
		ft.chrg.cpMin = wordEnd;
		posFind = wEditor.CallString(SCI_FINDTEXT, flags, reinterpret_cast<char *>(&ft));
	}
	size_t length = wordsNear.length();
	if ((length > 2) && (!onlyOneWord || (minWordLength > root.length()))) {
		// Protect spaces by temporrily transforming to \001
		wordsNear.substitute(' ', '\001');
		StringList wl(true);
		wl.Set(wordsNear.c_str());
		char *words = wl.GetNearestWords("", 0, autoCompleteIgnoreCase);
		SString acText(words);
		// Use \n as word separator
		acText.substitute(' ', '\n');
		// Return spaces from \001
		acText.substitute('\001', ' ');
		wEditor.Call(SCI_AUTOCSETSEPARATOR, '\n');
		wEditor.CallString(SCI_AUTOCSHOW, root.length(), acText.c_str());
		delete []words;
	} else {
		wEditor.Call(SCI_AUTOCCANCEL);
	}
	return true;
}

bool SciTEBase::PerformInsertAbbreviation() {
	SString data = propsAbbrev.Get(abbrevInsert);
	size_t dataLength = data.length();
	if (dataLength == 0) {
		return true; // returning if expanded abbreviation is empty
	}

	char *expbuf = new char[dataLength + 1];
	strcpy(expbuf, data.c_str());
	UnSlash(expbuf);
	size_t expbuflen = strlen(expbuf);
	int caret_pos = wEditor.Call(SCI_GETSELECTIONSTART);
	int sel_start = caret_pos;
	int sel_length = wEditor.Call(SCI_GETSELECTIONEND) - sel_start;
	bool at_start = true;
	bool double_pipe = false;
	size_t last_pipe = expbuflen;
	int currentLineNumber = wEditor.Call(SCI_LINEFROMPOSITION, caret_pos);
	int indent = 0;
	int indentSize = wEditor.Call(SCI_GETINDENT);
	int indentChars = (wEditor.Call(SCI_GETUSETABS) && wEditor.Call(SCI_GETTABWIDTH) ? wEditor.Call(SCI_GETTABWIDTH) : 1);
	int indentExtra = 0;
	bool isIndent = true;
	int eolMode = wEditor.Call(SCI_GETEOLMODE);
	if (props.GetInt("indent.automatic")) {
		indent = GetLineIndentation(currentLineNumber);
	}

	size_t i;
	// find last |, can't be strrchr(exbuf, '|') because of ||
	for (i = expbuflen; i--; ) {
		if (expbuf[i] == '|' && (i == 0 || expbuf[i-1] != '|')) {
			last_pipe = i;
			break;
		}
	}

	wEditor.Call(SCI_BEGINUNDOACTION);

	// add the abbreviation one character at a time
	for (i = 0; i < expbuflen; i++) {
		char c = expbuf[i];
		SString abbrevText("");
		if (isIndent && c == '\t') {
			if (props.GetInt("indent.automatic")) {
				indentExtra++;
				SetLineIndentation(currentLineNumber, indent + indentSize * indentExtra);
				caret_pos += indentSize / indentChars;
			}
		} else {
			switch (c) {
			case '|':
				// user may want to insert '|' instead of caret
				if (i < (dataLength - 1) && expbuf[i + 1] == '|') {
					// put '|' into the line
					abbrevText += c;
					i++;
				} else if (i != last_pipe) {
					double_pipe = true;
				} else {
					// indent on multiple lines
					int j = currentLineNumber + 1; // first line indented as others
					currentLineNumber = wEditor.Call(SCI_LINEFROMPOSITION, caret_pos + sel_length);
					for (; j <= currentLineNumber; j++) {
						SetLineIndentation(j, GetLineIndentation(j) + indentSize * indentExtra);
						caret_pos += indentExtra * indentSize / indentChars;
					}

					at_start = false;
					caret_pos += sel_length;
				}
				break;
			case '\r':
				// backward compatibility
				break;
			case '\n':
				if (eolMode == SC_EOL_CRLF || eolMode == SC_EOL_CR) {
					abbrevText += '\r';
				}
				if (eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) {
					abbrevText += '\n';
				}
				break;
			default:
				abbrevText += c;
				break;
			}
			if (caret_pos > wEditor.Call(SCI_GETLENGTH)) {
				caret_pos = wEditor.Call(SCI_GETLENGTH);
			}
			wEditor.CallString(SCI_INSERTTEXT, caret_pos, abbrevText.c_str());
			if (!double_pipe && at_start) {
				sel_start += static_cast<int>(abbrevText.length());
			}
			caret_pos += static_cast<int>(abbrevText.length());
			if (c == '\n') {
				isIndent = true;
				indentExtra = 0;
				currentLineNumber++;
				SetLineIndentation(currentLineNumber, indent);
				caret_pos += indent / indentChars;
				if (!double_pipe && at_start) {
					sel_start += indent / indentChars;
				}
			} else {
				isIndent = false;
			}
		}
	}

	// set the caret to the desired position
	if (double_pipe) {
		sel_length = 0;
	}
	wEditor.Call(SCI_SETSEL, sel_start, sel_start + sel_length);

	wEditor.Call(SCI_ENDUNDOACTION);
	delete []expbuf;
	return true;
}

bool SciTEBase::StartInsertAbbreviation() {
	if (!AbbrevDialog()) {
		return true;
	}

	return PerformInsertAbbreviation();
}

bool SciTEBase::StartExpandAbbreviation() {
	int currentPos = GetCaretInLine();
	int position = wEditor.Call(SCI_GETCURRENTPOS); // from the beginning
	char *linebuf = new char[currentPos + 2];
	GetLine(linebuf, currentPos + 2);	// Just get text to the left of the caret
	linebuf[currentPos] = '\0';
	int abbrevPos = (currentPos > 32 ? currentPos - 32 : 0);
	const char *abbrev = linebuf + abbrevPos;
	SString data;
	size_t dataLength = 0;
	int abbrevLength = currentPos - abbrevPos;
	// Try each potential abbreviation from the first letter on a line
	// and expanding to the right.
	// We arbitrarily limit the length of an abbreviation (seems a reasonable value..),
	// and of course stop on the caret.
	while (abbrevLength > 0) {
		data = propsAbbrev.Get(abbrev);
		dataLength = data.length();
		if (dataLength > 0) {
			break;	/* Found */
		}
		abbrev++;	// One more letter to the right
		abbrevLength--;
	}

	if (dataLength == 0) {
		delete []linebuf;
		WarnUser(warnNotFound);	// No need for a special warning
		return true; // returning if expanded abbreviation is empty
	}

	char *expbuf = new char[dataLength + 1];
	strcpy(expbuf, data.c_str());
	UnSlash(expbuf);
	size_t expbuflen = strlen(expbuf);
	int caret_pos = -1; // caret position
	int currentLineNumber = GetCurrentLineNumber();
	int indent = 0;
	int indentExtra = 0;
	bool isIndent = true;
	int eolMode = wEditor.Call(SCI_GETEOLMODE);
	if (props.GetInt("indent.automatic")) {
		indent = GetLineIndentation(currentLineNumber);
	}

	wEditor.Call(SCI_BEGINUNDOACTION);
	wEditor.Call(SCI_SETSEL, position - abbrevLength, position);

	// add the abbreviation one character at a time
	for (size_t i = 0; i < expbuflen; i++) {
		char c = expbuf[i];
		SString abbrevText("");
		if (isIndent && c == '\t') {
			indentExtra++;
			SetLineIndentation(currentLineNumber, indent + wEditor.Call(SCI_GETINDENT) * indentExtra);
		} else {
			switch (c) {
			case '|':
				// user may want to insert '|' instead of caret
				if (i < (dataLength - 1) && expbuf[i + 1] == '|') {
					// put '|' into the line
					abbrevText += c;
					i++;
				} else if (caret_pos == -1) {
					if (i == 0) {
						// when caret is set at the first place in abbreviation
						caret_pos = wEditor.Call(SCI_GETCURRENTPOS) - abbrevLength;
					} else {
						caret_pos = wEditor.Call(SCI_GETCURRENTPOS);
					}
				}
				break;
			case '\n':
				if (eolMode == SC_EOL_CRLF || eolMode == SC_EOL_CR) {
					abbrevText += '\r';
				}
				if (eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) {
					abbrevText += '\n';
				}
				break;
			default:
				abbrevText += c;
				break;
			}
			wEditor.CallString(SCI_REPLACESEL, 0, abbrevText.c_str());
			if (c == '\n') {
				isIndent = true;
				indentExtra = 0;
				currentLineNumber++;
				SetLineIndentation(currentLineNumber, indent);
			} else {
				isIndent = false;
			}
		}
	}

	// set the caret to the desired position
	if (caret_pos != -1) {
		wEditor.Call(SCI_GOTOPOS, caret_pos);
	}

	wEditor.Call(SCI_ENDUNDOACTION);
	delete []expbuf;
	delete []linebuf;
	return true;
}

bool SciTEBase::StartBlockComment() {
	SString fileNameForExtension = ExtensionFileName();
	SString lexerName = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString base("comment.block.");
	SString comment_at_line_start("comment.block.at.line.start.");
	base += lexerName;
	comment_at_line_start += lexerName;
	bool placeCommentsAtLineStart = props.GetInt(comment_at_line_start.c_str()) != 0;

	SString comment = props.Get(base.c_str());
	if (comment == "") { // user friendly error message box
		GUI::gui_string sBase = GUI::StringFromUTF8(base.c_str());
		GUI::gui_string error = LocaliseMessage(
		            "Block comment variable '^0' is not defined in SciTE *.properties!", sBase.c_str());
		WindowMessageBox(wSciTE, error, MB_OK | MB_ICONWARNING);
		return true;
	}
	SString long_comment = comment;
	long_comment.append(" ");
	int selectionStart = wEditor.Call(SCI_GETSELECTIONSTART);
	int selectionEnd = wEditor.Call(SCI_GETSELECTIONEND);
	int caretPosition = wEditor.Call(SCI_GETCURRENTPOS);
	// checking if caret is located in _beginning_ of selected block
	bool move_caret = caretPosition < selectionEnd;
	int selStartLine = wEditor.Call(SCI_LINEFROMPOSITION, selectionStart);
	int selEndLine = wEditor.Call(SCI_LINEFROMPOSITION, selectionEnd);
	int lines = selEndLine - selStartLine;
	int firstSelLineStart = wEditor.Call(SCI_POSITIONFROMLINE, selStartLine);
	// "caret return" is part of the last selected line
	if ((lines > 0) &&
	        (selectionEnd == wEditor.Call(SCI_POSITIONFROMLINE, selEndLine)))
		selEndLine--;
	wEditor.Call(SCI_BEGINUNDOACTION);
	for (int i = selStartLine; i <= selEndLine; i++) {
		int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, i);
		int lineIndent = lineStart;
		int lineEnd = wEditor.Call(SCI_GETLINEENDPOSITION, i);
		if (!placeCommentsAtLineStart) {
			lineIndent = GetLineIndentPosition(i);
		}
		SString linebuf = GetRange(wEditor, lineIndent, lineEnd);
		// empty lines are not commented
		if (linebuf.length() < 1)
			continue;
		if (linebuf.startswith(comment.c_str())) {
			int commentLength = static_cast<int>(comment.length());
			if (linebuf.startswith(long_comment.c_str())) {
				// Removing comment with space after it.
				commentLength = static_cast<int>(long_comment.length());
			}
			wEditor.Call(SCI_SETSEL, lineIndent, lineIndent + commentLength);
			wEditor.CallString(SCI_REPLACESEL, 0, "");
			if (i == selStartLine) // is this the first selected line?
				selectionStart -= commentLength;
			selectionEnd -= commentLength; // every iteration
			continue;
		}
		if (i == selStartLine) // is this the first selected line?
			selectionStart += static_cast<int>(long_comment.length());
		selectionEnd += static_cast<int>(long_comment.length()); // every iteration
		wEditor.CallString(SCI_INSERTTEXT, lineIndent, long_comment.c_str());
	}
	// after uncommenting selection may promote itself to the lines
	// before the first initially selected line;
	// another problem - if only comment symbol was selected;
	if (selectionStart < firstSelLineStart) {
		if (selectionStart >= selectionEnd - (static_cast<int>(long_comment.length()) - 1))
			selectionEnd = firstSelLineStart;
		selectionStart = firstSelLineStart;
	}
	if (move_caret) {
		// moving caret to the beginning of selected block
		wEditor.Call(SCI_GOTOPOS, selectionEnd);
		wEditor.Call(SCI_SETCURRENTPOS, selectionStart);
	} else {
		wEditor.Call(SCI_SETSEL, selectionStart, selectionEnd);
	}
	wEditor.Call(SCI_ENDUNDOACTION);
	return true;
}

static const char *LineEndString(int eolMode) {
	switch (eolMode) {
		case SC_EOL_CRLF:
			return "\r\n";
		case SC_EOL_CR:
			return "\r";
		case SC_EOL_LF:
		default:
			return "\n";
	}
}

bool SciTEBase::StartBoxComment() {
	// Get start/middle/end comment strings from options file(s)
	SString fileNameForExtension = ExtensionFileName();
	SString lexerName = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString start_base("comment.box.start.");
	SString middle_base("comment.box.middle.");
	SString end_base("comment.box.end.");
	SString white_space(" ");
	SString eol(LineEndString(wEditor.Call(SCI_GETEOLMODE)));
	start_base += lexerName;
	middle_base += lexerName;
	end_base += lexerName;
	SString start_comment = props.Get(start_base.c_str());
	SString middle_comment = props.Get(middle_base.c_str());
	SString end_comment = props.Get(end_base.c_str());
	if (start_comment == "" || middle_comment == "" || end_comment == "") {
		GUI::gui_string sStart = GUI::StringFromUTF8(start_base.c_str());
		GUI::gui_string sMiddle = GUI::StringFromUTF8(middle_base.c_str());
		GUI::gui_string sEnd = GUI::StringFromUTF8(end_base.c_str());
		GUI::gui_string error = LocaliseMessage(
		            "Box comment variables '^0', '^1' and '^2' are not defined in SciTE *.properties!",
		            sStart.c_str(), sMiddle.c_str(), sEnd.c_str());
		WindowMessageBox(wSciTE, error, MB_OK | MB_ICONWARNING);
		return true;
	}

	// Note selection and cursor location so that we can reselect text and reposition cursor after we insert comment strings
	size_t selectionStart = wEditor.Call(SCI_GETSELECTIONSTART);
	size_t selectionEnd = wEditor.Call(SCI_GETSELECTIONEND);
	size_t caretPosition = wEditor.Call(SCI_GETCURRENTPOS);
	bool move_caret = caretPosition < selectionEnd;
	size_t selStartLine = wEditor.Call(SCI_LINEFROMPOSITION, selectionStart);
	size_t selEndLine = wEditor.Call(SCI_LINEFROMPOSITION, selectionEnd);
	size_t lines = selEndLine - selStartLine + 1;

	// If selection ends at start of last selected line, fake it so that selection goes to end of second-last selected line
	if (lines > 1 && selectionEnd == static_cast<size_t>(wEditor.Call(SCI_POSITIONFROMLINE, selEndLine))) {
		selEndLine--;
		lines--;
		selectionEnd = wEditor.Call(SCI_GETLINEENDPOSITION, selEndLine);
	}

	// Pad comment strings with appropriate whitespace, then figure out their lengths (end_comment is a bit special-- see below)
	start_comment += white_space;
	middle_comment += white_space;
	int start_comment_length = static_cast<int>(start_comment.length());
	int middle_comment_length = static_cast<int>(middle_comment.length());
	int end_comment_length = static_cast<int>(end_comment.length());
	int whitespace_length = static_cast<int>(white_space.length());

	// Calculate the length of the longest comment string to be inserted, and allocate a null-terminated char buffer of equal size
	int maxCommentLength = start_comment_length;
	if (middle_comment_length > maxCommentLength)
		maxCommentLength = middle_comment_length;
	if (end_comment_length + whitespace_length > maxCommentLength)
		maxCommentLength = end_comment_length + whitespace_length;
	char *tempString = new char[maxCommentLength + 1];

	wEditor.Call(SCI_BEGINUNDOACTION);

	// Insert start_comment if needed
	int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, selStartLine);
	GetRange(wEditor, lineStart, lineStart + start_comment_length, tempString);
	tempString[start_comment_length] = '\0';
	if (start_comment != tempString) {
		wEditor.CallString(SCI_INSERTTEXT, lineStart, start_comment.c_str());
		selectionStart += start_comment_length;
		selectionEnd += start_comment_length;
	}

	if (lines <= 1) {
		// Only a single line was selected, so just append whitespace + end-comment at end of line if needed
		int lineEnd = wEditor.Call(SCI_GETLINEENDPOSITION, selEndLine);
		GetRange(wEditor, lineEnd - end_comment_length, lineEnd, tempString);
		tempString[end_comment_length] = '\0';
		if (end_comment != tempString) {
			end_comment.insert(0, white_space.c_str());
			wEditor.CallString(SCI_INSERTTEXT, lineEnd, end_comment.c_str());
		}
	} else {
		// More than one line selected, so insert middle_comments where needed
		for (size_t i = selStartLine + 1; i < selEndLine; i++) {
			lineStart = wEditor.Call(SCI_POSITIONFROMLINE, i);
			GetRange(wEditor, lineStart, lineStart + middle_comment_length, tempString);
			tempString[middle_comment_length] = '\0';
			if (middle_comment != tempString) {
				wEditor.CallString(SCI_INSERTTEXT, lineStart, middle_comment.c_str());
				selectionEnd += middle_comment_length;
			}
		}

		// If last selected line is not middle-comment or end-comment, we need to insert
		// a middle-comment at the start of last selected line and possibly still insert
		// and end-comment tag after the last line (extra logic is necessary to
		// deal with the case that user selected the end-comment tag)
		lineStart = wEditor.Call(SCI_POSITIONFROMLINE, selEndLine);
		GetRange(wEditor, lineStart, lineStart + end_comment_length, tempString);
		tempString[end_comment_length] = '\0';
		if (end_comment != tempString) {
			GetRange(wEditor, lineStart, lineStart + middle_comment_length, tempString);
			tempString[middle_comment_length] = '\0';
			if (middle_comment != tempString) {
				wEditor.CallString(SCI_INSERTTEXT, lineStart, middle_comment.c_str());
				selectionEnd += middle_comment_length;
			}

			// And since we didn't find the end-comment string yet, we need to check the *next* line
			//  to see if it's necessary to insert an end-comment string and a linefeed there....
			lineStart = wEditor.Call(SCI_POSITIONFROMLINE, selEndLine + 1);
			GetRange(wEditor, lineStart, lineStart + (int) end_comment_length, tempString);
			tempString[end_comment_length] = '\0';
			if (end_comment != tempString) {
				end_comment += eol;
				wEditor.CallString(SCI_INSERTTEXT, lineStart, end_comment.c_str());
			}
		}
	}

	if (move_caret) {
		// moving caret to the beginning of selected block
		wEditor.Call(SCI_GOTOPOS, selectionEnd);
		wEditor.Call(SCI_SETCURRENTPOS, selectionStart);
	} else {
		wEditor.Call(SCI_SETSEL, selectionStart, selectionEnd);
	}

	wEditor.Call(SCI_ENDUNDOACTION);

	delete[] tempString;

	return true;
}

bool SciTEBase::StartStreamComment() {
	SString fileNameForExtension = ExtensionFileName();
	SString lexerName = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString start_base("comment.stream.start.");
	SString end_base("comment.stream.end.");
	SString white_space(" ");
	start_base += lexerName;
	end_base += lexerName;
	SString start_comment = props.Get(start_base.c_str());
	SString end_comment = props.Get(end_base.c_str());
	if (start_comment == "" || end_comment == "") {
		GUI::gui_string sStart = GUI::StringFromUTF8(start_base.c_str());
		GUI::gui_string sEnd = GUI::StringFromUTF8(end_base.c_str());
		GUI::gui_string error = LocaliseMessage(
		            "Stream comment variables '^0' and '^1' are not defined in SciTE *.properties!",
		            sStart.c_str(), sEnd.c_str());
		WindowMessageBox(wSciTE, error, MB_OK | MB_ICONWARNING);
		return true;
	}
	start_comment += white_space;
	white_space += end_comment;
	end_comment = white_space;
	size_t start_comment_length = start_comment.length();
	size_t selectionStart = wEditor.Call(SCI_GETSELECTIONSTART);
	size_t selectionEnd = wEditor.Call(SCI_GETSELECTIONEND);
	size_t caretPosition = wEditor.Call(SCI_GETCURRENTPOS);
	// checking if caret is located in _beginning_ of selected block
	bool move_caret = caretPosition < selectionEnd;
	// if there is no selection?
	if (selectionEnd - selectionStart <= 0) {
		int selLine = wEditor.Call(SCI_LINEFROMPOSITION, selectionStart);
		int lineIndent = GetLineIndentPosition(selLine);
		int lineEnd = wEditor.Call(SCI_GETLINEENDPOSITION, selLine);
		if (RangeIsAllWhitespace(lineIndent, lineEnd))
			return true; // we are not dealing with empty lines
		char linebuf[1000];
		GetLine(linebuf, sizeof(linebuf));
		int current = GetCaretInLine();
		// checking if we are not inside a word
		if (!wordCharacters.contains(linebuf[current]))
			return true; // caret is located _between_ words
		int startword = current;
		int endword = current;
		int start_counter = 0;
		int end_counter = 0;
		while (startword > 0 && wordCharacters.contains(linebuf[startword - 1])) {
			start_counter++;
			startword--;
		}
		// checking _beginning_ of the word
		if (startword == current)
			return true; // caret is located _before_ a word
		while (linebuf[endword + 1] != '\0' && wordCharacters.contains(linebuf[endword + 1])) {
			end_counter++;
			endword++;
		}
		selectionStart -= start_counter;
		selectionEnd += (end_counter + 1);
	}
	wEditor.Call(SCI_BEGINUNDOACTION);
	wEditor.CallString(SCI_INSERTTEXT, selectionStart, start_comment.c_str());
	selectionEnd += start_comment_length;
	selectionStart += start_comment_length;
	wEditor.CallString(SCI_INSERTTEXT, selectionEnd, end_comment.c_str());
	if (move_caret) {
		// moving caret to the beginning of selected block
		wEditor.Call(SCI_GOTOPOS, selectionEnd);
		wEditor.Call(SCI_SETCURRENTPOS, selectionStart);
	} else {
		wEditor.Call(SCI_SETSEL, selectionStart, selectionEnd);
	}
	wEditor.Call(SCI_ENDUNDOACTION);
	return true;
}

/**
 * Return the length of the given line, not counting the EOL.
 */
int SciTEBase::GetLineLength(int line) {
	return wEditor.Call(SCI_GETLINEENDPOSITION, line) - wEditor.Call(SCI_POSITIONFROMLINE, line);
}

int SciTEBase::GetCurrentLineNumber() {
	return wEditor.Call(SCI_LINEFROMPOSITION,
	        wEditor.Call(SCI_GETCURRENTPOS));
}

int SciTEBase::GetCurrentScrollPosition() {
	int lineDisplayTop = wEditor.Call(SCI_GETFIRSTVISIBLELINE);
	return wEditor.Call(SCI_DOCLINEFROMVISIBLE, lineDisplayTop);
}

/**
 * Set up properties for ReadOnly, EOLMode, BufferLength, NbOfLines, SelLength, SelHeight.
 */
void SciTEBase::SetTextProperties(
    PropSetFile &ps) {			///< Property set to update.

	const int TEMP_LEN = 100;
	char temp[TEMP_LEN];

	std::string ro = GUI::UTF8FromString(localiser.Text("READ"));
	ps.Set("ReadOnly", isReadOnly ? ro.c_str() : "");

	int eolMode = wEditor.Call(SCI_GETEOLMODE);
	ps.Set("EOLMode", eolMode == SC_EOL_CRLF ? "CR+LF" : (eolMode == SC_EOL_LF ? "LF" : "CR"));

	sprintf(temp, "%d", LengthDocument());
	ps.Set("BufferLength", temp);

	ps.SetInteger("NbOfLines", wEditor.Call(SCI_GETLINECOUNT));

	Sci_CharacterRange crange = GetSelection();
	int selFirstLine = wEditor.Call(SCI_LINEFROMPOSITION, crange.cpMin);
	int selLastLine = wEditor.Call(SCI_LINEFROMPOSITION, crange.cpMax);
	long charCount = 0;
	if (wEditor.Call(SCI_GETSELECTIONMODE) == SC_SEL_RECTANGLE) {
		for (int line = selFirstLine; line <= selLastLine; line++) {
			int startPos = wEditor.Call(SCI_GETLINESELSTARTPOSITION, line);
			int endPos = wEditor.Call(SCI_GETLINESELENDPOSITION, line);
			charCount += wEditor.Call(SCI_COUNTCHARACTERS, startPos, endPos);
		}
	} else {
		charCount = wEditor.Call(SCI_COUNTCHARACTERS, crange.cpMin, crange.cpMax);
	}
	sprintf(temp, "%ld", charCount);
	ps.Set("SelLength", temp);
	int caretPos = wEditor.Call(SCI_GETCURRENTPOS);
	int selAnchor = wEditor.Call(SCI_GETANCHOR);
	if (0 == (crange.cpMax - crange.cpMin)) {
		sprintf(temp, "%d", 0);
	} else if (selLastLine == selFirstLine) {
		sprintf(temp, "%d", 1);
	} else if ((wEditor.Call(SCI_GETCOLUMN, caretPos) == 0 && (selAnchor <= caretPos)) ||
	        ((wEditor.Call(SCI_GETCOLUMN, selAnchor) == 0) && (selAnchor > caretPos ))) {
		sprintf(temp, "%d", selLastLine - selFirstLine);
	} else {
		sprintf(temp, "%d", selLastLine - selFirstLine + 1);
	}
	ps.Set("SelHeight", temp);
}

void SciTEBase::UpdateStatusBar(bool bUpdateSlowData) {
	if (sbVisible) {
		if (bUpdateSlowData) {
			SetFileProperties(propsStatus);
		}
		SetTextProperties(propsStatus);
		int caretPos = wEditor.Call(SCI_GETCURRENTPOS);
		propsStatus.SetInteger("LineNumber",
		        wEditor.Call(SCI_LINEFROMPOSITION, caretPos) + 1);
		propsStatus.SetInteger("ColumnNumber",
		        wEditor.Call(SCI_GETCOLUMN, caretPos) + 1);
		propsStatus.Set("OverType", wEditor.Call(SCI_GETOVERTYPE) ? "OVR" : "INS");

		char sbKey[32];
		sprintf(sbKey, "statusbar.text.%d", sbNum);
		SString msg = propsStatus.GetExpanded(sbKey);
		if (msg.size() && sbValue != msg) {	// To avoid flickering, update only if needed
			SetStatusBarText(msg.c_str());
			sbValue = msg;
		}
	} else {
		sbValue = "";
	}
}

void SciTEBase::SetLineIndentation(int line, int indent) {
	if (indent < 0)
		return;
	Sci_CharacterRange crange = GetSelection();
	int posBefore = GetLineIndentPosition(line);
	wEditor.Call(SCI_SETLINEINDENTATION, line, indent);
	int posAfter = GetLineIndentPosition(line);
	int posDifference = posAfter - posBefore;
	if (posAfter > posBefore) {
		// Move selection on
		if (crange.cpMin >= posBefore) {
			crange.cpMin += posDifference;
		}
		if (crange.cpMax >= posBefore) {
			crange.cpMax += posDifference;
		}
	} else if (posAfter < posBefore) {
		// Move selection back
		if (crange.cpMin >= posAfter) {
			if (crange.cpMin >= posBefore)
				crange.cpMin += posDifference;
			else
				crange.cpMin = posAfter;
		}
		if (crange.cpMax >= posAfter) {
			if (crange.cpMax >= posBefore)
				crange.cpMax += posDifference;
			else
				crange.cpMax = posAfter;
		}
	}
	SetSelection(static_cast<int>(crange.cpMin), static_cast<int>(crange.cpMax));
}

int SciTEBase::GetLineIndentation(int line) {
	return wEditor.Call(SCI_GETLINEINDENTATION, line);
}

int SciTEBase::GetLineIndentPosition(int line) {
	return wEditor.Call(SCI_GETLINEINDENTPOSITION, line);
}

static SString CreateIndentation(int indent, int tabSize, bool insertSpaces) {
	SString indentation;
	if (!insertSpaces) {
		while (indent >= tabSize) {
			indentation.append("\t", 1);
			indent -= tabSize;
		}
	}
	while (indent > 0) {
		indentation.append(" ", 1);
		indent--;
	}
	return indentation;
}

void SciTEBase::ConvertIndentation(int tabSize, int useTabs) {
	wEditor.Call(SCI_BEGINUNDOACTION);
	int maxLine = wEditor.Call(SCI_GETLINECOUNT);
	for (int line = 0; line < maxLine; line++) {
		int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, line);
		int indent = GetLineIndentation(line);
		int indentPos = GetLineIndentPosition(line);
		const int maxIndentation = 1000;
		if (indent < maxIndentation) {
			SString indentationNow = GetRange(wEditor, lineStart, indentPos);
			SString indentationWanted = CreateIndentation(indent, tabSize, !useTabs);
			if (indentationNow != indentationWanted) {
				wEditor.Call(SCI_SETTARGETSTART, lineStart);
				wEditor.Call(SCI_SETTARGETEND, indentPos);
				wEditor.CallString(SCI_REPLACETARGET, indentationWanted.length(),
					indentationWanted.c_str());
			}
		}
	}
	wEditor.Call(SCI_ENDUNDOACTION);
}

bool SciTEBase::RangeIsAllWhitespace(int start, int end) {
	TextReader acc(wEditor);
	for (int i = start; i < end; i++) {
		if ((acc[i] != ' ') && (acc[i] != '\t'))
			return false;
	}
	return true;
}

unsigned int SciTEBase::GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len) {
	for (int i = 0; i < len; i++)
		sv[i] = "";
	TextReader acc(wEditor);
	SString s;
	int part = 0;
	int thisLineStart = wEditor.Call(SCI_POSITIONFROMLINE, line);
	int nextLineStart = wEditor.Call(SCI_POSITIONFROMLINE, line + 1);
	for (int pos = thisLineStart; pos < nextLineStart; pos++) {
		if ((acc.StyleAt(pos) == style1) || (acc.StyleAt(pos) == style2)) {
			char c[2];
			c[0] = acc[pos];
			c[1] = '\0';
			s += c;
		} else if (s.length() > 0) {
			if (part < len) {
				sv[part++] = s;
			}
			s = "";
		}
	}
	if ((s.length() > 0) && (part < len)) {
		sv[part++] = s;
	}
	return part;
}

inline bool IsAlphabetic(unsigned int ch) {
	return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'));
}

static bool includes(const StyleAndWords &symbols, const SString &value) {
	if (symbols.words.length() == 0) {
		return false;
	} else if (IsAlphabetic(symbols.words[0])) {
		// Set of symbols separated by spaces
		size_t lenVal = value.length();
		const char *symbol = symbols.words.c_str();
		while (symbol) {
			const char *symbolEnd = strchr(symbol, ' ');
			size_t lenSymbol = strlen(symbol);
			if (symbolEnd)
				lenSymbol = symbolEnd - symbol;
			if (lenSymbol == lenVal) {
				if (strncmp(symbol, value.c_str(), lenSymbol) == 0) {
					return true;
				}
			}
			symbol = symbolEnd;
			if (symbol)
				symbol++;
		}
	} else {
		// Set of individual characters. Only one character allowed for now
		char ch = symbols.words[0];
		return strchr(value.c_str(), ch) != 0;
	}
	return false;
}

IndentationStatus SciTEBase::GetIndentState(int line) {
	// C like language indentation defined by braces and keywords
	IndentationStatus indentState = isNone;
	SString controlWords[20];
	unsigned int parts = GetLinePartsInStyle(line, statementIndent.styleNumber,
	        -1, controlWords, ELEMENTS(controlWords));
	unsigned int i;
	for (i = 0; i < parts; i++) {
		if (includes(statementIndent, controlWords[i]))
			indentState = isKeyWordStart;
	}
	parts = GetLinePartsInStyle(line, statementEnd.styleNumber,
	        -1, controlWords, ELEMENTS(controlWords));
	for (i = 0; i < parts; i++) {
		if (includes(statementEnd, controlWords[i]))
			indentState = isNone;
	}
	// Braces override keywords
	SString controlStrings[20];
	parts = GetLinePartsInStyle(line, blockEnd.styleNumber,
	        -1, controlStrings, ELEMENTS(controlStrings));
	for (unsigned int j = 0; j < parts; j++) {
		if (includes(blockEnd, controlStrings[j]))
			indentState = isBlockEnd;
		if (includes(blockStart, controlStrings[j]))
			indentState = isBlockStart;
	}
	return indentState;
}

int SciTEBase::IndentOfBlock(int line) {
	if (line < 0)
		return 0;
	int indentSize = wEditor.Call(SCI_GETINDENT);
	int indentBlock = GetLineIndentation(line);
	int backLine = line;
	IndentationStatus indentState = isNone;
	if (statementIndent.IsEmpty() && blockStart.IsEmpty() && blockEnd.IsEmpty())
		indentState = isBlockStart;	// Don't bother searching backwards

	int lineLimit = line - statementLookback;
	if (lineLimit < 0)
		lineLimit = 0;
	while ((backLine >= lineLimit) && (indentState == 0)) {
		indentState = GetIndentState(backLine);
		if (indentState != 0) {
			indentBlock = GetLineIndentation(backLine);
			if (indentState == isBlockStart) {
				if (!indentOpening)
					indentBlock += indentSize;
			}
			if (indentState == isBlockEnd) {
				if (indentClosing)
					indentBlock -= indentSize;
				if (indentBlock < 0)
					indentBlock = 0;
			}
			if ((indentState == isKeyWordStart) && (backLine == line))
				indentBlock += indentSize;
		}
		backLine--;
	}
	return indentBlock;
}

void SciTEBase::MaintainIndentation(char ch) {
	int eolMode = wEditor.Call(SCI_GETEOLMODE);
	int curLine = GetCurrentLineNumber();
	int lastLine = curLine - 1;

	if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && ch == '\n') ||
	        (eolMode == SC_EOL_CR && ch == '\r')) {
		if (props.GetInt("indent.automatic")) {
			while (lastLine >= 0 && GetLineLength(lastLine) == 0)
				lastLine--;
		}
		int indentAmount = 0;
		if (lastLine >= 0) {
			indentAmount = GetLineIndentation(lastLine);
		}
		if (indentAmount > 0) {
			SetLineIndentation(curLine, indentAmount);
		}
	}
}

void SciTEBase::AutomaticIndentation(char ch) {
	Sci_CharacterRange crange = GetSelection();
	int selStart = static_cast<int>(crange.cpMin);
	int curLine = GetCurrentLineNumber();
	int thisLineStart = wEditor.Call(SCI_POSITIONFROMLINE, curLine);
	int indentSize = wEditor.Call(SCI_GETINDENT);
	int indentBlock = IndentOfBlock(curLine - 1);

	if ((wEditor.Call(SCI_GETLEXER) == SCLEX_PYTHON) &&
			(props.GetInt("indent.python.colon") == 1)) {
		int eolMode = wEditor.Call(SCI_GETEOLMODE);
		int eolChar = (eolMode == SC_EOL_CR ? '\r' : '\n');
		int eolChars = (eolMode == SC_EOL_CRLF ? 2 : 1);
		int prevLineStart = wEditor.Call(SCI_POSITIONFROMLINE, curLine - 1);
		int prevIndentPos = GetLineIndentPosition(curLine - 1);
		int indentExisting = GetLineIndentation(curLine);

		if (ch == eolChar) {
			// Find last noncomment, nonwhitespace character on previous line
			int character = 0;
			int style = 0;
			for (int p = selStart - eolChars - 1; p > prevLineStart; p--) {
				style = wEditor.Call(SCI_GETSTYLEAT, p);
				if (style != SCE_P_DEFAULT && style != SCE_P_COMMENTLINE &&
						style != SCE_P_COMMENTBLOCK) {
					character = wEditor.Call(SCI_GETCHARAT, p);
					break;
				}
			}
			indentBlock = GetLineIndentation(curLine - 1);
			if (style == SCE_P_OPERATOR && character == ':') {
				SetLineIndentation(curLine, indentBlock + indentSize);
			} else if (selStart == prevIndentPos + eolChars) {
				// Preserve the indentation of preexisting text beyond the caret
				SetLineIndentation(curLine, indentBlock + indentExisting);
			} else {
				SetLineIndentation(curLine, indentBlock);
			}
		}
		return;
	}

	if (blockEnd.IsSingleChar() && ch == blockEnd.words[0]) {	// Dedent maybe
		if (!indentClosing) {
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				SetLineIndentation(curLine, indentBlock - indentSize);
			}
		}
	} else if (!blockEnd.IsSingleChar() && (ch == ' ')) {	// Dedent maybe
		if (!indentClosing && (GetIndentState(curLine) == isBlockEnd)) {}
	} else if (blockStart.IsSingleChar() && (ch == blockStart.words[0])) {
		// Dedent maybe if first on line and previous line was starting keyword
		if (!indentOpening && (GetIndentState(curLine - 1) == isKeyWordStart)) {
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				SetLineIndentation(curLine, indentBlock - indentSize);
			}
		}
	} else if ((ch == '\r' || ch == '\n') && (selStart == thisLineStart)) {
		if (!indentClosing && !blockEnd.IsSingleChar()) {	// Dedent previous line maybe
			SString controlWords[1];
			if (GetLinePartsInStyle(curLine - 1, blockEnd.styleNumber,
			        -1, controlWords, ELEMENTS(controlWords))) {
				if (includes(blockEnd, controlWords[0])) {
					// Check if first keyword on line is an ender
					SetLineIndentation(curLine - 1, IndentOfBlock(curLine - 2) - indentSize);
					// Recalculate as may have changed previous line
					indentBlock = IndentOfBlock(curLine - 1);
				}
			}
		}
		SetLineIndentation(curLine, indentBlock);
	}
}

/**
 * Upon a character being added, SciTE may decide to perform some action
 * such as displaying a completion list or auto-indentation.
 */
void SciTEBase::CharAdded(char ch) {
	if (recording)
		return;
	Sci_CharacterRange crange = GetSelection();
	int selStart = static_cast<int>(crange.cpMin);
	int selEnd = static_cast<int>(crange.cpMax);
	if ((selEnd == selStart) && (selStart > 0)) {
		if (wEditor.Call(SCI_CALLTIPACTIVE)) {
			if (calltipParametersEnd.contains(ch)) {
				braceCount--;
				if (braceCount < 1)
					wEditor.Call(SCI_CALLTIPCANCEL);
				else
					StartCallTip();
			} else if (calltipParametersStart.contains(ch)) {
				braceCount++;
				StartCallTip();
			} else {
				ContinueCallTip();
			}
		} else if (wEditor.Call(SCI_AUTOCACTIVE)) {
			if (calltipParametersStart.contains(ch)) {
				braceCount++;
				StartCallTip();
			} else if (calltipParametersEnd.contains(ch)) {
				braceCount--;
			} else if (!wordCharacters.contains(ch)) {
				wEditor.Call(SCI_AUTOCCANCEL);
				if (autoCompleteStartCharacters.contains(ch)) {
					StartAutoComplete();
				}
			} else if (autoCCausedByOnlyOne) {
				StartAutoCompleteWord(true);
			}
		} else if (HandleXml(ch)) {
			// Handled in the routine
		} else {
			if (calltipParametersStart.contains(ch)) {
				braceCount = 1;
				StartCallTip();
			} else {
				autoCCausedByOnlyOne = false;
				if (indentMaintain)
					MaintainIndentation(ch);
				else if (props.GetInt("indent.automatic"))
					AutomaticIndentation(ch);
				if (autoCompleteStartCharacters.contains(ch)) {
					StartAutoComplete();
				} else if (props.GetInt("autocompleteword.automatic") && wordCharacters.contains(ch)) {
					StartAutoCompleteWord(true);
					autoCCausedByOnlyOne = wEditor.Call(SCI_AUTOCACTIVE);
				}
			}
		}
	}
}

/**
 * Upon a character being added to the output, SciTE may decide to perform some action
 * such as displaying a completion list or running a shell command.
 */
void SciTEBase::CharAddedOutput(int ch) {
	if (ch == '\n') {
		NewLineInOutput();
	} else if (ch == '(') {
		// Potential autocompletion of symbols when $( typed
		int selStart = wOutput.Call(SCI_GETSELECTIONSTART);
		if ((selStart > 1) && (wOutput.Call(SCI_GETCHARAT, selStart - 2, 0) == '$')) {
			SString symbols;
			const char *key = NULL;
			const char *val = NULL;
			bool b = props.GetFirst(key, val);
			while (b) {
				symbols.append(key);
				symbols.append(") ");
				b = props.GetNext(key, val);
			}
			StringList symList;
			symList.Set(symbols.c_str());
			char *words = symList.GetNearestWords("", 0, true);
			if (words) {
				wEditor.Call(SCI_AUTOCSETSEPARATOR, ' ');
				wOutput.CallString(SCI_AUTOCSHOW, 0, words);
				delete []words;
			}
		}
	}
}

/**
 * This routine will auto complete XML or HTML tags that are still open by closing them
 * @parm ch The characer we are dealing with, currently only works with the '>' character
 * @return True if handled, false otherwise
 */
bool SciTEBase::HandleXml(char ch) {
	// We're looking for this char
	// Quit quickly if not found
	if (ch != '>') {
		return false;
	}

	// This may make sense only in certain languages
	if (lexLanguage != SCLEX_HTML && lexLanguage != SCLEX_XML) {
		return false;
	}

	// If the user has turned us off, quit now.
	// Default is off
	SString value = props.GetExpanded("xml.auto.close.tags");
	if ((value.length() == 0) || (value == "0")) {
		return false;
	}

	// Grab the last 512 characters or so
	int nCaret = wEditor.Call(SCI_GETCURRENTPOS);
	char sel[512];
	int nMin = nCaret - (sizeof(sel) - 1);
	if (nMin < 0) {
		nMin = 0;
	}

	if (nCaret - nMin < 3) {
		return false; // Smallest tag is 3 characters ex. <p>
	}
	GetRange(wEditor, nMin, nCaret, sel);
	sel[sizeof(sel) - 1] = '\0';

	if (sel[nCaret - nMin - 2] == '/') {
		// User typed something like "<br/>"
		return false;
	}

	if (sel[nCaret - nMin - 2] == '-') {
		// User typed something like "<a $this->"
		return false;
	}

	SString strFound = FindOpenXmlTag(sel, nCaret - nMin);

	if (strFound.length() > 0) {
		wEditor.Call(SCI_BEGINUNDOACTION);
		SString toInsert = "</";
		toInsert += strFound;
		toInsert += ">";
		wEditor.CallString(SCI_REPLACESEL, 0, toInsert.c_str());
		SetSelection(nCaret, nCaret);
		wEditor.Call(SCI_ENDUNDOACTION);
		return true;
	}

	return false;
}

/** Search backward through nSize bytes looking for a '<', then return the tag if any
 * @return The tag name
 */
SString SciTEBase::FindOpenXmlTag(const char sel[], int nSize) {
	SString strRet = "";

	if (nSize < 3) {
		// Smallest tag is "<p>" which is 3 characters
		return strRet;
	}
	const char *pBegin = &sel[0];
	const char *pCur = &sel[nSize - 1];

	pCur--; // Skip past the >
	while (pCur > pBegin) {
		if (*pCur == '<') {
			break;
		} else if (*pCur == '>') {
			if (*(pCur - 1) != '-') {
				break;
			}
		}
		--pCur;
	}

	if (*pCur == '<') {
		pCur++;
		while (strchr(":_-.", *pCur) || isalnum(*pCur)) {
			strRet += *pCur;
			pCur++;
		}
	}

	// Return the tag name or ""
	return strRet;
}

void SciTEBase::GoMatchingBrace(bool select) {
	int braceAtCaret = -1;
	int braceOpposite = -1;
	GUI::ScintillaWindow &wCurrent = wOutput.HasFocus() ? wOutput : wEditor;
	bool isInside = FindMatchingBracePosition(!wOutput.HasFocus(), braceAtCaret, braceOpposite, true);
	// Convert the character positions into caret positions based on whether
	// the caret position was inside or outside the braces.
	if (isInside) {
		if (braceOpposite > braceAtCaret) {
			braceAtCaret++;
		} else {
			braceOpposite++;
		}
	} else {    // Outside
		if (braceOpposite > braceAtCaret) {
			braceOpposite++;
		} else {
			braceAtCaret++;
		}
	}
	if (braceOpposite >= 0) {
		EnsureRangeVisible(wCurrent, braceOpposite, braceOpposite);
		if (select) {
			wCurrent.Call(SCI_SETSEL, braceAtCaret, braceOpposite);
		} else {
			wCurrent.Call(SCI_SETSEL, braceOpposite, braceOpposite);
		}
	}
}

// Text	ConditionalUp	Ctrl+J	Finds the previous matching preprocessor condition
// Text	ConditionalDown	Ctrl+K	Finds the next matching preprocessor condition
void SciTEBase::GoMatchingPreprocCond(int direction, bool select) {
	int mppcAtCaret = wEditor.Call(SCI_GETCURRENTPOS);
	int mppcMatch = -1;
	int forward = (direction == IDM_NEXTMATCHPPC);
	bool isInside = FindMatchingPreprocCondPosition(forward, mppcAtCaret, mppcMatch);

	if (isInside && mppcMatch >= 0) {
		EnsureRangeVisible(wEditor, mppcMatch, mppcMatch);
		if (select) {
			// Selection changes the rules a bit...
			int selStart = wEditor.Call(SCI_GETSELECTIONSTART);
			int selEnd = wEditor.Call(SCI_GETSELECTIONEND);
			// pivot isn't the caret position but the opposite (if there is a selection)
			int pivot = (mppcAtCaret == selStart ? selEnd : selStart);
			if (forward) {
				// Caret goes one line beyond the target, to allow selecting the whole line
				int lineNb = wEditor.Call(SCI_LINEFROMPOSITION, mppcMatch);
				mppcMatch = wEditor.Call(SCI_POSITIONFROMLINE, lineNb + 1);
			}
			SetSelection(pivot, mppcMatch);
		} else {
			SetSelection(mppcMatch, mppcMatch);
		}
	} else {
		WarnUser(warnNotFound);
	}
}

void SciTEBase::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, const SString &input, int flags) {
	// If no explicit directory, use the directory of the current file
	FilePath directoryRun;
	if (dir.length()) {
		FilePath directoryExplicit(GUI::StringFromUTF8(dir.c_str()));
		if (directoryExplicit.IsAbsolute()) {
			directoryRun = directoryExplicit;
		} else {
			// Relative paths are relative to the current file
			directoryRun = FilePath(filePath.Directory(), directoryExplicit).NormalizePath();
		}
	} else {
		directoryRun = filePath.Directory();
	}
	jobQueue.AddCommand(cmd, directoryRun, jobType, input, flags);
}

int ControlIDOfCommand(unsigned long wParam) {
	return wParam & 0xffff;
}

void WindowSetFocus(GUI::ScintillaWindow &w) {
	w.Send(SCI_GRABFOCUS, 0, 0);
}

void SciTEBase::SetLineNumberWidth() {
	if (lineNumbers) {
		int lineNumWidth = lineNumbersWidth;

		if (lineNumbersExpand) {
			// The margin size will be expanded if the current buffer's maximum
			// line number would overflow the margin.

			int lineCount = wEditor.Call(SCI_GETLINECOUNT);

			lineNumWidth = 1;
			while (lineCount >= 10) {
				lineCount /= 10;
				++lineNumWidth;
			}

			if (lineNumWidth < lineNumbersWidth) {
				lineNumWidth = lineNumbersWidth;
			}
		}

		// The 4 here allows for spacing: 1 pixel on left and 3 on right.
		std::string nNines(lineNumWidth, '9');
		int pixelWidth = 4 + wEditor.CallString(SCI_TEXTWIDTH, STYLE_LINENUMBER, nNines.c_str());

		wEditor.Call(SCI_SETMARGINWIDTHN, 0, pixelWidth);
	} else {
		wEditor.Call(SCI_SETMARGINWIDTHN, 0, 0);
	}
}

void SciTEBase::MenuCommand(int cmdID, int source) {
	switch (cmdID) {
	case IDM_NEW:
		// For the New command, the "are you sure" question is always asked as this gives
		// an opportunity to abandon the edits made to a file when are.you.sure is turned off.
		if (CanMakeRoom()) {
			New();
			ReadProperties();
			SetIndentSettings();
			SetEol();
			UpdateStatusBar(true);
			WindowSetFocus(wEditor);
		}
		break;
	case IDM_OPEN:
		// No need to see if can make room as that will occur
		// when doing the opening. Must be done there as user
		// may decide to open multiple files so do not know yet
		// how much room needed.
		OpenDialog(filePath.Directory(), GUI::StringFromUTF8(props.GetExpanded("open.filter").c_str()).c_str());
		WindowSetFocus(wEditor);
		break;
	case IDM_OPENSELECTED:
		if (OpenSelected())
			WindowSetFocus(wEditor);
		break;
	case IDM_REVERT:
		Revert();
		WindowSetFocus(wEditor);
		break;
	case IDM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			WindowSetFocus(wEditor);
		}
		break;
	case IDM_CLOSEALL:
		CloseAllBuffers();
		break;
	case IDM_SAVE:
		Save();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEALL:
		SaveAllBuffers(false, true);
		break;
	case IDM_SAVEAS:
		SaveAsDialog();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEACOPY:
		SaveACopy();
		WindowSetFocus(wEditor);
		break;
	case IDM_COPYPATH:
		CopyPath();
		break;
	case IDM_SAVEASHTML:
		SaveAsHTML();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEASRTF:
		SaveAsRTF();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEASPDF:
		SaveAsPDF();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEASTEX:
		SaveAsTEX();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVEASXML:
		SaveAsXML();
		WindowSetFocus(wEditor);
		break;
	case IDM_PRINT:
		Print(true);
		break;
	case IDM_PRINTSETUP:
		PrintSetup();
		break;
	case IDM_LOADSESSION:
		LoadSessionDialog();
		WindowSetFocus(wEditor);
		break;
	case IDM_SAVESESSION:
		SaveSessionDialog();
		WindowSetFocus(wEditor);
		break;
	case IDM_ABOUT:
		AboutDialog();
		break;
	case IDM_QUIT:
		QuitProgram();
		break;
	case IDM_ENCODING_DEFAULT:
	case IDM_ENCODING_UCS2BE:
	case IDM_ENCODING_UCS2LE:
	case IDM_ENCODING_UTF8:
	case IDM_ENCODING_UCOOKIE:
		CurrentBuffer()->unicodeMode = static_cast<UniMode>(cmdID - IDM_ENCODING_DEFAULT);
		if (CurrentBuffer()->unicodeMode != uni8Bit) {
			// Override the code page if Unicode
			codePage = SC_CP_UTF8;
		} else {
			codePage = props.GetInt("code.page");
		}
		wEditor.Call(SCI_SETCODEPAGE, codePage);
		break;

	case IDM_NEXTFILESTACK:
		if (buffers.size > 1 && props.GetInt("buffers.zorder.switching")) {
			NextInStack(); // next most recently selected buffer
			WindowSetFocus(wEditor);
			break;
		}
		// else fall through and do NEXTFILE behaviour...
	case IDM_NEXTFILE:
		if (buffers.size > 1) {
			Next(); // Use Next to tabs move left-to-right
			WindowSetFocus(wEditor);
		} else {
			// Not using buffers - switch to next file on MRU
			StackMenuNext();
		}
		break;

	case IDM_PREVFILESTACK:
		if (buffers.size > 1 && props.GetInt("buffers.zorder.switching")) {
			PrevInStack(); // next least recently selected buffer
			WindowSetFocus(wEditor);
			break;
		}
		// else fall through and do PREVFILE behaviour...
	case IDM_PREVFILE:
		if (buffers.size > 1) {
			Prev(); // Use Prev to tabs move right-to-left
			WindowSetFocus(wEditor);
		} else {
			// Not using buffers - switch to previous file on MRU
			StackMenuPrev();
		}
		break;

	case IDM_MOVETABRIGHT:
		MoveTabRight();
		WindowSetFocus(wEditor);
		break;
	case IDM_MOVETABLEFT:
		MoveTabLeft();
		WindowSetFocus(wEditor);
		break;

	case IDM_UNDO:
		CallPane(source, SCI_UNDO);
		CheckMenus();
		break;
	case IDM_REDO:
		CallPane(source, SCI_REDO);
		CheckMenus();
		break;

	case IDM_CUT:
		if (CallPane(source, SCI_GETSELECTIONSTART) != CallPane(source, SCI_GETSELECTIONEND)) {
			CallPane(source, SCI_CUT);
		}
		break;
	case IDM_COPY:
		if (CallPane(source, SCI_GETSELECTIONSTART) != CallPane(source, SCI_GETSELECTIONEND)) {
			//fprintf(stderr, "Copy from %d\n", source);
			CallPane(source, SCI_COPY);
		}
		// does not trigger SCN_UPDATEUI, so do CheckMenusClipboard() here
		CheckMenusClipboard();
		break;
	case IDM_PASTE:
		CallPane(source, SCI_PASTE);
		break;
	case IDM_DUPLICATE:
		CallPane(source, SCI_SELECTIONDUPLICATE);
		break;
	case IDM_PASTEANDDOWN: {
			int pos = CallFocused(SCI_GETCURRENTPOS);
			CallFocused(SCI_PASTE);
			CallFocused(SCI_SETCURRENTPOS, pos);
			CallFocused(SCI_CHARLEFT);
			CallFocused(SCI_LINEDOWN);
		}
		break;
	case IDM_CLEAR:
		CallPane(source, SCI_CLEAR);
		break;
	case IDM_SELECTALL:
		CallPane(source, SCI_SELECTALL);
		break;
	case IDM_COPYASRTF:
		CopyAsRTF();
		break;

	case IDM_FIND:
		Find();
		break;

	case IDM_INCSEARCH:
		IncrementSearchMode();
		break;

	case IDM_FINDNEXT:
		FindNext(reverseFind);
		break;

	case IDM_FINDNEXTBACK:
		FindNext(!reverseFind);
		break;

	case IDM_FINDNEXTSEL:
		SelectionIntoFind();
		FindNext(reverseFind, true, false);
		break;

	case IDM_ENTERSELECTION:
		SelectionIntoFind();
		break;

	case IDM_FINDNEXTBACKSEL:
		SelectionIntoFind();
		FindNext(!reverseFind, true, false);
		break;

	case IDM_FINDINFILES:
		FindInFiles();
		break;

	case IDM_REPLACE:
		Replace();
		break;

	case IDM_GOTO:
		GoLineDialog();
		break;

	case IDM_MATCHBRACE:
		GoMatchingBrace(false);
		break;

	case IDM_SELECTTOBRACE:
		GoMatchingBrace(true);
		break;

	case IDM_PREVMATCHPPC:
		GoMatchingPreprocCond(IDM_PREVMATCHPPC, false);
		break;

	case IDM_SELECTTOPREVMATCHPPC:
		GoMatchingPreprocCond(IDM_PREVMATCHPPC, true);
		break;

	case IDM_NEXTMATCHPPC:
		GoMatchingPreprocCond(IDM_NEXTMATCHPPC, false);
		break;

	case IDM_SELECTTONEXTMATCHPPC:
		GoMatchingPreprocCond(IDM_NEXTMATCHPPC, true);
		break;
	case IDM_SHOWCALLTIP:
		if (wEditor.Call(SCI_CALLTIPACTIVE)) {
			currentCallTip = (currentCallTip + 1 == maxCallTips) ? 0 : currentCallTip + 1;
			FillFunctionDefinition();
		} else {
			StartCallTip();
		}
		break;
	case IDM_COMPLETE:
		autoCCausedByOnlyOne = false;
		StartAutoComplete();
		break;

	case IDM_COMPLETEWORD:
		autoCCausedByOnlyOne = false;
		StartAutoCompleteWord(false);
		break;

	case IDM_ABBREV:
		wEditor.Call(SCI_CANCEL);
		StartExpandAbbreviation();
		break;

	case IDM_INS_ABBREV:
		wEditor.Call(SCI_CANCEL);
		StartInsertAbbreviation();
		break;

	case IDM_BLOCK_COMMENT:
		StartBlockComment();
		break;

	case IDM_BOX_COMMENT:
		StartBoxComment();
		break;

	case IDM_STREAM_COMMENT:
		StartStreamComment();
		break;

	case IDM_TOGGLE_FOLDALL:
		FoldAll();
		break;

	case IDM_UPRCASE:
		CallFocused(SCI_UPPERCASE);
		break;

	case IDM_LWRCASE:
		CallFocused(SCI_LOWERCASE);
		break;

	case IDM_JOIN:
		CallFocused(SCI_TARGETFROMSELECTION);
		CallFocused(SCI_LINESJOIN);
		break;

	case IDM_SPLIT:
		CallFocused(SCI_TARGETFROMSELECTION);
		CallFocused(SCI_LINESSPLIT);
		break;

	case IDM_EXPAND:
		wEditor.Call(SCI_TOGGLEFOLD, GetCurrentLineNumber());
		break;

	case IDM_TOGGLE_FOLDRECURSIVE: {
			int line = GetCurrentLineNumber();
			int level = wEditor.Call(SCI_GETFOLDLEVEL, line);
			ToggleFoldRecursive(line, level);
		}
		break;

	case IDM_EXPAND_ENSURECHILDRENVISIBLE: {
			int line = GetCurrentLineNumber();
			int level = wEditor.Call(SCI_GETFOLDLEVEL, line);
			EnsureAllChildrenVisible(line, level);
		}
		break;

	case IDM_SPLITVERTICAL:
		{
			GUI::Rectangle rcClient = GetClientRectangle();
			heightOutput = splitVertical ?
				int((double)heightOutput * rcClient.Height() / rcClient.Width() + 0.5) : 
				int((double)heightOutput * rcClient.Width() / rcClient.Height() + 0.5);
			previousHeightOutput = splitVertical ?
				int((double)previousHeightOutput * rcClient.Height() / rcClient.Width() + 0.5) : 
				int((double)previousHeightOutput * rcClient.Width() / rcClient.Height() + 0.5);
		}
		splitVertical = !splitVertical;
		heightOutput = NormaliseSplit(heightOutput);
		SizeSubWindows();
		CheckMenus();
		Redraw();
		break;

	case IDM_LINENUMBERMARGIN:
		lineNumbers = !lineNumbers;
		SetLineNumberWidth();
		CheckMenus();
		break;

	case IDM_SELMARGIN:
		margin = !margin;
		wEditor.Call(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);
		CheckMenus();
		break;

	case IDM_FOLDMARGIN:
		foldMargin = !foldMargin;
		wEditor.Call(SCI_SETMARGINWIDTHN, 2, foldMargin ? foldMarginWidth : 0);
		CheckMenus();
		break;

	case IDM_VIEWEOL:
		wEditor.Call(SCI_SETVIEWEOL, !wEditor.Call(SCI_GETVIEWEOL));
		CheckMenus();
		break;

	case IDM_VIEWTOOLBAR:
		tbVisible = !tbVisible;
		ShowToolBar();
		CheckMenus();
		break;

	case IDM_TOGGLEOUTPUT:
		ToggleOutputVisible();
		CheckMenus();
		break;

	case IDM_TOGGLEPARAMETERS:
		ParametersDialog(false);
		CheckMenus();
		break;

	case IDM_WRAP:
		wrap = !wrap;
		wEditor.Call(SCI_SETWRAPMODE, wrap ? wrapStyle : SC_WRAP_NONE);
		CheckMenus();
		break;

	case IDM_WRAPOUTPUT:
		wrapOutput = !wrapOutput;
		wOutput.Call(SCI_SETWRAPMODE, wrapOutput ? wrapStyle : SC_WRAP_NONE);
		CheckMenus();
		break;

	case IDM_READONLY:
		isReadOnly = !isReadOnly;
		CurrentBuffer()->isReadOnly = false;
		wEditor.Call(SCI_SETREADONLY, isReadOnly);
		UpdateStatusBar(true);
		CheckMenus();
		break;

	case IDM_VIEWTABBAR:
		tabVisible = !tabVisible;
		ShowTabBar();
		CheckMenus();
		break;

	case IDM_VIEWSTATUSBAR:
		sbVisible = !sbVisible;
		ShowStatusBar();
		UpdateStatusBar(true);
		CheckMenus();
		break;

	case IDM_CLEAROUTPUT:
		wOutput.Send(SCI_CLEARALL);
		break;

	case IDM_SWITCHPANE:
		if (wEditor.HasFocus())
			WindowSetFocus(wOutput);
		else
			WindowSetFocus(wEditor);
		break;

	case IDM_EOL_CRLF:
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CRLF);
		CheckMenus();
		UpdateStatusBar(false);
		break;

	case IDM_EOL_CR:
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CR);
		CheckMenus();
		UpdateStatusBar(false);
		break;
	case IDM_EOL_LF:
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_LF);
		CheckMenus();
		UpdateStatusBar(false);
		break;
	case IDM_EOL_CONVERT:
		wEditor.Call(SCI_CONVERTEOLS, wEditor.Call(SCI_GETEOLMODE));
		break;

	case IDM_VIEWSPACE:
		ViewWhitespace(!wEditor.Call(SCI_GETVIEWWS));
		CheckMenus();
		Redraw();
		break;

	case IDM_VIEWGUIDES: {
			bool viewIG = wEditor.Call(SCI_GETINDENTATIONGUIDES, 0, 0) == 0;
			wEditor.Call(SCI_SETINDENTATIONGUIDES, viewIG ? indentExamine : SC_IV_NONE);
			CheckMenus();
			Redraw();
		}
		break;

	case IDM_COMPILE: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(props.GetWild("command.compile.", FileNameExt().AsUTF8().c_str()), "",
				        SubsystemType("command.compile.subsystem."));
				if (jobQueue.HasCommandToRun())
					Execute();
			}
		}
		break;

	case IDM_BUILD: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(
				    props.GetWild("command.build.", FileNameExt().AsUTF8().c_str()),
				    props.GetNewExpand("command.build.directory.", FileNameExt().AsUTF8().c_str()),
				    SubsystemType("command.build.subsystem."));
				if (jobQueue.HasCommandToRun()) {
					jobQueue.isBuilding = true;
					Execute();
				}
			}
		}
		break;

	case IDM_GO: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				int flags = 0;

				if (!jobQueue.isBuilt) {
					SString buildcmd = props.GetNewExpand("command.go.needs.", FileNameExt().AsUTF8().c_str());
					AddCommand(buildcmd, "",
					        SubsystemType("command.go.needs.subsystem."));
					if (buildcmd.length() > 0) {
						jobQueue.isBuilding = true;
						flags |= jobForceQueue;
					}
				}
				AddCommand(props.GetWild("command.go.", FileNameExt().AsUTF8().c_str()), "",
				        SubsystemType("command.go.subsystem."), "", flags);
				if (jobQueue.HasCommandToRun())
					Execute();
			}
		}
		break;

	case IDM_STOPEXECUTE:
		StopExecute();
		break;

	case IDM_NEXTMSG:
		GoMessage(1);
		break;

	case IDM_PREVMSG:
		GoMessage(-1);
		break;

	case IDM_OPENLOCALPROPERTIES:
		OpenProperties(IDM_OPENLOCALPROPERTIES);
		WindowSetFocus(wEditor);
		break;

	case IDM_OPENUSERPROPERTIES:
		OpenProperties(IDM_OPENUSERPROPERTIES);
		WindowSetFocus(wEditor);
		break;

	case IDM_OPENGLOBALPROPERTIES:
		OpenProperties(IDM_OPENGLOBALPROPERTIES);
		WindowSetFocus(wEditor);
		break;

	case IDM_OPENABBREVPROPERTIES:
		OpenProperties(IDM_OPENABBREVPROPERTIES);
		WindowSetFocus(wEditor);
		break;

	case IDM_OPENLUAEXTERNALFILE:
		OpenProperties(IDM_OPENLUAEXTERNALFILE);
		WindowSetFocus(wEditor);
		break;

	case IDM_OPENDIRECTORYPROPERTIES:
		OpenProperties(IDM_OPENDIRECTORYPROPERTIES);
		WindowSetFocus(wEditor);
		break;

	case IDM_SRCWIN:
		break;

	case IDM_BOOKMARK_TOGGLE:
		BookmarkToggle();
		break;

	case IDM_BOOKMARK_NEXT:
		BookmarkNext(true);
		break;

	case IDM_BOOKMARK_PREV:
		BookmarkNext(false);
		break;

	case IDM_BOOKMARK_NEXT_SELECT:
		BookmarkNext(true, true);
		break;

	case IDM_BOOKMARK_PREV_SELECT:
		BookmarkNext(false, true);
		break;

	case IDM_BOOKMARK_CLEARALL:
		wEditor.Call(SCI_MARKERDELETEALL, markerBookmark);
		RemoveFindMarks();
		break;

	case IDM_TABSIZE:
		TabSizeDialog();
		break;

	case IDM_MONOFONT:
		CurrentBuffer()->useMonoFont = !CurrentBuffer()->useMonoFont;
		ReadFontProperties();
		Redraw();
		break;

	case IDM_MACROLIST:
		AskMacroList();
		break;
	case IDM_MACROPLAY:
		StartPlayMacro();
		break;
	case IDM_MACRORECORD:
		StartRecordMacro();
		break;
	case IDM_MACROSTOPRECORD:
		StopRecordMacro();
		break;

	case IDM_HELP: {
			SelectionIntoProperties();
			AddCommand(props.GetWild("command.help.", FileNameExt().AsUTF8().c_str()), "",
			        SubsystemType("command.help.subsystem."));
			if (jobQueue.HasCommandToRun()) {
				jobQueue.isBuilding = true;
				Execute();
			}
		}
		break;

	case IDM_HELP_SCITE: {
			SelectionIntoProperties();
			AddCommand(props.Get("command.scite.help"), "",
			        SubsystemType(props.Get("command.scite.help.subsystem")[0]));
			if (jobQueue.HasCommandToRun()) {
				jobQueue.isBuilding = true;
				Execute();
			}
		}
		break;

	default:
		if ((cmdID >= bufferCmdID) &&
		        (cmdID < bufferCmdID + buffers.size)) {
			SetDocumentAt(cmdID - bufferCmdID);
			CheckReload();
		} else if ((cmdID >= fileStackCmdID) &&
		        (cmdID < fileStackCmdID + fileStackMax)) {
			StackMenu(cmdID - fileStackCmdID);
		} else if (cmdID >= importCmdID &&
		        (cmdID < importCmdID + importMax)) {
			ImportMenu(cmdID - importCmdID);
		} else if (cmdID >= IDM_TOOLS && cmdID < IDM_TOOLS + toolMax) {
			ToolsMenu(cmdID - IDM_TOOLS);
		} else if (cmdID >= IDM_LANGUAGE && cmdID < IDM_LANGUAGE + 100) {
			SetOverrideLanguage(cmdID - IDM_LANGUAGE);
		}
		break;
	}
}

void SciTEBase::FoldChanged(int line, int levelNow, int levelPrev) {
	if (levelNow & SC_FOLDLEVELHEADERFLAG) {
		if (!(levelPrev & SC_FOLDLEVELHEADERFLAG)) {
			// Adding a fold point.
			wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
			if (!wEditor.Call(SCI_GETALLLINESVISIBLE))
				Expand(line, true, false, 0, levelPrev);
		}
	} else if (levelPrev & SC_FOLDLEVELHEADERFLAG) {
		if (!wEditor.Call(SCI_GETFOLDEXPANDED, line)) {
			// Removing the fold from one that has been contracted so should expand
			// otherwise lines are left invisible with no way to make them visible
			wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
			if (!wEditor.Call(SCI_GETALLLINESVISIBLE))
				Expand(line, true, false, 0, levelPrev);
		}
	}
	if (!(levelNow & SC_FOLDLEVELWHITEFLAG) &&
	        ((levelPrev & SC_FOLDLEVELNUMBERMASK) > (levelNow & SC_FOLDLEVELNUMBERMASK))) {
		if (!wEditor.Call(SCI_GETALLLINESVISIBLE)) {
			// See if should still be hidden
			int parentLine = wEditor.Call(SCI_GETFOLDPARENT, line);
			if (parentLine < 0) {
				wEditor.Call(SCI_SHOWLINES, line, line);
			} else if (wEditor.Call(SCI_GETFOLDEXPANDED, parentLine) && wEditor.Call(SCI_GETLINEVISIBLE, parentLine)) {
				wEditor.Call(SCI_SHOWLINES, line, line);
			}
		}
	}
}

void SciTEBase::Expand(int &line, bool doExpand, bool force, int visLevels, int level) {
	int lineMaxSubord = wEditor.Call(SCI_GETLASTCHILD, line, level & SC_FOLDLEVELNUMBERMASK);
	line++;
	while (line <= lineMaxSubord) {
		if (force) {
			if (visLevels > 0)
				wEditor.Call(SCI_SHOWLINES, line, line);
			else
				wEditor.Call(SCI_HIDELINES, line, line);
		} else {
			if (doExpand)
				wEditor.Call(SCI_SHOWLINES, line, line);
		}
		int levelLine = level;
		if (levelLine == -1)
			levelLine = wEditor.Call(SCI_GETFOLDLEVEL, line);
		if (levelLine & SC_FOLDLEVELHEADERFLAG) {
			if (force) {
				if (visLevels > 1)
					wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
				else
					wEditor.Call(SCI_SETFOLDEXPANDED, line, 0);
				Expand(line, doExpand, force, visLevels - 1);
			} else {
				if (doExpand) {
					if (!wEditor.Call(SCI_GETFOLDEXPANDED, line))
						wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
					Expand(line, true, force, visLevels - 1);
				} else {
					Expand(line, false, force, visLevels - 1);
				}
			}
		} else {
			line++;
		}
	}
}

void SciTEBase::FoldAll() {
	wEditor.Call(SCI_COLOURISE, 0, -1);
	int maxLine = wEditor.Call(SCI_GETLINECOUNT);
	bool expanding = true;
	for (int lineSeek = 0; lineSeek < maxLine; lineSeek++) {
		if (wEditor.Call(SCI_GETFOLDLEVEL, lineSeek) & SC_FOLDLEVELHEADERFLAG) {
			expanding = !wEditor.Call(SCI_GETFOLDEXPANDED, lineSeek);
			break;
		}
	}
	for (int line = 0; line < maxLine; line++) {
		int level = wEditor.Call(SCI_GETFOLDLEVEL, line);
		if ((level & SC_FOLDLEVELHEADERFLAG) &&
		        (SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK))) {
			if (expanding) {
				wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
				Expand(line, true, false, 0, level);
				line--;
			} else {
				int lineMaxSubord = wEditor.Call(SCI_GETLASTCHILD, line, -1);
				wEditor.Call(SCI_SETFOLDEXPANDED, line, 0);
				if (lineMaxSubord > line)
					wEditor.Call(SCI_HIDELINES, line + 1, lineMaxSubord);
			}
		}
	}
}

void SciTEBase::GotoLineEnsureVisible(int line) {
	wEditor.Call(SCI_ENSUREVISIBLEENFORCEPOLICY, line);
	wEditor.Call(SCI_GOTOLINE, line);
}

void SciTEBase::EnsureRangeVisible(GUI::ScintillaWindow &win, int posStart, int posEnd, bool enforcePolicy) {
	int lineStart = win.Call(SCI_LINEFROMPOSITION, Minimum(posStart, posEnd));
	int lineEnd = win.Call(SCI_LINEFROMPOSITION, Maximum(posStart, posEnd));
	for (int line = lineStart; line <= lineEnd; line++) {
		win.Call(enforcePolicy ? SCI_ENSUREVISIBLEENFORCEPOLICY : SCI_ENSUREVISIBLE, line);
	}
}

bool SciTEBase::MarginClick(int position, int modifiers) {
	int lineClick = wEditor.Call(SCI_LINEFROMPOSITION, position);
	if ((modifiers & SCMOD_SHIFT) && (modifiers & SCMOD_CTRL)) {
		FoldAll();
	} else {
		int levelClick = wEditor.Call(SCI_GETFOLDLEVEL, lineClick);
		if (levelClick & SC_FOLDLEVELHEADERFLAG) {
			if (modifiers & SCMOD_SHIFT) {
				EnsureAllChildrenVisible(lineClick, levelClick);
			} else if (modifiers & SCMOD_CTRL) {
				ToggleFoldRecursive(lineClick, levelClick);
			} else {
				// Toggle this line
				wEditor.Call(SCI_TOGGLEFOLD, lineClick);
			}
		}
	}
	return true;
}

void SciTEBase::ToggleFoldRecursive(int line, int level) {
	if (wEditor.Call(SCI_GETFOLDEXPANDED, line)) {
		// Contract this line and all children
		wEditor.Call(SCI_SETFOLDEXPANDED, line, 0);
		Expand(line, false, true, 0, level);
	} else {
		// Expand this line and all children
		wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
		Expand(line, true, true, 100, level);
	}
}

void SciTEBase::EnsureAllChildrenVisible(int line, int level) {
	// Ensure all children visible
	wEditor.Call(SCI_SETFOLDEXPANDED, line, 1);
	Expand(line, true, true, 100, level);
}

void SciTEBase::NewLineInOutput() {
	if (jobQueue.IsExecuting())
		return;
	int line = wOutput.Call(SCI_LINEFROMPOSITION,
	        wOutput.Call(SCI_GETCURRENTPOS)) - 1;
	SString cmd = GetLine(wOutput, line);
	if (cmd == ">") {
		// Search output buffer for previous command
		line--;
		while (line >= 0) {
			cmd = GetLine(wOutput, line);
			if (cmd.startswith(">") && !cmd.startswith(">Exit")) {
				cmd = cmd.substr(1);
				break;
			}
			line--;
		}
	} else if (cmd.startswith(">")) {
		cmd = cmd.substr(1);
	}
	returnOutputToCommand = false;
	AddCommand(cmd, "", jobCLI);
	Execute();
}

void SciTEBase::Notify(SCNotification *notification) {
	bool handled = false;
	switch (notification->nmhdr.code) {
	case SCN_PAINTED:
		if ((notification->nmhdr.idFrom == IDM_SRCWIN) == (wEditor.HasFocus())) {
			// Obly highlight focussed pane.
			// Manage delay before highlight when no user selection but there is word at the caret.
			// So the Delay is based on the blinking of caret, scroll...
			// If currentWordHighlight.statesOfDelay == currentWordHighlight.delay,
			// then there is word at the caret without selection, and need some delay.
			if (currentWordHighlight.statesOfDelay == currentWordHighlight.delay) {
				if (currentWordHighlight.elapsedTimes.Duration() >= 0.5) {
					currentWordHighlight.statesOfDelay = currentWordHighlight.delayJustEnded;
					HighlightCurrentWord(true);
					(wOutput.HasFocus() ? wOutput : wEditor).InvalidateAll();
				}
			}
		}
		break;
	case SCEN_SETFOCUS:
	case SCEN_KILLFOCUS:
		CheckMenus();
		break;

	case SCN_STYLENEEDED: {
			if (extender) {
				// Colourisation may be performed by script
				if ((notification->nmhdr.idFrom == IDM_SRCWIN) && (lexLanguage == SCLEX_CONTAINER)) {
					int endStyled = wEditor.Call(SCI_GETENDSTYLED);
					int lineEndStyled = wEditor.Call(SCI_LINEFROMPOSITION, endStyled);
					endStyled = wEditor.Call(SCI_POSITIONFROMLINE, lineEndStyled);
					StyleWriter styler(wEditor);
					int styleStart = 0;
					if (endStyled > 0)
						styleStart = styler.StyleAt(endStyled - 1);
					styler.SetCodePage(codePage);
					extender->OnStyle(endStyled, notification->position - endStyled,
					        styleStart, &styler);
					styler.Flush();
				}
			}
		}
		break;

	case SCN_CHARADDED:
		if (extender)
			handled = extender->OnChar(static_cast<char>(notification->ch));
		if (!handled) {
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				CharAdded(static_cast<char>(notification->ch));
			} else {
				CharAddedOutput(notification->ch);
			}
		}
		break;

	case SCN_SAVEPOINTREACHED:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointReached();
			if (!handled) {
				CurrentBuffer()->isDirty = false;
			}
		}
		CheckMenus();
		SetWindowName();
		SetBuffersMenu();
		break;

	case SCN_SAVEPOINTLEFT:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointLeft();
			if (!handled) {
				CurrentBuffer()->isDirty = true;
				jobQueue.isBuilt = false;
			}
		}
		CheckMenus();
		SetWindowName();
		SetBuffersMenu();
		break;

	case SCN_DOUBLECLICK:
		if (extender)
			handled = extender->OnDoubleClick();
		if (!handled && notification->nmhdr.idFrom == IDM_RUNWIN) {
			GoMessage(0);
		}
		break;

	case SCN_UPDATEUI:
		if (extender)
			handled = extender->OnUpdateUI();
		if (!handled) {
			BraceMatch(notification->nmhdr.idFrom == IDM_SRCWIN);
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				UpdateStatusBar(false);
			}
			CheckMenusClipboard();
		}
		if (CurrentBuffer()->findMarks == Buffer::fmModified) {
			RemoveFindMarks();
		}
		if (notification->updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT)) {
			if ((notification->nmhdr.idFrom == IDM_SRCWIN) == (wEditor.HasFocus())) {
				// Obly highlight focussed pane.
				if (notification->updated & SC_UPDATE_SELECTION)
					currentWordHighlight.statesOfDelay = currentWordHighlight.noDelay; // Selection has just been updated, so delay is disabled.
				if (currentWordHighlight.statesOfDelay != currentWordHighlight.delayJustEnded)
					HighlightCurrentWord(notification->updated != SC_UPDATE_CONTENT);
				else
					currentWordHighlight.statesOfDelay = currentWordHighlight.delayAlreadyElapsed;
			}
		}
		break;

	case SCN_MODIFIED:
		if (notification->nmhdr.idFrom == IDM_SRCWIN)
			CurrentBuffer()->DocumentModified();
		if (notification->modificationType & SC_LASTSTEPINUNDOREDO) {
			//when the user hits undo or redo, several normal insert/delete
			//notifications may fire, but we will end up here in the end
			EnableAMenuItem(IDM_UNDO, CallFocused(SCI_CANUNDO));
			EnableAMenuItem(IDM_REDO, CallFocused(SCI_CANREDO));
		} else if (notification->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
			//this will be called a lot, and usually means "typing".
			EnableAMenuItem(IDM_UNDO, true);
			EnableAMenuItem(IDM_REDO, false);
			if (CurrentBuffer()->findMarks == Buffer::fmMarked) {
				CurrentBuffer()->findMarks = Buffer::fmModified;
			}
		}

		if (notification->linesAdded && lineNumbers && lineNumbersExpand)
			SetLineNumberWidth();

		if (0 != (notification->modificationType & SC_MOD_CHANGEFOLD)) {
			FoldChanged(notification->line,
			        notification->foldLevelNow, notification->foldLevelPrev);
		}
		break;

	case SCN_MARGINCLICK: {
			if (extender)
				handled = extender->OnMarginClick();
			if (!handled) {
				if (notification->margin == 2) {
					MarginClick(notification->position, notification->modifiers);
				}
			}
		}
		break;

	case SCN_NEEDSHOWN: {
			EnsureRangeVisible(wEditor, notification->position, notification->position + notification->length, false);
		}
		break;

	case SCN_USERLISTSELECTION: {
			if (notification->wParam == 2)
				ContinueMacroList(notification->text);
			else if (extender && notification->wParam > 2)
				extender->OnUserListSelection(static_cast<int>(notification->wParam), notification->text);
		}
		break;

	case SCN_CALLTIPCLICK: {
			if (notification->position == 1 && currentCallTip > 0) {
				currentCallTip--;
				FillFunctionDefinition();
			} else if (notification->position == 2 && currentCallTip + 1 < maxCallTips) {
				currentCallTip++;
				FillFunctionDefinition();
			}
		}
		break;

	case SCN_MACRORECORD:
		RecordMacroCommand(notification);
		break;

	case SCN_URIDROPPED:
		OpenUriList(notification->text);
		break;

	case SCN_DWELLSTART:
		if (extender && (INVALID_POSITION != notification->position)) {
			int endWord = notification->position;
			SString message =
				RangeExtendAndGrab(wEditor,
					notification->position, endWord, &SciTEBase::iswordcharforsel);
			if (message.length()) {
				extender->OnDwellStart(notification->position,message.c_str());
			}
		}
		break;

	case SCN_DWELLEND:
		if (extender) {
			extender->OnDwellStart(0,""); // flags end of calltip
		}
		break;

	case SCN_ZOOM:
		SetLineNumberWidth();
		break;

	case SCN_MODIFYATTEMPTRO:
		AbandonAutomaticSave();
		break;
	}
}

void SciTEBase::CheckMenusClipboard() {
	bool hasSelection = CallFocused(SCI_GETSELECTIONSTART) != CallFocused(SCI_GETSELECTIONEND);
	EnableAMenuItem(IDM_CUT, hasSelection);
	EnableAMenuItem(IDM_COPY, hasSelection);
	EnableAMenuItem(IDM_CLEAR, hasSelection);
	EnableAMenuItem(IDM_PASTE, CallFocused(SCI_CANPASTE));
}

void SciTEBase::CheckMenus() {
	CheckMenusClipboard();
	EnableAMenuItem(IDM_UNDO, CallFocused(SCI_CANUNDO));
	EnableAMenuItem(IDM_REDO, CallFocused(SCI_CANREDO));
	EnableAMenuItem(IDM_DUPLICATE, !isReadOnly);
	EnableAMenuItem(IDM_SHOWCALLTIP, apis != 0);
	EnableAMenuItem(IDM_COMPLETE, apis != 0);
	CheckAMenuItem(IDM_SPLITVERTICAL, splitVertical);
	EnableAMenuItem(IDM_OPENFILESHERE, props.GetInt("check.if.already.open") != 0);
	CheckAMenuItem(IDM_OPENFILESHERE, openFilesHere);
	CheckAMenuItem(IDM_WRAP, wrap);
	CheckAMenuItem(IDM_WRAPOUTPUT, wrapOutput);
	CheckAMenuItem(IDM_READONLY, isReadOnly);
	CheckAMenuItem(IDM_FULLSCREEN, fullScreen);
	CheckAMenuItem(IDM_VIEWTOOLBAR, tbVisible);
	CheckAMenuItem(IDM_VIEWTABBAR, tabVisible);
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	CheckAMenuItem(IDM_VIEWEOL, wEditor.Call(SCI_GETVIEWEOL));
	CheckAMenuItem(IDM_VIEWSPACE, wEditor.Call(SCI_GETVIEWWS));
	CheckAMenuItem(IDM_VIEWGUIDES, wEditor.Call(SCI_GETINDENTATIONGUIDES));
	CheckAMenuItem(IDM_LINENUMBERMARGIN, lineNumbers);
	CheckAMenuItem(IDM_SELMARGIN, margin);
	CheckAMenuItem(IDM_FOLDMARGIN, foldMargin);
	CheckAMenuItem(IDM_TOGGLEOUTPUT, heightOutput > 0);
	CheckAMenuItem(IDM_TOGGLEPARAMETERS, ParametersOpen());
	CheckAMenuItem(IDM_MONOFONT, CurrentBuffer()->useMonoFont);
	EnableAMenuItem(IDM_COMPILE, !jobQueue.IsExecuting() &&
	        props.GetWild("command.compile.", FileNameExt().AsUTF8().c_str()).size() != 0);
	EnableAMenuItem(IDM_BUILD, !jobQueue.IsExecuting() &&
	        props.GetWild("command.build.", FileNameExt().AsUTF8().c_str()).size() != 0);
	EnableAMenuItem(IDM_GO, !jobQueue.IsExecuting() &&
	        props.GetWild("command.go.", FileNameExt().AsUTF8().c_str()).size() != 0);
	EnableAMenuItem(IDM_OPENDIRECTORYPROPERTIES, props.GetInt("properties.directory.enable") != 0);
	for (int toolItem = 0; toolItem < toolMax; toolItem++)
		EnableAMenuItem(IDM_TOOLS + toolItem, !jobQueue.IsExecuting());
	EnableAMenuItem(IDM_STOPEXECUTE, jobQueue.IsExecuting());
	if (buffers.size > 0) {
		TabSelect(buffers.Current());
		for (int bufferItem = 0; bufferItem < buffers.lengthVisible; bufferItem++) {
			CheckAMenuItem(IDM_BUFFER + bufferItem, bufferItem == buffers.Current());
		}
	}
	EnableAMenuItem(IDM_MACROPLAY, !recording);
	EnableAMenuItem(IDM_MACRORECORD, !recording);
	EnableAMenuItem(IDM_MACROSTOPRECORD, recording);
}

void SciTEBase::ContextMenu(GUI::ScintillaWindow &wSource, GUI::Point pt, GUI::Window wCmd) {
	int currentPos = wSource.Call(SCI_GETCURRENTPOS);
	int anchor = wSource.Call(SCI_GETANCHOR);
	popup.CreatePopUp();
	bool writable = !wSource.Call(SCI_GETREADONLY);
	AddToPopUp("Undo", IDM_UNDO, writable && wSource.Call(SCI_CANUNDO));
	AddToPopUp("Redo", IDM_REDO, writable && wSource.Call(SCI_CANREDO));
	AddToPopUp("");
	AddToPopUp("Cut", IDM_CUT, writable && currentPos != anchor);
	AddToPopUp("Copy", IDM_COPY, currentPos != anchor);
	AddToPopUp("Paste", IDM_PASTE, writable && wSource.Call(SCI_CANPASTE));
	AddToPopUp("Delete", IDM_CLEAR, writable && currentPos != anchor);
	AddToPopUp("");
	AddToPopUp("Select All", IDM_SELECTALL);
	AddToPopUp("");
	if (wSource.GetID() == wOutput.GetID()) {
		AddToPopUp("Hide", IDM_TOGGLEOUTPUT, true);
	} else {
		AddToPopUp("Close", IDM_CLOSE, true);
	}
	SString userContextMenu = props.GetNewExpand("user.context.menu");
	userContextMenu.substitute('|', '\0');
	const char *userContextItem = userContextMenu.c_str();
	const char *endDefinition = userContextItem + userContextMenu.length();
	while (userContextItem < endDefinition) {
		const char *caption = userContextItem;
		userContextItem += strlen(userContextItem) + 1;
		if (userContextItem < endDefinition) {
			int cmd = GetMenuCommandAsInt(userContextItem);
			userContextItem += strlen(userContextItem) + 1;
			AddToPopUp(caption, cmd);
		}
	}
	popup.Show(pt, wCmd);
}

/**
 * Ensure that a splitter bar position is inside the main window.
 */
int SciTEBase::NormaliseSplit(int splitPos) {
	GUI::Rectangle rcClient = GetClientRectangle();
	int w = rcClient.Width();
	int h = rcClient.Height();
	if (splitPos < 20)
		splitPos = 0;
	if (splitVertical) {
		if (splitPos > w - heightBar - 20)
			splitPos = w - heightBar;
	} else {
		if (splitPos > h - heightBar - 20)
			splitPos = h - heightBar;
	}
	return splitPos;
}

void SciTEBase::MoveSplit(GUI::Point ptNewDrag) {
	int newHeightOutput = heightOutputStartDrag + (ptStartDrag.y - ptNewDrag.y);
	if (splitVertical)
		newHeightOutput = heightOutputStartDrag + (ptStartDrag.x - ptNewDrag.x);
	newHeightOutput = NormaliseSplit(newHeightOutput);
	if (heightOutput != newHeightOutput) {
		heightOutput = newHeightOutput;
		SizeContentWindows();
		//Redraw();
	}

	previousHeightOutput = newHeightOutput;
}

void SciTEBase::TimerStart(int /* mask */) {
}

void SciTEBase::TimerEnd(int /* mask */) {
}

void SciTEBase::OnTimer() {
	if (delayBeforeAutoSave) {
		// First save the visible buffer to avoid any switching if not needed
		if (CurrentBuffer()->NeedsSave(delayBeforeAutoSave)) {
			Save(sfNone);
		}
		// Then look through the other buffers to save any that need to be saved
		int currentBuffer = buffers.Current();
		for (int i = 0; i < buffers.length; i++) {
			if (buffers.buffers[i].NeedsSave(delayBeforeAutoSave)) {
				SetDocumentAt(i);
				Save(sfNone);
			}
		}
		SetDocumentAt(currentBuffer);
	}
}

void SciTEBase::UIAvailable() {
	SetImportMenu();
	if (extender) {
		FilePath homepath = GetSciteDefaultHome();
		props.Set("SciteDefaultHome", homepath.AsUTF8().c_str());
		homepath = GetSciteUserHome();
		props.Set("SciteUserHome", homepath.AsUTF8().c_str());
		extender->Initialise(this);
	}
}

/**
 * Find the character following a name which is made up of characters from
 * the set [a-zA-Z.]
 */
static GUI::gui_char AfterName(const GUI::gui_char *s) {
	while (*s && ((*s == '.') ||
	        (*s >= 'a' && *s <= 'z') ||
	        (*s >= 'A' && *s <= 'Z')))
		s++;
	return *s;
}

void SciTEBase::PerformOne(char *action) {
	unsigned int len = UnSlash(action);
	char *arg = strchr(action, ':');
	if (arg) {
		arg++;
		if (isprefix(action, "askfilename:")) {
			extender->OnMacro("filename", filePath.AsUTF8().c_str());
		} else if (isprefix(action, "askproperty:")) {
			PropertyToDirector(arg);
		} else if (isprefix(action, "close:")) {
			Close();
			WindowSetFocus(wEditor);
		} else if (isprefix(action, "currentmacro:")) {
			currentMacro = arg;
		} else if (isprefix(action, "cwd:")) {
			FilePath dirTarget(GUI::StringFromUTF8(arg));
			if (!dirTarget.SetWorkingDirectory()) {
				GUI::gui_string msg = LocaliseMessage("Invalid directory '^0'.", dirTarget.AsInternal());
				WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
			}
		} else if (isprefix(action, "enumproperties:")) {
			EnumProperties(arg);
		} else if (isprefix(action, "exportashtml:")) {
			SaveToHTML(GUI::StringFromUTF8(arg));
		} else if (isprefix(action, "exportasrtf:")) {
			SaveToRTF(GUI::StringFromUTF8(arg));
		} else if (isprefix(action, "exportaspdf:")) {
			SaveToPDF(GUI::StringFromUTF8(arg));
		} else if (isprefix(action, "exportaslatex:")) {
			SaveToTEX(GUI::StringFromUTF8(arg));
		} else if (isprefix(action, "exportasxml:")) {
			SaveToXML(GUI::StringFromUTF8(arg));
		} else if (isprefix(action, "find:") && wEditor.Created()) {
			findWhat = arg;
			FindNext(false, false);
		} else if (isprefix(action, "goto:") && wEditor.Created()) {
			int line = atoi(arg) - 1;
			GotoLineEnsureVisible(line);
			// jump to column if given and greater than 0
			char *colstr = strchr(arg, ',');
			if (colstr != NULL) {
				int col = atoi(colstr + 1);
				if (col > 0) {
					int pos = wEditor.Call(SCI_GETCURRENTPOS) + col;
					// select the word you have found there
					int wordStart = wEditor.Call(SCI_WORDSTARTPOSITION, pos, true);
					int wordEnd = wEditor.Call(SCI_WORDENDPOSITION, pos, true);
					wEditor.Call(SCI_SETSEL, wordStart, wordEnd);
				}
			}
		} else if (isprefix(action, "insert:") && wEditor.Created()) {
			wEditor.CallString(SCI_REPLACESEL, 0, arg);
		} else if (isprefix(action, "loadsession:")) {
			if (*arg) {
				LoadSessionFile(GUI::StringFromUTF8(arg).c_str());
				RestoreSession();
			}
		} else if (isprefix(action, "macrocommand:")) {
			ExecuteMacroCommand(arg);
		} else if (isprefix(action, "macroenable:")) {
			macrosEnabled = atoi(arg);
			SetToolsMenu();
		} else if (isprefix(action, "macrolist:")) {
			StartMacroList(arg);
		} else if (isprefix(action, "menucommand:")) {
			MenuCommand(atoi(arg));
		} else if (isprefix(action, "open:")) {
			Open(GUI::StringFromUTF8(arg), ofSynchronous);
		} else if (isprefix(action, "output:") && wOutput.Created()) {
			wOutput.Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(arg));
		} else if (isprefix(action, "property:")) {
			PropertyFromDirector(arg);
		} else if (isprefix(action, "reloadproperties:")) {
			ReloadProperties();
		} else if (isprefix(action, "quit:")) {
			QuitProgram();
		} else if (isprefix(action, "replaceall:") && wEditor.Created()) {
			if (len > strlen(action)) {
				char *arg2 = arg + strlen(arg) + 1;
				findWhat = arg;
				replaceWhat = arg2;
				ReplaceAll(false);
			}
		} else if (isprefix(action, "saveas:")) {
			if (*arg) {
				SaveAs(GUI::StringFromUTF8(arg).c_str(), true);
			} else {
				SaveAsDialog();
			}
		} else if (isprefix(action, "savesession:")) {
			if (*arg) {
				SaveSessionFile(GUI::StringFromUTF8(arg).c_str());
			}
		} else if (isprefix(action, "extender:")) {
			extender->OnExecute(arg);
		} else if (isprefix(action, "focus:")) {
			ActivateWindow(arg);
		}
	}
}

static bool IsSwitchCharacter(GUI::gui_char ch) {
#ifdef __unix__
	return ch == '-';
#else
	return (ch == '-') || (ch == '/');
#endif
}

// Called by SciTEBase::PerformOne when action="enumproperties:"
void SciTEBase::EnumProperties(const char *propkind) {
	PropSetFile *pf = NULL;

	if (!extender)
		return;
	if (!strcmp(propkind, "dyn")) {
		SelectionIntoProperties(); // Refresh properties ...
		pf = &props;
	} else if (!strcmp(propkind, "local"))
		pf = &propsLocal;
	else if (!strcmp(propkind, "directory"))
		pf = &propsDirectory;
	else if (!strcmp(propkind, "user"))
		pf = &propsUser;
	else if (!strcmp(propkind, "base"))
		pf = &propsBase;
	else if (!strcmp(propkind, "embed"))
		pf = &propsEmbed;
	else if (!strcmp(propkind, "abbrev"))
		pf = &propsAbbrev;

	if (pf != NULL) {
		const char *key = NULL;
		const char *val = NULL;
		bool b = pf->GetFirst(key, val);
		while (b) {
			SendOneProperty(propkind, key, val);
			b = pf->GetNext(key, val);
		}
	}
}

void SciTEBase::SendOneProperty(const char *kind, const char *key, const char *val) {
	size_t keysize = strlen(kind) + 1 + strlen(key) + 1 + strlen(val) + 1;
	char *m = new char[keysize];
	strcpy(m, kind);
	strcat(m, ":");
	strcat(m, key);
	strcat(m, "=");
	strcat(m, val);
	extender->SendProperty(m);
	delete []m;
}

void SciTEBase::PropertyFromDirector(const char *arg) {
	props.Set(arg);
}

void SciTEBase::PropertyToDirector(const char *arg) {
	if (!extender)
		return;
	SelectionIntoProperties();
	SString gotprop = props.Get(arg);
	extender->OnMacro("macro:stringinfo", gotprop.c_str());
}

/**
 * Menu/Toolbar command "Record".
 */
void SciTEBase::StartRecordMacro() {
	recording = true;
	CheckMenus();
	wEditor.Call(SCI_STARTRECORD);
}

/**
 * Received a SCN_MACRORECORD from Scintilla: send it to director.
 */
bool SciTEBase::RecordMacroCommand(SCNotification *notification) {
	if (extender) {
		char *szMessage;
		char *t;
		bool handled;
		t = (char *)(notification->lParam);
		SString sWParam(static_cast<size_t>(notification->wParam));
		if (t != NULL) {
			//format : "<message>;<wParam>;1;<text>"
			szMessage = new char[50 + strlen(t) + 4];
			sprintf(szMessage, "%d;%s;1;%s", notification->message, sWParam.c_str(), t);
		} else {
			//format : "<message>;<wParam>;0;"
			szMessage = new char[50];
			sprintf(szMessage, "%d;%s;0;", notification->message, sWParam.c_str());
		}
		handled = extender->OnMacro("macro:record", szMessage);
		delete []szMessage;
		return handled;
	}
	return true;
}

/**
 * Menu/Toolbar command "Stop recording".
 */
void SciTEBase::StopRecordMacro() {
	wEditor.Call(SCI_STOPRECORD);
	if (extender)
		extender->OnMacro("macro:stoprecord", "");
	recording = false;
	CheckMenus();
}

/**
 * Menu/Toolbar command "Play macro...": tell director to build list of Macro names
 * Through this call, user has access to all macros in Filerx.
 */
void SciTEBase::AskMacroList() {
	if (extender)
		extender->OnMacro("macro:getlist", "");
}

/**
 * List of Macro names has been created. Ask Scintilla to show it.
 */
bool SciTEBase::StartMacroList(const char *words) {
	if (words) {
		wEditor.CallString(SCI_USERLISTSHOW, 2, words); //listtype=2
	}

	return true;
}

/**
 * User has chosen a macro in the list. Ask director to execute it.
 */
void SciTEBase::ContinueMacroList(const char *stext) {
	if ((extender) && (*stext != '\0')) {
		currentMacro = stext;
		StartPlayMacro();
	}
}

/**
 * Menu/Toolbar command "Play current macro" (or called from ContinueMacroList).
 */
void SciTEBase::StartPlayMacro() {
	if (extender)
		extender->OnMacro("macro:run", currentMacro.c_str());
}

/*
SciTE received a macro command from director : execute it.
If command needs answer (SCI_GETTEXTLENGTH ...) : give answer to director
*/

static unsigned int ReadNum(const char *&t) {
	const char *argend = strchr(t, ';');	// find ';'
	unsigned int v = 0;
	if (*t)
		v = atoi(t);					// read value
	t = argend + 1;					// update pointer
	return v;						// return value
}

void SciTEBase::ExecuteMacroCommand(const char *command) {
	const char *nextarg = command;
	uptr_t wParam;
	sptr_t lParam = 0;
	int rep = 0;				//Scintilla's answer
	const char *answercmd;
	int l;
	char *string1 = NULL;
	char params[4];
	//params describe types of return values and of arguments
	//0 : void or no param
	//I : integer
	//S : string
	//R : return string (for lParam only)

	//extract message,wParam ,lParam

	unsigned int message = ReadNum(nextarg);
	strncpy(params, nextarg, 3);
	params[3] = '\0';
	nextarg += 4;
	if (*(params + 1) == 'R') {
		// in one function wParam is a string  : void SetProperty(string key,string name)
		const char *s1 = nextarg;
		while (*nextarg != ';')
			nextarg++;
		size_t lstring1 = nextarg - s1;
		string1 = new char[lstring1 + 1];
		if (lstring1 > 0)
			strncpy(string1, s1, lstring1);
		*(string1 + lstring1) = '\0';
		wParam = reinterpret_cast<uptr_t>(string1);
		nextarg++;
	} else {
		wParam = ReadNum(nextarg);
	}

	if (*(params + 2) == 'S')
		lParam = reinterpret_cast<sptr_t>(nextarg);
	else if (*(params + 2) == 'I')
		lParam = atoi(nextarg);

	if (*params == '0') {
		// no answer ...
		wEditor.Call(message, wParam, lParam);
		delete []string1;
		return;
	}

	if (*params == 'S') {
		// string answer
		if (message == SCI_GETSELTEXT) {
			l = wEditor.Call(SCI_GETSELTEXT, 0, 0);
			wParam = 0;
		} else if (message == SCI_GETCURLINE) {
			int line = wEditor.Call(SCI_LINEFROMPOSITION, wEditor.Call(SCI_GETCURRENTPOS));
			l = wEditor.Call(SCI_LINELENGTH, line);
			wParam = l;
		} else if (message == SCI_GETTEXT) {
			l = wEditor.Call(SCI_GETLENGTH);
			wParam = l;
		} else if (message == SCI_GETLINE) {
			l = wEditor.Call(SCI_LINELENGTH, wParam);
		} else {
			l = 0; //unsupported calls EM
		}
		answercmd = "stringinfo:";

	} else {
		//int answer
		answercmd = "intinfo:";
		l = 30;
	}

	size_t alen = strlen(answercmd);
	char *tbuff = new char[l + alen + 1];
	strcpy(tbuff, answercmd);
	if (*params == 'S')
		lParam = reinterpret_cast<sptr_t>(tbuff + alen);

	if (l > 0)
		rep = wEditor.Call(message, wParam, lParam);
	if (*params == 'I')
		sprintf(tbuff + alen, "%0d", rep);
	extender->OnMacro("macro", tbuff);
	delete []tbuff;
}

std::vector<GUI::gui_string> ListFromString(const GUI::gui_string &args) {
	// Split on \n
	std::vector<GUI::gui_string> vs;
	GUI::gui_string s;
	bool lastNewLine = false;
	for (size_t i=0; i<args.size(); i++) {
		lastNewLine = args[i] == '\n';
		if (lastNewLine) {
			vs.push_back(s);
			s = GUI::gui_string();
		} else {
			s += args[i];
		}
	}
	if ((s.size() > 0) || lastNewLine) {
		vs.push_back(s);
	}
	return vs;
}

/**
 * Process all the command line arguments.
 * Arguments that start with '-' (also '/' on Windows) are switches or commands with
 * other arguments being file names which are opened. Commands are distinguished
 * from switches by containing a ':' after the command name.
 * The print switch /p is special cased.
 * Processing occurs in two phases to allow switches that occur before any file opens
 * to be evaluated before creating the UI.
 * Call twice, first with phase=0, then with phase=1 after creating UI.
 */
bool SciTEBase::ProcessCommandLine(GUI::gui_string &args, int phase) {
	bool performPrint = false;
	bool evaluate = phase == 0;
	std::vector<GUI::gui_string> wlArgs = ListFromString(args);
	// Convert args to vector
	for (size_t i = 0; i < wlArgs.size(); i++) {
		const GUI::gui_char *arg = wlArgs[i].c_str();
		if (IsSwitchCharacter(arg[0])) {
			arg++;
			if (arg[0] == '\0' || (arg[0] == '-' && arg[1] == '\0')) {
				if (phase == 1) {
					OpenFromStdin(arg[0] == '-');
				}
			} else if (arg[0] == '@') {
				if (phase == 1) {
					OpenFilesFromStdin();
				}
			} else if ((tolower(arg[0]) == 'p') && (arg[1] == 0)) {
				performPrint = true;
			} else if (GUI::gui_string(arg) == GUI_TEXT("grep") && (wlArgs.size() - i >= 4)) {
				// in form -grep [w~][c~][d~][b~] "<file-patterns>" "<search-string>"
				GrepFlags gf = grepStdOut;
				if (wlArgs[i+1][0] == 'w')
					gf = static_cast<GrepFlags>(gf | grepWholeWord);
				if (wlArgs[i+1][1] == 'c')
					gf = static_cast<GrepFlags>(gf | grepMatchCase);
				if (wlArgs[i+1][2] == 'd')
					gf = static_cast<GrepFlags>(gf | grepDot);
				if (wlArgs[i+1][3] == 'b')
					gf = static_cast<GrepFlags>(gf | grepBinary);
				char unquoted[1000];
				strcpy(unquoted, GUI::UTF8FromString(wlArgs[i+3].c_str()).c_str());
				UnSlash(unquoted);
				sptr_t originalEnd = 0;
				InternalGrep(gf, FilePath::GetWorkingDirectory().AsInternal(), wlArgs[i+2].c_str(), unquoted, originalEnd);
				exit(0);
			} else {
				if (AfterName(arg) == ':') {
					if (StartsWith(arg, GUI_TEXT("open:")) || StartsWith(arg, GUI_TEXT("loadsession:"))) {
						if (phase == 0)
							return performPrint;
						else
							evaluate = true;
					}
					if (evaluate) {
						const std::string sArg = GUI::UTF8FromString(arg);
						std::vector<char> vcArg(sArg.size() + 1);
						std::copy(sArg.begin(), sArg.end(), vcArg.begin());
						PerformOne(&vcArg[0]);
					}
				} else {
					if (evaluate) {
						props.ReadLine(GUI::UTF8FromString(arg).c_str(), true, FilePath::GetWorkingDirectory(), filter);
					}
				}
			}
		} else {	// Not a switch: it is a file name
			if (phase == 0)
				return performPrint;
			else
				evaluate = true;

			InitialiseBuffers();
			if (props.GetInt("save.recent"))
				RestoreRecentMenu();

			if (!PreOpenCheck(arg))
				Open(arg, static_cast<OpenFlags>(ofQuiet|ofSynchronous));
		}
	}
	if (phase == 1) {
		// If we have finished with all args and no buffer is open
		// try to load session.
		if (!buffers.initialised) {
			InitialiseBuffers();
			if (props.GetInt("save.recent"))
				RestoreRecentMenu();
			if (props.GetInt("buffers") && props.GetInt("save.session"))
				RestoreSession();
		}
		// No open file after session load so create empty document.
		if (filePath.IsUntitled() && buffers.length == 1 && !buffers.buffers[0].isDirty) {
			Open(GUI_TEXT(""));
		}
	}
	return performPrint;
}

// Implement ExtensionAPI methods
sptr_t SciTEBase::Send(Pane p, unsigned int msg, uptr_t wParam, sptr_t lParam) {
	if (p == paneEditor)
		return wEditor.Call(msg, wParam, lParam);
	else
		return wOutput.Call(msg, wParam, lParam);
}

char *SciTEBase::Range(Pane p, int start, int end) {
	int len = end - start;
	char *s = new char[len + 1];
	if (p == paneEditor)
		GetRange(wEditor, start, end, s);
	else
		GetRange(wOutput, start, end, s);
	return s;
}

void SciTEBase::Remove(Pane p, int start, int end) {
	// Should have a scintilla call for this
	if (p == paneEditor) {
		wEditor.Call(SCI_SETSEL, start, end);
		wEditor.Call(SCI_CLEAR);
	} else {
		wOutput.Call(SCI_SETSEL, start, end);
		wOutput.Call(SCI_CLEAR);
	}
}

void SciTEBase::Insert(Pane p, int pos, const char *s) {
	if (p == paneEditor)
		wEditor.CallString(SCI_INSERTTEXT, pos, s);
	else
		wOutput.CallString(SCI_INSERTTEXT, pos, s);
}

void SciTEBase::Trace(const char *s) {
	MakeOutputVisible();
	OutputAppendStringSynchronised(s);
}

char *SciTEBase::Property(const char *key) {
	SString value = props.GetExpanded(key);
	char *retval = new char[value.length() + 1];
	strcpy(retval, value.c_str());
	return retval;
}

void SciTEBase::SetProperty(const char *key, const char *val) {
	SString value = props.GetExpanded(key);
	if (value != val) {
		props.Set(key, val);
		needReadProperties = true;
	}
}

void SciTEBase::UnsetProperty(const char *key) {
	props.Unset(key);
	needReadProperties = true;
}

uptr_t SciTEBase::GetInstance() {
	return 0;
}

void SciTEBase::ShutDown() {
	QuitProgram();
}

void SciTEBase::Perform(const char *actionList) {
	char *actionsDup = StringDup(actionList);
	char *actions = actionsDup;
	char *nextAct;
	while ((nextAct = strchr(actions, '\n')) != NULL) {
		*nextAct = '\0';
		PerformOne(actions);
		actions = nextAct + 1;
	}
	PerformOne(actions);
	delete []actionsDup;
}

void SciTEBase::DoMenuCommand(int cmdID) {
	MenuCommand(cmdID, 0);
}

