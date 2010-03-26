// SciTE - Scintilla based Text Editor
/** @file GUIGTK.cxx
 ** Interface to platform GUI facilities.
 ** Split off from Scintilla's Platform.h to avoid SciTE depending on implementation of Scintilla.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <time.h>

#include <string>

#include <gtk/gtk.h>

#include "Scintilla.h"
#include "ScintillaWidget.h"

#include "GUI.h"

namespace GUI {

gui_string StringFromUTF8(const char *s) {
	return gui_string(s);
}

std::string UTF8FromString(const gui_string &s) {
	return s;
}

gui_string StringFromInteger(int i) {
	char number[32];
	sprintf(number, "%0d", i);
	return gui_string(number);
}

static GtkWidget *PWidget(WindowID wid) {
	return reinterpret_cast<GtkWidget *>(wid);
}

void Window::Destroy() {
	if (wid)
		gtk_widget_destroy(GTK_WIDGET(wid));
	wid = 0;
}

bool Window::HasFocus() {
	return GTK_WIDGET_HAS_FOCUS(wid);
}

Rectangle Window::GetPosition() {
	// Before any size allocated pretend its 1000 wide so not scrolled
	Rectangle rc(0, 0, 1000, 1000);
	if (wid) {
		rc.left = PWidget(wid)->allocation.x;
		rc.top = PWidget(wid)->allocation.y;
		if (PWidget(wid)->allocation.width > 20) {
			rc.right = rc.left + PWidget(wid)->allocation.width;
			rc.bottom = rc.top + PWidget(wid)->allocation.height;
		}
	}
	return rc;
}

void Window::SetPosition(Rectangle rc) {
	GtkAllocation alloc;
	alloc.x = rc.left;
	alloc.y = rc.top;
	alloc.width = rc.Width();
	alloc.height = rc.Height();
	gtk_widget_size_allocate(PWidget(wid), &alloc);
}

Rectangle Window::GetClientPosition() {
	// On GTK+, the client position is the window position
	return GetPosition();
}

void Window::Show(bool show) {
	if (show)
		gtk_widget_show(PWidget(wid));
}

void Window::InvalidateAll() {
	if (wid) {
		gtk_widget_queue_draw(PWidget(wid));
	}
}

void Window::SetTitle(const char *s) {
	gtk_window_set_title(GTK_WINDOW(wid), s);
}

void Menu::CreatePopUp() {
	Destroy();
	mid = gtk_item_factory_new(GTK_TYPE_MENU, "<main>", NULL);
}

void Menu::Destroy() {
	if (mid)
		g_object_unref(G_OBJECT(mid));
	mid = 0;
}

void Menu::Show(Point pt, Window &) {
	int screenHeight = gdk_screen_height();
	int screenWidth = gdk_screen_width();
	GtkItemFactory *factory = reinterpret_cast<GtkItemFactory *>(mid);
	GtkWidget *widget = gtk_item_factory_get_widget(factory, "<main>");
	gtk_widget_show_all(widget);
	GtkRequisition requisition;
	gtk_widget_size_request(widget, &requisition);
	if ((pt.x + requisition.width) > screenWidth) {
		pt.x = screenWidth - requisition.width;
	}
	if ((pt.y + requisition.height) > screenHeight) {
		pt.y = screenHeight - requisition.height;
	}
	gtk_item_factory_popup(factory, pt.x - 4, pt.y - 4, 3,
		gtk_get_current_event_time());
}

ElapsedTime::ElapsedTime() {
	GTimeVal curTime;
	g_get_current_time(&curTime);
	bigBit = curTime.tv_sec;
	littleBit = curTime.tv_usec;
}

double ElapsedTime::Duration(bool reset) {
	GTimeVal curTime;
	g_get_current_time(&curTime);
	long endBigBit = curTime.tv_sec;
	long endLittleBit = curTime.tv_usec;
	double result = 1000000.0 * (endBigBit - bigBit);
	result += endLittleBit - littleBit;
	result /= 1000000.0;
	if (reset) {
		bigBit = endBigBit;
		littleBit = endLittleBit;
	}
	return result;
}

long ScintillaWindow::Send(unsigned int msg, unsigned long wParam, long lParam) {
	return scintilla_send_message(SCINTILLA(GetID()), msg, wParam, lParam);
}

long ScintillaWindow::SendPointer(unsigned int msg, unsigned long wParam, void *lParam) {
	return scintilla_send_message(SCINTILLA(GetID()), msg, wParam, reinterpret_cast<sptr_t>(lParam));
}

bool IsDBCSLeadByte(int codePage, char ch) {
	// Byte ranges found in Wikipedia articles with relevant search strings in each case
	unsigned char uch = static_cast<unsigned char>(ch);
	switch (codePage) {
		case 932:
			// Shift_jis
			return ((uch >= 0x81) && (uch <= 0x9F)) ||
				((uch >= 0xE0) && (uch <= 0xEF));
		case 936:
			// GBK
			return (uch >= 0x81) && (uch <= 0xFE);
		case 950:
			// Big5
			return (uch >= 0x81) && (uch <= 0xFE);
		// Korean EUC-KR may be code page 949.
	}
	return false;
}

}
