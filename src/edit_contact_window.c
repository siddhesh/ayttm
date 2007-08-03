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

#include "gtk/gtkutils.h"
#include <gdk/gdkkeysyms.h>

#include "pixmaps/tb_edit.xpm"
#include "pixmaps/cancel.xpm"

extern GtkWidget * statuswindow;

static gint window_open = 0;
static struct contact * my_contact = NULL;
static GtkWidget * edit_contact_window = NULL;
static GtkWidget * nick = NULL;
static GtkWidget * service_list = NULL;
static GtkWidget * group_list = NULL;

static void destroy( GtkWidget *widget, gpointer data )
{
    window_open = 0;
}

static void ok_callback( GtkWidget * widget, gpointer data )
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
		GtkWidget * vbox = gtk_vbox_new( FALSE, 5 );
		GtkWidget * hbox = gtk_hbox_new( FALSE, 0 );
		GtkWidget * hbox2;
		GtkWidget * button;
		GtkWidget * label;
		guint label_key;
		GtkWidget * frame;
		GtkWidget * table;
		GtkWidget * separator;
		GtkAccelGroup *accel_group;
		GList * list;
		GList *gwalker;

		accel_group = gtk_accel_group_new();

		edit_contact_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_transient_for(GTK_WINDOW(edit_contact_window), GTK_WINDOW(statuswindow));
		gtk_window_set_position(GTK_WINDOW(edit_contact_window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(edit_contact_window);
		gtk_container_set_border_width(GTK_CONTAINER(edit_contact_window), 5);
		
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
		gtk_widget_add_accelerator(nick, "grab_focus", accel_group,
				label_key, GDK_MOD1_MASK, (GtkAccelFlags)0);
		
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
		gtk_widget_add_accelerator(group_list, "grab_focus", accel_group,
				label_key, GDK_MOD1_MASK, (GtkAccelFlags)0);

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
		gtk_widget_add_accelerator(service_list, "grab_focus", accel_group,
				label_key, GDK_MOD1_MASK, (GtkAccelFlags)0);
		
		frame = gtk_frame_new(NULL);

		g_snprintf(buff,1024,_("Edit Properties for %s"), c->nick);
		gtk_frame_set_label(GTK_FRAME(frame), buff);

		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show(frame);

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);

		hbox = gtk_hbox_new(FALSE, 5);
		hbox2 = gtk_hbox_new(TRUE, 5);
		gtk_widget_set_size_request(hbox2, 200,25 );
		
		/*Ok Button*/
   
		button = gtkut_create_icon_button( _("Save"), tb_edit_xpm, edit_contact_window );

		g_signal_connect(button, "clicked", G_CALLBACK(ok_callback), NULL );

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
		gtk_widget_add_accelerator(button, "clicked", accel_group,
				GDK_Return, 0, GTK_ACCEL_VISIBLE);
		gtk_widget_add_accelerator(button, "clicked", accel_group,
				GDK_KP_Enter, 0, GTK_ACCEL_VISIBLE);
		
		/*Cancel Button*/
      
		button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, edit_contact_window );

		g_signal_connect_swapped(button, "clicked",
				G_CALLBACK(gtk_widget_destroy),edit_contact_window);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
		gtk_widget_add_accelerator(button, "clicked", accel_group,
				GDK_Escape, 0, GTK_ACCEL_VISIBLE);
		
		/*Buttons End*/
		
		hbox = gtk_hbox_new(FALSE, 5);
		
		gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0 );
		gtk_widget_show(hbox2);
		
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
		
		table = gtk_table_new(1, 1, FALSE);
		gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
				GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show(vbox);
		
		gtk_container_add(GTK_CONTAINER(edit_contact_window), table);
		gtk_widget_show(table);
		
		g_signal_connect(edit_contact_window, "destroy", G_CALLBACK(destroy), NULL );
		gtk_window_add_accel_group(GTK_WINDOW(edit_contact_window), accel_group);
		gtk_widget_show(edit_contact_window);
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

	g_signal_connect(edit_contact_window, "destroy", G_CALLBACK(destroy), NULL );
	my_contact = c;
	window_open = 1;
}
			
