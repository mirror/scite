// SciTE - Scintilla based Text Editor
// SciTEGTK.cxx - main code for the GTK+ version of the editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "Platform.h"

#include <unistd.h>
#include <glib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "WinDefs.h"

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciTEBase.h"

#define MB_ABOUTBOX	0x100000L

#ifdef STATIC_BUILD
const char appName[] = "Sc1";
#else
const char appName[] = "SciTE";
#endif

class SciTEGTK : public SciTEBase {

protected:

	Point ptOld;
	GdkGC *xor_gc;

	bool sbVisible;
	Window wStatusBar;
	guint sbContextID;
	SString sbValue;

	// Control of sub process
	int icmd;
	int originalEnd;
	int fdFIFO;
	int pidShell;
	char resultsFile[MAX_PATH];
	int inputHandle;

	Window content;
	bool savingHTML;
	Window fileSelector;
	Window findInFilesDialog;
	GtkWidget *comboFiles;
	Window gotoDialog;
	GtkWidget *gotoEntry;
	GtkWidget *toggleWord;
	GtkWidget *toggleCase;
	GtkWidget *toggleReverse;
	GtkWidget *comboFind;
	GtkWidget *comboReplace;
	GtkItemFactory *itemFactory;
	GtkAccelGroup *accelGroup;

	virtual void ReadPropertiesInitial();
	virtual void ReadProperties();

	virtual void SetMenuItem(int menuNumber, int position, int itemID, 
		const char *text, const char *mnemonic=0);
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();

	virtual void AbsolutePath(char *absPath, const char *relativePath, int size);
	virtual bool OpenDialog();
	virtual bool SaveAsDialog();
	virtual void SaveAsHTML();

	virtual void Print();
	virtual void PrintSetup();

	virtual void AboutDialog();
	virtual void QuitProgram();

	void FindReplaceGrabFields();
	void HandleFindReplace();
	virtual void Find();
	virtual void FindInFiles();
	virtual void Replace();
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();
	virtual void GoLineDialog();

	virtual PRectangle GetClientRectangle();
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps, unsigned int lenPath);
	virtual bool GetUserPropertiesFileName(char *pathDefaultProps, unsigned int lenPath);

	virtual void Notify(SCNotification *notification);
	void Command(WPARAM wParam, LPARAM lParam = 0);
	void ContinueExecute();

	// GTK+ Signal Handlers

	static void OpenCancelSignal(GtkWidget *w, SciTEGTK *scitew);
	static void OpenKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	static void OpenOKSignal(GtkWidget *w, SciTEGTK *scitew);
	static void SaveAsSignal(GtkWidget *w, SciTEGTK *scitew);

	static void FindInFilesSignal(GtkWidget *w, SciTEGTK *scitew);
	static void FindInFilesCancelSignal(GtkWidget *w, SciTEGTK *scitew);
	static void FindInFilesKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);

	static void GotoCancelSignal(GtkWidget *w, SciTEGTK *scitew);
	static void GotoKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	static void GotoSignal(GtkWidget *w, SciTEGTK *scitew);

	static void FRCancelSignal(GtkWidget *w, SciTEGTK *scitew);
	static void FRKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	static void FRFindSignal(GtkWidget *w, SciTEGTK *scitew);
	static void FRReplaceSignal(GtkWidget *w, SciTEGTK *scitew);
	static void FRReplaceAllSignal(GtkWidget *w, SciTEGTK *scitew);

	static void IOSignal(SciTEGTK *scitew);

	static gint MoveResize(GtkWidget *widget, GtkAllocation *allocation, SciTEGTK *scitew);
	static void QuitSignal(GtkWidget *w, SciTEGTK *scitew);
	static void MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *w);
	static void CommandSignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);
	static void NotifySignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);

	void DividerXOR(Point pt);
	static gint DividerExpose(GtkWidget *widget, GdkEventExpose *ose, SciTEGTK *scitew);
	static gint DividerMotion(GtkWidget *widget, GdkEventMotion *event, SciTEGTK *scitew);
	static gint DividerPress(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);
	static gint DividerRelease(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);

public:

	SciTEGTK();
	~SciTEGTK();

	void Run(const char *cmdLine);
	void ProcessExecute();
	virtual void Execute();
	virtual void StopExecute();
};

SciTEGTK::SciTEGTK() {
	// Control of sub process
	icmd = 0;
	originalEnd = 0;
	fdFIFO = 0;
	pidShell = 0;
	sprintf(resultsFile, "/tmp/SciTE%x.results",
	        static_cast<int>(getpid()));
	inputHandle = 0;

	ReadGlobalPropFile();

	ptOld = Point(0, 0);
	xor_gc = 0;
	savingHTML = false;
	comboFiles = 0;
	gotoEntry = 0;
	toggleWord = 0;
	toggleCase = 0;
	toggleReverse = 0;
	comboFind = 0;
	comboReplace = 0;
	itemFactory = 0;
}

SciTEGTK::~SciTEGTK() {
}

static void destroyDialog(GtkWidget *) {
}

static GtkWidget *messageBoxDialog = 0;
static int messageBoxResult = 0;

static gint messageBoxKey(GtkWidget *w, GdkEventKey *event, gpointer p) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		gtk_widget_destroy(GTK_WIDGET(w));
		messageBoxDialog = 0;
		messageBoxResult = reinterpret_cast<int>(p);
	}
	return 0;
}

static void messageBoxOK(GtkWidget *, gpointer p) {
	gtk_widget_destroy(GTK_WIDGET(messageBoxDialog));
	messageBoxDialog = 0;
	messageBoxResult = reinterpret_cast<int>(p);
}

static GtkWidget *AddMBButton(GtkWidget *dialog, const char *label,
	                              int val, bool isDefault = false) {
	GtkWidget * button = gtk_button_new_with_label(label);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
	                   GtkSignalFunc(messageBoxOK), reinterpret_cast<gpointer>(val));
	if (isDefault) {
		GTK_WIDGET_SET_FLAGS(GTK_WIDGET(button), GTK_CAN_DEFAULT);
	}
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
	                   button, TRUE, TRUE, 0);
	if (isDefault) {
		gtk_widget_grab_default(GTK_WIDGET(button));
	}
	gtk_widget_show(button);
	return button;
}

int MessageBox(GtkWidget *wParent, const char *m, const char *t, int style) {
	if (!messageBoxDialog) {
		messageBoxResult = -1;
		messageBoxDialog = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(messageBoxDialog), t);
		gtk_container_border_width(GTK_CONTAINER(messageBoxDialog), 0);

		gtk_signal_connect(GTK_OBJECT(messageBoxDialog),
		                   "destroy", GtkSignalFunc(destroyDialog), 0);

		int escapeResult = IDOK;
		if ((style & 0xf) == MB_OK) {
			AddMBButton(messageBoxDialog, "OK", IDOK, true);
		} else {
			AddMBButton(messageBoxDialog, "Yes", IDYES, true);
			AddMBButton(messageBoxDialog, "No", IDNO);
			escapeResult = IDNO;
			if (style == MB_YESNOCANCEL) {
				AddMBButton(messageBoxDialog, "Cancel", IDCANCEL);
				escapeResult = IDCANCEL;
			}
		}
		gtk_signal_connect(GTK_OBJECT(messageBoxDialog),
		                   "key_press_event", GtkSignalFunc(messageBoxKey),
		                   reinterpret_cast<gpointer>(escapeResult));

		if (style & MB_ABOUTBOX) {
			GtkWidget *explanation = scintilla_new();
			scintilla_set_id(SCINTILLA(explanation), 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(messageBoxDialog)->vbox),
			                   explanation, TRUE, TRUE, 0);
			gtk_widget_set_usize(GTK_WIDGET(explanation), 480, 380);
			gtk_widget_show_all(explanation);
#ifdef STATIC_BUILD
			SetAboutMessage(explanation, "Sc1  ");
#else
			SetAboutMessage(explanation, "SciTE");
#endif
		} else {
			GtkWidget *label = gtk_label_new(m);
			gtk_misc_set_padding(GTK_MISC(label), 10, 10);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(messageBoxDialog)->vbox),
			                   label, TRUE, TRUE, 0);
			gtk_widget_show(label);
		}

		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(messageBoxDialog), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(messageBoxDialog),
		                              GTK_WINDOW(wParent));

		gtk_widget_show(messageBoxDialog);

		while (messageBoxResult < 0) {
			gtk_main_iteration();
		}
	}
	return messageBoxResult;
}

bool SciTEGTK::GetDefaultPropertiesFileName(char *pathDefaultProps, unsigned int lenPath) {
	strncpy(pathDefaultProps, getenv("HOME"), lenPath);
	strncat(pathDefaultProps, pathSepString, lenPath);
	strncat(pathDefaultProps, propGlobalFileName, lenPath);
	return true;
}

bool SciTEGTK::GetUserPropertiesFileName(char *pathDefaultProps, unsigned int lenPath) {
	strncpy(pathDefaultProps, getenv("HOME"), lenPath);
	strncat(pathDefaultProps, pathSepString, lenPath);
	strncat(pathDefaultProps, propUserFileName, lenPath);
	return true;
}

void SciTEGTK::Notify(SCNotification *notification) {
	switch (notification->nmhdr.code) {
	case SCN_KEY: {
			int mods = 0;
			if (notification->modifiers & SHIFT_PRESSED)
				mods |= GDK_SHIFT_MASK;
			if (notification->modifiers & LEFT_CTRL_PRESSED)
				mods |= GDK_CONTROL_MASK;
			if (notification->modifiers & LEFT_ALT_PRESSED)
				mods |= GDK_MOD1_MASK;
			//Platform::DebugPrintf("SCN_KEY: %d %d\n", notification->ch, mods);
			if ((mods == GDK_CONTROL_MASK) && (notification->ch == VK_TAB)) {
				Command(IDM_NEXTFILE);
			} else if ((mods == GDK_CONTROL_MASK | GDK_SHIFT_MASK ) && (notification->ch == VK_TAB)) {
				Command(IDM_PREVFILE);
			} else {
				gtk_accel_group_activate(accelGroup, notification->ch,
				                         static_cast<GdkModifierType>(mods));
			}
		}
		break;

	case SCN_UPDATEUI:
		BraceMatch(notification->nmhdr.idFrom == IDM_SRCWIN);
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (sbVisible) {
				SString msg;
				int caretPos = SendEditor(SCI_GETCURRENTPOS);
				int caretLine = SendEditor(EM_LINEFROMCHAR, caretPos);
				int caretLineStart = SendEditor(EM_LINEINDEX, caretLine);
				msg = "Column=";
				msg += SString(caretPos - caretLineStart + 1).c_str();
				msg += "    Line=";
				msg += SString(caretLine + 1).c_str();
				if (!(sbValue == msg)) {
					gtk_statusbar_pop(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID);
					gtk_statusbar_push(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID, msg.c_str());
					sbValue = msg;
				}
			} else {
				sbValue = "";
			}
		}
		break;

	default:
		SciTEBase::Notify(notification);
		break;

	}
}

void SciTEGTK::Command(WPARAM wParam, LPARAM) {
	int cmdID = ControlIDOfCommand(wParam);
	switch (cmdID) {

	case IDM_VIEWSTATUSBAR:
		sbVisible = ! sbVisible;
		if (sbVisible) {
			gtk_widget_show(GTK_WIDGET(wStatusBar.GetID()));
		} else {
			gtk_widget_hide(GTK_WIDGET(wStatusBar.GetID()));
		}
		CheckMenus();
		break;
		
	case IDM_SRCWIN:
	case IDM_RUNWIN:
		if ((wParam >> 16) == EN_SETFOCUS) 
			CheckMenus();
		break;

	default:
		SciTEBase::MenuCommand(cmdID);
	}
}

void SciTEGTK::ReadPropertiesInitial() {
	SciTEBase::ReadPropertiesInitial();
	sbVisible = props.GetInt("statusbar.visible");
	if (sbVisible)
		gtk_widget_show(GTK_WIDGET(wStatusBar.GetID()));
	else
		gtk_widget_hide(GTK_WIDGET(wStatusBar.GetID()));
}

void SciTEGTK::ReadProperties() {
	SciTEBase::ReadProperties();

	CheckMenus();
}

void SciTEGTK::SetMenuItem(int, int, int itemID, const char *text, const char *) {
	// On GTK+ the menuNumber and position are ignored as the menu item already exists and is in the right
	// place so only needs to be shown and have its text set.
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, itemID);
	if (item) {
		GList *al = gtk_container_children(GTK_CONTAINER(item));
		for (unsigned int ii = 0; ii < g_list_length(al); ii++) {
			gpointer d = g_list_nth(al, ii);
			GtkWidget **w = (GtkWidget **)d;
			gtk_label_set_text(GTK_LABEL(*w), text);
		}
		g_list_free(al);
		gtk_widget_show(item);
	}
}

void SciTEGTK::DestroyMenuItem(int, int itemID) {
	// On GTK+ menu items are just hidden rather than destroyed as they can not be recreated in the middle of a menu
	// The menuNumber is ignored as all menu items in GTK+ can be found from the root of the menu tree
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, itemID);
	if (item) {
		//Platform::DebugPrintf("Destroying[%0d]\n", itemID);
		gtk_widget_hide(item);
	}
}

void SciTEGTK::CheckAMenuItem(int wIDCheckItem, bool val) {
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, wIDCheckItem);
	allowMenuActions = false;
	if (item)
		gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(item), val ? TRUE : FALSE);
	else
		Platform::DebugPrintf("Could not find %x\n", wIDCheckItem);
	allowMenuActions = true;
}

void SciTEGTK::EnableAMenuItem(int wIDCheckItem, bool val) {
	GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, wIDCheckItem);
	if (item) {
		//Platform::DebugPrintf("Set %d to %d\n", wIDCheckItem, val);
		if (GTK_IS_WIDGET(item))
			gtk_widget_set_sensitive(item, val);
	}
}

void SciTEGTK::CheckMenus() {
	SciTEBase::CheckMenus();

	CheckAMenuItem(IDM_EOL_CRLF, SendEditor(SCI_GETEOLMODE) == SC_EOL_CRLF);
	CheckAMenuItem(IDM_EOL_CR, SendEditor(SCI_GETEOLMODE) == SC_EOL_CR);
	CheckAMenuItem(IDM_EOL_LF, SendEditor(SCI_GETEOLMODE) == SC_EOL_LF);
	
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
}

// Windows specific so no action on *x
void SciTEGTK::AbsolutePath(char *absPath, const char *relativePath, int size) {
	strncpy(absPath, relativePath, size);
	absPath[size - 1] = '\0';
}

bool SciTEGTK::OpenDialog() {
	if (!fileSelector.Created()) {
		fileSelector = gtk_file_selection_new("Open File");
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->ok_button),
		                   "clicked", GtkSignalFunc(OpenOKSignal), this);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->cancel_button),
		                   "clicked", GtkSignalFunc(OpenCancelSignal), this);
		gtk_signal_connect(GTK_OBJECT(fileSelector.GetID()),
		                   "key_press_event", GtkSignalFunc(OpenKeySignal),
		                   this);
		// Other ways to destroy
		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(fileSelector.GetID()), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(fileSelector.GetID()),
		                              GTK_WINDOW(wSciTE.GetID()));
		fileSelector.Show();
	}
	return true;
}

bool SciTEGTK::SaveAsDialog() {
	savingHTML = false;
	if (!fileSelector.Created()) {
		fileSelector = gtk_file_selection_new("Save File As");
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->ok_button),
		                   "clicked", GtkSignalFunc(SaveAsSignal), this);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->cancel_button),
		                   "clicked", GtkSignalFunc(OpenCancelSignal), this);
		gtk_signal_connect(GTK_OBJECT(fileSelector.GetID()),
		                   "key_press_event", GtkSignalFunc(OpenKeySignal),
		                   this);
		// Other ways to destroy
		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(fileSelector.GetID()), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(fileSelector.GetID()),
		                              GTK_WINDOW(wSciTE.GetID()));
		fileSelector.Show();
	}
	return true;
}

void SciTEGTK::SaveAsHTML() {
	if (!fileSelector.Created()) {
		savingHTML = true;
		fileSelector = gtk_file_selection_new("Save File As HTML");
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->ok_button),
		                   "clicked", GtkSignalFunc(SaveAsSignal), this);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSelector.GetID())->cancel_button),
		                   "clicked", GtkSignalFunc(OpenCancelSignal), this);
		gtk_signal_connect(GTK_OBJECT(fileSelector.GetID()),
		                   "key_press_event", GtkSignalFunc(OpenKeySignal),
		                   this);
		// Other ways to destroy
		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(fileSelector.GetID()), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(fileSelector.GetID()),
		                              GTK_WINDOW(wSciTE.GetID()));
		fileSelector.Show();
	}
}

void SciTEGTK::Print() {
	// Printing not yet supported on GTK+
}

void SciTEGTK::PrintSetup() {
	// Printing not yet supported on GTK+
}

void SciTEGTK::HandleFindReplace() {
}

void SciTEGTK::Find() {
	SelectionIntoFind();
	FindReplace(false);
}

static void FillComboFromMemory(GtkWidget *combo, const ComboMemory &mem) {
	GtkWidget *list = GTK_COMBO(combo)->list;
	for (int i = 0; i < mem.Length(); i++) {
		GtkWidget *item = gtk_list_item_new_with_label(mem.At(i).c_str());
		gtk_container_add(GTK_CONTAINER(list), item);
		gtk_widget_show(item);
	}
}

void SciTEGTK::FindReplaceGrabFields() {
	char *findEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(comboFind)->entry));
	strncpy(findWhat, findEntry, sizeof(findWhat));
	memFinds.Insert(findWhat);
	if (comboReplace) {
		char *replaceEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(comboReplace)->entry));
		strncpy(replaceWhat, replaceEntry, sizeof(replaceWhat));
		memReplaces.Insert(replaceWhat);
	}
	wholeWord = GTK_TOGGLE_BUTTON(toggleWord)->active;
	matchCase = GTK_TOGGLE_BUTTON(toggleCase)->active;
	reverseFind = GTK_TOGGLE_BUTTON(toggleReverse)->active;
}

void SciTEGTK::FRCancelSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->wFindReplace.Destroy();
}

void SciTEGTK::FRKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->wFindReplace.Destroy();
	}
}

void SciTEGTK::FRFindSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->FindReplaceGrabFields();
	if (scitew->findWhat[0]) {
		scitew->FindNext();
	}
}

void SciTEGTK::FRReplaceSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->FindReplaceGrabFields();
	scitew->ReplaceOnce();
}

void SciTEGTK::FRReplaceAllSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->FindReplaceGrabFields();
	scitew->ReplaceAll();
	scitew->wFindReplace.Destroy();
}

void SciTEGTK::FindInFilesSignal(GtkWidget *, SciTEGTK *scitew) {
	char *findEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scitew->comboFind)->entry));
	scitew->props.Set("find.what", findEntry);
	scitew->memFinds.Insert(findEntry);
	char *filesEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scitew->comboFiles)->entry));
	scitew->props.Set("find.files", filesEntry);
	scitew->memFiles.Insert(filesEntry);

	scitew->findInFilesDialog.Destroy();

	//printf("Grepping for <%s> in <%s>\n",
	//	scitew->props.Get("find.what"),
	//	scitew->props.Get("find.files"));
	scitew->SelectionIntoProperties();
	scitew->AddCommand(scitew->props.GetNewExpand("find.command", ""), "", jobCLI);
	if (scitew->commandCurrent > 0)
		scitew->Execute();
}

void SciTEGTK::FindInFilesCancelSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->findInFilesDialog.Destroy();
}

void SciTEGTK::FindInFilesKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->findInFilesDialog.Destroy();
	}
}

void SciTEGTK::FindInFiles() {
	SelectionIntoFind();
	props.Set("find.what", findWhat);
	props.Set("find.directory", ".");

	findInFilesDialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(findInFilesDialog.GetID()), TRUE, TRUE, TRUE);
	findInFilesDialog.SetTitle("Find In Files");

	gtk_signal_connect(GTK_OBJECT(findInFilesDialog.GetID()),
	                   "destroy", GtkSignalFunc(destroyDialog), 0);

	GtkWidget *table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(findInFilesDialog.GetID())->vbox),
	                   table, TRUE, TRUE, 0);

	GtkAttachOptions opts = static_cast<GtkAttachOptions>(
	                            GTK_SHRINK | GTK_FILL);

	GtkAttachOptions optse = static_cast<GtkAttachOptions>(
	                             GTK_SHRINK | GTK_FILL | GTK_EXPAND);

	int row = 0;

	GtkWidget *labelFind = gtk_label_new("Find what:");
	gtk_table_attach(GTK_TABLE(table), labelFind, 0, 1,
	                 row, row + 1, opts, opts, 5, 5);
	gtk_widget_show(labelFind);

	comboFind = gtk_combo_new();
	FillComboFromMemory(comboFind, memFinds);

	gtk_table_attach(GTK_TABLE(table), comboFind, 1, 2,
	                 row, row + 1, optse, opts, 5, 5);
	gtk_widget_show(comboFind);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(comboFind)->entry), findWhat);
	gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(comboFind)->entry), 0, strlen(findWhat));
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboFind)->entry),
	                   "activate", GtkSignalFunc(FindInFilesSignal), this);

	row++;

	GtkWidget *labelFiles = gtk_label_new("Files:");
	gtk_table_attach(GTK_TABLE(table), labelFiles, 0, 1,
	                 row, row + 1, opts, opts, 5, 5);
	gtk_widget_show(labelFiles);

	comboFiles = gtk_combo_new();
	FillComboFromMemory(comboFiles, memFiles);

	gtk_table_attach(GTK_TABLE(table), comboFiles, 1, 2,
	                 row, row + 1, optse, opts, 5, 5);
	gtk_widget_show(comboFiles);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(comboFiles)->entry), props.Get("find.files").c_str());
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboFiles)->entry),
	                   "activate", GtkSignalFunc(FindInFilesSignal), this);

	gtk_widget_show(table);

	GtkWidget *buttonFind = gtk_button_new_with_label("Find");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(findInFilesDialog.GetID())->action_area),
	                   buttonFind, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(buttonFind),
	                   "clicked", GtkSignalFunc(FindInFilesSignal), this);
	gtk_widget_show(buttonFind);

	GtkWidget *buttonCancel = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(findInFilesDialog.GetID())->action_area),
	                   buttonCancel, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(buttonCancel),
	                   "clicked", GtkSignalFunc(FindInFilesCancelSignal), this);
	gtk_signal_connect(GTK_OBJECT(findInFilesDialog.GetID()),
	                   "key_press_event", GtkSignalFunc(FindInFilesKeySignal),
	                   this);

	gtk_widget_show(buttonCancel);

	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(buttonFind), GTK_CAN_DEFAULT);
	gtk_widget_grab_default(GTK_WIDGET(buttonFind));
	gtk_widget_grab_focus(GTK_WIDGET(GTK_COMBO(comboFind)->entry));

	// Mark it as a modal transient dialog
	gtk_window_set_modal(GTK_WINDOW(findInFilesDialog.GetID()), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW(findInFilesDialog.GetID()),
	                              GTK_WINDOW(wSciTE.GetID()));

	findInFilesDialog.Show();
}

void SciTEGTK::Replace() {
	SelectionIntoFind();
	FindReplace(true);
}

void SciTEGTK::ContinueExecute() {
	char buf[256];
	int count = 0;
	count = read(fdFIFO, buf, sizeof(buf) - 1 );
	if (count < 0) {
		OutputAppendString(">End Bad\n");
		return;
	}
	if (count == 0) {
		int status;
		//int pidWaited = wait(&status);
		wait(&status);
		char exitmessage[80];
		sprintf(exitmessage, ">Exit code: %d\n", status);
		OutputAppendString(exitmessage);
		// Move selection back to beginning of this run so that F4 will go
		// to first error of this run.
		SendOutput(SCI_GOTOPOS, originalEnd);
		gdk_input_remove(inputHandle);
		close(fdFIFO);
		unlink(resultsFile);
		icmd++;
		if (icmd < commandCurrent && icmd < commandMax) {
			Execute();
		} else {
			icmd = 0;
			executing = false;
			CheckMenus();
			for (int ic = 0; ic < commandMax; ic++) {
				jobQueue[ic].Clear();
			}
			commandCurrent = 0;
		}
		return;
	}
	if (count > 0) {
		buf[count] = '\0';
		OutputAppendString(buf);
		if (count < 0) {
			OutputAppendString(">Continue no data\n");
			return;
		}
	}
}

void SciTEGTK::IOSignal(SciTEGTK *scitew) {
	scitew->ContinueExecute();
}

int xsystem(const char *s, const char *resultsFile) {
	int pid = 0;
	//printf("xsystem %s %s\n", s, resultsFile);
	if ((pid = fork()) == 0) {
		close(0);
		int fh = open(resultsFile, O_WRONLY);
		close(1);
		dup(fh);
		close(2);
		dup(fh);
		execlp("/bin/sh", "sh", "-c", s, 0);
		exit(127);
	}
	return pid;
}

void SciTEGTK::Execute() {
	SciTEBase::Execute();

	SendOutput(SCI_GOTOPOS, SendOutput(WM_GETTEXTLENGTH));
	originalEnd = SendOutput(SCI_GETCURRENTPOS);

	OutputAppendString(">");
	OutputAppendString(jobQueue[icmd].command.c_str());
	OutputAppendString("\n");

	unlink(resultsFile);

	int fdp = mkfifo(resultsFile, S_IRUSR | S_IWUSR);
	if (fdp < 0) {
		OutputAppendString(">Failed to create FIFO\n");
		return;
	}
	close(fdp);

	SString commandPlus = jobQueue[icmd].command;
	pidShell = xsystem(commandPlus.c_str(), resultsFile);
	fdFIFO = open(resultsFile, O_RDONLY);
	if (fdFIFO < 0) {
		OutputAppendString(">Failed to open\n");
		return;
	}

	inputHandle = gdk_input_add(fdFIFO, GDK_INPUT_READ,
	                            (GdkInputFunction) IOSignal, this);
}

void SciTEGTK::StopExecute() {
	kill(pidShell, SIGKILL);
}

void SciTEGTK::GotoCancelSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->gotoDialog.Destroy();
}

void SciTEGTK::GotoKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		gtk_widget_destroy(GTK_WIDGET(w));
		scitew->gotoDialog = 0;
	}
}

void SciTEGTK::GotoSignal(GtkWidget *, SciTEGTK *scitew) {
	char *lineEntry = gtk_entry_get_text(GTK_ENTRY(scitew->gotoEntry));
	int lineNo = atoi(lineEntry);

	scitew->SendEditor(SCI_GOTOLINE, lineNo - 1);

	scitew->gotoDialog.Destroy();
}

void SciTEGTK::GoLineDialog() {
	gotoDialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(gotoDialog.GetID()), "Go To");
	gtk_container_border_width(GTK_CONTAINER(gotoDialog.GetID()), 0);

	gtk_signal_connect(GTK_OBJECT(gotoDialog.GetID()),
	                   "destroy", GtkSignalFunc(destroyDialog), 0);

	GtkWidget *table = gtk_table_new(2, 1, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gotoDialog.GetID())->vbox),
	                   table, TRUE, TRUE, 0);

	GtkAttachOptions opts = static_cast<GtkAttachOptions>(
	                            GTK_EXPAND | GTK_SHRINK | GTK_FILL);
	GtkWidget *label = gtk_label_new("Go to line:");
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, opts, opts, 5, 5);
	gtk_widget_show(label);

	gotoEntry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), gotoEntry, 1, 2, 0, 1, opts, opts, 5, 5);
	gtk_signal_connect(GTK_OBJECT(gotoEntry),
	                   "activate", GtkSignalFunc(GotoSignal), this);
	gtk_widget_grab_focus(GTK_WIDGET(gotoEntry));
	gtk_widget_show(gotoEntry);

	gtk_widget_show(table);

	GtkWidget *buttonGoTo = gtk_button_new_with_label("Go To");
	gtk_signal_connect(GTK_OBJECT(buttonGoTo),
	                   "clicked", GtkSignalFunc(GotoSignal), this);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(buttonGoTo), GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gotoDialog.GetID())->action_area),
	                   buttonGoTo, TRUE, TRUE, 0);
	gtk_widget_grab_default(GTK_WIDGET(buttonGoTo));
	gtk_widget_show(buttonGoTo);

	GtkWidget *buttonCancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect(GTK_OBJECT(buttonCancel),
	                   "clicked", GtkSignalFunc(GotoCancelSignal), this);
	gtk_signal_connect(GTK_OBJECT(gotoDialog.GetID()),
	                   "key_press_event", GtkSignalFunc(GotoKeySignal),
	                   this);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gotoDialog.GetID())->action_area),
	                   buttonCancel, TRUE, TRUE, 0);
	gtk_widget_show(buttonCancel);

	// Mark it as a modal transient dialog
	gtk_window_set_modal(GTK_WINDOW(gotoDialog.GetID()), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW(gotoDialog.GetID()),
	                              GTK_WINDOW(wSciTE.GetID()));

	gotoDialog.Show();
}

void SciTEGTK::FindReplace(bool replace) {
	replacing = replace;
	wFindReplace = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(wFindReplace.GetID()), TRUE, TRUE, TRUE);
	wFindReplace.SetTitle(replace ? "Replace" : "Find");

	gtk_signal_connect(GTK_OBJECT(wFindReplace.GetID()),
	                   "destroy", GtkSignalFunc(destroyDialog), 0);

	GtkWidget *table = gtk_table_new(2, replace ? 4 : 3, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->vbox),
	                   table, TRUE, TRUE, 0);

	GtkAttachOptions opts = static_cast<GtkAttachOptions>(
	                            GTK_SHRINK | GTK_FILL);

	GtkAttachOptions optse = static_cast<GtkAttachOptions>(
	                             GTK_SHRINK | GTK_FILL | GTK_EXPAND);

	int row = 0;

	GtkWidget *labelFind = gtk_label_new("Find:");
	gtk_table_attach(GTK_TABLE(table), labelFind, 0, 1,
	                 row, row + 1, opts, opts, 5, 5);
	gtk_widget_show(labelFind);

	comboFind = gtk_combo_new();
	FillComboFromMemory(comboFind, memFinds);

	gtk_table_attach(GTK_TABLE(table), comboFind, 1, 2,
	                 row, row + 1, optse, opts, 5, 5);
	gtk_widget_show(comboFind);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(comboFind)->entry), findWhat);
	gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(comboFind)->entry), 0, strlen(findWhat));
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboFind)->entry),
	                   "activate", GtkSignalFunc(FRFindSignal), this);

	row++;

	if (replace) {
		GtkWidget *labelReplace = gtk_label_new("Replace:");
		gtk_table_attach(GTK_TABLE(table), labelReplace, 0, 1,
		                 row, row + 1, opts, opts, 5, 5);
		gtk_widget_show(labelReplace);

		comboReplace = gtk_combo_new();
		FillComboFromMemory(comboReplace, memReplaces);

		gtk_table_attach(GTK_TABLE(table), comboReplace, 1, 2,
		                 row, row + 1, optse, opts, 5, 5);
		gtk_widget_show(comboReplace);
		gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboReplace)->entry),
		                   "activate", GtkSignalFunc(FRFindSignal), this);

		row++;
	} else {
		comboReplace = 0;
	}

	// Case Sensitive
	toggleCase = gtk_check_button_new_with_label("Case Sensitive");
	gtk_table_attach(GTK_TABLE(table), toggleCase, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleCase);
	row++;

	// Whole Word
	toggleWord = gtk_check_button_new_with_label("Whole Word");
	gtk_table_attach(GTK_TABLE(table), toggleWord, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleWord);
	row++;

	// Whole Word
	toggleReverse = gtk_check_button_new_with_label("Reverse Direction");
	gtk_table_attach(GTK_TABLE(table), toggleReverse, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleReverse);

	gtk_widget_show(table);

	GtkWidget *buttonFind = gtk_button_new_with_label("Find");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
	                   buttonFind, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(buttonFind),
	                   "clicked", GtkSignalFunc(FRFindSignal), this);
	gtk_widget_show(buttonFind);

	if (replace) {
		GtkWidget *buttonReplace = gtk_button_new_with_label("Replace");
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
		                   buttonReplace, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(buttonReplace),
		                   "clicked", GtkSignalFunc(FRReplaceSignal), this);
		gtk_widget_show(buttonReplace);

		GtkWidget *buttonReplaceAll = gtk_button_new_with_label("Replace All");
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
		                   buttonReplaceAll, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(buttonReplaceAll),
		                   "clicked", GtkSignalFunc(FRReplaceAllSignal), this);
		gtk_widget_show(buttonReplaceAll);
	}

	GtkWidget *buttonCancel = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
	                   buttonCancel, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(buttonCancel),
	                   "clicked", GtkSignalFunc(FRCancelSignal), this);
	gtk_signal_connect(GTK_OBJECT(wFindReplace.GetID()),
	                   "key_press_event", GtkSignalFunc(FRKeySignal),
	                   this);
	gtk_widget_show(buttonCancel);

	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(buttonFind), GTK_CAN_DEFAULT);
	gtk_widget_grab_default(GTK_WIDGET(buttonFind));
	gtk_widget_grab_focus(GTK_WIDGET(GTK_COMBO(comboFind)->entry));

	// Mark it as a transient dialog
	gtk_window_set_transient_for (GTK_WINDOW(wFindReplace.GetID()),
	                              GTK_WINDOW(wSciTE.GetID()));

	wFindReplace.Show();
}

void SciTEGTK::DestroyFindReplace() {
	wFindReplace.Destroy();
}

void SciTEGTK::AboutDialog() {
	MessageBox(wSciTE.GetID(), "SciTE\nby Neil Hodgson neilh@scintilla.org .",
	           appName, MB_OK | MB_ABOUTBOX);
}

void SciTEGTK::QuitProgram() {
	if (SaveIfUnsure() != IDCANCEL) {
		gtk_exit(0);
	}
}

PRectangle SciTEGTK::GetClientRectangle() {
	return content.GetClientPosition();
}

gint SciTEGTK::MoveResize(GtkWidget *, GtkAllocation *allocation, SciTEGTK *scitew) {
	//Platform::DebugPrintf("SciTEGTK move resize %d %d\n", allocation->width, allocation->height);
	scitew->SizeSubWindows();
	return TRUE;
}

void SciTEGTK::QuitSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->Command(IDM_QUIT);
}

void SciTEGTK::MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *) {
	//Platform::DebugPrintf("action %d %x \n", action, w);
	if (scitew->allowMenuActions)
		scitew->Command(action);
}

void SciTEGTK::CommandSignal(GtkWidget *, gint wParam, gpointer lParam, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Command: %x %x %x\n", w, wParam, lParam);
	scitew->Command(wParam, reinterpret_cast<LPARAM>(lParam));
}

void SciTEGTK::NotifySignal(GtkWidget *, gint wParam, gpointer lParam, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Notify: %x %x %x\n", w, wParam, lParam);
	scitew->Notify(reinterpret_cast<SCNotification *>(lParam));
}

void SciTEGTK::DividerXOR(Point pt) {
	if (!xor_gc) {
		GdkGCValues values;
		values.foreground = wSciTE.GetID()->style->white;
		values.function = GDK_XOR;
		values.subwindow_mode = GDK_INCLUDE_INFERIORS;
		xor_gc = gdk_gc_new_with_values(wSciTE.GetID()->window,
		                                &values,
		                                static_cast<enum GdkGCValuesMask>(
		                                    GDK_GC_FOREGROUND | GDK_GC_FUNCTION | GDK_GC_SUBWINDOW));
	}
	if (splitVertical) {
		gdk_draw_line(wSciTE.GetID()->window, xor_gc,
		              pt.x,
		              0,
		              pt.x,
		              wSciTE.GetID()->allocation.height - 1);
	} else {
		gdk_draw_line(wSciTE.GetID()->window, xor_gc,
		              0,
		              pt.y,
		              wSciTE.GetID()->allocation.width - 1,
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
		// Vertical divider
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
			gdk_window_get_pointer(scitew->wSciTE.GetID()->window, &x, &y, &state);
			if (state & GDK_BUTTON1_MASK) {
				scitew->DividerXOR(scitew->ptOld);
				scitew->DividerXOR(Point(x, y));
			}
		}
	}
	return TRUE;
}

gint SciTEGTK::DividerPress(GtkWidget *, GdkEventButton *, SciTEGTK *scitew) {
	int x = 0;
	int y = 0;
	GdkModifierType state;
	gdk_window_get_pointer(scitew->wSciTE.GetID()->window, &x, &y, &state);
	scitew->ptStartDrag = Point(x, y);
	scitew->capturedMouse = true;
	scitew->heightOutputStartDrag = scitew->heightOutput;
	gtk_widget_grab_focus(GTK_WIDGET(scitew->wDivider.GetID()));
	gtk_grab_add(GTK_WIDGET(scitew->wDivider.GetID()));
	scitew->DividerXOR(scitew->ptStartDrag);
	return TRUE;
}

gint SciTEGTK::DividerRelease(GtkWidget *, GdkEventButton *, SciTEGTK *scitew) {
	if (scitew->capturedMouse) {
		scitew->capturedMouse = false;
		gtk_grab_remove(GTK_WIDGET(scitew->wDivider.GetID()));
		scitew->DividerXOR(scitew->ptOld);
		int x = 0;
		int y = 0;
		GdkModifierType state;
		gdk_window_get_pointer(scitew->wSciTE.GetID()->window, &x, &y, &state);
		scitew->MoveSplit(Point(x, y));
	}
	return TRUE;
}

void SciTEGTK::OpenCancelSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->fileSelector.Destroy();
}

void SciTEGTK::OpenKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->fileSelector.Destroy();
	}
}

void SciTEGTK::OpenOKSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->Open(gtk_file_selection_get_filename(
	                 GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	scitew->fileSelector.Destroy();
}

void SciTEGTK::SaveAsSignal(GtkWidget *, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Do Save As\n");
	if (scitew->savingHTML)
		scitew->SaveToHTML(gtk_file_selection_get_filename(
		                       GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	else
		scitew->SaveAs(gtk_file_selection_get_filename(
		                   GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	scitew->fileSelector.Destroy();
}

void SetFocus(GtkWidget *hwnd) {
	Platform::SendScintilla(hwnd, SCI_GRABFOCUS, 0, 0);
}

void SciTEGTK::Run(const char *cmdLine) {

	GtkItemFactoryCallback menuSig = GtkItemFactoryCallback(MenuSignal);
	GtkItemFactoryEntry menuItems[] = {
	    {"/_File", NULL, NULL, 0, "<Branch>"},
	    {"/_File/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/File/_New", "<control>N", menuSig, IDM_NEW, 0},
	    {"/File/_Open", "<control>O", menuSig, IDM_OPEN, 0},
	    {"/File/_Close", "<control>W", menuSig, IDM_CLOSE, 0},
	    {"/File/_Save", "<control>S", menuSig, IDM_SAVE, 0},
	    {"/File/Save _As", NULL, menuSig, IDM_SAVEAS, 0},
	    {"/File/Save As _HTML", NULL, menuSig, IDM_SAVEASHTML, 0},
	    {"/File/sep1", NULL, NULL, 0, "<Separator>"},
	    {"/File/File1", "", menuSig, fileStackCmdID + 1, 0},
	    {"/File/File2", "", menuSig, fileStackCmdID + 2, 0},
	    {"/File/File3", "", menuSig, fileStackCmdID + 3, 0},
	    {"/File/File4", "", menuSig, fileStackCmdID + 4, 0},
	    {"/File/File5", "", menuSig, fileStackCmdID + 5, 0},
	    {"/File/File6", "", menuSig, fileStackCmdID + 6, 0},
	    {"/File/File7", "", menuSig, fileStackCmdID + 7, 0},
	    {"/File/File8", "", menuSig, fileStackCmdID + 8, 0},
	    {"/File/File9", "", menuSig, fileStackCmdID + 9, 0},
	    {"/File/sep2", NULL, menuSig, IDM_MRU_SEP, "<Separator>"},
	    {"/File/_Quit", "<control>Q", menuSig, IDM_QUIT, 0},

	    {"/_Edit", NULL, NULL, 0, "<Branch>"},
	    {"/_Edit/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Edit/_Undo", "<control>Z", menuSig, IDM_UNDO, 0},
	    {"/Edit/_Redo", "<control>Y", menuSig, IDM_REDO, 0},
	    {"/Edit/sep1", NULL, NULL, 0, "<Separator>"},
	    {"/Edit/Cu_t", "<control>X", menuSig, IDM_CUT, 0},
	    {"/Edit/_Copy", "<control>C", menuSig, IDM_COPY, 0},
	    {"/Edit/_Paste", "<control>V", menuSig, IDM_PASTE, 0},
	    {"/Edit/_Delete", "Del", menuSig, IDM_CLEAR, 0},
	    {"/Edit/Select _All", "", menuSig, IDM_SELECTALL, 0},
	    {"/Edit/sep2", NULL, NULL, 0, "<Separator>"},
	    {"/Edit/_Find...", "<control>F", menuSig, IDM_FIND, 0},
	    {"/Edit/Find _Next", "F3", menuSig, IDM_FINDNEXT, 0},
	    {"/Edit/F_ind in Files...", "", menuSig, IDM_FINDINFILES, 0},
	    {"/Edit/R_eplace", "<control>H", menuSig, IDM_REPLACE, 0},
	    {"/Edit/sep3", NULL, NULL, 0, "<Separator>"},
	    {"/Edit/_Go To", "<control>G", menuSig, IDM_GOTO, 0},
	    {"/Edit/Match _Brace", "<control>B", menuSig, IDM_MATCHBRACE, 0},
	    {"/Edit/C_omplete Identifier", "<control>I", menuSig, IDM_COMPLETE, 0},
	    {"/Edit/Make Selection _Uppercase", "<control><shift>U", menuSig, IDM_UPRCASE, 0},
	    {"/Edit/Make Selection _Lowercase", "<control>U", menuSig, IDM_LWRCASE, 0},

	    {"/_Tools", NULL, NULL, 0, "<Branch>"},
	    {"/_Tools/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Tools/_Compile", "<control>F7", menuSig, IDM_COMPILE, 0},
	    {"/Tools/_Build", "F7", menuSig, IDM_BUILD, 0},
	    {"/Tools/_Go", "F5", menuSig, IDM_GO, 0},
	    {"/Tools/Tool0", "<control>0", menuSig, IDM_TOOLS + 0, 0},
	    {"/Tools/Tool1", "<control>1", menuSig, IDM_TOOLS + 1, 0},
	    {"/Tools/Tool2", "<control>2", menuSig, IDM_TOOLS + 2, 0},
	    {"/Tools/Tool3", "<control>3", menuSig, IDM_TOOLS + 3, 0},
	    {"/Tools/Tool4", "<control>4", menuSig, IDM_TOOLS + 4, 0},
	    {"/Tools/Tool5", "<control>5", menuSig, IDM_TOOLS + 5, 0},
	    {"/Tools/Tool6", "<control>6", menuSig, IDM_TOOLS + 6, 0},
	    {"/Tools/Tool7", "<control>7", menuSig, IDM_TOOLS + 7, 0},
	    {"/Tools/Tool8", "<control>8", menuSig, IDM_TOOLS + 8, 0},
	    {"/Tools/Tool9", "<control>9", menuSig, IDM_TOOLS + 9, 0},
	    {"/Tools/_Stop Executing", "<control>.", menuSig, IDM_STOPEXECUTE, NULL},
	    {"/Tools/sep1", NULL, NULL, 0, "<Separator>"},
	    {"/Tools/_Next Message", "F4", menuSig, IDM_NEXTMSG, 0},
	    {"/Tools/_Previous Message", "<shift>F4", menuSig, IDM_PREVMSG, 0},

	    {"/_Options", NULL, NULL, 0, "<Branch>"},
	    {"/_Options/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Options/Vertical _Split", "", menuSig, IDM_SPLITVERTICAL, "<CheckItem>"},
	    {"/Options/sep1", NULL, NULL, 0, "<Separator>"},
	    {"/Options/View _Whitespace", "<control>T", menuSig, IDM_VIEWSPACE, "<CheckItem>"},
	    {"/Options/View _Fold Margin", NULL, menuSig, IDM_FOLDMARGIN, "<CheckItem>"},
	    {"/Options/View _Margin", NULL, menuSig, IDM_SELMARGIN, "<CheckItem>"},
	    {"/Options/View Line _Numbers", "", menuSig, IDM_LINENUMBERMARGIN, "<CheckItem>"},
	    {"/Options/_View End of Line", "", menuSig, IDM_VIEWEOL, "<CheckItem>"},
	    {"/Options/View Status _Bar", "", menuSig, IDM_VIEWSTATUSBAR, "<CheckItem>"},
	    {"/Options/sep2", NULL, NULL, 0, "<Separator>"},
	    {"/Options/Line End C_haracters", "", 0, 0, "<Branch>"},
	    {"/Options/Line End Characters/CR _+ LF", "", menuSig, IDM_EOL_CRLF, "<RadioItem>"},
	    {"/Options/Line End Characters/_CR", "", menuSig, IDM_EOL_CR, "/Options/Line End Characters/CR + LF"},
	    {"/Options/Line End Characters/_LF", "", menuSig, IDM_EOL_LF, "/Options/Line End Characters/CR + LF"},
	    {"/Options/_Convert Line End Characters", "", menuSig, IDM_EOL_CONVERT, 0},
	    {"/Options/sep3", NULL, NULL, 0, "<Separator>"},
	    {"/Options/Open _Local Options File", "", menuSig, IDM_OPENLOCALPROPERTIES, 0},
	    {"/Options/Open _User Options File", "", menuSig, IDM_OPENUSERPROPERTIES, 0},
	    {"/Options/Open _Global Options File", "", menuSig, IDM_OPENGLOBALPROPERTIES, 0},

	    {"/_Help", NULL, NULL, 0, "<Branch>"},
	    {"/_Help/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Help/About SciTE", "", menuSig, IDM_ABOUT, 0},
	};

	char *gthis = reinterpret_cast<char *>(this);

	wSciTE = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//GTK_WIDGET_UNSET_FLAGS(wSciTE.GetID(), GTK_CAN_FOCUS);
	gtk_window_set_policy(GTK_WINDOW(wSciTE.GetID()), TRUE, TRUE, FALSE);

	gtk_widget_set_events(wSciTE.GetID(),
	                      GDK_EXPOSURE_MASK
	                      | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                     );
	gtk_signal_connect(GTK_OBJECT(wSciTE.GetID()), "destroy",
	                   GTK_SIGNAL_FUNC(QuitSignal), gthis);

	gtk_window_set_title(GTK_WINDOW(wSciTE.GetID()), appName);
	int left = props.GetInt("position.left", 10);
	int top = props.GetInt("position.top", 30);
	int width = props.GetInt("position.width", 300);
	int height = props.GetInt("position.height", 400);
	gtk_widget_set_usize(GTK_WIDGET(wSciTE.GetID()), width, height);

	GtkWidget *boxMain = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(wSciTE.GetID()), boxMain);
	GTK_WIDGET_UNSET_FLAGS(boxMain, GTK_CAN_FOCUS);

	int nItems = sizeof(menuItems) / sizeof(menuItems[0]);
	accelGroup = gtk_accel_group_new();
	itemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelGroup);
	gtk_item_factory_create_items(itemFactory, nItems, menuItems, gthis);

	gtk_accel_group_attach(accelGroup, GTK_OBJECT(wSciTE.GetID()));

	GtkWidget *handle_box = gtk_handle_box_new();

	gtk_container_add(GTK_CONTAINER(handle_box),
	                  gtk_item_factory_get_widget(itemFactory, "<main>"));

	gtk_box_pack_start(GTK_BOX(boxMain),
	                   handle_box,
	                   FALSE, FALSE, 0);

	content = gtk_fixed_new();
	GTK_WIDGET_UNSET_FLAGS(content.GetID(), GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(boxMain), content.GetID(), TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(content.GetID()), "size_allocate",
	                   GTK_SIGNAL_FUNC(MoveResize), gthis);

	wEditor = scintilla_new();
	scintilla_set_id(SCINTILLA(wEditor.GetID()), IDM_SRCWIN);
	gtk_fixed_put(GTK_FIXED(content.GetID()), wEditor.GetID(), 0, 0);
	gtk_widget_set_usize(wEditor.GetID(), 600, 600);
	gtk_signal_connect(GTK_OBJECT(wEditor.GetID()), "command",
	                   GtkSignalFunc(CommandSignal), this);
	gtk_signal_connect(GTK_OBJECT(wEditor.GetID()), "notify",
	                   GtkSignalFunc(NotifySignal), this);

	wDivider = gtk_drawing_area_new();
	gtk_signal_connect(GTK_OBJECT(wDivider.GetID()), "expose_event",
	                   GtkSignalFunc(DividerExpose), this);
	gtk_signal_connect(GTK_OBJECT(wDivider.GetID()), "motion_notify_event",
	                   GtkSignalFunc(DividerMotion), this);
	gtk_signal_connect(GTK_OBJECT(wDivider.GetID()), "button_press_event",
	                   GtkSignalFunc(DividerPress), this);
	gtk_signal_connect(GTK_OBJECT(wDivider.GetID()), "button_release_event",
	                   GtkSignalFunc(DividerRelease), this);
	gtk_widget_set_events(wDivider.GetID(),
	                      GDK_EXPOSURE_MASK
	                      | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                      | GDK_POINTER_MOTION_MASK
	                      | GDK_POINTER_MOTION_HINT_MASK
	                     );
	gtk_drawing_area_size(GTK_DRAWING_AREA(wDivider.GetID()), width, 10);
	gtk_fixed_put(GTK_FIXED(content.GetID()), wDivider.GetID(), 0, 600);

	wOutput = scintilla_new();
	scintilla_set_id(SCINTILLA(wOutput.GetID()), IDM_RUNWIN);
	gtk_fixed_put(GTK_FIXED(content.GetID()), wOutput.GetID(), 0, width);
	gtk_widget_set_usize(wOutput.GetID(), width, 100);
	gtk_signal_connect(GTK_OBJECT(wOutput.GetID()), "command",
	                   GtkSignalFunc(CommandSignal), this);
	gtk_signal_connect(GTK_OBJECT(wOutput.GetID()), "notify",
	                   GtkSignalFunc(NotifySignal), this);

	SendOutput(SCI_SETMARGINWIDTHN, 1, 0);
	//SendOutput(SCI_SETCARETPERIOD, 0);

	gtk_widget_set_uposition(GTK_WIDGET(wSciTE.GetID()), left, top);
	gtk_widget_show_all(wSciTE.GetID());

	wStatusBar = gtk_statusbar_new();
	sbContextID = gtk_statusbar_get_context_id(
	                  GTK_STATUSBAR(wStatusBar.GetID()), "global");
	gtk_box_pack_start(GTK_BOX(boxMain), wStatusBar.GetID(), FALSE, FALSE, 0);
	gtk_statusbar_push(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID, "Initial");
	sbVisible = false;

	SetFocus(wOutput.GetID());

	Open(cmdLine, true);
	CheckMenus();
	SizeSubWindows();
	SetFocus(wEditor.GetID());

	gtk_main();
}

int main(int argc, char *argv[]) {
	gtk_init(&argc, &argv);
	SciTEGTK scite;
	if (argc > 1) {
		//Platform::DebugPrintf("args: %d %s\n", argc, argv[1]);
		scite.Run(argv[1]);
	} else {
		scite.Run("");
	}

	return 0;
}
