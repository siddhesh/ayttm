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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>

#include "ui_file_selection_dlg.h"


class ay_file_selection_dlg
{
	public:
		ay_file_selection_dlg( const char *inSelectedFile, const char *inWindowTitle,
							t_file_selection_callback *inCallback, void *inData );
		
	private:	// Gtk callbacks
		static void		s_ok_callback( GtkButton *button, gpointer data );
		static void		s_cancel_callback( GtkButton *button, gpointer data );
	
	private:
		GtkWidget					*m_dialog;
		t_file_selection_callback	*m_callback;
		void 						*m_data;
};

#ifdef __cplusplus
extern "C" {
#endif

/// Entry point to this file - construct and display the file selection dialog
void	ay_ui_do_file_selection( const char *inDefaultFile, const char *inWindowTitle, 
			 t_file_selection_callback *inCallback, void *inData )
{
	new ay_file_selection_dlg( inDefaultFile, inWindowTitle, inCallback, inData );
}

#ifdef __cplusplus
}
#endif

////////////////
//// ay_file_selection_dlg implementation

ay_file_selection_dlg::ay_file_selection_dlg( const char *inSelectedFile, const char *inWindowTitle,
										t_file_selection_callback *inCallback, void *inData )
:	m_callback( inCallback ),
	m_data( inData )
{
	assert( m_callback );
	
	m_dialog = gtk_file_selection_new( inWindowTitle );
	gtk_container_set_border_width( GTK_CONTAINER(m_dialog), 10 );
	gtk_window_set_modal( GTK_WINDOW(m_dialog), TRUE );

	GtkWidget	*ok_button = GTK_FILE_SELECTION(m_dialog)->ok_button;
	gtk_widget_show( ok_button );
	GTK_WIDGET_SET_FLAGS( ok_button, GTK_CAN_DEFAULT );

	GtkWidget	*cancel_button = GTK_FILE_SELECTION(m_dialog)->cancel_button;
	gtk_widget_show( cancel_button );
	GTK_WIDGET_SET_FLAGS( cancel_button, GTK_CAN_DEFAULT );

	gtk_signal_connect( GTK_OBJECT(ok_button), "clicked", GTK_SIGNAL_FUNC (s_ok_callback), this );
	gtk_signal_connect( GTK_OBJECT(cancel_button), "clicked", GTK_SIGNAL_FUNC(s_cancel_callback), this );
	
	if ( inSelectedFile != NULL )
		gtk_file_selection_set_filename( GTK_FILE_SELECTION(m_dialog), inSelectedFile );
	
	gtk_widget_show( m_dialog );
}

////
// ay_file_selection_dlg callbacks

// s_ok_callback
void	ay_file_selection_dlg::s_ok_callback( GtkButton *button, void *data )
{
	ay_file_selection_dlg	*the_dlg = reinterpret_cast<ay_file_selection_dlg *>(data);
	
	assert( the_dlg != NULL );
	
	const char	*selected_file = gtk_file_selection_get_filename( GTK_FILE_SELECTION( the_dlg->m_dialog ) );
	
	(the_dlg->m_callback)( selected_file, the_dlg->m_data );

	gtk_widget_destroy( GTK_WIDGET( the_dlg->m_dialog ) );
	delete the_dlg;
}

// s_cancel_callback
void	ay_file_selection_dlg::s_cancel_callback( GtkButton *button, void *data )
{
	ay_file_selection_dlg	*the_dlg = reinterpret_cast<ay_file_selection_dlg *>(data);
	
	assert( the_dlg != NULL );
	
	(the_dlg->m_callback)( NULL, the_dlg->m_data );

	gtk_widget_destroy( GTK_WIDGET( the_dlg->m_dialog ) );
	delete the_dlg;
}
