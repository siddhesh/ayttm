/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ui_message_windows.h"

#include "gtkutils.h"

#include "pixmaps/info.xpm"
#include "pixmaps/warning.xpm"
#include "pixmaps/error.xpm"
#include "pixmaps/ok.xpm"


typedef enum
{
	MESSAGE_INFO,
	MESSAGE_WARNING,
	MESSAGE_ERROR
} tMessageType;


static void	s_do_message_dialog( tMessageType inType, const char *inTitle, const char *inMessage, int inModal );



void	ay_ui_info_window_create( const char *inTitle, const char *inMessage )
{
	s_do_message_dialog( MESSAGE_INFO, inTitle, inMessage, 0 );
}

void	ay_ui_warning_window_create( const char *inTitle, const char *inMessage )
{
	s_do_message_dialog( MESSAGE_WARNING, inTitle, inMessage, 0 );
}

void	ay_ui_error_window_create( const char *inTitle, const char *inMessage )
{
	s_do_message_dialog( MESSAGE_ERROR, inTitle, inMessage, 1 );
}


static void	s_handle_key( GtkWidget *inWidget, GdkEventAny *inEvent, gpointer inData )
{
	if ( inEvent->type == GDK_KEY_PRESS )
	{
		GtkWidget		*ok_button = (GtkWidget *)inData;
		unsigned int	key = ((GdkEventKey *)inEvent)->keyval;
		
		switch ( key )
		{
			case GDK_Escape:
			case GDK_KP_Enter:
			case GDK_Return:
				gtk_signal_emit_by_name( GTK_OBJECT(ok_button), "clicked" );
				break;
				
			default:
				/* do nothing */
				break;
		}
	}
}

static void	s_do_message_dialog( tMessageType inType, const char *inTitle, const char *inMessage, int inModal)
{
	GtkWidget	*dlg = NULL;
	GtkWidget	*label = NULL;
	GtkWidget	*ok_button = NULL;
	GtkWidget	*hbox2 = NULL;
	GtkWidget	*hbox_xpm = NULL;
	GtkWidget	*iconwid = NULL;
	GdkPixmap	*icon = NULL;
	GdkBitmap	*mask = NULL;
	GtkStyle	*style = NULL;
	char		**pixmap = NULL;
	
	
	label = gtk_label_new( inMessage );
	gtk_widget_set_usize( label, 240, -1 );
	gtk_misc_set_alignment( GTK_MISC(label), 0.1, 0.5 );
	gtk_label_set_line_wrap( GTK_LABEL(label), TRUE );
	gtk_widget_show( label );
	
	dlg = gtk_dialog_new();
	gtk_window_set_modal( GTK_WINDOW(dlg), inModal );
	gtk_window_set_title( GTK_WINDOW(dlg), inTitle );
	gtk_container_set_border_width( GTK_CONTAINER(dlg), 0 );
	gtk_widget_realize( dlg );
	
	hbox_xpm = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox_xpm );
	
	switch ( inType )
	{
		case MESSAGE_INFO:
			pixmap = info_xpm;
			break;
		
		case MESSAGE_WARNING:
			pixmap = warning_xpm;
			break;
			
		case MESSAGE_ERROR:
			pixmap = error_xpm;
			break;
	}
	
	style = gtk_widget_get_style( dlg );
	
	icon = gdk_pixmap_create_from_xpm_d( dlg->window, &mask, &style->bg[GTK_STATE_NORMAL], pixmap );
	iconwid = gtk_pixmap_new( icon, mask );
	gtk_widget_show( iconwid );
	
	gtk_box_pack_start( GTK_BOX(hbox_xpm), iconwid, FALSE, FALSE, 20 );
	gtk_box_pack_start( GTK_BOX(hbox_xpm), label, TRUE, TRUE, 0 );

	gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox), hbox_xpm, TRUE, TRUE, 15 );
	
	hbox2 = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox2 );
	
	ok_button = gtkut_create_icon_button( _("OK"), ok_xpm, dlg );
	gtk_signal_connect_object( GTK_OBJECT(ok_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(dlg) );
	gtk_signal_connect( GTK_OBJECT(dlg), "key_press_event", GTK_SIGNAL_FUNC(s_handle_key), ok_button );
	gtk_box_pack_end( GTK_BOX(hbox2), ok_button, FALSE, FALSE, 0 );
	
	gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->action_area), hbox2, TRUE, TRUE, 0 );

	gtk_window_set_position( GTK_WINDOW(dlg), GTK_WIN_POS_MOUSE );
	gtk_widget_grab_focus( ok_button );
	
	gtk_widget_show( dlg );
}
