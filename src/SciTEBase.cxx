// SciTE - Scintilla based Text Editor
// SciTEBase.cxx - platform independent base class of editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
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
#include "ScintillaWidget.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "Extender.h"
#include "SciTEBase.h"

#define SciTE_MARKER_BOOKMARK 1

const char propFileName[] = "SciTE.properties";

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
};

const char *extList[] = {
    "x", "x.cpp", "x.bas", "x.rc", "x.html", "x.xml", "x.js", "x.vbs",
    "x.properties", "x.bat", "x.mak", "x.err", "x.java", "x.lua", "x.py",
    "x.pl", "x.sql", "x.spec", "x.php3", "x.tex", "x.diff"
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
		                        reinterpret_cast<unsigned int>("new century schoolbook"));
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
		AddStyledText(wsci, "Version 1.32\n", 1);
		SetAboutStyle(wsci, 2, Colour(0, 0, 0));
		Platform::SendScintilla(wsci, SCI_STYLESETITALIC, 2, 1);
		AddStyledText(wsci, "by Neil Hodgson.\n", 2);
		SetAboutStyle(wsci, 3, Colour(0, 0, 0));
		AddStyledText(wsci, "December 1998-September 2000.\n", 3);
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

BufferList::BufferList() : buffers(0), size(0), length(0), current(0) {}


BufferList::~BufferList() {
	delete []buffers;
}

void BufferList::Allocate(int maxSize) {
	size = maxSize;
	buffers = new Buffer[size];
	length = 1;
	current = 0;
}

int BufferList::Add() {
	if (length < size)
		length++;
	buffers[length - 1].Init();
	return length - 1;
}

int BufferList::GetDocumentByName(const char *filename) {
	if (!filename || !filename[0])
		return -1;
	for (int i = 0;i < length;i++)
		if (buffers[i].fileName == filename)
			return i;
	return -1;
}

void BufferList::RemoveCurrent() {
	// Delete and move up to fill gap but ensure doc pointer is saved.
	int currentDoc = buffers[current].doc;
	for (int i = current;i < length - 1;i++)
		buffers[i] = buffers[i + 1];
	buffers[length - 1].doc = currentDoc;
	if (length > 1) {
		length--;
		buffers[length].Init();
		if (current >= length)
			current = length - 1;
		if (current < 0)
			current = 0;
	} else {
		buffers[current].Init();
	}
}

Job::Job() {
	Clear();
}

void Job::Clear() {
	command = "";
	directory = "";
	jobType = jobCLI;
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
	visHeightTools = 0;
	visHeightStatus = 0;
	visHeightEditor = 1;
	heightBar = 7;
	dialogsOnScreen = 0;

	heightOutput = 0;

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

	windowName[0] = '\0';
	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	dirNameAtExecute[0] = '\0';
	fileModTime = 0;

	propsBase.superPS = &propsEmbed;
	propsUser.superPS = &propsBase;
	props.superPS = &propsUser;

	if (extender)
		extender->Initialise(this);
}

int SciTEBase::GetDocumentAt(int index) {
	if (index < 0 || index >= buffers.size) {
		//Platform::DebugPrintf("SciTEBase::GetDocumentAt: Index out of range.\n");
		return 0;
	}
	if (buffers.buffers[index].doc == 0) {
		// Create a new document buffer
		buffers.buffers[index].doc = SendEditor(SCI_CREATEDOCUMENT, 0, 0);
	}
	return buffers.buffers[index].doc;
}

void SciTEBase::SetDocumentAt(int index) {
	if (	index < 0 ||
	        index >= buffers.length ||
	        index == buffers.current ||
	        buffers.current < 0 ||
	        buffers.current >= buffers.length)
		return ;
	UpdateBuffersCurrent();

	buffers.current = index;

	Buffer bufferNext = buffers.buffers[buffers.current];
	overrideExtension = bufferNext.overrideExtension;
	isDirty = bufferNext.isDirty;
	SetFileName(bufferNext.fileName.c_str());
	SendEditor(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.current));
	SetWindowName();
	ReadProperties();
	DisplayAround(bufferNext);

	CheckMenus();
}

void SciTEBase::UpdateBuffersCurrent() {
	if ((buffers.length > 0) && (buffers.current >= 0)) {
		buffers.buffers[buffers.current].fileName = fullPath;
		buffers.buffers[buffers.current].selection = GetSelection();
		buffers.buffers[buffers.current].scrollPosition = GetCurrentScrollPosition();
		buffers.buffers[buffers.current].isDirty = isDirty;
		buffers.buffers[buffers.current].overrideExtension = overrideExtension;
	}
}

bool SciTEBase::IsBufferAvailable() {
	return buffers.size > 1 && buffers.length < buffers.size;
}

static bool IsUntitledFileName(const char *name) {
	const char *dirEnd = strrchr(name, pathSepChar);
	return !dirEnd || !dirEnd[1];
}

void SciTEBase::ClearDocument() {
	SendEditor(SCI_CLEARALL);
	SendEditor(SCI_EMPTYUNDOBUFFER);
	SendEditor(SCI_SETSAVEPOINT);
}

void SciTEBase::InitialiseBuffers() {
	if (buffers.size == 0) {
		int buffersWanted = props.GetInt("buffers");
		if (buffersWanted > bufferMax)
			buffersWanted = bufferMax;
		if (buffersWanted < 1)
			buffersWanted = 1;
		buffers.Allocate(buffersWanted);
		// First document is the default from creation of control
		buffers.buffers[0].doc = SendEditor(SCI_GETDOCPOINTER, 0, 0);
		SendEditor(SCI_ADDREFDOCUMENT, 0, buffers.buffers[0].doc);	// We own this reference
		if (buffersWanted == 1) {
			DestroyMenuItem(4, IDM_PREV);
			DestroyMenuItem(4, IDM_NEXT);
			DestroyMenuItem(4, IDM_CLOSEALL);
			DestroyMenuItem(4, 0);
		}
	}
}

void SciTEBase::New() {
	InitialiseBuffers();
	UpdateBuffersCurrent();

	// If the current buffer is the initial untitled, clean buffer then overwrite it,
	// otherwise add a new buffer.
	if ((buffers.length > 1) ||
	        (buffers.current != 0) ||
	        (buffers.buffers[0].isDirty) ||
	        (!IsUntitledFileName(buffers.buffers[0].fileName.c_str())))
		buffers.current = buffers.Add();

	int doc = GetDocumentAt(buffers.current);
	SendEditor(SCI_SETDOCPOINTER, 0, doc);

	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	SetFileName(fileName);
	isDirty = false;
	isBuilding = false;
	isBuilt = false;
	ClearDocument();
	DeleteFileStackMenu();
	SetFileStackMenu();
}

void SciTEBase::Close(bool updateUI) {
	if (buffers.size == 1) {
		// With no buffer list, Close means close from MRU
		buffers.buffers[0].Init();
		fullPath[0] = '\0';
		StackMenu(0);
	} else {
		if (buffers.current >= 0 && buffers.current < buffers.length) {
			UpdateBuffersCurrent();
			Buffer buff = buffers.buffers[buffers.current];
			AddFileToStack(buff.fileName.c_str(), buff.selection, buff.scrollPosition);
		}
		bool closingLast = buffers.length == 1;
		if (closingLast) {
			buffers.buffers[0].Init();
		} else {
			buffers.RemoveCurrent();
		}
		Buffer bufferNext = buffers.buffers[buffers.current];
		overrideExtension = bufferNext.overrideExtension;
		isDirty = bufferNext.isDirty;
		if (updateUI)
			SetFileName(bufferNext.fileName.c_str());
		SendEditor(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.current));
		if (closingLast) {
			ClearDocument();
		}
		if (updateUI) {
			SetWindowName();
			ReadProperties();
			DisplayAround(bufferNext);
		}
	}
	if (updateUI)
		BuffersMenu();
}

void SciTEBase::CloseAllBuffers() {

	UpdateBuffersCurrent();	// Ensure isDirty copied
	for (int i = 0; i < buffers.length; i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (SaveIfUnsure() == IDCANCEL)
				return ;
		}
	}

	while (buffers.length > 1)
		Close(false);

	Close();
}

void SciTEBase::Next() {
	int next = buffers.current;
	if (++next >= buffers.length)
		next = 0;
	SetDocumentAt(next);
}

void SciTEBase::Prev() {
	int prev = buffers.current;
	if (--prev < 0)
		prev = buffers.length - 1;
	SetDocumentAt(prev);
}

void SciTEBase::BuffersMenu() {
	UpdateBuffersCurrent();
	int pos;
	DestroyMenuItem(4, IDM_BUFFERSEP);
	for (pos = 0; pos < bufferMax; pos++) {
		DestroyMenuItem(4, IDM_BUFFER + pos);
	}
	if (buffers.size > 1) {
		int menuStart = 4;
		SetMenuItem(4, menuStart, IDM_BUFFERSEP, "");
		for (pos = 0; pos < buffers.length; pos++) {
			int itemID = bufferCmdID + pos;
			char entry[MAX_PATH + 20];
			entry[0] = '\0';
#if PLAT_WIN
			sprintf(entry, "&%d ", pos);
#endif 
			if (IsUntitledFileName(buffers.buffers[pos].fileName.c_str()))
				strcat(entry, "Untitled");
			else
				strcat(entry, buffers.buffers[pos].fileName.c_str());
			// For short file names:
			//char *cpDirEnd = strrchr(buffers.buffers[pos]->fileName, pathSepChar);
			//strcat(entry, cpDirEnd + 1);

			if (buffers.buffers[pos].isDirty)
				strcat(entry, " *");

			SetMenuItem(4, menuStart + pos + 1, itemID, entry);
		}
	}
	CheckMenus();
}

SciTEBase::~SciTEBase() {
	if (extender)
		extender->Finalise();
}

void SciTEBase::ReadGlobalPropFile() {
	char propfile[MAX_PATH + 20];
	char propdir[MAX_PATH + 20];
	propsBase.Clear();
#if PLAT_GTK
	propsBase.Set("PLAT_GTK", "1");
#else
	propsBase.Set("PLAT_WIN", "1");
#endif 
	if (GetDefaultPropertiesFileName(propfile, propdir, sizeof(propfile))) {
		strcat(propdir, pathSepString);
		propsBase.Read(propfile, propdir);
	}
	propsUser.Clear();
	if (GetUserPropertiesFileName(propfile, propdir, sizeof(propfile))) {
		strcat(propdir, pathSepString);
		propsUser.Read(propfile, propdir);
	}
}

void SciTEBase::GetDocumentDirectory(char *docDir, int len) {
	if (dirName[0])
		strncpy(docDir, dirName, len);
	else
		getcwd(docDir, len);
	docDir[len - 1] = '\0';
}

void SciTEBase::ReadLocalPropFile() {
	char propdir[MAX_PATH + 20];
	GetDocumentDirectory(propdir, sizeof(propdir));
	char propfile[MAX_PATH + 20];
	strcpy(propfile, propdir);
	strcat(propdir, pathSepString);
	strcat(propfile, pathSepString);
	strcat(propfile, propFileName);
	props.Clear();
	props.Read(propfile, propdir);
	//Platform::DebugPrintf("Reading local properties from %s\n", propfile);

	// TODO: Grab these from Platform and update when environment says to
	props.Set("Chrome", "#C0C0C0");
	props.Set("ChromeHighlight", "#FFFFFF");
}

long SciTEBase::SendEditor(unsigned int msg, unsigned long wParam, long lParam) {
	return fnEditor(ptrEditor, msg, wParam, lParam);
}

long SciTEBase::SendEditorString(unsigned int msg, unsigned long wParam, const char *s) {
	return SendEditor(msg, wParam, reinterpret_cast<long>(s));
}

long SciTEBase::SendOutput(unsigned int msg, unsigned long wParam, long lParam) {
	return fnOutput(ptrOutput, msg, wParam, lParam);
}

void SciTEBase::SendChildren(unsigned int msg, unsigned long wParam, long lParam) {
	SendEditor(msg, wParam, lParam);
	SendOutput(msg, wParam, lParam);
}

long SciTEBase::SendFocused(unsigned int msg, unsigned long wParam, long lParam) {
	if (wEditor.HasFocus())
		return SendEditor(msg, wParam, lParam);
	else
		return SendOutput(msg, wParam, lParam);
}

long SciTEBase::SendOutputEx(unsigned int msg, unsigned long wParam/*= 0*/, long lParam /*= 0*/, bool direct /*= true*/) {
	if (direct)
		return SendOutput(msg, wParam, lParam);
	return Platform::SendScintilla(wOutput.GetID(), msg, wParam, lParam);
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(0, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(0, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	int menuStart = 7;
	if (recentFileStack[0].fileName[0]) {
		SetMenuItem(0, menuStart, IDM_MRU_SEP, "");
		for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
			//Platform::DebugPrintf("Setfile %d %s\n", stackPos, recentFileStack[stackPos].fileName.c_str());
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].fileName[0]) {
				char entry[MAX_PATH + 20];
				entry[0] = '\0';
#if PLAT_WIN
				sprintf(entry, "&%d ", stackPos);
#endif 
				strcat(entry, recentFileStack[stackPos].fileName.c_str());
				SetMenuItem(0, menuStart + stackPos + 1, itemID, entry);
			}
		}
	}
}

void SciTEBase::DropFileStackTop() {
	DeleteFileStackMenu();
	for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++)
		recentFileStack[stackPos] = recentFileStack[stackPos + 1];
	recentFileStack[fileStackMax - 1].Init();
	SetFileStackMenu();
}

void SciTEBase::AddFileToStack(const char *file, CharacterRange selection, int scrollPos) {
	if (!file)
		return ;
	DeleteFileStackMenu();
	// Only stack non-empty names
	if ((file[0]) && (file[strlen(file) - 1] != pathSepChar)) {
		int stackPos;
		int eqPos = fileStackMax - 1;
		for (stackPos = 0; stackPos < fileStackMax; stackPos++)
			if (recentFileStack[stackPos].fileName == file)
				eqPos = stackPos;
		for (stackPos = eqPos; stackPos > 0; stackPos--)
			recentFileStack[stackPos] = recentFileStack[stackPos - 1];
		recentFileStack[0].fileName = file;
		recentFileStack[0].selection = selection;
		recentFileStack[0].scrollPosition = scrollPos;
	}
	SetFileStackMenu();
}

void SciTEBase::RemoveFileFromStack(const char *file) {
	if (!file || !file[0])
		return ;
	DeleteFileStackMenu();
	int stackPos;
	for (stackPos = 0; stackPos < fileStackMax; stackPos++) {
		if (recentFileStack[stackPos].fileName == file) {
			for (int movePos = stackPos; movePos < fileStackMax - 1; movePos++)
				recentFileStack[movePos] = recentFileStack[movePos + 1];
			recentFileStack[fileStackMax - 1].Init();
			break;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::DisplayAround(const RecentFile &rf) {
	if ((rf.selection.cpMin != INVALID_POSITION) && (rf.selection.cpMax != INVALID_POSITION)) {
		int curScrollPos = SendEditor(SCI_GETFIRSTVISIBLELINE);
		int curTop = SendEditor(SCI_VISIBLEFROMDOCLINE, curScrollPos);
		int lineTop = SendEditor(SCI_VISIBLEFROMDOCLINE, rf.scrollPosition);
		SendEditor(SCI_LINESCROLL, 0, lineTop - curTop);
		int lineStart = SendEditor(SCI_LINEFROMPOSITION, rf.selection.cpMin);
		SendEditor(SCI_ENSUREVISIBLE, lineStart);
		int lineEnd = SendEditor(SCI_LINEFROMPOSITION, rf.selection.cpMax);
		SendEditor(SCI_ENSUREVISIBLE, lineEnd);
		SetSelection(rf.selection.cpMax, rf.selection.cpMin);
	}
}

// Next and Prev file comments.
// Decided that "Prev" file should mean the file you had opened last
// This means "Next" file means the file you opened longest ago.
void SciTEBase::StackMenuNext() {
	DeleteFileStackMenu();
	for (int stackPos = fileStackMax - 1; stackPos >= 0;stackPos--) {
		if (recentFileStack[stackPos].fileName[0] != '\0') {
			SetFileStackMenu();
			StackMenu(stackPos);
			return ;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::StackMenuPrev() {
	if (recentFileStack[0].fileName[0] != '\0') {
		StackMenu(0);
		// And rotate the one we just closed to the end.
		DeleteFileStackMenu();
		RecentFile temp = recentFileStack[0];
		int stackPos = 1;
		for (; stackPos < fileStackMax - 1; stackPos++) {
			if (recentFileStack[stackPos].fileName[0] == '\0') {
				stackPos--;
				break;
			}
			recentFileStack[stackPos - 1] = recentFileStack[stackPos];
		}
		recentFileStack[stackPos] = temp;
		SetFileStackMenu();
	}
}

void SciTEBase::StackMenu(int pos) {
	//Platform::DebugPrintf("Stack menu %d\n", pos);
	if (pos >= 0) {
		if ((pos == 0) && (recentFileStack[pos].fileName[0] == '\0')) {	// Empty
			New();
			SetWindowName();
			ReadProperties();
		} else if (recentFileStack[pos].fileName[0] != '\0') {
			RecentFile rf = recentFileStack[pos];
			//Platform::DebugPrintf("Opening pos %d %s\n",recentFileStack[pos].lineNumber,recentFileStack[pos].fileName);
			overrideExtension = "";
			isDirty = false;
			Open(rf.fileName.c_str());
			DisplayAround(rf);
		}
	}
}

void SciTEBase::RemoveToolsMenu() {
	for (int pos = 0; pos < toolMax; pos++) {
		DestroyMenuItem(2, IDM_TOOLS + pos);
	}
}

void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	const int menuPos = 3;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		SString prefix = "command.name.";
		prefix += SString(item).c_str();
		prefix += ".";
		SString commandName = props.GetNewExpand(prefix.c_str(), fileName);
		if (commandName.length()) {
			SString sMenuItem = commandName;
			SString sMnemonic = "Ctrl+";
			sMnemonic += SString(item).c_str();
			SetMenuItem(2, menuPos + item, itemID, sMenuItem.c_str(), sMnemonic.c_str());
		}
	}
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd, int item) {
	JobSubsystem jobType = jobCLI;
	SString subsysprefix = cmd;
	if (item >= 0) {
		subsysprefix += SString(item).c_str();
		subsysprefix += ".";
	}
	SString subsystem = props.GetNewExpand(subsysprefix.c_str(), fileName);
	if (subsystem[0] == '1')
		jobType = jobGUI;
	else if (subsystem[0] == '2')
		jobType = jobShell;
	else if (subsystem[0] == '3')
		jobType = jobExtension;
	else if (subsystem[0] == '4')
		jobType = jobHelp;
	return jobType;
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	SString prefix = "command.";
	prefix += SString(item).c_str();
	prefix += ".";
	SString command = props.GetNewExpand(prefix.c_str(), fileName);
	if (command.length()) {
		if (SaveIfUnsure() != IDCANCEL) {
			SString isfilter = "command.is.filter.";
			isfilter += SString(item).c_str();
			isfilter += ".";
			SString filter = props.GetNewExpand(isfilter.c_str(), fileName);
			if (filter[0] == '1')
				fileModTime -= 1;

			JobSubsystem jobType = SubsystemType("command.subsystem.", item);

			AddCommand(command, "", jobType);
			if (commandCurrent > 0)
				Execute();
		}
	}
}

static int IntFromHexDigit(const char ch) {
	if (isdigit(ch))
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else
		return 0;
}

static Colour ColourFromString(const char *val) {
	int r = IntFromHexDigit(val[1]) * 16 + IntFromHexDigit(val[2]);
	int g = IntFromHexDigit(val[3]) * 16 + IntFromHexDigit(val[4]);
	int b = IntFromHexDigit(val[5]) * 16 + IntFromHexDigit(val[6]);
	return Colour(r, g, b);
}

void SciTEBase::SetOneStyle(Window &win, int style, const char *s) {
	char *val = StringDup(s);
	//Platform::DebugPrintf("Style %d is [%s]\n", style, val);
	char *opt = val;
	while (opt) {
		char *cpComma = strchr(opt, ',');
		if (cpComma)
			*cpComma = '\0';
		char *colon = strchr(opt, ':');
		if (colon)
			*colon++ = '\0';
		if (0 == strcmp(opt, "italics"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETITALIC, style, 1);
		if (0 == strcmp(opt, "notitalics"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETITALIC, style, 0);
		if (0 == strcmp(opt, "bold"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBOLD, style, 1);
		if (0 == strcmp(opt, "notbold"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBOLD, style, 0);
		if (0 == strcmp(opt, "font"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETFONT, style, reinterpret_cast<long>(colon));
		if (0 == strcmp(opt, "fore"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETFORE, style, ColourFromString(colon).AsLong());
		if (0 == strcmp(opt, "back"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBACK, style, ColourFromString(colon).AsLong());
		if (0 == strcmp(opt, "size"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETSIZE, style, atoi(colon));
		if (0 == strcmp(opt, "eolfilled"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETEOLFILLED, style, 1);
		if (0 == strcmp(opt, "noteolfilled"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETEOLFILLED, style, 0);
		if (0 == strcmp(opt, "underlined"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETUNDERLINE, style, 1);
		if (0 == strcmp(opt, "notunderlined"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETUNDERLINE, style, 0);
		if (cpComma)
			opt = cpComma + 1;
		else
			opt = 0;
	}
	if (val)
		delete []val;
	Platform::SendScintilla(win.GetID(), SCI_STYLESETCHARACTERSET, style, characterSet);
}

void SciTEBase::SetStyleFor(Window &win, const char *lang) {
	for (int style = 0; style <= STYLE_MAX; style++) {
		if (style != STYLE_DEFAULT) {
			char key[200];
			sprintf(key, "style.%s.%0d", lang, style);
			SString sval = props.GetExpanded(key);
			SetOneStyle(win, style, sval.c_str());
		}
	}
}

void SciTEBase::ViewWhitespace(bool view) {
	if (view && indentationWSVisible)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
	else if (view)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEAFTERINDENT);
	else
		SendEditor(SCI_SETVIEWWS, SCWS_INVISIBLE);
}

// Properties that are interactively modifiable are only read from the properties file once.
void SciTEBase::ReadPropertiesInitial() {
	splitVertical = props.GetInt("split.vertical");
	int sizeHorizontal = props.GetInt("output.horizontal.size", 0);
	int sizeVertical = props.GetInt("output.vertical.size", 0);
	if (splitVertical && sizeVertical > 0 && heightOutput < sizeVertical || sizeHorizontal && heightOutput < sizeHorizontal) {
		heightOutput = NormaliseSplit(splitVertical ? sizeVertical : sizeHorizontal);
		SizeSubWindows();
		Redraw();
	}
	indentationWSVisible = props.GetInt("view.indentation.whitespace", 1);
	ViewWhitespace(props.GetInt("view.whitespace"));
	SendEditor(SCI_SETINDENTATIONGUIDES, props.GetInt("view.indentation.guides"));
	SendEditor(SCI_SETVIEWEOL, props.GetInt("view.eol"));

	sbVisible = props.GetInt("statusbar.visible");
	tbVisible = props.GetInt("toolbar.visible");

	lineNumbersWidth = 0;
	SString linenums = props.Get("line.numbers");
	if (linenums.length())
		lineNumbersWidth = linenums.value();
	lineNumbers = lineNumbersWidth;
	if (lineNumbersWidth == 0)
		lineNumbersWidth = lineNumbersWidthDefault;

	marginWidth = 0;
	SString margwidth = props.Get("margin.width");
	if (margwidth.length())
		marginWidth = margwidth.value();
	margin = marginWidth;
	if (marginWidth == 0)
		marginWidth = marginWidthDefault;

	foldMarginWidth = props.GetInt("fold.margin.width", foldMarginWidthDefault);
	foldMargin = foldMarginWidth;
	if (foldMarginWidth == 0)
		foldMarginWidth = foldMarginWidthDefault;

	char homepath[MAX_PATH + 20];
	if (GetSciteDefaultHome(homepath, sizeof(homepath))) {
		props.Set("SciteDefaultHome", homepath);
	}
	if (GetSciteUserHome(homepath, sizeof(homepath))) {
		props.Set("SciteUserHome", homepath);
	}
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

static void lowerCaseString(char *s) {
	while (*s) {
		*s = static_cast<char>(tolower(*s));
		s++;
	}
}

SString SciTEBase::ExtensionFileName() {
	if (overrideExtension.length())
		return overrideExtension;
	else if (fileName[0]) {
		// Force extension to lower case
		char fileNameWithLowerCaseExtension[MAX_PATH];
		strcpy(fileNameWithLowerCaseExtension, fileName);
		char *extension = strrchr(fileNameWithLowerCaseExtension, '.');
		if (extension) {
			lowerCaseString(extension);
		}
		return SString(fileNameWithLowerCaseExtension);
	} else
		return props.Get("default.file.ext");
}

void SciTEBase::AssignKey(int key, int mods, int cmd) {
	SendEditor(SCI_ASSIGNCMDKEY,
	           Platform::LongFromTwoShorts(static_cast<short>(key),
	                                       static_cast<short>(mods)), cmd);
}

void SciTEBase::ReadProperties() {
	//DWORD dwStart = timeGetTime();
	SString fileNameForExtension = ExtensionFileName();

	language = props.GetNewExpand("lexer.", fileNameForExtension.c_str());

	if (language == "python") {
		lexLanguage = SCLEX_PYTHON;
	} else if (language == "cpp") {
		lexLanguage = SCLEX_CPP;
	} else if (language == "hypertext") {
		lexLanguage = SCLEX_HTML;
	} else if (language == "xml") {
		lexLanguage = SCLEX_XML;
	} else if (language == "perl") {
		lexLanguage = SCLEX_PERL;
	} else if (language == "sql") {
		lexLanguage = SCLEX_SQL;
	} else if (language == "vb") {
		lexLanguage = SCLEX_VB;
	} else if (language == "props") {
		lexLanguage = SCLEX_PROPERTIES;
	} else if (language == "errorlist") {
		lexLanguage = SCLEX_ERRORLIST;
	} else if (language == "makefile") {
		lexLanguage = SCLEX_MAKEFILE;
	} else if (language == "batch") {
		lexLanguage = SCLEX_BATCH;
	} else if (language == "latex") {
		lexLanguage = SCLEX_LATEX;
	} else if (language == "lua") {
		lexLanguage = SCLEX_LUA;
	} else if (language == "diff") {
		lexLanguage = SCLEX_DIFF;
	} else if (language == "container") {
		lexLanguage = SCLEX_CONTAINER;
	} else {
		lexLanguage = SCLEX_NULL;
	}

	if ((lexLanguage == SCLEX_HTML) || (lexLanguage == SCLEX_XML))
		SendEditor(SCI_SETSTYLEBITS, 7);
	else
		SendEditor(SCI_SETSTYLEBITS, 5);

	SendEditor(SCI_SETLEXER, lexLanguage);
	SendOutput(SCI_SETLEXER, SCLEX_ERRORLIST);

	apis.Clear();

	SString kw0 = props.GetNewExpand("keywords.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 0, kw0.c_str());
	SString kw1 = props.GetNewExpand("keywords2.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 1, kw1.c_str());
	SString kw2 = props.GetNewExpand("keywords3.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 2, kw2.c_str());
	SString kw3 = props.GetNewExpand("keywords4.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 3, kw3.c_str());
	SString kw4 = props.GetNewExpand("keywords5.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 4, kw4.c_str());

	char homepath[MAX_PATH + 20];
	if (GetSciteDefaultHome(homepath, sizeof(homepath))) {
		props.Set("SciteDefaultHome", homepath);
	}
	if (GetSciteUserHome(homepath, sizeof(homepath))) {
		props.Set("SciteUserHome", homepath);
	}

	SString fold = props.Get("fold");
	SendEditorString(SCI_SETPROPERTY, reinterpret_cast<unsigned long>("fold"),
	                 fold.c_str());
	SString stylingWithinPreprocessor = props.Get("styling.within.preprocessor");
	SendEditorString(SCI_SETPROPERTY, reinterpret_cast<unsigned long>("styling.within.preprocessor"),
	                 stylingWithinPreprocessor.c_str());
	SString ttwl = props.Get("tab.timmy.whinge.level");
	SendEditorString(SCI_SETPROPERTY, reinterpret_cast<unsigned long>("tab.timmy.whinge.level"),
	                 ttwl.c_str());

	SString apifilename = props.GetNewExpand("api.", fileNameForExtension.c_str());
	if (apifilename.length()) {
		FILE *fp = fopen(apifilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			int len = ftell(fp);
			char *buffer = apis.Allocate(len);
			if (buffer) {
				fseek(fp, 0, SEEK_SET);
				fread(buffer, 1, len, fp);
				apis.SetFromAllocated();
			}
			fclose(fp);
			//Platform::DebugPrintf("Finished api file %d\n", len);
		}


	}

	SString eol_mode = props.Get("eol.mode");
	if (eol_mode == "LF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode == "CR") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode == "CRLF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
	}

	codePage = props.GetInt("code.page");
	SendEditor(SCI_SETCODEPAGE, codePage);

	characterSet = props.GetInt("character.set");

	SString colour;
	colour = props.Get("caret.fore");
	if (colour.length()) {
		SendEditor(SCI_SETCARETFORE, ColourFromString(colour.c_str()).AsLong());
	}

	SendEditor(SCI_SETCARETWIDTH, props.GetInt("caret.width", 1));
	SendOutput(SCI_SETCARETWIDTH, props.GetInt("caret.width", 1));

	colour = props.Get("calltip.back");
	if (colour.length()) {
		SendEditor(SCI_CALLTIPSETBACK, ColourFromString(colour.c_str()).AsLong());
	}

	SString caretPeriod = props.Get("caret.period");
	if (caretPeriod.length()) {
		SendEditor(SCI_SETCARETPERIOD, caretPeriod.value());
		SendOutput(SCI_SETCARETPERIOD, caretPeriod.value());
	}

	SendEditor(SCI_SETEDGECOLUMN, props.GetInt("edge.column", 0));
	SendEditor(SCI_SETEDGEMODE, props.GetInt("edge.mode", EDGE_NONE));
	colour = props.Get("edge.colour");
	if (colour.length()) {
		SendEditor(SCI_SETEDGECOLOUR, ColourFromString(colour.c_str()).AsLong());
	}

	SString selfore = props.Get("selection.fore");
	if (selfore.length()) {
		SendChildren(SCI_SETSELFORE, 1, ColourFromString(selfore.c_str()).AsLong());
	} else {
		SendChildren(SCI_SETSELFORE, 0, 0);
	}
	colour = props.Get("selection.back");
	if (colour.length()) {
		SendChildren(SCI_SETSELBACK, 1, ColourFromString(colour.c_str()).AsLong());
	} else {
		if (selfore.length())
			SendChildren(SCI_SETSELBACK, 0, 0);
		else	// Have to show selection somehow
			SendChildren(SCI_SETSELBACK, 1, Colour(0xC0, 0xC0, 0xC0).AsLong());
	}

	char bracesStyleKey[200];
	sprintf(bracesStyleKey, "braces.%s.style", language.c_str());
	bracesStyle = props.GetInt(bracesStyleKey, 0);

	char key[200];
	SString sval;

	sprintf(key, "calltip.%s.ignorecase", "*");
	sval = props.GetNewExpand(key, "");
	callTipIgnoreCase = sval == "1";
	sprintf(key, "calltip.%s.ignorecase", language.c_str());
	sval = props.GetNewExpand(key, "");
	if (sval != "")
		callTipIgnoreCase = sval == "1";

	sprintf(key, "autocomplete.%s.ignorecase", "*");
	sval = props.GetNewExpand(key, "");
	autoCompleteIgnoreCase = sval == "1";
	sprintf(key, "autocomplete.%s.ignorecase", language.c_str());
	sval = props.GetNewExpand(key, "");
	if (sval != "")
		autoCompleteIgnoreCase = sval == "1";
	SendEditor(SCI_AUTOCSETIGNORECASE, autoCompleteIgnoreCase ? 1 : 0);

	int autoCChooseSingle = props.GetInt("autocomplete.choose.single");
	SendEditor(SCI_AUTOCSETCHOOSESINGLE, autoCChooseSingle),

	// Set styles
	// For each window set the global default style, then the language default style, then the other global styles, then the other language styles

	SendEditor(SCI_STYLERESETDEFAULT, 0, 0);
	SendOutput(SCI_STYLERESETDEFAULT, 0, 0);

	sprintf(key, "style.%s.%0d", "*", STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	sprintf(key, "style.%s.%0d", language.c_str(), STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());

	SendEditor(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wEditor, "*");
	SetStyleFor(wEditor, language.c_str());

	SendOutput(SCI_STYLECLEARALL, 0, 0);

	sprintf(key, "style.%s.%0d", "errorlist", STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	SendOutput(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wOutput, "*");
	SetStyleFor(wOutput, "errorlist");

	if (firstPropertiesRead) {
		ReadPropertiesInitial();
	}

	SendEditor(SCI_SETUSEPALETTE, props.GetInt("use.palette"));
	SendEditor(SCI_SETPRINTMAGNIFICATION, props.GetInt("print.magnification"));
	SendEditor(SCI_SETPRINTCOLOURMODE, props.GetInt("print.colour.mode"));

	clearBeforeExecute = props.GetInt("clear.before.execute");

	int blankMarginLeft = props.GetInt("blank.margin.left", 1);
	int blankMarginRight = props.GetInt("blank.margin.right", 1);
	//long marginCombined = Platform::LongFromTwoShorts(blankMarginLeft, blankMarginRight);
	SendEditor(SCI_SETMARGINLEFT, 0, blankMarginLeft);
	SendEditor(SCI_SETMARGINRIGHT, 0, blankMarginRight);
	SendOutput(SCI_SETMARGINLEFT, 0, blankMarginLeft);
	SendOutput(SCI_SETMARGINRIGHT, 0, blankMarginRight);

	SendEditor(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);
	SendEditor(SCI_SETMARGINWIDTHN, 0, lineNumbers ? lineNumbersWidth : 0);

	bufferedDraw = props.GetInt("buffered.draw", 1);
	SendEditor(SCI_SETBUFFEREDDRAW, bufferedDraw);

	bracesCheck = props.GetInt("braces.check");
	bracesSloppy = props.GetInt("braces.sloppy");

	SString wordCharacters = props.GetNewExpand("word.characters.", fileNameForExtension.c_str());
	if (wordCharacters.length()) {
		SendEditorString(SCI_SETWORDCHARS, 0, wordCharacters.c_str());
	} else {
		SendEditor(SCI_SETWORDCHARS, 0, 0);
	}

	SendEditor(SCI_MARKERDELETEALL, static_cast<unsigned long>( -1));

	int tabSize = props.GetInt("tabsize");
	if (tabSize) {
		SendEditor(SCI_SETTABWIDTH, tabSize);
	}
	indentSize = props.GetInt("indent.size");
	SendEditor(SCI_SETINDENT, indentSize);
	indentOpening = props.GetInt("indent.opening");
	indentClosing = props.GetInt("indent.closing");
	SString lookback = props.GetNewExpand("statement.lookback.", fileNameForExtension.c_str());
	statementLookback = lookback.value();
	statementIndent = GetStyleAndWords("statement.indent.");
	statementEnd = GetStyleAndWords("statement.end.");
	blockStart = GetStyleAndWords("block.start.");
	blockEnd = GetStyleAndWords("block.end.");

	SendEditor(SCI_SETUSETABS, props.GetInt("use.tabs", 1));
	if (props.GetInt("vc.home.key", 1)) {
		AssignKey(SCK_HOME, 0, SCI_VCHOME);
		AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_VCHOMEEXTEND);
	} else {
		AssignKey(SCK_HOME, 0, SCI_HOME);
		AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_HOMEEXTEND);
	}
	SendEditor(SCI_SETHSCROLLBAR, props.GetInt("horizontal.scrollbar", 1));
	SendOutput(SCI_SETHSCROLLBAR, props.GetInt("output.horizontal.scrollbar", 1));

	SetToolsMenu();

	SendEditor(SCI_SETFOLDFLAGS, props.GetInt("fold.flags"));

	// To put the folder markers in the line number region
	//SendEditor(SCI_SETMARGINMASKN, 0, SC_MASK_FOLDERS);

	SendEditor(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD);

	// Create a margin column for the folding symbols
	SendEditor(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);

	SendEditor(SCI_SETMARGINWIDTHN, 2, foldMargin ? foldMarginWidth : 0);

	SendEditor(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
	SendEditor(SCI_SETMARGINSENSITIVEN, 2, 1);

	if (props.GetInt("fold.use.plus")) {
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, Colour(0xff, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_PLUS);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, Colour(0xff, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
	} else {
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_ARROWDOWN);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_ARROW);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
	}
	if (extender) {
		extender->Clear();

		char defaultDir[MAX_PATH];
		GetDefaultDirectory(defaultDir, sizeof(defaultDir));
		char scriptPath[MAX_PATH];
		if (Exists(defaultDir, "SciTEGlobal.lua", scriptPath)) {
			// Found fglobal file in global directory
			extender->Load(scriptPath);
		}

		// Check for an extension script
		SString extensionFile = props.GetNewExpand("extension.", fileNameForExtension.c_str());
		if (extensionFile.length()) {
			// find file in local directory
			char docDir[MAX_PATH];
			GetDocumentDirectory(docDir, sizeof(docDir));
			if (Exists(docDir, extensionFile.c_str(), scriptPath)) {
				// Found file in document directory
				extender->Load(scriptPath);
			} else if (Exists(defaultDir, extensionFile.c_str(), scriptPath)) {
				// Found file in global directory
				extender->Load(scriptPath);
			} else if (Exists("", extensionFile.c_str(), scriptPath)) {
				// Found as completely specified file name
				extender->Load(scriptPath);
			}
		}
	}

	firstPropertiesRead = false;
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("Properties read took %d\n", dwEnd - dwStart);
}



void SciTEBase::SetOverrideLanguage(int cmdID) {
	// Zero all the style bytes
	SendEditor(SCI_CLEARDOCUMENTSTYLE);

	overrideExtension = extList[cmdID - LEXER_BASE];
	ReadProperties();
	SendEditor(SCI_COLOURISE, 0, -1);
	Redraw();
}

int SciTEBase::LengthDocument() {
	return SendEditor(SCI_GETLENGTH);
}

int SciTEBase::GetLine(char *text, int sizeText, int line) {
	if (line == -1) {
		return SendEditor(SCI_GETCURLINE, sizeText, reinterpret_cast<long>(text));
	} else {
		short buflen = static_cast<short>(sizeText);
		memcpy(text, &buflen, sizeof(buflen));
		return SendEditor(SCI_GETLINE, line, reinterpret_cast<long>(text));
	}
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

// Find if there is a brace next to the caret, checking before caret first, then
// after caret. If brace found also find its matching brace.
// Returns true if inside a bracket pair.
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
		return ;
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


void SciTEBase::FixFilePath() {
	// Only used on Windows to fix the case of file names
}


void SciTEBase::SetFileName(const char *openName, bool fixCase) {
	if (openName[0] == '\"') {
		char pathCopy[MAX_PATH + 1];
		pathCopy[0] = '\0';
		strncpy(pathCopy, openName + 1, MAX_PATH);
		pathCopy[MAX_PATH] = '\0';
		if (pathCopy[strlen(pathCopy) - 1] == '\"')
			pathCopy[strlen(pathCopy) - 1] = '\0';
		AbsolutePath(fullPath, pathCopy, MAX_PATH);
	} else if (openName[0]) {
		AbsolutePath(fullPath, openName, MAX_PATH);
	} else {
		fullPath[0] = '\0';
	}

	char *cpDirEnd = strrchr(fullPath, pathSepChar);
	if (cpDirEnd) {
		strcpy(fileName, cpDirEnd + 1);
		strcpy(dirName, fullPath);
		dirName[cpDirEnd - fullPath] = '\0';
		//Platform::DebugPrintf("SetFileName: <%s> <%s>\n", fileName, dirName);
	}
	else {
		strcpy(fileName, fullPath);
		getcwd(dirName, sizeof(dirName));
		//Platform::DebugPrintf("Working directory: <%s>\n", dirName);
		strcpy(fullPath, dirName);
		strcat(fullPath, pathSepString);
		strcat(fullPath, fileName);
	}
	if (fixCase)
		FixFilePath();
	char fileBase[MAX_PATH];
	strcpy(fileBase, fileName);
	char *extension = strrchr(fileBase, '.');
	if (extension) {
		*extension = '\0';
		strcpy(fileExt, extension + 1);
	} else { // No extension
		fileExt[0] = '\0';
	}

	chdir(dirName);

	ReadLocalPropFile();

	props.Set("FilePath", fullPath);
	props.Set("FileDir", dirName);
	props.Set("FileName", fileBase);
	props.Set("FileExt", fileExt);
	props.Set("FileNameExt", fileName);

	SetWindowName();
	if (buffers.buffers)
		buffers.buffers[buffers.current].fileName = fullPath;
	BuffersMenu();
}

bool SciTEBase::Exists(const char *dir, const char *path, char *testPath) {
	char copyPath[MAX_PATH];
	if (dir) {
		if ((strlen(dir) + strlen(pathSepString) + strlen(path) + 1) > MAX_PATH)
			return false;
		strcpy(copyPath, dir);
		strcat(copyPath, pathSepString);
		strcat(copyPath, path);
	} else {
		strcpy(copyPath, path);
	}
	FILE *fp = fopen(copyPath, "rb");
	if (!fp)
		return false;
	fclose(fp);
	AbsolutePath(testPath, copyPath, MAX_PATH);
	return true;
}

time_t GetModTime(const char *fullPath) {
	struct stat statusFile;
	if (stat(fullPath, &statusFile) != -1)
		return statusFile.st_mtime;
	else
		return 0;
}

void SciTEBase::Open(const char *file, bool initialCmdLine) {

	InitialiseBuffers();

	if (!file) {
		MessageBox(wSciTE.GetID(), "Bad file", appName, MB_OK);
	}

	int index = buffers.GetDocumentByName(file);
	if (index >= 0) {
		SetDocumentAt(index);
		DeleteFileStackMenu();
		SetFileStackMenu();
		return ;
	}

	if (buffers.size == buffers.length) {
		AddFileToStack(fullPath, GetSelection(), GetCurrentScrollPosition());
		ClearDocument();
	} else {
		New();
	}

	//Platform::DebugPrintf("Opening %s\n", file);
	SetFileName(file);
	overrideExtension = "";
	ReadProperties();
	UpdateBuffersCurrent();

	if (fileName[0]) {
		SendEditor(SCI_CANCEL);
		SendEditor(SCI_SETUNDOCOLLECTION, 0);

		fileModTime = GetModTime(fullPath);

		FILE *fp = fopen(fullPath, "rb");
		if (fp || initialCmdLine) {
			if (fp) {
				char data[blockSize];
				int lenFile = fread(data, 1, sizeof(data), fp);
				while (lenFile > 0) {
					SendEditorString(SCI_ADDTEXT, lenFile, data);
					lenFile = fread(data, 1, sizeof(data), fp);
				}
				fclose(fp);
			}

		} else {
			char msg[MAX_PATH + 100];
			strcpy(msg, "Could not open file \"");
			strcat(msg, fullPath);
			strcat(msg, "\".");
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		}
		SendEditor(SCI_SETUNDOCOLLECTION, 1);
		// Flick focus to the output window and back to
		// ensure palette realised correctly.
		SetFocus(wOutput.GetID());
		SetFocus(wEditor.GetID());
		SendEditor(SCI_EMPTYUNDOBUFFER);
		SendEditor(SCI_SETSAVEPOINT);
		if (props.GetInt("fold.on.open") > 0) {
			FoldAll();
		}
		SendEditor(SCI_GOTOPOS, 0);
	}
	Redraw();
	RemoveFileFromStack(fullPath);
	DeleteFileStackMenu();
	SetFileStackMenu();
	SetWindowName();
	if (extender)
		extender->OnOpen();
}

void SciTEBase::Revert() {
	FILE *fp = fopen(fullPath, "rb");

	if (fp) {
		SendEditor(SCI_CANCEL);
		SendEditor(SCI_CLEARALL);
		fileModTime = GetModTime(fullPath);
		char data[blockSize];
		int lenFile = fread(data, 1, sizeof(data), fp);
		while (lenFile > 0) {
			SendEditorString(SCI_ADDTEXT, lenFile, data);
			lenFile = fread(data, 1, sizeof(data), fp);
		}
		fclose(fp);
		SendEditor(SCI_SETUNDOCOLLECTION, 1);
		// Flick focus to the output window and back to
		// ensure palette realised correctly.
		SendEditor(SCI_SETSAVEPOINT);
		SendEditor(SCI_GOTOPOS, 0);
		Redraw();
	} else {
		char msg[MAX_PATH + 100];
		strcpy(msg, "Could not revert file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
	}
}

int SciTEBase::SaveIfUnsure(bool forceQuestion) {
	if (isDirty) {
		if (props.GetInt("are.you.sure", 1) ||
		        IsUntitledFileName(fullPath) ||
		        forceQuestion) {
			char msg[MAX_PATH + 100];
			if (fileName[0]) {
				strcpy(msg, "Save changes to \"");
				strcat(msg, fullPath);
				strcat(msg, "\"?");
			} else {
				strcpy(msg, "Save changes to (Untitled)?");
			}
			dialogsOnScreen++;
			int decision = MessageBox(wSciTE.GetID(), msg, appName, MB_YESNOCANCEL);
			dialogsOnScreen--;
			if (decision == IDYES) {
				if (!Save())
					decision = IDCANCEL;
			}
			return decision;
		} else {
			if (!Save())
				return IDCANCEL;
		}
	}
	return IDYES;
}

int SciTEBase::SaveIfUnsureAll(bool forceQuestion) {
	UpdateBuffersCurrent();	// Ensure isDirty copied
	for (int i = 0; i < buffers.length; i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (SaveIfUnsure(forceQuestion) == IDCANCEL)
				return IDCANCEL;
		}
	}
	// Definitely going to exit now, so delete all documents
	// Set editor back to initial document
	SendEditor(SCI_SETDOCPOINTER, 0, buffers.buffers[0].doc);
	// Release all the extra documents
	for (int j = 0; j < buffers.size; j++) {
		if (buffers.buffers[j].doc)
			SendEditor(SCI_RELEASEDOCUMENT, 0, buffers.buffers[j].doc);
	}
	// Initial document will be deleted when editor deleted
	return IDYES;
}

int SciTEBase::SaveIfUnsureForBuilt() {
	if (isDirty) {
		if (props.GetInt("are.you.sure.for.build", 0))
			return SaveIfUnsure(true);

		Save();
	}
	return IDYES;
}

int StripTrailingSpaces(char *data, int ds, bool lastBlock) {
	int lastRead = 0;
	char *w = data;

	for (int i = 0; i < ds; i++) {
		char ch = data[i];
		if ((ch == ' ') || (ch == '\t')) {
			// Swallow those spaces
		}
		else if ((ch == '\r') || (ch == '\n')) {
			*w++ = ch;
			lastRead = i + 1;
		} else {
			while (lastRead < i) {
				*w++ = data[lastRead++];
			}
			*w++ = ch;
			lastRead = i + 1;
		}
	}
	// If a non-final block, then preserve whitespace at end of block as it may be significant.
	if (!lastBlock) {
		while (lastRead < ds) {
			*w++ = data[lastRead++];
		}
	}
	return w - data;
}

// Returns false only if cancelled
bool SciTEBase::Save() {
	if (fileName[0]) {

		if (props.GetInt("save.deletes.first")) {
			unlink(fullPath);
		}

		//Platform::DebugPrintf("Saving <%s><%s>\n", fileName, fullPath);
		//DWORD dwStart = timeGetTime();
		FILE *fp = fopen(fullPath, "wb");
		if (fp) {
			char data[blockSize + 1];
			int lengthDoc = LengthDocument();
			for (int i = 0; i < lengthDoc; i += blockSize) {
				int grabSize = lengthDoc - i;
				if (grabSize > blockSize)
					grabSize = blockSize;
				GetRange(wEditor, i, i + grabSize, data);
				if (props.GetInt("strip.trailing.spaces"))
					grabSize = StripTrailingSpaces(
					               data, grabSize, grabSize != blockSize);
				fwrite(data, grabSize, 1, fp);
			}
			fclose(fp);

			//MoveFile(fullPath, fullPath);

			//DWORD dwEnd = timeGetTime();
			//Platform::DebugPrintf("Saved file=%d\n", dwEnd - dwStart);
			fileModTime = GetModTime(fullPath);
			SendEditor(SCI_SETSAVEPOINT);
			char fileNameLowered[MAX_PATH];
			strcpy(fileNameLowered, fileName);
			lowerCaseString(fileNameLowered);
			if (0 != strstr(fileNameLowered, ".properties")) {
				ReadGlobalPropFile();
				ReadLocalPropFile();
				ReadProperties();
				SetWindowName();
				BuffersMenu();
				Redraw();
			}
		} else {
			char msg[200];
			strcpy(msg, "Could not save file \"");
			strcat(msg, fullPath);
			strcat(msg, "\".");
			dialogsOnScreen++;
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
			dialogsOnScreen--;
		}
		return true;
	} else {
		return SaveAs();
	}
}

bool SciTEBase::SaveAs(char *file) {
	if (file && *file) {
		SetFileName(file);
		Save();
		ReadProperties();
		SendEditor(SCI_CLEARDOCUMENTSTYLE);
		SendEditor(SCI_COLOURISE, 0, -1);
		Redraw();
		SetWindowName();
		BuffersMenu();
		return true;
	} else {
		return SaveAsDialog();
	}
}

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
	    { 0x00, 0x00, 0x00 },  // highlight1  0;0;0       black
	    { 0x00, 0x00, 0xFF },  // highlight2  0;0;255     blue
	    { 0x00, 0xFF, 0xFF },  // highlight3  0;255;255   cyan
	    { 0x00, 0xFF, 0x00 },  // highlight4  0;255;0     green
	    { 0xFF, 0x00, 0xFF },  // highlight5  255;0;255   violet
	    { 0xFF, 0x00, 0x00 },  // highlight6  255;0;0     red
	    { 0xFF, 0xFF, 0x00 },  // highlight7  255;255;0   yellow
	    { 0xFF, 0xFF, 0xFF },  // highlight8  255;255;255 white
	    { 0x00, 0x00, 0x80 },  // highlight9  0;0;128     dark blue
	    { 0x00, 0x80, 0x80 },  // highlight10 0;128;128   dark cyan
	    { 0x00, 0x80, 0x00 },  // highlight11 0;128;0     dark green
	    { 0x80, 0x00, 0x80 },  // highlight12 128;0;128   dark violet
	    { 0x80, 0x00, 0x00 },  // highlight13 128;0;0     brown
	    { 0x80, 0x80, 0x00 },  // highlight14 128;128;0   khaki
	    { 0x80, 0x80, 0x80 },  // highlight15 128;128;128 dark grey
	    { 0xC0, 0xC0, 0xC0 },  // highlight16 192;192;192 grey
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
	if (lastOffset != currentOffset ||  // change
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
	if (lastOffset != currentOffset ||  // change
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
	if (lastOffset != currentOffset ||  // change
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
	if (lastOffset != currentOffset ||  // change
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

void SciTEBase::SaveToRTF(const char *saveName) {
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
						if (!strcasecmp(family.c_str(), fonts[i]))
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
						if (!strcasecmp(fore.c_str(), colors[i]))
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
		int lengthDoc = LengthDocument();
		bool prevCR = false;
		int styleCurrent = -1;
		WindowAccessor acc(wEditor.GetID(), props);
		for (i = 0; i < lengthDoc; i++) {
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
	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		int styleCurrent = 0;
		fputs("<HTML>\n", fp);
		fputs("<HEAD>\n", fp);
		fputs("<STYLE>\n", fp);
		SString colour;
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
				if (istyle == STYLE_DEFAULT)
					fprintf(fp, "SPAN {\n");
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
		fputs("</STYLE>\n", fp);
		fputs("</HEAD>\n", fp);
		fputs("<BODY>\n", fp);
		if (wysiwyg)
			fputs("<SPAN class=S0>", fp);
		else
			fputs("<PRE class=S0>", fp);
		int lengthDoc = LengthDocument();
		bool prevCR = false;
		WindowAccessor acc(wEditor.GetID(), props);
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style != styleCurrent) {
				if (wysiwyg || styleCurrent != 0)
					fputs("</SPAN>", fp);
				if (wysiwyg || style != 0)
					fprintf(fp, "<SPAN class=S%0d>", style);
				styleCurrent = style;
			}
			if (ch == ' ') {
				if (wysiwyg)
					fputs("&nbsp;", fp);
				else
					fputc(' ', fp);
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
			} else if (ch == '\r') {
				if (wysiwyg)
					fputs("<BR>\n", fp);
				else
					fputc('\n', fp);
			} else if (ch == '\n') {
				if (!prevCR)
					fputs("<BR>\n", fp);
			} else if (ch == '<') {
				fputs("&lt;", fp);
			} else if (ch == '>') {
				fputs("&gt;", fp);
			} else if (ch == '&') {
				fputs("&amp;", fp);
			} else {
				fputc(ch, fp);
			}
			prevCR = ch == '\r';
		}
		if (wysiwyg)
			fputs("</SPAN>", fp);
		else
			fputs("</PRE>", fp);
		fputs("</BODY>\n</HTML>\n", fp);
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

void SciTEBase::OpenProperties(int propsFile) {
	if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
		char propfile[MAX_PATH + 20];
		char propdir[MAX_PATH + 20];
		if (propsFile == IDM_OPENLOCALPROPERTIES) {
			getcwd(propfile, sizeof(propfile));
			strcat(propfile, pathSepString);
			strcat(propfile, propFileName);
			Open(propfile);
		} else if (propsFile == IDM_OPENUSERPROPERTIES) {
			if (GetUserPropertiesFileName(propfile, propdir, sizeof(propfile))) {
				Open(propfile);
			}
		} else {	// IDM_OPENGLOBALPROPERTIES
			if (GetDefaultPropertiesFileName(propfile, propdir, sizeof(propfile))) {
				Open(propfile);
			}
		}
	}
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

static bool iswordcharforsel(char ch) {
	return !strchr("\t\n\r !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", ch);
}

void SciTEBase::SelectionWord(char *word, int len) {
	int lengthDoc = LengthDocument();
	CharacterRange cr = GetSelection();
	int selStart = cr.cpMin;
	int selEnd = cr.cpMax;
	if (selStart == selEnd) {
		WindowAccessor acc(wEditor.GetID(), props);
		// Try and find a word at the caret
		if (iswordcharforsel(acc[selStart])) {
			while ((selStart > 0) && (iswordcharforsel(acc[selStart - 1])))
				selStart--;
			while ((selEnd < lengthDoc - 1) && (iswordcharforsel(acc[selEnd + 1])))
				selEnd++;
			if (selStart < selEnd)
				selEnd++;   	// Because normal selections end one past
		}


	}
	word[0] = '\0';
	if ((selStart < selEnd) && ((selEnd - selStart + 1) < len)) {
		GetRange(wEditor, selStart, selEnd, word);
	}
}

void SciTEBase::SelectionIntoProperties() {
	CharacterRange cr = GetSelection();
	char currentSelection[1000];
	if ((cr.cpMin < cr.cpMax) && ((cr.cpMax - cr.cpMin + 1) < static_cast<int>(sizeof(currentSelection)))) {
		GetRange(wEditor, cr.cpMin, cr.cpMax, currentSelection);
		int len = strlen(currentSelection);
		if (len > 2 && iscntrl(currentSelection[len - 1]))
			currentSelection[len - 1] = '\0';
		if (len > 2 && iscntrl(currentSelection[len - 2]))
			currentSelection[len - 2] = '\0';
		props.Set("CurrentSelection", currentSelection);
	}
	char word[200];
	SelectionWord(word, sizeof(word));
	props.Set("CurrentWord", word);
}

void SciTEBase::SelectionIntoFind() {
	SelectionWord(findWhat, sizeof(findWhat));
}

void SciTEBase::FindMessageBox(const char *msg) {
	dialogsOnScreen++;
#if PLAT_GTK
	MessageBox(wSciTE.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#else
	MessageBox(wFindReplace.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#endif 
	dialogsOnScreen--;
}

void SciTEBase::FindNext(bool reverseDirection) {
	if (!findWhat[0]) {
		Find();
		return ;
	}
	TextToFind ft = {{0, 0}, 0, {0, 0}};
	CharacterRange crange = GetSelection();
	if (reverseDirection) {
		ft.chrg.cpMin = crange.cpMin - 1;
		ft.chrg.cpMax = 0;
	} else {
		ft.chrg.cpMin = crange.cpMax;
		ft.chrg.cpMax = LengthDocument();
	}
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) | (matchCase ? SCFIND_MATCHCASE : 0);
	//DWORD dwStart = timeGetTime();
	int posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found at %d took %d\n", findWhat, posFind, dwEnd - dwStart);
	if (posFind == -1) {
		// Failed to find in indicated direction so search on other side
		if (reverseDirection) {
			ft.chrg.cpMin = LengthDocument();
			ft.chrg.cpMax = 0;
		} else {
			ft.chrg.cpMin = 0;
			ft.chrg.cpMax = LengthDocument();
		}
		posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
	}
	if (posFind == -1) {
		havefound = false;
		char msg[300];
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
	} else {
		havefound = true;
		EnsureRangeVisible(ft.chrgText.cpMin, ft.chrgText.cpMax);
		SetSelection(ft.chrgText.cpMin, ft.chrgText.cpMax);
		if (!replacing) {
			DestroyFindReplace();
		}
	}
}

void SciTEBase::ReplaceOnce() {
	if (havefound) {
		SendEditorString(SCI_REPLACESEL, 0, replaceWhat);
		havefound = false;
		//Platform::DebugPrintf("Replace <%s> -> <%s>\n", findWhat, replaceWhat);
	}


	FindNext(reverseFind);
}

void SciTEBase::ReplaceAll() {
	if (findWhat[0] == '\0') {
		FindMessageBox("Find string for Replace All must not be empty.");
		return ;
	}

	TextToFind ft;
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = LengthDocument();
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) | (matchCase ? SCFIND_MATCHCASE : 0);
	int posFind = SendEditor(SCI_FINDTEXT, flags,
	                         reinterpret_cast<long>(&ft));
	if (posFind != -1) {
		SendEditor(SCI_BEGINUNDOACTION);
		while (posFind != -1) {
			SetSelection(ft.chrgText.cpMin, ft.chrgText.cpMax);
			SendEditorString(SCI_REPLACESEL, 0, replaceWhat);
			ft.chrg.cpMin = posFind + strlen(replaceWhat);
			ft.chrg.cpMax = LengthDocument();
			posFind = SendEditor(SCI_FINDTEXT, flags,
			                     reinterpret_cast<long>(&ft));
		}
		SendEditor(SCI_ENDUNDOACTION);
	} else {
		char msg[200];
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
	SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
	SendOutput(SCI_ADDTEXT, len, reinterpret_cast<long>(s));
	SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
}

void SciTEBase::OutputAppendStringEx(const char *s, int len /*= -1*/, bool direct /*= true*/) {
	if (direct)
		OutputAppendString(s, len);
	else {
		if (len == -1)
			len = strlen(s);
		SendOutputEx(SCI_GOTOPOS, SendOutputEx(SCI_GETTEXTLENGTH, 0, 0, false));
		SendOutputEx(SCI_ADDTEXT, len, reinterpret_cast<long>(s), false);
		SendOutputEx(SCI_GOTOPOS, SendOutputEx(SCI_GETTEXTLENGTH, 0, 0, false), false);
	}
}

void SciTEBase::Execute() {
	if (clearBeforeExecute) {
		SendOutput(SCI_CLEARALL);
	}

	SendOutput(SCI_MARKERDELETEALL, static_cast<unsigned long>( -1));
	SendEditor(SCI_MARKERDELETEALL, 0);
	// Ensure the output pane is visible
	if (jobUsesOutputPane && heightOutput < 20) {
		if (splitVertical)
			heightOutput = NormaliseSplit(300);
		else
			heightOutput = NormaliseSplit(100);
		SizeSubWindows();
		Redraw();
	}

	cancelFlag = 0L;
	executing = true;
	CheckMenus();
	chdir(dirName);
	strcpy(dirNameAtExecute, dirName);
}

void SciTEBase::BookmarkToggle( int lineno ) {
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	int state = SendEditor(SCI_MARKERGET, lineno);
	if ( state & (1 << SciTE_MARKER_BOOKMARK))
		SendEditor(SCI_MARKERDELETE, lineno, SciTE_MARKER_BOOKMARK);
	else {
		// no need to do each time, but can't hurt either :-)
		SendEditor(SCI_MARKERDEFINE, SciTE_MARKER_BOOKMARK, SC_MARK_CIRCLE);
		SendEditor(SCI_MARKERSETFORE, SciTE_MARKER_BOOKMARK, Colour(0x7f, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SciTE_MARKER_BOOKMARK, Colour(0x80, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERADD, lineno, SciTE_MARKER_BOOKMARK);
	}

}

void SciTEBase::BookmarkNext() {
	int lineno = GetCurrentLineNumber();
	int nextLine = SendEditor(SCI_MARKERNEXT, lineno + 1, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0)
		nextLine = SendEditor(SCI_MARKERNEXT, 0, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0 || nextLine == lineno)
		; // how do I beep?
	else {
		SendEditor(SCI_ENSUREVISIBLE, nextLine);
		SendEditor(SCI_GOTOLINE, nextLine);
	}
}

void SciTEBase::CheckReload() {
	if (props.GetInt("load.on.activate")) {
		// Make a copy of fullPath as otherwise it gets aliased in Open
		char fullPathToCheck[MAX_PATH];
		strcpy(fullPathToCheck, fullPath);
		time_t newModTime = GetModTime(fullPathToCheck);
		//Platform::DebugPrintf("Times are %d %d\n", fileModTime, newModTime);
		if (newModTime > fileModTime) {
			if (isDirty) {
				static bool entered = false;   	// Stop reentrancy
				if (!entered && (0 == dialogsOnScreen)) {
					entered = true;
					char msg[MAX_PATH + 100];
					strcpy(msg, "The file \"");
					strcat(msg, fullPathToCheck);
					strcat(msg, "\" has been modified. Should it be reloaded?");
					dialogsOnScreen++;
					int decision = MessageBox(wSciTE.GetID(), msg, appName, MB_YESNO);
					dialogsOnScreen--;
					if (decision == IDYES) {
						Open(fullPathToCheck);
					}
					entered = false;
				}
			} else {
				Open(fullPathToCheck);
			}
		}
	}
}

void SciTEBase::Activate(bool activeApp) {
	if (activeApp) {
		CheckReload();
	} else {
		if (isDirty) {
			if (props.GetInt("save.on.deactivate") && fileName[0]) {
				// Trying to save at deactivation when no file name -> dialogs
				Save();
			}
		}
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

int DecodeMessage(char *cdoc, char *sourcePath, int format) {
	sourcePath[0] = '\0';
	if (format == SCE_ERR_PYTHON) {
		// Python
		char *startPath = strchr(cdoc, '\"') + 1;
		char *endPath = strchr(startPath, '\"');
		strncpy(sourcePath, startPath, endPath - startPath);
		sourcePath[endPath - startPath] = 0;
		endPath++;
		while (*endPath && !isdigit(*endPath))
			endPath++;
		int sourceNumber = atoi(endPath) - 1;
		return sourceNumber;
	} else if (format == SCE_ERR_GCC) {
		// GCC - look for number followed by colon to be line number
		// This will be preceded by file name
		for (int i = 0; cdoc[i]; i++) {
			if (isdigit(cdoc[i]) && cdoc[i + 1] == ':') {
				int j = i;
				while (j > 0 && isdigit(cdoc[j - 1]))
					j--;
				int sourceNumber = atoi(cdoc + j) - 1;
				strncpy(sourcePath, cdoc, j - 1);
				sourcePath[j - 1] = 0;
				return sourceNumber;
			}
		}
	} else if (format == SCE_ERR_MS) {
		// Visual *
		char *endPath = strchr(cdoc, '(');
		strncpy(sourcePath, cdoc, endPath - cdoc);
		sourcePath[endPath - cdoc] = 0;
		endPath++;
		return atoi(endPath) - 1;
	} else if (format == SCE_ERR_BORLAND) {
		// Borland
		char *space = strchr(cdoc, ' ');
		if (space) {
			while (isspace(*space))
				space++;
			while (*space && !isspace(*space))
				space++;
			while (isspace(*space))
				space++;
			char *space2 = strchr(space, ' ');
			if (space2) {
				strncpy(sourcePath, space, space2 - space);
				return atoi(space2) - 1;
			}
		}
	} else if (format == SCE_ERR_PERL) {
		// perl
		char *at = strstr(cdoc, " at ");
		char *line = strstr(cdoc, " line ");
		if (at && line) {
			strncpy(sourcePath, at + 4, line - (at + 4));
			sourcePath[line - (at + 4)] = 0;
			line += 6;
			return atoi(line) - 1;
		}
	}
	return - 1;
}

void SciTEBase::GoMessage(int dir) {
	CharacterRange crange;
	crange.cpMin = SendOutput(SCI_GETSELECTIONSTART);
	crange.cpMax = SendOutput(SCI_GETSELECTIONEND);
	int selStart = crange.cpMin;
	int curLine = SendOutput(SCI_LINEFROMPOSITION, selStart);
	int maxLine = SendOutput(SCI_GETLINECOUNT);
	int lookLine = curLine + dir;
	if (lookLine < 0)
		lookLine = maxLine - 1;
	else if (lookLine >= maxLine)
		lookLine = 0;
	WindowAccessor acc(wOutput.GetID(), props);
	while ((dir == 0) || (lookLine != curLine)) {
		int startPosLine = SendOutput(SCI_POSITIONFROMLINE, lookLine, 0);
		int lineLength = SendOutput(SCI_LINELENGTH, lookLine, 0);
		//Platform::DebugPrintf("GOMessage %d %d %d of %d linestart = %d\n", selStart, curLine, lookLine, maxLine, startPosLine);
		char style = acc.StyleAt(startPosLine);
		if (style != 0 && style != 4) {
			//Platform::DebugPrintf("Marker to %d\n", lookLine);
			SendOutput(SCI_MARKERDELETEALL, static_cast<unsigned long>( -1));
			SendOutput(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			SendOutput(SCI_MARKERSETFORE, 0, Colour(0x7f, 0, 0).AsLong());
			SendOutput(SCI_MARKERSETBACK, 0, Colour(0xff, 0xff, 0).AsLong());
			SendOutput(SCI_MARKERADD, lookLine, 0);
			SendOutput(SCI_SETSEL, startPosLine, startPosLine);
			char *cdoc = new char[lineLength + 1];
			if (!cdoc)
				return ;
			GetRange(wOutput, startPosLine, startPosLine + lineLength, cdoc);
			char sourcePath[MAX_PATH];
			int sourceLine = DecodeMessage(cdoc, sourcePath, style);
			//printf("<%s> %d %d\n",sourcePath, sourceLine, lookLine);
			//Platform::DebugPrintf("<%s> %d %d\n",sourcePath, sourceLine, lookLine);
			if (sourceLine >= 0) {
				if (0 != strcmp(sourcePath, fileName)) {
					char messagePath[MAX_PATH];
					if (Exists(dirNameAtExecute, sourcePath, messagePath)) {
						if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
							Open(messagePath);
						} else {
							delete []cdoc;
							return ;
						}
					} else if (Exists(dirName, sourcePath, messagePath)) {
						if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
							Open(messagePath);
						} else {
							delete []cdoc;
							return ;
						}
					}
				}
				SendEditor(SCI_MARKERDELETEALL, 0);
				SendEditor(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				SendEditor(SCI_MARKERSETFORE, 0, Colour(0x7f, 0, 0).AsLong());
				SendEditor(SCI_MARKERSETBACK, 0, Colour(0xff, 0xff, 0).AsLong());
				SendEditor(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = SendEditor(SCI_POSITIONFROMLINE, sourceLine, 0);
				EnsureRangeVisible(startSourceLine, startSourceLine);
				SetSelection(startSourceLine, startSourceLine);
				SetFocus(wEditor.GetID());
			}
			delete []cdoc;
			return ;
		}
		if (dir == 0)
			dir = 1;
		lookLine += dir;
		if (lookLine < 0)
			lookLine = maxLine - 1;
		else if (lookLine >= maxLine)
			lookLine = 0;
	}
}

bool SciTEBase::StartCallTip() {
	//Platform::DebugPrintf("StartCallTip\n");
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));
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
	} while (current > 0 && nonFuncChar(linebuf[current - 1]));
	if (current <= 0)
		return true;

	int startword = current - 1;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	linebuf[current] = '\0';
	int rootlen = current - startword;
	functionDefinition = "";
	//Platform::DebugPrintf("word  is [%s] %d %d %d\n", linebuf + startword, rootlen, pos, pos - rootlen);
	if (apis) {
		const char *word = apis.GetNearestWord (linebuf + startword, rootlen, callTipIgnoreCase);
		if (word) {
			functionDefinition = word;
			SendEditorString(SCI_CALLTIPSHOW, pos - rootlen + 1, word);
			ContinueCallTip();
		}
	}
	return true;
}

void SciTEBase::ContinueCallTip() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int commas = 0;
	for (int i = 0; i < current; i++) {
		if (linebuf[i] == ',')
			commas++;
	}

	int startHighlight = 0;
	while (functionDefinition[startHighlight] && functionDefinition[startHighlight] != '(')
		startHighlight++;
	if (functionDefinition[startHighlight] == '(')
		startHighlight++;
	while (functionDefinition[startHighlight] && commas > 0) {
		if (functionDefinition[startHighlight] == ',' || functionDefinition[startHighlight] == ')')
			commas--;
		startHighlight++;
	}
	if (functionDefinition[startHighlight] == ',' || functionDefinition[startHighlight] == ')')
		startHighlight++;
	int endHighlight = startHighlight;
	if (functionDefinition[endHighlight])
		endHighlight++;
	while (functionDefinition[endHighlight] && functionDefinition[endHighlight] != ',' && functionDefinition[endHighlight] != ')')
		endHighlight++;

	SendEditor(SCI_CALLTIPSETHLT, startHighlight, endHighlight);
}

bool SciTEBase::StartAutoComplete() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int startword = current;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
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
			free(words);
		}
	}
	return true;
}

const char *strcasestr(const char *str, const char *pattn) {
	int i;
	int pattn0 = tolower (pattn[0]);

	for (; *str; str++) {
		if (tolower (*str) == pattn0) {
			for (i = 1; tolower (str[i]) == tolower (pattn[i]); i++)
				if (pattn[i] == '\0')
					return str;
			if (pattn[i] == '\0')
				return str;
		}
	}
	return NULL;
}

bool SciTEBase::StartAutoCompleteWord() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int startword = current;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
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
	//int flags = (autoCompleteIgnoreCase ? 0 : SCFIND_MATCHCASE);
	int posCurrentWord = SendEditor (SCI_GETCURRENTPOS) - rootlen;
	//DWORD dwStart = timeGetTime();
	int length = 0;	// variables for reallocatable array creation
	int newlength;
#undef WORDCHUNK
#define WORDCHUNK 100
	int size = WORDCHUNK;
	char *words = (char*) malloc(size);
	*words = '\0';
	for (;;) {	// search all the document
		ft.chrg.cpMax = doclen;
		int posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
		if (posFind == -1 || posFind >= doclen)
			break;
		if (posFind == posCurrentWord) {
			ft.chrg.cpMin = posFind + rootlen;
			continue;
		}
		char wordstart[WORDCHUNK];
		GetRange(wEditor, posFind, Platform::Minimum(posFind + WORDCHUNK - 1, doclen), wordstart);
		char *wordend = wordstart + rootlen;
		while (iswordcharforsel(*wordend))
			wordend++;
		*wordend = '\0';
		int wordlen = wordend - wordstart;
		const char *wordbreak = words;
		const char *wordpos;
		for (;;) {	// searches the found word in the storage
			wordpos = strstr (wordbreak, wordstart);
			if (!wordpos)
				break;
			if (wordpos > words && wordpos[ -1] != ' ' ||
			        wordpos[wordlen] && wordpos[wordlen] != ' ')
				wordbreak = wordpos + wordlen;
			else
				break;
		}
		if (!wordpos) {	// add a new entry
			newlength = length + wordlen;
			if (length)
				newlength++;
			if (newlength >= size) {
				do
					size += WORDCHUNK;
				while (size <= newlength);
				words = (char*) realloc (words, size);
			}
			if (length)
				words[length++] = ' ';
			memcpy (words + length, wordstart, wordlen);
			length = newlength;
			words[length] = '\0';
		}
		ft.chrg.cpMin = posFind + wordlen;
	}
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found %d characters took %d\n", root, length, dwEnd - dwStart);
	if (length) {
		SendEditorString(SCI_AUTOCSHOW, rootlen, words);
	}
	free(words);
	return true;
}

int SciTEBase::GetCurrentLineNumber() {
	CharacterRange crange = GetSelection();
	int selStart = crange.cpMin;
	return SendEditor(SCI_LINEFROMPOSITION, selStart);
}

int SciTEBase::GetCurrentScrollPosition() {
	int lineDisplayTop = SendEditor(SCI_GETFIRSTVISIBLELINE);
	return SendEditor(SCI_DOCLINEFROMVISIBLE, lineDisplayTop);
}

void SciTEBase::UpdateStatusBar() {
	if (sbVisible) {
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
		return false;
	} else {
		// Set of individual characters. Only one character allowed for now
		char ch = symbols.words[0];
		return strchr(value.c_str(), ch) != 0;
	}
}

#define ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))

int SciTEBase::GetIndentState(int line) {
	// C like language indentation defined by braces and keywords
	int indentState = 0;
	SString controlWords[10];
	GetLinePartsInStyle(line, SCE_C_WORD, -1, controlWords, ELEMENTS(controlWords));
	for (unsigned int i = 0;i < ELEMENTS(controlWords);i++) {
		if (includes(statementIndent, controlWords[i]))
			indentState = 2;
	}
	// Braces override keywords
	SString controlStrings[10];
	GetLinePartsInStyle(line, SCE_C_OPERATOR, -1, controlStrings, ELEMENTS(controlStrings));
	for (unsigned int j = 0;j < ELEMENTS(controlStrings);j++) {
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

// Upon a character being added, SciTE may decide to perform some action
// such as displaying a completion list.
void SciTEBase::CharAdded(char ch) {
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
				} else if (!isalpha(ch) && (ch != '_')) {
					SendEditor(SCI_AUTOCCANCEL);
				}
			} else {
				if (ch == '(') {
					braceCount = 1;
					StartCallTip();
				} else {
					if (props.GetInt("indent.automatic"))
						AutomaticIndentation(ch);
				}
			}
		}
	}
}

void SciTEBase::GoMatchingBrace(bool select) {
	int braceAtCaret = -1;
	int braceOpposite = -1;
	bool isInside = FindMatchingBracePosition(true, braceAtCaret, braceOpposite, true);
	// Convert the chracter positions into caret positions based on whether
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
		// For the New command, the are you sure question is always asked as this gives
		// an opportunity to abandon the edits made to a file when are.you.sure is turned off.

		if (IsBufferAvailable() || (SaveIfUnsure(true) != IDCANCEL)) {
			New();
			ReadProperties();
		}
		break;
	case IDM_OPEN:
		if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
			OpenDialog();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_PREV:
		Prev();
		SetFocus(wEditor.GetID());
		break;
	case IDM_NEXT:
		Next();
		SetFocus(wEditor.GetID());
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
	case IDM_ABOUT:
		AboutDialog();
		break;
	case IDM_QUIT:
		QuitProgram();
		break;
	case IDM_NEXTFILE:
		if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
			StackMenuNext();
		}
		break;
	case IDM_PREVFILE:
		if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
			StackMenuPrev();
		}
		break;

	case IDM_UNDO:
		SendFocused(SCI_UNDO);
		break;
	case IDM_REDO:
		SendFocused(SCI_REDO);
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

	case IDM_SHOWCALLTIP:
		StartCallTip();
		break;

	case IDM_COMPLETE:
		StartAutoComplete();
		break;

	case IDM_COMPLETEWORD:
		StartAutoCompleteWord();
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

	case IDM_VIEWSTATUSBAR:
		sbVisible = !sbVisible;
		ShowStatusBar();
		UpdateStatusBar();
		CheckMenus();
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
				AddCommand(props.GetNewExpand("command.build.", fileName), "",
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

	case IDM_SRCWIN:
		break;

	case IDM_BOOKMARK_TOGGLE:
		BookmarkToggle();
		break;

	case IDM_BOOKMARK_NEXT:
		BookmarkNext();
		break;

	case IDM_TABSIZE:
		TabSizeDialog();
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
		SetOverrideLanguage(cmdID);
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
		} else if ((cmdID >= fileStackCmdID) &&
		           (cmdID < fileStackCmdID + fileStackMax)) {
			if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
				StackMenu(cmdID - fileStackCmdID);
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
		SendEditor(SCI_SETFOLDEXPANDED, line, 1);
	} else if (levelPrev & SC_FOLDLEVELHEADERFLAG) {
		//Platform::DebugPrintf("Fold removed %d-%d\n", line, SendEditor(SCI_GETLASTCHILD, line));
		if (!SendEditor(SCI_GETFOLDEXPANDED, line)) {
			// Removing the fold from one that has been contracted so dhould expand
			// otherwise lines are left invisibe with no war to make them visible
			Expand(line, true, false, 0, levelPrev);
		}
	}
}

void SciTEBase::Expand(int &line, bool doExpand, bool force, int visLevels, int level) {
	int lineMaxSubord = SendEditor(SCI_GETLASTCHILD, line, level);
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
				if (doExpand && SendEditor(SCI_GETFOLDEXPANDED, line)) {
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
				Expand(line, true);
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
	} else if (SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG) {
		if (modifiers & SCMOD_SHIFT) {
			// Ensure all children visible
			SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
			Expand(lineClick, true, true, 100);
		} else if (modifiers & SCMOD_CTRL) {
			if (SendEditor(SCI_GETFOLDEXPANDED, lineClick)) {
				// Contract this line and all children
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 0);
				Expand(lineClick, false, true, 0);
			} else {
				// Expand this line and all children
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
				Expand(lineClick, true, true, 100);
			}
		} else {
			// Toggle this line
			SendEditor(SCI_TOGGLEFOLD, lineClick);
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
				CheckMenus();
				SetWindowName();
				BuffersMenu();
			}
		}
		break;

	case SCN_SAVEPOINTLEFT:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointLeft();
			if (!handled) {
				isDirty = true;
				isBuilt = false;
				CheckMenus();
				SetWindowName();
				BuffersMenu();
			}
		}
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
				UpdateStatusBar();
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
	CheckAMenuItem(IDM_VIEWSPACE, SendEditor(SCI_GETVIEWWS));
	CheckAMenuItem(IDM_VIEWGUIDES, SendEditor(SCI_GETINDENTATIONGUIDES));
	CheckAMenuItem(IDM_LINENUMBERMARGIN, lineNumbers);
	CheckAMenuItem(IDM_SELMARGIN, margin);
	CheckAMenuItem(IDM_FOLDMARGIN, foldMargin);
	CheckAMenuItem(IDM_VIEWEOL, SendEditor(SCI_GETVIEWEOL));
	CheckAMenuItem(IDM_VIEWTOOLBAR, tbVisible);
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	EnableAMenuItem(IDM_COMPILE, !executing);
	EnableAMenuItem(IDM_BUILD, !executing);
	EnableAMenuItem(IDM_GO, !executing);
	for (int toolItem = 0; toolItem < toolMax; toolItem++)
		EnableAMenuItem(IDM_TOOLS + toolItem, !executing);
	EnableAMenuItem(IDM_STOPEXECUTE, executing);
	if (buffers.size > 0) {
		for (int bufferItem = 0; bufferItem < buffers.length; bufferItem++) {
			CheckAMenuItem(IDM_BUFFER + bufferItem, bufferItem == buffers.current);
		}
	}
}


// Ensure that a splitter bar position is inside the main window
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

}

// Implement ExtensionAPI methods
int SciTEBase::Send(Pane p, unsigned int msg, unsigned long wParam, long lParam) {
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
