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
 * conversation.h
 * header file for the conversation window
 *
 */

#ifndef __CONVERSATION_H__
#define __CONVERSATION_H__

#include <time.h>

#include "contact.h"

typedef struct _eb_conversation_buddy {
	char alias[255];
	char handle[255];
	int color;
} ConversationFellow;

typedef struct _conversation {
	eb_local_account *local_user;

	time_t next_typing_send;
	LList *history;
	LList *hist_pos;
	int this_msg_in_history;
	log_file *logfile;
	char *name;

	struct contact *contact;
	eb_account *preferred;	/*for sanity reasons, try using the
				   most recently used account first */
	int is_room;
	int is_public;

	/* Set to FALSE on init, TRUE when away msg first sent,
	   FALSE when user sends regular message */
	time_t away_msg_sent;
	time_t away_warn_displayed;
	LList *fellows;
	int num_fellows;

	t_log_window_id lw;
	char *encoding;
	void *protocol_local_conversation_data;

	struct _chat_window *window;		/* Either a chat room or window */
} Conversation;

/* Add more here or you can make your own */
typedef enum {
	CHAT_NOTIFICATION_NOTE = 0x0000ff,	/* Blue */
	CHAT_NOTIFICATION_ERROR = 0xff0000,	/* Red */
	CHAT_NOTIFICATION_HIGHLIGHT = 0x00ff00,	/* Green */
	CHAT_NOTIFICATION_WORKING = 0xcccccc,	/* Gray */
	CHAT_NOTIFICATION_JOIN = 0x777777,
	CHAT_NOTIFICATION_LEAVE = 0x777777,
	CHAT_NOTIFICATION_NICK_CHANGE = 0x777777
} ChatNotificationType;

Conversation *ay_conversation_new(eb_local_account *local, struct contact *remote,
				  const char *name, int is_room, int is_public);

Conversation *ay_conversation_clone_as_room(Conversation *conv);

gchar *ay_chat_convert_incoming(Conversation *conv, const char *msg);
gchar *ay_chat_convert_outgoing(Conversation *conv, const char *msg);

void ay_conversation_send_message(Conversation *conv, char *text);
void ay_conversation_display_status(eb_account *remote, gchar *message);

void ay_conversation_got_message(Conversation *conv, const gchar *from, 
				 const gchar *o_message);

void ay_conversation_display_notification(Conversation *conv, const gchar *message,
					  ChatNotificationType type);

void ay_conversation_set_encoding(const char *value, void *data);

void ay_conversation_send_typing_status(Conversation *conv);

void ay_conversation_end(Conversation *conv);

void ay_conversation_log_status_changed(eb_account *ea, const gchar *status);

void ay_conversation_chat_with_contact(struct contact *remote);
void ay_conversation_chat_with_account(eb_account *remote_account);

Conversation *ay_conversation_find_by_name(eb_local_account *ela, const char *name);

void ay_conversation_fellows_append(Conversation *conv, const char *alias,
	const char *handle);
void ay_conversation_buddy_arrive(Conversation *conv, const char *alias,
	const char *handle);
void ay_conversation_buddy_chnick(Conversation *conv, const char *handle,
	const char *newalias);

void ay_conversation_buddy_leave(Conversation *conv, const char *handle);
void ay_conversation_buddy_leave_ex(Conversation *conv, const char *handle,
	const char *message);

int ay_conversation_buddy_connected(Conversation *conv, const char *alias);

void ay_conversation_rename(Conversation *conv, char *new_name);

void ay_conversation_invite_fellow(Conversation *conv, const char *fellow,
	const char *message);

#endif
