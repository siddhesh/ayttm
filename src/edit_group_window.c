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
#include <gtk/gtk.h>

#include "status.h"
#include "util.h"


extern GtkWidget * statuswindow;

static gint window_open = 0;
static GtkWidget * edit_group_window;
static GtkWidget * group_name;


static void ok_callback(grouplist *current_group)
{
	if (group_name == NULL || gtk_entry_get_text(GTK_ENTRY(group_name)) == NULL
			||  strlen(gtk_entry_get_text(GTK_ENTRY(group_name))) == 0)
		return;

	if (current_group)
		rename_group(current_group, gtk_entry_get_text(GTK_ENTRY(group_name)));
	else
		add_group(gtk_entry_get_text(GTK_ENTRY(group_name)));
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
		GtkWidget * hbox = gtk_hbox_new( FALSE, 5 );
		GtkWidget * label;
		GtkWidget * frame;
		GtkWidget *dialog_content_area;
		
		label = gtk_label_new(_("Group Name:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

		group_name = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(hbox), group_name, TRUE, TRUE, 5);
		gtk_entry_set_text(GTK_ENTRY(group_name), name);

		frame = gtk_frame_new(NULL);

		g_snprintf(buff,1024,_("%s group%s%s"), g?_("Edit"):_("Add"), g?" ":"", name);
		
		gtk_frame_set_label(GTK_FRAME(frame), buff);
		gtk_container_add(GTK_CONTAINER(frame), hbox);

		edit_group_window = gtk_dialog_new_with_buttons(
								"Ayttm",
								GTK_WINDOW(statuswindow),
								GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
								g?GTK_STOCK_SAVE:GTK_STOCK_ADD,
								GTK_RESPONSE_ACCEPT,
								GTK_STOCK_CANCEL,
								GTK_RESPONSE_REJECT,
								NULL
								);

		gtk_dialog_set_default_response(GTK_DIALOG(edit_group_window), GTK_RESPONSE_ACCEPT);

#ifdef HAVE_GTK_2_14
	        dialog_content_area = gtk_dialog_get_content_area(GTK_DIALOG(edit_group_window));
#else
		dialog_content_area = GTK_DIALOG(edit_group_window)->vbox;
#endif

		gtk_box_pack_start(GTK_BOX(dialog_content_area), frame, TRUE, TRUE, 5);
		gtk_widget_show_all(edit_group_window);

		window_open = 1;
	}

	gtk_entry_set_text(GTK_ENTRY(group_name), name);
	if (g)
		g_snprintf(buff, 1024, _("Edit Properties for %s"), name);
	else
		g_snprintf(buff, 1024, _("Add group"));

	gtk_window_set_title(GTK_WINDOW(edit_group_window), buff);

	gtk_widget_grab_focus(group_name);

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(edit_group_window)))
		ok_callback(g);

	gtk_widget_destroy(edit_group_window);
	window_open = 0;
}


