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

#ifndef __CHAT_ROOM_H__
#define __CHAT_ROOM_H__

#include <gtk/gtk.h>
#include <stdio.h>
#include <time.h>

#include "account.h"
#include "log_window.h"

typedef struct _eb_chat_room_buddy
{
	char alias[255];
	char handle[255];
	int color;
} eb_chat_room_buddy;

typedef struct _eb_chat_room
{
	GtkWidget *window;
	GtkWidget *chat;
	GtkWidget *entry;
	GtkWidget *smiley_button;
	GtkWidget *smiley_window;
	GtkWidget *sound_button;
	GtkWidget *status_label;
	
	/* sound stuff */
	int sound_enabled;
	int send_enabled;
	int first_enabled;
	int receive_enabled;

	eb_local_account * local_user; /* who are we talking as? */
	
	time_t next_typing_send; 
	LList * history;
	LList * hist_pos;
	int this_msg_in_history;
	log_info *loginfo;

	/* begin chat room specific stuff */
	
	int connected; /* are we currently in this chat room */
	char id[255];      /* who are we? */
	/*int service_id;*/

	char room_name[1024];  /* what is this chat room called */
	LList * fellows;   /* who is in the chat room */
	GtkWidget *fellows_widget;  /* CList of online folks */
	LList * typing_fellows;
	int total_arrivals;
	
	void *protocol_local_chat_room_data; /* For protocol-specific storage */

	/*
	 * the folloing data members is for the invite window gui
	 * since each chat room may spawn an invite window
	 */

	int invite_window_is_open;
	GtkWidget * invite_window;
	GtkWidget * invite_buddy;
	GtkWidget * invite_message;

} eb_chat_room;

#ifdef __cplusplus
extern "C" {
#endif

void eb_join_chat_room( eb_chat_room * chat_room );
void eb_chat_room_show_3rdperson( eb_chat_room * chat_room, char * message);
void eb_chat_room_show_message( eb_chat_room * chat_room, char * user, char * message );
void eb_start_chat_room( int service, char * name );
void eb_chat_room_buddy_arrive( eb_chat_room * room, char * alias, char * handle );
void eb_chat_room_buddy_leave( eb_chat_room * room, char * handle );
int eb_chat_room_buddy_connected( eb_chat_room * room, char * user );
void open_join_chat_window();
char* next_chatroom_name();
void eb_chat_room_display_status (eb_account *remote, char *message);
void eb_destroy_chat_room (eb_chat_room *ecr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

