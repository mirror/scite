// SciTE - Scintilla based Text Editor
/** @file GUIWin.cxx
 ** Interface to platform GUI facilities.
 ** Split off from Scintilla's Platform.h to avoid SciTE depending on implementation of Scintilla.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <time.h>

#ifdef __MINGW_H
#define _WIN32_IE	0x0400
#endif

#ifdef __BORLANDC__
// Borland includes Windows.h for STL and defaults to different API number
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#endif

#define _WIN32_WINNT  0x0400
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
#pragma warning(disable: 4244)
#endif

#include "Scintilla.h"
#include "GUI.h"

namespace GUI {

void Window::Destroy() {
	if (wid)
		::DestroyWindow(reinterpret_cast<HWND>(wid));
	wid = 0;
}

bool Window::HasFocus() {
	return ::GetFocus() == wid;
}

Rectangle Window::GetPosition() {
	RECT rc;
	::GetWindowRect(reinterpret_cast<HWND>(wid), &rc);
	return Rectangle(rc.left, rc.top, rc.right, rc.bottom);
}

void Window::SetPosition(Rectangle rc) {
	::SetWindowPos(reinterpret_cast<HWND>(wid),
		0, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

Rectangle Window::GetClientPosition() {
	RECT rc={0,0,0,0};
	if (wid)
		::GetClientRect(reinterpret_cast<HWND>(wid), &rc);
	return  Rectangle(rc.left, rc.top, rc.right, rc.bottom);
}

void Window::Show(bool show) {
	if (show)
		::ShowWindow(reinterpret_cast<HWND>(wid), SW_SHOWNOACTIVATE);
	else
		::ShowWindow(reinterpret_cast<HWND>(wid), SW_HIDE);
}

void Window::InvalidateAll() {
	::InvalidateRect(reinterpret_cast<HWND>(wid), NULL, FALSE);
}

void Window::SetTitle(const char *s) {
	::SetWindowTextA(reinterpret_cast<HWND>(wid), s);
}

void Menu::CreatePopUp() {
	Destroy();
	mid = ::CreatePopupMenu();
}

void Menu::Destroy() {
	if (mid)
		::DestroyMenu(reinterpret_cast<HMENU>(mid));
	mid = 0;
}

void Menu::Show(Point pt, Window &w) {
	::TrackPopupMenu(reinterpret_cast<HMENU>(mid),
		0, pt.x - 4, pt.y, 0,
		reinterpret_cast<HWND>(w.GetID()), NULL);
	Destroy();
}

static bool initialisedET = false;
static bool usePerformanceCounter = false;
static LARGE_INTEGER frequency;

ElapsedTime::ElapsedTime() {
	if (!initialisedET) {
		usePerformanceCounter = ::QueryPerformanceFrequency(&frequency) != 0;
		initialisedET = true;
	}
	if (usePerformanceCounter) {
		LARGE_INTEGER timeVal;
		::QueryPerformanceCounter(&timeVal);
		bigBit = timeVal.HighPart;
		littleBit = timeVal.LowPart;
	} else {
		bigBit = clock();
	}
}

double ElapsedTime::Duration(bool reset) {
	double result;
	long endBigBit;
	long endLittleBit;

	if (usePerformanceCounter) {
		LARGE_INTEGER lEnd;
		::QueryPerformanceCounter(&lEnd);
		endBigBit = lEnd.HighPart;
		endLittleBit = lEnd.LowPart;
		LARGE_INTEGER lBegin;
		lBegin.HighPart = bigBit;
		lBegin.LowPart = littleBit;
		double elapsed = lEnd.QuadPart - lBegin.QuadPart;
		result = elapsed / static_cast<double>(frequency.QuadPart);
	} else {
		endBigBit = clock();
		endLittleBit = 0;
		double elapsed = endBigBit - bigBit;
		result = elapsed / CLOCKS_PER_SEC;
	}
	if (reset) {
		bigBit = endBigBit;
		littleBit = endLittleBit;
	}
	return result;
}

long ScintillaWindow::Send(unsigned int msg, unsigned long wParam, long lParam) {
	return ::SendMessage(reinterpret_cast<HWND>(GetID()), msg, wParam, lParam);
}

long ScintillaWindow::SendPointer(unsigned int msg, unsigned long wParam, void *lParam) {
	return ::SendMessage(reinterpret_cast<HWND>(GetID()), msg, wParam, reinterpret_cast<LPARAM>(lParam));
}

bool IsDBCSLeadByte(int codePage, char ch) {
	if (SC_CP_UTF8 == codePage)
		// For lexing, all characters >= 0x80 are treated the
		// same so none is considered a lead byte.
		return false;
	else
		return ::IsDBCSLeadByteEx(codePage, ch) != 0;
}

}
