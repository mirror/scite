// SciTE - Scintilla based Text Editor
/** @file Strips.h
 ** Definition of UI strips.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef UISF_HIDEACCEL
#define UISF_HIDEACCEL 2
#define UISF_HIDEFOCUS 1
#define UIS_CLEAR 2
#define UIS_SET 1
#endif

inline HWND HwndOf(GUI::Window w) {
	return reinterpret_cast<HWND>(w.GetID());
}

void *PointerFromWindow(HWND hWnd);
void SetWindowPointer(HWND hWnd, void *ptr);

class BaseWin : public GUI::Window {
protected:
	ILocalize *localiser;
public:
	BaseWin() : localiser(0) {
	}
	void SetLocalizer(ILocalize *localiser_) {
		localiser = localiser_;
	}
	HWND Hwnd() const {
		return reinterpret_cast<HWND>(GetID());
	}
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) = 0;
	static LRESULT PASCAL StWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
};

class Strip : public BaseWin {
protected:
	HFONT fontText;
	HTHEME hTheme;
	bool capturedMouse;
	SIZE closeSize;
	enum stripCloseState { csNone, csOver, csClicked, csClickedOver } closeState;
	GUI::Window wToolTip;

	GUI::Window CreateText(const char *text);
	GUI::Window CreateButton(const char *text, int ident, bool check=false);
	void Tab(bool forwards);
	virtual void Creation();
	virtual void Destruction();
	virtual void Close();
	virtual bool KeyDown(WPARAM key);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual bool HasClose() const;
	GUI::Rectangle CloseArea();
	void InvalidateClose();
	bool MouseInClose(GUI::Point pt);
	void TrackMouse(GUI::Point pt);
	void SetTheme();
	virtual LRESULT CustomDraw(NMHDR *pnmh);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
public:
	bool visible;
	Strip() : fontText(0), hTheme(0), capturedMouse(false), closeState(csNone), visible(false) {
		closeSize.cx = 11;
		closeSize.cy = 11;
	}
	virtual int Height() {
		return 25;
	}
};

class BackgroundStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wExplanation;
	GUI::Window wProgress;
public:
	BackgroundStrip() : entered(0), lineHeight(20) {
	}
	virtual void Creation();
	virtual void Destruction();
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual bool HasClose() const;
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	virtual int Height() {
		return lineHeight + 1;
	}
	void SetProgress(const GUI::gui_string &explanation, int size, int progress);
};

class SearchStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wButton;
	Searcher *pSearcher;
public:
	SearchStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void Next(bool select);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	virtual int Height() {
		return lineHeight + 1;
	}
};

class FindStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wButton;
	GUI::Window wButtonMarkAll;
	GUI::Window wCheckWord;
	GUI::Window wCheckCase;
	GUI::Window wCheckRE;
	GUI::Window wCheckBE;
	GUI::Window wCheckWrap;
	GUI::Window wCheckUp;
	Searcher *pSearcher;
public:
	FindStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void Next(bool markAll, bool invertDirection);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked);
	void ShowPopup();
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	void CheckButtons();
	void Show();
	virtual int Height() {
		return lineHeight + 1;
	}
};

class ReplaceStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wCheckWord;
	GUI::Window wCheckCase;
	GUI::Window wButtonFind;
	GUI::Window wButtonReplaceAll;
	GUI::Window wCheckRE;
	GUI::Window wCheckWrap;
	GUI::Window wCheckBE;
	GUI::Window wStaticReplace;
	GUI::Window wReplace;
	GUI::Window wButtonReplace;
	GUI::Window wButtonReplaceInSelection;
	Searcher *pSearcher;
public:
	ReplaceStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked);
	void ShowPopup();
	void HandleReplaceCommand(int cmd, bool reverseFind = false);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	void CheckButtons();
	void Show();
	virtual int Height() {
		return lineHeight * 2 + 1;
	}
};

class StripDefinition;

class UserStrip : public Strip {
	int entered;
	int lineHeight;
	StripDefinition *psd;
	Extension *extender;
	SciTEWin *pSciTEWin;
public:
	UserStrip() : entered(0), lineHeight(26), psd(0), extender(0), pSciTEWin(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual bool HasClose() const;
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	virtual int Height() {
		return lineHeight * Lines() + 1;
	}
	int Lines() const;
	void SetDescription(const char *description);
	void SetExtender(Extension *extender_);
	void SetSciTE(SciTEWin *pSciTEWin_);
	UserControl *FindControl(int control);
	void Set(int control, const char *value);
	void SetList(int control, const char *value);
	std::string GetValue(int control);
};
