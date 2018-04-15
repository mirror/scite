// SciTE - Scintilla based Text Editor
/** @file ScintillaWindow.h
 ** Interface to a Scintilla instance.
 **/
// Copyright 1998-2018 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <string>

#include "Scintilla.h"

#include "GUI.h"
#include "ScintillaWindow.h"

namespace GUI {

ScintillaWindow::ScintillaWindow() : fn(0), ptr(0), status() {
}

ScintillaWindow::~ScintillaWindow() = default;

void ScintillaWindow::SetScintilla(GUI::WindowID wid_) {
	SetID(wid_);
	fn = 0;
	ptr = 0;
	if (wid) {
		fn = reinterpret_cast<SciFnDirect>(
			Send(SCI_GETDIRECTFUNCTION, 0, 0));
		ptr = Send(SCI_GETDIRECTPOINTER, 0, 0);
	}
}

bool ScintillaWindow::CanCall() const {
	return wid && fn && ptr;
}

int ScintillaWindow::Call(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	switch (msg) {
	case SCI_CREATEDOCUMENT:
	case SCI_CREATELOADER:
	case SCI_PRIVATELEXERCALL:
	case SCI_GETDIRECTFUNCTION:
	case SCI_GETDIRECTPOINTER:
	case SCI_GETDOCPOINTER:
	case SCI_GETCHARACTERPOINTER:
		throw ScintillaFailure(SC_STATUS_FAILURE);
	}
	if (!fn)
		throw ScintillaFailure(SC_STATUS_FAILURE);
	const sptr_t retVal = fn(ptr, msg, wParam, lParam);
	status = fn(ptr, SCI_GETSTATUS, 0, 0);
	if (status > 0 && status < SC_STATUS_WARN_START)
		throw ScintillaFailure(status);
	return static_cast<int>(retVal);
}

sptr_t ScintillaWindow::CallReturnPointer(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	const sptr_t retVal = fn(ptr, msg, wParam, lParam);
	status = fn(ptr, SCI_GETSTATUS, 0, 0);
	if (status > 0 && status < SC_STATUS_WARN_START)
		throw ScintillaFailure(status);
	return retVal;
}

int ScintillaWindow::CallPointer(unsigned int msg, uptr_t wParam, void *s) {
	return Call(msg, wParam, reinterpret_cast<sptr_t>(s));
}

int ScintillaWindow::CallString(unsigned int msg, uptr_t wParam, const char *s) {
	return Call(msg, wParam, reinterpret_cast<sptr_t>(s));
}

// Common APIs made more accessible
int ScintillaWindow::LineStart(int line) {
	return Call(SCI_POSITIONFROMLINE, line);
}
int ScintillaWindow::LineFromPosition(int position) {
	return Call(SCI_LINEFROMPOSITION, position);
}

}
