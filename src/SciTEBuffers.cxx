// SciTE - Scintilla based Text Editor
/** @file SciTEBuffers.cxx
 ** Buffers and jobs management.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>
#include <gtk/gtk.h>

#endif

#if PLAT_WIN

#define _WIN32_WINNT  0x0400
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

// For chdir
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
#include "Scintilla.h"
#include "SciLexer.h"
#include "Extender.h"
#include "SciTEBase.h"

const char recentFileName[] = "SciTE.recent";
const char defaultSessionFileName[] = "SciTE.ses";

Job::Job() {
	Clear();
}

void Job::Clear() {
	command = "";
	directory = "";
	jobType = jobCLI;
}

bool FilePath::SameNameAs(const char *other) const {
#ifdef WIN32
	return EqualCaseInsensitive(fileName.c_str(), other);
#else

	return fileName == other;
#endif
}

bool FilePath::SameNameAs(const FilePath &other) const {
	return SameNameAs(other.fileName.c_str());
}

bool FilePath::IsUntitled() const {
	const char *dirEnd = strrchr(fileName.c_str(), pathSepChar);
	return !dirEnd || !dirEnd[1];
}

const char *FilePath::FullPath() const {
	return fileName.c_str();
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
	if (length < size) {
		length++;
	}
	buffers[length - 1].Init();
	return length - 1;
}

int BufferList::GetDocumentByName(const char *filename) {
	if (!filename || !filename[0]) {
		return -1;
	}
	for (int i = 0;i < length;i++) {
		if (buffers[i].SameNameAs(filename)) {
			return i;
		}
	}
	return -1;
}

void BufferList::RemoveCurrent() {
	// Delete and move up to fill gap but ensure doc pointer is saved.
	int currentDoc = buffers[current].doc;
	for (int i = current;i < length - 1;i++) {
		buffers[i] = buffers[i + 1];
	}
	buffers[length - 1].doc = currentDoc;
	if (length > 1) {
		length--;
		buffers[length].Init();
		if (current >= length) {
			current = length - 1;
		}
		if (current < 0) {
			current = 0;
		}
	} else {
		buffers[current].Init();
	}
}

sptr_t SciTEBase::GetDocumentAt(int index) {
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
	        buffers.current >= buffers.length) {
		return;
	}
	UpdateBuffersCurrent();

	buffers.current = index;

	Buffer bufferNext = buffers.buffers[buffers.current];
	isDirty = bufferNext.isDirty;
	useMonoFont = bufferNext.useMonoFont;
	unicodeMode = bufferNext.unicodeMode;
	fileModTime = bufferNext.fileModTime;
	overrideExtension = bufferNext.overrideExtension;
	SetFileName(bufferNext.FullPath());
	SendEditor(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.current));
	SetWindowName();
	ReadProperties();
	if (unicodeMode != uni8Bit) {
		// Override the code page if Unicode
		codePage = SC_CP_UTF8;
		SendEditor(SCI_SETCODEPAGE, codePage);
	}
	isReadOnly = SendEditor(SCI_GETREADONLY);

#if PLAT_WIN
	// Tab Bar
	::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()), TCM_SETCURSEL, (WPARAM)index, (LPARAM)0);
#endif

	DisplayAround(bufferNext);

	CheckMenus();
	UpdateStatusBar(true);

	if (extender)
		extender->OnSwitchFile(fullPath);
}

void SciTEBase::UpdateBuffersCurrent() {
	if ((buffers.length > 0) && (buffers.current >= 0)) {
		buffers.buffers[buffers.current].Set(fullPath);
		buffers.buffers[buffers.current].selection = GetSelection();
		buffers.buffers[buffers.current].scrollPosition = GetCurrentScrollPosition();
		buffers.buffers[buffers.current].isDirty = isDirty;
		buffers.buffers[buffers.current].useMonoFont = useMonoFont;
		buffers.buffers[buffers.current].fileModTime = fileModTime;
		buffers.buffers[buffers.current].overrideExtension = overrideExtension;
		buffers.buffers[buffers.current].unicodeMode = unicodeMode;
	}
}

bool SciTEBase::IsBufferAvailable() {
	return buffers.size > 1 && buffers.length < buffers.size;
}

bool SciTEBase::CanMakeRoom(bool maySaveIfDirty) {
	if (IsBufferAvailable()) {
		return true;
	} else if (maySaveIfDirty) {
		// All available buffers are taken, try and close the current one
		if (SaveIfUnsure(true) != IDCANCEL) {
			// The file isn't dirty, or the user agreed to close the current one
			return true;
		}
	} else {
		return true;	// Told not to save so must be OK.
	}
	return false;
}

bool IsUntitledFileName(const char *name) {
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
		if (buffersWanted > bufferMax) {
			buffersWanted = bufferMax;
		}
		if (buffersWanted < 1) {
			buffersWanted = 1;
		}
		buffers.Allocate(buffersWanted);
		// First document is the default from creation of control
		buffers.buffers[0].doc = SendEditor(SCI_GETDOCPOINTER, 0, 0);
		SendEditor(SCI_ADDREFDOCUMENT, 0, buffers.buffers[0].doc); // We own this reference
		if (buffersWanted == 1) {
			// No buffers, delete the Buffers main menu entry
			DestroyMenuItem(menuBuffers, 0);
#if PLAT_WIN
			// Make previous change visible.
			::DrawMenuBar(reinterpret_cast<HWND>(wSciTE.GetID()));
			// Destroy command "View Tab Bar" in the menu "View"
			DestroyMenuItem(menuView, IDM_VIEWTABBAR);
#endif

		}
	}
}

static void EnsureEndsWithPathSeparator(char *path) {
	size_t pathLen = strlen(path);
	if ((pathLen > 0) && path[pathLen - 1] != pathSepChar) {
		strcat(path, pathSepString);
	}
}

static void RecentFilePath(char *path, const char *name) {
	char *where = getenv("SciTE_HOME");
	if (!where)
		where = getenv("HOME");
	if (!where) {
#if PLAT_WIN
		::GetModuleFileName(0, path, MAX_PATH);
		char *lastSlash = strrchr(path, pathSepChar);
		if (lastSlash)
			*(lastSlash + 1) = '\0';
#else

		*path = '\0';
#endif

	} else {
		strcpy(path, where);
		EnsureEndsWithPathSeparator(path);
	}
	strcat(path, configFileVisibilityString);
	strcat(path, name);
}

void SciTEBase::LoadRecentMenu() {
	char recentPathName[MAX_PATH + 1];
	RecentFilePath(recentPathName, recentFileName);
	FILE *recentFile = fopen(recentPathName, fileRead);
	if (!recentFile) {
		DeleteFileStackMenu();
		return;
	}
	char line[MAX_PATH + 1];
	CharacterRange cr;
	cr.cpMin = cr.cpMax = 0;
	for (int i = 0; i < fileStackMax; i++) {
		if (!fgets (line, sizeof (line), recentFile))
			break;
		line[strlen (line) - 1] = '\0';
		AddFileToStack(line, cr, 0);
	}
	fclose(recentFile);
}

void SciTEBase::SaveRecentStack() {
	char recentPathName[MAX_PATH + 1];
	RecentFilePath(recentPathName, recentFileName);
	FILE *recentFile = fopen(recentPathName, fileWrite);
	if (!recentFile)
		return;
	int i;
	// save recent files list
	for (i = fileStackMax - 1; i >= 0; i--) {
		if (recentFileStack[i].IsSet())
			fprintf(recentFile, "%s\n", recentFileStack[i].FullPath());
	}
	// save buffers list
	for (i = buffers.length - 1; i >= 0; i--) {
		if (buffers.buffers[i].IsSet())
			fprintf(recentFile, "%s\n", buffers.buffers[i].FullPath());
	}
	fclose(recentFile);
}

void SciTEBase::LoadSession(const char *sessionName) {
	char sessionPathName[MAX_PATH + 1];
	if (sessionName[0] == '\0') {
		RecentFilePath(sessionPathName, defaultSessionFileName);
	} else {
		strcpy(sessionPathName, sessionName);
	}
	FILE *sessionFile = fopen(sessionPathName, fileRead);
	if (!sessionFile)
		return;
	// comment next line if you don't want to close all buffers before loading session
	CloseAllBuffers();
	int curr = -1, pos = 0;
	char *file, line[MAX_PATH + 128];
	for (int i = 0; i < bufferMax; i++) {
		if (!fgets (line, sizeof (line), sessionFile))
			break;
		line[strlen (line) - 1] = '\0';
		if (sscanf(line, "<pos=%i>", &pos) != 1)
			break;
		file = strchr(line, '>') + 2;
		//Platform::DebugPrintf("pos=%i file=:%s:", pos, file);
		if (pos < 0) {
			curr = i;
			pos = -pos;
		}
		AddFileToBuffer(file, pos - 1);
	}
	fclose(sessionFile);
	if (curr != -1)
		SetDocumentAt(curr);
}

void SciTEBase::SaveSession(const char *sessionName) {
	char sessionPathName[MAX_PATH + 1];
	if (sessionName[0] == '\0') {
		RecentFilePath(sessionPathName, defaultSessionFileName);
	} else {
		strcpy(sessionPathName, sessionName);
	}
	FILE *sessionFile = fopen(sessionPathName, fileWrite);
	if (!sessionFile)
		return;
	int curr = buffers.current;
	for (int i = 0; i < buffers.length; i++) {
		if (buffers.buffers[i].IsSet() && !buffers.buffers[i].IsUntitled()) {
			int pos;
			SetDocumentAt(i);
			pos = SendEditor(SCI_GETCURRENTPOS) + 1;
			if (i == curr)
				pos = -pos;
			fprintf(sessionFile, "<pos=%i> %s\n", pos, buffers.buffers[i].FullPath());
		}
	}
	fclose(sessionFile);
	SetDocumentAt(curr);
}

void SciTEBase::SetIndentSettings() {
	// Get default values
	int useTabs = props.GetInt("use.tabs", 1);
	int tabSize = props.GetInt("tabsize");
	int indentSize = props.GetInt("indent.size");
	// Either set the settings related to the extension or the default ones
	SString fileNameForExtension = ExtensionFileName();
	SString useTabsChars = props.GetNewExpand("use.tabs.",
	                       fileNameForExtension.c_str());
	if (useTabsChars.length() != 0) {
		SendEditor(SCI_SETUSETABS, useTabsChars.value());
	} else {
		SendEditor(SCI_SETUSETABS, useTabs);
	}
	SString tabSizeForExt = props.GetNewExpand("tab.size.",
	                        fileNameForExtension.c_str());
	if (tabSizeForExt.length() != 0) {
		SendEditor(SCI_SETTABWIDTH, tabSizeForExt.value());
	} else if (tabSize != 0) {
		SendEditor(SCI_SETTABWIDTH, tabSize);
	}
	SString indentSizeForExt = props.GetNewExpand("indent.size.",
	                           fileNameForExtension.c_str());
	if (indentSizeForExt.length() != 0) {
		SendEditor(SCI_SETINDENT, indentSizeForExt.value());
	} else {
		SendEditor(SCI_SETINDENT, indentSize);
	}
}

void SciTEBase::SetEol() {
	SString eol_mode = props.Get("eol.mode");
	if (eol_mode == "LF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode == "CR") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode == "CRLF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
	}
}

void SciTEBase::New() {
	InitialiseBuffers();
	UpdateBuffersCurrent();

	if ((buffers.size == 1) && (!buffers.buffers[0].IsUntitled())) {
		AddFileToStack(buffers.buffers[0].FullPath(),
		               buffers.buffers[0].selection,
		               buffers.buffers[0].scrollPosition);
	}

	// If the current buffer is the initial untitled, clean buffer then overwrite it,
	// otherwise add a new buffer.
	if ((buffers.length > 1) ||
	        (buffers.current != 0) ||
	        (buffers.buffers[0].isDirty) ||
	        (!buffers.buffers[0].IsUntitled()))
		buffers.current = buffers.Add();

	sptr_t doc = GetDocumentAt(buffers.current);
	SendEditor(SCI_SETDOCPOINTER, 0, doc);

	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	SetFileName(fileName);
	isDirty = false;
	isBuilding = false;
	isBuilt = false;
	isReadOnly = false;	// No sense to create an empty, read-only buffer...

	ClearDocument();
	DeleteFileStackMenu();
	SetFileStackMenu();
}

void SciTEBase::Close(bool updateUI) {
	if (buffers.size == 1) {
		// With no buffer list, Close means close from MRU
		buffers.buffers[0].Init();
		buffers.buffers[0].useMonoFont = useMonoFont;
		fullPath[0] = '\0';
		StackMenu(0);
	} else {
		if (buffers.current >= 0 && buffers.current < buffers.length) {
			UpdateBuffersCurrent();
			Buffer buff = buffers.buffers[buffers.current];
			AddFileToStack(buff.FullPath(), buff.selection, buff.scrollPosition);
		}
		bool closingLast = buffers.length == 1;
		if (closingLast) {
			buffers.buffers[0].Init();
			buffers.buffers[0].useMonoFont = useMonoFont;
		} else {
			buffers.RemoveCurrent();
		}
		Buffer bufferNext = buffers.buffers[buffers.current];
		isDirty = bufferNext.isDirty;
		useMonoFont = bufferNext.useMonoFont;
		unicodeMode = bufferNext.unicodeMode;
		fileModTime = bufferNext.fileModTime;
		overrideExtension = bufferNext.overrideExtension;
		if (updateUI)
			SetFileName(bufferNext.FullPath());
		SendEditor(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.current));
		if (closingLast) {
			ClearDocument();
		}
		if (updateUI) {
			SetWindowName();
			ReadProperties();
			if (unicodeMode != uni8Bit) {
				// Override the code page if Unicode
				codePage = SC_CP_UTF8;
				SendEditor(SCI_SETCODEPAGE, codePage);
			}
			isReadOnly = SendEditor(SCI_GETREADONLY);
			DisplayAround(bufferNext);
		}
	}
	if (updateUI) {
		BuffersMenu();
		UpdateStatusBar(true);
	}
}

void SciTEBase::CloseAllBuffers() {
	if (SaveAllBuffers(false) != IDCANCEL) {
		while (buffers.length > 1)
			Close(false);

		Close();
	}
}

int SciTEBase::SaveAllBuffers(bool forceQuestion, bool alwaysYes) {
	int choice = IDYES;
	UpdateBuffersCurrent();	// Ensure isDirty copied
	int currentBuffer = buffers.current;
	for (int i = 0; (i < buffers.length) && (choice != IDCANCEL); i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (alwaysYes) {
				if (!Save()) {
					choice = IDCANCEL;
				}
			} else {
				choice = SaveIfUnsure(forceQuestion);
			}
		}
	}
	SetDocumentAt(currentBuffer);
	return choice;
}

void SciTEBase::Next() {
	int next = buffers.current;
	if (++next >= buffers.length)
		next = 0;
	SetDocumentAt(next);
	CheckReload();
}

void SciTEBase::Prev() {
	int prev = buffers.current;
	if (--prev < 0)
		prev = buffers.length - 1;
	SetDocumentAt(prev);
	CheckReload();
}

void SciTEBase::BuffersMenu() {
	UpdateBuffersCurrent();
	DestroyMenuItem(menuBuffers, IDM_BUFFERSEP);
#if PLAT_WIN

	::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()), TCM_DELETEALLITEMS, (WPARAM)0, (LPARAM)0);
#endif

	int pos;
	for (pos = 0; pos < bufferMax; pos++) {
		DestroyMenuItem(menuBuffers, IDM_BUFFER + pos);
	}
	if (buffers.size > 1) {
		int menuStart = 5;
		SetMenuItem(menuBuffers, menuStart, IDM_BUFFERSEP, "");
		for (pos = 0; pos < buffers.length; pos++) {
			int itemID = bufferCmdID + pos;
			char entry[MAX_PATH*2 + 20];
			entry[0] = '\0';
			char titleTab[MAX_PATH*2 + 20];
			titleTab[0] = '\0';
#if PLAT_WIN

			if (pos < 10) {
				sprintf(entry, "&%d ", (pos + 1) % 10 ); // hotkey 1..0
				sprintf(titleTab, "&%d ", (pos + 1) % 10); // add hotkey to the tabbar
			}
#endif

			if (buffers.buffers[pos].IsUntitled()) {
				SString untitled = LocaliseString("Untitled");
				strcat(entry, untitled.c_str());
				strcat(titleTab, untitled.c_str());
			} else {
				SString path = buffers.buffers[pos].FullPath();
#if PLAT_WIN
				// Handle '&' characters in path, since they are interpreted in
				// menues and tab names.
				int amp = 0;
				while ((amp = path.search("&", amp)) >= 0) {
					path.insert(amp, "&");
					amp += 2;
				}
#endif
				strcat(entry, path.c_str());

				char *cpDirEnd = strrchr(entry, pathSepChar);
				if (cpDirEnd) {
					strcat(titleTab, cpDirEnd + 1);
				} else {
					strcat(titleTab, entry);
				}
			}
			// For short file names:
			//char *cpDirEnd = strrchr(buffers.buffers[pos]->fileName, pathSepChar);
			//strcat(entry, cpDirEnd + 1);

			if (buffers.buffers[pos].isDirty) {
				strcat(entry, " *");
				strcat(titleTab, " *");
			}

			SetMenuItem(menuBuffers, menuStart + pos + 1, itemID, entry);
#if PLAT_WIN
			// Windows specific !
			TCITEM tie;
			tie.mask = TCIF_TEXT | TCIF_IMAGE;
			tie.iImage = -1;

			tie.pszText = titleTab;
			::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()), TCM_INSERTITEM, (WPARAM)pos, (LPARAM)&tie);
			//::SendMessage(wTabBar.GetID(), TCM_SETCURSEL, (WPARAM)pos, (LPARAM)0);
#endif

		}
	}
	CheckMenus();
	if (tabVisible)
		SizeSubWindows();
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(menuFile, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(menuFile, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	if (recentFileStack[0].IsSet()) {
		SetMenuItem(menuFile, MRU_START, IDM_MRU_SEP, "");
		for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
			//Platform::DebugPrintf("Setfile %d %s\n", stackPos, recentFileStack[stackPos].fileName.c_str());
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].IsSet()) {
				char entry[MAX_PATH + 20];
				entry[0] = '\0';
#if PLAT_WIN

				sprintf(entry, "&%d ", (stackPos + 1) % 10);
#endif

				strcat(entry, recentFileStack[stackPos].FullPath());
				SetMenuItem(menuFile, MRU_START + stackPos + 1, itemID, entry);
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

void SciTEBase::AddFileToBuffer(const char *file, int pos) {
	FILE *fp = fopen(file, fileRead);  // file existence test
	if (fp) {                      // for missing files Open() gives an empty buffer - do not want this
		fclose(fp);
		Open(file, false, file);
		SendEditor(SCI_GOTOPOS, pos);
	}
}

void SciTEBase::AddFileToStack(const char *file, CharacterRange selection, int scrollPos) {
	if (!file)
		return;
	DeleteFileStackMenu();
	// Only stack non-empty names
	if ((file[0]) && (file[strlen(file) - 1] != pathSepChar)) {
		int stackPos;
		int eqPos = fileStackMax - 1;
		for (stackPos = 0; stackPos < fileStackMax; stackPos++)
			if (recentFileStack[stackPos].SameNameAs(file))
				eqPos = stackPos;
		for (stackPos = eqPos; stackPos > 0; stackPos--)
			recentFileStack[stackPos] = recentFileStack[stackPos - 1];
		recentFileStack[0].Set(file);
		recentFileStack[0].selection = selection;
		recentFileStack[0].scrollPosition = scrollPos;
	}
	SetFileStackMenu();
}

void SciTEBase::RemoveFileFromStack(const char *file) {
	if (!file || !file[0])
		return;
	DeleteFileStackMenu();
	int stackPos;
	for (stackPos = 0; stackPos < fileStackMax; stackPos++) {
		if (recentFileStack[stackPos].SameNameAs(file)) {
			for (int movePos = stackPos; movePos < fileStackMax - 1; movePos++)
				recentFileStack[movePos] = recentFileStack[movePos + 1];
			recentFileStack[fileStackMax - 1].Init();
			break;
		}
	}
	SetFileStackMenu();
}

RecentFile SciTEBase::GetFilePosition() {
	RecentFile rf;
	rf.selection = GetSelection();
	rf.scrollPosition = GetCurrentScrollPosition();
	return rf;
}

void SciTEBase::DisplayAround(const RecentFile &rf) {
	if ((rf.selection.cpMin != INVALID_POSITION) && (rf.selection.cpMax != INVALID_POSITION)) {
		// This can produce better file state restoring
		bool foldOnOpen = props.GetInt("fold.on.open");
		if (foldOnOpen)
			FoldAll();

		int lineStart = SendEditor(SCI_LINEFROMPOSITION, rf.selection.cpMin);
		SendEditor(SCI_ENSUREVISIBLEENFORCEPOLICY, lineStart);
		int lineEnd = SendEditor(SCI_LINEFROMPOSITION, rf.selection.cpMax);
		SendEditor(SCI_ENSUREVISIBLEENFORCEPOLICY, lineEnd);
		SetSelection(rf.selection.cpMax, rf.selection.cpMin);

		// Folding can mess up next scrolling, so will be better without scrolling
		if (!foldOnOpen) {
			int curTop = SendEditor(SCI_GETFIRSTVISIBLELINE);
			int lineTop = SendEditor(SCI_VISIBLEFROMDOCLINE, rf.scrollPosition);
			SendEditor(SCI_LINESCROLL, 0, lineTop - curTop);
		}
	}
}

// Next and Prev file comments.
// Decided that "Prev" file should mean the file you had opened last
// This means "Next" file means the file you opened longest ago.
void SciTEBase::StackMenuNext() {
	DeleteFileStackMenu();
	for (int stackPos = fileStackMax - 1; stackPos >= 0;stackPos--) {
		if (recentFileStack[stackPos].IsSet()) {
			SetFileStackMenu();
			StackMenu(stackPos);
			return;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::StackMenuPrev() {
	if (recentFileStack[0].IsSet()) {
		// May need to restore last entry if removed by StackMenu
		RecentFile rfLast = recentFileStack[fileStackMax - 1];
		StackMenu(0);	// Swap current with top of stack
		for (int checkPos = 0; checkPos < fileStackMax; checkPos++) {
			if (rfLast.SameNameAs(recentFileStack[checkPos])) {
				rfLast.Init();
			}
		}
		// And rotate the MRU
		RecentFile rfCurrent = recentFileStack[0];
		// Move them up
		for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++) {
			recentFileStack[stackPos] = recentFileStack[stackPos + 1];
		}
		recentFileStack[fileStackMax - 1].Init();
		// Copy current file into first empty
		for (int emptyPos = 0; emptyPos < fileStackMax; emptyPos++) {
			if (!recentFileStack[emptyPos].IsSet()) {
				if (rfLast.IsSet()) {
					recentFileStack[emptyPos] = rfLast;
					rfLast.Init();
				} else {
					recentFileStack[emptyPos] = rfCurrent;
					break;
				}
			}
		}

		DeleteFileStackMenu();
		SetFileStackMenu();
	}
}

void SciTEBase::StackMenu(int pos) {
	//Platform::DebugPrintf("Stack menu %d\n", pos);
	if (SaveIfUnsure() != IDCANCEL) {
		if (pos >= 0) {
			if ((pos == 0) && (!recentFileStack[pos].IsSet())) {	// Empty
				New();
				SetWindowName();
				ReadProperties();
				SetIndentSettings();
			} else if (recentFileStack[pos].IsSet()) {
				RecentFile rf = recentFileStack[pos];
				//Platform::DebugPrintf("Opening pos %d %s\n",recentFileStack[pos].lineNumber,recentFileStack[pos].fileName);
				overrideExtension = "";
				// Already asked user so don't allow Open to ask again.
				Open(rf.FullPath(), false, false, false);
				DisplayAround(rf);
			}
		}
	}
}

void SciTEBase::RemoveToolsMenu() {
	for (int pos = 0; pos < toolMax; pos++) {
		DestroyMenuItem(menuTools, IDM_TOOLS + pos);
	}
}

void SciTEBase::SetMenuItemLocalised(int menuNumber, int position, int itemID,
                                     const char *text, const char *mnemonic) {
	SString localised = LocaliseString(text);
	SetMenuItem(menuNumber, position, itemID, localised.c_str(), mnemonic);
}

void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	int menuPos = TOOLS_START;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		SString prefix = "command.name.";
		prefix += SString(item);
		prefix += ".";
		SString commandName = props.GetNewExpand(prefix.c_str(), fileName);
		if (commandName.length()) {
			SString sMenuItem = commandName;
			SString sMnemonic = "Ctrl+";
			sMnemonic += SString(item);
			SetMenuItem(menuTools, menuPos, itemID, sMenuItem.c_str(), sMnemonic.c_str());
			menuPos++;
		}
	}

	DestroyMenuItem(menuTools, IDM_MACRO_SEP);
	DestroyMenuItem(menuTools, IDM_MACROLIST);
	DestroyMenuItem(menuTools, IDM_MACROPLAY);
	DestroyMenuItem(menuTools, IDM_MACRORECORD);
	DestroyMenuItem(menuTools, IDM_MACROSTOPRECORD);
	menuPos++;
	if (macrosEnabled) {
		SetMenuItem(menuTools, menuPos++, IDM_MACRO_SEP, "");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROLIST,
		                     "&List Macros...", "Shift+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROPLAY,
		                     "Run Current &Macro", "F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACRORECORD,
		                     "&Record Macro", "Ctrl+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROSTOPRECORD,
		                     "S&top Recording Macro", "Ctrl+Shift+F9");
	}
}

JobSubsystem SciTEBase::SubsystemType(char c) {
	if (c == '1')
		return jobGUI;
	else if (c == '2')
		return jobShell;
	else if (c == '3')
		return jobExtension;
	else if (c == '4')
		return jobHelp;
	else if (c == '5')
		return jobOtherHelp;
	return jobCLI;
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd, int item) {
	SString subsysprefix = cmd;
	if (item >= 0) {
		subsysprefix += SString(item);
		subsysprefix += ".";
	}
	SString subsystem = props.GetNewExpand(subsysprefix.c_str(), fileName);
	return SubsystemType(subsystem[0]);
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	SString prefix = "command.";
	prefix += SString(item);
	prefix += ".";
	SString command = props.GetWild(prefix.c_str(), fileName);
	if (command.length()) {
		if (SaveIfUnsure() != IDCANCEL) {
			SString isfilter = "command.is.filter.";
			isfilter += SString(item);
			isfilter += ".";
			SString filter = props.GetNewExpand(isfilter.c_str(), fileName);
			if (filter[0] == '1')
				fileModTime -= 1;

			JobSubsystem jobType = SubsystemType("command.subsystem.", item);

			SString inputProp = "command.input.";
			inputProp += SString(item);
			inputProp += ".";

			SString input;
			if (props.GetWild(inputProp.c_str(), fileName).length()) {
				input += props.GetNewExpand(inputProp.c_str(), fileName);
				input += '\x1a';
			}

			AddCommand(command, "", jobType, input);
			if (commandCurrent > 0)
				Execute();
		}
	}
}

int DecodeMessage(char *cdoc, char *sourcePath, int format) {
	sourcePath[0] = '\0';
	switch (format) {
	case SCE_ERR_PYTHON: {
			// Python
			char *startPath = strchr(cdoc, '\"') + 1;
			char *endPath = strchr(startPath, '\"');
			int length = endPath - startPath;
			if (length > 0) {
				strncpy(sourcePath, startPath, length);
				sourcePath[length] = 0;
			}
			endPath++;
			while (*endPath && !isdigit(*endPath)) {
				endPath++;
			}
			int sourceNumber = atoi(endPath) - 1;
			return sourceNumber;
		}
	case SCE_ERR_GCC: {
			// GCC - look for number followed by colon to be line number
			// This will be preceded by file name
			for (int i = 0; cdoc[i]; i++) {
				if (isdigit(cdoc[i]) && cdoc[i + 1] == ':') {
					int j = i;
					while (j > 0 && isdigit(cdoc[j - 1])) {
						j--;
					}
					int sourceNumber = atoi(cdoc + j) - 1;
					strncpy(sourcePath, cdoc, j - 1);
					sourcePath[j - 1] = 0;
					return sourceNumber;
				}
			}
			break;
		}
	case SCE_ERR_MS: {
			// Visual *
			char *endPath = strchr(cdoc, '(');
			int length = endPath - cdoc;
			if ((length > 0) && (length < MAX_PATH)) {
				strncpy(sourcePath, cdoc, length);
				sourcePath[length] = 0;
			}
			endPath++;
			return atoi(endPath) - 1;
		}
	case SCE_ERR_BORLAND: {
			// Borland
			char *space = strchr(cdoc, ' ');
			if (space) {
				while (isspace(*space)) {
					space++;
				}
				while (*space && !isspace(*space)) {
					space++;
				}
				while (isspace(*space)) {
					space++;
				}
				char *space2 = strchr(space, ' ');
				if (space2) {
					unsigned int length = space2 - space;
					strncpy(sourcePath, space, length);
					sourcePath[length] = '\0';
					return atoi(space2) - 1;
				}
			}
			break;
		}
	case SCE_ERR_PERL: {
			// perl
			char *at = strstr(cdoc, " at ");
			char *line = strstr(cdoc, " line ");
			int length = line - (at + 4);
			if (at && line && length > 0) {
				strncpy(sourcePath, at + 4, length);
				sourcePath[length] = 0;
				line += 6;
				return atoi(line) - 1;
			}
			break;
		}
	case SCE_ERR_NET: {
			// .NET traceback
			char *in = strstr(cdoc, " in ");
			char *line = strstr(cdoc, ":line ");
			if (in && line && (line > in)) {
				in += 4;
				strncpy(sourcePath, in, line - in);
				sourcePath[line - in] = 0;
				line += 6;
				return atoi(line) - 1;
			}
		}
	case SCE_ERR_LUA: {
			// Lua error look like: last token read: `result' at line 40 in file `Test.lua'
			char *idLine = "at line ";
			char *idFile = "file ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			char *line = strstr(cdoc, idLine);
			char *file = strstr(cdoc, idFile);
			if (line && file) {
				char *quote = strstr(file, "'");
				size_t length = quote - (file + lenFile + 1);
				if (quote && length > 0) {
					strncpy(sourcePath, file + lenFile + 1, length);
					sourcePath[length] = '\0';
				}
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_CTAG: {
			//~ char *endPath = strchr(cdoc, '\t');
			//~ int length = endPath - cdoc;
			//~ if ((length > 0) && (length < MAX_PATH)) {
			//~ strncpy(sourcePath, cdoc, length);
			//~ sourcePath[length] = 0;
			//~ }
			for (int i = 0; cdoc[i]; i++) {
				if ((isdigit(cdoc[i + 1]) || (cdoc[i + 1] == '/' && cdoc[i + 2] == '^')) && cdoc[i] == '\t') {
					int j = i - 1;
					while (j > 0 && ! strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j--;
					}
					if (strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j++;
					}
					strncpy(sourcePath, &cdoc[j], i - j);
					sourcePath[i - j] = 0;
					// Because usually the address is a searchPattern, lineNumber has to be evaluated later
					return 0;
				}
			}
		}
	case SCE_ERR_PHP: {
			// PHP error look like: Fatal error: Call to undefined function:  foo() in example.php on line 11
			char *idLine = " on line ";
			char *idFile = " in ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			char *line = strstr(cdoc, idLine);
			char *file = strstr(cdoc, idFile);
			if (line && file) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	}	// switch
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
		if (style != SCE_ERR_DEFAULT &&
		        style != SCE_ERR_CMD &&
		        (style < SCE_ERR_DIFF_CHANGED || style > SCE_ERR_DIFF_MESSAGE)) {
			//Platform::DebugPrintf("Marker to %d\n", lookLine);
			SendOutput(SCI_MARKERDELETEALL, static_cast<uptr_t>( -1));
			SendOutput(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			SendOutput(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
			           "error.marker.fore", ColourDesired(0x7f, 0, 0)));
			SendOutput(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
			           "error.marker.back", ColourDesired(0xff, 0xff, 0)));
			SendOutput(SCI_MARKERADD, lookLine, 0);
			SendOutput(SCI_SETSEL, startPosLine, startPosLine);
			char *cdoc = new char[lineLength + 1];
			if (!cdoc)
				return;
			GetRange(wOutput, startPosLine, startPosLine + lineLength, cdoc);
			char sourcePath[MAX_PATH];
			int sourceLine = DecodeMessage(cdoc, sourcePath, style);
			//printf("<%s> %d %d\n",sourcePath, sourceLine, lookLine);
			//Platform::DebugPrintf("<%s> %d %d\n",sourcePath, sourceLine, lookLine);
			if (sourceLine >= 0) {
				if (0 != strcmp(sourcePath, fileName)) {
					char messagePath[MAX_PATH];
					bool bExists = false;
					if (Exists(dirNameAtExecute.c_str(), sourcePath, messagePath)) {
						bExists = true;
					} else if (Exists(dirNameForExecute.c_str(), sourcePath, messagePath)) {
						bExists = true;
					} else if (Exists(dirName, sourcePath, messagePath)) {
						bExists = true;
					} else if (Exists(NULL, sourcePath, messagePath)) {
						bExists = true;
					}
					if (bExists) {
						if (!Open(messagePath)) {
							delete []cdoc;
							return;
						}
						CheckReload();
					}
				}

				// If ctag then get line number after search tag or use ctag line number
				if (style == SCE_ERR_CTAG) {
					char cTag[200];
					//without following focus GetCTag wouldn't work correct
					WindowSetFocus(wOutput);
					GetCTag(cTag, 200);
					if (cTag[0] != '\0') {
						if (atol(cTag) > 0) {
							//if tag is linenumber, get line
							sourceLine = atol(cTag) - 1;
						} else {
							findWhat = cTag;
							FindNext(false);
							//get linenumber for marker from found position
							sourceLine = SendEditor(SCI_LINEFROMPOSITION, SendEditor(SCI_GETCURRENTPOS));
						}
					}
				}

				SendEditor(SCI_MARKERDELETEALL, 0);
				SendEditor(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				SendEditor(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
				           "error.marker.fore", ColourDesired(0x7f, 0, 0)));
				SendEditor(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
				           "error.marker.back", ColourDesired(0xff, 0xff, 0)));
				SendEditor(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = SendEditor(SCI_POSITIONFROMLINE, sourceLine, 0);
				EnsureRangeVisible(startSourceLine, startSourceLine);
				SetSelection(startSourceLine, startSourceLine);
				WindowSetFocus(wEditor);
			}
			delete []cdoc;
			return;
		}
		lookLine += dir;
		if (lookLine < 0)
			lookLine = maxLine - 1;
		else if (lookLine >= maxLine)
			lookLine = 0;
		if (dir == 0)
			return;
	}
}
