#!/usr/bin/env python3
# APIFacer.py - regenerate the ScintillaTypes.h, ScintillaMessages.h, ScintillaCall.h,and ScintillaCall.cxx files
# from the Scintilla.iface interface definition file.
# Implemented 2019 by Neil Hodgson neilh@scintilla.org
# Requires Python 3.6 or later

import sys

srcRoot = "../.."

sys.path.append(srcRoot + "/scintilla/scripts")

import Face
import FileGenerator

typeAliases = {
	# Convert iface types to C++ types
	# bool and void are OK as is
	"cells": "const char *",
	"colour": "Colour",
	"colouralpha": "ColourAlpha",
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
	"ColourAlpha",
	"const char *",
	"int",
	"intptr_t",
	"Line",
	"Position",
	"void",
	"void *",
]

namespace = "Scintilla::"

def ActualTypeName(type, identifier=None):
	if type in typeAliases:
		return typeAliases[type]
	else:
		return type

def IsEnumeration(s):
	if s in ["Position", "Line", "Colour", "ColourAlpha"]:
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

def ParametersExceptLast(parameters):
	if "," in parameters:
		return parameters[:parameters.rfind(",")]
	else:
		return ""

def HMethods(f):
	out = []
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["fun", "get", "set"]:
				if v["FeatureType"] == "get" and name.startswith("Get"):
					name = name[len("Get"):]
				retType = ActualTypeName(v["ReturnType"])
				if IsEnumeration(retType):
					retType = namespace + retType
				parameters, args, callName = ParametersArgsCallname(v)

				out.append("\t" + JoinTypeAndIdentifier(retType, name) + "(" + parameters + ");")

				# Extra method for stringresult that returns std::string
				if v["Param2Type"] == "stringresult":
					out.append("\t" + JoinTypeAndIdentifier("std::string", name) + \
						"(" + ParametersExceptLast(parameters) + ");")
	return out

def CXXMethods(f):
	out = []
	for name in f.order:
		v = f.features[name]
		if v["Category"] != "Deprecated":
			if v["FeatureType"] in ["fun", "get", "set"]:
				msgName = "Message::" + name
				if v["FeatureType"] == "get" and name.startswith("Get"):
					name = name[len("Get"):]
				retType = ActualTypeName(v["ReturnType"])
				parameters, args, callName = ParametersArgsCallname(v)
				returnIfNeeded = "return " if retType != "void" else ""

				out.append(JoinTypeAndIdentifier(retType, "ScintillaCall::" + name) + "(" + parameters + ")" + " {")
				retCast = ""
				retCastEnd = ""
				if retType not in basicTypes or retType in ["int", "Colour", "ColourAlpha"]:
					if IsEnumeration(retType):
						retType = namespace + retType
					retCast = "static_cast<" + retType + ">("
					retCastEnd = ")"
				elif retType in ["void *"]:
					retCast = "reinterpret_cast<" + retType + ">("
					retCastEnd = ")"
				out.append("\t" + returnIfNeeded + retCast + callName + "(" + msgName + args + ")" + retCastEnd + ";")
				out.append("}")
				out.append("")

				# Extra method for stringresult that returns std::string
				if v["Param2Type"] == "stringresult":
					paramList = ParametersExceptLast(parameters)
					argList = ParametersExceptLast(args)
					out.append(JoinTypeAndIdentifier("std::string", "ScintillaCall::" + name) + \
						"(" + paramList + ") {")
					out.append("\treturn CallReturnString(" + msgName + argList + ");")
					out.append("}")
					out.append("")

	return out

def RegenerateAll(root):
	f = Face.Face()
	f.ReadFromFile(root + "../scintilla/" + "include/Scintilla.iface")
	FileGenerator.Regenerate(root + "src/ScintillaCall.h", "//", HMethods(f))
	FileGenerator.Regenerate(root + "src/ScintillaCall.cxx", "//", CXXMethods(f))

if __name__ == "__main__":
	RegenerateAll("../")
