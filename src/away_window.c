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

#include "gtk/gtkutils.h"

#ifndef MIN
#define MIN(x, y)	((x)<(y)?(x):(y))
#endif

/* globals */
gint is_away = 0;

static GtkWidget *awaybox;
static GtkWidget *away_message_text_entry;

static void show_away(gchar *a_message, void *unused);

static void imback()
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
		gdk_window_raise (awaybox->window);
		return;
	}
	snprintf(file, 1024, "%saway_messages", config_dir);
	show_data_choicewindow(file, _("Away message"), _("Set away"),
				"AWAY_MESSAGE", "MESSAGE", show_away, NULL);
}

static void destroy_away()
{
	LList * list;
	eb_local_account * ela = NULL;

	is_away = 0;
	for (list = accounts; list; list = list->next) {
    		ela = list->data;
		/* Only change state for those accounts which are connected */
		if(ela->connected)
			eb_services[ela->service_id].sc->set_away(ela, NULL);
	}
}


static void show_away(gchar *a_message, void *unused)
{
	LList * list;
	eb_local_account * ela = NULL;
	
	if (!is_away) {
		GtkWidget *label;
		GtkWidget *vbox;

		awaybox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_realize(awaybox);

		vbox = gtk_vbox_new(FALSE, 0);

		label = gtk_label_new(_("You are currently away, click \"I'm back\" to return"));

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
		gtk_widget_show(label);

		label = gtk_label_new(_("This is the away message that will \nbe sent to people messaging you.\nYou may edit this message if you wish."));

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
		gtk_widget_show(label);


		away_message_text_entry = gtk_text_new(NULL,NULL);
		gtk_text_set_editable(GTK_TEXT(away_message_text_entry), TRUE);
		gtk_widget_set_usize(away_message_text_entry, 300, 60);
		gtk_text_insert(GTK_TEXT(away_message_text_entry), NULL,NULL,NULL,
				a_message, strlen(a_message));
		
		gtk_box_pack_start(GTK_BOX(vbox), away_message_text_entry, TRUE, TRUE, 5);
		gtk_widget_show(away_message_text_entry);

		label = gtk_button_new_with_label(_("I'm Back"));
		gtk_signal_connect_object(GTK_OBJECT(label), "clicked",
				  GTK_SIGNAL_FUNC(imback), GTK_OBJECT(awaybox));

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);
		GTK_WIDGET_SET_FLAGS(label, GTK_CAN_DEFAULT);
		gtk_widget_grab_default(label);
		gtk_widget_show(label);

		gtk_signal_connect_object(GTK_OBJECT(awaybox), "destroy",
				  GTK_SIGNAL_FUNC(destroy_away), GTK_OBJECT(awaybox));

		gtk_container_add(GTK_CONTAINER(awaybox), vbox);
		gtk_widget_show(vbox);
	}

	gtk_window_set_title(GTK_WINDOW(awaybox), _("Away"));
	gtkut_set_window_icon(awaybox->window, NULL);
	gtk_container_border_width(GTK_CONTAINER(awaybox), 2);
	gtk_widget_show(awaybox);
	is_away = 1;
    
	for (list = accounts; list; list = list->next) {
    		ela = list->data;
		/* Only change state for those accounts which are connected */
		if(ela->connected)
			eb_services[ela->service_id].sc->set_away(ela, a_message);
	}
}

gchar * get_away_message()
{
	return gtk_editable_get_chars(GTK_EDITABLE(away_message_text_entry),0,-1);
}

