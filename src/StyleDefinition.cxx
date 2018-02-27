// SciTE - Scintilla based Text Editor
/** @file StyleDefinition.cxx
 ** Implementation of style aggregate.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <cassert>

#include <tuple>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

#include "Scintilla.h"

#include "GUI.h"
#include "StringHelpers.h"
#include "StyleDefinition.h"

namespace {

typedef std::tuple<std::string_view, std::string_view> ViewPair;

// Split view around first separator returning the portion before and after the separator.
// If the separator is not present then return whole view and an empty view.
ViewPair ViewSplit(std::string_view view, char separator) noexcept {
	const size_t sepPos = view.find_first_of(separator);
	std::string_view first = view.substr(0, sepPos);
	std::string_view second = sepPos == (std::string_view::npos) ? "" : view.substr(sepPos + 1);
	return { first, second };
}

}

StyleDefinition::StyleDefinition(std::string_view definition) :
		sizeFractional(10.0), size(10), fore("#000000"), back("#FFFFFF"),
		weight(SC_WEIGHT_NORMAL), italics(false), eolfilled(false), underlined(false),
		caseForce(SC_CASE_MIXED),
		visible(true), changeable(true),
		specified(sdNone) {
	ParseStyleDefinition(definition);
}

bool StyleDefinition::ParseStyleDefinition(std::string_view definition) {
	if (definition.empty()) {
		return false;
	}
	while (!definition.empty()) {
		// Find attribute separator ',' and select front attribute
		const ViewPair optionRest = ViewSplit(definition, ',');
		const std::string_view option = std::get<0>(optionRest);
		definition = std::get<1>(optionRest);
		// Find value separator ':' and break into name and value
		const auto [optionName, optionValue] = ViewSplit(option, ':');

		if (optionName == "italics") {
			specified = static_cast<flags>(specified | sdItalics);
			italics = true;
		}
		if (optionName == "notitalics") {
			specified = static_cast<flags>(specified | sdItalics);
			italics = false;
		}
		if (optionName == "bold") {
			specified = static_cast<flags>(specified | sdWeight);
			weight = SC_WEIGHT_BOLD;
		}
		if (optionName == "notbold") {
			specified = static_cast<flags>(specified | sdWeight);
			weight = SC_WEIGHT_NORMAL;
		}
		if ((optionName == "weight") && !optionValue.empty()) {
			specified = static_cast<flags>(specified | sdWeight);
			weight = std::stoi(std::string(optionValue));
		}
		if ((optionName == "font") && !optionValue.empty()) {
			specified = static_cast<flags>(specified | sdFont);
			font = optionValue;
			std::replace(font.begin(), font.end(), '|', ',');
		}
		if ((optionName == "fore") && !optionValue.empty()) {
			specified = static_cast<flags>(specified | sdFore);
			fore = optionValue;
		}
		if ((optionName == "back") && !optionValue.empty()) {
			specified = static_cast<flags>(specified | sdBack);
			back = optionValue;
		}
		if ((optionName == "size") && !optionValue.empty()) {
			specified = static_cast<flags>(specified | sdSize);
			sizeFractional = std::stof(std::string(optionValue));
			size = static_cast<int>(sizeFractional);
		}
		if (optionName == "eolfilled") {
			specified = static_cast<flags>(specified | sdEOLFilled);
			eolfilled = true;
		}
		if (optionName == "noteolfilled") {
			specified = static_cast<flags>(specified | sdEOLFilled);
			eolfilled = false;
		}
		if (optionName == "underlined") {
			specified = static_cast<flags>(specified | sdUnderlined);
			underlined = true;
		}
		if (optionName == "notunderlined") {
			specified = static_cast<flags>(specified | sdUnderlined);
			underlined = false;
		}
		if (optionName == "case") {
			specified = static_cast<flags>(specified | sdCaseForce);
			caseForce = SC_CASE_MIXED;
			if (!optionValue.empty()) {
				if (optionValue.front() == 'u')
					caseForce = SC_CASE_UPPER;
				else if (optionValue.front() == 'l')
					caseForce = SC_CASE_LOWER;
			}
		}
		if (optionName == "visible") {
			specified = static_cast<flags>(specified | sdVisible);
			visible = true;
		}
		if (optionName == "notvisible") {
			specified = static_cast<flags>(specified | sdVisible);
			visible = false;
		}
		if (optionName == "changeable") {
			specified = static_cast<flags>(specified | sdChangeable);
			changeable = true;
		}
		if (optionName == "notchangeable") {
			specified = static_cast<flags>(specified | sdChangeable);
			changeable = false;
		}
	}
	return true;
}

long StyleDefinition::ForeAsLong() const {
	return ColourFromString(fore);
}

long StyleDefinition::BackAsLong() const {
	return ColourFromString(back);
}

int StyleDefinition::FractionalSize() const noexcept {
	return static_cast<int>(sizeFractional * SC_FONT_SIZE_MULTIPLIER);
}

bool StyleDefinition::IsBold() const noexcept {
	return weight > SC_WEIGHT_NORMAL;
}

int IntFromHexDigit(int ch) noexcept {
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

int IntFromHexByte(std::string_view hexByte) noexcept {
	return IntFromHexDigit(hexByte[0]) * 16 + IntFromHexDigit(hexByte[1]);
}

Colour ColourFromString(const std::string &s) {
	if (s.length() >= 7) {
		const int r = IntFromHexByte(&s[1]);
		const int g = IntFromHexByte(&s[3]);
		const int b = IntFromHexByte(&s[5]);
		return ColourRGB(r, g, b);
	} else {
		return 0;
	}
}

IndicatorDefinition::IndicatorDefinition(std::string_view definition) :
	style(INDIC_PLAIN), colour(0), fillAlpha(30), outlineAlpha(50), under(false) {
	ParseIndicatorDefinition(definition);
}

bool IndicatorDefinition::ParseIndicatorDefinition(std::string_view definition) {
	if (definition.empty()) {
		return false;
	}
	struct NameValue {
		std::string_view name;
		int value;
	};
	const NameValue indicStyleNames[] = {
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
		{ "compositionthin", INDIC_COMPOSITIONTHIN },
		{ "fullbox", INDIC_FULLBOX },
		{ "textfore", INDIC_TEXTFORE },
		{ "point", INDIC_POINT },
		{ "pointcharacter", INDIC_POINTCHARACTER },
	};

	std::string val(definition);
	LowerCaseAZ(val);
	std::string_view indicatorDefinition = val;
	while (!indicatorDefinition.empty()) {
		// Find attribute separator ',' and select front attribute
		const ViewPair optionRest = ViewSplit(indicatorDefinition, ',');
		const std::string_view option = std::get<0>(optionRest);
		indicatorDefinition = std::get<1>(optionRest);
		// Find value separator ':' and break into name and value
		const auto [optionName, optionValue] = ViewSplit(option, ':');

		if (!optionValue.empty() && (optionName == "style")) {
			bool found = false;
			for (const NameValue &indicStyleName : indicStyleNames) {
				if (optionValue == indicStyleName.name) {
					style = indicStyleName.value;
					found = true;
				}
			}
			if (!found)
				style = std::stoi(std::string(optionValue));
		}
		if (!optionValue.empty() && ((optionName == "colour") || (optionName == "color"))) {
			colour = ColourFromString(std::string(optionValue));
		}
		if (!optionValue.empty() && (optionName == "fillalpha")) {
			fillAlpha = std::stoi(std::string(optionValue));
		}
		if (!optionValue.empty() && (optionName == "outlinealpha")) {
			outlineAlpha = std::stoi(std::string(optionValue));
		}
		if (optionName == "under") {
			under = true;
		}
		if (optionName == "notunder") {
			under = false;
		}
	}
	return true;
}
