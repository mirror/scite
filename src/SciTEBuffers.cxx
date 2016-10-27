// SciTE - Scintilla based Text Editor
/** @file SciTEBuffers.cxx
 ** Buffers and jobs management.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "Scintilla.h"
#include "SciLexer.h"
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
#include "FileWorker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"

const GUI::gui_char defaultSessionFileName[] = GUI_TEXT("SciTE.session");

void Buffer::DocumentModified() {
	documentModTime = time(0);
}

bool Buffer::NeedsSave(int delayBeforeSave) const {
	time_t now = time(0);
	return now && documentModTime && isDirty && !pFileWorker && (now-documentModTime > delayBeforeSave) && !IsUntitled() && !failedSave;
}

void Buffer::CompleteLoading() {
	lifeState = open;
	if (pFileWorker && pFileWorker->IsLoading()) {
		delete pFileWorker;
		pFileWorker = 0;
	}
}

void Buffer::CompleteStoring() {
	if (pFileWorker && !pFileWorker->IsLoading()) {
		delete pFileWorker;
		pFileWorker = 0;
	}
	SetTimeFromFile();
}

void Buffer::AbandonAutomaticSave() {
	if (pFileWorker && !pFileWorker->IsLoading()) {
		FileStorer *pFileStorer = static_cast<FileStorer *>(pFileWorker);
		if (!pFileStorer->visibleProgress) {
			pFileWorker->Cancel();
			// File is in partially saved state so may be better to remove
		}
	}
}

void Buffer::CancelLoad() {
	// Complete any background loading
	if (pFileWorker && pFileWorker->IsLoading()) {
		pFileWorker->Cancel();
		CompleteLoading();
		lifeState = empty;
	}
}

BufferList::BufferList() : current(0), stackcurrent(0), stack(0), buffers(0), size(0), length(0), lengthVisible(0), initialised(false) {}

BufferList::~BufferList() {
	delete []buffers;
	delete []stack;
}

void BufferList::Allocate(int maxSize) {
	length = 1;
	lengthVisible = 1;
	current = 0;
	size = maxSize;
	buffers = new Buffer[size];
	stack = new int[size];
	stack[0] = 0;
}

int BufferList::Add() {
	if (length < size) {
		length++;
	}
	buffers[length - 1].Init();
	stack[length - 1] = length - 1;
	MoveToStackTop(length - 1);
	SetVisible(length-1, true);

	return lengthVisible - 1;
}

int BufferList::GetDocumentByWorker(FileWorker *pFileWorker) const {
	for (int i = 0;i < length;i++) {
		if (buffers[i].pFileWorker == pFileWorker) {
			return i;
		}
	}
	return -1;
}

int BufferList::GetDocumentByName(const FilePath &filename, bool excludeCurrent) {
	if (!filename.IsSet()) {
		return -1;
	}
	for (int i = 0;i < length;i++) {
		if ((!excludeCurrent || i != current) && buffers[i].SameNameAs(filename)) {
			return i;
		}
	}
	return -1;
}

void BufferList::RemoveInvisible(int index) {
	assert(!GetVisible(index));
	if (index == current) {
		RemoveCurrent();
	} else {
		if (index < length-1) {
			// Swap with last visible
			Swap(index, length-1);
		}
		length--;
	}
}

void BufferList::RemoveCurrent() {
	// Delete and move up to fill gap but ensure doc pointer is saved.
	sptr_t currentDoc = buffers[current].doc;
	buffers[current].CompleteLoading();
	for (int i = current;i < length - 1;i++) {
		buffers[i] = buffers[i + 1];
	}
	buffers[length - 1].doc = currentDoc;

	if (length > 1) {
		CommitStackSelection();
		PopStack();
		length--;
		lengthVisible--;

		buffers[length].Init();
		if (current >= lengthVisible) {
			SetCurrent(lengthVisible - 1);
		}
		if (current < 0) {
			SetCurrent(0);
		}
	} else {
		buffers[current].Init();
	}
	MoveToStackTop(current);
}

int BufferList::Current() const {
	return current;
}

Buffer *BufferList::CurrentBuffer() const {
	return &buffers[Current()];
}

void BufferList::SetCurrent(int index) {
	current = index;
}

void BufferList::PopStack() {
	for (int i = 0; i < length - 1; ++i) {
		int index = stack[i + 1];
		// adjust the index for items that will move in buffers[]
		if (index > current)
			--index;
		stack[i] = index;
	}
}

int BufferList::StackNext() {
	if (++stackcurrent >= length)
		stackcurrent = 0;
	return stack[stackcurrent];
}

int BufferList::StackPrev() {
	if (--stackcurrent < 0)
		stackcurrent = length - 1;
	return stack[stackcurrent];
}

void BufferList::MoveToStackTop(int index) {
	// shift top chunk of stack down into the slot that index occupies
	bool move = false;
	for (int i = length - 1; i > 0; --i) {
		if (stack[i] == index)
			move = true;
		if (move)
			stack[i] = stack[i-1];
	}
	stack[0] = index;
}

void BufferList::CommitStackSelection() {
	// called only when ctrl key is released when ctrl-tabbing
	// or when a document is closed (in case of Ctrl+F4 during ctrl-tabbing)
	MoveToStackTop(stack[stackcurrent]);
	stackcurrent = 0;
}

void BufferList::ShiftTo(int indexFrom, int indexTo) {
	// shift buffer to new place in buffers array
	if (indexFrom == indexTo ||
		indexFrom < 0 || indexFrom >= length ||
		indexTo < 0 || indexTo >= length) return;
	int step = (indexFrom > indexTo) ? -1 : 1;
	Buffer tmp = buffers[indexFrom];
	int i;
	for (i = indexFrom; i != indexTo; i += step) {
		buffers[i] = buffers[i+step];
	}
	buffers[indexTo] = tmp;
	// update stack indexes
	for (i = 0; i < length; i++) {
		if (stack[i] == indexFrom) {
			stack[i] = indexTo;
		} else if (step == 1) {
			if (indexFrom < stack[i] && stack[i] <= indexTo) stack[i] -= step;
		} else {
			if (indexFrom > stack[i] && stack[i] >= indexTo) stack[i] -= step;
		}
	}
}

void BufferList::Swap(int indexA, int indexB) {
	// shift buffer to new place in buffers array
	if (indexA == indexB ||
		indexA < 0 || indexA >= length ||
		indexB < 0 || indexB >= length) return;
	Buffer tmp = buffers[indexA];
	buffers[indexA] = buffers[indexB];
	buffers[indexB] = tmp;
	// update stack indexes
	for (int i = 0; i < length; i++) {
		if (stack[i] == indexA) {
			stack[i] = indexB;
		} else if (stack[i] == indexB) {
			stack[i] = indexA;
		}
	}
}

bool BufferList::SingleBuffer() const {
	return size == 1;
}

BackgroundActivities BufferList::CountBackgroundActivities() const {
	BackgroundActivities bg;
	bg.loaders = 0;
	bg.storers = 0;
	bg.totalWork = 0;
	bg.totalProgress = 0;
	for (int i = 0;i < length;i++) {
		if (buffers[i].pFileWorker) {
			if (!buffers[i].pFileWorker->FinishedJob()) {
				if (!buffers[i].pFileWorker->IsLoading()) {
					FileStorer *fstorer = static_cast<FileStorer*>(buffers[i].pFileWorker);
					if (!fstorer->visibleProgress)
						continue;
				}
				if (buffers[i].pFileWorker->IsLoading())
					bg.loaders++;
				else
					bg.storers++;
				bg.fileNameLast = buffers[i].AsInternal();
				bg.totalWork += buffers[i].pFileWorker->SizeJob();
				bg.totalProgress += buffers[i].pFileWorker->ProgressMade();
			}
		}
	}
	return bg;
}

bool BufferList::SavingInBackground() const {
	for (int i = 0; i<length; i++) {
		if (buffers[i].pFileWorker && !buffers[i].pFileWorker->IsLoading() && !buffers[i].pFileWorker->FinishedJob()) {
			return true;
		}
	}
	return false;
}

bool BufferList::GetVisible(int index) const {
	return index < lengthVisible;
}

void BufferList::SetVisible(int index, bool visible) {
	if (visible != GetVisible(index)) {
		if (visible) {
			if (index > lengthVisible) {
				// Swap with first invisible
				Swap(index, lengthVisible);
			}
			lengthVisible++;
		} else {
			if (index < lengthVisible-1) {
				// Swap with last visible
				Swap(index, lengthVisible-1);
			}
			lengthVisible--;
			if (current >= lengthVisible && lengthVisible > 0)
				SetCurrent(lengthVisible-1);
		}
	}
}

void BufferList::AddFuture(int index, Buffer::FutureDo fd) {
	if (index >= 0 || index < length) {
		buffers[index].futureDo = static_cast<Buffer::FutureDo>(buffers[index].futureDo | fd);
	}
}

void BufferList::FinishedFuture(int index, Buffer::FutureDo fd) {
	if (index >= 0 || index < length) {
		buffers[index].futureDo = static_cast<Buffer::FutureDo>(buffers[index].futureDo & ~(fd));
	}
}

sptr_t SciTEBase::GetDocumentAt(int index) {
	if (index < 0 || index >= buffers.size) {
		return 0;
	}
	if (buffers.buffers[index].doc == 0) {
		// Create a new document buffer
		buffers.buffers[index].doc = wEditor.CallReturnPointer(SCI_CREATEDOCUMENT, 0, 0);
	}
	return buffers.buffers[index].doc;
}

void SciTEBase::SwitchDocumentAt(int index, sptr_t pdoc) {
	if (index < 0 || index >= buffers.size) {
		return;
	}
	sptr_t pdocOld = buffers.buffers[index].doc;
	buffers.buffers[index].doc = pdoc;
	if (pdocOld) {
		wEditor.Call(SCI_RELEASEDOCUMENT, 0, pdocOld);
	}
	if (index == buffers.Current()) {
		wEditor.Call(SCI_SETDOCPOINTER, 0, buffers.buffers[index].doc);
	}
}

void SciTEBase::SetDocumentAt(int index, bool updateStack) {
	int currentbuf = buffers.Current();

	if (	index < 0 ||
	        index >= buffers.length ||
	        index == currentbuf ||
	        currentbuf < 0 ||
	        currentbuf >= buffers.length) {
		return;
	}
	UpdateBuffersCurrent();

	buffers.SetCurrent(index);
	if (updateStack) {
		buffers.MoveToStackTop(index);
	}

	if (extender) {
		if (buffers.size > 1)
			extender->ActivateBuffer(index);
		else
			extender->InitBuffer(0);
	}

	Buffer bufferNext = buffers.buffers[buffers.Current()];
	SetFileName(bufferNext);
	propsDiscovered = bufferNext.props;
	propsDiscovered.superPS = &propsLocal;
	wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
	bool restoreBookmarks = bufferNext.lifeState == Buffer::readAll;
	PerformDeferredTasks();
	if (bufferNext.lifeState == Buffer::readAll) {
		CompleteOpen(ocCompleteSwitch);
		if (extender)
			extender->OnOpen(filePath.AsUTF8().c_str());
	}
	RestoreState(bufferNext, restoreBookmarks);

	TabSelect(index);

	if (lineNumbers && lineNumbersExpand)
		SetLineNumberWidth();

	DisplayAround(bufferNext);
	if (restoreBookmarks) {
		// Restoring a session does not restore the scroll position
		// so make the selection visible.
		wEditor.Call(SCI_SCROLLCARET);
	}

	SetBuffersMenu();
	CheckMenus();
	UpdateStatusBar(true);

	if (extender) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}
}

void SciTEBase::UpdateBuffersCurrent() {
	int currentbuf = buffers.Current();

	if ((buffers.length > 0) && (currentbuf >= 0) && (buffers.GetVisible(currentbuf))) {
		Buffer &bufferCurrent = buffers.buffers[currentbuf];
		bufferCurrent.Set(filePath);
		if (bufferCurrent.lifeState != Buffer::reading && bufferCurrent.lifeState != Buffer::readAll) {
			bufferCurrent.selection.position = wEditor.Call(SCI_GETCURRENTPOS);
			bufferCurrent.selection.anchor = wEditor.Call(SCI_GETANCHOR);
			bufferCurrent.scrollPosition = GetCurrentScrollPosition();

			// Retrieve fold state and store in buffer state info

			std::vector<int> *f = &bufferCurrent.foldState;
			f->clear();

			if (props.GetInt("fold")) {
				for (int line = 0; ; line++) {
					int lineNext = wEditor.Call(SCI_CONTRACTEDFOLDNEXT, line);
					if ((line < 0) || (lineNext < line))
						break;
					line = lineNext;
					f->push_back(line);
				}
			}

			if (props.GetInt("session.bookmarks")) {
				buffers.buffers[buffers.Current()].bookmarks.clear();
				int lineBookmark = -1;
				while ((lineBookmark = wEditor.Call(SCI_MARKERNEXT, lineBookmark + 1, 1 << markerBookmark)) >= 0) {
					bufferCurrent.bookmarks.push_back(lineBookmark);
				}
			}
		}
	}
}

bool SciTEBase::IsBufferAvailable() const {
	return buffers.size > 1 && buffers.length < buffers.size;
}

bool SciTEBase::CanMakeRoom(bool maySaveIfDirty) {
	if (IsBufferAvailable()) {
		return true;
	} else if (maySaveIfDirty) {
		// All available buffers are taken, try and close the current one
		if (SaveIfUnsure(true, static_cast<SaveFlags>(sfProgressVisible | sfSynchronous)) != saveCancelled) {
			// The file isn't dirty, or the user agreed to close the current one
			return true;
		}
	} else {
		return true;	// Told not to save so must be OK.
	}
	return false;
}

void SciTEBase::ClearDocument() {
	wEditor.Call(SCI_SETREADONLY, 0);
	wEditor.Call(SCI_SETUNDOCOLLECTION, 0);
	wEditor.Call(SCI_CLEARALL);
	wEditor.Call(SCI_EMPTYUNDOBUFFER);
	wEditor.Call(SCI_SETUNDOCOLLECTION, 1);
	wEditor.Call(SCI_SETSAVEPOINT);
	wEditor.Call(SCI_SETREADONLY, CurrentBuffer()->isReadOnly);
}

void SciTEBase::CreateBuffers() {
	int buffersWanted = props.GetInt("buffers");
	if (buffersWanted > bufferMax) {
		buffersWanted = bufferMax;
	}
	if (buffersWanted < 1) {
		buffersWanted = 1;
	}
	buffers.Allocate(buffersWanted);
}

void SciTEBase::InitialiseBuffers() {
	if (!buffers.initialised) {
		buffers.initialised = true;
		// First document is the default from creation of control
		buffers.buffers[0].doc = wEditor.CallReturnPointer(SCI_GETDOCPOINTER, 0, 0);
		wEditor.Call(SCI_ADDREFDOCUMENT, 0, buffers.buffers[0].doc); // We own this reference
		if (buffers.size == 1) {
			// Single buffer mode, delete the Buffers main menu entry
			DestroyMenuItem(menuBuffers, 0);
			// Destroy command "View Tab Bar" in the menu "View"
			DestroyMenuItem(menuView, IDM_VIEWTABBAR);
			// Make previous change visible.
			RedrawMenu();
		}
	}
}

FilePath SciTEBase::UserFilePath(const GUI::gui_char *name) {
	GUI::gui_string nameWithVisibility(configFileVisibilityString);
	nameWithVisibility += name;
	return FilePath(GetSciteUserHome(), nameWithVisibility.c_str());
}

static std::string IndexPropKey(const char *bufPrefix, int bufIndex, const char *bufAppendix) {
	std::string pKey = bufPrefix;
	pKey += '.';
	pKey += StdStringFromInteger(bufIndex + 1);
	if (bufAppendix != NULL) {
		pKey += ".";
		pKey += bufAppendix;
	}
	return pKey;
}

void SciTEBase::LoadSessionFile(const GUI::gui_char *sessionName) {
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
	} else {
		sessionPathName.Set(sessionName);
	}

	propsSession.Clear();
	propsSession.Read(sessionPathName, sessionPathName.Directory(), filter, NULL, 0);

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::RestoreRecentMenu() {
	SelectedRange sr(0,0);

	DeleteFileStackMenu();

	for (int i = 0; i < fileStackMax; i++) {
		std::string propKey = IndexPropKey("mru", i, "path");
		std::string propStr = propsSession.GetString(propKey.c_str());
		if (propStr == "")
			continue;
		AddFileToStack(GUI::StringFromUTF8(propStr), sr, 0);
	}
}

static std::vector<int> LinesFromString(const std::string &s) {
	std::vector<int> result;
	if (s.length()) {
		size_t start = 0;
		for (;;) {
			const int line = atoi(s.c_str() + start) - 1;
			result.push_back(line);
			const size_t posComma = s.find(',', start);
			if (posComma == std::string::npos)
				break;
			start = posComma + 1;
		}
	}
	return result;
}

void SciTEBase::RestoreFromSession(const Session &session) {
	for (std::vector<BufferState>::const_iterator bs=session.buffers.begin(); bs != session.buffers.end(); ++bs)
		AddFileToBuffer(*bs);
	int iBuffer = buffers.GetDocumentByName(session.pathActive);
	if (iBuffer >= 0)
		SetDocumentAt(iBuffer);
}

void SciTEBase::RestoreSession() {
	if (props.GetInt("save.find") != 0) {
		for (int i = 0;; i++) {
			std::string propKey = IndexPropKey("search", i, "findwhat");
			std::string propStr = propsSession.GetString(propKey.c_str());
			if (propStr == "")
				break;
			memFinds.AppendList(propStr.c_str());
		}

		for (int i = 0;; i++) {
			std::string propKey = IndexPropKey("search", i, "replacewith");
			std::string propStr = propsSession.GetString(propKey.c_str());
			if (propStr == "")
				break;
			memReplaces.AppendList(propStr.c_str());
		}
	}

	// Comment next line if you don't want to close all buffers before restoring session
	CloseAllBuffers(true);

	Session session;

	for (int i = 0; i < bufferMax; i++) {
		std::string propKey = IndexPropKey("buffer", i, "path");
		std::string propStr = propsSession.GetString(propKey.c_str());
		if (propStr == "")
			continue;

		BufferState bufferState;
		bufferState.Set(GUI::StringFromUTF8(propStr));

		propKey = IndexPropKey("buffer", i, "current");
		if (propsSession.GetInt(propKey.c_str()))
			session.pathActive = bufferState;

		propKey = IndexPropKey("buffer", i, "scroll");
		int scroll = propsSession.GetInt(propKey.c_str());
		bufferState.scrollPosition = scroll;

		propKey = IndexPropKey("buffer", i, "position");
		int pos = propsSession.GetInt(propKey.c_str());

		bufferState.selection.anchor = pos - 1;
		bufferState.selection.position = bufferState.selection.anchor;

		if (props.GetInt("session.bookmarks")) {
			propKey = IndexPropKey("buffer", i, "bookmarks");
			propStr = propsSession.GetString(propKey.c_str());
			bufferState.bookmarks = LinesFromString(propStr);
		}

		if (props.GetInt("fold") && !props.GetInt("fold.on.open") &&
			props.GetInt("session.folds")) {
			propKey = IndexPropKey("buffer", i, "folds");
			propStr = propsSession.GetString(propKey.c_str());
			bufferState.foldState = LinesFromString(propStr);
		}

		session.buffers.push_back(bufferState);
	}

	RestoreFromSession(session);
}

void SciTEBase::SaveSessionFile(const GUI::gui_char *sessionName) {
	UpdateBuffersCurrent();
	bool defaultSession;
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
		defaultSession = true;
	} else {
		sessionPathName.Set(sessionName);
		defaultSession = false;
	}
	FILE *sessionFile = sessionPathName.Open(fileWrite);
	if (!sessionFile)
		return;

	fprintf(sessionFile, "# SciTE session file\n");

	if (defaultSession && props.GetInt("save.position")) {
		int top, left, width, height, maximize;
		GetWindowPosition(&left, &top, &width, &height, &maximize);

		fprintf(sessionFile, "\n");
		fprintf(sessionFile, "position.left=%d\n", left);
		fprintf(sessionFile, "position.top=%d\n", top);
		fprintf(sessionFile, "position.width=%d\n", width);
		fprintf(sessionFile, "position.height=%d\n", height);
		fprintf(sessionFile, "position.maximize=%d\n", maximize);
	}

	if (defaultSession && props.GetInt("save.recent")) {
		std::string propKey;
		int j = 0;

		fprintf(sessionFile, "\n");

		// Save recent files list
		for (int i = fileStackMax - 1; i >= 0; i--) {
			if (recentFileStack[i].IsSet()) {
				propKey = IndexPropKey("mru", j++, "path");
				fprintf(sessionFile, "%s=%s\n", propKey.c_str(), recentFileStack[i].AsUTF8().c_str());
			}
		}
	}

	if (defaultSession && props.GetInt("save.find")) {
		std::string propKey;
		std::vector<std::string>::iterator it;
		std::vector<std::string> mem = memFinds.AsVector();
		if (!mem.empty()) {
			fprintf(sessionFile, "\n");
			it = mem.begin();
			for (int i = 0; it != mem.end(); i++, ++it) {
				propKey = IndexPropKey("search", i, "findwhat");
				fprintf(sessionFile, "%s=%s\n", propKey.c_str(), (*it).c_str());
			}
		}

		mem = memReplaces.AsVector();
		if (!mem.empty()) {
			fprintf(sessionFile, "\n");
			mem = memReplaces.AsVector();
			it = mem.begin();
			for (int i = 0; it != mem.end(); i++, ++it) {
				propKey = IndexPropKey("search", i, "replacewith");
				fprintf(sessionFile, "%s=%s\n", propKey.c_str(), (*it).c_str());
			}
		}
	}

	if (props.GetInt("buffers") && (!defaultSession || props.GetInt("save.session"))) {
		int curr = buffers.Current();
		for (int i = 0; i < buffers.lengthVisible; i++) {
			if (buffers.buffers[i].IsSet() && !buffers.buffers[i].IsUntitled()) {
				Buffer &buff = buffers.buffers[i];
				std::string propKey = IndexPropKey("buffer", i, "path");
				fprintf(sessionFile, "\n%s=%s\n", propKey.c_str(), buff.AsUTF8().c_str());

				int pos = buff.selection.position + 1;
				propKey = IndexPropKey("buffer", i, "position");
				fprintf(sessionFile, "%s=%d\n", propKey.c_str(), pos);

				int scroll = buff.scrollPosition;
				propKey = IndexPropKey("buffer", i, "scroll");
				fprintf(sessionFile, "%s=%d\n", propKey.c_str(), scroll);

				if (i == curr) {
					propKey = IndexPropKey("buffer", i, "current");
					fprintf(sessionFile, "%s=1\n", propKey.c_str());
				}

				if (props.GetInt("session.bookmarks")) {
					bool found = false;
					for (std::vector<int>::iterator itBM=buff.bookmarks.begin();
						itBM != buff.bookmarks.end(); ++itBM) {
						if (!found) {
							propKey = IndexPropKey("buffer", i, "bookmarks");
							fprintf(sessionFile, "%s=%d", propKey.c_str(), *itBM + 1);
							found = true;
						} else {
							fprintf(sessionFile, ",%d", *itBM + 1);
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}

				if (props.GetInt("fold") && props.GetInt("session.folds")) {
					bool found = false;
					for (std::vector<int>::iterator itF=buff.foldState.begin();
						itF != buff.foldState.end(); ++itF) {
						if (!found) {
							propKey = IndexPropKey("buffer", i, "folds");
							fprintf(sessionFile, "%s=%d", propKey.c_str(), *itF + 1);
							found = true;
						} else {
							fprintf(sessionFile, ",%d", *itF + 1);
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}
			}
		}
	}

	if (fclose(sessionFile) != 0) {
		FailedSaveMessageBox(sessionPathName);
	}

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::SetIndentSettings() {
	// Get default values
	int useTabs = props.GetInt("use.tabs", 1);
	int tabSize = props.GetInt("tabsize");
	int indentSize = props.GetInt("indent.size");
	// Either set the settings related to the extension or the default ones
	std::string fileNameForExtension = ExtensionFileName();
	std::string useTabsChars = props.GetNewExpandString("use.tabs.",
	        fileNameForExtension.c_str());
	if (useTabsChars.length() != 0) {
		wEditor.Call(SCI_SETUSETABS, atoi(useTabsChars.c_str()));
	} else {
		wEditor.Call(SCI_SETUSETABS, useTabs);
	}
	std::string tabSizeForExt = props.GetNewExpandString("tab.size.",
	        fileNameForExtension.c_str());
	if (tabSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETTABWIDTH, atoi(tabSizeForExt.c_str()));
	} else if (tabSize != 0) {
		wEditor.Call(SCI_SETTABWIDTH, tabSize);
	}
	std::string indentSizeForExt = props.GetNewExpandString("indent.size.",
	        fileNameForExtension.c_str());
	if (indentSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETINDENT, atoi(indentSizeForExt.c_str()));
	} else {
		wEditor.Call(SCI_SETINDENT, indentSize);
	}
}

void SciTEBase::SetEol() {
	std::string eol_mode = props.GetString("eol.mode");
	if (eol_mode == "LF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode == "CR") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode == "CRLF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CRLF);
	}
}

void SciTEBase::New() {
	InitialiseBuffers();
	UpdateBuffersCurrent();

	propsDiscovered.Clear();

	if ((buffers.size == 1) && (!buffers.buffers[0].IsUntitled())) {
		AddFileToStack(buffers.buffers[0],
		        buffers.buffers[0].selection,
		        buffers.buffers[0].scrollPosition);
	}

	// If the current buffer is the initial untitled, clean buffer then overwrite it,
	// otherwise add a new buffer.
	if ((buffers.length > 1) ||
	        (buffers.Current() != 0) ||
	        (buffers.buffers[0].isDirty) ||
	        (!buffers.buffers[0].IsUntitled())) {
		if (buffers.size == buffers.length) {
			Close(false, false, true);
		}
		buffers.SetCurrent(buffers.Add());
	}

	sptr_t doc = GetDocumentAt(buffers.Current());
	wEditor.Call(SCI_SETDOCPOINTER, 0, doc);

	FilePath curDirectory(filePath.Directory());
	filePath.Set(curDirectory, GUI_TEXT(""));
	SetFileName(filePath);
	UpdateBuffersCurrent();
	SetBuffersMenu();
	CurrentBuffer()->isDirty = false;
	CurrentBuffer()->failedSave = false;
	CurrentBuffer()->lifeState = Buffer::open;
	jobQueue.isBuilding = false;
	jobQueue.isBuilt = false;
	CurrentBuffer()->isReadOnly = false;	// No sense to create an empty, read-only buffer...

	ClearDocument();
	DeleteFileStackMenu();
	SetFileStackMenu();
	if (extender)
		extender->InitBuffer(buffers.Current());
}

void SciTEBase::RestoreState(const Buffer &buffer, bool restoreBookmarks) {
	SetWindowName();
	ReadProperties();
	if (CurrentBuffer()->unicodeMode != uni8Bit) {
		// Override the code page if Unicode
		codePage = SC_CP_UTF8;
		wEditor.Call(SCI_SETCODEPAGE, codePage);
	}

	// check to see whether there is saved fold state, restore
	if (!buffer.foldState.empty()) {
		wEditor.Call(SCI_COLOURISE, 0, -1);
		for (std::vector<int>::const_iterator fold=buffer.foldState.begin(); fold != buffer.foldState.end(); ++fold) {
			wEditor.Call(SCI_TOGGLEFOLD, *fold);
		}
	}
	if (restoreBookmarks) {
		for (std::vector<int>::const_iterator mark=buffer.bookmarks.begin(); mark != buffer.bookmarks.end(); ++mark) {
			wEditor.Call(SCI_MARKERADD, *mark, markerBookmark);
		}
	}
}

void SciTEBase::Close(bool updateUI, bool loadingSession, bool makingRoomForNew) {
	bool closingLast = true;
	int index = buffers.Current();
	if ((index >= 0) && buffers.initialised) {
		buffers.buffers[index].CancelLoad();
	}

	if (extender) {
		extender->OnClose(filePath.AsUTF8().c_str());
	}

	if (buffers.size == 1) {
		// With no buffer list, Close means close from MRU
		closingLast = !(recentFileStack[0].IsSet());
		buffers.buffers[0].Init();
		filePath.Set(GUI_TEXT(""));
		ClearDocument(); //avoid double are-you-sure
		if (!makingRoomForNew)
			StackMenu(0); // calls New, or Open, which calls InitBuffer
	} else if (buffers.size > 1) {
		if (buffers.Current() >= 0 && buffers.Current() < buffers.length) {
			UpdateBuffersCurrent();
			Buffer buff = buffers.buffers[buffers.Current()];
			AddFileToStack(buff, buff.selection, buff.scrollPosition);
		}
		closingLast = (buffers.lengthVisible == 1) && !buffers.buffers[0].pFileWorker;
		if (closingLast) {
			buffers.buffers[0].Init();
			buffers.buffers[0].lifeState = Buffer::open;
			if (extender)
				extender->InitBuffer(0);
		} else {
			if (extender)
				extender->RemoveBuffer(buffers.Current());
			if (buffers.buffers[buffers.Current()].pFileWorker) {
				buffers.SetVisible(buffers.Current(), false);
				if (buffers.lengthVisible == 0)
					New();
			} else {
				wEditor.Call(SCI_SETREADONLY, 0);
				ClearDocument();
				buffers.RemoveCurrent();
			}
			if (extender && !makingRoomForNew)
				extender->ActivateBuffer(buffers.Current());
		}
		Buffer bufferNext = buffers.buffers[buffers.Current()];

		if (updateUI)
			SetFileName(bufferNext);
		else
			filePath = bufferNext;
		propsDiscovered = bufferNext.props;
		propsDiscovered.superPS = &propsLocal;
		wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
		PerformDeferredTasks();
		if (bufferNext.lifeState == Buffer::readAll) {
			//restoreBookmarks = true;
			CompleteOpen(ocCompleteSwitch);
			if (extender)
				extender->OnOpen(filePath.AsUTF8().c_str());
		}
		if (closingLast) {
			wEditor.Call(SCI_SETREADONLY, 0);
			ClearDocument();
		}
		if (updateUI)
			CheckReload();
		if (updateUI) {
			RestoreState(bufferNext, false);
			DisplayAround(bufferNext);
		}
	}

	if (updateUI && buffers.initialised) {
		BuffersMenu();
		UpdateStatusBar(true);
	}

	if (extender && !closingLast && !makingRoomForNew) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}

	if (closingLast && props.GetInt("quit.on.close.last") && !loadingSession) {
		QuitProgram();
	}
}

void SciTEBase::CloseTab(int tab) {
	int tabCurrent = buffers.Current();
	if (tab == tabCurrent) {
		if (SaveIfUnsure() != saveCancelled) {
			Close();
			WindowSetFocus(wEditor);
		}
	} else {
		FilePath fpCurrent = buffers.buffers[tabCurrent].AbsolutePath();
		SetDocumentAt(tab);
		if (SaveIfUnsure() != saveCancelled) {
			Close();
			WindowSetFocus(wEditor);
			// Return to the previous buffer
			SetDocumentAt(buffers.GetDocumentByName(fpCurrent));
		}
	}
}

void SciTEBase::CloseAllBuffers(bool loadingSession) {
	if (SaveAllBuffers(false) != saveCancelled) {
		while (buffers.lengthVisible > 1)
			Close(false, loadingSession);

		Close(true, loadingSession);
	}
}

SciTEBase::SaveResult SciTEBase::SaveAllBuffers(bool alwaysYes) {
	SaveResult choice = saveCompleted;
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; (i < buffers.lengthVisible) && (choice != saveCancelled); i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (alwaysYes) {
				if (!Save()) {
					choice = saveCancelled;
				}
			} else {
				choice = SaveIfUnsure(false);
			}
		}
	}
	SetDocumentAt(currentBuffer);
	return choice;
}

void SciTEBase::SaveTitledBuffers() {
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; i < buffers.lengthVisible; i++) {
		if (buffers.buffers[i].isDirty && !buffers.buffers[i].IsUntitled()) {
			SetDocumentAt(i);
			Save();
		}
	}
	SetDocumentAt(currentBuffer);
}

void SciTEBase::Next() {
	int next = buffers.Current();
	if (++next >= buffers.lengthVisible)
		next = 0;
	SetDocumentAt(next);
	CheckReload();
}

void SciTEBase::Prev() {
	int prev = buffers.Current();
	if (--prev < 0)
		prev = buffers.lengthVisible - 1;

	SetDocumentAt(prev);
	CheckReload();
}

void SciTEBase::ShiftTab(int indexFrom, int indexTo) {
	buffers.ShiftTo(indexFrom, indexTo);
	buffers.SetCurrent(indexTo);
	BuffersMenu();

	TabSelect(indexTo);

	DisplayAround(buffers.buffers[buffers.Current()]);
}

void SciTEBase::MoveTabRight() {
	if (buffers.lengthVisible < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom + 1;
	if (indexTo >= buffers.lengthVisible) indexTo = 0;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::MoveTabLeft() {
	if (buffers.lengthVisible < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom - 1;
	if (indexTo < 0) indexTo = buffers.lengthVisible - 1;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::NextInStack() {
	SetDocumentAt(buffers.StackNext(), false);
	CheckReload();
}

void SciTEBase::PrevInStack() {
	SetDocumentAt(buffers.StackPrev(), false);
	CheckReload();
}

void SciTEBase::EndStackedTabbing() {
	buffers.CommitStackSelection();
}

static void EscapeFilePathsForMenu(GUI::gui_string &path) {
	// Escape '&' characters in path, since they are interpreted in
	// menues.
	Substitute(path, GUI_TEXT("&"), GUI_TEXT("&&"));
#if defined(GTK)
	GUI::gui_string homeDirectory = getenv("HOME");
	if (StartsWith(path, homeDirectory)) {
		path.replace(static_cast<size_t>(0), homeDirectory.size(), GUI_TEXT("~"));
	}
#endif
}

void SciTEBase::SetBuffersMenu() {
	if (buffers.size <= 1) {
        DestroyMenuItem(menuBuffers, IDM_BUFFERSEP);
    }
	RemoveAllTabs();

	int pos;
	for (pos = buffers.lengthVisible; pos < bufferMax; pos++) {
		DestroyMenuItem(menuBuffers, IDM_BUFFER + pos);
	}
	if (buffers.size > 1) {
		int menuStart = 4;
		SetMenuItem(menuBuffers, menuStart, IDM_BUFFERSEP, GUI_TEXT(""));
		for (pos = 0; pos < buffers.lengthVisible; pos++) {
			int itemID = bufferCmdID + pos;
			GUI::gui_string entry;
			GUI::gui_string titleTab;

#if defined(_WIN32) || defined(GTK)
			if (pos < 10) {
				GUI::gui_string sPos = GUI::StringFromInteger((pos + 1) % 10);
				GUI::gui_string sHotKey = GUI_TEXT("&") + sPos + GUI_TEXT(" ");
				entry = sHotKey;	// hotkey 1..0
#if defined(_WIN32)
				titleTab = sHotKey; // add hotkey to the tabbar
#elif defined(GTK)
				titleTab = sPos + GUI_TEXT(" ");
#endif
			}
#endif

			if (buffers.buffers[pos].IsUntitled()) {
				GUI::gui_string untitled = localiser.Text("Untitled");
				entry += untitled;
				titleTab += untitled;
			} else {
				GUI::gui_string path = buffers.buffers[pos].AsInternal();
				GUI::gui_string filename = buffers.buffers[pos].Name().AsInternal();

				EscapeFilePathsForMenu(path);
#if defined(_WIN32)
				// On Windows, '&' are also interpreted in tab names, so we need
				// the escaped filename
				EscapeFilePathsForMenu(filename);
#endif
				entry += path;
				titleTab += filename;
			}
			// For short file names:
			//char *cpDirEnd = strrchr(buffers.buffers[pos]->fileName, pathSepChar);
			//strcat(entry, cpDirEnd + 1);

			if (buffers.buffers[pos].isReadOnly && props.GetInt("read.only.indicator"))  {
				entry=entry+GUI_TEXT(" |");
				titleTab=titleTab+GUI_TEXT(" |");
			}

			if (buffers.buffers[pos].isDirty) {
				entry += GUI_TEXT(" *");
				titleTab += GUI_TEXT(" *");
			}

			SetMenuItem(menuBuffers, menuStart + pos + 1, itemID, entry.c_str());
			TabInsert(pos, titleTab.c_str());
		}
	}
	CheckMenus();
#if !defined(GTK)

	if (tabVisible)
		SizeSubWindows();
#endif
#if defined(GTK)
	ShowTabBar();
#endif
}

void SciTEBase::BuffersMenu() {
	UpdateBuffersCurrent();
	SetBuffersMenu();
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(menuFile, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(menuFile, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	if (recentFileStack[0].IsSet()) {
		SetMenuItem(menuFile, MRU_START, IDM_MRU_SEP, GUI_TEXT(""));
		for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].IsSet()) {
				GUI::gui_string sEntry;

#if defined(_WIN32) || defined(GTK)
				GUI::gui_string sPos = GUI::StringFromInteger((stackPos + 1) % 10);
				GUI::gui_string sHotKey = GUI_TEXT("&") + sPos + GUI_TEXT(" ");
				sEntry = sHotKey;
#endif

				GUI::gui_string path = recentFileStack[stackPos].AsInternal();
				EscapeFilePathsForMenu(path);

				sEntry += path;
				SetMenuItem(menuFile, MRU_START + stackPos + 1, itemID, sEntry.c_str());
			}
		}
	}
}

bool SciTEBase::AddFileToBuffer(const BufferState &bufferState) {
	// Return whether file loads successfully
	bool opened = false;
	if (bufferState.Exists()) {
		opened = Open(bufferState, static_cast<OpenFlags>(ofForceLoad));
		// If forced synchronous should set up position, foldState and bookmarks
		if (opened) {
			int iBuffer = buffers.GetDocumentByName(bufferState, false);
			if (iBuffer >= 0) {
				buffers.buffers[iBuffer].scrollPosition = bufferState.scrollPosition;
				buffers.buffers[iBuffer].selection = bufferState.selection;
				buffers.buffers[iBuffer].foldState = bufferState.foldState;
				buffers.buffers[iBuffer].bookmarks = bufferState.bookmarks;
				if (buffers.buffers[iBuffer].lifeState == Buffer::open) {
					// File was opened synchronously
					RestoreState(buffers.buffers[iBuffer], true);
					DisplayAround(buffers.buffers[iBuffer]);
					wEditor.Call(SCI_SCROLLCARET);
				}
			}
		}
	}
	return opened;
}

void SciTEBase::AddFileToStack(const FilePath &file, SelectedRange selection, int scrollPos) {
	if (!file.IsSet())
		return;
	DeleteFileStackMenu();
	// Only stack non-empty names
	if (file.IsSet() && !file.IsUntitled()) {
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

void SciTEBase::RemoveFileFromStack(const FilePath &file) {
	if (!file.IsSet())
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
	rf.selection = GetSelectedRange();
	rf.scrollPosition = GetCurrentScrollPosition();
	return rf;
}

void SciTEBase::DisplayAround(const RecentFile &rf) {
	if ((rf.selection.position != INVALID_POSITION) && (rf.selection.anchor != INVALID_POSITION)) {
		SetSelection(rf.selection.anchor, rf.selection.position);

		int curTop = wEditor.Call(SCI_GETFIRSTVISIBLELINE);
		int lineTop = wEditor.Call(SCI_VISIBLEFROMDOCLINE, rf.scrollPosition);
		wEditor.Call(SCI_LINESCROLL, 0, lineTop - curTop);
		wEditor.Call(SCI_CHOOSECARETX, 0, 0);
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
	if (CanMakeRoom(true)) {
		if (pos >= 0) {
			if ((pos == 0) && (!recentFileStack[pos].IsSet())) {	// Empty
				New();
				SetWindowName();
				ReadProperties();
				SetIndentSettings();
				SetEol();
			} else if (recentFileStack[pos].IsSet()) {
				RecentFile rf = recentFileStack[pos];
				// Already asked user so don't allow Open to ask again.
				Open(rf, ofNoSaveIfDirty);
				CurrentBuffer()->scrollPosition = rf.scrollPosition;
				CurrentBuffer()->selection = rf.selection;
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
	GUI::gui_string localised = localiser.Text(text);
	SetMenuItem(menuNumber, position, itemID, localised.c_str(), GUI::StringFromUTF8(mnemonic).c_str());
}

bool SciTEBase::ToolIsImmediate(int item) {
	std::string itemSuffix = StdStringFromInteger(item);
	itemSuffix += '.';

	std::string propName = "command.";
	propName += itemSuffix;

	std::string command = props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str());
	if (command.length()) {
		JobMode jobMode(props, item, FileNameExt().AsUTF8().c_str());
		return jobMode.jobType == jobImmediate;
	}
	return false;
}

void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	int menuPos = TOOLS_START;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		std::string prefix = "command.name.";
		prefix += StdStringFromInteger(item);
		prefix += ".";
		std::string commandName = props.GetNewExpandString(prefix.c_str(), FileNameExt().AsUTF8().c_str());
		if (commandName.length()) {
			std::string sMenuItem = commandName;
			prefix = "command.shortcut.";
			prefix += StdStringFromInteger(item);
			prefix += ".";
			std::string sMnemonic = props.GetNewExpandString(prefix.c_str(), FileNameExt().AsUTF8().c_str());
			if (item < 10 && sMnemonic.length() == 0) {
				sMnemonic += "Ctrl+";
				sMnemonic += StdStringFromInteger(item);
			}
			SetMenuItemLocalised(menuTools, menuPos, itemID, sMenuItem.c_str(),
				sMnemonic.length() ? sMnemonic.c_str() : NULL);
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
		SetMenuItem(menuTools, menuPos++, IDM_MACRO_SEP, GUI_TEXT(""));
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROLIST,
		        "&List Macros...", "Shift+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROPLAY,
		        "Run Current &Macro", "F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACRORECORD,
		        "&Record Macro", "Ctrl+F9");
		SetMenuItemLocalised(menuTools, menuPos, IDM_MACROSTOPRECORD,
		        "S&top Recording Macro", "Ctrl+Shift+F9");
	}
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd) {
	std::string subsystem = props.GetNewExpandString(cmd, FileNameExt().AsUTF8().c_str());
	return subsystem.empty() ? jobCLI : SubsystemFromChar(subsystem.at(0));
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	const std::string itemSuffix = StdStringFromInteger(item) + ".";
	const std::string propName = std::string("command.") + itemSuffix;
	std::string command(props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).c_str());
	if (command.length()) {
		JobMode jobMode(props, item, FileNameExt().AsUTF8().c_str());
		if (jobQueue.IsExecuting() && (jobMode.jobType != jobImmediate))
			// Busy running a tool and running a second can cause failures.
			return;
		if (jobMode.saveBefore == 2 || (jobMode.saveBefore == 1 && (!(CurrentBuffer()->isDirty) || Save())) || SaveIfUnsure() != saveCancelled) {
			if (jobMode.isFilter)
				CurrentBuffer()->fileModTime -= 1;
			if (jobMode.jobType == jobImmediate) {
				if (extender) {
					extender->OnExecute(command.c_str());
				}
			} else {
				AddCommand(command.c_str(), "", jobMode.jobType, jobMode.input, jobMode.flags);
				if (jobQueue.HasCommandToRun())
					Execute();
			}
		}
	}
}

inline bool isdigitchar(int ch) {
	return (ch >= '0') && (ch <= '9');
}

static int DecodeMessage(const char *cdoc, std::string &sourcePath, int format, int &column) {
	sourcePath.clear();
	column = -1; // default to not detected
	switch (format) {
	case SCE_ERR_PYTHON: {
			// Python
			const char *startPath = strchr(cdoc, '\"');
			if (startPath) {
				startPath++;
				const char *endPath = strchr(startPath, '\"');
				if (endPath) {
					ptrdiff_t length = endPath - startPath;
					sourcePath.assign(startPath, length);
					endPath++;
					while (*endPath && !isdigitchar(*endPath)) {
						endPath++;
					}
					int sourceNumber = atoi(endPath) - 1;
					return sourceNumber;
				}
			}
			break;
		}
	case SCE_ERR_GCC:
	case SCE_ERR_GCC_INCLUDED_FROM: {
			// GCC - look for number after colon to be line number
			// This will be preceded by file name.
			// Lua debug traceback messages also piggyback this style, but begin with a tab.
			// GCC include paths are similar but start with either "In file included from " or
			// "                 from "
			if (format == SCE_ERR_GCC_INCLUDED_FROM) {
				cdoc += strlen("In file included from ");
			}
			if (cdoc[0] == '\t')
				++cdoc;
			for (int i = 0; cdoc[i]; i++) {
				if (cdoc[i] == ':' && isdigitchar(cdoc[i + 1])) {
					const int sourceLine = atoi(cdoc + i + 1);
					sourcePath.assign(cdoc, i);
					i += 2;
					while (isdigitchar(cdoc[i]))
						++i;
					if (cdoc[i] == ':' && isdigitchar(cdoc[i + 1]))
						column = atoi(cdoc + i + 1) - 1;
					// Some tools show whole file errors as occurring at line 0
					return (sourceLine > 0) ? sourceLine - 1 : 0;
				}
			}
			break;
		}
	case SCE_ERR_MS: {
			// Visual *
			const char *start = cdoc;
			while (isspacechar(*start)) {
				start++;
			}
			const char *endPath = strchr(start, '(');
			if (endPath) {
				if (!isdigitchar(endPath[1])) {
					// This handles the common case of include files in the C:\Program Files (x86)\ directory
					endPath = strchr(endPath + 1, '(');
				}
				if (endPath) {
					ptrdiff_t length = endPath - start;
					sourcePath.assign(start, length);
					endPath++;
					return atoi(endPath) - 1;
				}
			}
			break;
		}
	case SCE_ERR_BORLAND: {
			// Borland
			const char *space = strchr(cdoc, ' ');
			if (space) {
				while (isspacechar(*space)) {
					space++;
				}
				while (*space && !isspacechar(*space)) {
					space++;
				}
				while (isspacechar(*space)) {
					space++;
				}

				const char *space2 = NULL;

				if (strlen(space) > 2) {
					space2 = strchr(space + 2, ':');
				}

				if (space2) {
					while (!isspacechar(*space2)) {
						space2--;
					}

					while (isspacechar(*(space2 - 1))) {
						space2--;
					}

					ptrdiff_t length = space2 - space;

					if (length > 0) {
						sourcePath.assign(space, length);
						return atoi(space2) - 1;
					}
				}
			}
			break;
		}
	case SCE_ERR_PERL: {
			// perl
			const char *at = strstr(cdoc, " at ");
			const char *line = strstr(cdoc, " line ");
			ptrdiff_t length = line - (at + 4);
			if (at && line && length > 0) {
				sourcePath.assign(at + 4, length);
				line += 6;
				return atoi(line) - 1;
			}
			break;
		}
	case SCE_ERR_NET: {
			// .NET traceback
			const char *in = strstr(cdoc, " in ");
			const char *line = strstr(cdoc, ":line ");
			if (in && line && (line > in)) {
				in += 4;
				sourcePath.assign(in, line - in);
				line += 6;
				return atoi(line) - 1;
			}
			break;
		}
	case SCE_ERR_LUA: {
			// Lua 4 error looks like: last token read: `result' at line 40 in file `Test.lua'
			const char *idLine = "at line ";
			const char *idFile = "file ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file) {
				const char *fileStart = file + lenFile + 1;
				const char *quote = strstr(fileStart, "'");
				size_t length = quote - fileStart;
				if (quote && length > 0) {
					sourcePath.assign(fileStart, length);
				}
				line += lenLine;
				return atoi(line) - 1;
			} else {
				// Lua 5.1 error looks like: lua.exe: test1.lua:3: syntax error
				// reuse the GCC error parsing code above!
				const char* colon = strstr(cdoc, ": ");
				if (colon)
					return DecodeMessage(colon + 2, sourcePath, SCE_ERR_GCC, column);
			}
			break;
		}

	case SCE_ERR_CTAG: {
			for (int i = 0; cdoc[i]; i++) {
				if ((isdigitchar(cdoc[i + 1]) || (cdoc[i + 1] == '/' && cdoc[i + 2] == '^')) && cdoc[i] == '\t') {
					int j = i - 1;
					while (j > 0 && ! strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j--;
					}
					if (strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j++;
					}
					sourcePath.assign(&cdoc[j], i - j);
					// Because usually the address is a searchPattern, lineNumber has to be evaluated later
					return 0;
				}
			}
			break;
		}
	case SCE_ERR_PHP: {
			// PHP error look like: Fatal error: Call to undefined function:  foo() in example.php on line 11
			const char *idLine = " on line ";
			const char *idFile = " in ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				sourcePath.assign(file, length);
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_ELF: {
			// Essential Lahey Fortran error look like: Line 11, file c:\fortran90\codigo\demo.f90
			const char *line = strchr(cdoc, ' ');
			if (line) {
				while (isspacechar(*line)) {
					line++;
				}
				const char *file = strchr(line, ' ');
				if (file) {
					while (isspacechar(*file)) {
						file++;
					}
					while (*file && !isspacechar(*file)) {
						file++;
					}
					size_t length = strlen(file);
					sourcePath.assign(file, length);
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_IFC: {
			/* Intel Fortran Compiler error/warnings look like:
			 * Error 71 at (17:teste.f90) : The program unit has no name
			 * Warning 4 at (9:modteste.f90) : Tab characters are an extension to standard Fortran 95
			 *
			 * Depending on the option, the error/warning messages can also appear on the form:
			 * modteste.f90(9): Warning 4 : Tab characters are an extension to standard Fortran 95
			 *
			 * These are trapped by the MS handler, and are identified OK, so no problem...
			 */
			const char *line = strchr(cdoc, '(');
			if (line) {
				const char *file = strchr(line, ':');
				if (file) {
					file++;
					const char *endfile = strchr(file, ')');
					size_t length = endfile - file;
					sourcePath.assign(file, length);
					line++;
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_ABSF: {
			// Absoft Pro Fortran 90/95 v8.x, v9.x  errors look like: cf90-113 f90fe: ERROR SHF3D, File = shf.f90, Line = 1101, Column = 19
			const char *idFile = " File = ";
			const char *idLine = ", Line = ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			//const char *idColumn = ", Column = ";
			//const char *column = strstr(cdoc, idColumn);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				sourcePath.assign(file, length);
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_IFORT: {
			/* Intel Fortran Compiler v8.x error/warnings look like:
			 * fortcom: Error: shf.f90, line 5602: This name does not have ...
				 */
			const char *idFile = ": Error: ";
			const char *idLine = ", line ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			const char *lineend = strrchr(cdoc, ':');
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				sourcePath.assign(file, length);
				line += lenLine;
				if ((lineend > line)) {
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_TIDY: {
			/* HTML Tidy error/warnings look like:
			 * line 8 column 1 - Error: unexpected </head> in <meta>
			 * line 41 column 1 - Warning: <table> lacks "summary" attribute
			 */
			const char *line = strchr(cdoc, ' ');
			if (line) {
				const char *col = strchr(line + 1, ' ');
				if (col) {
					//*col = '\0';
					int lnr = atoi(line) - 1;
					col = strchr(col + 1, ' ');
					if (col) {
						const char *endcol = strchr(col + 1, ' ');
						if (endcol) {
							//*endcol = '\0';
							column = atoi(col) - 1;
							return lnr;
						}
					}
				}
			}
			break;
		}

	case SCE_ERR_JAVA_STACK: {
			/* Java runtime stack trace
				\tat <methodname>(<filename>:<line>)
				 */
			const char *startPath = strrchr(cdoc, '(') + 1;
			const char *endPath = strchr(startPath, ':');
			ptrdiff_t length = endPath - startPath;
			if (length > 0) {
				sourcePath.assign(startPath, length);
				int sourceNumber = atoi(endPath + 1) - 1;
				return sourceNumber;
			}
			break;
		}

	case SCE_ERR_DIFF_MESSAGE: {
			// Diff file header, either +++ <filename> or --- <filename>, may be followed by \t
			// Often followed by a position line @@ <linenumber>
			const char *startPath = cdoc + 4;
			const char *endPath = strpbrk(startPath, "\t\r\n");
			if (endPath) {
				ptrdiff_t length = endPath - startPath;
				sourcePath.assign(startPath, length);
				return 0;
			}
			break;
		}
	}	// switch
	return -1;
}

#define CSI "\033["

static bool SeqEnd(int ch) {
	return (ch == 0) || ((ch >= '@') && (ch <= '~'));
}

static void RemoveEscSeq(std::string &s) {
	size_t csi = s.find(CSI);
	while (csi != std::string::npos) {
		size_t endSeq = csi + 2;
		while (endSeq < s.length() && !SeqEnd(s.at(endSeq)))
			endSeq++;
		s.erase(csi, endSeq-csi+1);
		csi = s.find(CSI);
	}
}

// Remove up to and including ch
static void Chomp(std::string &s, int ch) {
	const size_t posCh = s.find(static_cast<char>(ch));
	if (posCh != std::string::npos)
		s.erase(0, posCh + 1);
}

void SciTEBase::ShowMessages(int line) {
	wEditor.Call(SCI_ANNOTATIONSETSTYLEOFFSET, diagnosticStyleStart);
	wEditor.Call(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_BOXED);
	wEditor.Call(SCI_ANNOTATIONCLEARALL);
	TextReader acc(wOutput);
	while ((line > 0) && (acc.StyleAt(acc.LineStart(line-1)) != SCE_ERR_CMD))
		line--;
	int maxLine = wOutput.Call(SCI_GETLINECOUNT);
	while ((line < maxLine) && (acc.StyleAt(acc.LineStart(line)) != SCE_ERR_CMD)) {
		int startPosLine = wOutput.Call(SCI_POSITIONFROMLINE, line, 0);
		int lineEnd = wOutput.Call(SCI_GETLINEENDPOSITION, line, 0);
		std::string message = GetRangeString(wOutput, startPosLine, lineEnd);
		std::string source;
		int column;
		int style = acc.StyleAt(startPosLine);
		if ((style == SCE_ERR_ESCSEQ) || (style == SCE_ERR_ESCSEQ_UNKNOWN) || (style >= SCE_ERR_ES_BLACK)) {
			// GCC message with ANSI escape sequences
			RemoveEscSeq(message);
			style = SCE_ERR_GCC;
		}
		int sourceLine = DecodeMessage(message.c_str(), source, style, column);
		Chomp(message, ':');
		if (style == SCE_ERR_GCC) {
			Chomp(message, ':');
		}
		GUI::gui_string sourceString = GUI::StringFromUTF8(source);
		FilePath sourcePath = FilePath(sourceString).NormalizePath();
		if (filePath.Name().SameNameAs(sourcePath.Name())) {
			if (style == SCE_ERR_GCC) {
				const char *sColon = strchr(message.c_str(), ':');
				if (sColon) {
					std::string editLine = GetLine(wEditor, sourceLine);
					if (editLine == (sColon+1)) {
						line++;
						continue;
					}
				}
			}
			int lenCurrent = wEditor.CallString(SCI_ANNOTATIONGETTEXT, sourceLine, NULL);
			std::string msgCurrent(lenCurrent, '\0');
			std::string stylesCurrent(lenCurrent, '\0');
			if (lenCurrent) {
				wEditor.CallString(SCI_ANNOTATIONGETTEXT, sourceLine, &msgCurrent[0]);
				wEditor.CallString(SCI_ANNOTATIONGETSTYLES, sourceLine, &stylesCurrent[0]);
				msgCurrent += "\n";
				stylesCurrent += '\0';
			}
			if (msgCurrent.find(message.c_str()) == std::string::npos) {
				// Only append unique messages
				msgCurrent += message.c_str();
				int msgStyle = 0;
				if (message.find("warning") != std::string::npos)
					msgStyle = 1;
				if (message.find("error") != std::string::npos)
					msgStyle = 2;
				if (message.find("fatal") != std::string::npos)
					msgStyle = 3;
				stylesCurrent += std::string(message.length(), static_cast<char>(msgStyle));
				wEditor.CallString(SCI_ANNOTATIONSETTEXT, sourceLine, msgCurrent.c_str());
				wEditor.CallString(SCI_ANNOTATIONSETSTYLES, sourceLine, stylesCurrent.c_str());
			}
		}
		line++;
	}
}

void SciTEBase::GoMessage(int dir) {
	Sci_CharacterRange crange;
	crange.cpMin = wOutput.Call(SCI_GETSELECTIONSTART);
	crange.cpMax = wOutput.Call(SCI_GETSELECTIONEND);
	long selStart = static_cast<long>(crange.cpMin);
	int curLine = wOutput.Call(SCI_LINEFROMPOSITION, selStart);
	int maxLine = wOutput.Call(SCI_GETLINECOUNT);
	int lookLine = curLine + dir;
	if (lookLine < 0)
		lookLine = maxLine - 1;
	else if (lookLine >= maxLine)
		lookLine = 0;
	TextReader acc(wOutput);
	while ((dir == 0) || (lookLine != curLine)) {
		int startPosLine = wOutput.Call(SCI_POSITIONFROMLINE, lookLine, 0);
		int lineLength = wOutput.Call(SCI_LINELENGTH, lookLine, 0);
		int style = acc.StyleAt(startPosLine);
		if (style != SCE_ERR_DEFAULT &&
		        style != SCE_ERR_CMD &&
		        style != SCE_ERR_DIFF_ADDITION &&
		        style != SCE_ERR_DIFF_CHANGED &&
		        style != SCE_ERR_DIFF_DELETION) {
			wOutput.Call(SCI_MARKERDELETEALL, static_cast<uptr_t>(-1));
			wOutput.Call(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			wOutput.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
			        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
			wOutput.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
			        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
			wOutput.Call(SCI_MARKERADD, lookLine, 0);
			wOutput.Call(SCI_SETSEL, startPosLine, startPosLine);
			std::string message = GetRangeString(wOutput, startPosLine, startPosLine + lineLength);
			if ((style == SCE_ERR_ESCSEQ) || (style == SCE_ERR_ESCSEQ_UNKNOWN) || (style >= SCE_ERR_ES_BLACK)) {
				// GCC message with ANSI escape sequences
				RemoveEscSeq(message);
				style = SCE_ERR_GCC;
			}
			std::string source;
			int column;
			long sourceLine = DecodeMessage(message.c_str(), source, style, column);
			if (sourceLine >= 0) {
				GUI::gui_string sourceString = GUI::StringFromUTF8(source);
				FilePath sourcePath = FilePath(sourceString).NormalizePath();
				if (!filePath.Name().SameNameAs(sourcePath)) {
					FilePath messagePath;
					bool bExists = false;
					if (Exists(dirNameAtExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(dirNameForExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(filePath.Directory().AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(NULL, sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else {
						// Look through buffers for name match
						for (int i = buffers.lengthVisible - 1; i >= 0; i--) {
							if (sourcePath.Name().SameNameAs(buffers.buffers[i].Name())) {
								messagePath = buffers.buffers[i];
								bExists = true;
							}
						}
					}
					if (bExists) {
						if (!Open(messagePath, ofSynchronous)) {
							return;
						}
						CheckReload();
					}
				}

				// If ctag then get line number after search tag or use ctag line number
				if (style == SCE_ERR_CTAG) {
					//without following focus GetCTag wouldn't work correct
					WindowSetFocus(wOutput);
					std::string cTag = GetCTag();
					if (cTag.length() != 0) {
						if (atoi(cTag.c_str()) > 0) {
							//if tag is linenumber, get line
							sourceLine = atoi(cTag.c_str()) - 1;
						} else {
							findWhat = cTag;
							FindNext(false);
							//get linenumber for marker from found position
							sourceLine = wEditor.Call(SCI_LINEFROMPOSITION, wEditor.Call(SCI_GETCURRENTPOS));
						}
					}
				}

				else if (style == SCE_ERR_DIFF_MESSAGE) {
					const bool isAdd = message.find("+++ ") == 0;
					const int atLine = lookLine + (isAdd ? 1 : 2); // lines are in this order: ---, +++, @@
					std::string atMessage = GetLine(wOutput, atLine);
					if (StartsWith(atMessage, "@@ -")) {
						size_t atPos = 4; // deleted position starts right after "@@ -"
						if (isAdd) {
							const size_t linePlace = atMessage.find(" +", 7);
							if (linePlace != std::string::npos)
								atPos = linePlace + 2; // skip "@@ -1,1" and then " +"
						}
						sourceLine = atol(atMessage.c_str() + atPos) - 1;
					}
				}

				if (props.GetInt("error.inline")) {
					ShowMessages(lookLine);
				}

				wEditor.Call(SCI_MARKERDELETEALL, 0);
				wEditor.Call(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				wEditor.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
				        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
				wEditor.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
				        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
				wEditor.Call(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine, 0);
				int endSourceline = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine + 1, 0);
				if (column >= 0) {
					// Get the position in line according to current tab setting
					startSourceLine = wEditor.Call(SCI_FINDCOLUMN, sourceLine, column);
				}
				EnsureRangeVisible(wEditor, startSourceLine, startSourceLine);
				if (props.GetInt("error.select.line") == 1) {
					//select whole source source line from column with error
					SetSelection(endSourceline, startSourceLine);
				} else {
					//simply move cursor to line, don't do any selection
					SetSelection(startSourceLine, startSourceLine);
				}
				std::replace(message.begin(), message.end(), '\t', ' ');
				::Remove(message, std::string("\n"));
				props.Set("CurrentMessage", message.c_str());
				UpdateStatusBar(false);
				WindowSetFocus(wEditor);
			}
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

