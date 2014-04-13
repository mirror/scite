// SciTE - Scintilla based Text Editor
/** @file StyleDefinition.cxx
 ** Implementation of style aggregate.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>
#include <algorithm>

#include "Scintilla.h"

#include "GUI.h"
#include "SString.h"
#include "StringHelpers.h"
#include "StyleDefinition.h"

StyleDefinition::StyleDefinition(const char *definition) :
		sizeFractional(10.0), size(10), fore("#000000"), back("#FFFFFF"),
		weight(SC_WEIGHT_NORMAL), italics(false), eolfilled(false), underlined(false),
		caseForce(SC_CASE_MIXED),
		visible(true), changeable(true),
		specified(sdNone) {
	ParseStyleDefinition(definition);
}

bool StyleDefinition::ParseStyleDefinition(const char *definition) {
	if (definition == NULL || *definition == '\0') {
		return false;
	}
	char *val = StringDup(definition);
	char *opt = val;
	while (opt) {
		// Find attribute separator
		char *cpComma = strchr(opt, ',');
		if (cpComma) {
			// If found, we terminate the current attribute (opt) string
			*cpComma = '\0';
		}
		// Find attribute name/value separator
		char *colon = strchr(opt, ':');
		if (colon) {
			// If found, we terminate the current attribute name and point on the value
			*colon++ = '\0';
		}
		if (0 == strcmp(opt, "italics")) {
			specified = static_cast<flags>(specified | sdItalics);
			italics = true;
		}
		if (0 == strcmp(opt, "notitalics")) {
			specified = static_cast<flags>(specified | sdItalics);
			italics = false;
		}
		if (0 == strcmp(opt, "bold")) {
			specified = static_cast<flags>(specified | sdWeight);
			weight = SC_WEIGHT_BOLD;
		}
		if (0 == strcmp(opt, "notbold")) {
			specified = static_cast<flags>(specified | sdWeight);
			weight = SC_WEIGHT_NORMAL;
		}
		if ((0 == strcmp(opt, "weight")) && colon) {
			specified = static_cast<flags>(specified | sdWeight);
			weight = atoi(colon);
		}
		if (0 == strcmp(opt, "font")) {
			specified = static_cast<flags>(specified | sdFont);
			font = colon;
			std::replace(font.begin(), font.end(), '|', ',');
		}
		if (0 == strcmp(opt, "fore")) {
			specified = static_cast<flags>(specified | sdFore);
			fore = colon;
		}
		if (0 == strcmp(opt, "back")) {
			specified = static_cast<flags>(specified | sdBack);
			back = colon;
		}
		if ((0 == strcmp(opt, "size")) && colon) {
			specified = static_cast<flags>(specified | sdSize);
			sizeFractional = static_cast<float>(atof(colon));
			size = static_cast<int>(sizeFractional);
		}
		if (0 == strcmp(opt, "eolfilled")) {
			specified = static_cast<flags>(specified | sdEOLFilled);
			eolfilled = true;
		}
		if (0 == strcmp(opt, "noteolfilled")) {
			specified = static_cast<flags>(specified | sdEOLFilled);
			eolfilled = false;
		}
		if (0 == strcmp(opt, "underlined")) {
			specified = static_cast<flags>(specified | sdUnderlined);
			underlined = true;
		}
		if (0 == strcmp(opt, "notunderlined")) {
			specified = static_cast<flags>(specified | sdUnderlined);
			underlined = false;
		}
		if (0 == strcmp(opt, "case")) {
			specified = static_cast<flags>(specified | sdCaseForce);
			caseForce = SC_CASE_MIXED;
			if (colon) {
				if (*colon == 'u')
					caseForce = SC_CASE_UPPER;
				else if (*colon == 'l')
					caseForce = SC_CASE_LOWER;
			}
		}
		if (0 == strcmp(opt, "visible")) {
			specified = static_cast<flags>(specified | sdVisible);
			visible = true;
		}
		if (0 == strcmp(opt, "notvisible")) {
			specified = static_cast<flags>(specified | sdVisible);
			visible = false;
		}
		if (0 == strcmp(opt, "changeable")) {
			specified = static_cast<flags>(specified | sdChangeable);
			changeable = true;
		}
		if (0 == strcmp(opt, "notchangeable")) {
			specified = static_cast<flags>(specified | sdChangeable);
			changeable = false;
		}
		if (cpComma)
			opt = cpComma + 1;
		else
			opt = 0;
	}
	delete []val;
	return true;
}

long StyleDefinition::ForeAsLong() const {
	return ColourFromString(fore);
}

long StyleDefinition::BackAsLong() const {
	return ColourFromString(back);
}

int StyleDefinition::FractionalSize() const {
	return static_cast<int>(sizeFractional * SC_FONT_SIZE_MULTIPLIER);
}

bool StyleDefinition::IsBold() const {
	return weight > SC_WEIGHT_NORMAL;
}

int IntFromHexDigit(int ch) {
	if ((ch >= '0') && (ch <= '9')) {
		return ch - '0';
	} else if (ch >= 'A' && ch <= 'F') {
		return ch - 'A' + 10;
	} else if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	} else {
		return 0;
	}
}

int IntFromHexByte(const char *hexByte) {
	return IntFromHexDigit(hexByte[0]) * 16 + IntFromHexDigit(hexByte[1]);
}

Colour ColourFromString(const std::string &s) {
	if (s.length()) {
		int r = IntFromHexByte(s.c_str() + 1);
		int g = IntFromHexByte(s.c_str() + 3);
		int b = IntFromHexByte(s.c_str() + 5);
		return ColourRGB(r, g, b);
	} else {
		return 0;
	}
}

IndicatorDefinition::IndicatorDefinition(const char *definition) :
	style(INDIC_PLAIN), colour(0), fillAlpha(30), outlineAlpha(50), under(false) {
	ParseIndicatorDefinition(definition);
}

bool IndicatorDefinition::ParseIndicatorDefinition(const char *definition) {
	if (definition == NULL || *definition == '\0') {
		return false;
	}
	struct {
		const char *name;
		int value;
	} indicStyleNames[] = {
		{ "plain", INDIC_PLAIN },
		{ "squiggle", INDIC_SQUIGGLE },
		{ "tt", INDIC_TT },
		{ "diagonal", INDIC_DIAGONAL },
		{ "strike", INDIC_STRIKE },
		{ "hidden", INDIC_HIDDEN },
		{ "box", INDIC_BOX },
		{ "roundbox", INDIC_ROUNDBOX },
		{ "straightbox", INDIC_STRAIGHTBOX },
		{ "dash", INDIC_DASH },
		{ "dots", INDIC_DOTS },
		{ "squigglelow", INDIC_SQUIGGLELOW },
		{ "dotbox", INDIC_DOTBOX },
		{ "squigglepixmap", INDIC_SQUIGGLEPIXMAP },
		{ "compositionthick", INDIC_COMPOSITIONTHICK },
	};

	std::string val(definition);
	LowerCaseAZ(val);
	char *opt = &val[0];
	while (opt) {
		// Find attribute separator
		char *cpComma = strchr(opt, ',');
		if (cpComma) {
			// If found, we terminate the current attribute (opt) string
			*cpComma = '\0';
		}
		// Find attribute name/value separator
		char *colon = strchr(opt, ':');
		if (colon) {
			// If found, we terminate the current attribute name and point on the value
			*colon++ = '\0';
		}
		if (colon && (0 == strcmp(opt, "style"))) {
			bool found = false;
			for (size_t i=0;i<ELEMENTS(indicStyleNames);i++) {
				if ((indicStyleNames[i].name) && (0 == strcmp(colon, indicStyleNames[i].name))) {
					style = indicStyleNames[i].value;
					found = true;
				}
			}
			if (!found)
				style = atoi(colon);
		}
		if ((0 == strcmp(opt, "colour")) || (0 == strcmp(opt, "color"))) {
			colour = ColourFromString(std::string(colon));
		}
		if (colon && (0 == strcmp(opt, "fillalpha"))) {
			fillAlpha = atoi(colon);
		}
		if (colon && (0 == strcmp(opt, "outlinealpha"))) {
			outlineAlpha = atoi(colon);
		}
		if (0 == strcmp(opt, "under")) {
			under = true;
		}
		if (0 == strcmp(opt, "notunder")) {
			under = false;
		}
		if (cpComma)
			opt = cpComma + 1;
		else
			opt = 0;
	}
	return true;
}
