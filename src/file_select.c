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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gtk/gtk.h>

#include "file_select.h"

struct eb_file_selector_data {
	void (*callback)(char *filename, gpointer data);
	gpointer data;
};


static void file_selector_cancel_callback(GtkButton *button, gpointer data)
{
	struct eb_file_selector_data *efsd = data;

	efsd->callback(NULL, efsd->data);
}

static void file_selector_ok_callback(GtkButton *button, gpointer data)
{
	struct eb_file_selector_data *efsd = (struct eb_file_selector_data *)data;
	char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(gtk_widget_get_toplevel(GTK_WIDGET(button))));

	efsd->callback(filename, efsd->data);
}

static void do_file_selector(char *default_filename, char *title,
		void (*action_ok)(GtkButton *button, gpointer data), 
		void (*action_cancel)(GtkButton *button, gpointer data), 
		gpointer data)
{
	GtkWidget *file_selector;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;

	file_selector = gtk_file_selection_new(title);
	gtk_object_set_data (GTK_OBJECT (file_selector), "file_selector", file_selector);
	gtk_container_set_border_width (GTK_CONTAINER (file_selector), 10);
	/*gtk_window_set_modal (GTK_WINDOW (file_selector), TRUE);*/

	ok_button = GTK_FILE_SELECTION (file_selector)->ok_button;
	gtk_object_set_data (GTK_OBJECT (file_selector), "ok_button", ok_button);
	gtk_widget_show (ok_button);
	GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);

	cancel_button = GTK_FILE_SELECTION (file_selector)->cancel_button;
	gtk_object_set_data (GTK_OBJECT (file_selector), "cancel_button", cancel_button);
	gtk_widget_show (cancel_button);
	GTK_WIDGET_SET_FLAGS (cancel_button, GTK_CAN_DEFAULT);

	gtk_signal_connect(GTK_OBJECT (ok_button), "clicked",
	                    GTK_SIGNAL_FUNC (action_ok), data);

	gtk_signal_connect_object(GTK_OBJECT (ok_button), "clicked",
	                           GTK_SIGNAL_FUNC (gtk_widget_destroy),
	                           GTK_OBJECT (file_selector));

	gtk_signal_connect(GTK_OBJECT (cancel_button), "clicked",
	                    GTK_SIGNAL_FUNC (action_cancel), data);

	gtk_signal_connect_object(GTK_OBJECT (cancel_button), "clicked",
	                           GTK_SIGNAL_FUNC (gtk_widget_destroy),
	                           GTK_OBJECT (file_selector));

	if(default_filename)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), default_filename);

	gtk_widget_show (file_selector);
}

void eb_do_file_selector(char *default_filename, char *window_title, 
		void (*callback)(char *filename, gpointer data), gpointer data)
{
	struct eb_file_selector_data *efsd = g_new0(struct eb_file_selector_data, 1);
	efsd->callback = callback;
	efsd->data = data;

	do_file_selector(default_filename, window_title, 
			file_selector_ok_callback, file_selector_cancel_callback, efsd);
}


