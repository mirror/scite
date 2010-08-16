// SciTE - Scintilla based Text Editor
// SciTEGTK.cxx - main code for the GTK+ version of the editor
// Copyright 1998-2004 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>

#include <string>
#include <map>
#include <algorithm>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <unistd.h>
#include <glib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#include "Scintilla.h"
#include "ScintillaWidget.h"

#include "GUI.h"
#include "SString.h"
#include "StringList.h"
#include "FilePath.h"
#include "PropSetFile.h"

#include "Extender.h"

#ifndef NO_EXTENSIONS
#include "MultiplexExtension.h"

#ifndef NO_FILER
#include "DirectorExtension.h"
#endif

#ifndef NO_LUA
#include "LuaExtension.h"
#endif

#endif

#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "pixmapsGNOME.h"
#include "SciIcon.h"
#include "SciTEBase.h"
#include "SciTEKeys.h"

#define MB_ABOUTBOX	0x100000L

const char appName[] = "SciTE";

static GtkWidget *PWidget(GUI::Window &w) {
	return reinterpret_cast<GtkWidget *>(w.GetID());
}

static GtkWidget *MakeToggle(const char *text, bool active) {
	GtkWidget *toggle = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), active);
	return toggle;
}

static GtkWidget *MakeCommand(const char *text, GtkSignalFunc func, gpointer data) {
	GtkWidget *command = gtk_button_new_with_mnemonic(text);
	GTK_WIDGET_SET_FLAGS(command, GTK_CAN_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(command), "clicked", func, data);
	return command;
}

class SciTEGTK;

typedef void (*SigFunction)(GtkWidget *w, SciTEGTK *app);

// Callback thunk class connects GTK+ signals to a SciTEGTK instance method.
template< void (SciTEGTK::*method)() >
class Signal {
public:
	static void Function(GtkWidget */*w*/, SciTEGTK *app) {
		(app->*method)();
	}
};

// Callback thunk class connects GTK+ signals to an instance method.
template< class T, void (T::*method)() >
class ObjectSignal {
public:
	static void Function(GtkWidget */*w*/, T *object) {
		(object->*method)();
	}
};

// Callback thunk class connects GTK+ signals to a SciTEGTK instance method.
template< void (SciTEGTK::*method)(int responseID) >
class ResponseSignal {
public:
	static void Function(GtkDialog */*w*/, gint responseID, SciTEGTK *app) {
		(app->*method)(responseID);
	}
};

template< void (SciTEGTK::*method)(int responseID) >
inline void AttachResponse(GtkWidget *w, SciTEGTK *object) {
	ResponseSignal <method> sig;
	g_signal_connect(GTK_OBJECT(w), "response", GCallback(sig.Function), object);
}

class Dialog : public GUI::Window {
public:
	Dialog() : app(0), dialogCanceled(true), localiser(0) {}
	void Create(SciTEGTK *app_, const char *title, Localization *localiser_, bool resizable=true) {
		app = app_;
		localiser = localiser_;
		wid = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(Widget()), localiser->Text(title).c_str());
		gtk_window_set_resizable(GTK_WINDOW(Widget()), resizable);
	}
	bool Display(GtkWidget *parent = 0, bool modal=true) {
		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(Widget()), modal);
		if (parent) {
			gtk_window_set_transient_for(GTK_WINDOW(Widget()), GTK_WINDOW(parent));
		}
		gtk_signal_connect(GTK_OBJECT(Widget()), "key_press_event",
		                   GtkSignalFunc(SignalKey), this);
		gtk_signal_connect(GTK_OBJECT(Widget()), "destroy",
		                   GtkSignalFunc(SignalDestroy), this);
		gtk_widget_show_all(Widget());
		if (modal) {
			while (Created()) {
				gtk_main_iteration();
			}
		}
		return dialogCanceled;
	}
	void OK() {
		dialogCanceled = false;
		Destroy();
	}
	void Cancel() {
		dialogCanceled = true;
		Destroy();
	}
	static void SignalCancel(GtkWidget *, Dialog *d) {
		if (d) {
			d->Cancel();
		}
	}
	GtkWidget *Toggle(const char *original, bool active) {
		return MakeToggle(localiser->Text(original).c_str(), active);
	}
	GtkWidget *CommandButton(const char *original, SigFunction func, bool makeDefault=false) {
		return CreateButton(original, GtkSignalFunc(func), app, makeDefault);
	}
	GtkWidget *ResponseButton(const char *original, int responseID) {
		return gtk_dialog_add_button(GTK_DIALOG(Widget()),
			localiser->Text(original).c_str(), responseID);
	}
	void CancelButton() {
		CreateButton("_Cancel", GtkSignalFunc(SignalCancel), this, false);
	}
	GtkWidget *Button(const char *original, SigFunction func) {
		return MakeCommand(localiser->Text(original).c_str(), GtkSignalFunc(func), app);
	}
	void OnActivate(GtkWidget *w, SigFunction func) {
		gtk_signal_connect(GTK_OBJECT(w), "activate", GtkSignalFunc(func), app);
	}
	void Present() {
		gtk_window_present(GTK_WINDOW(Widget()));
	}

private:
	SciTEGTK *app;
	bool dialogCanceled;
	GtkAccelGroup *accel_group;
	Localization *localiser;
	GtkWidget *Widget() const {
		return reinterpret_cast<GtkWidget *>(GetID());
	}
	static void SignalDestroy(GtkWidget *, Dialog *d) {
		if (d) {
			d->wid = 0;
		}
	}
	static gint SignalKey(GtkWidget *w, GdkEventKey *event, Dialog *d) {
		if (event->keyval == GDK_Escape) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
			d->Cancel();
		}
		return FALSE;
	}
	GtkWidget *CreateButton(const char *original, GtkSignalFunc func, gpointer data, bool makeDefault) {
		GtkWidget *btn = MakeCommand(localiser->Text(original).c_str(), func, data);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Widget())->action_area), btn, TRUE, TRUE, 0);
		if (makeDefault) {
			gtk_widget_grab_default(btn);
		}
		gtk_widget_show(btn);
		return btn;
	}
};

// Field added to GTK+ 1.x ItemFactoryEntry for 2.x  so have a struct that is the same as 1.x
struct SciTEItemFactoryEntry {
	const char *path;
	const char *accelerator;
	GtkItemFactoryCallback callback;
	unsigned int callback_action;
	const char *item_type;
};

long SciTEKeys::ParseKeyCode(const char *mnemonic) {
	int modsInKey = 0;
	int keyval = -1;

	if (mnemonic && *mnemonic) {
		SString sKey = mnemonic;

		if (sKey.contains("Ctrl+")) {
			modsInKey |= GDK_CONTROL_MASK;
			sKey.remove("Ctrl+");
		}
		if (sKey.contains("Shift+")) {
			modsInKey |= GDK_SHIFT_MASK;
			sKey.remove("Shift+");
		}
		if (sKey.contains("Alt+")) {
			modsInKey |= GDK_MOD1_MASK;
			sKey.remove("Alt+");
		}

		if (sKey.length() == 1) {
			if (modsInKey & GDK_CONTROL_MASK && !(modsInKey & GDK_SHIFT_MASK))
				sKey.lowercase();
			keyval = sKey[0];
		} else if ((sKey.length() > 1)) {
			if ((sKey[0] == 'F') && (isdigit(sKey[1]))) {
				sKey.remove("F");
				int fkeyNum = sKey.value();
				if (fkeyNum >= 1 && fkeyNum <= 12)
					keyval = fkeyNum - 1 + GDK_F1;
			} else {
				if (sKey == "Left") {
					keyval = GDK_Left;
				} else if (sKey == "Right") {
					keyval = GDK_Right;
				} else if (sKey == "Up") {
					keyval = GDK_Up;
				} else if (sKey == "Down") {
					keyval = GDK_Down;
				} else if (sKey == "Insert") {
					keyval = GDK_Insert;
				} else if (sKey == "End") {
					keyval = GDK_End;
				} else if (sKey == "Home") {
					keyval = GDK_Home;
				} else if (sKey == "Enter") {
					keyval = GDK_Return;
				} else if (sKey == "Space") {
					keyval = GDK_space;
				} else if (sKey == "Tab") {
					keyval = GDK_Tab;
				} else if (sKey == "KeypadPlus") {
					keyval = GDK_KP_Add;
				} else if (sKey == "KeypadMinus") {
					keyval = GDK_KP_Subtract;
				} else if (sKey == "KeypadMultiply") {
					keyval = GDK_KP_Multiply;
				} else if (sKey == "KeypadDivide") {
					keyval = GDK_KP_Divide;
				} else if (sKey == "Escape") {
					keyval = GDK_Escape;
				} else if (sKey == "Delete") {
					keyval = GDK_Delete;
				} else if (sKey == "PageUp") {
					keyval = GDK_Page_Up;
				} else if (sKey == "PageDown") {
					keyval = GDK_Page_Down;
				} else if (sKey == "Slash") {
					keyval = GDK_slash;
				} else if (sKey == "Question") {
					keyval = GDK_question;
				} else if (sKey == "Equal") {
					keyval = GDK_equal;
				} else if (sKey == "Win") {
					keyval = GDK_Super_L;
				} else if (sKey == "Menu") {
					keyval = GDK_Menu;
				}
			}
		}
	}

	return (keyval > 0) ? (keyval | (modsInKey<<16)) : 0;
}

bool SciTEKeys::MatchKeyCode(long parsedKeyCode, int keyval, int modifiers) {
	return parsedKeyCode && !(0xFFFF0000 & (keyval | modifiers)) && (parsedKeyCode == (keyval | (modifiers<<16)));
}

class Strip;

class WWidget : public GUI::Window {
public:
	operator GtkWidget*() {
		return GTK_WIDGET(GetID());
	}
	GtkWidget* Widget() {
		return GTK_WIDGET(GetID());
	}
};

class WStatic : public WWidget {
public:
	void Create(const GUI::gui_string &text) {
		SetID(gtk_label_new_with_mnemonic(text.c_str()));
	}
};

class WEntry : public WWidget {
public:
	void Create(const char *text=0) {
		SetID(gtk_entry_new());
		if (text)
			gtk_entry_set_text(GTK_ENTRY(GetID()), text);
	}
	void ActivatesDefault() {
		gtk_entry_set_activates_default(GTK_ENTRY(GetID()), TRUE);
	}
	const char *Text() {
		return gtk_entry_get_text(GTK_ENTRY(GetID()));
	}
	int Value() {
		return atoi(Text());
	}
};

class WComboBoxEntry : public WWidget {
public:
	void Create() {
		SetID(gtk_combo_box_entry_new_text());
	}
	GtkEntry *Entry() {
		return GTK_ENTRY(gtk_bin_get_child(GTK_BIN(GetID())));
	}
	const char *Text() {
		return gtk_entry_get_text(Entry());
	}
	bool HasFocusOnSelfOrChild() {
		return HasFocus() || GTK_WIDGET_HAS_FOCUS(Entry());
	}
};

class WButton : public WWidget {
public:
	void Create(const GUI::gui_string &text, GtkSignalFunc func, gpointer data) {
		SetID(gtk_button_new_with_mnemonic(text.c_str()));
		GTK_WIDGET_SET_FLAGS(GetID(), GTK_CAN_DEFAULT);
		gtk_signal_connect(GTK_OBJECT(GetID()), "clicked", func, data);
	}
};

class WToggle : public WWidget {
public:
	void Create(const GUI::gui_string &text, bool active) {
		SetID(gtk_check_button_new_with_mnemonic(text.c_str()));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GetID()), active);
	}
	bool Active() {
		return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GetID()));
	}
};

class WCheckDraw : public WWidget {
	bool *pControlVariable;
	GdkPixbuf *pbAlpha;
	Strip *pstrip;
	bool over;
public:
	WCheckDraw() : pControlVariable(0), pbAlpha(0), pstrip(0), over(false) {
	}
	void Create(const char **xpmImage, const GUI::gui_string &toolTip, bool *pControlVariable_, Strip *pstrip_);
	void Toggle();
	static gboolean Focus(GtkWidget *widget, GdkEventFocus *event, WCheckDraw *pcd);
	gint Press(GtkWidget *widget, GdkEventButton *event);
	static gint ButtonsPress(GtkWidget *widget, GdkEventButton *event, WCheckDraw *pcd);
	static gboolean MouseEnterLeave(GtkWidget *widget, GdkEventCrossing *event, WCheckDraw *pcd);
	static gboolean KeyDown(GtkWidget *widget, GdkEventKey *event, WCheckDraw *pcd);
	gboolean Expose(GtkWidget *widget, GdkEventExpose *event);
	static gboolean ExposeEvent(GtkWidget *widget, GdkEventExpose *event, WCheckDraw *pcd);
};

class DialogGoto : public Dialog {
public:
	WEntry entryGoto;
};

class BaseWin : public GUI::Window {
public:
	SciTEGTK *pSciTEGTK;
	Localization *localiser;
	BaseWin() : pSciTEGTK(0), localiser(0) {
	}
	void SetSciTE(SciTEGTK *pSciTEGTK_, Localization *localiser_) {
		pSciTEGTK = pSciTEGTK_;
		localiser = localiser_;
	}
};

class Strip : public BaseWin {
protected:
	bool allowMenuActions;
	bool childHasFocus;
	enum { heightButton=23, heightStatic=12, widthCombo=20};
public:
	bool visible;
	Strip() : allowMenuActions(false), childHasFocus(false), visible(false) {
	}
	virtual ~Strip() {
	}
	virtual void Show();
	virtual void Close();
	virtual bool KeyDown(GdkEventKey *event);
	virtual void ShowPopup() = 0;
	virtual GtkStyle *ButtonStyle() = 0;
	virtual void MenuAction(guint action) = 0;
	static void MenuSignal(GtkMenuItem *menuItem, Strip *pStrip);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked);
	void ChildFocus(GtkWidget *widget);
	static gboolean ChildFocusSignal(GtkContainer *container, GtkWidget *widget, Strip *pStrip);
	virtual gboolean Focus(GtkDirectionType direction) = 0;
	static gboolean FocusSignal(GtkWidget *widget, GtkDirectionType direction, Strip *pStrip);
	bool VisibleHasFocus();
};

class FindStrip : public Strip {
public:
	WStatic wStaticFind;
	WComboBoxEntry wText;
	WButton wButton;
	WButton wButtonMarkAll;
	enum { checks = 6 };
	WCheckDraw wCheck[checks];

	FindStrip() {
	}
	virtual void Creation(GtkWidget *boxMain);
	virtual void Destruction();
	virtual void Show();
	virtual void Close();
	virtual bool KeyDown(GdkEventKey *event);
	void MenuAction(guint action);
	static void ActivateSignal(GtkWidget *w, FindStrip *pStrip);
	static gboolean EscapeSignal(GtkWidget *w, GdkEventKey *event, FindStrip *pStrip);
	void ShowPopup();
	void FindNextCmd();
	void MarkAllCmd();
	GtkStyle *ButtonStyle();
	gboolean Focus(GtkDirectionType direction);
};

class ReplaceStrip : public Strip {
public:
	WStatic wStaticFind;
	WComboBoxEntry wText;
	WButton wButtonFind;
	WButton wButtonReplaceAll;
	WStatic wStaticReplace;
	WComboBoxEntry wReplace;
	WButton wButtonReplace;
	WButton wButtonReplaceInSelection;
	enum { checks = 5 };
	WCheckDraw wCheck[checks];

	virtual void Creation(GtkWidget *boxMain);
	virtual void Destruction();
	virtual void Show();
	virtual void Close();
	virtual bool KeyDown(GdkEventKey *event);
	void MenuAction(guint action);
	static void ActivateSignal(GtkWidget *w, ReplaceStrip *pStrip);
	static gboolean EscapeSignal(GtkWidget *w, GdkEventKey *event, ReplaceStrip *pStrip);
	void GrabFields();
	void FindCmd();
	void ReplaceAllCmd();
	void ReplaceCmd();
	void ReplaceInSelectionCmd();
	void ShowPopup();
	GtkStyle *ButtonStyle();
	gboolean Focus(GtkDirectionType direction);
};

class SciTEGTK : public SciTEBase {
	friend class Strip;
	friend class FindStrip;
	friend class ReplaceStrip;

protected:

	GUI::Window wDivider;
	GUI::Point ptOld;
	GdkGC *xor_gc;
	bool focusEditor;
	bool focusOutput;

	guint sbContextID;
	GUI::Window wToolBarBox;
	int toolbarDetachable;
	int menuSource;

	// Control of sub process
	FilePath sciteExecutable;
	int icmd;
	int originalEnd;
	int fdFIFO;
	int pidShell;
	bool triedKill;
	int exitStatus;
	guint pollID;
	int inputHandle;
	GUI::ElapsedTime commandTime;
	SString lastOutput;
	int lastFlags;

	// For single instance
	guint32 startupTimestamp;

	enum FileFormat { sfSource, sfCopy, sfHTML, sfRTF, sfPDF, sfTEX, sfXML } saveFormat;
	Dialog dlgFileSelector;
	Dialog dlgFindInFiles;
	WComboBoxEntry comboFiles;
	DialogGoto dlgGoto;
	Dialog dlgTabSize;
	bool paramDialogCanceled;

	GtkWidget *wIncrementPanel;
	GtkWidget *IncSearchEntry;

	FindStrip findStrip;
	ReplaceStrip replaceStrip;

	Dialog dlgFindReplace;
	Dialog dlgParameters;

	GtkWidget *entryTabSize;
	GtkWidget *entryIndentSize;
	GtkWidget *toggleWord;
	GtkWidget *toggleCase;
	GtkWidget *toggleRegExp;
	GtkWidget *toggleWrap;
	GtkWidget *toggleUnSlash;
	GtkWidget *toggleReverse;
	GtkWidget *toggleUseTabs;
	WComboBoxEntry comboFind;
	WComboBoxEntry comboFindInFiles;
	WComboBoxEntry comboDir;
	WComboBoxEntry comboReplace;
	GtkWidget *entryParam[maxParam];
	GtkWidget *btnCompile;
	GtkWidget *btnBuild;
	GtkWidget *btnStop;
	GtkItemFactory *itemFactory;
	GtkAccelGroup *accelGroup;

	gint	fileSelectorWidth;
	gint	fileSelectorHeight;

	// Fullscreen handling
	GdkRectangle saved;

	GtkWidget *AddMBButton(GtkWidget *dialog, const char *label,
	                       int val, GtkAccelGroup *accel_group, bool isDefault = false);
	void SetWindowName();
	void ShowFileInStatus();
	void SetIcon();

	virtual void ReadLocalization();
	virtual void ReadPropertiesInitial();
	virtual void ReadProperties();
	virtual void GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize);

	virtual void SizeContentWindows();
	virtual void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const char *text, const char *mnemonic = 0);
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenusClipboard();
	virtual void CheckMenus();
	static void PopUpCmd(GtkMenuItem *menuItem, SciTEGTK *scitew);
	virtual void AddToPopUp(const char *label, int cmd = 0, bool enabled = true);
	virtual void ExecuteNext();
	void ResetExecution();

	virtual void OpenUriList(const char *list);
	virtual bool OpenDialog(FilePath directory, const char *filter);
	bool HandleSaveAs(const char *savePath);
	bool SaveAsXXX(FileFormat fmt, const char *title, const char *ext=0);
	virtual bool SaveAsDialog();
	virtual void SaveACopy();
	virtual void SaveAsHTML();
	virtual void SaveAsRTF();
	virtual void SaveAsPDF();
	virtual void SaveAsTEX();
	virtual void SaveAsXML();
	virtual void LoadSessionDialog();
	virtual void SaveSessionDialog();

	virtual void Print(bool);
	virtual void PrintSetup();

	virtual SString GetRangeInUIEncoding(GUI::ScintillaWindow &wCurrent, int selStart, int selEnd);

	virtual int WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, int style);
	virtual void FindMessageBox(const SString &msg, const SString *findItem=0);
	virtual void AboutDialog();
	virtual void QuitProgram();

	bool FindReplaceAdvanced();
	virtual SString EncodeString(const SString &s);
	void FindReplaceGrabFields();
	void HandleFindReplace();
	virtual void Find();
	void TranslatedSetTitle(GtkWindow *w, const char *original);
	GtkWidget *TranslatedLabel(const char *original);
	virtual void FindIncrement();
	void FindInFilesResponse(int responseID);
	virtual void FindInFiles();
	virtual void Replace();
	void FindReplaceResponse(int responseID);
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();
	virtual void GoLineDialog();
	virtual bool AbbrevDialog();
	virtual void TabSizeDialog();
	virtual bool ParametersDialog(bool modal);

	virtual FilePath GetDefaultDirectory();
	virtual FilePath GetSciteDefaultHome();
	virtual FilePath GetSciteUserHome();

	virtual void SetStatusBarText(const char *s);
	virtual void TabInsert(int index, const GUI::gui_char *title);
	virtual void TabSelect(int index);
	virtual void RemoveAllTabs();
	virtual void SetFileProperties(PropSetFile &ps);
	virtual void UpdateStatusBar(bool bUpdateSlowData);

	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar();
	virtual void ShowTabBar();
	virtual void ShowStatusBar();
	virtual void ActivateWindow(const char *timestamp);
	void CopyPath();
	bool &FlagFromCmd(int cmd);
	void Command(unsigned long wParam, long lParam = 0);
	void ContinueExecute(int fromPoll);

	// Single instance
	void SendFileName(int sendPipe, const char* filename);
	bool CheckForRunningInstance(int argc, char* argv[]);

	// GTK+ Signal Handlers

	void FindInFilesCmd();
	void FindInFilesDotDot();
	void FindInFilesBrowse();

	void GotoCmd();
	void GotoResponse(int responseID);
	void TabSizeSet(int &tabSize, bool &useTabs);
	void TabSizeCmd();
	void TabSizeConvertCmd();
	void TabSizeResponse(int responseID);
	void FindIncrementCmd();
	void FindIncrementCompleteCmd();
	static gboolean FindIncrementFocusOutSignal(GtkWidget *w);
	static gboolean FindIncrementEscapeSignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);

	void FRCancelCmd();
	static gint FRKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	void FRFindCmd();
	void FRReplaceCmd();
	void FRReplaceAllCmd();
	void FRReplaceInSelectionCmd();
	void FRReplaceInBuffersCmd();
	void FRMarkAllCmd();

	virtual bool ParametersOpen();
	virtual void ParamGrab();
	static gint ParamKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	void ParamCancelCmd();
	void ParamCmd();
	void ParamResponse(int responseID);

	static void IOSignal(SciTEGTK *scitew);
	static gint MoveResize(GtkWidget *widget, GtkAllocation *allocation, SciTEGTK *scitew);
	static gint QuitSignal(GtkWidget *w, GdkEventAny *e, SciTEGTK *scitew);
	static void ButtonSignal(GtkWidget *widget, gpointer data);
	static void MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *w);
	static void CommandSignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);
	static void NotifySignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);
	static gint KeyPress(GtkWidget *widget, GdkEventKey *event, SciTEGTK *scitew);
	gint Key(GdkEventKey *event);
	static gint MousePress(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);
	gint Mouse(GdkEventButton *event);

	void DividerXOR(GUI::Point pt);
	static gint DividerExpose(GtkWidget *widget, GdkEventExpose *ose, SciTEGTK *scitew);
	static gint DividerMotion(GtkWidget *widget, GdkEventMotion *event, SciTEGTK *scitew);
	static gint DividerPress(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);
	static gint DividerRelease(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);
	static void DragDataReceived(GtkWidget *widget, GdkDragContext *context,
	                             gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, SciTEGTK *scitew);

	gint TabBarRelease(GtkNotebook *notebook, GdkEventButton *event);
	gint TabBarScroll(GdkEventScroll *event);
	static gint TabBarReleaseSignal(GtkNotebook *notebook, GdkEventButton *event, SciTEGTK *scitew) {
		return scitew->TabBarRelease(notebook, event);
	}
	static gint TabBarScrollSignal(GtkNotebook *, GdkEventScroll *event, SciTEGTK *scitew) {
		return scitew->TabBarScroll(event);
	}

	// This is used to create the pixmaps used in the interface.
	GdkPixbuf *CreatePixbuf(const char *filename);
	// Callback function to show hidden files in filechooser
	static void toggle_hidden_cb(GtkToggleButton *toggle, gpointer data);
public:

	// TODO: get rid of this - use callback argument to find SciTEGTK
	static SciTEGTK *instance;

	SciTEGTK(Extension *ext = 0);
	~SciTEGTK();

	void WarnUser(int warnID);
	GtkWidget *pixmap_new(GtkWidget *window, gchar **xpm);
	GtkWidget *AddToolButton(const char *text, int cmd, GtkWidget *toolbar_icon);
	void AddToolBar();
	SString TranslatePath(const char *path);
	void CreateTranslatedMenu(int n, SciTEItemFactoryEntry items[],
	                          int nRepeats = 0, const char *prefix = 0, int startNum = 0,
	                          int startID = 0, const char *radioStart = 0);
	void CreateMenu();
	void CreateStrips(GtkWidget *boxMain);
	bool StripHasFocus();
	void CreateUI();
	void Run(int argc, char *argv[]);
	void ProcessExecute();
	virtual void Execute();
	virtual void StopExecute();
	static int PollTool(SciTEGTK *scitew);
	static void ChildSignal(int);
	// Single instance
	void SetStartupTime(const char *timestamp);
};

SciTEGTK *SciTEGTK::instance;

SciTEGTK::SciTEGTK(Extension *ext) : SciTEBase(ext) {
	menuSource = 0;
	// Control of sub process
	icmd = 0;
	originalEnd = 0;
	fdFIFO = 0;
	pidShell = 0;
	triedKill = false;
	exitStatus = 0;
	pollID = 0;
	inputHandle = 0;

	startupTimestamp = 0;

	PropSetFile::SetCaseSensitiveFilenames(true);
	propsEmbed.Set("PLAT_GTK", "1");

	pathAbbreviations = GetAbbrevPropertiesFileName();

	ReadGlobalPropFile();
	ReadAbbrevPropFile();

	ptOld = GUI::Point(0, 0);
	xor_gc = 0;
	saveFormat = sfSource;
	paramDialogCanceled = true;
	entryTabSize = 0;
	entryIndentSize = 0;
	IncSearchEntry = 0;
	toggleWord = 0;
	toggleCase = 0;
	toggleRegExp = 0;
	toggleWrap = 0;
	toggleUnSlash = 0;
	toggleReverse = 0;
	toggleUseTabs = 0;
	btnCompile = 0;
	btnBuild = 0;
	btnStop = 0;
	itemFactory = 0;

	fileSelectorWidth = 580;
	fileSelectorHeight = 480;

	// Fullscreen handling
	fullScreen = false;

	instance = this;
}

SciTEGTK::~SciTEGTK() {}

static void destroyDialog(GtkWidget *, gpointer *window) {
	if (window) {
		GUI::Window *pwin = reinterpret_cast<GUI::Window *>(window);
		*(pwin) = 0;
	}
}

void SciTEGTK::WarnUser(int) {}

static GtkWidget *messageBoxDialog = 0;
static long messageBoxResult = 0;

static gint messageBoxKey(GtkWidget *w, GdkEventKey *event, gpointer p) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		gtk_widget_destroy(GTK_WIDGET(w));
		messageBoxDialog = 0;
		messageBoxResult = reinterpret_cast<long>(p);
	}
	return FALSE;
}

static void messageBoxDestroy(GtkWidget *, gpointer *) {
	messageBoxDialog = 0;
}

static void messageBoxOK(GtkWidget *, gpointer p) {
	gtk_widget_destroy(GTK_WIDGET(messageBoxDialog));
	messageBoxDialog = 0;
	messageBoxResult = reinterpret_cast<long>(p);
}

GtkWidget *SciTEGTK::AddMBButton(GtkWidget *dialog, const char *label,
	int val, GtkAccelGroup *accel_group, bool isDefault) {
	GUI::gui_string translated = localiser.Text(label);
	GtkWidget *button = gtk_button_new_with_mnemonic(translated.c_str());
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	guint key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(button)->child), translated.c_str());
	gtk_widget_add_accelerator(button, "clicked", accel_group,
	                           key, GdkModifierType(0), (GtkAccelFlags)0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GtkSignalFunc(messageBoxOK), reinterpret_cast<gpointer>(val));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
 	                   button, TRUE, TRUE, 0);
	if (isDefault) {
		gtk_widget_grab_default(GTK_WIDGET(button));
	}
	gtk_widget_show(button);
	return button;
}

// This is an internally used function to create pixmaps.
GdkPixbuf *SciTEGTK::CreatePixbuf(const char *filename) {
	char path[MAX_PATH + 20];
	strncpy(path, PIXMAP_PATH, sizeof(path));
	strcat(path, pathSepString);
	strcat(path, filename);

	GError *error = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &error);
	if (!pixbuf) {
		//~ fprintf(stderr, "Failed to load pixbuf file: %s: %s\n",
			//~ path, error->message);
		g_error_free(error);
	}
	return pixbuf;
}

FilePath SciTEGTK::GetDefaultDirectory() {
	const char *where = getenv("SciTE_HOME");
#ifdef SYSCONF_PATH
	if (!where) {
		where = SYSCONF_PATH;
	}
#else
	if (!where) {
		where = getenv("HOME");
	}
#endif
	if (where) {
		return FilePath(where);
	}

	return FilePath("");
}

FilePath SciTEGTK::GetSciteDefaultHome() {
	const char *where = getenv("SciTE_HOME");
#ifdef SYSCONF_PATH
	if (!where) {
		where = SYSCONF_PATH;
	}
#else
	if (!where) {
		where = getenv("HOME");
	}
#endif
	if (where) {
		return FilePath(where);

	}
	return FilePath("");
}

FilePath SciTEGTK::GetSciteUserHome() {
	char *where = getenv("SciTE_HOME");
	if (!where) {
		where = getenv("HOME");
	}
	return FilePath(where);
}

void SciTEGTK::ShowFileInStatus() {
	char sbText[1000];
	sprintf(sbText, " File: ");
	if (filePath.IsUntitled())
		strcat(sbText, "Untitled");
	else
		strcat(sbText, filePath.AsInternal());
	SetStatusBarText(sbText);
}

void SciTEGTK::SetWindowName() {
	SciTEBase::SetWindowName();
	ShowFileInStatus();
}

void SciTEGTK::SetStatusBarText(const char *s) {
	gtk_statusbar_pop(GTK_STATUSBAR(PWidget(wStatusBar)), sbContextID);
	gtk_statusbar_push(GTK_STATUSBAR(PWidget(wStatusBar)), sbContextID, s);
}

void SciTEGTK::TabInsert(int index, const GUI::gui_char *title) {
	if (wTabBar.GetID()) {
		GtkWidget *tablabel = gtk_label_new(title);
		GtkWidget *tabcontent;
		if (props.GetInt("pathbar.visible")) {
			if (buffers.buffers[index].IsUntitled())
				tabcontent = gtk_label_new(localiser.Text("Untitled").c_str());
			else
				tabcontent = gtk_label_new(buffers.buffers[index].AsInternal());
		} else {
			// No path bar
			tabcontent = gtk_image_new();
		}

		gtk_widget_show(tablabel);
		gtk_widget_show(tabcontent);

		gtk_notebook_append_page(GTK_NOTEBOOK(wTabBar.GetID()), tabcontent, tablabel);
	}
}

void SciTEGTK::TabSelect(int index) {
	if (wTabBar.GetID())
		gtk_notebook_set_page(GTK_NOTEBOOK(wTabBar.GetID()), index);
}

void SciTEGTK::RemoveAllTabs() {
	if (wTabBar.GetID()) {
		while (gtk_notebook_get_nth_page(GTK_NOTEBOOK(wTabBar.GetID()), 0))
			gtk_notebook_remove_page(GTK_NOTEBOOK(wTabBar.GetID()), 0);
	}
}

void SciTEGTK::SetFileProperties(PropSetFile &ps) {
	// Could use Unix standard calls here, someone less lazy than me (PL) should do it.
	ps.Set("FileTime", "");
	ps.Set("FileDate", "");
	ps.Set("FileAttr", "");
	ps.Set("CurrentDate", "");
	ps.Set("CurrentTime", "");
}

void SciTEGTK::UpdateStatusBar(bool bUpdateSlowData) {
	SciTEBase::UpdateStatusBar(bUpdateSlowData);
}

void SciTEGTK::Notify(SCNotification *notification) {
	SciTEBase::Notify(notification);
}

void SciTEGTK::ShowToolBar() {
	if (GTK_TOOLBAR(PWidget(wToolBar))->num_children < 1) {
		AddToolBar();
	}

	if (tbVisible) {
		if (toolbarDetachable == 1) {
			gtk_widget_show(GTK_WIDGET(PWidget(wToolBarBox)));
		} else {
			gtk_widget_show(GTK_WIDGET(PWidget(wToolBar)));
		}
	} else {
		if (toolbarDetachable == 1) {
			gtk_widget_hide(GTK_WIDGET(PWidget(wToolBarBox)));
		} else {
			gtk_widget_hide(GTK_WIDGET(PWidget(wToolBar)));
		}
	}
}

void SciTEGTK::ShowTabBar() {
	if (tabVisible && (!tabHideOne || buffers.length > 1) && buffers.size>1) {
		gtk_widget_show(GTK_WIDGET(PWidget(wTabBar)));
	} else {
		gtk_widget_hide(GTK_WIDGET(PWidget(wTabBar)));
	}
}

void SciTEGTK::ShowStatusBar() {
	if (sbVisible) {
		gtk_widget_show(GTK_WIDGET(PWidget(wStatusBar)));
	} else {
		gtk_widget_hide(GTK_WIDGET(PWidget(wStatusBar)));
	}
}

void SciTEGTK::ActivateWindow(const char *timestamp) {
	char *end;
	// Reset errno so we can reliably test it
	errno = 0;
	gulong ts = strtoul(timestamp, &end, 0);
	if (end != timestamp && errno == 0) {
#if GTK_CHECK_VERSION(2,8,0)
		gtk_window_present_with_time(GTK_WINDOW(PWidget(wSciTE)), ts);
#endif
	} else {
		gtk_window_present(GTK_WINDOW(PWidget(wSciTE)));
	}
}

void SciTEGTK::CopyPath() {
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
		filePath.AsInternal(), -1);
}

// The find and replace dialogs and strips often manipulate boolean
// flags based on dialog control IDs and menu IDs.
bool &SciTEGTK::FlagFromCmd(int cmd) {
	static bool notFound;
	switch (cmd) {
		case IDWHOLEWORD:
		case IDM_WHOLEWORD:
			return wholeWord;
		case IDMATCHCASE:
		case IDM_MATCHCASE:
			return matchCase;
		case IDREGEXP:
		case IDM_REGEXP:
			return regExp;
		case IDUNSLASH:
		case IDM_UNSLASH:
			return unSlash;
		case IDWRAP:
		case IDM_WRAPAROUND:
			return wrapFind;
		case IDDIRECTIONUP:
		case IDM_DIRECTIONUP:
			return reverseFind;
	}
	return notFound;
}

void SciTEGTK::Command(unsigned long wParam, long) {
	int cmdID = ControlIDOfCommand(wParam);
	int notifyCode = wParam >> 16;
	switch (cmdID) {

	case IDM_SRCWIN:
		if (notifyCode == SCEN_SETFOCUS) {
			Activate(true);
			CheckMenus();
		} else if (notifyCode == SCEN_KILLFOCUS) {
			Activate(false);
		}
		break;

	case IDM_RUNWIN:
		if (notifyCode == SCEN_SETFOCUS)
			CheckMenus();
		break;

	case IDM_FULLSCREEN:
		fullScreen = !fullScreen;
		{
			GdkWindow *parent_w = PWidget(wSciTE)->window;
			if (fullScreen)
				gdk_window_fullscreen(parent_w);
			else
				gdk_window_unfullscreen(parent_w);
		}
		SizeSubWindows();
		CheckMenus();
		break;

	default:
		SciTEBase::MenuCommand(cmdID, menuSource);
		menuSource = 0;
	}
	UpdateStatusBar(true);
}

void SciTEGTK::ReadLocalization() {
	SciTEBase::ReadLocalization();
	SString encoding = localiser.Get("translation.encoding");
	if (encoding.length()) {
		GIConv iconvh = g_iconv_open("UTF-8", encoding.c_str());
		const char *key = NULL;
		const char *val = NULL;
		// Get encoding
		bool more = localiser.GetFirst(key, val);
		while (more) {
			char converted[1000];
			converted[0] = '\0';
			// Cast needed for some versions of iconv not marking input argument as const
			char *pin = const_cast<char *>(val);
			size_t inLeft = strlen(val);
			char *pout = converted;
			size_t outLeft = sizeof(converted);
			size_t conversions = g_iconv(iconvh, &pin, &inLeft, &pout, &outLeft);
			if (conversions != ((size_t)(-1))) {
				*pout = '\0';
				localiser.Set(key, converted);
			}
			more = localiser.GetNext(key, val);
		}
		g_iconv_close(iconvh);
	}
}

void SciTEGTK::ReadPropertiesInitial() {
	SciTEBase::ReadPropertiesInitial();
	ShowToolBar();
	ShowTabBar();
	ShowStatusBar();
}

void SciTEGTK::ReadProperties() {
	SciTEBase::ReadProperties();

	CallChildren(SCI_SETRECTANGULARSELECTIONMODIFIER,
		props.GetInt("rectangular.selection.modifier", SCMOD_CTRL));

	CheckMenus();

	// Need this here to handle tabbar.hide.one properly
	ShowTabBar();
}

void SciTEGTK::GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize) {
	gtk_window_get_position(GTK_WINDOW(PWidget(wSciTE)), left, top);
	gtk_window_get_size(GTK_WINDOW(PWidget(wSciTE)), width, height);
	*maximize = (gdk_window_get_state(PWidget(wSciTE)->window) & GDK_WINDOW_STATE_MAXIMIZED) != 0;
}

void SciTEGTK::SizeContentWindows() {
	GUI::Rectangle rcClient = GetClientRectangle();
	int left = rcClient.left;
	int top = rcClient.top;
	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	heightOutput = NormaliseSplit(heightOutput);
	if (splitVertical) {
		wEditor.SetPosition(GUI::Rectangle(left, top, w - heightOutput - heightBar + left, h + top));
		wDivider.SetPosition(GUI::Rectangle(w - heightOutput - heightBar + left, top, w - heightOutput + left, h + top));
		wOutput.SetPosition(GUI::Rectangle(w - heightOutput + left, top, w + left, h + top));
	} else {
		wEditor.SetPosition(GUI::Rectangle(left, top, w + left, h - heightOutput - heightBar + top));
		wDivider.SetPosition(GUI::Rectangle(left, h - heightOutput - heightBar + top, w + left, h - heightOutput + top));
		wOutput.SetPosition(GUI::Rectangle(left, h - heightOutput + top, w + left, h + top));
	}
}

void SciTEGTK::SizeSubWindows() {
	SizeContentWindows();
}

void SciTEGTK::SetMenuItem(int, int, int itemID, const char *text, const char *mnemonic) {
	DestroyMenuItem(0, itemID);

	// On GTK+ the menuNumber and position are ignored as the menu item already exists and is in the right
	// place so only needs to be shown and have its text set.

	SString itemText(text);
	// Remove accelerator as does not work.
	itemText.remove("&");

	long keycode = 0;
	if (mnemonic && *mnemonic) {
		keycode = SciTEKeys::ParseKeyCode(mnemonic);
		if (keycode) {
			itemText += " ";
			itemText += mnemonic;
		}
		// the keycode could be used to make a custom accelerator table
		// but for now, the menu's item data is used instead for command
		// tools, and for other menu entries it is just discarded.
	}

	// Reorder shift and ctrl indicators for compatibility with other menus
	itemText.substitute("Ctrl+Shift+", "Shift+Ctrl+");

	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, itemID);
	if (item) {
		GList *al = gtk_container_children(GTK_CONTAINER(item));
		for (unsigned int ii = 0; ii < g_list_length(al); ii++) {
			gpointer d = g_list_nth(al, ii);
			GtkWidget **w = (GtkWidget **)d;
			gtk_label_set_text(GTK_LABEL(*w), itemText.c_str());
			// Have not managed to make accelerator work
			//guint key = gtk_label_parse_uline(GTK_LABEL(*w), itemText);
			//gtk_widget_add_accelerator(*w, "clicked", accelGroup,
			//           key, 0, (GtkAccelFlags)0);
		}
		g_list_free(al);
		gtk_widget_show(item);

		if (itemID >= IDM_TOOLS && itemID < IDM_TOOLS + toolMax) {
			// Stow the keycode for later retrieval.
			// Do this even if 0, in case the menu already existed (e.g. ModifyMenu)
			gtk_object_set_user_data(GTK_OBJECT(item), reinterpret_cast<gpointer>(keycode));
		}
	}
}

void SciTEGTK::DestroyMenuItem(int, int itemID) {
	// On GTK+ menu items are just hidden rather than destroyed as they can not be recreated in the middle of a menu
	// The menuNumber is ignored as all menu items in GTK+ can be found from the root of the menu tree

	if (itemID) {
		GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, itemID);

		if (item) {
			gtk_widget_hide(item);
			gtk_object_set_user_data(GTK_OBJECT(item), 0);
		}
	}
}

void SciTEGTK::CheckAMenuItem(int wIDCheckItem, bool val) {
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, wIDCheckItem);
	allowMenuActions = false;
	if (item)
		gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(item), val ? TRUE : FALSE);
	allowMenuActions = true;
}

void SciTEGTK::EnableAMenuItem(int wIDCheckItem, bool val) {
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, wIDCheckItem);
	if (item) {
		if (GTK_IS_WIDGET(item))
			gtk_widget_set_sensitive(item, val);

	}
}

void SciTEGTK::CheckMenusClipboard() {
	if (StripHasFocus()) {
		EnableAMenuItem(IDM_CUT, false);
		EnableAMenuItem(IDM_COPY, false);
		EnableAMenuItem(IDM_CLEAR, false);
		EnableAMenuItem(IDM_PASTE, false);
	} else {
		SciTEBase::CheckMenusClipboard();
	}
}

void SciTEGTK::CheckMenus() {
	SciTEBase::CheckMenus();

	CheckAMenuItem(IDM_EOL_CRLF, wEditor.Call(SCI_GETEOLMODE) == SC_EOL_CRLF);
	CheckAMenuItem(IDM_EOL_CR, wEditor.Call(SCI_GETEOLMODE) == SC_EOL_CR);
	CheckAMenuItem(IDM_EOL_LF, wEditor.Call(SCI_GETEOLMODE) == SC_EOL_LF);

	CheckAMenuItem(IDM_ENCODING_DEFAULT, CurrentBuffer()->unicodeMode == uni8Bit);
	CheckAMenuItem(IDM_ENCODING_UCS2BE, CurrentBuffer()->unicodeMode == uni16BE);
	CheckAMenuItem(IDM_ENCODING_UCS2LE, CurrentBuffer()->unicodeMode == uni16LE);
	CheckAMenuItem(IDM_ENCODING_UTF8, CurrentBuffer()->unicodeMode == uniUTF8);
	CheckAMenuItem(IDM_ENCODING_UCOOKIE, CurrentBuffer()->unicodeMode == uniCookie);

	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	CheckAMenuItem(IDM_VIEWTABBAR, tabVisible);

	if (btnBuild) {
		gtk_widget_set_sensitive(btnBuild, !jobQueue.IsExecuting());
		gtk_widget_set_sensitive(btnCompile, !jobQueue.IsExecuting());
		gtk_widget_set_sensitive(btnStop, jobQueue.IsExecuting());
	}
}

/**
 * Replace any %xx escapes by their single-character equivalent.
 */
static void unquote(char *s) {
	char *o = s;
	while (*s) {
		if ((*s == '%') && s[1] && s[2]) {
			*o = IntFromHexDigit(s[1]) * 16 + IntFromHexDigit(s[2]);
			s += 2;
		} else {
			*o = *s;
		}
		o++;
		s++;
	}
	*o = '\0';
}

/**
 * Open a list of URIs each terminated by "\r\n".
 * Only "file:" URIs currently understood.
 * In KDE 4, the last URI is not terminated by "\r\n"!
 */
void SciTEGTK::OpenUriList(const char *list) {
	if (list) {
		char *uri = StringDup(list);
		char *lastenduri = uri + strlen(uri);
		if (uri) {
			while (uri < lastenduri) {
				char *enduri = strchr(uri, '\r');
				if (enduri == NULL)
					enduri = lastenduri;	// if last URI has no "\r\n".
				*enduri = '\0';
				if (isprefix(uri, "file:")) {
					uri += strlen("file:");
					if (isprefix(uri, "///")) {
						uri += 2;	// There can be an optional // before the file path that starts with /
					}

					unquote(uri);
					Open(uri);
				} else {
					GUI::gui_string msg = LocaliseMessage("URI '^0' not understood.", uri);
					WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
				}

				uri = enduri + 1;
				if (*uri == '\n')
					uri++;
			}
		}
	}
}

bool SciTEGTK::OpenDialog(FilePath directory, const char *filter) {
	directory.SetWorkingDirectory();
	bool canceled = true;
	if (!dlgFileSelector.Created()) {
		GtkWidget *dlg = gtk_file_chooser_dialog_new(
					localiser.Text("Open File").c_str(),
				      GTK_WINDOW(wSciTE.GetID()),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
		gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), TRUE);
		gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
		if (props.GetInt("open.dialog.in.file.directory")) {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg),
				filePath.Directory().AsInternal());
		}

		// Add a show hidden files toggle
		GtkWidget *toggle = gtk_check_button_new_with_label(
			localiser.Text("Show hidden files").c_str());
		gtk_widget_show(toggle);
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dlg), toggle);
		g_signal_connect(toggle, "toggled",
			G_CALLBACK(toggle_hidden_cb), GTK_DIALOG(dlg));
		if (props.GetInt("fileselector.show.hidden"))
			g_object_set(GTK_OBJECT(toggle), "active", TRUE, NULL);

		SString openFilter = filter;
		if (openFilter.length()) {
			openFilter.substitute('|', '\0');
			size_t start = 0;
			while (start < openFilter.length()) {
				const char *filterName = openFilter.c_str() + start;
				GUI::gui_string localised = localiser.Text(filterName, false);
				if (localised.length()) {
					openFilter.remove(start, strlen(filterName));
					openFilter.insert(start, localised.c_str());
				}
				if (openFilter.c_str()[start] == '#') {
					start += strlen(openFilter.c_str() + start) + 1;
				} else {
					GtkFileFilter *filter = gtk_file_filter_new();
					gtk_file_filter_set_name(filter, openFilter.c_str() + start);
					start += strlen(openFilter.c_str() + start) + 1;
					SString oneSet(openFilter.c_str() + start);
					oneSet.substitute(';', '\0');
					size_t item = 0;
					while (item < oneSet.length()) {
						gtk_file_filter_add_pattern(filter, oneSet.c_str() + item);
						item += strlen(oneSet.c_str() + item) + 1;
					}
					gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filter);
				}
				start += strlen(openFilter.c_str() + start) + 1;
			}
		}

		gtk_window_set_default_size(GTK_WINDOW(dlg), fileSelectorWidth, fileSelectorHeight);
		if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
			GSList *names = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dlg));
			GSList *nameCurrent = names;
			while (nameCurrent) {
				char *filename = static_cast<char *>(nameCurrent->data);
				Open(filename);
				g_free(filename);
				nameCurrent = g_slist_next(nameCurrent);
			}
			g_slist_free(names);
			canceled = false;
		}
		gtk_widget_destroy(dlg);
	}
	return !canceled;
}

// Callback function to show hidden files in filechooser
void SciTEGTK::toggle_hidden_cb(GtkToggleButton *toggle, gpointer data) {
	GtkWidget *file_chooser = GTK_WIDGET(data);

	if (gtk_toggle_button_get_active(toggle))
		g_object_set(GTK_FILE_CHOOSER(file_chooser), "show-hidden", TRUE, NULL);
	else
		g_object_set(GTK_FILE_CHOOSER(file_chooser), "show-hidden", FALSE, NULL);
}

bool SciTEGTK::HandleSaveAs(const char *savePath) {
	switch (saveFormat) {
	case sfCopy:
		SaveBuffer(savePath);
		break;
	case sfHTML:
		SaveToHTML(savePath);
		break;
	case sfRTF:
		SaveToRTF(savePath);
		break;
	case sfPDF:
		SaveToPDF(savePath);
		break;
	case sfTEX:
		SaveToTEX(savePath);
		break;
	case sfXML:
		SaveToXML(savePath);
		break;
	default: {
			/* Checking that no other buffer refers to the same filename */
			FilePath destFile(savePath);
			return SaveIfNotOpen(destFile, true);
		}
	}
	dlgFileSelector.OK();
	return true;
}

bool SciTEGTK::SaveAsXXX(FileFormat fmt, const char *title, const char *ext) {
	filePath.SetWorkingDirectory();
	bool saved = false;
	saveFormat = fmt;
	if (!dlgFileSelector.Created()) {
		GtkWidget *dlg = gtk_file_chooser_dialog_new(
					localiser.Text(title).c_str(),
				      GTK_WINDOW(wSciTE.GetID()),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
		FilePath savePath = SaveName(ext);
		if (ext) {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), savePath.Directory().AsInternal());
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), savePath.Name().AsInternal());
		} else if (savePath.IsUntitled()) { // saving 'untitled'
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), savePath.Directory().AsInternal());
		} else {
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), savePath.AsInternal());
		}

		if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
			char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
			gtk_widget_destroy(dlg);
			saved = HandleSaveAs(filename);
			g_free(filename);
		} else {
			gtk_widget_destroy(dlg);
		}
	}
	return saved;
}

bool SciTEGTK::SaveAsDialog() {
	return SaveAsXXX(sfSource, "Save File As");
}

void SciTEGTK::SaveACopy() {
	SaveAsXXX(sfCopy, "Save a Copy");
}

void SciTEGTK::SaveAsHTML() {
	SaveAsXXX(sfHTML, "Export File As HTML", ".html");
}

void SciTEGTK::SaveAsRTF() {
	SaveAsXXX(sfRTF, "Export File As RTF", ".rtf");
}

void SciTEGTK::SaveAsPDF() {
	SaveAsXXX(sfPDF, "Export File As PDF", ".pdf");
}

void SciTEGTK::SaveAsTEX() {
	SaveAsXXX(sfTEX, "Export File As LaTeX", ".tex");
}

void SciTEGTK::SaveAsXML() {
	SaveAsXXX(sfXML, "Export File As XML", ".xml");
}

void SciTEGTK::LoadSessionDialog() {
	filePath.SetWorkingDirectory();
	if (!dlgFileSelector.Created()) {
		GtkWidget *dlg = gtk_file_chooser_dialog_new(
					localiser.Text("Load Session").c_str(),
				      GTK_WINDOW(wSciTE.GetID()),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

		gtk_window_set_default_size(GTK_WINDOW(dlg), fileSelectorWidth, fileSelectorHeight);
		if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
			char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));

			LoadSessionFile(filename);
			RestoreSession();
			g_free(filename);
		}
		gtk_widget_destroy(dlg);
	}
}

void SciTEGTK::SaveSessionDialog() {
	filePath.SetWorkingDirectory();
	if (!dlgFileSelector.Created()) {
		GtkWidget *dlg = gtk_file_chooser_dialog_new(
					localiser.Text("Save Session").c_str(),
				      GTK_WINDOW(wSciTE.GetID()),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);

		if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
			char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));

			SaveSessionFile(filename);
			g_free(filename);
		}
		gtk_widget_destroy(dlg);
	}
}

void SciTEGTK::Print(bool) {
	RemoveFindMarks();
	SelectionIntoProperties();
	AddCommand(props.GetWild("command.print.", filePath.AsInternal()), "",
	           SubsystemType("command.print.subsystem."));
	if (jobQueue.commandCurrent > 0) {
		jobQueue.isBuilding = true;
		Execute();
	}
}

void SciTEGTK::PrintSetup() {
	// Printing not yet supported on GTK+
}

SString SciTEGTK::GetRangeInUIEncoding(GUI::ScintillaWindow &win, int selStart, int selEnd) {
	int len = selEnd - selStart;
	SBuffer allocation(len * 3);
	win.Call(SCI_SETTARGETSTART, selStart);
	win.Call(SCI_SETTARGETEND, selEnd);
	int byteLength = win.Call(SCI_TARGETASUTF8, 0, reinterpret_cast<sptr_t>(allocation.ptr()));
	SString sel(allocation);
	sel.remove(byteLength, 0);
	return sel;
}

void SciTEGTK::HandleFindReplace() {}

void SciTEGTK::Find() {
	if (dlgFindReplace.Created()) {
		dlgFindReplace.Present();
		return;
	}
	SelectionIntoFind();
	if (props.GetInt("find.use.strip")) {
		replaceStrip.Close();
		findStrip.Show();
	} else {
		if (findStrip.visible || replaceStrip.visible)
			return;
		FindReplace(false);
	}
}

void SciTEGTK::TranslatedSetTitle(GtkWindow *w, const char *original) {
	gtk_window_set_title(w, localiser.Text(original).c_str());
}

GtkWidget *SciTEGTK::TranslatedLabel(const char *original) {
	GUI::gui_string text = localiser.Text(original);
	return gtk_label_new_with_mnemonic(text.c_str());
}

static void FillComboFromMemory(GtkWidget *combo, const ComboMemory &mem, bool useTop = false) {
	for (int i = 0; i < 10; i++) {
		gtk_combo_box_remove_text(GTK_COMBO_BOX(combo), 0);
	}
	for (int i = 0; i < mem.Length(); i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), mem.At(i).c_str());
	}
	if (useTop) {
		GtkWidget* entry = gtk_bin_get_child(GTK_BIN(combo));
		gtk_entry_set_text(GTK_ENTRY(entry), mem.At(0).c_str());
	}
}

SString SciTEGTK::EncodeString(const SString &s) {
	wEditor.Call(SCI_SETLENGTHFORENCODE, s.length());
	int len = wEditor.Call(SCI_ENCODEDFROMUTF8,
		reinterpret_cast<uptr_t>(s.c_str()), 0);
	SBuffer ret(len);
	wEditor.CallString(SCI_ENCODEDFROMUTF8,
		reinterpret_cast<uptr_t>(s.c_str()), ret.ptr());
	return SString(ret);
}

void SciTEGTK::FindReplaceGrabFields() {
	const char *findEntry = gtk_entry_get_text(comboFind.Entry());
	findWhat = findEntry;
	memFinds.Insert(findWhat);
	if (comboReplace) {
		const char *replaceEntry = gtk_entry_get_text(comboReplace.Entry());
		replaceWhat = replaceEntry;
		memReplaces.Insert(replaceWhat);
	}
	wholeWord = GTK_TOGGLE_BUTTON(toggleWord)->active;
	matchCase = GTK_TOGGLE_BUTTON(toggleCase)->active;
	regExp = GTK_TOGGLE_BUTTON(toggleRegExp)->active;
	wrapFind = GTK_TOGGLE_BUTTON(toggleWrap)->active;
	unSlash = GTK_TOGGLE_BUTTON(toggleUnSlash)->active;
	reverseFind = GTK_TOGGLE_BUTTON(toggleReverse)->active;
}

void SciTEGTK::FRCancelCmd() {
	dlgFindReplace.Destroy();
}

gint SciTEGTK::FRKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->dlgFindReplace.Destroy();
	}
	return FALSE;
}

void SciTEGTK::FRFindCmd() {
	FindReplaceGrabFields();
	bool isFindDialog = !comboReplace;
	if (isFindDialog)
		dlgFindReplace.Destroy();
	if (findWhat[0]) {
		FindNext(isFindDialog && reverseFind);
	}
}

void SciTEGTK::FRReplaceCmd() {
	FindReplaceGrabFields();
	if (findWhat[0]) {
		ReplaceOnce();
	}
}

void SciTEGTK::FRReplaceAllCmd() {
	FindReplaceGrabFields();
	if (findWhat[0]) {
		ReplaceAll(false);
		dlgFindReplace.Destroy();
	}
}

void SciTEGTK::FRReplaceInSelectionCmd() {
	FindReplaceGrabFields();
	if (findWhat[0]) {
		ReplaceAll(true);
		dlgFindReplace.Destroy();
	}
}

void SciTEGTK::FRReplaceInBuffersCmd() {
	FindReplaceGrabFields();
	if (findWhat[0]) {
		ReplaceInBuffers();
		dlgFindReplace.Destroy();
	}
}

void SciTEGTK::FRMarkAllCmd() {
	FindReplaceGrabFields();
	MarkAll();
	FindNext(reverseFind);
}

void SciTEGTK::FindInFilesCmd() {
	const char *findEntry = gtk_entry_get_text(comboFindInFiles.Entry());
	props.Set("find.what", findEntry);
	memFinds.Insert(findEntry);

	const char *dirEntry = gtk_entry_get_text(comboDir.Entry());
	props.Set("find.directory", dirEntry);
	memDirectory.Insert(dirEntry);

	const char *filesEntry = gtk_entry_get_text(comboFiles.Entry());
	props.Set("find.files", filesEntry);
	memFiles.Insert(filesEntry);

	wholeWord = GTK_TOGGLE_BUTTON(toggleWord)->active;
	matchCase = GTK_TOGGLE_BUTTON(toggleCase)->active;

	dlgFindInFiles.Destroy();

	//printf("Grepping for <%s> in <%s>\n",
	//	props.Get("find.what"),
	//	props.Get("find.files"));
	SelectionIntoProperties();
	SString findCommand = props.GetNewExpand("find.command");
	if (findCommand == "") {
		findCommand = sciteExecutable.AsInternal();
		findCommand += " -grep ";
		findCommand += wholeWord ? "w" : "~";
		findCommand += matchCase ? "c" : "~";
		findCommand += props.GetInt("find.in.dot") ? "d" : "~";
		findCommand += props.GetInt("find.in.binary") ? "b" : "~";
		findCommand += " \"";
		findCommand += props.Get("find.files");
		findCommand += "\" \"";
		char *quotedForm = Slash(props.Get("find.what").c_str(), true);
		findCommand += quotedForm;
		findCommand += "\"";
		delete []quotedForm;
		//~ fprintf(stderr, "%s\n", findCommand.c_str());
	}
	AddCommand(findCommand, props.Get("find.directory"), jobCLI);
	if (jobQueue.commandCurrent > 0)
		Execute();
}

void SciTEGTK::FindInFilesDotDot() {
	FilePath findInDir(gtk_entry_get_text(comboDir.Entry()));
	gtk_entry_set_text(comboDir.Entry(), findInDir.Directory().AsInternal());
}

void SciTEGTK::FindInFilesBrowse() {
	FilePath findInDir(gtk_entry_get_text(comboDir.Entry()));
	GtkWidget *dialog = gtk_file_chooser_dialog_new(
							localiser.Text("Select a folder to search from").c_str(),
							GTK_WINDOW(dlgFindInFiles.GetID()), // parent_window,
							GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), findInDir.AsInternal());

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_entry_set_text(comboDir.Entry(), filename);
		g_free(filename);
	}

	gtk_widget_destroy(dialog);
}

static const char *searchText = "Fi_nd:";
static const char *replaceText = "Rep_lace:";

struct Toggle {
	enum { tWord, tCase, tRegExp, tBackslash, tWrap, tUp };
	const char *label;
	int cmd;
	int id;
};

const static Toggle toggles[] = {
	{"Match _whole word only", IDM_WHOLEWORD, IDWHOLEWORD},
	{"Match _case", IDM_MATCHCASE, IDMATCHCASE},
	{"Regular _expression", IDM_REGEXP, IDREGEXP},
	{"Transform _backslash expressions", IDM_UNSLASH, IDUNSLASH},
	{"Wrap ar_ound", IDM_WRAPAROUND, IDWRAP},
	{"_Up", IDM_DIRECTIONUP, IDDIRECTIONUP},
	{0, 0, 0},
};

class Table {
private:
	GtkWidget *table;
	int rows;
	int columns;
	int next;
public:
	Table(int rows_, int columns_) :
		table(0), rows(rows_), columns(columns_), next(0) {
		table = gtk_table_new(rows, columns, FALSE);
	}
	void Add(GtkWidget *child=0, int width=1, bool expand=false,
		int xpadding=5, int ypadding=5) {
		GtkAttachOptions opts = static_cast<GtkAttachOptions>(
			GTK_SHRINK | GTK_FILL);
		GtkAttachOptions optsExpand = static_cast<GtkAttachOptions>(
			GTK_SHRINK | GTK_FILL | GTK_EXPAND);

		if (child) {
			gtk_table_attach(GTK_TABLE(table), child,
				next % columns, next % columns + width,
				next / columns, (next / columns) + 1,
				expand ? optsExpand : opts, opts,
				xpadding, ypadding);
		}
		next += width;
	}
	void Label(GtkWidget *child) {
		gtk_misc_set_alignment(GTK_MISC(child), 1.0, 0.5);
		Add(child);
	}
	void PackInto(GtkBox *box, gboolean expand=TRUE) {
		gtk_box_pack_start(box, table, expand, expand, 0);
	}
	GtkWidget *Widget() const {
		return table;
	}
};

void SciTEGTK::FindInFilesResponse(int responseID) {
	switch (responseID) {
		case GTK_RESPONSE_OK:
			FindInFilesCmd() ;
			break;

		case GTK_RESPONSE_CANCEL:
			dlgFindInFiles.Destroy();
			break;
	}
}

void SciTEGTK::FindInFiles() {
	SelectionIntoFind();
	props.Set("find.what", findWhat.c_str());

	FilePath findInDir = filePath.Directory().AbsolutePath();
	props.Set("find.directory", findInDir.AsInternal());

	dlgFindInFiles.Create(this, "Find in Files", &localiser);

	Table table(4, 5);
	table.PackInto(GTK_BOX(GTK_DIALOG(PWidget(dlgFindInFiles))->vbox));

	GtkWidget *labelFind = TranslatedLabel("Fi_nd what:");
	table.Label(labelFind);

	comboFindInFiles.Create();

	FillComboFromMemory(comboFindInFiles, memFinds);

	table.Add(comboFindInFiles, 4, true);

	gtk_entry_set_text(comboFindInFiles.Entry(), findWhat.c_str());
	gtk_entry_select_region(comboFindInFiles.Entry(), 0, findWhat.length());
	gtk_entry_set_activates_default(comboFindInFiles.Entry(), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelFind), comboFindInFiles);

	GtkWidget *labelFiles = TranslatedLabel("_Files:");
	table.Label(labelFiles);

	comboFiles.Create();
	FillComboFromMemory(comboFiles, memFiles, true);

	table.Add(comboFiles, 4, true);
	gtk_entry_set_activates_default(comboFiles.Entry(), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelFiles), comboFiles);

	GtkWidget *labelDirectory = TranslatedLabel("_Directory:");
	table.Label(labelDirectory);

	comboDir.Create();
	FillComboFromMemory(comboDir, memDirectory);
	table.Add(comboDir, 2, true);

	gtk_entry_set_text(comboDir.Entry(), findInDir.AsInternal());
	// Make a little wider than would happen automatically to show realistic paths
	gtk_entry_set_width_chars(comboDir.Entry(), 40);
	gtk_entry_set_activates_default(comboDir.Entry(), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelDirectory), comboDir);

	Signal<&SciTEGTK::FindInFilesDotDot> sigDotDot;
	GtkWidget *btnDotDot = dlgFindInFiles.Button("_..", sigDotDot.Function);
	table.Add(btnDotDot);

	Signal<&SciTEGTK::FindInFilesBrowse> sigBrowse;
	GtkWidget *btnBrowse = dlgFindInFiles.Button("_Browse...", sigBrowse.Function);
	table.Add(btnBrowse);

	table.Add();	// Space

	bool enableToggles = props.GetNewExpand("find.command") == "";

	// Whole Word
	toggleWord = dlgFindInFiles.Toggle(toggles[Toggle::tWord].label, wholeWord && enableToggles);
	gtk_widget_set_sensitive(toggleWord, enableToggles);
	table.Add(toggleWord, 1, true, 3, 0);

	// Case Sensitive
	toggleCase = dlgFindInFiles.Toggle(toggles[Toggle::tCase].label, matchCase || !enableToggles);
	gtk_widget_set_sensitive(toggleCase, enableToggles);
	table.Add(toggleCase, 1, true, 3, 0);

	AttachResponse<&SciTEGTK::FindInFilesResponse>(PWidget(dlgFindInFiles), this);
	dlgFindInFiles.ResponseButton("_Cancel", GTK_RESPONSE_CANCEL);
	dlgFindInFiles.ResponseButton("F_ind", GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(PWidget(dlgFindInFiles)), GTK_RESPONSE_OK);

	gtk_widget_grab_focus(GTK_WIDGET(comboFindInFiles.Entry()));

	dlgFindInFiles.Display(PWidget(wSciTE));
}

void SciTEGTK::Replace() {
	if (dlgFindReplace.Created()) {
		dlgFindReplace.Present();
		return;
	}
	SelectionIntoFind();
	if (props.GetInt("replace.use.strip")) {
		findStrip.Close();
		replaceStrip.Show();
	} else {
		if (findStrip.visible || replaceStrip.visible)
			return;
		FindReplace(true);
	}
}

void SciTEGTK::ResetExecution() {
	icmd = 0;
	jobQueue.SetExecuting(false);
	if (needReadProperties)
		ReadProperties();
	CheckReload();
	CheckMenus();
	ClearJobQueue();
}

void SciTEGTK::ExecuteNext() {
	icmd++;
	if (icmd < jobQueue.commandCurrent && icmd < jobQueue.commandMax) {
		Execute();
	} else {
		ResetExecution();
	}
}

void SciTEGTK::ContinueExecute(int fromPoll) {
	char buf[8192];
	int count = read(fdFIFO, buf, sizeof(buf) - 1);
	if (count > 0) {
		buf[count] = '\0';
		OutputAppendString(buf);
		lastOutput += buf;
	} else if (count == 0) {
		SString sExitMessage(WEXITSTATUS(exitStatus));
		sExitMessage.insert(0, ">Exit code: ");
		if (WIFSIGNALED(exitStatus)) {
			SString sSignal(WTERMSIG(exitStatus));
			sSignal.insert(0, " Signal: ");
			sExitMessage += sSignal;
		}
		if (jobQueue.TimeCommands()) {
			sExitMessage += "    Time: ";
			sExitMessage += SString(commandTime.Duration(), 3);
		}
		if ((lastFlags & jobRepSelYes)
			|| ((lastFlags & jobRepSelAuto) && !exitStatus)) {
			int cpMin = wEditor.Send(SCI_GETSELECTIONSTART, 0, 0);
			wEditor.Send(SCI_REPLACESEL,0,(sptr_t)(lastOutput.c_str()));
			wEditor.Send(SCI_SETSEL, cpMin, cpMin+lastOutput.length());
		}
		sExitMessage.append("\n");
		OutputAppendString(sExitMessage.c_str());
		// Move selection back to beginning of this run so that F4 will go
		// to first error of this run.
		if ((scrollOutput == 1) && returnOutputToCommand)
			wOutput.Send(SCI_GOTOPOS, originalEnd);
		returnOutputToCommand = true;
		gdk_input_remove(inputHandle);
		inputHandle = 0;
		gtk_timeout_remove(pollID);
		pollID = 0;
		close(fdFIFO);
		fdFIFO = 0;
		pidShell = 0;
		triedKill = false;
		if (WEXITSTATUS(exitStatus))
			ResetExecution();
		else
			ExecuteNext();
	} else { // count < 0
		// The FIFO is not ready - expected when called from polling callback.
		if (!fromPoll) {
			OutputAppendString(">End Bad\n");
		}
	}
}

void SciTEGTK::IOSignal(SciTEGTK *scitew) {
	scitew->ContinueExecute(FALSE);
}

int xsystem(const char *s, int fh) {
	int pid = 0;
	if ((pid = fork()) == 0) {
		for (int i=getdtablesize();i>=0;--i) if (i != fh) close(i);
		if (open("/dev/null", O_RDWR) >= 0) { // stdin
			if (dup(fh) >= 0) { // stdout
				if (dup(fh) >= 0) { // stderr
					close(fh);
					setpgid(0, 0);
					execlp("/bin/sh", "sh", "-c", s, static_cast<char *>(NULL));
				}
			}
		}
		_exit(127);
	}
	close(fh);
	return pid;
}

void SciTEGTK::Execute() {
	SciTEBase::Execute();

	commandTime.Duration(true);
	if (scrollOutput)
		wOutput.Send(SCI_GOTOPOS, wOutput.Send(SCI_GETTEXTLENGTH));
	originalEnd = wOutput.Send(SCI_GETCURRENTPOS);

	lastOutput = "";
	lastFlags = jobQueue.jobQueue[icmd].flags;

	if (jobQueue.jobQueue[icmd].jobType != jobExtension) {
		OutputAppendString(">");
		OutputAppendString(jobQueue.jobQueue[icmd].command.c_str());
		OutputAppendString("\n");
	}

	if (jobQueue.jobQueue[icmd].directory.IsSet()) {
		jobQueue.jobQueue[icmd].directory.SetWorkingDirectory();
	}

	if (jobQueue.jobQueue[icmd].jobType == jobShell) {
		if (fork() == 0)
			execlp("/bin/sh", "sh", "-c", jobQueue.jobQueue[icmd].command.c_str(),
				static_cast<char *>(NULL));
		else
			ExecuteNext();
	} else if (jobQueue.jobQueue[icmd].jobType == jobExtension) {
		if (extender)
			extender->OnExecute(jobQueue.jobQueue[icmd].command.c_str());
		ExecuteNext();
	} else {
		int pipefds[2];
		if (pipe(pipefds)) {
			OutputAppendString(">Failed to create FIFO\n");
			ExecuteNext();
			return;
		}

		pidShell = xsystem(jobQueue.jobQueue[icmd].command.c_str(), pipefds[1]);
		triedKill = false;
		fdFIFO = pipefds[0];
		fcntl(fdFIFO, F_SETFL, fcntl(fdFIFO, F_GETFL) | O_NONBLOCK);
		inputHandle = gdk_input_add(pipefds[0], GDK_INPUT_READ,
		                            (GdkInputFunction) IOSignal, this);
		// Also add a background task in case there is no output from the tool
		pollID = gtk_timeout_add(200, (gint (*)(void *)) SciTEGTK::PollTool, this);
	}
}

void SciTEGTK::StopExecute() {
	if (!triedKill && pidShell) {
		kill(-pidShell, SIGKILL);
		triedKill = true;
	}
}

static int EntryValue(GtkWidget *entry) {
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	return atoi(text);
}

void SciTEGTK::GotoCmd() {
	int lineNo = dlgGoto.entryGoto.Value();
	GotoLineEnsureVisible(lineNo - 1);
	dlgGoto.Destroy();
}

void SciTEGTK::GotoResponse(int responseID) {
	switch (responseID) {
		case GTK_RESPONSE_OK:
			GotoCmd() ;
			break;

		case GTK_RESPONSE_CANCEL:
			dlgGoto.Destroy();
			break;
	}
}

void SciTEGTK::GoLineDialog() {

	dlgGoto.Create(this, "Go To", &localiser);

	gtk_container_border_width(GTK_CONTAINER(PWidget(dlgGoto)), 0);

	Table table(1, 2);
	table.PackInto(GTK_BOX(GTK_DIALOG(PWidget(dlgGoto))->vbox));

	GtkWidget *labelGoto = TranslatedLabel("_Destination Line Number:");
	table.Label(labelGoto);

	dlgGoto.entryGoto.Create();
	table.Add(dlgGoto.entryGoto);
	dlgGoto.entryGoto.ActivatesDefault();
	gtk_widget_grab_focus(dlgGoto.entryGoto);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelGoto), dlgGoto.entryGoto);

	AttachResponse<&SciTEGTK::GotoResponse>(PWidget(dlgGoto), this);
	dlgGoto.ResponseButton("_Cancel", GTK_RESPONSE_CANCEL);
	dlgGoto.ResponseButton("_Go To", GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(PWidget(dlgGoto)), GTK_RESPONSE_OK);

	dlgGoto.Display(PWidget(wSciTE));
}

bool SciTEGTK::AbbrevDialog() { return false; }

void SciTEGTK::TabSizeSet(int &tabSize, bool &useTabs) {
	tabSize = EntryValue(entryTabSize);
	if (tabSize > 0)
		wEditor.Call(SCI_SETTABWIDTH, tabSize);
	int indentSize = EntryValue(entryIndentSize);
	if (indentSize > 0)
		wEditor.Call(SCI_SETINDENT, indentSize);
	useTabs = GTK_TOGGLE_BUTTON(toggleUseTabs)->active;
	wEditor.Call(SCI_SETUSETABS, useTabs);
}

void SciTEGTK::TabSizeCmd() {
	int tabSize;
	bool useTabs;
	TabSizeSet(tabSize, useTabs);
	dlgTabSize.Destroy();
}

void SciTEGTK::TabSizeConvertCmd() {
	int tabSize;
	bool useTabs;
	TabSizeSet(tabSize, useTabs);
	ConvertIndentation(tabSize, useTabs);
	dlgTabSize.Destroy();
}

#define RESPONSE_CONVERT 1001

void SciTEGTK::TabSizeResponse(int responseID) {
	switch (responseID) {
		case RESPONSE_CONVERT:
			TabSizeConvertCmd();
			break;

		case GTK_RESPONSE_OK:
			TabSizeCmd() ;
			break;

		case GTK_RESPONSE_CANCEL:
			dlgTabSize.Destroy();
			break;
	}
}

void SciTEGTK::TabSizeDialog() {

	dlgTabSize.Create(this, "Indentation Settings", &localiser);

	gtk_container_border_width(GTK_CONTAINER(PWidget(dlgTabSize)), 0);

	Table table(3, 2);
	table.PackInto(GTK_BOX(GTK_DIALOG(PWidget(dlgTabSize))->vbox));

	GtkWidget *labelTabSize = TranslatedLabel("_Tab Size:");
	table.Label(labelTabSize);

	entryTabSize = gtk_entry_new();
	table.Add(entryTabSize);
	gtk_entry_set_activates_default(GTK_ENTRY(entryTabSize), TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(entryTabSize));
	SString tabSize(wEditor.Call(SCI_GETTABWIDTH));
	gtk_entry_set_text(GTK_ENTRY(entryTabSize), tabSize.c_str());
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelTabSize), entryTabSize);

	GtkWidget *labelIndentSize = TranslatedLabel("_Indent Size:");
	table.Label(labelIndentSize);
	entryIndentSize = gtk_entry_new();
	table.Add(entryIndentSize);
	gtk_entry_set_activates_default(GTK_ENTRY(entryTabSize), TRUE);
	SString indentSize(wEditor.Call(SCI_GETINDENT));
	gtk_entry_set_text(GTK_ENTRY(entryIndentSize), indentSize.c_str());
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelIndentSize), entryIndentSize);

	bool useTabs = wEditor.Call(SCI_GETUSETABS);
	toggleUseTabs = dlgTabSize.Toggle("_Use Tabs", useTabs);
	table.Add();
	table.Add(toggleUseTabs);

	AttachResponse<&SciTEGTK::TabSizeResponse>(PWidget(dlgTabSize), this);
	dlgTabSize.ResponseButton("Con_vert", RESPONSE_CONVERT);
	dlgTabSize.ResponseButton("_Cancel", GTK_RESPONSE_CANCEL);
	dlgTabSize.ResponseButton("_OK", GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(PWidget(dlgTabSize)), GTK_RESPONSE_OK);

	dlgTabSize.Display(PWidget(wSciTE));
}

bool SciTEGTK::ParametersOpen() {
	return dlgParameters.Created();
}

void SciTEGTK::ParamGrab() {
	if (dlgParameters.Created()) {
		for (int param = 0; param < maxParam; param++) {
			SString paramText(param + 1);
			const char *paramVal = gtk_entry_get_text(GTK_ENTRY(entryParam[param]));
			props.Set(paramText.c_str(), paramVal);
		}
		UpdateStatusBar(true);
	}
}

gint SciTEGTK::ParamKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->dlgParameters.Destroy();
	}
	return FALSE;
}

void SciTEGTK::ParamCancelCmd() {
	dlgParameters.Destroy();
	CheckMenus();
}

void SciTEGTK::ParamCmd() {
	paramDialogCanceled = false;
	ParamGrab();
	dlgParameters.Destroy();
	CheckMenus();
}

void SciTEGTK::ParamResponse(int responseID) {
	switch (responseID) {
		case GTK_RESPONSE_OK:
			ParamCmd() ;
			break;

		case GTK_RESPONSE_CANCEL:
			ParamCancelCmd();
			break;
	}
}

bool SciTEGTK::ParametersDialog(bool modal) {
	if (dlgParameters.Created()) {
		ParamGrab();
		if (!modal) {
			dlgParameters.Destroy();
		}
		return true;
	}
	paramDialogCanceled = true;
	dlgParameters.Create(this, "Parameters", &localiser);

	gtk_signal_connect(GTK_OBJECT(PWidget(dlgParameters)),
	                   "destroy", GtkSignalFunc(destroyDialog), &dlgParameters);

	Table table(modal ? 10 : 9, 2);
	table.PackInto(GTK_BOX(GTK_DIALOG(PWidget(dlgParameters))->vbox));

	if (modal) {
		GtkWidget *cmd = gtk_label_new(parameterisedCommand.c_str());
		table.Add(cmd, 2);
	}

	for (int param = 0; param < maxParam; param++) {
		SString paramText(param + 1);
		SString paramTextVal = props.Get(paramText.c_str());
		paramText.insert(0, "_");
		paramText.append(":");
		GtkWidget *label = gtk_label_new_with_mnemonic(paramText.c_str());
		table.Label(label);

		entryParam[param] = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(entryParam[param]), paramTextVal.c_str());
		if (param == 0)
			gtk_entry_select_region(GTK_ENTRY(entryParam[param]), 0, paramTextVal.length());
		table.Add(entryParam[param]);
		gtk_entry_set_activates_default(GTK_ENTRY(entryParam[param]), TRUE);

		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entryParam[param]);
	}

	gtk_widget_grab_focus(GTK_WIDGET(entryParam[0]));

	AttachResponse<&SciTEGTK::ParamResponse>(PWidget(dlgParameters), this);
	dlgParameters.ResponseButton(modal ? "_Cancel" : "_Close", GTK_RESPONSE_CANCEL);
	dlgParameters.ResponseButton(modal ? "_Execute" : "_Set", GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(PWidget(dlgParameters)), GTK_RESPONSE_OK);

	dlgParameters.Display(PWidget(wSciTE), modal);

	return !paramDialogCanceled;
}

bool SciTEGTK::FindReplaceAdvanced() {
	return props.GetInt("find.replace.advanced");
}

#define RESPONSE_MARK_ALL 1002
#define RESPONSE_REPLACE 1003
#define RESPONSE_REPLACE_ALL 1004
#define RESPONSE_REPLACE_IN_SELECTION 1005
#define RESPONSE_REPLACE_IN_BUFFERS 1006

void SciTEGTK::FindReplaceResponse(int responseID) {
	switch (responseID) {
		case GTK_RESPONSE_OK:
			FRFindCmd() ;
			break;

		case GTK_RESPONSE_CANCEL:
			dlgFindReplace.Destroy();
			break;

		case RESPONSE_MARK_ALL:
			FRMarkAllCmd();
			break;

		case RESPONSE_REPLACE:
			FRReplaceCmd();
			break;

		case RESPONSE_REPLACE_ALL:
			FRReplaceAllCmd();
			break;

		case RESPONSE_REPLACE_IN_SELECTION:
			FRReplaceInSelectionCmd();
			break;

		case RESPONSE_REPLACE_IN_BUFFERS:
			FRReplaceInBuffersCmd();
			break;
	}
}

void SciTEGTK::FindReplace(bool replace) {

	replacing = replace;
	dlgFindReplace.Create(this, replace ? "Replace" : "Find", &localiser);

	gtk_signal_connect(GTK_OBJECT(PWidget(dlgFindReplace)),
	                   "destroy", GtkSignalFunc(destroyDialog), &dlgFindReplace);

	Table table(replace ? 8 : 7, 2);
	table.PackInto(GTK_BOX(GTK_DIALOG(PWidget(dlgFindReplace))->vbox));

	GtkWidget *labelFind = TranslatedLabel("Fi_nd what:");
	table.Label(labelFind);

	comboFind.Create();
	FillComboFromMemory(comboFind, memFinds);
	table.Add(comboFind, 1, true);

	gtk_entry_set_text(comboFind.Entry(), findWhat.c_str());
	gtk_entry_set_width_chars(comboFind.Entry(), 40);
	gtk_entry_select_region(comboFind.Entry(), 0, findWhat.length());
	gtk_entry_set_activates_default(comboFind.Entry(), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelFind), comboFind);

	if (replace) {
		GtkWidget *labelReplace = TranslatedLabel("Rep_lace with:");
		table.Label(labelReplace);

		comboReplace.Create();
		FillComboFromMemory(comboReplace, memReplaces);
		table.Add(comboReplace, 1, true);

		gtk_entry_set_activates_default(comboReplace.Entry(), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(labelReplace), comboReplace);

	} else {
		comboReplace.SetID(0);
	}

	// Whole Word
	toggleWord = dlgFindReplace.Toggle(toggles[Toggle::tWord].label, wholeWord);
	table.Add(toggleWord, 2, false, 3, 0);

	// Case Sensitive
	toggleCase = dlgFindReplace.Toggle(toggles[Toggle::tCase].label, matchCase);
	table.Add(toggleCase, 2, false, 3, 0);

	// Regular Expression
	toggleRegExp = dlgFindReplace.Toggle(toggles[Toggle::tRegExp].label, regExp);
	table.Add(toggleRegExp, 2, false, 3, 0);

	// Transform backslash expressions
	toggleUnSlash = dlgFindReplace.Toggle(toggles[Toggle::tBackslash].label, unSlash);
	table.Add(toggleUnSlash, 2, false, 3, 0);

	// Wrap Around
	toggleWrap = dlgFindReplace.Toggle(toggles[Toggle::tWrap].label, wrapFind);
	table.Add(toggleWrap, 2, false, 3, 0);

	// Reverse
	toggleReverse = dlgFindReplace.Toggle(toggles[Toggle::tUp].label, reverseFind);
	table.Add(toggleReverse, 2, false, 3, 0);

	if (!replace) {
		dlgFindReplace.ResponseButton("Mark _All", RESPONSE_MARK_ALL);
	}

	if (replace) {
		dlgFindReplace.ResponseButton("_Replace", RESPONSE_REPLACE);
		dlgFindReplace.ResponseButton("Replace _All", RESPONSE_REPLACE_ALL);
		dlgFindReplace.ResponseButton("In _Selection", RESPONSE_REPLACE_IN_SELECTION);
		if (FindReplaceAdvanced()) {
			dlgFindReplace.ResponseButton("Replace In _Buffers", RESPONSE_REPLACE_IN_BUFFERS);
		}
	}

	dlgFindReplace.ResponseButton("Close", GTK_RESPONSE_CANCEL);
	dlgFindReplace.ResponseButton("F_ind", GTK_RESPONSE_OK);

	AttachResponse<&SciTEGTK::FindReplaceResponse>(PWidget(dlgFindReplace), this);
	gtk_dialog_set_default_response(GTK_DIALOG(PWidget(dlgFindReplace)), GTK_RESPONSE_OK);

	gtk_signal_connect(GTK_OBJECT(PWidget(dlgFindReplace)),
	                   "key_press_event", GtkSignalFunc(FRKeySignal), this);

	gtk_widget_grab_focus(GTK_WIDGET(comboFind.Entry()));

	dlgFindReplace.Display(PWidget(wSciTE), false);
}

void SciTEGTK::DestroyFindReplace() {
	dlgFindReplace.Destroy();
}

int SciTEGTK::WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, int style) {
	if (!messageBoxDialog) {
		SString sMsg(msg.c_str());
		dialogsOnScreen++;
		GtkAccelGroup *accel_group = gtk_accel_group_new();

		messageBoxResult = -1;
		messageBoxDialog = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(messageBoxDialog), appName);
		gtk_container_border_width(GTK_CONTAINER(messageBoxDialog), 0);

		gtk_signal_connect(GTK_OBJECT(messageBoxDialog),
		                   "destroy", GtkSignalFunc(messageBoxDestroy), &messageBoxDialog);

		int escapeResult = IDOK;
		if ((style & 0xf) == MB_OK) {
			AddMBButton(messageBoxDialog, "_OK", IDOK, accel_group, true);
		} else {
			AddMBButton(messageBoxDialog, "_Yes", IDYES, accel_group, true);
			AddMBButton(messageBoxDialog, "_No", IDNO, accel_group);
			escapeResult = IDNO;
			if ((style & 0xf) == MB_YESNOCANCEL) {
				AddMBButton(messageBoxDialog, "_Cancel", IDCANCEL, accel_group);
				escapeResult = IDCANCEL;
			}
		}
		gtk_signal_connect(GTK_OBJECT(messageBoxDialog),
		                   "key_press_event", GtkSignalFunc(messageBoxKey),
		                   reinterpret_cast<gpointer>(escapeResult));

		if (style & MB_ABOUTBOX) {
			GtkWidget *explanation = scintilla_new();
			GUI::ScintillaWindow scExplanation;
			scExplanation.SetID(explanation);
			scintilla_set_id(SCINTILLA(explanation), 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(messageBoxDialog)->vbox),
			                   explanation, TRUE, TRUE, 0);
			gtk_widget_set_usize(GTK_WIDGET(explanation), 480, 380);
			gtk_widget_show_all(explanation);
			SetAboutMessage(scExplanation, "SciTE");
		} else {
			GtkWidget *label = gtk_label_new(sMsg.c_str());
			gtk_misc_set_padding(GTK_MISC(label), 10, 10);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(messageBoxDialog)->vbox),
			                   label, TRUE, TRUE, 0);
			gtk_widget_show(label);
		}

		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(messageBoxDialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(messageBoxDialog),
		                             GTK_WINDOW(PWidget(w)));

		gtk_widget_show(messageBoxDialog);
		gtk_window_add_accel_group(GTK_WINDOW(messageBoxDialog), accel_group);
		while (messageBoxResult < 0) {
			gtk_main_iteration();
		}
		dialogsOnScreen--;
	}
	return messageBoxResult;
}

void SciTEGTK::FindMessageBox(const SString &msg, const SString *findItem) {
	if (findItem == 0) {
		GUI::gui_string msgBuf = LocaliseMessage(msg.c_str());
		WindowMessageBox(wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
	} else {
		GUI::gui_string msgBuf = LocaliseMessage(msg.c_str(), findItem->c_str());
		WindowMessageBox(wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
	}
}

void SciTEGTK::AboutDialog() {
	WindowMessageBox(wSciTE, GUI::gui_string("SciTE\nby Neil Hodgson neilh@scintilla.org ."),
	                 MB_OK | MB_ABOUTBOX);
}

void SciTEGTK::QuitProgram() {
	if (SaveIfUnsureAll() != IDCANCEL) {
		gtk_main_quit();
	}
}

gint SciTEGTK::MoveResize(GtkWidget *, GtkAllocation * /*allocation*/, SciTEGTK *scitew) {
	scitew->SizeSubWindows();
	return TRUE;
}

gint SciTEGTK::QuitSignal(GtkWidget *, GdkEventAny *, SciTEGTK *scitew) {
	scitew->QuitProgram();
	return TRUE;
}

void SciTEGTK::ButtonSignal(GtkWidget *, gpointer data) {
	instance->Command((guint)(long)data);
}

void SciTEGTK::MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *) {
	if (scitew->allowMenuActions)
		scitew->Command(action);
}

void SciTEGTK::CommandSignal(GtkWidget *, gint wParam, gpointer lParam, SciTEGTK *scitew) {

	scitew->Command(wParam, reinterpret_cast<long>(lParam));
}

void SciTEGTK::NotifySignal(GtkWidget *, gint /*wParam*/, gpointer lParam, SciTEGTK *scitew) {
	scitew->Notify(reinterpret_cast<SCNotification *>(lParam));
}

gint SciTEGTK::KeyPress(GtkWidget * /*widget*/, GdkEventKey *event, SciTEGTK *scitew) {
	return scitew->Key(event);
}

gint SciTEGTK::MousePress(GtkWidget * /*widget*/, GdkEventButton *event, SciTEGTK *scitew) {
	return scitew->Mouse(event);
}

// Translate key strokes that are not in a menu into commands
class KeyToCommand {
public:
	int modifiers;
	unsigned int key;	// For alphabetic keys has to match the shift modifier.
	int msg;
};

enum {
    m__ = 0,
    mS_ = GDK_SHIFT_MASK,
    m_C = GDK_CONTROL_MASK,
    mSC = GDK_SHIFT_MASK | GDK_CONTROL_MASK
};

static KeyToCommand kmap[] = {
                                 {m_C, GDK_Tab, IDM_NEXTFILE},
                                 {mSC, GDK_ISO_Left_Tab, IDM_PREVFILE},
                                 {m_C, GDK_KP_Enter, IDM_COMPLETEWORD},
                                 {m_C, GDK_F3, IDM_FINDNEXTSEL},
                                 {mSC, GDK_F3, IDM_FINDNEXTBACKSEL},
                                 {m_C, GDK_F4, IDM_CLOSE},
                                 {m_C, 'j', IDM_PREVMATCHPPC},
                                 {mSC, 'J', IDM_SELECTTOPREVMATCHPPC},
                                 {m_C, 'k', IDM_NEXTMATCHPPC},
                                 {mSC, 'K', IDM_SELECTTONEXTMATCHPPC},
                                 {m_C, GDK_KP_Multiply, IDM_EXPAND},
                                 {0, 0, 0},
                             };

inline bool KeyMatch(const char *menuKey, int keyval, int modifiers) {
	return SciTEKeys::MatchKeyCode(
		SciTEKeys::ParseKeyCode(menuKey), keyval, modifiers);
}

gint SciTEGTK::Key(GdkEventKey *event) {
	//printf("S-key: %d %x %x %x %x\n",event->keyval, event->state, GDK_SHIFT_MASK, GDK_CONTROL_MASK, GDK_F3);
	int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

	int cmodifiers = // modifier mask for Lua extension
		(event->state & GDK_SHIFT_MASK   ? SCMOD_SHIFT : 0) |
		(event->state & GDK_CONTROL_MASK ? SCMOD_CTRL  : 0) |
		(event->state & GDK_MOD1_MASK    ? SCMOD_ALT   : 0);
	if (extender && extender->OnKey(event->keyval, cmodifiers))
		return 1;

	int commandID = 0;
	for (int i = 0; kmap[i].msg; i++) {
		if ((event->keyval == kmap[i].key) && (modifiers == kmap[i].modifiers)) {
			commandID = kmap[i].msg;
		}
	}
	if (!commandID) {
		// Look through language menu
		for (int j = 0; j < languageItems; j++) {
			if (KeyMatch(languageMenu[j].menuKey.c_str(), event->keyval, modifiers)) {
				commandID = IDM_LANGUAGE + j;
			}
		}
	}
	if (commandID) {
		Command(commandID);
	}
	if ((commandID == IDM_NEXTFILE) || (commandID == IDM_PREVFILE)) {
		// Stop the default key processing from moving the focus
		gtk_signal_emit_stop_by_name(
		    GTK_OBJECT(PWidget(wSciTE)), "key_press_event");
	}

	// check tools menu command shortcuts
	for (int tool_i = 0; tool_i < toolMax; ++tool_i) {
		GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, IDM_TOOLS + tool_i);
		if (item) {
			long keycode = reinterpret_cast<long>(gtk_object_get_user_data(GTK_OBJECT(item)));
			if (keycode && SciTEKeys::MatchKeyCode(keycode, event->keyval, modifiers)) {
				SciTEBase::MenuCommand(IDM_TOOLS + tool_i);
				return 1;
			}
		}
	}

	// check user defined keys
	for (int cut_i = 0; cut_i < shortCutItems; cut_i++) {
		if (KeyMatch(shortCutItemList[cut_i].menuKey.c_str(), event->keyval, modifiers)) {
			int commandNum = SciTEBase::GetMenuCommandAsInt(shortCutItemList[cut_i].menuCommand);
			if (commandNum != -1) {
				if (commandNum < 2000) {
					SciTEBase::MenuCommand(commandNum);
				} else {
					SciTEBase::CallFocused(commandNum);
				}
				gtk_signal_emit_stop_by_name(
				    GTK_OBJECT(PWidget(wSciTE)), "key_press_event");
				return 1;
			}
		}
	}

	if (findStrip.KeyDown(event) || replaceStrip.KeyDown(event)) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(PWidget(wSciTE)), "key_press_event");
		return 1;
	}

	return 0;
}

void SciTEGTK::PopUpCmd(GtkMenuItem *menuItem, SciTEGTK *scitew) {
	sptr_t cmd = (sptr_t)(g_object_get_data(G_OBJECT(menuItem), "CmdNum"));
	scitew->Command(cmd);
}

void SciTEGTK::AddToPopUp(const char *label, int cmd, bool enabled) {
	GUI::gui_string localised = localiser.Text(label);
	GtkWidget *menuItem;
	if (label[0])
		menuItem = gtk_menu_item_new_with_label(localised.c_str());
	else
		menuItem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(popup.GetID()), menuItem);
	g_object_set_data(G_OBJECT(menuItem), "CmdNum", reinterpret_cast<void *>((sptr_t)(cmd)));
	g_signal_connect(G_OBJECT(menuItem),"activate", G_CALLBACK(PopUpCmd), this);

	if (cmd) {
		if (menuItem)
			gtk_widget_set_sensitive(menuItem, enabled);
	}
}

gint SciTEGTK::Mouse(GdkEventButton *event) {
	if (event->button == 3) {
		// PopUp menu
		GUI::ScintillaWindow *w = &wEditor;
		menuSource = IDM_SRCWIN;
		if (PWidget(*w)->window != event->window) {
			if (PWidget(wOutput)->window == event->window) {
				menuSource = IDM_RUNWIN;
				w = &wOutput;
			} else {
				menuSource = 0;
				//fprintf(stderr, "Menu source focus\n");
				return FALSE;
			}
		}
		// Convert to screen
		int ox = 0;
		int oy = 0;
		gdk_window_get_origin(PWidget(*w)->window, &ox, &oy);
		ContextMenu(*w, GUI::Point(static_cast<int>(event->x) + ox,
		                     static_cast<int>(event->y) + oy), wSciTE);
		//fprintf(stderr, "Menu source %s\n",
		//	(menuSource == IDM_SRCWIN) ? "IDM_SRCWIN" : "IDM_RUNWIN");
	} else {
		menuSource = 0;
		//fprintf(stderr, "Menu source focus\n");
	}

	return FALSE;
}

void SciTEGTK::DividerXOR(GUI::Point pt) {
	if (!xor_gc) {
		GdkGCValues values;
		values.foreground = PWidget(wSciTE)->style->white;
		values.function = GDK_XOR;
		values.subwindow_mode = GDK_INCLUDE_INFERIORS;
		xor_gc = gdk_gc_new_with_values(PWidget(wSciTE)->window,
		                                &values,
		                                static_cast<GdkGCValuesMask>(
		                                    GDK_GC_FOREGROUND | GDK_GC_FUNCTION | GDK_GC_SUBWINDOW));
	}
	if (splitVertical) {
		gdk_draw_line(PWidget(wSciTE)->window, xor_gc,
		              pt.x,
		              PWidget(wDivider)->allocation.y,
		              pt.x,
		              PWidget(wDivider)->allocation.y + PWidget(wDivider)->allocation.height - 1);
	} else {
		gdk_draw_line(PWidget(wSciTE)->window, xor_gc,
		              PWidget(wDivider)->allocation.x,
		              pt.y,
		              PWidget(wDivider)->allocation.x + PWidget(wDivider)->allocation.width - 1,
		              pt.y);
	}
	ptOld = pt;
}

gint SciTEGTK::DividerExpose(GtkWidget *widget, GdkEventExpose *, SciTEGTK *sciThis) {
	//GtkStyle style = gtk_widget_get_default_style();
	GdkRectangle area;
	area.x = 0;
	area.y = 0;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;
	gdk_window_clear_area(widget->window,
	                      area.x, area.y, area.width, area.height);
	if (widget->allocation.width > widget->allocation.height) {
		// Horizontal divider
		gtk_paint_hline(widget->style, widget->window, GTK_STATE_NORMAL,
		                &area, widget, const_cast<char *>("vpaned"),
		                0, widget->allocation.width - 1,
		                area.height / 2 - 1);
		gtk_paint_box (widget->style, widget->window,
		               GTK_STATE_NORMAL,
		               GTK_SHADOW_OUT,
		               &area, widget, const_cast<char *>("paned"),
		               area.width - sciThis->heightBar * 2, 1,
		               sciThis->heightBar - 2, sciThis->heightBar - 2);
	} else {
		gtk_paint_vline(widget->style, widget->window, GTK_STATE_NORMAL,
		                &area, widget, const_cast<char *>("hpaned"),
		                0, widget->allocation.height - 1,
		                area.width / 2 - 1);
		gtk_paint_box (widget->style, widget->window,
		               GTK_STATE_NORMAL,
		               GTK_SHADOW_OUT,
		               &area, widget, const_cast<char *>("paned"),
		               1, area.height - sciThis->heightBar * 2,
		               sciThis->heightBar - 2, sciThis->heightBar - 2);
	}
	return TRUE;
}

gint SciTEGTK::DividerMotion(GtkWidget *, GdkEventMotion *event, SciTEGTK *scitew) {
	if (scitew->capturedMouse) {
		int x = 0;
		int y = 0;
		GdkModifierType state;
		if (event->is_hint) {
			gdk_window_get_pointer(PWidget(scitew->wSciTE)->window, &x, &y, &state);
			if (state & GDK_BUTTON1_MASK) {
				scitew->DividerXOR(scitew->ptOld);
				scitew->DividerXOR(GUI::Point(x, y));
			}
		}
	}
	return TRUE;
}

gint SciTEGTK::DividerPress(GtkWidget *, GdkEventButton *event, SciTEGTK *scitew) {
	if (event->type == GDK_BUTTON_PRESS) {
		int x = 0;
		int y = 0;
		GdkModifierType state;
		gdk_window_get_pointer(PWidget(scitew->wSciTE)->window, &x, &y, &state);
		scitew->ptStartDrag = GUI::Point(x, y);
		scitew->capturedMouse = true;
		scitew->heightOutputStartDrag = scitew->heightOutput;
		scitew->focusEditor = scitew->wEditor.Call(SCI_GETFOCUS) != 0;
		if (scitew->focusEditor) {
			scitew->wEditor.Call(SCI_SETFOCUS, 0);
		}
		scitew->focusOutput = scitew->wOutput.Call(SCI_GETFOCUS) != 0;
		if (scitew->focusOutput) {
			scitew->wOutput.Call(SCI_SETFOCUS, 0);
		}
		gtk_widget_grab_focus(GTK_WIDGET(PWidget(scitew->wDivider)));
		gtk_grab_add(GTK_WIDGET(PWidget(scitew->wDivider)));
		gtk_widget_draw(PWidget(scitew->wDivider), NULL);
		scitew->DividerXOR(scitew->ptStartDrag);
	}
	return TRUE;
}

gint SciTEGTK::DividerRelease(GtkWidget *, GdkEventButton *, SciTEGTK *scitew) {
	if (scitew->capturedMouse) {
		scitew->capturedMouse = false;
		gtk_grab_remove(GTK_WIDGET(PWidget(scitew->wDivider)));
		scitew->DividerXOR(scitew->ptOld);
		int x = 0;
		int y = 0;
		GdkModifierType state;
		gdk_window_get_pointer(PWidget(scitew->wSciTE)->window, &x, &y, &state);
		scitew->MoveSplit(GUI::Point(x, y));
		if (scitew->focusEditor)
			scitew->wEditor.Call(SCI_SETFOCUS, 1);
		if (scitew->focusOutput)
			scitew->wOutput.Call(SCI_SETFOCUS, 1);
	}
	return TRUE;
}

void SciTEGTK::DragDataReceived(GtkWidget *, GdkDragContext *context,
                                gint /*x*/, gint /*y*/, GtkSelectionData *seldata, guint /*info*/, guint time, SciTEGTK *scitew) {
	scitew->OpenUriList(reinterpret_cast<const char *>(seldata->data));
	gtk_drag_finish(context, TRUE, FALSE, time);
}

void SetFocus(GUI::ScintillaWindow &w) {
	w.Call(SCI_GRABFOCUS);
}

gint SciTEGTK::TabBarRelease(GtkNotebook *notebook, GdkEventButton *event) {
	if (event->button == 1) {
		SetDocumentAt(gtk_notebook_current_page(GTK_NOTEBOOK(wTabBar.GetID())));
		CheckReload();
	} else if (event->button == 2) {
		for (int pageNum=0;pageNum<gtk_notebook_get_n_pages(notebook);pageNum++) {
			GtkWidget *page = gtk_notebook_get_nth_page(notebook, pageNum);
			if (page) {
				GtkWidget *label = gtk_notebook_get_tab_label(notebook, page);
				if (event->x < (label->allocation.x + label->allocation.width)) {
					CloseTab(pageNum);
					break;
				}
			}
		}
	}
	return FALSE;
}

gint SciTEGTK::TabBarScroll(GdkEventScroll *event) {
	switch (event->direction) {
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_DOWN:
			Next();
			WindowSetFocus(wEditor);
			break;
		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_UP:
			Prev();
			WindowSetFocus(wEditor);
			break;
	}
	// Return true because Next() or Prev() already switches the tab
	return TRUE;
}

GtkWidget *SciTEGTK::pixmap_new(GtkWidget *window, gchar **xpm) {
	GdkBitmap *mask = 0;

	/* now for the pixmap from gdk */
	GtkStyle *style = gtk_widget_get_style(window);
	GdkPixmap *pixmap = gdk_pixmap_create_from_xpm_d(
	                        window->window,
	                        &mask,
	                        &style->bg[GTK_STATE_NORMAL],
	                        xpm);

	/* a pixmap widget to contain the pixmap */
	GtkWidget *pixmapwid = gtk_pixmap_new(pixmap, mask);
	gtk_widget_show(pixmapwid);

	return pixmapwid;
}

GtkWidget *SciTEGTK::AddToolButton(const char *text, int cmd, GtkWidget *toolbar_icon) {

	GtkWidget *button = gtk_toolbar_append_element(GTK_TOOLBAR(PWidget(wToolBar)),
	                    GTK_TOOLBAR_CHILD_BUTTON,
	                    NULL,
	                    NULL,
	                    text, NULL,
	                    toolbar_icon, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
	                   GTK_SIGNAL_FUNC (ButtonSignal),
	                   (gpointer)cmd);
	return button;
}

void SciTEGTK::AddToolBar() {
	if (props.GetInt("toolbar.usestockicons") == 1) {
		AddToolButton("New", IDM_NEW, gtk_image_new_from_stock("gtk-new", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Open", IDM_OPEN, gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Save", IDM_SAVE, gtk_image_new_from_stock("gtk-save", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Close", IDM_CLOSE, gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_LARGE_TOOLBAR));

		gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
		AddToolButton("Undo", IDM_UNDO, gtk_image_new_from_stock("gtk-undo", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Redo", IDM_REDO, gtk_image_new_from_stock("gtk-redo", GTK_ICON_SIZE_LARGE_TOOLBAR));

		gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
		AddToolButton("Cut", IDM_CUT, gtk_image_new_from_stock("gtk-cut", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Copy", IDM_COPY, gtk_image_new_from_stock("gtk-copy", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Paste", IDM_PASTE, gtk_image_new_from_stock("gtk-paste", GTK_ICON_SIZE_LARGE_TOOLBAR));

		gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
		AddToolButton("Find in Files", IDM_FINDINFILES, gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Find", IDM_FIND, gtk_image_new_from_stock("gtk-zoom-fit", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Find Next", IDM_FINDNEXT, gtk_image_new_from_stock("gtk-jump-to", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Replace", IDM_REPLACE, gtk_image_new_from_stock("gtk-find-and-replace", GTK_ICON_SIZE_LARGE_TOOLBAR));

		gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
		btnCompile = AddToolButton("Compile", IDM_COMPILE, gtk_image_new_from_stock("gtk-execute", GTK_ICON_SIZE_LARGE_TOOLBAR));
		btnBuild = AddToolButton("Build", IDM_BUILD, gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_LARGE_TOOLBAR));
		btnStop = AddToolButton("Stop", IDM_STOPEXECUTE, gtk_image_new_from_stock("gtk-stop", GTK_ICON_SIZE_LARGE_TOOLBAR));

		gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
		AddToolButton("Previous", IDM_PREVFILE, gtk_image_new_from_stock("gtk-go-back", GTK_ICON_SIZE_LARGE_TOOLBAR));
		AddToolButton("Next Buffer", IDM_NEXTFILE, gtk_image_new_from_stock("gtk-go-forward", GTK_ICON_SIZE_LARGE_TOOLBAR));
		return;
	}
	AddToolButton("New", IDM_NEW, pixmap_new(PWidget(wSciTE), (gchar**)filenew_xpm));
	AddToolButton("Open", IDM_OPEN, pixmap_new(PWidget(wSciTE), (gchar**)fileopen_xpm));
	AddToolButton("Save", IDM_SAVE, pixmap_new(PWidget(wSciTE), (gchar**)filesave_xpm));
	AddToolButton("Close", IDM_CLOSE, pixmap_new(PWidget(wSciTE), (gchar**)close_xpm));

	gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
	AddToolButton("Undo", IDM_UNDO, pixmap_new(PWidget(wSciTE), (gchar**)undo_xpm));
	AddToolButton("Redo", IDM_REDO, pixmap_new(PWidget(wSciTE), (gchar**)redo_xpm));

	gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
	AddToolButton("Cut", IDM_CUT, pixmap_new(PWidget(wSciTE), (gchar**)editcut_xpm));
	AddToolButton("Copy", IDM_COPY, pixmap_new(PWidget(wSciTE), (gchar**)editcopy_xpm));
	AddToolButton("Paste", IDM_PASTE, pixmap_new(PWidget(wSciTE), (gchar**)editpaste_xpm));

	gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
	AddToolButton("Find in Files", IDM_FINDINFILES, pixmap_new(PWidget(wSciTE), (gchar**)findinfiles_xpm));
	AddToolButton("Find", IDM_FIND, pixmap_new(PWidget(wSciTE), (gchar**)search_xpm));
	AddToolButton("Find Next", IDM_FINDNEXT, pixmap_new(PWidget(wSciTE), (gchar**)findnext_xpm));
	AddToolButton("Replace", IDM_REPLACE, pixmap_new(PWidget(wSciTE), (gchar**)replace_xpm));

	gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
	btnCompile = AddToolButton("Compile", IDM_COMPILE, pixmap_new(PWidget(wSciTE), (gchar**)compile_xpm));
	btnBuild = AddToolButton("Build", IDM_BUILD, pixmap_new(PWidget(wSciTE), (gchar**)build_xpm));
	btnStop = AddToolButton("Stop", IDM_STOPEXECUTE, pixmap_new(PWidget(wSciTE), (gchar**)stop_xpm));

	gtk_toolbar_append_space(GTK_TOOLBAR(PWidget(wToolBar)));
	AddToolButton("Previous", IDM_PREVFILE, pixmap_new(PWidget(wSciTE), (gchar**)prev_xpm));
	AddToolButton("Next Buffer", IDM_NEXTFILE, pixmap_new(PWidget(wSciTE), (gchar**)next_xpm));
}

SString SciTEGTK::TranslatePath(const char *path) {
	if (path && path[0] == '/') {
		SString spathTranslated;
		SString spath(path, 1, strlen(path));
		spath.append("/");
		int end = spath.search("/");
		while (spath.length() > 1) {
			SString segment(spath.c_str(), 0, end);
			GUI::gui_string segmentLocalised = localiser.Text(segment.c_str());
			std::replace(segmentLocalised.begin(), segmentLocalised.end(), '/', '|');
			spathTranslated.append("/");
			spathTranslated.append(segmentLocalised.c_str());
			spath.remove(0, end + 1);
			end = spath.search("/");
		}
		return spathTranslated;
	} else {
		return path;
	}
}

void SciTEGTK::CreateTranslatedMenu(int n, SciTEItemFactoryEntry items[],
                                    int nRepeats, const char *prefix, int startNum,
                                    int startID, const char *radioStart) {

	char *gthis = reinterpret_cast<char *>(this);
	int dim = n + nRepeats;
	GtkItemFactoryEntry *translatedItems = new GtkItemFactoryEntry[dim];
	SString *translatedText = new SString[dim];
	SString *translatedRadios = new SString[dim];
	char **userDefinedAccels = new char*[n];
	SString menuPath;
	int i = 0;

	for (; i < n; i++) {
		// Try to find user-defined accelerator key
		menuPath = "menukey";			// menupath="menukey"
		menuPath += items[i].path;		// menupath="menukey/File/Save _As..."
		menuPath.remove("_");			// menupath="menukey/File/Save As..."
		menuPath.remove(".");			// menupath="menukey/File/Save As"
		menuPath.substitute('/', '.');	// menupath="menukey.File.Save As"
		menuPath.substitute(' ', '_');	// menupath="menukey.File.Save_As"
		menuPath.lowercase();			// menupath="menukey.file.save_as"

		SString accelKey = props.Get(menuPath.c_str());

		int accLength = accelKey.length();
		if (accLength > 0) {
			if (accelKey == "\"\"" || accelKey == "none") {
				accelKey.clear();	// Allow user to clear accelerator key
			}
			userDefinedAccels[i] = new char[accLength + 1];
			strncpy(userDefinedAccels[i], accelKey.c_str(), accLength + 1);
			items[i].accelerator = userDefinedAccels[i];
		} else {
			userDefinedAccels[i] = NULL;
		}

		translatedItems[i].path = (gchar*) items[i].path;
		translatedItems[i].accelerator = (gchar*) items[i].accelerator;
		translatedItems[i].callback = items[i].callback;
		translatedItems[i].callback_action = items[i].callback_action;
		translatedItems[i].item_type = (gchar*) items[i].item_type;
		translatedItems[i].extra_data = 0;
		translatedText[i] = TranslatePath(translatedItems[i].path);
		translatedItems[i].path = const_cast<char *>(translatedText[i].c_str());
		translatedRadios[i] = TranslatePath(translatedItems[i].item_type);
		translatedItems[i].item_type = const_cast<char *>(translatedRadios[i].c_str());

	}
	GtkItemFactoryCallback menuSig = GtkItemFactoryCallback(MenuSignal);
	for (; i < dim; i++) {
		int suffix = i - n + startNum;
		SString ssnum(suffix);
		translatedText[i] = TranslatePath(prefix);
		translatedText[i] += ssnum;
		translatedItems[i].path = const_cast<char *>(translatedText[i].c_str());
		translatedItems[i].accelerator = NULL;
		translatedItems[i].callback = menuSig;
		translatedItems[i].callback_action = startID + suffix;
		translatedRadios[i] = TranslatePath(radioStart);
		translatedItems[i].item_type = const_cast<char *>(translatedRadios[i].c_str());
	}
	gtk_item_factory_create_items(itemFactory, dim, translatedItems, gthis);
	delete []translatedRadios;
	delete []translatedText;
	delete []translatedItems;

	// Release all the memory allocated for the user-defined accelerator keys
	for (i = 0; i < n; i++) {
		if (userDefinedAccels[i] != NULL)
			delete[] userDefinedAccels[i];
	}
	delete[] userDefinedAccels;
}

void SciTEGTK::CreateMenu() {

	GtkItemFactoryCallback menuSig = GtkItemFactoryCallback(MenuSignal);
	SciTEItemFactoryEntry menuItems[] = {
	                                      {"/_File", NULL, NULL, 0, "<Branch>"},
	                                      {"/File/_New", "<control>N", menuSig, IDM_NEW, 0},
	                                      {"/File/_Open...", "<control>O", menuSig, IDM_OPEN, 0},
	                                      {"/File/Open Selected _Filename", "<control><shift>O", menuSig, IDM_OPENSELECTED, 0},
	                                      {"/File/_Revert", "<control>R", menuSig, IDM_REVERT, 0},
	                                      {"/File/_Close", "<control>W", menuSig, IDM_CLOSE, 0},
	                                      {"/File/_Save", "<control>S", menuSig, IDM_SAVE, 0},
	                                      {"/File/Save _As...", "<control><shift>S", menuSig, IDM_SAVEAS, 0},
	                                      {"/File/Save a Co_py...", "<control><shift>P", menuSig, IDM_SAVEACOPY, 0},
	                                      {"/File/Copy Pat_h", NULL, menuSig, IDM_COPYPATH, 0},
	                                      {"/File/Encodin_g", NULL, NULL, 0, "<Branch>"},
	                                      {"/File/Encoding/_Code Page Property", NULL, menuSig, IDM_ENCODING_DEFAULT, "<RadioItem>"},
	                                      {"/File/Encoding/UTF-16 _Big Endian", NULL, menuSig, IDM_ENCODING_UCS2BE, "/File/Encoding/Code Page Property"},
	                                      {"/File/Encoding/UTF-16 _Little Endian", NULL, menuSig, IDM_ENCODING_UCS2LE, "/File/Encoding/Code Page Property"},
	                                      {"/File/Encoding/UTF-8 _with BOM", NULL, menuSig, IDM_ENCODING_UTF8, "/File/Encoding/Code Page Property"},
	                                      {"/File/Encoding/_UTF-8", NULL, menuSig, IDM_ENCODING_UCOOKIE, "/File/Encoding/Code Page Property"},
	                                      {"/File/_Export", "", 0, 0, "<Branch>"},
	                                      {"/File/Export/As _HTML...", NULL, menuSig, IDM_SAVEASHTML, 0},
	                                      {"/File/Export/As _RTF...", NULL, menuSig, IDM_SAVEASRTF, 0},
	                                      {"/File/Export/As _PDF...", NULL, menuSig, IDM_SAVEASPDF, 0},
	                                      {"/File/Export/As _LaTeX...", NULL, menuSig, IDM_SAVEASTEX, 0},
	                                      {"/File/Export/As _XML...", NULL, menuSig, IDM_SAVEASXML, 0},
	                                      {"/File/_Print", "<control>P", menuSig, IDM_PRINT, 0},
	                                      {"/File/sep1", NULL, NULL, 0, "<Separator>"},
	                                      {"/File/_Load Session...", "", menuSig, IDM_LOADSESSION, 0},
	                                      {"/File/Sa_ve Session...", "", menuSig, IDM_SAVESESSION, 0},
	                                      {"/File/sep2", NULL, menuSig, IDM_MRU_SEP, "<Separator>"},
	                                      {"/File/File0", "", menuSig, fileStackCmdID + 0, 0},
	                                      {"/File/File1", "", menuSig, fileStackCmdID + 1, 0},
	                                      {"/File/File2", "", menuSig, fileStackCmdID + 2, 0},
	                                      {"/File/File3", "", menuSig, fileStackCmdID + 3, 0},
	                                      {"/File/File4", "", menuSig, fileStackCmdID + 4, 0},
	                                      {"/File/File5", "", menuSig, fileStackCmdID + 5, 0},
	                                      {"/File/File6", "", menuSig, fileStackCmdID + 6, 0},
	                                      {"/File/File7", "", menuSig, fileStackCmdID + 7, 0},
	                                      {"/File/File8", "", menuSig, fileStackCmdID + 8, 0},
	                                      {"/File/File9", "", menuSig, fileStackCmdID + 9, 0},
	                                      {"/File/sep3", NULL, NULL, 0, "<Separator>"},
	                                      {"/File/E_xit", "", menuSig, IDM_QUIT, 0},

	                                      {"/_Edit", NULL, NULL, 0, "<Branch>"},
	                                      {"/Edit/_Undo", "<control>Z", menuSig, IDM_UNDO, 0},

	                                      {"/Edit/_Redo", "<control>Y", menuSig, IDM_REDO, 0},
	                                      {"/Edit/sep1", NULL, NULL, 0, "<Separator>"},
	                                      {"/Edit/Cu_t", "<control>X", menuSig, IDM_CUT, 0},
	                                      {"/Edit/_Copy", "<control>C", menuSig, IDM_COPY, 0},
	                                      {"/Edit/_Paste", "<control>V", menuSig, IDM_PASTE, 0},
	                                      {"/Edit/Duplicat_e", "<control>D", menuSig, IDM_DUPLICATE, 0},
	                                      {"/Edit/_Delete", "Del", menuSig, IDM_CLEAR, 0},
	                                      {"/Edit/Select _All", "<control>A", menuSig, IDM_SELECTALL, 0},
	                                      {"/Edit/sep2", NULL, NULL, 0, "<Separator>"},
	                                      {"/Edit/Match _Brace", "<control>E", menuSig, IDM_MATCHBRACE, 0},
	                                      {"/Edit/Select t_o Brace", "<control><shift>E", menuSig, IDM_SELECTTOBRACE, 0},
	                                      {"/Edit/S_how Calltip", "<control><shift>space", menuSig, IDM_SHOWCALLTIP, 0},
	                                      {"/Edit/Complete S_ymbol", "<control>I", menuSig, IDM_COMPLETE, 0},
	                                      {"/Edit/Complete _Word", "<control>Return", menuSig, IDM_COMPLETEWORD, 0},
	                                      {"/Edit/Expand Abbre_viation", "<control>B", menuSig, IDM_ABBREV, 0},
	                                      //~ {"/Edit/_Insert Abbreviation", "<control>D", menuSig, IDM_INS_ABBREV, 0},
	                                      {"/Edit/Block Co_mment or Uncomment", "<control>Q", menuSig, IDM_BLOCK_COMMENT, 0},
	                                      {"/Edit/Bo_x Comment", "<control><shift>B", menuSig, IDM_BOX_COMMENT, 0},
	                                      {"/Edit/Stream Comme_nt", "<control><shift>Q", menuSig, IDM_STREAM_COMMENT, 0},
	                                      {"/Edit/Make _Selection Uppercase", "<control><shift>U", menuSig, IDM_UPRCASE, 0},
	                                      {"/Edit/Make Selection _Lowercase", "<control>U", menuSig, IDM_LWRCASE, 0},
	                                      {"/Edit/Para_graph", NULL, NULL, 0, "<Branch>"},
	                                      {"/Edit/Para_graph/_Join", NULL, menuSig, IDM_JOIN, 0},
	                                      {"/Edit/Para_graph/_Split", NULL, menuSig, IDM_SPLIT, 0},

	                                      {"/_Search", NULL, NULL, 0, "<Branch>"},
	                                      {"/Search/_Find...", "<control>F", menuSig, IDM_FIND, 0},
	                                      {"/Search/Find _Next", "F3", menuSig, IDM_FINDNEXT, 0},
	                                      {"/Search/Find Previou_s", "<shift>F3", menuSig, IDM_FINDNEXTBACK, 0},
	                                      {"/Search/F_ind in Files...", "<control><shift>F", menuSig, IDM_FINDINFILES, 0},
	                                      {"/Search/R_eplace...", "<control>H", menuSig, IDM_REPLACE, 0},
	                                      {"/Search/Incrementa_l Search", "<control><alt>I", menuSig, IDM_INCSEARCH, 0},
	                                      {"/Search/sep3", NULL, NULL, 0, "<Separator>"},
	                                      {"/Search/_Go To...", "<control>G", menuSig, IDM_GOTO, 0},
	                                      {"/Search/Next Book_mark", "F2", menuSig, IDM_BOOKMARK_NEXT, 0},
	                                      {"/Search/Pre_vious Bookmark", "<shift>F2", menuSig, IDM_BOOKMARK_PREV, 0},
	                                      {"/Search/Toggle Bookmar_k", "<control>F2", menuSig, IDM_BOOKMARK_TOGGLE, 0},
	                                      {"/Search/_Clear All Bookmarks", "", menuSig, IDM_BOOKMARK_CLEARALL, 0},

	                                      {"/_View", NULL, NULL, 0, "<Branch>"},
	                                      {"/View/Toggle _current fold", "", menuSig, IDM_EXPAND, 0},
	                                      {"/View/Toggle _all folds", "", menuSig, IDM_TOGGLE_FOLDALL, 0},
	                                      {"/View/sep1", NULL, NULL, 0, "<Separator>"},
	                                      {"/View/Full Scree_n", "F11", menuSig, IDM_FULLSCREEN, "<CheckItem>"},
	                                      {"/View/_Tool Bar", "", menuSig, IDM_VIEWTOOLBAR, "<CheckItem>"},
	                                      {"/View/Tab _Bar", "", menuSig, IDM_VIEWTABBAR, "<CheckItem>"},
	                                      {"/View/_Status Bar", "", menuSig, IDM_VIEWSTATUSBAR, "<CheckItem>"},
	                                      {"/View/sep2", NULL, NULL, 0, "<Separator>"},
	                                      {"/View/_Whitespace", "<control><shift>A", menuSig, IDM_VIEWSPACE, "<CheckItem>"},
	                                      {"/View/_End of Line", "<control><shift>D", menuSig, IDM_VIEWEOL, "<CheckItem>"},
	                                      {"/View/_Indentation Guides", NULL, menuSig, IDM_VIEWGUIDES, "<CheckItem>"},
	                                      {"/View/_Line Numbers", "", menuSig, IDM_LINENUMBERMARGIN, "<CheckItem>"},
	                                      {"/View/_Margin", NULL, menuSig, IDM_SELMARGIN, "<CheckItem>"},
	                                      {"/View/_Fold Margin", NULL, menuSig, IDM_FOLDMARGIN, "<CheckItem>"},
	                                      {"/View/_Output", "F8", menuSig, IDM_TOGGLEOUTPUT, "<CheckItem>"},
	                                      {"/View/_Parameters", NULL, menuSig, IDM_TOGGLEPARAMETERS, "<CheckItem>"},

	                                      {"/_Tools", NULL, NULL, 0, "<Branch>"},
	                                      {"/Tools/_Compile", "<control>F7", menuSig, IDM_COMPILE, 0},
	                                      {"/Tools/_Build", "F7", menuSig, IDM_BUILD, 0},
	                                      {"/Tools/_Go", "F5", menuSig, IDM_GO, 0},

	                                      {"/Tools/Tool0", NULL, menuSig, IDM_TOOLS + 0, 0},
	                                      {"/Tools/Tool1", NULL, menuSig, IDM_TOOLS + 1, 0},
	                                      {"/Tools/Tool2", NULL, menuSig, IDM_TOOLS + 2, 0},
	                                      {"/Tools/Tool3", NULL, menuSig, IDM_TOOLS + 3, 0},
	                                      {"/Tools/Tool4", NULL, menuSig, IDM_TOOLS + 4, 0},
	                                      {"/Tools/Tool5", NULL, menuSig, IDM_TOOLS + 5, 0},
	                                      {"/Tools/Tool6", NULL, menuSig, IDM_TOOLS + 6, 0},
	                                      {"/Tools/Tool7", NULL, menuSig, IDM_TOOLS + 7, 0},
	                                      {"/Tools/Tool8", NULL, menuSig, IDM_TOOLS + 8, 0},
	                                      {"/Tools/Tool9", NULL, menuSig, IDM_TOOLS + 9, 0},

	                                      {"/Tools/Tool10", NULL, menuSig, IDM_TOOLS + 10, 0},
	                                      {"/Tools/Tool11", NULL, menuSig, IDM_TOOLS + 11, 0},
	                                      {"/Tools/Tool12", NULL, menuSig, IDM_TOOLS + 12, 0},
	                                      {"/Tools/Tool13", NULL, menuSig, IDM_TOOLS + 13, 0},
	                                      {"/Tools/Tool14", NULL, menuSig, IDM_TOOLS + 14, 0},
	                                      {"/Tools/Tool15", NULL, menuSig, IDM_TOOLS + 15, 0},
	                                      {"/Tools/Tool16", NULL, menuSig, IDM_TOOLS + 16, 0},
	                                      {"/Tools/Tool17", NULL, menuSig, IDM_TOOLS + 17, 0},
	                                      {"/Tools/Tool18", NULL, menuSig, IDM_TOOLS + 18, 0},
	                                      {"/Tools/Tool19", NULL, menuSig, IDM_TOOLS + 19, 0},

	                                      {"/Tools/Tool20", NULL, menuSig, IDM_TOOLS + 20, 0},
	                                      {"/Tools/Tool21", NULL, menuSig, IDM_TOOLS + 21, 0},
	                                      {"/Tools/Tool22", NULL, menuSig, IDM_TOOLS + 22, 0},
	                                      {"/Tools/Tool23", NULL, menuSig, IDM_TOOLS + 23, 0},
	                                      {"/Tools/Tool24", NULL, menuSig, IDM_TOOLS + 24, 0},
	                                      {"/Tools/Tool25", NULL, menuSig, IDM_TOOLS + 25, 0},
	                                      {"/Tools/Tool26", NULL, menuSig, IDM_TOOLS + 26, 0},
	                                      {"/Tools/Tool27", NULL, menuSig, IDM_TOOLS + 27, 0},
	                                      {"/Tools/Tool28", NULL, menuSig, IDM_TOOLS + 28, 0},
	                                      {"/Tools/Tool29", NULL, menuSig, IDM_TOOLS + 29, 0},

	                                      {"/Tools/Tool30", NULL, menuSig, IDM_TOOLS + 30, 0},
	                                      {"/Tools/Tool31", NULL, menuSig, IDM_TOOLS + 31, 0},
	                                      {"/Tools/Tool32", NULL, menuSig, IDM_TOOLS + 32, 0},
	                                      {"/Tools/Tool33", NULL, menuSig, IDM_TOOLS + 33, 0},
	                                      {"/Tools/Tool34", NULL, menuSig, IDM_TOOLS + 34, 0},
	                                      {"/Tools/Tool35", NULL, menuSig, IDM_TOOLS + 35, 0},
	                                      {"/Tools/Tool36", NULL, menuSig, IDM_TOOLS + 36, 0},
	                                      {"/Tools/Tool37", NULL, menuSig, IDM_TOOLS + 37, 0},
	                                      {"/Tools/Tool38", NULL, menuSig, IDM_TOOLS + 38, 0},
	                                      {"/Tools/Tool39", NULL, menuSig, IDM_TOOLS + 39, 0},

	                                      {"/Tools/Tool40", NULL, menuSig, IDM_TOOLS + 40, 0},
	                                      {"/Tools/Tool41", NULL, menuSig, IDM_TOOLS + 41, 0},
	                                      {"/Tools/Tool42", NULL, menuSig, IDM_TOOLS + 42, 0},
	                                      {"/Tools/Tool43", NULL, menuSig, IDM_TOOLS + 43, 0},
	                                      {"/Tools/Tool44", NULL, menuSig, IDM_TOOLS + 44, 0},
	                                      {"/Tools/Tool45", NULL, menuSig, IDM_TOOLS + 45, 0},
	                                      {"/Tools/Tool46", NULL, menuSig, IDM_TOOLS + 46, 0},
	                                      {"/Tools/Tool47", NULL, menuSig, IDM_TOOLS + 47, 0},
	                                      {"/Tools/Tool48", NULL, menuSig, IDM_TOOLS + 48, 0},
	                                      {"/Tools/Tool49", NULL, menuSig, IDM_TOOLS + 49, 0},


	                                      {"/Tools/_Stop Executing", "<control>.", menuSig, IDM_STOPEXECUTE, NULL},
	                                      {"/Tools/sep1", NULL, NULL, 0, "<Separator>"},
	                                      {"/Tools/_Next Message", "F4", menuSig, IDM_NEXTMSG, 0},
	                                      {"/Tools/_Previous Message", "<shift>F4", menuSig, IDM_PREVMSG, 0},
	                                      {"/Tools/Clear _Output", "<shift>F5", menuSig, IDM_CLEAROUTPUT, 0},
	                                      {"/Tools/_Switch Pane", "<control>F6", menuSig, IDM_SWITCHPANE, 0},
	                                  };

	SciTEItemFactoryEntry menuItemsOptions[] = {
	            {"/_Options", NULL, NULL, 0, "<Branch>"},
	            {"/Options/Vertical _Split", "", menuSig, IDM_SPLITVERTICAL, "<CheckItem>"},
	            {"/Options/_Wrap", "", menuSig, IDM_WRAP, "<CheckItem>"},
	            {"/Options/Wrap Out_put", "", menuSig, IDM_WRAPOUTPUT, "<CheckItem>"},
	            {"/Options/_Read-Only", "", menuSig, IDM_READONLY, "<CheckItem>"},
	            {"/Options/sep1", NULL, NULL, 0, "<Separator>"},
	            {"/Options/_Line End Characters", "", 0, 0, "<Branch>"},
	            {"/Options/Line End Characters/CR _+ LF", "", menuSig, IDM_EOL_CRLF, "<RadioItem>"},
	            {"/Options/Line End Characters/_CR", "", menuSig, IDM_EOL_CR, "/Options/Line End Characters/CR + LF"},
	            {"/Options/Line End Characters/_LF", "", menuSig, IDM_EOL_LF, "/Options/Line End Characters/CR + LF"},
	            {"/Options/_Convert Line End Characters", "", menuSig, IDM_EOL_CONVERT, 0},
	            {"/Options/sep2", NULL, NULL, 0, "<Separator>"},
	            {"/Options/Change Inden_tation Settings", "<control><shift>I", menuSig, IDM_TABSIZE, 0},
	            {"/Options/Use _Monospaced Font", "<control>F11", menuSig, IDM_MONOFONT, "<CheckItem>"},
	            {"/Options/sep3", NULL, NULL, 0, "<Separator>"},
	            {"/Options/Open Local _Options File", "", menuSig, IDM_OPENLOCALPROPERTIES, 0},
	            {"/Options/Open _Directory Options File", "", menuSig, IDM_OPENDIRECTORYPROPERTIES, 0},
	            {"/Options/Open _User Options File", "", menuSig, IDM_OPENUSERPROPERTIES, 0},
	            {"/Options/Open _Global Options File", "", menuSig, IDM_OPENGLOBALPROPERTIES, 0},
	            {"/Options/Open A_bbreviations File", "", menuSig, IDM_OPENABBREVPROPERTIES, 0},
	            {"/Options/Open Lua Startup Scr_ipt", "", menuSig, IDM_OPENLUAEXTERNALFILE, 0},
	            {"/Options/sep4", NULL, NULL, 0, "<Separator>"},
	            {"/Options/_Edit Properties", "", 0, 0, "<Branch>"},
	        };

	SciTEItemFactoryEntry menuItemsLanguage[] = {
	            {"/_Language", NULL, NULL, 0, "<Branch>"},
	        };

	SciTEItemFactoryEntry menuItemsBuffer[] = {
	                                            {"/_Buffers", NULL, NULL, 0, "<Branch>"},
	                                            {"/Buffers/_Previous", "<shift>F6", menuSig, IDM_PREVFILE, 0},
	                                            {"/Buffers/_Next", "F6", menuSig, IDM_NEXTFILE, 0},
	                                            {"/Buffers/_Close All", "", menuSig, IDM_CLOSEALL, 0},
	                                            {"/Buffers/_Save All", "", menuSig, IDM_SAVEALL, 0},
	                                            {"/Buffers/sep2", NULL, NULL, 0, "<Separator>"},
	                                            {"/Buffers/Buffer0", "<alt>1", menuSig, bufferCmdID + 0, "<RadioItem>"},
	                                            {"/Buffers/Buffer1", "<alt>2", menuSig, bufferCmdID + 1, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer2", "<alt>3", menuSig, bufferCmdID + 2, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer3", "<alt>4", menuSig, bufferCmdID + 3, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer4", "<alt>5", menuSig, bufferCmdID + 4, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer5", "<alt>6", menuSig, bufferCmdID + 5, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer6", "<alt>7", menuSig, bufferCmdID + 6, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer7", "<alt>8", menuSig, bufferCmdID + 7, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer8", "<alt>9", menuSig, bufferCmdID + 8, "/Buffers/Buffer0"},
	                                            {"/Buffers/Buffer9", "<alt>0", menuSig, bufferCmdID + 9, "/Buffers/Buffer0"},
	                                        };

	SciTEItemFactoryEntry menuItemsHelp[] = {
	                                          {"/_Help", NULL, NULL, 0, "<Branch>"},
	                                          {"/Help/_Help", "F1", menuSig, IDM_HELP, 0},
	                                          {"/Help/_SciTE Help", "", menuSig, IDM_HELP_SCITE, 0},
	                                          {"/Help/_About SciTE", "", menuSig, IDM_ABOUT, 0},
	                                      };

	accelGroup = gtk_accel_group_new();
	itemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelGroup);
	CreateTranslatedMenu(ELEMENTS(menuItems), menuItems);
	CreateTranslatedMenu(ELEMENTS(menuItemsOptions), menuItemsOptions,
	                     50, "/Options/Edit Properties/Props", 0, IDM_IMPORT, 0);
	CreateTranslatedMenu(ELEMENTS(menuItemsLanguage), menuItemsLanguage,
	                     100, "/Language/Language", 0, IDM_LANGUAGE, 0);
	if (props.GetInt("buffers") > 1)
		CreateTranslatedMenu(ELEMENTS(menuItemsBuffer), menuItemsBuffer,
		                     30, "/Buffers/Buffer", 10, bufferCmdID, "/Buffers/Buffer0");
	CreateTranslatedMenu(ELEMENTS(menuItemsHelp), menuItemsHelp);
	gtk_window_add_accel_group(GTK_WINDOW(PWidget(wSciTE)), accelGroup);
}

void Strip::Show() {
	gtk_widget_show(PWidget(*this));
	visible = true;
}

void Strip::Close() {
	gtk_widget_hide(PWidget(*this));
	visible = false;
}

static char KeyFromLabel(std::string label) {
	if (!label.empty()) {
		size_t posMnemonic = label.find('_');
		return tolower(label[posMnemonic + 1]);
	}
	return 0;
}

bool Strip::KeyDown(GdkEventKey *event) {
	bool retVal = false;

	if (visible) {
		if (event->keyval == GDK_Escape) {
			Close();
			return true;
		}

		if (event->state & GDK_MOD1_MASK) {
			GList *childWidgets = gtk_container_children(GTK_CONTAINER(GetID()));
			for (GList *child = g_list_first(childWidgets); child; child = g_list_next(child)) {
				GtkWidget **w = (GtkWidget **)child;
				std::string name = gtk_widget_get_name(*w);
				std::string label;
				if (name == "GtkButton" || name == "GtkCheckButton") {
					label = gtk_button_get_label(GTK_BUTTON(*w));
				} else if (name == "GtkLabel") {
					label = gtk_label_get_label(GTK_LABEL(*w));
				}
				char key = KeyFromLabel(label);
				if (static_cast<unsigned int>(key) == event->keyval) {
					//fprintf(stderr, "%p %s %s %c\n", *w, name.c_str(), label.c_str(), key);
					if (name == "GtkButton" || name == "GtkCheckButton") {
						gtk_button_clicked(GTK_BUTTON(*w));
					} else if (name == "GtkLabel") {
						// Only ever use labels to label ComboBoxEntry
						GtkWidget *pwidgetSelect = gtk_label_get_mnemonic_widget(GTK_LABEL(*w));
						if (pwidgetSelect) {
							gtk_widget_grab_focus(pwidgetSelect);
						}
					}
					retVal = true;
					break;
				}
			}
			g_list_free(childWidgets);
		}
	}
	return retVal;
}

void Strip::MenuSignal(GtkMenuItem *menuItem, Strip *pStrip) {
	sptr_t cmd = (sptr_t)(g_object_get_data(G_OBJECT(menuItem), "CmdNum"));
	pStrip->MenuAction(cmd);
}

void Strip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	allowMenuActions = false;
	GUI::gui_string localised = localiser->Text(label);
	GtkWidget *menuItem = gtk_check_menu_item_new_with_mnemonic(localised.c_str());
	gtk_menu_shell_append(GTK_MENU_SHELL(popup.GetID()), menuItem);
	g_object_set_data(G_OBJECT(menuItem), "CmdNum", reinterpret_cast<void *>((sptr_t)(cmd)));
	g_signal_connect(G_OBJECT(menuItem),"activate", G_CALLBACK(MenuSignal), this);
	gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(menuItem), checked ? TRUE : FALSE);
	allowMenuActions = true;
}

void Strip::ChildFocus(GtkWidget *widget) {
	childHasFocus = widget != 0;
	pSciTEGTK->CheckMenusClipboard();
}

gboolean Strip::ChildFocusSignal(GtkContainer */*container*/, GtkWidget *widget, Strip *pStrip) {
	pStrip->ChildFocus(widget);
	return FALSE;
}

gboolean Strip::FocusSignal(GtkWidget */*widget*/, GtkDirectionType direction, Strip *pStrip) {
	return pStrip->Focus(direction);
}

bool Strip::VisibleHasFocus() {
	return visible && childHasFocus;
}

const int stripIconWidth = 16;
const int stripButtonWidth = 16 + 3 * 2 + 1;
const int stripButtonPitch = stripButtonWidth;

static void GreyToAlpha(GdkPixbuf *ppb) {
	guchar *pixels = gdk_pixbuf_get_pixels(ppb);
	int rowStride = gdk_pixbuf_get_rowstride(ppb);
	int width = gdk_pixbuf_get_width(ppb);
	int height = gdk_pixbuf_get_height(ppb);
	for (int y =0; y<height; y++) {
		guchar *pixelsRow = pixels + rowStride * y;
		for (int x =0; x<width; x++) {
			guchar alpha = pixelsRow[0];
			pixelsRow[3] = 255 - alpha;
			pixelsRow[0] = 0;
			pixelsRow[1] = 0;
			pixelsRow[2] = 0;
			pixelsRow += 4;
		}
	}
}

/* XPM */
static const char * word1_x_xpm[] = {
"16 16 15 1",
" 	c #FFFFFF",
".	c #000000",
"+	c #E1E1E1",
"@	c #D9D9D9",
"#	c #4D4D4D",
"$	c #7C7C7C",
"%	c #D0D0D0",
"&	c #A7A7A7",
"*	c #8C8C8C",
"=	c #BDBDBD",
"-	c #B2B2B2",
";	c #686868",
">	c #F0F0F0",
",	c #9A9A9A",
"'	c #C7C7C7",
"                ",
"                ",
"                ",
"                ",
"                ",
"                ",
"               .",
".+.+.@##@ .$%#&.",
"*=.-*;++; .>;++.",
",*.$,.  . . .  .",
"=#,.=;++; . ;++.",
"'.=.'@##@ . @#&.",
"                ",
".              .",
".              .",
"................"};

/* XPM */
static const char * case_x_xpm[] = {
"16 16 12 1",
" 	c #FFFFFF",
".	c #BDBDBD",
"+	c #4D4D4D",
"@	c #000000",
"#	c #D0D0D0",
"$	c #C7C7C7",
"%	c #7C7C7C",
"&	c #B2B2B2",
"*	c #8C8C8C",
"=	c #E1E1E1",
"-	c #686868",
";	c #9A9A9A",
"                ",
"                ",
"                ",
"                ",
"   .+@@         ",
"  #@$           ",
"  %%      &+@@  ",
"  @*     &+=    ",
"  @*     @&     ",
"  -%     @&     ",
"  .@#    ;+=    ",
"   &+@@   ;@@@  ",
"                ",
"                ",
"                ",
"                "};

/* XPM */
static const char * regex_x_xpm[] = {
"16 16 11 1",
" 	c #FFFFFF",
".	c #888888",
"+	c #696969",
"@	c #000000",
"#	c #E0E0E0",
"$	c #484848",
"%	c #B6B6B6",
"&	c #C4C4C4",
"*	c #787878",
"=	c #383838",
"-	c #D3D3D3",
"                ",
"                ",
"                ",
"  .+        @   ",
" #$$#     % + % ",
" +&&+    #*=@=*#",
"-$  $&     +++  ",
"$&  &$    -$ =- ",
"                ",
"                ",
"      %@%       ",
"      %@%       ",
"                ",
"                ",
"                ",
"                "};

/* XPM */
static const char * backslash_x_xpm[] = {
"16 16 15 1",
" 	c #FFFFFF",
".	c #141414",
"+	c #585858",
"@	c #A6A6A6",
"#	c #B6B6B6",
"$	c #272727",
"%	c #E0E0E0",
"&	c #979797",
"*	c #000000",
"=	c #696969",
"-	c #484848",
";	c #787878",
">	c #D3D3D3",
",	c #383838",
"'	c #888888",
"                ",
"                ",
"                ",
".       .       ",
"+@      +@   #  ",
"#+      #+   $  ",
" .%  .&* .% =*--",
" ;&  *&  ;&  *  ",
" >,  *   >,  *  ",
"  $> *    $> *  ",
"  &; *    &; *% ",
"  %. *    %. '*.",
"                ",
"                ",
"                ",
"                "};

/* XPM */
static const char * around_x_xpm[] = {
"16 16 2 1",
" 	c #FFFFFF",
".	c #000000",
"                ",
"      .....     ",
"     .......    ",
"    ...   ...   ",
"            ..  ",
"            ..  ",
"   ..        .. ",
"  ....       .. ",
" ......      .. ",
"   ..       ..  ",
"   ...      ..  ",
"    ...   ...   ",
"     .......    ",
"      .....     ",
"                ",
"                "};

/* XPM */
static const char * up_x_xpm[] = {
"16 16 8 1",
" 	c None",
".	c #FFFFFF",
"+	c #9C9C9C",
"@	c #000000",
"#	c #747474",
"$	c #484848",
"%	c #DFDFDF",
"&	c #BFBFBF",
"................",
"................",
"........+.......",
".......@@#......",
"......@@@@#.....",
".....@@$@$@$....",
"....@@%#@&+@$...",
"...@@%.#@&.&@$..",
"..#@%..#@&..&@&.",
"..#....#@&...&&.",
".......#@&......",
".......#@&......",
".......#@&......",
".......#@&......",
".......#@&......",
"................"};

void WCheckDraw::Create(const char **xpmImage, const GUI::gui_string &toolTip, bool *pControlVariable_, Strip *pstrip_) {
	pControlVariable = pControlVariable_;
	pstrip = pstrip_;
	GdkPixbuf *pbGrey = gdk_pixbuf_new_from_xpm_data(xpmImage);
	// Give it an alpha channel
	pbAlpha = gdk_pixbuf_add_alpha(pbGrey, TRUE, 0xff, 0xff, 0);
	// Convert the grey to alpha and make black
	GreyToAlpha(pbAlpha);
	g_object_unref(pbGrey);

	GtkWidget *da = gtk_drawing_area_new();
	GTK_WIDGET_SET_FLAGS(da, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS(da, GTK_SENSITIVE);
	gtk_widget_set_events(da,
			      GDK_EXPOSURE_MASK
	                      | GDK_FOCUS_CHANGE_MASK
			      | GDK_ENTER_NOTIFY_MASK
			      | GDK_LEAVE_NOTIFY_MASK
			      | GDK_BUTTON_PRESS_MASK
			      | GDK_BUTTON_RELEASE_MASK
			      | GDK_POINTER_MOTION_MASK
			      | GDK_POINTER_MOTION_HINT_MASK
			     );
	gtk_widget_set_size_request(da, stripButtonPitch, 20);
	SetID(da);
	GUI::gui_string toolTipNoMnemonic = toolTip;
	Substitute(toolTipNoMnemonic, "_", "");
#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(da, toolTipNoMnemonic.c_str());
#endif
	g_signal_connect(G_OBJECT(da), "focus-in-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "focus-out-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "button_press_event", G_CALLBACK(ButtonsPress), this);
	g_signal_connect(G_OBJECT(da), "enter-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "leave-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "key_press_event", G_CALLBACK(KeyDown), this);
	g_signal_connect(G_OBJECT(da), "expose_event", G_CALLBACK(ExposeEvent), this);
}

void WCheckDraw::Toggle() {
	*pControlVariable = !*pControlVariable;
	InvalidateAll();
}

gboolean WCheckDraw::Focus(GtkWidget */*widget*/, GdkEventFocus */*event*/, WCheckDraw *pcd) {
	pcd->InvalidateAll();
	return FALSE;
}

gboolean WCheckDraw::KeyDown(GtkWidget */*widget*/, GdkEventKey *event, WCheckDraw *pcd) {
	if (event->keyval == ' ') {
		pcd->Toggle();
	}
	return FALSE;
}

gint WCheckDraw::Press(GtkWidget *widget, GdkEventButton *event) {
	if (event->button == 3) {
		// PopUp menu
		pstrip->ShowPopup();
	} else {
		gtk_widget_grab_focus(widget);
		Toggle();
	}
	return TRUE;
}

gint WCheckDraw::ButtonsPress(GtkWidget *widget, GdkEventButton *event, WCheckDraw *pcd) {
	return pcd->Press(widget, event);
}

gboolean WCheckDraw::MouseEnterLeave(GtkWidget */*widget*/, GdkEventCrossing *event, WCheckDraw *pcd) {
	pcd->over = event->type == GDK_ENTER_NOTIFY;
	pcd->InvalidateAll();
	return FALSE;
}

gboolean WCheckDraw::Expose(GtkWidget *widget, GdkEventExpose */*event*/) {
	GdkRectangle area;
	area.x = 0;
	area.y = 0;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;
	int heightOffset = (area.height - stripButtonWidth) / 2;
	if (heightOffset < 0)
		heightOffset = 0;
	GdkGC *gcDraw = gdk_gc_new(GDK_DRAWABLE(widget->window));
	bool active = *pControlVariable;
	GtkStateType state = active ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL;
	GtkShadowType shadow = GTK_SHADOW_IN;
	if (over) {
		state = GTK_STATE_PRELIGHT;
		shadow = GTK_SHADOW_OUT;
	}
	if (active || over)
		gtk_paint_box(pstrip->ButtonStyle(), widget->window,
			       state,
			       shadow,
			       &area, widget, const_cast<char *>("button"),
			       0, 0,
			       area.width, area.height);
	if (HasFocus()) {
		// Draw focus inset by 2 pixels
		gtk_paint_focus(pstrip->ButtonStyle(), widget->window,
			       state,
			       &area, widget, const_cast<char *>("button"),
			       2, 2,
			       area.width-4, area.height-4);
	}

	int activeOffset = active ? 1 : 0;
	gdk_pixbuf_render_to_drawable(pbAlpha,
		widget->window,
		gcDraw,
		0, 0,
		1 + 2 + activeOffset, 3 + heightOffset + activeOffset,
		stripIconWidth, stripIconWidth,
		GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(gcDraw);
	return TRUE;
}

gboolean WCheckDraw::ExposeEvent(GtkWidget *widget, GdkEventExpose *event, WCheckDraw *pcd) {
	return pcd->Expose(widget, event);
}

void FindStrip::Creation(GtkWidget *boxMain) {
	Table table(1, 10);
	SetID(table.Widget());
	gtk_container_set_border_width(GTK_CONTAINER(GetID()), 1);
	gtk_box_pack_start(GTK_BOX(boxMain), GTK_WIDGET(GetID()), FALSE, FALSE, 0);
	wStaticFind.Create(localiser->Text(searchText).c_str());
	table.Label(wStaticFind);

	g_signal_connect(G_OBJECT(GetID()), "set-focus-child", G_CALLBACK(ChildFocusSignal), this);
	g_signal_connect(G_OBJECT(GetID()), "focus", G_CALLBACK(FocusSignal), this);

	wText.Create();
	table.Add(wText, 1, true, 0, 0);

	gtk_widget_show(wText);

	gtk_widget_show(GTK_WIDGET(GetID()));

	gtk_signal_connect(GTK_OBJECT(wText.Entry()), "key-press-event",
		GtkSignalFunc(EscapeSignal), this);

	gtk_signal_connect(GTK_OBJECT(wText.Entry()), "activate",
		GtkSignalFunc(ActivateSignal), this);

	gtk_label_set_mnemonic_widget(GTK_LABEL(wStaticFind.GetID()), GTK_WIDGET(wText.Entry()));

	static ObjectSignal<FindStrip, &FindStrip::FindNextCmd> sigFindNext;
	wButton.Create(localiser->Text("_Find Next"), GtkSignalFunc(sigFindNext.Function), this);
	table.Add(wButton, 1, false, 0, 0);

	static ObjectSignal<FindStrip, &FindStrip::MarkAllCmd> sigMarkAll;
	wButtonMarkAll.Create(localiser->Text("_Mark All"), GtkSignalFunc(sigMarkAll.Function), this);
	table.Add(wButtonMarkAll, 1, false, 0, 0);

	wCheck[0].Create(word1_x_xpm, localiser->Text(toggles[Toggle::tWord].label), &pSciTEGTK->wholeWord, this);
	wCheck[1].Create(case_x_xpm, localiser->Text(toggles[Toggle::tCase].label), &pSciTEGTK->matchCase, this);
	wCheck[2].Create(regex_x_xpm, localiser->Text(toggles[Toggle::tRegExp].label), &pSciTEGTK->regExp, this);
	wCheck[3].Create(backslash_x_xpm, localiser->Text(toggles[Toggle::tBackslash].label), &pSciTEGTK->unSlash, this);
	wCheck[4].Create(around_x_xpm, localiser->Text(toggles[Toggle::tWrap].label), &pSciTEGTK->wrapFind, this);
	wCheck[5].Create(up_x_xpm, localiser->Text(toggles[Toggle::tUp].label), &pSciTEGTK->reverseFind, this);
	for (int i=0;i<checks;i++)
		table.Add(wCheck[i], 1, false, 0, 0);
}

void FindStrip::Destruction() {
}

void FindStrip::Show() {
	Strip::Show();

	int buttonHeight = pSciTEGTK->props.GetInt("strip.button.height", -1);
	gtk_widget_set_size_request(wButton, -1, buttonHeight);
	gtk_widget_set_size_request(wButtonMarkAll, -1, buttonHeight);
	gtk_widget_set_size_request(wText, widthCombo, buttonHeight);
	gtk_widget_set_size_request(GTK_WIDGET(wText.Entry()), -1, buttonHeight);
	gtk_widget_set_size_request(wStaticFind, -1, heightStatic);
	for (int i=0; i<checks; i++)
		gtk_widget_set_size_request(wCheck[i], stripButtonPitch, buttonHeight);

	FillComboFromMemory(wText, pSciTEGTK->memFinds);

	gtk_entry_set_text(wText.Entry(), pSciTEGTK->findWhat.c_str());
	gtk_entry_select_region(wText.Entry(), 0, pSciTEGTK->findWhat.length());

	gtk_widget_grab_focus(GTK_WIDGET(wText.Entry()));
}

void FindStrip::Close() {
	if (visible) {
		Strip::Close();
		SetFocus(pSciTEGTK->wEditor);
	}
}

bool FindStrip::KeyDown(GdkEventKey *event) {
	if (visible) {
		if (Strip::KeyDown(event))
			return true;
		if (event->state & GDK_MOD1_MASK) {
			for (int i=Toggle::tWord; i<=Toggle::tUp; i++) {
				GUI::gui_string localised = localiser->Text(toggles[i].label);
				char key = KeyFromLabel(localised);
				if (static_cast<unsigned int>(key) == event->keyval) {
					wCheck[i].Toggle();
					return true;
				}
			}
		}
	}
	return false;
}

void FindStrip::MenuAction(guint action) {
	if (allowMenuActions) {
		pSciTEGTK->FlagFromCmd(action) = !pSciTEGTK->FlagFromCmd(action);
		InvalidateAll();
	}
}

void FindStrip::ActivateSignal(GtkWidget *, FindStrip *pStrip) {
	pStrip->FindNextCmd();
}

gboolean FindStrip::EscapeSignal(GtkWidget *w, GdkEventKey *event, FindStrip *pStrip) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key-press-event");
		pStrip->Close();
	}
	return FALSE;
}

void FindStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=Toggle::tWord; i<=Toggle::tUp; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSciTEGTK->FlagFromCmd(toggles[i].cmd));
	}
	GUI::Rectangle rcButton = wCheck[0].GetPosition();
	GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

void FindStrip::FindNextCmd() {
	const char *findEntry = wText.Text();
	pSciTEGTK->findWhat = findEntry;
	pSciTEGTK->memFinds.Insert(pSciTEGTK->findWhat);
	if (pSciTEGTK->findWhat[0]) {
		pSciTEGTK->FindNext(pSciTEGTK->reverseFind);
	}
	Close();
}

void FindStrip::MarkAllCmd() {
	const char *findEntry = wText.Text();
	pSciTEGTK->findWhat = findEntry;
	pSciTEGTK->memFinds.Insert(pSciTEGTK->findWhat);
	pSciTEGTK->MarkAll();
	pSciTEGTK->FindNext(pSciTEGTK->reverseFind);
	Close();
}

GtkStyle *FindStrip::ButtonStyle() {
	return PWidget(wButton)->style;
}

gboolean FindStrip::Focus(GtkDirectionType direction) {
	const int lastFocusCheck = 5;
	if ((direction == GTK_DIR_TAB_BACKWARD) && wText.HasFocusOnSelfOrChild()) {
		gtk_widget_grab_focus(wCheck[lastFocusCheck]);
		return TRUE;
	} else if ((direction == GTK_DIR_TAB_FORWARD) && wCheck[lastFocusCheck].HasFocus()) {
		gtk_widget_grab_focus(GTK_WIDGET(wText.Entry()));
		return TRUE;
	}
	return FALSE;
}

void ReplaceStrip::Creation(GtkWidget *boxMain) {
	Table tableReplace(2, 7);
	SetID(tableReplace.Widget());
	tableReplace.PackInto(GTK_BOX(boxMain), false);

	wStaticFind.Create(localiser->Text(searchText));
	tableReplace.Label(wStaticFind);

	g_signal_connect(G_OBJECT(GetID()), "set-focus-child", G_CALLBACK(ChildFocusSignal), this);
	g_signal_connect(G_OBJECT(GetID()), "focus", G_CALLBACK(FocusSignal), this);

	wText.Create();
	tableReplace.Add(wText, 1, true, 0, 0);
	wText.Show();

	gtk_signal_connect(GTK_OBJECT(wText.Entry()), "key-press-event",
		GtkSignalFunc(EscapeSignal), this);

	gtk_signal_connect(GTK_OBJECT(wText.Entry()), "activate",
		GtkSignalFunc(ActivateSignal), this);

	gtk_label_set_mnemonic_widget(GTK_LABEL(wStaticFind.GetID()), GTK_WIDGET(wText.Entry()));

	static ObjectSignal<ReplaceStrip, &ReplaceStrip::FindCmd> sigFindNext;
	wButtonFind.Create(localiser->Text("_Find Next"),
			GtkSignalFunc(sigFindNext.Function), this);
	tableReplace.Add(wButtonFind, 1, false, 0, 0);

	static ObjectSignal<ReplaceStrip, &ReplaceStrip::ReplaceAllCmd> sigReplaceAll;
	wButtonReplaceAll.Create(localiser->Text("Replace _All"),
			GtkSignalFunc(sigReplaceAll.Function), this);
	tableReplace.Add(wButtonReplaceAll, 1, false, 0, 0);

	wCheck[0].Create(word1_x_xpm, localiser->Text(toggles[Toggle::tWord].label), &pSciTEGTK->wholeWord, this);
	wCheck[1].Create(case_x_xpm, localiser->Text(toggles[Toggle::tCase].label), &pSciTEGTK->matchCase, this);
	wCheck[2].Create(regex_x_xpm, localiser->Text(toggles[Toggle::tRegExp].label), &pSciTEGTK->regExp, this);
	wCheck[3].Create(backslash_x_xpm, localiser->Text(toggles[Toggle::tBackslash].label), &pSciTEGTK->unSlash, this);
	wCheck[4].Create(around_x_xpm, localiser->Text(toggles[Toggle::tWrap].label), &pSciTEGTK->wrapFind, this);

	tableReplace.Add(wCheck[0], 1, false, 0, 0);
	tableReplace.Add(wCheck[1], 1, false, 0, 0);
	tableReplace.Add(wCheck[2], 1, false, 0, 0);

	wStaticReplace.Create(localiser->Text(replaceText));
	tableReplace.Label(wStaticReplace);

	wReplace.Create();
	tableReplace.Add(wReplace, 1, true, 0, 0);

	gtk_signal_connect(GTK_OBJECT(wReplace.Entry()), "key-press-event",
		GtkSignalFunc(EscapeSignal), this);

	gtk_signal_connect(GTK_OBJECT(wReplace.Entry()), "activate",
		GtkSignalFunc(ActivateSignal), this);

	//gtk_combo_disable_activate(pComboReplace);

	gtk_label_set_mnemonic_widget(GTK_LABEL(wStaticReplace.GetID()), GTK_WIDGET(wReplace.Entry()));

	static ObjectSignal<ReplaceStrip, &ReplaceStrip::ReplaceCmd> sigReplace;
	wButtonReplace.Create(localiser->Text("_Replace"),
			GtkSignalFunc(sigReplace.Function), this);
	tableReplace.Add(wButtonReplace, 1, false, 0, 0);

	static ObjectSignal<ReplaceStrip, &ReplaceStrip::ReplaceInSelectionCmd> sigReplaceInSelection;
	wButtonReplaceInSelection.Create(localiser->Text("_In Selection"),
			GtkSignalFunc(sigReplaceInSelection.Function), this);
	tableReplace.Add(wButtonReplaceInSelection, 1, false, 0, 0);

	tableReplace.Add(wCheck[3], 1, false, 0, 0);
	tableReplace.Add(wCheck[4], 1, false, 0, 0);

	// Make the fccus chain move down before moving right
	GList *focusChain = 0;
	focusChain = g_list_append(focusChain, wText.Widget());
	focusChain = g_list_append(focusChain, wReplace.Widget());
	focusChain = g_list_append(focusChain, wButtonFind.Widget());
	focusChain = g_list_append(focusChain, wButtonReplace.Widget());
	focusChain = g_list_append(focusChain, wButtonReplaceAll.Widget());
	focusChain = g_list_append(focusChain, wButtonReplaceInSelection.Widget());
	focusChain = g_list_append(focusChain, wCheck[0].Widget());
	focusChain = g_list_append(focusChain, wCheck[3].Widget());
	focusChain = g_list_append(focusChain, wCheck[1].Widget());
	focusChain = g_list_append(focusChain, wCheck[4].Widget());
	focusChain = g_list_append(focusChain, wCheck[2].Widget());
	gtk_container_set_focus_chain(GTK_CONTAINER(GetID()), focusChain);
	g_list_free(focusChain);
}

void ReplaceStrip::Destruction() {
}

void ReplaceStrip::Show() {
	Strip::Show();

	int buttonHeight = pSciTEGTK->props.GetInt("strip.button.height", -1);

	gtk_widget_set_size_request(wButtonFind, -1, buttonHeight);
	gtk_widget_set_size_request(wButtonReplaceAll, -1, buttonHeight);
	gtk_widget_set_size_request(wButtonReplace, -1, buttonHeight);
	gtk_widget_set_size_request(wButtonReplaceInSelection, -1, buttonHeight);

	gtk_widget_set_size_request(wText, widthCombo, buttonHeight);
	gtk_widget_set_size_request(GTK_WIDGET(wText.Entry()), -1, buttonHeight);
	gtk_widget_set_size_request(wReplace, widthCombo, buttonHeight);
	gtk_widget_set_size_request(GTK_WIDGET(wReplace.Entry()), -1, buttonHeight);

	gtk_widget_set_size_request(wStaticFind, -1, heightStatic);
	gtk_widget_set_size_request(wStaticReplace, -1, heightStatic);

	for (int i=0; i<checks; i++)
		gtk_widget_set_size_request(wCheck[i], stripButtonPitch, buttonHeight);

	FillComboFromMemory(wText, pSciTEGTK->memFinds);
	FillComboFromMemory(wReplace, pSciTEGTK->memReplaces);

	gtk_entry_set_text(wText.Entry(), pSciTEGTK->findWhat.c_str());
	gtk_entry_select_region(wText.Entry(), 0, pSciTEGTK->findWhat.length());

	gtk_widget_grab_focus(GTK_WIDGET(wText.Entry()));
}

void ReplaceStrip::Close() {
	if (visible) {
		Strip::Close();
		SetFocus(pSciTEGTK->wEditor);
	}
}

bool ReplaceStrip::KeyDown(GdkEventKey *event) {
	if (visible) {
		if (Strip::KeyDown(event))
			return true;
		if (event->state & GDK_MOD1_MASK) {
			for (int i=Toggle::tWord; i<=Toggle::tUp; i++) {
				GUI::gui_string localised = localiser->Text(toggles[i].label);
				char key = KeyFromLabel(localised);
				if (static_cast<unsigned int>(key) == event->keyval) {
					wCheck[i].Toggle();
					return true;
				}
			}
		}
	}
	return false;
}

void ReplaceStrip::MenuAction(guint action) {
	if (allowMenuActions) {
		pSciTEGTK->FlagFromCmd(action) = !pSciTEGTK->FlagFromCmd(action);
		InvalidateAll();
	}
}

void ReplaceStrip::ActivateSignal(GtkWidget *, ReplaceStrip *pStrip) {
	pStrip->FindCmd();
}

gboolean ReplaceStrip::EscapeSignal(GtkWidget *w, GdkEventKey *event, ReplaceStrip *pStrip) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key-press-event");
		pStrip->Close();
	}
	return FALSE;
}

void ReplaceStrip::GrabFields() {
	const char *findEntry = wText.Text();
	pSciTEGTK->findWhat = findEntry;
	pSciTEGTK->memFinds.Insert(pSciTEGTK->findWhat);
	const char *replaceEntry = wReplace.Text();
	pSciTEGTK->replaceWhat = replaceEntry;
	pSciTEGTK->memReplaces.Insert(pSciTEGTK->replaceWhat);
}

void ReplaceStrip::FindCmd() {
	GrabFields();
	if (pSciTEGTK->findWhat[0]) {
		pSciTEGTK->FindNext(pSciTEGTK->reverseFind);
	}
}

void ReplaceStrip::ReplaceAllCmd() {
	GrabFields();
	if (pSciTEGTK->findWhat[0]) {
		pSciTEGTK->ReplaceAll(false);
	}
}

void ReplaceStrip::ReplaceCmd() {
	GrabFields();
	if (pSciTEGTK->findWhat[0]) {
		pSciTEGTK->ReplaceOnce();
	}
}

void ReplaceStrip::ReplaceInSelectionCmd() {
	GrabFields();
	if (pSciTEGTK->findWhat[0]) {
		pSciTEGTK->ReplaceAll(true);
	}
}

void ReplaceStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=Toggle::tWord; i<=Toggle::tWrap; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSciTEGTK->FlagFromCmd(toggles[i].cmd));
	}
	GUI::Rectangle rcButton = wCheck[0].GetPosition();
	GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

GtkStyle *ReplaceStrip::ButtonStyle() {
	return PWidget(wButtonFind)->style;
}

gboolean ReplaceStrip::Focus(GtkDirectionType direction) {
	const int lastFocusCheck = 2;	// Due to last column starting with the thirs checkbox
	if ((direction == GTK_DIR_TAB_BACKWARD) && wText.HasFocusOnSelfOrChild()) {
		gtk_widget_grab_focus(wCheck[lastFocusCheck]);
		return TRUE;
	} else if ((direction == GTK_DIR_TAB_FORWARD) && wCheck[lastFocusCheck].HasFocus()) {
		gtk_widget_grab_focus(GTK_WIDGET(wText.Entry()));
		return TRUE;
	}
	return FALSE;
}

void SciTEGTK::CreateStrips(GtkWidget *boxMain) {
	findStrip.SetSciTE(this, &localiser);
	findStrip.Creation(boxMain);

	replaceStrip.SetSciTE(this, &localiser);
	replaceStrip.Creation(boxMain);
}

bool SciTEGTK::StripHasFocus() {
	return findStrip.VisibleHasFocus() || replaceStrip.VisibleHasFocus();
}

void SciTEGTK::CreateUI() {
	CreateBuffers();
	wSciTE = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//GTK_WIDGET_UNSET_FLAGS(PWidget(wSciTE), GTK_CAN_FOCUS);
	gtk_window_set_policy(GTK_WINDOW(PWidget(wSciTE)), TRUE, TRUE, FALSE);

	char *gthis = reinterpret_cast<char *>(this);

	gtk_widget_set_events(PWidget(wSciTE),
	                      GDK_EXPOSURE_MASK
	                      | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                     );
	gtk_signal_connect(GTK_OBJECT(PWidget(wSciTE)), "delete_event",
	                   GTK_SIGNAL_FUNC(QuitSignal), gthis);

	gtk_signal_connect(GTK_OBJECT(PWidget(wSciTE)), "key_press_event",
	                   GtkSignalFunc(KeyPress), gthis);

	gtk_signal_connect(GTK_OBJECT(PWidget(wSciTE)), "button_press_event",
	                   GtkSignalFunc(MousePress), gthis);

	gtk_window_set_title(GTK_WINDOW(PWidget(wSciTE)), appName);
	const int useDefault = 0x10000000;
	int left = props.GetInt("position.left", useDefault);
	int top = props.GetInt("position.top", useDefault);
	int width = props.GetInt("position.width", useDefault);
	int height = props.GetInt("position.height", useDefault);
	bool maximize = props.GetInt("position.maximize", 0) ? true : false;
	if (width == -1 || height == -1) {
		maximize = true;
		width = gdk_screen_width() - left - 10;
		height = gdk_screen_height() - top - 30;
	}

	if (props.GetInt("save.position")) {
		left = propsSession.GetInt("position.left", useDefault);
		top = propsSession.GetInt("position.top", useDefault);
		width = propsSession.GetInt("position.width", useDefault);
		height = propsSession.GetInt("position.height", useDefault);
		maximize = propsSession.GetInt("position.maximize", 0) ? true : false;
	}

	fileSelectorWidth = props.GetInt("fileselector.width", fileSelectorWidth);
	fileSelectorHeight = props.GetInt("fileselector.height", fileSelectorHeight);

	GtkWidget *boxMain = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(PWidget(wSciTE)), boxMain);
	GTK_WIDGET_UNSET_FLAGS(boxMain, GTK_CAN_FOCUS);

 	// The Menubar
	CreateMenu();
	if (props.GetInt("menubar.detachable") == 1) {
		GtkWidget *handle_box = gtk_handle_box_new();
		gtk_container_add(GTK_CONTAINER(handle_box),
			gtk_item_factory_get_widget(itemFactory, "<main>"));
		gtk_box_pack_start(GTK_BOX(boxMain),
			handle_box,
			FALSE, FALSE, 0);
	} else {
		gtk_box_pack_start(GTK_BOX(boxMain),
			gtk_item_factory_get_widget(itemFactory, "<main>"),
			FALSE, FALSE, 0);
	}

	// The Toolbar
	wToolBar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(PWidget(wToolBar)), GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(PWidget(wToolBar)), GTK_TOOLBAR_ICONS);
	toolbarDetachable = props.GetInt("toolbar.detachable");
	if (toolbarDetachable == 1) {
		wToolBarBox = gtk_handle_box_new();
		gtk_container_add(GTK_CONTAINER(PWidget(wToolBarBox)), PWidget(wToolBar));
		gtk_box_pack_start(GTK_BOX(boxMain), PWidget(wToolBarBox), FALSE, FALSE, 0);
		gtk_widget_hide(GTK_WIDGET(PWidget(wToolBarBox)));
	} else {
		gtk_box_pack_start(GTK_BOX(boxMain), PWidget(wToolBar), FALSE, FALSE, 0);
		gtk_widget_hide(GTK_WIDGET(PWidget(wToolBar)));
	}
	gtk_container_set_border_width(GTK_CONTAINER(PWidget(wToolBar)), 0);
	tbVisible = false;

	// The Notebook (GTK2)
	wTabBar = gtk_notebook_new();
	GTK_WIDGET_UNSET_FLAGS(PWidget(wTabBar),GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(boxMain),PWidget(wTabBar),FALSE,FALSE,0);
	gtk_signal_connect(GTK_OBJECT(PWidget(wTabBar)),
		"button-release-event", GTK_SIGNAL_FUNC(TabBarReleaseSignal), gthis);
	g_signal_connect(GTK_OBJECT(PWidget(wTabBar)),
		"scroll-event", GTK_SIGNAL_FUNC(TabBarScrollSignal), gthis);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(PWidget(wTabBar)), TRUE);
	tabVisible = false;

	wContent = gtk_fixed_new();
	GTK_WIDGET_UNSET_FLAGS(PWidget(wContent), GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(boxMain), PWidget(wContent), TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(PWidget(wContent)), "size_allocate",
	                   GTK_SIGNAL_FUNC(MoveResize), gthis);

	wEditor.SetID(scintilla_new());
	scintilla_set_id(SCINTILLA(PWidget(wEditor)), IDM_SRCWIN);
	wEditor.Call(SCI_USEPOPUP, 0);
	gtk_fixed_put(GTK_FIXED(PWidget(wContent)), PWidget(wEditor), 0, 0);

	gtk_signal_connect(GTK_OBJECT(PWidget(wEditor)), "command",
	                   GtkSignalFunc(CommandSignal), this);
	gtk_signal_connect(GTK_OBJECT(PWidget(wEditor)), SCINTILLA_NOTIFY,
	                   GtkSignalFunc(NotifySignal), this);

	wDivider = gtk_drawing_area_new();
	gtk_signal_connect(GTK_OBJECT(PWidget(wDivider)), "expose_event",
	                   GtkSignalFunc(DividerExpose), this);
	gtk_signal_connect(GTK_OBJECT(PWidget(wDivider)), "motion_notify_event",
	                   GtkSignalFunc(DividerMotion), this);
	gtk_signal_connect(GTK_OBJECT(PWidget(wDivider)), "button_press_event",
	                   GtkSignalFunc(DividerPress), this);
	gtk_signal_connect(GTK_OBJECT(PWidget(wDivider)), "button_release_event",
	                   GtkSignalFunc(DividerRelease), this);
	gtk_widget_set_events(PWidget(wDivider),
	                      GDK_EXPOSURE_MASK
	                      | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                      | GDK_POINTER_MOTION_MASK
	                      | GDK_POINTER_MOTION_HINT_MASK
	                     );
	gtk_drawing_area_size(GTK_DRAWING_AREA(PWidget(wDivider)), (width == useDefault) ? 100 : width, 10);
	gtk_fixed_put(GTK_FIXED(PWidget(wContent)), PWidget(wDivider), 0, 600);

	wOutput.SetID(scintilla_new());
	scintilla_set_id(SCINTILLA(PWidget(wOutput)), IDM_RUNWIN);
	wOutput.Call(SCI_USEPOPUP, 0);
	gtk_fixed_put(GTK_FIXED(PWidget(wContent)), PWidget(wOutput), (width == useDefault) ? 100 : width, 0);
	gtk_signal_connect(GTK_OBJECT(PWidget(wOutput)), "command",
	                   GtkSignalFunc(CommandSignal), this);
	gtk_signal_connect(GTK_OBJECT(PWidget(wOutput)), SCINTILLA_NOTIFY,
	                   GtkSignalFunc(NotifySignal), this);

	Table table(1, 2);
	wIncrementPanel = table.Widget();
	table.PackInto(GTK_BOX(boxMain), false);
	table.Label(TranslatedLabel("Find:"));

	IncSearchEntry = gtk_entry_new();
	table.Add(IncSearchEntry, 1, true, 5, 1);
	Signal<&SciTEGTK::FindIncrementCompleteCmd> sigFindIncrementComplete;
	gtk_signal_connect(GTK_OBJECT(IncSearchEntry),"activate", GtkSignalFunc(sigFindIncrementComplete.Function), this);
	gtk_signal_connect(GTK_OBJECT(IncSearchEntry), "key-press-event", GtkSignalFunc(FindIncrementEscapeSignal), this);
	Signal<&SciTEGTK::FindIncrementCmd> sigFindIncrement;
	gtk_signal_connect(GTK_OBJECT(IncSearchEntry),"changed", GtkSignalFunc(sigFindIncrement.Function), this);
	gtk_signal_connect(GTK_OBJECT(IncSearchEntry),"focus-out-event", GtkSignalFunc(FindIncrementFocusOutSignal), NULL);
	gtk_widget_show(IncSearchEntry);

	CreateStrips(boxMain);

	wOutput.Call(SCI_SETMARGINWIDTHN, 1, 0);

	wStatusBar = gtk_statusbar_new();
	sbContextID = gtk_statusbar_get_context_id(
	                  GTK_STATUSBAR(PWidget(wStatusBar)), "global");
	gtk_box_pack_start(GTK_BOX(boxMain), PWidget(wStatusBar), FALSE, FALSE, 0);
	gtk_statusbar_push(GTK_STATUSBAR(PWidget(wStatusBar)), sbContextID, "Initial");
	sbVisible = false;

	static const GtkTargetEntry dragtypes[] = { { (gchar*)"text/uri-list", 0, 0 } };
	static const gint n_dragtypes = ELEMENTS(dragtypes);

	gtk_drag_dest_set(PWidget(wSciTE), GTK_DEST_DEFAULT_ALL, dragtypes,
	                  n_dragtypes, GDK_ACTION_COPY);
	(void)gtk_signal_connect(GTK_OBJECT(PWidget(wSciTE)), "drag_data_received",
	                         GTK_SIGNAL_FUNC(DragDataReceived), this);

	SetFocus(wOutput);

	if ((left != useDefault) && (top != useDefault))
		gtk_widget_set_uposition(GTK_WIDGET(PWidget(wSciTE)), left, top);
	if ((width != useDefault) && (height != useDefault))
		gtk_window_set_default_size(GTK_WINDOW(PWidget(wSciTE)), width, height);
	gtk_widget_show_all(PWidget(wSciTE));
	SetIcon();

	if (maximize)
		gtk_window_maximize(GTK_WINDOW(PWidget(wSciTE)));

	gtk_widget_hide(wIncrementPanel);
	gtk_widget_hide(PWidget(findStrip));
	gtk_widget_hide(PWidget(replaceStrip));

	UIAvailable();
}

void SciTEGTK::FindIncrementCmd() {
	const char *lineEntry = gtk_entry_get_text(GTK_ENTRY(IncSearchEntry));
	findWhat = lineEntry;
	wholeWord = false;
	if (findWhat != "") {
		FindNext(false, false);
		if (!havefound) {
			GdkColor red = { 0, 0xFFFF, 0x8888, 0x8888 };
			gtk_widget_modify_base(GTK_WIDGET(IncSearchEntry), GTK_STATE_NORMAL, &red);
		} else {
			GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF};
			gtk_widget_modify_base(GTK_WIDGET(IncSearchEntry), GTK_STATE_NORMAL, &white);
		}
	}
}

gboolean SciTEGTK::FindIncrementEscapeSignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key-press-event");
		gtk_widget_hide(scitew->wIncrementPanel);
		SetFocus(scitew->wEditor);
	}
	return FALSE;
}

void SciTEGTK::FindIncrementCompleteCmd() {
	gtk_widget_hide(wIncrementPanel);
	SetFocus(wEditor);
}

gboolean SciTEGTK::FindIncrementFocusOutSignal(GtkWidget *w) {
	gtk_widget_hide(w->parent);
	return FALSE;
}

void SciTEGTK::FindIncrement() {
	findStrip.Close();
	replaceStrip.Close();
	GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF};
	gtk_widget_modify_base(GTK_WIDGET(IncSearchEntry), GTK_STATE_NORMAL, &white);
	gtk_widget_show(wIncrementPanel);
	gtk_widget_grab_focus(GTK_WIDGET(IncSearchEntry));
	gtk_entry_set_text(GTK_ENTRY(IncSearchEntry), "");
}

void SciTEGTK::SetIcon() {
	GdkPixbuf *icon_pix_buf = CreatePixbuf("Sci48M.png");
	if (icon_pix_buf) {
		gtk_window_set_icon(GTK_WINDOW(PWidget(wSciTE)), icon_pix_buf);
		gdk_pixbuf_unref(icon_pix_buf);
		return;
	}
	GtkStyle *style = gtk_widget_get_style(PWidget(wSciTE));
	GdkBitmap *mask;
	GdkPixmap *icon_pix = gdk_pixmap_create_from_xpm_d(
		PWidget(wSciTE)->window, &mask,
		&style->bg[GTK_STATE_NORMAL], (gchar **)SciIcon_xpm);
	gdk_window_set_icon(PWidget(wSciTE)->window, NULL, icon_pix, mask);
}

void SciTEGTK::SetStartupTime(const char *timestamp) {
	if (timestamp != NULL) {
		char *end;
		// Reset errno from any previous errors
		errno = 0;
		gulong ts = strtoul(timestamp, &end, 0);
		if (end != timestamp && errno == 0) {
			startupTimestamp = ts;
		}
	}
}

// Send the filename through the pipe using the director command "open:"
// Make the path absolute if it is not already.
// If filename is empty, we send a message to the existing instance to tell
// it to present itself (ie. the window should come to the front)
void SciTEGTK::SendFileName(int sendPipe, const char* filename) {

	SString command;
	const char *pipeData;

	if (strlen(filename) != 0) {
		command = "open:";

		// Check to see if path is already absolute.
		if (!g_path_is_absolute(filename)) {
			gchar *currentPath = g_get_current_dir();
			command += currentPath;
			command += '/';
			g_free(currentPath);
		}
		command += filename;
		command += "\n";

	} else {
		command = "focus:";

		if (startupTimestamp != 0) {
			char timestamp[14];
			snprintf(timestamp, 14, "%d", startupTimestamp);
			command += timestamp;
		}
		command += "\n";
	}
	pipeData = command.c_str();

	// Send it.
	if (write(sendPipe, pipeData, strlen(pipeData)) == -1)
		perror("Unable to write to pipe");
}

bool SciTEGTK::CheckForRunningInstance(int argc, char *argv[]) {

	const gchar *tmpdir = g_get_tmp_dir();
	GDir *dir = g_dir_open(tmpdir, 0, NULL);
	if (dir == NULL) {
		return false; // Couldn't open the directory
	}

	GPatternSpec *pattern = g_pattern_spec_new("SciTE.*.in");

	char *pipeFileName = NULL;
	const char *filename;

	// Find a working pipe in our temporary directory
	while ((filename = g_dir_read_name(dir))) {
		if (g_pattern_match_string(pattern, filename)) {
			pipeFileName = g_build_filename(tmpdir, filename, NULL);

			// Attempt to open the pipe as a writer to send out data.
			int sendPipe = open(pipeFileName, O_WRONLY | O_NONBLOCK);

			// If open succeeded, write filename data.
			if (sendPipe != -1) {
				for (int ii = 1; ii < argc; ++ii) {
					if (argv[ii][0] != '-')
						SendFileName(sendPipe, argv[ii]);
				}

				// Force the SciTE instance to come to the front.
				SendFileName(sendPipe, "");

				// We're done
				if (close(sendPipe) == -1)
					perror("Unable to close pipe");
				break;
			} else {
				// We don't care about the error. Try another pipe.
				pipeFileName = NULL;
			}
		}
	}
	g_pattern_spec_free(pattern);
	g_dir_close(dir);

	if (pipeFileName != NULL) {
		// We need to call this since we're not displaying a window
		gdk_notify_startup_complete();
		g_free(pipeFileName);
		return true;
	}

	// If we arrived here, there is no SciTE instance we could talk to.
	// We'll start a new one
	return false;
}

void SciTEGTK::Run(int argc, char *argv[]) {
	// Load the default session file
	if (props.GetInt("save.session") || props.GetInt("save.position") || props.GetInt("save.recent")) {
		LoadSessionFile("");
	}

	// Find the SciTE executable, first trying to use argv[0] and converting
	// to an absolute path and if that fails, searching the path.
	sciteExecutable = FilePath(argv[0]).AbsolutePath();
	if (!sciteExecutable.Exists()) {
		gchar *progPath = g_find_program_in_path(argv[0]);
		sciteExecutable = FilePath(progPath);
		g_free(progPath);
	}

	// Collect the argv into one string with each argument separated by '\n'
	GUI::gui_string args;
	for (int arg = 1; arg < argc; arg++) {
		if (arg > 1)
			args += '\n';
		args += argv[arg];
	}

	// Process any initial switches
	ProcessCommandLine(args, 0);

	// Check if SciTE is already running.
	if ((props.Get("ipc.director.name").size() == 0) && props.GetInt ("check.if.already.open")) {
		if (CheckForRunningInstance (argc, argv)) {
			// Returning from this function exits the program.
			return;
		}
	}

	CreateUI();

	// Process remaining switches and files
	ProcessCommandLine(args, 1);

	CheckMenus();
	SizeSubWindows();
	SetFocus(wEditor);
	gtk_widget_grab_focus(GTK_WIDGET(PWidget(wSciTE)));
	gtk_main();
}

// Avoid zombie detached processes by reaping their exit statuses when
// they are shut down.
void SciTEGTK::ChildSignal(int) {
	int status = 0;
	int pid = wait(&status);
	if (instance && (pid == instance->pidShell)) {
		// If this child is the currently running tool, save the exit status
		instance->pidShell = 0;
		instance->triedKill = false;
		instance->exitStatus = status;
	}
}

// Detect if the tool has exited without producing any output
int SciTEGTK::PollTool(SciTEGTK *scitew) {
	scitew->ContinueExecute(TRUE);
	return TRUE;
}

int main(int argc, char *argv[]) {
#ifdef NO_EXTENSIONS
	Extension *extender = 0;
#else
	MultiplexExtension multiExtender;
	Extension *extender = &multiExtender;

#ifndef NO_LUA
	multiExtender.RegisterExtension(LuaExtension::Instance());
#endif
#ifndef NO_FILER
	multiExtender.RegisterExtension(DirectorExtension::Instance());
#endif
#endif

	signal(SIGCHLD, SciTEGTK::ChildSignal);

	// Get this now because gtk_init() clears it
	const gchar *startup_id = g_getenv("DESKTOP_STARTUP_ID");
	char *timestamp = NULL;
	if (startup_id != NULL) {
		char *pos = g_strrstr(startup_id, "_TIME");
		if (pos != NULL) {
			timestamp = pos + 5; // Skip "_TIME"
		}
	}

	gtk_set_locale();
	gtk_init(&argc, &argv);

	SciTEGTK scite(extender);
	scite.SetStartupTime(timestamp);
	scite.Run(argc, argv);

	return 0;
}
