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

#include <time.h>

#include <gtk/gtk.h>

#include "account.h"
#include "logs.h"
#include "chat_window.h"

typedef struct _eb_chat_room_buddy
{
	char alias[255];
	char handle[255];
	int color;
} eb_chat_room_buddy;

#define eb_chat_room chat_window

#ifdef __cplusplus
extern "C" {
#endif

void eb_join_chat_room( eb_chat_room * chat_room );
void eb_chat_room_show_3rdperson( eb_chat_room * chat_room, char * message);
void eb_chat_room_show_message( eb_chat_room * chat_room, char * user, char * message );
eb_chat_room* eb_start_chat_room( eb_local_account *ela, char * name , int is_public);
void eb_chat_room_buddy_arrive( eb_chat_room * room, char * alias, char * handle );
void eb_chat_room_buddy_leave( eb_chat_room * room, char * handle );
void eb_chat_room_refresh_list(eb_chat_room * room );
int eb_chat_room_buddy_connected( eb_chat_room * room, char * user );
void open_join_chat_window();
char* next_chatroom_name();
void eb_chat_room_display_status (eb_account *remote, char *message);
void eb_destroy_chat_room (eb_chat_room *ecr);
eb_chat_room* find_tabbed_chat_room(void);
eb_chat_room *find_tabbed_chat_room_index (int current_page);
void do_invite_window(void *widget, eb_chat_room * room );
void eb_chat_room_notebook_switch(GtkNotebook *notebook, GtkNotebookPage *page, gint page_num);
eb_chat_room * find_chat_room_by_id( char * id );
eb_chat_room * find_chat_room_by_name( char * name, int service_id );
LList * find_chatrooms_with_remote_account(eb_account *remote);
void invite_dialog( eb_local_account * ela, char * user, char * chat_room,
		    void * id );
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

