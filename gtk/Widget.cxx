// SciTE - Scintilla based Text Editor
// Widget.cxx - code for manipulating  GTK+ widgets
// Copyright 2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "Scintilla.h"

#include "GUI.h"
#include "StringHelpers.h"
#include "Widget.h"

WBase::operator GtkWidget*() {
	return GTK_WIDGET(GetID());
}

GtkWidget* WBase::Pointer() {
	return GTK_WIDGET(GetID());
}

bool WBase::Sensitive() {
	return IS_WIDGET_SENSITIVE(Pointer());
}

void WBase::SetSensitive(bool sensitive) {
	gtk_widget_set_sensitive(Pointer(), sensitive);
}

void WStatic::Create(GUI::gui_string text) {
	SetID(gtk_label_new_with_mnemonic(text.c_str()));
}

void WStatic::SetMnemonicFor(WBase &w) {
	gtk_label_set_mnemonic_widget(GTK_LABEL(Pointer()), w);
}

void WEntry::Create(const GUI::gui_char *text) {
	SetID(gtk_entry_new());
	if (text)
		gtk_entry_set_text(GTK_ENTRY(GetID()), text);
}

void WEntry::ActivatesDefault() {
	gtk_entry_set_activates_default(GTK_ENTRY(GetID()), TRUE);
}

const GUI::gui_char *WEntry::Text() {
	return gtk_entry_get_text(GTK_ENTRY(GetID()));
}

int WEntry::Value() {
	return atoi(Text());
}

void WEntry::SetText(const GUI::gui_char *text) {
	return gtk_entry_set_text(GTK_ENTRY(GetID()), text);
}

void WComboBoxEntry::Create() {
	SetID(gtk_combo_box_entry_new_text());
}

GtkEntry *WComboBoxEntry::Entry() {
	return GTK_ENTRY(gtk_bin_get_child(GTK_BIN(GetID())));
}

void WComboBoxEntry::ActivatesDefault() {
	gtk_entry_set_activates_default(Entry(), TRUE);
}

const GUI::gui_char *WComboBoxEntry::Text() {
	return gtk_entry_get_text(Entry());
}

void WComboBoxEntry::SetText(const GUI::gui_char *text) {
	return gtk_entry_set_text(Entry(), text);
}

bool WComboBoxEntry::HasFocusOnSelfOrChild() {
	return HasFocus() || IS_WIDGET_FOCUSSED(Entry());
}

void WComboBoxEntry::FillFromMemory(const std::vector<std::string> &mem, bool useTop) {
	for (int i = 0; i < 10; i++) {
		gtk_combo_box_remove_text(GTK_COMBO_BOX(GetID()), 0);
	}
	for (size_t i = 0; i < mem.size(); i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(GetID()), mem[i].c_str());
	}
	if (useTop) {
		gtk_entry_set_text(Entry(), mem[0].c_str());
	}
}

void WButton::Create(GUI::gui_string text, GCallback func, gpointer data) {
	SetID(gtk_button_new_with_mnemonic(text.c_str()));
#if GTK_CHECK_VERSION(2,20,0)
	gtk_widget_set_can_default(GTK_WIDGET(GetID()), TRUE);
#else
	GTK_WIDGET_SET_FLAGS(GetID(), GTK_CAN_DEFAULT);
#endif
	g_signal_connect(G_OBJECT(GetID()), "clicked", func, data);
}

void WButton::Create(GUI::gui_string text) {
	SetID(gtk_button_new_with_mnemonic(text.c_str()));
#if GTK_CHECK_VERSION(2,20,0)
	gtk_widget_set_can_default(GTK_WIDGET(GetID()), TRUE);
#else
	GTK_WIDGET_SET_FLAGS(GetID(), GTK_CAN_DEFAULT);
#endif
}

void WToggle::Create(const GUI::gui_string &text) {
	SetID(gtk_check_button_new_with_mnemonic(text.c_str()));
}
bool WToggle::Active() {
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GetID()));
}
void WToggle::SetActive(bool active) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GetID()), active);
}

WCheckDraw::WCheckDraw() : isActive(false), pbGrey(0), pStyle(0), over(false) {
}

WCheckDraw::~WCheckDraw() {
	if (pbGrey)
		g_object_unref(pbGrey);
	pbGrey = 0;
	if (pStyle)
		g_object_unref(pStyle);
	pStyle = 0;
}

static void GreyToAlpha(GdkPixbuf *ppb, GdkColor fore) {
	guchar *pixels = gdk_pixbuf_get_pixels(ppb);
	int rowStride = gdk_pixbuf_get_rowstride(ppb);
	int width = gdk_pixbuf_get_width(ppb);
	int height = gdk_pixbuf_get_height(ppb);
	for (int y =0; y<height; y++) {
		guchar *pixelsRow = pixels + rowStride * y;
		for (int x =0; x<width; x++) {
			guchar alpha = pixelsRow[0];
			pixelsRow[3] = 255 - alpha;
			pixelsRow[0] = fore.red / 256;
			pixelsRow[1] = fore.green / 256;
			pixelsRow[2] = fore.blue / 256;
			pixelsRow += 4;
		}
	}
}

const int stripIconWidth = 16;

void WCheckDraw::Create(const char **xpmImage, GUI::gui_string toolTip, GtkStyle *pStyle_) {
	isActive = false;
	pbGrey = gdk_pixbuf_new_from_xpm_data(xpmImage);

	GtkWidget *da = gtk_drawing_area_new();
	pStyle = gtk_style_copy(pStyle_);

#if GTK_CHECK_VERSION(2,20,0)
	gtk_widget_set_can_focus(da, TRUE);
	gtk_widget_set_sensitive(da, TRUE);
#else
	GTK_WIDGET_SET_FLAGS(da, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS(da, GTK_SENSITIVE);
#endif

	gtk_widget_set_events(da,
			      GDK_EXPOSURE_MASK
	                      | GDK_FOCUS_CHANGE_MASK
			      | GDK_ENTER_NOTIFY_MASK
			      | GDK_LEAVE_NOTIFY_MASK
			      | GDK_BUTTON_PRESS_MASK
			      | GDK_BUTTON_RELEASE_MASK
			      | GDK_POINTER_MOTION_MASK
			      | GDK_POINTER_MOTION_HINT_MASK
			     );
	gtk_widget_set_size_request(da, checkButtonWidth, 20);
	SetID(da);

	GUI::gui_string toolTipNoMnemonic = toolTip;
	size_t posMnemonic = toolTipNoMnemonic.find("_");
	if (posMnemonic != GUI::gui_string::npos)
		toolTipNoMnemonic.replace(posMnemonic, 1, "");
#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(da, toolTipNoMnemonic.c_str());
#endif

	g_signal_connect(G_OBJECT(da), "focus-in-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "focus-out-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "button-press-event", G_CALLBACK(ButtonsPress), this);
	g_signal_connect(G_OBJECT(da), "enter-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "leave-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "key-press-event", G_CALLBACK(KeyDown), this);
	g_signal_connect(G_OBJECT(da), "expose-event", G_CALLBACK(ExposeEvent), this);
}

bool WCheckDraw::Active() {
	return isActive;
}

void WCheckDraw::SetActive(bool active) {
	isActive = active;
	InvalidateAll();
}

void WCheckDraw::Toggle() {
	isActive = !isActive;
	InvalidateAll();
}

gboolean WCheckDraw::Focus(GtkWidget */*widget*/, GdkEventFocus */*event*/, WCheckDraw *pcd) {
	pcd->InvalidateAll();
	return FALSE;
}

gboolean WCheckDraw::KeyDown(GtkWidget */*widget*/, GdkEventKey *event, WCheckDraw *pcd) {
	if (event->keyval == ' ') {
		pcd->Toggle();
	}
	return FALSE;
}

gint WCheckDraw::Press(GtkWidget *widget, GdkEventButton *event) {
	if (event->button == 3) {
		// Allow to go to container for pop up menu
		return FALSE;
	} else {
		gtk_widget_grab_focus(widget);
		Toggle();
		return TRUE;
	}
}

gint WCheckDraw::ButtonsPress(GtkWidget *widget, GdkEventButton *event, WCheckDraw *pcd) {
	return pcd->Press(widget, event);
}

gboolean WCheckDraw::MouseEnterLeave(GtkWidget */*widget*/, GdkEventCrossing *event, WCheckDraw *pcd) {
	pcd->over = event->type == GDK_ENTER_NOTIFY;
	pcd->InvalidateAll();
	return FALSE;
}

gboolean WCheckDraw::Expose(GtkWidget *widget, GdkEventExpose */*event*/) {
	pStyle = gtk_style_attach(pStyle, widget->window);

	GdkRectangle area;
	area.x = 0;
	area.y = 0;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;
	int heightOffset = (area.height - checkButtonWidth) / 2;
	if (heightOffset < 0)
		heightOffset = 0;
	GdkGC *gcDraw = gdk_gc_new(GDK_DRAWABLE(widget->window));
	bool active = isActive;
	GtkStateType state = active ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL;
	GtkShadowType shadow = GTK_SHADOW_IN;
	if (over) {
		state = GTK_STATE_PRELIGHT;
		shadow = GTK_SHADOW_OUT;
	}
	if (active || over)
		gtk_paint_box(pStyle, widget->window,
			       state,
			       shadow,
			       &area, widget, const_cast<char *>("button"),
			       0, 0,
			       area.width, area.height);
	if (HasFocus()) {
		// Draw focus inset by 2 pixels
		gtk_paint_focus(pStyle, widget->window,
			       state,
			       &area, widget, const_cast<char *>("button"),
			       2, 2,
			       area.width-4, area.height-4);
	}

	GdkColor fore = pStyle->fg[GTK_STATE_NORMAL];
	// Give it an alpha channel
	GdkPixbuf *pbAlpha = gdk_pixbuf_add_alpha(pbGrey, TRUE, 0xff, 0xff, 0);
	// Convert the grey to alpha and make black
	GreyToAlpha(pbAlpha, fore);

	int activeOffset = active ? 1 : 0;
	gdk_draw_pixbuf(
		widget->window,
		gcDraw,
		pbAlpha,
		0, 0,
		1 + 2 + activeOffset, 3 + heightOffset + activeOffset,
		checkIconWidth, checkIconWidth,
		GDK_RGB_DITHER_NONE, 0, 0);
	g_object_unref(pbAlpha);
	g_object_unref(gcDraw);
	return TRUE;
}

gboolean WCheckDraw::ExposeEvent(GtkWidget *widget, GdkEventExpose *event, WCheckDraw *pcd) {
	return pcd->Expose(widget, event);
}

WTable::WTable(int rows_, int columns_) :
	rows(rows_), columns(columns_), next(0) {
	SetID(gtk_table_new(rows, columns, FALSE));
}

void WTable::Add(GtkWidget *child, int width, bool expand, int xpadding, int ypadding) {
	GtkAttachOptions opts = static_cast<GtkAttachOptions>(
		GTK_SHRINK | GTK_FILL);
	GtkAttachOptions optsExpand = static_cast<GtkAttachOptions>(
		GTK_SHRINK | GTK_FILL | GTK_EXPAND);

	if (child) {
		gtk_table_attach(GTK_TABLE(GetID()), child,
			next % columns, next % columns + width,
			next / columns, (next / columns) + 1,
			expand ? optsExpand : opts, opts,
			xpadding, ypadding);
	}
	next += width;
}

void WTable::Label(GtkWidget *child) {
	gtk_misc_set_alignment(GTK_MISC(child), 1.0, 0.5);
	Add(child);
}

void WTable::PackInto(GtkBox *box, gboolean expand) {
	gtk_box_pack_start(box, Pointer(), expand, expand, 0);
}

GUI::gui_char KeyFromLabel(GUI::gui_string label) {
	if (!label.empty()) {
		size_t posMnemonic = label.find('_');
		return tolower(label[posMnemonic + 1]);
	}
	return 0;
}

void Dialog::Create(const GUI::gui_string &title) {
	wid = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(GetID()), title.c_str());
	gtk_window_set_resizable(GTK_WINDOW(GetID()), TRUE);
}

void Dialog::Display(GtkWidget *parent, bool modal) {
	// Mark it as a modal transient dialog
	gtk_window_set_modal(GTK_WINDOW(GetID()), modal);
	if (parent) {
		gtk_window_set_transient_for(GTK_WINDOW(GetID()), GTK_WINDOW(parent));
	}
	g_signal_connect(G_OBJECT(GetID()), "destroy", G_CALLBACK(SignalDestroy), this);
	gtk_widget_show_all(GTK_WIDGET(GetID()));
	if (modal) {
		while (Created()) {
			gtk_main_iteration();
		}
	}
}

GtkWidget *Dialog::ResponseButton(const GUI::gui_string &text, int responseID) {
	return gtk_dialog_add_button(GTK_DIALOG(GetID()), text.c_str(), responseID);
}

void Dialog::Present() {
	gtk_window_present(GTK_WINDOW(GetID()));
}

void Dialog::SignalDestroy(GtkWidget *, Dialog *d) {
	if (d) {
		d->SetID(0);
	}
}

void DestroyDialog(GtkWidget *, gpointer *window) {
	if (window) {
		GUI::Window *pwin = reinterpret_cast<GUI::Window *>(window);
		*(pwin) = 0;
	}
}

void Strip::Creation(GtkWidget *) {
	g_signal_connect(G_OBJECT(GetID()), "button-press-event", G_CALLBACK(ButtonsPress), this);
}

void Strip::Show(int) {
	gtk_widget_show(Widget(*this));
	visible = true;
}

void Strip::Close() {
	gtk_widget_hide(Widget(*this));
	visible = false;
}

bool Strip::KeyDown(GdkEventKey *event) {
	bool retVal = false;

	if (visible) {
		if (event->keyval == GDK_Escape) {
			Close();
			return true;
		}

		if (event->state & GDK_MOD1_MASK) {
			GList *childWidgets = gtk_container_get_children(GTK_CONTAINER(GetID()));
			for (GList *child = g_list_first(childWidgets); child; child = g_list_next(child)) {
				GtkWidget **w = (GtkWidget **)child;
				std::string name = gtk_widget_get_name(*w);
				std::string label;
				if (name == "GtkButton" || name == "GtkCheckButton") {
					label = gtk_button_get_label(GTK_BUTTON(*w));
				} else if (name == "GtkLabel") {
					label = gtk_label_get_label(GTK_LABEL(*w));
				}
				char key = KeyFromLabel(label);
				if (static_cast<unsigned int>(key) == event->keyval) {
					//fprintf(stderr, "%p %s %s %c\n", *w, name.c_str(), label.c_str(), key);
					if (name == "GtkButton" || name == "GtkCheckButton") {
						gtk_button_clicked(GTK_BUTTON(*w));
					} else if (name == "GtkLabel") {
						// Only ever use labels to label ComboBoxEntry
						GtkWidget *pwidgetSelect = gtk_label_get_mnemonic_widget(GTK_LABEL(*w));
						if (pwidgetSelect) {
							gtk_widget_grab_focus(pwidgetSelect);
						}
					}
					retVal = true;
					break;
				}
			}
			g_list_free(childWidgets);
		}
	}
	return retVal;
}

void Strip::MenuSignal(GtkMenuItem *menuItem, Strip *pStrip) {
	sptr_t cmd = (sptr_t)(g_object_get_data(G_OBJECT(menuItem), "CmdNum"));
	pStrip->MenuAction(cmd);
}

void Strip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	allowMenuActions = false;
	GUI::gui_string localised = localiser->Text(label);
	GtkWidget *menuItem = gtk_check_menu_item_new_with_mnemonic(localised.c_str());
	gtk_menu_shell_append(GTK_MENU_SHELL(popup.GetID()), menuItem);
	g_object_set_data(G_OBJECT(menuItem), "CmdNum", reinterpret_cast<void *>((sptr_t)(cmd)));
	g_signal_connect(G_OBJECT(menuItem),"activate", G_CALLBACK(MenuSignal), this);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuItem), checked ? TRUE : FALSE);
	allowMenuActions = true;
}

void Strip::ChildFocus(GtkWidget *widget) {
	childHasFocus = widget != 0;
}

gboolean Strip::ChildFocusSignal(GtkContainer */*container*/, GtkWidget *widget, Strip *pStrip) {
	pStrip->ChildFocus(widget);
	return FALSE;
}

gboolean Strip::FocusSignal(GtkWidget */*widget*/, GtkDirectionType direction, Strip *pStrip) {
	return pStrip->Focus(direction);
}

bool Strip::VisibleHasFocus() {
	return visible && childHasFocus;
}

gint Strip::ButtonsPress(GtkWidget *, GdkEventButton *event, Strip *pstrip) {
	if (event->button == 3) {
		pstrip->ShowPopup();
		return TRUE;
	}
	return FALSE;
}
