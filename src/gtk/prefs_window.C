/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
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

#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ui_prefs_window.h"

#include "service.h"
#include "util.h"
#include "libproxy/libproxy.h"
#include "sound.h"
#include "value_pair.h"
#include "gtk_globals.h"
#include "status.h"
#include "plugin.h"
#include "file_select.h"
#include "gtkutils.h"

#ifdef HAVE_ISPELL
#include "gtkspell.h"
#endif

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/error.xpm"


// forward declaration
class ay_prefs_window_panel;


/// The main prefs window
class ay_prefs_window
{
	private:
		ay_prefs_window( struct prefs &inPrefs );
		
	public:
		~ay_prefs_window( void );
		
		static ay_prefs_window	&Create( struct prefs *inPrefs )
		{
			if ( s_only_prefs_window == NULL )
			{
				assert( inPrefs != NULL );
				
				s_only_prefs_window = new ay_prefs_window( *inPrefs );
			}
				
			return( *s_only_prefs_window );
		}
		
		void	Show( void );
	
	public:
		enum ePanelID
		{
			PANEL_GENERAL = 0,
			PANEL_LOGGING,
			PANEL_SOUND_GENERAL,
			PANEL_SOUND_FILES,
			PANEL_CHAT_GENERAL,
			PANEL_CHAT_TABS,
			PANEL_ADVANCED,
			PANEL_PROXY,
#ifdef HAVE_ICONV
			PANEL_ENCODING,
#endif
			PANEL_SERVICES,
			PANEL_UTILITIES,
			
			PANEL_MAX
		};

	private:	// Gtk callbacks
		static void		s_tree_item_selected( GtkWidget *widget, gpointer data );
		static void		s_ok_callback( GtkWidget *widget, gpointer data );
		static void		s_cancel_callback( GtkWidget *widget, gpointer data );
	
	private:		
		static const char	*s_titles[PANEL_MAX];
		
	private:	
		void	AddToTree( const char *inName, ay_prefs_window_panel *inPanel );
		
		void	OK( void );
		void	Cancel( void );
		
		static ay_prefs_window	*s_only_prefs_window;		///< the instance of the prefs window
		
		struct prefs			&m_prefs;
		GtkWidget				*m_prefs_window_widget;	///< the actual dialog widget
		
		GtkTree					*m_tree;
		GList					*m_panels;		///< a list of the panels (ay_prefs_window_panel *)
};

/// A prefs panel
class ay_prefs_window_panel
{
	protected:
		ay_prefs_window_panel( const char *inTopFrameText );
		
	public:
		virtual ~ay_prefs_window_panel( void );
		
		void		SetNotebookID( int inID ) { m_notebook_id = inID; }
		
		const char	*Name( void ) const { return( m_name ); }
		GtkWidget	*TopLevelWidget( void ) { return( m_top_vbox ); }
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
		
		void		Show( void );
		
		static ay_prefs_window_panel	*Create( GtkWidget *inParentWindow, GtkWidget *inParent, struct prefs &inPrefs,
			ay_prefs_window::ePanelID inPanelID, const char *inName );

		static ay_prefs_window_panel	*CreateModulePanel( GtkWidget *inParentWindow, GtkWidget *inParent, t_module_pref &inPrefs );

	protected:
		GtkWidget	*m_top_vbox;
		GtkWidget	*m_parent_window;
	
	private:
		void	AddTopFrame( const char *in_text );
		
		const char					*m_name;
		GtkWidget					*m_parent;
		int							m_notebook_id;
		struct prefs				*m_prefs;
		ay_prefs_window::ePanelID	m_panel_id;
};

/// General prefs panel
class ay_general_panel : public ay_prefs_window_panel
{
	public:
		ay_general_panel( const char *inTopFrameText, struct prefs::general &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
	
	private:	// Gtk callbacks
		static void		s_toggle_checkbox( GtkWidget *widget, int *data );
		static void		s_set_browser_path( char *selected_filename, void *data );
		static void		s_get_alt_browser_path( GtkWidget *t_browser_browse_button, int *data );
		
	private:
		void	SetActiveWidgets( void );
		
		struct prefs::general &m_prefs;
		
		GtkWidget	*m_dictionary_entry;
		GtkWidget	*m_alternate_browser_entry;
		GtkWidget	*m_browser_browse_button;
};

/// Logging prefs panel
class ay_logging_panel : public ay_prefs_window_panel
{
	public:
		ay_logging_panel( const char *inTopFrameText, struct prefs::logging &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		
	private:
		struct prefs::logging &m_prefs;
};


/// Sound prefs panel
class ay_sound_general_panel : public ay_prefs_window_panel
{
	public:
		ay_sound_general_panel( const char *inTopFrameText, struct prefs::sound &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
	
	private:
		struct prefs::sound &m_prefs;
};

/// Sound:Files panel
class ay_sound_files_panel : public ay_prefs_window_panel
{
	public:
		ay_sound_files_panel( const char *inTopFrameText, struct prefs::sound &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
	
	private:	// Gtk callbacks
		static void		s_setsoundfilename( char *selected_filename, void *data );
		static void		s_getsoundfile( GtkWidget *widget, void *data );
		static void		s_testsoundfile( GtkWidget *widget, void *data );
		static void		s_soundvolume_changed( GtkAdjustment *adjust, void *data );

		/// Callback data struct for the files panel
		typedef struct
		{
			ay_sound_files_panel	*m_panel;
			int						m_sound_id;
		} t_cb_data;
		
		t_cb_data	m_cb_data[SOUND_MAX];

	private:
		GtkWidget	*AddSoundFileSelectionBox( const char *inLabelString,
						const char *inInitialFilename, int inSoundID );
							
		GtkWidget	*AddSoundVolumeSelectionBox( const char *inLabelString, GtkAdjustment *inAdjustment );
		
		struct prefs::sound &m_prefs;
		
		GtkWidget	*m_arrivesound_entry;
		GtkWidget	*m_awaysound_entry;
		GtkWidget	*m_leavesound_entry;
		GtkWidget	*m_sendsound_entry;
		GtkWidget	*m_receivesound_entry;
		GtkWidget	*m_firstmsgsound_entry;
		GtkWidget	*m_volumesound_entry;
};

/// Chat prefs panel
class ay_chat_panel : public ay_prefs_window_panel
{
	public:
		ay_chat_panel( const char *inTopFrameText, struct prefs::chat &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );		
		
	private:
		struct prefs::chat &m_prefs;
		
		GtkWidget		*m_font_size_entry;
};

/// Chat:Tabs prefs panel
class ay_tabs_panel : public ay_prefs_window_panel
{
	public:
		ay_tabs_panel( const char *inTopFrameText, struct prefs::tabs &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );		
	
	private:	// Gtk callbacks
		static void		s_set_tabbed( GtkWidget *w, void *data );
		static void		s_change_orientation( GtkWidget *widget, void *data );
		static gboolean	s_newkey_callback( GtkWidget *keybutton, GdkEventKey *event, void *data );
		static void		s_getnewkey( GtkWidget *keybutton, void *data );

		/// Callback data struct for the tabs panel
		typedef struct
		{
			ay_tabs_panel	*m_panel;
			
			union
			{
				int				m_orientation_id;
				int				*m_toggle_data;
				GdkDeviceKey	*m_device_key;
			};
		} t_cb_data;
		
		t_cb_data	m_orientation_cb_data[4];	///< callback data for each orientation
		t_cb_data	m_key_cb_data[2];			///< callback data for each key
		t_cb_data	m_toggle_data;				///< callback data for the toggle button
		
	private:
		void	AddKeySet( const char *labelString, t_cb_data *cb_data, GtkWidget *vbox );
		void	SetActiveWidgets( void );
		
		struct prefs::tabs &m_prefs;
		
		GtkWidget		*m_orientation_frame;
		GtkWidget		*m_hotkey_frame;

		GdkDeviceKey 	m_local_accel_prev_tab;
		GdkDeviceKey 	m_local_accel_next_tab;

		guint 			m_accel_change_handler_id;
};

/// Advanced prefs panel
class ay_advanced_panel : public ay_prefs_window_panel
{
	public:
		ay_advanced_panel( const char *inTopFrameText );
		
		virtual void	Build( GtkWidget *inParent );
};

/// Proxy prefs panel
class ay_proxy_panel : public ay_prefs_window_panel
{
	public:
		ay_proxy_panel( const char *inTopFrameText, struct prefs::advanced &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
	
	private:	// Gtk callbacks
		static void		s_toggle_checkbox( GtkWidget *w, void *data );
		static void		s_set_proxy_type( GtkWidget *w, void *data );
		
		/// Callback data struct for the proxy panel
		typedef struct
		{
			ay_proxy_panel	*m_panel;
			
			union
			{
				eProxyType		m_proxy_type;
				int				*m_toggle_data;
			};
		} t_cb_data;
		
		t_cb_data	m_toggle_data;			///< callback data for the toggle button
		t_cb_data	m_cb_data[PROXY_MAX];	///< one for each proxy type
		t_cb_data	m_auth_cb_data;
		
	private:
		void	SetActiveWidgets( void );
		
		struct prefs::advanced &m_prefs;

		eProxyType	m_last_proxy_type;
		
		GtkWidget	*m_proxy_checkbox;
		
		GtkWidget	*m_proxy_frame;
		GtkWidget	*m_proxy_server_entry;
		GtkWidget	*m_proxy_port_entry;
		
		GtkWidget	*m_auth_toggle;
		GtkWidget	*m_proxy_user_entry;
		GtkWidget	*m_proxy_password_entry;
		
		bool		m_finished_construction;
};

#ifdef HAVE_ICONV
/// Encoding prefs panel
class ay_encoding_panel : public ay_prefs_window_panel
{
	public:
		ay_encoding_panel( const char *inTopFrameText, struct prefs::advanced &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
	
	private:	// Gtk callbacks
		static void		s_set_use_of_recoding( GtkWidget *widget, void *data );

	private:
		static const int	ENCODE_LEN;
		
		void	SetActiveWidgets( void );
		
		struct prefs::advanced &m_prefs;

		GtkWidget	*m_local_encoding_entry;
		GtkWidget	*m_remote_encoding_entry;
};
#endif	// HAVE_ICONV

/// Services prefs panel
class ay_services_panel : public ay_prefs_window_panel
{
	public:
		ay_services_panel( const char *inTopFrameText );
		
		virtual void	Build( GtkWidget *inParent );
};

/// Utilities prefs panel
class ay_utilities_panel : public ay_prefs_window_panel
{
	public:
		ay_utilities_panel( const char *inTopFrameText );
		
		virtual void	Build( GtkWidget *inParent );
};

/// A module prefs panel
class ay_module_panel : public ay_prefs_window_panel
{
	public:
		ay_module_panel( const char *inTopFrameText, t_module_pref &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
		
	private:
		static void		AddInfoRow( GtkWidget *inTable, int inRow, const char *inHeader, const char *inInfo );
		
		t_module_pref &m_prefs;
};

#ifdef __cplusplus
extern "C" {
#endif

/// Entry point to this file - construct and display the prefs window
void	ay_ui_prefs_window_create( struct prefs *inPrefs )
{
	ay_prefs_window::Create( inPrefs ).Show();
}

#ifdef __cplusplus
}
#endif

////////////////
//// ay_prefs_window implementation

const char	*ay_prefs_window::s_titles[PANEL_MAX] =
{
	_( "Preferences" ),
	_( "Preferences:Logging" ),
	_( "Sound" ),
	_( "Sound:Files" ),
	_( "Chat" ),
	_( "Chat:Tabs" ),
	_( "Advanced" ),
	_( "Advanced:Proxy" ),
#ifdef HAVE_ICONV
	_( "Advanced:Encoding" ),
#endif
	_( "Services" ),
	_( "Utilities" )
};

ay_prefs_window	*ay_prefs_window::s_only_prefs_window = NULL;

ay_prefs_window::ay_prefs_window( struct prefs &inPrefs )
:	m_prefs( inPrefs ),
	m_prefs_window_widget( NULL ),
	m_tree( NULL ),
	m_panels( NULL )
{
	m_prefs_window_widget = gtk_window_new( GTK_WINDOW_DIALOG );

	gtk_window_set_position( GTK_WINDOW(m_prefs_window_widget), GTK_WIN_POS_MOUSE );
	gtk_window_set_policy( GTK_WINDOW(m_prefs_window_widget), FALSE, FALSE, TRUE );
	gtk_widget_realize( m_prefs_window_widget );
	gtk_window_set_title( GTK_WINDOW(m_prefs_window_widget), _("Ayttm Preferences") );
	gtkut_set_window_icon( m_prefs_window_widget->window, NULL );
	gtk_container_set_border_width( GTK_CONTAINER(m_prefs_window_widget), 5 );
	
	GtkWidget	*main_hbox = gtk_hbox_new( FALSE, 5 );

	// a scrolled window for the tree
	GtkWidget	*scrolled_win = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize( scrolled_win, 210, 430 );
	gtk_box_pack_start( GTK_BOX(main_hbox), GTK_WIDGET(scrolled_win), FALSE, FALSE, 0 );

	m_tree = GTK_TREE(gtk_tree_new());
	gtk_tree_set_view_lines( m_tree, FALSE );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(scrolled_win), GTK_WIDGET(m_tree) );
	
	GtkWidget	*notebook = gtk_notebook_new();
	gtk_widget_set_usize( notebook, 410, -1 );
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), FALSE );
	
	for ( int i = PANEL_GENERAL; i < PANEL_MAX; i++ )
	{
		ay_prefs_window_panel	*the_panel = ay_prefs_window_panel::Create( m_prefs_window_widget, notebook, m_prefs,
				static_cast<ePanelID>(i), s_titles[i] );
		
		if ( the_panel == NULL )
			continue;
		
		m_panels = g_list_append( m_panels, the_panel );
			
		GtkWidget	*top_level_w = the_panel->TopLevelWidget();
		
		if ( top_level_w != NULL )
		{
			gtk_notebook_append_page( GTK_NOTEBOOK(notebook), top_level_w, gtk_label_new( s_titles[i] ) );
			
			the_panel->SetNotebookID( gtk_notebook_page_num( GTK_NOTEBOOK(notebook), top_level_w ) );
			
			AddToTree( the_panel->Name(), the_panel );
		}
	}
	
	// now add modules
	LList	*module_pref = m_prefs.module.module_info;
	
	while ( module_pref != NULL )
	{
		t_module_pref		*pref_info = reinterpret_cast<t_module_pref *>(module_pref->data);
		
		ay_prefs_window_panel	*the_panel = ay_prefs_window_panel::CreateModulePanel( m_prefs_window_widget, notebook, *pref_info );
		
		if ( the_panel != NULL )
		{
			m_panels = g_list_append( m_panels, the_panel );

			GtkWidget	*top_level_w = the_panel->TopLevelWidget();

			if ( top_level_w != NULL )
			{
				gtk_notebook_append_page( GTK_NOTEBOOK(notebook), top_level_w, NULL );

				the_panel->SetNotebookID( gtk_notebook_page_num( GTK_NOTEBOOK(notebook), top_level_w ) );

				AddToTree( the_panel->Name(), the_panel );
			}
		}
		
		module_pref = module_pref->next;
	}
	
	gtk_widget_show( notebook );
	gtk_widget_show( GTK_WIDGET(m_tree) );
	gtk_widget_show( scrolled_win );
	gtk_widget_show( main_hbox );

	GtkWidget	*prefs_vbox = gtk_vbox_new( FALSE, 5 );
	gtk_widget_show( prefs_vbox );
	gtk_box_pack_start( GTK_BOX(prefs_vbox), notebook, TRUE, TRUE, 0 );

	// OK Button
	GtkWidget	*hbox2 = gtk_hbox_new( TRUE, 5 );
	gtk_widget_show( hbox2 );
	gtk_widget_set_usize( hbox2, 200, 25 );

	GtkWidget	*button = gtkut_create_icon_button( _("OK"), ok_xpm, m_prefs_window_widget );
	gtk_widget_show( button );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC( s_ok_callback ), this );
	gtk_box_pack_start( GTK_BOX(hbox2), button, TRUE, TRUE, 5 );

	// Cancel Button
	button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, m_prefs_window_widget );
	gtk_widget_show( button );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC( s_cancel_callback ), this );
	gtk_box_pack_start( GTK_BOX(hbox2), button, TRUE, TRUE, 5 );

	GtkWidget	*hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );

	gtk_box_pack_end( GTK_BOX(hbox), hbox2, FALSE, FALSE, 5 );
	gtk_box_pack_start( GTK_BOX(prefs_vbox), hbox, FALSE, FALSE, 5 );
	gtk_box_pack_start( GTK_BOX(main_hbox), prefs_vbox, FALSE, FALSE, 3 );

	gtk_container_add( GTK_CONTAINER(m_prefs_window_widget), main_hbox );
}

ay_prefs_window::~ay_prefs_window( void )
{
	GList	*iter = m_panels;
	
	while ( iter != NULL )
	{
		ay_prefs_window_panel	*the_panel = reinterpret_cast<ay_prefs_window_panel *>(iter->data);
		delete the_panel;
		
		iter = g_list_next( iter );
	}
	g_list_free( m_panels );

#ifdef HAVE_ISPELL
	if ((!iGetLocalPref("do_spell_checking")) && (gtkspell_running()))
		gtkspell_stop();
#endif
	
	gtk_widget_destroy( m_prefs_window_widget );
		
	s_only_prefs_window = NULL;
}

// Show
void	ay_prefs_window::Show( void )
{
	if ( m_prefs_window_widget != NULL )
	{
		gtk_widget_show( m_prefs_window_widget );
	
		/* Because of some problem with GTK, we cannot create the tree expanded without creating drawing artifacts.
			So we expand the tree 'by hand' after we've shown the window.
		*/
		GList		*child = m_tree->children;
		GtkWidget	*the_tree_item = NULL;
		
		while ( child != NULL )
		{
			the_tree_item = GTK_WIDGET(child->data);
			
			gtk_tree_item_expand( GTK_TREE_ITEM(the_tree_item) );
			
			child = child->next;
		}
		
		gtk_tree_select_item( m_tree, 0 );
	}
}

// AddToTree
void	ay_prefs_window::AddToTree( const char *inName, ay_prefs_window_panel *inPanel )
{
	const int	name_len = 64;
	char		name[name_len];
	
	strncpy( name, inName, name_len );
	
	char		*child_name = strchr( name, ':' );
	GtkWidget	*section_tree = GTK_WIDGET(m_tree);
	
	if ( child_name != NULL )
	{
		*child_name = '\0';
		child_name++;
		
		// find the section in the tree
		GList		*child = m_tree->children;
		GtkWidget	*the_tree_item = NULL;
		
		while ( child != NULL )
		{
			the_tree_item = GTK_WIDGET(child->data);
			
			GtkLabel	*label = GTK_LABEL(GTK_BIN(the_tree_item)->child);
			gchar		*text = NULL;
			
			gtk_label_get( label, &text );
			
			if ( !strncmp( name, text, name_len ) )
				break;
				
			child = child->next;
		}
		
		// not found, so add it
		if ( child == NULL )
		{
			the_tree_item = gtk_tree_item_new_with_label( name );
			gtk_widget_show( the_tree_item );
			
			gtk_tree_append( GTK_TREE(section_tree), the_tree_item );
		}
		
		section_tree = GTK_TREE_ITEM_SUBTREE( GTK_TREE_ITEM(the_tree_item) );
		
		// we don't have a subtree yet, so make one
		if ( section_tree == NULL )
		{
			section_tree = gtk_tree_new();
			
			gtk_tree_item_set_subtree( GTK_TREE_ITEM(the_tree_item), section_tree );
			gtk_tree_set_view_lines( GTK_TREE(section_tree), TRUE );
		}
	}
	else
	{
		child_name = name;
	}
	
	GtkWidget	*item = gtk_tree_item_new_with_label( child_name );
	gtk_widget_show( item );

	gtk_tree_append( GTK_TREE(section_tree), item );

	gtk_signal_connect( GTK_OBJECT(item), "select", GTK_SIGNAL_FUNC(s_tree_item_selected), inPanel );
}

// OK
void	ay_prefs_window::OK( void )
{
	GList	*iter = m_panels;
	
	while ( iter != NULL )
	{
		ay_prefs_window_panel	*the_panel = reinterpret_cast<ay_prefs_window_panel *>(iter->data);
		
		if ( the_panel != NULL );
			the_panel->Apply();
			
		iter = g_list_next( iter );
	}

	ayttm_prefs_apply( &m_prefs );
}

// Cancel
void	ay_prefs_window::Cancel( void )
{
	ayttm_prefs_cancel( &m_prefs );
}

////
// ay_prefs_window callbacks

// tree_item_selected
void	ay_prefs_window::s_tree_item_selected( GtkWidget *widget, gpointer data )
{
	ay_prefs_window_panel	*the_panel = reinterpret_cast<ay_prefs_window_panel *>( data );

	the_panel->Show();
}

// s_ok_callback
void	ay_prefs_window::s_ok_callback( GtkWidget *widget, gpointer data )
{
	ay_prefs_window	*the_window = reinterpret_cast<ay_prefs_window *>( data );
	assert( the_window != NULL );
	
	the_window->OK();
	delete the_window;
}

// s_cancel_callback
void	ay_prefs_window::s_cancel_callback( GtkWidget *widget, gpointer data )
{
	ay_prefs_window	*the_window = reinterpret_cast<ay_prefs_window *>( data );
	assert( the_window != NULL );
	
	the_window->Cancel();
	delete the_window;
}

////////////////
//// ay_prefs_window_panel implementation

ay_prefs_window_panel::ay_prefs_window_panel( const char *inTopFrameText )
:	m_parent_window( NULL ),
	m_parent( NULL ),
	m_prefs( NULL ),
	m_panel_id( ay_prefs_window::PANEL_MAX )
{
	m_name = strdup( inTopFrameText );
	m_top_vbox = gtk_vbox_new( FALSE, 3 );
	gtk_widget_show( m_top_vbox );
	gtk_container_set_border_width( GTK_CONTAINER(m_top_vbox), 3 );
	
	if ( inTopFrameText )
		AddTopFrame( inTopFrameText );
}

// Create
ay_prefs_window_panel	*ay_prefs_window_panel::Create( GtkWidget *inParentWindow, GtkWidget *inParent, struct prefs &inPrefs,
	ay_prefs_window::ePanelID inPanelID, const char *inName )
{
	ay_prefs_window_panel	*new_panel = NULL;
	
	switch ( inPanelID )
	{
		case ay_prefs_window::PANEL_GENERAL:
			new_panel = new ay_general_panel( inName, inPrefs.general );
			break;
			
		case ay_prefs_window::PANEL_LOGGING:
			new_panel = new ay_logging_panel( inName, inPrefs.logging );
			break;

		case ay_prefs_window::PANEL_CHAT_TABS:
			new_panel = new ay_tabs_panel( inName, inPrefs.tabs );
			break;

		case ay_prefs_window::PANEL_SOUND_GENERAL:
			new_panel = new ay_sound_general_panel( inName, inPrefs.sound );
			break;

		case ay_prefs_window::PANEL_SOUND_FILES:
			new_panel = new ay_sound_files_panel( inName, inPrefs.sound );
			break;
			
		case ay_prefs_window::PANEL_CHAT_GENERAL:
			new_panel = new ay_chat_panel( inName, inPrefs.chat );
			break;

		case ay_prefs_window::PANEL_ADVANCED:
			new_panel = new ay_advanced_panel( inName );
			break;

		case ay_prefs_window::PANEL_PROXY:
			new_panel = new ay_proxy_panel( inName, inPrefs.advanced );
			break;

#ifdef HAVE_ICONV
		case ay_prefs_window::PANEL_ENCODING:
			new_panel = new ay_encoding_panel( inName, inPrefs.advanced );
			break;
#endif

		case ay_prefs_window::PANEL_SERVICES:
			new_panel = new ay_services_panel( inName );
			break;

		case ay_prefs_window::PANEL_UTILITIES:
			new_panel = new ay_utilities_panel( inName );
			break;
		
		default:
			assert( false );
			break;
	}
			
	assert( new_panel != NULL );
	
	new_panel->m_parent_window = inParentWindow;
	new_panel->m_parent = inParent;
	new_panel->Build( inParent );
	
	return( new_panel );
}

// CreateModulePanel
ay_prefs_window_panel	*ay_prefs_window_panel::CreateModulePanel( GtkWidget *inParentWindow, GtkWidget *inParent, t_module_pref &inPrefs )
{
	const int	name_len = 64;
	char		name[name_len];
	
	
	if ( !strcmp( inPrefs.module_type, "SERVICE" ) )
		snprintf( name, name_len, "%s:%s", _( "Services" ), inPrefs.service_name );
	else if ( !strcmp( inPrefs.module_type, "UTILITY" ) )
		snprintf( name, name_len, "%s:%s", _( "Utilities" ), inPrefs.brief_desc );
	else
		snprintf( name, name_len, "%s:%s", _( "Other Plugins" ), inPrefs.brief_desc );
	
	ay_prefs_window_panel	*new_panel = new_panel = new ay_module_panel( name, inPrefs );
			
	assert( new_panel != NULL );
	
	new_panel->m_parent_window = inParentWindow;
	new_panel->m_parent = inParent;
	new_panel->Build( inParent );
	
	return( new_panel );
}

ay_prefs_window_panel::~ay_prefs_window_panel( void )
{
	free( (void *)m_name );
}

// Build
void	ay_prefs_window_panel::Build( GtkWidget *inParent )
{
	// default - do nothing
}

// Apply
void	ay_prefs_window_panel::Apply( void )
{	
	// default - do nothing
}

// Show
void	ay_prefs_window_panel::Show( void )
{
	gtk_notebook_set_page( GTK_NOTEBOOK(m_parent), m_notebook_id );
}

// AddTopFrame
void	ay_prefs_window_panel::AddTopFrame( const char *in_text )
{
	const char	*ayttm_yellow = "#F9E589";
	GdkColor	the_color;
	
	gdk_color_parse( ayttm_yellow, &the_color );
	
	GtkRcStyle	*rc_style = gtk_rc_style_new();
	rc_style->bg[GTK_STATE_NORMAL] = the_color;
	rc_style->color_flags[GTK_STATE_NORMAL] = static_cast<GtkRcFlags>(rc_style->color_flags[GTK_STATE_NORMAL] | GTK_RC_BG);

	GtkWidget	*frame = gtk_frame_new( NULL );
	gtk_widget_show( frame );
	gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );
	gtk_widget_modify_style( frame, rc_style );
	gtk_container_set_border_width( GTK_CONTAINER(frame), 1 );

	GtkWidget	*useless_event_box_because_of_stupid_gtk_colour_handling = gtk_event_box_new();
	gtk_widget_show( useless_event_box_because_of_stupid_gtk_colour_handling );
	gtk_widget_modify_style( useless_event_box_because_of_stupid_gtk_colour_handling, rc_style );
	gtk_rc_style_unref( rc_style );
	gtk_container_add( GTK_CONTAINER(frame), useless_event_box_because_of_stupid_gtk_colour_handling );
	
	GtkWidget	*label = gtk_label_new( in_text );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC(label), 0.0, 0.5 );
	gtk_misc_set_padding( GTK_MISC(label), 4, 3 );
	gtk_container_add( GTK_CONTAINER(useless_event_box_because_of_stupid_gtk_colour_handling), label );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), frame, FALSE, FALSE, 0 );
}

////////////////
//// ay_general_panel implementation

ay_general_panel::ay_general_panel( const char *inTopFrameText, struct prefs::general &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_dictionary_entry( NULL ),
	m_alternate_browser_entry( NULL ),
	m_browser_browse_button( NULL )
{
}

// Build
void	ay_general_panel::Build( GtkWidget *inParent )
{
	GtkWidget	*brbutton = NULL;
	GtkWidget	*hbox = NULL;
	GtkWidget	*spacer = NULL;
	GtkWidget	*label = NULL;

#ifdef HAVE_ISPELL
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	brbutton = gtkut_button( _("Use spell checking"), &m_prefs.do_spell_checking, hbox );
	gtk_signal_connect( GTK_OBJECT(brbutton), "clicked", GTK_SIGNAL_FUNC(s_toggle_checkbox), this );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
	
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, 15, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Dictionary (blank for default):") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
	
	m_dictionary_entry = gtk_entry_new();
	gtk_widget_show( m_dictionary_entry );
	gtk_entry_set_text( GTK_ENTRY(m_dictionary_entry), m_prefs.spell_dictionary );
	gtk_box_pack_start( GTK_BOX(hbox), m_dictionary_entry, TRUE, TRUE, 10 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
#endif

	brbutton = gtkut_button( _("Use alternate browser"), &m_prefs.use_alternate_browser, m_top_vbox );
	gtk_signal_connect( GTK_OBJECT(brbutton), "clicked", GTK_SIGNAL_FUNC(s_toggle_checkbox), this );

	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, 15, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Browser command:") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

	m_alternate_browser_entry = gtk_entry_new();
	gtk_widget_show( m_alternate_browser_entry );
	gtk_entry_set_text( GTK_ENTRY(m_alternate_browser_entry), m_prefs.alternate_browser );
	gtk_box_pack_start( GTK_BOX(hbox), m_alternate_browser_entry, TRUE, TRUE, 10 );

	m_browser_browse_button = gtk_button_new_with_label( _("Browse") );
	gtk_widget_show( m_browser_browse_button );
	gtk_signal_connect( GTK_OBJECT(m_browser_browse_button), "clicked", GTK_SIGNAL_FUNC(s_get_alt_browser_path), this );
	gtk_box_pack_start( GTK_BOX(hbox), m_browser_browse_button, FALSE, FALSE, 5 );

	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );

	gtkut_button( _("Enable debug messages"), &m_prefs.do_ayttm_debug, m_top_vbox );
	
	SetActiveWidgets();
}

// Apply
void	ay_general_panel::Apply( void )
{
#ifdef HAVE_ISPELL
	if ( m_prefs.do_spell_checking )
	{
		const char	*new_dict = gtk_entry_get_text( GTK_ENTRY(m_dictionary_entry) );

		if ( (new_dict != NULL) && strncmp( new_dict, m_prefs.spell_dictionary, MAX_PREF_LEN ) )
			gtkspell_stop();
	}
	
	strncpy( m_prefs.spell_dictionary, gtk_entry_get_text(GTK_ENTRY(m_dictionary_entry)), MAX_PREF_LEN );
#endif

	char	alt_browser_command[MAX_PREF_LEN];
	
	strncpy( alt_browser_command, gtk_entry_get_text(GTK_ENTRY(m_alternate_browser_entry)), MAX_PREF_LEN );
	
	// add "%s" for the URL if the user didn't
	if ( (alt_browser_command[0] != '\0') && !strstr( alt_browser_command, "%s" ) )
		strncat( alt_browser_command, " %s", MAX_PREF_LEN );
	
	strncpy( m_prefs.alternate_browser, alt_browser_command, MAX_PREF_LEN );
}

// SetActiveWidgets
void	ay_general_panel::SetActiveWidgets( void )
{
	gtk_widget_set_sensitive( m_alternate_browser_entry, m_prefs.use_alternate_browser );
	gtk_widget_set_sensitive( m_browser_browse_button, m_prefs.use_alternate_browser );
#ifdef HAVE_ISPELL
	gtk_widget_set_sensitive( m_dictionary_entry, m_prefs.do_spell_checking );
#endif
}

////
// ay_general_panel callbacks

// s_toggle_checkbox
void	ay_general_panel::s_toggle_checkbox( GtkWidget *widget, int *data )
{
	ay_general_panel	*the_panel = reinterpret_cast<ay_general_panel *>( data );
	assert( the_panel != NULL );

	the_panel->SetActiveWidgets();
}

// s_set_browser_path
void	ay_general_panel::s_set_browser_path( char *selected_filename, void *data )
{
	if ( selected_filename == NULL )
		return;
	
	ay_general_panel	*the_panel = reinterpret_cast<ay_general_panel *>( data );
	assert( the_panel != NULL );
		
	gtk_entry_set_text( GTK_ENTRY(the_panel->m_alternate_browser_entry), selected_filename );
}

// s_get_alt_browser_path
void	ay_general_panel::s_get_alt_browser_path( GtkWidget *t_browser_browse_button, int *data )
{
	eb_debug(DBG_CORE, "Just entered get_alt_browser_path\n");
	
	ay_general_panel	*the_panel = reinterpret_cast<ay_general_panel *>( data );
	assert( the_panel != NULL );

	char	*alt_browser_text = gtk_entry_get_text( GTK_ENTRY(the_panel->m_alternate_browser_entry) );
	eb_do_file_selector( alt_browser_text, _("Select your browser"),
		ay_general_panel::s_set_browser_path, the_panel );
}


////////////////
//// ay_logging_panel implementation

ay_logging_panel::ay_logging_panel( const char *inTopFrameText, struct prefs::logging &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs )
{
}

// Build
void	ay_logging_panel::Build( GtkWidget *inParent )
{
	gtkut_button( _("Save all conversations to logfiles"), &m_prefs.do_logging, m_top_vbox );
	gtkut_button( _("Restore last conversation when opening a chat window"), &m_prefs.do_restore_last_conv, m_top_vbox );
}


////////////////
//// ay_sound_general_panel implementation

ay_sound_general_panel::ay_sound_general_panel( const char *inTopFrameText, struct prefs::sound &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs )
{
}

// Build
void	ay_sound_general_panel::Build( GtkWidget *inParent )
{
	gtkut_button( _("Disable sounds when I am away"), &m_prefs.do_no_sound_when_away, m_top_vbox );
	gtkut_button( _("Disable sounds for Ignored people"), &m_prefs.do_no_sound_for_ignore, m_top_vbox );
	gtkut_button( _("Play sounds when people sign on or off"), &m_prefs.do_online_sound, m_top_vbox );
	gtkut_button( _("Play a sound when sending a message"), &m_prefs.do_play_send, m_top_vbox );
	gtkut_button( _("Play a sound when receiving a message"), &m_prefs.do_play_receive, m_top_vbox );
	gtkut_button( _("Play a special sound when receiving first message"), &m_prefs.do_play_first, m_top_vbox );
}


////////////////
//// ay_sound_files_panel implementation

ay_sound_files_panel::ay_sound_files_panel( const char *inTopFrameText, struct prefs::sound &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_arrivesound_entry( NULL ),
	m_awaysound_entry( NULL ),
	m_leavesound_entry( NULL ),
	m_sendsound_entry( NULL ),
	m_receivesound_entry( NULL ),
	m_firstmsgsound_entry( NULL ),
	m_volumesound_entry( NULL )
{
}

// Build
void	ay_sound_files_panel::Build( GtkWidget *inParent )
{
	m_arrivesound_entry = AddSoundFileSelectionBox( _("Contact signs on: "), m_prefs.BuddyArriveFilename, SOUND_BUDDY_ARRIVE );
					   
	m_awaysound_entry = AddSoundFileSelectionBox( _("Contact goes away: "), m_prefs.BuddyAwayFilename, SOUND_BUDDY_AWAY );
					   
	m_leavesound_entry = AddSoundFileSelectionBox( _("Contact signs off: "), m_prefs.BuddyLeaveFilename, SOUND_BUDDY_LEAVE );

	m_sendsound_entry = AddSoundFileSelectionBox( _("Message sent: "), m_prefs.SendFilename, SOUND_SEND );

	m_receivesound_entry = AddSoundFileSelectionBox( _("Message received: "), m_prefs.ReceiveFilename, SOUND_RECEIVE );

	m_firstmsgsound_entry = AddSoundFileSelectionBox( _("First message received: "), m_prefs.FirstMsgFilename, SOUND_FIRSTMSG );

	m_volumesound_entry = AddSoundVolumeSelectionBox( _("Relative volume (dB)"),
				GTK_ADJUSTMENT(gtk_adjustment_new( m_prefs.SoundVolume, -40,0,1,5,0 )) );

}

// Apply
void	ay_sound_files_panel::Apply( void )
{
	strncpy( m_prefs.BuddyArriveFilename, gtk_entry_get_text( GTK_ENTRY(m_arrivesound_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.BuddyLeaveFilename, gtk_entry_get_text( GTK_ENTRY(m_leavesound_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.SendFilename, gtk_entry_get_text( GTK_ENTRY(m_sendsound_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.ReceiveFilename, gtk_entry_get_text( GTK_ENTRY(m_receivesound_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.BuddyAwayFilename, gtk_entry_get_text( GTK_ENTRY(m_awaysound_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.FirstMsgFilename, gtk_entry_get_text( GTK_ENTRY(m_firstmsgsound_entry) ), MAX_PREF_LEN );
}

// AddSoundFileSelectionBox
GtkWidget	*ay_sound_files_panel::AddSoundFileSelectionBox( const char *inLabelString,
						const char *inInitialFilename, int inSoundID ) 
{
	GtkWidget	*vbox = gtk_vbox_new( FALSE, 1 );
	gtk_widget_show( vbox );
	
	GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
	gtk_widget_show( hbox );

	GtkWidget	*label = gtk_label_new( inLabelString );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, 125, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, TRUE, TRUE, 3 );

	const int	button_width = 54;
	
	GtkWidget	*preview_button = gtk_button_new_with_label( _("Preview") );
	gtk_widget_show( preview_button );
	gtk_widget_set_usize( preview_button, button_width, -1 );
	gtk_signal_connect( GTK_OBJECT(preview_button), "clicked", GTK_SIGNAL_FUNC(s_testsoundfile), &(m_cb_data[inSoundID]) );
	gtk_box_pack_start( GTK_BOX(hbox), preview_button, FALSE, FALSE, 5 );
	
	gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 0 );

	hbox = gtk_hbox_new( FALSE, 3 );
	gtk_widget_show( hbox );
	
	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, 15, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 3 );

	GtkWidget	*widget = gtk_entry_new();
	gtk_widget_show( widget );
	gtk_entry_set_text( GTK_ENTRY (widget), inInitialFilename );
	gtk_box_pack_start( GTK_BOX(hbox), widget, TRUE, TRUE, 0 );

	GtkWidget	*browse_button = gtk_button_new_with_label( _("Browse") );
	gtk_widget_show( browse_button );
	gtk_widget_set_usize( browse_button, button_width, -1 );
	
	m_cb_data[inSoundID].m_panel = this;
	m_cb_data[inSoundID].m_sound_id = inSoundID;
	gtk_signal_connect( GTK_OBJECT(browse_button), "clicked", GTK_SIGNAL_FUNC(s_getsoundfile), &(m_cb_data[inSoundID]) );
	gtk_box_pack_start( GTK_BOX(hbox), browse_button, FALSE, FALSE, 5 );
	
	gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 0 );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	gtk_box_pack_start( GTK_BOX(vbox), spacer, FALSE, FALSE, 3 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), vbox, FALSE, FALSE, 0 );

	return( widget );
}

// AddSoundVolumeSelectionBox
GtkWidget	*ay_sound_files_panel::AddSoundVolumeSelectionBox( const char *inLabelString, GtkAdjustment *inAdjustment )
{
	GtkWidget *hbox = gtk_hbox_new( FALSE, 3 );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );

	GtkWidget *label = gtk_label_new( inLabelString );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 3 );
	gtk_widget_set_usize( label, 125, 10 );
	gtk_widget_show( label );

	GtkWidget *widget = gtk_hscale_new( inAdjustment );
	gtk_signal_connect( GTK_OBJECT(inAdjustment), "value_changed", GTK_SIGNAL_FUNC(s_soundvolume_changed), this );
	gtk_box_pack_start( GTK_BOX(hbox), widget, TRUE, TRUE, 0 );
	gtk_widget_show( widget );

	gtk_widget_show( hbox );

	return( widget );
}

////
// ay_sound_files_panel callbacks

// s_setsoundfilename
void	ay_sound_files_panel::s_setsoundfilename( char *selected_filename, void *data )
{
	if ( selected_filename == NULL )
		return;

	eb_debug( DBG_CORE, "Just entered setsoundfilename and the selected file is %s\n", selected_filename );
	
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	const ay_sound_files_panel	*the_panel = cb_data->m_panel;
	
	switch( cb_data->m_sound_id ) 
	{
		case SOUND_BUDDY_ARRIVE: 	
			strncpy( the_panel->m_prefs.BuddyArriveFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_arrivesound_entry), selected_filename);
			break;
		
		case SOUND_BUDDY_LEAVE:	
			strncpy( the_panel->m_prefs.BuddyLeaveFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_leavesound_entry), selected_filename);
			break;
		
		case SOUND_SEND:		
			strncpy( the_panel->m_prefs.SendFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_sendsound_entry), selected_filename);
			break;
		
		case SOUND_RECEIVE:		
			strncpy( the_panel->m_prefs.ReceiveFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_receivesound_entry), selected_filename);
			break;
		
		case SOUND_BUDDY_AWAY: 	
			strncpy( the_panel->m_prefs.BuddyAwayFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_awaysound_entry), selected_filename);
			break;
		
		case SOUND_FIRSTMSG:		
			strncpy( the_panel->m_prefs.FirstMsgFilename, selected_filename, MAX_PREF_LEN);
			gtk_entry_set_text (GTK_ENTRY(the_panel->m_firstmsgsound_entry), selected_filename);
			break;
		
		default:
			assert( false );
			break;
	}
}

// s_getsoundfile
void	ay_sound_files_panel::s_getsoundfile( GtkWidget *widget, void *data )
{
	eb_debug( DBG_CORE, "Just entered getsoundfile\n" );
	
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	eb_do_file_selector( NULL, _("Select a file to use"), s_setsoundfilename, cb_data );
}

// s_testsoundfile
void	ay_sound_files_panel::s_testsoundfile( GtkWidget *widget, void *data )
{
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	const ay_sound_files_panel	*the_panel = cb_data->m_panel;
	
	switch( cb_data->m_sound_id ) 
	{
		case SOUND_BUDDY_ARRIVE:
			playsoundfile( the_panel->m_prefs.BuddyArriveFilename );
			break;

		case SOUND_BUDDY_LEAVE:
			playsoundfile( the_panel->m_prefs.BuddyLeaveFilename );
			break;

		case SOUND_SEND:
			playsoundfile( the_panel->m_prefs.SendFilename );
			break;

		case SOUND_RECEIVE:
			playsoundfile( the_panel->m_prefs.ReceiveFilename );
			break;

		case SOUND_BUDDY_AWAY:
			playsoundfile( the_panel->m_prefs.BuddyAwayFilename );
			break;

		case SOUND_FIRSTMSG:
			playsoundfile( the_panel->m_prefs.FirstMsgFilename );
			break;
		
		default:
			assert( false );
			break;
	}
}

// s_soundvolume_changed
void	ay_sound_files_panel::s_soundvolume_changed( GtkAdjustment *adjust, void *data )
{
	ay_sound_files_panel	*the_panel = reinterpret_cast<ay_sound_files_panel *>( data );
	assert( the_panel != NULL );

	the_panel->m_prefs.SoundVolume = adjust->value;
}


////////////////
//// ay_chat_panel implementation

ay_chat_panel::ay_chat_panel( const char *inTopFrameText, struct prefs::chat &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_font_size_entry( NULL )
{
}

// Build
void	ay_chat_panel::Build( GtkWidget *inParent )
{
	gtkut_button( _("Send idle/away status to servers"), &m_prefs.do_send_idle_time, m_top_vbox );
	gtkut_button( _("Raise chat-window when receiving a message"), &m_prefs.do_raise_window, m_top_vbox );
	gtkut_button( _("Ignore unknown people"), &m_prefs.do_ignore_unknown, m_top_vbox );
	gtkut_button( _("Ignore foreground Colors"), &m_prefs.do_ignore_fore, m_top_vbox );
	gtkut_button( _("Ignore background Colors"), &m_prefs.do_ignore_back, m_top_vbox );
	gtkut_button( _("Ignore fonts"), &m_prefs.do_ignore_font, m_top_vbox );

	GtkWidget	*hbox = gtk_hbox_new( FALSE, 0 );

	GtkWidget	*label = gtk_label_new( _("Font size: ") );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
	gtk_widget_show( label );

	const int	buff_len = 10;
	char		buff[buff_len];
	
	m_font_size_entry = gtk_entry_new();
	gtk_widget_set_usize( m_font_size_entry, 60, -1 );
	g_snprintf( buff, buff_len, "%d", m_prefs.font_size );

	gtk_entry_set_text( GTK_ENTRY(m_font_size_entry), buff );
	gtk_box_pack_start( GTK_BOX(hbox), m_font_size_entry, FALSE, FALSE, 0 );
	gtk_widget_show( m_font_size_entry );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
}

// Apply
void	ay_chat_panel::Apply( void )
{
	const char	*ptr = gtk_entry_get_text( GTK_ENTRY(m_font_size_entry) );
	
	if ( ptr != NULL )
		m_prefs.font_size = atoi( ptr );
}

////////////////
//// ay_tabs_panel implementation

ay_tabs_panel::ay_tabs_panel( const char *inTopFrameText, struct prefs::tabs &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_orientation_frame( NULL ),
	m_hotkey_frame( NULL ),
	m_accel_change_handler_id( 0 )
{
}

// Build
void	ay_tabs_panel::Build( GtkWidget *inParent )
{
	m_toggle_data.m_panel = this;
	m_toggle_data.m_toggle_data = &m_prefs.do_tabbed_chat;
	gtkut_check_button( m_top_vbox, _("Use tabs in chat windows"), m_prefs.do_tabbed_chat,
		GTK_SIGNAL_FUNC(s_set_tabbed), &m_toggle_data );
	
	// orientation
	m_orientation_frame = gtk_frame_new( _( "Tab Position" ) );
	gtk_widget_show( m_orientation_frame );
	gtk_container_set_border_width( GTK_CONTAINER(m_orientation_frame), 3 );
	
	GtkWidget	*vbox = gtk_vbox_new( FALSE, 4 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER(m_orientation_frame), vbox );
	
	/* Because it seems that the 'clicked' function is called when we create the radio buttons [!],
		we must save our current value and restore it after the creation of the radio buttons
	*/
	const int	old_value = m_prefs.do_tabbed_chat_orient;
	
	GSList		*radio_group = NULL;
	
	m_orientation_cb_data[0].m_panel = this;
	m_orientation_cb_data[0].m_orientation_id = 0;

	radio_group = gtkut_add_radio_button_to_group( radio_group, vbox,
			_("Bottom"), (m_prefs.do_tabbed_chat_orient == 0),
			GTK_SIGNAL_FUNC(s_change_orientation), &(m_orientation_cb_data[0]) );
	
	m_orientation_cb_data[1].m_panel = this;
	m_orientation_cb_data[1].m_orientation_id = 1;

	radio_group = gtkut_add_radio_button_to_group( radio_group, vbox,
			_("Top"), (m_prefs.do_tabbed_chat_orient == 1),
			GTK_SIGNAL_FUNC(s_change_orientation), &(m_orientation_cb_data[1]) );
	
	m_orientation_cb_data[2].m_panel = this;
	m_orientation_cb_data[2].m_orientation_id = 2;

	radio_group = gtkut_add_radio_button_to_group( radio_group, vbox,
			_("Left"), (m_prefs.do_tabbed_chat_orient == 2),
			GTK_SIGNAL_FUNC(s_change_orientation), &(m_orientation_cb_data[2]) );
	
	m_orientation_cb_data[3].m_panel = this;
	m_orientation_cb_data[3].m_orientation_id = 3;

	radio_group = gtkut_add_radio_button_to_group( radio_group, vbox,
			_("Right"), (m_prefs.do_tabbed_chat_orient == 3),
			GTK_SIGNAL_FUNC(s_change_orientation), &(m_orientation_cb_data[3]) );

	gtk_box_pack_start( GTK_BOX(m_top_vbox), m_orientation_frame, FALSE, FALSE, 5 );
	
	m_prefs.do_tabbed_chat_orient = old_value;

	// hotkeys
	gtk_accelerator_parse( m_prefs.accel_next_tab,
		&(m_local_accel_next_tab.keyval), &(m_local_accel_next_tab.modifiers) );
	gtk_accelerator_parse( m_prefs.accel_prev_tab,
		&(m_local_accel_prev_tab.keyval), &(m_local_accel_prev_tab.modifiers) );

	m_hotkey_frame = gtk_frame_new( _( "Hotkeys" ) );
	gtk_widget_show( m_hotkey_frame );
	gtk_container_set_border_width( GTK_CONTAINER(m_hotkey_frame), 3 );
	
	GtkWidget	*hotkey_vbox = gtk_vbox_new( FALSE, 4 );
	gtk_widget_show( hotkey_vbox );
	gtk_container_add( GTK_CONTAINER(m_hotkey_frame), hotkey_vbox );

	m_key_cb_data[0].m_panel = this;
	m_key_cb_data[0].m_device_key = &m_local_accel_prev_tab;
	AddKeySet( _("Previous tab:"), &(m_key_cb_data[0]), hotkey_vbox );
	
	m_key_cb_data[1].m_panel = this;
	m_key_cb_data[1].m_device_key = &m_local_accel_next_tab;
	AddKeySet( _("Next tab:"), &(m_key_cb_data[1]), hotkey_vbox );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), m_hotkey_frame, FALSE, FALSE, 5 );
	
	SetActiveWidgets();
}

void	ay_tabs_panel::Apply( void )
{
	char	*ptr = NULL;

	ptr = gtk_accelerator_name( m_local_accel_next_tab.keyval, m_local_accel_next_tab.modifiers );
	strncpy( m_prefs.accel_next_tab, ptr, MAX_PREF_LEN );
	g_free( ptr );
	 
	ptr = gtk_accelerator_name( m_local_accel_prev_tab.keyval, m_local_accel_prev_tab.modifiers );
	strncpy( m_prefs.accel_prev_tab, ptr, MAX_PREF_LEN );
	g_free( ptr );
}

// AddKeySet
void	ay_tabs_panel::AddKeySet( const char *inLabelString, t_cb_data *cb_data, GtkWidget *vbox )
{
	GtkWidget	*hbox = gtk_hbox_new( FALSE, 2 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 5 );

	GtkWidget	*label = gtk_label_new( inLabelString );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, 75, 10 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 3 );

	const char	*clabel = gtk_accelerator_name( cb_data->m_device_key->keyval, cb_data->m_device_key->modifiers );
	GtkWidget	*button = gtk_button_new_with_label( clabel );
	g_free( (void *)clabel );
	gtk_widget_show( button );

	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(s_getnewkey), cb_data );
	gtk_box_pack_start( GTK_BOX(hbox), button, TRUE, TRUE, 5 );
}

// SetActiveWidgets
void	ay_tabs_panel::SetActiveWidgets( void )
{
	gtk_widget_set_sensitive( m_orientation_frame, m_prefs.do_tabbed_chat );
	gtk_widget_set_sensitive( m_hotkey_frame, m_prefs.do_tabbed_chat );
}

////
// ay_tabs_panel callbacks

// s_set_tabbed
void	ay_tabs_panel::s_set_tabbed( GtkWidget *w, void *data )
{
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );

	ay_tabs_panel	*the_panel = cb_data->m_panel;
	
	// toggle the data
	int	*value = cb_data->m_toggle_data;
	*value = !(*value);
	
	the_panel->SetActiveWidgets();
}

// s_change_orientation
void	ay_tabs_panel::s_change_orientation( GtkWidget *widget, void *data )
{
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	ay_tabs_panel	*the_panel = cb_data->m_panel;
	the_panel->m_prefs.do_tabbed_chat_orient = cb_data->m_orientation_id;
}

// s_newkey_callback
gboolean	ay_tabs_panel::s_newkey_callback( GtkWidget *keybutton, GdkEventKey *event, void *data )
{
	// IF the user hits escape
	//	THEN cancel the hotkey selection
	if ( event->keyval == GDK_Escape )
	{
		t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
		assert( cb_data != NULL );

		ay_tabs_panel		*the_panel = cb_data->m_panel;
		GtkWidget		*label = GTK_BIN(keybutton)->child;
		const GdkDeviceKey	*the_key = cb_data->m_device_key;

		gtk_label_set_text( GTK_LABEL(label), gtk_accelerator_name( the_key->keyval, the_key->modifiers) );
		gtk_signal_disconnect( GTK_OBJECT(keybutton), the_panel->m_accel_change_handler_id );
		gtk_grab_remove( keybutton );
		the_panel->m_accel_change_handler_id = 0;
		
		return( gtk_true() );
	}
	
	/* remove stupid things like.. numlock scrolllock and capslock
	 * mod1 = alt, mod2 = numlock, mod3 = modeshift/altgr, mod4 = meta, mod5 = scrolllock */
	const int	state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
	
	if ( state == 0 )
		return( gtk_true() );

	/* this unfortunately was the only way I could do this without using
	 * a key release event
	 */
	switch( event->keyval )
	{
			case GDK_Shift_L:
			case GDK_Shift_R:
			case GDK_Control_L:
			case GDK_Control_R:
			case GDK_Caps_Lock:
			case GDK_Shift_Lock:
			case GDK_Meta_L:
			case GDK_Meta_R:
			case GDK_Alt_L:
			case GDK_Alt_R:
			case GDK_Super_L:
			case GDK_Super_R:
			case GDK_Hyper_L:
			case GDK_Hyper_R:
				// don't let the user set a modifier as a hotkey
				break;

			case GDK_Return:
				// don't let the user set Return as a hotkey
				break;
				
			default:
				{
					t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
					assert( cb_data != NULL );

					ay_tabs_panel	*the_panel = cb_data->m_panel;
					GtkWidget	*label = GTK_BIN(keybutton)->child;
					GdkDeviceKey	*the_key = cb_data->m_device_key;

					the_key->keyval = event->keyval;
					the_key->modifiers = static_cast<GdkModifierType>(state);

					gtk_label_set_text( GTK_LABEL(label), gtk_accelerator_name( the_key->keyval, the_key->modifiers) );
					gtk_signal_disconnect( GTK_OBJECT(keybutton), the_panel->m_accel_change_handler_id );
					gtk_grab_remove( keybutton );
					the_panel->m_accel_change_handler_id = 0;
				}
				break;
	}
	
	/* eat the event and make focus keys (arrows) not change the focus */
	return( gtk_true() );
}

// getnewkey
void	ay_tabs_panel::s_getnewkey( GtkWidget *keybutton, void *data )
{
	GtkWidget	*label = GTK_BIN(keybutton)->child;
	
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );

	ay_tabs_panel	*the_panel = cb_data->m_panel;
	
	if ( the_panel->m_accel_change_handler_id == 0 )
	{
		gtk_label_set_text( GTK_LABEL(label), _("<Press modifier + key or escape to cancel>") );

		gtk_object_set_data( GTK_OBJECT(keybutton), "accel", data );

		/* it's sad how this works: It grabs the events in the event mask
		 * of the widget the mouse is over, NOT the grabbed widget.
		 * Oh, and persistantly clicking makes the grab go away...
		 */
		gtk_grab_add( keybutton );

		the_panel->m_accel_change_handler_id = gtk_signal_connect_after( GTK_OBJECT(keybutton),
					"key_press_event",
					GTK_SIGNAL_FUNC(s_newkey_callback), data );
	}
}

////////////////
//// ay_advanced_panel implementation
ay_advanced_panel::ay_advanced_panel( const char *inTopFrameText )
:	ay_prefs_window_panel( inTopFrameText )
{
}

// Build
void	ay_advanced_panel::Build( GtkWidget *inParent )
{	
	GtkWidget	*label = gtk_label_new( _("This section is for configuration of advanced options such as proxy\nservers and encoding conversion [if your system supports them].") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 10 );
	
	label = gtk_label_new( _("[There are no general preferences for the Advanced section]") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 5 );
}

////////////////
//// ay_proxy_panel implementation

ay_proxy_panel::ay_proxy_panel( const char *inTopFrameText, struct prefs::advanced &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_last_proxy_type( PROXY_NONE ),
	m_proxy_checkbox( NULL ),
	m_proxy_frame( NULL ),
	m_proxy_server_entry( NULL ),
	m_proxy_port_entry( NULL ),
	m_auth_toggle( NULL ),
	m_proxy_user_entry( NULL ),
	m_proxy_password_entry( NULL ),
	m_finished_construction( false )
{
}

// Build
void	ay_proxy_panel::Build( GtkWidget *inParent )
{
	m_last_proxy_type = static_cast<eProxyType>(m_prefs.proxy_type);
	
	// set last type to HTTP if we are currently set to NONE
	//	because NONE is not one of the radio buttons
	//	[NOTE we have to do a bit of fancy footwork to work around the fact that
	//		PROXY_NONE is a proxy type...]
	if ( m_last_proxy_type == PROXY_NONE )
		m_last_proxy_type = PROXY_HTTP;
		
	m_proxy_server_entry = gtk_entry_new();
	m_proxy_port_entry = gtk_entry_new();
	m_proxy_user_entry = gtk_entry_new();
	m_proxy_password_entry = gtk_entry_new();
	
	m_toggle_data.m_panel = this;
	m_toggle_data.m_toggle_data = NULL;
	m_proxy_checkbox = gtkut_check_button( m_top_vbox, _("Use a proxy server"), (m_prefs.proxy_type != PROXY_NONE),
		GTK_SIGNAL_FUNC(s_toggle_checkbox), &m_toggle_data );
	
	m_proxy_frame = gtk_frame_new( _( "Proxy Type" ) );
	gtk_widget_show( m_proxy_frame );
	gtk_container_set_border_width( GTK_CONTAINER(m_proxy_frame), 0 );
	
	GtkWidget	*proxy_vbox = gtk_vbox_new( FALSE, 5 );
	gtk_widget_show( proxy_vbox );
	gtk_container_set_border_width( GTK_CONTAINER(proxy_vbox), 5 );
	gtk_container_add( GTK_CONTAINER(m_proxy_frame), proxy_vbox );
	
	/* Because it seems that the 'clicked' function is called when we create the radio buttons [!],
		we must save our current value and restore it after the creation of the radio buttons
	*/
	const eProxyType	old_value = m_last_proxy_type;
	
	GSList	*radio_group = NULL;
	
	// PROXY_HTTP
	m_cb_data[PROXY_HTTP].m_panel = this;
	m_cb_data[PROXY_HTTP].m_proxy_type = PROXY_HTTP;
	
	radio_group = gtkut_add_radio_button_to_group( radio_group, proxy_vbox,
			_("HTTP"), (m_last_proxy_type == PROXY_HTTP),
			GTK_SIGNAL_FUNC(s_set_proxy_type), &(m_cb_data[PROXY_HTTP]) );

	// PROXY_SOCKS4	
	m_cb_data[PROXY_SOCKS4].m_panel = this;
	m_cb_data[PROXY_SOCKS4].m_proxy_type = PROXY_SOCKS4;
	
	radio_group = gtkut_add_radio_button_to_group( radio_group, proxy_vbox,
			_("SOCKS4"), (m_last_proxy_type == PROXY_SOCKS4),
			GTK_SIGNAL_FUNC(s_set_proxy_type), &(m_cb_data[PROXY_SOCKS4]) );

	// PROXY_SOCKS5
	m_cb_data[PROXY_SOCKS5].m_panel = this;
	m_cb_data[PROXY_SOCKS5].m_proxy_type = PROXY_SOCKS5;

	radio_group = gtkut_add_radio_button_to_group( radio_group, proxy_vbox,
			_("SOCKS5"), (m_last_proxy_type == PROXY_SOCKS5),
			GTK_SIGNAL_FUNC(s_set_proxy_type), &(m_cb_data[PROXY_SOCKS5]) );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), m_proxy_frame, FALSE, FALSE, 5 );

	m_last_proxy_type = old_value;
	
	const int	label_len = 75;
	const int	default_entry_len = 75;
	
	// server
	GtkWidget	*hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	GtkWidget	*label = gtk_label_new( _("Server:") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 5 );

	gtk_entry_set_text( GTK_ENTRY(m_proxy_server_entry), m_prefs.proxy_host );
	gtk_widget_show( m_proxy_server_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_proxy_server_entry, TRUE, TRUE, 5 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 3 );

	// port
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	label = gtk_label_new( _("Port:") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 5 );

	const int	buff_len = 10;
	char		buff[buff_len];   
     
	g_snprintf( buff, buff_len, "%d", m_prefs.proxy_port );
	gtk_entry_set_text( GTK_ENTRY(m_proxy_port_entry), buff );
	gtk_widget_set_usize( m_proxy_port_entry, default_entry_len, -1 );
	gtk_widget_show( m_proxy_port_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_proxy_port_entry, FALSE, FALSE, 5 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 3 );

	// authentication
	m_auth_cb_data.m_panel = this;
	m_auth_cb_data.m_toggle_data = &m_prefs.do_proxy_auth;
	m_auth_toggle = gtkut_check_button( m_top_vbox, _("Proxy requires authentication"), m_prefs.do_proxy_auth,
		GTK_SIGNAL_FUNC(s_toggle_checkbox), &m_auth_cb_data );

	const int	spacer_len = 15;
	
	// user ID
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, spacer_len, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("User ID:") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len - spacer_len, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 5 );

	gtk_entry_set_text( GTK_ENTRY(m_proxy_user_entry), m_prefs.proxy_user );
	gtk_widget_set_usize( m_proxy_user_entry, default_entry_len, -1 );
	gtk_widget_show( m_proxy_user_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_proxy_user_entry, FALSE, FALSE, 5 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 3 );

	// password
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, spacer_len, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Password:") );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len - spacer_len, 15 );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 5 );

	gtk_entry_set_visibility( GTK_ENTRY(m_proxy_password_entry), FALSE );
	gtk_entry_set_text( GTK_ENTRY(m_proxy_password_entry), m_prefs.proxy_password );
	gtk_widget_set_usize( m_proxy_password_entry, default_entry_len, -1 );
	gtk_widget_show( m_proxy_password_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_proxy_password_entry, FALSE, FALSE, 5 );

	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 3 );

	m_finished_construction = true;
	SetActiveWidgets();
}

// Apply
void	ay_proxy_panel::Apply( void )
{
	if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(m_proxy_checkbox) ) )
		m_prefs.proxy_type = PROXY_NONE;
	else
		m_prefs.proxy_type = m_last_proxy_type;
		
	strncpy( m_prefs.proxy_host, gtk_entry_get_text(GTK_ENTRY(m_proxy_server_entry)), MAX_PREF_LEN );
	m_prefs.proxy_port = atol( gtk_entry_get_text(GTK_ENTRY(m_proxy_port_entry)) );
	strncpy( m_prefs.proxy_user, gtk_entry_get_text(GTK_ENTRY(m_proxy_user_entry)), MAX_PREF_LEN );
	strncpy( m_prefs.proxy_password, gtk_entry_get_text(GTK_ENTRY(m_proxy_password_entry)), MAX_PREF_LEN );
}

// SetActiveWidgets
void	ay_proxy_panel::SetActiveWidgets( void )
{
	if ( !m_finished_construction )
		return;
		
	const bool	use_proxy = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(m_proxy_checkbox) );

	gtk_widget_set_sensitive( m_proxy_frame, use_proxy );

	gtk_widget_set_sensitive( m_proxy_server_entry, use_proxy );
	gtk_widget_set_sensitive( m_proxy_port_entry, use_proxy );
	gtk_widget_set_sensitive( m_auth_toggle, use_proxy );

	const bool	auth_active = use_proxy && m_prefs.do_proxy_auth;

	gtk_widget_set_sensitive( m_proxy_user_entry, auth_active );
	gtk_widget_set_sensitive( m_proxy_password_entry, auth_active );
}

////
// ay_proxy_panel callbacks

// s_toggle_checkbox
void	ay_proxy_panel::s_toggle_checkbox( GtkWidget *w, void *data )
{
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );

	ay_proxy_panel	*the_panel = cb_data->m_panel;
	
	// toggle the data
	int	*value = cb_data->m_toggle_data;
	if ( value != NULL )
		*value = !(*value);

	the_panel->SetActiveWidgets();
}

// s_set_proxy_type
void	ay_proxy_panel::s_set_proxy_type( GtkWidget *w, void *data )
{
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );

	ay_proxy_panel	*the_panel = cb_data->m_panel;
	eProxyType		the_type = cb_data->m_proxy_type;
    
	the_panel->m_last_proxy_type = the_type;

	the_panel->SetActiveWidgets();
}


////////////////
//// ay_encoding_panel implementation
#ifdef HAVE_ICONV

const int	ay_encoding_panel::ENCODE_LEN = 64;

ay_encoding_panel::ay_encoding_panel( const char *inTopFrameText, struct prefs::advanced &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_local_encoding_entry( NULL ),
	m_remote_encoding_entry( NULL )
{
}

// Build
void	ay_encoding_panel::Build( GtkWidget *inParent )
{
	m_local_encoding_entry = gtk_entry_new();
	m_remote_encoding_entry = gtk_entry_new();

	GtkWidget	*label = gtk_label_new( _("Note: Conversion is made using iconv() which is UNIX98 standard.\nFor list of possible encodings run 'iconv --list'.") );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 0 );
	gtk_widget_show( label);

	GtkWidget	*button = gtkut_button( _("Use recoding in conversations"), &m_prefs.use_recoding, m_top_vbox );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(s_set_use_of_recoding), this );

	const int	label_len = 120;
	const int 	spacer_len = 15;
	
	// LOCAL
	GtkWidget	*hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, spacer_len, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Local encoding:") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len - spacer_len, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

	gtk_entry_set_text( GTK_ENTRY(m_local_encoding_entry), cGetLocalPref("local_encoding") );
	gtk_widget_show( m_local_encoding_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_local_encoding_entry, TRUE, TRUE, 10 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 10 );

	// REMOTE
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, spacer_len, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Remote encoding:") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_usize( label, label_len - spacer_len, 15 );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

	gtk_entry_set_text( GTK_ENTRY(m_remote_encoding_entry), cGetLocalPref("remote_encoding") );
	gtk_widget_show( m_remote_encoding_entry );
	gtk_box_pack_start( GTK_BOX(hbox), m_remote_encoding_entry, TRUE, TRUE, 10 );

	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 10 );

	SetActiveWidgets();
}

// Apply
void	ay_encoding_panel::Apply( void )
{
	strncpy( m_prefs.local_encoding, gtk_entry_get_text( GTK_ENTRY(m_local_encoding_entry) ), MAX_PREF_LEN );
	strncpy( m_prefs.remote_encoding, gtk_entry_get_text( GTK_ENTRY(m_remote_encoding_entry) ), MAX_PREF_LEN );
}

// SetActiveWidgets
void	ay_encoding_panel::SetActiveWidgets( void )
{
	gtk_widget_set_sensitive( m_local_encoding_entry, m_prefs.use_recoding );
	gtk_widget_set_sensitive( m_remote_encoding_entry, m_prefs.use_recoding );
}

////
// ay_encoding_panel callbacks

// s_set_use_of_recoding
void	ay_encoding_panel::s_set_use_of_recoding( GtkWidget *widget, void *data )
{
	ay_encoding_panel	*the_panel = reinterpret_cast<ay_encoding_panel *>( data );
	assert( the_panel != NULL );
 
	the_panel->SetActiveWidgets();
}

#endif	// HAVE_ICONV


////////////////
//// ay_services_panel implementation
ay_services_panel::ay_services_panel( const char *inTopFrameText )
:	ay_prefs_window_panel( inTopFrameText )
{
}

// Build
void	ay_services_panel::Build( GtkWidget *inParent )
{
	GtkWidget	*label = gtk_label_new( _("Services allow you to connect to and chat with people using a\nvariety of messenger protocols.") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 10 );
	
	label = gtk_label_new( _("[There are no general preferences for the Services section]") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 5 );
}


////////////////
//// ay_utilities_panel implementation
ay_utilities_panel::ay_utilities_panel( const char *inTopFrameText )
:	ay_prefs_window_panel( inTopFrameText )
{
}

// Build
void	ay_utilities_panel::Build( GtkWidget *inParent )
{
	GtkWidget	*label = gtk_label_new( _("Utilities add additional capabilities such as importing contacts or\nchanging your smiley sets.") );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 10 );
	
	label = gtk_label_new( _("[There are no general preferences for the Utilities section]") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 5 );
}


////////////////
//// ay_module_panel implementation

ay_module_panel::ay_module_panel( const char *inTopFrameText, t_module_pref &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs )
{
}

// Build
void	ay_module_panel::Build( GtkWidget *inParent )
{
	const bool	has_error = (m_prefs.status_desc != NULL) && (m_prefs.status_desc[0] != '\0');
	
	GtkWidget	*info_frame = gtk_frame_new( _( "Info" ) );
	gtk_widget_show( info_frame );
	
	GtkWidget	*info_table = gtk_table_new( 4, 4, FALSE );
	gtk_widget_show( info_table );
	gtk_container_set_border_width( GTK_CONTAINER(info_table), 3 );

	gtk_container_add( GTK_CONTAINER(info_frame), info_table );
	
	int	row = 0;
	AddInfoRow( info_table, row++, _( "Description:" ), m_prefs.brief_desc );
	AddInfoRow( info_table, row++, _( "Version:" ), m_prefs.version );
	AddInfoRow( info_table, row++, _( "Date:" ), m_prefs.date );
	AddInfoRow( info_table, row++, _( "Path:" ), m_prefs.file_name );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), info_frame, FALSE, FALSE, 5 );
	
	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), spacer, FALSE, FALSE, 0 );

	if ( has_error )
	{
		GtkWidget	*frame = gtk_frame_new( _("Plugin Error") );
		gtk_widget_show( frame );

		GtkWidget	*hbox = gtk_hbox_new( FALSE, 0 );
		gtk_widget_show( hbox );
		
		GtkStyle	*style = gtk_widget_get_style( m_parent_window );
		GdkBitmap	*mask = NULL;
	
		GdkPixmap	*icon = gdk_pixmap_create_from_xpm_d( m_parent_window->window, &mask, &style->bg[GTK_STATE_NORMAL], error_xpm );
		GtkWidget	*icon_widget = gtk_pixmap_new( icon, mask );
		gtk_widget_show( icon_widget );
		gtk_box_pack_start( GTK_BOX(hbox), icon_widget, FALSE, FALSE, 0 );

		GtkWidget	*label = gtk_label_new( m_prefs.status_desc );
		gtk_widget_show( label );
		gtk_misc_set_alignment( GTK_MISC(label), 0.0, 0.5 );
		gtk_label_set_line_wrap( GTK_LABEL(label), TRUE );
		gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
		gtk_misc_set_padding( GTK_MISC(label), 5, 5 );
		gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
		
		gtk_container_add( GTK_CONTAINER(frame), hbox );

		gtk_box_pack_start( GTK_BOX(m_top_vbox), frame, FALSE, FALSE, 0 );
	}
	else
	{	
		if ( m_prefs.pref_list != NULL )
		{
			eb_input_render( m_prefs.pref_list, m_top_vbox );
		}
		else
		{	
			GtkWidget	*label = gtk_label_new( NULL );
			gtk_widget_show( label );

			GString		*labelText = g_string_new( _("[There are no preferences for ") );

			if ( m_prefs.service_name != NULL )
			{
				labelText = g_string_append( labelText, m_prefs.service_name );
			}
			else
			{
				labelText = g_string_append( labelText, "'" );
				labelText = g_string_append( labelText, m_prefs.brief_desc );
				labelText = g_string_append( labelText, "'" );
			}

			labelText = g_string_append( labelText, "]" );

			gtk_label_set_text( GTK_LABEL(label), labelText->str );
			gtk_box_pack_start( GTK_BOX(m_top_vbox), GTK_WIDGET(label), FALSE, FALSE, 2 );

			g_string_free( labelText, TRUE );
		}
	}
}

// Apply
void	ay_module_panel::Apply( void )
{
	eb_input_accept( m_prefs.pref_list );
}

// AddInfoRow
void	ay_module_panel::AddInfoRow( GtkWidget *inTable, int inRow, const char *inHeader, const char *inInfo )
{
	GtkWidget	*label = gtk_label_new( inHeader );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_widget_show( label );
	gtk_widget_set_usize( label, 65, -1 );
	gtk_table_attach( GTK_TABLE(inTable), label, 0, 1, inRow, inRow + 1, GTK_SHRINK, GTK_SHRINK, 10, 0 );
	
	label = gtk_label_new( inInfo );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );
	gtk_label_set_line_wrap( GTK_LABEL(label), TRUE );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_table_attach_defaults( GTK_TABLE(inTable), label, 1, 2, inRow, inRow + 1 );
}
