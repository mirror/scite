// SciTE - Scintilla based Text Editor
/** @file LexillaLibrary.h
 ** Interface to loadable lexers.
 **/
// Copyright 2019 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

bool LexillaLoad(FilePathSet sharedLibraries);
Scintilla::ILexer5 *LexillaCreateLexer(const std::string &languageName);
