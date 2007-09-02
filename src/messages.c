/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
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
 
#include "messages.h"
#include <gtk/gtk.h>


void	ay_do_info( const char *inTitle, const char *inMessage )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new_with_markup(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK, inMessage);
	gtk_window_set_title(GTK_WINDOW(dialog), inTitle);
	gtk_widget_show(dialog);

	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
}

void	ay_do_warning( const char *inTitle, const char *inMessage )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, inMessage);
	gtk_window_set_title(GTK_WINDOW(dialog), inTitle);
	gtk_widget_show(dialog);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
}

void	ay_do_error( const char *inTitle, const char *inMessage )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, inMessage);
	gtk_window_set_title(GTK_WINDOW(dialog), inTitle);
	gtk_widget_show(dialog);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
}
