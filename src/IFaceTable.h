// SciTE - Scintilla based Text Editor
/** @file IFaceTable.h
 ** SciTE iface function and constant descriptors.
 **/
// Copyright 1998-2004 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef IFACETABLE_H
#define IFACETABLE_H

enum IFaceType {
	iface_void,
	iface_int,
	iface_length,
	iface_bool,
	iface_position,
	iface_colour,
	iface_keymod,
	iface_string,
	iface_stringresult,
	iface_cells,
	iface_textrange,
	iface_findtext,
	iface_formatrange
};	

struct IFaceConstant {
	char *name;
	int value;
};

struct IFaceFunction {
	char *name;
	int value;
	IFaceType returnType;
	IFaceType paramType[2];
};

class IFaceTable {
public:
	static const IFaceFunction * const functions;
	static const IFaceConstant * const constants;

	static const int functionCount;
	static const int constantCount;

	static int FindConstant(const char *name);
	static int FindFunction(const char *name);
	static int FindFunctionByConstantName(const char *name);
};

#endif
