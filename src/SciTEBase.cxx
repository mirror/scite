// SciTE - Scintilla based Text Editor
/** @file SciTEBase.cxx
 ** Platform independent base class of editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>

#endif

#if PLAT_WIN

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
#include "KeyWords.h"
#include "Scintilla.h"
#include "ScintillaWidget.h"
#include "SciLexer.h"
#include "Extender.h"
#include "SciTEBase.h"

const char *contributors[] = {
    "Atsuo Ishimoto",
    "Mark Hammond",
    "Francois Le Coguiec",
    "Dale Nagata",
    "Ralf Reinhardt",
    "Philippe Lhoste",
    "Andrew McKinlay",
    "Stephan R. A. Deibel",
    "Hans Eckardt",
    "Vassili Bourdo",
    "Maksim Lin",
    "Robin Dunn",
    "John Ehresman",
    "Steffen Goeldner",
    "Deepak S.",
    "DevelopMentor http://www.develop.com",
    "Yann Gaillard",
    "Aubin Paul",
    "Jason Diamond",
    "Ahmad Baitalmal",
    "Paul Winwood",
    "Maxim Baranov",
#if PLAT_GTK
    "Icons Copyright(C) 1998 by Dean S. Jones",
    "    http://jfa.javalobby.org/projects/icons/",
#endif
    "Ragnar Højland",
    "Christian Obrecht",
    "Andreas Neukoetter",
    "Adam Gates",
    "Steve Lhomme",
    "Ferdinand Prantl",
    "Jan Dries",
    "Markus Gritsch",
    "Tahir Karaca",
    "Ahmad Zawawi",
    "Laurent le Tynevez",
    "Walter Braeu",
    "Ashley Cambrell",
    "Garrett Serack",
    "Holger Schmidt",
    "ActiveState http://www.activestate.com",
    "James Larcombe",
    "Alexey Yutkin",
    "Jan Hercek",
    "Richard Pecl",
    "Edward K. Ream",
    "Valery Kondakoff",
    "Smári McCarthy",
    "Clemens Wyss",
    "Simon Steele",
};

const char *extList[] = {
    "x", "x.cpp", "x.bas", "x.rc", "x.html", "x.xml", "x.js", "x.vbs",
    "x.properties", "x.bat", "x.mak", "x.err", "x.java", "x.lua", "x.py",
    "x.pl", "x.sql", "x.spec", "x.php3", "x.tex", "x.diff", "x.cs", "x.conf",
    "x.pas", "x.ave", "x.ads", "x.lisp", "x.rb", "x.e"
};

// AddStyledText only called from About so static size buffer is OK
void AddStyledText(WindowID hwnd, const char *s, int attr) {
	char buf[1000];
	int len = strlen(s);
	for (int i = 0; i < len; i++) {
		buf[i*2] = s[i];
		buf[i*2 + 1] = static_cast<char>(attr);
	}
	Platform::SendScintilla(hwnd, SCI_ADDSTYLEDTEXT, len*2,
	                        reinterpret_cast<long>(static_cast < char * > (buf)));
}

void SetAboutStyle(WindowID wsci, int style, Colour fore) {
	Platform::SendScintilla(wsci, SCI_STYLESETFORE, style, fore.AsLong());
}

static void HackColour(int &n) {
	n += (rand() % 100) - 50;
	if (n > 0xE7)
		n = 0x60;
	if (n < 0)
		n = 0x80;
}

void SetAboutMessage(WindowID wsci, const char *appTitle) {
	if (wsci) {
		Platform::SendScintilla(wsci, SCI_SETSTYLEBITS, 7, 0);
		Platform::SendScintilla(wsci, SCI_STYLERESETDEFAULT, 0, 0);
		int fontSize = 15;
#if PLAT_GTK
		// On GTK+, new century schoolbook looks better in large sizes than default font
		Platform::SendScintilla(wsci, SCI_STYLESETFONT, STYLE_DEFAULT,
		                        reinterpret_cast<uptr_t>("new century schoolbook"));
		fontSize = 14;
#endif
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, STYLE_DEFAULT, Colour(0xff, 0xff, 0xff).AsLong());
		Platform::SendScintilla(wsci, SCI_STYLECLEARALL, 0, 0);

		SetAboutStyle(wsci, 0, Colour(0xff, 0xff, 0xff));
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, 0, fontSize);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, 0, Colour(0, 0, 0x80).AsLong());
		AddStyledText(wsci, appTitle, 0);
		AddStyledText(wsci, "\n", 0);
		SetAboutStyle(wsci, 1, Colour(0, 0, 0));
		AddStyledText(wsci, "Version 1.38\n", 1);
		SetAboutStyle(wsci, 2, Colour(0, 0, 0));
		Platform::SendScintilla(wsci, SCI_STYLESETITALIC, 2, 1);
		AddStyledText(wsci, "by Neil Hodgson.\n", 2);
		SetAboutStyle(wsci, 3, Colour(0, 0, 0));
		AddStyledText(wsci, "December 1998-May 2001.\n", 3);
		SetAboutStyle(wsci, 4, Colour(0, 0x7f, 0x7f));
		AddStyledText(wsci, "http://www.scintilla.org\n", 4);
		AddStyledText(wsci, "Contributors:\n", 1);
		srand(static_cast<unsigned>(time(0)));
		int r = rand() % 256;
		int g = rand() % 256;
		int b = rand() % 256;
		for (unsigned int co = 0;co < (sizeof(contributors) / sizeof(contributors[0]));co++) {
			HackColour(r);
			HackColour(g);
			HackColour(b);
			SetAboutStyle(wsci, 50 + co, Colour(r, g, b));
			AddStyledText(wsci, "    ", 50 + co);
			AddStyledText(wsci, contributors[co], 50 + co);
			AddStyledText(wsci, "\n", 50 + co);
		}
		Platform::SendScintilla(wsci, SCI_SETREADONLY, 1, 0);
	}
}

SciTEBase::SciTEBase(Extension *ext) : apis(true), extender(ext) {
	codePage = 0;
	characterSet = 0;
	language = "java";
	lexLanguage = SCLEX_CPP;
	functionDefinition = 0;
	indentSize = 8;
	indentOpening = true;
	indentClosing = true;
	statementLookback = 10;

	fnEditor = 0;
	ptrEditor = 0;
	fnOutput = 0;
	ptrOutput = 0;
	tbVisible = false;
	sbVisible = false;
	tabVisible = false;
	tabHideOne = false;
	tabMultiLine = false;
	sbNum = 1;
	visHeightTools = 0;
	visHeightStatus = 0;
	visHeightEditor = 1;
	heightBar = 7;
	dialogsOnScreen = 0;
	topMost = false;
	fullScreen = false;

	heightOutput = 0;
	previousHeightOutput = 0;

	allowMenuActions = true;
	isDirty = false;
	isBuilding = false;
	isBuilt = false;
	executing = false;
	commandCurrent = 0;
	jobUsesOutputPane = false;
	cancelFlag = 0L;

	ptStartDrag.x = 0;
	ptStartDrag.y = 0;
	capturedMouse = false;
	firstPropertiesRead = true;
	splitVertical = false;
	bufferedDraw = true;
	bracesCheck = true;
	bracesSloppy = false;
	bracesStyle = 0;
	braceCount = 0;

	indentationWSVisible = true;

	autoCompleteIgnoreCase = false;
	callTipIgnoreCase = false;
	autoCCausedByOnlyOne = false;

	margin = false;
	marginWidth = marginWidthDefault;
	foldMargin = true;
	foldMarginWidth = foldMarginWidthDefault;
	lineNumbers = false;
	lineNumbersWidth = lineNumbersWidthDefault;
	usePalette = false;

	clearBeforeExecute = false;
	findWhat[0] = '\0';
	replaceWhat[0] = '\0';
	replacing = false;
	havefound = false;
	matchCase = false;
	wholeWord = false;
	reverseFind = false;
	regExp = false;
	wrapFind = true;
	unSlash = false;

	windowName[0] = '\0';
	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	dirNameAtExecute[0] = '\0';
	useMonoFont = false;
	fileModTime = 0;

	macrosEnabled = false;
	currentmacro[0] = '\0';
	recording = false;

	propsBase.superPS = &propsEmbed;
	propsUser.superPS = &propsBase;
	propsLocal.superPS = &propsUser;
	props.superPS = &propsLocal;

	propsStatus.superPS = &props;
}

SciTEBase::~SciTEBase() {
	if (extender)
		extender->Finalise();
}

sptr_t SciTEBase::SendEditor(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	return fnEditor(ptrEditor, msg, wParam, lParam);
}

sptr_t SciTEBase::SendEditorString(unsigned int msg, uptr_t wParam, const char *s) {
	return SendEditor(msg, wParam, reinterpret_cast<sptr_t>(s));
}

sptr_t SciTEBase::SendOutput(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	return fnOutput(ptrOutput, msg, wParam, lParam);
}

void SciTEBase::SendChildren(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	SendEditor(msg, wParam, lParam);
	SendOutput(msg, wParam, lParam);
}

sptr_t SciTEBase::SendFocused(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	if (wOutput.HasFocus())
		return SendOutput(msg, wParam, lParam);
	else
		return SendEditor(msg, wParam, lParam);
}

sptr_t SciTEBase::SendOutputEx(unsigned int msg, uptr_t wParam/*= 0*/, sptr_t lParam /*= 0*/, bool direct /*= true*/) {
	if (direct)
		return SendOutput(msg, wParam, lParam);
	return Platform::SendScintilla(wOutput.GetID(), msg, wParam, lParam);
}

void SciTEBase::ViewWhitespace(bool view) {
	if (view && indentationWSVisible)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
	else if (view)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEAFTERINDENT);
	else
		SendEditor(SCI_SETVIEWWS, SCWS_INVISIBLE);
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
	SendEditor(SCI_ASSIGNCMDKEY,
	           Platform::LongFromTwoShorts(static_cast<short>(key),
	                                       static_cast<short>(mods)), cmd);
}

/**
 * Keep the colors and other attributes, set the size and font to
 * those defined in the @c font.monospace property.
 */
void SciTEBase::SetMonoFont() {
	if (useMonoFont) {
		SString sval = props.GetExpanded("font.monospace");
		StyleDefinition sd(sval.c_str());
		for (int style = 0; style <= STYLE_MAX; style++) {
			if (style != STYLE_DEFAULT) {
				if (sd.specified & StyleDefinition::sdSize) {
					Platform::SendScintilla(wEditor.GetID(), SCI_STYLESETSIZE, style, sd.size);
				}
				if (sd.specified & StyleDefinition::sdFont) {
					Platform::SendScintilla(wEditor.GetID(), SCI_STYLESETFONT, style, reinterpret_cast<long>(sd.font.c_str()));
				}
			}
		}
	} else {
		// Set to standard styles by rereading the properties and applying them
		ReadProperties();
	}
	SendEditor(SCI_COLOURISE, 0, -1);
	Redraw();
}

/**
 * Override the language of the current file with the one indicated by @a cmdID.
 * Mostly used to set a language on a file of unknown extension.
 */
void SciTEBase::SetOverrideLanguage(int cmdID) {
	RecentFile rf = GetFilePosition();
	EnsureRangeVisible(0, SendEditor(SCI_GETLENGTH));
	// Zero all the style bytes
	SendEditor(SCI_CLEARDOCUMENTSTYLE);
	useMonoFont = false;

	overrideExtension = extList[cmdID - LEXER_BASE];
	ReadProperties();
	SendEditor(SCI_COLOURISE, 0, -1);
	Redraw();
	DisplayAround(rf);
}

int SciTEBase::LengthDocument() {
	return SendEditor(SCI_GETLENGTH);
}

int SciTEBase::GetCaretInLine() {
	int caret = SendEditor(SCI_GETCURRENTPOS);
	int line = SendEditor(SCI_LINEFROMPOSITION, caret);
	int lineStart = SendEditor(SCI_POSITIONFROMLINE, line);
	return caret - lineStart;
}

void SciTEBase::GetLine(char *text, int sizeText, int line) {
	if (line < 0)
		line = GetCurrentLineNumber();
	int lineStart = SendEditor(SCI_POSITIONFROMLINE, line);
	int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, line);
	if ((lineStart < 0) || (lineEnd < 0)) {
		text[0] = '\0';
	}
	int lineMax = lineStart + sizeText - 1;
	if (lineEnd > lineMax)
		lineEnd = lineMax;
	GetRange(wEditor, lineStart, lineEnd, text);
	text[lineEnd - lineStart] = '\0';
}

void SciTEBase::GetRange(Window &win, int start, int end, char *text) {
	TextRange tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	Platform::SendScintilla(win.GetID(), SCI_GETTEXTRANGE, 0, reinterpret_cast<long>(&tr));
}

#ifdef OLD_CODE
void SciTEBase::Colourise(int start, int end, bool editor) {
	// Colourisation is now performed by the SciLexer DLL
	//DWORD dwStart = timeGetTime();
	Window &win = editor ? wEditor : wOutput;
	int lengthDoc = Platform::SendScintilla(win.GetID(), SCI_GETLENGTH, 0, 0);
	if (end == -1)
		end = lengthDoc;
	int len = end - start;

	StylingContext styler(win.GetID(), props);

	int styleStart = 0;
	if (start > 0)
		styleStart = styler.StyleAt(start - 1);
	styler.SetCodePage(codePage);

	if (editor) {
		LexerModule::Colourise(start, len, styleStart, lexLanguage, keyWordLists, styler);
	} else {
		LexerModule::Colourise(start, len, 0, SCLEX_ERRORLIST, 0, styler);
	}
	styler.Flush();
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("end colourise %d\n", dwEnd - dwStart);
}

#endif

/**
 * Check if the given line is a preprocessor condition line.
 * @return The kind of preprocessor condition (enum values).
 */
int SciTEBase::IsLinePreprocessorCondition(char *line) {
	char *currChar = line;
	char word[32];
	int i = 0;

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
		while (!isspacechar(*currChar) && *currChar) {
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
    int &curLine,  		///< Number of the line where to start the search
    int direction,  		///< Direction of search: 1 = forward, -1 = backward
    int condEnd1,  		///< First status of line for which the search is OK
    int condEnd2) {		///< Second one

	bool isInside = false;
	char line[80];
	int status, level = 0;
	int maxLines = SendEditor(SCI_GETLINECOUNT);

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
 * @return true if inside a preprocessor condition.
 */
#ifdef __BORLANDC__
// Borland warns that isInside is assigned a value that is never used in this method.
// This is OK so turn off the warning just for this method.
#pragma warn -aus
#endif
bool SciTEBase::FindMatchingPreprocCondPosition(
    bool isForward,  		///< @c true if search forward
    int &mppcAtCaret,  	///< Matching preproc. cond.: current position of caret
    int &mppcMatch) {	///< Matching preproc. cond.: matching position

	bool isInside = false;
	int curLine;
	char line[80];	// Probably no need to get more characters, even if the line is longer, unless very strange layout...
	int status;

	// Get current line
	curLine = SendEditor(SCI_LINEFROMPOSITION, mppcAtCaret);
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
	default:  	// Should be noPPC

		if (isForward) {
			isInside = FindMatchingPreprocessorCondition(curLine, 1, ppcMiddle, ppcEnd);
		} else {
			isInside = FindMatchingPreprocessorCondition(curLine, -1, ppcStart, ppcMiddle);
		}
		break;
	}

	if (isInside) {
		mppcMatch = SendEditor(SCI_POSITIONFROMLINE, curLine);
	}
	return isInside;
}
#ifdef __BORLANDC__
#pragma warn .aus
#endif

/**
 * Find if there is a brace next to the caret, checking before caret first, then
 * after caret. If brace found also find its matching brace.
 * @return true if inside a bracket pair.
 */
bool SciTEBase::FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy) {
	bool isInside = false;
	Window &win = editor ? wEditor : wOutput;
	int bracesStyleCheck = editor ? bracesStyle : 0;
	int caretPos = Platform::SendScintilla(win.GetID(), SCI_GETCURRENTPOS, 0, 0);
	braceAtCaret = -1;
	braceOpposite = -1;
	char charBefore = '\0';
	char styleBefore = '\0';
	WindowAccessor acc(win.GetID(), props);
	if (caretPos > 0) {
		charBefore = acc[caretPos - 1];
		styleBefore = static_cast<char>(acc.StyleAt(caretPos - 1) & 31);
	}
	// Priority goes to character before caret
	if (charBefore && strchr("[](){}", charBefore) &&
	        ((styleBefore == bracesStyleCheck) || (!bracesStyle))) {
		braceAtCaret = caretPos - 1;
	}
	bool colonMode = false;
	if (lexLanguage == SCLEX_PYTHON && ':' == charBefore) {
		braceAtCaret = caretPos - 1;
		colonMode = true;
	}
	bool isAfter = true;
	if (sloppy && (braceAtCaret < 0)) {
		// No brace found so check other side
		char charAfter = acc[caretPos];
		char styleAfter = static_cast<char>(acc.StyleAt(caretPos) & 31);
		if (charAfter && strchr("[](){}", charAfter) && (styleAfter == bracesStyleCheck)) {
			braceAtCaret = caretPos;
			isAfter = false;
		}
		if (lexLanguage == SCLEX_PYTHON && ':' == charAfter) {
			braceAtCaret = caretPos;
			colonMode = true;
		}
	}
	if (braceAtCaret >= 0) {
		if (colonMode) {
			int lineStart = Platform::SendScintilla(win.GetID(), SCI_LINEFROMPOSITION, braceAtCaret);
			int lineMaxSubord = Platform::SendScintilla(win.GetID(), SCI_GETLASTCHILD, lineStart, -1);
			braceOpposite = Platform::SendScintilla(win.GetID(), SCI_GETLINEENDPOSITION, lineMaxSubord);
		} else {
			braceOpposite = Platform::SendScintilla(win.GetID(), SCI_BRACEMATCH, braceAtCaret, 0);
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
	Window &win = editor ? wEditor : wOutput;
	if ((braceAtCaret != -1) && (braceOpposite == -1)) {
		Platform::SendScintilla(win.GetID(), SCI_BRACEBADLIGHT, braceAtCaret, 0);
		SendEditor(SCI_SETHIGHLIGHTGUIDE, 0);
	} else {
		char chBrace = static_cast<char>(Platform::SendScintilla(
		                                         win.GetID(), SCI_GETCHARAT, braceAtCaret, 0));
		Platform::SendScintilla(win.GetID(), SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
		int columnAtCaret = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, braceAtCaret, 0);
		if (chBrace == ':') {
			int lineStart = Platform::SendScintilla(win.GetID(), SCI_LINEFROMPOSITION, braceAtCaret);
			int indentPos = Platform::SendScintilla(win.GetID(), SCI_GETLINEINDENTPOSITION, lineStart, 0);
			int indentPosNext = Platform::SendScintilla(win.GetID(), SCI_GETLINEINDENTPOSITION, lineStart + 1, 0);
			columnAtCaret = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, indentPos, 0);
			int columnAtCaretNext = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, indentPosNext, 0);
			indentSize = props.GetInt("indent.size");
			if (columnAtCaretNext - indentSize > 1)
				columnAtCaret = columnAtCaretNext - indentSize;
			//Platform::DebugPrintf(": %d %d %d\n", lineStart, indentPos, columnAtCaret);
		}

		int columnOpposite = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, braceOpposite, 0);
		if (props.GetInt("highlight.indentation.guides"))
			Platform::SendScintilla(win.GetID(), SCI_SETHIGHLIGHTGUIDE, Platform::Minimum(columnAtCaret, columnOpposite), 0);
	}
}

void SciTEBase::SetWindowName() {
	if (fileName[0] == '\0')
		strcpy(windowName, "(Untitled)");
	else if (props.GetInt("title.full.path"))
		strcpy(windowName, fullPath);
	else
		strcpy(windowName, fileName);
	if (isDirty)
		strcat(windowName, " * ");
	else
		strcat(windowName, " - ");
	strcat(windowName, appName);
	wSciTE.SetTitle(windowName);
	//Platform::DebugPrintf("SetWindowname %s\n", windowName);
}

CharacterRange SciTEBase::GetSelection() {
	CharacterRange crange;
	crange.cpMin = SendEditor(SCI_GETSELECTIONSTART);
	crange.cpMax = SendEditor(SCI_GETSELECTIONEND);
	return crange;
}

void SciTEBase::SetSelection(int anchor, int currentPos) {
	SendEditor(SCI_SETSEL, anchor, currentPos);
}

void SciTEBase::GetCTag(char *sel, int len) {
	int lengthDoc, selStart, selEnd;
	int mustStop = 0;
	char c;
	Window wCurrent;

	if (wEditor.HasFocus()) {
		wCurrent = wEditor;
	} else {
		wCurrent = wOutput;
	}
	lengthDoc = SendFocused(SCI_GETLENGTH);
	selStart = selEnd = SendFocused(SCI_GETSELECTIONEND);
	WindowAccessor acc(wCurrent.GetID(), props);
	while (!mustStop) {
		if (selStart < lengthDoc - 1) {
			selStart++;
			c = acc[selStart];
			if (c == '\r' || c == '\n') {
				mustStop = -1;
			} else if (c == '\t') {
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
	}
	sel[0] = '\0';
	if ((selStart < selEnd) && ((selEnd - selStart + 1) < len)) {
		GetRange(wCurrent, selStart, selEnd, sel);
	}
}

// Should also use word.characters.*, if exists, in the opposite way (in set instead of not in set)
static bool iswordcharforsel(char ch) {
	return !strchr("\t\n\r !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", ch);
}

// Accept slighly more characters than for a word
// Doesn't accept all valid characters, as they are rarely used in source filenames...
// Accept path separators '/' and '\', extension separator '.', and ':', MS drive unit
// separator, and also used for separating the line number for grep. Same for '(' and ')' for cl.
static bool isfilenamecharforsel(char ch) {
	return !strchr("\t\n\r \"$%'*,;<>?[]^`{|}", ch);
}

/**
 * If there is selected text, either in the editor or the output pane,
 * put the selection in the @a sel buffer, up to @a len characters.
 * Otherwise, try and select characters around the caret, as long as they are OK
 * for the @a ischarforsel function.
 * Remove the last two character controls from the result, as they are likely
 * to be CR and/or LF.
 */
void SciTEBase::SelectionExtend(
    char *sel,  	///< Buffer receiving the result.
    int len,  	///< Size of the buffer.
    bool (*ischarforsel)(char ch)) {	///< Function returning @c true if the given char. is part of the selection.

	int lengthDoc, selStart, selEnd;
	Window wCurrent;

	if (wEditor.HasFocus()) {
		wCurrent = wEditor;
	} else {
		wCurrent = wOutput;
	}
	lengthDoc = SendFocused(SCI_GETLENGTH);
	selStart = SendFocused(SCI_GETSELECTIONSTART);
	selEnd = SendFocused(SCI_GETSELECTIONEND);
	if (selStart == selEnd && ischarforsel) {
		WindowAccessor acc(wCurrent.GetID(), props);
		// Try and find a word at the caret
		if (ischarforsel(acc[selStart])) {
			while ((selStart > 0) && (ischarforsel(acc[selStart - 1]))) {
				selStart--;
			}
			while ((selEnd < lengthDoc - 1) && (ischarforsel(acc[selEnd + 1]))) {
				selEnd++;
			}
			if (selStart < selEnd) {
				selEnd++;   	// Because normal selections end one past
			}

		}
	}
	sel[0] = '\0';
	if (selEnd - selStart + 1 > len - 1) {
		selEnd = selStart + len - 1;
	}
	if (selStart < selEnd) {
		GetRange(wCurrent, selStart, selEnd, sel);
	}
	// Change whole line selected but normally end of line characters not wanted.
	// Remove possible terminating \r, \n, or \r\n.
	int sellen = strlen(sel);
	if (sellen >= 1 && (sel[sellen - 1] == '\r' || sel[sellen - 1] == '\n')) {
		if (sellen >= 2 && (sel[sellen - 2] == '\r' && sel[sellen - 1] == '\n')) {
			sel[sellen - 2] = '\0';
		}
		sel[sellen - 1] = '\0';
	}
}

void SciTEBase::SelectionWord(char *word, int len) {
	SelectionExtend(word, len, iswordcharforsel);
}

void SciTEBase::SelectionFilename(char *filename, int len) {
	SelectionExtend(filename, len, isfilenamecharforsel);
}

void SciTEBase::SelectionIntoProperties() {
	char currentSelection[1000];
	SelectionExtend(currentSelection, sizeof(currentSelection), 0);
	props.Set("CurrentSelection", currentSelection);
	char word[200];
	SelectionWord(word, sizeof(word));
	props.Set("CurrentWord", word);
}

void SciTEBase::SelectionIntoFind() {
	SelectionWord(findWhat, sizeof(findWhat));
	if (unSlash) {
		char *slashedFind = Slash(findWhat);
		if (slashedFind) {
			strncpy(findWhat, slashedFind, sizeof(findWhat));
			findWhat[sizeof(findWhat) - 1] = '\0';
			delete []slashedFind;
		}
	}
}

void SciTEBase::FindMessageBox(const char *msg) {
	dialogsOnScreen++;
#if PLAT_GTK
	MessageBox(wSciTE.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#endif
#if PLAT_WIN
	MessageBox(wFindReplace.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#endif
	dialogsOnScreen--;
}

/**
 * Convert a string into C string literal form using \a, \b, \f, \n, \r, \t, \v, and \ooo.
 * The return value is a newly allocated character array containing the result.
 * 4 bytes are allocated for each byte of the input because that is the maximum
 * expansion needed when all of the input needs to be output using the octal form.
 * The return value should be deleted with delete[].
 */
char *Slash(const char *s) {
	char *oRet = new char[strlen(s) * 4 + 1];
	if (oRet) {
		char *o = oRet;
		while (*s) {
			if (*s == '\a') {
				*o++ = '\\';
				*o++ = 'a';
			} else if (*s == '\b') {
				*o++ = '\\';
				*o++ = 'b';
			} else if (*s == '\f') {
				*o++ = '\\';
				*o++ = 'f';
			} else if (*s == '\n') {
				*o++ = '\\';
				*o++ = 'n';
			} else if (*s == '\r') {
				*o++ = '\\';
				*o++ = 'r';
			} else if (*s == '\t') {
				*o++ = '\\';
				*o++ = 't';
			} else if (*s == '\v') {
				*o++ = '\\';
				*o++ = 'v';
			} else if (*s == '\\') {
				*o++ = '\\';
				*o++ = '\\';
			} else if (*s < ' ') {
				*o++ = '\\';
				*o++ = static_cast<char>((*s >> 6) + '0');
				*o++ = static_cast<char>((*s >> 3) + '0');
				*o++ = static_cast<char>((*s & 0x7) + '0');
			} else {
				*o++ = *s;
			}
			s++;
		}
		*o = '\0';
	}
	return oRet;
}

/**
 * Is the character an octal digit?
 */
static bool IsOctalDigit(char ch) {
	return ch >= '0' && ch <= '7';
}

/**
 * If the character is an hexa digit, get its value.
 */
static int GetHexaDigit(char ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'A' && ch <= 'F') {
		return ch - 'A' + 10;
	}
	if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	}
	return -1;
}

/**
 * Convert C style \a, \b, \f, \n, \r, \t, \v, \ooo and \xhh into their indicated characters.
 */
unsigned int UnSlash(char *s) {
	char *sStart = s;
	char *o = s;

	while (*s) {
		if (*s == '\\') {
			s++;
			if (*s == 'a') {
				*o = '\a';
			} else if (*s == 'b') {
				*o = '\b';
			} else if (*s == 'f') {
				*o = '\f';
			} else if (*s == 'n') {
				*o = '\n';
			} else if (*s == 'r') {
				*o = '\r';
			} else if (*s == 't') {
				*o = '\t';
			} else if (*s == 'v') {
				*o = '\v';
			} else if (IsOctalDigit(*s)) {
				int val = *s - '0';
				if (IsOctalDigit(*(s + 1))) {
					s++;
					val *= 8;
					val += *s - '0';
					if (IsOctalDigit(*(s + 1))) {
						s++;
						val *= 8;
						val += *s - '0';
					}
				}
				*o = static_cast<char>(val);
			} else if (*s == 'x') {
				s++;
				int val = 0;
				int ghd = GetHexaDigit(*s);
				if (ghd >= 0) {
					s++;
					val = ghd;
					ghd = GetHexaDigit(*s);
					if (ghd >= 0) {
						s++;
						val *= 16;
						val += ghd;
					}
				}
				*o = static_cast<char>(val);
			} else {
				*o = *s;
			}
		} else {
			*o = *s;
		}
		o++;
		if (*s) {
			s++;
		}
	}
	*o = '\0';
	return o - sStart;
}

/**
 * Convert C style \0oo into their indicated characters.
 * This is used to get control characters into the regular expresion engine.
 */
unsigned int UnSlashLowOctal(char *s) {
	char *sStart = s;
	char *o = s;
	while (*s) {
		if ((s[0] == '\\') && (s[1] == '0') && IsOctalDigit(s[2]) && IsOctalDigit(s[3])) {
			*o = static_cast<char>(8 * (s[2] - '0') + (s[3] - '0'));
			s += 3;
		} else {
			*o = *s;
		}
		o++;
		if (*s)
			s++;
	}
	*o = '\0';
	return o - sStart;
}

static int UnSlashAsNeeded(char *s, bool escapes, bool regularExpression) {
	if (escapes) {
		if (regularExpression) {
			// For regular expressions only escape sequences allowed start with \0
			return UnSlashLowOctal(s);
		} else {
			// C style escapes allowed
			return UnSlash(s);
		}
	} else {
		return strlen(s);
	}
}

void SciTEBase::FindNext(bool reverseDirection, bool showWarnings) {
	if (!findWhat[0]) {
		Find();
		return;
	}
	char findTarget[findReplaceMaxLen + 1];
	strcpy(findTarget, findWhat);
	int lenFind = UnSlashAsNeeded(findTarget, unSlash, regExp);
	if (lenFind == 0)
		return;

	CharacterRange cr = GetSelection();
	int startPosition = cr.cpMax;
	int endPosition = LengthDocument();
	if (reverseDirection) {
		startPosition = cr.cpMin - 1;
		endPosition = 0;
	}

	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) |
	            (matchCase ? SCFIND_MATCHCASE : 0) |
	            (regExp ? SCFIND_REGEXP : 0);

	SendEditor(SCI_SETTARGETSTART, startPosition);
	SendEditor(SCI_SETTARGETEND, endPosition);
	SendEditor(SCI_SETSEARCHFLAGS, flags);
	//DWORD dwStart = timeGetTime();
	int posFind = SendEditorString(SCI_SEARCHINTARGET, lenFind, findTarget);
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found at %d took %d\n", findWhat, posFind, dwEnd - dwStart);
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
		SendEditor(SCI_SETTARGETSTART, startPosition);
		SendEditor(SCI_SETTARGETEND, endPosition);
		posFind = SendEditorString(SCI_SEARCHINTARGET, lenFind, findTarget);
		WarnUser(warnFindWrapped);
	}
	if (posFind == -1) {
		havefound = false;
		if (showWarnings) {
			WarnUser(warnNotFound);
			if (strlen(findWhat) > findReplaceMaxLen)
				findWhat[findReplaceMaxLen] = '\0';
			char msg[findReplaceMaxLen + 50];
			strcpy(msg, "Cannot find the string \"");
			strcat(msg, findWhat);
			strcat(msg, "\".");
			if (wFindReplace.Created()) {
				FindMessageBox(msg);
			} else {
				dialogsOnScreen++;
				MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
				dialogsOnScreen--;
			}
		}
	} else {
		havefound = true;
		int start = SendEditor(SCI_GETTARGETSTART);
		int end = SendEditor(SCI_GETTARGETEND);
		EnsureRangeVisible(start, end);
		SetSelection(start, end);
		if (!replacing) {
			DestroyFindReplace();
		}
	}
}

void SciTEBase::ReplaceOnce() {
	if (havefound) {
		char replaceTarget[findReplaceMaxLen + 1];
		strcpy(replaceTarget, replaceWhat);
		int replaceLen = UnSlashAsNeeded(replaceTarget, unSlash, regExp);
		CharacterRange cr = GetSelection();
		SendEditor(SCI_SETTARGETSTART, cr.cpMin);
		SendEditor(SCI_SETTARGETEND, cr.cpMax);
		int lenReplaced = replaceLen;
		if (regExp)
			lenReplaced = SendEditorString(SCI_REPLACETARGETRE, replaceLen, replaceTarget);
		else	// Allow \0 in replacement
			SendEditorString(SCI_REPLACETARGET, replaceLen, replaceTarget);
		SetSelection(cr.cpMin + lenReplaced, cr.cpMin);
		havefound = false;
		//Platform::DebugPrintf("Replace <%s> -> <%s>\n", findWhat, replaceWhat);
	}

	FindNext(reverseFind);
}

void SciTEBase::ReplaceAll(bool inSelection) {
	char findTarget[findReplaceMaxLen + 1];
	strcpy(findTarget, findWhat);
	int findLen = UnSlashAsNeeded(findTarget, unSlash, regExp);
	if (findLen == 0) {
		FindMessageBox("Find string for \"Replace All\" must not be empty.");
		return;
	}

	CharacterRange cr = GetSelection();
	int startPosition = cr.cpMin;
	int endPosition = cr.cpMax;
	if (inSelection) {
		if (startPosition == endPosition) {
			FindMessageBox("Selection for \"Replace in Selection\" must not be empty.");
			return;
		}
	} else {
		endPosition = LengthDocument();
		if (wrapFind) {
			// Whole document
			startPosition = 0;
		}
		// If not wrapFind, replace all only from caret to end of document
	}

	char replaceTarget[findReplaceMaxLen + 1];
	strcpy(replaceTarget, replaceWhat);
	int replaceLen = UnSlashAsNeeded(replaceTarget, unSlash, regExp);
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) |
	            (matchCase ? SCFIND_MATCHCASE : 0) |
	            (regExp ? SCFIND_REGEXP : 0);
	SendEditor(SCI_SETTARGETSTART, startPosition);
	SendEditor(SCI_SETTARGETEND, endPosition);
	SendEditor(SCI_SETSEARCHFLAGS, flags);
	int posFind = SendEditorString(SCI_SEARCHINTARGET, findLen, findTarget);
	if ((findLen == 1) && regExp && (findTarget[0] == '^')) {
		// Special case for replace all start of line so it hits the first line
		posFind = startPosition;
		SendEditor(SCI_SETTARGETSTART, startPosition);
		SendEditor(SCI_SETTARGETEND, startPosition);
	}
	if ((posFind != -1) && (posFind <= endPosition)) {
		int lastMatch = posFind;
		SendEditor(SCI_BEGINUNDOACTION);
		while (posFind != -1) {
			int lenTarget = SendEditor(SCI_GETTARGETEND) - SendEditor(SCI_GETTARGETSTART);
			int lenReplaced = replaceLen;
			if (regExp)
				lenReplaced = SendEditorString(SCI_REPLACETARGETRE, replaceLen, replaceTarget);
			else
				SendEditorString(SCI_REPLACETARGET, replaceLen, replaceTarget);
			// Modify for change caused by replacement
			endPosition += lenReplaced - lenTarget;
			lastMatch = posFind + lenReplaced;
			// For the special cases of start of line and end of line
			// Something better could be done but there are too many special cases
			if (lenTarget <= 0)
				lastMatch++;
			SendEditor(SCI_SETTARGETSTART, lastMatch);
			SendEditor(SCI_SETTARGETEND, endPosition);
			posFind = SendEditorString(SCI_SEARCHINTARGET, findLen, findTarget);
		}
		if (inSelection)
			SetSelection(startPosition, endPosition);
		else
			SetSelection(lastMatch, lastMatch);
		SendEditor(SCI_ENDUNDOACTION);
		//FindMessageBox("bow");
	} else {
		if (strlen(findWhat) > findReplaceMaxLen)
			findWhat[findReplaceMaxLen] = '\0';
		char msg[findReplaceMaxLen + 50];
		strcpy(msg, "No replacements because string \"");
		strcat(msg, findWhat);
		strcat(msg, "\" was not present.");
		FindMessageBox(msg);
	}
	//Platform::DebugPrintf("ReplaceAll <%s> -> <%s>\n", findWhat, replaceWhat);
}

void SciTEBase::OutputAppendString(const char *s, int len) {
	if (len == -1)
		len = strlen(s);
	int docLength = SendOutput(SCI_GETTEXTLENGTH);
	SendOutput(SCI_SETTARGETSTART, docLength);
	SendOutput(SCI_SETTARGETEND, docLength);
	SendOutput(SCI_REPLACETARGET, len, reinterpret_cast<sptr_t>(s));
	int line = SendOutput(SCI_GETLINECOUNT, 0, 0);
	int lineStart = SendOutput(SCI_POSITIONFROMLINE, line);
	SendOutput(SCI_GOTOPOS, lineStart);
}

void SciTEBase::OutputAppendStringSynchronised(const char *s, int len /*= -1*/) {
	if (len == -1)
		len = strlen(s);
	int docLength = SendOutputEx(SCI_GETTEXTLENGTH, 0, 0, false);
	SendOutputEx(SCI_SETTARGETSTART, docLength, 0, false);
	SendOutputEx(SCI_SETTARGETEND, docLength, 0, false);
	SendOutputEx(SCI_REPLACETARGET, len, reinterpret_cast<sptr_t>(s), false);
	int line = SendOutputEx(SCI_GETLINECOUNT, 0, 0, false);
	int lineStart = SendOutputEx(SCI_POSITIONFROMLINE, line, 0, false);
	SendOutputEx(SCI_GOTOPOS, lineStart, 0, false);
}

void SciTEBase::MakeOutputVisible() {
	if (heightOutput < 20) {
		if (splitVertical)
			heightOutput = NormaliseSplit(300);
		else
			heightOutput = NormaliseSplit(100);
		SizeSubWindows();
		Redraw();
	}
}

void SciTEBase::Execute() {
	if (clearBeforeExecute) {
		SendOutput(SCI_CLEARALL);
	}

	SendOutput(SCI_MARKERDELETEALL, static_cast<uptr_t>( -1));
	SendEditor(SCI_MARKERDELETEALL, 0);
	// Ensure the output pane is visible
	if (jobUsesOutputPane) {
		MakeOutputVisible();
	}

	cancelFlag = 0L;
	executing = true;
	CheckMenus();
	chdir(dirName);
	strcpy(dirNameAtExecute, dirName);
}

void SciTEBase::ToggleOutputVisible() {
	if (heightOutput > 0) {
		heightOutput = NormaliseSplit(0);
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

void SciTEBase::BookmarkToggle(int lineno) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	int state = SendEditor(SCI_MARKERGET, lineno);
	if ( state & (1 << SciTE_MARKER_BOOKMARK)) {
		SendEditor(SCI_MARKERDELETE, lineno, SciTE_MARKER_BOOKMARK);
	} else {
		SendEditor(SCI_MARKERADD, lineno, SciTE_MARKER_BOOKMARK);
	}
}

void SciTEBase::BookmarkNext() {
	int lineno = GetCurrentLineNumber();
	int nextLine = SendEditor(SCI_MARKERNEXT, lineno + 1, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0)
		nextLine = SendEditor(SCI_MARKERNEXT, 0, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0 || nextLine == lineno)	// No bookmark (of the given type) or only one, and already on it
		WarnUser(warnNoOtherBookmark);
	else {
		GotoLineEnsureVisible(nextLine);
	}
}

void SciTEBase::BookmarkPrev() {
	// maybe this and BookmarkNext() should be merged
	int lineno = GetCurrentLineNumber();
	int linenb = SendEditor(SCI_GETLINECOUNT, 0, 0L);
	int prevLine = SendEditor(SCI_MARKERPREVIOUS, lineno - 1, 1 << SciTE_MARKER_BOOKMARK);
	if (prevLine < 0)
		prevLine = SendEditor(SCI_MARKERPREVIOUS, linenb, 1 << SciTE_MARKER_BOOKMARK);
	if (prevLine < 0 || prevLine == lineno) {	// No bookmark (of the given type) or only one, and already on it
		WarnUser(warnNoOtherBookmark);
	} else {
		GotoLineEnsureVisible(prevLine);
	}
}

PRectangle SciTEBase::GetClientRectangle() {
	return wContent.GetClientPosition();
}

void SciTEBase::Redraw() {
	wSciTE.InvalidateAll();
	wEditor.InvalidateAll();
	wOutput.InvalidateAll();
}

bool SciTEBase::StartCallTip() {
	//Platform::DebugPrintf("StartCallTip\n");
	char linebuf[1000];
	GetLine(linebuf, sizeof(linebuf));
	int current = GetCaretInLine();
	int pos = SendEditor(SCI_GETCURRENTPOS);
	int braces;
	do {
		braces = 0;
		while (current > 0 && (braces || linebuf[current - 1] != '(')) {
			if (linebuf[current - 1] == '(')
				braces--;
			else if (linebuf[current - 1] == ')')
				braces++;
			current--;
			pos--;
		}
		if (current > 0) {
			current--;
			pos--;
		} else
			break;
		while (current > 0 && isspace(linebuf[current - 1])) {
			current--;
			pos--;
		}
	} while (current > 0 && !calltipWordCharacters.contains(linebuf[current - 1]));
	if (current <= 0)
		return true;

	int startword = current - 1;
	while (startword > 0 && calltipWordCharacters.contains(linebuf[startword - 1]))
		startword--;

	linebuf[current] = '\0';
	int rootlen = current - startword;
	functionDefinition = "";
	//Platform::DebugPrintf("word  is [%s] %d %d %d\n", linebuf + startword, rootlen, pos, pos - rootlen);
	if (apis) {
		const char *word = apis.GetNearestWord (linebuf + startword, rootlen, callTipIgnoreCase);
		if (word) {
			functionDefinition = word;
			SendEditorString(SCI_CALLTIPSHOW, pos - rootlen, word);
			ContinueCallTip();
		}
	}
	return true;
}

static bool IsCallTipSeparator(char ch) {
	return (ch == ',') || (ch == ';');
}

void SciTEBase::ContinueCallTip() {
	char linebuf[1000];
	GetLine(linebuf, sizeof(linebuf));
	int current = GetCaretInLine();

	int commas = 0;
	for (int i = 0; i < current; i++) {
		if (IsCallTipSeparator(linebuf[i]))
			commas++;
	}

	int startHighlight = 0;
	while (functionDefinition[startHighlight] && functionDefinition[startHighlight] != '(')
		startHighlight++;
	if (functionDefinition[startHighlight] == '(')
		startHighlight++;
	while (functionDefinition[startHighlight] && commas > 0) {
		if (IsCallTipSeparator(functionDefinition[startHighlight]) || functionDefinition[startHighlight] == ')')
			commas--;
		startHighlight++;
	}
	if (IsCallTipSeparator(functionDefinition[startHighlight]) || functionDefinition[startHighlight] == ')')
		startHighlight++;
	int endHighlight = startHighlight;
	if (functionDefinition[endHighlight])
		endHighlight++;
	while (functionDefinition[endHighlight] && !IsCallTipSeparator(functionDefinition[endHighlight]) && functionDefinition[endHighlight] != ')')
		endHighlight++;

	SendEditor(SCI_CALLTIPSETHLT, startHighlight, endHighlight);
}

bool SciTEBase::StartAutoComplete() {
	char linebuf[1000];
	GetLine(linebuf, sizeof(linebuf));
	int current = GetCaretInLine();

	int startword = current;

	while ((startword > 0) &&
	        (wordCharacters.contains(linebuf[startword - 1]) ||
	         autoCompleteStartCharacters.contains(linebuf[startword - 1])))
		startword--;

	if (startword == current)
		return true;
	linebuf[current] = '\0';
	const char *root = linebuf + startword;
	int rootlen = current - startword;
	if (apis) {
		char *words = apis.GetNearestWords(root, rootlen, autoCompleteIgnoreCase);
		if (words) {
			SendEditorString(SCI_AUTOCSHOW, rootlen, words);
			delete []words;
		}
	}
	return true;
}

bool SciTEBase::StartAutoCompleteWord(bool onlyOneWord) {
	char linebuf[1000];
	GetLine(linebuf, sizeof(linebuf));
	int current = GetCaretInLine();

	int startword = current;
	while (startword > 0 && wordCharacters.contains(linebuf[startword - 1]))
		startword--;
	if (startword == current)
		return true;
	linebuf[current] = '\0';
	const char *root = linebuf + startword;
	int rootlen = current - startword;
	int doclen = LengthDocument();
	TextToFind ft = {{0, 0}, 0, {0, 0}};
	ft.lpstrText = const_cast<char*>(root);
	ft.chrg.cpMin = 0;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = SCFIND_WORDSTART | (autoCompleteIgnoreCase ? 0 : SCFIND_MATCHCASE);
	int posCurrentWord = SendEditor (SCI_GETCURRENTPOS) - rootlen;
	//DWORD dwStart = timeGetTime();
	int minWordLength = 0;
	int nwords = 0;

	// wordsNear contains a list of words separated by single spaces and with a space
	// at the start and end. this makes it easy to search for words.
	SString wordsNear;
	wordsNear.setsizegrowth(1000);
	wordsNear.append(" ");

	for (;;) {	// search all the document
		ft.chrg.cpMax = doclen;
		int posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
		if (posFind == -1 || posFind >= doclen)
			break;
		if (posFind == posCurrentWord) {
			ft.chrg.cpMin = posFind + rootlen;
			continue;
		}
		// Grab the word and put spaces around it
		const int wordMaxSize = 80;
		char wordstart[wordMaxSize];
		wordstart[0] = ' ';
		GetRange(wEditor, posFind, Platform::Minimum(posFind + wordMaxSize - 3, doclen), wordstart + 1);
		char *wordend = wordstart + 1 + rootlen;
		while (iswordcharforsel(*wordend))
			wordend++;
		*wordend++ = ' ';
		*wordend = '\0';
		int wordlen = wordend - wordstart - 2;
		if (wordlen > rootlen) {
			const char *wordpos;
			wordpos = strstr(wordsNear.c_str(), wordstart);
			if (!wordpos) {	// add a new entry
				wordsNear.append(wordstart + 1);
				if (minWordLength < wordlen)
					minWordLength = wordlen;

				nwords++;
				if (onlyOneWord && nwords > 1) {
					return true;
				}
			}
		}
		ft.chrg.cpMin = posFind + wordlen;
	}
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found %d characters took %d\n", root, length, dwEnd - dwStart);
	int length = wordsNear.length();
	if ((length > 2) && (!onlyOneWord || (minWordLength > rootlen))) {
		char *words = wordsNear.detach();
		words[length - 1] = '\0';
		SendEditorString(SCI_AUTOCSHOW, rootlen, words + 1);
		delete []words;
	} else {
		SendEditor(SCI_AUTOCCANCEL);
	}
	return true;
}

bool SciTEBase::StartExpandAbbreviation() {
	char linebuf[1000];
	GetLine(linebuf, sizeof(linebuf));
	int current = GetCaretInLine();
	int position = SendEditor(SCI_GETCURRENTPOS); // from the beginning
	int startword = current;
	int counter = 0;
	while (startword > 0 && wordCharacters.contains(linebuf[startword - 1])) {
		counter++;
		startword--;
	}
	if (startword == current)
		return true;
	linebuf[current] = '\0';
	const char *abbrev = linebuf + startword;
	SString data = propsAbbrev.Get(abbrev);
	int dataLength = data.length();
	if (dataLength == 0) {
		return true; // returning if expanded abbreviation is empty
	}

	char expbuf[1000];
	strcpy(expbuf, data.c_str());
	UnSlash(expbuf);
	SString expanded(""); // parsed expanded abbreviation
	int j = 0; // temporary variable for caret position counting
	int caret_pos = -1; // caret position
	int line = 0; // counting lines in multiline abbreviations
	int line_before_caret = 0;
	for (int i = 0; i < dataLength; i++) {
		char c = expbuf[i];
		switch (c) {
		case '|':
			if (i < (dataLength - 1) && expbuf[i + 1] == '|') {
				i++;
				expanded += c;
			} else {
				if (caret_pos != -1) {
					break;
				}
				caret_pos = j; // this is the caret position
			}

			break;
		case '\n':
			// we can't use tabs and spaces after '\n'
			if (expbuf[i + 1] == '\t' || expbuf[i + 1] == ' ') {
				SString error("Abbreviation \"");
				error += abbrev;
				error += "=";
				error += data.c_str();
				error += "\".\n";
				error += "Don't use tabs and spaces after \'\\n\' symbol!";
				MessageBox(wSciTE.GetID(), error.c_str(),
					"Expand Abbreviation Error", MB_OK | MB_ICONWARNING);
				return true;
			}
			line++;
			expanded += c;
			// counting newlines before caret position
			if (caret_pos != -1)
				break;
			line_before_caret++;
			break;
		default:
			expanded += c;
			break;
		}
		j++;
	}
	// taking indent position from current line - to use with (auto)indent
	int currentLineNumber = GetCurrentLineNumber();
	int indent = GetLineIndentation(currentLineNumber);
	SendEditor(SCI_BEGINUNDOACTION);
	SendEditor(SCI_SETSEL, position - counter, position);
	SendEditorString(SCI_REPLACESEL, 0, expanded.c_str());
	// indenting expanded abbreviation using previous line indent
	// if there is empty line in expanded abbreviation it is indented twice!
	if (line > 0) {
		for (int i = 1; i <= line; i++) {
			SetLineIndentation(currentLineNumber + i , indent);
			int lineIndent = GetLineIndentPosition(currentLineNumber + i);
			int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, currentLineNumber + i);
			if (lineIndent == lineEnd) {
				SetLineIndentation(currentLineNumber + i , indent + indentSize);
			} else {
				SetLineIndentation(currentLineNumber + i , indent);
			}
		}
		// setting cursor after expanded abbreviation
		if (caret_pos == -1) {
			int curLine = GetCurrentLineNumber();
			int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, curLine);
			SendEditor(SCI_GOTOPOS, lineEnd);
		}
	}
	if (caret_pos != -1) {
		// calculating caret position _after_ (auto)indenting
		int add_indent = 0;
		for (int i = 1; i <= line_before_caret; i++) {
			int lineIndent = GetLineIndentPosition(currentLineNumber + i);
			int lineStart = SendEditor(SCI_POSITIONFROMLINE, currentLineNumber + i);
			int difference = lineIndent - lineStart;
			add_indent += difference;
		}
		SendEditor(SCI_GOTOPOS, position - counter + caret_pos + add_indent);
	}
	SendEditor(SCI_ENDUNDOACTION);
	return true;
}

bool SciTEBase::StartBlockComment() {
	SString fileNameForExtension = ExtensionFileName();
	SString language = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString base("comment.block.");
	base += language;
	SString comment = props.Get(base.c_str());
	if (comment == "") { // user friendly error message box
		SString error("Block comment variable \"");
		error += base.c_str();
		error += "\" is not defined in SciTE *.properties!";
		MessageBox(wSciTE.GetID(), error.c_str(), "Block Comment Error", MB_OK | MB_ICONWARNING);
		return true;
	}
	SString long_comment = comment.append(" ");
	char linebuf[1000];
	int comment_length = comment.length();
	int selectionStart = SendEditor(SCI_GETSELECTIONSTART);
	int selectionEnd = SendEditor(SCI_GETSELECTIONEND);
	int caretPosition = SendEditor(SCI_GETCURRENTPOS);
	// checking if caret is located in _beginning_ of selected block
	bool move_caret = caretPosition < selectionEnd;
	int selStartLine = SendEditor(SCI_LINEFROMPOSITION, selectionStart);
	int selEndLine = SendEditor(SCI_LINEFROMPOSITION, selectionEnd);
	int lines = selEndLine - selStartLine;
	int firstSelLineStart = SendEditor(SCI_POSITIONFROMLINE, selStartLine);
	// "carret return" is part of the last selected line
	if (lines > 0 && selectionEnd == SendEditor(SCI_POSITIONFROMLINE, selEndLine))
		selEndLine--;
	SendEditor(SCI_BEGINUNDOACTION);
	for (int i = selStartLine; i <= selEndLine; i++) {
		int lineIndent = GetLineIndentPosition(i);
		int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, i);
		GetRange(wEditor, lineIndent, lineEnd, linebuf);
		// empty lines are not commented
		if (strlen(linebuf) < 1)
			continue;
		if (memcmp(linebuf, comment.c_str(), comment_length - 1) == 0) {
			if (memcmp(linebuf, long_comment.c_str(), comment_length) == 0) {
				// removing comment with space after it
				SendEditor(SCI_SETSEL, lineIndent, lineIndent + comment_length);
				SendEditorString(SCI_REPLACESEL, 0, "");
				if (i == selStartLine) // is this the first selected line?
					selectionStart -= comment_length;
				selectionEnd -= comment_length; // every iteration
				continue;
			} else {
				// removing comment _without_ space
				SendEditor(SCI_SETSEL, lineIndent, lineIndent + comment_length - 1);
				SendEditorString(SCI_REPLACESEL, 0, "");
				if (i == selStartLine) // is this the first selected line?
					selectionStart -= (comment_length - 1);
				selectionEnd -= (comment_length - 1); // every iteration
				continue;
			}
		}
		if (i == selStartLine) // is this the first selected line?
			selectionStart += comment_length;
		selectionEnd += comment_length; // every iteration
		SendEditorString(SCI_INSERTTEXT, lineIndent, long_comment.c_str());
	}
	// after uncommenting selection may promote itself to the lines
	// before the first initially selected line
	if (selectionStart < firstSelLineStart)
		selectionStart = firstSelLineStart;
	if (move_caret) {
		// moving caret to the beginning of selected block
		SendEditor(SCI_GOTOPOS, selectionEnd);
		SendEditor(SCI_SETCURRENTPOS, selectionStart);
	} else {
		SendEditor(SCI_SETSEL, selectionStart, selectionEnd);
	}
	SendEditor(SCI_ENDUNDOACTION);
	return true;
}

bool SciTEBase::StartBoxComment() {
	SString fileNameForExtension = ExtensionFileName();
	SString language = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString start_base("comment.box.start.");
	SString middle_base("comment.box.middle.");
	SString end_base("comment.box.end.");
	SString white_space(" ");
	start_base += language;
	middle_base += language;
	end_base += language;
	SString start_comment = props.Get(start_base.c_str());
	SString middle_comment = props.Get(middle_base.c_str());
	SString end_comment = props.Get(end_base.c_str());
	if (start_comment == "" || middle_comment == "" || end_comment == "") {
		SString error("Box comment variables \"");
		error += start_base.c_str();
		error += "\", \"";
		error += middle_base.c_str();
		error += "\"\nand \"";
		error += end_base.c_str();
		error += "\" are not ";
		error += "defined in SciTE *.properties!";
		MessageBox(wSciTE.GetID(), error.c_str(), "Box Comment Error", MB_OK | MB_ICONWARNING);
		return true;
	}
	start_comment += white_space;
	middle_comment += white_space;
	end_comment = white_space.append(end_comment.c_str());
	int start_comment_length = start_comment.length();
	int middle_comment_length = middle_comment.length();
	int selectionStart = SendEditor(SCI_GETSELECTIONSTART);
	int selectionEnd = SendEditor(SCI_GETSELECTIONEND);
	int caretPosition = SendEditor(SCI_GETCURRENTPOS);
	// checking if caret is located in _beginning_ of selected block
	bool move_caret = caretPosition < selectionEnd;
	int selStartLine = SendEditor(SCI_LINEFROMPOSITION, selectionStart);
	int selEndLine = SendEditor(SCI_LINEFROMPOSITION, selectionEnd);
	int lines = selEndLine - selStartLine;
	// "carret return" is part of the last selected line
	if (lines > 0 && selectionEnd == SendEditor(SCI_POSITIONFROMLINE, selEndLine)) {
		selEndLine--;
		lines--;
		// get rid of CRLF problems
		selectionEnd = SendEditor(SCI_GETLINEENDPOSITION, selEndLine);
	}
	SendEditor(SCI_BEGINUNDOACTION);
	// first commented line (start_comment)
	int lineStart = SendEditor(SCI_POSITIONFROMLINE, selStartLine);
	SendEditorString(SCI_INSERTTEXT, lineStart, start_comment.c_str());
	selectionStart += start_comment_length;
	// lines between first and last commented lines (middle_comment)
	for (int i = selStartLine + 1; i <= selEndLine; i++) {
		lineStart = SendEditor(SCI_POSITIONFROMLINE, i);
		SendEditorString(SCI_INSERTTEXT, lineStart, middle_comment.c_str());
		selectionEnd += middle_comment_length;
	}
	// last commented line (end_comment)
	int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, selEndLine);
	if (lines > 0) {
		SendEditorString(SCI_INSERTTEXT, lineEnd, "\n");
		SendEditorString(SCI_INSERTTEXT, lineEnd + 1, (end_comment.c_str() + 1));
	} else {
		SendEditorString(SCI_INSERTTEXT, lineEnd, end_comment.c_str());
	}
	selectionEnd += (start_comment_length);
	if (move_caret) {
		// moving caret to the beginning of selected block
		SendEditor(SCI_GOTOPOS, selectionEnd);
		SendEditor(SCI_SETCURRENTPOS, selectionStart);
	} else {
		SendEditor(SCI_SETSEL, selectionStart, selectionEnd);
	}
	SendEditor(SCI_ENDUNDOACTION);
	return true;
}

bool SciTEBase::StartStreamComment() {
	SString fileNameForExtension = ExtensionFileName();
	SString language = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	SString start_base("comment.stream.start.");
	SString end_base("comment.stream.end.");
	SString white_space(" ");
	start_base += language;
	end_base += language;
	SString start_comment = props.Get(start_base.c_str());
	SString end_comment = props.Get(end_base.c_str());
	if (start_comment == "" || end_comment == "") {
		SString error("Stream comment variables \"");
		error += start_base.c_str();
		error += "\" and \n\"";
		error += end_base.c_str();
		error += "\" are not ";
		error += "defined in SciTE *.properties!";
		MessageBox(wSciTE.GetID(), error.c_str(), "Stream Comment Error", MB_OK | MB_ICONWARNING);
		return true;
	}
	start_comment += white_space;
	end_comment = white_space.append(end_comment.c_str());
	int start_comment_length = start_comment.length();
	int selectionStart = SendEditor(SCI_GETSELECTIONSTART);
	int selectionEnd = SendEditor(SCI_GETSELECTIONEND);
	int caretPosition = SendEditor(SCI_GETCURRENTPOS);
	// checking if caret is located in _beginning_ of selected block
	bool move_caret = caretPosition < selectionEnd;
	// if there is no selection?
	if (selectionEnd - selectionStart <= 0) {
		int selLine = SendEditor(SCI_LINEFROMPOSITION, selectionStart);
		int lineIndent = GetLineIndentPosition(selLine);
		int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, selLine);
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
	SendEditor(SCI_BEGINUNDOACTION);
	SendEditorString(SCI_INSERTTEXT, selectionStart, start_comment.c_str());
	selectionEnd += start_comment_length;
	selectionStart += start_comment_length;
	SendEditorString(SCI_INSERTTEXT, selectionEnd, end_comment.c_str());
	if (move_caret) {
		// moving caret to the beginning of selected block
		SendEditor(SCI_GOTOPOS, selectionEnd);
		SendEditor(SCI_SETCURRENTPOS, selectionStart);
	} else {
		SendEditor(SCI_SETSEL, selectionStart, selectionEnd);
	}
	SendEditor(SCI_ENDUNDOACTION);
	return true;
}

int SciTEBase::GetCurrentLineNumber() {
	return SendEditor(SCI_LINEFROMPOSITION,
	                  SendEditor(SCI_GETCURRENTPOS));
}

int SciTEBase::GetCurrentScrollPosition() {
	int lineDisplayTop = SendEditor(SCI_GETFIRSTVISIBLELINE);
	return SendEditor(SCI_DOCLINEFROMVISIBLE, lineDisplayTop);
}

/**
 * Set up properties for FileTime, FileDate, CurrentTime, CurrentDate and FileAttr.
 */
void SciTEBase::SetFileProperties(
    PropSet &ps) {			///< Property set to update.

	const int TEMP_LEN = 100;
	char temp[TEMP_LEN];
#if PLAT_WIN	// Use the Windows specific locale functions
	HANDLE hf = ::CreateFile(fullPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hf != INVALID_HANDLE_VALUE) {
		FILETIME ft;
		::GetFileTime(hf, NULL, NULL, &ft);
		FILETIME lft;
		::FileTimeToLocalFileTime(&ft, &lft);
		SYSTEMTIME st;
		::FileTimeToSystemTime(&lft, &st);
		::CloseHandle(hf);
		::GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
		                0, &st,
		                NULL, temp, TEMP_LEN);
		ps.Set("FileTime", temp);

		::GetDateFormat(LOCALE_SYSTEM_DEFAULT,
		                DATE_SHORTDATE, &st,
		                NULL, temp, TEMP_LEN);
		ps.Set("FileDate", temp);

		DWORD attr = GetFileAttributes(fullPath);
		SString fa;
		if (attr & FILE_ATTRIBUTE_READONLY) {
			fa += "R";
		}
		if (attr & FILE_ATTRIBUTE_HIDDEN) {
			fa += "H";
		}
		if (attr & FILE_ATTRIBUTE_SYSTEM) {
			fa += "S";
		}
		ps.Set("FileAttr", fa.c_str());
	}

	::GetDateFormat(LOCALE_SYSTEM_DEFAULT,
	                DATE_SHORTDATE, NULL,   	// Current date
	                NULL, temp, TEMP_LEN);
	ps.Set("CurrentDate", temp);

	::GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
	                0, NULL,   	// Current time
	                NULL, temp, TEMP_LEN);
	ps.Set("CurrentTime", temp);
#endif  	// PLAT_WIN
#if PLAT_GTK	// Could use Unix standard calls here, someone less lazy than me (PL) should do it.
	// Or just declare the function in SciTEBase.h and define it in SciTEWin.cxx and SciTEGTK.cxx?
	temp[0] = '\0';
	/* If file exists */
	{
		ps.Set("FileTime", temp);
		ps.Set("FileDate", temp);
		ps.Set("FileAttr", temp);
	}
	ps.Set("CurrentDate", temp);
	ps.Set("CurrentTime", temp);
#endif
}

/**
 * Set up properties for EOLMode, BufferLength, NbOfLines, SelLength.
 */
void SciTEBase::SetTextProperties(
    PropSet &ps) {			///< Property set to update.

	const int TEMP_LEN = 100;
	char temp[TEMP_LEN];

	int eolMode = SendEditor(SCI_GETEOLMODE);
	ps.Set("EOLMode", eolMode == SC_EOL_CRLF ? "CR+LF" : (eolMode == SC_EOL_LF ? "LF" : "CR"));

	sprintf(temp, "%d", LengthDocument());
	ps.Set("BufferLength", temp);

	sprintf(temp, "%d", static_cast<int>(SendEditor(SCI_GETLINECOUNT)));
	ps.Set("NbOfLines", temp);

	CharacterRange crange = GetSelection();
	sprintf(temp, "%ld", crange.cpMax - crange.cpMin);
	ps.Set("SelLength", temp);
}

void SciTEBase::UpdateStatusBar(bool bUpdateSlowData) {
	if (sbVisible) {
#ifdef OLD
		SString msg;
		int caretPos = SendEditor(SCI_GETCURRENTPOS);
		int caretLine = SendEditor(SCI_LINEFROMPOSITION, caretPos);
		int caretColumn = SendEditor(SCI_GETCOLUMN, caretPos);
		msg = "Column=";
		msg += SString(caretColumn + 1).c_str();
		msg += "    Line=";
		msg += SString(caretLine + 1).c_str();
		msg += SendEditor(SCI_GETOVERTYPE) ? "    OVR" : "    INS";
		if (sbValue != msg) {
			SetStatusBarText(msg.c_str());
			sbValue = msg;
		}
#else
		char tmp[32];
		if (bUpdateSlowData) {
			SetFileProperties(propsStatus);
		}
		SetTextProperties(propsStatus);
		int caretPos = SendEditor(SCI_GETCURRENTPOS);
		sprintf(tmp, "%d", static_cast<int>(SendEditor(SCI_LINEFROMPOSITION, caretPos)) + 1);
		propsStatus.Set("LineNumber", tmp);
		sprintf(tmp, "%d", static_cast<int>(SendEditor(SCI_GETCOLUMN, caretPos)) + 1);
		propsStatus.Set("ColumnNumber", tmp);
		propsStatus.Set("OverType", SendEditor(SCI_GETOVERTYPE) ? "OVR" : "INS");

		char sbKey[32];
		sprintf(sbKey, "statusbar.text.%d", sbNum);
		SString msg = propsStatus.GetExpanded(sbKey);
		if (msg.size() && sbValue != msg) {	// To avoid flickering, update only if needed
			SetStatusBarText(msg.c_str());
			sbValue = msg;
		}
#endif
	} else {
		sbValue = "";
	}
}

int SciTEBase::SetLineIndentation(int line, int indent) {
	SendEditor(SCI_SETLINEINDENTATION, line, indent);
	int pos = GetLineIndentPosition(line);
	SetSelection(pos, pos);
	return pos;
}

int SciTEBase::GetLineIndentation(int line) {
	return SendEditor(SCI_GETLINEINDENTATION, line);
}

int SciTEBase::GetLineIndentPosition(int line) {
	return SendEditor(SCI_GETLINEINDENTPOSITION, line);
}

bool SciTEBase::RangeIsAllWhitespace(int start, int end) {
	WindowAccessor acc(wEditor.GetID(), props);
	for (int i = start;i < end;i++) {
		if ((acc[i] != ' ') && (acc[i] != '\t'))
			return false;
	}
	return true;
}

void SciTEBase::GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len) {
	for (int i = 0; i < len; i++)
		sv[i] = "";
	WindowAccessor acc(wEditor.GetID(), props);
	SString s;
	int part = 0;
	int thisLineStart = SendEditor(SCI_POSITIONFROMLINE, line);
	int nextLineStart = SendEditor(SCI_POSITIONFROMLINE, line + 1);
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
		sv[part] = s;
	}
}

static bool includes(const StyleAndWords &symbols, const SString value) {
	if (symbols.words.length() == 0) {
		return false;
	} else if (strchr(symbols.words.c_str(), ' ')) {
		// Set of symbols separated by spaces
		int lenVal = value.length();
		const char *symbol = symbols.words.c_str();
		while (symbol) {
			const char *symbolEnd = strchr(symbol, ' ');
			int lenSymbol = strlen(symbol);
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

#define ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))

int SciTEBase::GetIndentState(int line) {
	// C like language indentation defined by braces and keywords
	int indentState = 0;
	SString controlWords[10];
	GetLinePartsInStyle(line, SCE_C_WORD, -1, controlWords, ELEMENTS(controlWords));
	for (unsigned int i = 0; i < ELEMENTS(controlWords); i++) {
		if (includes(statementIndent, controlWords[i]))
			indentState = 2;
	}
	// Braces override keywords
	SString controlStrings[10];
	GetLinePartsInStyle(line, SCE_C_OPERATOR, -1, controlStrings, ELEMENTS(controlStrings));
	for (unsigned int j = 0; j < ELEMENTS(controlStrings); j++) {
		if (includes(blockEnd, controlStrings[j]))
			indentState = -1;
		if (includes(blockStart, controlStrings[j]))
			indentState = 1;
	}
	return indentState;
}

void SciTEBase::AutomaticIndentation(char ch) {
	CharacterRange crange = GetSelection();
	int selStart = crange.cpMin;
	int curLine = GetCurrentLineNumber();
	int thisLineStart = SendEditor(SCI_POSITIONFROMLINE, curLine);
	int indent = GetLineIndentation(curLine - 1);
	int indentBlock = indent;
	int backLine = curLine - 1;
	int indentState = 0;
	if (statementIndent.IsEmpty() && blockStart.IsEmpty() && blockEnd.IsEmpty())
		indentState = 1;	// Don't bother searching backwards

	int lineLimit = curLine - statementLookback;
	if (lineLimit < 0)
		lineLimit = 0;
	while ((backLine >= lineLimit) && (indentState == 0)) {
		indentState = GetIndentState(backLine);
		if (indentState != 0) {
			indentBlock = GetLineIndentation(backLine);
			if (indentState == 1) {
				if (!indentOpening)
					indentBlock += indentSize;
			}
			if (indentState == -1) {
				if (indentClosing)
					indentBlock -= indentSize;
				if (indentBlock < 0)
					indentBlock = 0;
			}
			if ((indentState == 2) && (backLine == (curLine - 1)))
				indentBlock += indentSize;
		}
		backLine--;
	}
	if (ch == blockEnd.words[0]) {	// Dedent maybe
		if (!indentClosing) {
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '}'
				SetSelection(pos + 1, pos + 1);
			}
		}
	} else if (ch == blockStart.words[0]) {	// Dedent maybe if first on line
		if (!indentOpening) {
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '{'
				SetSelection(pos + 1, pos + 1);
			}
		}
	} else if ((ch == '\r' || ch == '\n') && (selStart == thisLineStart)) {
		SetLineIndentation(curLine, indentBlock);
	}
}

/**
 * Upon a character being added, SciTE may decide to perform some action
 * such as displaying a completion list.
 */
void SciTEBase::CharAdded(char ch) {
	if (recording)
		return;
	CharacterRange crange = GetSelection();
	int selStart = crange.cpMin;
	int selEnd = crange.cpMax;
	if ((selEnd == selStart) && (selStart > 0)) {
		int style = SendEditor(SCI_GETSTYLEAT, selStart - 1, 0);
		//Platform::DebugPrintf("Char added %d style = %d %d\n", ch, style, braceCount);
		if (style != 1) {
			if (SendEditor(SCI_CALLTIPACTIVE)) {
				if (ch == ')') {
					braceCount--;
					if (braceCount < 1)
						SendEditor(SCI_CALLTIPCANCEL);
				} else if (ch == '(') {
					braceCount++;
				} else {
					ContinueCallTip();
				}
			} else if (SendEditor(SCI_AUTOCACTIVE)) {
				if (ch == '(') {
					braceCount++;
					StartCallTip();
				} else if (ch == ')') {
					braceCount--;
				} else if (!wordCharacters.contains(ch)) {
					SendEditor(SCI_AUTOCCANCEL);
				} else if (autoCCausedByOnlyOne) {
					StartAutoCompleteWord(true);
				}
			} else {
				if (ch == '(') {
					braceCount = 1;
					StartCallTip();
				} else {
					autoCCausedByOnlyOne = false;
					if (props.GetInt("indent.automatic"))
						AutomaticIndentation(ch);
					if (autoCompleteStartCharacters.contains(ch)) {
						StartAutoComplete();
					} else if (props.GetInt("autocompleteword.automatic") && wordCharacters.contains(ch)) {
						StartAutoCompleteWord(true);
						autoCCausedByOnlyOne = SendEditor(SCI_AUTOCACTIVE);
					}
				}
			}
		}
	}
}

void SciTEBase::GoMatchingBrace(bool select) {
	int braceAtCaret = -1;
	int braceOpposite = -1;
	bool isInside = FindMatchingBracePosition(true, braceAtCaret, braceOpposite, true);
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
		EnsureRangeVisible(braceOpposite, braceOpposite);
		if (select) {
			SetSelection(braceAtCaret, braceOpposite);
		} else {
			SetSelection(braceOpposite, braceOpposite);
		}
	}
}

// Text	ConditionalUp	Ctrl+J	Finds the previous matching preprocessor condition
// Text	ConditionalDown	Ctrl+K	Finds the next matching preprocessor condition
void SciTEBase::GoMatchingPreprocCond(int direction, bool select) {
	int mppcAtCaret = SendEditor(SCI_GETCURRENTPOS);
	int mppcMatch = -1;
	int forward = (direction == IDM_NEXTMATCHPPC);
	bool isInside = FindMatchingPreprocCondPosition(forward, mppcAtCaret, mppcMatch);

	if (isInside && mppcMatch >= 0) {
		EnsureRangeVisible(mppcMatch, mppcMatch);
		if (select) {
			// Selection changes the rules a bit...
			int selStart = SendEditor(SCI_GETSELECTIONSTART);
			int selEnd = SendEditor(SCI_GETSELECTIONEND);
			// pivot isn't the caret position but the opposite (if there is a selection)
			int pivot = (mppcAtCaret == selStart ? selEnd : selStart);
			if (forward) {
				// Caret goes one line beyond the target, to allow selecting the whole line
				int lineNb = SendEditor(SCI_LINEFROMPOSITION, mppcMatch);
				mppcMatch = SendEditor(SCI_POSITIONFROMLINE, lineNb + 1);
			}
			SetSelection(pivot, mppcMatch);
		} else {
			SetSelection(mppcMatch, mppcMatch);
		}
	} else {
		WarnUser(warnNotFound);
	}
}

void SciTEBase::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool) {
	if (commandCurrent == 0)
		jobUsesOutputPane = false;
	if (cmd.length()) {
		jobQueue[commandCurrent].command = cmd;
		jobQueue[commandCurrent].directory = dir;
		jobQueue[commandCurrent].jobType = jobType;
		commandCurrent++;
		if ((jobType == jobCLI) || (jobType == jobExtension))
			jobUsesOutputPane = true;
	}
}

int ControlIDOfCommand(unsigned long wParam) {
	return wParam & 0xffff;
}

void SciTEBase::MenuCommand(int cmdID) {
	switch (cmdID) {
	case IDM_NEW:
		// For the New command, the "are you sure" question is always asked as this gives
		// an opportunity to abandon the edits made to a file when are.you.sure is turned off.

		if (CanMakeRoom()) {
			New();
			ReadProperties();
			useMonoFont = false;
			UpdateStatusBar(true);
		}
		break;
	case IDM_OPEN:
		// No need to see if can make room as that will occur
		// when doing the opening. Must be done there as user
		// may decide to open multiple files so do not know yet
		// how much room needed.
		OpenDialog();
		SetFocus(wEditor.GetID());
		break;
	case IDM_OPENSELECTED:
		OpenSelected();
		SetFocus(wEditor.GetID());
		break;
	case IDM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_CLOSEALL:
		CloseAllBuffers();
		break;
	case IDM_SAVE:
		Save();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEAS:
		SaveAs();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEASHTML:
		SaveAsHTML();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEASRTF:
		SaveAsRTF();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEASPDF:
		SaveAsPDF();
		SetFocus(wEditor.GetID());
		break;
	case IDM_REVERT:
		if (SaveIfUnsure() != IDCANCEL) {
			Revert();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_PRINT:
		Print(true);
		break;
	case IDM_PRINTSETUP:
		PrintSetup();
		break;
	case IDM_LOADSESSION:
		LoadSessionDialog();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVESESSION:
		SaveSessionDialog();
		SetFocus(wEditor.GetID());
		break;
	case IDM_ABOUT:
		AboutDialog();
		break;
	case IDM_QUIT:
		QuitProgram();
		break;
	case IDM_NEXTFILE:
		if (buffers.size > 1) {
			Prev(); // Use Prev to tabs move left-to-right
			SetFocus(wEditor.GetID());
		} else {
			// Not using buffers - switch to next file on MRU
			if (SaveIfUnsure() != IDCANCEL)
				StackMenuNext();
		}
		break;
	case IDM_PREVFILE:
		if (buffers.size > 1) {
			Next(); // Use Next to tabs move right-to-left
			SetFocus(wEditor.GetID());
		} else {
			// Not using buffers - switch to previous file on MRU
			if (SaveIfUnsure() != IDCANCEL)
				StackMenuPrev();
		}
		break;

	case IDM_UNDO:
		SendFocused(SCI_UNDO);
		CheckMenus();
		break;
	case IDM_REDO:
		SendFocused(SCI_REDO);
		CheckMenus();
		break;

	case IDM_CUT:
		SendFocused(SCI_CUT);
		break;
	case IDM_COPY:
		SendFocused(SCI_COPY);
		break;
	case IDM_PASTE:
		SendFocused(SCI_PASTE);
		break;
	case IDM_CLEAR:
		SendFocused(SCI_CLEAR);
		break;
	case IDM_SELECTALL:
		SendFocused(SCI_SELECTALL);
		break;
	case IDM_COPYASRTF:
		CopyAsRTF();
		break;

	case IDM_FIND:
		Find();
		break;

	case IDM_FINDNEXT:
		FindNext(reverseFind);
		break;

	case IDM_FINDNEXTBACK:
		FindNext(!reverseFind);
		break;

	case IDM_FINDNEXTSEL:
		SelectionIntoFind();
		FindNext(reverseFind);
		break;

	case IDM_FINDNEXTBACKSEL:
		SelectionIntoFind();
		FindNext(!reverseFind);
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
		StartCallTip();
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
		StartExpandAbbreviation();
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
		SendFocused(SCI_UPPERCASE);
		break;

	case IDM_LWRCASE:
		SendFocused(SCI_LOWERCASE);
		break;

	case IDM_EXPAND:
		SendEditor(SCI_TOGGLEFOLD, GetCurrentLineNumber());
		break;

	case IDM_SPLITVERTICAL:
		splitVertical = !splitVertical;
		heightOutput = NormaliseSplit(heightOutput);
		SizeSubWindows();
		CheckMenus();
		Redraw();
		break;

	case IDM_LINENUMBERMARGIN:
		lineNumbers = !lineNumbers;
		SendEditor(SCI_SETMARGINWIDTHN, 0, lineNumbers ? lineNumbersWidth : 0);
		CheckMenus();
		break;

	case IDM_SELMARGIN:
		margin = !margin;
		SendEditor(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);
		CheckMenus();
		break;

	case IDM_FOLDMARGIN:
		foldMargin = !foldMargin;
		SendEditor(SCI_SETMARGINWIDTHN, 2, foldMargin ? foldMarginWidth : 0);
		CheckMenus();
		break;

	case IDM_VIEWEOL:
		SendEditor(SCI_SETVIEWEOL, !SendEditor(SCI_GETVIEWEOL));
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
		SendOutput(SCI_CLEARALL);
		break;

	case IDM_EOL_CRLF:
		SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
		CheckMenus();
		break;

	case IDM_EOL_CR:
		SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
		CheckMenus();
		break;
	case IDM_EOL_LF:
		SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
		CheckMenus();
		break;
	case IDM_EOL_CONVERT:
		SendEditor(SCI_CONVERTEOLS, SendEditor(SCI_GETEOLMODE));
		break;

	case IDM_VIEWSPACE:
		ViewWhitespace(!SendEditor(SCI_GETVIEWWS));
		CheckMenus();
		Redraw();
		break;

	case IDM_VIEWGUIDES: {
			int viewIG = SendEditor(SCI_GETINDENTATIONGUIDES, 0, 0);
			SendEditor(SCI_SETINDENTATIONGUIDES, !viewIG);
			CheckMenus();
			Redraw();
		}
		break;

	case IDM_COMPILE: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(props.GetNewExpand("command.compile.", fileName), "",
				           SubsystemType("command.compile.subsystem."));
				if (commandCurrent > 0)
					Execute();
			}
		}
		break;

	case IDM_BUILD: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(
				    props.GetNewExpand("command.build.", fileName),
				    props.GetNewExpand("command.build.directory.", fileName),
				    SubsystemType("command.build.subsystem."));
				if (commandCurrent > 0) {
					isBuilding = true;
					Execute();
				}
			}
		}
		break;

	case IDM_GO: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				bool forceQueue = false;
				if (!isBuilt) {
					SString buildcmd = props.GetNewExpand("command.go.needs.", fileName);
					AddCommand(buildcmd, "",
					           SubsystemType("command.go.needs.subsystem."));
					if (buildcmd.length() > 0) {
						isBuilding = true;
						forceQueue = true;
					}
				}
				AddCommand(props.GetNewExpand("command.go.", fileName), "",
				           SubsystemType("command.go.subsystem."), forceQueue);
				if (commandCurrent > 0)
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
		GoMessage( -1);
		break;

	case IDM_OPENLOCALPROPERTIES:
		OpenProperties(IDM_OPENLOCALPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_OPENUSERPROPERTIES:
		OpenProperties(IDM_OPENUSERPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_OPENGLOBALPROPERTIES:
		OpenProperties(IDM_OPENGLOBALPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_OPENABBREVPROPERTIES:
		OpenProperties(IDM_OPENABBREVPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_SRCWIN:
		break;

	case IDM_BOOKMARK_TOGGLE:
		BookmarkToggle();
		break;

	case IDM_BOOKMARK_NEXT:
		BookmarkNext();
		break;

	case IDM_BOOKMARK_PREV:
		BookmarkPrev();
		break;

	case IDM_BOOKMARK_CLEARALL:
		SendEditor(SCI_MARKERDELETEALL, SciTE_MARKER_BOOKMARK);
		break;

	case IDM_TABSIZE:
		TabSizeDialog();
		break;

	case IDM_MONOFONT:
		useMonoFont = !useMonoFont;
		SetMonoFont();
		break;

	case IDM_LEXER_NONE:
	case IDM_LEXER_CPP:
	case IDM_LEXER_VB:
	case IDM_LEXER_RC:
	case IDM_LEXER_HTML:
	case IDM_LEXER_XML:
	case IDM_LEXER_JS:
	case IDM_LEXER_WSCRIPT:
	case IDM_LEXER_PROPS:
	case IDM_LEXER_BATCH:
	case IDM_LEXER_MAKE:
	case IDM_LEXER_ERRORL:
	case IDM_LEXER_JAVA:
	case IDM_LEXER_LUA:
	case IDM_LEXER_PYTHON:
	case IDM_LEXER_PERL:
	case IDM_LEXER_SQL:
	case IDM_LEXER_PLSQL:
	case IDM_LEXER_PHP:
	case IDM_LEXER_LATEX:
	case IDM_LEXER_DIFF:
	case IDM_LEXER_CS:
	case IDM_LEXER_CONF:
	case IDM_LEXER_PASCAL:
	case IDM_LEXER_AVE:
	case IDM_LEXER_ADA:
	case IDM_LEXER_LISP:
	case IDM_LEXER_RUBY:
	case IDM_LEXER_EIFFEL:
		SetOverrideLanguage(cmdID);
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
			AddCommand(props.GetNewExpand("command.help.", fileName), "",
			           SubsystemType("command.help.subsystem."));
			if (commandCurrent > 0) {
				isBuilding = true;
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
			if (CanMakeRoom()) {
				StackMenu(cmdID - fileStackCmdID);
			}
		} else if (cmdID >= importCmdID &&
		           (cmdID < importCmdID + importMax)) {
			if (CanMakeRoom()) {
				ImportMenu(cmdID - importCmdID);
			}
		} else if (cmdID >= IDM_TOOLS && cmdID < IDM_TOOLS + 10) {
			ToolsMenu(cmdID - IDM_TOOLS);
		}
		break;
	}
}

void SciTEBase::FoldChanged(int line, int levelNow, int levelPrev) {
	//Platform::DebugPrintf("Fold %d %x->%x\n", line, levelPrev, levelNow);
	if (levelNow & SC_FOLDLEVELHEADERFLAG) {
		if (!(levelPrev & SC_FOLDLEVELHEADERFLAG)) {
			SendEditor(SCI_SETFOLDEXPANDED, line, 1);
		}
	} else if (levelPrev & SC_FOLDLEVELHEADERFLAG) {
		//Platform::DebugPrintf("Fold removed %d-%d\n", line, SendEditor(SCI_GETLASTCHILD, line));
		if (!SendEditor(SCI_GETFOLDEXPANDED, line)) {
			// Removing the fold from one that has been contracted so should expand
			// otherwise lines are left invisible with no way to make them visible
			Expand(line, true, false, 0, levelPrev);
		}
	}
}

void SciTEBase::Expand(int &line, bool doExpand, bool force, int visLevels, int level) {
	int lineMaxSubord = SendEditor(SCI_GETLASTCHILD, line, level & SC_FOLDLEVELNUMBERMASK);
	line++;
	while (line <= lineMaxSubord) {
		if (force) {
			if (visLevels > 0)
				SendEditor(SCI_SHOWLINES, line, line);
			else
				SendEditor(SCI_HIDELINES, line, line);
		} else {
			if (doExpand)
				SendEditor(SCI_SHOWLINES, line, line);
		}
		int levelLine = level;
		if (levelLine == -1)
			levelLine = SendEditor(SCI_GETFOLDLEVEL, line);
		if (levelLine & SC_FOLDLEVELHEADERFLAG) {
			if (force) {
				if (visLevels > 1)
					SendEditor(SCI_SETFOLDEXPANDED, line, 1);
				else
					SendEditor(SCI_SETFOLDEXPANDED, line, 0);
				Expand(line, doExpand, force, visLevels - 1);
			} else {
				if (doExpand) {
					if (!SendEditor(SCI_GETFOLDEXPANDED, line))
						SendEditor(SCI_SETFOLDEXPANDED, line, 1);
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
	SendEditor(SCI_COLOURISE, 0, -1);
	int maxLine = SendEditor(SCI_GETLINECOUNT);
	bool expanding = true;
	for (int lineSeek = 0; lineSeek < maxLine; lineSeek++) {
		if (SendEditor(SCI_GETFOLDLEVEL, lineSeek) & SC_FOLDLEVELHEADERFLAG) {
			expanding = !SendEditor(SCI_GETFOLDEXPANDED, lineSeek);
			break;
		}
	}
	for (int line = 0; line < maxLine; line++) {
		int level = SendEditor(SCI_GETFOLDLEVEL, line);
		if ((level & SC_FOLDLEVELHEADERFLAG) &&
		        (SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK))) {
			if (expanding) {
				SendEditor(SCI_SETFOLDEXPANDED, line, 1);
				Expand(line, true, false, 0, level);
				line--;
			} else {
				int lineMaxSubord = SendEditor(SCI_GETLASTCHILD, line, -1);
				SendEditor(SCI_SETFOLDEXPANDED, line, 0);
				if (lineMaxSubord > line)
					SendEditor(SCI_HIDELINES, line + 1, lineMaxSubord);
			}
		}
	}
}

void SciTEBase::GotoLineEnsureVisible(int line) {
	SendEditor(SCI_ENSUREVISIBLE, line);
	SendEditor(SCI_GOTOLINE, line);
}

void SciTEBase::EnsureRangeVisible(int posStart, int posEnd) {
	int lineStart = SendEditor(SCI_LINEFROMPOSITION, Platform::Minimum(posStart, posEnd));
	int lineEnd = SendEditor(SCI_LINEFROMPOSITION, Platform::Maximum(posStart, posEnd));
	for (int line = lineStart; line <= lineEnd; line++) {
		SendEditor(SCI_ENSUREVISIBLE, line);
	}
}

bool SciTEBase::MarginClick(int position, int modifiers) {
	int lineClick = SendEditor(SCI_LINEFROMPOSITION, position);
	//Platform::DebugPrintf("Margin click %d %d %x\n", position, lineClick,
	//	SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG);
	if ((modifiers & SCMOD_SHIFT) && (modifiers & SCMOD_CTRL)) {
		FoldAll();
	} else {
		int levelClick = SendEditor(SCI_GETFOLDLEVEL, lineClick);
		if (levelClick & SC_FOLDLEVELHEADERFLAG) {
			if (modifiers & SCMOD_SHIFT) {
				// Ensure all children visible
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
				Expand(lineClick, true, true, 100, levelClick);
			} else if (modifiers & SCMOD_CTRL) {
				if (SendEditor(SCI_GETFOLDEXPANDED, lineClick)) {
					// Contract this line and all children
					SendEditor(SCI_SETFOLDEXPANDED, lineClick, 0);
					Expand(lineClick, false, true, 0, levelClick);
				} else {
					// Expand this line and all children
					SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
					Expand(lineClick, true, true, 100, levelClick);
				}
			} else {
				// Toggle this line
				SendEditor(SCI_TOGGLEFOLD, lineClick);
			}
		}
	}
	return true;
}

void SciTEBase::Notify(SCNotification *notification) {
	bool handled = false;
	//Platform::DebugPrintf("Notify %d\n", notification->nmhdr.code);
	switch (notification->nmhdr.code) {
	case SCEN_SETFOCUS:
	case SCEN_KILLFOCUS:
		CheckMenus();
		break;

	case SCN_STYLENEEDED: {
			if (extender) {
				// Colourisation may be performed by script
				if ((notification->nmhdr.idFrom == IDM_SRCWIN) && (lexLanguage == SCLEX_CONTAINER)) {
					int endStyled = SendEditor(SCI_GETENDSTYLED);
					int lineEndStyled = SendEditor(SCI_LINEFROMPOSITION, endStyled);
					endStyled = SendEditor(SCI_POSITIONFROMLINE, lineEndStyled);
					WindowAccessor styler(wEditor.GetID(), props);
					int styleStart = 0;
					if (endStyled > 0)
						styleStart = styler.StyleAt(endStyled - 1);
					styler.SetCodePage(codePage);
					extender->OnStyle(endStyled, notification->position - endStyled,
					                  styleStart, &styler);
					styler.Flush();
				}
			}
			// Colourisation is now normally performed by the SciLexer DLL
#ifdef OLD_CODE
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				int endStyled = SendEditor(SCI_GETENDSTYLED);
				int lineEndStyled = SendEditor(SCI_LINEFROMPOSITION, endStyled);
				endStyled = SendEditor(SCI_POSITIONFROMLINE, lineEndStyled);
				Colourise(endStyled, notification->position);
			} else {
				int endStyled = SendOutput(SCI_GETENDSTYLED);
				int lineEndStyled = SendOutput(SCI_LINEFROMPOSITION, endStyled);
				endStyled = SendOutput(SCI_POSITIONFROMLINE, lineEndStyled);
				Colourise(endStyled, notification->position, false);
			}
#endif
		}
		break;

	case SCN_CHARADDED:
		if (extender)
			handled = extender->OnChar(static_cast<char>(notification->ch));
		if (!handled)
			CharAdded(static_cast<char>(notification->ch));
		break;

	case SCN_SAVEPOINTREACHED:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointReached();
			if (!handled) {
				isDirty = false;
			}
		}
		CheckMenus();
		SetWindowName();
		BuffersMenu();
		break;

	case SCN_SAVEPOINTLEFT:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointLeft();
			if (!handled) {
				isDirty = true;
				isBuilt = false;
			}
		}
		CheckMenus();
		SetWindowName();
		BuffersMenu();
		break;

	case SCN_DOUBLECLICK:
		if (extender)
			handled = extender->OnDoubleClick();
		if (!handled && notification->nmhdr.idFrom == IDM_RUNWIN) {
			//Platform::DebugPrintf("Double click 0\n");
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
		}
		break;

	case SCN_MODIFIED:
		if (notification->modificationType == SC_MOD_CHANGEFOLD) {
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
			EnsureRangeVisible(notification->position, notification->position + notification->length);
		}
		break;

	case SCN_USERLISTSELECTION: {
			if (notification->wParam == 2)
				ContinueMacroList(notification->text);
		}
		break;

	case SCN_MACRORECORD:
		RecordMacroCommand(notification);
		break;

	case SCN_URIDROPPED:
		OpenUriList(notification->text);
		break;
	}
}

void SciTEBase::CheckMenus() {
	EnableAMenuItem(IDM_SAVE, isDirty);
	EnableAMenuItem(IDM_UNDO, SendFocused(SCI_CANUNDO));
	EnableAMenuItem(IDM_REDO, SendFocused(SCI_CANREDO));
	EnableAMenuItem(IDM_PASTE, SendFocused(SCI_CANPASTE));
	EnableAMenuItem(IDM_FINDINFILES, !executing);
	EnableAMenuItem(IDM_SHOWCALLTIP, apis != 0);
	EnableAMenuItem(IDM_COMPLETE, apis != 0);
	CheckAMenuItem(IDM_SPLITVERTICAL, splitVertical);
	CheckAMenuItem(IDM_FULLSCREEN, fullScreen);
	CheckAMenuItem(IDM_VIEWTOOLBAR, tbVisible);
	CheckAMenuItem(IDM_VIEWTABBAR, tabVisible);
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	CheckAMenuItem(IDM_VIEWEOL, SendEditor(SCI_GETVIEWEOL));
	CheckAMenuItem(IDM_VIEWSPACE, SendEditor(SCI_GETVIEWWS));
	CheckAMenuItem(IDM_VIEWGUIDES, SendEditor(SCI_GETINDENTATIONGUIDES));
	CheckAMenuItem(IDM_LINENUMBERMARGIN, lineNumbers);
	CheckAMenuItem(IDM_SELMARGIN, margin);
	CheckAMenuItem(IDM_FOLDMARGIN, foldMargin);
	CheckAMenuItem(IDM_TOGGLEOUTPUT, heightOutput > 0);
	CheckAMenuItem(IDM_MONOFONT, useMonoFont);
	EnableAMenuItem(IDM_COMPILE, !executing);
	EnableAMenuItem(IDM_BUILD, !executing);
	EnableAMenuItem(IDM_GO, !executing);
	for (int toolItem = 0; toolItem < toolMax; toolItem++)
		EnableAMenuItem(IDM_TOOLS + toolItem, !executing);
	EnableAMenuItem(IDM_STOPEXECUTE, executing);
	if (buffers.size > 0) {
#if PLAT_WIN
		// Tab Bar
#ifndef TCM_DESELECTALL
#define TCM_DESELECTALL TCM_FIRST+50
#endif
		::SendMessage(wTabBar.GetID(), TCM_DESELECTALL, (WPARAM)0, (LPARAM)0);
		::SendMessage(wTabBar.GetID(), TCM_SETCURSEL, (WPARAM)buffers.current, (LPARAM)0);
#endif
		for (int bufferItem = 0; bufferItem < buffers.length; bufferItem++) {
			CheckAMenuItem(IDM_BUFFER + bufferItem, bufferItem == buffers.current);
		}
	}
	EnableAMenuItem(IDM_MACROPLAY, !recording);
	EnableAMenuItem(IDM_MACRORECORD, !recording);
	EnableAMenuItem(IDM_MACROSTOPRECORD, recording);
}

/**
 * Ensure that a splitter bar position is inside the main window.
 */
int SciTEBase::NormaliseSplit(int splitPos) {
	PRectangle rcClient = GetClientRectangle();
	int w = rcClient.Width();
	int h = rcClient.Height();
	if (splitPos < 20)
		splitPos = heightBar - heightBar;
	if (splitVertical) {
		if (splitPos > w - heightBar - 20)
			splitPos = w - heightBar;
	} else {
		if (splitPos > h - heightBar - 20)
			splitPos = h - heightBar;
	}
	return splitPos;
}

void SciTEBase::MoveSplit(Point ptNewDrag) {
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

void SciTEBase::UIAvailable() {
	SetImportMenu();
	if (extender)
		extender->Initialise(this);
}

/**
 * Find the character following a name which is made up of characters from
 * the set [a-zA-Z.]
 */
char AfterName(const char *s) {
	while (*s && ((*s == '.') ||
	              (*s >= 'a' && *s <= 'z') ||
	              (*s >= 'A' && *s <= 'A')))
		s++;
	return *s;
}

void SciTEBase::PerformOne(char *action) {
	unsigned int len = UnSlash(action);
	char *arg = strchr(action, ':');
	if (arg) {
		arg++;
		if (isprefix(action, "open:")) {
			Open(arg);
		} else if (isprefix(action, "enumproperties:")) {
			EnumProperties(arg);
		} else if (isprefix(action, "property:")) {
			PropertyFromDirector(arg);
		} else if (isprefix(action, "goto:")) {
			GotoLineEnsureVisible(atoi(arg) - 1);
		} else if (isprefix(action, "find:")) {
			strncpy(findWhat, arg, sizeof(findWhat));
			findWhat[sizeof(findWhat) - 1] = '\0';
			FindNext(false, false);
		} else if (isprefix(action, "macroenable:")) {
			macrosEnabled = atoi(arg);
			SetToolsMenu();
		} else if (isprefix(action, "macrolist:")) {
			StartMacroList(arg);
		} else if (isprefix(action, "currentmacro:")) {
			strcpy(currentmacro, arg);
		} else if (isprefix(action, "macrocommand:")) {
			ExecuteMacroCommand(arg);
		} else if (isprefix(action, "askfilename:")) {
			extender->OnMacro("filename", fullPath);
		} else if (isprefix(action, "insert:")) {
			SendEditorString(SCI_REPLACESEL, 0, arg);
		} else if (isprefix(action, "replaceall:")) {
			if (len > strlen(action)) {
				char *arg2 = arg + strlen(arg) + 1;
				strcpy(findWhat, arg);
				strcpy(replaceWhat, arg2);
				ReplaceAll(false);
			}
		} else if (isprefix(action, "saveas:")) {
			SaveAs(arg);
		} else if (isprefix(action, "close:")) {
			Close();
			SetFocus(wEditor.GetID());
		} else if (isprefix(action, "quit:")) {
			QuitProgram();
		} else if (isprefix(action, "exportashtml:")) {
			SaveToHTML(arg);
		} else if (isprefix(action, "exportasrtf:")) {
			SaveToRTF(arg);
		} else if (isprefix(action, "exportaspdf:")) {
			SaveToPDF(arg);
		} else if (isprefix(action, "menucommand:")) {
			MenuCommand(atoi(arg));
		}
	}
}

static bool IsSwitchCharacter(char ch) {
#ifdef unix
	return ch == '-';
#else
	return (ch == '-') || (ch == '/');
#endif
}

// Called by SciTEBase::PerformOne when action="enumproperties:"
void SciTEBase::EnumProperties(const char *propkind) {
	char *key = NULL;
	char *val = NULL;
	PropSetFile *pf = NULL;

	if (!extender)
		return;
	if (!strcmp(propkind, "dyn"))
		pf = &props;
	else if (!strcmp(propkind, "local"))
		pf = &propsLocal;
	else if (!strcmp(propkind, "user"))
		pf = &propsUser;
	else if (!strcmp(propkind, "base"))
		pf = &propsBase;
	else if (!strcmp(propkind, "embed"))
		pf = &propsEmbed;
	else if (!strcmp(propkind, "abbrev"))
		pf = &propsAbbrev;

	if (pf != NULL) {
		bool b = pf->GetFirst(&key, &val);
		while (b) {
			SendOneProperty(propkind, key, val);
			b = pf->GetNext(&key, &val);
		}
	}
}

void SciTEBase::SendOneProperty(const char *kind, const char *key, const char *val) {
	int keysize = strlen(kind) + 1 + strlen(key) + 1 + strlen(val) + 1;
	char *m = new char[keysize];
	if (m) {
		strcpy(m, kind);
		strcat(m, ":");
		strcat(m, key);
		strcat(m, "=");
		strcat(m, val);
		extender->SendProperty(m);
		delete []m;
	}
}

void SciTEBase::PropertyFromDirector(const char *arg) {
	props.Set(arg);
}

/**
 * Menu/Toolbar command "Record".
 */
void SciTEBase::StartRecordMacro() {
	recording = TRUE;
	CheckMenus();
	SendEditor(SCI_STARTRECORD);
}

/**
 * Received a SCN_MACRORECORD from Scintilla: send it to director.
 */
bool SciTEBase::RecordMacroCommand(SCNotification *notification) {
	if (extender) {
		char *szMessage;
		char *t;
		bool handled;
		t = (char*)(notification->lParam);
		if (t != NULL) {
			//format : "<message>;<wParam>;1;<text>"
			szMessage = new char[50 + strlen(t) + 4];
			sprintf(szMessage, "%d;%ld;1;%s", notification->message, notification->wParam, t);
		} else {
			//format : "<message>;<wParam>;0;"
			szMessage = new char[50];
			sprintf(szMessage, "%d;%ld;0;", notification->message, notification->wParam);
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
	SendEditor(SCI_STOPRECORD);
	if (extender)
		extender->OnMacro("macro:stoprecord", "");
	recording = FALSE;
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
		SendEditorString(SCI_USERLISTSHOW, 2, words);//listtype=2
	}

	return true;
}

/**
 * User has chosen a macro in the list. Ask director to execute it.
 */
void SciTEBase::ContinueMacroList(const char *stext) {
	if ((extender) && (*stext != '\0')) {
		strcpy(currentmacro, stext);
		StartPlayMacro();
	}
}

/**
 * Menu/Toolbar command "Play current macro" (or called from ContinueMacroList).
 */
void SciTEBase::StartPlayMacro() {
	if (extender)
		extender->OnMacro("macro:run", currentmacro);
}

/*
SciTE received a macro command from director : execute it.
If command needs answer (SCI_GETTEXTLENGTH ...) : give answer to director
*/

static uptr_t ReadNum(const char *&t) {
	const char *argend = strchr(t, ';');	// find ';'
	uptr_t v = 0;
	if (*t)
		v = atoi(t);					// read value
	t = argend + 1;					// update pointer
	return v;						// return value
}

void SciTEBase::ExecuteMacroCommand(const char *command) {
	const char *nextarg = command;
	uptr_t wParam;
	long lParam = 0;
	int rep = 0;				//Scintilla's answer
	char *answercmd;
	int l;
	char *string1 = NULL;
	char params[4];
	//params describe types of return values and of arguments
	//0 : void or no param
	//I : integer
	//S : string
	//R : return string (for lParam only)

	//extract message,wParam ,lParam

	uptr_t message = ReadNum(nextarg);
	strncpy(params, nextarg, 3);
	nextarg += 4;
	if (*(params + 1) == 'R') {
		// in one function wParam is a string  : void SetProperty(string key,string name)
		const char *s1 = nextarg;
		while (*nextarg != ';')
			nextarg++;
		int lstring1 = nextarg - s1;
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
		lParam = reinterpret_cast<long>(nextarg);
	else if (*(params + 2) == 'I')
		lParam = atoi(nextarg);

	if (*params == '0') {
		// no answer ...
		SendEditor(message, wParam, lParam);
		if (string1 != NULL)
			delete []string1;
		return;
	}

	if (*params == 'S') {
		// string answer
		if (message == SCI_GETSELTEXT) {
			l = SendEditor(SCI_GETSELECTIONEND) - SendEditor(SCI_GETSELECTIONSTART);
			wParam = 0;
		} else if (message == SCI_GETCURLINE) {
			int line = SendEditor(SCI_LINEFROMPOSITION, SendEditor(SCI_GETCURRENTPOS));
			l = SendEditor(SCI_LINELENGTH, line);
			wParam = l;
		} else if (message == SCI_GETTEXT)
			l = wParam;
		else if (message == SCI_GETLINE)
			l = SendEditor(SCI_LINELENGTH, wParam);
		else
			l = 0; //unsupported calls EM

		answercmd = "stringinfo:";

	} else {
		//int answer
		answercmd = "intinfo:";
		l = 30;
	}

	int alen = strlen(answercmd);
	char *tbuff = new char[l + alen + 1];
	strcpy(tbuff, answercmd);
	if (*params == 'S')
		lParam = reinterpret_cast<long>(tbuff + alen);

	if (l > 0)
		rep = SendEditor(message, wParam, lParam);
	if (*params == 'I')
		sprintf(tbuff + alen, "%0d", rep);
	extender->OnMacro("macro", tbuff);
	delete []tbuff;
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
bool SciTEBase::ProcessCommandLine(SString &args, int phase) {
	bool performPrint = false;
	bool performedOpen = false;
	bool evaluate = phase == 0;
	WordList wlArgs(true);
	wlArgs.Set(args.c_str());
	for (int i = 0;i < wlArgs.len;i++) {
		char *arg = wlArgs[i];
		if (IsSwitchCharacter(arg[0])) {
			arg++;
			if ((tolower(arg[0]) == 'p') && (strlen(arg) == 1)) {
				performPrint = true;
			} else {
				if (AfterName(arg) == ':') {
					if (isprefix(arg, "open:")) {
						if (phase == 0)
							return performPrint;
						else
							evaluate = true;
						performedOpen = true;
					}
					if (evaluate)
						PerformOne(arg);
				} else {
					if (evaluate)
						props.ReadLine(arg, true, "");
				}
			}
		} else {	// Not a switch: it is a file name
			if (phase == 0)
				return performPrint;
			else
				evaluate = true;
			Open(arg, true);
			performedOpen = true;
		}
	}
	if ((phase == 1) && !performedOpen) {
		Open("", true);
	}
	return performPrint;
}

// Implement ExtensionAPI methods
sptr_t SciTEBase::Send(Pane p, unsigned int msg, uptr_t wParam, sptr_t lParam) {
	if (p == paneEditor)
		return SendEditor(msg, wParam, lParam);
	else
		return SendOutput(msg, wParam, lParam);
}

char *SciTEBase::Range(Pane p, int start, int end) {
	int len = end - start;
	char *s = new char[len + 1];
	if (s) {
		if (p == paneEditor)
			GetRange(wEditor, start, end, s);
		else
			GetRange(wOutput, start, end, s);
	}
	return s;
}

void SciTEBase::Remove(Pane p, int start, int end) {
	// Should have a scintilla call for this
	if (p == paneEditor) {
		SendEditor(SCI_SETSEL, start, end);
		SendEditor(SCI_CLEAR);
	} else {
		SendOutput(SCI_SETSEL, start, end);
		SendOutput(SCI_CLEAR);
	}
}

void SciTEBase::Insert(Pane p, int pos, const char *s) {
	if (p == paneEditor)
		SendEditorString(SCI_INSERTTEXT, pos, s);
	else
		SendOutput(SCI_INSERTTEXT, pos, reinterpret_cast<long>(s));
}

void SciTEBase::Trace(const char *s) {
	OutputAppendString(s);
}

char *SciTEBase::Property(const char *key) {
	SString value = props.GetExpanded(key);
	char *retval = new char[value.length() + 1];
	if (retval)
		strcpy(retval, value.c_str());
	return retval;
}

void SciTEBase::SetProperty(const char *key, const char *val) {
	props.Set(key, val);
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
