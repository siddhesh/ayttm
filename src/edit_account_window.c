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

#include <stdlib.h>
#include <string.h>

#include "status.h"
#include "util.h"
#include "gtk_globals.h"
#include "messages.h"
#include "service.h"
#include "chat_window.h"

static gint window_open = 0;
static eb_account *account = NULL;
static GtkWidget *edit_account_window = NULL;
static GtkWidget *nick = NULL;
static GtkWidget *laccount = NULL;
static GtkWidget *group = NULL;

#define COMBO_TEXT(x) gtk_entry_get_text(GTK_ENTRY(GTK_BIN(x)->child))

static void ok_callback(void)
{
	grouplist *gl;
	struct contact *con;
	gchar *service =
		gtk_editable_get_chars(GTK_EDITABLE(GTK_BIN(laccount)->child),
		0, -1);
	gint service_id = -1;
	gchar *local_acc = strstr(service, " ") + 1;
	eb_local_account *ela = NULL;
	int reshow_chatwindow = FALSE;

	if (account && account->account_contact
		&& account->account_contact->conversation)
		reshow_chatwindow = TRUE;

	if (strcmp(service, _("[None]")) && strstr(service, "]")) {
		char *mservice = NULL;
		*(strstr(service, "]")) = '\0';
		mservice = strstr(service, "[") + 1;

		service_id = get_service_id(mservice);
		ela = find_local_account_by_handle(local_acc, service_id);
		if (!ela) {
			ay_do_error(_("Account error"),
				_("The local account doesn't exist."));
			g_free(service);
			return;
		}
		account->ela = ela;
		account->service_id = service_id;
	} else if (!strcmp(service, _("[None]"))) {
		account->ela = NULL;	/* let people keep their accounts even if they 
					   can't message them. Maybe they'll readd the 
					   local account later. */
	} else {
		ay_do_error(_("Account error"),
			_("The local account doesn't exist."));
		g_free(service);
		return;
	}

	g_free(service);

	gl = find_grouplist_by_name(COMBO_TEXT(group));
	if (!gl) {
		add_group(COMBO_TEXT(group));
		gl = find_grouplist_by_name(COMBO_TEXT(group));
	}

	con = find_contact_in_group_by_nick(COMBO_TEXT(nick), gl);
	if (!con) {
		con = add_new_contact(COMBO_TEXT(group), COMBO_TEXT(nick),
			account->service_id);
	}

	if (con && con->conversation) {
		reshow_chatwindow = TRUE;
	}
	if (!account->account_contact)
		add_account(con->nick, account);
	else
		move_account(con, account);

	if (l_list_empty(con->accounts))
		remove_contact(con);

	update_contact_list();
	write_contact_list();
	gtk_widget_destroy(edit_account_window);
	if (reshow_chatwindow) {
		ay_conversation_chat_with_contact(con);
	}
}

static gint strcasecmp_glist(gconstpointer a, gconstpointer b)
{
	return strcasecmp((const char *)a, (const char *)b);
}

static LList *get_contacts(const gchar *group)
{
	LList *node = NULL, *newlist = NULL;
	grouplist *g;

	g = find_grouplist_by_name(group);

	if (g)
		node = g->members;

	while (node) {
		newlist =
			l_list_insert_sorted(newlist,
			((struct contact *)node->data)->nick, strcasecmp_glist);
		node = node->next;
	}

	return newlist;
}

static void group_changed(GtkEditable *editable, gpointer user_data)
{
	GList *list = llist_to_glist(get_contacts(COMBO_TEXT(group)), 1);
	GList *gwalker;
	for (gwalker = list; gwalker; gwalker = g_list_next(gwalker))
		gtk_combo_box_append_text(GTK_COMBO_BOX(nick), gwalker->data);
	g_list_free(list);

	if (account->account_contact)
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(nick)->child),
			account->account_contact->nick);
	else
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(nick)->child),
			account->handle);
}

static void draw_edit_account_window(eb_account *ea, char *window_title,
	char *frame_title, gboolean add)
{
	gchar buff[1024];
	static GtkWidget *frame;
	GtkWidget *dialog_content_area;
	char *cur_la = NULL;
	account = ea;

	if (!window_open) {
		GtkWidget *vbox = NULL;
		GtkWidget *hbox = NULL;
		GtkWidget *label = NULL;
		GtkWidget *table = NULL;
		GList *list = NULL;
		LList *walk = NULL;
		GList *gwalker;

		table = gtk_table_new(3, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		hbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);

		/*Entry for Contact Name */

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Contact Name: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL,
			GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		nick = gtk_combo_box_entry_new_text();
		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), nick, 1, 2, 0, 1, GTK_FILL,
			GTK_FILL, 0, 0);
		gtk_widget_show(nick);

		/*Entry for Group Name */

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Group Name: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL,
			GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		group = gtk_combo_box_entry_new_text();
		list = llist_to_glist(get_groups(), 1);

		for (gwalker = list; gwalker; gwalker = g_list_next(gwalker))
			gtk_combo_box_append_text(GTK_COMBO_BOX(group),
				gwalker->data);

		g_list_free(list);
		g_signal_connect(group, "changed", G_CALLBACK(group_changed),
			NULL);
		gtk_table_attach(GTK_TABLE(table), group, 1, 2, 1, 2, GTK_FILL,
			GTK_FILL, 0, 0);
		gtk_widget_show(group);

		/*Entry for Local account */

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Local account: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL,
			GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		laccount = gtk_combo_box_entry_new_text();
		list = NULL;
		for (walk = accounts; walk; walk = walk->next) {
			eb_local_account *ela = (eb_local_account *)walk->data;
			if (ela) {
				char *str =
					g_strdup_printf("[%s] %s",
					get_service_name(ela->service_id),
					ela->handle);
				gtk_combo_box_append_text(GTK_COMBO_BOX
					(laccount), str);
				list = g_list_insert_sorted(list, str,
					strcasecmp_glist);

				if (ela == ea->ela)
					cur_la = strdup(str);
			}
		}
		if (cur_la == NULL) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(laccount),
				_("[None]"));
			list = g_list_prepend(list, _("[None]"));
		}
		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), laccount, 1, 2, 2, 3,
			GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(laccount);

		/*Lets create the frame to put this in */

		frame = gtk_frame_new(NULL);

		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		edit_account_window = gtk_dialog_new_with_buttons("Ayttm",
			GTK_WINDOW(statuswindow),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			add ? GTK_STOCK_ADD : GTK_STOCK_SAVE,
			GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(edit_account_window),
			GTK_RESPONSE_ACCEPT);

#ifdef HAVE_GTK_2_14
		dialog_content_area =
			gtk_dialog_get_content_area(GTK_DIALOG
			(edit_account_window));
#else
		dialog_content_area = GTK_DIALOG(edit_account_window)->vbox;
#endif

		gtk_box_pack_start(GTK_BOX(dialog_content_area), frame, TRUE,
			TRUE, 5);
		window_open = 1;
	}

	if (account->account_contact) {
		if (strncmp(account->account_contact->group->name,
				"__Ayttm_Dummy_Group__",
				strlen("__Ayttm_Dummy_Group__"))) {
			gtk_entry_set_text(GTK_ENTRY(GTK_BIN(group)->child),
				account->account_contact->group->name);
		}
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(nick)->child),
			account->account_contact->nick);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(group)->child),
			_("Unknown"));
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(nick)->child),
			account->handle);
	}

	if (cur_la) {
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(laccount)->child), cur_la);
		free(cur_la);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(laccount)->child),
			_("[None]"));
	}

	gtk_frame_set_label(GTK_FRAME(frame), frame_title);

	g_snprintf(buff, 1024, _("%s - Account edition"), account->handle);
	gtk_window_set_title(GTK_WINDOW(edit_account_window), buff);

	gtk_widget_show_all(edit_account_window);

	if (GTK_RESPONSE_ACCEPT ==
		gtk_dialog_run(GTK_DIALOG(edit_account_window)))
		ok_callback();

	gtk_widget_destroy(edit_account_window);
	window_open = 0;

}

void add_unknown_account_window_new(eb_account *ea)
{
	draw_edit_account_window(ea, _("Add %s to Contact List"),
		_("Add Unknown Contact"), TRUE);
}

void edit_account_window_new(eb_account *ea)
{
	if (!strcmp(ea->account_contact->group->name, _("Unknown")))
		add_unknown_account_window_new(ea);
	else
		draw_edit_account_window(ea, _("Edit %s"), _("Edit Account"),
			FALSE);
}
