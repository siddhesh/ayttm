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
#include "dialog.h"
#include "globals.h"
#include "messages.h"
#include "service.h"
#include "gtk/gtkutils.h"

#include "pixmaps/tb_preferences.xpm"
#include "pixmaps/cancel.xpm"


static gint window_open = 0;
static eb_account *account = NULL;
static GtkWidget *edit_account_window = NULL;
static GtkWidget *nick = NULL;
static GtkWidget *laccount = NULL;
static GtkWidget *group = NULL;

#define COMBO_TEXT(x) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(x)->entry))

static void destroy(GtkWidget *widget, gpointer data)
{
	window_open = 0;
}

static void ok_callback(GtkWidget *widget, gpointer data)
{
	grouplist *gl;
	struct contact *con;
	gchar *service = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(laccount)->entry),0,-1);
	gint service_id = -1;
	gchar *local_acc = strstr(service, " ") +1;
	eb_local_account *ela = NULL;
	
	if (strcmp(service, _("[None]")) && strstr(service, "]")) {
		*(strstr(service, "]")) = '\0';
		service = strstr(service,"[")+1;

		service_id = get_service_id( service );
		ela = find_local_account_by_handle(local_acc, service_id);
		if (!ela) {
			ay_do_error(_("Account error"), _("The local account doesn't exist."));
			g_free(service);
			return;			
		}
		account->ela = ela;
		account->service_id = service_id;
	} else if (!strcmp(service, _("[None]"))) {
		account->ela = NULL; /* let people keep their accounts even if they 
					can't message them. Maybe they'll readd the 
					local account later. */
	} else {
		ay_do_error(_("Account error"), _("The local account doesn't exist."));
		g_free(service);
		return;
	}
	
	g_free(service);
	
	gl = find_grouplist_by_name(COMBO_TEXT(group));
	if(!gl) {
		add_group(COMBO_TEXT(group));
		gl = find_grouplist_by_name(COMBO_TEXT(group));
	}

	con = find_contact_in_group_by_nick(COMBO_TEXT(nick), gl);
	if(!con) {
		con = add_new_contact(COMBO_TEXT(group), COMBO_TEXT(nick), account->service_id);
	}

	if(!account->account_contact)
		add_account(con->nick, account);
	else
		move_account(con, account);

	if (l_list_empty(con->accounts))
		remove_contact(con);
		
	update_contact_list();
	write_contact_list();
	gtk_widget_destroy(edit_account_window);
}

static gint strcasecmp_glist(gconstpointer a, gconstpointer b)
{
	return strcasecmp((const char *)a, (const char *)b);
}

static LList * get_contacts(gchar * group)
{
	LList * node = NULL, *newlist = NULL;
	grouplist * g;

	g = find_grouplist_by_name(group);

	if(g)
		node = g->members;

	while(node) {
		newlist = l_list_insert_sorted(newlist, ((struct contact *)node->data)->nick, strcasecmp_glist);
		node = node->next;
	}

	return newlist;
}


static void  group_changed(GtkEditable *editable, gpointer user_data)
{
	GList * list = llist_to_glist(get_contacts(COMBO_TEXT(group)), 1);
	gtk_combo_set_popdown_strings(GTK_COMBO(nick), list );
	g_list_free(list);

	if(account->account_contact)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(nick)->entry), 
				account->account_contact->nick);
	else
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(nick)->entry), 
				account->handle);
}

static void draw_edit_account_window(eb_account *ea, char *window_title, char *frame_title, char *add_label)
{
	gchar buff[1024];
	static GtkWidget *frame;
	char *cur_la = NULL;
	account = ea;

	if(!window_open) {
		GtkWidget *vbox = NULL;
		GtkWidget *hbox = NULL;
		GtkWidget *hbox2 = NULL;
		GtkWidget *label = NULL;
		GtkWidget *button = NULL;
		GtkWidget *table = NULL;
		GtkWidget *separator = NULL;
		GList *list = NULL;
		LList *walk = NULL;
		
		edit_account_window = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_position(GTK_WINDOW(edit_account_window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(edit_account_window);
		gtk_container_set_border_width(GTK_CONTAINER(edit_account_window), 5);

		table = gtk_table_new(3, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		hbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);

		/*Entry for Contact Name*/

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Contact Name: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		nick = gtk_combo_new();
		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), nick, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(nick);

		/*Entry for Group Name*/

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Group Name: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		group = gtk_combo_new();
		list = llist_to_glist(get_groups(), 1);
		gtk_combo_set_popdown_strings(GTK_COMBO(group), list);

		g_list_free(list);
		gtk_signal_connect(GTK_OBJECT(GTK_COMBO(group)->entry), "changed",
				GTK_SIGNAL_FUNC(group_changed), NULL);
		gtk_table_attach(GTK_TABLE(table), group, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(group);

		/*Entry for Local account */

		hbox = gtk_hbox_new(FALSE, 5);

		label = gtk_label_new(_("Local account: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		laccount = gtk_combo_new();
		list = NULL;
		for (walk = accounts; walk; walk = walk->next) {
			eb_local_account *ela = (eb_local_account *)walk->data;
			if (ela) {
				char *str = g_strdup_printf("[%s] %s", get_service_name(ela->service_id), ela->handle);
				list = g_list_insert_sorted(list, str, strcasecmp_glist);
				
				if (ela == ea->ela)
					cur_la = strdup(str);
			}
		}
		if (cur_la == NULL) 
			list = g_list_prepend(list, _("[None]"));
		gtk_combo_set_popdown_strings(GTK_COMBO(laccount), list);
		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), laccount, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(laccount);

		/*Lets create the frame to put this in*/

		frame = gtk_frame_new(NULL);

		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show(frame);

		hbox = gtk_hbox_new(FALSE, 5);
		hbox2 = gtk_hbox_new(TRUE, 5);
		gtk_widget_set_usize(hbox2, 200,25 );

		/*Lets try adding a seperator*/

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);

		/*Add Button*/

		button = do_icon_button(add_label,tb_preferences_xpm, edit_account_window);
		
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(ok_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/*Cancel Button*/

		button = do_icon_button(_("Cancel"),cancel_xpm, edit_account_window);
		
		gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(gtk_widget_destroy),
				GTK_OBJECT(edit_account_window));

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/*Buttons End*/

		hbox = gtk_hbox_new(FALSE, 0);

		gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
		gtk_widget_show(hbox2);

		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		gtk_widget_show(vbox);

		gtk_container_add(GTK_CONTAINER (edit_account_window), vbox);

		gtk_signal_connect(GTK_OBJECT(edit_account_window), "destroy",
				GTK_SIGNAL_FUNC(destroy), NULL);
	}

	if(account->account_contact) {
		if(strncmp(account->account_contact->group->name, "__Ayttm_Dummy_Group__",
			   strlen("__Ayttm_Dummy_Group__"))) {
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group)->entry), 
				account->account_contact->group->name);
		}
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(nick)->entry), 
				account->account_contact->nick);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group)->entry), 
				_("Unknown"));
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(nick)->entry), 
				account->handle);
	}
	
	if (cur_la) {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(laccount)->entry), 
				cur_la);
		free(cur_la);	
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(laccount)->entry),
				_("[None]"));
	}
	
	gtk_frame_set_label(GTK_FRAME(frame), frame_title);

	g_snprintf(buff,1024,_("%s - Account edition"), account->handle);
	gtk_window_set_title(GTK_WINDOW(edit_account_window), buff ); 
	gtkut_set_window_icon(edit_account_window->window, NULL);
	
	gtk_widget_show(edit_account_window);

	window_open = 1;
}

void add_unknown_account_window_new(eb_account *ea)
{
	draw_edit_account_window(ea, _("Add %s to Contact List"), _("Add Unknown Contact"), _("Add"));
}

void edit_account_window_new(eb_account *ea)
{
	if(!strcmp(ea->account_contact->group->name, _("Unknown")))
		add_unknown_account_window_new(ea);
	else
		draw_edit_account_window(ea, _("Edit %s"), _("Edit Account"), _("Save"));
}

