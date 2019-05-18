// SciTE - Scintilla based Text Editor
/** @file SciTEProps.cxx
 ** Properties management.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <clocale>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <memory>

#include <fcntl.h>

#include "ILexer.h"

#include "ScintillaTypes.h"
#include "ScintillaMessages.h"
#include "ScintillaCall.h"

#include "Scintilla.h"
#include "SciLexer.h"

#include "GUI.h"
#include "ScintillaWindow.h"

#if defined(__unix__) || defined(__APPLE__)

const GUI::gui_char menuAccessIndicator[] = GUI_TEXT("_");

#else

const GUI::gui_char menuAccessIndicator[] = GUI_TEXT("&");

#endif

#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "StyleDefinition.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "MatchMarker.h"
#include "EditorConfig.h"
#include "SciTEBase.h"
#include "IFaceTable.h"

void SciTEBase::SetImportMenu() {
	for (int i = 0; i < importMax; i++) {
		DestroyMenuItem(menuOptions, importCmdID + i);
	}
	if (!importFiles.empty()) {
		for (int stackPos = 0; stackPos < static_cast<int>(importFiles.size()) && stackPos < importMax; stackPos++) {
			const int itemID = importCmdID + stackPos;
			if (importFiles[stackPos].IsSet()) {
				GUI::gui_string entry = localiser.Text("Open");
				entry += GUI_TEXT(" ");
				entry += importFiles[stackPos].Name().AsInternal();
				SetMenuItem(menuOptions, IMPORT_START + stackPos, itemID, entry.c_str());
			}
		}
	}
}

void SciTEBase::ImportMenu(int pos) {
	if (pos >= 0) {
		if (importFiles[pos].IsSet()) {
			Open(importFiles[pos]);
		}
	}
}

void SciTEBase::SetLanguageMenu() {
	for (int i = 0; i < 100; i++) {
		DestroyMenuItem(menuLanguage, languageCmdID + i);
	}
	for (unsigned int item = 0; item < languageMenu.size(); item++) {
		const int itemID = languageCmdID + item;
		GUI::gui_string entry = localiser.Text(languageMenu[item].menuItem.c_str());
		if (languageMenu[item].menuKey.length()) {
#if defined(GTK)
			entry += GUI_TEXT(" ");
#else
			entry += GUI_TEXT("\t");
#endif
			entry += GUI::StringFromUTF8(languageMenu[item].menuKey);
		}
		if (entry.size() && entry[0] != '#') {
			SetMenuItem(menuLanguage, item, itemID, entry.c_str());
		}
	}
}

// Null except on Windows where it may be overridden
void SciTEBase::ReadEmbeddedProperties() {
}

const GUI::gui_char propLocalFileName[] = GUI_TEXT("SciTE.properties");
const GUI::gui_char propDirectoryFileName[] = GUI_TEXT("SciTEDirectory.properties");

void SciTEBase::ReadEnvironment() {
#if defined(__unix__) || defined(__APPLE__)
	extern char **environ;
	char **e = environ;
#else
	char **e = _environ;
#endif
	for (; e && *e; e++) {
		char key[1024];
		char *k = *e;
		char *v = strchr(k, '=');
		if (v && (static_cast<size_t>(v - k) < sizeof(key))) {
			memcpy(key, k, v - k);
			key[v - k] = '\0';
			propsPlatform.Set(key, v + 1);
		}
	}
}

/**
Read global and user properties files.
*/
void SciTEBase::ReadGlobalPropFile() {
	std::string excludes;
	std::string includes;

	// Want to apply imports.exclude and imports.include but these may well be in
	// user properties.

	for (int attempt=0; attempt<2; attempt++) {

		std::string excludesRead = props.GetString("imports.exclude");
		std::string includesRead = props.GetString("imports.include");
		if ((attempt > 0) && ((excludesRead == excludes) && (includesRead == includes)))
			break;

		excludes = excludesRead;
		includes = includesRead;

		filter.SetFilter(excludes.c_str(), includes.c_str());

		importFiles.clear();

		ReadEmbeddedProperties();

		propsBase.Clear();
		FilePath propfileBase = GetDefaultPropertiesFileName();
		propsBase.Read(propfileBase, propfileBase.Directory(), filter, &importFiles, 0);

		propsUser.Clear();
		FilePath propfileUser = GetUserPropertiesFileName();
		propsUser.Read(propfileUser, propfileUser.Directory(), filter, &importFiles, 0);
	}

	if (!localiser.read) {
		ReadLocalization();
	}
}

void SciTEBase::ReadAbbrevPropFile() {
	propsAbbrev.Clear();
	propsAbbrev.Read(pathAbbreviations, pathAbbreviations.Directory(), filter, &importFiles, 0);
}

/**
Reads the directory properties file depending on the variable
"properties.directory.enable". Also sets the variable $(SciteDirectoryHome) to the path
where this property file is found. If it is not found $(SciteDirectoryHome) will
be set to $(FilePath).
*/
void SciTEBase::ReadDirectoryPropFile() {
	propsDirectory.Clear();

	if (props.GetInt("properties.directory.enable") != 0) {
		FilePath propfile = GetDirectoryPropertiesFileName();
		props.Set("SciteDirectoryHome", propfile.Directory().AsUTF8().c_str());

		propsDirectory.Read(propfile, propfile.Directory(), filter, nullptr, 0);
	}
}

/**
Read local and directory properties file.
*/
void SciTEBase::ReadLocalPropFile() {
	// The directory properties acts like a base local properties file.
	// Therefore it must be read always before reading the local prop file.
	ReadDirectoryPropFile();

	FilePath propfile = GetLocalPropertiesFileName();

	propsLocal.Clear();
	propsLocal.Read(propfile, propfile.Directory(), filter, nullptr, 0);

	props.Set("Chrome", "#C0C0C0");
	props.Set("ChromeHighlight", "#FFFFFF");

	FilePath fileDirectory = filePath.Directory();
	editorConfig->Clear();
	if (props.GetInt("editor.config.enable", 0)) {
		editorConfig->ReadFromDirectory(fileDirectory);
	}
}

SA::Colour ColourOfProperty(const PropSetFile &props, const char *key, SA::Colour colourDefault) {
	std::string colour = props.GetExpandedString(key);
	if (colour.length()) {
		return ColourFromString(colour);
	}
	return colourDefault;
}

/**
 * Put the next property item from the given property string
 * into the buffer pointed by @a pPropItem.
 * @return NULL if the end of the list is met, else, it points to the next item.
 */
const char *SciTEBase::GetNextPropItem(
	const char *pStart,	/**< the property string to parse for the first call,
						 * pointer returned by the previous call for the following. */
	char *pPropItem,	///< pointer on a buffer receiving the requested prop item
	int maxLen)			///< size of the above buffer
{
	ptrdiff_t size = maxLen - 1;

	*pPropItem = '\0';
	if (!pStart) {
		return nullptr;
	}
	const char *pNext = strchr(pStart, ',');
	if (pNext) {	// Separator is found
		if (size > pNext - pStart) {
			// Found string fits in buffer
			size = pNext - pStart;
		}
		pNext++;
	}
	strncpy(pPropItem, pStart, size);
	pPropItem[size] = '\0';
	return pNext;
}

std::string SciTEBase::StyleString(const char *lang, int style) const {
	char key[200];
	sprintf(key, "style.%s.%0d", lang, style);
	return props.GetExpandedString(key);
}

StyleDefinition SciTEBase::StyleDefinitionFor(int style) {
	const std::string languageName = !StartsWith(language, "lpeg_") ? language : "lpeg";

	const std::string ssDefault = StyleString("*", style);
	std::string ss = StyleString(languageName.c_str(), style);

	if (!subStyleBases.empty()) {
		const int baseStyle = wEditor.StyleFromSubStyle(style);
		if (baseStyle != style) {
			const int primaryStyle = wEditor.PrimaryStyleFromStyle(style);
			const int distanceSecondary = (style == primaryStyle) ? 0 : wEditor.DistanceToSecondaryStyles();
			const int primaryBase = baseStyle - distanceSecondary;
			const int subStylesStart = wEditor.SubStylesStart(primaryBase);
			const int subStylesLength = wEditor.SubStylesLength(primaryBase);
			const int subStyle = style - (subStylesStart + distanceSecondary);
			if (subStyle < subStylesLength) {
				char key[200];
				sprintf(key, "style.%s.%0d.%0d", languageName.c_str(), baseStyle, subStyle + 1);
				ss = props.GetNewExpandString(key);
			}
		}
	}

	StyleDefinition sd(ssDefault);
	sd.ParseStyleDefinition(ss);
	return sd;
}

void SciTEBase::SetOneStyle(GUI::ScintillaWindow &win, int style, const StyleDefinition &sd) {
	if (sd.specified & StyleDefinition::sdItalics)
		win.StyleSetItalic(style, sd.italics ? 1 : 0);
	if (sd.specified & StyleDefinition::sdWeight)
		win.StyleSetWeight(style, sd.weight);
	if (sd.specified & StyleDefinition::sdFont)
		win.StyleSetFont(style,
			sd.font.c_str());
	if (sd.specified & StyleDefinition::sdFore)
		win.StyleSetFore(style, sd.Fore());
	if (sd.specified & StyleDefinition::sdBack)
		win.StyleSetBack(style, sd.Back());
	if (sd.specified & StyleDefinition::sdSize)
		win.StyleSetSizeFractional(style, sd.FractionalSize());
	if (sd.specified & StyleDefinition::sdEOLFilled)
		win.StyleSetEOLFilled(style, sd.eolfilled ? 1 : 0);
	if (sd.specified & StyleDefinition::sdUnderlined)
		win.StyleSetUnderline(style, sd.underlined ? 1 : 0);
	if (sd.specified & StyleDefinition::sdCaseForce)
		win.StyleSetCase(style, sd.caseForce);
	if (sd.specified & StyleDefinition::sdVisible)
		win.StyleSetVisible(style, sd.visible ? 1 : 0);
	if (sd.specified & StyleDefinition::sdChangeable)
		win.StyleSetChangeable(style, sd.changeable ? 1 : 0);
	win.StyleSetCharacterSet(style, characterSet);
}

void SciTEBase::SetStyleBlock(GUI::ScintillaWindow &win, const char *lang, int start, int last) {
	for (int style = start; style <= last; style++) {
		if (style != STYLE_DEFAULT) {
			char key[200];
			sprintf(key, "style.%s.%0d", lang, style-start);
			std::string sval = props.GetExpandedString(key);
			if (sval.length()) {
				SetOneStyle(win, style, StyleDefinition(sval));
			}
		}
	}
}

void SciTEBase::SetStyleFor(GUI::ScintillaWindow &win, const char *lang) {
	SetStyleBlock(win, lang, 0, STYLE_MAX);
}

void SciTEBase::SetOneIndicator(GUI::ScintillaWindow &win, int indicator, const IndicatorDefinition &ind) {
	win.IndicSetStyle(indicator, ind.style);
	win.IndicSetFore(indicator, ind.colour);
	win.IndicSetAlpha(indicator, ind.fillAlpha);
	win.IndicSetOutlineAlpha(indicator, ind.outlineAlpha);
	win.IndicSetUnder(indicator, ind.under);
}

std::string SciTEBase::ExtensionFileName() const {
	if (CurrentBufferConst()->overrideExtension.length()) {
		return CurrentBufferConst()->overrideExtension;
	} else {
		FilePath name = FileNameExt();
		if (name.IsSet()) {
#if !defined(GTK)
			// Force extension to lower case
			std::string extension = name.Extension().AsUTF8();
			if (extension.empty()) {
				return name.AsUTF8();
			} else {
				LowerCaseAZ(extension);
				return name.BaseName().AsUTF8() + "." + extension;
			}
#else
			return name.AsUTF8();
#endif
		} else {
			return props.GetString("default.file.ext");
		}
	}
}

void SciTEBase::ForwardPropertyToEditor(const char *key) {
	if (props.Exists(key)) {
		std::string value = props.GetExpandedString(key);
		wEditor.SetProperty(
						 key, value.c_str());
		wOutput.SetProperty(
						 key, value.c_str());
	}
}

void SciTEBase::DefineMarker(int marker, int markerType, SA::Colour fore, SA::Colour back, SA::Colour backSelected) {
	wEditor.MarkerDefine(marker, markerType);
	wEditor.MarkerSetFore(marker, fore);
	wEditor.MarkerSetBack(marker, back);
	wEditor.MarkerSetBackSelected(marker, backSelected);
}

void SciTEBase::ReadAPI(const std::string &fileNameForExtension) {
	std::string sApiFileNames = props.GetNewExpandString("api.",
	                        fileNameForExtension.c_str());
	if (sApiFileNames.length() > 0) {
		std::vector<std::string> vApiFileNames = StringSplit(sApiFileNames, ';');
		std::vector<char> data;

		// Load files into data
		for (const std::string &vApiFileName : vApiFileNames) {
			std::string contents = FilePath(GUI::StringFromUTF8(vApiFileName)).Read();
			data.insert(data.end(), contents.begin(), contents.end());
		}

		// Initialise apis
		if (data.size() > 0) {
			apis.Set(data);
		}
	}
}

std::string SciTEBase::FindLanguageProperty(const char *pattern, const char *defaultValue) {
	std::string key = pattern;
	Substitute(key, "*", language.c_str());
	std::string ret = props.GetExpandedString(key.c_str());
	if (ret == "")
		ret = props.GetExpandedString(pattern);
	if (ret == "")
		ret = defaultValue;
	return ret;
}

/**
 * A list of all the properties that should be forwarded to Scintilla lexers.
 */
static const char *propertiesToForward[] = {
	"fold.lpeg.by.indentation",
	"lexer.lpeg.color.theme",
	"lexer.lpeg.home",
	"lexer.lpeg.script",
//++Autogenerated -- run ../scripts/RegenerateSource.py to regenerate
//**\(\t"\*",\n\)
	"asp.default.language",
	"fold",
	"fold.abl.comment.multiline",
	"fold.abl.syntax.based",
	"fold.asm.comment.explicit",
	"fold.asm.comment.multiline",
	"fold.asm.explicit.anywhere",
	"fold.asm.explicit.end",
	"fold.asm.explicit.start",
	"fold.asm.syntax.based",
	"fold.at.else",
	"fold.baan.inner.level",
	"fold.baan.keywords.based",
	"fold.baan.sections",
	"fold.baan.syntax.based",
	"fold.basic.comment.explicit",
	"fold.basic.explicit.anywhere",
	"fold.basic.explicit.end",
	"fold.basic.explicit.start",
	"fold.basic.syntax.based",
	"fold.cil.comment.multiline",
	"fold.coffeescript.comment",
	"fold.comment",
	"fold.comment.nimrod",
	"fold.comment.yaml",
	"fold.compact",
	"fold.cpp.comment.explicit",
	"fold.cpp.comment.multiline",
	"fold.cpp.explicit.anywhere",
	"fold.cpp.explicit.end",
	"fold.cpp.explicit.start",
	"fold.cpp.preprocessor.at.else",
	"fold.cpp.syntax.based",
	"fold.d.comment.explicit",
	"fold.d.comment.multiline",
	"fold.d.explicit.anywhere",
	"fold.d.explicit.end",
	"fold.d.explicit.start",
	"fold.d.syntax.based",
	"fold.directive",
	"fold.haskell.imports",
	"fold.html",
	"fold.html.preprocessor",
	"fold.hypertext.comment",
	"fold.hypertext.heredoc",
	"fold.perl.at.else",
	"fold.perl.comment.explicit",
	"fold.perl.package",
	"fold.perl.pod",
	"fold.preprocessor",
	"fold.quotes.nimrod",
	"fold.quotes.python",
	"fold.rust.comment.explicit",
	"fold.rust.comment.multiline",
	"fold.rust.explicit.anywhere",
	"fold.rust.explicit.end",
	"fold.rust.explicit.start",
	"fold.rust.syntax.based",
	"fold.sql.at.else",
	"fold.sql.only.begin",
	"fold.verilog.flags",
	"html.tags.case.sensitive",
	"lexer.asm.comment.delimiter",
	"lexer.baan.styling.within.preprocessor",
	"lexer.caml.magic",
	"lexer.cpp.allow.dollars",
	"lexer.cpp.backquoted.strings",
	"lexer.cpp.escape.sequence",
	"lexer.cpp.hashquoted.strings",
	"lexer.cpp.track.preprocessor",
	"lexer.cpp.triplequoted.strings",
	"lexer.cpp.update.preprocessor",
	"lexer.cpp.verbatim.strings.allow.escapes",
	"lexer.css.hss.language",
	"lexer.css.less.language",
	"lexer.css.scss.language",
	"lexer.d.fold.at.else",
	"lexer.edifact.highlight.un.all",
	"lexer.errorlist.escape.sequences",
	"lexer.errorlist.value.separate",
	"lexer.flagship.styling.within.preprocessor",
	"lexer.haskell.allow.hash",
	"lexer.haskell.allow.questionmark",
	"lexer.haskell.allow.quotes",
	"lexer.haskell.cpp",
	"lexer.haskell.import.safe",
	"lexer.html.django",
	"lexer.html.mako",
	"lexer.json.allow.comments",
	"lexer.json.escape.sequence",
	"lexer.metapost.comment.process",
	"lexer.metapost.interface.default",
	"lexer.nim.raw.strings.highlight.ident",
	"lexer.pascal.smart.highlighting",
	"lexer.props.allow.initial.spaces",
	"lexer.python.keywords2.no.sub.identifiers",
	"lexer.python.literals.binary",
	"lexer.python.strings.b",
	"lexer.python.strings.f",
	"lexer.python.strings.over.newline",
	"lexer.python.strings.u",
	"lexer.python.unicode.identifiers",
	"lexer.rust.fold.at.else",
	"lexer.sql.allow.dotted.word",
	"lexer.sql.backticks.identifier",
	"lexer.sql.numbersign.comment",
	"lexer.tex.auto.if",
	"lexer.tex.comment.process",
	"lexer.tex.interface.default",
	"lexer.tex.use.keywords",
	"lexer.verilog.allupperkeywords",
	"lexer.verilog.fold.preprocessor.else",
	"lexer.verilog.portstyling",
	"lexer.verilog.track.preprocessor",
	"lexer.verilog.update.preprocessor",
	"lexer.xml.allow.scripts",
	"nsis.ignorecase",
	"nsis.uservars",
	"ps.level",
	"sql.backslash.escapes",
	"styling.within.preprocessor",
	"tab.timmy.whinge.level",

//--Autogenerated -- end of automatically generated section

	nullptr,
};

/* XPM */
static const char *bookmarkBluegem[] = {
/* width height num_colors chars_per_pixel */
"    15    15      64            1",
/* colors */
"  c none",
". c #0c0630",
"# c #8c8a8c",
"a c #244a84",
"b c #545254",
"c c #cccecc",
"d c #949594",
"e c #346ab4",
"f c #242644",
"g c #3c3e3c",
"h c #6ca6fc",
"i c #143789",
"j c #204990",
"k c #5c8dec",
"l c #707070",
"m c #3c82dc",
"n c #345db4",
"o c #619df7",
"p c #acacac",
"q c #346ad4",
"r c #1c3264",
"s c #174091",
"t c #5482df",
"u c #4470c4",
"v c #2450a0",
"w c #14162c",
"x c #5c94f6",
"y c #b7b8b7",
"z c #646464",
"A c #3c68b8",
"B c #7cb8fc",
"C c #7c7a7c",
"D c #3462b9",
"E c #7c7eac",
"F c #44464c",
"G c #a4a4a4",
"H c #24224c",
"I c #282668",
"J c #5c5a8c",
"K c #7c8ebc",
"L c #dcd7e4",
"M c #141244",
"N c #1c2e5c",
"O c #24327c",
"P c #4472cc",
"Q c #6ca2fc",
"R c #74b2fc",
"S c #24367c",
"T c #b4b2c4",
"U c #403e58",
"V c #4c7fd6",
"W c #24428c",
"X c #747284",
"Y c #142e7c",
"Z c #64a2fc",
"0 c #3c72dc",
"1 c #bcbebc",
"2 c #6c6a6c",
"3 c #848284",
"4 c #2c5098",
"5 c #1c1a1c",
"6 c #243250",
"7 c #7cbefc",
"8 c #d4d2d4",
/* pixels */
"    yCbgbCy    ",
"   #zGGyGGz#   ",
"  #zXTLLLTXz#  ",
" p5UJEKKKEJU5p ",
" lfISa444aSIfl ",
" wIYij444jsYIw ",
" .OsvnAAAnvsO. ",
" MWvDuVVVPDvWM ",
" HsDPVkxxtPDsH ",
" UiAtxohZxtuiU ",
" pNnkQRBRhkDNp ",
" 1FrqoR7Bo0rF1 ",
" 8GC6aemea6CG8 ",
"  cG3l2z2l3Gc  ",
"    1GdddG1    "
};

std::string SciTEBase::GetFileNameProperty(const char *name) {
	std::string namePlusDot = name;
	namePlusDot.append(".");
	std::string valueForFileName = props.GetNewExpandString(namePlusDot.c_str(),
	        ExtensionFileName().c_str());
	if (valueForFileName.length() != 0) {
		return valueForFileName;
	} else {
		return props.GetString(name);
	}
}

void SciTEBase::ReadProperties() {
	if (extender)
		extender->Clear();

	const std::string fileNameForExtension = ExtensionFileName();

	std::string modulePath = props.GetNewExpandString("lexerpath.",
	    fileNameForExtension.c_str());
	if (modulePath.length())
	    wEditor.LoadLexerLibrary(modulePath.c_str());
	language = props.GetNewExpandString("lexer.", fileNameForExtension.c_str());
	if (wEditor.DocumentOptions() & SC_DOCUMENTOPTION_STYLES_NONE) {
		language = "";
	}
	if (language.length()) {
		if (StartsWith(language, "script_")) {
			wEditor.SetLexer(SCLEX_CONTAINER);
		} else if (StartsWith(language, "lpeg_")) {
			modulePath = props.GetNewExpandString("lexerpath.*.lpeg");
			if (modulePath.length()) {
				wEditor.LoadLexerLibrary(modulePath.c_str());
				wEditor.SetLexerLanguage("lpeg");
				lexLPeg = wEditor.Lexer();
				const char *lexer = language.c_str() + language.find('_') + 1;
				wEditor.PrivateLexerCall(SCI_SETLEXERLANGUAGE,
					SptrFromString(lexer));
			}
		} else {
			wEditor.SetLexerLanguage(language.c_str());
		}
	} else {
		wEditor.SetLexer(SCLEX_NULL);
	}

	props.Set("Language", language.c_str());

	lexLanguage = wEditor.Lexer();

	wOutput.SetLexer(SCLEX_ERRORLIST);

	const std::string kw0 = props.GetNewExpandString("keywords.", fileNameForExtension.c_str());
	wEditor.SetKeyWords(0, kw0.c_str());

	for (int wl = 1; wl <= KEYWORDSET_MAX; wl++) {
		std::string kwk = StdStringFromInteger(wl+1);
		kwk += '.';
		kwk.insert(0, "keywords");
		const std::string kw = props.GetNewExpandString(kwk.c_str(), fileNameForExtension.c_str());
		wEditor.SetKeyWords(wl, kw.c_str());
	}

	subStyleBases.clear();
	const int lenSSB = wEditor.SubStyleBases(nullptr);
	if (lenSSB) {
		wEditor.FreeSubStyles();

		subStyleBases.resize(lenSSB+1);
		wEditor.SubStyleBases(&subStyleBases[0]);
		subStyleBases.resize(lenSSB);	// Remove NUL

		for (int baseStyle=0;baseStyle<lenSSB;baseStyle++) {
			//substyles.cpp.11=2
			std::string ssSubStylesKey = "substyles.";
			ssSubStylesKey += language;
			ssSubStylesKey += ".";
			ssSubStylesKey += StdStringFromInteger(subStyleBases[baseStyle]);
			std::string ssNumber = props.GetNewExpandString(ssSubStylesKey.c_str());
			int subStyleIdentifiers = atoi(ssNumber.c_str());

			int subStyleIdentifiersStart = 0;
			if (subStyleIdentifiers) {
				subStyleIdentifiersStart = wEditor.AllocateSubStyles(subStyleBases[baseStyle], subStyleIdentifiers);
				if (subStyleIdentifiersStart < 0)
					subStyleIdentifiers = 0;
			}
			for (int subStyle=0; subStyle<subStyleIdentifiers; subStyle++) {
				// substylewords.11.1.$(file.patterns.cpp)=CharacterSet LexAccessor SString WordList
				std::string ssWordsKey = "substylewords.";
				ssWordsKey += StdStringFromInteger(subStyleBases[baseStyle]);
				ssWordsKey += ".";
				ssWordsKey += StdStringFromInteger(subStyle + 1);
				ssWordsKey += ".";
				std::string ssWords = props.GetNewExpandString(ssWordsKey.c_str(), fileNameForExtension.c_str());
				wEditor.SetIdentifiers(subStyleIdentifiersStart + subStyle, ssWords.c_str());
			}
		}
	}

	FilePath homepath = GetSciteDefaultHome();
	props.Set("SciteDefaultHome", homepath.AsUTF8().c_str());
	homepath = GetSciteUserHome();
	props.Set("SciteUserHome", homepath.AsUTF8().c_str());

	for (size_t i=0; propertiesToForward[i]; i++) {
		ForwardPropertyToEditor(propertiesToForward[i]);
	}

	if (apisFileNames != props.GetNewExpandString("api.", fileNameForExtension.c_str())) {
		apis.Clear();
		ReadAPI(fileNameForExtension);
		apisFileNames = props.GetNewExpandString("api.", fileNameForExtension.c_str());
	}

	props.Set("APIPath", apisFileNames.c_str());

	FilePath fileAbbrev = GUI::StringFromUTF8(props.GetNewExpandString("abbreviations.", fileNameForExtension.c_str()));
	if (!fileAbbrev.IsSet())
		fileAbbrev = GetAbbrevPropertiesFileName();
	if (!pathAbbreviations.SameNameAs(fileAbbrev)) {
		pathAbbreviations = fileAbbrev;
		ReadAbbrevPropFile();
	}

	props.Set("AbbrevPath", pathAbbreviations.AsUTF8().c_str());

	const int tech = props.GetInt("technology");
	wEditor.SetTechnology(tech);
	wOutput.SetTechnology(tech);

	const int bidirectional = props.GetInt("bidirectional");
	wEditor.SetBidirectional(bidirectional);
	wOutput.SetBidirectional(bidirectional);

	codePage = props.GetInt("code.page");
	if (CurrentBuffer()->unicodeMode != uni8Bit) {
		// Override properties file to ensure Unicode displayed.
		codePage = SC_CP_UTF8;
	}
	wEditor.SetCodePage(codePage);
	const int outputCodePage = props.GetInt("output.code.page", codePage);
	wOutput.SetCodePage(outputCodePage);

	characterSet = props.GetInt("character.set", SC_CHARSET_DEFAULT);

#if defined(__unix__) || defined(__APPLE__)
	const std::string localeCType = props.GetString("LC_CTYPE");
	if (localeCType.length())
		setlocale(LC_CTYPE, localeCType.c_str());
	else
		setlocale(LC_CTYPE, "C");
#endif

	std::string imeInteraction = props.GetString("ime.interaction");
	if (imeInteraction.length()) {
		CallChildren(SCI_SETIMEINTERACTION, props.GetInt("ime.interaction", SC_IME_WINDOWED));
	}
	imeAutoComplete = props.GetInt("ime.autocomplete", 0) == 1;

	const int accessibility = props.GetInt("accessibility", 1);
	wEditor.SetAccessibility(accessibility);
	wOutput.SetAccessibility(accessibility);

	wrapStyle = props.GetInt("wrap.style", SC_WRAP_WORD);

	CallChildren(SCI_SETCARETFORE,
	           ColourOfProperty(props, "caret.fore", ColourRGB(0, 0, 0)));

	CallChildren(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, props.GetInt("selection.rectangular.switch.mouse", 0));
	CallChildren(SCI_SETMULTIPLESELECTION, props.GetInt("selection.multiple", 1));
	CallChildren(SCI_SETADDITIONALSELECTIONTYPING, props.GetInt("selection.additional.typing", 1));
	CallChildren(SCI_SETMULTIPASTE, props.GetInt("selection.multipaste", 1));
	CallChildren(SCI_SETADDITIONALCARETSBLINK, props.GetInt("caret.additional.blinks", 1));
	CallChildren(SCI_SETVIRTUALSPACEOPTIONS, props.GetInt("virtual.space"));

	wEditor.SetMouseDwellTime(
	           props.GetInt("dwell.period", SC_TIME_FOREVER));

	wEditor.SetCaretStyle(props.GetInt("caret.style", CARETSTYLE_LINE));
	wOutput.SetCaretStyle(props.GetInt("caret.style", CARETSTYLE_LINE));
	wEditor.SetCaretWidth(props.GetInt("caret.width", 1));
	wOutput.SetCaretWidth(props.GetInt("caret.width", 1));

	std::string caretLineBack = props.GetExpandedString("caret.line.back");
	if (caretLineBack.length()) {
		wEditor.SetCaretLineVisible(1);
		wEditor.SetCaretLineBack(ColourFromString(caretLineBack));
	} else {
		wEditor.SetCaretLineVisible(0);
	}
	wEditor.SetCaretLineBackAlpha(
		props.GetInt("caret.line.back.alpha", SC_ALPHA_NOALPHA));

	alphaIndicator = props.GetInt("indicators.alpha", 30);
	if (alphaIndicator < 0 || 255 < alphaIndicator) // If invalid value,
		alphaIndicator = 30; //then set default value.
	underIndicator = props.GetInt("indicators.under", 0) == 1;

	closeFind = static_cast<CloseFind>(props.GetInt("find.close.on.find", 1));

	const std::string controlCharSymbol = props.GetString("control.char.symbol");
	if (controlCharSymbol.length()) {
		wEditor.SetControlCharSymbol(static_cast<unsigned char>(controlCharSymbol[0]));
	} else {
		wEditor.SetControlCharSymbol(0);
	}

	const std::string caretPeriod = props.GetString("caret.period");
	if (caretPeriod.length()) {
		wEditor.SetCaretPeriod(atoi(caretPeriod.c_str()));
		wOutput.SetCaretPeriod(atoi(caretPeriod.c_str()));
	}

	int caretSlop = props.GetInt("caret.policy.xslop", 1) ? CARET_SLOP : 0;
	int caretZone = props.GetInt("caret.policy.width", 50);
	int caretStrict = props.GetInt("caret.policy.xstrict") ? CARET_STRICT : 0;
	int caretEven = props.GetInt("caret.policy.xeven", 1) ? CARET_EVEN : 0;
	int caretJumps = props.GetInt("caret.policy.xjumps") ? CARET_JUMPS : 0;
	wEditor.SetXCaretPolicy(caretStrict | caretSlop | caretEven | caretJumps, caretZone);

	caretSlop = props.GetInt("caret.policy.yslop", 1) ? CARET_SLOP : 0;
	caretZone = props.GetInt("caret.policy.lines");
	caretStrict = props.GetInt("caret.policy.ystrict") ? CARET_STRICT : 0;
	caretEven = props.GetInt("caret.policy.yeven", 1) ? CARET_EVEN : 0;
	caretJumps = props.GetInt("caret.policy.yjumps") ? CARET_JUMPS : 0;
	wEditor.SetYCaretPolicy(caretStrict | caretSlop | caretEven | caretJumps, caretZone);

	const int visibleStrict = props.GetInt("visible.policy.strict") ? VISIBLE_STRICT : 0;
	const int visibleSlop = props.GetInt("visible.policy.slop", 1) ? VISIBLE_SLOP : 0;
	const int visibleLines = props.GetInt("visible.policy.lines");
	wEditor.SetVisiblePolicy(visibleStrict | visibleSlop, visibleLines);

	wEditor.SetEdgeColumn(props.GetInt("edge.column", 0));
	wEditor.SetEdgeMode(props.GetInt("edge.mode", EDGE_NONE));
	wEditor.SetEdgeColour(
	           ColourOfProperty(props, "edge.colour", ColourRGB(0xff, 0xda, 0xda)));

	std::string selFore = props.GetExpandedString("selection.fore");
	if (selFore.length()) {
		CallChildren(SCI_SETSELFORE, 1, ColourFromString(selFore));
	} else {
		CallChildren(SCI_SETSELFORE, 0, 0);
	}
	std::string selBack = props.GetExpandedString("selection.back");
	if (selBack.length()) {
		CallChildren(SCI_SETSELBACK, 1, ColourFromString(selBack));
	} else {
		if (selFore.length())
			CallChildren(SCI_SETSELBACK, 0, 0);
		else	// Have to show selection somehow
			CallChildren(SCI_SETSELBACK, 1, ColourRGB(0xC0, 0xC0, 0xC0));
	}
	const int selectionAlpha = props.GetInt("selection.alpha", SC_ALPHA_NOALPHA);
	CallChildren(SCI_SETSELALPHA, selectionAlpha);

	std::string selAdditionalFore = props.GetString("selection.additional.fore");
	if (selAdditionalFore.length()) {
		CallChildren(SCI_SETADDITIONALSELFORE, ColourFromString(selAdditionalFore));
	}
	std::string selAdditionalBack = props.GetString("selection.additional.back");
	if (selAdditionalBack.length()) {
		CallChildren(SCI_SETADDITIONALSELBACK, ColourFromString(selAdditionalBack));
	}
	const int selectionAdditionalAlpha = (selectionAlpha == SC_ALPHA_NOALPHA) ? SC_ALPHA_NOALPHA : selectionAlpha / 2;
	CallChildren(SCI_SETADDITIONALSELALPHA, props.GetInt("selection.additional.alpha", selectionAdditionalAlpha));

	foldColour = props.GetExpandedString("fold.margin.colour");
	if (foldColour.length()) {
		CallChildren(SCI_SETFOLDMARGINCOLOUR, 1, ColourFromString(foldColour));
	} else {
		CallChildren(SCI_SETFOLDMARGINCOLOUR, 0, 0);
	}
	foldHiliteColour = props.GetExpandedString("fold.margin.highlight.colour");
	if (foldHiliteColour.length()) {
		CallChildren(SCI_SETFOLDMARGINHICOLOUR, 1, ColourFromString(foldHiliteColour));
	} else {
		CallChildren(SCI_SETFOLDMARGINHICOLOUR, 0, 0);
	}

	std::string whitespaceFore = props.GetExpandedString("whitespace.fore");
	if (whitespaceFore.length()) {
		CallChildren(SCI_SETWHITESPACEFORE, 1, ColourFromString(whitespaceFore));
	} else {
		CallChildren(SCI_SETWHITESPACEFORE, 0, 0);
	}
	std::string whitespaceBack = props.GetExpandedString("whitespace.back");
	if (whitespaceBack.length()) {
		CallChildren(SCI_SETWHITESPACEBACK, 1, ColourFromString(whitespaceBack));
	} else {
		CallChildren(SCI_SETWHITESPACEBACK, 0, 0);
	}

	char bracesStyleKey[200];
	sprintf(bracesStyleKey, "braces.%s.style", language.c_str());
	bracesStyle = props.GetInt(bracesStyleKey, 0);

	char key[200];
	std::string sval;

	sval = FindLanguageProperty("calltip.*.ignorecase");
	callTipIgnoreCase = sval == "1";
	sval = FindLanguageProperty("calltip.*.use.escapes");
	callTipUseEscapes = sval == "1";

	calltipWordCharacters = FindLanguageProperty("calltip.*.word.characters",
		"_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	calltipParametersStart = FindLanguageProperty("calltip.*.parameters.start", "(");
	calltipParametersEnd = FindLanguageProperty("calltip.*.parameters.end", ")");
	calltipParametersSeparators = FindLanguageProperty("calltip.*.parameters.separators", ",;");

	calltipEndDefinition = FindLanguageProperty("calltip.*.end.definition");

	sprintf(key, "autocomplete.%s.start.characters", language.c_str());
	autoCompleteStartCharacters = props.GetExpandedString(key);
	if (autoCompleteStartCharacters == "")
		autoCompleteStartCharacters = props.GetExpandedString("autocomplete.*.start.characters");
	// "" is a quite reasonable value for this setting

	sprintf(key, "autocomplete.%s.fillups", language.c_str());
	autoCompleteFillUpCharacters = props.GetExpandedString(key);
	if (autoCompleteFillUpCharacters == "")
		autoCompleteFillUpCharacters =
			props.GetExpandedString("autocomplete.*.fillups");
	wEditor.AutoCSetFillUps(
		autoCompleteFillUpCharacters.c_str());

	sprintf(key, "autocomplete.%s.typesep", language.c_str());
	autoCompleteTypeSeparator = props.GetExpandedString(key);
	if (autoCompleteTypeSeparator == "")
		autoCompleteTypeSeparator =
			props.GetExpandedString("autocomplete.*.typesep");
	if (autoCompleteTypeSeparator.length()) {
		wEditor.AutoCSetTypeSeparator(
			static_cast<unsigned char>(autoCompleteTypeSeparator[0]));
	}

	sprintf(key, "autocomplete.%s.ignorecase", "*");
	sval = props.GetNewExpandString(key);
	autoCompleteIgnoreCase = sval == "1";
	sprintf(key, "autocomplete.%s.ignorecase", language.c_str());
	sval = props.GetNewExpandString(key);
	if (sval != "")
		autoCompleteIgnoreCase = sval == "1";
	wEditor.AutoCSetIgnoreCase(autoCompleteIgnoreCase ? 1 : 0);
	wOutput.AutoCSetIgnoreCase(1);

	const int autoCChooseSingle = props.GetInt("autocomplete.choose.single");
	wEditor.AutoCSetChooseSingle(autoCChooseSingle);

	wEditor.AutoCSetCancelAtStart(0);
	wEditor.AutoCSetDropRestOfWord(0);

	if (firstPropertiesRead) {
		ReadPropertiesInitial();
	}

	ReadFontProperties();

	wEditor.SetPrintMagnification(props.GetInt("print.magnification"));
	wEditor.SetPrintColourMode(props.GetInt("print.colour.mode"));

	jobQueue.clearBeforeExecute = props.GetInt("clear.before.execute");
	jobQueue.timeCommands = props.GetInt("time.commands");

	const int blankMarginLeft = props.GetInt("blank.margin.left", 1);
	const int blankMarginLeftOutput = props.GetInt("output.blank.margin.left", blankMarginLeft);
	const int blankMarginRight = props.GetInt("blank.margin.right", 1);
	wEditor.SetMarginLeft(blankMarginLeft);
	wEditor.SetMarginRight(blankMarginRight);
	wOutput.SetMarginLeft(blankMarginLeftOutput);
	wOutput.SetMarginRight(blankMarginRight);

	marginWidth = props.GetInt("margin.width");
	if (marginWidth == 0)
		marginWidth = marginWidthDefault;
	wEditor.SetMarginWidthN(1, margin ? marginWidth : 0);

	const std::string lineMarginProp = props.GetString("line.margin.width");
	lineNumbersWidth = atoi(lineMarginProp.c_str());
	if (lineNumbersWidth == 0)
		lineNumbersWidth = lineNumbersWidthDefault;
	lineNumbersExpand = lineMarginProp.find('+') != std::string::npos;

	SetLineNumberWidth();

	bufferedDraw = props.GetInt("buffered.draw");
	wEditor.SetBufferedDraw(bufferedDraw);
	wOutput.SetBufferedDraw(bufferedDraw);

	const int phasesDraw = props.GetInt("phases.draw", 1);
	wEditor.SetPhasesDraw(phasesDraw);
	wOutput.SetPhasesDraw(phasesDraw);

	wEditor.SetLayoutCache(props.GetInt("cache.layout", SC_CACHE_CARET));
	wOutput.SetLayoutCache(props.GetInt("output.cache.layout", SC_CACHE_CARET));

	bracesCheck = props.GetInt("braces.check");
	bracesSloppy = props.GetInt("braces.sloppy");

	wEditor.SetCharsDefault();
	wordCharacters = props.GetNewExpandString("word.characters.", fileNameForExtension.c_str());
	if (wordCharacters.length()) {
		wEditor.SetWordChars(wordCharacters.c_str());
	} else {
		wordCharacters = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	}

	whitespaceCharacters = props.GetNewExpandString("whitespace.characters.", fileNameForExtension.c_str());
	if (whitespaceCharacters.length()) {
		wEditor.SetWhitespaceChars(whitespaceCharacters.c_str());
	}

	const std::string viewIndentExamine = GetFileNameProperty("view.indentation.examine");
	indentExamine = viewIndentExamine.length() ? (atoi(viewIndentExamine.c_str())) : SC_IV_REAL;
	wEditor.SetIndentationGuides(props.GetInt("view.indentation.guides") ?
		indentExamine : SC_IV_NONE);

	wEditor.SetTabIndents(props.GetInt("tab.indents", 1));
	wEditor.SetBackSpaceUnIndents(props.GetInt("backspace.unindents", 1));

	wEditor.CallTipUseStyle(32);

	std::string useStripTrailingSpaces = props.GetNewExpandString("strip.trailing.spaces.", ExtensionFileName().c_str());
	if (useStripTrailingSpaces.length() > 0) {
		stripTrailingSpaces = atoi(useStripTrailingSpaces.c_str()) != 0;
	} else {
		stripTrailingSpaces = props.GetInt("strip.trailing.spaces") != 0;
	}
	ensureFinalLineEnd = props.GetInt("ensure.final.line.end") != 0;
	ensureConsistentLineEnds = props.GetInt("ensure.consistent.line.ends") != 0;

	indentOpening = props.GetInt("indent.opening");
	indentClosing = props.GetInt("indent.closing");
	indentMaintain = atoi(props.GetNewExpandString("indent.maintain.", fileNameForExtension.c_str()).c_str());

	const std::string lookback = props.GetNewExpandString("statement.lookback.", fileNameForExtension.c_str());
	statementLookback = atoi(lookback.c_str());
	statementIndent = GetStyleAndWords("statement.indent.");
	statementEnd = GetStyleAndWords("statement.end.");
	blockStart = GetStyleAndWords("block.start.");
	blockEnd = GetStyleAndWords("block.end.");

	struct PropToPPC {
		const char *propName;
		PreProcKind ppc;
	};
	PropToPPC propToPPC[] = {
		{"preprocessor.start.", ppcStart},
		{"preprocessor.middle.", ppcMiddle},
		{"preprocessor.end.", ppcEnd},
	};
	const std::string ppSymbol = props.GetNewExpandString("preprocessor.symbol.", fileNameForExtension.c_str());
	preprocessorSymbol = ppSymbol.empty() ? 0 : ppSymbol[0];
	preprocOfString.clear();
	for (const PropToPPC &preproc : propToPPC) {
		const std::string list = props.GetNewExpandString(preproc.propName, fileNameForExtension.c_str());
		const std::vector<std::string> words = StringSplit(list, ' ');
		for (const std::string &word : words) {
			preprocOfString[word] = preproc.ppc;
		}
	}

	memFiles.AppendList(props.GetNewExpandString("find.files").c_str());

	wEditor.SetWrapVisualFlags(props.GetInt("wrap.visual.flags"));
	wEditor.SetWrapVisualFlagsLocation(props.GetInt("wrap.visual.flags.location"));
 	wEditor.SetWrapStartIndent(props.GetInt("wrap.visual.startindent"));
 	wEditor.SetWrapIndentMode(props.GetInt("wrap.indent.mode"));

	idleStyling = props.GetInt("idle.styling", SC_IDLESTYLING_NONE);
	wEditor.SetIdleStyling(idleStyling);
	wOutput.SetIdleStyling(props.GetInt("output.idle.styling", SC_IDLESTYLING_NONE));

	if (props.GetInt("os.x.home.end.keys")) {
		AssignKey(SCK_HOME, 0, SCI_SCROLLTOSTART);
		AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_NULL);
		AssignKey(SCK_HOME, SCMOD_SHIFT | SCMOD_ALT, SCI_NULL);
		AssignKey(SCK_END, 0, SCI_SCROLLTOEND);
		AssignKey(SCK_END, SCMOD_SHIFT, SCI_NULL);
	} else {
		if (props.GetInt("wrap.aware.home.end.keys",0)) {
			if (props.GetInt("vc.home.key", 1)) {
				AssignKey(SCK_HOME, 0, SCI_VCHOMEWRAP);
				AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_VCHOMEWRAPEXTEND);
				AssignKey(SCK_HOME, SCMOD_SHIFT | SCMOD_ALT, SCI_VCHOMERECTEXTEND);
			} else {
				AssignKey(SCK_HOME, 0, SCI_HOMEWRAP);
				AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_HOMEWRAPEXTEND);
				AssignKey(SCK_HOME, SCMOD_SHIFT | SCMOD_ALT, SCI_HOMERECTEXTEND);
			}
			AssignKey(SCK_END, 0, SCI_LINEENDWRAP);
			AssignKey(SCK_END, SCMOD_SHIFT, SCI_LINEENDWRAPEXTEND);
		} else {
			if (props.GetInt("vc.home.key", 1)) {
				AssignKey(SCK_HOME, 0, SCI_VCHOME);
				AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_VCHOMEEXTEND);
				AssignKey(SCK_HOME, SCMOD_SHIFT | SCMOD_ALT, SCI_VCHOMERECTEXTEND);
			} else {
				AssignKey(SCK_HOME, 0, SCI_HOME);
				AssignKey(SCK_HOME, SCMOD_SHIFT, SCI_HOMEEXTEND);
				AssignKey(SCK_HOME, SCMOD_SHIFT | SCMOD_ALT, SCI_HOMERECTEXTEND);
			}
			AssignKey(SCK_END, 0, SCI_LINEEND);
			AssignKey(SCK_END, SCMOD_SHIFT, SCI_LINEENDEXTEND);
		}
	}

	AssignKey('L', SCMOD_SHIFT | SCMOD_CTRL, SCI_LINEDELETE);


	scrollOutput = props.GetInt("output.scroll", 1);

	tabHideOne = props.GetInt("tabbar.hide.one");

	SetToolsMenu();

	wEditor.SetFoldFlags(props.GetInt("fold.flags"));

	// To put the folder markers in the line number region
	//wEditor.SetMarginMaskN(0, SC_MASK_FOLDERS);

	wEditor.SetModEventMask(SC_MOD_CHANGEFOLD);

	if (0==props.GetInt("undo.redo.lazy")) {
		// Trap for insert/delete notifications (also fired by undo
		// and redo) so that the buttons can be enabled if needed.
		wEditor.SetModEventMask(SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT
			| SC_LASTSTEPINUNDOREDO | wEditor.ModEventMask());

		//SC_LASTSTEPINUNDOREDO is probably not needed in the mask; it
		//doesn't seem to fire as an event of its own; just modifies the
		//insert and delete events.
	}

	// Create a margin column for the folding symbols
	wEditor.SetMarginTypeN(2, SC_MARGIN_SYMBOL);

	foldMarginWidth = props.GetInt("fold.margin.width");
	if (foldMarginWidth == 0)
		foldMarginWidth = foldMarginWidthDefault;
	wEditor.SetMarginWidthN(2, foldMargin ? foldMarginWidth : 0);

	wEditor.SetMarginMaskN(2, SC_MASK_FOLDERS);
	wEditor.SetMarginSensitiveN(2, 1);

	// Define foreground (outline) and background (fill) color of folds
	const int foldSymbols = props.GetInt("fold.symbols");
	std::string foldFore = props.GetExpandedString("fold.fore");
	if (foldFore.length() == 0) {
		// Set default colour for outline
		switch (foldSymbols) {
		case 0: // Arrows
			foldFore = "#000000";
			break;
		case 1: // + -
			foldFore = "#FFFFFF";
			break;
		case 2: // Circles
			foldFore = "#404040";
			break;
		case 3: // Squares
			foldFore = "#808080";
			break;
		}
	}
	const SA::Colour colourFoldFore = ColourFromString(foldFore);

	std::string foldBack = props.GetExpandedString("fold.back");
	// Set default colour for fill
	if (foldBack.length() == 0) {
		switch (foldSymbols) {
		case 0:
		case 1:
			foldBack = "#000000";
			break;
		case 2:
		case 3:
			foldBack = "#FFFFFF";
			break;
		}
	}
	const SA::Colour colourFoldBack = ColourFromString(foldBack);

	// Enable/disable highlight for current folding block (smallest one that contains the caret)
	const int isHighlightEnabled = props.GetInt("fold.highlight", 0);
	// Define the colour of highlight
	const SA::Colour colourFoldBlockHighlight = ColourOfProperty(props, "fold.highlight.colour", ColourRGB(0xFF, 0, 0));

	switch (foldSymbols) {
	case 0:
		// Arrow pointing right for contracted folders, arrow pointing down for expanded
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_ARROWDOWN,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_ARROW,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY,
					 colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		// The highlight is disabled for arrow.
		wEditor.MarkerEnableHighlight(false);
		break;
	case 1:
		// Plus for contracted folders, minus for expanded
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_PLUS,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY,
		             colourFoldFore, colourFoldBack, colourFoldBlockHighlight);
		// The highlight is disabled for plus/minus.
		wEditor.MarkerEnableHighlight(false);
		break;
	case 2:
		// Like a flattened tree control using circular headers and curved joins
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		wEditor.MarkerEnableHighlight(isHighlightEnabled);
		break;
	case 3:
		// Like a flattened tree control using square headers
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER,
		             colourFoldBack, colourFoldFore, colourFoldBlockHighlight);
		wEditor.MarkerEnableHighlight(isHighlightEnabled);
		break;
	}

	wEditor.MarkerSetFore(markerBookmark,
		ColourOfProperty(props, "bookmark.fore", ColourRGB(0xbe, 0, 0)));
	wEditor.MarkerSetBack(markerBookmark,
		ColourOfProperty(props, "bookmark.back", ColourRGB(0xe2, 0x40, 0x40)));
	wEditor.MarkerSetAlpha(markerBookmark,
		props.GetInt("bookmark.alpha", SC_ALPHA_NOALPHA));
	const std::string bookMarkXPM = props.GetString("bookmark.pixmap");
	if (bookMarkXPM.length()) {
		wEditor.MarkerDefinePixmap(markerBookmark,
			bookMarkXPM.c_str());
	} else if (props.GetString("bookmark.fore").length()) {
		wEditor.MarkerDefine(markerBookmark, props.GetInt("bookmark.symbol", SC_MARK_BOOKMARK));
	} else {
		// No bookmark.fore setting so display default pixmap.
		wEditor.MarkerDefinePixmap(markerBookmark, reinterpret_cast<const char *>(bookmarkBluegem));
	}

	wEditor.SetScrollWidth(props.GetInt("horizontal.scroll.width", 2000));
	wEditor.SetScrollWidthTracking(props.GetInt("horizontal.scroll.width.tracking", 1));
	wOutput.SetScrollWidth(props.GetInt("output.horizontal.scroll.width", 2000));
	wOutput.SetScrollWidthTracking(props.GetInt("output.horizontal.scroll.width.tracking", 1));

	// Do these last as they force a style refresh
	wEditor.SetHScrollBar(props.GetInt("horizontal.scrollbar", 1));
	wOutput.SetHScrollBar(props.GetInt("output.horizontal.scrollbar", 1));

	wEditor.SetEndAtLastLine(props.GetInt("end.at.last.line", 1));
	wEditor.SetCaretSticky(props.GetInt("caret.sticky", 0));

	// Clear all previous indicators.
	wEditor.SetIndicatorCurrent(indicatorHighlightCurrentWord);
	wEditor.IndicatorClearRange(0, wEditor.Length());
	wOutput.SetIndicatorCurrent(indicatorHighlightCurrentWord);
	wOutput.IndicatorClearRange(0, wOutput.Length());
	currentWordHighlight.statesOfDelay = currentWordHighlight.noDelay;

	currentWordHighlight.isEnabled = props.GetInt("highlight.current.word", 0) == 1;
	if (currentWordHighlight.isEnabled) {
		const std::string highlightCurrentWordIndicatorString = props.GetExpandedString("highlight.current.word.indicator");
		IndicatorDefinition highlightCurrentWordIndicator(highlightCurrentWordIndicatorString.c_str());
		if (highlightCurrentWordIndicatorString.length() == 0) {
			highlightCurrentWordIndicator.style = INDIC_ROUNDBOX;
			std::string highlightCurrentWordColourString = props.GetExpandedString("highlight.current.word.colour");
			if (highlightCurrentWordColourString.length() == 0) {
				// Set default colour for highlight.
				highlightCurrentWordColourString = "#A0A000";
			}
			highlightCurrentWordIndicator.colour = ColourFromString(highlightCurrentWordColourString);
			highlightCurrentWordIndicator.fillAlpha = alphaIndicator;
			highlightCurrentWordIndicator.under = underIndicator;
		}
		SetOneIndicator(wEditor, indicatorHighlightCurrentWord, highlightCurrentWordIndicator);
		SetOneIndicator(wOutput, indicatorHighlightCurrentWord, highlightCurrentWordIndicator);
		currentWordHighlight.isOnlyWithSameStyle = props.GetInt("highlight.current.word.by.style", 0) == 1;
		HighlightCurrentWord(true);
	}

	std::map<std::string, std::string> eConfig = editorConfig->MapFromAbsolutePath(filePath);
	for (const std::pair<const std::string, std::string> &pss : eConfig) {
		if (pss.first == "indent_style") {
			wEditor.SetUseTabs(pss.second == "tab" ? 1 : 0);
		} else if (pss.first == "indent_size") {
			wEditor.SetIndent(std::stoi(pss.second));
		} else if (pss.first == "tab_width") {
			wEditor.SetTabWidth(std::stoi(pss.second));
		} else if (pss.first == "end_of_line") {
			if (pss.second == "lf") {
				wEditor.SetEOLMode(SC_EOL_LF);
			} else if (pss.second == "cr") {
				wEditor.SetEOLMode(SC_EOL_CR);
			} else if (pss.second == "crlf") {
				wEditor.SetEOLMode(SC_EOL_CRLF);
			}
		} else if (pss.first == "charset") {
			if (pss.second == "latin1") {
				CurrentBuffer()->unicodeMode = uni8Bit;
				codePage = 0;
			} else {
				if (pss.second == "utf-8")
					CurrentBuffer()->unicodeMode = uniCookie;
				if (pss.second == "utf-8-bom")
					CurrentBuffer()->unicodeMode = uniUTF8;
				if (pss.second == "utf-16be")
					CurrentBuffer()->unicodeMode = uni16BE;
				if (pss.second == "utf-16le")
					CurrentBuffer()->unicodeMode = uni16LE;
				codePage = SC_CP_UTF8;
			}
			wEditor.SetCodePage(codePage);
		} else if (pss.first == "trim_trailing_whitespace") {
			stripTrailingSpaces = pss.second == "true";
		} else if (pss.first == "insert_final_newline") {
			ensureFinalLineEnd = pss.second == "true";
		}
	}

	if (extender) {
		FilePath defaultDir = GetDefaultDirectory();
		FilePath scriptPath;

		// Check for an extension script
		GUI::gui_string extensionFile = GUI::StringFromUTF8(
			props.GetNewExpandString("extension.", fileNameForExtension.c_str()));
		if (extensionFile.length()) {
			// find file in local directory
			FilePath docDir = filePath.Directory();
			if (Exists(docDir.AsInternal(), extensionFile.c_str(), &scriptPath)) {
				// Found file in document directory
				extender->Load(scriptPath.AsUTF8().c_str());
			} else if (Exists(defaultDir.AsInternal(), extensionFile.c_str(), &scriptPath)) {
				// Found file in global directory
				extender->Load(scriptPath.AsUTF8().c_str());
			} else if (Exists(GUI_TEXT(""), extensionFile.c_str(), &scriptPath)) {
				// Found as completely specified file name
				extender->Load(scriptPath.AsUTF8().c_str());
			}
		}
	}

	delayBeforeAutoSave = props.GetInt("save.on.timer");
	if (delayBeforeAutoSave) {
		TimerStart(timerAutoSave);
	} else {
		TimerEnd(timerAutoSave);
	}

	firstPropertiesRead = false;
	needReadProperties = false;
}

void SciTEBase::ReadFontProperties() {
	char key[200];
	const char *languageName = language.c_str();

	if (lexLanguage == lexLPeg) {
		// Retrieve style info.
		char propStr[256];
		for (int i = 0; i < STYLE_MAX; i++) {
			sprintf(key, "style.lpeg.%0d", i);
			wEditor.PrivateLexerCall(i - STYLE_MAX,
				SptrFromString(propStr));
			props.Set(key, static_cast<const char *>(propStr));
		}
		languageName = "lpeg";
	}

	// Set styles
	// For each window set the global default style, then the language default style, then the other global styles, then the other language styles

	const int fontQuality = props.GetInt("font.quality");
	wEditor.SetFontQuality(fontQuality);
	wOutput.SetFontQuality(fontQuality);

	wEditor.StyleResetDefault();
	wOutput.StyleResetDefault();

	sprintf(key, "style.%s.%0d", "*", STYLE_DEFAULT);
	std::string sval = props.GetNewExpandString(key);
	SetOneStyle(wEditor, STYLE_DEFAULT, StyleDefinition(sval));
	SetOneStyle(wOutput, STYLE_DEFAULT, StyleDefinition(sval));

	sprintf(key, "style.%s.%0d", languageName, STYLE_DEFAULT);
	sval = props.GetNewExpandString(key);
	SetOneStyle(wEditor, STYLE_DEFAULT, StyleDefinition(sval));

	wEditor.StyleClearAll();

	SetStyleFor(wEditor, "*");
	SetStyleFor(wEditor, languageName);
	if (props.GetInt("error.inline")) {
		wEditor.ReleaseAllExtendedStyles();
		diagnosticStyleStart = wEditor.AllocateExtendedStyles(diagnosticStyles);
		SetStyleBlock(wEditor, "error", diagnosticStyleStart, diagnosticStyleStart+diagnosticStyles-1);
	}

	const int diffToSecondary = static_cast<int>(wEditor.DistanceToSecondaryStyles());
	for (const char subStyleBase : subStyleBases) {
		const int subStylesStart = wEditor.SubStylesStart(subStyleBase);
		const int subStylesLength = wEditor.SubStylesLength(subStyleBase);
		for (int subStyle=0; subStyle<subStylesLength; subStyle++) {
			for (int active=0; active<(diffToSecondary?2:1); active++) {
				const int activity = active * diffToSecondary;
				sprintf(key, "style.%s.%0d.%0d", languageName, subStyleBase + activity, subStyle+1);
				sval = props.GetNewExpandString(key);
				SetOneStyle(wEditor, subStylesStart + subStyle + activity, StyleDefinition(sval));
			}
		}
	}

	// Turn grey while loading
	if (CurrentBuffer()->lifeState == Buffer::reading)
		wEditor.StyleSetBack(STYLE_DEFAULT, 0xEEEEEE);

	wOutput.StyleClearAll();

	sprintf(key, "style.%s.%0d", "errorlist", STYLE_DEFAULT);
	sval = props.GetNewExpandString(key);
	SetOneStyle(wOutput, STYLE_DEFAULT, StyleDefinition(sval));

	wOutput.StyleClearAll();

	SetStyleFor(wOutput, "*");
	SetStyleFor(wOutput, "errorlist");

	if (CurrentBuffer()->useMonoFont) {
		sval = props.GetExpandedString("font.monospace");
		StyleDefinition sd(sval.c_str());
		for (int style = 0; style <= STYLE_MAX; style++) {
			if (style != STYLE_LINENUMBER) {
				if (sd.specified & StyleDefinition::sdFont) {
					wEditor.StyleSetFont(style, sd.font.c_str());
				}
				if (sd.specified & StyleDefinition::sdSize) {
					wEditor.StyleSetSizeFractional(style, sd.FractionalSize());
				}
			}
		}
	}
}

// Properties that are interactively modifiable are only read from the properties file once.
void SciTEBase::SetPropertiesInitial() {
	splitVertical = props.GetInt("split.vertical");
	openFilesHere = props.GetInt("check.if.already.open");
	wrap = props.GetInt("wrap");
	wrapOutput = props.GetInt("output.wrap");
	indentationWSVisible = props.GetInt("view.indentation.whitespace", 1);
	sbVisible = props.GetInt("statusbar.visible");
	tbVisible = props.GetInt("toolbar.visible");
	tabVisible = props.GetInt("tabbar.visible");
	tabMultiLine = props.GetInt("tabbar.multiline");
	lineNumbers = props.GetInt("line.margin.visible");
	margin = props.GetInt("margin.width");
	foldMargin = props.GetInt("fold.margin.width", foldMarginWidthDefault);

	matchCase = props.GetInt("find.replace.matchcase");
	regExp = props.GetInt("find.replace.regexp");
	unSlash = props.GetInt("find.replace.escapes");
	wrapFind = props.GetInt("find.replace.wrap", 1);
	focusOnReplace = props.GetInt("find.replacewith.focus", 1);
}

GUI::gui_string Localization::Text(const char *s, bool retainIfNotFound) {
	const std::string sEllipse("...");	// An ASCII ellipse
	const std::string utfEllipse("\xe2\x80\xa6");	// A UTF-8 ellipse
	std::string translation = s;
	const int ellipseIndicator = Remove(translation, sEllipse);
	const int utfEllipseIndicator = Remove(translation, utfEllipse);
	const std::string menuAccessIndicatorChar(1, static_cast<char>(menuAccessIndicator[0]));
	const int accessKeyPresent = Remove(translation, menuAccessIndicatorChar);
	LowerCaseAZ(translation);
	Substitute(translation, "\n", "\\n");
	translation = GetString(translation.c_str());
	if (translation.length()) {
		if (ellipseIndicator)
			translation += sEllipse;
		if (utfEllipseIndicator)
			translation += utfEllipse;
		if (0 == accessKeyPresent) {
#if !defined(GTK)
			// Following codes are required because accelerator is not always
			// part of alphabetical word in several language. In these cases,
			// accelerator is written like "(&O)".
			const size_t posOpenParenAnd = translation.find("(&");
			if ((posOpenParenAnd != std::string::npos) && (translation.find(")", posOpenParenAnd) == posOpenParenAnd + 3)) {
				translation.erase(posOpenParenAnd, 4);
			} else {
				Remove(translation, std::string("&"));
			}
#else
			Remove(translation, std::string("&"));
#endif
		}
		Substitute(translation, "&", menuAccessIndicatorChar);
		Substitute(translation, "\\n", "\n");
	} else {
		translation = missing;
	}
	if ((translation.length() > 0) || !retainIfNotFound) {
		return GUI::StringFromUTF8(translation);
	}
	return GUI::StringFromUTF8(s);
}

GUI::gui_string SciTEBase::LocaliseMessage(const char *s, const GUI::gui_char *param0, const GUI::gui_char *param1, const GUI::gui_char *param2) {
	GUI::gui_string translation = localiser.Text(s);
	if (param0)
		Substitute(translation, GUI_TEXT("^0"), param0);
	if (param1)
		Substitute(translation, GUI_TEXT("^1"), param1);
	if (param2)
		Substitute(translation, GUI_TEXT("^2"), param2);
	return translation;
}

void SciTEBase::ReadLocalization() {
	localiser.Clear();
	GUI::gui_string title = GUI_TEXT("locale.properties");
	const std::string localeProps = props.GetExpandedString("locale.properties");
	if (localeProps.length()) {
		title = GUI::StringFromUTF8(localeProps);
	}
	FilePath propdir = GetSciteDefaultHome();
	FilePath localePath(propdir, title);
	localiser.Read(localePath, propdir, filter, &importFiles, 0);
	localiser.SetMissing(props.GetString("translation.missing"));
	localiser.read = true;
}

void SciTEBase::ReadPropertiesInitial() {
	SetPropertiesInitial();
	const int sizeHorizontal = props.GetInt("output.horizontal.size", 0);
	const int sizeVertical = props.GetInt("output.vertical.size", 0);
	const int hideOutput = props.GetInt("output.initial.hide", 0);
	if ((!splitVertical && (sizeVertical > 0) && (heightOutput < sizeVertical)) ||
		(splitVertical && (sizeHorizontal > 0) && (heightOutput < sizeHorizontal))) {
		previousHeightOutput = splitVertical ? sizeHorizontal : sizeVertical;
		if (!hideOutput) {
			heightOutput = NormaliseSplit(previousHeightOutput);
			SizeSubWindows();
			Redraw();
		}
	}
	ViewWhitespace(props.GetInt("view.whitespace"));
	wEditor.SetIndentationGuides(props.GetInt("view.indentation.guides") ?
		indentExamine : SC_IV_NONE);

	wEditor.SetViewEOL(props.GetInt("view.eol"));
	wEditor.SetZoom(props.GetInt("magnification"));
	wOutput.SetZoom(props.GetInt("output.magnification"));
	wEditor.SetWrapMode(wrap ? wrapStyle : SC_WRAP_NONE);
	wOutput.SetWrapMode(wrapOutput ? wrapStyle : SC_WRAP_NONE);

	std::string menuLanguageProp = props.GetExpandedString("menu.language");
	std::replace(menuLanguageProp.begin(), menuLanguageProp.end(), '|', '\0');
	const char *sMenuLanguage = menuLanguageProp.c_str();
	while (*sMenuLanguage) {
		LanguageMenuItem lmi;
		lmi.menuItem = sMenuLanguage;
		sMenuLanguage += strlen(sMenuLanguage) + 1;
		lmi.extension = sMenuLanguage;
		sMenuLanguage += strlen(sMenuLanguage) + 1;
		lmi.menuKey = sMenuLanguage;
		sMenuLanguage += strlen(sMenuLanguage) + 1;
		languageMenu.push_back(lmi);
	}
	SetLanguageMenu();

	// load the user defined short cut props
	std::string shortCutProp = props.GetNewExpandString("user.shortcuts");
	if (shortCutProp.length()) {
		const size_t pipes = std::count(shortCutProp.begin(), shortCutProp.end(), '|');
		std::replace(shortCutProp.begin(), shortCutProp.end(), '|', '\0');
		const char *sShortCutProp = shortCutProp.c_str();
		for (size_t item = 0; item < pipes/2; item++) {
			ShortcutItem sci;
			sci.menuKey = sShortCutProp;
			sShortCutProp += strlen(sShortCutProp) + 1;
			sci.menuCommand = sShortCutProp;
			sShortCutProp += strlen(sShortCutProp) + 1;
			shortCutItemList.push_back(sci);
		}
	}
	// end load the user defined short cut props

	FilePath homepath = GetSciteDefaultHome();
	props.Set("SciteDefaultHome", homepath.AsUTF8().c_str());
	homepath = GetSciteUserHome();
	props.Set("SciteUserHome", homepath.AsUTF8().c_str());
}

FilePath SciTEBase::GetDefaultPropertiesFileName() {
	return FilePath(GetSciteDefaultHome(), propGlobalFileName);
}

FilePath SciTEBase::GetAbbrevPropertiesFileName() {
	return FilePath(GetSciteUserHome(), propAbbrevFileName);
}

FilePath SciTEBase::GetUserPropertiesFileName() {
	return FilePath(GetSciteUserHome(), propUserFileName);
}

FilePath SciTEBase::GetLocalPropertiesFileName() {
	return FilePath(filePath.Directory(), propLocalFileName);
}

FilePath SciTEBase::GetDirectoryPropertiesFileName() {
	FilePath propfile;

	if (filePath.IsSet()) {
		propfile.Set(filePath.Directory(), propDirectoryFileName);

		// if this file does not exist try to find the prop file in a parent directory
		while (!propfile.Directory().IsRoot() && !propfile.Exists()) {
			propfile.Set(propfile.Directory().Directory(), propDirectoryFileName);
		}

		// not found -> set it to the initial directory
		if (!propfile.Exists()) {
			propfile.Set(filePath.Directory(), propDirectoryFileName);
		}
	}
	return propfile;
}

void SciTEBase::OpenProperties(int propsFile) {
	FilePath propfile;
	switch (propsFile) {
	case IDM_OPENLOCALPROPERTIES:
		propfile = GetLocalPropertiesFileName();
		Open(propfile, ofQuiet);
		break;
	case IDM_OPENUSERPROPERTIES:
		propfile = GetUserPropertiesFileName();
		Open(propfile, ofQuiet);
		break;
	case IDM_OPENABBREVPROPERTIES:
		propfile = pathAbbreviations;
		Open(propfile, ofQuiet);
		break;
	case IDM_OPENGLOBALPROPERTIES:
		propfile = GetDefaultPropertiesFileName();
		Open(propfile, ofQuiet);
		break;
	case IDM_OPENLUAEXTERNALFILE: {
			GUI::gui_string extlua = GUI::StringFromUTF8(props.GetExpandedString("ext.lua.startup.script"));
			if (extlua.length()) {
				Open(extlua, ofQuiet);
			}
			break;
		}
	case IDM_OPENDIRECTORYPROPERTIES: {
			propfile = GetDirectoryPropertiesFileName();
			const bool alreadyExists = propfile.Exists();
			Open(propfile, ofQuiet);
			if (!alreadyExists)
				SaveAsDialog();
		}
		break;
	}
}

// return the int value of the command name passed in.
int SciTEBase::GetMenuCommandAsInt(std::string commandName) {
	int i = IFaceTable::FindConstant(commandName.c_str());
	if (i != -1) {
		return IFaceTable::constants[i].value;
	}

	// Check also for a SCI command, as long as it has no parameters
	i = IFaceTable::FindFunctionByConstantName(commandName.c_str());
	if (i != -1 &&
		IFaceTable::functions[i].paramType[0] == iface_void &&
		IFaceTable::functions[i].paramType[1] == iface_void) {
		return IFaceTable::functions[i].value;
	}

	// Otherwise we might have entered a number as command to access a "SCI_" command
	return atoi(commandName.c_str());
}
