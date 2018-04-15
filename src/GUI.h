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

#define ELEMENTS(a) (sizeof(a) / sizeof(a[0]))

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
	bool operator==(const Rectangle &other) const {
		return (left == other.left) &&
			(top == other.top) &&
			(right == other.right) &&
			(bottom == other.bottom);
	}
};

#if defined(GTK) || defined(__APPLE__)

// On GTK+ and OS X use UTF-8 char strings

typedef char gui_char;
typedef std::string gui_string;
typedef std::string_view gui_string_view;

#define GUI_TEXT(q) q

#else

// On Win32 use UTF-16 wide char strings

typedef wchar_t gui_char;
typedef std::wstring gui_string;
typedef std::wstring_view gui_string_view;

#define GUI_TEXT(q) L##q

#endif

gui_string StringFromUTF8(const char *s);
gui_string StringFromUTF8(const std::string &s);
std::string UTF8FromString(const gui_string &s);
gui_string StringFromInteger(long i);
gui_string StringFromLongLong(long long i);
gui_string HexStringFromInteger(long i);

std::string LowerCaseUTF8(std::string_view sv);

typedef void *WindowID;
class Window {
protected:
	WindowID wid;
public:
	Window() : wid(0) {
	}
	virtual ~Window() = default;
	Window &operator=(WindowID wid_) {
		wid = wid_;
		return *this;
	}
	WindowID GetID() const {
		return wid;
	}
	void SetID(WindowID wid_) {
		wid = wid_;
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
	void SetTitle(const gui_char *s);
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

class ScintillaPrimitive : public Window {
public:
	// Send is the basic method and can be used between threads on Win32
	sptr_t Send(unsigned int msg, uptr_t wParam=0, sptr_t lParam=0);
};

bool IsDBCSLeadByte(int codePage, char ch);

}

#endif
