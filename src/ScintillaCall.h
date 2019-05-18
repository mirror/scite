// SciTE - Scintilla based Text Editor
/** @file ScintillaCall.h
 ** Interface to calling a Scintilla instance.
 **/
// Copyright 1998-2019 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

/* Most of this file is automatically generated from the Scintilla.iface interface definition
 * file which contains any comments about the definitions. APIFacer.py does the generation. */

#ifndef SCINTILLACALL_H
#define SCINTILLACALL_H

namespace Scintilla::API {

enum class Message;	// Declare in case ScintillaMessages.h not included

using FunctionDirect = intptr_t(*)(intptr_t ptr, unsigned int iMessage, uintptr_t wParam, intptr_t lParam);

class ScintillaCall {
	FunctionDirect fn;
	intptr_t ptr;
	intptr_t CallPointer(Message msg, uintptr_t wParam, void *s);
	intptr_t CallString(Message msg, uintptr_t wParam, const char *s);
public:
	API::Status statusLastCall;
	ScintillaCall() noexcept;
	// All standard methods are fine

	void SetFnPtr(FunctionDirect fn_, intptr_t ptr_) noexcept;
	bool IsValid() const noexcept;
	void SetCallStatus();
	intptr_t Call(Message msg, uintptr_t wParam=0, intptr_t lParam=0);

	// Common APIs made more structured and type-safe
	Position LineStart(Line line);
	Position LineEnd(Line line);
	Position CurrentPosition();
	Range SelectionRange();
	void GotoPosition(Position position);
	Range TargetRange();
	void SetTarget(Range range);
	int IndentSize();
	void Colourise(Range range);
	void ColouriseAll();
	char CharacterAt(Position position);
	int UnsignedStyleAt(Position position);
	void ReplaceSelection(const char *text);
	Position ReplaceTarget(std::string_view text);
	Position ReplaceTargetRE(std::string_view text);
	Position SearchInTarget(std::string_view text);

	// Generated APIs
//++Autogenerated -- start of section automatically generated from Scintilla.iface
//**\(\*\n\)
	void AddText(Position length, const char *text);
	void AddStyledText(Position length, const char *c);
	void InsertText(Position pos, const char *text);
	void ChangeInsertion(Position length, const char *text);
	void ClearAll();
	void DeleteRange(Position start, Position lengthDelete);
	void ClearDocumentStyle();
	Position Length();
	int CharAt(Position pos);
	Position CurrentPos();
	Position Anchor();
	int StyleAt(Position pos);
	void Redo();
	void SetUndoCollection(bool collectUndo);
	void SelectAll();
	void SetSavePoint();
	Position GetStyledText(void *tr);
	bool CanRedo();
	int MarkerLineFromHandle(int markerHandle);
	void MarkerDeleteHandle(int markerHandle);
	bool UndoCollection();
	int ViewWS();
	void SetViewWS(int viewWS);
	int TabDrawMode();
	void SetTabDrawMode(int tabDrawMode);
	Position PositionFromPoint(int x, int y);
	Position PositionFromPointClose(int x, int y);
	void GotoLine(Line line);
	void GotoPos(Position caret);
	void SetAnchor(Position anchor);
	Position GetCurLine(Position length, char *text);
	Position EndStyled();
	void ConvertEOLs(int eolMode);
	int EOLMode();
	void SetEOLMode(int eolMode);
	void StartStyling(Position start, int unused);
	void SetStyling(Position length, int style);
	bool BufferedDraw();
	void SetBufferedDraw(bool buffered);
	void SetTabWidth(int tabWidth);
	int TabWidth();
	void ClearTabStops(Line line);
	void AddTabStop(Line line, int x);
	int GetNextTabStop(Line line, int x);
	void SetCodePage(int codePage);
	int IMEInteraction();
	void SetIMEInteraction(int imeInteraction);
	void MarkerDefine(int markerNumber, int markerSymbol);
	void MarkerSetFore(int markerNumber, Colour fore);
	void MarkerSetBack(int markerNumber, Colour back);
	void MarkerSetBackSelected(int markerNumber, Colour back);
	void MarkerEnableHighlight(bool enabled);
	int MarkerAdd(Line line, int markerNumber);
	void MarkerDelete(Line line, int markerNumber);
	void MarkerDeleteAll(int markerNumber);
	int MarkerGet(Line line);
	Line MarkerNext(Line lineStart, int markerMask);
	Line MarkerPrevious(Line lineStart, int markerMask);
	void MarkerDefinePixmap(int markerNumber, const char *pixmap);
	void MarkerAddSet(Line line, int markerSet);
	void MarkerSetAlpha(int markerNumber, int alpha);
	void SetMarginTypeN(int margin, int marginType);
	int MarginTypeN(int margin);
	void SetMarginWidthN(int margin, int pixelWidth);
	int MarginWidthN(int margin);
	void SetMarginMaskN(int margin, int mask);
	int MarginMaskN(int margin);
	void SetMarginSensitiveN(int margin, bool sensitive);
	bool MarginSensitiveN(int margin);
	void SetMarginCursorN(int margin, int cursor);
	int MarginCursorN(int margin);
	void SetMarginBackN(int margin, Colour back);
	Colour MarginBackN(int margin);
	void SetMargins(int margins);
	int Margins();
	void StyleClearAll();
	void StyleSetFore(int style, Colour fore);
	void StyleSetBack(int style, Colour back);
	void StyleSetBold(int style, bool bold);
	void StyleSetItalic(int style, bool italic);
	void StyleSetSize(int style, int sizePoints);
	void StyleSetFont(int style, const char *fontName);
	void StyleSetEOLFilled(int style, bool eolFilled);
	void StyleResetDefault();
	void StyleSetUnderline(int style, bool underline);
	Colour StyleGetFore(int style);
	Colour StyleGetBack(int style);
	bool StyleGetBold(int style);
	bool StyleGetItalic(int style);
	int StyleGetSize(int style);
	int StyleGetFont(int style, char *fontName);
	bool StyleGetEOLFilled(int style);
	bool StyleGetUnderline(int style);
	int StyleGetCase(int style);
	int StyleGetCharacterSet(int style);
	bool StyleGetVisible(int style);
	bool StyleGetChangeable(int style);
	bool StyleGetHotSpot(int style);
	void StyleSetCase(int style, int caseVisible);
	void StyleSetSizeFractional(int style, int sizeHundredthPoints);
	int StyleGetSizeFractional(int style);
	void StyleSetWeight(int style, int weight);
	int StyleGetWeight(int style);
	void StyleSetCharacterSet(int style, int characterSet);
	void StyleSetHotSpot(int style, bool hotspot);
	void SetSelFore(bool useSetting, Colour fore);
	void SetSelBack(bool useSetting, Colour back);
	int SelAlpha();
	void SetSelAlpha(int alpha);
	bool SelEOLFilled();
	void SetSelEOLFilled(bool filled);
	void SetCaretFore(Colour fore);
	void AssignCmdKey(int keyDefinition, int sciCommand);
	void ClearCmdKey(int keyDefinition);
	void ClearAllCmdKeys();
	void SetStylingEx(Position length, const char *styles);
	void StyleSetVisible(int style, bool visible);
	int CaretPeriod();
	void SetCaretPeriod(int periodMilliseconds);
	void SetWordChars(const char *characters);
	int WordChars(char *characters);
	void SetCharacterCategoryOptimization(int countCharacters);
	int CharacterCategoryOptimization();
	void BeginUndoAction();
	void EndUndoAction();
	void IndicSetStyle(int indicator, int indicatorStyle);
	int IndicGetStyle(int indicator);
	void IndicSetFore(int indicator, Colour fore);
	Colour IndicGetFore(int indicator);
	void IndicSetUnder(int indicator, bool under);
	bool IndicGetUnder(int indicator);
	void IndicSetHoverStyle(int indicator, int indicatorStyle);
	int IndicGetHoverStyle(int indicator);
	void IndicSetHoverFore(int indicator, Colour fore);
	Colour IndicGetHoverFore(int indicator);
	void IndicSetFlags(int indicator, int flags);
	int IndicGetFlags(int indicator);
	void SetWhitespaceFore(bool useSetting, Colour fore);
	void SetWhitespaceBack(bool useSetting, Colour back);
	void SetWhitespaceSize(int size);
	int WhitespaceSize();
	void SetLineState(Line line, int state);
	int LineState(Line line);
	int MaxLineState();
	bool CaretLineVisible();
	void SetCaretLineVisible(bool show);
	Colour CaretLineBack();
	void SetCaretLineBack(Colour back);
	int CaretLineFrame();
	void SetCaretLineFrame(int width);
	void StyleSetChangeable(int style, bool changeable);
	void AutoCShow(Position lengthEntered, const char *itemList);
	void AutoCCancel();
	bool AutoCActive();
	Position AutoCPosStart();
	void AutoCComplete();
	void AutoCStops(const char *characterSet);
	void AutoCSetSeparator(int separatorCharacter);
	int AutoCGetSeparator();
	void AutoCSelect(const char *select);
	void AutoCSetCancelAtStart(bool cancel);
	bool AutoCGetCancelAtStart();
	void AutoCSetFillUps(const char *characterSet);
	void AutoCSetChooseSingle(bool chooseSingle);
	bool AutoCGetChooseSingle();
	void AutoCSetIgnoreCase(bool ignoreCase);
	bool AutoCGetIgnoreCase();
	void UserListShow(int listType, const char *itemList);
	void AutoCSetAutoHide(bool autoHide);
	bool AutoCGetAutoHide();
	void AutoCSetDropRestOfWord(bool dropRestOfWord);
	bool AutoCGetDropRestOfWord();
	void RegisterImage(int type, const char *xpmData);
	void ClearRegisteredImages();
	int AutoCGetTypeSeparator();
	void AutoCSetTypeSeparator(int separatorCharacter);
	void AutoCSetMaxWidth(int characterCount);
	int AutoCGetMaxWidth();
	void AutoCSetMaxHeight(int rowCount);
	int AutoCGetMaxHeight();
	void SetIndent(int indentSize);
	int Indent();
	void SetUseTabs(bool useTabs);
	bool UseTabs();
	void SetLineIndentation(Line line, int indentation);
	int LineIndentation(Line line);
	Position LineIndentPosition(Line line);
	Position Column(Position pos);
	Position CountCharacters(Position start, Position end);
	Position CountCodeUnits(Position start, Position end);
	void SetHScrollBar(bool visible);
	bool HScrollBar();
	void SetIndentationGuides(int indentView);
	int IndentationGuides();
	void SetHighlightGuide(Position column);
	int HighlightGuide();
	Position LineEndPosition(Line line);
	int CodePage();
	Colour CaretFore();
	bool ReadOnly();
	void SetCurrentPos(Position caret);
	void SetSelectionStart(Position anchor);
	Position SelectionStart();
	void SetSelectionEnd(Position caret);
	Position SelectionEnd();
	void SetEmptySelection(Position caret);
	void SetPrintMagnification(int magnification);
	int PrintMagnification();
	void SetPrintColourMode(int mode);
	int PrintColourMode();
	Position FindText(int searchFlags, void *ft);
	Position FormatRange(bool draw, void *fr);
	Line FirstVisibleLine();
	int GetLine(Line line, char *text);
	Line LineCount();
	void SetMarginLeft(int pixelWidth);
	int MarginLeft();
	void SetMarginRight(int pixelWidth);
	int MarginRight();
	bool Modify();
	void SetSel(Position anchor, Position caret);
	int GetSelText(char *text);
	Position GetTextRange(void *tr);
	void HideSelection(bool hide);
	int PointXFromPosition(Position pos);
	int PointYFromPosition(Position pos);
	Line LineFromPosition(Position pos);
	Position PositionFromLine(Line line);
	void LineScroll(int columns, Line lines);
	void ScrollCaret();
	void ScrollRange(Position secondary, Position primary);
	void ReplaceSel(const char *text);
	void SetReadOnly(bool readOnly);
	void Null();
	bool CanPaste();
	bool CanUndo();
	void EmptyUndoBuffer();
	void Undo();
	void Cut();
	void Copy();
	void Paste();
	void Clear();
	void SetText(const char *text);
	Position GetText(Position length, char *text);
	Position TextLength();
	intptr_t DirectFunction();
	intptr_t DirectPointer();
	void SetOvertype(bool overType);
	bool Overtype();
	void SetCaretWidth(int pixelWidth);
	int CaretWidth();
	void SetTargetStart(Position start);
	Position TargetStart();
	void SetTargetEnd(Position end);
	Position TargetEnd();
	void SetTargetRange(Position start, Position end);
	int TargetText(char *text);
	void TargetFromSelection();
	void TargetWholeDocument();
	Position ReplaceTarget(Position length, const char *text);
	Position ReplaceTargetRE(Position length, const char *text);
	Position SearchInTarget(Position length, const char *text);
	void SetSearchFlags(int searchFlags);
	int SearchFlags();
	void CallTipShow(Position pos, const char *definition);
	void CallTipCancel();
	bool CallTipActive();
	Position CallTipPosStart();
	void CallTipSetPosStart(Position posStart);
	void CallTipSetHlt(int highlightStart, int highlightEnd);
	void CallTipSetBack(Colour back);
	void CallTipSetFore(Colour fore);
	void CallTipSetForeHlt(Colour fore);
	void CallTipUseStyle(int tabSize);
	void CallTipSetPosition(bool above);
	Line VisibleFromDocLine(Line docLine);
	Line DocLineFromVisible(Line displayLine);
	Line WrapCount(Line docLine);
	void SetFoldLevel(Line line, int level);
	int FoldLevel(Line line);
	int LastChild(Line line, int level);
	int FoldParent(Line line);
	void ShowLines(Line lineStart, Line lineEnd);
	void HideLines(Line lineStart, Line lineEnd);
	bool LineVisible(Line line);
	bool AllLinesVisible();
	void SetFoldExpanded(Line line, bool expanded);
	bool FoldExpanded(Line line);
	void ToggleFold(Line line);
	void ToggleFoldShowText(Line line, const char *text);
	void FoldDisplayTextSetStyle(int style);
	int FoldDisplayTextGetStyle();
	void SetDefaultFoldDisplayText(const char *text);
	int GetDefaultFoldDisplayText(char *text);
	void FoldLine(Line line, int action);
	void FoldChildren(Line line, int action);
	void ExpandChildren(Line line, int level);
	void FoldAll(int action);
	void EnsureVisible(Line line);
	void SetAutomaticFold(int automaticFold);
	int AutomaticFold();
	void SetFoldFlags(int flags);
	void EnsureVisibleEnforcePolicy(Line line);
	void SetTabIndents(bool tabIndents);
	bool TabIndents();
	void SetBackSpaceUnIndents(bool bsUnIndents);
	bool BackSpaceUnIndents();
	void SetMouseDwellTime(int periodMilliseconds);
	int MouseDwellTime();
	Position WordStartPosition(Position pos, bool onlyWordCharacters);
	Position WordEndPosition(Position pos, bool onlyWordCharacters);
	bool IsRangeWord(Position start, Position end);
	void SetIdleStyling(int idleStyling);
	int IdleStyling();
	void SetWrapMode(int wrapMode);
	int WrapMode();
	void SetWrapVisualFlags(int wrapVisualFlags);
	int WrapVisualFlags();
	void SetWrapVisualFlagsLocation(int wrapVisualFlagsLocation);
	int WrapVisualFlagsLocation();
	void SetWrapStartIndent(int indent);
	int WrapStartIndent();
	void SetWrapIndentMode(int wrapIndentMode);
	int WrapIndentMode();
	void SetLayoutCache(int cacheMode);
	int LayoutCache();
	void SetScrollWidth(int pixelWidth);
	int ScrollWidth();
	void SetScrollWidthTracking(bool tracking);
	bool ScrollWidthTracking();
	int TextWidth(int style, const char *text);
	void SetEndAtLastLine(bool endAtLastLine);
	bool EndAtLastLine();
	int TextHeight(Line line);
	void SetVScrollBar(bool visible);
	bool VScrollBar();
	void AppendText(Position length, const char *text);
	int PhasesDraw();
	void SetPhasesDraw(int phases);
	void SetFontQuality(int fontQuality);
	int FontQuality();
	void SetFirstVisibleLine(Line displayLine);
	void SetMultiPaste(int multiPaste);
	int MultiPaste();
	int Tag(int tagNumber, char *tagValue);
	void LinesJoin();
	void LinesSplit(int pixelWidth);
	void SetFoldMarginColour(bool useSetting, Colour back);
	void SetFoldMarginHiColour(bool useSetting, Colour fore);
	void SetAccessibility(int accessibility);
	int Accessibility();
	void LineDown();
	void LineDownExtend();
	void LineUp();
	void LineUpExtend();
	void CharLeft();
	void CharLeftExtend();
	void CharRight();
	void CharRightExtend();
	void WordLeft();
	void WordLeftExtend();
	void WordRight();
	void WordRightExtend();
	void Home();
	void HomeExtend();
	void LineEnd();
	void LineEndExtend();
	void DocumentStart();
	void DocumentStartExtend();
	void DocumentEnd();
	void DocumentEndExtend();
	void PageUp();
	void PageUpExtend();
	void PageDown();
	void PageDownExtend();
	void EditToggleOvertype();
	void Cancel();
	void DeleteBack();
	void Tab();
	void BackTab();
	void NewLine();
	void FormFeed();
	void VCHome();
	void VCHomeExtend();
	void ZoomIn();
	void ZoomOut();
	void DelWordLeft();
	void DelWordRight();
	void DelWordRightEnd();
	void LineCut();
	void LineDelete();
	void LineTranspose();
	void LineReverse();
	void LineDuplicate();
	void LowerCase();
	void UpperCase();
	void LineScrollDown();
	void LineScrollUp();
	void DeleteBackNotLine();
	void HomeDisplay();
	void HomeDisplayExtend();
	void LineEndDisplay();
	void LineEndDisplayExtend();
	void HomeWrap();
	void HomeWrapExtend();
	void LineEndWrap();
	void LineEndWrapExtend();
	void VCHomeWrap();
	void VCHomeWrapExtend();
	void LineCopy();
	void MoveCaretInsideView();
	Position LineLength(Line line);
	void BraceHighlight(Position posA, Position posB);
	void BraceHighlightIndicator(bool useSetting, int indicator);
	void BraceBadLight(Position pos);
	void BraceBadLightIndicator(bool useSetting, int indicator);
	Position BraceMatch(Position pos, int maxReStyle);
	bool ViewEOL();
	void SetViewEOL(bool visible);
	intptr_t DocPointer();
	void SetDocPointer(intptr_t doc);
	void SetModEventMask(int eventMask);
	Position EdgeColumn();
	void SetEdgeColumn(Position column);
	int EdgeMode();
	void SetEdgeMode(int edgeMode);
	Colour EdgeColour();
	void SetEdgeColour(Colour edgeColour);
	void MultiEdgeAddLine(Position column, Colour edgeColour);
	void MultiEdgeClearAll();
	void SearchAnchor();
	int SearchNext(int searchFlags, const char *text);
	int SearchPrev(int searchFlags, const char *text);
	Line LinesOnScreen();
	void UsePopUp(int popUpMode);
	bool SelectionIsRectangle();
	void SetZoom(int zoomInPoints);
	int Zoom();
	intptr_t CreateDocument(Position bytes, int documentOptions);
	void AddRefDocument(intptr_t doc);
	void ReleaseDocument(intptr_t doc);
	int DocumentOptions();
	int ModEventMask();
	void SetCommandEvents(bool commandEvents);
	bool CommandEvents();
	void SetFocus(bool focus);
	bool Focus();
	void SetStatus(int status);
	int Status();
	void SetMouseDownCaptures(bool captures);
	bool MouseDownCaptures();
	void SetMouseWheelCaptures(bool captures);
	bool MouseWheelCaptures();
	void SetCursor(int cursorType);
	int Cursor();
	void SetControlCharSymbol(int symbol);
	int ControlCharSymbol();
	void WordPartLeft();
	void WordPartLeftExtend();
	void WordPartRight();
	void WordPartRightExtend();
	void SetVisiblePolicy(int visiblePolicy, int visibleSlop);
	void DelLineLeft();
	void DelLineRight();
	void SetXOffset(int xOffset);
	int XOffset();
	void ChooseCaretX();
	void GrabFocus();
	void SetXCaretPolicy(int caretPolicy, int caretSlop);
	void SetYCaretPolicy(int caretPolicy, int caretSlop);
	void SetPrintWrapMode(int wrapMode);
	int PrintWrapMode();
	void SetHotspotActiveFore(bool useSetting, Colour fore);
	Colour HotspotActiveFore();
	void SetHotspotActiveBack(bool useSetting, Colour back);
	Colour HotspotActiveBack();
	void SetHotspotActiveUnderline(bool underline);
	bool HotspotActiveUnderline();
	void SetHotspotSingleLine(bool singleLine);
	bool HotspotSingleLine();
	void ParaDown();
	void ParaDownExtend();
	void ParaUp();
	void ParaUpExtend();
	Position PositionBefore(Position pos);
	Position PositionAfter(Position pos);
	Position PositionRelative(Position pos, Position relative);
	Position PositionRelativeCodeUnits(Position pos, Position relative);
	void CopyRange(Position start, Position end);
	void CopyText(Position length, const char *text);
	void SetSelectionMode(int selectionMode);
	int SelectionMode();
	bool MoveExtendsSelection();
	Position GetLineSelStartPosition(Line line);
	Position GetLineSelEndPosition(Line line);
	void LineDownRectExtend();
	void LineUpRectExtend();
	void CharLeftRectExtend();
	void CharRightRectExtend();
	void HomeRectExtend();
	void VCHomeRectExtend();
	void LineEndRectExtend();
	void PageUpRectExtend();
	void PageDownRectExtend();
	void StutteredPageUp();
	void StutteredPageUpExtend();
	void StutteredPageDown();
	void StutteredPageDownExtend();
	void WordLeftEnd();
	void WordLeftEndExtend();
	void WordRightEnd();
	void WordRightEndExtend();
	void SetWhitespaceChars(const char *characters);
	int WhitespaceChars(char *characters);
	void SetPunctuationChars(const char *characters);
	int PunctuationChars(char *characters);
	void SetCharsDefault();
	int AutoCGetCurrent();
	int AutoCGetCurrentText(char *text);
	void AutoCSetCaseInsensitiveBehaviour(int behaviour);
	int AutoCGetCaseInsensitiveBehaviour();
	void AutoCSetMulti(int multi);
	int AutoCGetMulti();
	void AutoCSetOrder(int order);
	int AutoCGetOrder();
	void Allocate(Position bytes);
	int TargetAsUTF8(char *s);
	void SetLengthForEncode(Position bytes);
	int EncodedFromUTF8(const char *utf8, char *encoded);
	Position FindColumn(Line line, Position column);
	int CaretSticky();
	void SetCaretSticky(int useCaretStickyBehaviour);
	void ToggleCaretSticky();
	void SetPasteConvertEndings(bool convert);
	bool PasteConvertEndings();
	void SelectionDuplicate();
	void SetCaretLineBackAlpha(int alpha);
	int CaretLineBackAlpha();
	void SetCaretStyle(int caretStyle);
	int CaretStyle();
	void SetIndicatorCurrent(int indicator);
	int IndicatorCurrent();
	void SetIndicatorValue(int value);
	int IndicatorValue();
	void IndicatorFillRange(Position start, Position lengthFill);
	void IndicatorClearRange(Position start, Position lengthClear);
	int IndicatorAllOnFor(Position pos);
	int IndicatorValueAt(int indicator, Position pos);
	int IndicatorStart(int indicator, Position pos);
	int IndicatorEnd(int indicator, Position pos);
	void SetPositionCache(int size);
	int PositionCache();
	void CopyAllowLine();
	intptr_t CharacterPointer();
	intptr_t RangePointer(Position start, Position lengthRange);
	Position GapPosition();
	void IndicSetAlpha(int indicator, int alpha);
	int IndicGetAlpha(int indicator);
	void IndicSetOutlineAlpha(int indicator, int alpha);
	int IndicGetOutlineAlpha(int indicator);
	void SetExtraAscent(int extraAscent);
	int ExtraAscent();
	void SetExtraDescent(int extraDescent);
	int ExtraDescent();
	int MarkerSymbolDefined(int markerNumber);
	void MarginSetText(Line line, const char *text);
	int MarginGetText(Line line, char *text);
	void MarginSetStyle(Line line, int style);
	int MarginGetStyle(Line line);
	void MarginSetStyles(Line line, const char *styles);
	int MarginGetStyles(Line line, char *styles);
	void MarginTextClearAll();
	void MarginSetStyleOffset(int style);
	int MarginGetStyleOffset();
	void SetMarginOptions(int marginOptions);
	int MarginOptions();
	void AnnotationSetText(Line line, const char *text);
	int AnnotationGetText(Line line, char *text);
	void AnnotationSetStyle(Line line, int style);
	int AnnotationGetStyle(Line line);
	void AnnotationSetStyles(Line line, const char *styles);
	int AnnotationGetStyles(Line line, char *styles);
	int AnnotationGetLines(Line line);
	void AnnotationClearAll();
	void AnnotationSetVisible(int visible);
	int AnnotationGetVisible();
	void AnnotationSetStyleOffset(int style);
	int AnnotationGetStyleOffset();
	void ReleaseAllExtendedStyles();
	int AllocateExtendedStyles(int numberStyles);
	void AddUndoAction(int token, int flags);
	Position CharPositionFromPoint(int x, int y);
	Position CharPositionFromPointClose(int x, int y);
	void SetMouseSelectionRectangularSwitch(bool mouseSelectionRectangularSwitch);
	bool MouseSelectionRectangularSwitch();
	void SetMultipleSelection(bool multipleSelection);
	bool MultipleSelection();
	void SetAdditionalSelectionTyping(bool additionalSelectionTyping);
	bool AdditionalSelectionTyping();
	void SetAdditionalCaretsBlink(bool additionalCaretsBlink);
	bool AdditionalCaretsBlink();
	void SetAdditionalCaretsVisible(bool additionalCaretsVisible);
	bool AdditionalCaretsVisible();
	int Selections();
	bool SelectionEmpty();
	void ClearSelections();
	void SetSelection(Position caret, Position anchor);
	void AddSelection(Position caret, Position anchor);
	void DropSelectionN(int selection);
	void SetMainSelection(int selection);
	int MainSelection();
	void SetSelectionNCaret(int selection, Position caret);
	Position SelectionNCaret(int selection);
	void SetSelectionNAnchor(int selection, Position anchor);
	Position SelectionNAnchor(int selection);
	void SetSelectionNCaretVirtualSpace(int selection, Position space);
	int SelectionNCaretVirtualSpace(int selection);
	void SetSelectionNAnchorVirtualSpace(int selection, Position space);
	int SelectionNAnchorVirtualSpace(int selection);
	void SetSelectionNStart(int selection, Position anchor);
	Position SelectionNStart(int selection);
	void SetSelectionNEnd(int selection, Position caret);
	Position SelectionNEnd(int selection);
	void SetRectangularSelectionCaret(Position caret);
	Position RectangularSelectionCaret();
	void SetRectangularSelectionAnchor(Position anchor);
	Position RectangularSelectionAnchor();
	void SetRectangularSelectionCaretVirtualSpace(Position space);
	int RectangularSelectionCaretVirtualSpace();
	void SetRectangularSelectionAnchorVirtualSpace(Position space);
	int RectangularSelectionAnchorVirtualSpace();
	void SetVirtualSpaceOptions(int virtualSpaceOptions);
	int VirtualSpaceOptions();
	void SetRectangularSelectionModifier(int modifier);
	int RectangularSelectionModifier();
	void SetAdditionalSelFore(Colour fore);
	void SetAdditionalSelBack(Colour back);
	void SetAdditionalSelAlpha(int alpha);
	int AdditionalSelAlpha();
	void SetAdditionalCaretFore(Colour fore);
	Colour AdditionalCaretFore();
	void RotateSelection();
	void SwapMainAnchorCaret();
	void MultipleSelectAddNext();
	void MultipleSelectAddEach();
	int ChangeLexerState(Position start, Position end);
	Line ContractedFoldNext(Line lineStart);
	void VerticalCentreCaret();
	void MoveSelectedLinesUp();
	void MoveSelectedLinesDown();
	void SetIdentifier(int identifier);
	int Identifier();
	void RGBAImageSetWidth(int width);
	void RGBAImageSetHeight(int height);
	void RGBAImageSetScale(int scalePercent);
	void MarkerDefineRGBAImage(int markerNumber, const char *pixels);
	void RegisterRGBAImage(int type, const char *pixels);
	void ScrollToStart();
	void ScrollToEnd();
	void SetTechnology(int technology);
	int Technology();
	intptr_t CreateLoader(Position bytes, int documentOptions);
	void FindIndicatorShow(Position start, Position end);
	void FindIndicatorFlash(Position start, Position end);
	void FindIndicatorHide();
	void VCHomeDisplay();
	void VCHomeDisplayExtend();
	bool CaretLineVisibleAlways();
	void SetCaretLineVisibleAlways(bool alwaysVisible);
	void SetLineEndTypesAllowed(int lineEndBitSet);
	int LineEndTypesAllowed();
	int LineEndTypesActive();
	void SetRepresentation(const char *encodedCharacter, const char *representation);
	int Representation(const char *encodedCharacter, char *representation);
	void ClearRepresentation(const char *encodedCharacter);
	void StartRecord();
	void StopRecord();
	void SetLexer(int lexer);
	int Lexer();
	void Colourise(Position start, Position end);
	void SetProperty(const char *key, const char *value);
	void SetKeyWords(int keyWordSet, const char *keyWords);
	void SetLexerLanguage(const char *language);
	void LoadLexerLibrary(const char *path);
	int Property(const char *key, char *value);
	int PropertyExpanded(const char *key, char *value);
	int PropertyInt(const char *key, int defaultValue);
	int LexerLanguage(char *language);
	intptr_t PrivateLexerCall(int operation, intptr_t pointer);
	int PropertyNames(char *names);
	int PropertyType(const char *name);
	int DescribeProperty(const char *name, char *description);
	int DescribeKeyWordSets(char *descriptions);
	int LineEndTypesSupported();
	int AllocateSubStyles(int styleBase, int numberStyles);
	int SubStylesStart(int styleBase);
	int SubStylesLength(int styleBase);
	int StyleFromSubStyle(int subStyle);
	int PrimaryStyleFromStyle(int style);
	void FreeSubStyles();
	void SetIdentifiers(int style, const char *identifiers);
	int DistanceToSecondaryStyles();
	int SubStyleBases(char *styles);
	int NamedStyles();
	int NameOfStyle(int style, char *name);
	int TagsOfStyle(int style, char *tags);
	int DescriptionOfStyle(int style, char *description);
	int Bidirectional();
	void SetBidirectional(int bidirectional);
	int LineCharacterIndex();
	void AllocateLineCharacterIndex(int lineCharacterIndex);
	void ReleaseLineCharacterIndex(int lineCharacterIndex);
	Line LineFromIndexPosition(Position pos, int lineCharacterIndex);
	Position IndexPositionFromLine(Line line, int lineCharacterIndex);

//--Autogenerated -- end of section automatically generated from Scintilla.iface

};

}

#endif
