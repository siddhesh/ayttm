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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>

#include "util.h"
#include "gtk_globals.h"
#include "sound.h"
#include "prefs.h"
#include "smileys.h"
#include "service.h"
#include "add_contact_window.h"
#include "action.h"
#include "messages.h"
#include "mem_util.h"
#include "chat_window.h"
#include "dialog.h"
#include "gtk/html_text_buffer.h"
#include "gtk/gtkutils.h"

#ifdef HAVE_LIBPSPELL
#include "gtk/gtkspell.h"
#endif

#include "pixmaps/tb_mail_send.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/tb_volume.xpm"
#include "pixmaps/smiley_button.xpm"
#include "pixmaps/action.xpm"
#include "pixmaps/invite_btn.xpm"
#include "pixmaps/reconnect.xpm"

#include "auto_complete.h"

#define GET_CHAT_ROOM(cur_cw) {\
	if (iGetLocalPref("do_tabbed_chat") ) { \
		eb_chat_room *bck = cur_cw; \
		if (cur_cw && cur_cw->notebook) \
			cur_cw = find_tabbed_chat_room_index(gtk_notebook_get_current_page(GTK_NOTEBOOK(cur_cw->notebook))); \
		if (cur_cw == NULL) \
			cur_cw = bck; \
	} \
}

#define ENTRY_FOCUS(x) { eb_chat_room *x2 = x; \
			 GET_CHAT_ROOM(x2); \
			 gtk_widget_grab_focus(x2->entry); \
}

LList *chat_rooms = NULL;

static void get_contacts(eb_chat_room *room);
static gboolean handle_focus(GtkWidget *widget, GdkEventFocus *event, gpointer userdata);
static void eb_chat_room_update_window_title(eb_chat_room *ecb, gboolean new_message);

static void init_chat_room_log_file(eb_chat_room *chat_room);
static void	destroy_chat_room_log_file(eb_chat_room *chat_room);

/* Not used yet */
/*
static const char *cr_colors[]={
	"#ff0055",
	"#ff009d",
	"#6e00ff",
	"#0072ff",
	"#00ffaa",
	"#21ff00",
	"#9dff00",
	"#ffff00",
	"#ff9400"
};
*/
static const int nb_cr_colors = 9;

static void	free_chat_room(eb_chat_room *chat_room)
{
	LList *history = NULL;

	if (!chat_room)
		return;

	chat_rooms = l_list_remove(chat_rooms, chat_room);
	destroy_chat_room_log_file(chat_room);
	history = chat_room->history;

	for (; history; history = history->next)
		free(history->data);

	l_list_free(chat_room->history);
	g_free(chat_room);
}

static void handle_fellow_click(char *name, eb_chat_room *cr)
{
	eb_account *ea = NULL;
	LList *node = NULL;
	char *handle = NULL;

	for (node = cr->fellows; node; node = node->next)
	{
		eb_chat_room_buddy *ecrb = node->data;
		if (!strcasecmp(ecrb->alias, name)) {
			handle = ecrb->handle;
			break;
		}
	}

	if (find_account_by_handle(name, cr->local_user->service_id))
		ea = find_account_by_handle(name, cr->local_user->service_id);
	else if (handle && find_account_by_handle(handle, cr->local_user->service_id))
		ea = find_account_by_handle(handle, cr->local_user->service_id);
	else if (find_contact_by_nick(name)) {
		eb_chat_window_display_contact(find_contact_by_nick(name));
		return;
	} else {
		ea = RUN_SERVICE(cr->local_user)->new_account(cr->local_user, name);
		add_unknown_account_window_new(ea);
		return; /* as add_unknown_account_window_new isn't modal
			we'll open the chat_window next time */
	}
	eb_chat_window_display_account(ea);
}

static void fellows_activated(GtkTreeView *tree_view, GtkTreePath *path,
		GtkTreeViewColumn *column, gpointer data)
{
	eb_chat_room *ecr = (eb_chat_room *)data;
	char *name = NULL;

	GtkTreeIter iter;

	eb_debug(DBG_CORE, "activated_callback\n");

	gtk_tree_model_get_iter(GTK_TREE_MODEL(ecr->fellows_model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(ecr->fellows_model), &iter, 0, &name, -1);

	handle_fellow_click(name, ecr);
}

static void send_cr_message(GtkWidget *widget, gpointer data)
{
	GtkTextIter start, end;
	char *text;
#ifdef __MINGW32__
	char *recoded;
#endif

	eb_chat_room *ecr = (eb_chat_room *)data;
	GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(ecr->entry));

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	if (!text || !strlen(text))
		return;

#ifdef __MINGW32__
	recoded = ay_utf8_to_str(text);
	g_free(text);
	text = recoded;
#endif

	RUN_SERVICE(ecr->local_user)->send_chat_room_message(ecr, text);

	if (ecr->this_msg_in_history) {
		LList *node = NULL, *node2 = NULL;

		for (node = ecr->history; node; node = node->next)
			node2 = node;

		free(node2->data);
		node2->data = strdup(text);
		ecr->this_msg_in_history = 0;
	} else {
		ecr->history = l_list_append(ecr->history, strdup(text));
		ecr->hist_pos = NULL;
	}

	g_free(text);
	if (ecr->sound_enabled && ecr->send_enabled)
		play_sound(SOUND_SEND);
	gtk_text_buffer_delete(buffer, &start, &end);
}

static void send_typing_status(eb_chat_room *cr)
{
	time_t now = time(NULL);
	/* LList *others; */

	if (!iGetLocalPref("do_send_typing_notify"))
		return;

	if (now >= cr->next_typing_send &&
	   RUN_SERVICE(cr->local_user)->send_cr_typing != NULL)
		cr->next_typing_send = now + RUN_SERVICE(cr->local_user)->send_cr_typing(cr);
}

int eb_chat_room_notebook_switch(void *notebook, int page_num)
{
	LList *w = NULL;
	GtkWidget *label = NULL;

	for (w = chat_rooms; w; w = w->next) {
		eb_chat_room *cr = (eb_chat_room *)w->data;
		if (cr->notebook_page == page_num) {
			if (cr->is_child_red) {
				eb_debug(DBG_CORE, "Setting tab to normal\n");
				label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(cr->notebook), cr->notebook_child);
				gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, NULL);
				cr->is_child_red = FALSE;
			}

			ENTRY_FOCUS(cr);
			eb_chat_room_update_window_title(cr, FALSE);

			return FALSE;
		}
	}

	return TRUE;
}

extern void chat_history_up(chat_window *cw);
extern void chat_history_down(chat_window *cw);
extern void chat_scroll(chat_window *cw, GdkEventKey *event);

static gboolean cr_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	eb_chat_room *cr = (eb_chat_room *)data;
	GdkModifierType modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

	static gboolean complete_mode = FALSE;

	if (!iGetLocalPref("do_multi_line"))
		return FALSE;

	if (event->keyval == GDK_Return) {
		/* Just print a newline on Shift-Return */
		if (event->state & GDK_SHIFT_MASK)
			event->state = 0;
		else if (iGetLocalPref("do_enter_send")) {
			gtk_text_buffer_delete_selection(
					gtk_text_view_get_buffer(GTK_TEXT_VIEW(cr->entry)), FALSE, TRUE);

			chat_auto_complete_insert(cr->entry, event);

			/*Prevents a newline from being printed*/
			g_signal_stop_emission_by_name(G_OBJECT(widget), "key-press-event");

			send_cr_message(NULL, cr);
			return TRUE;
		}
	}
	else if (event->keyval == GDK_Up && (!modifiers))
		chat_history_up(cr);
	else if (event->keyval == GDK_Down && (!modifiers))
		chat_history_down(cr);
	else if (event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down)
		chat_scroll(cr, event);

	else if (iGetLocalPref("do_auto_complete")) {
		if (event->keyval == GDK_space || ispunct(event->keyval)) {
			eb_debug(DBG_CORE, "AUTO COMPLETE INSERT\n");
			chat_auto_complete_insert(cr->entry, event);
			complete_mode = FALSE;
		}
		else if (event->keyval == GDK_Tab) {
			eb_debug(DBG_CORE, "AUTO COMPLETE VALIDATE\n");
			chat_auto_complete_validate(cr->entry);
			complete_mode = FALSE;
			return TRUE;
		}
		else if ((event->keyval == GDK_Right || event->keyval == GDK_Left) && !modifiers) {
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(cr->entry));
			GtkTextIter start, end;

			if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end) && complete_mode) {
				complete_mode = FALSE;
				gtk_text_buffer_select_range(buffer, &start, &start);
				return TRUE;
			}
			else {
				complete_mode = FALSE;
				return FALSE;
			}
		}
		else if	((event->keyval >= GDK_a && event->keyval <= GDK_z)
			  || (event->keyval >= GDK_A && event->keyval <= GDK_Z))
		{
			eb_debug(DBG_CORE, "AUTO COMPLETE\n");
			complete_mode = TRUE;

			if (!chat_auto_complete(cr->entry, auto_complete_session_words, event))
				return chat_auto_complete(cr->entry, cr->fellows, event);

			return TRUE;
		}
	}

	/* check tab changes if this is a tabbed chat window */
	if (cr->notebook && check_tab_accelerators(widget, cr, modifiers, event))
		return TRUE;

	if (!modifiers)
		send_typing_status(cr);

	return FALSE;
}

static void set_sound_on_toggle(GtkWidget *sound_button, gpointer userdata)
{
	eb_chat_room *cr = (eb_chat_room *)userdata;

	/* Set the sound_enable variable depending on the toggle button */
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON (sound_button)))
		cr->sound_enabled = TRUE;
	else
		cr->sound_enabled = FALSE;
}

void start_auto_chatrooms(eb_local_account *ela)
{
	FILE *fp;
	char buff [4096];
	snprintf(buff, 4095, "%s%cchatroom_autoconnect",
				config_dir,
				G_DIR_SEPARATOR);
	if (!ela)
		return;

	eb_debug(DBG_CORE, "buff %s\n", buff);

	if (!(fp = fopen(buff, "r"))) {
		eb_debug(DBG_CORE, "Could not open file %s\n", buff);
		return;
	}

	while (fgets(buff, sizeof(buff), fp)) {
		char **tokens = ay_strsplit(buff, "\t", -1);
		if (!strcmp(tokens[0], get_service_name(ela->service_id))
		&&  !strcmp(tokens[1], ela->handle))
			eb_start_chat_room(ela, tokens[2], atoi(tokens[3]));
		ay_strfreev(tokens);
	}
	if (fp)
		fclose(fp);
}

static int eb_is_chatroom_auto(eb_chat_room *cr)
{
	FILE *fp;
	char buff [4096];
	snprintf(buff, 4095, "%s%cchatroom_autoconnect",
				config_dir,
				G_DIR_SEPARATOR);
	if (!cr->local_user || !cr->room_name)
		return FALSE;

	eb_debug(DBG_CORE, "buff %s\n",buff);

	if (!(fp = fopen(buff, "r"))) {
		eb_debug(DBG_CORE, "Could not open file %s\n", buff);
		return FALSE;
	}

	while (fgets(buff, sizeof(buff), fp)) {
		char **tokens = ay_strsplit(buff, "\t", -1);
		if (!strcmp(tokens[0], get_service_name(cr->local_user->service_id))
		&&  !strcmp(tokens[1], cr->local_user->handle)
		&&  !strcmp(tokens[2], cr->room_name)
		&&  atoi(tokens[3]) == cr->is_public) {
			fclose(fp);
			ay_strfreev(tokens);
			return TRUE; /* already in */
		}
		ay_strfreev(tokens);
	}
	if (fp)
		fclose(fp);

	return FALSE;
}

static void eb_add_auto_chatroom(eb_local_account *ela, const char *name, int public)
{
	FILE *fp;
	char buff[4096];
	snprintf(buff, 4095, "%s%cchatroom_autoconnect", config_dir, G_DIR_SEPARATOR);

	if (!ela || !name) 
		return;

	eb_debug(DBG_CORE, "buff %s\n", buff);

	if ( (fp = fopen(buff, "r")) ) {
		while (fgets(buff, sizeof(buff), fp)) {
			char **tokens = ay_strsplit(buff, "\t", -1);
			if (!strcmp(tokens[0], get_service_name(ela->service_id))
			&&  !strcmp(tokens[1], ela->handle)
			&&  !strcmp(tokens[2], name)
			&&  atoi(tokens[3]) == public) {
				fclose(fp);
				ay_strfreev(tokens);
				return; /* already in */
			}
			ay_strfreev(tokens);
		}
		fclose(fp);
	}

	snprintf(buff, 4095, "%s%cchatroom_autoconnect", config_dir, G_DIR_SEPARATOR);
	fp = fopen(buff, "a");
	if (!fp) 
		return;
	fprintf(fp, "%s\t%s\t%s\t%d\t--\n", get_service_name(ela->service_id), ela->handle, name, public);
	fclose(fp);
}

static void eb_remove_auto_chatroom(eb_local_account *ela, const char *name, int public)
{
	FILE *fp, *tmp;
	char buffin[4095], bufftmp[4095], buff[4095];
	snprintf(bufftmp, 4095, "%s%cchatroom_autoconnect.tmp",
				config_dir, 
				G_DIR_SEPARATOR);
	snprintf(buffin, 4095, "%s%cchatroom_autoconnect",
				config_dir,
				G_DIR_SEPARATOR);
	if (!ela || !name)
		return;

	eb_debug(DBG_CORE, "buff %s\n", buff);
	fp = fopen(buffin, "r");
	tmp = fopen(bufftmp, "w");

	if (!tmp) {
		if (fp)
			fclose(fp);
		return;
	}

	while (fp && fgets(buff, sizeof(buff), fp)) {
		char **tokens = ay_strsplit(buff, "\t", -1);
		if (strcmp(tokens[0], get_service_name(ela->service_id))
		||  strcmp(tokens[1], ela->handle)
		||  strcmp(tokens[2], name)
		||  atoi(tokens[3]) != public) {
			fprintf(tmp, "%s", buff);
		}
	}

	fclose(tmp);

	if (fp) {
		fclose(fp);
		unlink(buffin);
	}
	rename(bufftmp, buffin);
}

static void set_reconnect_on_toggle(GtkWidget *reconnect_button, gpointer userdata)
{
	eb_chat_room *cr = (eb_chat_room *)userdata;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(reconnect_button)))
		eb_add_auto_chatroom(cr->local_user, cr->room_name, cr->is_public);
	else
		eb_remove_auto_chatroom(cr->local_user, cr->room_name, cr->is_public);
}

static GList *chat_service_list(GtkComboBox *service_list)
{
	GList *list = NULL;
	LList *walk = NULL;

	for (walk = accounts; walk; walk = walk->next) {
		eb_local_account *ela = (eb_local_account *)walk->data;
		if (ela && ela->connected && can_group_chat(eb_services[ela->service_id])) {
			char *str = g_strdup_printf("[%s] %s", get_service_name(ela->service_id), ela->handle);
			gtk_combo_box_append_text(service_list, str);
		}
	}
	return list;
}

static void invite_callback(GtkWidget *widget, gpointer data)
{
	eb_chat_room *ecr = data;
	char *acc = NULL;
	char *invited = gtk_combo_box_get_active_text(GTK_COMBO_BOX(ecr->invite_buddy));
	if (!invited || !strstr(invited, "(") || !strstr(invited, ")"))
		return;
	if (ecr->preferred) {
		/* this is a chat window */
		eb_account *third = NULL;
		acc = strstr(invited, "(") + 1;
		*strstr(acc, ")") = '\0';
		third = find_account_by_handle(acc, ecr->local_user->service_id);
		if (third) {
			chat_window_to_chat_room(ecr, third, gtk_entry_get_text(GTK_ENTRY(ecr->invite_message)));
			ecr->preferred = NULL;
		}
	} else {
		acc = strstr(invited, "(")+1;
		*strstr(acc, ")") = '\0';
		RUN_SERVICE(ecr->local_user)->send_invite(
					ecr->local_user, ecr,
					acc,
					gtk_entry_get_text(GTK_ENTRY(ecr->invite_message)));
	}
	g_free(invited);
	gtk_widget_destroy(ecr->invite_window);
}

static void destroy_invite(GtkWidget *widget, gpointer data)
{
	eb_chat_room *ecr = data;
	ecr->invite_window_is_open = 0;
}

void do_invite_window(void *widget, eb_chat_room *room)
{
	GtkWidget *box, *box2, *mbox, *label, *table, *frame, *separator;

	if (!room || room->invite_window_is_open)
		return;

	if (!room->local_user && room->preferred)
		room->local_user = room->preferred->ela;
	if (!room->local_user) {
		ay_do_error(_("Chatroom error"), _("Cannot invite a third party until a protocol has been chosen."));
		return;
	}

	room->invite_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_transient_for(GTK_WINDOW(room->invite_window), GTK_WINDOW(room->window));
	gtk_window_set_position(GTK_WINDOW(room->invite_window), GTK_WIN_POS_MOUSE);
	gtk_window_set_title(GTK_WINDOW(room->invite_window), _("Invite a contact"));
	gtk_container_set_border_width(GTK_CONTAINER(room->invite_window), 5);
	gtk_widget_realize(room->invite_window);

	box = gtk_vbox_new(FALSE, 3);
	box2 = gtk_hbox_new(FALSE, 3);
	table = gtk_table_new(2, 2, FALSE);

	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);

	label = gtk_label_new(_("Handle: "));
	mbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(mbox);
	gtk_box_pack_end(GTK_BOX(mbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), mbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);

	room->invite_buddy = gtk_combo_box_new_text();
	get_contacts(room);
	gtk_combo_box_set_active(GTK_COMBO_BOX(room->invite_buddy), -1);
	gtk_widget_show(room->invite_buddy);

	gtk_table_attach(GTK_TABLE(table), room->invite_buddy, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new(_("Message: "));
	mbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(mbox);
	gtk_box_pack_end(GTK_BOX(mbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), mbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);

	room->invite_message = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), room->invite_message, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(room->invite_message);

	frame = gtk_frame_new(NULL);

	gtk_frame_set_label(GTK_FRAME(frame), _("Invite a contact"));
	gtk_container_add(GTK_CONTAINER(frame), table);

	gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), separator, FALSE, FALSE, 5);
	gtk_widget_show(separator);
	gtk_widget_show(frame);
	gtk_widget_show(table);

	label = gtkut_create_icon_button(_("Invite"), invite_btn_xpm, room->invite_window);
	gtk_box_pack_start(GTK_BOX(box2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	g_signal_connect(label, "clicked", G_CALLBACK(invite_callback), room);

	label = gtkut_create_icon_button(_("Cancel"), cancel_xpm, room->invite_window);
	gtk_box_pack_start(GTK_BOX(box2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	g_signal_connect_swapped(label, "clicked", G_CALLBACK(gtk_widget_destroy),
			room->invite_window);
	mbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(mbox), box2, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(box), mbox, FALSE, FALSE, 0);
	gtk_widget_show(mbox);
	gtk_widget_show(box);
	gtk_widget_show(box2);
	gtk_widget_show(table);

	gtk_container_add(GTK_CONTAINER(room->invite_window), box);

	gtk_widget_show(room->invite_window);

	g_signal_connect(room->invite_window, "destroy", G_CALLBACK(destroy_invite), room);
}

static gboolean join_service_is_open = 0;

static GtkWidget *chat_room_name;
static GtkWidget *chat_room_type;
static GtkWidget *join_chat_window;
static GtkWidget *public_chkbtn;
static GtkWidget *public_list_btn;
static GtkWidget *reconnect_chkbtn;

static int total_rooms;

char *next_chatroom_name(void)
{
	return g_strdup_printf(_("Chatroom %d"), ++total_rooms);
}

static LList *get_chatroom_mru(void)
{
	LList *mru = NULL;
	char buff [4096];
	FILE *fp = NULL;

	snprintf(buff, 4095, "%s%cchatroom_mru",
				config_dir,
				G_DIR_SEPARATOR);

	fp = fopen(buff, "r");
	memset(buff, 0, 4096);
	while (fp && fgets(buff, sizeof(buff), fp)) {
		char *name = strdup((char *)buff);
		if (name[strlen(name)-1] == '\n')
			name[strlen(name)-1] = '\0';
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
	char buff [4096];
	FILE *fp = NULL;

	snprintf(buff, 4095, "%s%cchatroom_mru",
				config_dir,
				G_DIR_SEPARATOR);

	fp = fopen(buff, "r");
	memset (buff, 0, 4096);
	while (fp && fgets(buff, sizeof(buff), fp)) {
		char *name = strdup((char *)buff);
		if (name[strlen(name)-1] == '\n')
			name[strlen(name)-1] = '\0';
		gtk_combo_box_append_text(combo, name);

		eb_debug(DBG_CORE, "name='%s'\n",name);
		memset(buff, 0, 4096);
	}
	if (fp)
		fclose(fp);
}

static void add_chatroom_mru(const char *name)
{
	LList *mru = get_chatroom_mru();
	LList *cur = NULL;
	char buff [4096];
	FILE *fp = NULL;
	int i = 0;

	snprintf(buff, 4095, "%s%cchatroom_mru",
				config_dir,
				G_DIR_SEPARATOR);

	fp = fopen(buff, "w");
	memset(buff, 0, 4096);

	mru = l_list_prepend(mru, strdup(name));

	if (fp) {
		for (cur = mru; cur && cur->data && i < 10; cur = cur->next)
			if (!i || strcmp(cur->data, name)) {
				/* not the same twice */
				fprintf(fp, "%s\n", (const char*) cur->data);
				i++;
			}
		fclose(fp);
	}

	l_list_free(mru);
}

static void join_chat_callback(GtkWidget *widget, gpointer data)
{
	char *service = gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *name = NULL;
	char *mservice = NULL;
	char *local_acc = NULL;
	eb_local_account *ela = NULL;
	int service_id = -1;

	name = gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_name));

	if (!service || !strstr(service, "]") || !strstr(service," ")) {
		ay_do_error(_("Cannot join"), _("No local account specified."));
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;

	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[") + 1;

	service_id = get_service_id(mservice);
	eb_debug(DBG_CORE, "local_acc: %s, service_id: %d, mservice: %s\n", local_acc, service_id, mservice);
	ela = find_local_account_by_handle(local_acc, service_id);

	g_free(service);

	if (!ela) {
		ay_do_error(_("Cannot join"), _("The local account doesn't exist."));
		return;
	}

	if (!name || !strlen(name))
		name = next_chatroom_name();

	add_chatroom_mru(name);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reconnect_chkbtn)))
		eb_add_auto_chatroom(ela, name, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(public_chkbtn)));

	eb_start_chat_room(ela, name, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(public_chkbtn)));

	g_free(name);
	gtk_widget_destroy(join_chat_window);
}

static void update_public_sensitivity(GtkWidget *widget, gpointer data) {
	char *service = gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	int has_public = 0;

	if (!service && (!strstr(service, "]") || !strstr(service," "))) {
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[") + 1;

	service_id = get_service_id(mservice);
	g_free(service);

	has_public = (eb_services[service_id].sc->get_public_chatrooms != NULL);
	gtk_widget_set_sensitive(public_chkbtn, has_public);
	gtk_widget_set_sensitive(public_list_btn, has_public);
	if (has_public)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn), FALSE);
}

static void choose_list_cb(const char *text, gpointer data) {
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(chat_room_name), text);
	gtk_combo_box_set_active(GTK_COMBO_BOX(chat_room_name), 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_chkbtn), TRUE);
}

static void got_chatroom_list(LList *list, void *data)
{
	if (!list) {
		ay_do_error(_("Cannot list chatrooms"), _("No list available."));
		return;
	}

	do_llist_dialog(_("Select a chatroom."), _("Public chatrooms list"), list, choose_list_cb, NULL);

	gtk_button_set_label(GTK_BUTTON(data), _("List public chatrooms..."));
	gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);

	l_list_free(list);
}

static void list_public_chatrooms(GtkWidget *widget, gpointer data) {
	char *service = gtk_combo_box_get_active_text(GTK_COMBO_BOX(chat_room_type));
	char *mservice = NULL;
	char *local_acc = NULL;
	int service_id = -1;
	/*int has_public = 0;*/
	eb_local_account *ela = NULL;

	if (!strstr(service, "]") || !strstr(service," ")) {
		g_free(service);
		return;
	}

	local_acc = strstr(service, " ") + 1;
	*(strstr(service, "]")) = '\0';
	mservice = strstr(service,"[") + 1;

	service_id = get_service_id(mservice);
	ela = find_local_account_by_handle(local_acc, service_id);
	eb_debug(DBG_CORE, "local_acc: %s, service_id: %d, mservice: %s\n", local_acc, service_id, mservice);

	g_free(service);

	if (!ela) {
		ay_do_error(_("Cannot list chatrooms"), _("The local account doesn't exist."));
		return;
	}

	gtk_button_set_label(GTK_BUTTON(widget), _("Loading List. This may take a while..."));
	gtk_widget_set_sensitive(widget, FALSE);

	RUN_SERVICE(ela)->get_public_chatrooms(ela, got_chatroom_list, widget);
}

static void join_chat_destroy(GtkWidget *widget, gpointer data)
{
	join_service_is_open = 0;
}

/*
 *  Let's build ourselfs a nice little dialog window to
 *  ask us what chat window we want to join :)
 */
void open_join_chat_window()
{
	GtkWidget *label, *frame, *table, *vbox, *hbox, *hbox2, *button, *separator;

	if (join_service_is_open)
		return;

	join_service_is_open = 1;

	join_chat_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(join_chat_window), GTK_WINDOW(statuswindow));

	gtk_widget_realize(join_chat_window);

	vbox = gtk_vbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(join_chat_window), vbox);

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
	gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_widget_show(label);

	chat_room_type = gtk_combo_box_new_text();
	chat_service_list(GTK_COMBO_BOX(chat_room_type));

	gtk_table_attach_defaults (GTK_TABLE(table), chat_room_type, 1, 2, 1, 2);
	gtk_widget_show(chat_room_type);

	reconnect_chkbtn = gtk_check_button_new_with_label(_("Reconnect at login"));
	gtk_table_attach_defaults (GTK_TABLE(table), reconnect_chkbtn, 1, 2, 2, 3);
	gtk_widget_show(reconnect_chkbtn);

	public_chkbtn = gtk_check_button_new_with_label(_("Chatroom is public"));
	gtk_table_attach_defaults(GTK_TABLE(table), public_chkbtn, 1, 2, 3, 4);
	gtk_widget_set_sensitive(public_chkbtn, FALSE);
	gtk_widget_show(public_chkbtn);

	public_list_btn = gtk_button_new_with_label(_("List public chatrooms..."));
	gtk_table_attach_defaults (GTK_TABLE(table), public_list_btn, 1, 2, 4, 5);
	gtk_widget_set_sensitive(public_list_btn, FALSE);
	gtk_widget_show(public_list_btn);

	/* set up a frame (hopefully) */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Join a Chat Room"));

	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_widget_show(table);

	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);

	/* This is a test to see if I can make some padding action happen */
	gtk_container_set_border_width(GTK_CONTAINER(join_chat_window), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);

	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);

	/* Window resize BAD, Same size GOOD */
	gtk_window_set_resizable (GTK_WINDOW(join_chat_window), FALSE);
	gtk_window_set_position(GTK_WINDOW(join_chat_window), GTK_WIN_POS_MOUSE);

	/* Show the frame and window */
	gtk_widget_show(frame);

	/* add in that nice separator */
	separator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
	gtk_widget_show(separator);

	/* Add in the pretty buttons with pixmaps on them */
	hbox2 = gtk_hbox_new(TRUE, 5);

	gtk_widget_set_size_request(hbox2, 200, 25);

	/* stuff for the join button */
	button = gtkut_create_icon_button(_("Join"), ok_xpm, join_chat_window);

	g_signal_connect(button, "clicked", G_CALLBACK(join_chat_callback), NULL);

	g_signal_connect(public_list_btn, "clicked", G_CALLBACK(list_public_chatrooms), NULL);

	g_signal_connect(GTK_COMBO_BOX(chat_room_type), "changed",
					G_CALLBACK(update_public_sensitivity),
					NULL);

	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	/* stuff for the cancel button */
	button = gtkut_create_icon_button(_("Cancel"), cancel_xpm, join_chat_window);

	g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_widget_destroy),
			join_chat_window);
	gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	/* now put the hbox with all the buttons into the vbox? */
	hbox = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_widget_show(hbox2);
	gtk_widget_show(vbox);

	/* show the window */
	gtk_widget_show(join_chat_window);

	gtk_widget_grab_focus(chat_room_name);

	g_signal_connect(join_chat_window, "destroy", G_CALLBACK(join_chat_destroy), NULL);
}

void destroy_chat_room (GtkWidget *widget, gpointer data)
{
	eb_chat_room *room = (eb_chat_room *)data;

	if (iGetLocalPref("do_tabbed_chat")) {
		GtkWidget *window = room->window;
		GtkWidget *notebook = room->notebook;

		gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), room->notebook_page);

		if (!gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0)) {
			eb_debug(DBG_CORE, "destroying the whole window\n");
			gtk_widget_destroy(window);
			return;
		}
		reassign_tab_pages();
	}
	else
		gtk_widget_destroy(room->window);
}

void eb_destroy_all_chat_rooms(void)
{
	while (chat_rooms && chat_rooms->data)
		destroy_chat_room(NULL, (eb_chat_room *)chat_rooms->data);
}

void eb_destroy_chat_room(eb_chat_room *room)
{
	destroy_chat_room(NULL, room);
}

static void destroy(GtkWidget *widget, gpointer data)
{
	eb_chat_room *ecr = data;

	if (!ecr)
		return;

	g_signal_handlers_disconnect_by_func(ecr->window, G_CALLBACK(handle_focus), ecr);

	while (l_list_find(chat_rooms, data))
		chat_rooms = l_list_remove(chat_rooms, data);

	RUN_SERVICE(ecr->local_user)->leave_chat_room(ecr);

	free_chat_room(ecr);
}

static LList *find_chat_room_buddy(eb_chat_room *room, const gchar *user)
{
	LList *node;
	for (node = room->fellows; node; node = node->next) {
		eb_chat_room_buddy *ecrb = node->data;
		if (!strcmp(user, ecrb->handle))
			return node;
	}
	return NULL;
}

eb_chat_room *find_tabbed_chat_room(void)
{
	LList *w = chat_rooms;
	while (w) {
		eb_chat_room *cr = (eb_chat_room *)w->data;
		if (cr->notebook)
			return cr;
		w = w->next;
	}

	eb_debug(DBG_CORE, "no window found\n");
	return NULL;
}

eb_chat_room *find_tabbed_chat_room_index(int current_page)
{
	LList *l1;
	/*LList *l2;*/
	/*struct contact *c;*/

	eb_chat_room *notebook_window = find_tabbed_chat_room();
	if (!notebook_window || !notebook_window->notebook)
		return NULL;

	if (current_page == -1)
		current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook_window->notebook));

	for (l1 = chat_rooms; l1; l1 = l1->next) {
		eb_chat_room *cr = (eb_chat_room *)l1->data;
		if (notebook_window->notebook_page == current_page)
			return cr;
	}
	return NULL;
}

void eb_chat_room_refresh_list(eb_chat_room *room, const char *buddy, ChatRoomRefreshType refresh)
{
	GtkTreeIter insert;

	eb_debug(DBG_CORE, "refresh list (%d)%s\n", refresh, buddy);

	if (refresh == CHAT_ROOM_JOIN) {
		gtk_list_store_append(room->fellows_model, &insert);
		gtk_list_store_set(room->fellows_model, &insert, 0, buddy, -1);
	}
	else {
		GtkTreeIter del_iter;

		if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(room->fellows_model), &del_iter)) {
			eb_debug(DBG_CORE, "Nothing in the list yet??: %s\n", buddy);
			return;
		}

		do {
			char *name = NULL;

			gtk_tree_model_get(GTK_TREE_MODEL(room->fellows_model), &del_iter, 0, &name, -1);

			if (buddy && name && !strcmp(name, buddy)) {
				gtk_list_store_remove(room->fellows_model, &del_iter);
				return;
			}
		}
		while (gtk_tree_model_iter_next(GTK_TREE_MODEL(room->fellows_model), &del_iter));
	}
}

gboolean eb_chat_room_buddy_connected(eb_chat_room *room, gchar *user)
{
	return find_chat_room_buddy(room, user) != NULL;
}

static void eb_chat_room_private_log_reference(eb_chat_room *room, const char *alias, const char *handle)
{
	struct contact	*con = find_contact_by_handle(handle);
	const int		buff_size = 1024;
	char			buff[buff_size];
	log_file		*log = NULL;
	int				err = 0;

	if (!con || !strcmp(handle, room->local_user->handle))
		return;

	make_safe_filename(buff, con->nick, con->group->name);
	log = ay_log_file_create(buff);

	err = ay_log_file_open(log, "a");

	if (err) {
		eb_debug(DBG_CORE, "eb_chat_room_private_log_reference: could not open log file [%s]\n", buff);
		ay_log_file_destroy(&log);
		return;
	}

	memset(&buff, 0, buff_size);
	if (!room->logfile)
		init_chat_room_log_file(room);

	g_snprintf(buff, buff_size, "You had a <a href=\"log://%s\">group chat with %s (%s)</a>.\n",
			room->logfile->filename, alias, handle);

	ay_log_file_message(log, "", buff);
	ay_log_file_destroy(&log);
}

void eb_chat_room_buddy_arrive(eb_chat_room *room, const gchar *alias, const gchar *handle)
{
	eb_chat_room_buddy *ecrb = NULL;
	gchar *buf;
	LList *t;
	ecrb = g_new0(eb_chat_room_buddy, 1);
	strncpy(ecrb->alias, alias, sizeof(ecrb->alias));
	strncpy(ecrb->handle, handle, sizeof(ecrb->handle));
	room->total_arrivals++;
	ecrb->color = room->total_arrivals % nb_cr_colors;

	for (t = room->fellows; t && t->data; t = t->next)
		if (!strcasecmp(handle, ((eb_chat_room_buddy *)t->data)->handle))
			return;

	buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s (%s) has joined the chat.</b></body>"), alias, handle);
	eb_chat_room_show_3rdperson(room, buf);

	eb_chat_room_private_log_reference(room, alias, handle);

	g_free(buf);

	room->fellows = l_list_append(room->fellows, ecrb);

	eb_chat_room_refresh_list(room, handle, CHAT_ROOM_JOIN);
}

void eb_chat_room_buddy_chnick(eb_chat_room *room, const gchar *buddy, const gchar *newnick)
{
	GtkTreeIter iter;
	gchar *buf;
	LList *node;
	eb_chat_room_buddy *ecrb;

	if (!buddy || !(node = find_chat_room_buddy(room, buddy)))
		return;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(room->fellows_model), &iter)) {
		eb_debug(DBG_CORE, "Nothing in the list?: %s\n", buddy);
		return;
	}

	ecrb = node->data;

	strncpy(ecrb->alias, newnick, sizeof(ecrb->alias));
	strncpy(ecrb->handle, newnick, sizeof(ecrb->handle));

	buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s is now known as %s</b></body>"), buddy, newnick);
	eb_chat_room_show_3rdperson(room, buf);
	g_free(buf);

	eb_chat_room_private_log_reference(room, newnick, newnick);

	do {
		char *name = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(room->fellows_model), &iter, 0, &name, -1);

		if (name && !strcmp(name, buddy)) {
			gtk_list_store_set(room->fellows_model, &iter, 0, newnick, -1);
			return;
		}
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(room->fellows_model), &iter));
}

void eb_chat_room_buddy_leave(eb_chat_room *room, const gchar *handle)
{
	LList *node = find_chat_room_buddy(room, handle);
	gchar *buf;

	if (node) {
		eb_chat_room_buddy *ecrb = node->data;
	        buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s (%s) has left the chat.</b></body>"),
				ecrb->alias, handle);
	} else
		buf = g_strdup_printf(_("<body bgcolor=#F9E589 width=*><b> %s has left the chat.</b></body>"),
				handle);
        eb_chat_room_show_3rdperson(room, buf);
	g_free(buf);

	if (node && room->fellows) {
		eb_chat_room_buddy *ecrb = node->data;
		eb_account *ea = find_account_by_handle(ecrb->handle, room->local_user->service_id);
		if (ea)
			eb_chat_room_display_status (ea, NULL);
		room->fellows = l_list_remove(room->fellows, ecrb);
		g_free(ecrb);
	}
	eb_chat_room_refresh_list(room, handle, CHAT_ROOM_LEAVE);
}

static gboolean handle_focus(GtkWidget *widget, GdkEventFocus *event, gpointer userdata)
{
	eb_chat_room *cr = (eb_chat_room *)userdata;

	if (iGetLocalPref("do_tabbed_chat")
	&&  cr->notebook_page != gtk_notebook_get_current_page(GTK_NOTEBOOK(cr->notebook)))
		return FALSE;

	eb_chat_room_update_window_title(cr, FALSE);
	if (cr->entry)
		ENTRY_FOCUS(cr);
	return FALSE;

}

static void eb_chat_room_update_window_title(eb_chat_room *ecb, gboolean new_message)
{
	char *room_title;
	if (!ecb || !ecb->local_user)
		return;
	room_title = g_strdup_printf("%s%s [%s]",
			new_message ? "* " : "",
			ecb->room_name,
			get_service_name(ecb->local_user->service_id));
	gtk_window_set_title(GTK_WINDOW(ecb->window), room_title);
	g_free(room_title);
}

static eb_chat_room *find_chatroom_by_ela_and_name(eb_local_account *ela, const char *name, int is_public)
{
	LList *node = NULL;

	for (node = chat_rooms; node && node->data; node = node->next) {
		eb_chat_room *ecr = node->data;
		if (ecr->local_user == ela && !strcmp(ecr->room_name, name)
		&&  ecr->is_public == is_public)
			return ecr;
	}
	return NULL;
}

eb_chat_room *eb_start_chat_room(eb_local_account *ela, gchar *name, int is_public)
{
	eb_chat_room *ecb = NULL;

	eb_debug(DBG_CORE, "Starting chatroom %s(%s)\n", name, (is_public ? "public" : "non-public"));

	if ( (ecb = find_chatroom_by_ela_and_name(ela, name, is_public)) ) {
		/* we have to destroy and recreate it in case we have been disconnected */
		eb_destroy_chat_room(ecb);
		ecb = NULL;
	}
	if (!ela)
		return NULL;

	ecb = RUN_SERVICE(ela)->make_chat_room(name, ela, is_public);

	if (ecb) {
		ecb->is_public = is_public;
		if (!l_list_find(chat_rooms,ecb))
			chat_rooms = l_list_append(chat_rooms, ecb);

		eb_chat_room_update_window_title(ecb, FALSE);
	}

	eb_debug(DBG_CORE, "Started chatroom %s\n", name);
	return ecb;
}

void eb_chat_room_show_3rdperson(eb_chat_room *chat_room, gchar *message)
{
	char *link_message = NULL ;

	link_message = linkify(message);

	html_text_buffer_append(GTK_TEXT_VIEW(chat_room->chat), link_message, HTML_IGNORE_NONE);
	html_text_buffer_append(GTK_TEXT_VIEW(chat_room->chat), "\n", HTML_IGNORE_NONE);
	ay_log_file_message(chat_room->logfile, "", link_message);

	g_free(link_message);
}

void eb_chat_room_show_message(eb_chat_room *chat_room, const gchar *user, const gchar *message)
{
	gchar buff[2048];
	gchar *temp_message, *link_message;
#ifdef __MINGW32__
	char *recoded;
#endif

	if (!message || !strlen(message))
		return;

	if (!strcmp(chat_room->local_user->handle, user)) {
		time_t t;
		struct tm *cur_time;
		char *color;

		color = "#0000ff";

		time(&t);
		cur_time = localtime(&t);
		if (iGetLocalPref("do_convo_timestamp")) {
			char *disp = chat_room->local_user->alias;
			if (!strcmp(disp, ""))
				disp = chat_room->local_user->handle;
				g_snprintf(buff, 2048,
					"<B><FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s: </FONT></B>",
			 		color, cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec, color, disp);
		} else {
			char *disp = chat_room->local_user->alias;
			if (!strcmp(disp, ""))
				disp = chat_room->local_user->handle;
			g_snprintf(buff, 2048, "<FONT COLOR=\"%s\"><B>%s: </B></FONT>", color, disp);
		}
	}
	else {
		LList *walk;
		eb_account *acc = NULL;
		gchar *color = NULL;
		time_t t;
		struct tm *cur_time;

		if (RUN_SERVICE(chat_room->local_user)->get_color)
			color = RUN_SERVICE(chat_room->local_user)->get_color();
		else
			color = "#ff0000";

		time(&t);
		cur_time = localtime(&t);
		if (iGetLocalPref("do_convo_timestamp")) {
				g_snprintf(buff, 2048,
					"<B><FONT COLOR=\"%s\">%d:%.2d:%.2d</FONT> <FONT COLOR=\"%s\">%s: </FONT></B>",
					color, cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec, color, user);
		} else
			g_snprintf(buff, 2048, "<FONT COLOR=\"%s\"><B>%s: </B></FONT>", color, user);

		for (walk = chat_room->typing_fellows; walk && walk->data; walk = walk->next) {
			acc = walk->data;
			if (!strcasecmp(acc->handle, user) ||  !strcasecmp(acc->account_contact->nick, user))
				break;
			else
				acc = NULL;
		}
		if (acc && chat_room->typing_fellows) {
			chat_room->typing_fellows = l_list_remove(chat_room->typing_fellows, acc);
			eb_chat_room_display_status(acc, NULL);
		}
		if (!gtk_window_is_active(GTK_WINDOW(chat_room->window)))
			eb_chat_room_update_window_title(chat_room, TRUE);
		if (iGetLocalPref("do_raise_window"))
			gdk_window_raise(chat_room->window->window);
		if (chat_room->notebook) {
			int current_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(chat_room->notebook));
			if (chat_room->notebook_page != current_num)
				set_tab_red(chat_room);
		}

	}
	if (iGetLocalPref("do_smiley") && RUN_SERVICE(chat_room->local_user)->get_smileys)
	 	temp_message = eb_smilify(message, RUN_SERVICE(chat_room->local_user)->get_smileys(),
				get_service_name(chat_room->local_user->service_id));
	else
		temp_message = g_strdup(message);

	link_message = linkify(temp_message);

	g_free(temp_message);

#ifdef __MINGW32__
	recoded = ay_str_to_utf8(link_message);
	g_free(link_message);
	link_message = recoded;
#endif

	html_text_buffer_append(GTK_TEXT_VIEW(chat_room->chat), buff, HTML_IGNORE_NONE);
	html_text_buffer_append(GTK_TEXT_VIEW(chat_room->chat), link_message,
		(iGetLocalPref("do_ignore_back") ? HTML_IGNORE_BACKGROUND : HTML_IGNORE_NONE) |
		(iGetLocalPref("do_ignore_fore") ? HTML_IGNORE_FOREGROUND : HTML_IGNORE_NONE) |
		(iGetLocalPref("do_ignore_font") ? HTML_IGNORE_FONT : HTML_IGNORE_NONE));
	html_text_buffer_append(GTK_TEXT_VIEW(chat_room->chat), "\n", HTML_IGNORE_NONE);

	ay_log_file_message(chat_room->logfile, buff, link_message);

	g_free(link_message);

	if (chat_room->sound_enabled && strcmp(chat_room->local_user->handle, user))
		if (chat_room->receive_enabled)
			play_sound(SOUND_RECEIVE);
}

static void invite_button_callback(GtkWidget *widget, gpointer data)
{
	do_invite_window((void *)widget, data);
}

static void	destroy_smiley_cb_data(GtkWidget *widget, gpointer data)
{
	smiley_callback_data *scd = data;
	if (!data)
		return;

/* if (scd->c_window->smiley_window != NULL) {
		gtk_widget_destroy(scd->c_window->smiley_window);
		scd->c_window->smiley_window = NULL;
	}
*/
	g_free(scd);
}

static void action_callback(GtkWidget *widget, gpointer d)
{
	eb_chat_room *ecr =(eb_chat_room*)d;
	conversation_action(ecr->logfile, TRUE);
}

static void _show_smileys_cb(GtkWidget *widget, smiley_callback_data *data)
{
	show_smileys_cb(data);
}

void eb_join_chat_room(eb_chat_room *chat_room, int send_join)
{
	GtkWidget *vbox, *vbox2, *hbox, *hbox2, *label, *toolbar, *icon, *iconwid;
	GtkWidget *send_button, *close_button, *print_button, *separator;
	GtkWidget *scrollwindow = gtk_scrolled_window_new(NULL, NULL);
	gboolean enableSoundButton = FALSE;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	char *room_title = NULL;
	GtkWidget *scrollwindow2;

	GList *focus_chain = NULL;

	/* if we are already here, just leave */
	if (chat_room->connected)
		return;

	if (!l_list_find(chat_rooms, chat_room))
		chat_rooms = l_list_append(chat_rooms, chat_room);

	/* otherwise we are going to make ourselves a gui right here */
	vbox = gtk_vbox_new(FALSE, 1);
	hbox = gtk_hpaned_new();

	chat_room->fellows_model = gtk_list_store_new(1, G_TYPE_STRING);
	chat_room->fellows_widget = gtk_tree_view_new_with_model(GTK_TREE_MODEL(chat_room->fellows_model));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Online"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(chat_room->fellows_widget), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(chat_room->fellows_model),
			0, GTK_SORT_ASCENDING);

	gtk_widget_set_size_request(chat_room->fellows_widget, 100, 20);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow), GTK_SHADOW_ETCHED_IN);

	chat_room->chat = gtk_text_view_new();
	gtk_widget_set_size_request(chat_room->chat, 400, 200);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(chat_room->chat), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(chat_room->chat), 5);

	html_text_view_init(GTK_TEXT_VIEW(chat_room->chat), HTML_IGNORE_NONE);
	gtk_container_add(GTK_CONTAINER(scrollwindow), chat_room->chat);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
					   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	g_signal_connect(chat_room->fellows_widget, "row-activated",
					G_CALLBACK(fellows_activated), chat_room);
	/* make ourselves something to type into */

	scrollwindow2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrollwindow2),
				     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(scrollwindow2), GTK_SHADOW_ETCHED_IN);

	chat_room->entry = gtk_text_view_new();

	gtk_widget_set_size_request(chat_room->entry, -1, 50);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(chat_room->entry), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(chat_room->entry), 5);

	gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_room->entry), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_room->entry), GTK_WRAP_WORD_CHAR);

#ifdef HAVE_LIBPSPELL
	if (iGetLocalPref("do_spell_checking"))
		gtkspell_attach(GTK_TEXT_VIEW(chat_room->entry));
#endif

	gtk_container_add(GTK_CONTAINER(scrollwindow2), chat_room->entry);
	gtk_widget_show(scrollwindow2);

	g_signal_connect(chat_room->entry, "key-press-event",
			 G_CALLBACK(cr_key_press),
			 chat_room);

	gtk_paned_pack1(GTK_PANED(hbox), scrollwindow, TRUE, FALSE);
	gtk_widget_show(scrollwindow);

	vbox2 = gtk_vbox_new(FALSE, 5);

	scrollwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow),
					   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwindow), GTK_SHADOW_ETCHED_IN);

	gtk_container_add(GTK_CONTAINER(scrollwindow), chat_room->fellows_widget);
	gtk_widget_show(chat_room->fellows_widget);

	gtk_box_pack_start(GTK_BOX(vbox2), scrollwindow, TRUE, TRUE, 3);
	gtk_widget_show(scrollwindow);

	label = gtk_button_new_with_label(_("Invite User"));
	g_signal_connect(label, "clicked", G_CALLBACK(invite_button_callback), chat_room);

	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 3);
	gtk_widget_show(label);

	gtk_paned_pack2(GTK_PANED(hbox), vbox2, FALSE, FALSE);
	gtk_widget_show(vbox2);

	gtk_paned_pack2(GTK_PANED(hbox), chat_room->fellows_widget, FALSE, FALSE);
	gtk_widget_show(chat_room->chat);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE,TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), scrollwindow2, FALSE, FALSE, 1);
	gtk_widget_show(hbox);
	gtk_widget_show(chat_room->entry);

	focus_chain = g_list_append(focus_chain, chat_room->entry);
	focus_chain = g_list_append(focus_chain, chat_room->chat);
	focus_chain = g_list_append(focus_chain, chat_room->fellows_widget);

	if (chat_room->local_user)
		room_title = g_strdup_printf("%s [%s]", chat_room->room_name,
					get_service_name(chat_room->local_user->service_id));
	else
		room_title = g_strdup_printf("%s", chat_room->room_name);

	layout_chatwindow(chat_room, vbox, room_title);

	gtk_window_set_title(GTK_WINDOW(chat_room->window), room_title);
	g_free(room_title);

	g_signal_connect(chat_room->window, "focus-in-event", G_CALLBACK(handle_focus), chat_room);

	gtk_container_set_focus_chain (GTK_CONTAINER(vbox), focus_chain);

	/* start a toolbar here */
	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox2, 200, 40);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);

#define TOOLBAR_APPEND_SPACE() {\
	separator = GTK_WIDGET(gtk_separator_tool_item_new());\
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), TRUE);\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(separator), -1);\
	gtk_widget_show(separator); }
#define TOOLBAR_APPEND_TOGGLE_BUTTON(tool_btn, txt, tip, icn, cbk, cwx) {\
	tool_btn = GTK_WIDGET(gtk_toggle_tool_button_new());\
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(tool_btn), icn);\
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(tool_btn), txt);\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(tool_btn), -1);\
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(tool_btn), tip);\
	g_signal_connect(tool_btn, "clicked", G_CALLBACK(cbk) ,cwx);\
        gtk_widget_show(tool_btn); }

#define TOOLBAR_APPEND(tool_btn,txt,icn,cbk,cwx) {\
	tool_btn = GTK_WIDGET(gtk_tool_button_new(icn, txt));\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(tool_btn), -1);\
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(tool_btn), txt);\
	g_signal_connect(tool_btn,"clicked", G_CALLBACK(cbk), cwx); \
	gtk_widget_show(tool_btn); }
#define ICON_CREATE(icon,iconwid,xpm) {\
	icon = gdk_pixbuf_new_from_xpm_data((const char **)xpm); \
	iconwid = gtk_image_new_from_pixbuf(icon); \
	gtk_widget_show(iconwid); }

	/* smileys */
	if (iGetLocalPref("do_smiley")) {
		smiley_callback_data *scd = g_new0(smiley_callback_data, 1);
		scd->c_window = chat_room;
		ICON_CREATE(icon, iconwid, smiley_button_xpm);
		TOOLBAR_APPEND(chat_room->smiley_button, _("Insert Smiley"),
				iconwid, _show_smileys_cb, scd);

		g_signal_connect(chat_room->smiley_button, "destroy",
					   G_CALLBACK(destroy_smiley_cb_data), scd);
		/*Create the separator for the toolbar*/
	}

	ICON_CREATE(icon, iconwid, reconnect_xpm);
	TOOLBAR_APPEND_TOGGLE_BUTTON(chat_room->reconnect_button,
			_("Reconnect at login"),
			_("Reconnect at login"),
			iconwid,
			set_reconnect_on_toggle,
			chat_room
			);

	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(chat_room->reconnect_button),
				     eb_is_chatroom_auto(chat_room));

	ICON_CREATE(icon, iconwid, tb_volume_xpm);
	TOOLBAR_APPEND_TOGGLE_BUTTON(chat_room->sound_button,
			_("Sound"),
			_("Enable Sounds"),
			iconwid,
			set_sound_on_toggle,
			chat_room
			);

	/* Toggle the sound button based on preferences */
	if (iGetLocalPref("do_play_send"))
		chat_room->send_enabled = enableSoundButton = TRUE;

	if (iGetLocalPref("do_play_receive"))
		chat_room->receive_enabled = enableSoundButton = TRUE;

	if (iGetLocalPref("do_play_first"))
		chat_room->first_enabled = enableSoundButton = TRUE;

	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(chat_room->sound_button),
				     enableSoundButton);

	TOOLBAR_APPEND_SPACE();
	ICON_CREATE(icon, iconwid, action_xpm);
	TOOLBAR_APPEND(print_button, _("Actions..."), iconwid, action_callback, chat_room);

	ICON_CREATE(icon, iconwid, tb_mail_send_xpm);
	TOOLBAR_APPEND(send_button, _("Send Message"), iconwid, send_cr_message, chat_room);

	ICON_CREATE(icon, iconwid, cancel_xpm);

	TOOLBAR_APPEND(close_button,_("Close"), iconwid, destroy_chat_room, chat_room);

	chat_room->status_label = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(hbox2), chat_room->status_label, FALSE, FALSE, 0);
	gtk_widget_show(chat_room->status_label);
	chat_room->typing_fellows = NULL;

	gtk_box_pack_end(GTK_BOX(hbox2), toolbar, FALSE, FALSE, 0);
	gtk_widget_show(toolbar);

	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 5);
	gtk_widget_show(hbox2);

	gtk_widget_show(vbox);

	gtk_container_set_border_width(GTK_CONTAINER(chat_room->window), 5);

	g_signal_connect(vbox, "destroy", G_CALLBACK(destroy), chat_room);
	gtk_widget_show(chat_room->window);
	gdk_window_raise(chat_room->window->window);

	/*then mark the fact that we have joined that room*/
	chat_room->connected = 1;

#undef TOOLBAR_APPEND
#undef ICON_CREATE
#undef TOOLBAR_APPEND_TOGGLE_BUTTON
#undef TOOLBAR_APPEND_SPACE

	/* actually call the callback :P ... */
	if (!chat_room) {
		eb_debug(DBG_CORE,"!chat_room\n");
		return;
	}
	if (!chat_room->local_user) {
		eb_debug(DBG_CORE,"!chat_room->account\n");
		return;
	}
	if (RUN_SERVICE(chat_room->local_user) == NULL) {
		eb_debug(DBG_CORE,"!RUN_SERVICE(chat_room->local_user)\n");
		return;
	}
	if (RUN_SERVICE(chat_room->local_user)->join_chat_room == NULL) {
		eb_debug(DBG_CORE,"!RUN_SERVICE(chat_room->local_user)->join_chat_room\n");
		return;
	}

	init_chat_room_log_file(chat_room);

	if (send_join)
		RUN_SERVICE(chat_room->local_user)->join_chat_room(chat_room);

	gtk_widget_grab_focus(chat_room->entry);

	if (iGetLocalPref("do_tabbed_chat"))
		gtk_notebook_set_current_page(GTK_NOTEBOOK(chat_room->notebook), chat_room->notebook_page);
}

static void init_chat_room_log_file(eb_chat_room *chat_room)
{
	assert(chat_room != NULL);

	if (!chat_room->logfile) {
		time_t      mytime = time(NULL);
		char        buff[2048];
		char	    tmpnam[128];

		g_snprintf(tmpnam, 128, "cr_log%lu%d", mytime, total_rooms);
		make_safe_filename(buff, tmpnam, NULL);

		chat_room->logfile = ay_log_file_create(buff);

		ay_log_file_open(chat_room->logfile, "a");
	}
}

static void	destroy_chat_room_log_file(eb_chat_room *chat_room)
{
	if (!chat_room || !chat_room->logfile)
		return;

	ay_log_file_destroy(&(chat_room->logfile));
}

static void get_group_contacts(gchar *group, eb_chat_room *room)
{
	LList *node = NULL, *accounts = NULL;
	grouplist *g;

	g = find_grouplist_by_name(group);

	if (g)
		node = g->members;

	while (node) {
		struct contact *contact = (struct contact *)node->data;
		accounts = contact->accounts;
		while (accounts) {
			if (((struct account *)accounts->data)->ela == room->local_user
			&&  ((struct account *)accounts->data)->online) {
				char *buf = g_strdup_printf("%s (%s)", contact->nick,
						((struct account *)accounts->data)->handle);
				gtk_combo_box_append_text(GTK_COMBO_BOX(room->invite_buddy), buf);
			}
			accounts = accounts->next;
		}
		node = node->next;
	}
}

static void get_contacts(eb_chat_room *room)
{
	LList *node = groups;
	while (node) {
		get_group_contacts(node->data, room);
		node = node->next;
	}
}

void eb_chat_room_display_status(eb_account *remote, char *message)
{
	LList *rooms = NULL;
	if (!iGetLocalPref("do_typing_notify"))
		return;

	rooms = find_chatrooms_with_remote_account(remote);
	for (; rooms && rooms->data; rooms = rooms->next) {

		eb_chat_room *ecr = rooms->data;
		gchar *tmp = NULL;
		gchar *typing = NULL, *typing_old = NULL;
		LList *walk2 = NULL;

		if (message && !strcmp(_("typing..."), message)) {
			if (!ecr->typing_fellows || !l_list_find(ecr->typing_fellows, remote))
				ecr->typing_fellows = l_list_append(ecr->typing_fellows, remote);
		} else if (ecr->typing_fellows) {
			ecr->typing_fellows = l_list_remove(ecr->typing_fellows, remote);
		}

		for (walk2 = ecr->typing_fellows; walk2 && walk2->data; walk2 = walk2->next) {
			eb_account *acc = walk2->data;
			typing_old = typing;
			typing = g_strdup_printf("%s%s%s",
					acc->account_contact->nick,
					typing_old ? ", " : "",
					typing_old ? typing_old : "");
			g_free(typing_old);
		}

		if (typing != NULL && strlen(typing) > 0)
			tmp = g_strdup_printf("%s: %s", typing, _("typing..."));
		else
			tmp = g_strdup_printf(" ");

		if (ecr && ecr->status_label)
			gtk_label_set_text(GTK_LABEL(ecr->status_label), tmp);
		g_free(tmp);
	}
}
