/** @file testUtf8_16.cxx
 ** Unit Tests for SciTE internal data structures
 **/

#define _CRT_SECURE_NO_WARNINGS

#include <cstddef>
#include <cstring>
#include <cstdio>

#include <stdexcept>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <memory>

#include "Cookie.h"
#include "Utf8_16.h"

#include "catch.hpp"

using namespace std::literals;

namespace {

constexpr bool IsUTF8TrailByte(int ch) noexcept {
	return (ch >= 0x80) && (ch < (0x80 + 0x40));
}

struct MemDoc {
	// Read a byte-sequence (equivalent to a file) into a string+encoding
	UniMode unicodeMode = UniMode::uni8Bit;
	std::string result;
	MemDoc(std::string_view text, size_t blockSize) {
		// Read in from byte string
		std::unique_ptr<Utf8_16::Reader> convert = Utf8_16::Reader::Allocate();
		size_t lenRead = std::min(text.size(), blockSize);
		while (lenRead > 0) {
			const std::string_view converted = convert->convert(std::string_view(text.data(), lenRead));
			result.append(converted);
			text.remove_prefix(lenRead);
			lenRead = std::min(text.size(), blockSize);
		}
		const std::string_view convertedTrail = convert->convert("");
		result.append(convertedTrail);
		unicodeMode = convert->getEncoding();
	}
};

std::string Encode(const std::string &s, UniMode unicodeMode, size_t blockSize) {
	assert(blockSize >= 4);	// Maximum UTF-8 character is 4 bytes
	FILE *fp = fopen("x.txt", "wb");
	if (!fp) {
		return "";
	}
	const std::string_view documentView(s);
	std::unique_ptr<Utf8_16::Writer> convert = Utf8_16::Writer::Allocate(unicodeMode, blockSize);
	const size_t lengthDoc = s.size();
	for (size_t startBlock = 0; startBlock < lengthDoc;) {
		size_t grabSize = std::min(lengthDoc - startBlock, blockSize);
		if ((unicodeMode != UniMode::uni8Bit) && (startBlock + grabSize < lengthDoc)) {
			// Round down so only whole characters retrieved.
			size_t startLast = grabSize;
			while ((startLast > 0) && ((grabSize - startLast) < 6) &&
				IsUTF8TrailByte(static_cast<unsigned char>(documentView.at(startBlock + startLast))))
				startLast--;
			if ((grabSize - startLast) < 5)
				grabSize = startLast;
		}
		convert->fwrite(documentView.substr(startBlock, grabSize), fp);
		startBlock += grabSize;
	}
	fclose(fp);
	std::string out;
	{
		FILE *fpr = fopen("x.txt", "rb");
		std::vector<char> vdat(10240);
		const size_t lenFile = fread(vdat.data(), 1, vdat.size(), fpr);
		out.insert(0, vdat.data(), lenFile);
		fclose(fpr);
		remove("x.txt");
	}
	return out;
}

std::string OutBytes(const MemDoc &md, size_t blockSize) {
	return Encode(md.result, md.unicodeMode, blockSize);
}

std::string ByteReverse(std::string_view sv) {
	// Require: even number of bytes
	assert(sv.length() % 2 == 0);

	std::string ret(sv);
	for (size_t i = 0; i < ret.length(); i += 2) {
		std::swap(ret.at(i), ret.at(i+1));
	}
	return ret;
}

}

// Using preprocessor for implicit concatenation

#define BOM_UTF8 "\xEF\xBB\xBF"sv
#define BOM_UTF16LE "\xFF\xFE"sv
#define BOM_UTF16BE "\xFE\xFF"sv

// 2,3,4 byte UTF-8 characters, U+0393, U+30A6, U+10348
#define GAMMA "\xCE\x93"sv
#define KATAKANA_U "\xE3\x82\xA6"sv
#define HWAIR "\xF0\x90\x8D\x88"sv

// UTF-16LE versions
#define GAMMA_LE "\x93\x03"sv
#define KATAKANA_U_LE "\xA6\x30"sv
#define HWAIR_LE "\x00\xD8\x48\xDF"sv
#define LEAD_SURROGATE_LE "\x00\xD8"sv

// space a gamma gamma z space
#define GAMMA_TEXT " a"sv GAMMA GAMMA "z "sv

// a katakana_u
#define KATAKANA_U_TEXT "a"sv KATAKANA_U

// a hwair
#define HWAIR_TEXT "a"sv HWAIR
#define AB_HWAIR_TEXT "ab"sv HWAIR

constexpr std::string_view sABHwairU16LE = BOM_UTF16LE "a\000b\0"sv HWAIR_LE;

TEST_CASE("Conversion") {

	std::unique_ptr<Utf8_16::Reader> convert = Utf8_16::Reader::Allocate();

	SECTION("ASCII") {
		constexpr std::string_view sText = "abc";
		MemDoc md(sText, 1024);
		REQUIRE(md.unicodeMode == UniMode::uni8Bit);
		REQUIRE(md.result == sText);
		
		const std::string out = OutBytes(md, 32);
		REQUIRE(out == sText);
		
		const std::string outLE = Encode(md.result, UniMode::uni16LE, 32);
		constexpr std::string_view sTextLE = BOM_UTF16LE "a\0b\0c\0"sv;
		REQUIRE(outLE.length() == 8);
		REQUIRE(sTextLE.length() == 8);
		REQUIRE(outLE == sTextLE);

		const std::string outBE = Encode(md.result, UniMode::uni16BE, 32);
		const std::string sTextBE = ByteReverse(sTextLE);
		REQUIRE(outBE.length() == 8);
		REQUIRE(sTextBE.length() == 8);
		REQUIRE(outBE == sTextBE);
	}

	SECTION("ShortUTF8") {
		constexpr std::string_view sFile = BOM_UTF8 GAMMA_TEXT;
		constexpr std::string_view sText = GAMMA_TEXT;

		MemDoc md(sFile, 1024);
		REQUIRE(md.unicodeMode == UniMode::utf8);
		REQUIRE(md.result == sText);
		
		const std::string out = OutBytes(md, 32);
		REQUIRE(out == sFile);
		
		constexpr std::string_view sTextLE = BOM_UTF16LE " \0a\0"sv GAMMA_LE GAMMA_LE "z\0 \0"sv;
		const std::string sTextBE = ByteReverse(sTextLE);

		const std::string outLE = Encode(md.result, UniMode::uni16LE, 32);
		REQUIRE(outLE == sTextLE);

		const std::string outBE = Encode(md.result, UniMode::uni16BE, 32);
		REQUIRE(outBE == sTextBE);

		// With a short decode buffer
		const std::string outLEShort = Encode(md.result, UniMode::uni16LE, 4);
		REQUIRE(outLEShort == sTextLE);
	}

	SECTION("3ByteUTF8") {
		constexpr std::string_view sFile = BOM_UTF8 KATAKANA_U_TEXT;
		constexpr std::string_view sText = KATAKANA_U_TEXT;

		MemDoc md(sFile, 1024);
		REQUIRE(md.unicodeMode == UniMode::utf8);
		REQUIRE(md.result == sText);
		
		const std::string out = OutBytes(md, 32);
		REQUIRE(out == sFile);
		
		// Katakana U = U+30A6
		constexpr std::string_view sTextLE = BOM_UTF16LE "a\0"sv KATAKANA_U_LE;
		const std::string sTextBE = ByteReverse(sTextLE);

		const std::string outLE = Encode(md.result, UniMode::uni16LE, 32);
		REQUIRE(outLE == sTextLE);

		const std::string outBE = Encode(md.result, UniMode::uni16BE, 32);
		REQUIRE(outBE == sTextBE);

		// With a short decode buffer
		const std::string outLEShort = Encode(md.result, UniMode::uni16LE, 4);
		REQUIRE(outLEShort == sTextLE);
	}

	SECTION("Non-BMP") {
		// GOTHIC LETTER HWAIR = U+10348 
		constexpr std::string_view sFile = BOM_UTF8 HWAIR_TEXT;
		constexpr std::string_view sHwair = HWAIR_TEXT;

		MemDoc md(sFile, 1024);
		REQUIRE(md.unicodeMode == UniMode::utf8);
		REQUIRE(md.result == sHwair);
		
		const std::string out = OutBytes(md, 32);
		REQUIRE(out == sFile);
		
		// "\uD800\uDF48"
		const std::string_view sTextLE = BOM_UTF16LE "a\0"sv HWAIR_LE;
		const std::string sTextBE = ByteReverse(sTextLE);

		const std::string outLE = Encode(md.result, UniMode::uni16LE, 32);
		REQUIRE(outLE == sTextLE);

		const std::string outBE = Encode(md.result, UniMode::uni16BE, 32);
		REQUIRE(outBE == sTextBE);

		// With a short decode buffer
		const std::string outLEShort = Encode(md.result, UniMode::uni16LE, 4);
		REQUIRE(outLEShort == sTextLE);
	}

	SECTION("U16Input") {
		// Check cases of UTF-16 input 

		constexpr std::string_view sFile = sABHwairU16LE; // a b hwair
		constexpr std::string_view sHwair = AB_HWAIR_TEXT;

		{
			// Buffer big enough to read in one go
			MemDoc md(sFile, 1024);
			REQUIRE(md.unicodeMode == UniMode::uni16LE);
			REQUIRE(md.result == sHwair);

			const std::string out = OutBytes(md, 32);
			REQUIRE(out == sFile);

			const std::string sFileBE = ByteReverse(sABHwairU16LE); // a hwair
			MemDoc mdBE(sFileBE, 1024);
			REQUIRE(mdBE.unicodeMode == UniMode::uni16BE);
			REQUIRE(mdBE.result == sHwair);
		}

		{
			// Small buffer to check character straddling buffer boundary
			// 10-byte input BOM A B HWAIR
			// With 4-byte reads, reads as [BOM, A], [B, LEAD(HWAIR)], [TRAIL(HWAIR)]
			MemDoc md(sFile, 4);
			REQUIRE(md.unicodeMode == UniMode::uni16LE);
			REQUIRE(md.result == sHwair);

			const std::string out = OutBytes(md, 32);
			REQUIRE(out == sFile);

			const std::string sFileBE = ByteReverse(sABHwairU16LE); // a hwair
			MemDoc mdBE(sFileBE, 4);
			REQUIRE(mdBE.unicodeMode == UniMode::uni16BE);
			REQUIRE(mdBE.result == sHwair);
		}

		{
			// Small buffer to check character straddling buffer boundary in first block
			// 6-byte input BOM HWAIR
			// With 4-byte reads, reads as [BOM, LEAD(HWAIR)], [TRAIL(HWAIR)]
			constexpr std::string_view sFileShort = BOM_UTF16LE HWAIR_LE;
			constexpr std::string_view sHwairShort = HWAIR;
			MemDoc md(sFileShort, 4);
			REQUIRE(md.unicodeMode == UniMode::uni16LE);
			REQUIRE(md.result == sHwairShort);

			const std::string out = OutBytes(md, 32);
			REQUIRE(out == sFileShort);

			const std::string sFileBE = ByteReverse(sFileShort); // hwair
			MemDoc mdBE(sFileBE, 4);
			REQUIRE(mdBE.unicodeMode == UniMode::uni16BE);
			REQUIRE(mdBE.result == sHwairShort);
		}

	}

}
