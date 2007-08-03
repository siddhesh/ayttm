/*
 * User Counter for Ayttm 
 *
 * Copyright (C) 2004, the Ayttm team
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "prefs.h"
#include "gtk_globals.h"

static void ay_usercount(GtkWidget *widget, gpointer data)
{
	int val = GPOINTER_TO_INT(data);

	if(val) {
		g_print("Yes\n");
		iSetLocalPref("usercount_window_seen", 1);
	} else {
		g_print("No\n");
		iSetLocalPref("usercount_window_seen", -1);
	}
	ayttm_prefs_write();
}

void show_wnd_usercount(void)
{
	static GtkWidget *wnd_usercount;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *chk_country;
	GtkWidget *chk_services;
	GtkWidget *label2;
	GtkWidget *hbox2;
	GtkWidget *label3;
	GtkWidget *hbuttonbox1;
	guint btn_no_key;
	GtkWidget *btn_no;
	guint btn_yes_key;
	GtkWidget *btn_yes;
	GtkAccelGroup *accel_group;

	if(wnd_usercount) {
		gtk_widget_show(wnd_usercount);
		return;
	}

	accel_group = gtk_accel_group_new();

	wnd_usercount = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(wnd_usercount), _("Stand up and be counted"));

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_container_add(GTK_CONTAINER(wnd_usercount), hbox1);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 4);

	label1 = gtk_label_new(_("The Ayttm team would like to know how many people use ayttm.  "
			"Clicking Yes below will include you in this count.  "
			"Click on No to never be asked this question again.  "
			"No personal information will be collected during this process."));
	gtk_widget_show(label1);
	gtk_box_pack_start(GTK_BOX(vbox1), label1, TRUE, TRUE, 10);
	gtk_widget_set_size_request(label1, 410, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_FILL);
	gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE);

	frame1 = gtk_frame_new(_("Information that will be collected"));
	gtk_widget_show(frame1);
	gtk_box_pack_start(GTK_BOX(vbox1), frame1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(frame1), 5);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox2);
	gtk_container_add(GTK_CONTAINER(frame1), vbox2);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);

	chk_country = gtk_check_button_new_with_label(_("Your Country"));
	gtk_widget_show(chk_country);
	gtk_box_pack_start(GTK_BOX(vbox2), chk_country, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_country), TRUE);

	chk_services = gtk_check_button_new_with_label(_("The services you use (Eg: MSN, Yahoo, etc., but no usernames)"));
	gtk_widget_show(chk_services);
	gtk_box_pack_start(GTK_BOX(vbox2), chk_services, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_services), TRUE);

	label2 = gtk_label_new(_("Deselect any of the above checkboxes if you would prefer that that information not be included in the survey."));
	gtk_widget_show(label2);
	gtk_box_pack_start(GTK_BOX(vbox1), label2, TRUE, TRUE, 5);
	gtk_widget_set_size_request(label2, 410, 0);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_FILL);
	gtk_label_set_line_wrap(GTK_LABEL(label2), TRUE);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox2, TRUE, TRUE, 0);
	gtk_widget_show(hbox2);

	label3 = gtk_label_new("");
	gtk_widget_show(label3);
	gtk_box_pack_start(GTK_BOX(hbox2), label3, TRUE, TRUE, 0);

	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox1);
	gtk_box_pack_start(GTK_BOX(hbox2), hbuttonbox1, FALSE, TRUE, 0);
	gtk_box_set_spacing(GTK_BOX(hbuttonbox1), 5);
	
	btn_no = gtk_button_new_with_mnemonic(_("_No"));
	btn_no_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(GTK_BIN(btn_no)->child));
	gtk_widget_add_accelerator(btn_no, "clicked", accel_group, btn_no_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
	gtk_widget_set_size_request(btn_no, 70, 0);
	gtk_widget_show(btn_no);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), btn_no);
	GTK_WIDGET_SET_FLAGS(btn_no, GTK_CAN_DEFAULT);

	btn_yes = gtk_button_new_with_mnemonic(_("_Yes"));
	btn_yes_key = gtk_label_get_mnemonic_keyval(GTK_LABEL(GTK_BIN(btn_yes)->child));
	gtk_widget_add_accelerator(btn_yes, "clicked", accel_group, btn_yes_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
	gtk_widget_set_size_request(btn_yes, 70, 0);
	gtk_widget_show(btn_yes);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), btn_yes);
	GTK_WIDGET_SET_FLAGS(btn_yes, GTK_CAN_DEFAULT);

	g_signal_connect_data(btn_no, "clicked", G_CALLBACK(gtk_widget_destroy),
			wnd_usercount, NULL, G_CONNECT_AFTER|G_CONNECT_SWAPPED);

	g_signal_connect(btn_no, "clicked", G_CALLBACK(ay_usercount), GINT_TO_POINTER(0));
	g_signal_connect_data(btn_yes, "clicked", G_CALLBACK(gtk_widget_destroy),
			wnd_usercount, NULL, G_CONNECT_AFTER|G_CONNECT_SWAPPED);
	g_signal_connect(btn_yes, "clicked", G_CALLBACK(ay_usercount), GINT_TO_POINTER(1));

	gtk_window_add_accel_group(GTK_WINDOW(wnd_usercount), accel_group);

	g_signal_connect(wnd_usercount, "destroy", G_CALLBACK(gtk_widget_destroyed), &wnd_usercount);
	gtk_widget_show(wnd_usercount);
	gtk_window_set_transient_for(GTK_WINDOW(wnd_usercount), GTK_WINDOW(statuswindow));
}
