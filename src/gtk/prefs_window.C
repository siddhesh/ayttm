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

#include <gtk/gtk.h>
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
#include "smileys.h"

#include "gtkutils.h"

#ifdef HAVE_LIBXFT
#include "mem_util.h"
#endif

#ifdef HAVE_LIBPSPELL
#include "spellcheck.h"
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
			PANEL_CHAT_GENERAL = 0,
			PANEL_CHAT_LOGS,
			PANEL_CHAT_TABS,
			PANEL_SOUND_GENERAL,
			PANEL_SOUND_FILES,
			PANEL_MISC,
			PANEL_ADVANCED,
#ifdef HAVE_ICONV
			PANEL_ENCODING,
#endif
			PANEL_PROXY,
			PANEL_ACCOUNTS,
			PANEL_SERVICES,
			PANEL_FILTERS,
			PANEL_UTILITIES,
			PANEL_IMPORTERS,
			
			PANEL_MAX
		};
		
		static const char	*s_titles[PANEL_MAX];

	private:	// Gtk callbacks
		static void		s_tree_item_selected( GtkWidget *widget, gpointer data );
		static void		s_destroy_callback( GtkWidget* widget, gpointer data );
		static int		s_delete_event_callback( GtkWidget* widget, GdkEvent *inEvent, gpointer data );
		static void		s_ok_callback( GtkWidget *widget, gpointer data );
		static void		s_cancel_callback( GtkWidget *widget, gpointer data );
	
	private:	
		void	AddToTree( const char *inName, ay_prefs_window_panel *inPanel );
		
		void	OK( void );
		void	Cancel( void );
		
		static ay_prefs_window	*s_only_prefs_window;	///< the instance of the prefs window
		
		struct prefs		&m_prefs;
		GtkWidget		*m_prefs_window_widget;	///< the actual dialog widget
		GtkWidget		*m_notebook_widget;
		
		GtkTree			*m_tree;
		GList			*m_panels;		///< a list of the panels (ay_prefs_window_panel *)
};

/// A prefs panel
class ay_prefs_window_panel
{
	protected:
		ay_prefs_window_panel( const char *inTopFrameText );
		GtkWidget *_gtkut_button( const char *inText, int *inValue, GtkWidget *inPage );
	public:
		virtual ~ay_prefs_window_panel( void );
		
		void		SetNotebookID( int inID ) { m_notebook_id = inID; }
		int		GetNotebookID( void ) { return m_notebook_id; }
		
		const char	*Name( void ) const { return( m_name ); }
		GtkWidget	*TopLevelWidget( void ) { return( m_super_vbox ); }
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
		
		void		Show( void );
		
		static ay_prefs_window_panel	*Create( GtkWidget *inParentWindow, GtkWidget *inParent, struct prefs &inPrefs,
			ay_prefs_window::ePanelID inPanelID, const char *inName );

		static ay_prefs_window_panel	*CreateModulePanel( GtkWidget *inParentWindow, GtkWidget *inParent, t_module_pref &inPrefs );
		static ay_prefs_window_panel	*CreateAccountPanel( GtkWidget *inParentWindow, GtkWidget *inParent, t_account_pref &inPrefs );

		GtkAccelGroup	*m_accel_group;
	protected:
		GtkWidget	*m_super_vbox;
		GtkWidget	*m_top_vbox;
		GtkWidget	*m_top_scrollbox;
		GtkWidget	*m_parent_window;
		GtkWidget	*m_parent;
	
	private:
		void	AddTopFrame( const char *in_text );
		
		const char			*m_name;
		int				m_notebook_id;
		struct prefs			*m_prefs;
		ay_prefs_window::ePanelID	m_panel_id;
};

/// Section info prefs panel
class ay_section_info_panel : public ay_prefs_window_panel
{
	public:
		ay_section_info_panel( const char *inTopFrameText, const char *inSectionStr, const char *inText );
		~ay_section_info_panel( void );
				
		virtual void	Build( GtkWidget *inParent );
	
	private:
		const char	*m_section_str;
		const char	*m_text;
};

/// Chat prefs panel
class ay_chat_panel : public ay_prefs_window_panel
{
	public:
		ay_chat_panel( const char *inTopFrameText, struct prefs::chat &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );		
	
	private:	// Gtk callbacks
		static void		s_toggle_checkbox( GtkWidget *widget, int *data );
		static void		s_browse_font( GtkWidget *widget, void *data );
		static void		s_font_selection_ok( GtkButton *button, void *data );
		
	private:
		void	SetActiveWidgets( void );
		
		struct prefs::chat &m_prefs;
		
		GtkWidget	*m_dictionary_entry;
		GtkWidget	*m_font_face_entry;
		GtkWidget	*m_font_sel_win;
		int			 m_font_sel_conn_id;
};

/// Chat:Logs prefs panel
class ay_logs_panel : public ay_prefs_window_panel
{
	public:
		ay_logs_panel( const char *inTopFrameText, struct prefs::logging &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		
	private:
		struct prefs::logging &m_prefs;
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
		static void		s_setsoundfilename( const char *selected_filename, void *data );
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

/// Misc prefs panel
class ay_misc_panel : public ay_prefs_window_panel
{
	public:
		ay_misc_panel( const char *inTopFrameText, struct prefs::misc &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );
	
	private:	// Gtk callbacks
		static void		s_toggle_checkbox( GtkWidget *widget, int *data );
		static void		s_set_browser_path( const char *selected_filename, void *data );
		static void		s_get_alt_browser_path( GtkWidget *t_browser_browse_button, int *data );
		
	private:
		void	SetActiveWidgets( void );
		
		struct prefs::misc &m_prefs;
		
		GtkWidget	*m_alternate_browser_entry;
		GtkWidget	*m_browser_browse_button;
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

/// A module prefs panel
class ay_module_panel : public ay_prefs_window_panel
{
	public:
		ay_module_panel( const char *inTopFrameText, t_module_pref &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );

	private:	// Gtk callbacks
		static void		s_load_module( GtkWidget *widget, void *data );
		static void		s_unload_module( GtkWidget *widget, void *data );
		
	private:
		static void		AddInfoRow( GtkWidget *inTable, int inRow, const char *inHeader, const char *inInfo );

		void			RenderModulePrefs( void );
		void			LoadModule( void );
		void			UnloadModule( void );
		
		t_module_pref	&m_prefs;
		GtkWidget		*m_top_container;	///< this holds all the widgets to be reset when the module is reloaded
};

/// An account prefs panel
class ay_account_panel : public ay_prefs_window_panel
{
	public:
		ay_account_panel( const char *inTopFrameText, t_account_pref &inPrefs );
		
		virtual void	Build( GtkWidget *inParent );
		virtual void	Apply( void );

	private:	// Gtk callbacks
		static void		s_connect_account( GtkWidget *widget, void *data );
		static void		s_disconnect_account( GtkWidget *widget, void *data );
		
	private:
		void			RenderAccountPrefs( void );
		void			ConnectAccount( void );
		void			DisconnectAccount( void );
		
		t_account_pref	&m_prefs;
		GtkWidget		*m_top_container;	///< this holds all the widgets to be reset when the account is reloaded
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
	_( "Chat" ),
	_( "Chat:Logs" ),
	_( "Chat:Tabs" ),
	_( "Sound" ),
	_( "Sound:Files" ),
	_( "Misc" ),
	_( "Advanced" ),
#ifdef HAVE_ICONV
	_( "Advanced:Encoding" ),
#endif
	_( "Advanced:Proxy" ),
	_( "Accounts" ),
	_( "Services" ),
	_( "Filters" ),
	_( "Utilities" ),
	_( "Importers" )
};

ay_prefs_window	*ay_prefs_window::s_only_prefs_window = NULL;

ay_prefs_window::ay_prefs_window( struct prefs &inPrefs )
:	m_prefs( inPrefs ),
	m_prefs_window_widget( NULL ),
	m_tree( NULL ),
	m_panels( NULL )
{
	m_prefs_window_widget = gtk_window_new( GTK_WINDOW_TOPLEVEL );

	gtk_window_set_position( GTK_WINDOW(m_prefs_window_widget), GTK_WIN_POS_MOUSE );
	gtk_window_set_policy( GTK_WINDOW(m_prefs_window_widget), TRUE, TRUE, TRUE );
	gtk_widget_realize( m_prefs_window_widget );
	gtk_window_set_title( GTK_WINDOW(m_prefs_window_widget), _("Ayttm Preferences") );
	gtkut_set_window_icon( m_prefs_window_widget->window, NULL );
	gtk_container_set_border_width( GTK_CONTAINER(m_prefs_window_widget), 5 );
	
	gint height=460;
	if(height > gdk_screen_height() - 40)
		height = gdk_screen_height() - 40;

	gtk_window_set_default_size(GTK_WINDOW(m_prefs_window_widget), -1, height);

	gtk_signal_connect( GTK_OBJECT(m_prefs_window_widget), "delete_event", GTK_SIGNAL_FUNC(s_delete_event_callback), this );
	
	GtkWidget	*main_hbox = gtk_hbox_new( FALSE, 5 );

	// a scrolled window for the tree
	GtkWidget	*scrolled_win = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	gtk_widget_set_usize( scrolled_win, 210, height );
	gtk_box_pack_start( GTK_BOX(main_hbox), GTK_WIDGET(scrolled_win), FALSE, FALSE, 0 );

	m_tree = GTK_TREE(gtk_tree_new());
	gtk_tree_set_view_lines( m_tree, FALSE );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(scrolled_win), GTK_WIDGET(m_tree) );
	
	m_notebook_widget = gtk_notebook_new();
	gtk_widget_set_usize( m_notebook_widget, 410, -1 );
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(m_notebook_widget), FALSE );
	
	for ( int i = 0; i < PANEL_MAX; i++ )
	{
		ay_prefs_window_panel	*the_panel = ay_prefs_window_panel::Create( m_prefs_window_widget, m_notebook_widget, m_prefs,
				static_cast<ePanelID>(i), s_titles[i] );
		
		if ( the_panel == NULL )
			continue;
		
		m_panels = g_list_append( m_panels, the_panel );
			
		GtkWidget	*top_level_w = the_panel->TopLevelWidget();
		
		if ( top_level_w != NULL )
		{
			gtk_notebook_append_page( GTK_NOTEBOOK(m_notebook_widget), top_level_w, gtk_label_new( s_titles[i] ) );
			
			the_panel->SetNotebookID( gtk_notebook_page_num( GTK_NOTEBOOK(m_notebook_widget), top_level_w ) );
			
			AddToTree( the_panel->Name(), the_panel );
		}
	}
	
	// now add accounts
	LList	*account_pref = m_prefs.account.account_info;
	
	while ( account_pref != NULL )
	{
		t_account_pref		*pref_info = reinterpret_cast<t_account_pref *>(account_pref->data);
		
		ay_prefs_window_panel	*the_panel = ay_prefs_window_panel::CreateAccountPanel( m_prefs_window_widget, m_notebook_widget, *pref_info );
		
		if ( the_panel != NULL )
		{
			m_panels = g_list_append( m_panels, the_panel );

			GtkWidget	*top_level_w = the_panel->TopLevelWidget();

			if ( top_level_w != NULL )
			{
				gtk_notebook_append_page( GTK_NOTEBOOK(m_notebook_widget), top_level_w, NULL );

				the_panel->SetNotebookID( gtk_notebook_page_num( GTK_NOTEBOOK(m_notebook_widget), top_level_w ) );

				AddToTree( the_panel->Name(), the_panel );
			}
		}
		
		account_pref = account_pref->next;
	}

	// now add modules
	LList	*module_pref = m_prefs.module.module_info;
	
	while ( module_pref != NULL )
	{
		t_module_pref		*pref_info = reinterpret_cast<t_module_pref *>(module_pref->data);
		
		ay_prefs_window_panel	*the_panel = ay_prefs_window_panel::CreateModulePanel( m_prefs_window_widget, m_notebook_widget, *pref_info );
		
		if ( the_panel != NULL )
		{
			m_panels = g_list_append( m_panels, the_panel );

			GtkWidget	*top_level_w = the_panel->TopLevelWidget();

			if ( top_level_w != NULL )
			{
				gtk_notebook_append_page( GTK_NOTEBOOK(m_notebook_widget), top_level_w, NULL );

				the_panel->SetNotebookID( gtk_notebook_page_num( GTK_NOTEBOOK(m_notebook_widget), top_level_w ) );

				AddToTree( the_panel->Name(), the_panel );
			}
		}
		
		module_pref = module_pref->next;
	}
	
	gtk_widget_show( m_notebook_widget );
	gtk_widget_show( GTK_WIDGET(m_tree) );
	gtk_widget_show( scrolled_win );
	gtk_widget_show( main_hbox );

	GtkWidget	*prefs_vbox = gtk_vbox_new( FALSE, 7 );
	gtk_widget_show( prefs_vbox );
	gtk_box_pack_start( GTK_BOX(prefs_vbox), m_notebook_widget, TRUE, TRUE, 0 );

	// OK Button
	GtkWidget	*hbox2 = gtk_hbox_new( TRUE, 5 );
	gtk_widget_show( hbox2 );
	gtk_widget_set_usize( hbox2, 200, 25 );

	GtkAccelGroup *accel_group = gtk_accel_group_new();

	GtkWidget	*button = gtkut_create_icon_button( _("OK"), ok_xpm, m_prefs_window_widget );
	gtk_widget_show( button );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC( s_ok_callback ), this );
	gtk_box_pack_start( GTK_BOX(hbox2), button, TRUE, TRUE, 5 );
	gtk_widget_add_accelerator(button, "clicked", accel_group,
			GDK_Return, 0, GTK_ACCEL_VISIBLE);
	gtk_widget_add_accelerator(button, "clicked", accel_group,
			GDK_KP_Enter, 0, GTK_ACCEL_VISIBLE);

	// Cancel Button
	button = gtkut_create_icon_button( _("Cancel"), cancel_xpm, m_prefs_window_widget );
	gtk_widget_show( button );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC( s_cancel_callback ), this );
	gtk_box_pack_start( GTK_BOX(hbox2), button, TRUE, TRUE, 5 );
	gtk_widget_add_accelerator(button, "clicked", accel_group,
			GDK_Escape, 0, GTK_ACCEL_VISIBLE);

	GtkWidget	*hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );

	gtk_box_pack_end( GTK_BOX(hbox), hbox2, FALSE, FALSE, 5 );
	gtk_box_pack_start( GTK_BOX(prefs_vbox), hbox, FALSE, FALSE, 5 );
	gtk_box_pack_start( GTK_BOX(main_hbox), prefs_vbox, FALSE, FALSE, 3 );

	gtk_container_add( GTK_CONTAINER(m_prefs_window_widget), main_hbox );

	gtk_window_add_accel_group(GTK_WINDOW(m_prefs_window_widget), 
			accel_group);
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

	gtk_widget_destroy( m_prefs_window_widget );
	
	s_only_prefs_window = NULL;
}

// Show
void	ay_prefs_window::Show( void )
{
	if ( m_prefs_window_widget != NULL )
	{
		if(!GTK_WIDGET_VISIBLE( m_prefs_window_widget )) {
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
		} else {
			gdk_window_show( m_prefs_window_widget->window );
		}
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
	int			position = -1;	// inserting at position -1 puts it at the end
	
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
		
		// find the position to put it in alphabetically
		child = GTK_TREE(section_tree)->children;
		position = 0;
		
		while ( child != NULL )
		{
			GtkWidget	*the_tree_item = GTK_WIDGET(child->data);
			GtkLabel	*label = GTK_LABEL(GTK_BIN(the_tree_item)->child);
			gchar		*text = NULL;

			gtk_label_get( label, &text );

			if ( strncmp( text, child_name, name_len ) > 0 )
				break;
			
			child = child->next;
			position++;	
		}
	}
	else
	{
		child_name = name;
	}
	
	GtkWidget	*item = gtk_tree_item_new_with_label( child_name );
	gtk_widget_show( item );

	gtk_tree_insert( GTK_TREE(section_tree), item, position );

	gtk_signal_connect( GTK_OBJECT(item), "select", GTK_SIGNAL_FUNC(s_tree_item_selected), inPanel );
}

// OK
void	ay_prefs_window::OK( void )
{
	GList	*iter = m_panels;
	
	while ( iter != NULL )
	{
		ay_prefs_window_panel	*the_panel = reinterpret_cast<ay_prefs_window_panel *>(iter->data);
		
		if ( the_panel != NULL )
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

	for(GList *iter = s_only_prefs_window->m_panels; iter; iter=g_list_next(iter))
	{
		ay_prefs_window_panel *old_panel = reinterpret_cast<ay_prefs_window_panel *>(iter->data);
		if(old_panel->GetNotebookID() == gtk_notebook_get_current_page(
					GTK_NOTEBOOK(s_only_prefs_window->m_notebook_widget)))
		{
			gtk_window_remove_accel_group(GTK_WINDOW(s_only_prefs_window->m_prefs_window_widget),
					the_panel->m_accel_group);
			break;
		}
	}
		
	the_panel->Show();
	gtk_window_add_accel_group(GTK_WINDOW(s_only_prefs_window->m_prefs_window_widget), 
			the_panel->m_accel_group);
}

// s_delete_event_callback
int	ay_prefs_window::s_delete_event_callback( GtkWidget* widget, GdkEvent *inEvent, gpointer data )
{
	// don't allow closing of the window from the window manager
	//	i.e. force the user to click OK or Cancel
	return( TRUE );
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
	m_super_vbox = gtk_vbox_new( FALSE, 3 );
	gtk_widget_show( m_super_vbox );
	gtk_container_set_border_width( GTK_CONTAINER(m_super_vbox), 3 );
	
	if ( inTopFrameText )
		AddTopFrame( inTopFrameText );

	m_accel_group = gtk_accel_group_new();

	m_top_scrollbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_top_scrollbox), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX(m_super_vbox), GTK_WIDGET(m_top_scrollbox), TRUE, TRUE, 0 );
	gtk_widget_set_usize( m_top_scrollbox, -1, 460 );
	gtk_widget_show(m_top_scrollbox);

	m_top_vbox = gtk_vbox_new( FALSE, 0 );
	gtk_widget_show( m_top_vbox );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(m_top_scrollbox), GTK_WIDGET(m_top_vbox) );
}

GtkWidget *ay_prefs_window_panel::_gtkut_button( const char *inText, int *inValue, GtkWidget *inPage )
{
	return gtkut_button( inText, inValue, inPage, m_accel_group );
}		

// Create
ay_prefs_window_panel	*ay_prefs_window_panel::Create( GtkWidget *inParentWindow, GtkWidget *inParent, struct prefs &inPrefs,
	ay_prefs_window::ePanelID inPanelID, const char *inName )
{
	ay_prefs_window_panel	*new_panel = NULL;
	const char				*section_info = NULL;
	
	switch ( inPanelID )
	{
		case ay_prefs_window::PANEL_CHAT_GENERAL:
			new_panel = new ay_chat_panel( inName, inPrefs.chat );
			break;
		
		case ay_prefs_window::PANEL_CHAT_LOGS:
			new_panel = new ay_logs_panel( inName, inPrefs.logging );
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
		
		case ay_prefs_window::PANEL_MISC:
			new_panel = new ay_misc_panel( inName, inPrefs.general );
			break;
		
		case ay_prefs_window::PANEL_ADVANCED:
			section_info = _("This section is for configuration of advanced options such as proxy\nservers and encoding conversion [if your system supports them].");
			break;
		
		case ay_prefs_window::PANEL_PROXY:
			new_panel = new ay_proxy_panel( inName, inPrefs.advanced );
			break;

#ifdef HAVE_ICONV
		case ay_prefs_window::PANEL_ENCODING:
			new_panel = new ay_encoding_panel( inName, inPrefs.advanced );
			break;
#endif

		case ay_prefs_window::PANEL_ACCOUNTS:
			section_info = _("All your messenger accounts are listed here.  You may change\ntheir settings, and sign on/off from here.");
			break;

		case ay_prefs_window::PANEL_SERVICES:
			section_info = _("Services allow you to connect to and chat with people using a\nvariety of messenger protocols.");
			break;

		case ay_prefs_window::PANEL_UTILITIES:
			section_info = _("Utilities add additional capabilities to Ayttm.");
			break;
		
		case ay_prefs_window::PANEL_FILTERS:
			section_info = _("Filters change messages when sending and/or receiving them.\nThis may be used to add functionality such as translation and\nencryption.");
			break;
			
		case ay_prefs_window::PANEL_IMPORTERS:
			section_info = _("Importers allow you to import contacts and other preferences\ninto Ayttm from a variety of other applications.");
			break;
		
		default:
			assert( false );
			break;
	}
	
	if ( section_info != NULL )
	{
		assert( new_panel == NULL );
		
		new_panel = new ay_section_info_panel( inName, ay_prefs_window::s_titles[inPanelID], section_info );
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
		snprintf( name, name_len, "%s:%s", _( "Services" ), inPrefs.module_name );
	else if ( !strcmp( inPrefs.module_type, "FILTER" ) )
		snprintf( name, name_len, "%s:%s", _( "Filters" ), inPrefs.module_name );
	else if ( !strcmp( inPrefs.module_type, "IMPORTER" ) )
		snprintf( name, name_len, "%s:%s", _( "Importers" ), inPrefs.module_name );
	else if ( !strcmp( inPrefs.module_type, "SMILEY" ) )
		snprintf( name, name_len, "%s:%s", _( "Utilities" ), inPrefs.module_name );
	else if ( !strcmp( inPrefs.module_type, "UTILITY" ) )
		snprintf( name, name_len, "%s:%s", _( "Utilities" ), inPrefs.module_name );
	else
		snprintf( name, name_len, "%s:%s", _( "Other Plugins" ), inPrefs.module_name );
	
	ay_prefs_window_panel	*new_panel = new_panel = new ay_module_panel( name, inPrefs );
			
	assert( new_panel != NULL );
	
	new_panel->m_parent_window = inParentWindow;
	new_panel->m_parent = inParent;
	new_panel->Build( inParent );
	
	return( new_panel );
}

// CreateAccountPanel
ay_prefs_window_panel	*ay_prefs_window_panel::CreateAccountPanel( GtkWidget *inParentWindow, GtkWidget *inParent, t_account_pref &inPrefs )
{
	const int	name_len = 64;
	char		name[name_len];
	
	
	snprintf( name, name_len, "%s:%s:%s", _( "Accounts" ), get_service_name(inPrefs.service_id),
			inPrefs.screen_name );
	ay_prefs_window_panel	*new_panel = new_panel = new ay_account_panel( name, inPrefs );
			
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
	GdkColor	the_colour;
	
	gdk_color_parse( ayttm_yellow, &the_colour );
	
	GtkRcStyle	*rc_style = gtk_rc_style_new();
	rc_style->bg[GTK_STATE_NORMAL] = the_colour;
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
	
	gtk_box_pack_start( GTK_BOX(m_super_vbox), frame, FALSE, FALSE, 0 );
}


////////////////
//// ay_section_info_panel implementation
ay_section_info_panel::ay_section_info_panel( const char *inTopFrameText, const char *inSectionStr, const char *inText )
:	ay_prefs_window_panel( inTopFrameText ),
	m_section_str( NULL ),
	m_text( NULL )
{
	assert( inSectionStr != NULL );
	assert( inText != NULL );
	
	m_section_str = strdup( inSectionStr );
	m_text = strdup( inText );
}

ay_section_info_panel::~ay_section_info_panel( void )
{
	if ( m_section_str != NULL )
		free( (char *)m_section_str );
		
	if ( m_text != NULL )
		free( (char *)m_text );
}

// Build
void	ay_section_info_panel::Build( GtkWidget *inParent )
{
	GtkWidget	*label = gtk_label_new( m_text );
	gtk_widget_show( label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 10 );
	
	const int	buffer_len = 128;
	char		buffer[128];
	
	snprintf( buffer, buffer_len, _("[There are no general preferences for the %s section]"), m_section_str );
	
	label = gtk_label_new( buffer );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), label, FALSE, FALSE, 5 );
}

////////////////
//// ay_chat_panel implementation

ay_chat_panel::ay_chat_panel( const char *inTopFrameText, struct prefs::chat &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_dictionary_entry( NULL ),
	m_font_face_entry( NULL ),
	m_font_sel_win( NULL ),
	m_font_sel_conn_id( 0 )
{
}

// Build
void	ay_chat_panel::Build( GtkWidget *inParent )
{
	_gtkut_button( _("Send idle/away status to servers"), &m_prefs.do_send_idle_time, m_top_vbox );
	_gtkut_button( _("Show timestamps in chat window"), &m_prefs.do_convo_timestamp, m_top_vbox );
	_gtkut_button( _("Raise chat-window when receiving a message"), &m_prefs.do_raise_window, m_top_vbox );
	_gtkut_button( _("Ignore unknown people"), &m_prefs.do_ignore_unknown, m_top_vbox );
	
	GtkWidget *hbox = NULL;
	GtkWidget *spacer = NULL;
	GtkWidget *label = NULL;
	GtkWidget *button = NULL;
	
#ifdef HAVE_LIBPSPELL
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	button = _gtkut_button( _("Use spell checking"), &m_prefs.do_spell_checking, hbox );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(s_toggle_checkbox), this );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
	
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, 15, -1 );
	gtk_box_pack_start( GTK_BOX(hbox), spacer, FALSE, FALSE, 0 );
	
	label = gtk_label_new( _("Alternate dictionary:") );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
	
	m_dictionary_entry = gtk_entry_new();
	gtk_widget_show( m_dictionary_entry );
	gtk_entry_set_text( GTK_ENTRY(m_dictionary_entry), m_prefs.spell_dictionary );
	gtk_box_pack_start( GTK_BOX(hbox), m_dictionary_entry, TRUE, TRUE, 10 );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
#endif
	
	// spacer
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), spacer, FALSE, FALSE, 0 );
	
	// message received prefs
	GtkWidget	*info_frame = gtk_frame_new( _( "In messages I receive:" ) );
	gtk_widget_show( info_frame );
	
	GtkWidget *vbox = gtk_vbox_new( FALSE, 3 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER(info_frame), vbox );

	_gtkut_button( _("Ignore fonts"), &m_prefs.do_ignore_font, vbox );
	_gtkut_button( _("Ignore foreground colors"), &m_prefs.do_ignore_fore, vbox );
	_gtkut_button( _("Ignore background colors"), &m_prefs.do_ignore_back, vbox );
	
	gtk_box_pack_start( GTK_BOX(m_top_vbox), info_frame, FALSE, FALSE, 0 );
	
	// spacer
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 10 );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), spacer, FALSE, FALSE, 0 );

	// font selection
	hbox = gtk_hbox_new( FALSE, 0 );

	label = gtk_label_new( _("Font: ") );
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );
	gtk_widget_show( label );

	m_font_face_entry = gtk_entry_new();
	gtk_widget_set_usize( m_font_face_entry, 300, -1 );

	button = gtk_button_new_with_label( _("Choose") );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(s_browse_font), this ); 
	
	const int	buff_len = 1024;
	char		buff [buff_len];
	g_snprintf( buff, buff_len, "%s", m_prefs.font_face );
		
	gtk_entry_set_text( GTK_ENTRY(m_font_face_entry), buff );
	gtk_editable_set_position( GTK_EDITABLE(m_font_face_entry), 0 );
	
	gtk_box_pack_start( GTK_BOX(hbox), m_font_face_entry, FALSE, FALSE, 3 );
	gtk_widget_show( m_font_face_entry );

	gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, FALSE, 0 );
	gtk_widget_show( button );

	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );
	
	SetActiveWidgets();
}

// Apply
void	ay_chat_panel::Apply( void )
{
	char	*ptr = gtk_entry_get_text( GTK_ENTRY(m_font_face_entry) );
	
	if ( ptr != NULL )
		strncpy(m_prefs.font_face, ptr, MAX_PREF_LEN );
		
#ifdef HAVE_LIBPSPELL
	bool	needs_reload = false;
	
	if ( m_prefs.do_spell_checking )
	{
		const char	*new_dict = gtk_entry_get_text( GTK_ENTRY(m_dictionary_entry) );

		if ( (new_dict != NULL) && strncmp( new_dict, m_prefs.spell_dictionary, MAX_PREF_LEN ) )
			needs_reload = true;
	}
	
	strncpy( m_prefs.spell_dictionary, gtk_entry_get_text(GTK_ENTRY(m_dictionary_entry)), MAX_PREF_LEN );
	
	if ( needs_reload )
		ay_spell_check_reload();
#endif
}

// SetActiveWidgets
void	ay_chat_panel::SetActiveWidgets( void )
{
#ifdef HAVE_LIBPSPELL
	gtk_widget_set_sensitive( m_dictionary_entry, m_prefs.do_spell_checking );
#endif
}

////
// ay_chat_panel callbacks

// s_toggle_checkbox
void	ay_chat_panel::s_toggle_checkbox( GtkWidget *widget, int *data )
{
	ay_chat_panel	*the_panel = reinterpret_cast<ay_chat_panel *>( data );
	assert( the_panel != NULL );

	the_panel->SetActiveWidgets();
}

// s_browse_font
void	ay_chat_panel::s_browse_font( GtkWidget *widget, void *data )
{
	ay_chat_panel	*the_panel = reinterpret_cast<ay_chat_panel *>( data );
	assert( the_panel != NULL );
 	
	g_return_if_fail( the_panel->m_font_face_entry != NULL );
	
	if ( !the_panel->m_font_sel_win )
	{
		the_panel->m_font_sel_win = gtk_font_selection_dialog_new( _("Font selection") );
		gtk_window_position( GTK_WINDOW(the_panel->m_font_sel_win), GTK_WIN_POS_CENTER );
		gtk_signal_connect_object(
			GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(the_panel->m_font_sel_win)->cancel_button),
			"clicked",
			GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
			GTK_OBJECT(the_panel->m_font_sel_win) );
	}
	
	if ( the_panel->m_font_sel_conn_id )
	{
		gtk_signal_disconnect( GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(
				the_panel->m_font_sel_win)->ok_button), the_panel->m_font_sel_conn_id );
	}

	the_panel->m_font_sel_conn_id = gtk_signal_connect(
		GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(the_panel->m_font_sel_win)->ok_button),
		"clicked",
		GTK_SIGNAL_FUNC(s_font_selection_ok),
		(GtkObject *)the_panel );

	char	*font_name = gtk_editable_get_chars( GTK_EDITABLE(the_panel->m_font_face_entry), 0, -1 );
	gtk_font_selection_dialog_set_font_name( GTK_FONT_SELECTION_DIALOG( the_panel->m_font_sel_win), font_name );
	g_free( font_name );
	
	gtk_window_set_modal( GTK_WINDOW(the_panel->m_font_sel_win), TRUE );
	gtk_window_set_wmclass( GTK_WINDOW(the_panel->m_font_sel_win), "ayttm", "Ayttm" );
	
	gtk_widget_show( the_panel->m_font_sel_win );
	gtk_widget_grab_focus( GTK_FONT_SELECTION_DIALOG(the_panel->m_font_sel_win)->ok_button );
}

// s_font_selection_ok
void	ay_chat_panel::s_font_selection_ok( GtkButton *button, void *data )
{
	ay_chat_panel	*the_panel = reinterpret_cast<ay_chat_panel *>( data );

	gchar	*fontname = gtk_font_selection_dialog_get_font_name(
							GTK_FONT_SELECTION_DIALOG(the_panel->m_font_sel_win) );

	if ( fontname )
	{
#ifdef HAVE_LIBXFT
		/* convert to XFT fontname */
		/*-bitstream-charter-medium-r-normal-*-13-160-*-*-p-*-iso8859-1*/
		/*1         2       3      4 5      6 7  8   9 0 1 2 3      (4)*/
		/*charter-13:bold:slant=italic,oblique*/
		char	**tokens = ay_strsplit( fontname, "-", -1 );
		char	result[1024];
		
		strcpy( result, tokens[2] );
		strcat( result, "-" );

		if ( strcmp( tokens[8], "*" ) )
		{
			int		i = atoi(tokens[8]);
			char	b[10];
			snprintf( b, 10, "%d", i/10 );
			
			strcat( result, b );
		}
		else if ( strcmp( tokens[7], "*" ) )
		{
			strcat( result, tokens[7] );
		}
		else
		{
			strcat( result, "12" );
		}
		
		if ( !strcmp( tokens[3], "bold" ) )
			strcat( result, ":bold" );
		
		if ( !strcmp( tokens[4], "i" ) || !strcmp( tokens[4], "o" ) )
			strcat( result, ":slant=italic,oblique" );

		ay_strfreev( tokens );
		gtk_entry_set_text( GTK_ENTRY(the_panel->m_font_face_entry), result );
#else
		gtk_entry_set_text( GTK_ENTRY(the_panel->m_font_face_entry), fontname );
#endif
	
		gtk_editable_set_position( GTK_EDITABLE(the_panel->m_font_face_entry), 0 );
		
		g_free( fontname );
	}

	gtk_widget_hide( the_panel->m_font_sel_win );
}

////////////////
//// ay_logs_panel implementation

ay_logs_panel::ay_logs_panel( const char *inTopFrameText, struct prefs::logging &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs )
{
}

// Build
void	ay_logs_panel::Build( GtkWidget *inParent )
{
	_gtkut_button( _("Save all conversations to logfiles"), &m_prefs.do_logging, m_top_vbox );
	_gtkut_button( _("Restore last conversation when opening a chat window"), &m_prefs.do_restore_last_conv, m_top_vbox );
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
//// ay_sound_general_panel implementation

ay_sound_general_panel::ay_sound_general_panel( const char *inTopFrameText, struct prefs::sound &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs )
{
}

// Build
void	ay_sound_general_panel::Build( GtkWidget *inParent )
{
	_gtkut_button( _("Disable sounds when I am away"), &m_prefs.do_no_sound_when_away, m_top_vbox );
	_gtkut_button( _("Disable sounds for Ignored people"), &m_prefs.do_no_sound_for_ignore, m_top_vbox );
	_gtkut_button( _("Play sounds when people sign on or off"), &m_prefs.do_online_sound, m_top_vbox );
	_gtkut_button( _("Play a sound when sending a message"), &m_prefs.do_play_send, m_top_vbox );
	_gtkut_button( _("Play a sound when receiving a message"), &m_prefs.do_play_receive, m_top_vbox );
	_gtkut_button( _("Play a special sound when receiving first message"), &m_prefs.do_play_first, m_top_vbox );
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
	gtk_entry_set_text( GTK_ENTRY(widget), inInitialFilename );
	gtk_editable_set_position( GTK_EDITABLE(widget), 0 );
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
void	ay_sound_files_panel::s_setsoundfilename( const char *selected_filename, void *data )
{
	if ( selected_filename == NULL )
		return;

	eb_debug( DBG_CORE, "Just entered setsoundfilename and the selected file is %s\n", selected_filename );
	
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	const ay_sound_files_panel	*the_panel = cb_data->m_panel;
	GtkEntry					*the_entry = NULL;
	
	switch( cb_data->m_sound_id ) 
	{
		case SOUND_BUDDY_ARRIVE: 	
			strncpy( the_panel->m_prefs.BuddyArriveFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_arrivesound_entry);
			break;
		
		case SOUND_BUDDY_LEAVE:	
			strncpy( the_panel->m_prefs.BuddyLeaveFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_leavesound_entry);
			break;
		
		case SOUND_SEND:		
			strncpy( the_panel->m_prefs.SendFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_sendsound_entry);
			break;
		
		case SOUND_RECEIVE:		
			strncpy( the_panel->m_prefs.ReceiveFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_receivesound_entry);
			break;
		
		case SOUND_BUDDY_AWAY: 	
			strncpy( the_panel->m_prefs.BuddyAwayFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_awaysound_entry);
			break;
		
		case SOUND_FIRSTMSG:		
			strncpy( the_panel->m_prefs.FirstMsgFilename, selected_filename, MAX_PREF_LEN);
			the_entry = GTK_ENTRY(the_panel->m_firstmsgsound_entry);
			break;
		
		default:
			assert( false );
			break;
	}
	
	assert( the_entry != NULL );
	
	gtk_entry_set_text( the_entry, selected_filename);
	gtk_editable_set_position( GTK_EDITABLE(the_entry), 0 );
}

// s_getsoundfile
void	ay_sound_files_panel::s_getsoundfile( GtkWidget *widget, void *data )
{
	eb_debug( DBG_CORE, "Just entered getsoundfile\n" );
	
	t_cb_data	*cb_data = reinterpret_cast<t_cb_data *>(data);
	assert( cb_data != NULL );
	
	const ay_sound_files_panel	*the_panel = cb_data->m_panel;
	GtkEntry					*sound_entry = NULL;
	
	switch( cb_data->m_sound_id ) 
	{
		case SOUND_BUDDY_ARRIVE: 	
			sound_entry = GTK_ENTRY(the_panel->m_arrivesound_entry);
			break;
		
		case SOUND_BUDDY_LEAVE:	
			sound_entry = GTK_ENTRY(the_panel->m_leavesound_entry);
			break;
		
		case SOUND_SEND:		
			sound_entry = GTK_ENTRY(the_panel->m_sendsound_entry);
			break;
		
		case SOUND_RECEIVE:		
			sound_entry = GTK_ENTRY(the_panel->m_receivesound_entry);
			break;
		
		case SOUND_BUDDY_AWAY: 	
			sound_entry = GTK_ENTRY(the_panel->m_awaysound_entry);
			break;
		
		case SOUND_FIRSTMSG:		
			sound_entry = GTK_ENTRY(the_panel->m_firstmsgsound_entry);
			break;
		
		default:
			assert( false );
			break;
	}
	
	const char	*current_sound = gtk_entry_get_text( sound_entry );
	
	ay_do_file_selection( current_sound, _("Select a file to use"), s_setsoundfilename, cb_data );
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
//// ay_misc_panel implementation

ay_misc_panel::ay_misc_panel( const char *inTopFrameText, struct prefs::misc &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_alternate_browser_entry( NULL ),
	m_browser_browse_button( NULL )
{
}

// Build
void	ay_misc_panel::Build( GtkWidget *inParent )
{
	GtkWidget	*brbutton = NULL;
	GtkWidget	*hbox = NULL;
	GtkWidget	*spacer = NULL;
	GtkWidget	*label = NULL;


	brbutton = _gtkut_button( _("Use alternate browser"), &m_prefs.use_alternate_browser, m_top_vbox );
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
	gtk_editable_set_position( GTK_EDITABLE(m_alternate_browser_entry), 0 );
	gtk_box_pack_start( GTK_BOX(hbox), m_alternate_browser_entry, TRUE, TRUE, 10 );

	m_browser_browse_button = gtk_button_new_with_label( _("Browse") );
	gtk_widget_show( m_browser_browse_button );
	gtk_signal_connect( GTK_OBJECT(m_browser_browse_button), "clicked", GTK_SIGNAL_FUNC(s_get_alt_browser_path), this );
	gtk_box_pack_start( GTK_BOX(hbox), m_browser_browse_button, FALSE, FALSE, 5 );

	gtk_box_pack_start( GTK_BOX(m_top_vbox), hbox, FALSE, FALSE, 0 );

	_gtkut_button( _("Enable debug messages"), &m_prefs.do_ayttm_debug, m_top_vbox );
	
	_gtkut_button( _("Show tooltips in status window"), &m_prefs.do_show_tooltips, m_top_vbox );
	
	_gtkut_button( _("Check for latest version when signing on all"), &m_prefs.do_version_check, m_top_vbox );
	
	SetActiveWidgets();
}

// Apply
void	ay_misc_panel::Apply( void )
{
	char	alt_browser_command[MAX_PREF_LEN];
	
	strncpy( alt_browser_command, gtk_entry_get_text(GTK_ENTRY(m_alternate_browser_entry)), MAX_PREF_LEN );
	
	// add "%s" for the URL if the user didn't
	if ( (alt_browser_command[0] != '\0') && !strstr( alt_browser_command, "%s" ) )
		strncat( alt_browser_command, " %s", MAX_PREF_LEN );
	
	strncpy( m_prefs.alternate_browser, alt_browser_command, MAX_PREF_LEN );
}

// SetActiveWidgets
void	ay_misc_panel::SetActiveWidgets( void )
{
	gtk_widget_set_sensitive( m_alternate_browser_entry, m_prefs.use_alternate_browser );
	gtk_widget_set_sensitive( m_browser_browse_button, m_prefs.use_alternate_browser );
}

////
// ay_misc_panel callbacks

// s_toggle_checkbox
void	ay_misc_panel::s_toggle_checkbox( GtkWidget *widget, int *data )
{
	ay_misc_panel	*the_panel = reinterpret_cast<ay_misc_panel *>( data );
	assert( the_panel != NULL );

	the_panel->SetActiveWidgets();
}

// s_set_browser_path
void	ay_misc_panel::s_set_browser_path( const char *selected_filename, void *data )
{
	if ( selected_filename == NULL )
		return;
	
	ay_misc_panel	*the_panel = reinterpret_cast<ay_misc_panel *>( data );
	assert( the_panel != NULL );
		
	gtk_entry_set_text( GTK_ENTRY(the_panel->m_alternate_browser_entry), selected_filename );
	gtk_editable_set_position( GTK_EDITABLE(the_panel->m_alternate_browser_entry), 0 );
}

// s_get_alt_browser_path
void	ay_misc_panel::s_get_alt_browser_path( GtkWidget *t_browser_browse_button, int *data )
{
	eb_debug(DBG_CORE, "Just entered get_alt_browser_path\n");
	
	ay_misc_panel	*the_panel = reinterpret_cast<ay_misc_panel *>( data );
	assert( the_panel != NULL );

	const char	*alt_browser_text = gtk_entry_get_text( GTK_ENTRY(the_panel->m_alternate_browser_entry) );
	
	ay_do_file_selection( alt_browser_text, _("Select your browser"), s_set_browser_path, the_panel );
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

	GtkWidget	*button = _gtkut_button( _("Use recoding in conversations"), &m_prefs.use_recoding, m_top_vbox );
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
//// ay_module_panel implementation

ay_module_panel::ay_module_panel( const char *inTopFrameText, t_module_pref &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_top_container( NULL )
{
}

// Build
void	ay_module_panel::Build( GtkWidget *inParent )
{
	if ( m_top_container != NULL )
		gtk_widget_destroy( m_top_container );
		
	m_top_container = gtk_vbox_new( FALSE, 0 );
	gtk_widget_show( m_top_container );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), m_top_container, TRUE, TRUE, 0 );
	
	const bool	has_error = (m_prefs.status_desc != NULL) && (m_prefs.status_desc[0] != '\0');
	
	GtkWidget	*info_frame = gtk_frame_new( _( "Info" ) );
	gtk_widget_show( info_frame );
	
	GtkWidget	*info_table = gtk_table_new( 5, 2, FALSE );
	gtk_widget_show( info_table );
	gtk_container_set_border_width( GTK_CONTAINER(info_table), 3 );

	gtk_container_add( GTK_CONTAINER(info_frame), info_table );
	
	int	row = 0;
	AddInfoRow( info_table, row++, _( "Description:" ), m_prefs.description );
	AddInfoRow( info_table, row++, _( "Version:" ), m_prefs.version );
	AddInfoRow( info_table, row++, _( "Date:" ), m_prefs.date );
	AddInfoRow( info_table, row++, _( "Path:" ), m_prefs.file_name );
	AddInfoRow( info_table, row++, _( "Status:" ), m_prefs.loaded_status );
	
	gtk_box_pack_start( GTK_BOX(m_top_container), info_frame, FALSE, FALSE, 5 );
	
	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	
	gtk_box_pack_start( GTK_BOX(m_top_container), spacer, FALSE, FALSE, 0 );

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

		gtk_box_pack_start( GTK_BOX(m_top_container), frame, FALSE, FALSE, 0 );
	}
	else
	{	
		if ( m_prefs.pref_list != NULL )
		{
			RenderModulePrefs();
		}
		else
		{	
			GtkWidget	*label = gtk_label_new( NULL );
			gtk_widget_show( label );

			GString		*labelText = NULL;
			
			labelText = g_string_new( _("[The module '") );
			labelText = g_string_append( labelText, m_prefs.module_name );
			
			if ( !m_prefs.is_loaded )
				labelText = g_string_append( labelText, _("' is not loaded]") );
			else
				labelText = g_string_append( labelText, _("' has no preferences]") );

			gtk_label_set_text( GTK_LABEL(label), labelText->str );
			gtk_box_pack_start( GTK_BOX(m_top_container), GTK_WIDGET(label), FALSE, FALSE, 2 );

			g_string_free( labelText, TRUE );
		}
	}
	
	LList	*smiley_sets = ay_get_smiley_sets();
	
	if ( !strcmp( m_prefs.module_type, "SMILEY" ) && (smiley_sets != NULL) )
	{	
		spacer = gtk_label_new( "" );
		gtk_widget_show( spacer );
		gtk_widget_set_usize( spacer, -1, 5 );
		gtk_box_pack_start( GTK_BOX(m_top_container), spacer, FALSE, FALSE, 0 );
		
		// a scrolled window for the smiley sets
		GtkWidget	*scrolled_win = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
		
		gtk_box_pack_start( GTK_BOX(m_top_container), GTK_WIDGET(scrolled_win), TRUE, TRUE, 0 );
		
		GtkWidget	*vbox = gtk_vbox_new( FALSE, 0 );
		gtk_widget_show( vbox );
		
		while ( smiley_sets != NULL )
		{
			t_smiley_set	*the_set = reinterpret_cast<t_smiley_set *>(smiley_sets->data);
			
			GtkWidget	*smilies_frame = gtk_frame_new( the_set->set_name );
			gtk_container_set_border_width( GTK_CONTAINER(smilies_frame), 5 );
			gtk_widget_show( smilies_frame );
			
			GtkWidget	*smilies_table = gtk_table_new( 0, 0, TRUE );
			gtk_widget_show( smilies_table );
			gtk_container_set_border_width( GTK_CONTAINER(smilies_table), 3 );
	
			int			row = 0;
			int			col = -1;
			int			num_icons = 0;
			const int	num_cols = 5;

			LList 		*smiley_list = the_set->set_smiley_list;
	
			for (; smiley_list != NULL; smiley_list = l_list_next(smiley_list) )
			{
				smiley	*de_smile = reinterpret_cast<smiley *>(smiley_list->data);
				
				if ( de_smile == NULL )
					continue;

				if ( col < num_cols )
				{
					col++;
				}
				
				if ( col == num_cols )
				{
					row++;
					col = 0;
				}

				GtkWidget	*icon = gtkut_create_icon_widget( de_smile->pixmap, m_parent_window );
				gtk_widget_set_name( icon, de_smile->name );

				gtk_table_attach( GTK_TABLE(smilies_table),
					icon,
					col, col + 1,
					row, row + 1,
					GTK_EXPAND, GTK_EXPAND, 4, 4 );

				num_icons++;
			}

			gtk_table_resize( GTK_TABLE(smilies_table), num_icons/num_cols +((num_icons % num_cols == 0) ? 0:1), num_cols );
			gtk_container_add( GTK_CONTAINER(smilies_frame), smilies_table );
	
			gtk_box_pack_start( GTK_BOX(vbox), smilies_frame, FALSE, FALSE, 0 );
			
			smiley_sets = smiley_sets->next;
		}
		
		gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(scrolled_win), GTK_WIDGET(vbox) );
	
		gtk_widget_show( scrolled_win );
	}
	
	GtkWidget	*unload_button = gtkut_create_label_button( _( "Unload Module" ), GTK_SIGNAL_FUNC(s_unload_module), this );
	gtk_container_set_border_width( GTK_CONTAINER(unload_button), 5 );
	gtk_widget_set_sensitive( unload_button, m_prefs.is_loaded );
	
	GtkWidget	*load_button = gtkut_create_label_button( _( "Load Module" ), GTK_SIGNAL_FUNC(s_load_module), this );
	gtk_container_set_border_width( GTK_CONTAINER(load_button), 5 );
	gtk_widget_set_sensitive( load_button, !m_prefs.is_loaded );
	
	GtkWidget	*button_hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( button_hbox );
	gtk_box_pack_start( GTK_BOX(button_hbox), unload_button, FALSE, FALSE, 0 );
	gtk_box_pack_end( GTK_BOX(button_hbox), load_button, FALSE, FALSE, 0 );
	
	gtk_box_pack_end( GTK_BOX(m_top_container), button_hbox, FALSE, FALSE, 0 );
	
	GtkWidget	*separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_end( GTK_BOX(m_top_container), separator, FALSE, FALSE, 0 );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	gtk_box_pack_end( GTK_BOX(m_top_container), spacer, FALSE, FALSE, 0 );
}

// Apply
void	ay_module_panel::Apply( void )
{
	input_list	*the_list = m_prefs.pref_list;

	
	while ( the_list != NULL )
	{
		switch(the_list->type)
		{
			case EB_INPUT_CHECKBOX:
				break;
				
			case EB_INPUT_ENTRY:
				{
					GtkWidget	*entry_widget = reinterpret_cast<GtkWidget *>(the_list->widget.entry.entry);
					const char	*text = gtk_entry_get_text(GTK_ENTRY(entry_widget));
					
					strncpy( the_list->widget.entry.value, text, MAX_PREF_LEN );
					
					gtk_entry_set_text( GTK_ENTRY(entry_widget), the_list->widget.entry.value );
				}
				break;
				
			case EB_INPUT_LIST:
				{
					GtkWidget	*list_widget = reinterpret_cast<GtkWidget *>(the_list->widget.listbox.widget);
					GtkWidget	*list_item = gtk_menu_get_active(GTK_MENU(list_widget));
					const char	*text = gtk_widget_get_name(GTK_WIDGET(list_item));
					if(text)
						*the_list->widget.listbox.value = atoi(text);
				}
				break;
		}

		the_list = the_list->next;
	}
}

// AddInfoRow
void	ay_module_panel::AddInfoRow( GtkWidget *inTable, int inRow, const char *inHeader, const char *inInfo )
{
	GtkWidget	*label = gtk_label_new( inHeader );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.0 );
	gtk_widget_show( label );
	gtk_widget_set_usize( label, 65, -1 );
	gtk_table_attach( GTK_TABLE(inTable), label, 0, 1, inRow, inRow + 1, GTK_SHRINK, GTK_FILL, 10, 0 );
	
	label = gtk_label_new( inInfo );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.0 );
	gtk_widget_show( label );
	gtk_label_set_line_wrap( GTK_LABEL(label), TRUE );
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_table_attach( GTK_TABLE(inTable), label, 1, 2, inRow, inRow + 1, GTK_FILL, GTK_SHRINK, 10, 0 );
}

// RenderModulePrefs
void	ay_module_panel::RenderModulePrefs( void )
{
	input_list	*the_list = m_prefs.pref_list;

	
	while ( the_list != NULL )
	{
		switch ( the_list->type )
		{
			case EB_INPUT_CHECKBOX:
				{
					char	*item_label = NULL;
					
					if ( the_list->widget.checkbox.label != NULL )
						item_label = the_list->widget.checkbox.label;
					else
						item_label = the_list->widget.checkbox.name;
						
					_gtkut_button( item_label, the_list->widget.checkbox.value, m_top_container );
					the_list->widget.checkbox.saved_value = *(the_list->widget.checkbox.value);
				}
				break;

			case EB_INPUT_ENTRY:
				{
					GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
					gtk_widget_show( hbox );
						
					char	*item_label = NULL;
					
					if ( the_list->widget.entry.label != NULL )
						item_label = the_list->widget.entry.label;
					else
						item_label = the_list->widget.entry.name;
					
					GtkWidget	*label = gtk_label_new( item_label );
					gtk_widget_show( label );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_set_usize( label, 130, 15 );
					gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

					GtkWidget	*widget = gtk_entry_new();
					gtk_widget_show( widget );
					the_list->widget.entry.entry = widget;
					gtk_entry_set_text( GTK_ENTRY(widget), the_list->widget.entry.value );
					gtk_editable_set_position( GTK_EDITABLE(widget), 0 );
					gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, FALSE, 0 );

					gtk_box_pack_start( GTK_BOX(m_top_container), hbox, FALSE, FALSE, 0 );

				}
				break;
			
			case EB_INPUT_LIST:
				{
					GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
					gtk_widget_show( hbox );
						
					char	*item_label = NULL;
					
					if ( the_list->widget.listbox.label != NULL )
						item_label = the_list->widget.listbox.label;
					else
						item_label = the_list->widget.listbox.name;
					
					GtkWidget	*label = gtk_label_new( item_label );
					gtk_widget_show( label );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_set_usize( label, 130, 15 );
					gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

					GtkWidget	*menu = gtk_option_menu_new();
					gtk_widget_show(menu);
					GtkWidget	*widget = gtk_menu_new();
					gtk_widget_show( widget );
					the_list->widget.listbox.widget = widget;
					
					int i; LList *l;
					for(i=0, l=the_list->widget.listbox.list; l; l=l_list_next(l), i++) {
						char *label = (char *)l->data;
						char name[10];
						GtkWidget *w = gtk_menu_item_new_with_label(label);
						gtk_widget_show(w);
						snprintf(name, sizeof(name), "%d", i);
						gtk_widget_set_name(w, name);
						gtk_menu_append(GTK_MENU(widget), w);
					}
					
					gtk_option_menu_set_menu(GTK_OPTION_MENU(menu), widget);
					gtk_option_menu_set_history(GTK_OPTION_MENU(menu), 
							*the_list->widget.listbox.value);

					gtk_box_pack_start( GTK_BOX(hbox), menu, FALSE, FALSE, 0 );

					gtk_box_pack_start( GTK_BOX(m_top_container), hbox, FALSE, FALSE, 0 );

				}
				break;
			
			default:
				assert( FALSE );
				break;
		}

		the_list = the_list->next;
	}
}

// LoadModule
void	ay_module_panel::LoadModule( void )
{
	int	err = ayttm_prefs_load_module( &m_prefs );
	
	if ( err == 0 )
		Build( m_parent );
}

// UnloadModule
void	ay_module_panel::UnloadModule( void )
{
	int	err = ayttm_prefs_unload_module( &m_prefs );
	
	if ( err == 0 )
		Build( m_parent );
}

////
// ay_module_panel callbacks

// s_load_module
void	ay_module_panel::s_load_module( GtkWidget *widget, void *data )
{
	ay_module_panel	*the_panel = reinterpret_cast<ay_module_panel *>( data );
	assert( the_panel != NULL );

	the_panel->LoadModule();
}

// s_unload_module
void	ay_module_panel::s_unload_module( GtkWidget *widget, void *data )
{
	ay_module_panel	*the_panel = reinterpret_cast<ay_module_panel *>( data );
	assert( the_panel != NULL );

	the_panel->UnloadModule();
}

////////////////
//// ay_account_panel implementation

ay_account_panel::ay_account_panel( const char *inTopFrameText, t_account_pref &inPrefs )
:	ay_prefs_window_panel( inTopFrameText ),
	m_prefs( inPrefs ),
	m_top_container( NULL )
{
}

// Build
void	ay_account_panel::Build( GtkWidget *inParent )
{
	if ( m_top_container != NULL )
		gtk_widget_destroy( m_top_container );
		
	m_top_container = gtk_vbox_new( FALSE, 0 );
	gtk_widget_show( m_top_container );
	gtk_box_pack_start( GTK_BOX(m_top_vbox), m_top_container, TRUE, TRUE, 0 );

	GtkWidget	*spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	gtk_box_pack_start( GTK_BOX(m_top_container), spacer, FALSE, FALSE, 0 );

	if ( m_prefs.pref_list != NULL )
	{
		RenderAccountPrefs();
	} else {	
		GtkWidget	*label = gtk_label_new( NULL );
		gtk_widget_show( label );

		GString		*labelText = NULL;

		labelText = g_string_new( _("[The account '") );
		labelText = g_string_append( labelText, m_prefs.screen_name );
		labelText = g_string_append( labelText, _("' has no preferences]") );

		gtk_label_set_text( GTK_LABEL(label), labelText->str );
		gtk_box_pack_start( GTK_BOX(m_top_container), GTK_WIDGET(label), FALSE, FALSE, 2 );

		g_string_free( labelText, TRUE );
	}

	GtkWidget	*connect_button = gtkut_create_label_button( _( "Connect" ), GTK_SIGNAL_FUNC(s_connect_account), this );
	gtk_container_set_border_width( GTK_CONTAINER(connect_button), 5 );
	gtk_widget_set_sensitive( connect_button, !m_prefs.is_connected );
	
	GtkWidget	*disconnect_button = gtkut_create_label_button( _( "Disconnect" ), GTK_SIGNAL_FUNC(s_disconnect_account), this );
	gtk_container_set_border_width( GTK_CONTAINER(disconnect_button), 5 );
	gtk_widget_set_sensitive( disconnect_button, m_prefs.is_connected );
	
	GtkWidget	*button_hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( button_hbox );
	gtk_box_pack_start( GTK_BOX(button_hbox), connect_button, FALSE, FALSE, 0 );
	gtk_box_pack_end( GTK_BOX(button_hbox), disconnect_button, FALSE, FALSE, 0 );
	
	gtk_box_pack_end( GTK_BOX(m_top_container), button_hbox, FALSE, FALSE, 0 );
	
	GtkWidget	*separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_end( GTK_BOX(m_top_container), separator, FALSE, FALSE, 0 );
	
	spacer = gtk_label_new( "" );
	gtk_widget_show( spacer );
	gtk_widget_set_usize( spacer, -1, 5 );
	gtk_box_pack_end( GTK_BOX(m_top_container), spacer, FALSE, FALSE, 0 );
}

// Apply
void	ay_account_panel::Apply( void )
{
	input_list	*the_list = m_prefs.pref_list;

	
	while ( the_list != NULL )
	{
		switch(the_list->type)
		{
			case EB_INPUT_CHECKBOX:
				break;
				
			case EB_INPUT_PASSWORD:
			case EB_INPUT_ENTRY:
				{
					GtkWidget	*entry_widget = reinterpret_cast<GtkWidget *>(the_list->widget.entry.entry);
					const char	*text = gtk_entry_get_text(GTK_ENTRY(entry_widget));

					strncpy( the_list->widget.entry.value, text, MAX_PREF_LEN );

					gtk_entry_set_text( GTK_ENTRY(entry_widget), the_list->widget.entry.value );
				}
				break;
				
			case EB_INPUT_LIST:
				{
					GtkWidget	*list_widget = reinterpret_cast<GtkWidget *>(the_list->widget.listbox.widget);
					GtkWidget	*list_item = gtk_menu_get_active(GTK_MENU(list_widget));
					const char	*text = gtk_widget_get_name(GTK_WIDGET(list_item));
					if(text)
						*the_list->widget.listbox.value = atoi(text);
				}
				break;
		}

		the_list = the_list->next;
	}
}

// RenderAccountPrefs
void	ay_account_panel::RenderAccountPrefs( void )
{
	input_list	*the_list = m_prefs.pref_list;

	while ( the_list != NULL )
	{
		switch ( the_list->type )
		{
			case EB_INPUT_HIDDEN:
				break;

			case EB_INPUT_CHECKBOX:
				{
					char	*item_label = NULL;

					if ( the_list->widget.checkbox.label != NULL )
						item_label = the_list->widget.checkbox.label;
					else
						item_label = the_list->widget.checkbox.name;

					_gtkut_button( item_label, the_list->widget.checkbox.value, m_top_container );
					the_list->widget.checkbox.saved_value = *(the_list->widget.checkbox.value);
				}
				break;

			case EB_INPUT_ENTRY:
				{
					GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
					gtk_widget_show( hbox );

					char	*item_label = NULL;

					if ( the_list->widget.entry.label != NULL )
						item_label = the_list->widget.entry.label;
					else
						item_label = the_list->widget.entry.name;

					GtkWidget	*label = gtk_label_new( "" );
					int key = gtk_label_parse_uline(GTK_LABEL(label), item_label);
					gtk_widget_show( label );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_set_usize( label, 130, 15 );
					gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

					GtkWidget	*widget = gtk_entry_new();
					gtk_widget_show( widget );
					gtk_widget_add_accelerator(widget, "grab_focus", m_accel_group,
							key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
					the_list->widget.entry.entry = widget;
					gtk_entry_set_text( GTK_ENTRY(widget), the_list->widget.entry.value );
					gtk_editable_set_position( GTK_EDITABLE(widget), 0 );
					gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, FALSE, 0 );

					gtk_box_pack_start( GTK_BOX(m_top_container), hbox, FALSE, FALSE, 0 );

				}
				break;

			case EB_INPUT_PASSWORD:
				{
					GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
					gtk_widget_show( hbox );
						
					char	*item_label = NULL;
					
					if ( the_list->widget.entry.label != NULL )
						item_label = the_list->widget.entry.label;
					else
						item_label = the_list->widget.entry.name;
					
					GtkWidget	*label = gtk_label_new( "" );
					int key = gtk_label_parse_uline(GTK_LABEL(label), item_label);
					gtk_widget_show( label );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_set_usize( label, 130, 15 );
					gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

					GtkWidget	*widget = gtk_entry_new();
					gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
					gtk_widget_show( widget );
					gtk_widget_add_accelerator(widget, "grab_focus", m_accel_group,
							key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
					the_list->widget.entry.entry = widget;
					gtk_entry_set_text( GTK_ENTRY(widget), the_list->widget.entry.value );
					gtk_editable_set_position( GTK_EDITABLE(widget), 0 );
					gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, FALSE, 0 );

					gtk_box_pack_start( GTK_BOX(m_top_container), hbox, FALSE, FALSE, 0 );

				}
				break;
			
			case EB_INPUT_LIST:
				{
					GtkWidget	*hbox = gtk_hbox_new( FALSE, 3 );
					gtk_widget_show( hbox );
						
					char	*item_label = NULL;
					
					if ( the_list->widget.listbox.label != NULL )
						item_label = the_list->widget.listbox.label;
					else
						item_label = the_list->widget.listbox.name;
					
					GtkWidget	*label = gtk_label_new( "" );
					int key = gtk_label_parse_uline(GTK_LABEL(label), item_label);
					gtk_widget_show( label );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_set_usize( label, 130, 15 );
					gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

					GtkWidget	*menu = gtk_option_menu_new();
					gtk_widget_show(menu);
					GtkWidget	*widget = gtk_menu_new();
					gtk_widget_show( widget );
					the_list->widget.listbox.widget = widget;
					
					int i; LList *l;
					for(i=0, l=the_list->widget.listbox.list; l; l=l_list_next(l), i++) {
						char *label = (char *)l->data;
						char name[10];
						GtkWidget *w = gtk_menu_item_new_with_label(label);
						gtk_widget_show(w);
						snprintf(name, sizeof(name), "%d", i);
						gtk_widget_set_name(w, name);
						gtk_menu_append(GTK_MENU(widget), w);
					}
					
					gtk_option_menu_set_menu(GTK_OPTION_MENU(menu), widget);
					gtk_option_menu_set_history(GTK_OPTION_MENU(menu), 
							*the_list->widget.listbox.value);

					gtk_widget_add_accelerator(menu, "grab_focus", m_accel_group,
							key, GDK_MOD1_MASK, (GtkAccelFlags) 0);

					gtk_box_pack_start( GTK_BOX(hbox), menu, FALSE, FALSE, 0 );

					gtk_box_pack_start( GTK_BOX(m_top_container), hbox, FALSE, FALSE, 0 );

				}
				break;
			
			default:
				assert( FALSE );
				break;
		}

		the_list = the_list->next;
	}
}

// ConnectAccount
void	ay_account_panel::ConnectAccount( void )
{
	ayttm_prefs_connect_account( &m_prefs );
	Build( m_parent );
}

// DisconnectAccount
void	ay_account_panel::DisconnectAccount( void )
{
	ayttm_prefs_disconnect_account( &m_prefs );
	Build( m_parent );
}

////
// ay_account_panel callbacks

// s_connect_account
void	ay_account_panel::s_connect_account( GtkWidget *widget, void *data )
{
	ay_account_panel	*the_panel = reinterpret_cast<ay_account_panel *>( data );
	assert( the_panel != NULL );

	the_panel->ConnectAccount();
}

// s_disconnect_account
void	ay_account_panel::s_disconnect_account( GtkWidget *widget, void *data )
{
	ay_account_panel	*the_panel = reinterpret_cast<ay_account_panel *>( data );
	assert( the_panel != NULL );

	the_panel->DisconnectAccount();
}
