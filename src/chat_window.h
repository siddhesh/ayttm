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

#include <gtk/gtk.h>

#include "contact.h"
#include "conversation.h"

extern LList *chat_window_list;

typedef struct _chat_window {
	GtkWidget *pane;
	GtkWidget *window;
	GtkWidget *chat;
	GtkWidget *entry;
	GtkWidget *smiley_button;
	GtkWidget *smiley_window;
	GtkWidget *sound_button;
	GtkWidget *offline_button;
	GtkWidget *reconnect_button;
	GtkWidget *fellows_widget;

	GtkListStore *fellows_model;

	GtkWidget *notebook;
	GtkWidget *notebook_child;
	int notebook_page;
	int is_child_red;

	int sound_enabled;
	int send_enabled;
	int first_enabled;
	int receive_enabled;
	int firstmsg;

	char *name;

	Conversation *conv;
	LList *typing_fellows;
} chat_window;

/* Struct to hold info used by get_local_accounts to hold callback info */
typedef struct _chat_window_account {
	chat_window *cw;
	gpointer data;
} chat_window_account;

chat_window *ay_chat_window_new(Conversation *conv);
void ay_chat_window_free(chat_window *cw);

void eb_chat_window_display_remote_message(eb_local_account *account,
	eb_account *remote, gchar *message);

void eb_chat_window_display_status(eb_account *remote, gchar *message);
void eb_chat_window_display_contact(struct contact *remote_contact);
void eb_chat_window_display_account(eb_account *remote_account);
void eb_chat_window_display_notification(chat_window *cw, const gchar *message,
	ChatNotificationType type);
void eb_log_status_changed(eb_account *ea, const gchar *status);
void eb_chat_window_do_timestamp(struct contact *c, gboolean online);
void send_message(GtkWidget *widget, gpointer d);
chat_window *find_tabbed_chat_window_index(int current_page);
void set_tab_red(chat_window *cw);
void reassign_tab_pages();
void chat_window_to_chat_room(chat_window *cw, eb_account *third_party,
	const char *msg);
gboolean check_tab_accelerators(const GtkWidget *inWidget,
	const chat_window *inCW, GdkModifierType inModifiers,
	const GdkEventKey *inEvent);
void chat_auto_complete_validate(GtkWidget *entry);

gchar *ay_chat_convert_message(chat_window *cw, char *msg);

void ay_set_chat_encoding(GtkWidget *widget, void *data);

void ay_chat_window_raise(chat_window *window, const char *message);

void ay_chat_window_printr(chat_window *cw, gchar *o_message);
void ay_chat_window_print(chat_window *cw, gchar *o_message);
void ay_chat_window_print_message(chat_window *cw, gchar *o_message, int send);
#define ay_chat_window_print_recv(cw, o_message) ay_chat_window_print_message(cw, o_message, 0)
#define ay_chat_window_print_send(cw, o_message) ay_chat_window_print_message(cw, o_message, 1)

void ay_chat_window_fellows_append(chat_window *cw, ConversationFellow *fellow);
void ay_chat_window_fellows_remove(chat_window *cw, ConversationFellow *fellow);
void ay_chat_window_fellows_rename(chat_window *cw, ConversationFellow *fellow);
void ay_chat_window_set_name(chat_window *cw);

#endif
