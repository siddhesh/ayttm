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

#include "status.h"
#include "dialog.h"
#include "util.h"

#include "gtk/gtkutils.h"

#include "pixmaps/tb_edit.xpm"
#include "pixmaps/cancel.xpm"



static gint window_open = 0;
static GtkWidget * edit_group_window;
static GtkWidget * group_name;
static grouplist * current_group;

static void destroy( GtkWidget *widget, gpointer data)
{
	window_open = 0;
}

static void ok_callback( GtkWidget * widget, gpointer data)
{
	if (group_name == NULL || gtk_entry_get_text(GTK_ENTRY(group_name)) == NULL
	||  strlen(gtk_entry_get_text(GTK_ENTRY(group_name))) == 0)
		return;
	if (current_group) { /*edit*/
		rename_group(current_group, gtk_entry_get_text(GTK_ENTRY(group_name)));
	} else { /*add*/
		add_group(gtk_entry_get_text(GTK_ENTRY(group_name)));
	}
	gtk_widget_destroy(edit_group_window);
}

static void cancel_callback( GtkWidget * widget, gpointer data)
{
	gtk_widget_destroy(edit_group_window);
}

void edit_group_window_new( grouplist * g)
{
	gchar buff[1024];
	gchar *name;
	if (g)
		name = g->name;
	else
		name = "";
	if ( !window_open )
	{
		GtkWidget * vbox = gtk_vbox_new( FALSE, 5 );
		GtkWidget * hbox = gtk_hbox_new( FALSE, 5 );
		GtkWidget * hbox2;
		GtkWidget * label;
		GtkWidget * button;
		GtkWidget * frame;
		GtkWidget * separator;
		
		edit_group_window = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_position(GTK_WINDOW(edit_group_window), GTK_WIN_POS_MOUSE);
		gtk_widget_realize(edit_group_window);
		gtk_container_set_border_width(GTK_CONTAINER(edit_group_window), 5);
		
		label = gtk_label_new(_("Group Name:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		group_name = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(hbox), group_name, TRUE, TRUE, 5);
		gtk_entry_set_text(GTK_ENTRY(group_name), name);
		gtk_widget_show(group_name);

		frame = gtk_frame_new(NULL);

		g_snprintf(buff,1024,_("%s group%s%s"), g?_("Edit"):_("Add"), g?" ":"", name);
		
		gtk_frame_set_label(GTK_FRAME(frame), buff);

		hbox2=gtk_vbox_new(FALSE,5);
		gtk_box_pack_start(GTK_BOX(hbox2),hbox,TRUE,TRUE,5);
		gtk_container_add(GTK_CONTAINER(frame), hbox2);
		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
		
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
		gtk_widget_show(frame);

		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
		gtk_widget_show(separator);

		hbox = gtk_hbox_new(TRUE, 5);

		/*Ok Button*/
		hbox2=gtk_hbox_new(FALSE,5);
		button = do_icon_button(g?_("Save"):_("Add"), tb_edit_xpm, edit_group_window);

		gtk_signal_connect(GTK_OBJECT(button), "clicked", 
						   GTK_SIGNAL_FUNC(ok_callback), NULL );

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
		
		/*Cancel Button*/
      
		button = do_icon_button(_("Cancel"), cancel_xpm, edit_group_window);

		gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
						GTK_SIGNAL_FUNC(cancel_callback), NULL);

		gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
		
		hbox = gtk_hbox_new(FALSE, 5);
		
		gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0 );
		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
		
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
		gtk_container_add(GTK_CONTAINER(edit_group_window), vbox);
		gtk_widget_show(vbox);
		
		gtk_signal_connect( GTK_OBJECT(edit_group_window), "destroy",
				GTK_SIGNAL_FUNC(destroy), NULL);
		gtk_widget_show(edit_group_window);
	}

	gtk_entry_set_text(GTK_ENTRY(group_name), name);
	if (g)
		g_snprintf(buff, 1024, _("Edit Properties for %s"), name);
	else
		g_snprintf(buff, 1024, _("Add group"));
	gtk_window_set_title(GTK_WINDOW(edit_group_window), buff);
	gtkut_set_window_icon(edit_group_window->window, NULL);
	gtk_signal_connect(GTK_OBJECT(edit_group_window), "destroy",
				GTK_SIGNAL_FUNC(destroy), NULL);

	current_group = g;
	window_open = 1;
	gtk_widget_grab_focus(group_name);
	
}

