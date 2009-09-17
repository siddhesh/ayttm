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

#include <assert.h>

#include "intl.h"

#include "ui_about_window.h"
# include "gtkutils.h"

#include "pixmaps/ayttmlogo.xpm"

/* the one and only about window */
static GtkWidget *sAboutWindow = NULL;

/*
  Functions
*/

static void destroy_about(void)
{
	if (sAboutWindow != NULL)
		gtk_widget_destroy(sAboutWindow);

	sAboutWindow = NULL;
}

static GtkTable *sMakeDeveloperTable(int inNumDevelopers,
	const tDeveloperInfo *inDevInfo)
{
	GtkTable *table = GTK_TABLE(gtk_table_new(inNumDevelopers, 3, FALSE));
	GtkLabel *label = NULL;
	int i = 0;

	for (i = 0; i < inNumDevelopers; i++) {
		label = GTK_LABEL(gtk_label_new(inDevInfo[i].m_name));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_table_attach(table, GTK_WIDGET(label), 0, 1, i, i + 1,
			GTK_FILL, GTK_FILL, 10, 0);

		label = GTK_LABEL(gtk_label_new(inDevInfo[i].m_email));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_table_attach(table, GTK_WIDGET(label), 1, 2, i, i + 1,
			GTK_FILL, GTK_FILL, 10, 0);

		label = GTK_LABEL(gtk_label_new(inDevInfo[i].m_role));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_table_attach(table, GTK_WIDGET(label), 2, 3, i, i + 1,
			GTK_FILL, GTK_FILL, 10, 0);
	}

	gtk_widget_show(GTK_WIDGET(table));

	return (table);
}

void ay_ui_about_window_create(const tAboutInfo *inAboutInfo)
{
	GtkWidget *logo = NULL;
	GtkWidget *ok = NULL;
	GtkLabel *label = NULL;
	GtkTable *table = NULL;
	GtkWidget *separator = NULL;
	GtkBox *vbox = NULL;
	GtkBox *vbox2 = NULL;
	GtkStyle *style = NULL;
	GdkPixbuf *pm = NULL;
	GtkWidget *scroll = NULL;

	if (sAboutWindow != NULL) {
		gtk_widget_show(sAboutWindow);
		return;
	}

	assert(inAboutInfo != NULL);

	/* create the window and set some attributes */
	sAboutWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(sAboutWindow);
	gtk_window_set_position(GTK_WINDOW(sAboutWindow), GTK_WIN_POS_MOUSE);
	gtk_window_set_resizable(GTK_WINDOW(sAboutWindow), FALSE);
	gtk_window_set_title(GTK_WINDOW(sAboutWindow), _("About Ayttm"));
	gtk_container_set_border_width(GTK_CONTAINER(sAboutWindow), 5);

	vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
	gtk_widget_show(GTK_WIDGET(vbox));

	/* logo */
	style = gtk_widget_get_style(sAboutWindow);
	pm = gdk_pixbuf_new_from_xpm_data((const char **)ayttmlogo_xpm);

	logo = gtk_image_new_from_pixbuf(pm);
	gtk_box_pack_start(vbox, logo, FALSE, FALSE, 5);
	gtk_widget_show(logo);

	/* version */
	label = GTK_LABEL(gtk_label_new(inAboutInfo->m_version));
	gtk_widget_show(GTK_WIDGET(label));
	gtk_box_pack_start(vbox, GTK_WIDGET(label), FALSE, FALSE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show(separator);
	gtk_box_pack_start(vbox, separator, FALSE, FALSE, 5);

	/* scroll */
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	vbox2 = GTK_BOX(gtk_vbox_new(FALSE, 0));
	gtk_widget_show(GTK_WIDGET(vbox2));

	/* text */
	label = GTK_LABEL(gtk_label_new(_("Ayttm is brought to you by:")));
	gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(GTK_WIDGET(label));
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of Ayttm developers */
	table = sMakeDeveloperTable(inAboutInfo->m_num_ay_developers,
		inAboutInfo->m_ay_developers);
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show(separator);
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* text */
	label = GTK_LABEL(gtk_label_new(_("\"With a little help from\":")));
	gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(GTK_WIDGET(label));
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of kudos to people who've helped us */
	table = sMakeDeveloperTable(inAboutInfo->m_num_ay_kudos,
		inAboutInfo->m_ay_kudos);
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show(separator);
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* another separator */
	separator = gtk_hseparator_new();
	gtk_widget_show(separator);
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* text */
	label = GTK_LABEL(gtk_label_new(_
			("Ayttm is based on Everybuddy 0.4.3 which was brought to you by:")));
	gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(GTK_WIDGET(label));
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of Everybuddy developers */
	table = sMakeDeveloperTable(inAboutInfo->m_num_eb_developers,
		inAboutInfo->m_eb_developers);
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
		GTK_WIDGET(vbox2));

	gtk_box_pack_start(vbox, GTK_WIDGET(scroll), TRUE, TRUE, 5);

	/* close button */
	ok = gtk_button_new_with_label(_("Close"));
	g_signal_connect_swapped(ok, "clicked", G_CALLBACK(destroy_about),
		sAboutWindow);

	gtk_box_pack_start(vbox, ok, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_widget_show(ok);

	g_signal_connect_swapped(sAboutWindow, "destroy",
		G_CALLBACK(destroy_about), sAboutWindow);

	gtk_container_add(GTK_CONTAINER(sAboutWindow), GTK_WIDGET(vbox));

	gtk_widget_set_size_request(sAboutWindow, -1, 400);
	gtk_widget_show(sAboutWindow);
	gtk_widget_grab_default(ok);
	gtk_widget_grab_focus(ok);
}
