/*
* Ayttm 
*
* Copyright (C) 1999, Torrey Searle <tsearle@uci.edu>
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

#include "service.h"
#include "status.h"
#include "util.h"
#include "add_contact_window.h"
#include "messages.h"
#include "dialog.h"

#include "gtk/gtkutils.h"

#include "pixmaps/tb_preferences.xpm"
#include "pixmaps/cancel.xpm"

/*
* This is the GUI that gets created when you click on the "Add" button
* on the edit contacts tab, this is to add a new contact to your contact
* list
*/

static gint window_open = 0;
static GtkWidget *add_contact_window;
static GtkWidget *service_list;
static GtkWidget *account_name;
static GtkWidget *contact_name;
static GtkWidget *group_name;
static int flag;
static int contact_input_handler;

#define COMBO_TEXT(x) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(x)->entry))

static void destroy(GtkWidget *widget, gpointer data)
{
	window_open = 0;
}

static gint strcasecmp_glist(gconstpointer a, gconstpointer b)
{
	return strcasecmp((const char *)a, (const char *)b);
}

/*
* This function just gets a linked list of all contacts associated, this 
* with a particular group this we use it to populate the contacts combo widget
* with this information
*/

static LList * get_contacts(gchar *group)
{
	LList *node = NULL, *newlist = NULL;
	grouplist *g;
	
	g = find_grouplist_by_name(group);

	if(g)
		node = g->members;
	
	while(node) {
		newlist = l_list_insert_sorted(newlist, 
				((struct contact *)node->data)->nick, 
				strcasecmp_glist);
		node = node->next;
	}
	
	return newlist;
}

LList * get_all_contacts()
{
	LList *node = get_groups();
	LList *newlist = NULL;

	while(node) {
		LList * g = get_contacts(node->data);
		newlist = l_list_concat(newlist, g);	
		node = node->next;
	}

	return newlist;
}

/*
* this gets a list of all accounts associated with a contact
*/

static LList * get_eb_accounts(gchar *contact)
{
	LList *node = NULL, *newlist = NULL;
	struct contact * c;
	
	c = find_contact_by_nick(contact);

	if(c)
		node = c->accounts;
	
	while(node) {
		newlist = l_list_append(newlist, ((eb_account *)node->data));
		node = node->next;
	}
	
	return newlist;
}

LList * get_all_accounts(int serviceid)
{
	LList *node = get_all_contacts();
	LList *newlist = NULL;

	while(node) {
		LList * g = get_eb_accounts(((struct contact*)node->data)->nick);
		while (g) {
			eb_account	*ac = (eb_account *)g->data;
			LList		*next = g->next;
			
			if (ac->service_id == serviceid)
				newlist = l_list_append(newlist, ac->handle);
			
			free( g );	
			g = next;
		}
		node = node->next;
	}

	return newlist;
}

static void  dif_group(GtkEditable *editable, gpointer user_data)
{
		GList * list = llist_to_glist(get_contacts(COMBO_TEXT(group_name)), 1);
		
		char *tmp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(contact_name)->entry));
		
		gtk_signal_handler_block(GTK_OBJECT(GTK_COMBO(contact_name)->entry),
				contact_input_handler);
		
		list = g_list_prepend(list, tmp?tmp:"");
		
		gtk_combo_set_popdown_strings(GTK_COMBO(contact_name), list);
		g_list_free(list);
		gtk_signal_handler_unblock(GTK_OBJECT(GTK_COMBO(contact_name)->entry),
				contact_input_handler);
}

/*This is the function for changing the contact entry*/

static void set_con(GtkEditable *editable, gpointer user_data)
{
	if(flag == 0){ 
		gtk_signal_handler_block(GTK_OBJECT(GTK_COMBO(contact_name)->entry),
				contact_input_handler);
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(contact_name)->entry), 
				gtk_entry_get_text(GTK_ENTRY(account_name)));
		gtk_signal_handler_unblock(GTK_OBJECT(GTK_COMBO(contact_name)->entry),
				contact_input_handler);
	}
}

/*callback that sets the flag if the contact name has been modified*/

static void con_modified(GtkEditable *editable, gpointer user_data)
{
	flag = 1;
}

static void add_button_callback(GtkButton *button, gpointer userdata)
{
	grouplist *gl;
	struct contact *con;
	gchar *service = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(service_list)->entry));
	gchar *account = gtk_entry_get_text(GTK_ENTRY(account_name));
	
	gint service_id = get_service_id( service );
				
	eb_account *ea = eb_services[service_id].sc->new_account(account);
	ea->service_id = service_id;

	if (eb_services[service_id].sc->check_login) {
		char *buf = NULL;
		char *err = eb_services[service_id].sc->check_login((char *)account, (char *)"");
		if (err != NULL) {
			buf = g_strdup_printf(_("This account is not a valid %s account: \n\n %s"), 
					get_service_name(service_id), err);
			g_free(err);
			ay_do_error( _("Invalid Account"), buf );
			g_free(buf);
			return;
		}
	}
	gl = find_grouplist_by_name(COMBO_TEXT(group_name));
	con = find_contact_by_nick(COMBO_TEXT(contact_name));

	if(!gl) {
		add_group(COMBO_TEXT(group_name));
		gl = find_grouplist_by_name(COMBO_TEXT(group_name));
	}

	if(!con) {	
		con = add_new_contact(COMBO_TEXT(group_name), COMBO_TEXT(contact_name), ea->service_id);
	}  
	
	add_account(con->nick, ea);
/*	update_contact_list ();
	write_contact_list(); */
	gtk_widget_destroy(add_contact_window);
}

/*
* Create a Add contact window and put it on the screen
*/

static void show_add_defined_contact_window(struct contact * cont, grouplist *grp)
{
	/*
	 * if the add contact window is already open, we don't want to 
	 * do anything
	 */

	if(!window_open) {
		GtkWidget *hbox;
		GtkWidget *hbox2;
		GtkWidget *vbox;
		GtkWidget *label;
		GtkWidget *button;
		GtkWidget *table;
		GtkWidget *frame;
		GtkWidget *separator;
		GList *list;

		add_contact_window = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_position(GTK_WINDOW(add_contact_window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(add_contact_window);
		gtk_container_set_border_width(GTK_CONTAINER(add_contact_window), 5);      

		table = gtk_table_new(4, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		vbox = gtk_vbox_new(FALSE, 5);
		hbox = gtk_hbox_new(FALSE, 0);

		/*Section for adding account*/

		label = gtk_label_new(_("Account: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		account_name = gtk_entry_new();
		if (cont == NULL)
			gtk_signal_connect(GTK_OBJECT(GTK_ENTRY(account_name)), "changed",
					GTK_SIGNAL_FUNC(set_con), NULL);
		gtk_table_attach(GTK_TABLE(table), account_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(account_name);

		/*Section for declaring the protocol*/
	      
		hbox = gtk_hbox_new(FALSE, 0);

		label = gtk_label_new(_("Protocol: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		/*List of Protocols*/

		service_list = gtk_combo_new();
		list = llist_to_glist(get_service_list(), 1);
		gtk_combo_set_popdown_strings(GTK_COMBO(service_list), list );
		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), service_list, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(service_list);

		/*Section for Contact Name*/

		hbox = gtk_hbox_new(FALSE, 0);

		label = gtk_label_new(_("Contact: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		/*List of available contacts*/

		contact_name = gtk_combo_new();
		list = llist_to_glist(get_all_contacts(), 1);
		gtk_combo_set_popdown_strings(GTK_COMBO(contact_name), list );
		if(cont != NULL)
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(contact_name)->entry), cont->nick);
		else
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(contact_name)->entry), "");
		g_list_free(list);
		contact_input_handler = gtk_signal_connect(GTK_OBJECT(GTK_ENTRY(GTK_COMBO(contact_name)->entry)), "changed",
													GTK_SIGNAL_FUNC(con_modified), NULL);
		gtk_table_attach(GTK_TABLE(table), contact_name, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(contact_name);

		/*Section for Group declaration*/

		hbox = gtk_hbox_new(FALSE, 0);

		label = gtk_label_new(_("Group: "));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);

		/*List of available groups*/

		group_name = gtk_combo_new();
		list = llist_to_glist(get_groups(), 1);
		gtk_combo_set_popdown_strings(GTK_COMBO(group_name), list );

		if(strlen(COMBO_TEXT(group_name)) == 0) {
			if(cont != NULL)
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group_name)->entry), cont->group->name);
			else if (grp != NULL)
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group_name)->entry), grp->name);
			else				
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group_name)->entry), _("Buddies"));
		} else if(cont != NULL) {
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group_name)->entry), cont->group->name);
	        } else if (grp != NULL) {
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(group_name)->entry), grp->name);
		}
		g_list_free(list);
		gtk_signal_connect(GTK_OBJECT(GTK_COMBO(group_name)->entry), "changed",
		  			GTK_SIGNAL_FUNC(dif_group), NULL);
		gtk_table_attach(GTK_TABLE(table), group_name, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(group_name);

		/*Lets create a frame to put all of this in*/

		frame = gtk_frame_new(NULL);
		gtk_frame_set_label(GTK_FRAME(frame), _("Add Contact"));

		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show(frame);

		/*Lets try adding a seperator*/

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);
		
		hbox = gtk_hbox_new(FALSE, 5);
		hbox2 = gtk_hbox_new(TRUE, 5);

		/*Add Button*/

		gtk_widget_set_usize(hbox2, 200,25);

		button = do_icon_button(_("Add"), tb_preferences_xpm, add_contact_window);

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
							GTK_SIGNAL_FUNC(add_button_callback),
							NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/*Cancel Button*/

		button = do_icon_button(_("Cancel"), cancel_xpm, add_contact_window);

		gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
						GTK_SIGNAL_FUNC(gtk_widget_destroy),
						GTK_OBJECT(add_contact_window));
	 	gtk_widget_show(hbox);     

		gtk_container_add(GTK_CONTAINER (button), hbox);		

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);

		/*Buttons End*/

		hbox = gtk_hbox_new(FALSE, 0);
		table = gtk_table_new(1, 1, FALSE);

		gtk_box_pack_end(GTK_BOX(hbox),hbox2, FALSE, FALSE, 0);
		gtk_widget_show(hbox2);      

		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(vbox);

		gtk_container_add(GTK_CONTAINER(add_contact_window), table);
		gtk_widget_show(table);

		gtk_window_set_title(GTK_WINDOW(add_contact_window), _("Ayttm - Add Contact"));
		gtk_widget_grab_focus(account_name);
		gtkut_set_window_icon( add_contact_window->window, NULL );
		gtk_widget_show(add_contact_window);

		gtk_signal_connect(GTK_OBJECT(add_contact_window), "destroy",
					GTK_SIGNAL_FUNC(destroy), NULL);

		window_open = 1;
	}
}

void show_add_contact_window()
{
	show_add_defined_contact_window(NULL, NULL);
}

void show_add_contact_to_group_window(grouplist *g)
{
	show_add_defined_contact_window(NULL, g);
}

void show_add_group_window() 
{
	edit_group_window_new(NULL);
}

void show_add_account_to_contact_window(struct contact * cont) 
{
	show_add_defined_contact_window(cont, NULL);
}
