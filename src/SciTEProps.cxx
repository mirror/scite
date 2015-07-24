// SciTE - Scintilla based Text Editor
/** @file SciTEProps.cxx
 ** Properties management.
 **/
// Copyright 1998-2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <time.h>
#include <locale.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "Scintilla.h"
#include "SciLexer.h"
#include "ILexer.h"

#include "GUI.h"

#if defined(__unix__)

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
#include "IFaceTable.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"

void SciTEBase::SetImportMenu() {
	for (int i = 0; i < importMax; i++) {
		DestroyMenuItem(menuOptions, importCmdID + i);
	}
	if (!importFiles.empty()) {
		for (int stackPos = 0; stackPos < static_cast<int>(importFiles.size()) && stackPos < importMax; stackPos++) {
			int itemID = importCmdID + stackPos;
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
		int itemID = languageCmdID + item;
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

const GUI::gui_char propLocalFileName[] = GUI_TEXT("SciTE.properties");
const GUI::gui_char propDirectoryFileName[] = GUI_TEXT("SciTEDirectory.properties");

/**
Read global and user properties files.
*/
void SciTEBase::ReadGlobalPropFile() {
#if defined(__unix__)
	extern char **environ;
	char **e=environ;
#else
	char **e=_environ;
#endif
	for (; e && *e; e++) {
		char key[1024];
		char *k=*e;
		char *v=strchr(k,'=');
		if (v && (static_cast<size_t>(v-k) < sizeof(key))) {
			memcpy(key, k, v-k);
			key[v-k] = '\0';
			propsEmbed.Set(key, v+1);
		}
	}

	std::string excludes;
	std::string includes;

	for (int attempt=0; attempt<2; attempt++) {

		std::string excludesRead = props.GetString("imports.exclude");
		std::string includesRead = props.GetString("imports.include");
		if ((attempt > 0) && ((excludesRead == excludes) && (includesRead == includes)))
			break;

		excludes = excludesRead;
		includes = includesRead;

		filter.SetFilter(excludes.c_str(), includes.c_str());

		importFiles.clear();

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

		propsDirectory.Read(propfile, propfile.Directory(), filter, NULL, 0);
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
	propsLocal.Read(propfile, propfile.Directory(), filter, NULL, 0);

	props.Set("Chrome", "#C0C0C0");
	props.Set("ChromeHighlight", "#FFFFFF");
}

Colour ColourOfProperty(PropSetFile &props, const char *key, Colour colourDefault) {
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
	if (pStart == NULL) {
		return NULL;
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
		const int baseStyle = wEditor.Call(SCI_GETSTYLEFROMSUBSTYLE, style);
		if (baseStyle != style) {
			const int primaryStyle = wEditor.Call(SCI_GETPRIMARYSTYLEFROMSTYLE, style);
			const int distanceSecondary = (style == primaryStyle) ? 0 : wEditor.Call(SCI_DISTANCETOSECONDARYSTYLES);
			const int primaryBase = baseStyle - distanceSecondary;
			const int subStylesStart = wEditor.Call(SCI_GETSUBSTYLESSTART, primaryBase);
			const int subStylesLength = wEditor.Call(SCI_GETSUBSTYLESLENGTH, primaryBase);
			const int subStyle = style - (subStylesStart + distanceSecondary);
			if (subStyle < subStylesLength) {
				char key[200];
				sprintf(key, "style.%s.%0d.%0d", languageName.c_str(), baseStyle, subStyle + 1);
				ss = props.GetNewExpandString(key);
			}
		}
	}

	StyleDefinition sd(ssDefault.c_str());
	sd.ParseStyleDefinition(ss.c_str());
	return sd;
}

void SciTEBase::SetOneStyle(GUI::ScintillaWindow &win, int style, const StyleDefinition &sd) {
	if (sd.specified & StyleDefinition::sdItalics)
		win.Send(SCI_STYLESETITALIC, style, sd.italics ? 1 : 0);
	if (sd.specified & StyleDefinition::sdWeight)
		win.Send(SCI_STYLESETWEIGHT, style, sd.weight);
	if (sd.specified & StyleDefinition::sdFont)
		win.SendPointer(SCI_STYLESETFONT, style,
			const_cast<char *>(sd.font.c_str()));
	if (sd.specified & StyleDefinition::sdFore)
		win.Send(SCI_STYLESETFORE, style, sd.ForeAsLong());
	if (sd.specified & StyleDefinition::sdBack)
		win.Send(SCI_STYLESETBACK, style, sd.BackAsLong());
	if (sd.specified & StyleDefinition::sdSize)
		win.Send(SCI_STYLESETSIZEFRACTIONAL, style, sd.FractionalSize());
	if (sd.specified & StyleDefinition::sdEOLFilled)
		win.Send(SCI_STYLESETEOLFILLED, style, sd.eolfilled ? 1 : 0);
	if (sd.specified & StyleDefinition::sdUnderlined)
		win.Send(SCI_STYLESETUNDERLINE, style, sd.underlined ? 1 : 0);
	if (sd.specified & StyleDefinition::sdCaseForce)
		win.Send(SCI_STYLESETCASE, style, sd.caseForce);
	if (sd.specified & StyleDefinition::sdVisible)
		win.Send(SCI_STYLESETVISIBLE, style, sd.visible ? 1 : 0);
	if (sd.specified & StyleDefinition::sdChangeable)
		win.Send(SCI_STYLESETCHANGEABLE, style, sd.changeable ? 1 : 0);
	win.Send(SCI_STYLESETCHARACTERSET, style, characterSet);
}

void SciTEBase::SetStyleBlock(GUI::ScintillaWindow &win, const char *lang, int start, int last) {
	for (int style = start; style <= last; style++) {
		if (style != STYLE_DEFAULT) {
			char key[200];
			sprintf(key, "style.%s.%0d", lang, style-start);
			std::string sval = props.GetExpandedString(key);
			if (sval.length()) {
				SetOneStyle(win, style, sval.c_str());
			}
		}
	}
}

void SciTEBase::SetStyleFor(GUI::ScintillaWindow &win, const char *lang) {
	int maxStyle = (1 << win.Call(SCI_GETSTYLEBITS)) - 1;
	if (maxStyle < STYLE_LASTPREDEFINED)
		maxStyle = STYLE_LASTPREDEFINED;
	SetStyleBlock(win, lang, 0, maxStyle);
}

void SciTEBase::SetOneIndicator(GUI::ScintillaWindow &win, int indicator, const IndicatorDefinition &ind) {
	win.Call(SCI_INDICSETSTYLE, indicator, ind.style);
	win.Call(SCI_INDICSETFORE, indicator, ind.colour);
	win.Call(SCI_INDICSETALPHA, indicator, ind.fillAlpha);
	win.Call(SCI_INDICSETOUTLINEALPHA, indicator, ind.outlineAlpha);
	win.Call(SCI_INDICSETUNDER, indicator, ind.under);
}

std::string SciTEBase::ExtensionFileName() const {
	if (CurrentBuffer()->overrideExtension.length()) {
		return CurrentBuffer()->overrideExtension;
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
		wEditor.CallString(SCI_SETPROPERTY,
						 reinterpret_cast<uptr_t>(key), value.c_str());
		wOutput.CallString(SCI_SETPROPERTY,
						 reinterpret_cast<uptr_t>(key), value.c_str());
	}
}

void SciTEBase::DefineMarker(int marker, int markerType, Colour fore, Colour back, Colour backSelected) {
	wEditor.Call(SCI_MARKERDEFINE, marker, markerType);
	wEditor.Call(SCI_MARKERSETFORE, marker, fore);
	wEditor.Call(SCI_MARKERSETBACK, marker, back);
	wEditor.Call(SCI_MARKERSETBACKSELECTED, marker, backSelected);
}

void SciTEBase::ReadAPI(const std::string &fileNameForExtension) {
	std::string sApiFileNames = props.GetNewExpandString("api.",
	                        fileNameForExtension.c_str());
	if (sApiFileNames.length() > 0) {
		std::vector<std::string> vApiFileNames = StringSplit(sApiFileNames, ';');
		std::vector<char> data;

		// Load files into data
		for (std::vector<std::string>::iterator it = vApiFileNames.begin(); it != vApiFileNames.end(); ++it) {
			std::vector<char> contents = FilePath(GUI::StringFromUTF8(*it)).Read();
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
	"fold.asm.comment.explicit",
	"fold.asm.comment.multiline",
	"fold.asm.explicit.anywhere",
	"fold.asm.explicit.end",
	"fold.asm.explicit.start",
	"fold.asm.syntax.based",
	"fold.at.else",
	"fold.basic.comment.explicit",
	"fold.basic.explicit.anywhere",
	"fold.basic.explicit.end",
	"fold.basic.explicit.start",
	"fold.basic.syntax.based",
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
	"lexer.errorlist.value.separate",
	"lexer.flagship.styling.within.preprocessor",
	"lexer.haskell.allow.hash",
	"lexer.haskell.allow.questionmark",
	"lexer.haskell.allow.quotes",
	"lexer.haskell.cpp",
	"lexer.haskell.import.safe",
	"lexer.html.django",
	"lexer.html.mako",
	"lexer.metapost.comment.process",
	"lexer.metapost.interface.default",
	"lexer.pascal.smart.highlighting",
	"lexer.props.allow.initial.spaces",
	"lexer.python.keywords2.no.sub.identifiers",
	"lexer.python.literals.binary",
	"lexer.python.strings.b",
	"lexer.python.strings.over.newline",
	"lexer.python.strings.u",
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

	0,
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
	    wEditor.CallString(SCI_LOADLEXERLIBRARY, 0, modulePath.c_str());
	language = props.GetNewExpandString("lexer.", fileNameForExtension.c_str());
	if (language.length()) {
		if (StartsWith(language, "script_")) {
			wEditor.Call(SCI_SETLEXER, SCLEX_CONTAINER);
		} else if (StartsWith(language, "lpeg_")) {
			modulePath = props.GetNewExpandString("lexerpath.*.lpeg");
			if (modulePath.length()) {
				wEditor.CallString(SCI_LOADLEXERLIBRARY, 0, modulePath.c_str());
				wEditor.CallString(SCI_SETLEXERLANGUAGE, 0, "lpeg");
				lexLPeg = wEditor.Call(SCI_GETLEXER);
				const char *lexer = language.c_str() + language.find("_") + 1;
				wEditor.CallReturnPointer(SCI_PRIVATELEXERCALL, SCI_SETLEXERLANGUAGE,
					reinterpret_cast<sptr_t>(lexer));
			}
		} else {
			wEditor.CallString(SCI_SETLEXERLANGUAGE, 0, language.c_str());
		}
	} else {
		wEditor.Call(SCI_SETLEXER, SCLEX_NULL);
	}

	props.Set("Language", language.c_str());

	lexLanguage = wEditor.Call(SCI_GETLEXER);

	if (StartsWith(language, "script_") || StartsWith(language, "lpeg_"))
		wEditor.Call(SCI_SETSTYLEBITS, 8);
	else
		wEditor.Call(SCI_SETSTYLEBITS, wEditor.Call(SCI_GETSTYLEBITSNEEDED));

	wOutput.Call(SCI_SETLEXER, SCLEX_ERRORLIST);

	const std::string kw0 = props.GetNewExpandString("keywords.", fileNameForExtension.c_str());
	wEditor.CallString(SCI_SETKEYWORDS, 0, kw0.c_str());

	for (int wl = 1; wl <= KEYWORDSET_MAX; wl++) {
		std::string kwk = StdStringFromInteger(wl+1);
		kwk += '.';
		kwk.insert(0, "keywords");
		const std::string kw = props.GetNewExpandString(kwk.c_str(), fileNameForExtension.c_str());
		wEditor.CallString(SCI_SETKEYWORDS, wl, kw.c_str());
	}

	subStyleBases.clear();
	int lenSSB = wEditor.CallString(SCI_GETSUBSTYLEBASES, 0, NULL);
	if (lenSSB) {
		wEditor.Call(SCI_FREESUBSTYLES);

		subStyleBases.resize(lenSSB+1);
		wEditor.CallString(SCI_GETSUBSTYLEBASES, 0, &subStyleBases[0]);
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
				subStyleIdentifiersStart = wEditor.Call(SCI_ALLOCATESUBSTYLES, subStyleBases[baseStyle], subStyleIdentifiers);
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
				wEditor.CallString(SCI_SETIDENTIFIERS, subStyleIdentifiersStart + subStyle, ssWords.c_str());
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

	int tech = props.GetInt("technology");
	wEditor.Call(SCI_SETTECHNOLOGY, tech);
	wOutput.Call(SCI_SETTECHNOLOGY, tech);

	codePage = props.GetInt("code.page");
	if (CurrentBuffer()->unicodeMode != uni8Bit) {
		// Override properties file to ensure Unicode displayed.
		codePage = SC_CP_UTF8;
	}
	wEditor.Call(SCI_SETCODEPAGE, codePage);
	int outputCodePage = props.GetInt("output.code.page", codePage);
	wOutput.Call(SCI_SETCODEPAGE, outputCodePage);

	characterSet = props.GetInt("character.set", SC_CHARSET_DEFAULT);

#ifdef __unix__
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

	wrapStyle = props.GetInt("wrap.style", SC_WRAP_WORD);

	CallChildren(SCI_SETCARETFORE,
	           ColourOfProperty(props, "caret.fore", ColourRGB(0, 0, 0)));

	CallChildren(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, props.GetInt("selection.rectangular.switch.mouse", 0));
	CallChildren(SCI_SETMULTIPLESELECTION, props.GetInt("selection.multiple", 1));
	CallChildren(SCI_SETADDITIONALSELECTIONTYPING, props.GetInt("selection.additional.typing", 1));
	CallChildren(SCI_SETADDITIONALCARETSBLINK, props.GetInt("caret.additional.blinks", 1));
	CallChildren(SCI_SETVIRTUALSPACEOPTIONS, props.GetInt("virtual.space"));

	wEditor.Call(SCI_SETMOUSEDWELLTIME,
	           props.GetInt("dwell.period", SC_TIME_FOREVER), 0);

	wEditor.Call(SCI_SETCARETWIDTH, props.GetInt("caret.width", 1));
	wOutput.Call(SCI_SETCARETWIDTH, props.GetInt("caret.width", 1));

	std::string caretLineBack = props.GetString("caret.line.back");
	if (caretLineBack.length()) {
		wEditor.Call(SCI_SETCARETLINEVISIBLE, 1);
		wEditor.Call(SCI_SETCARETLINEBACK, ColourFromString(caretLineBack));
	} else {
		wEditor.Call(SCI_SETCARETLINEVISIBLE, 0);
	}
	wEditor.Call(SCI_SETCARETLINEBACKALPHA,
		props.GetInt("caret.line.back.alpha", SC_ALPHA_NOALPHA));

	alphaIndicator = props.GetInt("indicators.alpha", 30);
	if (alphaIndicator < 0 || 255 < alphaIndicator) // If invalid value,
		alphaIndicator = 30; //then set default value.
	underIndicator = props.GetInt("indicators.under", 0) == 1;

	closeFind = props.GetInt("find.close.on.find", 1);

	const std::string controlCharSymbol = props.GetString("control.char.symbol");
	if (controlCharSymbol.length()) {
		wEditor.Call(SCI_SETCONTROLCHARSYMBOL, static_cast<unsigned char>(controlCharSymbol[0]));
	} else {
		wEditor.Call(SCI_SETCONTROLCHARSYMBOL, 0);
	}

	const std::string caretPeriod = props.GetString("caret.period");
	if (caretPeriod.length()) {
		wEditor.Call(SCI_SETCARETPERIOD, atoi(caretPeriod.c_str()));
		wOutput.Call(SCI_SETCARETPERIOD, atoi(caretPeriod.c_str()));
	}

	int caretSlop = props.GetInt("caret.policy.xslop", 1) ? CARET_SLOP : 0;
	int caretZone = props.GetInt("caret.policy.width", 50);
	int caretStrict = props.GetInt("caret.policy.xstrict") ? CARET_STRICT : 0;
	int caretEven = props.GetInt("caret.policy.xeven", 1) ? CARET_EVEN : 0;
	int caretJumps = props.GetInt("caret.policy.xjumps") ? CARET_JUMPS : 0;
	wEditor.Call(SCI_SETXCARETPOLICY, caretStrict | caretSlop | caretEven | caretJumps, caretZone);

	caretSlop = props.GetInt("caret.policy.yslop", 1) ? CARET_SLOP : 0;
	caretZone = props.GetInt("caret.policy.lines");
	caretStrict = props.GetInt("caret.policy.ystrict") ? CARET_STRICT : 0;
	caretEven = props.GetInt("caret.policy.yeven", 1) ? CARET_EVEN : 0;
	caretJumps = props.GetInt("caret.policy.yjumps") ? CARET_JUMPS : 0;
	wEditor.Call(SCI_SETYCARETPOLICY, caretStrict | caretSlop | caretEven | caretJumps, caretZone);

	int visibleStrict = props.GetInt("visible.policy.strict") ? VISIBLE_STRICT : 0;
	int visibleSlop = props.GetInt("visible.policy.slop", 1) ? VISIBLE_SLOP : 0;
	int visibleLines = props.GetInt("visible.policy.lines");
	wEditor.Call(SCI_SETVISIBLEPOLICY, visibleStrict | visibleSlop, visibleLines);

	wEditor.Call(SCI_SETEDGECOLUMN, props.GetInt("edge.column", 0));
	wEditor.Call(SCI_SETEDGEMODE, props.GetInt("edge.mode", EDGE_NONE));
	wEditor.Call(SCI_SETEDGECOLOUR,
	           ColourOfProperty(props, "edge.colour", ColourRGB(0xff, 0xda, 0xda)));

	std::string selFore = props.GetString("selection.fore");
	if (selFore.length()) {
		CallChildren(SCI_SETSELFORE, 1, ColourFromString(selFore));
	} else {
		CallChildren(SCI_SETSELFORE, 0, 0);
	}
	std::string selBack = props.GetString("selection.back");
	if (selBack.length()) {
		CallChildren(SCI_SETSELBACK, 1, ColourFromString(selBack));
	} else {
		if (selFore.length())
			CallChildren(SCI_SETSELBACK, 0, 0);
		else	// Have to show selection somehow
			CallChildren(SCI_SETSELBACK, 1, ColourRGB(0xC0, 0xC0, 0xC0));
	}
	int selectionAlpha = props.GetInt("selection.alpha", SC_ALPHA_NOALPHA);
	CallChildren(SCI_SETSELALPHA, selectionAlpha);

	std::string selAdditionalFore = props.GetString("selection.additional.fore");
	if (selAdditionalFore.length()) {
		CallChildren(SCI_SETADDITIONALSELFORE, ColourFromString(selAdditionalFore));
	}
	std::string selAdditionalBack = props.GetString("selection.additional.back");
	if (selAdditionalBack.length()) {
		CallChildren(SCI_SETADDITIONALSELBACK, ColourFromString(selAdditionalBack));
	}
	int selectionAdditionalAlpha = (selectionAlpha == SC_ALPHA_NOALPHA) ? SC_ALPHA_NOALPHA : selectionAlpha / 2;
	CallChildren(SCI_SETADDITIONALSELALPHA, props.GetInt("selection.additional.alpha", selectionAdditionalAlpha));

	std::string foldColour = props.GetString("fold.margin.colour");
	if (foldColour.length()) {
		CallChildren(SCI_SETFOLDMARGINCOLOUR, 1, ColourFromString(foldColour));
	} else {
		CallChildren(SCI_SETFOLDMARGINCOLOUR, 0, 0);
	}
	std::string foldHiliteColour = props.GetString("fold.margin.highlight.colour");
	if (foldHiliteColour.length()) {
		CallChildren(SCI_SETFOLDMARGINHICOLOUR, 1, ColourFromString(foldHiliteColour));
	} else {
		CallChildren(SCI_SETFOLDMARGINHICOLOUR, 0, 0);
	}

	std::string whitespaceFore = props.GetString("whitespace.fore");
	if (whitespaceFore.length()) {
		CallChildren(SCI_SETWHITESPACEFORE, 1, ColourFromString(whitespaceFore));
	} else {
		CallChildren(SCI_SETWHITESPACEFORE, 0, 0);
	}
	std::string whitespaceBack = props.GetString("whitespace.back");
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
	wEditor.CallString(SCI_AUTOCSETFILLUPS, 0,
		autoCompleteFillUpCharacters.c_str());

	sprintf(key, "autocomplete.%s.ignorecase", "*");
	sval = props.GetNewExpandString(key);
	autoCompleteIgnoreCase = sval == "1";
	sprintf(key, "autocomplete.%s.ignorecase", language.c_str());
	sval = props.GetNewExpandString(key);
	if (sval != "")
		autoCompleteIgnoreCase = sval == "1";
	wEditor.Call(SCI_AUTOCSETIGNORECASE, autoCompleteIgnoreCase ? 1 : 0);
	wOutput.Call(SCI_AUTOCSETIGNORECASE, 1);

	int autoCChooseSingle = props.GetInt("autocomplete.choose.single");
	wEditor.Call(SCI_AUTOCSETCHOOSESINGLE, autoCChooseSingle);

	wEditor.Call(SCI_AUTOCSETCANCELATSTART, 0);
	wEditor.Call(SCI_AUTOCSETDROPRESTOFWORD, 0);

	if (firstPropertiesRead) {
		ReadPropertiesInitial();
	}

	ReadFontProperties();

	wEditor.Call(SCI_SETPRINTMAGNIFICATION, props.GetInt("print.magnification"));
	wEditor.Call(SCI_SETPRINTCOLOURMODE, props.GetInt("print.colour.mode"));

	jobQueue.clearBeforeExecute = props.GetInt("clear.before.execute");
	jobQueue.timeCommands = props.GetInt("time.commands");

	int blankMarginLeft = props.GetInt("blank.margin.left", 1);
	int blankMarginRight = props.GetInt("blank.margin.right", 1);
	wEditor.Call(SCI_SETMARGINLEFT, 0, blankMarginLeft);
	wEditor.Call(SCI_SETMARGINRIGHT, 0, blankMarginRight);
	wOutput.Call(SCI_SETMARGINLEFT, 0, blankMarginLeft);
	wOutput.Call(SCI_SETMARGINRIGHT, 0, blankMarginRight);

	marginWidth = props.GetInt("margin.width");
	if (marginWidth == 0)
		marginWidth = marginWidthDefault;
	wEditor.Call(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);

	const std::string lineMarginProp = props.GetString("line.margin.width");
	lineNumbersWidth = atoi(lineMarginProp.c_str());
	if (lineNumbersWidth == 0)
		lineNumbersWidth = lineNumbersWidthDefault;
	lineNumbersExpand = lineMarginProp.find('+') != std::string::npos;

	SetLineNumberWidth();

	bufferedDraw = props.GetInt("buffered.draw", 1);
	wEditor.Call(SCI_SETBUFFEREDDRAW, bufferedDraw);
	wOutput.Call(SCI_SETBUFFEREDDRAW, bufferedDraw);

	int phasesDraw = props.GetInt("phases.draw", -1);
	if (phasesDraw < 0 || phasesDraw > SC_PHASES_MULTIPLE)
		phasesDraw = props.GetInt("two.phase.draw", 1) ? SC_PHASES_TWO : SC_PHASES_ONE;
	wEditor.Call(SCI_SETPHASESDRAW, phasesDraw);
	wOutput.Call(SCI_SETPHASESDRAW, phasesDraw);

	wEditor.Call(SCI_SETLAYOUTCACHE, props.GetInt("cache.layout", SC_CACHE_CARET));
	wOutput.Call(SCI_SETLAYOUTCACHE, props.GetInt("output.cache.layout", SC_CACHE_CARET));

	bracesCheck = props.GetInt("braces.check");
	bracesSloppy = props.GetInt("braces.sloppy");

	wEditor.Call(SCI_SETCHARSDEFAULT);
	wordCharacters = props.GetNewExpandString("word.characters.", fileNameForExtension.c_str());
	if (wordCharacters.length()) {
		wEditor.CallString(SCI_SETWORDCHARS, 0, wordCharacters.c_str());
	} else {
		wordCharacters = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	}

	whitespaceCharacters = props.GetNewExpandString("whitespace.characters.", fileNameForExtension.c_str());
	if (whitespaceCharacters.length()) {
		wEditor.CallString(SCI_SETWHITESPACECHARS, 0, whitespaceCharacters.c_str());
	}

	const std::string viewIndentExamine = GetFileNameProperty("view.indentation.examine");
	indentExamine = viewIndentExamine.length() ? (atoi(viewIndentExamine.c_str())) : SC_IV_REAL;
	wEditor.Call(SCI_SETINDENTATIONGUIDES, props.GetInt("view.indentation.guides") ?
		indentExamine : SC_IV_NONE);

	wEditor.Call(SCI_SETTABINDENTS, props.GetInt("tab.indents", 1));
	wEditor.Call(SCI_SETBACKSPACEUNINDENTS, props.GetInt("backspace.unindents", 1));

	wEditor.Call(SCI_CALLTIPUSESTYLE, 32);

	indentOpening = props.GetInt("indent.opening");
	indentClosing = props.GetInt("indent.closing");
	indentMaintain = atoi(props.GetNewExpandString("indent.maintain.", fileNameForExtension.c_str()).c_str());

	const std::string lookback = props.GetNewExpandString("statement.lookback.", fileNameForExtension.c_str());
	statementLookback = atoi(lookback.c_str());
	statementIndent = GetStyleAndWords("statement.indent.");
	statementEnd = GetStyleAndWords("statement.end.");
	blockStart = GetStyleAndWords("block.start.");
	blockEnd = GetStyleAndWords("block.end.");

	struct {
		const char *propName;
		PreProcKind ppc;
	} propToPPC[] = {
		{"preprocessor.start.", ppcStart},
		{"preprocessor.middle.", ppcMiddle},
		{"preprocessor.end.", ppcEnd},
	};
	std::string list = props.GetNewExpandString("preprocessor.symbol.", fileNameForExtension.c_str());
	preprocessorSymbol = list.empty() ? 0 : list[0];
	preprocOfString.clear();
	for (size_t iPreproc = 0; iPreproc < ELEMENTS(propToPPC); iPreproc++) {
		list = props.GetNewExpandString(propToPPC[iPreproc].propName, fileNameForExtension.c_str());
		std::vector<std::string> words = StringSplit(list, ' ');
		for (std::vector<std::string>::iterator it = words.begin(); it != words.end(); ++it) {
			preprocOfString[*it] = propToPPC[iPreproc].ppc;
		}
	}

	memFiles.AppendList(props.GetNewExpandString("find.files").c_str());

	wEditor.Call(SCI_SETWRAPVISUALFLAGS, props.GetInt("wrap.visual.flags"));
	wEditor.Call(SCI_SETWRAPVISUALFLAGSLOCATION, props.GetInt("wrap.visual.flags.location"));
 	wEditor.Call(SCI_SETWRAPSTARTINDENT, props.GetInt("wrap.visual.startindent"));
 	wEditor.Call(SCI_SETWRAPINDENTMODE, props.GetInt("wrap.indent.mode"));

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

	wEditor.Call(SCI_SETFOLDFLAGS, props.GetInt("fold.flags"));

	// To put the folder markers in the line number region
	//wEditor.Call(SCI_SETMARGINMASKN, 0, SC_MASK_FOLDERS);

	wEditor.Call(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD);

	if (0==props.GetInt("undo.redo.lazy")) {
		// Trap for insert/delete notifications (also fired by undo
		// and redo) so that the buttons can be enabled if needed.
		wEditor.Call(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT
			| SC_LASTSTEPINUNDOREDO | wEditor.Call(SCI_GETMODEVENTMASK, 0));

		//SC_LASTSTEPINUNDOREDO is probably not needed in the mask; it
		//doesn't seem to fire as an event of its own; just modifies the
		//insert and delete events.
	}

	// Create a margin column for the folding symbols
	wEditor.Call(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);

	foldMarginWidth = props.GetInt("fold.margin.width");
	if (foldMarginWidth == 0)
		foldMarginWidth = foldMarginWidthDefault;
	wEditor.Call(SCI_SETMARGINWIDTHN, 2, foldMargin ? foldMarginWidth : 0);

	wEditor.Call(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
	wEditor.Call(SCI_SETMARGINSENSITIVEN, 2, 1);

	// Enable/disable highlight for current folding bloc (smallest one that contains the caret)
	int isHighlightEnabled = props.GetInt("fold.highlight", 0);
	// Define the colour of highlight
	std::string foldBlockHighlight = props.GetString("fold.highlight.colour");
	if (foldBlockHighlight.length() == 0) {
		//Set default colour for highlight
		foldBlockHighlight = "#FF0000";
	}
	Colour colourFoldBlockHighlight = ColourFromString(foldBlockHighlight);
	switch (props.GetInt("fold.symbols")) {
	case 0:
		// Arrow pointing right for contracted folders, arrow pointing down for expanded
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_ARROWDOWN,
					 ColourRGB(0, 0, 0), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_ARROW,
					 ColourRGB(0, 0, 0), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY,
					 ColourRGB(0, 0, 0), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY,
					 ColourRGB(0, 0, 0), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY,
					 ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY,
					 ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY,
					 ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		// The highlight is disabled for arrow.
		wEditor.Call(SCI_MARKERENABLEHIGHLIGHT, false);
		break;
	case 1:
		// Plus for contracted folders, minus for expanded
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_PLUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0, 0, 0), colourFoldBlockHighlight);
		// The highlight is disabled for plus/minus.
		wEditor.Call(SCI_MARKERENABLEHIGHLIGHT, false);
		break;
	case 2:
		// Like a flattened tree control using circular headers and curved joins
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x40, 0x40, 0x40), colourFoldBlockHighlight);
		wEditor.Call(SCI_MARKERENABLEHIGHLIGHT, isHighlightEnabled);
		break;
	case 3:
		// Like a flattened tree control using square headers
		DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER,
		             ColourRGB(0xff, 0xff, 0xff), ColourRGB(0x80, 0x80, 0x80), colourFoldBlockHighlight);
		wEditor.Call(SCI_MARKERENABLEHIGHLIGHT, isHighlightEnabled);
		break;
	}

	wEditor.Call(SCI_MARKERSETFORE, markerBookmark,
		ColourOfProperty(props, "bookmark.fore", ColourRGB(0xbe, 0, 0)));
	wEditor.Call(SCI_MARKERSETBACK, markerBookmark,
		ColourOfProperty(props, "bookmark.back", ColourRGB(0xe2, 0x40, 0x40)));
	wEditor.Call(SCI_MARKERSETALPHA, markerBookmark,
		props.GetInt("bookmark.alpha", SC_ALPHA_NOALPHA));
	const std::string bookMarkXPM = props.GetString("bookmark.pixmap");
	if (bookMarkXPM.length()) {
		wEditor.CallString(SCI_MARKERDEFINEPIXMAP, markerBookmark,
			bookMarkXPM.c_str());
	} else if (props.GetString("bookmark.fore").length()) {
		wEditor.Call(SCI_MARKERDEFINE, markerBookmark, SC_MARK_BOOKMARK);
	} else {
		// No bookmark.fore setting so display default pixmap.
		wEditor.CallString(SCI_MARKERDEFINEPIXMAP, markerBookmark,
			reinterpret_cast<char *>(bookmarkBluegem));
	}

	wEditor.Call(SCI_SETSCROLLWIDTH, props.GetInt("horizontal.scroll.width", 2000));
	wEditor.Call(SCI_SETSCROLLWIDTHTRACKING, props.GetInt("horizontal.scroll.width.tracking", 1));
	wOutput.Call(SCI_SETSCROLLWIDTH, props.GetInt("output.horizontal.scroll.width", 2000));
	wOutput.Call(SCI_SETSCROLLWIDTHTRACKING, props.GetInt("output.horizontal.scroll.width.tracking", 1));

	// Do these last as they force a style refresh
	wEditor.Call(SCI_SETHSCROLLBAR, props.GetInt("horizontal.scrollbar", 1));
	wOutput.Call(SCI_SETHSCROLLBAR, props.GetInt("output.horizontal.scrollbar", 1));

	wEditor.Call(SCI_SETENDATLASTLINE, props.GetInt("end.at.last.line", 1));
	wEditor.Call(SCI_SETCARETSTICKY, props.GetInt("caret.sticky", 0));

	// Clear all previous indicators.
	wEditor.Call(SCI_SETINDICATORCURRENT, indicatorHighlightCurrentWord);
	wEditor.Call(SCI_INDICATORCLEARRANGE, 0, wEditor.Call(SCI_GETLENGTH));
	wOutput.Call(SCI_SETINDICATORCURRENT, indicatorHighlightCurrentWord);
	wOutput.Call(SCI_INDICATORCLEARRANGE, 0, wOutput.Call(SCI_GETLENGTH));
	currentWordHighlight.statesOfDelay = currentWordHighlight.noDelay;

	currentWordHighlight.isEnabled = props.GetInt("highlight.current.word", 0) == 1;
	if (currentWordHighlight.isEnabled) {
		const std::string highlightCurrentWordIndicatorString = props.GetString("highlight.current.word.indicator");
		IndicatorDefinition highlightCurrentWordIndicator(highlightCurrentWordIndicatorString.c_str());
		if (highlightCurrentWordIndicatorString.length() == 0) {
			highlightCurrentWordIndicator.style = INDIC_ROUNDBOX;
			std::string highlightCurrentWordColourString = props.GetString("highlight.current.word.colour");
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
			wEditor.Send(SCI_PRIVATELEXERCALL, i - STYLE_MAX, reinterpret_cast<sptr_t>(propStr));
			props.Set(key, static_cast<const char *>(propStr));
		}
		languageName = "lpeg";
	}

	// Set styles
	// For each window set the global default style, then the language default style, then the other global styles, then the other language styles

	int fontQuality = props.GetInt("font.quality");
	wEditor.Call(SCI_SETFONTQUALITY, fontQuality);
	wOutput.Call(SCI_SETFONTQUALITY, fontQuality);

	wEditor.Call(SCI_STYLERESETDEFAULT, 0, 0);
	wOutput.Call(SCI_STYLERESETDEFAULT, 0, 0);

	sprintf(key, "style.%s.%0d", "*", STYLE_DEFAULT);
	std::string sval = props.GetNewExpandString(key);
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	sprintf(key, "style.%s.%0d", languageName, STYLE_DEFAULT);
	sval = props.GetNewExpandString(key);
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());

	wEditor.Call(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wEditor, "*");
	SetStyleFor(wEditor, languageName);
	if (props.GetInt("error.inline")) {
		wEditor.Call(SCI_RELEASEALLEXTENDEDSTYLES, 0, 0);
		diagnosticStyleStart = wEditor.Call(SCI_ALLOCATEEXTENDEDSTYLES, diagnosticStyles, 0);
		SetStyleBlock(wEditor, "error", diagnosticStyleStart, diagnosticStyleStart+diagnosticStyles-1);
	}

	int diffToSecondary = static_cast<int>(wEditor.Call(SCI_DISTANCETOSECONDARYSTYLES));
	for (unsigned int baseStyle=0; baseStyle<subStyleBases.size(); baseStyle++) {
		int subStylesStart = wEditor.Call(SCI_GETSUBSTYLESSTART, subStyleBases[baseStyle]);
		int subStylesLength = wEditor.Call(SCI_GETSUBSTYLESLENGTH, subStyleBases[baseStyle]);
		for (int subStyle=0; subStyle<subStylesLength; subStyle++) {
			for (int active=0; active<(diffToSecondary?2:1); active++) {
				int activity = active * diffToSecondary;
				sprintf(key, "style.%s.%0d.%0d", languageName, subStyleBases[baseStyle] + activity, subStyle+1);
				sval = props.GetNewExpandString(key);
				SetOneStyle(wEditor, subStylesStart + subStyle + activity, sval.c_str());
			}
		}
	}

	// Turn grey while loading
	if (CurrentBuffer()->lifeState == Buffer::reading)
		wEditor.Call(SCI_STYLESETBACK, STYLE_DEFAULT, 0xEEEEEE);

	wOutput.Call(SCI_STYLECLEARALL, 0, 0);

	sprintf(key, "style.%s.%0d", "errorlist", STYLE_DEFAULT);
	sval = props.GetNewExpandString(key);
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	wOutput.Call(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wOutput, "*");
	SetStyleFor(wOutput, "errorlist");

	if (CurrentBuffer()->useMonoFont) {
		sval = props.GetExpandedString("font.monospace");
		StyleDefinition sd(sval.c_str());
		for (int style = 0; style <= STYLE_MAX; style++) {
			if (style != STYLE_LINENUMBER) {
				if (sd.specified & StyleDefinition::sdFont) {
					wEditor.CallString(SCI_STYLESETFONT, style, sd.font.c_str());
				}
				if (sd.specified & StyleDefinition::sdSize) {
					wEditor.Call(SCI_STYLESETSIZEFRACTIONAL, style, sd.FractionalSize());
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
	int sizeHorizontal = props.GetInt("output.horizontal.size", 0);
	int sizeVertical = props.GetInt("output.vertical.size", 0);
	int hideOutput = props.GetInt("output.initial.hide", 0);
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
	wEditor.Call(SCI_SETINDENTATIONGUIDES, props.GetInt("view.indentation.guides") ?
		indentExamine : SC_IV_NONE);

	wEditor.Call(SCI_SETVIEWEOL, props.GetInt("view.eol"));
	wEditor.Call(SCI_SETZOOM, props.GetInt("magnification"));
	wOutput.Call(SCI_SETZOOM, props.GetInt("output.magnification"));
	wEditor.Call(SCI_SETWRAPMODE, wrap ? wrapStyle : SC_WRAP_NONE);
	wOutput.Call(SCI_SETWRAPMODE, wrapOutput ? wrapStyle : SC_WRAP_NONE);

	std::string menuLanguageProp = props.GetNewExpandString("menu.language");
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
		size_t pipes = std::count(shortCutProp.begin(), shortCutProp.end(), '|');
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
				Open(extlua.c_str(), ofQuiet);
			}
			break;
		}
	case IDM_OPENDIRECTORYPROPERTIES: {
			propfile = GetDirectoryPropertiesFileName();
			bool alreadyExists = propfile.Exists();
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
	// Otherwise we might have entered a number as command to access a "SCI_" command
	return atoi(commandName.c_str());
}
