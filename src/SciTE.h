// SciTE - small text editor to debug Scintilla
// SciTE.h - define command IDs used within SciTE
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef SCITE_H
#define SCITE_H

// menu defines
#define IDM_MRUFILE	90

#define IDM_NEW		101
#define IDM_OPEN    102
#define IDM_CLOSE    103
#define IDM_SAVE    104
#define IDM_SAVEAS  105
#define IDM_REVERT	106
#define IDM_PRINT   107
#define IDM_PRINTSETUP   108
#define IDM_ABOUT   109
#define IDM_QUIT    110
#define IDM_NEXTFILE    111
#define IDM_MRU_SEP	112
#define IDM_SAVEASHTML 113
#define IDM_PREVFILE    114

#define IDM_UNDO	120
#define IDM_REDO	121
#define IDM_CUT		122
#define IDM_COPY	123
#define IDM_PASTE	124
#define IDM_CLEAR	125
#define IDM_SELECTALL 127
#define IDM_FIND 128
#define IDM_FINDNEXT 129
#define IDM_FINDINFILES 130
#define IDM_REPLACE 131
#define IDM_GOTO 132
#define IDM_UPRCASE 133
#define IDM_LWRCASE 134
#define IDM_VIEWSPACE 135
#define IDM_SPLITVERTICAL 136
#define IDM_NEXTMSG 137
#define IDM_PREVMSG 138
#define IDM_MATCHBRACE 139
#define IDM_COMPLETE 140
#define IDM_EXPAND 141

#define IDM_COMPILE 200
#define IDM_BUILD 201
#define IDM_GO 202
#define IDM_STOPEXECUTE 203
#define IDM_FINISHEDEXECUTE 204

#define IDM_OPENLOCALPROPERTIES 210
#define IDM_OPENUSERPROPERTIES 211
#define IDM_OPENGLOBALPROPERTIES 212

// Dialog control IDs
#define IDGOLINE 220
#define IDABOUTSCINTILLA 221
#define IDFINDWHAT 222
#define IDFILES 223
#define IDDIRECTORY 224
#define IDCURRLINE 225
#define IDLASTLINE 226
#define IDEXTEND 227

#define IDREPLACEWITH 231
#define IDWHOLEWORD 232
#define IDMATCHCASE 233
#define IDDIRECTIONUP 234
#define IDDIRECTIONDOWN 235
#define IDREPLACE 236
#define IDREPLACEALL 237

#define IDM_STEP 240
#define IDM_SELECTIONMARGIN  241
#define IDM_BUFFEREDDRAW  242
#define IDM_LINENUMBERMARGIN  243
#define IDM_USEPALETTE  244
#define IDM_SELMARGIN 245
#define IDM_FOLDMARGIN 246
#define IDM_VIEWEOL 247
#define IDM_SLAVE 248
#define IDM_VIEWSTATUSBAR 249

#define IDM_ACTIVATE    250
#define IDM_TOOLS 260

#define IDM_EOL_CRLF 270
#define IDM_EOL_CR 271
#define IDM_EOL_LF 272
#define IDM_EOL_CONVERT 273

#define IDM_SRCWIN 300
#define IDM_RUNWIN 301

#define IDM_BOOKMARK_TOGGLE 302
#define IDM_BOOKMARK_NEXT 303

// Dialog IDs
#define IDD_FIND 400
#define IDD_REPLACE 401

#endif
