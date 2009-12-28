/*
 * Ayttm 
 *
 * Copyright (C) 2007, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Tray Icon code
 */

#include "gtk_globals.h"
#include "status.h"
#include "ayttm_tray.h"
#include "intl.h"
#include "prefs.h"
#include "away_window.h"

GtkWidget *popup_menu = NULL;
GtkWidget *sign_on_menu_item = NULL;
GtkWidget *sign_off_menu_item = NULL;
GtkWidget *set_away_menu_item = NULL;
GtkWidget *prefs_menu_item = NULL;
GtkWidget *quit_menu_item = NULL;

static void build_prefs_callback(GtkWidget *widget, gpointer stats)
{
	ayttm_prefs_show_window(0);
}

/* Show or Hide the main status window */
gboolean icon_activate(GtkStatusIcon *status_icon, gpointer data)
{
	if (statuswindow) {
		if (GTK_WIDGET_VISIBLE(GTK_WIDGET(statuswindow)))
			hide_status_window();
		else
			show_status_window();
	} else
		eb_status_window();

	return FALSE;
}

/* End Ayttm */
void ayttm_end_app(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	eb_debug(DBG_CORE, "Writing preferences\n");
	ayttm_prefs_write();
	eb_debug(DBG_CORE, "Signing out...\n");
	eb_sign_off_all();
	eb_debug(DBG_CORE, "Signed out\n");

	gtk_main_quit();
}

void show_popup(GtkStatusIcon *status_icon, guint button, guint activate_time,
	gpointer data)
{
	if (!popup_menu)
		return;

	gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL,
		gtk_status_icon_position_menu,
		status_icon, button, activate_time);
}

#define APPEND_MENU_ITEM(menuitem,label,icon,callback) \
{ \
\
	if(label) {\
		menuitem = gtk_image_menu_item_new_from_stock(icon,NULL); \
		gtk_label_set_text_with_mnemonic(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),label); \
	} else {\
		menuitem = gtk_separator_menu_item_new(); \
	} \
\
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), menuitem); \
	if(callback != NULL) \
		g_signal_connect(menuitem, "activate", callback, NULL); \
	gtk_widget_show(menuitem); \
}

/* Build the popup menu for the tray icon */
void build_popup_menu()
{
	GtkWidget *separator_menu_item;

	popup_menu = gtk_menu_new();

	APPEND_MENU_ITEM(sign_on_menu_item, N_("S_how/Hide Status Window"),
		GTK_STOCK_FULLSCREEN, G_CALLBACK(icon_activate));
	APPEND_MENU_ITEM(sign_on_menu_item, N_("Sign o_n All"),
		GTK_STOCK_CONNECT, G_CALLBACK(eb_sign_on_all));
	APPEND_MENU_ITEM(sign_off_menu_item, N_("Sign o_ff All"),
		GTK_STOCK_DISCONNECT, G_CALLBACK(eb_sign_off_all));
	APPEND_MENU_ITEM(set_away_menu_item, N_("Set as _away"),
		"ayttm_away", G_CALLBACK(show_away_choicewindow));
	APPEND_MENU_ITEM(prefs_menu_item, N_("_Preferences"),
		GTK_STOCK_PREFERENCES, G_CALLBACK(build_prefs_callback));
	APPEND_MENU_ITEM(separator_menu_item, NULL, NULL, NULL);
	APPEND_MENU_ITEM(quit_menu_item, N_("_Quit"),
		GTK_STOCK_QUIT, G_CALLBACK(ayttm_end_app));
}

#undef APPEND_MENU_ITEM

void set_tray_menu_sensitive(gboolean online, unsigned int account_count)
{
	gtk_widget_set_sensitive(sign_on_menu_item, (online != account_count));
	gtk_widget_set_sensitive(sign_off_menu_item, online);
	gtk_widget_set_sensitive(set_away_menu_item, online);
}

/* Load the tray icon */
void ay_load_tray_icon(GdkPixbuf *default_icon)
{
	ayttm_status_icon = gtk_status_icon_new_from_pixbuf(default_icon);
	gtk_status_icon_set_tooltip(ayttm_status_icon, "Ayttm");

	build_popup_menu();
	eb_status_window();

	g_signal_connect(ayttm_status_icon, "activate",
		G_CALLBACK(icon_activate), NULL);
	g_signal_connect(ayttm_status_icon, "popup-menu",
		G_CALLBACK(show_popup), NULL);
}
