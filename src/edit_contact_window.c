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

#include "service.h"
#include "status.h"
#include "util.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>


extern GtkWidget * statuswindow;

static gint window_open = 0;
static GtkWidget * edit_contact_window = NULL;
static GtkWidget * nick = NULL;
static GtkWidget * service_list = NULL;
static GtkWidget * group_list = NULL;

static void ok_callback( struct contact *my_contact )
{
	gint service_id = get_service_id( gtk_combo_box_get_active_text(GTK_COMBO_BOX(service_list)) );
	grouplist *g = my_contact->group;
	
        /* Rename log if logging is enabled */
	rename_contact(my_contact, gtk_entry_get_text(GTK_ENTRY(nick)));
	/* contact may have changed... */
	my_contact = find_contact_in_group_by_nick(gtk_entry_get_text(GTK_ENTRY(nick)), g);
		
	my_contact->default_chatb = service_id;
	my_contact->default_filetransb = service_id;
	
	move_contact(gtk_entry_get_text(GTK_ENTRY(GTK_BIN(group_list)->child)),
			my_contact);
	update_contact_list ();
	write_contact_list();
	gtk_widget_destroy(edit_contact_window);
}

void edit_contact_window_new( struct contact * c )
{
	gchar buff[1024];
	if( !window_open ) {
		GtkWidget * hbox = gtk_hbox_new( FALSE, 0 );
		GtkWidget * label;
		guint label_key;
		GtkWidget * frame;
		GtkWidget * table;
		GList * list;
		GList *gwalker;
		GtkWidget *dialog_content_area = NULL;

		table = gtk_table_new(3, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		
		/* Contact */
		
		label = gtk_label_new_with_mnemonic(_("_Contact:"));
		label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL,
				GTK_FILL, 0, 0);
		gtk_widget_show(hbox);
		
		nick = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), nick, 1, 2, 0, 1, 
				GTK_FILL, GTK_FILL, 0, 0);
		
		gtk_entry_set_text(GTK_ENTRY(nick), c->nick );
		gtk_widget_show(nick);
		hbox = gtk_hbox_new( FALSE, 0 );

		/* Group */
		
		label = gtk_label_new_with_mnemonic(_("_Group: "));
		label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
		gtk_box_pack_end(GTK_BOX(hbox),label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
				GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(hbox);
		
		group_list = gtk_combo_box_entry_new_text();
		list = llist_to_glist(get_groups(), 1);

		for(gwalker = list; gwalker; gwalker = g_list_next(gwalker)) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(group_list), (gchar *)gwalker->data);
		}

		g_list_free(list);
		gtk_table_attach(GTK_TABLE(table), group_list, 1, 2, 1, 2,
				GTK_FILL, GTK_FILL, 0, 0);

		gtk_widget_show(group_list);
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(group_list)->child), 
				c->group->name);

		hbox = gtk_hbox_new(FALSE, 0);

		/* Default service */
		
		label = gtk_label_new_with_mnemonic(_("Default _Protocol: "));
		label_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(label));
		gtk_box_pack_end(GTK_BOX(hbox),label, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL,
				0, 0);
		gtk_widget_show(hbox);

		{
			LList *l, *l2;
			int i, def=0;
			service_list = gtk_combo_box_new_text();
			l2 = get_service_list();
			for(i=0, l=l2; l; i++, l=l_list_next(l)) {
				char *label = l->data;
				gtk_combo_box_append_text(GTK_COMBO_BOX(service_list), label);

				if(!strcmp(eb_services[c->default_chatb].name, label))
					def=i;
			}
			l_list_free(l2);

			gtk_combo_box_set_active(GTK_COMBO_BOX(service_list), def);
		}
		gtk_table_attach(GTK_TABLE(table), service_list, 1, 2, 2, 3,
				GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(service_list);
		
		frame = gtk_frame_new(NULL);

		g_snprintf(buff,1024,_("Edit Properties for %s"), c->nick);
		gtk_frame_set_label(GTK_FRAME(frame), buff);

		gtk_container_add(GTK_CONTAINER(frame), table);

		edit_contact_window = gtk_dialog_new_with_buttons(
								"Ayttm",
								GTK_WINDOW(statuswindow),
								GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
								GTK_STOCK_SAVE,
								GTK_RESPONSE_ACCEPT,
								GTK_STOCK_CANCEL,
								GTK_RESPONSE_REJECT,
								NULL
								);

		gtk_dialog_set_default_response(GTK_DIALOG(edit_contact_window), GTK_RESPONSE_ACCEPT);

#ifdef HAVE_GTK_2_14
	        dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(edit_contact_window));
#else
		dialog_content_area = GTK_DIALOG(edit_contact_window)->vbox;
#endif

		gtk_box_pack_start(GTK_BOX(dialog_content_area), frame, TRUE, TRUE, 5);
		gtk_widget_show_all(edit_contact_window);

		window_open = 1;
	}
	
	g_snprintf(buff,1024,_("%s - Edit Contact"), c->nick);
	gtk_window_set_title(GTK_WINDOW(edit_contact_window), buff ); 
	{
		LList *l, *l2;
		int i, def=0;
		l2 = get_service_list();
		for(i=0, l=l2; l; i++, l=l_list_next(l)) {
			char *label = l->data;
			if(!strcmp(eb_services[c->default_chatb].name, label))
				def=i;
		}
		l_list_free(l2);

		gtk_combo_box_set_active(GTK_COMBO_BOX(service_list), def);
	}

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(edit_contact_window)))
		ok_callback(c);

	gtk_widget_destroy(edit_contact_window);
	window_open = 0;
}


