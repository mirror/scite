// SciTE - Scintilla based Text Editor
/** @file SciTEBase.h
 ** Definition of platform independent base class of editor.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

extern const GUI::gui_char appName[];

extern const GUI::gui_char propUserFileName[];
extern const GUI::gui_char propGlobalFileName[];
extern const GUI::gui_char propAbbrevFileName[];

#ifdef _WIN32
#ifdef _MSC_VER
// Shut up level 4 warning:
// warning C4800: forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable: 4800)
#endif
#endif

inline int Minimum(int a, int b) {
	return (a < b) ? a : b;
}

inline int Maximum(int a, int b) {
	return (a > b) ? a : b;
}

inline long LongFromTwoShorts(short a,short b) {
	return (a) | ((b) << 16);
}

/**
 * The order of menus on Windows - the Buffers menu may not be present
 * and there is a Help menu at the end.
 */
enum {
    menuFile = 0, menuEdit = 1, menuSearch = 2, menuView = 3,
    menuTools = 4, menuOptions = 5, menuLanguage = 6, menuBuffers = 7,
    menuHelp = 8
};

struct SelectedRange {
	int position;
	int anchor;
	SelectedRange(int position_= INVALID_POSITION, int anchor_= INVALID_POSITION) :
		position(position_), anchor(anchor_) {
	}
};

class RecentFile : public FilePath {
public:
	SelectedRange selection;
	int scrollPosition;
	RecentFile() {
		scrollPosition = 0;
	}
	void Init() {
		FilePath::Init();
		selection.position = INVALID_POSITION;
		selection.anchor = INVALID_POSITION;
		scrollPosition = 0;
	}
};

struct BufferState : public RecentFile {
public:
	std::vector<int> foldState;
	std::vector<int> bookmarks;
};

class Session {
public:
	FilePath pathActive;
	std::vector<BufferState> buffers;
};

struct FileWorker;

class Buffer : public RecentFile {
public:
	sptr_t doc;
	bool isDirty;
	bool isReadOnly;
	bool failedSave;
	bool useMonoFont;
	enum { empty, reading, readAll, open } lifeState;
	UniMode unicodeMode;
	time_t fileModTime;
	time_t fileModLastAsk;
	time_t documentModTime;
	enum { fmNone, fmTemporary, fmMarked, fmModified} findMarks;
	std::string overrideExtension;	///< User has chosen to use a particular language
	std::vector<int> foldState;
	std::vector<int> bookmarks;
	FileWorker *pFileWorker;
	PropSetFile props;
	enum FutureDo { fdNone=0, fdFinishSave=1 } futureDo;
	Buffer() :
			RecentFile(), doc(0), isDirty(false), isReadOnly(false), failedSave(false), useMonoFont(false), lifeState(empty),
			unicodeMode(uni8Bit), fileModTime(0), fileModLastAsk(0), documentModTime(0),
			findMarks(fmNone), pFileWorker(0), futureDo(fdNone) {}

	void Init() {
		RecentFile::Init();
		isDirty = false;
		isReadOnly = false;
		failedSave = false;
		useMonoFont = false;
		lifeState = empty;
		unicodeMode = uni8Bit;
		fileModTime = 0;
		fileModLastAsk = 0;
		documentModTime = 0;
		findMarks = fmNone;
		overrideExtension = "";
		foldState.clear();
		bookmarks.clear();
		pFileWorker = 0;
		futureDo = fdNone;
	}

	void SetTimeFromFile() {
		fileModTime = ModifiedTime();
		fileModLastAsk = fileModTime;
		documentModTime = fileModTime;
		failedSave = false;
	}

	void DocumentModified();
	bool NeedsSave(int delayBeforeSave) const;

	void CompleteLoading();
	void CompleteStoring();
	void AbandonAutomaticSave();

	bool ShouldNotSave() const {
		return lifeState != open;
	}

	void CancelLoad();
};

struct BackgroundActivities {
	int loaders;
	int storers;
	int totalWork;
	int totalProgress;
	GUI::gui_string fileNameLast;
};

class BufferList {
protected:
	int current;
	int stackcurrent;
	int *stack;
public:
	Buffer *buffers;
	int size;
	int length;
	int lengthVisible;
	bool initialised;

	BufferList();
	~BufferList();
	void Allocate(int maxSize);
	int Add();
	int GetDocumentByWorker(FileWorker *pFileWorker) const;
	int GetDocumentByName(FilePath filename, bool excludeCurrent=false);
	void RemoveInvisible(int index);
	void RemoveCurrent();
	int Current() const;
	Buffer *CurrentBuffer() const;
	void SetCurrent(int index);
	int StackNext();
	int StackPrev();
	void CommitStackSelection();
	void MoveToStackTop(int index);
	void ShiftTo(int indexFrom, int indexTo);
	void Swap(int indexA, int indexB);
	bool SingleBuffer() const;
	BackgroundActivities CountBackgroundActivities() const;
	bool SavingInBackground() const;
	bool GetVisible(int index) const;
	void SetVisible(int index, bool visible);
	void AddFuture(int index, Buffer::FutureDo fd);
	void FinishedFuture(int index, Buffer::FutureDo fd);
private:
	void PopStack();
};

// class to hold user defined keyboard short cuts
class ShortcutItem {
public:
	std::string menuKey; // the keyboard short cut
	std::string menuCommand; // the menu command to be passed to "SciTEBase::MenuCommand"
};

class LanguageMenuItem {
public:
	std::string menuItem;
	std::string menuKey;
	std::string extension;
};

enum {
    heightTools = 24,
    heightToolsBig = 32,
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

struct StyleAndWords {
	int styleNumber;
	std::string words;
	StyleAndWords() : styleNumber(0) {
	}
	bool IsEmpty() const { return words.length() == 0; }
	bool IsSingleChar() const { return words.length() == 1; }
};

struct CurrentWordHighlight {
	enum {
		noDelay,            // No delay, and no word at the caret.
		delay,              // Delay before to highlight the word at the caret.
		delayJustEnded,     // Delay has just ended. This state allows to ignore next HighlightCurrentWord (SCN_UPDATEUI and SC_UPDATE_CONTENT for setting indicators).
		delayAlreadyElapsed // Delay has already elapsed, word at the caret and occurrences are (or have to be) highlighted.
	} statesOfDelay;
	bool isEnabled;
	bool textHasChanged;
	GUI::ElapsedTime elapsedTimes;
	bool isOnlyWithSameStyle;

	CurrentWordHighlight() {
		statesOfDelay = noDelay;
		isEnabled = false;
		isOnlyWithSameStyle = false;
		textHasChanged = false;
	}
};

class Localization : public PropSetFile, public ILocalize {
	std::string missing;
public:
	bool read;
	Localization() : PropSetFile(true), read(false) {
	}
	GUI::gui_string Text(const char *s, bool retainIfNotFound=true);
	void SetMissing(const std::string &missing_) {
		missing = missing_;
	}
};

// Interface between SciTE and dialogs and strips for find and replace
class Searcher {
public:
	std::string findWhat;
	std::string replaceWhat;

	bool wholeWord;
	bool matchCase;
	bool regExp;
	bool unSlash;
	bool wrapFind;
	bool reverseFind;

	int searchStartPosition;
	bool replacing;
	bool havefound;
	bool failedfind;
	bool findInStyle;
	int findStyle;
	bool closeFind;
	ComboMemory memFinds;
	ComboMemory memReplaces;

	bool focusOnReplace;

	Searcher();

	virtual void SetFindText(const char *sFind) = 0;
	virtual void SetFind(const char *sFind) = 0;
	virtual bool FindHasText() const = 0;
	void InsertFindInMemory();
	virtual void SetReplace(const char *sReplace) = 0;
	virtual void SetCaretAsStart() = 0;
	virtual void MoveBack() = 0;
	virtual void ScrollEditorIfNeeded() = 0;

	virtual int FindNext(bool reverseDirection, bool showWarnings=true, bool allowRegExp=true) = 0;
	virtual void HideMatch() = 0;
	enum MarkPurpose { markWithBookMarks, markIncremental };
	virtual void MarkAll(MarkPurpose purpose=markWithBookMarks) = 0;
	virtual int ReplaceAll(bool inSelection) = 0;
	virtual void ReplaceOnce(bool showWarnings=true) = 0;
	virtual void UIClosed() = 0;
	virtual void UIHasFocus() = 0;
	bool &FlagFromCmd(int cmd);
};

// User interface for search options implemented as both buttons and popup menu items
struct SearchOption {
	enum { tWord, tCase, tRegExp, tBackslash, tWrap, tUp };
	const char *label;
	int cmd;	// Menu item
	int id;	// Control in dialog
};

class SearchUI {
protected:
	Searcher *pSearcher;
public:
	SearchUI() : pSearcher(0) {
	}
	void SetSearcher(Searcher *pSearcher_) {
		pSearcher = pSearcher_;
	}
};

class SciTEBase : public ExtensionAPI, public Searcher, public WorkerListener {
protected:
	bool needIdle;
	GUI::gui_string windowName;
	FilePath filePath;
	FilePath dirNameAtExecute;
	FilePath dirNameForExecute;

	enum { fileStackMax = 10 };
	RecentFile recentFileStack[fileStackMax];
	enum { fileStackCmdID = IDM_MRUFILE, bufferCmdID = IDM_BUFFER };

	enum { importMax = 50 };
	std::vector<FilePath> importFiles;
	enum { importCmdID = IDM_IMPORT };
	ImportFilter filter;

	enum { indicatorMatch = INDIC_CONTAINER,
		indicatorHighlightCurrentWord,
		indicatorSpellingMistake,
		indicatorSentinel };
	enum { markerBookmark = 1 };
	ComboMemory memFiles;
	ComboMemory memDirectory;
	std::string parameterisedCommand;
	std::string abbrevInsert;

	enum { languageCmdID = IDM_LANGUAGE };
	std::vector<LanguageMenuItem> languageMenu;

	// an array of short cut items that are defined by the user in the properties file.
	std::vector<ShortcutItem> shortCutItemList;

	int codePage;
	int characterSet;
	std::string language;
	int lexLanguage;
	std::vector<char> subStyleBases;
	int lexLPeg;
	StringList apis;
	std::string apisFileNames;
	std::string functionDefinition;

	int diagnosticStyleStart;
	enum { diagnosticStyles=4};

	bool indentOpening;
	bool indentClosing;
	bool indentMaintain;
	int statementLookback;
	StyleAndWords statementIndent;
	StyleAndWords statementEnd;
	StyleAndWords blockStart;
	StyleAndWords blockEnd;
	enum PreProcKind { ppcNone, ppcStart, ppcMiddle, ppcEnd, ppcDummy };	///< Indicate the kind of preprocessor condition line
	char preprocessorSymbol;	///< Preprocessor symbol (in C, #)
	std::map<std::string, PreProcKind> preprocOfString; ///< Map preprocessor keywords to positions
	/// In C, if ifdef ifndef : start, else elif : middle, endif : end.

	GUI::Window wSciTE;  ///< Contains wToolBar, wTabBar, wContent, and wStatusBar
	GUI::Window wContent;    ///< Contains wEditor and wOutput
	GUI::ScintillaWindow wEditor;
	GUI::ScintillaWindow wOutput;
	GUI::Window wIncrement;
	GUI::Window wToolBar;
	GUI::Window wStatusBar;
	GUI::Window wTabBar;
	GUI::Menu popup;
	bool tbVisible;
	bool tabVisible;
	bool tabHideOne; // Hide tab bar if one buffer is opened only
	bool tabMultiLine;
	bool sbVisible;	///< @c true if status bar is visible.
	std::string sbValue;	///< Status bar text.
	int sbNum;	///< Number of the currently displayed status bar information.
	int visHeightTools;
	int visHeightTab;
	int visHeightStatus;
	int visHeightEditor;
	int heightBar;
	// Prevent automatic load dialog appearing at the same time as
	// other dialogs as this can leads to reentry errors.
	int dialogsOnScreen;
	bool topMost;
	bool wrap;
	bool wrapOutput;
	int wrapStyle;
	int alphaIndicator;
	bool underIndicator;
	bool openFilesHere;
	bool fullScreen;
	enum { toolMax = 50 };
	Extension *extender;
	bool needReadProperties;
	bool quitting;

	int timerMask;
	enum { timerAutoSave=1 };
	int delayBeforeAutoSave;

	int heightOutput;
	int heightOutputStartDrag;
	GUI::Point ptStartDrag;
	bool capturedMouse;
	int previousHeightOutput;
	bool firstPropertiesRead;
	bool splitVertical;	///< @c true if the split bar between editor and output is vertical.
	bool bufferedDraw;
	bool bracesCheck;
	bool bracesSloppy;
	int bracesStyle;
	int braceCount;

	int indentationWSVisible;
	int indentExamine;
	bool autoCompleteIgnoreCase;
	bool imeAutoComplete;
	bool callTipUseEscapes;
	bool callTipIgnoreCase;
	bool autoCCausedByOnlyOne;
	std::string calltipWordCharacters;
	std::string calltipParametersStart;
	std::string calltipParametersEnd;
	std::string calltipParametersSeparators;
	std::string calltipEndDefinition;
	std::string autoCompleteStartCharacters;
	std::string autoCompleteFillUpCharacters;
	std::string wordCharacters;
	std::string whitespaceCharacters;
	int startCalltipWord;
	int currentCallTip;
	int maxCallTips;
	std::string currentCallTipWord;
	int lastPosCallTip;

	bool margin;
	int marginWidth;
	enum { marginWidthDefault = 20};

	bool foldMargin;
	int foldMarginWidth;
	enum { foldMarginWidthDefault = 14};

	bool lineNumbers;
	int lineNumbersWidth;
	enum { lineNumbersWidthDefault = 4 };
	bool lineNumbersExpand;

	bool allowMenuActions;
	int scrollOutput;
	bool returnOutputToCommand;
	JobQueue jobQueue;

	bool macrosEnabled;
	std::string currentMacro;
	bool recording;

	PropSetFile propsPlatform;
	PropSetFile propsEmbed;
	PropSetFile propsBase;
	PropSetFile propsUser;
	PropSetFile propsDirectory;
	PropSetFile propsLocal;
	PropSetFile propsDiscovered;
	PropSetFile props;

	PropSetFile propsAbbrev;

	PropSetFile propsSession;

	FilePath pathAbbreviations;

	Localization localiser;

	PropSetFile propsStatus;	// Not attached to a file but need SetInteger method.

	enum { bufferMax = IDM_IMPORT - IDM_BUFFER };
	BufferList buffers;

	// Handle buffers
	sptr_t GetDocumentAt(int index);
	void SwitchDocumentAt(int index, sptr_t pdoc);
	int AddBuffer();
	void UpdateBuffersCurrent();
	bool IsBufferAvailable() const;
	bool CanMakeRoom(bool maySaveIfDirty = true);
	void SetDocumentAt(int index, bool updateStack = true);
	Buffer *CurrentBuffer() const {
		return buffers.CurrentBuffer();
	}
	void SetBuffersMenu();
	void BuffersMenu();
	void Next();
	void Prev();
	void NextInStack();
	void PrevInStack();
	void EndStackedTabbing();

	virtual void TabInsert(int index, const GUI::gui_char *title) = 0;
	virtual void TabSelect(int index) = 0;
	virtual void RemoveAllTabs() = 0;
	void ShiftTab(int indexFrom, int indexTo);
	void MoveTabRight();
	void MoveTabLeft();

	virtual void ReadEmbeddedProperties();
	void ReadEnvironment();
	void ReadGlobalPropFile();
	void ReadAbbrevPropFile();
	void ReadLocalPropFile();
	void ReadDirectoryPropFile();

	int CallFocused(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	int CallFocusedElseDefault(int defaultValue, unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	sptr_t CallPane(int destination, unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	void CallChildren(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	std::string GetTranslationToAbout(const char * const propname, bool retainIfNotFound = true);
	int LengthDocument();
	int GetCaretInLine();
	void GetLine(char *text, int sizeText, int line = -1);
	std::string GetCurrentLine();
	static void GetRange(GUI::ScintillaWindow &win, int start, int end, char *text);
	int IsLinePreprocessorCondition(char *line);
	bool FindMatchingPreprocessorCondition(int &curLine, int direction, int condEnd1, int condEnd2);
	bool FindMatchingPreprocCondPosition(bool isForward, int &mppcAtCaret, int &mppcMatch);
	bool FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy);
	void BraceMatch(bool editor);

	virtual void WarnUser(int warnID) = 0;
	void SetWindowName();
	void SetFileName(FilePath openName, bool fixCase = true);
	FilePath FileNameExt() const {
		return filePath.Name();
	}
	void ClearDocument();
	void CreateBuffers();
	void InitialiseBuffers();
	FilePath UserFilePath(const GUI::gui_char *name);
	void LoadSessionFile(const GUI::gui_char *sessionName);
	void RestoreRecentMenu();
	void RestoreFromSession(const Session &session);
	void RestoreSession();
	void SaveSessionFile(const GUI::gui_char *sessionName);
	virtual void GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize) = 0;
	void SetIndentSettings();
	void SetEol();
	void New();
	void RestoreState(const Buffer &buffer, bool restoreBookmarks);
	void Close(bool updateUI = true, bool loadingSession = false, bool makingRoomForNew = false);
	bool IsAbsolutePath(const char *path);
	static bool Exists(const GUI::gui_char *dir, const GUI::gui_char *path, FilePath *resultPath);
	void DiscoverEOLSetting();
	void DiscoverIndentSetting();
	std::string DiscoverLanguage();
	void OpenCurrentFile(long fileSize, bool suppressMessage, bool asynchronous);
	virtual void OpenUriList(const char *) {}
	virtual bool OpenDialog(FilePath directory, const GUI::gui_char *filesFilter) = 0;
	virtual bool SaveAsDialog() = 0;
	virtual void LoadSessionDialog() {}
	virtual void SaveSessionDialog() {}
	void CountLineEnds(int &linesCR, int &linesLF, int &linesCRLF);
	enum OpenFlags {
	    ofNone = 0, 		// Default
	    ofNoSaveIfDirty = 1, 	// Suppress check for unsaved changes
	    ofForceLoad = 2,	// Reload file even if already in a buffer
	    ofPreserveUndo = 4,	// Do not delete undo history
	    ofQuiet = 8,		// Avoid "Could not open file" message
	    ofSynchronous = 16	// Force synchronous read
	};
	void TextRead(FileWorker *pFileLoader);
	void TextWritten(FileWorker *pFileStorer);
	void UpdateProgress(Worker *pWorker);
	void PerformDeferredTasks();
	enum OpenCompletion { ocSynchronous, ocCompleteCurrent, ocCompleteSwitch };
	void CompleteOpen(OpenCompletion oc);
	virtual bool PreOpenCheck(const GUI::gui_char *file);
	bool Open(FilePath file, OpenFlags of = ofNone);
	bool OpenSelected();
	void Revert();
	FilePath SaveName(const char *ext) const;
	enum SaveFlags {
	    sfNone = 0, 		// Default
	    sfProgressVisible = 1, 	// Show in background save strip
	    sfSynchronous = 16	// Write synchronously blocking UI
	};
	enum SaveResult {
		saveCompleted,
		saveCancelled
	};
	SaveResult SaveIfUnsure(bool forceQuestion = false, SaveFlags sf = sfProgressVisible);
	SaveResult SaveIfUnsureAll();
	SaveResult SaveIfUnsureForBuilt();
	bool SaveIfNotOpen(const FilePath &destFile, bool fixCase);
	void AbandonAutomaticSave();
	bool Save(SaveFlags sf = sfProgressVisible);
	void SaveAs(const GUI::gui_char *file, bool fixCase);
	virtual void SaveACopy() = 0;
	void SaveToHTML(FilePath saveName);
	void StripTrailingSpaces();
	void EnsureFinalNewLine();
	bool PrepareBufferForSave(FilePath saveName);
	bool SaveBuffer(FilePath saveName, SaveFlags sf);
	virtual void SaveAsHTML() = 0;
	void SaveToStreamRTF(std::ostream &os, int start = 0, int end = -1);
	void SaveToRTF(FilePath saveName, int start = 0, int end = -1);
	virtual void SaveAsRTF() = 0;
	void SaveToPDF(FilePath saveName);
	virtual void SaveAsPDF() = 0;
	void SaveToTEX(FilePath saveName);
	virtual void SaveAsTEX() = 0;
	void SaveToXML(FilePath saveName);
	virtual void SaveAsXML() = 0;
	virtual FilePath GetDefaultDirectory() = 0;
	virtual FilePath GetSciteDefaultHome() = 0;
	virtual FilePath GetSciteUserHome() = 0;
	FilePath GetDefaultPropertiesFileName();
	FilePath GetUserPropertiesFileName();
	FilePath GetDirectoryPropertiesFileName();
	FilePath GetLocalPropertiesFileName();
	FilePath GetAbbrevPropertiesFileName();
	void OpenProperties(int propsFile);
	static int GetMenuCommandAsInt(std::string commandName);
	virtual void Print(bool) {}
	virtual void PrintSetup() {}
	virtual void UserStripShow(const char * /* description */) {}
	virtual void UserStripSet(int /* control */, const char * /* value */) {}
	virtual void UserStripSetList(int /* control */, const char * /* value */) {}
	virtual const char *UserStripValue(int /* control */) { return 0; }
	virtual void ShowBackgroundProgress(const GUI::gui_string & /* explanation */, int /* size */, int /* progress */) {}
	Sci_CharacterRange GetSelection();
	SelectedRange GetSelectedRange();
	void SetSelection(int anchor, int currentPos);
	std::string GetCTag();
	static std::string GetRangeString(GUI::ScintillaWindow &win, int selStart, int selEnd);
	virtual std::string GetRangeInUIEncoding(GUI::ScintillaWindow &win, int selStart, int selEnd);
	static std::string GetLine(GUI::ScintillaWindow &win, int line);
	void RangeExtend(GUI::ScintillaWindow &wCurrent, int &selStart, int &selEnd,
		bool (SciTEBase::*ischarforsel)(char ch));
	std::string RangeExtendAndGrab(GUI::ScintillaWindow &wCurrent, int &selStart, int &selEnd,
		bool (SciTEBase::*ischarforsel)(char ch), bool stripEol = true);
	std::string SelectionExtend(bool (SciTEBase::*ischarforsel)(char ch), bool stripEol = true);
	std::string SelectionWord(bool stripEol = true);
	std::string SelectionFilename();
	void SelectionIntoProperties();
	void SelectionIntoFind(bool stripEol = true);
	enum AddSelection { addNext, addEach };
	void SelectionAdd(AddSelection add);
	virtual std::string EncodeString(const std::string &s);
	virtual void Find() = 0;
	enum MessageBoxChoice {
		mbOK,
		mbCancel,
		mbYes,
		mbNo
	};
	typedef int MessageBoxStyle;
	enum {
		// Same as Win32 MB_*
		mbsOK = 0,
		mbsYesNo = 4,
		mbsYesNoCancel = 3,
		mbsIconQuestion = 0x20,
		mbsIconWarning = 0x30
	};
	virtual MessageBoxChoice WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, MessageBoxStyle style = mbsIconWarning) = 0;
	void FailedSaveMessageBox(const FilePath &filePathSaving);
	virtual void FindMessageBox(const std::string &msg, const std::string *findItem = 0) = 0;
	bool FindReplaceAdvanced() const;
	int FindInTarget(std::string findWhatText, int startPosition, int endPosition);
	virtual void SetFindText(const char *sFind);
	virtual void SetFind(const char *sFind);
	virtual bool FindHasText() const;
	virtual void SetReplace(const char *sReplace);
	virtual void SetCaretAsStart();
	virtual void MoveBack();
	virtual void ScrollEditorIfNeeded();
	virtual int FindNext(bool reverseDirection, bool showWarnings=true, bool allowRegExp=true);
	virtual void HideMatch();
	virtual void FindIncrement() = 0;
	int IncrementSearchMode();
	virtual void FindInFiles() = 0;
	virtual void Replace() = 0;
	void ReplaceOnce(bool showWarnings=true);
	int DoReplaceAll(bool inSelection); // returns number of replacements or negative value if error
	int ReplaceAll(bool inSelection);
	int ReplaceInBuffers();
	virtual void UIClosed();
	virtual void UIHasFocus();
	virtual void DestroyFindReplace() = 0;
	virtual void GoLineDialog() = 0;
	virtual bool AbbrevDialog() = 0;
	virtual void TabSizeDialog() = 0;
	virtual bool ParametersOpen() = 0;
	virtual void ParamGrab() = 0;
	virtual bool ParametersDialog(bool modal) = 0;
	bool HandleXml(char ch);
	static std::string FindOpenXmlTag(const char sel[], int nSize);
	void GoMatchingBrace(bool select);
	void GoMatchingPreprocCond(int direction, bool select);
	virtual void FindReplace(bool replace) = 0;
	void OutputAppendString(const char *s, int len = -1);
	virtual void OutputAppendStringSynchronised(const char *s, int len = -1);
	virtual void Execute();
	virtual void StopExecute() = 0;
	void ShowMessages(int line);
	void GoMessage(int dir);
	virtual bool StartCallTip();
	std::string GetNearestWords(const char *wordStart, size_t searchLen,
		const char *separators, bool ignoreCase=false, bool exactLen=false);
	virtual void FillFunctionDefinition(int pos = -1);
	void ContinueCallTip();
	virtual void EliminateDuplicateWords(std::string &words);
	virtual bool StartAutoComplete();
	virtual bool StartAutoCompleteWord(bool onlyOneWord);
	virtual bool StartExpandAbbreviation();
	bool PerformInsertAbbreviation();
	virtual bool StartInsertAbbreviation();
	virtual bool StartBlockComment();
	virtual bool StartBoxComment();
	virtual bool StartStreamComment();
	unsigned int GetLinePartsInStyle(int line, int style1, int style2, std::string sv[], int len);
	void SetLineIndentation(int line, int indent);
	int GetLineIndentation(int line);
	int GetLineIndentPosition(int line);
	void ConvertIndentation(int tabSize, int useTabs);
	bool RangeIsAllWhitespace(int start, int end);
	IndentationStatus GetIndentState(int line);
	int IndentOfBlock(int line);
	void MaintainIndentation(char ch);
	void AutomaticIndentation(char ch);
	void CharAdded(int ch);
	void CharAddedOutput(int ch);
	void SetTextProperties(PropSetFile &ps);
	virtual void SetFileProperties(PropSetFile &ps) = 0;
	virtual void UpdateStatusBar(bool bUpdateSlowData);
	int GetLineLength(int line);
	int GetCurrentLineNumber();
	int GetCurrentScrollPosition();
	virtual void AddCommand(const std::string &cmd, const std::string &dir,
	        JobSubsystem jobType, const std::string &input = "",
	        int flags = 0);
	virtual void AboutDialog() = 0;
	virtual void QuitProgram() = 0;
	void CloseTab(int tab);
	void CloseAllBuffers(bool loadingSession = false);
	SaveResult SaveAllBuffers(bool alwaysYes);
	void SaveTitledBuffers();
	virtual void CopyAsRTF() {}
	virtual void CopyPath() {}
	void SetLineNumberWidth();
	void MenuCommand(int cmdID, int source = 0);
	void FoldChanged(int line, int levelNow, int levelPrev);
	void FoldChanged(int position);
	void Expand(int &line, bool doExpand, bool force = false,
	        int visLevels = 0, int level = -1);
	void FoldAll();
	void ToggleFoldRecursive(int line, int level);
	void EnsureAllChildrenVisible(int line, int level);
	static void EnsureRangeVisible(GUI::ScintillaWindow &win, int posStart, int posEnd, bool enforcePolicy = true);
	void GotoLineEnsureVisible(int line);
	bool MarginClick(int position, int modifiers);
	void NewLineInOutput();
	virtual void SetStatusBarText(const char *s) = 0;
	virtual void Notify(const SCNotification *notification);
	virtual void ShowToolBar() = 0;
	virtual void ShowTabBar() = 0;
	virtual void ShowStatusBar() = 0;
	virtual void ActivateWindow(const char *timestamp) = 0;

	void RemoveFindMarks();
	void MarkAll(MarkPurpose purpose=markWithBookMarks);
	void BookmarkAdd(int lineno = -1);
	void BookmarkDelete(int lineno = -1);
	bool BookmarkPresent(int lineno = -1);
	void BookmarkToggle(int lineno = -1);
	void BookmarkNext(bool forwardScan = true, bool select = false);
	void SetOutputVisibility(bool show);
	virtual void ShowOutputOnMainThread();
	void ToggleOutputVisible();
	virtual void SizeContentWindows() = 0;
	virtual void SizeSubWindows() = 0;

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
		const GUI::gui_char *text, const GUI::gui_char *mnemonic = 0) = 0;
	virtual void RedrawMenu() {}
	virtual void DestroyMenuItem(int menuNumber, int itemID) = 0;
	virtual void CheckAMenuItem(int wIDCheckItem, bool val) = 0;
	virtual void EnableAMenuItem(int wIDCheckItem, bool val) = 0;
	virtual void CheckMenusClipboard();
	virtual void CheckMenus();
	virtual void AddToPopUp(const char *label, int cmd = 0, bool enabled = true) = 0;
	void ContextMenu(GUI::ScintillaWindow &wSource, GUI::Point pt, GUI::Window wCmd);

	void DeleteFileStackMenu();
	void SetFileStackMenu();
	bool AddFileToBuffer(const BufferState &bufferState);
	void AddFileToStack(FilePath file, SelectedRange selection, int scrollPos);
	void RemoveFileFromStack(FilePath file);
	RecentFile GetFilePosition();
	void DisplayAround(const RecentFile &rf);
	void StackMenu(int pos);
	void StackMenuNext();
	void StackMenuPrev();

	void RemoveToolsMenu();
	void SetMenuItemLocalised(int menuNumber, int position, int itemID,
	        const char *text, const char *mnemonic);
	bool ToolIsImmediate(int item);
	void SetToolsMenu();
	JobSubsystem SubsystemType(const char *cmd);
	void ToolsMenu(int item);

	void AssignKey(int key, int mods, int cmd);
	void ViewWhitespace(bool view);
	void SetAboutMessage(GUI::ScintillaWindow &wsci, const char *appTitle);
	void SetImportMenu();
	void ImportMenu(int pos);
	void SetLanguageMenu();
	void SetPropertiesInitial();
	GUI::gui_string LocaliseMessage(const char *s,
		const GUI::gui_char *param0 = 0, const GUI::gui_char *param1 = 0, const GUI::gui_char *param2 = 0);
	virtual void ReadLocalization();
	std::string GetFileNameProperty(const char *name);
	virtual void ReadPropertiesInitial();
	void ReadFontProperties();
	void SetOverrideLanguage(int cmdID);
	StyleAndWords GetStyleAndWords(const char *base);
	std::string ExtensionFileName() const;
	static const char *GetNextPropItem(const char *pStart, char *pPropItem, int maxLen);
	void ForwardPropertyToEditor(const char *key);
	void DefineMarker(int marker, int markerType, Colour fore, Colour back, Colour backSelected);
	void ReadAPI(const std::string &fileNameForExtension);
	std::string FindLanguageProperty(const char *pattern, const char *defaultValue = "");
	virtual void ReadProperties();
	std::string StyleString(const char *lang, int style) const;
	StyleDefinition StyleDefinitionFor(int style);
	void SetOneStyle(GUI::ScintillaWindow &win, int style, const StyleDefinition &sd);
	void SetStyleBlock(GUI::ScintillaWindow &win, const char *lang, int start, int last);
	void SetStyleFor(GUI::ScintillaWindow &win, const char *lang);
	static void SetOneIndicator(GUI::ScintillaWindow &win, int indicator, const IndicatorDefinition &ind);
	void ReloadProperties();

	void CheckReload();
	void Activate(bool activeApp);
	GUI::Rectangle GetClientRectangle();
	void Redraw();
	int NormaliseSplit(int splitPos);
	void MoveSplit(GUI::Point ptNewDrag);

	virtual void TimerStart(int mask);
	virtual void TimerEnd(int mask);
	void OnTimer();
	virtual void SetIdler(bool on);
	void OnIdle();

	void SetHomeProperties();
	void UIAvailable();
	void PerformOne(char *action);
	void StartRecordMacro();
	void StopRecordMacro();
	void StartPlayMacro();
	bool RecordMacroCommand(const SCNotification *notification);
	void ExecuteMacroCommand(const char *command);
	void AskMacroList();
	bool StartMacroList(const char *words);
	void ContinueMacroList(const char *stxt);
	bool ProcessCommandLine(GUI::gui_string &args, int phase);
	virtual bool IsStdinBlocked();
	void OpenFromStdin(bool UseOutputPane);
	void OpenFilesFromStdin();
	enum GrepFlags {
	    grepNone = 0, grepWholeWord = 1, grepMatchCase = 2, grepStdOut = 4,
	    grepDot = 8, grepBinary = 16, grepScroll = 32
	};
	virtual bool GrepIntoDirectory(const FilePath &directory);
	void GrepRecursive(GrepFlags gf, FilePath baseDir, const char *searchString, const GUI::gui_char *fileTypes);
	void InternalGrep(GrepFlags gf, const GUI::gui_char *directory, const GUI::gui_char *files,
			  const char *search, sptr_t &originalEnd);
	void EnumProperties(const char *action);
	void SendOneProperty(const char *kind, const char *key, const char *val);
	void PropertyFromDirector(const char *arg);
	void PropertyToDirector(const char *arg);

	// ExtensionAPI
	sptr_t Send(Pane p, unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0);
	char *Range(Pane p, int start, int end);
	void Remove(Pane p, int start, int end);
	void Insert(Pane p, int pos, const char *s);
	void Trace(const char *s);
	std::string Property(const char *key);
	void SetProperty(const char *key, const char *val);
	void UnsetProperty(const char *key);
	uptr_t GetInstance();
	void ShutDown();
	void Perform(const char *actions);
	void DoMenuCommand(int cmdID);

	// Valid CurrentWord characters
	bool iswordcharforsel(char ch);
	bool isfilenamecharforsel(char ch);
	bool islexerwordcharforsel(char ch);

	CurrentWordHighlight currentWordHighlight;
	void HighlightCurrentWord(bool highlight);
	MatchMarker matchMarker;
	MatchMarker findMarker;
public:

	enum { maxParam = 4 };

	explicit SciTEBase(Extension *ext = 0);
	virtual ~SciTEBase();

	void ProcessExecute();
	GUI::WindowID GetID() { return wSciTE.GetID(); }

	virtual bool PerformOnNewThread(Worker *pWorker) = 0;
	virtual void PostOnMainThread(int cmd, Worker *pWorker) = 0;
	virtual void WorkerCommand(int cmd, Worker *pWorker);

private:
	// un-implemented copy-constructor and assignment operator
	explicit SciTEBase(const SciTEBase&);
	void operator=(const SciTEBase&);
};

int ControlIDOfCommand(unsigned long);
long ColourOfProperty(PropSetFile &props, const char *key, Colour colourDefault);
void WindowSetFocus(GUI::ScintillaWindow &w);

inline bool isspacechar(unsigned char ch) {
    return (ch == ' ') || ((ch >= 0x09) && (ch <= 0x0d));
}
