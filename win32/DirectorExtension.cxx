// SciTE - Scintilla based Text Editor
// DirectorExtension.cxx - Extension for communicating with a director program.
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <stdio.h>

#include "Platform.h"

#include "PropSet.h"

#include "Scintilla.h"
#include "Accessor.h"
#include "Extender.h"
#include "DirectorExtension.h"
#include "SciTEDirector.h"

static ExtensionAPI *host = 0;
static DirectorExtension *pde = 0;
static HWND wDirector = 0;
static HWND wReceiver = 0;
static bool startedByDirector = false;

//#define INT_BASED_VERBS

static void SendDirector(int typ, const char *path) {
	if (wDirector != 0) {
		COPYDATASTRUCT cds;
		cds.dwData = typ;
		cds.cbData = strlen(path);
		cds.lpData = reinterpret_cast<void *>(
			const_cast<char *>(path));
		::SendMessage(wDirector, WM_COPYDATA,
			reinterpret_cast<WPARAM>(wReceiver),
			reinterpret_cast<LPARAM>(&cds));
	}
}

#ifndef INT_BASED_VERBS
static void SendDirector(const char *verb, const char *arg=0) {
	SString message(verb);
	message += ":";
	if (arg)
		message += arg;
	::SendDirector(0, message.c_str());
}

static void SendDirector(const char *verb, sptr_t arg) {
	SString s(arg);
	::SendDirector(verb, s.c_str());
}
#endif

static void CheckEnvironment(ExtensionAPI *host) {
	if (!host)
		return;
	if (!wDirector) {
		char *director = host->Property("director.hwnd");
        	if (director && *director) {
			startedByDirector = true;
			wDirector = reinterpret_cast<HWND>(atoi(director));
			// Director is just seen so identify this to it
#ifdef INT_BASED_VERBS
			::SendDirector(SCD_IDENTIFY, "SciTE");
#else
			::SendDirector("identity", reinterpret_cast<sptr_t>(wReceiver));
#endif
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
#ifdef INT_BASED_VERBS
	::SendDirector(SCD_CLOSING, "");
#else
	::SendDirector("closing");
#endif
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
	if (*path) {
#ifdef INT_BASED_VERBS
		::SendDirector(SCD_OPENED, path);
#else
		::SendDirector("opened", path);
#endif
	}
	return true;
}

bool DirectorExtension::OnSwitchFile(const char *path) { 
	CheckEnvironment(host);
	if (*path) {
#ifdef INT_BASED_VERBS
		::SendDirector(SCD_SWITCHED, path);
#else
		::SendDirector("switched", path);
#endif
	}
	return true; 
};

bool DirectorExtension::OnSave(const char *path) {
	CheckEnvironment(host);
	if (*path) {
#ifdef INT_BASED_VERBS
		::SendDirector(SCD_SAVED, path);
#else
		::SendDirector("saved", path);
#endif
	}
	return true;
}

bool DirectorExtension::OnChar(char) {
	return false;
}

bool DirectorExtension::OnExecute(const char *) {
	return false;
}

bool DirectorExtension::OnSavePointReached() {
	return false;
}

bool DirectorExtension::OnSavePointLeft() {
	return false;
}

bool DirectorExtension::OnStyle(unsigned int, int, int, Accessor *) {
	return false;
}

// These should probably have arguments

bool DirectorExtension::OnDoubleClick() {
	return false;
}

bool DirectorExtension::OnUpdateUI() {
	return false;
}

bool DirectorExtension::OnMarginClick() {
	return false;
}

void DirectorExtension::HandleStringMessage(const char *message) {
	const char *arg = strchr(message, ':');
	if (arg)
		arg++;
	if (isprefix(message, "identity:")) {
		wDirector = reinterpret_cast<HWND>(atoi(arg));
	} else if (isprefix(message, "closing:")) {
		wDirector = 0;
		if (startedByDirector)
			host->ShutDown();
	} else if (host) {
		host->Perform(message);
	}
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
			if (startedByDirector)
				host->ShutDown();
			break;
		case 0:
			HandleStringMessage(path);
			break;
	}
	return 0;
}

#ifdef _MSC_VER
// Unreferenced inline functions are OK
#pragma warning(disable: 4514)
#endif 
