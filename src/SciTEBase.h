// SciTE - Scintilla based Text Editor
// SciTEBase.h - definition of platform independent base class of editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

extern const char appName[];

#ifdef unix
const char pathSepString[] = "/";
const char pathSepChar = '/';
#define MAX_PATH 260
#else
const char pathSepString[] = "\\";
const char pathSepChar = '\\';
#endif

const char propGlobalFileName[] = "SciTEGlobal.properties";
const char propUserFileName[] = "SciTEUser.properties";
const char propFileName[] = "SciTE.properties";

class RecentFile {
public:
	char fileName[MAX_PATH];
	int lineNumber;
	int scrollPosition;
	RecentFile() {
		fileName[0] = '\0';
		lineNumber = -1;
		scrollPosition = 0;
	}
};

enum JobSubsystem { jobCLI = 0, jobGUI = 1, jobShell = 2};
class Job {
public:
	SString command;
	SString directory;
	JobSubsystem jobType;

	Job();
	void Clear();
};

typedef EntryMemory<10> ComboMemory;

class SciTEBase {
protected:
	char windowName[MAX_PATH + 20];
	char fullPath[MAX_PATH];
	char fileName[MAX_PATH];
	char fileExt[MAX_PATH];
	char dirName[MAX_PATH];
	char dirNameAtExecute[MAX_PATH];
	time_t fileModTime;

	enum { fileStackMax = 10 };
	RecentFile recentFileStack[fileStackMax];
	enum { fileStackCmdID = IDM_MRUFILE };

	char findWhat[200];
	char replaceWhat[200];
	Window wFindReplace;
	bool replacing;
	bool havefound;
	bool matchCase;
	bool wholeWord;
	bool reverseFind;
	ComboMemory memFinds;
	ComboMemory memReplaces;
	ComboMemory memFiles;

	int codePage;
	SString language;
	int lexLanguage;
	enum {numWordLists=5};
	WordList apis;
	SString functionDefinition;

	Window wSciTE;
	Window wEditor;
	Window wOutput;
#if PLAT_GTK
	Window wDivider;
#endif
	int dialogsOnScreen;
	enum { toolMax = 10 };

	int heightOutput;
	int heightOutputStartDrag;
	Point ptStartDrag;
	bool capturedMouse;
	bool firstPropertiesRead;
	bool splitVertical;
	bool bufferedDraw;
	bool bracesCheck;
	bool bracesSloppy;
	int bracesStyle;
	int braceCount;

	bool margin;
	int marginWidth;
	enum { marginWidthDefault = 20};

	bool foldMargin;
	int foldMarginWidth;
	enum { foldMarginWidthDefault = 14};
	
	bool lineNumbers;
	int lineNumbersWidth;
	enum { lineNumbersWidthDefault = 40};

	// On GTK+, heightBar should be 12 to be consistent with other apps but this is too big
	// for my tastes.
	enum { heightBar = 7};

	bool usePalette;
	bool clearBeforeExecute;
	bool allowMenuActions;
	bool isDirty;
	bool isBuilding;
	bool isBuilt;
	bool executing;
	enum { commandMax = 2 };
	int commandCurrent;
	Job jobQueue[commandMax];
	long cancelFlag;

	PropSet propsEmbed;
	PropSet propsBase;
	PropSet propsUser;
	PropSet props;

	void ReadGlobalPropFile();
	void ReadLocalPropFile();
	
	LRESULT SendEditor(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0);
	LRESULT SendEditorString(UINT msg, WPARAM wParam, const char *s);
	LRESULT SendOutput(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0);
	LRESULT SendFocused(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0);
	void SendChildren(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0);
	int LengthDocument();
	int GetLine(char *text, int sizeText, int line=-1);
	void GetRange(Window &win, int start, int end, char *text);
	void Colourise(int start = 0, int end = -1, bool editor = true);
	void FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite);
	void BraceMatch(bool editor);

	void SetWindowName();
	void SetFileName(const char *openName);
	void New();
	bool Exists(const char *dir, const char *path, char *testPath);
	virtual void AbsolutePath(char *fullPath, const char *basePath, int size)=0;
	virtual void FixFilePath();
	virtual bool OpenDialog()=0;
	virtual bool SaveAsDialog()=0;
	void Open(const char *file = 0, bool initialCmdLine = false);
	int SaveIfUnsure(bool forceQuestion = false);
	int SaveIfUnsureForBuilt();
	bool Save();
	virtual bool SaveAs(char *file = 0);
	void SaveToHTML(const char *saveName);
	virtual void SaveAsHTML()=0;
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps, unsigned int lenPath)=0;
	virtual bool GetUserPropertiesFileName(char *pathDefaultProps, unsigned int lenPath)=0;
	void OpenProperties(int propsFile);
	virtual void Print() {};
	virtual void PrintSetup() {};
	void SetSelection(int anchor, int currentPos);
	void SelectionIntoProperties();
	void SelectionIntoFind();
	virtual void Find()=0;
	void FindNext();
	virtual void FindInFiles()=0;
	virtual void Replace()=0;
	void ReplaceOnce();
	void ReplaceAll();
	virtual void DestroyFindReplace()=0;
	virtual void GoLineDialog()=0;
	void GoMatchingBrace();
	virtual void FindReplace(bool replace)=0;
	void OutputAppendString(const char *s, int len = -1);
	virtual void Execute();
	virtual void StopExecute()=0;
	void GoMessage(int dir);
	void StartCallTip();
	void ContinueCallTip();
	void StartAutoComplete();
	void CharAdded(char ch);
	int GetCurrentLineNumber();
	int GetCurrentScrollPosition();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue = false);
	virtual void AboutDialog()=0;
	virtual void QuitProgram()=0;
	void MenuCommand(int cmdID);
	void FoldChanged(int line, int levelNow, int levelPrev);
	void FoldChanged(int position);
	void Expand(int &line, bool doExpand, bool force=false, 
		int visLevels=0, int level=-1);
	void FoldAll();
	void EnsureRangeVisible(int posStart, int posEnd);
	bool MarginClick(int position, int modifiers);
	virtual void Notify(SCNotification *notification);

	void BookmarkToggle( int lineno = -1 );
	void BookmarkNext();
	void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID, 
		const char *text, const char *mnemonic=0)=0;
	virtual void DestroyMenuItem(int menuNumber, int itemID)=0;
	virtual void CheckAMenuItem(int wIDCheckItem, bool val)=0;
	virtual void EnableAMenuItem(int wIDCheckItem, bool val)=0;
	virtual void CheckMenus();
		
	void DeleteFileStackMenu();
	void SetFileStackMenu();
	void DropFileStackTop();
	void AddFileToStack(const char *file, int line = -1, int scrollPos=0);
	void RememberLineNumberStack(const char *file, int line = -1, int scrollPos=0);
	void DisplayAround(int scrollPosition, int lineNumber);
	void StackMenu(int pos);
	void StackMenuNext();
	void StackMenuPrev();

	void RemoveToolsMenu();
	void SetToolsMenu();
	JobSubsystem SubsystemType(const char *cmd, int item = -1);
	void ToolsMenu(int item);

	virtual void ReadPropertiesInitial();
	virtual void ReadProperties();
	void SetOneStyle(Window &win, int style, const char *s);
	void SetStyleFor(Window &win, const char *language);

	void CheckReload();
	void Activate(bool activeApp);
	virtual PRectangle GetClientRectangle()=0;
	void Redraw();
	int NormaliseSplit(int splitPos);
	void MoveSplit(Point ptNewDrag);

public:

	SciTEBase();
	virtual ~SciTEBase();

	void ProcessExecute();
	WindowID GetID() { return wSciTE.GetID(); }
};

int ControlIDOfCommand(WPARAM wParam);
void SetAboutMessage(WindowID wsci, const char *appTitle);
time_t GetModTime(const char *fullPath);
const int blockSize = 131072;

#if PLAT_GTK
// MessageBox
#define MB_OK	(0L)
#define MB_YESNO	(0x4L)
#define MB_YESNOCANCEL	(0x3L)
#define MB_ICONWARNING	(0x30L)
#define IDOK	(1)
#define IDCANCEL	(2)
#define IDYES	(6)
#define IDNO	(7)
int MessageBox(GtkWidget *wParent, const char *m, const char *t = appName, int style = MB_OK);
void SetFocus(GtkWidget *hwnd);
#endif
