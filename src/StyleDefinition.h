// SciTE - Scintilla based Text Editor
/** @file StyleDefinition.h
 ** Definition of style aggregate and helper functions.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

class StyleDefinition {
public:
	std::string font;
	float sizeFractional;
	int size;
	std::string fore;
	std::string back;
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

int IntFromHexDigit(int ch);
int IntFromHexByte(const char *hexByte);

Colour ColourFromString(const std::string &s);

struct IndicatorDefinition {
	int style;
	long colour;
	int fillAlpha;
	int outlineAlpha;
	bool under;
	IndicatorDefinition(const char *definition);
	bool ParseIndicatorDefinition(const char *definition);
};
