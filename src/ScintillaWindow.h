// SciTE - Scintilla based Text Editor
/** @file ScintillaWindow.h
 ** Interface to a Scintilla instance.
 **/
// Copyright 1998-2018 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef SCINTILLAWINDOW_H
#define SCINTILLAWINDOW_H

namespace GUI {

struct ScintillaFailure {
	sptr_t status;
	explicit ScintillaFailure(sptr_t status_) : status(status_) {
	}
};

class ScintillaWindow : public ScintillaPrimitive {
	SciFnDirect fn;
	sptr_t ptr;
public:
	sptr_t status;
	ScintillaWindow();
	~ScintillaWindow() override;
	// Deleted so ScintillaWindow objects can not be copied.
	ScintillaWindow(const ScintillaWindow &source) = delete;
	ScintillaWindow &operator=(const ScintillaWindow &) = delete;

	void SetScintilla(GUI::WindowID wid_);
	bool CanCall() const;
	int Call(unsigned int msg, uptr_t wParam=0, sptr_t lParam=0);
	sptr_t CallReturnPointer(unsigned int msg, uptr_t wParam=0, sptr_t lParam=0);
	int CallPointer(unsigned int msg, uptr_t wParam, void *s);
	int CallString(unsigned int msg, uptr_t wParam, const char *s);
};

}

#endif
