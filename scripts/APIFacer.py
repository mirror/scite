#!/usr/bin/env python
# APIFacer.py - regenerate the ScintillaTypes.h, ScintillaMessages.h, ScintillaCall.h,and ScintillaCall.cxx files
# from the Scintilla.iface interface definition file.
# Implemented 2019 by Neil Hodgson neilh@scintilla.org
# Requires Python 3.5 or later

import sys

srcRoot = "../.."

sys.path.append(srcRoot + "/scintilla/scripts")

import Face

from FileGenerator import UpdateFile, Generate, Regenerate, UpdateLineInFile, lineEnd

def IsNumeric(s):
	return all((c in "1234567890_") for c in s)

def PascalCase(s):
	return s.title().replace("_", "")

typeAliases = {
	# Convert iface types to C++ types
	# bool and void are OK as is
	"cells": "const char *",
	"colour": "Colour",
	"findtext": "void *",
	"formatrange": "void *",
	"int": "int",
	"keymod": "int",
	"line": "Line",
	"pointer": "void *",
	"position": "Position",
	"string": "const char *",
	"stringresult": "char *",
	"textrange": "void *",
}

basicTypes = [
	"bool",
	"char *",
	"Colour",
	"const char *",
	"int",
	"intptr_t",
	"Line",
	"Position",
	"void",
	"void *",
]

namespace = "API::"

lineArgNames = [
	"line",
	"lineStart",
	"lineEnd",
	"lines",
	"docLine",
	"displayLine",
]

positionArgNames = [
	"column",
	"lengthClear",
	"lengthDelete",
	"lengthEntered",
	"lengthFill",
	"lengthRange",
	"length",
	"bytes",
	"posStart",
	"relative",
	"space",
]

documentArgNames = [
	"doc",
	"pointer",
]

def ActualTypeName(type, identifier=None):
	if identifier in lineArgNames:
		return "Line"
	if identifier in positionArgNames:
		return "Position"
	if identifier in documentArgNames:
		return "void *"
	if identifier in enumArgNames:
		return namespace + enumArgNames[identifier]
	if type in typeAliases:
		return typeAliases[type]
	else:
		return type

returningMethods = {
	"Column": "Position",
	"CountCharacters": "Position",
	"CountCodeUnits": "Position",
	"EdgeColumn": "Position",
	"FindColumn": "Position",
	"GetLine": "Position",
	"GetCurLine": "Position",
	"GetSelText": "Position",
	"GetStyledText": "Position",
	"TargetText": "Position",
	"GetText": "Position",
	"GetTextRange": "Position",
	"TextLength": "Position",
	"Length": "Position",
	"LineLength": "Position",
	"ReplaceTarget": "Position",
	"ReplaceTargetRE": "Position",
	"SearchInTarget": "Position",
	"SearchNext": "Position",
	"SearchPrev": "Position",
	"WordStartPosition": "Position",
	"WordEndPosition": "Position",
	"MarkerNext": "Line",
	"MarkerPrevious": "Line",
	"GetFirstVisibleLine": "Line",
	"GetLineCount": "Line",
	"LastChild": "Line",
	"FoldParent": "Line",
	"LineFromPosition": "Line",
	"VisibleFromDocLine": "Line",
	"DocLineFromVisible": "Line",
	"WrapCount": "Line",
	"LinesOnScreen": "Line",
	"ContractedFoldNext": "Line",
	"LineFromIndexPosition": "Line",
	"FirstVisibleLine": "Line",
	"LineCount": "Line",
	"DirectFunction": "void *",
	"DirectPointer": "void *",
	"DocPointer": "void *",
	"CharacterPointer": "void *",
	"RangePointer": "void *",
	"CreateDocument": "void *",
	"CreateLoader": "void *",
	"PrivateLexerCall": "void *",

	"ViewWS": "WhiteSpace",
	"TabDrawMode": "TabDrawMode",
	"EOLMode": "EndOfLine",
	"IMEInteraction": "IMEInteraction",
	"StyleGetCase": "CaseVisible",
	"StyleGetCharacterSet": "CharacterSet",
	"StyleGetWeight": "FontWeight",
	"SelAlpha": "Alpha",
	"AdditionalSelAlpha": "Alpha",
	"IndicGetStyle": "IndicatorStyle",
	"IndicGetHoverStyle": "IndicatorStyle",
	"IndicGetFlags": "IndicFlag",
	"IndicGetAlpha": "Alpha",
	"IndicGetOutlineAlpha": "Alpha",
	"HighlightGuide": "Position",
	"IndentationGuides": "IndentView",
	"PrintColourMode": "PrintOption",
	"SearchFlags": "FindOption",
	"FoldLevel": "FoldLevel",
	"FoldDisplayTextGetStyle": "FoldDisplayTextStyle",
	"AutomaticFold": "AutomaticFold",
	"IdleStyling": "IdleStyling",
	"WrapMode": "Wrap",
	"WrapVisualFlags": "WrapVisualFlag",
	"WrapVisualFlagsLocation": "WrapVisualLocation",
	"WrapIndentMode": "WrapIndentMode",
	"LayoutCache": "LineCache",
	"PhasesDraw": "PhasesDraw",
	"FontQuality": "FontQuality",
	"MultiPaste": "MultiPaste",
	"Accessibility": "Accessibility",
	"EdgeMode": "EdgeVisualStyle",
	"DocumentOptions": "DocumentOption",
	"ModEventMask": "ModificationFlags",
	"Status": "Status",
	"Cursor": "CursorShape",
	"PrintWrapMode": "Wrap",
	"SelectionMode": "SelectionMode",
	"AutoCGetCaseInsensitiveBehaviour": "CaseInsensitiveBehaviour",
	"AutoCGetMulti": "MultiAutoComplete",
	"AutoCGetOrder": "Ordering",
	"CaretSticky": "CaretSticky",
	"CaretLineBackAlpha": "Alpha",
	"CaretStyle": "CaretStyle",
	"MarginOptions": "MarginOption",
	"AnnotationGetVisible": "AnnotationVisible",
	"VirtualSpaceOptions": "VirtualSpace",
	"Technology": "Technology",
	"LineEndTypesAllowed": "LineEndType",
	"LineEndTypesActive": "LineEndType",
	"PropertyType": "TypeProperty",
	"Bidirectional": "Bidirectional",
	"LineCharacterIndex": "LineCharacterIndexType",
}

enumArgNames = {
	"weight": "FontWeight",
	"viewWS": "WhiteSpace",
	"tabDrawMode": "TabDrawMode",
	"eolMode": "EndOfLine",
	"imeInteraction": "IMEInteraction",
	"caseVisible": "CaseVisible",
	"markerSymbol": "MarkerSymbol",
	"indentView": "IndentView",
	"mode": "PrintOption",
	"searchFlags": "FindOption",
	"indicatorStyle": "IndicatorStyle",
	"action": "FoldAction",
	"automaticFold": "AutomaticFold",
	"level": "FoldLevel",
	"idleStyling": "IdleStyling",
	"wrapMode": "Wrap",
	"wrapVisualFlags": "WrapVisualFlag",
	"wrapVisualFlagsLocation": "WrapVisualLocation",
	"wrapIndentMode": "WrapIndentMode",
	"cacheMode": "LineCache",
	"phases": "PhasesDraw",
	"fontQuality": "FontQuality",
	"multiPaste": "MultiPaste",
	"accessibility": "Accessibility",
	"eventMask": "ModificationFlags",
	"edgeMode": "EdgeVisualStyle",
	"popUpMode": "PopUp",
	"status": "Status",
	"cursorType": "CursorShape",
	"visiblePolicy": "VisiblePolicy",
	"caretPolicy": "CaretPolicy",
	"selectionMode": "SelectionMode",
	"behaviour": "CaseInsensitiveBehaviour",
	"multi": "MultiAutoComplete",
	"order": "Ordering",
	"useCaretStickyBehaviour": "CaretSticky",
	"alpha": "Alpha",
	"caretStyle": "CaretStyle",
	"marginOptions": "MarginOption",
	"virtualSpaceOptions": "VirtualSpace",
	"technology": "Technology",
	"lineEndBitSet": "LineEndType",
	"documentOptions": "DocumentOption",
	"bidirectional": "Bidirectional",
	"lineCharacterIndex": "LineCharacterIndexType",
}

enumerationAliases = {
	# Provide hard coded segmented versions of the enumerations
	"SCWS_VISIBLEALWAYS": "VISIBLE_ALWAYS",
	"SCWS_VISIBLEAFTERINDENT": "VISIBLE_AFTER_INDENT",
	"SCWS_VISIBLEONLYININDENT": "VISIBLE_ONLY_IN_INDENT",
	"SCTD_LONGARROW": "LONG_ARROW",
	"SCTD_STRIKEOUT": "STRIKE_OUT",
	"SC_EOL_CRLF": "CR_LF",
	"SC_MARK_ROUNDRECT": "ROUND_RECT",
	"SC_MARK_SMALLRECT": "SMALL_RECT",
	"SC_MARK_SHORTARROW": "SHORT_ARROW",
	"SC_MARK_ARROWDOWN": "ARROW_DOWN",
	"SC_MARK_VLINE": "V_LINE",
	"SC_MARK_LCORNER": "L_CORNER",
	"SC_MARK_TCORNER": "T_CORNER",
	"SC_MARK_BOXPLUS": "BOX_PLUS",
	"SC_MARK_BOXPLUSCONNECTED": "BOX_PLUS_CONNECTED",
	"SC_MARK_BOXMINUS": "BOX_MINUS",
	"SC_MARK_BOXMINUSCONNECTED": "BOX_MINUS_CONNECTED",
	"SC_MARK_LCORNERCURVE": "L_CORNER_CURVE",
	"SC_MARK_TCORNERCURVE": "T_CORNER_CURVE",
	"SC_MARK_CIRCLEPLUS": "CIRCLE_PLUS",
	"SC_MARK_CIRCLEPLUSCONNECTED": "CIRCLE_PLUS_CONNECTED",
	"SC_MARK_CIRCLEMINUS": "CIRCLE_MINUS",
	"SC_MARK_CIRCLEMINUSCONNECTED": "CIRCLE_MINUS_CONNECTED",
	"SC_MARK_DOTDOTDOT": "DOT_DOT_DOT",
	"SC_MARK_FULLRECT": "FULL_RECT",
	"SC_MARK_LEFTRECT": "LEFT_RECT",
	"SC_MARK_RGBAIMAGE": "R_G_B_A_IMAGE",
	"SC_MARK_VERTICALBOOKMARK": "VERTICAL_BOOKMARK",
	"SC_MARKNUM_FOLDEREND": "FOLDER_END",
	"SC_MARKNUM_FOLDEROPENMID": "FOLDER_OPEN_MID",
	"SC_MARKNUM_FOLDERMIDTAIL": "FOLDER_MID_TAIL",
	"SC_MARKNUM_FOLDERTAIL": "FOLDER_TAIL",
	"SC_MARKNUM_FOLDERSUB": "FOLDER_SUB",
	"SC_MARKNUM_FOLDEROPEN": "FOLDER_OPEN",
	"SC_MARGIN_RTEXT": "R_TEXT",
	"STYLE_LINENUMBER": "LINE_NUMBER",
	"STYLE_BRACELIGHT": "BRACE_LIGHT",
	"STYLE_BRACEBAD": "BRACE_BAD",
	"STYLE_CONTROLCHAR": "CONTROL_CHAR",
	"STYLE_INDENTGUIDE": "INDENT_GUIDE",
	"STYLE_CALLTIP": "CALL_TIP",
	"STYLE_FOLDDISPLAYTEXT": "FOLD_DISPLAY_TEXT",
	"STYLE_LASTPREDEFINED": "LAST_PREDEFINED",
	"SC_CHARSET_CHINESEBIG5": "CHINESE_BIG5",
	"SC_CHARSET_EASTEUROPE": "EAST_EUROPE",
	"SC_CHARSET_GB2312": "G_B_2312",
	"SC_CHARSET_OEM": "O_E_M",
	"SC_CHARSET_OEM866": "O_E_M_866",
	"SC_CHARSET_SHIFTJIS": "SHIFT_J_I_S",
	"SC_CHARSET_8859_15": "I_S_O_8859_15",
	"SC_WEIGHT_SEMIBOLD": "SEMI_BOLD",
	"INDIC_TT": "T_T",
	"INDIC_ROUNDBOX": "ROUND_BOX",
	"INDIC_STRAIGHTBOX": "STRAIGHT_BOX",
	"INDIC_SQUIGGLELOW": "SQUIGGLE_LOW",
	"INDIC_DOTBOX": "DOT_BOX",
	"INDIC_SQUIGGLEPIXMAP": "SQUIGGLE_PIXMAP",
	"INDIC_COMPOSITIONTHICK": "COMPOSITION_THICK",
	"INDIC_COMPOSITIONTHIN": "COMPOSITION_THIN",
	"INDIC_FULLBOX": "FULL_BOX",
	"INDIC_TEXTFORE": "TEXT_FORE",
	"INDIC_POINTCHARACTER": "POINT_CHARACTER",
	"INDIC_GRADIENTCENTRE": "GRADIENT_CENTRE",
	"SC_INDICFLAG_VALUEFORE": "VALUE_FORE",
	"SC_IV_LOOKFORWARD": "LOOK_FORWARD",
	"SC_IV_LOOKBOTH": "LOOK_BOTH",
	"SC_PRINT_INVERTLIGHT": "INVERT_LIGHT",
	"SC_PRINT_BLACKONWHITE": "BLACK_ON_WHITE",
	"SC_PRINT_COLOURONWHITE": "COLOUR_ON_WHITE",
	"SC_PRINT_COLOURONWHITEDEFAULTBG": "COLOUR_ON_WHITE_DEFAULT_B_G",
	"SC_PRINT_SCREENCOLOURS": "SCREEN_COLOURS",
	"SCFIND_WHOLEWORD": "WHOLE_WORD",
	"SCFIND_MATCHCASE": "MATCH_CASE",
	"SCFIND_WORDSTART": "WORD_START",
	"SCFIND_REGEXP": "REG_EXP",
	"SCFIND_POSIX": "P_O_S_I_X",
	"SCFIND_CXX11REGEX": "C_X_X_11_REG_EX",
	"SC_FOLDLEVELWHITEFLAG": "WHITE_FLAG",
	"SC_FOLDLEVELHEADERFLAG": "HEADER_FLAG",
	"SC_FOLDLEVELNUMBERMASK": "NUMBER_MASK",
	"SC_FOLDFLAG_LINEBEFORE_EXPANDED": "LINE_BEFORE_EXPANDED",
	"SC_FOLDFLAG_LINEBEFORE_CONTRACTED": "LINE_BEFORE_CONTRACTED",
	"SC_FOLDFLAG_LINEAFTER_EXPANDED": "LINE_AFTER_EXPANDED",
	"SC_FOLDFLAG_LINEAFTER_CONTRACTED": "LINE_AFTER_CONTRACTED",
	"SC_FOLDFLAG_LEVELNUMBERS": "LEVEL_NUMBERS",
	"SC_FOLDFLAG_LINESTATE": "LINE_STATE",
	"SC_IDLESTYLING_TOVISIBLE": "TO_VISIBLE",
	"SC_IDLESTYLING_AFTERVISIBLE": "AFTER_VISIBLE",
	"SC_WRAP_WHITESPACE": "WHITE_SPACE",
	"SC_WRAPINDENT_DEEPINDENT": "DEEP_INDENT",
	"EDGE_MULTILINE": "MULTI_LINE",
	"SC_STATUS_BADALLOC": "BAD_ALLOC",
	"SC_STATUS_WARN_REGEX": "REG_EX",
	"SC_CURSORREVERSEARROW": "REVERSE_ARROW",
	"SC_CASEINSENSITIVEBEHAVIOUR_RESPECTCASE": "RESPECT_CASE",
	"SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE": "IGNORE_CASE",
	"SC_ORDER_PRESORTED": "PRE_SORTED",
	"SC_ORDER_PERFORMSORT": "PERFORM_SORT",
	"SC_CARETSTICKY_WHITESPACE": "WHITE_SPACE",
	"SC_ALPHA_NOALPHA": "NO_ALPHA",
	"SC_MARGINOPTION_SUBLINESELECT": "SUB_LINE_SELECT",
	"SCVS_RECTANGULARSELECTION": "RECTANGULAR_SELECTION",
	"SCVS_USERACCESSIBLE": "USER_ACCESSIBLE",
	"SCVS_NOWRAPLINESTART": "NO_WRAP_LINE_START",
	"SC_TECHNOLOGY_DIRECTWRITE": "DIRECT_WRITE",
	"SC_TECHNOLOGY_DIRECTWRITERETAIN": "DIRECT_WRITE_RETAIN",
	"SC_TECHNOLOGY_DIRECTWRITEDC": "DIRECT_WRITE_D_C",
	"SC_MOD_INSERTTEXT": "INSERT_TEXT",
	"SC_MOD_DELETETEXT": "DELETE_TEXT",
	"SC_MOD_CHANGESTYLE": "CHANGE_STYLE",
	"SC_MOD_CHANGEFOLD": "CHANGE_FOLD",
	"SC_MULTISTEPUNDOREDO": "MULTI_STEP_UNDO_REDO",
	"SC_LASTSTEPINUNDOREDO": "LAST_STEP_IN_UNDO_REDO",
	"SC_MOD_CHANGEMARKER": "CHANGE_MARKER",
	"SC_MOD_BEFOREINSERT": "BEFORE_INSERT",
	"SC_MOD_BEFOREDELETE": "BEFORE_DELETE",
	"SC_MULTILINEUNDOREDO": "MULTILINE_UNDO_REDO",
	"SC_STARTACTION": "START_ACTION",
	"SC_MOD_CHANGEINDICATOR": "CHANGE_INDICATOR",
	"SC_MOD_CHANGELINESTATE": "CHANGE_LINE_STATE",
	"SC_MOD_CHANGEMARGIN": "CHANGE_MARGIN",
	"SC_MOD_CHANGEANNOTATION": "CHANGE_ANNOTATION",
	"SC_MOD_LEXERSTATE": "LEXER_STATE",
	"SC_MOD_INSERTCHECK": "INSERT_CHECK",
	"SC_MOD_CHANGETABSTOPS": "CHANGE_TAB_STOPS",
	"SC_MODEVENTMASKALL": "EVENT_MASK_ALL",
	"SCK_RWIN": "R_WIN",
	"SC_AC_FILLUP": "FILL_UP",
	"SC_AC_DOUBLECLICK": "DOUBLE_CLICK",
}

extraDefinitions = """
enu UndoFlags=UNDO_
val UNDO_NONE=0
"""

def AugmentFace(f):
	# These would be ambiguous if changed globally so edit the API definition directly
	f.features["StyleSetCharacterSet"]["Param2Type"] = "CharacterSet"
	f.features["IndicSetFlags"]["Param2Type"] = "IndicFlag"
	f.features["FoldDisplayTextSetStyle"]["Param1Type"] = "FoldDisplayTextStyle"
	f.features["SetFoldFlags"]["Param1Type"] = "FoldFlag"
	f.features["AnnotationSetVisible"]["Param1Type"] = "AnnotationVisible"
	f.features["AddUndoAction"]["Param2Type"] = "UndoFlags"
	
	# These are more complex

	name = "UndoFlags"
	f.features[name] = {
		"FeatureType": "enu",
		"Category": "",
		"Value": "UNDO_",
		"Comment": "" }
	f.order.append(name)

def IsEnumeration(s):
	if s in ["Position", "Line", "Colour"]:
		return False
	return s[:1].isupper()
	
def JoinTypeAndIdentifier(type, identifier):
	# Add a space to separate type from identifier unless type is pointer
	if type.endswith("*"):
		return type + identifier
	else:
		return type + " " + identifier

def ParametersArgsCallname(v):
	parameters = ""
	args = ""
	callName = "Call"

	param1TypeBase = v["Param1Type"]
	param1Name = v["Param1Name"]
	param1Type = ActualTypeName(param1TypeBase, param1Name)
	param1Arg = ""
	if param1Type:
		castName = param1Name
		if param1Type.endswith("*"):
			castName = "reinterpret_cast<uintptr_t>(" + param1Name + ")"
		elif param1Type not in basicTypes:
			castName = "static_cast<uintptr_t>(" + param1Name + ")"
		if IsEnumeration(param1TypeBase):
			param1Type = namespace + param1Type
		param1Arg = JoinTypeAndIdentifier(param1Type, param1Name)
		parameters = param1Arg
		args = castName

	param2TypeBase = v["Param2Type"]
	param2Name = v["Param2Name"]
	param2Type = ActualTypeName(param2TypeBase, param2Name)
	param2Arg = ""
	if param2Type:
		castName = param2Name
		if param2Type.endswith("*"):
			if param2Type == "const char *":
				callName = "CallString"
			else:
				callName = "CallPointer"
		elif param2Type not in basicTypes:
			castName = "static_cast<intptr_t>(" + param2Name + ")"
		if IsEnumeration(param2TypeBase):
			param2Type = namespace + param2Type
		param2Arg = JoinTypeAndIdentifier(param2Type, param2Name)
		if param1Arg:
			parameters = parameters + ", "
		parameters = parameters + param2Arg
		if not args:
			args = args + "0"
		if args:
			args = args + ", "
		args = args + castName

	if args:
		args = ", " + args
	return (parameters, args, callName)
	
def HMessages(f):
	out = ["enum class Message {"]
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["fun", "get", "set"]:
				out.append("\t" + name + " = " + v["Value"] + ",")
	out.append("};")
	return out

def HEnumerations(f):
	out = []
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			# Only want non-deprecated enumerations and lexers are not part of Scintilla API
			if v["FeatureType"] in ["enu"] and name != "Lexer":
				out.append("")
				prefixes = v["Value"].split()
				#out.append("enum class " + name + " {" + " // " + ",".join(prefixes))
				out.append("enum class " + name + " {")
				for valueName in f.order:
					prefixMatched = ""
					for p in prefixes:
						if valueName.startswith(p):
							prefixMatched = p
					if prefixMatched:
						vEnum = f.features[valueName]
						valueNameNoPrefix = ""
						if valueName in enumerationAliases:
							valueNameNoPrefix = enumerationAliases[valueName]
						else:
							valueNameNoPrefix = valueName[len(prefixMatched):]
							if not valueNameNoPrefix:	# Removed whole name
								valueNameNoPrefix = valueName
							if valueNameNoPrefix.startswith("SC_"):
								valueNameNoPrefix = valueNameNoPrefix[len("SC_"):]
						pascalName = PascalCase(valueNameNoPrefix)
						out.append("\t" + pascalName + " = " + vEnum["Value"] + ",")
				out.append("};")

	out.append("")
	out.append("enum class Notification {")
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["evt"]:
				out.append("\t" + name + " = " + v["Value"] + ",")
	out.append("};")

	return out

def HConstants(f):
	# Constants not in an eumeration
	out = []
	allEnumPrefixes = [
		"SCE_", # Lexical styles
		"SCI_", # Message number allocation
		"SCEN_", # Notifications sent with WM_COMMAND
	]
	for n, v in f.features.items():
		if v["Category"] != "Deprecated":
			# Only want non-deprecated enumerations and lexers are not part of Scintilla API
			if v["FeatureType"] in ["enu"]:
				allEnumPrefixes.extend(v["Value"].split())
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			# Only want non-deprecated enumerations and lexers are not part of Scintilla API
			if v["FeatureType"] in ["val"]:
				hasPrefix = False
				for prefix in allEnumPrefixes:
					if name.startswith(prefix):
						hasPrefix = True
				if not hasPrefix:
					if name.startswith("SC_"):
						name = name[3:]
					out.append("constexpr int " + PascalCase(name) + " = " + v["Value"] + ";")
	return out

def HMethods(f):
	out = []
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["fun", "get", "set"]:
				if v["FeatureType"] == "get" and name.startswith("Get"):
					name = name[len("Get"):]
				retType = ActualTypeName(v["ReturnType"])
				if name in returningMethods:
					retType = returningMethods[name]
				if IsEnumeration(retType):
					retType = namespace + retType
				parameters, args, callName = ParametersArgsCallname(v)

				out.append("\t" + JoinTypeAndIdentifier(retType, name) + "(" + parameters + ");")
	return out

def CXXMethods(f):
	out = []
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["fun", "get", "set"]:
				nameTransformed = name
				if v["FeatureType"] == "get" and name.startswith("Get"):
					nameTransformed = nameTransformed[len("Get"):]
				retTypeBase = ActualTypeName(v["ReturnType"])
				retType = ActualTypeName(retTypeBase)
				if nameTransformed in returningMethods:
					retType = returningMethods[nameTransformed]
				parameters, args, callName = ParametersArgsCallname(v)
				returnIfNeeded = "return " if retType != "void" else ""
				msgName = "Message::" + name

				out.append(JoinTypeAndIdentifier(retType, "ScintillaCall::" + nameTransformed) + "(" + parameters + ")" + " {")
				retCast = ""
				retCastEnd = ""
				if retType not in basicTypes or retType in ["int", "Colour"]:
					if IsEnumeration(retTypeBase) or IsEnumeration(retType):
						retType = namespace + retType
					retCast = "static_cast<" + retType + ">("
					retCastEnd = ")"
				elif retType in ["void *"]:
					retCast = "reinterpret_cast<" + retType + ">("
					retCastEnd = ")"
				out.append("\t" + returnIfNeeded + retCast + callName + "(" + msgName + args + ")" + retCastEnd + ";")
				out.append("}")
				out.append("")

	return out

def RegenerateAll(root):
	f = Face.Face()
	f.ReadFromFile(root + "../scintilla/" + "include/Scintilla.iface")
	AugmentFace(f)
	Regenerate(root + "src/ScintillaMessages.h", "//", HMessages(f))
	Regenerate(root + "src/ScintillaTypes.h", "//", HEnumerations(f), HConstants(f))
	Regenerate(root + "src/ScintillaCall.h", "//", HMethods(f))
	Regenerate(root + "src/ScintillaCall.cxx", "//", CXXMethods(f))

if __name__ == "__main__":
	RegenerateAll("../")
