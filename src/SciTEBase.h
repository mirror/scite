// SciTE - Scintilla based Text Editor
// SciTEBase.h - definition of platform independent base class of editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

extern const char appName[];

#ifdef unix
const char pathSepString[] = "/";
const char pathSepChar = '/';
const char propUserFileName[] = ".SciTEUser.properties";
#define MAX_PATH 260
#else
const char pathSepString[] = "\\";
const char pathSepChar = '\\';
const char propUserFileName[] = "SciTEUser.properties";
#ifdef _MSC_VER
// Shut up level 4 warning:  warning C4710: function 'void whatever(...)' not inlined
#pragma warning(disable:4710)
#endif
#endif

const char propGlobalFileName[] = "SciTEGlobal.properties";

// This is a fixed length list of strings suitable for display  in combo boxes
// as a memory of user entries
template<int sz>
class EntryMemory {
	SString entries[sz];
public:
	void Insert(SString s) {
		for (int i=0;i<sz;i++) {
			if (entries[i] == s) {
				for (int j=i;j>0;j--) {
					entries[j] = entries[j-1];
				}
				entries[0] = s;
				return;
			}
		}
		for (int k=sz-1;k>0;k--) {
			entries[k] = entries[k-1];
		}
		entries[0] = s;
	}
	int Length() const {
		int len = 0;
		for (int i=0;i<sz;i++)
			if (entries[i].length())
				len++;
		return len;
	}
	SString At(int n) const {
		return entries[n];
	}
};

class RecentFile {
public:
	SString fileName;
	CharacterRange selection;
	int scrollPosition;
	RecentFile() {
		fileName = "";
		selection.cpMin = INVALID_POSITION;
		selection.cpMax = INVALID_POSITION;
		scrollPosition = 0;
	}
	void Init() {
		fileName = "";
		selection.cpMin = INVALID_POSITION;
		selection.cpMax = INVALID_POSITION;
		scrollPosition = 0;
	}
};

class Buffer : public RecentFile {
public:
	int doc;
	bool isDirty;
	SString overrideExtension;
	Buffer() : RecentFile(), doc(0), isDirty(false) { 
	}
	void Init() {
		RecentFile::Init();
		isDirty = false;
		overrideExtension = "";
	}
};

class BufferList {
public:
	Buffer *buffers;
	int size;
	int length;
	int current;
	BufferList();
	~BufferList();
	void Allocate(int maxSize);
	int Add();
	int GetDocumentByName(const char *filename);
	void RemoveCurrent();
};

enum JobSubsystem { jobCLI=0, jobGUI=1, jobShell=2, jobExtension=3};
class Job {
public:
	SString command;
	SString directory;
	JobSubsystem jobType;

	Job();
	void Clear();
};

typedef EntryMemory<10> ComboMemory;

enum { 
	heightTools = 24, 
	heightStatus = 20, 
	statusPosWidth = 256
};

struct StyleAndWords {
	int styleNumber;
	SString words;
	bool IsEmpty() { return words.length() == 0; }
};

class SciTEBase : public ExtensionAPI {
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
	enum { fileStackCmdID = IDM_MRUFILE, bufferCmdID = IDM_BUFFER };

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
	int characterSet;
	SString language;
	int lexLanguage;
	SString overrideExtension;	// User has chosen to use a particular language
	enum {numWordLists=5};
	WordList apis;
	SString functionDefinition;

	int indentSize;
	bool indentOpening;
	bool indentClosing;
	int statementLookback;
	StyleAndWords statementIndent;
	StyleAndWords statementEnd;
	StyleAndWords blockStart;
	StyleAndWords blockEnd;

	Window wSciTE;  // Contains wToolBar, wContent, and wStatusBar
	Window wContent;    // Contains wEditor and wOutput
	Window wEditor;
	Window wOutput;
	Window wDivider;	// Not used on Windows
	Window wToolBar;
	Window wStatusBar;
	SciFnDirect fnEditor;
	long ptrEditor;
	SciFnDirect fnOutput;
	long ptrOutput;
	bool tbVisible;
	bool sbVisible;
	int visHeightTools;
	int visHeightStatus;
	int visHeightEditor;
	int heightBar;
	int dialogsOnScreen;
	enum { toolMax = 10 };
	Extension *extender;

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
	SString sbValue;

	bool indentationWSVisible;

	bool autoCompleteIgnoreCase;
	bool callTipIgnoreCase;
	
	bool margin;
	int marginWidth;
	enum { marginWidthDefault = 20};

	bool foldMargin;
	int foldMarginWidth;
	enum { foldMarginWidthDefault = 14};

	bool lineNumbers;
	int lineNumbersWidth;
	enum { lineNumbersWidthDefault = 40};

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

	enum { bufferMax = 10 };
	BufferList buffers;

	// Handle buffers
	int GetDocumentAt(int index);
	int AddBuffer();
	void UpdateBuffersCurrent();
	bool IsBufferAvailable();
	void SetDocumentAt(int index);
	void BuffersMenu();
	void Next();
	void Prev();

	void ReadGlobalPropFile();
	void GetDocumentDirectory(char *docDir, int len);
	void ReadLocalPropFile();

	long SendEditor(unsigned int msg, unsigned long wParam=0, long lParam=0);
	long SendEditorString(unsigned int msg, unsigned long wParam, const char *s);
	long SendOutput(unsigned int msg, unsigned long wParam= 0, long lParam = 0);
	long SendFocused(unsigned int msg, unsigned long wParam= 0, long lParam = 0);
	void SendChildren(unsigned int msg, unsigned long wParam= 0, long lParam = 0);
	long SendOutputEx(unsigned int msg, unsigned long wParam= 0, long lParam = 0, bool direct = true);
	int LengthDocument();
	int GetLine(char *text, int sizeText, int line=-1);
	void GetRange(Window &win, int start, int end, char *text);
	bool FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy);
	void BraceMatch(bool editor);

	void SetWindowName();
	void SetFileName(const char *openName);
	void ClearDocument();
	void InitialiseBuffers();
	void New();
	void Close(bool updateUI= true);
	bool Exists(const char *dir, const char *path, char *testPath);
	virtual void AbsolutePath(char *fullPath, const char *basePath, int size)=0;
	virtual void FixFilePath();
	virtual bool OpenDialog()=0;
	virtual bool SaveAsDialog()=0;
	void Open(const char *file = 0, bool initialCmdLine = false);
	void Revert();
	int SaveIfUnsure(bool forceQuestion = false);
	int SaveIfUnsureAll(bool forceQuestion = false);
	int SaveIfUnsureForBuilt();
	bool Save();
	virtual bool SaveAs(char *file = 0);
	void SaveToHTML(const char *saveName);
	virtual void SaveAsHTML()=0;
	void SaveToRTF(const char *saveName);
	virtual void SaveAsRTF()=0;
	virtual void GetDefaultDirectory(char *directory, size_t size)=0;
	virtual bool GetSciteDefaultHome(char *path, unsigned int lenPath)=0;
	virtual bool GetSciteUserHome(char *path, unsigned int lenPath)=0;
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps, 
		char *pathDefaultDir, unsigned int lenPath)=0;
	virtual bool GetUserPropertiesFileName(char *pathUserProps, 
		char *pathUserDir, unsigned int lenPath)=0;
	void OpenProperties(int propsFile);
	virtual void Print(bool) {};
	virtual void PrintSetup() {};
	CharacterRange GetSelection();
	void SetSelection(int anchor, int currentPos);
	void SelectionIntoProperties();
	void SelectionIntoFind();
	virtual void Find()=0;
	void FindMessageBox(const char *msg);
	void FindNext(bool reverseDirection);
	virtual void FindInFiles()=0;
	virtual void Replace()=0;
	void ReplaceOnce();
	void ReplaceAll();
	virtual void DestroyFindReplace()=0;
	virtual void GoLineDialog()=0;
	virtual void TabSizeDialog()=0;
	void GoMatchingBrace(bool select);
	virtual void FindReplace(bool replace)=0;
	void OutputAppendString(const char *s, int len = -1);
	void OutputAppendStringEx(const char *s, int len = -1, bool direct = true);
	virtual void Execute();
	virtual void StopExecute()=0;
	void GoMessage(int dir);
	virtual bool StartCallTip();
	void ContinueCallTip();
	virtual bool StartAutoComplete();
	virtual bool StartAutoCompleteWord();
	void GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len);
	int SetLineIndentation(int line, int indent);
	int GetLineIndentation(int line);
	int GetLineIndentPosition(int line);
	bool RangeIsAllWhitespace(int start, int end);
	int GetIndentState(int line);
	void AutomaticIndentation(char ch);
	void CharAdded(char ch);
	void UpdateStatusBar();
	int GetCurrentLineNumber();
	int GetCurrentScrollPosition();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue = false);
	virtual void AboutDialog()=0;
	virtual void QuitProgram()=0;
	void CloseAllBuffers();
	void MenuCommand(int cmdID);
	void FoldChanged(int line, int levelNow, int levelPrev);
	void FoldChanged(int position);
	void Expand(int &line, bool doExpand, bool force=false, 
		int visLevels=0, int level=-1);
	void FoldAll();
	void EnsureRangeVisible(int posStart, int posEnd);
	bool MarginClick(int position, int modifiers);
	virtual void SetStatusBarText(const char *s)=0;
	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar()=0;
	virtual void ShowStatusBar()=0;

	void BookmarkToggle( int lineno = -1 );
	void BookmarkNext();
	virtual void SizeContentWindows()=0;
	virtual void SizeSubWindows()=0;

	virtual void SetMenuItem(int menuNumber, int position, int itemID, 
		const char *text, const char *mnemonic=0)=0;
	virtual void DestroyMenuItem(int menuNumber, int itemID)=0;
	virtual void CheckAMenuItem(int wIDCheckItem, bool val)=0;
	virtual void EnableAMenuItem(int wIDCheckItem, bool val)=0;
	virtual void CheckMenus();

	void DeleteFileStackMenu();
	void SetFileStackMenu();
	void DropFileStackTop();
	void AddFileToStack(const char *file, CharacterRange selection, int scrollPos);
	void RemoveFileFromStack(const char *file);
	void DisplayAround(const RecentFile &rf);
	void StackMenu(int pos);
	void StackMenuNext();
	void StackMenuPrev();

	void RemoveToolsMenu();
	void SetToolsMenu();
	JobSubsystem SubsystemType(const char *cmd, int item = -1);
	void ToolsMenu(int item);

	void AssignKey(int key, int mods, int cmd);
	void ViewWhitespace(bool view);
	virtual void ReadPropertiesInitial();
	void SetOverrideLanguage(int cmdID);
	StyleAndWords GetStyleAndWords(const char *base);
	SString ExtensionFileName();
	virtual void ReadProperties();
	void SetOneStyle(Window &win, int style, const char *s);
	void SetStyleFor(Window &win, const char *language);

	void CheckReload();
	void Activate(bool activeApp);
	PRectangle GetClientRectangle();
	void Redraw();
	int NormaliseSplit(int splitPos);
	void MoveSplit(Point ptNewDrag);

	// ExtensionAPI
	int Send(Pane p, unsigned int msg, unsigned long wParam=0, long lParam=0);
	char *Range(Pane p, int start, int end);
	void Remove(Pane p, int start, int end);
	void Insert(Pane p, int pos, const char *s);
	void Trace(const char *s);
	char *Property(const char *key);

public:

	SciTEBase(Extension *ext=0);
	virtual ~SciTEBase();

	void ProcessExecute();
	WindowID GetID() { return wSciTE.GetID(); }
};

const char *strcasestr(const char *str, const char *pattn);
int ControlIDOfCommand(unsigned long);
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
