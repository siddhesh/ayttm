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

/*
 * chat_window.h
 * header file for the conversation window
 *
 */

#ifndef __CHAT_WINDOW_H__
#define __CHAT_WINDOW_H__

#include <time.h>

#include "contact.h"
#include "log_window.h"

typedef struct _chat_window
{
	GtkWidget *window;
	GtkWidget *chat;
	GtkWidget *entry;
	GtkWidget *smiley_button;
	GtkWidget *smiley_window;
	GtkWidget *sound_button;
	GtkWidget *offline_button;
	GtkWidget *status_label;

	int sound_enabled;
	int send_enabled;
	int first_enabled;
	int receive_enabled;
	
	eb_local_account * local_user;

	time_t next_typing_send;
	LList * history;
	LList * hist_pos;
	int this_msg_in_history;
	log_info *loginfo;

	/* begin chat window specific stuff */

	struct contact * contact;
	eb_account * preferred; /*for sanity reasons, try using the
			   	  most recently used account first */

	/* Set to FALSE on init, TRUE when away msg first sent,
	FALSE when user sends regular message */
	gint away_msg_sent;

	log_window* lw;

	GtkWidget* notebook; /* when using tabbed chat, this is the same for all chat_window structs. */
	GtkWidget* notebook_child; /* this part is different for each person we're talking to */
} chat_window;

/* Struct to hold info used by get_local_accounts to hold callback info */
typedef struct _chat_window_account {
	chat_window *cw;
	gpointer data;
} chat_window_account;

chat_window * eb_chat_window_new( eb_local_account * local, struct contact * remote );

void eb_chat_window_display_remote_message( eb_local_account * account, eb_account * remote, 
						gchar * message);

void eb_chat_window_display_status( eb_account * remote, gchar * message );

void eb_chat_window_display_contact( struct contact * remote_contact );
void eb_chat_window_display_account( eb_account * remote_account );
void eb_chat_window_display_error( eb_account * remote, gchar * message );
void eb_log_status_changed(eb_account *ea, gchar *status );
void eb_chat_window_do_timestamp( struct contact * c, gboolean online );
void eb_restore_last_conv(gchar *file_name, chat_window* cw);
void send_message(GtkWidget *widget, gpointer d);

#endif
