// SciTE - Scintilla based Text Editor
/** @file SciTEBase.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

extern const char appName[];

extern const char pathSepString[];
extern const char pathSepChar;
extern const char configFileVisibilityString[];
extern const char propUserFileName[];
extern const char propGlobalFileName[];
extern const char propAbbrevFileName[];
extern const char fileRead[];
extern const char fileWrite[];

extern const char menuAccessIndicator[];

#ifdef unix
#define MAX_PATH 260
#endif
#ifdef __vms
const char *VMSToUnixStyle(const char *fileName);
#endif
#ifdef WIN32
#ifdef _MSC_VER
// Shut up level 4 warning:  warning C4710: function 'void whatever(...)' not inlined
#pragma warning(disable: 4710)
#endif
#endif

/**
 * The order of menus on Windows - the Buffers menu may not be present
 * and there is a Help menu at the end.
 */
enum { menuFile = 0, menuEdit = 1, menuSearch = 2, menuView = 3,
       menuTools = 4, menuOptions = 5, menuLanguage = 6, menuBuffers = 7,
       menuHelp = 8};

/**
 * This is a fixed length list of strings suitable for display in combo boxes
 * as a memory of user entries.
 */
template < int sz >
class EntryMemory {
	SString entries[sz];
public:
	void Insert(const SString &s) {
		for (int i = 0;i < sz;i++) {
			if (entries[i] == s) {
				for (int j = i;j > 0;j--) {
					entries[j] = entries[j - 1];
				}
				entries[0] = s;
				return;
			}
		}
		for (int k = sz - 1;k > 0;k--) {
			entries[k] = entries[k - 1];
		}
		entries[0] = s;
	}
	void AppendIfNotPresent(const SString &s) {
		for (int i = 0;i < sz;i++) {
			if (entries[i] == s) {
				return;
			}
			if (0 == entries[i].length()) {
				entries[i] = s;
				return;
			}
		}
	}
	void AppendList(const SString &s, char sep='|') {
		int start = 0;
		int end = 0;
		while (s[end] != '\0') {
			while ((s[end] != sep) && (s[end] != '\0'))
				++end;
			AppendIfNotPresent(SString(s.c_str(), start, end));
			start = end + 1;
			end = start;
		}
	}
	int Length() const {
		int len = 0;
		for (int i = 0;i < sz;i++)
			if (entries[i].length())
				len++;
		return len;
	}
	SString At(int n) const {
		return entries[n];
	}
};

class PropSetFile : public PropSet {
public:
	PropSetFile();
	~PropSetFile();
	bool ReadLine(const char *data, bool ifIsTrue, const char *directoryForImports, SString imports[] = 0, int sizeImports = 0);
	void ReadFromMemory(const char *data, int len, const char *directoryForImports, SString imports[] = 0, int sizeImports = 0);
	void Read(const char *filename, const char *directoryForImports, SString imports[] = 0, int sizeImports = 0);
};

class FilePath {
	SString fileName;
public:
	FilePath() : fileName("") {
	}
	void Set(const char *fileName_) {
		fileName = fileName_;
	}
	void Init() {
		fileName = "";
	}
	bool SameNameAs(const char *other) const;
	bool SameNameAs(const FilePath &other) const;
	bool IsSet() const { return fileName.length() > 0; }
	bool IsUntitled() const;
	const char *FullPath() const;
};

class RecentFile : public FilePath {
public:
	CharacterRange selection;
	int scrollPosition;
	RecentFile() {
		selection.cpMin = INVALID_POSITION;
		selection.cpMax = INVALID_POSITION;
		scrollPosition = 0;
	}
	void Init() {
		FilePath::Init();
		selection.cpMin = INVALID_POSITION;
		selection.cpMax = INVALID_POSITION;
		scrollPosition = 0;
	}
};

class Buffer : public RecentFile {
public:
	int doc;
	bool isDirty;
	bool useMonoFont;
	time_t fileModTime;
	SString overrideExtension;
	Buffer() : 
		RecentFile(), doc(0), isDirty(false), useMonoFont(false), fileModTime(0) {
	}

	void Init() {
		RecentFile::Init();
		isDirty = false;
		useMonoFont = false;
		fileModTime = 0;
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

enum JobSubsystem {
    jobCLI = 0, jobGUI = 1, jobShell = 2, jobExtension = 3, jobHelp = 4, jobOtherHelp = 5};
class Job {
public:
	SString command;
	SString directory;
	JobSubsystem jobType;

	Job();
	void Clear();
};

class LanguageMenuItem {
public:
	SString menuItem;
	SString menuKey;
	SString extension;
};

/// Find the character following a name which is made up of character from
/// the set [a-zA-Z.]
char AfterName(const char *s);

typedef EntryMemory < 10 > ComboMemory;

enum {
	heightTools = 24,
	heightTab = 24,
	heightStatus = 20,
	statusPosWidth = 256
};

/// Warning IDs.
enum {
	warnFindWrapped = 1,
	warnNotFound,
	warnNoOtherBookmark,
	warnWrongFile,
	warnExecuteOK,
	warnExecuteKO
};

/// Codes representing the effect a line has on indentation.
enum IndentationStatus {
	isNone,		// no effect on indentation
	isBlockStart,	// indentation block begin such as "{" or VB "function"
	isBlockEnd,	// indentation end indicator such as "}" or VB "end"
	isKeyWordStart	// Keywords that cause indentation
};

class StyleDefinition {
public:
	SString font;
	int size;
	ColourDesired fore;
	ColourDesired back;
	bool bold;
	bool italics;
	bool eolfilled;
	bool underlined;
	int caseForce;
	enum flags { sdNone = 0, sdFont = 0x1, sdSize = 0x2, sdFore = 0x4, sdBack = 0x8,
	             sdBold = 0x10, sdItalics = 0x20, sdEOLFilled = 0x40, sdUnderlined = 0x80,
	              sdCaseForce = 0x100} specified;
	StyleDefinition(const char *definition);
};

struct StyleAndWords {
	int styleNumber;
	SString words;
	bool IsEmpty() { return words.length() == 0; }
	bool IsSingleChar() { return words.length() == 1; }
};

#define SciTE_MARKER_BOOKMARK 1

class SciTEBase : public ExtensionAPI {
protected:
	SString windowName;
	char fullPath[MAX_PATH];
	char fileName[MAX_PATH];
	char fileExt[MAX_PATH];
	char dirName[MAX_PATH];
	SString dirNameAtExecute;
	SString dirNameForExecute;
	bool useMonoFont;
	time_t fileModTime;
	time_t fileModLastAsk;

	enum { fileStackMax = 10 };
	RecentFile recentFileStack[fileStackMax];
	enum { fileStackCmdID = IDM_MRUFILE, bufferCmdID = IDM_BUFFER };

	enum { importMax = 20 };
	SString importFiles[importMax];
	enum { importCmdID = IDM_IMPORT };

	SString findWhat;
	SString replaceWhat;
	Window wFindReplace;
	bool replacing;
	bool havefound;
	bool matchCase;
	bool wholeWord;
	bool reverseFind;
	bool regExp;
	bool wrapFind;
	bool unSlash;
	ComboMemory memFinds;
	ComboMemory memReplaces;
	ComboMemory memFiles;
	ComboMemory memDirectory;
	enum { maxParam = 4 };
	Window wParameters;
	SString parameterisedCommand;

	enum { languageCmdID = IDM_LANGUAGE };
	LanguageMenuItem *languageMenu;
	int languageItems;

	int codePage;
	int characterSet;
	SString language;
	int lexLanguage;
	SString overrideExtension;	///< User has chosen to use a particular language
	enum {numWordLists = 6};
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
	enum { noPPC, ppcStart, ppcMiddle, ppcEnd, ppcDummy };	///< Indicate the kind of preprocessor condition line
	char preprocessorSymbol;	///< Preprocessor symbol (in C: #)
	WordList preprocCondStart;	///< List of preprocessor conditional start keywords (in C: if ifdef ifndef)
	WordList preprocCondMiddle;	///< List of preprocessor conditional middle keywords (in C: else elif)
	WordList preprocCondEnd;	///< List of preprocessor conditional end keywords (in C: endif)

	Window wSciTE;  ///< Contains wToolBar, wTabBar, wContent, and wStatusBar
	Window wContent;    ///< Contains wEditor and wOutput
	Window wEditor;
	Window wOutput;
#if PLAT_GTK
	Window wDivider;	// Not used on Windows
#endif
	Window wToolBar;
	Window wStatusBar;
	Window wTabBar;
	Menu popup;
	SciFnDirect fnEditor;
	long ptrEditor;
	SciFnDirect fnOutput;
	long ptrOutput;
	bool tbVisible;
	bool tabVisible;
	bool tabHideOne; // Hide tab bar if one buffer is opened only
	bool tabMultiLine;
	bool sbVisible;	///< @c true if status bar is visible.
	SString sbValue;	///< Status bar text.
	int sbNum;	///< Number of the currenly displayed status bar information.
	int visHeightTools;
	int visHeightTab;
	int visHeightStatus;
	int visHeightEditor;
	int heightBar;
	int dialogsOnScreen;
	bool topMost;
	bool wrap;
	bool checkIfOpen;
	bool fullScreen;
	enum { toolMax = 10 };
	Extension *extender;

	int heightOutput;
	int heightOutputStartDrag;
	Point ptStartDrag;
	bool capturedMouse;
	int previousHeightOutput;
	bool firstPropertiesRead;
	bool localisationRead;
	bool splitVertical;	///< @c true if the split bar between editor and output is vertical.
	bool bufferedDraw;
	bool bracesCheck;
	bool bracesSloppy;
	int bracesStyle;
	int braceCount;

	bool indentationWSVisible;

	bool autoCompleteIgnoreCase;
	bool callTipIgnoreCase;
	bool autoCCausedByOnlyOne;
	SString calltipWordCharacters;
	SString autoCompleteStartCharacters;
	SString wordCharacters;

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
	bool scrollOutput;
	bool returnOutputToCommand;
	enum { commandMax = 2 };
	int commandCurrent;
	Job jobQueue[commandMax];
	bool jobUsesOutputPane;
	long cancelFlag;
	bool timeCommands;

	bool macrosEnabled;
	SString currentMacro;
	bool recording;

	PropSetFile propsEmbed;
	PropSetFile propsBase;
	PropSetFile propsUser;
	PropSetFile propsLocal;
	PropSetFile props;

	PropSetFile propsAbbrev;
	
	PropSetFile propsUI;

	PropSet propsStatus;

	enum { bufferMax = 10 };
	BufferList buffers;

	// Handle buffers
	int GetDocumentAt(int index);
	int AddBuffer();
	void UpdateBuffersCurrent();
	bool IsBufferAvailable();
	bool CanMakeRoom(bool maySaveIfDirty=true);
	void SetDocumentAt(int index);
	void BuffersMenu();
	void Next();
	void Prev();

	void ReadGlobalPropFile();
	void GetDocumentDirectory(char *docDir, int len);
	void ReadAbbrevPropFile();
	void ReadLocalPropFile();

	sptr_t SendEditor(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	sptr_t SendEditorString(unsigned int msg, uptr_t wParam, const char *s);
	sptr_t SendOutput(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	sptr_t SendFocused(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	sptr_t SendWindow(Window &w, unsigned int msg, uptr_t wParam=0, sptr_t lParam=0);
	void SendChildren(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	sptr_t SendOutputEx(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0, bool direct = true);
	int LengthDocument();
	int GetCaretInLine();
	void GetLine(char *text, int sizeText, int line=-1);
	void GetRange(Window &win, int start, int end, char *text);
	int IsLinePreprocessorCondition(char *line);
	bool FindMatchingPreprocessorCondition(int &curLine, int direction, int condEnd1, int condEnd2);
	bool FindMatchingPreprocCondPosition(bool isForward, int &mppcAtCaret, int &mppcMatch);
	bool FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy);
	void BraceMatch(bool editor);

	virtual void WarnUser(int warnID) = 0;
	void SetWindowName();
	void SetFileName(const char *openName, bool fixCase = true);
	void ClearDocument();
	void InitialiseBuffers();
	void LoadRecentMenu();
	void SaveRecentStack();
	void LoadSession(const char *sessionName);
	void SaveSession(const char *sessionName);
	void New();
	void Close(bool updateUI = true);
	bool IsAbsolutePath(const char *path);
	bool Exists(const char *dir, const char *path, char *testPath);
	void OpenFile(bool initialCmdLine);
	virtual void OpenUriList(const char *) {};
	virtual void AbsolutePath(char *fullPath, const char *basePath, int size) = 0;
	virtual void FixFilePath();
	virtual bool OpenDialog() = 0;
	virtual bool SaveAsDialog() = 0;
	virtual void LoadSessionDialog() { };
	virtual void SaveSessionDialog() { };
	void CountLineEnds(int &linesCR, int &linesLF, int &linesCRLF);
	bool Open(const char *file = 0, bool initialCmdLine = false, 
		bool forceLoad = false, bool maySaveIfDirty=true);
	void OpenMultiple(const char *files = 0, bool initialCmdLine = false, bool forceLoad = false);
	void OpenSelected();
	void Revert();
	int SaveIfUnsure(bool forceQuestion = false);
	int SaveIfUnsureAll(bool forceQuestion = false);
	int SaveIfUnsureForBuilt();
	bool Save();
	virtual bool SaveAs(char *file = 0);
	void SaveToHTML(const char *saveName);
	virtual void SaveAsHTML() = 0;
	void SaveToRTF(const char *saveName, int start = 0, int end = -1);
	virtual void SaveAsRTF() = 0;
	void SaveToPDF(const char *saveName);
	virtual void SaveAsPDF() = 0;
	virtual void GetDefaultDirectory(char *directory, size_t size) = 0;
	virtual bool GetSciteDefaultHome(char *path, unsigned int lenPath) = 0;
	virtual bool GetSciteUserHome(char *path, unsigned int lenPath) = 0;
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps,
	        char *pathDefaultDir, unsigned int lenPath) = 0;
	virtual bool GetUserPropertiesFileName(char *pathUserProps,
	                                       char *pathUserDir, unsigned int lenPath) = 0;
	virtual bool GetAbbrevPropertiesFileName(char *pathAbbrevProps,
	        char *pathDefaultDir, unsigned int lenPath) = 0;
	void OpenProperties(int propsFile);
	virtual void Print(bool) {};
	virtual void PrintSetup() {};
	CharacterRange GetSelection();
	void SetSelection(int anchor, int currentPos);
	//	void SelectionExtend(char *sel, int len, char *notselchar);
	void GetCTag(char *sel, int len);
	void RangeExtendAndGrab(Window &wCurrent, char *sel, int len,
	    int selStart, int selEnd, int lengthDoc, bool (*ischarforsel)(char ch));
	void SelectionExtend(char *sel, int len, bool (*ischarforsel)(char ch));
	SString SelectionWord();
	SString SelectionFilename();
	void SelectionIntoProperties();
	void SelectionIntoFind();
	virtual void Find() = 0;
	virtual int WindowMessageBox(Window &w, const SString &m, int style)=0;
	void FindMessageBox(const SString &msg);
	void FindNext(bool reverseDirection, bool showWarnings = true);
	virtual void FindInFiles() = 0;
	virtual void Replace() = 0;
	void ReplaceOnce();
	void ReplaceAll(bool inSelection);
	virtual void DestroyFindReplace() = 0;
	virtual void GoLineDialog() = 0;
	virtual void TabSizeDialog() = 0;
	virtual void ParamGrab() = 0;
	virtual bool ParametersDialog(bool modal) = 0;
	void GoMatchingBrace(bool select);
	void GoMatchingPreprocCond(int direction, bool select);
	virtual void FindReplace(bool replace) = 0;
	void OutputAppendString(const char *s, int len = -1);
	void OutputAppendStringSynchronised(const char *s, int len = -1);
	void MakeOutputVisible();
	void ClearJobQueue();
	virtual void Execute();
	virtual void StopExecute() = 0;
	void GoMessage(int dir);
	virtual bool StartCallTip();
	void ContinueCallTip();
	virtual bool StartAutoComplete();
	virtual bool StartAutoCompleteWord(bool onlyOneWord);
	virtual bool StartExpandAbbreviation();
	virtual bool StartBlockComment();
	virtual bool StartBoxComment();
	virtual bool StartStreamComment();
	unsigned int GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len);
	void SetLineIndentation(int line, int indent);
	int GetLineIndentation(int line);
	int GetLineIndentPosition(int line);
	bool RangeIsAllWhitespace(int start, int end);
	IndentationStatus GetIndentState(int line);
	int IndentOfBlock(int line);
	void AutomaticIndentation(char ch);
	void CharAdded(char ch);
	void SetTextProperties(PropSet &ps);
	void SetFileProperties(PropSet &ps);
	virtual void UpdateStatusBar(bool bUpdateSlowData);
	int GetCurrentLineNumber();
	int GetCurrentScrollPosition();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue = false);
	virtual void AboutDialog() = 0;
	virtual void QuitProgram() = 0;
	void CloseAllBuffers();
	virtual void CopyAsRTF() {};
	void MenuCommand(int cmdID);
	void FoldChanged(int line, int levelNow, int levelPrev);
	void FoldChanged(int position);
	void Expand(int &line, bool doExpand, bool force = false,
	            int visLevels = 0, int level = -1);
	void FoldAll();
	void EnsureRangeVisible(int posStart, int posEnd, bool enforcePolicy=true);
	void GotoLineEnsureVisible(int line);
	bool MarginClick(int position, int modifiers);
	void NewLineInOutput();
	virtual void SetStatusBarText(const char *s) = 0;
	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar() = 0;
	virtual void ShowTabBar() = 0;
	virtual void ShowStatusBar() = 0;

	void BookmarkToggle( int lineno = -1 );
	void BookmarkNext();
	void BookmarkPrev();
	void ToggleOutputVisible();
	virtual void SizeContentWindows() = 0;
	virtual void SizeSubWindows() = 0;

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const char *text, const char *mnemonic = 0) = 0;
	virtual void DestroyMenuItem(int menuNumber, int itemID) = 0;
	virtual void CheckAMenuItem(int wIDCheckItem, bool val) = 0;
	virtual void EnableAMenuItem(int wIDCheckItem, bool val) = 0;
	virtual void CheckMenus();
	virtual void AddToPopUp(const char *label, int cmd=0, bool enabled=true)=0;
	void ContextMenu(Window wSource, Point pt, Window wCmd);

	void DeleteFileStackMenu();
	void SetFileStackMenu();
	void DropFileStackTop();
	void AddFileToBuffer(const char *file /*TODO:, CharacterRange selection, int scrollPos */);
	void AddFileToStack(const char *file, CharacterRange selection, int scrollPos);
	void RemoveFileFromStack(const char *file);
	RecentFile GetFilePosition();
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
	void SetAboutMessage(WindowID wsci, const char *appTitle);
	void SetImportMenu();
	void ImportMenu(int pos);
	void SetLanguageMenu();
	void SetPropertiesInitial();
	SString LocaliseString(const char *s, bool retainIfNotFound=true);
	SString LocaliseMessage(const char *s, const char *param0=0, const char *param1=0);
	void ReadLocalisation();
	virtual void ReadPropertiesInitial();
	void SetMonoFont();
	void SetOverrideLanguage(int cmdID);
	StyleAndWords GetStyleAndWords(const char *base);
	SString ExtensionFileName();
	const char *GetNextPropItem(const char *pStart, char *pPropItem, int maxLen);
	void ForwardPropertyToEditor(const char *key);
	void DefineMarker(int marker, int markerType, ColourDesired fore, ColourDesired back);
	void ReadAPI(const SString &fileNameForExtension);
	virtual void ReadProperties();
	void SetOneStyle(Window &win, int style, const char *s);
	void SetStyleFor(Window &win, const char *language);

	void CheckReload();
	void Activate(bool activeApp);
	PRectangle GetClientRectangle();
	void Redraw();
	int NormaliseSplit(int splitPos);
	void MoveSplit(Point ptNewDrag);

	void UIAvailable();
	void PerformOne(char *action);
	void StartRecordMacro();
	void StopRecordMacro();
	void StartPlayMacro();
	bool RecordMacroCommand(SCNotification *notification);
	void ExecuteMacroCommand(const char * command);
	void AskMacroList();
	bool StartMacroList(const char *words);
	void ContinueMacroList(const char *stxt);
	bool ProcessCommandLine(SString &args, int phase);
	void EnumProperties(const char *action);
	void SendOneProperty(const char *kind, const char *key, const char *val);
	void PropertyFromDirector(const char *arg);

	// ExtensionAPI
	sptr_t Send(Pane p, unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	char *Range(Pane p, int start, int end);
	void Remove(Pane p, int start, int end);
	void Insert(Pane p, int pos, const char *s);
	void Trace(const char *s);
	char *Property(const char *key);
	void SetProperty(const char *key, const char *val);
	uptr_t GetInstance();
	void ShutDown();
	void Perform(const char *actions);

public:

	SciTEBase(Extension *ext = 0);
	virtual ~SciTEBase();

	void ProcessExecute();
	WindowID GetID() { return wSciTE.GetID(); }
};

/// Base size of file I/O operations.
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
#endif

int ControlIDOfCommand(unsigned long);
time_t GetModTime(const char *fullPath);
bool IsUntitledFileName(const char *name);
void LowerCaseString(char *s);
void ChopTerminalSlash(char *path);
int IntFromHexDigit(const char ch);
ColourDesired ColourFromString(const char *val);
long ColourOfProperty(PropSet &props, const char *key, ColourDesired colourDefault);
char *Slash(const char *s);
unsigned int UnSlash(char *s);
void WindowSetFocus(Window &w);
