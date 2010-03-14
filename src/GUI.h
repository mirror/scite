// SciTE - Scintilla based Text Editor
/** @file GUI.h
 ** Interface to platform GUI facilities.
 ** Split off from Scintilla's Platform.h to avoid SciTE depending on implementation of Scintilla.
 ** Implementation in win32/GUIWin.cxx for Windows and gtk/GUIGTK.cxx for GTK+.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef GUI_H
#define GUI_H

namespace GUI {

class Point {
public:
	int x;
	int y;

	explicit Point(int x_=0, int y_=0) : x(x_), y(y_) {
	}
};

class Rectangle {
public:
	int left;
	int top;
	int right;
	int bottom;

	Rectangle(int left_=0, int top_=0, int right_=0, int bottom_ = 0) :
		left(left_), top(top_), right(right_), bottom(bottom_) {
	}
	bool Contains(Point pt) const {
		return (pt.x >= left) && (pt.x <= right) &&
			(pt.y >= top) && (pt.y <= bottom);
	}
	int Width() const { return right - left; }
	int Height() const { return bottom - top; }
};

typedef void *WindowID;
class Window {
protected:
	WindowID wid;
public:
	Window() : wid(0) {
	}
	Window &operator=(WindowID wid_) {
		wid = wid_;
		return *this;
	}
	WindowID GetID() const {
		return wid;
	}
	bool Created() const {
		return wid != 0;
	}
	void Destroy();
	bool HasFocus();
	Rectangle GetPosition();
	void SetPosition(Rectangle rc);
	Rectangle GetClientPosition();
	void Show(bool show=true);
	void InvalidateAll();
	void SetTitle(const char *s);
};

typedef void *MenuID;
class Menu {
	MenuID mid;
public:
	Menu() : mid(0) {
	}
	MenuID GetID() const {
		return mid;
	}
	void CreatePopUp();
	void Destroy();
	void Show(Point pt, Window &w);
};

class ElapsedTime {
	long bigBit;
	long littleBit;
public:
	ElapsedTime();
	double Duration(bool reset=false);
};

struct ScintillaFailure {
	int status;
	ScintillaFailure(int status_) : status(status_) {
	}
};

class ScintillaWindow : public Window {
	// Private so ScintillaWindow objects can not be copied
	ScintillaWindow(const ScintillaWindow &source);
	ScintillaWindow &operator=(const ScintillaWindow &);
	SciFnDirect fn;
	long ptr;
public:
	ScintillaWindow() : fn(0), ptr(0) {
	}
	void SetID(WindowID wid_) {
		wid = wid_;
		fn = 0;
		ptr = 0;
		if (wid) {
			fn = reinterpret_cast<SciFnDirect>(
				Send(SCI_GETDIRECTFUNCTION, 0, 0));
			ptr = Send(SCI_GETDIRECTPOINTER, 0, 0);
		}
	}
	bool CanCall() const {
		return wid && fn && ptr;
	}
	long Call(unsigned int msg, unsigned long wParam=0, long lParam=0) {
		sptr_t retVal = fn(ptr, msg, wParam, lParam);
		sptr_t status = fn(ptr, SCI_GETSTATUS, 0, 0);
		if (status > 0)
			throw ScintillaFailure(status);
		return retVal;
	}
	long CallString(unsigned int msg, unsigned long wParam, const char *s) {
		return Call(msg, wParam, reinterpret_cast<sptr_t>(s));
	}
	long Send(unsigned int msg, unsigned long wParam=0, long lParam=0);
	long SendPointer(unsigned int msg, unsigned long wParam=0, void *lParam=0);
};

bool IsDBCSLeadByte(int codePage, char ch);

}

#endif
