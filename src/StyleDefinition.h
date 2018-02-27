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
	explicit StyleDefinition(std::string_view definition);
	bool ParseStyleDefinition(std::string_view definition);
	long ForeAsLong() const;
	long BackAsLong() const;
	int FractionalSize() const noexcept;
	bool IsBold() const noexcept;
};

typedef long Colour;

inline constexpr Colour ColourRGB(unsigned int red, unsigned int green, unsigned int blue) noexcept {
	return red | (green << 8) | (blue << 16);
}

int IntFromHexDigit(int ch) noexcept;
int IntFromHexByte(std::string_view hexByte) noexcept;

Colour ColourFromString(const std::string &s);

struct IndicatorDefinition {
	int style;
	long colour;
	int fillAlpha;
	int outlineAlpha;
	bool under;
	explicit IndicatorDefinition(std::string_view definition);
	bool ParseIndicatorDefinition(std::string_view definition);
};
