// SciTE - Scintilla based Text Editor
// DirectorExtension.cxx - Extension for communicating with a director program.
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>

#include "Platform.h"

#include "Scintilla.h"
#include "Accessor.h"
#include "Extender.h"
#include "DirectorExtension.h"
#include "SciTEDirector.h"

static ExtensionAPI *host = 0;
static HWND wDirector = 0;
static HWND wReceiver = 0;

static void SendFiler(int typ, const char *path) {
	//typ values :
	//-1: Bring Filerx to foreground (no message is sent , calls SetForegroundWindow)
	//SCPROJ_OPENED=2000 : ShowPath
	//SCPROJ_SAVED=2001 : Refresh
	HWND dest = wDirector; 
	if (dest == 0) { //should disappear one day..
		dest = ::FindWindowEx(NULL,NULL,(const char *)32770L,"Filer"); 
		wDirector = dest; //defined in SciTEWin.h
	}
	if (dest != 0) {
		if (typ == -1) {
			SetForegroundWindow(dest);
		} else {
			COPYDATASTRUCT cds;
			cds.dwData = typ;
			cds.cbData = strlen(path);
			cds.lpData = reinterpret_cast<void *>(
				const_cast<char *>(path));
			::SendMessage(dest, WM_COPYDATA,
				reinterpret_cast<WPARAM>(wReceiver),
				reinterpret_cast<LPARAM>(&cds));
		}
	}
}

static void CheckEnvironment(ExtensionAPI *host) {
	if (!host)
		return;
	if (!wDirector) {
		char *director = host->Property("director.hwnd");
		if (director)
			wDirector = reinterpret_cast<HWND>(atoi(director));
	}
	if (!wReceiver) {
		char *receiver = host->Property("WindowID");
		if (receiver)
			wReceiver = reinterpret_cast<HWND>(atoi(receiver));
	}
}

DirectorExtension::DirectorExtension() {
}

DirectorExtension::~DirectorExtension() {}

bool DirectorExtension::Initialise(ExtensionAPI *host_) {
	host = host_;
	CheckEnvironment(host);
	return true;
}

bool DirectorExtension::Finalise() {
	return true;
}

bool DirectorExtension::Clear() {
	return true;
}

bool DirectorExtension::Load(const char *) {
	return true;
}

bool DirectorExtension::OnOpen(const char *path) {
	CheckEnvironment(host);
	if (*path)
		::SendFiler(SCD_OPENED, path);	// ShowPath 
	return true;
}

bool DirectorExtension::OnSave(const char *path) {
	CheckEnvironment(host);
	if (*path)
		::SendFiler(SCD_SAVED, path);	// Refresh 
	return true;
}

bool DirectorExtension::OnChar(char) {
	return true;
}

bool DirectorExtension::OnExecute(const char *) {
	return true;
}

bool DirectorExtension::OnSavePointReached() {
	return false;
}

bool DirectorExtension::OnSavePointLeft() {
	return false;
}

bool DirectorExtension::OnStyle(unsigned int, int, int, Accessor *) {
	return true;
}

// These should probably have arguments

bool DirectorExtension::OnDoubleClick() {
	return true;
}

bool DirectorExtension::OnUpdateUI() {
	return true;
}

bool DirectorExtension::OnMarginClick() {
	return true;
}

void DirectorExtension::HandleMessage(WPARAM wParam, LPARAM lParam) {
	COPYDATASTRUCT *pcds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
	char path[MAX_PATH];
	unsigned int nCopy = ((MAX_PATH-1) < pcds->cbData) ? (MAX_PATH-1) : pcds->cbData;
	if (pcds->lpData)
		strncpy(path, reinterpret_cast<char *>(pcds->lpData), nCopy);
	path[nCopy] = '\0';
	switch(pcds->dwData) {
		case SCD_IDENTIFY:
			wDirector = reinterpret_cast<HWND>(wParam);
			//caller's Window Name is in path ...
			break;
		case SCD_OPEN:
			host->OpenFromExtension(path);
			break;
		case SCD_CLOSING:
			wDirector = 0;
			break;
	}
}

#ifdef _MSC_VER
// Unreferenced inline functions are OK
#pragma warning(disable: 4514)
#endif 
