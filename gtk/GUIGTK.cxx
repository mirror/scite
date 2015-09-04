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
	if (s)
		return gui_string(s);
	else
		return gui_string("");
}

gui_string StringFromUTF8(const std::string &s) {
	return s;
}

std::string UTF8FromString(const gui_string &s) {
	return s;
}

gui_string StringFromInteger(long i) {
	char number[32];
	sprintf(number, "%0ld", i);
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
	return gtk_widget_has_focus(GTK_WIDGET(wid));
}

Rectangle Window::GetPosition() {
	// Before any size allocated pretend its 1000 wide so not scrolled
	Rectangle rc(0, 0, 1000, 1000);
	if (wid) {
		GtkAllocation allocation;
#if GTK_CHECK_VERSION(3,0,0)
		gtk_widget_get_allocation(PWidget(wid), &allocation);
#else
		allocation = PWidget(wid)->allocation;
#endif
		rc.left = allocation.x;
		rc.top = allocation.y;
		if (allocation.width > 20) {
			rc.right = rc.left + allocation.width;
			rc.bottom = rc.top + allocation.height;
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
	mid = gtk_menu_new();
	g_object_ref_sink(G_OBJECT(mid));
}

void Menu::Destroy() {
	if (mid)
		g_object_unref(mid);
	mid = 0;
}

static void  MenuPositionFunc(GtkMenu *, gint *x, gint *y, gboolean *, gpointer userData) {
	sptr_t intFromPointer = reinterpret_cast<sptr_t>(userData);
	*x = intFromPointer & 0xffff;
	*y = intFromPointer >> 16;
}

void Menu::Show(Point pt, Window &) {
	int screenHeight = gdk_screen_height();
	int screenWidth = gdk_screen_width();
	GtkMenu *widget = reinterpret_cast<GtkMenu *>(mid);
	gtk_widget_show_all(GTK_WIDGET(widget));
	GtkRequisition requisition;
#if GTK_CHECK_VERSION(3,0,0)
	gtk_widget_get_preferred_size(GTK_WIDGET(widget), NULL, &requisition);
#else
	gtk_widget_size_request(GTK_WIDGET(widget), &requisition);
#endif
	if ((pt.x + requisition.width) > screenWidth) {
		pt.x = screenWidth - requisition.width;
	}
	if ((pt.y + requisition.height) > screenHeight) {
		pt.y = screenHeight - requisition.height;
	}
	gtk_menu_popup(widget, NULL, NULL, MenuPositionFunc,
		reinterpret_cast<void *>((pt.y << 16) | pt.x), 0,
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

sptr_t ScintillaWindow::Send(unsigned int msg, uptr_t wParam, sptr_t lParam) {
	return scintilla_send_message(SCINTILLA(GetID()), msg, wParam, lParam);
}

sptr_t ScintillaWindow::SendPointer(unsigned int msg, uptr_t wParam, void *lParam) {
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
