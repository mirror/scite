// SciTE - Scintilla based Text Editor
// DirectorExtension.cxx - Extension for communicating with a director program.
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <stdio.h>

#include "Platform.h"

#include "Scintilla.h"
#include "Accessor.h"
#include "Extender.h"
#include "DirectorExtension.h"
#include "SciTEDirector.h"

static ExtensionAPI *host = 0;
static DirectorExtension *pde = 0;
static HWND wDirector = 0;
static HWND wReceiver = 0;

static void SendDirector(int typ, const char *path) {
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
        if (director) {
			wDirector = reinterpret_cast<HWND>(atoi(director));
            // Director is just seen so identify this to it
            SendDirector(SCD_IDENTIFY, "SciTE");
        }
        delete []director;
	}
	char number[32];
	sprintf(number, "%0d", reinterpret_cast<int>(wReceiver));
	host->SetProperty("WindowID", number);
}

static char DirectorExtension_ClassName[] = "DirectorExtension";

LRESULT PASCAL DirectorExtension_WndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	if (pde && (iMessage == WM_COPYDATA)) {
		return pde->HandleMessage(wParam, lParam);
	}
	return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
}

static void DirectorExtension_Register(HINSTANCE hInstance) {
	WNDCLASS wndclass;
	wndclass.style = 0;
	wndclass.lpfnWndProc = DirectorExtension_WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = 0;
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName = 0;
	wndclass.lpszClassName = DirectorExtension_ClassName;
	if (!::RegisterClass(&wndclass))
		::exit(FALSE);
}

DirectorExtension::DirectorExtension() {
    pde = this;
}

DirectorExtension::~DirectorExtension() {
    pde = 0;
}

bool DirectorExtension::Initialise(ExtensionAPI *host_) {
	host = host_;
	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(
		host->GetInstance());
	DirectorExtension_Register(hInstance);
	wReceiver = ::CreateWindow(
		DirectorExtension_ClassName, 
		DirectorExtension_ClassName,
		0,
		0,0,0,0,
		0,
		0,
		hInstance,
		0);
	if (!wReceiver)
		::exit(FALSE);
	CheckEnvironment(host);
	return true;
}

bool DirectorExtension::Finalise() {
    SendDirector(SCD_CLOSING, "");
	if (wReceiver)
		::DestroyWindow(wReceiver);
	wReceiver = 0;
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
		::SendDirector(SCD_OPENED, path);	// ShowPath 
	return true;
}

bool DirectorExtension::OnSwitchFile(const char *path) { 
	CheckEnvironment(host);
	if (*path)
		::SendDirector(SCD_OPENED, path);	// ShowPath 
	return true; 
};

bool DirectorExtension::OnSave(const char *path) {
	CheckEnvironment(host);
	if (*path)
		::SendDirector(SCD_SAVED, path);	// Refresh 
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
	return false;
}

LRESULT DirectorExtension::HandleMessage(WPARAM wParam, LPARAM lParam) {
	COPYDATASTRUCT *pcds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
	char path[MAX_PATH];
	unsigned int nCopy = ((MAX_PATH-1) < pcds->cbData) ? (MAX_PATH-1) : pcds->cbData;
	if (pcds->lpData)
		strncpy(path, reinterpret_cast<char *>(pcds->lpData), nCopy);
	path[nCopy] = '\0';
	switch (pcds->dwData) {
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
	return 0;
}

#ifdef _MSC_VER
// Unreferenced inline functions are OK
#pragma warning(disable: 4514)
#endif 
