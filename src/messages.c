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

typedef enum _ay_message_type { AY_MESSAGE_INFO, AY_MESSAGE_WARNING,
		AY_MESSAGE_ERROR } ay_message_type;

static void ay_do_message(const char *inTitle, const char *inMessage,
	ay_message_type type)
{
	GtkWidget *dialog;
	GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	GtkMessageType mtype = GTK_MESSAGE_INFO;

	switch (type) {
	case AY_MESSAGE_ERROR:
		flags |= GTK_DIALOG_MODAL;
		mtype = GTK_MESSAGE_ERROR;
		break;
	case AY_MESSAGE_WARNING:
		mtype = GTK_MESSAGE_WARNING;
		break;
	default:
		break;
	}

	dialog = gtk_message_dialog_new_with_markup(NULL, flags, mtype,
		GTK_BUTTONS_OK, inMessage);
	gtk_window_set_title(GTK_WINDOW(dialog), inTitle);
	gtk_widget_show(dialog);

	g_signal_connect_swapped(dialog, "response",
		G_CALLBACK(gtk_widget_destroy), dialog);
}

void ay_do_info(const char *inTitle, const char *inMessage)
{
	ay_do_message(inTitle, inMessage, AY_MESSAGE_INFO);
}

void ay_do_warning(const char *inTitle, const char *inMessage)
{
	ay_do_message(inTitle, inMessage, AY_MESSAGE_WARNING);
}

void ay_do_error(const char *inTitle, const char *inMessage)
{
	ay_do_message(inTitle, inMessage, AY_MESSAGE_ERROR);
}
