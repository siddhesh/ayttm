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
 
/*
 * info_window.c
 * implementation for the info window
 *
 */
 
#include "intl.h"

#include <stdlib.h>

#include "gtk/gtk_eb_html.h"
#include "info_window.h"
#include "dialog.h"

#include "pixmaps/cancel.xpm"


static void iw_destroy_event(GtkWidget *widget, gpointer data)
{
        info_window * iw = (info_window *)data;
      
        if(iw->info_data != NULL) {
          iw->cleanup(iw);
          free(iw->info_data);
          iw->info_data = NULL;
        }
        iw->remote_account->infowindow = NULL;
        gtk_widget_destroy(iw->window);
        iw->window=NULL;
        iw->info = NULL;
        g_free(iw);
}

static void iw_close_win(GtkWidget *widget, gpointer data)
{
        info_window * iw = (info_window *)data;
        gtk_widget_destroy(iw->window);
}



info_window * eb_info_window_new(eb_local_account * local, struct account * remote)
{
	GtkWidget *vbox;
        GtkWidget *hbox;
        GtkWidget *buttonbox;
        GtkWidget *label;
	GtkWidget *ok_button;
        GtkWidget *iconwid;
	GdkPixmap *icon;
        GdkBitmap *mask;
	info_window * iw;

        vbox = gtk_vbox_new(FALSE,0);
        hbox = gtk_hbox_new(FALSE,0);
        buttonbox = gtk_hbox_new(FALSE,0);

	iw = malloc(sizeof(info_window));
        iw->info_type = -1;
        iw->info_data = NULL;
	iw->remote_account = remote;
 	iw->local_user = local;

	iw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(iw->window), GTK_WIN_POS_MOUSE);
        gtk_window_set_policy(GTK_WINDOW(iw->window), TRUE, TRUE, TRUE);
        gtk_widget_realize(iw->window);

	iw->info = ext_gtk_text_new(NULL,NULL);
	gtk_eb_html_init(EXT_GTK_TEXT(iw->info));
        iw->scrollwindow = gtk_scrolled_window_new(NULL,NULL);

        gtk_widget_realize(iw->window);	
	gtk_window_set_title(GTK_WINDOW(iw->window), remote->handle);
	eb_icon(iw->window->window);

        gtk_widget_set_usize(iw->scrollwindow, 375, 150);
	gtk_container_add(GTK_CONTAINER(iw->scrollwindow),iw->info);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(iw->scrollwindow),GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	gtk_box_pack_start(GTK_BOX(vbox), iw->scrollwindow, TRUE,TRUE, 5);
	gtk_widget_show(iw->scrollwindow);

        gtk_container_set_border_width(GTK_CONTAINER(iw->window), 5);

        gtk_signal_connect (GTK_OBJECT (iw->window), "destroy", GTK_SIGNAL_FUNC (iw_destroy_event), iw);

        icon = gdk_pixmap_create_from_xpm_d(iw->window->window, &mask, NULL, cancel_xpm);
	iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show(iconwid);

        ok_button = gtk_button_new ();
        gtk_signal_connect (GTK_OBJECT (ok_button), "clicked", GTK_SIGNAL_FUNC (iw_close_win), iw);

        gtk_box_pack_start (GTK_BOX (buttonbox), iconwid,TRUE,TRUE,0);
        label = gtk_label_new(_("Close"));
        gtk_box_pack_start (GTK_BOX (buttonbox), label,TRUE,TRUE,5);
        gtk_widget_show(buttonbox);
        gtk_container_add(GTK_CONTAINER(ok_button), buttonbox);
 
        gtk_box_pack_start(GTK_BOX(hbox), ok_button, TRUE,FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE,FALSE, 5);

	gtk_container_add(GTK_CONTAINER(iw->window), vbox);
        gtk_widget_show(iw->info);
        gtk_widget_show(label);
        gtk_widget_show(ok_button);
        gtk_widget_show(hbox);
 
        gtk_widget_show(vbox);
        gtk_widget_show(iw->window);

        return iw;
}


void eb_info_window_clear(info_window *iw) 
{
	gtk_editable_delete_text(GTK_EDITABLE(iw->info), 0, -1);
}


void eb_info_window_add_info( eb_account * remote_account, gchar* text, gint ignore_bg, gint ignore_fg, gint ignore_font ) {
	
	if(remote_account->infowindow)
	{
		gtk_eb_html_add(EXT_GTK_TEXT(remote_account->infowindow->info), text,ignore_bg,ignore_fg,ignore_font);
	}
}

