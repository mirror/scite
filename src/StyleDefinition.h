// SciTE - Scintilla based Text Editor
/** @file StyleDefinition.h
 ** Definition of style aggregate and helper functions.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class StyleDefinition {
public:
	SString font;
	float sizeFractional;
	int size;
	SString fore;
	SString back;
	int weight;
	bool italics;
	bool eolfilled;
	bool underlined;
	int caseForce;
	bool visible;
	bool changeable;
	enum flags { sdNone = 0, sdFont = 0x1, sdSize = 0x2, sdFore = 0x4, sdBack = 0x8,
	        sdWeight = 0x10, sdItalics = 0x20, sdEOLFilled = 0x40, sdUnderlined = 0x80,
	        sdCaseForce = 0x100, sdVisible = 0x200, sdChangeable = 0x400} specified;
	StyleDefinition(const char *definition);
	bool ParseStyleDefinition(const char *definition);
	long ForeAsLong() const;
	long BackAsLong() const;
	int FractionalSize() const;
	bool IsBold() const;
};

typedef long Colour;

inline Colour ColourRGB(unsigned int red, unsigned int green, unsigned int blue) {
	return red | (green << 8) | (blue << 16);
}

int IntFromHexByte(const char *hexByte);

Colour ColourFromString(const SString &s);
