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

// Key names are longer for GTK+ 3
#if GTK_CHECK_VERSION(3,0,0)
#define GKEY_Escape GDK_KEY_Escape
#define GKEY_Void GDK_KEY_VoidSymbol
#else
#define GKEY_Escape GDK_Escape
#define GKEY_Void GDK_VoidSymbol
#endif

WBase::operator GtkWidget*() const {
	return GTK_WIDGET(GetID());
}

GtkWidget* WBase::Pointer() {
	return GTK_WIDGET(GetID());
}

bool WBase::Sensitive() {
	return gtk_widget_get_sensitive(GTK_WIDGET(Pointer()));
}

void WStatic::Create(GUI::gui_string text) {
	SetID(gtk_label_new_with_mnemonic(text.c_str()));
}

bool WStatic::HasMnemonic() {
	return gtk_label_get_mnemonic_keyval(GTK_LABEL(Pointer())) != GKEY_Void;
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

void WEntry::SetValid(GtkEntry *entry, bool valid) {
#if GTK_CHECK_VERSION(3,0,0)
	gtk_widget_set_name(GTK_WIDGET(entry), valid ? "" : "entryInvalid");
#else
	if (valid) {
		// Reset widget's background color
		// to the one given by the GTK theme
		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, NULL);
	} else {
		GdkColor red = { 0, 0xFFFF, 0x6666, 0x6666 };
		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, &red);
	}
#endif
}

void WComboBoxEntry::Create() {
#if GTK_CHECK_VERSION(3,0,0)
	SetID(gtk_combo_box_text_new_with_entry());
#else
	SetID(gtk_combo_box_entry_new_text());
#endif
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
	return HasFocus() || gtk_widget_has_focus(GTK_WIDGET(Entry()));
}

void WComboBoxEntry::RemoveText(int position) {
#if GTK_CHECK_VERSION(3,0,0)
	gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(GetID()), position);
#else
	gtk_combo_box_remove_text(GTK_COMBO_BOX(GetID()), position);
#endif
}

void WComboBoxEntry::AppendText(const char *text) {
#if GTK_CHECK_VERSION(3,0,0)
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(GetID()), text);
#if GTK_CHECK_VERSION(3,14,0)
	gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(GetID()), GTK_SENSITIVITY_ON);
#endif
#else
	gtk_combo_box_append_text(GTK_COMBO_BOX(GetID()), text);
#endif
}

void WComboBoxEntry::ClearList() {
#if GTK_CHECK_VERSION(3,0,0)
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(GetID()));
#if GTK_CHECK_VERSION(3,14,0)
	gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(GetID()), GTK_SENSITIVITY_OFF);
#endif
#else
	for (int i = 0; i < 10; i++) {
		RemoveText(0);
	}
#endif
}

void WComboBoxEntry::FillFromMemory(const std::vector<std::string> &mem, bool useTop) {
	ClearList();
	for (size_t i = 0; i < mem.size(); i++) {
		AppendText(mem[i].c_str());
	}
	if (useTop) {
		gtk_entry_set_text(Entry(), mem[0].c_str());
	}
}

void WButton::Create(GUI::gui_string text, GCallback func, gpointer data) {
	SetID(gtk_button_new_with_mnemonic(text.c_str()));
	gtk_widget_set_can_default(GTK_WIDGET(GetID()), TRUE);
	g_signal_connect(G_OBJECT(GetID()), "clicked", func, data);
}

void WButton::Create(GUI::gui_string text) {
	SetID(gtk_button_new_with_mnemonic(text.c_str()));
	gtk_widget_set_can_default(GTK_WIDGET(GetID()), TRUE);
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

void WProgress::Create() {
	SetID(gtk_progress_bar_new());
}

WCheckDraw::WCheckDraw() : isActive(false), pbGrey(0), over(false), cdfn(NULL), user(NULL) {
#if !GTK_CHECK_VERSION(3,4,0)
	pStyle = 0;
#endif
}

WCheckDraw::~WCheckDraw() {
	if (pbGrey)
		g_object_unref(pbGrey);
	pbGrey = 0;
#if !GTK_CHECK_VERSION(3,4,0)
	if (pStyle)
		g_object_unref(pStyle);
	pStyle = 0;
#endif
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

void WCheckDraw::Create(const char **xpmImage, GUI::gui_string toolTip, GtkStyle *pStyle_) {
	isActive = false;
	pbGrey = gdk_pixbuf_new_from_xpm_data(xpmImage);

	GtkWidget *da = gtk_drawing_area_new();
#if GTK_CHECK_VERSION(3,4,0)
	(void)pStyle_;
#else
	pStyle = gtk_style_copy(pStyle_);
#endif

	gtk_widget_set_can_focus(da, TRUE);
	gtk_widget_set_sensitive(da, TRUE);

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
	gtk_widget_set_tooltip_text(da, toolTipNoMnemonic.c_str());

	g_signal_connect(G_OBJECT(da), "focus-in-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "focus-out-event", G_CALLBACK(Focus), this);
	g_signal_connect(G_OBJECT(da), "button-press-event", G_CALLBACK(ButtonsPress), this);
	g_signal_connect(G_OBJECT(da), "enter-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "leave-notify-event", G_CALLBACK(MouseEnterLeave), this);
	g_signal_connect(G_OBJECT(da), "key-press-event", G_CALLBACK(KeyDown), this);
#if GTK_CHECK_VERSION(3,0,0)
	g_signal_connect(G_OBJECT(da), "draw", G_CALLBACK(DrawEvent), this);
#else
	g_signal_connect(G_OBJECT(da), "expose-event", G_CALLBACK(ExposeEvent), this);
#endif
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
	if (cdfn)
		cdfn(this, user);
}

void WCheckDraw::SetChangeFunction(ChangeFunction cdfn_, void *user_) {
	cdfn = cdfn_;
	user = user_;
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

#if GTK_CHECK_VERSION(3,0,0)

gboolean WCheckDraw::Draw(GtkWidget *widget, cairo_t *cr) {
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);

#if GTK_CHECK_VERSION(3,4,0)
	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	gtk_style_context_save(context);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
#else
	GdkWindow *window = gtk_widget_get_window(widget);
	pStyle = gtk_style_attach(pStyle, window);
#endif

	int heightOffset = (allocation.height - checkButtonWidth) / 2;
	if (heightOffset < 0)
		heightOffset = 0;

	bool active = isActive;
#if GTK_CHECK_VERSION(3,4,0)
	GtkStateFlags flags = active ? GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_NORMAL;
	if (over) {
		flags = static_cast<GtkStateFlags>(flags | GTK_STATE_FLAG_PRELIGHT);
	}
#else
	GtkStateType state = active ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL;
	if (over) {
		state = GTK_STATE_PRELIGHT;
	}
#endif
	if (active || over) {
#if GTK_CHECK_VERSION(3,4,0)
		gtk_style_context_set_state(context, flags);
		gtk_render_background(context, cr, 0, 0,
			allocation.width, allocation.height);
		gtk_render_frame(context, cr, 0, 0,
			allocation.width, allocation.height);
#else
		GtkShadowType shadow = over ? GTK_SHADOW_OUT : GTK_SHADOW_IN;
		gtk_paint_box(pStyle,
			cr,
			state,
			shadow,
			widget, "button",
			0, 0,
			allocation.width, allocation.height);
#endif
	}

	if (HasFocus()) {
		// Draw focus inset by 2 pixels
#if GTK_CHECK_VERSION(3,4,0)
		gtk_render_focus(context, cr, 2, 2,
			allocation.width-4, allocation.height-4);
#else
		gtk_paint_focus(pStyle,
			cr,
			state,
			widget, "button",
			2, 2,
			allocation.width-4, allocation.height-4);
#endif
	}

#if GTK_CHECK_VERSION(3,4,0)
	GdkRGBA rgbaFore;
	gtk_style_context_get_color(context, GTK_STATE_FLAG_NORMAL, &rgbaFore);
	GdkColor fore;
	fore.red = rgbaFore.red * 65535;
	fore.green = rgbaFore.green * 65535;
	fore.blue = rgbaFore.blue * 65535;
	fore.pixel = 0;
	gtk_style_context_restore(context);
#else
	GdkColor fore = pStyle->fg[GTK_STATE_NORMAL];
#endif
	// Give it an alpha channel
	GdkPixbuf *pbAlpha = gdk_pixbuf_add_alpha(pbGrey, TRUE, 0xff, 0xff, 0);
	// Convert the grey to alpha and make black
	GreyToAlpha(pbAlpha, fore);

	int activeOffset = active ? 1 : 0;
	int xOffset = 1 + 2 + activeOffset;
	int yOffset = 3 + heightOffset + activeOffset;
	gdk_cairo_set_source_pixbuf(cr, pbAlpha, xOffset, yOffset);
	cairo_rectangle(cr,
		xOffset, yOffset,
		checkIconWidth, checkIconWidth);
	cairo_fill(cr);
	g_object_unref(pbAlpha);

	return TRUE;
}

gboolean WCheckDraw::DrawEvent(GtkWidget *widget, cairo_t *cr, WCheckDraw *pcd) {
	return pcd->Draw(widget, cr);
}

#else

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
			       &area, widget, "button",
			       0, 0,
			       area.width, area.height);
	if (HasFocus()) {
		// Draw focus inset by 2 pixels
		gtk_paint_focus(pStyle, widget->window,
			       state,
			       &area, widget, "button",
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

#endif

#if GTK_CHECK_VERSION(3,4,0)
#define USE_GRID 1
#else
#define USE_GRID 0
#endif

WTable::WTable(int rows_, int columns_) :
	rows(rows_), columns(columns_), next(0) {
#if USE_GRID
	SetID(gtk_grid_new());
#else
	SetID(gtk_table_new(rows, columns, FALSE));
#endif
}

void WTable::Add(GtkWidget *child, int width, bool expand, int xpadding, int ypadding) {
	if (child) {
#if USE_GRID
		gtk_widget_set_hexpand(child, expand);
#if GTK_CHECK_VERSION(3,14,0)
		gtk_widget_set_margin_end(child, xpadding);
#else
		gtk_widget_set_margin_right(child, xpadding);
#endif
		gtk_widget_set_margin_bottom(child, ypadding);
		gtk_grid_attach(GTK_GRID(GetID()), child,
			next % columns, next / columns,
			width, 1);
#else
		GtkAttachOptions opts = static_cast<GtkAttachOptions>(
			GTK_SHRINK | GTK_FILL);
		GtkAttachOptions optsExpand = static_cast<GtkAttachOptions>(
			GTK_SHRINK | GTK_FILL | GTK_EXPAND);

		gtk_table_attach(GTK_TABLE(GetID()), child,
			next % columns, next % columns + width,
			next / columns, (next / columns) + 1,
			expand ? optsExpand : opts, opts,
			xpadding, ypadding);
#endif
	}
	next += width;
}

void WTable::Label(GtkWidget *child) {
#if GTK_CHECK_VERSION(3,14,0)
	gtk_widget_set_halign(child, GTK_ALIGN_END);
	gtk_widget_set_valign(child, GTK_ALIGN_BASELINE);
#else
	gtk_misc_set_alignment(GTK_MISC(child), 1.0, 0.5);
#endif
	Add(child);
}

void WTable::PackInto(GtkBox *box, gboolean expand) {
	gtk_box_pack_start(box, Pointer(), expand, expand, 0);
}

void WTable::Resize(int rows_, int columns_) {
	rows = rows_;
	columns = columns_;
#if !USE_GRID
	gtk_table_resize(GTK_TABLE(GetID()), rows, columns);
#endif
	next = 0;
}

void WTable::NextLine() {
	next = ((next - 1) / columns + 1) * columns;
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

GtkWidget *Dialog::ContentArea() {
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_dialog_get_content_area(GTK_DIALOG(GetID()));
#else
	return GTK_DIALOG(GetID())->vbox;
#endif
}

void Dialog::SignalDestroy(GtkWidget *, Dialog *d) {
	if (d) {
		d->SetID(0);
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
		if (event->keyval == GKEY_Escape) {
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
	sptr_t cmd = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuItem), "CmdNum"));
	pStrip->MenuAction(cmd);
}

void Strip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	allowMenuActions = false;
	GUI::gui_string localised = localiser->Text(label);
	GtkWidget *menuItem = gtk_check_menu_item_new_with_mnemonic(localised.c_str());
	gtk_menu_shell_append(GTK_MENU_SHELL(popup.GetID()), menuItem);
	g_object_set_data(G_OBJECT(menuItem), "CmdNum", GINT_TO_POINTER(cmd));
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

bool Strip::VisibleHasFocus() const {
	return visible && childHasFocus;
}

gint Strip::ButtonsPress(GtkWidget *, GdkEventButton *event, Strip *pstrip) {
	if (event->button == 3) {
		pstrip->ShowPopup();
		return TRUE;
	}
	return FALSE;
}
