// SciTE - Scintilla based Text Editor
/** @file DirectorExtension.cxx
 ** Extension for communicating with a director program.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>
#include <set>
#include <map>

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
#include <windows.h>
#include <commctrl.h>

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"
#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "StyleDefinition.h"
#include "PropSetFile.h"
#include "Extender.h"
#include "DirectorExtension.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"

static HWND wDirector = 0;
static HWND wCorrespondent = 0;
static HWND wReceiver = 0;
static bool startedByDirector = false;
static bool shuttingDown = false;
unsigned int SDI = 0;

static bool HasConnection() {
	return (wDirector != 0) || (wCorrespondent != 0);
}

static void SendDirector(const char *verb, const char *arg = 0) {
	if (HasConnection()) {
		HWND wDestination = wCorrespondent;
		std::string addressedMessage;
		if (wDestination) {
			addressedMessage += ":";
			std::string address = StdStringFromSizeT(reinterpret_cast<size_t>(wDestination));
			addressedMessage += address;
			addressedMessage += ":";
		} else {
			wDestination = wDirector;
		}
		addressedMessage += verb;
		addressedMessage += ":";
		if (arg)
			addressedMessage += arg;
		char *slashedMessage = Slash(addressedMessage.c_str(), false);
		if (slashedMessage) {
			COPYDATASTRUCT cds;
			cds.dwData = 0;
			cds.cbData = static_cast<int>(strlen(slashedMessage));
			cds.lpData = reinterpret_cast<void *>(
			                 const_cast<char *>(slashedMessage));
			::SendMessage(wDestination, WM_COPYDATA,
			              reinterpret_cast<WPARAM>(wReceiver),
			              reinterpret_cast<LPARAM>(&cds));
			delete []slashedMessage;
		}
	}
}

static void SendDirector(const char *verb, sptr_t arg) {
	std::string s = StdStringFromSizeT(static_cast<size_t>(arg));
	::SendDirector(verb, s.c_str());
}

static void CheckEnvironment(ExtensionAPI *phost) {
	if (phost && !shuttingDown) {
		if (!wDirector) {
			std::string director = phost->Property("director.hwnd");
			if (director.length() > 0) {
				startedByDirector = true;
				wDirector = reinterpret_cast<HWND>(atoi(director.c_str()));
				// Director is just seen so identify this to it
				::SendDirector("identity", reinterpret_cast<sptr_t>(wReceiver));
			}
		}
		std::string sReceiver = StdStringFromSizeT(reinterpret_cast<size_t>(wReceiver));
		phost->SetProperty("WindowID", sReceiver.c_str());
	}
}

static TCHAR DirectorExtension_ClassName[] = TEXT("DirectorExtension");

static LRESULT HandleCopyData(LPARAM lParam) {
	COPYDATASTRUCT *pcds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
	// Copy into an temporary buffer to ensure \0 terminated
	if (pcds->lpData) {
		char *dataCopy = new char[pcds->cbData + 1];
		strncpy(dataCopy, reinterpret_cast<char *>(pcds->lpData), pcds->cbData);
		dataCopy[pcds->cbData] = '\0';
		DirectorExtension::Instance().HandleStringMessage(dataCopy);
		delete []dataCopy;
	}
	return 0;
}

LRESULT PASCAL DirectorExtension_WndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	if (iMessage == WM_COPYDATA) {
		return HandleCopyData(lParam);
	} else if (iMessage == SDI) {
		return SDI;
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

DirectorExtension &DirectorExtension::Instance() {
	static DirectorExtension singleton;
	return singleton;
}

bool DirectorExtension::Initialise(ExtensionAPI *host_) {
	host = host_;
	SDI = ::RegisterWindowMessage(TEXT("SciTEDirectorInterface"));
	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(
	                          host->GetInstance());
	DirectorExtension_Register(hInstance);
	wReceiver = ::CreateWindow(
	                DirectorExtension_ClassName,
	                DirectorExtension_ClassName,
	                0,
	                0, 0, 0, 0,
	                0,
	                0,
	                hInstance,
	                0);
	if (!wReceiver)
		::exit(FALSE);
	// Make the frame window handle available so the director can activate it.
	::SetWindowLongPtr(wReceiver, GWLP_USERDATA,
		reinterpret_cast<LONG_PTR>((static_cast<SciTEBase*>(host))->GetID()));
	CheckEnvironment(host);
	return true;
}

bool DirectorExtension::Finalise() {
	::SendDirector("closing");
	if (wReceiver)
		::DestroyWindow(wReceiver);
	wReceiver = 0;
	return true;
}

bool DirectorExtension::Clear() {
	return false;
}

bool DirectorExtension::Load(const char *) {
	return false;
}

bool DirectorExtension::OnOpen(const char *path) {
	CheckEnvironment(host);
	if (*path) {
		::SendDirector("opened", path);
	}
	return false;
}

bool DirectorExtension::OnSwitchFile(const char *path) {
	CheckEnvironment(host);
	if (*path) {
		::SendDirector("switched", path);
	}
	return false;
}

bool DirectorExtension::OnSave(const char *path) {
	CheckEnvironment(host);
	if (*path) {
		::SendDirector("saved", path);
	}
	return false;
}

bool DirectorExtension::OnClose(const char *path) {
	CheckEnvironment(host);
	if (*path) {
		::SendDirector("closed", path);
	}
	return false;
}

bool DirectorExtension::NeedsOnClose() {
	CheckEnvironment(host);
	return HasConnection();
}

bool DirectorExtension::OnChar(char) {
	return false;
}

bool DirectorExtension::OnExecute(const char *cmd) {
	CheckEnvironment(host);
	::SendDirector("macro:run", cmd);
	return false;
}

bool DirectorExtension::OnSavePointReached() {
	return false;
}

bool DirectorExtension::OnSavePointLeft() {
	return false;
}

bool DirectorExtension::OnStyle(unsigned int, int, int, StyleWriter *) {
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

bool DirectorExtension::OnMacro(const char *command, const char *params) {
	SendDirector(command, params);
	return false;
}

bool DirectorExtension::SendProperty(const char *prop) {
	CheckEnvironment(host);
	if (*prop) {
		::SendDirector("property", prop);
	}
	return false;
}

void DirectorExtension::HandleStringMessage(const char *message) {
	// Message may contain multiple commands separated by '\n'
	// Reentrance trouble - if this function is reentered, the wCorrespondent may
	// be set to zero before time.
	StringList wlMessage(true);
	wlMessage.Set(message);
	for (int i = 0; i < wlMessage.len; i++) {
		// Message format is [:return address:]command:argument
		char *cmd = wlMessage[i];
		if (*cmd == ':') {
			// There is a return address
			char *colon = strchr(cmd + 1, ':');
			if (colon) {
				*colon = '\0';
				wCorrespondent = reinterpret_cast<HWND>(atoi(cmd + 1));
				cmd = colon + 1;
			}
		}
		if (isprefix(cmd, "identity:")) {
			char *arg = strchr(cmd, ':');
			if (arg)
				wDirector = reinterpret_cast<HWND>(atoi(arg + 1));
		} else if (isprefix(cmd, "closing:")) {
			wDirector = 0;
			if (startedByDirector) {
				shuttingDown = true;
				host->ShutDown();
				shuttingDown = false;
			}
		} else if (host) {
			host->Perform(cmd);
		}
		wCorrespondent = 0;
	}
}
