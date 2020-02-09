// SciTE - Scintilla based Text Editor
/** @file LexillaLibrary.h
 ** Interface to loadable lexers.
 **/
// Copyright 2019 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef LEXILLALIBRARY_H
#define LEXILLALIBRARY_H

bool LexillaLoad(FilePathSet sharedLibraries);
Scintilla::ILexer5 *LexillaCreateLexer(const std::string &languageName);

#endif
