// SciTE - Scintilla based Text Editor
// DirectorExtension.h - Extension for communicating with a director program.
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class DirectorExtension : public Extension {
public:
	DirectorExtension();
	virtual ~DirectorExtension();

	// Implement the Extension interface
	virtual bool Initialise(ExtensionAPI *host_);
	virtual bool Finalise();
	virtual bool Clear();
	virtual bool Load(const char *filename);
	
	virtual bool OnOpen(const char *path);
	virtual bool OnSwitchFile(const char *path);
	virtual bool OnSave(const char *path);
	virtual bool OnChar(char ch);
	virtual bool OnExecute(const char *s);
	virtual bool OnSavePointReached();
	virtual bool OnSavePointLeft();
	virtual bool OnStyle(unsigned int startPos, int lengthDoc, int initStyle, Accessor *styler);
	virtual bool OnDoubleClick();
	virtual bool OnUpdateUI();
	virtual bool OnMarginClick();

	// Allow messages through to extension
	LRESULT HandleMessage(WPARAM wParam, LPARAM lParam);
	void HandleStringMessage(const char *message);
};

