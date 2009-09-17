/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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

#include <assert.h>

#include <gdk/gdkprivate.h>

#include "gtkutils.h"
#include "gtk_globals.h"
#include "account.h"

static void s_set_option(GtkWidget *inWidget, int *ioData)
{
	*ioData = !(*ioData);
}

GtkWidget *gtkut_button(const char *inText, int *inValue, GtkWidget *inPage,
	GtkAccelGroup *inAccelGroup)
{
	GtkWidget *button = gtk_check_button_new_with_mnemonic(inText);

	assert(inValue != NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *inValue);
	gtk_box_pack_start(GTK_BOX(inPage), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(s_set_option), inValue);
	gtk_widget_show(button);

	return (button);
}

GtkWidget *gtkut_create_icon_widget(const char **inXPM, GtkWidget *inParent)
{
	GtkWidget *iconwid = NULL;
	GdkPixbuf *buf = NULL;

	buf = gdk_pixbuf_new_from_xpm_data(inXPM);
	iconwid = gtk_image_new_from_pixbuf(buf);
	gtk_widget_show(iconwid);

	return (iconwid);
}

GtkWidget *gtkut_stock_button_new_with_label(const char *label,
	const char *stock)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 5);
	GtkWidget *button = gtk_button_new();
	GtkWidget *iconwid =
		gtk_image_new_from_stock(stock, GTK_ICON_SIZE_BUTTON);
	GtkWidget *labelwid = gtk_label_new(label);

	gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), labelwid, FALSE, FALSE, 2);
	gtk_container_add(GTK_CONTAINER(button), hbox);

	gtk_widget_show(iconwid);
	gtk_widget_show(labelwid);
	gtk_widget_show(hbox);
	gtk_widget_show(button);

	return button;
}

GtkWidget *gtkut_create_icon_button(const char *inLabel, const char **inXPM,
	GtkWidget *inParent)
{
	GtkWidget *button = NULL;
	GtkWidget *label = NULL;
	GtkWidget *hbox = NULL;
	GtkWidget *iconwid = NULL;
	GtkStyle *style = NULL;
	const int min_width = 80;
	int width = min_width;
	int height = -1;
	int using_label = (inLabel != NULL);

	if (using_label) {
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(hbox);
	}

	style = gtk_widget_get_style(inParent);

	iconwid = gtkut_create_icon_widget(inXPM, inParent);
	height = gdk_pixbuf_get_height(gtk_image_get_pixbuf(GTK_IMAGE(iconwid)))
		+ 5;

	if (using_label) {
		gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 0);

		label = gtk_label_new(inLabel);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	}

	button = gtk_button_new();
	if (using_label) {
		gtk_container_add(GTK_CONTAINER(button), hbox);

		if (button->requisition.width > width)
			width = button->requisition.width;
		gtk_widget_set_size_request(button, width, height);
	} else {
		gtk_container_add(GTK_CONTAINER(button), iconwid);
	}

	gtk_widget_show(button);

	return (button);
}

void gtkut_set_pixbuf(eb_local_account *ela, const char **inXPM,
	GdkPixbuf **outPixbuf)
{

	if ((inXPM == NULL) || (*outPixbuf == NULL))
		return;
	/*
	 * FIXME: This is slowing things down for now as it compulsarily
	 * reloads the pixbuf everytime regardless of whether it has changed
	 * or not
	 */
	*outPixbuf = gdk_pixbuf_new_from_xpm_data(inXPM);
}

void gtkut_set_pixbuf_from_xpm(const char **inXPM, GdkPixbuf **outPixbuf)
{
	if ((inXPM == NULL) || (*outPixbuf == NULL))
		return;

#ifndef __MINGW32__
	*outPixbuf = gdk_pixbuf_new_from_xpm_data(inXPM);
#endif
}

void gtkut_widget_get_uposition(GtkWidget *inWidget, int *outXpos, int *outYpos)
{
	int x = 0;
	int y = 0;
	int sx = 0;
	int sy = 0;

	if (outXpos != NULL)
		*outXpos = 0;

	if (outYpos != NULL)
		*outYpos = 0;

	if ((inWidget == NULL) || (inWidget->window == NULL))
		return;

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_root_origin(inWidget->window, &x, &y);

	x %= sx;
	if (x < 0)
		x = 0;

	y %= sy;
	if (y < 0)
		y = 0;

	if (outXpos != NULL)
		*outXpos = x;

	if (outYpos != NULL)
		*outYpos = y;
}

GtkWidget *gtkut_create_label_button(const char *inButtonText,
	GCallback inSignalFunc, void *inCallbackData)
{
	GtkWidget *button = NULL;

	button = gtk_button_new_with_mnemonic(inButtonText);
	gtk_widget_show(button);

	g_signal_connect(button, "clicked", inSignalFunc, inCallbackData);

	return (button);
}

GSList *gtkut_add_radio_button_to_group(GSList *ioGroup, GtkWidget *inParentBox,
	const char *inButtonText, int inIsSelected,
	GCallback inSignalFunc, void *inCallbackData)
{
	GtkWidget *radio = NULL;

	if (inButtonText == NULL)
		radio = gtk_radio_button_new(ioGroup);
	else
		radio = gtk_radio_button_new_with_label(ioGroup, inButtonText);

	gtk_widget_show(radio);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), inIsSelected);

	gtk_box_pack_start(GTK_BOX(inParentBox), radio, FALSE, FALSE, 0);

	g_signal_connect(radio, "clicked", inSignalFunc, inCallbackData);

	return (gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio)));
}

GtkWidget *gtkut_check_button(GtkWidget *inParentBox, const char *inButtonText,
	int inIsSelected, GCallback inSignalFunc, void *inCallbackData)
{
	GtkWidget *button = NULL;

	if (inButtonText == NULL)
		button = gtk_check_button_new();
	else
		button = gtk_check_button_new_with_mnemonic(inButtonText);

	gtk_widget_show(button);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), inIsSelected);

	gtk_box_pack_start(GTK_BOX(inParentBox), button, FALSE, FALSE, 0);

	g_signal_connect(GTK_OBJECT(button), "clicked", inSignalFunc,
		inCallbackData);

	return (button);
}

GtkWidget *gtkut_create_menu_button(GtkMenu *inMenu, const char *inLabel,
	GCallback inSignalFunc, void *inCallbackData)
{
	GtkWidget *button = NULL;

	assert(inMenu != NULL);

	if (inLabel == NULL)
		button = gtk_menu_item_new();
	else
		button = gtk_menu_item_new_with_label(inLabel);

	if (inSignalFunc != NULL)
		g_signal_connect(button, "activate", inSignalFunc,
			inCallbackData);

	gtk_menu_shell_append(GTK_MENU_SHELL(inMenu), button);
	gtk_widget_show(button);

	return (button);
}

GtkWidget *gtkut_attach_submenu(GtkMenu *inMenu, const char *inLabel,
	GtkWidget *inSubmenu, int inActive)
{
	GtkWidget *button = NULL;

	if (inLabel == NULL)
		button = gtk_menu_item_new();
	else
		button = gtk_menu_item_new_with_label(inLabel);

	gtk_widget_set_sensitive(button, inActive);

	gtk_menu_shell_append(GTK_MENU_SHELL(inMenu), button);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(button), inSubmenu);
	gtk_widget_show(button);

	return (button);
}

void gtkut_set_window_icon(GtkWindow *inWindow, const gchar **inXPM)
{
	GdkPixbuf *the_pixbuf = NULL;

	if (inXPM != NULL) {
		the_pixbuf = gdk_pixbuf_new_from_xpm_data(inXPM);
		gtk_window_set_icon(inWindow, the_pixbuf);
	}
}
