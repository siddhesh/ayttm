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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "util.h"
#include "globals.h"
//#include "prefs.h"
#include "action.h"
#include "ui_log_window.h"

#include "gtk_eb_html.h"

#include "pixmaps/cancel.xpm"
#include "pixmaps/action.xpm"



static void	s_free_string_list( gpointer data, gpointer user_data )
{
	g_free( data );
}

static void	s_free_gs_list( gpointer data, gpointer user_data )
{
	g_slist_foreach( (GSList*)data, (GFunc)s_free_string_list, NULL );
	g_slist_free( (GSList*)data );
}


class ay_log_window
{
	public:
		ay_log_window( struct contact *inRemoteContact );
		ay_log_window( const char *inFileName );
		~ay_log_window( void );
		
	private:	// Gtk callbacks
		static void		s_destroy_callback( GtkWidget* widget, gpointer data );
		static void		s_close_callback( GtkWidget* widget, gpointer data );
		static void		s_action_callback( GtkWidget *widget, gpointer data );
		static void		s_select_date_callback( GtkCList *clist, int row, int column, 
				  			GdkEventButton *event, gpointer user_data );
		static gboolean s_search_callback( GtkWidget *widget, GdkEventKey *event, gpointer data );
	
	private:
		void	Init( const char *inTitle );
		void	AddIconToToolbar( char **inXPM, const char *inText, GtkSignalFunc inSignal, GtkToolbar *ioToolbar );
		
		void	SetHTMLText( GSList* gl );
		void	LoadInfo( void );

		
		GtkWidget	*m_window;
		GtkWidget	*m_date_list;
		GtkWidget	*m_date_scroller;
		GtkWidget	*m_html_display;
		GtkWidget	*m_html_scroller;
		GtkWidget	*m_date_html_hbox;

		struct contact	*m_remote;
		const char		*m_filename;
		GSList			*m_entries;   /* list of gslists */
		FILE			*m_fp;
		long			m_filepos;
		
		int		m_search_current_row;
		int		m_search_last_pos;
};


#ifdef __cplusplus
extern "C" {
#endif

/// Entry point to this file - construct and display a log window
t_log_window_id ay_ui_log_window_contact_create( struct contact *inRemoteContact )
{
	assert( inRemoteContact != NULL );
	
	ay_log_window	*new_window = new ay_log_window( inRemoteContact );
	
	return( new_window );
}

/// Entry point to this file - construct and display a log window
t_log_window_id ay_ui_log_window_file_create( const char *inFileName )
{
	assert( inFileName != NULL );
	
	ay_log_window	*new_window = new ay_log_window( inFileName );
	
	return( new_window );
}

#ifdef __cplusplus
}
#endif

////////////////
//// ay_log_window implementation

ay_log_window::ay_log_window( struct contact *inRemoteContact )
:	m_remote( inRemoteContact )
{
	m_filename = NULL;
	m_fp = NULL;
	m_filepos = 0;
	
	m_remote->logwindow = this;

	char	name_buffer[NAME_MAX];

	make_safe_filename( name_buffer, m_remote->nick, m_remote->group->name );
	m_filename = strdup( name_buffer );
	
	const int	title_len = 512;
	char 		title[title_len];
	
	snprintf( title, title_len, _("Past conversations with %s"), m_remote->nick );
	
	Init( title );
}

ay_log_window::ay_log_window( const char *inFileName )
:	m_remote( NULL )
{
	m_filename = NULL;
	m_fp = NULL;
	m_filepos = 0;
	
	Init( _("Past conversation") );
}

// Init
void	ay_log_window::Init( const char *inTitle )
{	
	m_html_display = NULL;
	m_html_scroller = NULL;
	m_search_current_row = -1;
	m_search_last_pos = -1;

	m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize( GTK_WIDGET(m_window) );
	gtk_window_set_position( GTK_WINDOW(m_window), GTK_WIN_POS_MOUSE );

	gtk_window_set_title( GTK_WINDOW(m_window), inTitle );
	gtk_container_set_border_width( GTK_CONTAINER(m_window), 5 );

	/* for the html and list box */
	m_date_html_hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( GTK_WIDGET(m_date_html_hbox) );

	m_date_list = gtk_clist_new( 1 );
	gtk_widget_show( GTK_WIDGET(m_date_list) );
	gtk_clist_set_column_width( GTK_CLIST(m_date_list), 0, 100 );
	gtk_clist_set_selection_mode( GTK_CLIST(m_date_list), GTK_SELECTION_BROWSE );

	/* scroll window for the date list */
	m_date_scroller = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_show( GTK_WIDGET(m_date_scroller) );
	gtk_widget_set_usize( m_date_scroller, 180, 150 );
	gtk_container_add( GTK_CONTAINER(m_date_scroller), m_date_list );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_date_scroller), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
	gtk_box_pack_start( GTK_BOX(m_date_html_hbox), m_date_scroller, TRUE, TRUE, 5 );
	
	GtkWidget	*vbox = gtk_vbox_new( FALSE, 0 );  
	gtk_widget_show(GTK_WIDGET(vbox));
	gtk_box_pack_start( GTK_BOX(vbox), m_date_html_hbox, TRUE, TRUE, 5 );
	gtk_container_add( GTK_CONTAINER(m_window), vbox );
	
	gtk_signal_connect( GTK_OBJECT(m_window), "destroy", GTK_SIGNAL_FUNC(s_destroy_callback), this );
	
	/* buttons toolbar */
	GtkWidget	*toolbar = gtk_toolbar_new( GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS );
	gtk_toolbar_set_button_relief( GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE );
	gtk_container_set_border_width( GTK_CONTAINER(toolbar), 0 );

	/* search field */
	GtkWidget	*hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show(GTK_WIDGET(hbox));  
	
	GtkWidget	*search_label = gtk_label_new( _("Search: ") );
	gtk_widget_show( search_label );
	gtk_box_pack_start( GTK_BOX(hbox), search_label, FALSE, FALSE, 0 );
	
	GtkWidget	*search_entry = gtk_entry_new();
	gtk_widget_show( search_entry );
	gtk_box_pack_start( GTK_BOX(hbox), search_entry, TRUE, TRUE, 0 );
	gtk_signal_connect( GTK_OBJECT(search_entry), "key_press_event", GTK_SIGNAL_FUNC(s_search_callback), this );

	/* icon toolbar */
	gtk_toolbar_append_space( GTK_TOOLBAR(toolbar) );

	AddIconToToolbar( action_xpm, _("Actions..."), GTK_SIGNAL_FUNC(s_action_callback), GTK_TOOLBAR(toolbar) );
	
	gtk_toolbar_append_space( GTK_TOOLBAR(toolbar) );
	GtkWidget	*separator = gtk_vseparator_new();
	gtk_widget_set_usize( GTK_WIDGET(separator), 0, 20 );
	gtk_toolbar_append_widget( GTK_TOOLBAR(toolbar), separator, NULL, NULL );
	gtk_widget_show( separator );
	gtk_toolbar_append_space( GTK_TOOLBAR(toolbar) );

	AddIconToToolbar( cancel_xpm, _("Close..."), GTK_SIGNAL_FUNC(s_close_callback), GTK_TOOLBAR(toolbar) );

	gtk_widget_show(GTK_WIDGET(toolbar));

	gtk_box_pack_end(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* connect signals to the date list */
	gtk_signal_connect( GTK_OBJECT(m_date_list), "select-row", GTK_SIGNAL_FUNC(s_select_date_callback), this );
	
	LoadInfo();  

	gtk_widget_show(GTK_WIDGET(m_window));

	gtk_clist_select_row(GTK_CLIST(m_date_list), 0, 0);
}

ay_log_window::~ay_log_window( void )
{
	g_slist_foreach( m_entries, (GFunc)s_free_gs_list, NULL );

	if ( m_remote )
		m_remote->logwindow = NULL;

	if ( m_filename )
		free( (char *)m_filename );
}

// AddIconToToolbar
void	ay_log_window::AddIconToToolbar( char **inXPM, const char *inText, GtkSignalFunc inSignal, GtkToolbar *ioToolbar )
{
	GdkBitmap	*mask = NULL;
	
	GdkPixmap	*icon = gdk_pixmap_create_from_xpm_d( m_window->window, &mask, NULL, inXPM );
	GtkWidget	*iconwid = gtk_pixmap_new(icon, mask);
	gtk_widget_show( iconwid );
	
	GtkWidget	*widget = gtk_toolbar_append_item( ioToolbar, inText, inText, inText, iconwid, inSignal, this ); 
	
	gtk_widget_show( widget );
}

/** Sets the html widget to display the log text */
void	ay_log_window::SetHTMLText( GSList *gl )
{
	/* need to destroy the html widget, and add it again */
	if ( m_html_display != NULL )
		gtk_widget_destroy( m_html_display );

	if ( m_html_scroller != NULL )
		gtk_widget_destroy( m_html_scroller );

	/* add in the html display */
	m_html_display = ext_gtk_text_new( NULL, NULL );
	gtk_widget_show(GTK_WIDGET(m_html_display));
	gtk_eb_html_init( EXT_GTK_TEXT(m_html_display) );

	/* need to add scrollbar to the html text box*/
	m_html_scroller = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_set_usize( m_html_scroller, 375, 250 );
	gtk_container_add( GTK_CONTAINER(m_html_scroller), m_html_display );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_html_scroller), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

	gtk_box_pack_end( GTK_BOX(m_date_html_hbox), m_html_scroller, TRUE, TRUE, 5 );  

	gtk_widget_show( m_html_scroller );
	gtk_widget_show( m_html_display );  

	ext_gtk_text_freeze( EXT_GTK_TEXT(m_html_display) );

	GSList	*h = gl;  

	while ( h != NULL )
	{
		if ( h->data != NULL ) 
			gtk_eb_html_log_parse_and_add( EXT_GTK_TEXT(m_html_display), (char *)h->data );

		h = g_slist_next( h );
	}

	ext_gtk_text_thaw( EXT_GTK_TEXT(m_html_display) );
}

// LoadInfo
void	ay_log_window::LoadInfo( void )
{
	assert( m_filename != NULL );
  
	GSList	*gl = g_slist_alloc();
	char	*p3[1];
	bool	is_empty = true;

	
	p3[0] = NULL;
	m_fp = fopen( m_filename, "r" );
	
	gtk_clist_freeze( GTK_CLIST(m_date_list) );
	
	if ( m_fp != NULL )
	{
		GSList		*gl1 = NULL;
		char		posbuf[128];
		int			idx = 0;
		long		fpos = 0;
		const int	read_buf_len = 4096;
		char		read_buffer[read_buf_len];

		while ( fgets( read_buffer, read_buf_len, m_fp ) != NULL )
		{
			is_empty = false;
			
			char	*p1 = strstr( read_buffer, _("Conversation started on ") );
			
			if ( p1 != NULL ) 
			{
				char	date_buffer[128];
				
				/* extract the date, I am not sure if this is portable (but it
				works on my machine, so if someone wants to hack it, go ahead */
				p1 += strlen( _("Conversation started on ") );

				/* if html tags are in the buffer, gotta strip the last two tags */
				const char	*p2 = strstr( read_buffer, "</B>" );
				if ( p2 != NULL && p2 > p1 )
				{
					strncpy( date_buffer, p1, (p2 - p1) > 127 ? 127:(p2-p1) );
					date_buffer[(p2 - p1) > 127 ? 127:(p2-p1)] = '\0';
				}
				else
				{
					int len = (read_buffer + strlen(read_buffer)) - p1;
					
					if ( len > 127 )
						len = 127;
						
					if ( len > 0 )
					{
						strncpy( date_buffer, p1, len );
						date_buffer[len - 1] = '\0';
					}
				}

				p3[0] = date_buffer;
				idx = gtk_clist_append( GTK_CLIST(m_date_list), p3 );

				/* add new gslist entry */
				gl1 = g_slist_alloc();
				snprintf( posbuf, sizeof(posbuf), "%lu", fpos );
				gl = g_slist_append( gl, gl1 );	
				gl1 = g_slist_append( gl1, strdup(posbuf) );
				eb_debug( DBG_CORE,"%s at %s\n", p3[0], posbuf );

				/* set the datapointer for the clist */
				gtk_clist_set_row_data( GTK_CLIST(m_date_list), idx, gl1 );
			}
			else if ( (p1 = strstr(read_buffer, _("Conversation ended on "))) == NULL )
			{
				if ( gl1 != NULL )
				{
					gl1 = g_slist_append( gl1, strdup(read_buffer) );
				}
				else
				{
					char	buff1[read_buf_len];
					
					p3[0] = _("Undated Log");

					strncpy( buff1, read_buffer, read_buf_len );
					idx = gtk_clist_append( GTK_CLIST(m_date_list), p3 );
					
					/* add new gslist entry */
					snprintf( posbuf, sizeof(posbuf), "%lu", ftell(m_fp) );
					gl = g_slist_append( gl, gl1 );	
					gl1 = g_slist_append( gl1, strdup(posbuf) );
					eb_debug( DBG_CORE,"%s at %s\n", p3[0], posbuf );

					/* set the datapointer for the clist */
					gtk_clist_set_row_data( GTK_CLIST(m_date_list), idx, gl1 );
					gl1 = g_slist_append( gl1, strdup(buff1) );
				}
			}
			
			fpos = ftell( m_fp );
		}
		
		fclose( m_fp );
		m_fp = NULL;
	}
	else
	{
		perror( m_filename );
	}

	if ( is_empty )
	{
		p3[0] = _("No log available");
		gtk_clist_append( GTK_CLIST(m_date_list), p3 );
	}	  

	m_entries = gl;

	gtk_clist_thaw( GTK_CLIST(m_date_list) );
}

////
// ay_log_window callbacks

// s_destroy_callback
void	ay_log_window::s_destroy_callback( GtkWidget* widget, gpointer data )
{
	ay_log_window	*lw = reinterpret_cast<ay_log_window*>(data);

	assert( lw != NULL );

	gtk_widget_destroy( lw->m_window );
	delete lw;
}

// s_close_callback
void	ay_log_window::s_close_callback( GtkWidget* widget, gpointer data )
{
	ay_log_window	*lw = reinterpret_cast<ay_log_window*>(data);

	assert( lw != NULL );

	gtk_widget_destroy( lw->m_window );
}

// s_action_callback
void	ay_log_window::s_action_callback( GtkWidget *widget, gpointer data )
{
	ay_log_window	*lw = reinterpret_cast<ay_log_window *>( data );
	
	assert( lw != NULL );
	
	log_file	*logfile = ay_log_file_create( lw->m_filename );
	
	logfile->fp = lw->m_fp;
	logfile->filepos = lw->m_filepos;
	
	conversation_action( logfile, FALSE );
	
	ay_log_file_destroy_no_close( &logfile );
}

// s_select_date_callback
void	ay_log_window::s_select_date_callback( GtkCList *clist, int row, int column, 
				  GdkEventButton *event, gpointer user_data )
{
	ay_log_window	*lw = reinterpret_cast<ay_log_window *>( user_data );
	
	assert( lw != NULL );
	
	GSList	*string_list = (GSList*)gtk_clist_get_row_data( GTK_CLIST(clist), row );
	
	while ( string_list && !string_list->data )
	{
		string_list = g_slist_next(string_list);
	}
		
	if ( string_list && string_list->data )
	{
		lw->m_filepos = atol( (char *)string_list->data );
		
		eb_debug( DBG_CORE,"m_filepos now %lu\n", lw->m_filepos );
		
		string_list = g_slist_next(string_list);
	}
	else 
	{
		eb_debug( DBG_CORE, "string_list->data null\n" );
	}

	lw->SetHTMLText( string_list );
	lw->m_search_current_row = row;
	lw->m_search_last_pos = 0;
}

// s_search_callback
gboolean ay_log_window::s_search_callback( GtkWidget *widget, GdkEventKey *event, gpointer data )
{
	ay_log_window	*lw = reinterpret_cast<ay_log_window *>( data );
	
	assert( lw != NULL );
	
	int cur_pos = 0;
	int	cur_log = lw->m_search_current_row;

	bool looped = false;
	
	/* if user wants to search */
	if ( event->keyval == GDK_Return )
	{
		char	*text = gtk_editable_get_chars( GTK_EDITABLE(widget), 0, -1 );
		
		strip_html( text );
		if ( !text || strlen(text) == 0 )
			return( TRUE );

		eb_debug( DBG_CORE,"search: begin at cur_log:%d\n", cur_log );

		while ( !looped )
		{
			int		total_length = 0;
			GSList	*c_slist = NULL;
			GSList	*s_list = NULL;
			
			/* get the selected conversation's log */
			s_list = (GSList*)gtk_clist_get_row_data( GTK_CLIST(lw->m_date_list), cur_log );
			if ( s_list == NULL )
			{
				/* end of the conversation's list, back to the begining */
				s_list = (GSList*)gtk_clist_get_row_data( GTK_CLIST(lw->m_date_list), 0 );
				/* reset */
				lw->m_search_current_row = 0;
				lw->m_search_last_pos = 0;
				cur_log = cur_pos = total_length = 0;
				
				eb_debug(DBG_CORE,"search:looped\n");
				looped = true;
			}
			
			if ( s_list == NULL )
			{
				eb_debug(DBG_CORE, "search: breaking\n");
				break;
			}

			/* keep a pointer to the beginning of the conversation 
			* as we'll move s_list pointer */
			c_slist = s_list;
			while ( s_list && cur_pos <= lw->m_search_last_pos )
			{
				s_list = s_list->next;
				
				if(s_list && s_list->data && cur_pos < lw->m_search_last_pos)
				{
					char	*list_text = (char *)s_list->data;
					char	*no_html_text = NULL;
					
					if ( list_text != NULL ) 
						no_html_text = strdup( list_text );
						
					if ( no_html_text )
					{
						strip_html( no_html_text );
						total_length += strlen( (char*)no_html_text );
						free( no_html_text );
					}
					
					eb_debug( DBG_CORE,"search:adding to %d\n", total_length );
				}
				
				if ( s_list )
					cur_pos++;
			}
		
			while ( s_list && s_list->data )
			{
				char	*list_text = (char *)s_list->data;
				char	*no_html_text = NULL;
				
				if ( list_text ) 
					no_html_text = strdup( list_text );
					
				if ( no_html_text ) 
					strip_html( no_html_text );
					
				/* found string ? */
				if ( no_html_text && strstr( no_html_text, text ) != NULL )
				{
					/* go to the the found conversation */
					gtk_clist_select_row( GTK_CLIST(lw->m_date_list), cur_log, 0 );
					gtk_clist_moveto( GTK_CLIST(lw->m_date_list), cur_log, 0, 0, 0 );
								
					/* update the conversation log */			     
					lw->SetHTMLText( c_slist );
					
					GtkEditable	*editable = GTK_EDITABLE( lw->m_html_display );

					lw->m_search_last_pos = cur_pos + 1;
					lw->m_search_current_row = cur_log;
					
					/* select the matching substring */
					eb_debug( DBG_CORE, "search: total2=%d (%d,%s)\n", total_length, cur_log, strstr(no_html_text,text) );
					gtk_editable_set_position ( editable,
									total_length+(int)(strstr(no_html_text,text)-no_html_text));
					gtk_editable_select_region( editable,
									total_length+(int)(strstr(no_html_text,text)-no_html_text),
									total_length+(int)(strstr(no_html_text,text)-no_html_text)+strlen(text));
					
					total_length += strlen( no_html_text );
					
					if ( text != NULL )
						free( text );
						
					return( TRUE );
				}
				else if ( no_html_text != NULL )
				{
					total_length += strlen( no_html_text );
				}
				
				if ( no_html_text != NULL )
					free( no_html_text );
					
				/* next line */
				s_list = s_list->next;
				cur_pos++; 
				lw->m_search_last_pos = cur_pos;
			}
			
			/* next conversation */
			eb_debug( DBG_CORE,"search: next one\n" );
			cur_log++;
			cur_pos = 0;
			lw->m_search_last_pos = 0;

			lw->m_search_current_row = cur_log;
		}
		
		if ( text != NULL )
			free( text );
	}
	return( TRUE );
}
