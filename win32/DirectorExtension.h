// SciTE - Scintilla based Text Editor
/** @file DirectorExtension.h
 ** Extension for communicating with a director program.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class DirectorExtension : public Extension {
private:
	ExtensionAPI *host;
	DirectorExtension() : host(0) {} // Singleton
	// Deleted so DirectorExtension objects can not be copied.
	DirectorExtension(const DirectorExtension &) = delete;
	void operator=(const DirectorExtension &) = delete;

public:
	static DirectorExtension &Instance();

	// Implement the Extension interface
	virtual bool Initialise(ExtensionAPI *host_);
	virtual bool Finalise();
	virtual bool Clear();
	virtual bool Load(const char *filename);

	virtual bool OnOpen(const char *path);
	virtual bool OnSwitchFile(const char *path);
	virtual bool OnSave(const char *path);
	virtual bool OnChar(char ch);
	virtual bool OnExecute(const char *cmd);
	virtual bool OnSavePointReached();
	virtual bool OnSavePointLeft();
	virtual bool OnStyle(unsigned int startPos, int lengthDoc, int initStyle, StyleWriter *styler);
	virtual bool OnDoubleClick();
	virtual bool OnUpdateUI();
	virtual bool OnMarginClick();
	virtual bool OnMacro(const char *command, const char *params);

	virtual bool SendProperty(const char *prop);
	virtual bool OnClose(const char *path);
	virtual bool NeedsOnClose();

	// Allow messages through to extension
	void HandleStringMessage(const char *message);
};

