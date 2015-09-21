// SciTE - Scintilla based Text Editor
/** @file ExportPDF.cxx
 ** Export the current document to PDF.
 **/
// Copyright 1998-2006 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"

#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "StyleDefinition.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"

//---------- Save to PDF ----------

/*
	PDF Exporter. Status: Beta
	Contributed by Ahmad M. Zawawi <zeus_go64@hotmail.com>
	Modifications by Darren Schroeder Feb 22, 2003; Philippe Lhoste 2003-10
	Overhauled by Kein-Hong Man 2003-11

	This exporter is meant to be small and simple; users are expected to
	use other methods for heavy-duty formatting. PDF elements marked with
	"PDF1.4Ref" states where in the PDF 1.4 Reference Spec (the PDF file of
	which is freely available from Adobe) the particular element can be found.

	Possible TODOs that will probably not be implemented: full styling,
	optimization, font substitution, compression, character set encoding.
*/
#define PDF_TAB_DEFAULT		8
#define PDF_FONT_DEFAULT	1	// Helvetica
#define PDF_FONTSIZE_DEFAULT	10
#define PDF_SPACING_DEFAULT	1.2
#define PDF_HEIGHT_DEFAULT	792	// Letter
#define PDF_WIDTH_DEFAULT	612
#define PDF_MARGIN_DEFAULT	72	// 1.0"
#define PDF_ENCODING		"WinAnsiEncoding"

struct PDFStyle {
	char fore[24];
	int font;
};

static const char *PDFfontNames[] = {
            "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
            "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
            "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic"
        };

// ascender, descender aligns font origin point with page
static short PDFfontAscenders[] =  { 629, 718, 699 };
static short PDFfontDescenders[] = { 157, 207, 217 };
static short PDFfontWidths[] =     { 600,   0,   0 };

inline void getPDFRGB(char* pdfcolour, const char* stylecolour) {
	// grab colour components (max string length produced = 18)
	for (int i = 1; i < 6; i += 2) {
		char val[20];
		// 3 decimal places for enough dynamic range
		int c = (IntFromHexByte(stylecolour + i) * 1000 + 127) / 255;
		if (c == 0 || c == 1000) {	// optimise
			sprintf(val, "%d ", c / 1000);
		} else {
			sprintf(val, "0.%03d ", c);
		}
		strcat(pdfcolour, val);
	}
}

void SciTEBase::SaveToPDF(FilePath saveName) {
	// This class conveniently handles the tracking of PDF objects
	// so that the cross-reference table can be built (PDF1.4Ref(p39))
	// All writes to fp passes through a PDFObjectTracker object.
	class PDFObjectTracker {
	private:
		FILE *fp;
		long *offsetList, tableSize;
		// Deleted so PDFObjectTracker objects can not be copied
		PDFObjectTracker(const PDFObjectTracker &) = delete;
	public:
		int index;
		explicit PDFObjectTracker(FILE *fp_) {
			fp = fp_;
			tableSize = 100;
			offsetList = new long[tableSize];
			index = 1;
		}
		~PDFObjectTracker() {
			delete []offsetList;
		}
		void write(const char *objectData) {
			size_t length = strlen(objectData);
			// note binary write used, open with "wb"
			fwrite(objectData, sizeof(char), length, fp);
		}
		void write(int objectData) {
			char val[20];
			sprintf(val, "%d", objectData);
			write(val);
		}
		// returns object number assigned to the supplied data
		int add(const char *objectData) {
			// resize xref offset table if too small
			if (index > tableSize) {
				long newSize = tableSize * 2;
				long *newList = new long[newSize];
				for (int i = 0; i < tableSize; i++) {
					newList[i] = offsetList[i];
				}
				delete []offsetList;
				offsetList = newList;
				tableSize = newSize;
			}
			// save offset, then format and write object
			offsetList[index - 1] = ftell(fp);
			write(index);
			write(" 0 obj\n");
			write(objectData);
			write("endobj\n");
			return index++;
		}
		// builds xref table, returns file offset of xref table
		long xref() {
			char val[32];
			// xref start index and number of entries
			long xrefStart = ftell(fp);
			write("xref\n0 ");
			write(index);
			// a xref entry *must* be 20 bytes long (PDF1.4Ref(p64))
			// so extra space added; also the first entry is special
			write("\n0000000000 65535 f \n");
			for (int i = 0; i < index - 1; i++) {
				sprintf(val, "%010ld 00000 n \n", offsetList[i]);
				write(val);
			}
			return xrefStart;
		}
	};

	// Object to manage line and page rendering. Apart from startPDF, endPDF
	// everything goes in via add() and nextLine() so that line formatting
	// and pagination can be done properly.
	class PDFRender {
	private:
		bool pageStarted;
		bool firstLine;
		int pageCount;
		int pageContentStart;
		double xPos, yPos;	// position tracking for line wrapping
		std::string pageData;	// holds PDF stream contents
		std::string segment;	// character data
		char *segStyle;		// style of segment
		bool justWhiteSpace;
		int styleCurrent, stylePrev;
		double leading;
		char *buffer;
		// Deleted so PDFRender objects can not be copied
		PDFRender(const PDFRender &) = delete;
	public:
		PDFObjectTracker *oT;
		PDFStyle *style;
		int fontSize;		// properties supplied by user
		int fontSet;
		long pageWidth, pageHeight;
		GUI::Rectangle pageMargin;
		//
		PDFRender() {
			pageStarted = false;
			firstLine = false;
			pageCount = 0;
			pageContentStart = 0;
			xPos = 0.0;
			yPos = 0.0;
			justWhiteSpace = true;
			styleCurrent = STYLE_DEFAULT;
			stylePrev = STYLE_DEFAULT;
			leading = PDF_FONTSIZE_DEFAULT * PDF_SPACING_DEFAULT;
			oT = NULL;
			style = NULL;
			fontSize = 0;
			fontSet = PDF_FONT_DEFAULT;
			pageWidth = 100;
			pageHeight = 100;
			buffer = new char[250];
			segStyle = new char[100];
		}
		~PDFRender() {
			delete []style;
			delete []buffer;
			delete []segStyle;
		}
		//
		double fontToPoints(int thousandths) const {
			return (double)fontSize * thousandths / 1000.0;
		}
		void setStyle(char *buff, int style_) {
			int styleNext = style_;
			if (style_ == -1) { styleNext = styleCurrent; }
			*buff = '\0';
			if (styleNext != styleCurrent || style_ == -1) {
				if (style[styleCurrent].font != style[styleNext].font
				        || style_ == -1) {
					sprintf(buff, "/F%d %d Tf ",
					        style[styleNext].font + 1, fontSize);
				}
				if (strcmp(style[styleCurrent].fore, style[styleNext].fore) != 0
				        || style_ == -1) {
					strcat(buff, style[styleNext].fore);
					strcat(buff, "rg ");
				}
			}
		}
		//
		void startPDF() {
			if (fontSize <= 0) {
				fontSize = PDF_FONTSIZE_DEFAULT;
			}
			// leading is the term for distance between lines
			leading = fontSize * PDF_SPACING_DEFAULT;
			// sanity check for page size and margins
			int pageWidthMin = (int)leading + pageMargin.left + pageMargin.right;
			if (pageWidth < pageWidthMin) {
				pageWidth = pageWidthMin;
			}
			int pageHeightMin = (int)leading + pageMargin.top + pageMargin.bottom;
			if (pageHeight < pageHeightMin) {
				pageHeight = pageHeightMin;
			}
			// start to write PDF file here (PDF1.4Ref(p63))
			// ASCII>127 characters to indicate binary-possible stream
			oT->write("%PDF-1.3\n%\xc7\xec\x8f\xa2\n");
			styleCurrent = STYLE_DEFAULT;

			// build objects for font resources; note that font objects are
			// *expected* to start from index 1 since they are the first objects
			// to be inserted (PDF1.4Ref(p317))
			for (int i = 0; i < 4; i++) {
				sprintf(buffer, "<</Type/Font/Subtype/Type1"
				        "/Name/F%d/BaseFont/%s/Encoding/"
				        PDF_ENCODING
				        ">>\n", i + 1,
				        PDFfontNames[fontSet * 4 + i]);
				oT->add(buffer);
			}
			pageContentStart = oT->index;
		}
		void endPDF() {
			if (pageStarted) {	// flush buffers
				endPage();
			}
			// refer to all used or unused fonts for simplicity
			int resourceRef = oT->add(
			            "<</ProcSet[/PDF/Text]\n"
			            "/Font<</F1 1 0 R/F2 2 0 R/F3 3 0 R"
			            "/F4 4 0 R>> >>\n");
			// create all the page objects (PDF1.4Ref(p88))
			// forward reference pages object; calculate its object number
			int pageObjectStart = oT->index;
			int pagesRef = pageObjectStart + pageCount;
			for (int i = 0; i < pageCount; i++) {
				sprintf(buffer, "<</Type/Page/Parent %d 0 R\n"
				        "/MediaBox[ 0 0 %ld %ld"
				        "]\n/Contents %d 0 R\n"
				        "/Resources %d 0 R\n>>\n",
				        pagesRef, pageWidth, pageHeight,
				        pageContentStart + i, resourceRef);
				oT->add(buffer);
			}
			// create page tree object (PDF1.4Ref(p86))
			pageData = "<</Type/Pages/Kids[\n";
			for (int j = 0; j < pageCount; j++) {
				sprintf(buffer, "%d 0 R\n", pageObjectStart + j);
				pageData += buffer;
			}
			sprintf(buffer, "]/Count %d\n>>\n", pageCount);
			pageData += buffer;
			oT->add(pageData.c_str());
			// create catalog object (PDF1.4Ref(p83))
			sprintf(buffer, "<</Type/Catalog/Pages %d 0 R >>\n", pagesRef);
			int catalogRef = oT->add(buffer);
			// append the cross reference table (PDF1.4Ref(p64))
			long xref = oT->xref();
			// end the file with the trailer (PDF1.4Ref(p67))
			sprintf(buffer, "trailer\n<< /Size %d /Root %d 0 R\n>>"
			        "\nstartxref\n%ld\n%%%%EOF\n",
			        oT->index, catalogRef, xref);
			oT->write(buffer);
		}
		void add(char ch, int style_) {
			if (!pageStarted) {
				startPage();
			}
			// get glyph width (TODO future non-monospace handling)
			double glyphWidth = fontToPoints(PDFfontWidths[fontSet]);
			xPos += glyphWidth;
			// if cannot fit into a line, flush, wrap to next line
			if (xPos > pageWidth - pageMargin.right) {
				nextLine();
				xPos += glyphWidth;
			}
			// if different style, then change to style
			if (style_ != styleCurrent) {
				flushSegment();
				// output code (if needed) for new style
				setStyle(segStyle, style_);
				stylePrev = styleCurrent;
				styleCurrent = style_;
			}
			// escape these characters
			if (ch == ')' || ch == '(' || ch == '\\') {
				segment += '\\';
			}
			if (ch != ' ') { justWhiteSpace = false; }
			segment += ch;	// add to segment data
		}
		void flushSegment() {
			if (segment.length() > 0) {
				if (justWhiteSpace) {	// optimise
					styleCurrent = stylePrev;
				} else {
					pageData += segStyle;
				}
				pageData += "(";
				pageData += segment;
				pageData += ")Tj\n";
			}
			segment.clear();
			*segStyle = '\0';
			justWhiteSpace = true;
		}
		void startPage() {
			pageStarted = true;
			firstLine = true;
			pageCount++;
			double fontAscender = fontToPoints(PDFfontAscenders[fontSet]);
			yPos = pageHeight - pageMargin.top - fontAscender;
			// start a new page
			sprintf(buffer, "BT 1 0 0 1 %d %d Tm\n",
			        pageMargin.left, (int)yPos);
			// force setting of initial font, colour
			setStyle(segStyle, -1);
			strcat(buffer, segStyle);
			pageData = buffer;
			xPos = pageMargin.left;
			segment.clear();
			flushSegment();
		}
		void endPage() {
			pageStarted = false;
			flushSegment();
			// build actual text object; +3 is for "ET\n"
			// PDF1.4Ref(p38) EOL marker preceding endstream not counted
			char *textObj = new char[pageData.length() + 100];
			// concatenate stream within the text object
			sprintf(textObj, "<</Length %d>>\nstream\n%s"
			        "ET\nendstream\n",
			        static_cast<int>(pageData.length() - 1 + 3),
			        pageData.c_str());
			oT->add(textObj);
			delete []textObj;
		}
		void nextLine() {
			if (!pageStarted) {
				startPage();
			}
			xPos = pageMargin.left;
			flushSegment();
			// PDF follows cartesian coords, subtract -> down
			yPos -= leading;
			double fontDescender = fontToPoints(PDFfontDescenders[fontSet]);
			if (yPos < pageMargin.bottom + fontDescender) {
				endPage();
				startPage();
				return;
			}
			if (firstLine) {
				// avoid breakage due to locale setting
				int f = (int)(leading * 10 + 0.5);
				sprintf(buffer, "0 -%d.%d TD\n", f / 10, f % 10);
				firstLine = false;
			} else {
				sprintf(buffer, "T*\n");
			}
			pageData += buffer;
		}
	};
	PDFRender pr;

	RemoveFindMarks();
	wEditor.Call(SCI_COLOURISE, 0, -1);
	// read exporter flags
	int tabSize = props.GetInt("tabsize", PDF_TAB_DEFAULT);
	if (tabSize < 0) {
		tabSize = PDF_TAB_DEFAULT;
	}
	// read magnification value to add to default screen font size
	pr.fontSize = props.GetInt("export.pdf.magnification");
	// set font family according to face name
	std::string propItem = props.GetExpandedString("export.pdf.font");
	pr.fontSet = PDF_FONT_DEFAULT;
	if (propItem.length()) {
		if (propItem == "Courier")
			pr.fontSet = 0;
		else if (propItem == "Helvetica")
			pr.fontSet = 1;
		else if (propItem == "Times")
			pr.fontSet = 2;
	}
	// page size: width, height
	propItem = props.GetExpandedString("export.pdf.pagesize");
	char buffer[200];
	const char *ps = propItem.c_str();
	const char *next = GetNextPropItem(ps, buffer, 32);
	if (0 >= (pr.pageWidth = atol(buffer))) {
		pr.pageWidth = PDF_WIDTH_DEFAULT;
	}
	GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageHeight = atol(buffer))) {
		pr.pageHeight = PDF_HEIGHT_DEFAULT;
	}
	// page margins: left, right, top, bottom
	propItem = props.GetExpandedString("export.pdf.margins");
	ps = propItem.c_str();
	next = GetNextPropItem(ps, buffer, 32);
	if (0 >= (pr.pageMargin.left = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.left = PDF_MARGIN_DEFAULT;
	}
	next = GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.right = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.right = PDF_MARGIN_DEFAULT;
	}
	next = GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.top = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.top = PDF_MARGIN_DEFAULT;
	}
	GetNextPropItem(next, buffer, 32);
	if (0 >= (pr.pageMargin.bottom = static_cast<int>(atol(buffer)))) {
		pr.pageMargin.bottom = PDF_MARGIN_DEFAULT;
	}

	// collect all styles available for that 'language'
	// or the default style if no language is available...
	pr.style = new PDFStyle[STYLE_MAX + 1];
	for (int i = 0; i <= STYLE_MAX; i++) {	// get keys
		pr.style[i].font = 0;
		pr.style[i].fore[0] = '\0';

		StyleDefinition sd = StyleDefinitionFor(i);

		if (sd.specified != StyleDefinition::sdNone) {
			if (sd.italics) { pr.style[i].font |= 2; }
			if (sd.IsBold()) { pr.style[i].font |= 1; }
			if (sd.fore.length()) {
				getPDFRGB(pr.style[i].fore, sd.fore.c_str());
			} else if (i == STYLE_DEFAULT) {
				strcpy(pr.style[i].fore, "0 0 0 ");
			}
			// grab font size from default style
			if (i == STYLE_DEFAULT) {
				if (sd.size > 0)
					pr.fontSize += sd.size;
				else
					pr.fontSize = PDF_FONTSIZE_DEFAULT;
			}
		}
	}
	// patch in default foregrounds
	for (int j = 0; j <= STYLE_MAX; j++) {
		if (pr.style[j].fore[0] == '\0') {
			strcpy(pr.style[j].fore, pr.style[STYLE_DEFAULT].fore);
		}
	}

	FILE *fp = saveName.Open(GUI_TEXT("wb"));
	if (!fp) {
		// couldn't open the file for saving, issue an error message
		FailedSaveMessageBox(saveName);
		return;
	}
	// initialise PDF rendering
	PDFObjectTracker ot(fp);
	pr.oT = &ot;
	pr.startPDF();

	// do here all the writing
	int lengthDoc = LengthDocument();
	TextReader acc(wEditor);

	if (!lengthDoc) {	// enable zero length docs
		pr.nextLine();
	} else {
		int lineIndex = 0;
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);

			if (ch == '\t') {
				// expand tabs
				int ts = tabSize - (lineIndex % tabSize);
				lineIndex += ts;
				for (; ts; ts--) {	// add ts count of spaces
					pr.add(' ', style);	// add spaces
				}
			} else if (ch == '\r' || ch == '\n') {
				if (ch == '\r' && acc[i + 1] == '\n') {
					i++;
				}
				// close and begin a newline...
				pr.nextLine();
				lineIndex = 0;
			} else {
				// write the character normally...
				pr.add(ch, style);
				lineIndex++;
			}
		}
	}
	// write required stuff and close the PDF file
	pr.endPDF();
	if (fclose(fp) != 0) {
		FailedSaveMessageBox(saveName);
	}
}

