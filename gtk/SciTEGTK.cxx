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
#include <assert.h> 

#include "Platform.h"

#include <unistd.h> 
#include <glib.h> 
#include <signal.h> 
#include <sys/wait.h> 
#include <sys/types.h> 
#include <errno.h> 

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "ScintillaWidget.h"
#include "Scintilla.h"
#include "Extender.h"
#include "SciTEBase.h"
#ifdef LUA_SCRIPTING
#include "LuaExtension.h"
#endif 
#include "pixmapsGNOME.h"

#define MB_ABOUTBOX	0x100000L

#ifdef STATIC_BUILD
const char appName[] = "Sc1";
#else
const char appName[] = "SciTE";
#endif 

#ifdef __vms
char g_modulePath[MAX_PATH];
#endif

class SciTEGTK : public SciTEBase {

protected:

	Point ptOld;
	GdkGC *xor_gc;

	guint sbContextID;
	Window wToolBarBox;

	// Control of sub process
	int icmd;
	int originalEnd;
	int fdFIFO;
	int pidShell;
	char resultsFile[MAX_PATH];
	int inputHandle;

#ifdef SINGLE_INSTANCE
	//Command Pipe variables
	int inputWatcher;
	int fdPipe;
	char pipeName[MAX_PATH];
#endif 

	char findInDir[1024];
	ComboMemory memDir;

	bool savingHTML;
	bool savingRTF;
	bool savingPDF;
	bool dialogCanceled;
	Window fileSelector;
	Window findInFilesDialog;
	GtkWidget *comboFiles;
	Window gotoDialog;
#ifdef CLIENT_3D_EFFECT
	Window topFrame;
	Window outputFrame;
#endif
	GtkWidget *gotoEntry;
	GtkWidget *toggleWord;
	GtkWidget *toggleCase;
	GtkWidget *toggleReverse;
	GtkWidget *toggleRec;
	GtkWidget *comboFind;
	GtkWidget *comboDir;
	GtkWidget *comboReplace;
	GtkWidget *compile_btn;
	GtkWidget *build_btn;
	GtkWidget *stop_btn;
	GtkItemFactory *itemFactory;
	GtkAccelGroup *accelGroup;

	gint	fileSelectorWidth;
	gint	fileSelectorHeight;

	void SetWindowName();
	void ShowFileInStatus();
	void SetIcon();

	virtual void ReadPropertiesInitial();
	virtual void ReadProperties();

	virtual void SizeContentWindows();
	virtual void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const char *text, const char *mnemonic = 0);
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();
	virtual void ExecuteNext();

	virtual void AbsolutePath(char *absPath, const char *relativePath, int size);
	virtual bool OpenDialog();
	virtual bool SaveAsDialog();
	virtual void SaveAsHTML();
	virtual void SaveAsRTF();
	virtual void SaveAsPDF();

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
	virtual void TabSizeDialog();

	virtual void GetDefaultDirectory(char *directory, size_t size);
	virtual bool GetSciteDefaultHome(char *path, unsigned int lenPath);
	virtual bool GetSciteUserHome(char *path, unsigned int lenPath);
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps,
	        char *pathDefaultDir, unsigned int lenPath);
	virtual bool GetUserPropertiesFileName(char *pathUserProps,
	                                       char *pathUserDir, unsigned int lenPath);

	virtual void SetStatusBarText(const char *s);
	void UpdateStatusBar();

	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar();
	virtual void ShowTabBar();
	virtual void ShowStatusBar();
	void Command(unsigned long wParam, long lParam = 0);
	void ContinueExecute();

#ifdef SINGLE_INSTANCE
	//ajkc
	bool SendPipeCommand(const char *pipeCommand);
	bool CreatePipe(bool forceNew = false);
	static void PipeSignal(SciTEGTK *scitew, gint fd, GdkInputCondition condition);
	void CheckAlreadyOpen(const char *cmdLine);
#endif 

	// GTK+ Signal Handlers

	static void OpenCancelSignal(GtkWidget *w, SciTEGTK *scitew);
	static void OpenKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew);
	static void OpenOKSignal(GtkWidget *w, SciTEGTK *scitew);
	static void OpenResizeSignal(GtkWidget *w, GtkAllocation *allocation, SciTEGTK *scitew);
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
	static gint QuitSignal(GtkWidget *w, GdkEventAny *e, SciTEGTK *scitew);
	static void ButtonSignal(GtkWidget *widget, gpointer data);
	static void MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *w);
	static void CommandSignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);
	static void NotifySignal(GtkWidget *w, gint wParam, gpointer lParam, SciTEGTK *scitew);
	static gint KeyPress(GtkWidget *widget, GdkEventKey *event, SciTEGTK *scitew);
	gint Key(GdkEventKey *event);

	void DividerXOR(Point pt);
	static gint DividerExpose(GtkWidget *widget, GdkEventExpose *ose, SciTEGTK *scitew);
	static gint DividerMotion(GtkWidget *widget, GdkEventMotion *event, SciTEGTK *scitew);
	static gint DividerPress(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);
	static gint DividerRelease(GtkWidget *widget, GdkEventButton *event, SciTEGTK *scitew);

public:

	// TODO: get rid of this - use callback argument to find SciTEGTK
	static SciTEGTK *instance;

	SciTEGTK(Extension *ext = 0);
	~SciTEGTK();

	void WarnUser(int warnID);
	GtkWidget *AddToolButton(const char *text, int cmd, char *icon[]);
	GtkWidget *pixmap_new(GtkWidget *window, gchar **xpm);
	void CreateMenu();
	void Run(int argc, char *argv[]);
	void ProcessExecute();
	virtual void Execute();
	virtual void StopExecute();
};

SciTEGTK *SciTEGTK::instance;

SciTEGTK::SciTEGTK(Extension *ext) : SciTEBase(ext) {
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
	savingRTF = false;
	savingPDF = false;
	dialogCanceled = false;
	comboFiles = 0;
	gotoEntry = 0;
	toggleWord = 0;
	toggleCase = 0;
	toggleReverse = 0;
	comboFind = 0;
	comboReplace = 0;
	itemFactory = 0;

	fileSelectorWidth = 580;
	fileSelectorHeight = 480;

	instance = this;
}

SciTEGTK::~SciTEGTK() {}

static void destroyDialog(GtkWidget *) {}

void SciTEGTK::WarnUser(int) {
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
		GtkWidget *button;
		guint Key;
		GtkAccelGroup *accel_group;
		accel_group = gtk_accel_group_new();

		messageBoxResult = -1;
		messageBoxDialog = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(messageBoxDialog), t);
		gtk_container_border_width(GTK_CONTAINER(messageBoxDialog), 0);

		gtk_signal_connect(GTK_OBJECT(messageBoxDialog),
		                   "destroy", GtkSignalFunc(destroyDialog), 0);

		int escapeResult = IDOK;
		if ((style & 0xf) == MB_OK) {
			button = AddMBButton(messageBoxDialog, "", IDOK, true);
			Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(button)->child), "_Ok");
			gtk_widget_add_accelerator(button, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
		} else {
			button = AddMBButton(messageBoxDialog, "", IDYES, true);
			Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(button)->child), "_Yes");
			gtk_widget_add_accelerator(button, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);

			button = AddMBButton(messageBoxDialog, "", IDNO);
			Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(button)->child), "_No");
			gtk_widget_add_accelerator(button, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
			escapeResult = IDNO;
			if (style == MB_YESNOCANCEL) {
				button = AddMBButton(messageBoxDialog, "", IDCANCEL);
				Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(button)->child), "_Cancel");
				gtk_widget_add_accelerator(button, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
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
		gtk_window_add_accel_group(GTK_WINDOW(messageBoxDialog), accel_group);
		while (messageBoxResult < 0) {
			gtk_main_iteration();
		}
	}
	return messageBoxResult;
}

void SciTEGTK::GetDefaultDirectory(char *directory, size_t size) {
	directory[0] = '\0';
	char *where = getenv("SciTE_HOME");
#ifdef SYSCONF_PATH
	if (!where) {
		where = SYSCONF_PATH;
	}
#else
	if (!where) {
		where = getenv("HOME");
	}
#endif 
	if (where)
#ifdef __vms
		strncpy(directory, VMSToUnixStyle(where), size);
#else
		strncpy(directory, where, size);
#endif
	directory[size - 1] = '\0';
}

bool SciTEGTK::GetSciteDefaultHome(char *path, unsigned int lenPath) {
	char *where = getenv("SciTE_HOME");
#ifdef __vms
	if (where == NULL) {
		where = g_modulePath;
	}
#endif
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
#ifdef __vms
		strncpy(path, VMSToUnixStyle(where), lenPath);
#else
		strncpy(path, where, lenPath);
#endif
		return true;
	}
	return false;
}

bool SciTEGTK::GetDefaultPropertiesFileName(char *pathDefaultProps,
        char *pathDefaultDir, unsigned int lenPath) {
	if (!GetSciteDefaultHome(pathDefaultDir, lenPath))
		return false;
	if (strlen(pathDefaultProps) + 1 + strlen(propGlobalFileName) < lenPath) {
		strncpy(pathDefaultProps, pathDefaultDir, lenPath);
		strncat(pathDefaultProps, pathSepString, lenPath);
		strncat(pathDefaultProps, propGlobalFileName, lenPath);
		return true;
	}
	return false;
}

bool SciTEGTK::GetSciteUserHome(char *path, unsigned int lenPath) {
	char *where = getenv("SciTE_HOME");
	if (!where) {
		where = getenv("HOME");
	}
	if (where) {
		strncpy(path, where, lenPath);
		return true;
	}
	return false;
}

bool SciTEGTK::GetUserPropertiesFileName(char *pathUserProps,
        char *pathUserDir, unsigned int lenPath) {
	if (!GetSciteUserHome(pathUserDir, lenPath))
		return false;
	if (strlen(pathUserProps) + 1 + strlen(propUserFileName) < lenPath) {
		strncpy(pathUserProps, pathUserDir, lenPath);
		strncat(pathUserProps, pathSepString, lenPath);
		strncat(pathUserProps, propUserFileName, lenPath);
		return true;
	}
	return false;
}

void SciTEGTK::ShowFileInStatus() {
	char sbText[1000];
	sprintf(sbText, " File: ");
	if (fileName[0] == '\0')
		strcat(sbText, "Untitled");
	else
		strcat(sbText, fullPath);
	SetStatusBarText(sbText);
}

void SciTEGTK::SetWindowName() {
	SciTEBase::SetWindowName();
	ShowFileInStatus();
}

void SciTEGTK::SetStatusBarText(const char *s) {
	gtk_statusbar_pop(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID);
	gtk_statusbar_push(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID, s);
}

void SciTEGTK::UpdateStatusBar() {
	SciTEBase::UpdateStatusBar();
}

void SciTEGTK::Notify(SCNotification *notification) {
	SciTEBase::Notify(notification);
}

void SciTEGTK::ShowToolBar() {
	if (tbVisible) {
		gtk_widget_show(GTK_WIDGET(wToolBarBox.GetID()));
	} else {
		gtk_widget_hide(GTK_WIDGET(wToolBarBox.GetID()));
	}
}

void SciTEGTK::ShowTabBar() {
	SizeSubWindows();
}

void SciTEGTK::ShowStatusBar() {
	if (sbVisible) {
		gtk_widget_show(GTK_WIDGET(wStatusBar.GetID()));
	} else {
		gtk_widget_hide(GTK_WIDGET(wStatusBar.GetID()));
	}
}

void SciTEGTK::Command(unsigned long wParam, long) {
	int cmdID = ControlIDOfCommand(wParam);
	switch (cmdID) {

#ifdef EN_SETFOCUS
	case IDM_SRCWIN:
	case IDM_RUNWIN:
		if ((wParam >> 16) == EN_SETFOCUS)
			CheckMenus();
		break;
#endif 

	default:
		SciTEBase::MenuCommand(cmdID);
	}
	UpdateStatusBar();
}

void SciTEGTK::ReadPropertiesInitial() {
	SciTEBase::ReadPropertiesInitial();
	ShowToolBar();
	ShowStatusBar();
}

void SciTEGTK::ReadProperties() {
	SciTEBase::ReadProperties();
	CheckMenus();
}

void SciTEGTK::SizeContentWindows() {
	PRectangle rcClient = GetClientRectangle();
	int w = rcClient.right - rcClient.left;
	int h = rcClient.bottom - rcClient.top;
	heightOutput = NormaliseSplit(heightOutput);
	if (splitVertical) {
#ifdef CLIENT_3D_EFFECT
		topFrame.SetPosition(PRectangle(0, 0, w - heightOutput - heightBar, h));
		wDivider.SetPosition(PRectangle(w - heightOutput - heightBar, 0, w - heightOutput, h));
		outputFrame.SetPosition(PRectangle(w - heightOutput, 0, w, h));
#else
		wEditor.SetPosition(PRectangle(0, 0, w - heightOutput - heightBar, h));
		wDivider.SetPosition(PRectangle(w - heightOutput - heightBar, 0, w - heightOutput, h));
		wOutput.SetPosition(PRectangle(w - heightOutput, 0, w, h));
#endif
	} else {
#ifdef CLIENT_3D_EFFECT
		topFrame.SetPosition(PRectangle(0, 0, w, h - heightOutput - heightBar));
		wDivider.SetPosition(PRectangle(0, h - heightOutput - heightBar, w, h - heightOutput));
		outputFrame.SetPosition(PRectangle(0, h - heightOutput, w, h));
#else
		wEditor.SetPosition(PRectangle(0, 0, w, h - heightOutput - heightBar));
		wDivider.SetPosition(PRectangle(0, h - heightOutput - heightBar, w, h - heightOutput));
		wOutput.SetPosition(PRectangle(0, h - heightOutput, w, h));
#endif
	}
}

void SciTEGTK::SizeSubWindows() {
	SizeContentWindows();
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
	if (itemID) {
		GtkWidget *item = gtk_item_factory_get_widget_by_action(itemFactory, itemID);
		if (item) {
			gtk_widget_hide(item);
		}
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

	gtk_widget_set_sensitive(build_btn, !executing);
	gtk_widget_set_sensitive(compile_btn, !executing);
	gtk_widget_set_sensitive(stop_btn, executing);
}

char *split(char*& s, char c) {
	char *t = s;
	if (s && (s = strchr(s, c))) * s++ = '\0';
	return t;
}

// on Linux return the shortest path equivalent to pathname (remove . and ..)
void SciTEGTK::AbsolutePath(char *absPath, const char *relativePath, int /*size*/) {
	char path[MAX_PATH + 1], *cur, *last, *part, *tmp;
	if (!absPath)
		return ;
	if (!relativePath)
		return ;
	strcpy(path, relativePath);
	cur = absPath;
	*cur = '\0';
	tmp = path;
	last = NULL;
	if (*tmp == pathSepChar) {
		*cur++ = pathSepChar;
		*cur = '\0';
		tmp++;
	}
	while ((part = split(tmp, pathSepChar))) {
		if (strcmp(part, ".") == 0)
			;
		else if (strcmp(part, "..") == 0 && (last = strrchr(absPath, pathSepChar))) {
			if (last > absPath)
				cur = last;
			else
				cur = last + 1;
			*cur = '\0';
		} else {
			if (cur > absPath && *(cur - 1) != pathSepChar) *cur++ = pathSepChar;
			strcpy(cur, part);
			cur += strlen(part);
		}
	}

	// Remove trailing backslash(es)
	while (absPath < cur - 1 && *(cur - 1) == pathSepChar && *(cur - 2) != ':') {
		*--cur = '\0';
	}
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
		gtk_signal_connect(GTK_OBJECT(fileSelector.GetID()),
		                   "size_allocate", GtkSignalFunc(OpenResizeSignal),
		                   this);
		// Other ways to destroy
		// Mark it as a modal transient dialog
		gtk_window_set_modal(GTK_WINDOW(fileSelector.GetID()), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(fileSelector.GetID()),
		                              GTK_WINDOW(wSciTE.GetID()));
		// Get a bigger open dialog
		gtk_window_set_default_size(GTK_WINDOW(fileSelector.GetID()),
		                            fileSelectorWidth, fileSelectorHeight);
		fileSelector.Show();
		while (fileSelector.Created()) {
			gtk_main_iteration();
		}
	}
	return !dialogCanceled;
}

bool SciTEGTK::SaveAsDialog() {
	savingHTML = false;
	savingRTF = false;
	savingPDF = false;
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
		// Get a bigger save as dialog
		gtk_window_set_default_size(GTK_WINDOW(fileSelector.GetID()),
		                            fileSelectorWidth, fileSelectorHeight);
		fileSelector.Show();
		while (fileSelector.Created()) {
			gtk_main_iteration();
		}
	}
	return !dialogCanceled;
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
		while (fileSelector.Created()) {
			gtk_main_iteration();
		}
	}
}

void SciTEGTK::SaveAsRTF() {
	if (!fileSelector.Created()) {
		savingRTF = true;
		fileSelector = gtk_file_selection_new("Save File As RTF");
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
		while (fileSelector.Created()) {
			gtk_main_iteration();
		}
	}
}

void SciTEGTK::SaveAsPDF() {
	if (!fileSelector.Created()) {
		savingPDF = true;
		fileSelector = gtk_file_selection_new("Save File As PDF");
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
		while (fileSelector.Created()) {
			gtk_main_iteration();
		}
	}
}

void SciTEGTK::Print() {
	// Printing not yet supported on GTK+
}

void SciTEGTK::PrintSetup() {
	// Printing not yet supported on GTK+
}

void SciTEGTK::HandleFindReplace() {}

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
	if (!scitew->comboReplace)
		scitew->wFindReplace.Destroy();
	if (scitew->findWhat[0]) {
		scitew->FindNext(scitew->reverseFind);
	}
}

void SciTEGTK::FRReplaceSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->FindReplaceGrabFields();
	if (scitew->findWhat[0]) {
		scitew->ReplaceOnce();
	}
}

void SciTEGTK::FRReplaceAllSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->FindReplaceGrabFields();
	if (scitew->findWhat[0]) {
		scitew->ReplaceAll();
		scitew->wFindReplace.Destroy();
	}
}

void SciTEGTK::FindInFilesSignal(GtkWidget *, SciTEGTK *scitew) {
	char *findEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scitew->comboFind)->entry));
	scitew->props.Set("find.what", findEntry);
	scitew->memFinds.Insert(findEntry);

	char *dirEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scitew->comboDir)->entry));
	scitew->props.Set("find.dir", dirEntry);
	scitew->memDir.Insert(dirEntry);

#ifdef RECURSIVE_GREP_WORKING
	if (GTK_TOGGLE_BUTTON(scitew->toggleRec)->active)
		scitew->props.Set("find.recursive", scitew->props.Get("find.recursive.recursive").c_str());
	else
		scitew->props.Set("find.recursive", scitew->props.Get("find.recursive.not").c_str());
#endif 

	char *filesEntry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scitew->comboFiles)->entry));
	scitew->props.Set("find.files", filesEntry);
	scitew->memFiles.Insert(filesEntry);

	scitew->findInFilesDialog.Destroy();

	//printf("Grepping for <%s> in <%s>\n",
	//	scitew->props.Get("find.what"),
	//	scitew->props.Get("find.files"));
	scitew->SelectionIntoProperties();
	scitew->AddCommand(scitew->props.GetNewExpand("find.command", ""), dirEntry, jobCLI);
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
	GtkAccelGroup *accel_group;
	accel_group = gtk_accel_group_new();

	SelectionIntoFind();
	props.Set("find.what", findWhat);

	getcwd(findInDir, sizeof(findInDir));
	props.Set("find.dir", findInDir);

	findInFilesDialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(findInFilesDialog.GetID()), TRUE, TRUE, TRUE);
	findInFilesDialog.SetTitle("Find In Files");

	gtk_signal_connect(GTK_OBJECT(findInFilesDialog.GetID()),
	                   "destroy", GtkSignalFunc(destroyDialog), 0);

#ifdef RECURSIVE_GREP_WORKING
	GtkWidget *table = gtk_table_new(4, 2, FALSE);
#else
	GtkWidget *table = gtk_table_new(3, 2, FALSE);
#endif 
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
	gtk_combo_set_case_sensitive(GTK_COMBO(comboFind), TRUE);
	gtk_combo_set_use_arrows_always(GTK_COMBO(comboFind), TRUE);

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
	gtk_combo_set_case_sensitive(GTK_COMBO(comboFiles), TRUE);
	gtk_combo_set_use_arrows_always(GTK_COMBO(comboFiles), TRUE);

	gtk_table_attach(GTK_TABLE(table), comboFiles, 1, 2,
	                 row, row + 1, optse, opts, 5, 5);
	gtk_widget_show(comboFiles);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(comboFiles)->entry), props.Get("find.files").c_str());
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboFiles)->entry),
	                   "activate", GtkSignalFunc(FindInFilesSignal), this);

	row++;

	GtkWidget *labelDir = gtk_label_new("Directory:");

	gtk_table_attach(GTK_TABLE(table), labelDir, 0, 1,
	                 row, row + 1, opts, opts, 5, 5);
	gtk_widget_show(labelDir);

	comboDir = gtk_combo_new();
	FillComboFromMemory(comboDir, memDir);
	gtk_combo_set_case_sensitive(GTK_COMBO(comboDir), TRUE);
	gtk_combo_set_use_arrows_always(GTK_COMBO(comboDir), TRUE);

	gtk_table_attach(GTK_TABLE(table), comboDir, 1, 2,
	                 row, row + 1, optse, opts, 5, 5);
	gtk_widget_show(comboDir);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(comboDir)->entry), findInDir);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(comboDir)->entry),
	                   "activate", GtkSignalFunc(FindInFilesSignal), this);

#ifdef RECURSIVE_GREP_WORKING
	row++;

	toggleRec = gtk_check_button_new_with_label("");
	GTK_WIDGET_UNSET_FLAGS(toggleRec, GTK_CAN_FOCUS);
	guint Key;
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(toggleRec)->child), "Re_cursive Directories");
	gtk_widget_add_accelerator(	toggleRec, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	gtk_table_attach(GTK_TABLE(table), toggleRec, 1, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleRec);
#endif 

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

	gtk_window_add_accel_group(GTK_WINDOW(findInFilesDialog.GetID()), accel_group);
	findInFilesDialog.Show();
}

void SciTEGTK::Replace() {
	SelectionIntoFind();
	FindReplace(true);
}

void SciTEGTK::ExecuteNext() {
	icmd++;
	if (icmd < commandCurrent && icmd < commandMax) {
		Execute();
	} else {
		icmd = 0;
		executing = false;
		CheckReload();
		CheckMenus();
		for (int ic = 0; ic < commandMax; ic++) {
			jobQueue[ic].Clear();
		}
		commandCurrent = 0;
	}
	return ;
}

void SciTEGTK::ContinueExecute() {
	char buf[8192];
	int count = 0;
	count = read(fdFIFO, buf, sizeof(buf) - 1);
	if (count < 0) {
		OutputAppendString(">End Bad\n");
		return ;
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
		ExecuteNext();
	}
	if (count > 0) {
		buf[count] = '\0';
		OutputAppendString(buf);
		if (count < 0) {
			OutputAppendString(">Continue no data\n");
			return ;
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

	SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
	originalEnd = SendOutput(SCI_GETCURRENTPOS);

	OutputAppendString(">");
	OutputAppendString(jobQueue[icmd].command.c_str());
	OutputAppendString("\n");

	unlink(resultsFile);
	if (jobQueue[icmd].directory != "") {
		chdir(jobQueue[icmd].directory.c_str());
	}

	if (jobQueue[icmd].jobType == jobShell) {
		if (fork() == 0)
			execlp("/bin/sh", "sh", "-c", jobQueue[icmd].command.c_str(), 0);
		else
			ExecuteNext();
	} else if (jobQueue[icmd].jobType == jobExtension) {
		if (extender)
			extender->OnExecute(jobQueue[icmd].command.c_str());
	} else {
#ifndef __vms
		if (mkfifo(resultsFile, S_IRUSR | S_IWUSR) < 0) {
#else           // no mkfifo on OpenVMS!
                creat (resultsFile, 0777);
                if (jobQueue[icmd].jobType == jobShell) {   // Always false!
#endif
			OutputAppendString(">Failed to create FIFO\n");
			ExecuteNext();
			return ;
		}

		pidShell = xsystem(jobQueue[icmd].command.c_str(), resultsFile);
		fdFIFO = open(resultsFile, O_RDONLY | O_NONBLOCK);
		if (fdFIFO < 0) {
			OutputAppendString(">Failed to open\n");
			return ;
		}
		inputHandle = gdk_input_add(fdFIFO, GDK_INPUT_READ,
		                            (GdkInputFunction) IOSignal, this);
	}
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

	scitew->GotoLineEnsureVisible(lineNo - 1);

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

void SciTEGTK::TabSizeDialog() {}

void SciTEGTK::FindReplace(bool replace) {
	guint Key;
	GtkAccelGroup *accel_group;
	accel_group = gtk_accel_group_new();

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
	gtk_combo_set_case_sensitive(GTK_COMBO(comboFind), TRUE);
	gtk_combo_set_use_arrows_always(GTK_COMBO(comboFind), TRUE);
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
		gtk_combo_set_case_sensitive(GTK_COMBO(comboReplace), TRUE);
		gtk_combo_set_use_arrows_always(GTK_COMBO(comboReplace), TRUE);

		row++;
	} else {
		comboReplace = 0;
	}

	// Case Sensitive
	toggleCase = gtk_check_button_new_with_label("");
	GTK_WIDGET_UNSET_FLAGS(toggleCase, GTK_CAN_FOCUS);
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(toggleCase)->child), "Case _Sensitive");
	gtk_widget_add_accelerator(	toggleCase, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	gtk_table_attach(GTK_TABLE(table), toggleCase, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleCase);
	row++;

	// Whole Word
	toggleWord = gtk_check_button_new_with_label("");
	GTK_WIDGET_UNSET_FLAGS(toggleWord, GTK_CAN_FOCUS);
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(toggleWord)->child), "Whole _Word");
	gtk_widget_add_accelerator(	toggleWord, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	gtk_table_attach(GTK_TABLE(table), toggleWord, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleWord);
	row++;

	// Whole Word
	toggleReverse = gtk_check_button_new_with_label("");
	GTK_WIDGET_UNSET_FLAGS(toggleReverse, GTK_CAN_FOCUS);
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(toggleReverse)->child), "_Reverse Direction");
	gtk_widget_add_accelerator(	toggleReverse, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	gtk_table_attach(GTK_TABLE(table), toggleReverse, 0, 2, row, row + 1, opts, opts, 3, 0);
	gtk_widget_show(toggleReverse);

	gtk_widget_show(table);

	GtkWidget *buttonFind = gtk_button_new_with_label("");
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(buttonFind)->child), "F_ind");
	//GTK_WIDGET_UNSET_FLAGS (buttonFind, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (buttonFind, GTK_CAN_DEFAULT);
	gtk_widget_add_accelerator(	buttonFind, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	gtk_signal_connect(GTK_OBJECT(buttonFind),
	                   "clicked", GtkSignalFunc(FRFindSignal), this);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
	                   buttonFind, TRUE, TRUE, 0);
	gtk_widget_show(buttonFind);

	if (replace) {
		GtkWidget *buttonReplace = gtk_button_new_with_label("");
		Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(buttonReplace)->child), "_Replace");
		gtk_widget_add_accelerator(	buttonReplace, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
		//GTK_WIDGET_UNSET_FLAGS (buttonReplace, GTK_CAN_FOCUS);
		GTK_WIDGET_SET_FLAGS (buttonReplace, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
		                   buttonReplace, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(buttonReplace),
		                   "clicked", GtkSignalFunc(FRReplaceSignal), this);
		gtk_widget_show(buttonReplace);

		GtkWidget *buttonReplaceAll = gtk_button_new_with_label("");
		Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(buttonReplaceAll)->child), "Replace _All");
		gtk_widget_add_accelerator(	buttonReplaceAll, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
		//GTK_WIDGET_UNSET_FLAGS (buttonReplaceAll, GTK_CAN_FOCUS);
		GTK_WIDGET_SET_FLAGS (buttonReplaceAll, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wFindReplace.GetID())->action_area),
		                   buttonReplaceAll, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(buttonReplaceAll),
		                   "clicked", GtkSignalFunc(FRReplaceAllSignal), this);
		gtk_widget_show(buttonReplaceAll);
	}

	GtkWidget *buttonCancel = gtk_button_new_with_label("");
	Key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(buttonCancel)->child), "_Cancel");
	gtk_widget_add_accelerator(	buttonCancel, "clicked", accel_group, Key, GDK_MOD1_MASK, (GtkAccelFlags)0);
	//GTK_WIDGET_UNSET_FLAGS (buttonCancel, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (buttonCancel, GTK_CAN_DEFAULT);
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

	gtk_window_add_accel_group(GTK_WINDOW(wFindReplace.GetID()), accel_group);
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
	if (SaveIfUnsureAll() != IDCANCEL) {
#ifdef SINGLE_INSTANCE
		//clean up any pipes that are ours
		if (fdPipe != -1 && inputWatcher != -1) {
			//printf("Cleaning up pipe\n");
			close(fdPipe);
			unlink(pipeName);

		}
#endif 
		gtk_exit(0);
	}
}

gint SciTEGTK::MoveResize(GtkWidget *, GtkAllocation * /*allocation*/, SciTEGTK *scitew) {
	//Platform::DebugPrintf("SciTEGTK move resize %d %d\n", allocation->width, allocation->height);
	scitew->SizeSubWindows();
	return TRUE;
}

gint SciTEGTK::QuitSignal(GtkWidget *, GdkEventAny *, SciTEGTK *scitew) {
	if (scitew->SaveIfUnsureAll() != IDCANCEL) {

#ifdef SINGLE_INSTANCE
		//clean up any pipes that are ours
		if (scitew->fdPipe != -1 && scitew->inputWatcher != -1) {
			//printf("Cleaning up pipe\n");
			close(scitew->fdPipe);
			unlink(scitew->pipeName);

		}
#endif 
		gtk_exit(0);
	}
	// No need to return FALSE for quit as gtk_exit will have been called
	// if needed.
	return TRUE;
}

void SciTEGTK::ButtonSignal(GtkWidget *, gpointer data) {
	instance->Command((guint)data);
}

void SciTEGTK::MenuSignal(SciTEGTK *scitew, guint action, GtkWidget *) {
	//Platform::DebugPrintf("action %d %x \n", action, w);
	if (scitew->allowMenuActions)
		scitew->Command(action);
}

void SciTEGTK::CommandSignal(GtkWidget *, gint wParam, gpointer lParam, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Command: %x %x %x\n", w, wParam, lParam);
	scitew->Command(wParam, reinterpret_cast<long>(lParam));
}

void SciTEGTK::NotifySignal(GtkWidget *, gint /*wParam*/, gpointer lParam, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Notify: %x %x %x\n", w, wParam, lParam);
	scitew->Notify(reinterpret_cast<SCNotification *>(lParam));
}

gint SciTEGTK::KeyPress(GtkWidget */*widget*/, GdkEventKey *event, SciTEGTK *scitew) {
	return scitew->Key(event);
}

gint SciTEGTK::Key(GdkEventKey *event) {
//printf("S-key: %d %x %x %x %x\n",event->keyval, event->state, GDK_SHIFT_MASK, GDK_CONTROL_MASK, GDK_F3);
	int mods = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
	int key = event->keyval;
	if ((mods == GDK_CONTROL_MASK) && (key == GDK_ISO_Left_Tab)) {
		Command(IDM_NEXTFILE);
	} else if ((mods == GDK_CONTROL_MASK) && (key == GDK_KP_Enter)) {
		Command(IDM_COMPLETEWORD);
	} else if ((mods == GDK_CONTROL_MASK | GDK_SHIFT_MASK) && (key == GDK_ISO_Left_Tab)) {
		Command(IDM_PREVFILE);
	} else if ((mods == GDK_CONTROL_MASK) && (key == GDK_F3)) {
		Command(IDM_FINDNEXTSEL);
	} else if ((mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) && (key == GDK_F3)) {
		Command(IDM_FINDNEXTBACKSEL);
	}
	return 0;
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
	scitew->dialogCanceled = true;
	scitew->fileSelector.Destroy();
}

void SciTEGTK::OpenKeySignal(GtkWidget *w, GdkEventKey *event, SciTEGTK *scitew) {
	if (event->keyval == GDK_Escape) {
		scitew->dialogCanceled = true;
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w), "key_press_event");
		scitew->fileSelector.Destroy();
	}
}

void SciTEGTK::OpenOKSignal(GtkWidget *, SciTEGTK *scitew) {
	scitew->dialogCanceled = false;
	scitew->Open(gtk_file_selection_get_filename(
	                 GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	scitew->fileSelector.Destroy();
}

void SciTEGTK::OpenResizeSignal(GtkWidget *, GtkAllocation *allocation, SciTEGTK *scitew) {
	scitew->fileSelectorWidth = allocation->width;
	scitew->fileSelectorHeight = allocation->height;
}

void SciTEGTK::SaveAsSignal(GtkWidget *, SciTEGTK *scitew) {
	//Platform::DebugPrintf("Do Save As\n");
	scitew->dialogCanceled = false;
	if (scitew->savingHTML)
		scitew->SaveToHTML(gtk_file_selection_get_filename(
		                       GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	else if (scitew->savingRTF)
		scitew->SaveToRTF(gtk_file_selection_get_filename(
		                      GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	else if (scitew->savingPDF)
		scitew->SaveToPDF(gtk_file_selection_get_filename(
		                      GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	else
		scitew->SaveAs(gtk_file_selection_get_filename(
		                   GTK_FILE_SELECTION(scitew->fileSelector.GetID())));
	scitew->fileSelector.Destroy();
}

void SetFocus(GtkWidget *hwnd) {
	Platform::SendScintilla(hwnd, SCI_GRABFOCUS, 0, 0);
}

GtkWidget *SciTEGTK::AddToolButton(const char *text, int cmd, char *icon[]) {

	GtkWidget *toolbar_icon = pixmap_new(wSciTE.GetID(), icon);
	GtkWidget *button = gtk_toolbar_append_element(GTK_TOOLBAR(wToolBar.GetID()),
	                    GTK_TOOLBAR_CHILD_BUTTON,
	                    NULL,
	                    NULL,
	                    text, NULL,
	                    toolbar_icon, NULL, NULL);

	//gtk_container_set_border_width(GTK_CONTAINER(button), 2);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
	                   GTK_SIGNAL_FUNC (ButtonSignal),
	                   (gpointer)cmd);
	return button;
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

#define ELEMENTS(a) (sizeof(a) / sizeof(a[0]))

void SciTEGTK::CreateMenu() {

	GtkItemFactoryCallback menuSig = GtkItemFactoryCallback(MenuSignal);
	GtkItemFactoryEntry menuItems[] = {
	    {"/_File", NULL, NULL, 0, "<Branch>"},
	    {"/_File/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/File/_New", "<control>N", menuSig, IDM_NEW, 0},
	    {"/File/_Open", "<control>O", menuSig, IDM_OPEN, 0},
	    {"/File/_Open Selected Filename", "<control><shift>O", menuSig, IDM_OPENSELECTED, 0},
	    {"/File/_Revert", "<control>R", menuSig, IDM_REVERT, 0},
	    {"/File/_Close", "<control>W", menuSig, IDM_CLOSE, 0},
	    {"/File/_Save", "<control>S", menuSig, IDM_SAVE, 0},
	    {"/File/Save _As", NULL, menuSig, IDM_SAVEAS, 0},
	    {"/File/_Export", "", 0, 0, "<Branch>"},
	    {"/File/Export/As _HTML", NULL, menuSig, IDM_SAVEASHTML, 0},
	    {"/File/Export/As _RTF", NULL, menuSig, IDM_SAVEASRTF, 0},
	    {"/File/Export/As _PDF", NULL, menuSig, IDM_SAVEASPDF, 0},
	    {"/File/sep1", NULL, NULL, 0, "<Separator>"},
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
	    {"/Edit/Select _All", "<control>A", menuSig, IDM_SELECTALL, 0},
	    {"/Edit/sep2", NULL, NULL, 0, "<Separator>"},
	    {"/Edit/Match _Brace", "<control>E", menuSig, IDM_MATCHBRACE, 0},
	    {"/Edit/Select t_o Brace", "<control><shift>E", menuSig, IDM_SELECTTOBRACE, 0},
	    {"/Edit/S_how CallTip", "<control><shift>space", menuSig, IDM_SHOWCALLTIP, 0},
	    {"/Edit/Complete S_ymbol", "<control>I", menuSig, IDM_COMPLETE, 0},
	    {"/Edit/Complete _Word", "<control>Return", menuSig, IDM_COMPLETEWORD, 0},
	    {"/Edit/Toggle _all folds", "", menuSig, IDM_TOGGLE_FOLDALL, 0},
	    {"/Edit/Make _Selection Uppercase", "<control><shift>U", menuSig, IDM_UPRCASE, 0},
	    {"/Edit/Make Selection _Lowercase", "<control>U", menuSig, IDM_LWRCASE, 0},

	    {"/_Search", NULL, NULL, 0, "<Branch>"},
	    {"/_Search/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Search/_Find...", "<control>F", menuSig, IDM_FIND, 0},
	    {"/Search/Find _Next", "F3", menuSig, IDM_FINDNEXT, 0},
	    {"/Search/Find Previou_s", "<shift>F3", menuSig, IDM_FINDNEXTBACK, 0},
	    {"/Search/F_ind in Files...", "<control>J", menuSig, IDM_FINDINFILES, 0},
	    {"/Search/R_eplace", "<control>H", menuSig, IDM_REPLACE, 0},
	    {"/Search/sep3", NULL, NULL, 0, "<Separator>"},
	    {"/Search/_Go To", "<control>G", menuSig, IDM_GOTO, 0},
	    {"/Search/Next Book_mark", "F2", menuSig, IDM_BOOKMARK_NEXT, 0},
	    {"/Search/Pre_vious Bookmark", "<shift>F2", menuSig, IDM_BOOKMARK_PREV, 0},
	    {"/Search/Toggle Bookmar_k", "<control>F2", menuSig, IDM_BOOKMARK_TOGGLE, 0},

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
	    {"/Options/View _Whitespace", "<control><shift>A", menuSig, IDM_VIEWSPACE, "<CheckItem>"},
	    {"/Options/_View End of Line", "<control><shift>B", menuSig, IDM_VIEWEOL, "<CheckItem>"},
	    {"/Options/View _Indentation Guides", NULL, menuSig, IDM_VIEWGUIDES, "<CheckItem>"},
	    {"/Options/View _Margin", NULL, menuSig, IDM_SELMARGIN, "<CheckItem>"},
	    {"/Options/View _Fold Margin", NULL, menuSig, IDM_FOLDMARGIN, "<CheckItem>"},
	    {"/Options/View Line _Numbers", "", menuSig, IDM_LINENUMBERMARGIN, "<CheckItem>"},
	    {"/Options/View _Toolbar", "", menuSig, IDM_VIEWTOOLBAR, "<CheckItem>"},
	    {"/Options/View _Output", "F8", menuSig, IDM_TOGGLEOUTPUT, "<CheckItem>"},
	    {"/Options/View Status _Bar", "", menuSig, IDM_VIEWSTATUSBAR, "<CheckItem>"},
	    {"/Options/sep2", NULL, NULL, 0, "<Separator>"},
	    {"/Options/Line End C_haracters", "", 0, 0, "<Branch>"},
	    {"/Options/Line End Characters/CR _+ LF", "", menuSig, IDM_EOL_CRLF, "<RadioItem>"},
	    {"/Options/Line End Characters/_CR", "", menuSig, IDM_EOL_CR, "/Options/Line End Characters/CR + LF"},
	    {"/Options/Line End Characters/_LF", "", menuSig, IDM_EOL_LF, "/Options/Line End Characters/CR + LF"},
	    {"/Options/_Convert Line End Characters", "", menuSig, IDM_EOL_CONVERT, 0},
	    {"/Options/Use le_xer", "", 0, 0, "<Branch>"},
	    {"/Options/Use lexer/none", "F11", menuSig, IDM_LEXER_NONE, "<RadioItem>"},
	    {"/Options/Use lexer/_C, C++", "", menuSig, IDM_LEXER_CPP, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/C_#", "", menuSig, IDM_LEXER_CS, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_VB", "", menuSig, IDM_LEXER_VB, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Reso_urce", "", menuSig, IDM_LEXER_RC, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/H_ypertext", "F12", menuSig, IDM_LEXER_HTML, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_XML", "<shift>F12", menuSig, IDM_LEXER_XML, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Java_Script", "", menuSig, IDM_LEXER_JS, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/VBScr_ipt", "", menuSig, IDM_LEXER_WSCRIPT, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Properties", "", menuSig, IDM_LEXER_PROPS, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Batch", "", menuSig, IDM_LEXER_BATCH, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Makefile", "", menuSig, IDM_LEXER_MAKE, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Errorlist", "", menuSig, IDM_LEXER_ERRORL, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Difference", "", menuSig, IDM_LEXER_DIFF, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/_Java", "", menuSig, IDM_LEXER_JAVA, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Lu_a", "", menuSig, IDM_LEXER_LUA, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Pytho_n", "", menuSig, IDM_LEXER_PYTHON, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Pe_rl", "", menuSig, IDM_LEXER_PERL, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/S_QL", "", menuSig, IDM_LEXER_SQL, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/P_LSQ", "", menuSig, IDM_LEXER_PLSQL, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/P_HP", "", menuSig, IDM_LEXER_PHP, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/La_TeX", "", menuSig, IDM_LEXER_LATEX, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Apache Config", "", menuSig, IDM_LEXER_CONF, "/Options/Use lexer/none"},
	    {"/Options/Use lexer/Pascal", "", menuSig, IDM_LEXER_PASCAL, "/Options/Use lexer/none"},
	    {"/Options/sep3", NULL, NULL, 0, "<Separator>"},
	    {"/Options/Open _Local Options File", "", menuSig, IDM_OPENLOCALPROPERTIES, 0},
	    {"/Options/Open _User Options File", "", menuSig, IDM_OPENUSERPROPERTIES, 0},
	    {"/Options/Open _Global Options File", "", menuSig, IDM_OPENGLOBALPROPERTIES, 0},
	};

	GtkItemFactoryEntry menuItemsBuffer[] = {
	    {"/_Buffers", NULL, NULL, 0, "<Branch>"},
	    {"/_Buffers/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Buffers/_Previous Buffer", "<shift>F6", menuSig, IDM_PREVFILE, 0},
	    {"/Buffers/_Next Buffer", "F6", menuSig, IDM_NEXTFILE, 0},
	    {"/Buffers/_Close All", "", menuSig, IDM_CLOSEALL, 0},
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

	GtkItemFactoryEntry menuItemsHelp[] = {
	    {"/_Help", NULL, NULL, 0, "<Branch>"},
	    {"/_Help/tear", NULL, NULL, 0, "<Tearoff>"},
	    {"/Help/_Help", "F1", menuSig, IDM_HELP, 0},
	    {"/Help/_About SciTE", "", menuSig, IDM_ABOUT, 0},
	};

	char *gthis = reinterpret_cast<char *>(this);
	accelGroup = gtk_accel_group_new();
	itemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelGroup);
	gtk_item_factory_create_items(itemFactory, ELEMENTS(menuItems), menuItems, gthis);
	if (props.GetInt("buffers") > 1)
		gtk_item_factory_create_items(itemFactory, ELEMENTS(menuItemsBuffer), menuItemsBuffer, gthis);
	gtk_item_factory_create_items(itemFactory, ELEMENTS(menuItemsHelp), menuItemsHelp, gthis);

	gtk_accel_group_attach(accelGroup, GTK_OBJECT(wSciTE.GetID()));
}

void SciTEGTK::SetIcon() {
#if WORKING
#include "Icon.xpm"
	GtkStyle *style;
	GdkPixmap *icon_pix;
	GdkBitmap *mask;
	style = gtk_widget_get_style(wSciTE.GetID());
	icon_pix = gdk_pixmap_create_from_xpm_d(wSciTE.GetID()->window,
	                                        &mask,
	                                        &style->bg[GTK_STATE_NORMAL],
	                                        (gchar **)Icon_xpm);
	gdk_window_set_icon(wSciTE.GetID()->window, NULL, icon_pix, mask);
#endif 
}

#ifdef SINGLE_INSTANCE
bool SciTEGTK::CreatePipe(bool forceNew) {

	bool anotherPipe = false;

	fdPipe = -1;
	inputWatcher = -1;

	//printf("In CreatePipe\n");

	//possible bug here (eventually), can't have more than a 1000 SciTE's open - ajkc 20001112
	for (int i=0; i<1000; i++) {

		//create the pipe name - we use a number as well just incase multiple people have pipes open
		//or we are forceing a new instance of scite (even if there is already one)
		sprintf(pipeName, "/tmp/.SciTE.%d.ipc", i);

		//printf("Trying pipe %s\n", pipeName);
		//check to see if there is already one
		fdPipe = open(pipeName, O_RDWR | O_NONBLOCK);

		//there is one but it isn't ours
		if (fdPipe == -1 && errno == EACCES) {
			//printf("No access\n");
			continue;
		}
		//there isn't one - so create one
		else if (fdPipe == -1) {
			//printf("Non found - making\n");
			mkfifo(pipeName, 0777);
			fdPipe = open(pipeName, O_RDWR | O_NONBLOCK);
			break;
		}
		//there is so just open it (and we don't want out own)
		else if (forceNew == false) {
			//printf("Another one there - opening\n");

			fdPipe = open(pipeName, O_RDWR | O_NONBLOCK);

			//there is already another pipe so set it to true for the return value
			anotherPipe = true;
			//I don;t think it is a good idea to be able to listen to our own pipes (yet) so just return
			//break;
			return anotherPipe;
		}
		//we must want another one
	}

	if (fdPipe != -1) {
		//store the inputwatcher so we can remove it.
		inputWatcher = gdk_input_add(fdPipe, GDK_INPUT_READ, PipeSignal, this);
		//store the file descriptor of the pipe so we can write to it again.

		return anotherPipe;
	}

	//we must have failed or something so there definately isn't "another pipe"
	return false;
}

//use to send a command through a pipe.  (there is no checking to see whos's pipe it is. Probably not a good
//idea to send commands to  our selves.
bool SciTEGTK::SendPipeCommand(const char *pipeCommand) {
	//check that there is actually a pipe
	int size = 0;

	if (fdPipe != -1) {
		size = write(fdPipe, pipeCommand, strlen(pipeCommand) + 1);
		//printf("dd: Send pipecommand: %s %d bytes\n", pipeCommand, size);
		if (size != -1)
			return true;
	}
	return false;
}

//signal handler for gdk_input_add for the pipe listener.
void SciTEGTK::PipeSignal(SciTEGTK *scitew, gint fd, GdkInputCondition condition) {
	int readLength;
	char pipeData[8192];
	PropSetFile pipeProps;

	//printf("Pipe read signal\n");

	if (condition == GDK_INPUT_READ) {
		//use a propset so we don't have to fuss (it's already done!)
		while ((readLength = read(fd, pipeData, sizeof(pipeData))) > 0) {
			//printf("Read: >%s< from pipedata\n", pipeData);
			//fill the propset with the data from the pipe
			pipeProps.ReadFromMemory(pipeData, readLength);

			//get filename from open command
			SString fileName = pipeProps.Get("open");

			//printf("filename from pipe: %s\n", fileName.c_str());

			//is filename zero length if propset.get fails?
			//if there is a file name, open it.
			if (fileName.size() > 0) {
				//printf("opening >%s< from pipecommand\n", fileName.c_str());
				scitew->Open(fileName.c_str());

				//grab the focus back to us.  (may not work) - any ideas?
				gtk_widget_grab_focus(GTK_WIDGET(scitew->GetID()));
			}
			//add other commands here
		}
	}
}

void SciTEGTK::CheckAlreadyOpen(const char *cmdLine) {
	// Create a pipe and see if it finds another one already there

	// If we are not given a command line filename, assume that we just want to load ourselves.
	if ((strlen(cmdLine) > 0) && (CreatePipe() == true)) {
		char currentPath[MAX_PATH];
		getcwd(currentPath, MAX_PATH);

		//create the command to send thru the pipe
		char pipeCommand[CHAR_MAX];

		//check to see if path is already absolute
		if (cmdLine[0] == '/')
			sprintf(pipeCommand, "open=%s", cmdLine);
		//if it isn't then add the absolute path to the from of the command to send.
		else
			sprintf(pipeCommand, "open=%s/%s", currentPath, cmdLine);

		//printf("Sending %s through pipe\n", pipeCommand);
		//send it
		bool piperet = SendPipeCommand(pipeCommand);
		//printf("Sent.\n");

		//if it was OK then quit (we should really get an anwser back just in case
		if (piperet == true) {
			//printf("Sent OK -> Quitting\n");
			gtk_exit(0);
		}
	}
	//create our own pipe.
	else if ((fdPipe == -1) && (inputWatcher == -1)) {
		CreatePipe(true);
	}
}

#endif 

void SciTEGTK::Run(int argc, char *argv[]) {
	SString files;
	int fileCount = 0;
	SString switches;
	for (int arg=1; arg < argc; arg++) {
		if (argv[arg][0] == '-') {
			if (switches.length())
				switches += "\n";
			switches += (argv[arg] + 1);
		} else {
			fileCount++;
			files += argv[arg];
			files += "\n";
		}
	}
	files.substitute('\n', '\0');	// Make into a set of strings
	props.ReadFromMemory(switches.c_str(), switches.length(), "");

	wSciTE = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//GTK_WIDGET_UNSET_FLAGS(wSciTE.GetID(), GTK_CAN_FOCUS);
	gtk_window_set_policy(GTK_WINDOW(wSciTE.GetID()), TRUE, TRUE, FALSE);

	char *gthis = reinterpret_cast<char *>(this);

#ifdef SINGLE_INSTANCE
	if (props.GetInt("check.if.already.open")) {
		if (fileCount == 1) 
			CheckAlreadyOpen(files.c_str());
	}
#endif 

	gtk_widget_set_events(wSciTE.GetID(),
	                      GDK_EXPOSURE_MASK
	                      | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                     );
	gtk_signal_connect(GTK_OBJECT(wSciTE.GetID()), "delete_event",
	                   GTK_SIGNAL_FUNC(QuitSignal), gthis);

	gtk_signal_connect(GTK_OBJECT(wSciTE.GetID()), "key_press_event",
			GtkSignalFunc(KeyPress), gthis);
	
	gtk_window_set_title(GTK_WINDOW(wSciTE.GetID()), appName);
	int left = props.GetInt("position.left", 10);
	int top = props.GetInt("position.top", 30);
	int width = props.GetInt("position.width", 300);
	int height = props.GetInt("position.height", 400);
	if (width == -1 || height == -1) {
		width = gdk_screen_width() - left - 10;
		height = gdk_screen_height() - top - 30;
	}
	gtk_widget_set_usize(GTK_WIDGET(wSciTE.GetID()), width, height);

	fileSelectorWidth = props.GetInt("fileselector.width", fileSelectorWidth);
	fileSelectorHeight = props.GetInt("fileselector.height", fileSelectorHeight);
	
	GtkWidget *boxMain = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(wSciTE.GetID()), boxMain);
	GTK_WIDGET_UNSET_FLAGS(boxMain, GTK_CAN_FOCUS);

	CreateMenu();

	GtkWidget *handle_box = gtk_handle_box_new();

	gtk_container_add(GTK_CONTAINER(handle_box),
	                  gtk_item_factory_get_widget(itemFactory, "<main>"));

	gtk_box_pack_start(GTK_BOX(boxMain),
	                   handle_box,
	                   FALSE, FALSE, 0);

	gtk_widget_set_uposition(GTK_WIDGET(wSciTE.GetID()), left, top);
	gtk_widget_show(wSciTE.GetID());

	wToolBarBox = gtk_handle_box_new();

	wToolBar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	//wToolBar.Show();
	tbVisible = false;

	gtk_container_add(GTK_CONTAINER(wToolBarBox.GetID()), wToolBar.GetID());

	gtk_box_pack_start(GTK_BOX(boxMain),
	                   wToolBarBox.GetID(),
	                   FALSE, FALSE, 0);

	wContent = gtk_fixed_new();
	GTK_WIDGET_UNSET_FLAGS(wContent.GetID(), GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(boxMain), wContent.GetID(), TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(wContent.GetID()), "size_allocate",
	                   GTK_SIGNAL_FUNC(MoveResize), gthis);

#ifdef CLIENT_3D_EFFECT
	topFrame = gtk_frame_new(NULL);
	gtk_widget_show(topFrame.GetID());
	gtk_frame_set_shadow_type(GTK_FRAME(topFrame.GetID()), GTK_SHADOW_IN);
	gtk_fixed_put(GTK_FIXED(wContent.GetID()), topFrame.GetID(), 0, 0);
	gtk_widget_set_usize(topFrame.GetID(), 600, 600);
#endif

	wEditor = scintilla_new();
	scintilla_set_id(SCINTILLA(wEditor.GetID()), IDM_SRCWIN);
	fnEditor = reinterpret_cast<SciFnDirect>(Platform::SendScintilla(
	               wEditor.GetID(), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrEditor = Platform::SendScintilla(wEditor.GetID(),
	                                    SCI_GETDIRECTPOINTER, 0, 0);
#ifdef CLIENT_3D_EFFECT
	gtk_container_add(GTK_CONTAINER(topFrame.GetID()), wEditor.GetID());
#else
	gtk_fixed_put(GTK_FIXED(wContent.GetID()), wEditor.GetID(), 0, 0);
	gtk_widget_set_usize(wEditor.GetID(), 600, 600);
#endif
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
	gtk_fixed_put(GTK_FIXED(wContent.GetID()), wDivider.GetID(), 0, 600);

#ifdef CLIENT_3D_EFFECT
	outputFrame = gtk_frame_new(NULL);
	gtk_widget_show(outputFrame.GetID());
	gtk_frame_set_shadow_type (GTK_FRAME(outputFrame.GetID()), GTK_SHADOW_IN);
	gtk_fixed_put(GTK_FIXED(wContent.GetID()), outputFrame.GetID(), 0, width);
	gtk_widget_set_usize(outputFrame.GetID(), width, 100);
#endif

	wOutput = scintilla_new();
	scintilla_set_id(SCINTILLA(wOutput.GetID()), IDM_RUNWIN);
	fnOutput = reinterpret_cast<SciFnDirect>(Platform::SendScintilla(
	               wOutput.GetID(), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrOutput = Platform::SendScintilla(wOutput.GetID(),
	                                    SCI_GETDIRECTPOINTER, 0, 0);
#ifdef CLIENT_3D_EFFECT
	gtk_container_add(GTK_CONTAINER(outputFrame.GetID()), wOutput.GetID());
#else
	gtk_fixed_put(GTK_FIXED(wContent.GetID()), wOutput.GetID(), 0, width);
	gtk_widget_set_usize(wOutput.GetID(), width, 100);
#endif
	gtk_signal_connect(GTK_OBJECT(wOutput.GetID()), "command",
	                   GtkSignalFunc(CommandSignal), this);
	gtk_signal_connect(GTK_OBJECT(wOutput.GetID()), "notify",
	                   GtkSignalFunc(NotifySignal), this);

	SendOutput(SCI_SETMARGINWIDTHN, 1, 0);
	//SendOutput(SCI_SETCARETPERIOD, 0);

	gtk_widget_set_uposition(GTK_WIDGET(wSciTE.GetID()), left, top);
	gtk_widget_show_all(wSciTE.GetID());

	gtk_widget_hide(GTK_WIDGET(wToolBarBox.GetID()));

	gtk_container_set_border_width(GTK_CONTAINER(wToolBar.GetID()), 2);
	gtk_toolbar_set_space_size(GTK_TOOLBAR(wToolBar.GetID()), 17);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(wToolBar.GetID()), GTK_TOOLBAR_SPACE_LINE);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(wToolBar.GetID()), GTK_RELIEF_NONE);

	AddToolButton("New", IDM_NEW, filenew_xpm);
	AddToolButton("Open", IDM_OPEN, fileopen_xpm);
	AddToolButton("Save", IDM_SAVE, filesave_xpm);
	AddToolButton("Close", IDM_CLOSE, close_xpm);

	gtk_toolbar_append_space(GTK_TOOLBAR(wToolBar.GetID()));
	AddToolButton("Undo", IDM_UNDO, undo_xpm);
	AddToolButton("Redo", IDM_REDO, redo_xpm);
	AddToolButton("Cut", IDM_CUT, editcut_xpm);
	AddToolButton("Copy", IDM_COPY, editcopy_xpm);
	AddToolButton("Paste", IDM_PASTE, editpaste_xpm);

	gtk_toolbar_append_space(GTK_TOOLBAR(wToolBar.GetID()));
	AddToolButton("Find in Files", IDM_FINDINFILES, findinfiles_xpm);
	AddToolButton("Find", IDM_FIND, search_xpm);
	AddToolButton("Find Next", IDM_FINDNEXT, findnext_xpm);
	AddToolButton("Replace", IDM_REPLACE, replace_xpm);

	gtk_toolbar_append_space(GTK_TOOLBAR(wToolBar.GetID()));
	compile_btn = AddToolButton("Compile", IDM_COMPILE, compile_xpm);
	build_btn = AddToolButton("Build", IDM_BUILD, build_xpm);
	stop_btn = AddToolButton("Stop", IDM_STOPEXECUTE, stop_xpm);

	gtk_toolbar_append_space(GTK_TOOLBAR(wToolBar.GetID()));
	AddToolButton("Previous Buffer", IDM_PREVFILE, prev_xpm);
	AddToolButton("Next Buffer", IDM_NEXTFILE, next_xpm);

	wStatusBar = gtk_statusbar_new();
	sbContextID = gtk_statusbar_get_context_id(
	                  GTK_STATUSBAR(wStatusBar.GetID()), "global");
	gtk_box_pack_start(GTK_BOX(boxMain), wStatusBar.GetID(), FALSE, FALSE, 0);
	gtk_statusbar_push(GTK_STATUSBAR(wStatusBar.GetID()), sbContextID, "Initial");
	sbVisible = false;

	SetFocus(wOutput.GetID());

	OpenMultiple(files.c_str(), true);
	CheckMenus();
	SizeSubWindows();
	SetFocus(wEditor.GetID());
	SetIcon();

	gtk_main();
}

int main(int argc, char *argv[]) {
	//assert(argc==2);
#ifdef LUA_SCRIPTING
	LuaExtension luaExtender;
	Extension *extender = &luaExtender;
#else
	Extension *extender = 0;
#endif 

#ifdef __vms
	// Store the path part of the module name
	strcpy(g_modulePath, argv[0]);
	char *p = strstr(g_modulePath, "][");
	if (p != NULL) {
		strcpy (p, p+ 2);
	}
	p = strchr(g_modulePath, ']');
	if (p == NULL) {
		p = strchr(g_modulePath, '>');
	}
	if (p == NULL) {
		p = strchr(g_modulePath, ':');
	}
	if (p != NULL) {
		*(p + 1) = '\0';
	}
	strcpy(g_modulePath, VMSToUnixStyle(g_modulePath));
	g_modulePath[strlen(g_modulePath) - 1] = '\0';  // remove trailing "/"
#endif
	
	gtk_init(&argc, &argv);
	SciTEGTK scite(extender);
	scite.Run(argc, argv);

	return 0;
}
