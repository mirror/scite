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

void *PointerFromWindow(HWND hWnd);
void SetWindowPointer(HWND hWnd, void *ptr);
GUI::gui_string TextOfWindow(HWND hWnd);
GUI::gui_string ClassNameOfWindow(HWND hWnd);

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
		return static_cast<HWND>(GetID());
	}
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) = 0;
	static LRESULT PASCAL StWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
};

class Strip : public BaseWin {
protected:
	HFONT fontText;
	HTHEME hTheme;
	int scale;
	int space;
	bool capturedMouse;
	SIZE closeSize;
	enum stripCloseState { csNone, csOver, csClicked, csClickedOver } closeState;
	GUI::Window wToolTip;
	int entered;
	int lineHeight;

	GUI::Window CreateText(const char *text);
	GUI::Window CreateButton(const char *text, size_t ident, bool check=false);
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
	GUI::Rectangle LineArea(int line);
	virtual int Lines() const;
	void InvalidateClose();
	bool MouseInClose(GUI::Point pt);
	void TrackMouse(GUI::Point pt);
	void SetTheme();
	virtual LRESULT EditColour(HWND hwnd, HDC hdc);
	virtual LRESULT CustomDraw(NMHDR *pnmh);
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) override;
	virtual void ShowPopup();
public:
	bool visible;
	Strip() : fontText(0), hTheme(0), scale(96), space(2), capturedMouse(false), closeState(csNone), entered(0), lineHeight(20), visible(false) {
		closeSize.cx = 11;
		closeSize.cy = 11;
	}
	virtual ~Strip() = default;
	virtual int Height() {
		return lineHeight * Lines() + space - 1;
	}
};

class BackgroundStrip : public Strip {
	GUI::Window wExplanation;
	GUI::Window wProgress;
public:
	BackgroundStrip() {
	}
	virtual ~BackgroundStrip() = default;
	void Creation() override;
	void Destruction() override;
	void Close() override;
	void Focus();
	bool KeyDown(WPARAM key) override;
	bool Command(WPARAM wParam) override;
	void Size() override;
	bool HasClose() const override;
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) override;
	void SetProgress(const GUI::gui_string &explanation, size_t size, size_t progress);
};

class SearchStripBase : public Strip {
protected:
	Searcher *pSearcher;
	HBRUSH hbrNoMatch;
public:
	SearchStripBase() : pSearcher(0), hbrNoMatch(0) {
	}
	virtual ~SearchStripBase() = default;
	void SetSearcher(Searcher *pSearcher_) {
		pSearcher = pSearcher_;
	}
	void Creation() override;
	void Destruction() override;
	LRESULT NoMatchColour(HDC hdc);
};

class SearchStrip : public SearchStripBase {
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wButton;
public:
	SearchStrip() {
	}
	virtual ~SearchStrip() = default;
	void Creation() override;
	void Destruction() override;
	void Close() override;
	void Focus();
	bool KeyDown(WPARAM key) override;
	void Next(bool select);
	bool Command(WPARAM wParam) override;
	void Size() override;
	void Paint(HDC hDC) override;
	LRESULT EditColour(HWND hwnd, HDC hdc) override;
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) override;
};

class FindReplaceStrip : public SearchStripBase {
protected:
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wCheckWord;
	GUI::Window wCheckCase;
	GUI::Window wCheckRE;
	GUI::Window wCheckBE;
	GUI::Window wCheckWrap;
	enum IncrementalBehaviour { simple, incremental, showAllMatches };
	IncrementalBehaviour incrementalBehaviour;
	FindReplaceStrip() : incrementalBehaviour(simple) {
	}
	LRESULT EditColour(HWND hwnd, HDC hdc) override;
	enum ChangingSource { changingEdit, changingCombo };
	void NextIncremental(ChangingSource source);
public:
	virtual ~FindReplaceStrip() = default;
	void Close() override;
	void SetIncrementalBehaviour(int behaviour);
	void MarkIncremental();
};

class FindStrip : public FindReplaceStrip {
	GUI::Window wButton;
	GUI::Window wButtonMarkAll;
	GUI::Window wCheckUp;
public:
	FindStrip() {
	}
	virtual ~FindStrip() = default;
	void Creation() override;
	void Destruction() override;
	void Focus();
	bool KeyDown(WPARAM key) override;
	void Next(bool markAll, bool invertDirection);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) const;
	void ShowPopup() override;
	bool Command(WPARAM wParam) override;
	void Size() override;
	void Paint(HDC hDC) override;
	void CheckButtons();
	void ShowStrip();
};

class ReplaceStrip : public FindReplaceStrip {
	GUI::Window wButtonFind;
	GUI::Window wButtonReplaceAll;
	GUI::Window wStaticReplace;
	GUI::Window wReplace;
	GUI::Window wButtonReplace;
	GUI::Window wButtonReplaceInSelection;
public:
	ReplaceStrip() {
	}
	virtual ~ReplaceStrip() = default;
	void Creation() override;
	void Destruction() override;
	int Lines() const override;
	void Focus();
	bool KeyDown(WPARAM key) override;
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) const;
	void ShowPopup() override;
	void HandleReplaceCommand(int cmd, bool reverseFind = false);
	bool Command(WPARAM wParam) override;
	void Size() override;
	void Paint(HDC hDC) override;
	void CheckButtons();
	void ShowStrip();
};

class StripDefinition;

class UserStrip : public Strip {
	std::unique_ptr<StripDefinition> psd;
	Extension *extender;
	SciTEWin *pSciTEWin;
public:
	UserStrip() : extender(nullptr), pSciTEWin(nullptr) {
		lineHeight = 26;
	}
	virtual ~UserStrip() = default;
	void Creation() override;
	void Destruction() override;
	void Close() override;
	void Focus();
	bool KeyDown(WPARAM key) override;
	bool Command(WPARAM wParam) override;
	void Size() override;
	bool HasClose() const override;
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) override;
	int Lines() const override;
	void SetDescription(const char *description);
	void SetExtender(Extension *extender_);
	void SetSciTE(SciTEWin *pSciTEWin_);
	UserControl *FindControl(int control);
	void Set(int control, const char *value);
	void SetList(int control, const char *value);
	std::string GetValue(int control);
};
