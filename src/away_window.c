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

#include "away_window.h"
#include "dialog.h"
#include "gtk_globals.h"
#include "service.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"

#ifndef MIN
#define MIN(x, y)	((x)<(y)?(x):(y))
#endif

/* types */
typedef struct _away {
        char title[2048];
        GString *message;
} away; 

/* globals */
static GtkWidget *away_message_text_entry = NULL;
gint is_away = 0;


static GtkWidget *awaybox = NULL;
static GtkWidget *away_window = NULL;

static GtkWidget *title = NULL;
static GtkWidget *save_later = NULL;
static LList *away_messages = NULL;

static gint away_open = 0;

static void show_away(GtkWidget *w, gchar *a_message);
static void show_away2(GtkWidget *w, void *null);

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

static void imback()
{
	if (awaybox)
		gtk_widget_destroy(awaybox);
	awaybox = NULL;
}

static void destroy( GtkWidget *widget, gpointer data)
{
	away_open = 0;
}

void load_away_messages()
{
	FILE * fp;
	char buff[2048];
	away * my_away = NULL;
	gboolean reading_title=FALSE;
	gboolean reading_message=FALSE;

	g_snprintf (buff, 1024, "%saway_messages", config_dir);

	if(!(fp = fopen(buff, "r")))
		return;

	away_messages = NULL;

	while ( fgets(buff, sizeof(buff), fp))
	{
		g_strchomp(buff);
		if (!g_strncasecmp(buff, "<AWAY_MESSAGE>", strlen("<AWAY_MESSAGE>"))) {
			my_away = g_new0(away, 1);
		} else if (!g_strncasecmp( buff, "</AWAY_MESSAGE>", strlen("</AWAY_MESSAGE>"))) {
			away_messages = l_list_append(away_messages, my_away);
		} else if (!g_strncasecmp(buff, "<TITLE>", strlen("<TITLE>"))) {
			reading_title=TRUE;
		} else if (!g_strncasecmp(buff, "</TITLE>", strlen("</TITLE>"))) {
			reading_title=FALSE;
		} else if (!g_strncasecmp(buff, "<MESSAGE>", strlen("<MESSAGE>"))) {
			reading_message=TRUE;
			my_away->message = g_string_new(NULL);
		} else if (!g_strncasecmp(buff, "</MESSAGE>", strlen("</MESSAGE>")))  {
			reading_message=FALSE;
		} else if(reading_title) {
			strncpy(my_away->title, buff, MIN(strlen(buff), sizeof(my_away->title)));
		} else if(reading_message) {
			if(my_away->message && my_away->message->len)
				g_string_append_c(my_away->message, '\n');
			g_string_append(my_away->message, buff);
		}
	}

	fclose(fp);
}


void build_away_menu()
{
	GList * l;
	GtkWidget * label;
	LList * away_list = away_messages;
	away * my_away = NULL;

	l = gtk_container_children(GTK_CONTAINER(away_menu));
	
	while (l) {
		gtk_widget_destroy(GTK_WIDGET(l->data));
		l = l->next;
	}

	label = gtk_tearoff_menu_item_new();
	gtk_menu_append(GTK_MENU(away_menu), label);
	gtk_widget_show(label);

	label = gtk_menu_item_new_with_label(_("New Away Message..."));
	gtk_menu_append(GTK_MENU(away_menu), label);
	gtk_signal_connect(GTK_OBJECT(label), "activate",
				show_away2, NULL );
	gtk_widget_show(label);

	if (away_list) {
		label = gtk_menu_item_new();
                gtk_menu_append(GTK_MENU(away_menu), label );
                gtk_widget_show(label);
	}
		
	while (away_list) {
		my_away = (away *)away_list->data;
		
		label = gtk_menu_item_new_with_label(my_away->title);
		gtk_menu_append(GTK_MENU(away_menu), label);
		gtk_signal_connect(GTK_OBJECT(label), "activate",
				show_away, my_away->message->str);
			
		gtk_widget_show(label);

		away_list = away_list->next;
	}
	gtk_widget_show(away_menu);
}

void write_away_messages()
{
	LList * away_list = away_messages;
	FILE *fp;
	char buff2[2048];
	away * my_away;

	g_snprintf(buff2, 1024, "%saway_messages",config_dir);

	if(!(fp = fdopen(creat(buff2,0700),"w")))
		return;

	while (away_list) {
		my_away = (away *)away_list->data;

		fprintf(fp,"<AWAY_MESSAGE>\n");
		fprintf(fp,"<TITLE>\n");
		strncpy(buff2, my_away->title, strlen(my_away->title) + 1);
		g_strchomp(buff2);
		fprintf(fp,"%s\n",buff2);
		fprintf(fp,"</TITLE>\n");
		fprintf(fp,"<MESSAGE>\n");
		strncpy(buff2, my_away->message->str, strlen(my_away->message->str) + 1);
		g_strchomp(buff2);
		fprintf(fp,"%s\n",buff2);
		fprintf(fp,"</MESSAGE>\n");
		fprintf(fp,"</AWAY_MESSAGE>\n");

		away_list = away_list->next;
	}	
	fclose(fp);
}

static void ok_callback( GtkWidget * widget, gpointer data)
{
	GString * a_title = g_string_sized_new(1024);
	GString * a_message = g_string_sized_new(1024);
	gint save = GTK_TOGGLE_BUTTON(save_later)->active;
	away * my_away;

	gchar *buff = NULL;

	buff = strdup(gtk_entry_get_text(GTK_ENTRY(title)));

	if (!buff || strlen(buff) == 0) {
		buff= gtk_editable_get_chars(GTK_EDITABLE(away_message_text_entry),0,-1);
		if (strchr(buff,'\n') != NULL) {
			char *t = strchr(buff,'\n');
			*t = 0;
		}
	}
	if (!buff || strlen(buff) == 0) {
		do_error_dialog (_("You have to enter a message."),_("Error"));
		return;
	}
	g_string_append(a_title, buff); 

	free(buff);
	
	buff = gtk_editable_get_chars(GTK_EDITABLE(away_message_text_entry),0,-1);
	
	if (!buff || strlen(buff) == 0) /* in this case title can't be empty */
		buff = strdup(a_title->str);
	
	g_string_append(a_message, buff);
	
	free (buff);
	
	if (save == 1) {
		my_away = g_new0(away, 1);
		strncpy(my_away->title, a_title->str, strlen(a_title->str));
		my_away->message = g_string_new(NULL);
		g_string_append(my_away->message, a_message->str);
		away_messages = l_list_append(away_messages, my_away);
		write_away_messages();
		build_away_menu();
	}
	
	show_away(NULL, a_message->str);

	g_string_free(a_title, TRUE);
	g_string_free(a_message, TRUE);

	gtk_widget_destroy(away_window);
}

static void cancel_callback( GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(away_window);
}

static void show_away2(GtkWidget *w, void *null)
{
	if ( !away_open ) {
		GtkWidget * vbox;
		GtkWidget * hbox;
		GtkWidget * hbox2;
		GtkWidget * label;
		GtkWidget * frame;
		GtkWidget * table;
		GtkWidget * separator;
		GtkWidget * button;

		away_window = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_position(GTK_WINDOW(away_window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(away_window);

		vbox = gtk_vbox_new(FALSE, 5);
	
		/* make the window not resizable */

		table = gtk_table_new(2, 3, FALSE);

		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("Title: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);

		title = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), title, 1, 2, 0, 1,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(title);
		
		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("Away Message: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		
		away_message_text_entry = gtk_text_new(NULL,NULL);
		gtk_text_set_editable(GTK_TEXT(away_message_text_entry), TRUE);
		gtk_widget_set_usize(away_message_text_entry, 300, 60);
		gtk_table_attach_defaults(GTK_TABLE(table), away_message_text_entry, 1, 2, 1, 2);
		gtk_widget_show(away_message_text_entry);

		save_later = gtk_check_button_new_with_label(_("Save for later use"));
		gtk_table_attach(GTK_TABLE(table), save_later, 1, 2, 2, 3,
				 GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(save_later);

		frame = gtk_frame_new(_("New Away Message"));
		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		gtk_table_set_row_spacings (GTK_TABLE(table), 5);
		gtk_table_set_col_spacings (GTK_TABLE(table), 5);

		gtk_container_set_border_width(GTK_CONTAINER(away_window), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);

		gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);
		gtk_widget_show(frame);

		/* seperator goes here */
	
		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);

		/* 'Set away' and 'Cancel' buttons go here */

		hbox2 = gtk_hbox_new(TRUE, 5);

		gtk_widget_set_usize(hbox2, 200, 25);

		/* put in the pixmap and label for the 'Set away' button */

		button = do_icon_button(_("Set away"), ok_xpm, away_window);

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(ok_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/* now start on the cancel button */

		button = do_icon_button(_("Cancel"), cancel_xpm, away_window);

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(cancel_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		hbox = gtk_hbox_new(FALSE, 0);
		
		gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
		gtk_widget_show(vbox);
		
		gtk_container_add(GTK_CONTAINER(away_window), vbox);
		
		gtk_window_set_policy(GTK_WINDOW(away_window), FALSE, FALSE, TRUE);		
		
		gtk_widget_show(away_window);
		gtk_widget_grab_focus(title);

	}

	gtk_window_set_title(GTK_WINDOW(away_window), _("New Away Message"));
	eb_icon(away_window->window);
	gtk_signal_connect(GTK_OBJECT(away_window), "destroy",
			GTK_SIGNAL_FUNC(destroy), NULL);
	away_open = 1; 
}

static void show_away(GtkWidget *w, gchar *a_message)
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
	eb_icon(awaybox->window);
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

