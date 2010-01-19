/*
 * Ayttm
 *
 * Copyright (C) 2003-2010 the Ayttm team
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
 * chat_window.c
 * implementation for the conversation window
 * This is the window where you will be doing most of your talking :)
 */

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <regex.h>
#include "messages.h"

#include "auto_complete.h"

#ifdef HAVE_ICONV_H
#include <iconv.h>
#include <errno.h>
#endif

#include "util.h"
#include "add_contact_window.h"
#include "sound.h"
#include "dialog.h"
#include "prefs.h"
#include "gtk_globals.h"
#include "status.h"
#include "away_window.h"
#include "message_parse.h"
#include "plugin.h"
#include "contact_actions.h"
#include "smileys.h"
#include "service.h"
#include "action.h"
#include "mem_util.h"
#include "chat_window.h"

#include "gtk/html_text_buffer.h"
#include "gtk/gtkutils.h"

#ifdef HAVE_LIBASPELL
#include "gtk/gtkspell.h"
#endif

#include "pixmaps/tb_volume.xpm"
#include "pixmaps/invite_btn.xpm"

#define BUF_SIZE 1024		/* Maximum message length */
#ifndef NAME_MAX
#define NAME_MAX 4096
#endif

/* flash_window_title.c */
void flash_title(GdkWindow *window);

/* forward declaration */
static gboolean handle_focus(GtkWidget *widget, GdkEventFocus *event,
	gpointer userdata);

LList *chat_window_list = NULL;

#ifdef __MINGW32__
static void redraw_chat_window(GtkWidget *text)
{
	gtk_widget_queue_draw_area(text, 0, 0, text->allocation.width,
		text->allocation.height);
}
#endif

static GtkWidget *ay_chat_get_tab_label(GtkNotebook *notebook, GtkWidget *child)
{
	GtkWidget *tab_label;
	GList *children;

	tab_label = gtk_notebook_get_tab_label(notebook, child);
	children = gtk_container_get_children(GTK_CONTAINER(tab_label));

	while(children) {
		if (GTK_IS_LABEL(children->data))
			return children->data;

		children = g_list_next(children);
	}

	return NULL;
}

void set_tab_red(chat_window *cw)
{
	GdkColor color;
	GtkWidget *notebook = NULL, *child = NULL, *label = NULL;

	if (!cw)
		return;

	eb_debug(DBG_CORE, "Setting tab to red\n");

	notebook = cw->notebook;
	child = cw->notebook_child;

	if (!notebook || !child)
		return;

	label = ay_chat_get_tab_label(GTK_NOTEBOOK(notebook), child);

	gdk_color_parse("red", &color);
	gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, &color);
	cw->is_child_red = TRUE;

	if (!gtk_widget_is_focus(cw->window))
		gtk_window_set_urgency_hint(GTK_WINDOW(cw->window), TRUE);
}

static void remove_smiley_window(chat_window *cw)
{
	eb_debug(DBG_CORE, "Removing Smiley Window\n");
	if (cw->smiley_window) {
		/* close smileys popup */
		gtk_widget_destroy(cw->smiley_window);
		cw->smiley_window = NULL;
	}
}

void remove_from_chat_window_list(chat_window *cw)
{
	if (chat_window_list)
		chat_window_list = l_list_remove(chat_window_list, cw);
	if (!l_list_length(chat_window_list) || !chat_window_list->data) {
		chat_window_list = NULL;
		eb_debug(DBG_CORE, "no more windows\n");
	}
}

/* 
 * The guy closed the chat window, so we need to clean up
 */

static void destroy_event(GtkWidget *widget, gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;

	eb_debug(DBG_CORE, "Destroyed VBox %p\n", widget);

	/* gotta clean up all of the people we're talking with */
//	g_signal_handlers_disconnect_by_func(cw->window,
//		G_CALLBACK(handle_focus), cw);
	remove_smiley_window(cw);
	cw->pane = NULL;

	eb_debug(DBG_CORE, "calling ay_conversation_end\n");

	ay_conversation_end(cw->conv);

	eb_debug(DBG_CORE, "calling remove_from_chat_window_list %p\n", cw);
	remove_from_chat_window_list(cw);

	g_free(cw);
}

/* This is the callback for closing the window*/
static gboolean cw_close_win(GtkWidget *window, gpointer userdata)
{
	eb_debug(DBG_CORE, "Deleted window\n");

	return FALSE;
}

static void close_tab_callback(GtkWidget *button, gpointer userdata)
{
	chat_window *cw = userdata;
	GtkWidget *window = cw->window;
	GtkWidget *notebook = cw->notebook;

	int tab_number = gtk_notebook_page_num(GTK_NOTEBOOK(notebook),
		cw->notebook_child);

	eb_debug(DBG_CORE, "tab_number %d notebook %p\n", tab_number, notebook);

/*	g_signal_handlers_block_by_func(notebook,
		G_CALLBACK(chat_notebook_switch_callback), NULL);
	g_signal_handlers_unblock_by_func(notebook,
		G_CALLBACK(chat_notebook_switch_callback), NULL);
*/
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), tab_number);
	eb_debug(DBG_CORE, "removed page\n");
	if (!gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0)) {
		eb_debug(DBG_CORE, "destroying the whole window\n");
		gtk_widget_destroy(window);
		return;
	}
}

void ay_chat_window_free(chat_window *cw)
{
	if (!cw->pane)
		return;

	if (cw->notebook_child)
		close_tab_callback(NULL, cw);
	else
		gtk_widget_destroy(cw->window);
}

static gboolean chat_focus_event(GtkNotebook *notebook, GtkNotebookPage *page,
	guint pagenum, gpointer data)
{
	GtkWidget *label, *chatpane, *window;

	eb_debug(DBG_CORE, "Tab %d selected\n", pagenum);

	chatpane = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), pagenum);
	label = ay_chat_get_tab_label(GTK_NOTEBOOK(notebook), chatpane);

	eb_debug(DBG_CORE, "Setting tab to normal\n");
	gtk_widget_modify_fg(label, GTK_STATE_ACTIVE,
		NULL);

	window = gtk_widget_get_toplevel(chatpane);

	if (GTK_WIDGET_TOPLEVEL(window)) {
		gtk_window_set_urgency_hint(GTK_WINDOW(window), FALSE);
		gtk_window_set_title(GTK_WINDOW(window),
			gtk_label_get_text(GTK_LABEL(label)));
	}

	return FALSE;
}

static void add_unknown_callback(GtkWidget *add_button, gpointer userdata)
{
	chat_window *cw = userdata;

	/* if something wierd is going on and the unknown contact has multiple
	 * accounts, find use the preferred account
	 */

	cw->conv->preferred =
		find_suitable_remote_account(cw->conv->preferred, cw->conv->contact);

	/* if in the weird case that the unknown user has gone offline
	 * just use the first account you see
	 */

	if (!cw->conv->preferred)
		cw->conv->preferred = cw->conv->contact->accounts->data;

	/* if that fails, something is seriously wrong
	 * bail out while you can
	 */

	if (!cw->conv->preferred)
		return;

	/* now that we have a valid account, pop up the window :) */

	add_unknown_account_window_new(cw->conv->preferred);
}

static char *cw_get_message(chat_window *cw)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->entry));
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);

	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static int cw_set_message(chat_window *cw, char *msg)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->entry));
	GtkTextIter end;

	gtk_text_buffer_set_text(buffer, msg, strlen(msg));
	gtk_text_buffer_get_end_iter(buffer, &end);

	return gtk_text_iter_get_offset(&end);
}

static void cw_reset_message(chat_window *cw)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->entry));
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);
}

void send_message(GtkWidget *widget, gpointer d)
{
	chat_window *data = (chat_window *)d;
	gchar *text;
#ifdef __MINGW32__
	char *recoded;
#endif

	text = cw_get_message(data);

	if (!strlen(text))
		return;

	if (data->conv->this_msg_in_history) {
		LList *node = NULL, *node2 = NULL;

		for (node = data->conv->history; node; node = node->next)
			node2 = node;
		free(node2->data);
		node2->data = strdup(text);
		data->conv->this_msg_in_history = 0;
	} else {
		data->conv->history = l_list_append(data->conv->history, strdup(text));
		data->conv->hist_pos = NULL;
	}

#ifdef __MINGW32__
	recoded = ay_utf8_to_str(text);
	g_free(text);
	text = recoded;
#endif

	ay_conversation_send_message (data->conv, text);

	if (data->sound_enabled && data->send_enabled)
		play_sound(SOUND_SEND);

	cw_reset_message(data);
#ifdef __MINGW32__
	redraw_chat_window(data->chat);
#endif
}

/* These are the action handlers for the buttons */

/* **MIZHI
 *callback function for viewing the log
 */
static void view_log_callback(GtkWidget *widget, gpointer d)
{
	chat_window *data = (chat_window *)d;
	eb_view_log(data->conv->contact);
}

static void action_callback(GtkWidget *widget, gpointer d)
{
	chat_window *data = (chat_window *)d;
	conversation_action(data->conv->logfile, TRUE);
}

/* This is the callback for ignoring a user*/

static void ignore_dialog_callback(gpointer userdata, int response)
{
	struct contact *c = (struct contact *)userdata;

	if (response) {
		move_contact(_("Ignore"), c);
		update_contact_list();
		write_contact_list();
	}
}

static void ignore_callback(GtkWidget *ignore_button, gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;
	gchar *buff = NULL;

	buff = g_strdup_printf(_("Do you really want to ignore %s?"),
		cw->conv->contact->nick);

	eb_do_dialog(buff, _("Ignore Contact"), ignore_dialog_callback,
		cw->conv->contact);

	g_free(buff);
}

/* This is the callback for file x-fer*/
static void send_file(GtkWidget *sendf_button, gpointer userdata)
{
	eb_account *ea;
	chat_window *data = (chat_window *)userdata;

	if (!data->conv->contact->online) {
		ay_conversation_display_notification(data->conv,
			_(" Cannot send message - user is offline.\n"),
			CHAT_NOTIFICATION_ERROR);
		return;
	}

	if ((ea = find_suitable_remote_account(data->conv->preferred, data->conv->contact)))
		eb_do_send_file(ea);
}

/* These are the callback for setting the sound*/
static void set_sound_callback(GtkWidget *sound_button, gpointer userdata);
static void cw_set_sound_active(chat_window *cw, int active)
{
	cw->sound_enabled = active;
	g_signal_handlers_block_by_func(cw->sound_button,
		G_CALLBACK(set_sound_callback), cw);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(cw->
			sound_button), active);
	g_signal_handlers_unblock_by_func(cw->sound_button,
		G_CALLBACK(set_sound_callback), cw);
}

static int cw_get_sound_active(chat_window *cw)
{
	return cw->sound_enabled;
}

static void set_sound_callback(GtkWidget *sound_button, gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;
	cw_set_sound_active(cw, !cw_get_sound_active(cw));
}

/*
static void allow_offline_callback(GtkWidget *offline_button,
	gpointer userdata);
static void cw_set_offline_active(chat_window *cw, int active)
{
	cw->contact->send_offline = active;
	g_signal_handlers_block_by_func(cw->offline_button,
		G_CALLBACK(allow_offline_callback), cw);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(cw->
			offline_button), active);
	g_signal_handlers_unblock_by_func(cw->offline_button,
		G_CALLBACK(allow_offline_callback), cw);
}

static int cw_get_offline_active(chat_window *cw)
{
	return cw->contact->send_offline;
}

static void allow_offline_callback(GtkWidget *offline_button, gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;
	cw_set_offline_active(cw, !cw_get_offline_active(cw));
}*/

static void get_group_contacts(gchar *group, chat_window *cw)
{
        LList *node = NULL, *accounts = NULL;
	grouplist *g;
	Conversation *conv = cw->conv;

	g = find_grouplist_by_name(group);

	if (g)
		node = g->members;

	while (node) {
		struct contact *contact = (struct contact *)node->data;
		accounts = contact->accounts;
		while (accounts) {
			struct account *acc = accounts->data;

			if (acc->ela == conv->local_user && acc->online) {
				char *buf = g_strdup_printf("%s (%s)",
						contact->nick, acc->handle);

				gtk_combo_box_append_text(
					GTK_COMBO_BOX(cw->invite_buddy),
					buf);
			}
			accounts = accounts->next;
		}
		node = node->next;
	}
}

static void get_contacts(chat_window *cw)
{
        LList *node = groups;
	while (node) {
		get_group_contacts(node->data, cw);
		node = node->next;
	}
}

static void invite_callback(GtkWidget *dialog, int response, gpointer data)
{
	chat_window *cw = data;
	Conversation *conv = cw->conv;

	char *acc = NULL;
	char *invited = NULL;
	
	if (response != GTK_RESPONSE_ACCEPT)
		goto out;

	invited = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cw->invite_buddy));

	if (!invited || !strstr(invited, "(") || !strstr(invited, ")"))
		goto out;

	acc = strstr(invited, "(") + 1;
	*strstr(acc, ")") = '\0';

	if (!conv->is_room) {
		eb_account *third = NULL;
		third = find_account_by_handle(acc,
			conv->local_user->service_id);
		if (third)
			conv = ay_conversation_clone_as_room(conv);
	}

	ay_conversation_invite_fellow(conv, acc, 
		gtk_entry_get_text(GTK_ENTRY(cw->invite_message)));

out:
	g_free(invited);
	gtk_widget_destroy(cw->invite_window);
	cw->invite_window = NULL;
	cw->invite_buddy = NULL;
	cw->invite_message = NULL;
}

static void do_invite_window(GtkWidget *source, gpointer data)
{
	GtkWidget *box, *label, *table;
	chat_window *cw = data;
	Conversation * conv = cw->conv;

	/* Do we already have an invite window open? */
	if (!cw || cw->invite_window)
		return;
	
	if (!conv->local_user && conv->preferred)
		conv->local_user = conv->preferred->ela;
	if (!conv->local_user) {
		ay_do_error(_("Chatroom error"),
		_("Cannot invite a third party until a protocol has been chosen."));
		return;
	}

	table = gtk_table_new(2, 2, FALSE);

	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);

	label = gtk_label_new(_("Handle: "));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(label);

	cw->invite_buddy = gtk_combo_box_new_text();
	get_contacts(cw);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw->invite_buddy), -1);
	gtk_widget_show(cw->invite_buddy);

	gtk_table_attach(GTK_TABLE(table), cw->invite_buddy, 1, 2, 0, 1,
		GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new(_("Message: "));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL,
		0, 0);
	gtk_widget_show(label);

	cw->invite_message = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), cw->invite_message, 1, 2, 1, 2,
		GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(cw->invite_message);

	gtk_widget_show(table);

	label = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Select the contact you want to invite</b>"));

	cw->invite_window = gtk_dialog_new_with_buttons(_("Invite a contact"),
							GTK_WINDOW(cw->window),
							GTK_DIALOG_MODAL | 
							GTK_DIALOG_DESTROY_WITH_PARENT,
							NULL);

	gtk_window_set_resizable(GTK_WINDOW(cw->invite_window), FALSE);

	box = gtk_dialog_get_content_area(GTK_DIALOG(cw->invite_window));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), table, TRUE, TRUE, 0);

	label = gtkut_stock_button_new_with_label(_("Invite"), GTK_STOCK_ADD);
	gtk_dialog_add_action_widget(GTK_DIALOG(cw->invite_window), label,
		GTK_RESPONSE_ACCEPT);

	label = gtkut_stock_button_new_with_label(_("Cancel"), GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget(GTK_DIALOG(cw->invite_window), label,
		GTK_RESPONSE_REJECT);

	g_signal_connect(cw->invite_window, "response",
		G_CALLBACK(invite_callback), cw);

	gtk_widget_show_all (cw->invite_window);
}

static void change_local_account_on_click(GtkWidget *button, gpointer userdata)
{
	GtkLabel *label = GTK_LABEL(GTK_BIN(button)->child);
	chat_window_account *cwa = (chat_window_account *)userdata;
	chat_window *cw;
	const gchar *account;
	eb_local_account *ela = NULL;

	/* Should never happen */
	if (!cwa)
		return;
	cw = cwa->cw;
	ela = (eb_local_account *)cwa->data;
	cw->conv->local_user = ela;
	/* don't free it */
	account = gtk_label_get_text(label);
	eb_debug(DBG_CORE, "change_local_account_on_click: %s\n", account);
}

static GtkWidget *get_local_accounts(Conversation *cw)
{
	GtkWidget *submenu, *label, *button;
	char *handle = NULL, buff[256];
	eb_local_account *first_act = NULL, *subsequent_act = NULL;
	chat_window_account *cwa = NULL;

	/* Do we have a preferred remote account, no, get one */
	if (!cw->preferred) {
		cw->preferred = find_suitable_remote_account(NULL, cw->contact);
		if (!cw->preferred)	/* The remote user is not online */
			return (NULL);
	}
	handle = cw->preferred->handle;
	eb_debug(DBG_CORE, "Setting menu item with label: %s\n", handle);
	/* Check to see if we have at least 2 local accounts suitable for the remote account */
	first_act = find_local_account_for_remote(cw->preferred, TRUE);
	subsequent_act = find_local_account_for_remote(NULL, TRUE);
	if (!first_act || !subsequent_act || first_act == subsequent_act)
		return (NULL);

	first_act = find_local_account_for_remote(cw->preferred, TRUE);

	/* Start building the menu */
	label = gtk_menu_item_new_with_label(_("Change Local Account"));
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label), submenu);

	subsequent_act = first_act;
	do {
		snprintf(buff, sizeof(buff), "%s:%s",
			get_service_name(subsequent_act->service_id),
			subsequent_act->handle);
		button = gtk_menu_item_new_with_label(buff);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), button);
		cwa = g_new0(chat_window_account, 1);
		cwa->cw = cw->window;
		cwa->data = subsequent_act;
		g_signal_connect(button, "activate",
			G_CALLBACK(change_local_account_on_click), cwa);
		gtk_widget_show(button);
	} while ((subsequent_act = find_local_account_for_remote(NULL, TRUE)));

	gtk_widget_show(label);
	gtk_widget_show(submenu);

	return (label);
}

static gboolean handle_focus(GtkWidget *widget, GdkEventFocus *event,
	gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;
	GtkWidget *chatpane = NULL;

	/* Bring this window to the front of the list */
	chat_window_list = l_list_remove(chat_window_list, cw);
	chat_window_list = l_list_prepend(chat_window_list, cw);

	if (cw->notebook)
		chatpane = gtk_notebook_get_nth_page(GTK_NOTEBOOK(cw->notebook),
			gtk_notebook_get_current_page(GTK_NOTEBOOK(cw->notebook)));
		cw = g_object_get_data(G_OBJECT(chatpane), "cw_object");

	if (cw)
		gtk_widget_grab_focus(cw->entry);

	return FALSE;
}

/* This handles the right mouse button clicks*/

static gboolean handle_click(GtkWidget *widget, GdkEventButton *event,
	gpointer userdata)
{
	chat_window *cw = (chat_window *)userdata;

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		GtkWidget *menu, *button;
		menu_data *md = NULL;
		char encoding_label[1024];

		g_signal_stop_emission_by_name(GTK_OBJECT(widget),
			"button-press-event");
		menu = gtk_menu_new();

		/* Add Contact Selection */
		if (!strcmp(cw->conv->contact->group->name, _("Unknown"))
			|| !strncmp(cw->conv->contact->group->name,
				"__Ayttm_Dummy_Group__",
				strlen("__Ayttm_Dummy_Group__"))) {
			button = gtk_menu_item_new_with_label(_("Add Contact"));
			g_signal_connect(button, "activate",
				G_CALLBACK(add_unknown_callback), cw);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}

		/* Allow Offline Messaging Selection */

/*		if (can_offline_message(cw->conv->contact)) {
			button = gtk_menu_item_new_with_label(_
				("Offline Messaging"));
			g_signal_connect(button, "activate",
				G_CALLBACK(allow_offline_callback), cw);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}*/

		/* Allow account selection when there are multiple accounts for the same protocl */
		if ((button = get_local_accounts(cw->conv)))
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);

		/* Sound Selection */
		button = gtk_menu_item_new_with_label(_(cw->
				sound_enabled ? "Disable Sounds" :
				"Enable Sounds"));

		g_signal_connect(button, "activate",
			G_CALLBACK(set_sound_callback), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		/* Character Encoding for the chat room */
		if (cw->conv->encoding)
			snprintf(encoding_label, sizeof(encoding_label),
				_("Set Character Encoding (%s)"), cw->conv->encoding);
		else
			snprintf(encoding_label, sizeof(encoding_label),
				_("Set Character Encoding"));

		button = gtk_menu_item_new_with_label(encoding_label);

		g_signal_connect(button, "activate",
			G_CALLBACK(ay_set_chat_encoding), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		/* View log selection */
		button = gtk_menu_item_new_with_label(_("View Log"));
		g_signal_connect(button, "activate",
			G_CALLBACK(view_log_callback), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

#ifndef __MINGW32__
		button = gtk_menu_item_new_with_label(_("Actions..."));
		g_signal_connect(button, "activate",
			G_CALLBACK(action_callback), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);
#endif

		gtkut_create_menu_button(GTK_MENU(menu), NULL, NULL, NULL);

		/* Send File Selection */
		button = gtk_menu_item_new_with_label(_("Send File"));
		g_signal_connect(button, "activate", G_CALLBACK(send_file), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		/* Invite Selection */
		if (cw->conv->is_room || 
		    can_conference(GET_SERVICE(cw->conv->local_user))) {
			button = gtk_menu_item_new_with_label(_("Invite"));
			g_signal_connect(button, "activate",
					 G_CALLBACK(do_invite_window), cw);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}

		/* Ignore Section */
		button = gtk_menu_item_new_with_label(_("Ignore Contact"));
		g_signal_connect(button, "activate",
			G_CALLBACK(ignore_callback), cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		/* Send message Section */
		button = gtk_menu_item_new_with_label(_("Send Message"));
		g_signal_connect(button, "activate", G_CALLBACK(send_message),
			cw);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		/* Add Plugin Menus */
		if ((md = GetPref(EB_CHAT_WINDOW_MENU))) {
			int should_sep = 0;
			gtkut_create_menu_button(GTK_MENU(menu), NULL, NULL,
				NULL);
			should_sep =
				(add_menu_items(menu, -1, should_sep, NULL,
					cw->conv->preferred, cw->conv->local_user) > 0);
			if (cw->conv->local_user)
				add_menu_items(menu, cw->conv->local_user->service_id,
					should_sep, NULL, cw->conv->preferred,
					cw->conv->local_user);
			gtkut_create_menu_button(GTK_MENU(menu), NULL, NULL,
				NULL);
		}

		/* Close Selection */
/* Useless...		button = gtk_menu_item_new_with_label(_("Close"));

		if (iGetLocalPref("do_tabbed_chat"))
			g_signal_connect(button, "activate",
				G_CALLBACK(close_tab_callback), cw);
		else
			g_signal_connect(button, "activate",
				G_CALLBACK(cw_close_win), cw);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);
*/

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	}

	return FALSE;
}

gboolean check_tab_accelerators(const GtkWidget *inWidget,
	const chat_window *inCW, GdkModifierType inModifiers,
	const GdkEventKey *inEvent)
{
	if (inCW->notebook) {	/* only change tabs if this window is tabbed */
		GdkDeviceKey accel_prev_tab, accel_next_tab;

		gtk_accelerator_parse(cGetLocalPref("accel_next_tab"),
			&(accel_next_tab.keyval), &(accel_next_tab.modifiers));

		gtk_accelerator_parse(cGetLocalPref("accel_prev_tab"),
			&(accel_prev_tab.keyval), &(accel_prev_tab.modifiers));

		/* 
		 * this does not allow for using the same keyval for both prev and next
		 * when next contains every modifier that previous has (with some more)
		 * but i really don't think that will be a huge problem =)
		 */
		if (inModifiers == accel_prev_tab.modifiers
			&& inEvent->keyval == accel_prev_tab.keyval) {

			g_signal_stop_emission_by_name(G_OBJECT(inWidget),
				"key-press-event");
			gtk_notebook_prev_page(GTK_NOTEBOOK(inCW->notebook));
			return TRUE;
		} else if (inModifiers == accel_next_tab.modifiers
			&& inEvent->keyval == accel_next_tab.keyval) {

			g_signal_stop_emission_by_name(G_OBJECT(inWidget),
				"key-press-event");
			gtk_notebook_next_page(GTK_NOTEBOOK(inCW->notebook));
			return TRUE;
		}
	}

	return FALSE;
}

static void chat_away_set_back(void *data, int value)
{
	if (value) {
		chat_window *cw = (chat_window *)data;
		cw->conv->away_warn_displayed = (time_t) NULL;
		away_window_set_back();
	}
}

static void chat_warn_if_away(chat_window *cw)
{
	if (is_away && (time(NULL) - cw->conv->away_warn_displayed) > (60 * 30)) {
		cw->conv->away_warn_displayed = time(NULL);
		eb_do_dialog(_
			("You are currently away. \n\nDo you want to be back Online?"),
			_("Away"), chat_away_set_back, cw);
	}
}

static void chat_history_up(chat_window *cw)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->entry));
	GtkTextIter start, end;
	int p = 0;

	gtk_text_buffer_get_bounds(buffer, &start, &end);

	if (!cw->conv->history)
		return;

	if (!cw->conv->hist_pos) {
		LList *node = NULL;
		char *s = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

		for (node = cw->conv->history; node; node = node->next)
			cw->conv->hist_pos = node;

		if (strlen(s) > 0) {
			cw->conv->history = l_list_append(cw->conv->history, strdup(s));
			g_free(s);
			cw->conv->this_msg_in_history = 1;
		}
	} else if (cw->conv->hist_pos->prev)
		cw->conv->hist_pos = cw->conv->hist_pos->prev;

	gtk_text_buffer_delete(buffer, &start, &end);
	p = cw_set_message(cw, cw->conv->hist_pos->data);
}

static void chat_history_down(chat_window *cw)
{
	GtkTextBuffer *buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->entry));
	GtkTextIter start, end;
	int p = 0;

	gtk_text_buffer_get_bounds(buffer, &start, &end);

	if (!cw->conv->history || !cw->conv->hist_pos)
		return;

	cw->conv->hist_pos = cw->conv->hist_pos->next;

	gtk_text_buffer_delete(buffer, &start, &end);

	if (cw->conv->hist_pos)
		p = cw_set_message(cw, cw->conv->hist_pos->data);
}

/* TODO: Review this */
static void chat_scroll(chat_window *cw, GdkEventKey *event)
{
	GtkWidget *scwin = cw->chat->parent;
	if (event->keyval == GDK_Page_Up) {
		GtkAdjustment *ga =
			gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW
			(scwin));
		if (ga && ga->value > ga->page_size) {
			gtk_adjustment_set_value(ga, ga->value - ga->page_size);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW
				(scwin), ga);
		} else if (ga) {
			gtk_adjustment_set_value(ga, 0);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW
				(scwin), ga);
		}
	} else if (event->keyval == GDK_Page_Down) {
		GtkWidget *scwin = cw->chat->parent;
		GtkAdjustment *ga =
			gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW
			(scwin));
		if (ga && ga->value < ga->upper - ga->page_size) {
			gtk_adjustment_set_value(ga, ga->value + ga->page_size);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW
				(scwin), ga);
		} else if (ga) {
			gtk_adjustment_set_value(ga, ga->upper);
			gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW
				(scwin), ga);
		}
	}
}

/* FIXME: rework this */
gboolean chat_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	static gboolean complete_mode = FALSE;
	chat_window *cw = (chat_window *)data;
	const GdkModifierType modifiers = event->state &
		(GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
		GDK_MOD4_MASK);
	gboolean do_complete = iGetLocalPref("do_auto_complete");
	gboolean do_multi = iGetLocalPref("do_multi_line");

	if (event->keyval == GDK_Return) {
		if (do_complete)
			chat_auto_complete_insert(cw->entry, event);
		/* Just print a newline on Shift-Return */
		/* But only if we are told to do multiline... */
		if (event->state & GDK_SHIFT_MASK && do_multi)
			event->state = 0;
		/* ... otherwise simply print out the grub */
		else if (!do_multi || iGetLocalPref("do_enter_send")) {
			gtk_text_buffer_delete_selection
				(gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->
						entry)), FALSE, TRUE);

			/* Prevents a newline from being printed */
			g_signal_stop_emission_by_name(GTK_OBJECT(widget),
				"key-press-event");

			chat_warn_if_away(cw);
			send_message(NULL, cw);
			return TRUE;
		}
	} else if (event->keyval == GDK_Up && event->state & GDK_CONTROL_MASK)
		chat_history_up(cw);
	else if (event->keyval == GDK_Down && event->state & GDK_CONTROL_MASK)
		chat_history_down(cw);
	else if (event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down)
		chat_scroll(cw, event);
	else if (modifiers
		&& check_tab_accelerators(widget, cw, modifiers, event))
		/* check tab changes if this is a tabbed chat window */
		return TRUE;
	else if (!modifiers && cw->conv->preferred && cw->conv->local_user)
		ay_conversation_send_typing_status(cw->conv);

	if (do_complete) {
		if (event->keyval == GDK_space || ispunct(event->keyval)) {
			chat_auto_complete_insert(cw->entry, event);
			complete_mode = FALSE;
		} else if (event->keyval == GDK_Tab) {
			chat_auto_complete_validate(cw->entry);
			complete_mode = FALSE;
			return TRUE;
		} else if ((event->keyval == GDK_Right
				|| event->keyval == GDK_Left) && !modifiers) {
			GtkTextBuffer *buffer =
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(cw->
					entry));
			GtkTextIter start, end;

			if (gtk_text_buffer_get_selection_bounds(buffer, &start,
					&end) && complete_mode) {
				complete_mode = FALSE;
				gtk_text_buffer_select_range(buffer, &start,
					&start);
				return TRUE;
			} else {
				complete_mode = FALSE;
				return FALSE;
			}
		} else if ((event->keyval >= GDK_a && event->keyval <= GDK_z)
			|| (event->keyval >= GDK_A && event->keyval <= GDK_Z)) {

			complete_mode = TRUE;
			return chat_auto_complete(cw->entry,
				auto_complete_session_words, event);
		}
	}
	return FALSE;
}

/*static gboolean chat_notebook_switch_callback(GtkNotebook *notebook,
	GtkNotebookPage *page, gint page_num, gpointer user_data)
{
	/ * find the contact for the page we just switched to and turn off their talking penguin icon * /
	LList *l1 = chat_window_list;
	GtkWidget *label = NULL;

	while (l1 && l1->data) {
		chat_window *cw = (chat_window *)l1->data;
		int tab_number = gtk_notebook_page_num(GTK_NOTEBOOK(cw->notebook),
			cw->notebook_child);

		if (tab_number == page_num) {
			label = ay_chat_get_tab_label(GTK_NOTEBOOK
				(cw->notebook), cw->notebook_child);

			if (cw->is_child_red) {
				eb_debug(DBG_CORE, "Setting tab to normal\n");
				gtk_widget_modify_fg(label, GTK_STATE_ACTIVE,
					NULL);
				cw->is_child_red = FALSE;

				gtk_window_set_urgency_hint(GTK_WINDOW(cw->window), FALSE);
			}

			gtk_window_set_title(GTK_WINDOW(cw->window),
				gtk_label_get_text(label));

			gtk_widget_grab_focus(cw->entry);

			return FALSE;
		}
		l1 = l1->next;
	}

	return TRUE;
}*/

static chat_window *find_current_chat_window()
{
	return (chat_window_list?chat_window_list->data:NULL);
}

static void _show_smileys_cb(GtkWidget *widget, smiley_callback_data *data)
{
	show_smileys_cb(data);
}

static int should_window_raise(const char *message)
{
	if (iGetLocalPref("do_raise_window")) {
		char *thepattern = cGetLocalPref("regex_pattern");

		if (thepattern && thepattern[0]) {
			regex_t myreg;
			regmatch_t pmatch;
			int rc = regcomp(&myreg, thepattern,
				REG_EXTENDED | REG_ICASE);

			if (!rc) {
				rc = regexec(&myreg, message, 1, &pmatch, 0);
				if (!rc)
					return 1;
			}
		} else
			/* no pattern specified. Assume always. */
			return 1;
	}
	return 0;
}

void ay_chat_window_printr(chat_window *cw, gchar *o_message)
{
	if (!o_message || !o_message[0])
		return;

	html_text_buffer_append(GTK_TEXT_VIEW(cw->chat),
		o_message,
		HTML_IGNORE_END |
		(iGetLocalPref("do_ignore_back") ? HTML_IGNORE_BACKGROUND :
			HTML_IGNORE_NONE)
		| (iGetLocalPref("do_ignore_fore") ?
			HTML_IGNORE_FOREGROUND : HTML_IGNORE_NONE)
		| (iGetLocalPref("do_ignore_font") ? HTML_IGNORE_FONT :
			HTML_IGNORE_NONE));
#ifdef __MINGW32__
	redraw_chat_window(cw->chat);
#endif
}

void ay_chat_window_raise(chat_window *cw, const char *message)
{
	if (!message || should_window_raise(message))
		gdk_window_raise(cw->window->window);
}

void ay_chat_window_print_message(chat_window *cw, gchar *o_message, int send)
{
	if (!o_message || !strlen(o_message))
		return;

	ay_chat_window_print(cw, o_message);

	/* TODO Flash tab/window */
	if (cw->sound_enabled) {
		if (cw->firstmsg) {
			play_sound(cw->first_enabled ? SOUND_FIRSTMSG : SOUND_RECEIVE);
			cw->firstmsg = FALSE;
		} else if (cw->receive_enabled && !send)
			play_sound(SOUND_RECEIVE);
		else if (cw->send_enabled && send)
			play_sound(SOUND_SEND);
	}

	/* for raising the window */
	ay_chat_window_raise(cw, o_message);
}

void ay_chat_window_print(chat_window *cw, gchar *o_message)
{
	html_text_buffer_append(GTK_TEXT_VIEW(cw->chat),
		o_message,
		(iGetLocalPref("do_ignore_back") ? HTML_IGNORE_BACKGROUND :
			HTML_IGNORE_NONE) | (iGetLocalPref("do_ignore_fore") ?
			HTML_IGNORE_FOREGROUND : HTML_IGNORE_NONE) |
		(iGetLocalPref("do_ignore_font") ? HTML_IGNORE_FONT :
			HTML_IGNORE_NONE));
#ifdef __MINGW32__
	redraw_chat_window(cw->chat);
#endif
}

void eb_chat_window_display_status(eb_account *remote, gchar *message)
{
	struct contact *remote_contact = remote->account_contact;
	char *tmp = NULL;

	if (!remote_contact)
		remote_contact = find_contact_by_handle(remote->handle);

	/* trim @.*part for User is typing */
	if (!remote_contact || !remote_contact->conversation) {
		gchar **tmp_ct = NULL;
		if (strchr(remote->handle, '@')) {
			tmp_ct = g_strsplit(remote->handle, "@", 2);
			remote_contact = find_contact_by_handle(tmp_ct[0]);
			g_strfreev(tmp_ct);
		}
	}

	if (!remote_contact || !remote_contact->conversation)
		return;

	if (message && strlen(message) > 0)
		tmp = g_strdup_printf("%s", message);
	else
		tmp = g_strdup_printf(" ");

	ay_conversation_display_notification(remote_contact->conversation, tmp,
		CHAT_NOTIFICATION_WORKING);

	g_free(tmp);
}

static void destroy_smiley_cb_data(GtkWidget *widget, gpointer data)
{
	smiley_callback_data *scd = data;

	if (data)
		g_free(scd);
}

static void add_page_with_pane(chat_window *cw, GtkWidget *vbox, const char *name,
	GtkAccelGroup *accel_group)
{
	GtkWidget *contact_label = NULL;
	GtkWidget *close_button;
	GtkWidget *tab_label;

	/* set up the text and close button */
	contact_label = gtk_label_new(name);
	close_button = gtk_button_new();

	gtk_label_set_max_width_chars(GTK_LABEL(contact_label), 24);
	gtk_label_set_ellipsize(GTK_LABEL(contact_label), PANGO_ELLIPSIZE_END);

	gtk_rc_parse_string (
		"style \"tab-button-style\"\n"
		"{\n"
			"  GtkWidget::focus-padding = 0\n"
			"  GtkWidget::focus-line-width = 0\n"
			"  xthickness = 0\n"
			"  ythickness = 0\n"
		"}\n"
		"widget \"*.tab-close-button\" style \"tab-button-style\"");

	gtk_widget_set_name(close_button, "tab-close-button");

	gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

	gtk_button_set_image(GTK_BUTTON(close_button),
		gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));

	g_signal_connect(close_button, "clicked", G_CALLBACK(close_tab_callback),
		cw);

/* FIXME: This doesn't quite work.
 * 	The idea is to install the handler for the window and then
 * 	once the window receives the event, we find the in-focus
 * 	tab and close it.
 * 	If that is the last tab then close the window.
 * 	If tabbed chat is disabled then close the window.

	gtk_widget_add_accelerator(close_button, "clicked", accel_group,
		GDK_w, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
*/
	tab_label = gtk_hbox_new(FALSE, 3);

	gtk_box_pack_start(GTK_BOX(tab_label), contact_label, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(tab_label), close_button, FALSE, FALSE, 0);

	gtk_widget_show(contact_label);
	gtk_widget_show(close_button);
	gtk_widget_show(tab_label);

	/* we use vbox as our child */
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(cw->notebook), vbox,
		tab_label);

	cw->notebook_child = vbox;
}

void ay_chat_window_set_name(chat_window *cw)
{
	Conversation *conv = cw->conv;

	g_free(cw->name);

	if (conv->local_user && GET_SERVICE(conv->local_user).name)
		cw->name = g_strdup_printf("%s (%s@%s)",
			conv->name,
			conv->local_user->handle,
			GET_SERVICE(conv->local_user).name);
	else if (conv->local_user)
		cw->name = g_strdup_printf("%s (%s)",
			conv->name,
			conv->local_user->handle);
	else
		cw->name = g_strdup(conv->name);
}

static void layout_chatwindow(chat_window *cw, GtkWidget *vbox,
	GtkAccelGroup *accel_group)
{
	chat_window *tab_cw = NULL;
	int pos;

	/* we're doing a tabbed chat */
	if (iGetLocalPref("do_tabbed_chat")) {
		/* look for an already open tabbed chat window */
		tab_cw = find_current_chat_window();
		if (!tab_cw) {
			/* none exists, create one */
			cw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

			gtk_container_set_border_width(GTK_CONTAINER(cw->window), 2);

			g_signal_connect(cw->window, "delete-event", G_CALLBACK(cw_close_win),
				cw);
			/* Adding accelerators to windows */
			gtk_window_add_accel_group(GTK_WINDOW(cw->window), accel_group);

			gtk_window_set_wmclass(GTK_WINDOW(cw->window),
				"ayttm-chat", "Ayttm");
			gtk_window_set_resizable(GTK_WINDOW(cw->window), TRUE);
			gtk_widget_show(cw->window);

			tab_cw = cw;
		}

		/* Make a notebook if it does not exist */
		if (!tab_cw->notebook) {
			GList *child_list = NULL;
			GtkWidget *origpane = NULL;

			tab_cw->notebook = gtk_notebook_new();
			eb_debug(DBG_CORE, "NOTEBOOK %p\n", tab_cw->notebook);
			/* Set tab orientation.... */
			pos = GTK_POS_BOTTOM;
			switch (iGetLocalPref("do_tabbed_chat_orient")) {
			case 1:
				pos = GTK_POS_TOP;
				break;
			case 2:
				pos = GTK_POS_LEFT;
				break;
			case 3:
				pos = GTK_POS_RIGHT;
				break;
			case 0:
			default:
				pos = GTK_POS_BOTTOM;
				break;
			}
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tab_cw->notebook),
				pos);
			/* End tab orientation */

			gtk_notebook_set_scrollable(GTK_NOTEBOOK(tab_cw->notebook),
				TRUE);

			if ((child_list = gtk_container_get_children(GTK_CONTAINER(tab_cw->window)))) {
				origpane = child_list->data;
				origpane = g_object_ref(origpane);
				gtk_container_remove(GTK_CONTAINER(tab_cw->window), origpane);
			}

			gtk_container_add(GTK_CONTAINER(tab_cw->window),
				tab_cw->notebook);

			/* setup a signal handler for the notebook to handle page switches * /
			g_signal_connect(tab_cw->notebook, "switch-page",
				G_CALLBACK(chat_notebook_switch_callback),
				NULL); */
			g_signal_connect(tab_cw->notebook, "switch-page",
				G_CALLBACK(chat_focus_event), NULL);

			/* Wonder why we put cw here? It's because that is what
			 * is being put into chat_window_list */
			g_signal_connect(tab_cw->window, "focus-in-event",
				G_CALLBACK(handle_focus),
				cw);

			if (origpane)
				add_page_with_pane(tab_cw, origpane, 
					gtk_window_get_title(GTK_WINDOW(tab_cw->window)),
					accel_group);

			gtk_widget_show(tab_cw->notebook);
		}
		
		/* 
		 * Copy widget stuff if the window has not been
		 * created in this call
		 */
		if (tab_cw != cw) {
			cw->window = tab_cw->window;
			cw->notebook = tab_cw->notebook;
			eb_debug(DBG_CORE, "NOTEBOOK now %p\n", cw->notebook);
		}

		add_page_with_pane(cw, vbox, cw->conv->name, accel_group);
		gtk_widget_show(vbox);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(cw->notebook), 
			gtk_notebook_page_num(GTK_NOTEBOOK(cw->notebook), vbox));

	} else {
		/* setup like normal */
		cw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_container_set_border_width(GTK_CONTAINER(cw->window), 5);

		gtk_window_set_wmclass(GTK_WINDOW(cw->window), "ayttm-chat",
			"Ayttm");
		gtk_window_set_resizable(GTK_WINDOW(cw->window), TRUE);
		gtk_widget_show(cw->window);

		cw->notebook = NULL;
		cw->notebook_child = NULL;
		gtk_container_add(GTK_CONTAINER(cw->window), vbox);
		gtk_widget_show(vbox);
	}

	cw->pane = vbox;

	chat_window_list = l_list_prepend(chat_window_list, cw);
}

void ay_chat_window_fellows_append(chat_window *cw, ConversationFellow *fellow)
{
	GtkTreeIter insert;
	gtk_list_store_append(cw->fellows_model, &insert);

	gtk_list_store_set(cw->fellows_model, &insert, 0, fellow->alias, -1);
}

void ay_chat_window_fellows_rename(chat_window *cw, ConversationFellow *fellow)
{
	GtkTreeIter upd_iter;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cw->fellows_model), &upd_iter)) {
		eb_debug(DBG_CORE, "Nothing in the list yet??: %s\n", fellow->alias);
		return;
	}

	do {
		char *name = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(cw->fellows_model),
			&upd_iter, 0, &name, -1);

		if (!strcmp(name, fellow->alias)) {
			gtk_list_store_set(cw->fellows_model, 
				&upd_iter, 
				0, fellow->alias, -1);
			return;
		}
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(cw->
				fellows_model), &upd_iter));
}

void ay_chat_window_fellows_remove(chat_window *cw, ConversationFellow *fellow)
{
	GtkTreeIter del_iter;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cw->fellows_model), &del_iter)) {
		eb_debug(DBG_CORE, "Nothing in the list yet??: %s\n", fellow->alias);
		return;
	}

	do {
		char *name = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(cw->fellows_model),
			&del_iter, 0, &name, -1);

		if (!strcmp(name, fellow->alias)) {
			gtk_list_store_remove(cw->fellows_model,
				&del_iter);
			return;
		}
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(cw->
				fellows_model), &del_iter));
}

chat_window *ay_chat_window_new(Conversation *conv)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *scrollwindow;
	GtkWidget *toolbar;
	GtkWidget *add_button;
	GtkWidget *sendf_button;
	GtkWidget *send_button;
	GtkWidget *view_log_button;
	GtkWidget *print_button;
	GtkWidget *ignore_button;
	GtkWidget *invite_button;
	GtkWidget *iconwid;
	GdkPixbuf *icon;
	GtkAccelGroup *accel_group = NULL;
	GtkCellRenderer *renderer;
	
	GList *focus_chain = NULL;

	GtkWidget *separator;
	GtkWidget *resize_bar;
	chat_window *cw;
	gboolean enableSoundButton = FALSE;

	GtkWidget *chatpane = NULL;

	/* first we allocate room for the new chat window */
	cw = g_new0(chat_window, 1);
	cw->conv = conv;
	cw->smiley_window = NULL;

	ay_chat_window_set_name(cw);

	vbox = gtk_vbox_new(FALSE, 0);

	accel_group = gtk_accel_group_new();
	cw->chat = gtk_text_view_new();
	html_text_view_init(GTK_TEXT_VIEW(cw->chat), HTML_IGNORE_NONE);
	scrollwindow = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
		GTK_SHADOW_ETCHED_IN);

	gtk_widget_set_size_request(cw->chat, 400, 200);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(cw->chat), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(cw->chat), 5);

	gtk_container_add(GTK_CONTAINER(scrollwindow), cw->chat);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	if (conv->is_room) {
		GtkTreeViewColumn *column;

		chatpane = gtk_hpaned_new();

		gtk_paned_pack1(GTK_PANED(chatpane), scrollwindow, FALSE, FALSE);
		gtk_widget_show(scrollwindow);

		cw->fellows_model = gtk_list_store_new(1, G_TYPE_STRING);
		cw->fellows_widget =
			gtk_tree_view_new_with_model(GTK_TREE_MODEL(cw->
				fellows_model));

		renderer = gtk_cell_renderer_text_new();
		column =
			gtk_tree_view_column_new_with_attributes(_("Online"), renderer,
				"text", 0, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(cw->fellows_widget),
			column);
		gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE(cw->fellows_model), 0,
			GTK_SORT_ASCENDING);
        
		gtk_widget_set_size_request(cw->fellows_widget, 100, 20);
        
		scrollwindow = gtk_scrolled_window_new(NULL, NULL);

		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
			GTK_SHADOW_ETCHED_IN);

		gtk_container_add(GTK_CONTAINER(scrollwindow), cw->fellows_widget);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

		gtk_paned_pack2(GTK_PANED(chatpane), scrollwindow, FALSE, FALSE);

		gtk_widget_show(scrollwindow);
		gtk_widget_show(cw->fellows_widget);
	}
	else
		chatpane = scrollwindow;

	/* Create the bar for resizing chat/window box */

	/* Add stuff for multi-line */

	resize_bar = gtk_vpaned_new();
	gtk_paned_pack1(GTK_PANED(resize_bar), chatpane, TRUE, TRUE);
	gtk_widget_show(chatpane);

	scrollwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow),
		GTK_SHADOW_ETCHED_IN);

	cw->entry = gtk_text_view_new();

	gtk_widget_set_size_request(cw->entry, -1, 50);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(cw->entry), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(cw->entry), 5);

	gtk_container_add(GTK_CONTAINER(scrollwindow), cw->entry);

	gtk_text_view_set_editable(GTK_TEXT_VIEW(cw->entry), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(cw->entry),
		GTK_WRAP_WORD_CHAR);

#ifdef HAVE_LIBASPELL
	if (iGetLocalPref("do_spell_checking"))
		gtkspell_attach(GTK_TEXT_VIEW(cw->entry));
#endif

	g_signal_connect(cw->entry, "key-press-event",
		G_CALLBACK(chat_key_press), cw);

	gtk_paned_pack2(GTK_PANED(resize_bar), scrollwindow, FALSE, FALSE);

	gtk_widget_show(scrollwindow);
	gtk_box_pack_start(GTK_BOX(vbox), resize_bar, TRUE, TRUE, 5);
	gtk_widget_show(resize_bar);

	g_signal_connect(cw->chat, "button-press-event",
		G_CALLBACK(handle_click), cw);

	focus_chain = g_list_append(focus_chain, cw->entry);
	focus_chain = g_list_append(focus_chain, cw->chat);

	gtk_container_set_focus_chain(GTK_CONTAINER(resize_bar), focus_chain);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox, 275, 40);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
		GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);

#define TOOLBAR_APPEND(tool_btn,txt,icn,cbk,cwx) {\
	tool_btn = GTK_WIDGET(gtk_tool_button_new(icn,txt));\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(tool_btn),-1);\
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(tool_btn),txt);\
	g_signal_connect(tool_btn,"clicked",G_CALLBACK(cbk),cwx); \
	gtk_widget_show(tool_btn); }

#define TOOLBAR_APPEND_TOGGLE_BUTTON(tool_btn, txt, tip, icn, cbk, cwx) {\
	tool_btn = GTK_WIDGET(gtk_toggle_tool_button_new());\
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(tool_btn), icn);\
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(tool_btn), txt);\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(tool_btn), -1);\
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(tool_btn), tip);\
	g_signal_connect(tool_btn, "clicked", G_CALLBACK(cbk) ,cwx);\
        gtk_widget_show(tool_btn); }

#define ICON_CREATE_XPM(icon,iconwid,xpm) {\
	icon = gdk_pixbuf_new_from_xpm_data((const char **) xpm); \
	iconwid = gtk_image_new_from_pixbuf(icon); \
	gtk_widget_show(iconwid); }
#define ICON_CREATE(iconwid,stock) {\
	iconwid = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_SMALL_TOOLBAR); \
	gtk_widget_show(iconwid); }

/* line will tell whether to draw the separator line or not */
#define TOOLBAR_APPEND_SPACE(line) { \
	separator = GTK_WIDGET(gtk_separator_tool_item_new()); \
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), line); \
	gtk_widget_show(separator); \
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(separator), -1); \
}

	/* This is where we decide whether or not the add button should be displayed */
	if ( !conv->is_room && cw->conv->contact && 
		(!strcmp(cw->conv->contact->group->name, _("Unknown"))
		|| !strncmp(cw->conv->contact->group->name, "__Ayttm_Dummy_Group__",
			strlen("__Ayttm_Dummy_Group__")))) {
		ICON_CREATE(iconwid, GTK_STOCK_ADD);
		TOOLBAR_APPEND(add_button, _("Add Contact"), iconwid,
			add_unknown_callback, cw);

		TOOLBAR_APPEND_SPACE(TRUE);
	}
	else if (conv->is_room) {
/*		ICON_CREATE(iconwid, GTK_STOCK_REFRESH);
		TOOLBAR_APPEND_TOGGLE_BUTTON(cw->reconnect_button,
			_("Reconnect at login"),
			_("Reconnect at login"),
			iconwid, set_reconnect_on_toggle, cw);
			
/ * TODO		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(chat_room->reconnect_button), 
			eb_is_chatroom_auto(chat_room));*/
	}

	/* Decide whether the offline messaging button should be displayed */
	/* Do we really need this?? */
#if 0
	if (can_offline_message(remote) && !conv->is_room) {
		ICON_CREATE(iconwid, GTK_STOCK_EDIT);
		cw->offline_button = GTK_WIDGET(gtk_toggle_tool_button_new());
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(cw->
				offline_button), iconwid);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(cw->offline_button),
			_("Allow"));

		gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
			GTK_TOOL_ITEM(cw->offline_button), -1);

		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(cw->
				offline_button),
			_("Allow Offline Messaging Ctrl+O"));

		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(cw->
				offline_button), TRUE);

		g_signal_connect(cw->offline_button, "clicked",
			G_CALLBACK(allow_offline_callback), cw);
		gtk_widget_show(cw->offline_button);

		gtk_widget_add_accelerator(cw->offline_button, "clicked",
			accel_group, GDK_o, GDK_CONTROL_MASK,
			GTK_ACCEL_VISIBLE);

		separator = GTK_WIDGET(gtk_separator_tool_item_new());

		TOOLBAR_APPEND_SPACE(TRUE);

		cw_set_offline_active(cw, TRUE);
	}
#endif
	/* smileys */
	if (iGetLocalPref("do_smiley")) {
		smiley_callback_data *scd = g_new0(smiley_callback_data, 1);
		scd->c_window = cw;

		ICON_CREATE(iconwid, "ayttm_smileys");
		TOOLBAR_APPEND(cw->smiley_button, _("Insert Smiley"), iconwid,
			_show_smileys_cb, scd);
		g_signal_connect(cw->smiley_button, "destroy",
			G_CALLBACK(destroy_smiley_cb_data), scd);
		/* Create the separator for the toolbar */
		TOOLBAR_APPEND_SPACE(FALSE);

	}
	/* This is the sound toggle button */

	ICON_CREATE_XPM(icon, iconwid, tb_volume_xpm);

	cw->sound_button = GTK_WIDGET(gtk_toggle_tool_button_new());
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(cw->sound_button),
		iconwid);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(cw->sound_button),
		_("Sound"));

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
		GTK_TOOL_ITEM(cw->sound_button), -1);

	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(cw->sound_button),
		_("Enable Sound"/* Ctrl+S"*/));

	g_signal_connect(cw->sound_button, "clicked",
		G_CALLBACK(set_sound_callback), cw);
	gtk_widget_show(cw->sound_button);
/*
	gtk_widget_add_accelerator(cw->sound_button, "clicked", accel_group,
		GDK_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
*/
	/* Toggle the sound button based on preferences */
	cw->send_enabled = iGetLocalPref("do_play_send");
	cw->receive_enabled = iGetLocalPref("do_play_receive");
	cw->first_enabled = iGetLocalPref("do_play_first");

	enableSoundButton = iGetLocalPref("do_play_send");
	enableSoundButton |= iGetLocalPref("do_play_receive");
	enableSoundButton |= iGetLocalPref("do_play_first");

	cw_set_sound_active(cw, enableSoundButton);

	ICON_CREATE(iconwid, GTK_STOCK_FIND);
	TOOLBAR_APPEND(view_log_button, _("View Log"/* CTRL+L"*/), iconwid,
		view_log_callback, cw);
	
/* FIXME: Doesn't quite work
	gtk_widget_add_accelerator(view_log_button, "clicked", accel_group,
		GDK_l, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
*/
	TOOLBAR_APPEND_SPACE(TRUE);

#ifndef __MINGW32__
	ICON_CREATE(iconwid, GTK_STOCK_EXECUTE);
	TOOLBAR_APPEND(print_button, _("Actions..."), iconwid, action_callback,
		cw);
	gtk_widget_add_accelerator(print_button, "clicked", accel_group, GDK_p,
		GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

	TOOLBAR_APPEND_SPACE(TRUE);
#endif

	/* This is the send file button... only for private chats */
	if (cw->conv->contact) {
		ICON_CREATE(iconwid, GTK_STOCK_OPEN);
		TOOLBAR_APPEND(sendf_button, _("Send File"/* CTRL+T"*/), iconwid, send_file,
			cw);
		gtk_widget_add_accelerator(sendf_button, "clicked", accel_group, GDK_t,
			GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

		TOOLBAR_APPEND_SPACE(TRUE);
	}

	/* This is the invite button */
	if (cw->conv->is_room || 
	    can_conference(GET_SERVICE(conv->local_user))) {
		ICON_CREATE_XPM(icon, iconwid, invite_btn_xpm);
		TOOLBAR_APPEND(invite_button, _("Invite"/* CTRL+I"*/), iconwid,
			       do_invite_window, cw);
		gtk_widget_add_accelerator(invite_button, "clicked", accel_group, GDK_i,
					   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

		TOOLBAR_APPEND_SPACE(TRUE);
	}

	if (!conv->is_room) {
		/* This is the ignore button */
		ICON_CREATE(iconwid, GTK_STOCK_REMOVE);
		TOOLBAR_APPEND(ignore_button, _("Ignore"/* CTRL+G"*/), iconwid,
			ignore_callback, cw);
		gtk_widget_add_accelerator(ignore_button, "clicked", accel_group, GDK_g,
			GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

		TOOLBAR_APPEND_SPACE(TRUE);
	}

	/* This is the send button */
	ICON_CREATE(iconwid, GTK_STOCK_OK);
	TOOLBAR_APPEND(send_button, _("Send Message"/* CTRL+R"*/), iconwid,
		send_message, cw);
	gtk_widget_add_accelerator(send_button, "clicked", accel_group, GDK_r,
		GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

#undef TOOLBAR_APPEND
#undef TOOLBAR_APPEND_SPACE
#undef ICON_CREATE

	gtk_box_pack_end(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	gtk_widget_show(cw->chat);
	gtk_widget_show(cw->entry);

	g_object_set_data(G_OBJECT(vbox), "cw_object", cw);
	g_signal_connect(vbox, "destroy", G_CALLBACK(destroy_event), cw);

	layout_chatwindow(cw, vbox, accel_group);

	return cw;
}

void ay_set_chat_encoding(GtkWidget *widget, void *data)
{
	char title[255];
	Conversation *conv = data;

	snprintf(title, sizeof(title), _("Character Encoding for %s"),
		conv->name);

	do_text_input_window(title, conv->encoding ? conv->encoding : "",
		ay_conversation_set_encoding, conv);
}

/* The Join group chat window */
static gboolean join_service_is_open = 0;

static GtkWidget *chat_room_name;
static GtkWidget *chat_room_type;
static GtkWidget *join_chat_window;
static GtkWidget *public_chkbtn;
static GtkWidget *public_list_btn;
static GtkWidget *reconnect_chkbtn;

static GList *chat_service_list(GtkComboBox *service_list)
{
	GList *list = NULL;
	LList *walk = NULL;

	for (walk = accounts; walk; walk = walk->next) {
		eb_local_account *ela = (eb_local_account *)walk->data;
		if (ela && ela->connected
			&& can_group_chat(eb_services[ela->service_id])) {
			char *str =
				g_strdup_printf("[%s] %s",
				get_service_name(ela->service_id), ela->handle);
			gtk_combo_box_append_text(service_list, str);
		}
	}
	return list;
}

static LList *get_chatroom_mru(void)
{
	LList *mru = NULL;
	char buff[4096];
	FILE *fp = NULL;

	snprintf(buff, 4095, "%schatroom_mru", config_dir);

	fp = fopen(buff, "r");
	memset(buff, 0, 4096);
	while (fp && fgets(buff, sizeof(buff), fp)) {
		char *name = strdup((char *)buff);
		if (name[strlen(name) - 1] == '\n')
			name[strlen(name) - 1] = '\0';
		mru = l_list_append(mru, name);
		eb_debug(DBG_CORE, "name='%s'\n", name);
		memset(buff, 0, 4096);
	}
	if (fp)
		fclose(fp);

	return mru;
}

static void load_chatroom_mru(GtkComboBox *combo)
{
	char buff[4096];
	FILE *fp = NULL;

	snprintf(buff, 4095, "%schatroom_mru", config_dir);

	fp = fopen(buff, "r");
	memset(buff, 0, 4096);
	while (fp && fgets(buff, sizeof(buff), fp)) {
		char *name = strdup((char *)buff);
		if (name[strlen(name) - 1] == '\n')
			name[strlen(name) - 1] = '\0';
		gtk_combo_box_append_text(combo, name);

		eb_debug(DBG_CORE, "name='%s'\n", name);
		memset(buff, 0, 4096);
	}
	if (fp)
		fclose(fp);
}

static void add_chatroom_mru(const char *name)
{
	LList *mru = get_chatroom_mru();
	LList *cur = NULL;
	char buff[4096];
	FILE *fp = NULL;
	int i = 0;

	snprintf(buff, 4095, "%schatroom_mru", config_dir);

	fp = fopen(buff, "w");
	memset(buff, 0, 4096);

	mru = l_list_prepend(mru, strdup(name));

	if (fp) {
		for (cur = mru; cur && cur->data && i < 10; cur = cur->next)
			if (!i || strcmp(cur->data, name)) {
				/* not the same twice */
				fprintf(fp, "%s\n", (const char *)cur->data);
				i++;
			}
		fclose(fp);
	}

	l_list_free(mru);
}

static void join_chat_callback(GtkWidget *widget, int response, gpointer data)
{
	char *service =
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *name = NULL;
	char *mservice = NULL;
	char *local_acc = NULL;
	eb_local_account *ela = NULL;
	int service_id = -1;
	Conversation *conv = NULL;

	if (response != GTK_RESPONSE_ACCEPT)
		goto out;

	name = gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_name));

	if (!service || !strstr(service, "]") || !strstr(service, " ")) {
		ay_do_error(_("Cannot join"), _("No local account specified."));
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;

	*(strstr(service, "]")) = '\0';
	mservice = strstr(service, "[") + 1;

	service_id = get_service_id(mservice);
	eb_debug(DBG_CORE, "local_acc: %s, service_id: %d, mservice: %s\n",
		local_acc, service_id, mservice);
	ela = find_local_account_by_handle(local_acc, service_id);

	g_free(service);

	if (!ela) {
		ay_do_error(_("Cannot join"),
			_("The local account doesn't exist."));
		return;
	}

/* TODO: Chat room autoreconnect
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reconnect_chkbtn)))
		eb_add_auto_chatroom(ela, name,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
				(public_chkbtn)));
*/

	conv = RUN_SERVICE(ela)->make_chat_room(name, ela,
			    gtk_toggle_button_get_active(
				    GTK_TOGGLE_BUTTON(public_chkbtn)));

	RUN_SERVICE(ela)->join_chat_room(conv);

	add_chatroom_mru(name);

	g_free(name);

out:
	gtk_widget_destroy(join_chat_window);

	join_service_is_open = 0;
}

static void update_public_sensitivity(GtkWidget *widget, gpointer data)
{
	char *service =
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	int has_public = 0;

	if (!service && (!strstr(service, "]") || !strstr(service, " "))) {
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service, "[") + 1;

	service_id = get_service_id(mservice);
	g_free(service);

	has_public = (eb_services[service_id].sc->get_public_chatrooms != NULL);
	gtk_widget_set_sensitive(public_chkbtn, has_public);
	gtk_widget_set_sensitive(public_list_btn, has_public);
	if (has_public)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn),
			FALSE);
}

static void choose_list_cb(const char *text, gpointer data)
{
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(chat_room_name), text);
	gtk_combo_box_set_active(GTK_COMBO_BOX(chat_room_name), 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn), TRUE);
}

static void got_chatroom_list(LList *list, void *data)
{
	if (!list) {
		ay_do_error(_("Cannot list chatrooms"),
			_("No list available."));
		return;
	}

	do_llist_dialog(_("Select a chatroom."), _("Public chatrooms list"),
		list, choose_list_cb, NULL);

	gtk_button_set_label(GTK_BUTTON(data), _("List public chatrooms..."));
	gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);

	l_list_free(list);
}

static void list_public_chatrooms(GtkWidget *widget, gpointer data)
{
	char *service =
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	/*int has_public = 0; */
	eb_local_account *ela = NULL;

	if (!strstr(service, "]") || !strstr(service, " ")) {
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service, "[") + 1;

	service_id = get_service_id(mservice);
	ela = find_local_account_by_handle(local_acc, service_id);
	eb_debug(DBG_CORE, "local_acc: %s, service_id: %d, mservice: %s\n",
		local_acc, service_id, mservice);

	g_free(service);

	if (!ela) {
		ay_do_error(_("Cannot list chatrooms"),
			_("The local account doesn't exist."));
		return;
	}

	gtk_button_set_label(GTK_BUTTON(widget),
		_("Loading List. This may take a while..."));
	gtk_widget_set_sensitive(widget, FALSE);

	RUN_SERVICE(ela)->get_public_chatrooms(ela, got_chatroom_list, widget);
}

/*
 *  Let's build ourselves a nice little dialog window to
 *  ask us what chat window we want to join :)
 */
void open_join_chat_window()
{
	GtkWidget *label, *table, *box, *button;

	if (join_service_is_open)
		return;

	join_service_is_open = 1;

	table = gtk_table_new(2, 5, FALSE);

	label = gtk_label_new(_("Chat Room Name: "));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_widget_show(label);

	/* mru */
	chat_room_name = gtk_combo_box_entry_new_text();
	load_chatroom_mru(GTK_COMBO_BOX(chat_room_name));

	gtk_table_attach_defaults(GTK_TABLE(table), chat_room_name, 1, 2, 0, 1);
	gtk_widget_show(chat_room_name);

	label = gtk_label_new(_("Local account: "));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_widget_show(label);

	chat_room_type = gtk_combo_box_new_text();
	chat_service_list(GTK_COMBO_BOX(chat_room_type));

	gtk_table_attach_defaults(GTK_TABLE(table), chat_room_type, 1, 2, 1, 2);
	gtk_widget_show(chat_room_type);

	reconnect_chkbtn =
		gtk_check_button_new_with_label(_("Reconnect at login"));
	gtk_table_attach_defaults(GTK_TABLE(table), reconnect_chkbtn, 1, 2, 2,
		3);
	gtk_widget_show(reconnect_chkbtn);

	public_chkbtn =
		gtk_check_button_new_with_label(_("Chatroom is public"));
	gtk_table_attach_defaults(GTK_TABLE(table), public_chkbtn, 1, 2, 3, 4);
	gtk_widget_set_sensitive(public_chkbtn, FALSE);
	gtk_widget_show(public_chkbtn);

	public_list_btn = gtk_button_new_with_label(_("List public chatrooms..."));
	gtk_table_attach_defaults(GTK_TABLE(table), public_list_btn, 1, 2, 4,
				  5);
	gtk_widget_set_sensitive(public_list_btn, FALSE);
	gtk_widget_show(public_list_btn);

	join_chat_window = gtk_dialog_new_with_buttons(_("Join a Chat Room"),
							GTK_WINDOW(statuswindow),
							GTK_DIALOG_MODAL | 
							GTK_DIALOG_DESTROY_WITH_PARENT,
							NULL);

	gtk_window_set_resizable(GTK_WINDOW(join_chat_window), FALSE);

	box = gtk_dialog_get_content_area(GTK_DIALOG(join_chat_window));
	gtk_box_pack_start(GTK_BOX(box), table, TRUE, TRUE, 0);

	button = gtkut_stock_button_new_with_label(_("Join"), GTK_STOCK_OK);
	gtk_dialog_add_action_widget(GTK_DIALOG(join_chat_window), button,
		GTK_RESPONSE_ACCEPT);

	button = gtkut_stock_button_new_with_label(_("Cancel"), GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget(GTK_DIALOG(join_chat_window), button,
		GTK_RESPONSE_REJECT);

	g_signal_connect(join_chat_window, "response",
		G_CALLBACK(join_chat_callback), NULL);

	g_signal_connect(public_list_btn, "clicked",
		G_CALLBACK(list_public_chatrooms), NULL);

	g_signal_connect(GTK_COMBO_BOX(chat_room_type), "changed",
		G_CALLBACK(update_public_sensitivity), NULL);

	gtk_widget_show_all (join_chat_window);

	gtk_widget_grab_focus(chat_room_name);
}
