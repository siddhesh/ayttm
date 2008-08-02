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

#include "info_window.h"
#include "dialog.h"

#include "gtk/html_text_buffer.h"
#include "gtk/gtkutils.h"

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
	GdkPixbuf *icon;
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
        gtk_window_set_resizable(GTK_WINDOW(iw->window), TRUE);
        gtk_widget_realize(iw->window);

	iw->info = gtk_text_view_new();
	html_text_view_init( GTK_TEXT_VIEW(iw->info), HTML_IGNORE_NONE );
        iw->scrollwindow = gtk_scrolled_window_new(NULL,NULL);

        gtk_widget_realize(iw->window);	
	gtk_window_set_title(GTK_WINDOW(iw->window), remote->handle);

        gtk_widget_set_size_request(iw->scrollwindow, 375, 150);
	gtk_container_add(GTK_CONTAINER(iw->scrollwindow),iw->info);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(iw->scrollwindow),GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	gtk_box_pack_start(GTK_BOX(vbox), iw->scrollwindow, TRUE,TRUE, 5);
	gtk_widget_show(iw->scrollwindow);

        gtk_container_set_border_width(GTK_CONTAINER(iw->window), 5);

        g_signal_connect(iw->window, "destroy", G_CALLBACK(iw_destroy_event), iw);

        icon = gdk_pixbuf_new_from_xpm_data( (const char **) cancel_xpm);
	iconwid = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(iconwid);

        ok_button = gtk_button_new ();
        g_signal_connect(ok_button, "clicked", G_CALLBACK(iw_close_win), iw);

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
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(iw->info));
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);
}


void eb_info_window_add_info( eb_account * remote_account, gchar* text, gint ignore_bg, gint ignore_fg, gint ignore_font ) 
{
	gchar msg[1024] ;
	gchar *valid_end;

	strncpy(msg, text, 1023);

	if(!g_utf8_validate(msg, -1, (const gchar **)&valid_end)) {
		*valid_end = '\0';

		strncat(msg, _("<font color=red> (Invalid UTF-8 characters in response)</font>"), 1023 - strlen(msg));
	}

	if(remote_account->infowindow) {
		html_text_buffer_append(GTK_TEXT_VIEW(remote_account->infowindow->info), msg,
				(ignore_bg?HTML_IGNORE_BACKGROUND:HTML_IGNORE_NONE) |
				(ignore_fg?HTML_IGNORE_FOREGROUND:HTML_IGNORE_NONE) |
				(ignore_font?HTML_IGNORE_FONT:HTML_IGNORE_NONE) );
	}
}

