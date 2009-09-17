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

#include "intl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "edit_list_window.h"
#include "away_window.h"
#include "gtk_globals.h"
#include "service.h"
#include "messages.h"
#include "dialog.h"

#ifndef MIN
#define MIN(x, y)	((x)<(y)?(x):(y))
#endif

/* globals */
gint is_away = 0;

static GtkWidget *awaybox;
static GtkWidget *away_message_text_entry;

static void show_away(gchar *a_message, void *unused);

void away_window_set_back(void)
{
	if (awaybox)
		gtk_widget_destroy(awaybox);
	awaybox = NULL;
	away_message_text_entry = NULL;
}

void show_away_choicewindow(void *w, void *data)
{
	char file[1024];

	if (is_away) {
		gdk_window_raise(awaybox->window);
		return;
	}
	snprintf(file, 1024, "%saway_messages", config_dir);
	show_data_choicewindow(file, _("Away message"), _("Set away"), NULL,
		"AWAY_MESSAGE", "MESSAGE", show_away, NULL);
}

static void destroy_away()
{
	LList *list;
	eb_local_account *ela = NULL;

	is_away = 0;
	for (list = accounts; list; list = list->next) {
		ela = list->data;
		/* Only change state for those accounts which are connected */
		if (ela->connected)
			eb_services[ela->service_id].sc->set_away(ela, NULL, 0);
	}
}

static void show_away(gchar *a_message, void *unused)
{
	LList *list;
	eb_local_account *ela = NULL;

	if (!is_away) {
		GtkWidget *label;
		GtkWidget *vbox;
		GtkTextBuffer *buffer;

		awaybox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_realize(awaybox);

		vbox = gtk_vbox_new(FALSE, 0);

		label = gtk_label_new(_
			("You are currently away, click \"I'm back\" to return"));

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
		gtk_widget_show(label);

		label = gtk_label_new(_
			("This is the away message that will \nbe sent to people messaging you.\nYou may edit this message if you wish."));

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
		gtk_widget_show(label);

		away_message_text_entry = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW
			(away_message_text_entry), TRUE);
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW
			(away_message_text_entry));

		gtk_text_buffer_insert_at_cursor(buffer, a_message,
			strlen(a_message));

		gtk_box_pack_start(GTK_BOX(vbox), away_message_text_entry, TRUE,
			TRUE, 5);
		gtk_widget_set_size_request(away_message_text_entry, 300, 60);
		gtk_widget_show(away_message_text_entry);

		label = gtk_button_new_with_label(_("I'm Back"));
		g_signal_connect_swapped(label, "clicked",
			G_CALLBACK(away_window_set_back), awaybox);

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);
		gtk_widget_show(label);

		g_signal_connect_swapped(awaybox, "destroy",
			G_CALLBACK(destroy_away), awaybox);

		gtk_container_add(GTK_CONTAINER(awaybox), vbox);
		GTK_WIDGET_SET_FLAGS(label, GTK_CAN_DEFAULT);
		gtk_widget_grab_default(label);
		gtk_widget_show(vbox);
	}

	gtk_window_set_title(GTK_WINDOW(awaybox), _("Away"));
	gtk_container_set_border_width(GTK_CONTAINER(awaybox), 2);
	gtk_widget_show(awaybox);
	is_away = 1;

	for (list = accounts; list; list = list->next) {
		ela = list->data;
		/* Only change state for those accounts which are connected */
		if (ela->connected)
			eb_services[ela->service_id].sc->set_away(ela,
				a_message, 1);
	}
}

char *get_away_message()
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW
		(away_message_text_entry));
	gtk_text_buffer_get_bounds(buffer, &start, &end);

	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}
