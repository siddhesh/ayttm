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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "service.h"
#include "value_pair.h"
#include "globals.h"
#include "defaults.h"
#include "libproxy/libproxy.h"
#include "prefs_window.h"
#include "plugin.h"

#ifdef __MINGW32__
#define	snprintf	_snprintf
#endif


enum {
	CORE_PREF,
	PLUGIN_PREF,
	SERVICE_PREF
};

typedef struct _ptr_list
{
	char	key[MAX_PREF_NAME_LEN+1];
	void	*value;
} ptr_list;


static LList 	*s_global_prefs = NULL;


static int s_compare_ptr_key( const void *a, const void *b )
{
	if ( !strcmp( ((ptr_list *)a)->key, (const char *)b) )
		return( 0 );
	
	return( 1 );
}

static int	s_compare_plugin_name( const void *a, const void *b)
{
	const module_pref *pref = a;
	
	if ( pref->file_name && !strcmp( pref->file_name, (const char *)b) )
		return( 0 );
		
	return( 1 );
}

static void s_add_pref( const char *key, void *data )
{
	ptr_list *pref_data = calloc( 1, sizeof( ptr_list ) );
	
	strcpy( pref_data->key, key );
	pref_data->value=(void *)data;
	
	s_global_prefs = l_list_append( s_global_prefs, pref_data );
}

static char	*s_strip_whitespace( char *inStr )
{
	char	*ptr = inStr;
	int		len = strlen( inStr );
	
	
	if ( (inStr == NULL) || (inStr[0] == '\0') )
		return( inStr );
		
	while ( isspace( *ptr ) )
		ptr++;
	
	if ( ptr != inStr )
	{
		const int	new_len = len - (ptr - inStr);
		
		memmove( inStr, ptr, new_len );
		inStr[new_len] = '\0';
	}
	
	len = strlen( inStr ) - 1;
	ptr = inStr + len;
	
	while ( isspace( *ptr ) )
		ptr--;
	
	if ( ptr != (inStr + len) )
	{
		ptr++;
		*ptr = '\0';
	}
	
	return( inStr );
}

static char	*s_strdup_allow_null( const char *inStr )
{
	if ( inStr == NULL )
		return( NULL );
		
	return( strdup( inStr ) );
}

static char	*s_strdup_pref( const char *inStr )
{
	char	*returnStr = NULL;
	
	returnStr = calloc( 1, MAX_PREF_LEN );
	
	if ( inStr == NULL )
		returnStr[0] = '\0';
	else	
		strncpy( returnStr, inStr, MAX_PREF_LEN );
	
	return( returnStr );
}

static input_list	*s_copy_input_list( input_list *inList )
{
	input_list	*new_list = NULL;
	input_list	*list = inList;
	input_list	*prev = NULL;

	
	while ( list != NULL  )
	{
		input_list	*new_item = calloc( 1, sizeof( input_list ) );
		
			
		new_item->type = list->type;
		new_item->next = NULL;
		
		switch( list->type )
		{
			case EB_INPUT_CHECKBOX:
				{
					new_item->widget.checkbox.name = s_strdup_allow_null( list->widget.checkbox.name );
					new_item->widget.checkbox.label = s_strdup_allow_null( list->widget.checkbox.label );
					new_item->widget.checkbox.value = calloc( 1, sizeof( int ) );
					
					if ( list->widget.checkbox.value != NULL )
						*(new_item->widget.checkbox.value) = *(list->widget.checkbox.value);
					
				}
				break;
				
			case EB_INPUT_ENTRY:
				{
					new_item->widget.entry.name = s_strdup_allow_null( list->widget.entry.name );
					new_item->widget.entry.label = s_strdup_allow_null( list->widget.entry.label );
					new_item->widget.entry.value = s_strdup_pref( list->widget.entry.value );
					new_item->widget.entry.entry = NULL;	/* this will be filled in when rendered - we ignore this field */
				}
				break;
			
			default:
				assert( FALSE );
				break;
		}
		
		if ( prev == NULL )
			new_list = new_item;
		else
			prev->next = new_item;
		
		prev = new_item;
		
		list = list->next;
	}

	return( new_list );
}

static void	s_destroy_input_list( input_list *inList )
{
	input_list	*list = inList;
	
	
	while ( list != NULL  )
	{
		input_list	*saved = list;
		
		switch( list->type )
		{
			case EB_INPUT_CHECKBOX:
				{
					free( list->widget.checkbox.name );
					free( list->widget.checkbox.label );
					free( list->widget.checkbox.value );
				}
				break;
				
			case EB_INPUT_ENTRY:
				{
					free( list->widget.entry.name );
					free( list->widget.entry.label );
					free( list->widget.entry.value );
				}
				break;
			
			default:
				assert( FALSE );
				break;
		}
				
		list = list->next;
		
		memset( saved, 0, sizeof( input_list ) );
		
		free( saved );
	}
}


static LList	*s_create_module_prefs_list( void )
{
	const LList		*plugins = GetPref( EB_PLUGIN_LIST );
	LList			*module_prefs_list = NULL;
	
	for ( ; plugins; plugins = plugins->next )
	{
		const eb_PLUGIN_INFO	*plugin_info = plugins->data;
		module_pref				*pref_info = NULL;
		
		
		if ( plugin_info == NULL )
			continue;
		
		pref_info = calloc( 1, sizeof( module_pref ) );

		pref_info->module_type = PLUGIN_TYPE_TXT[plugin_info->pi.type-1];
		pref_info->brief_desc = s_strdup_allow_null( plugin_info->pi.brief_desc );
		pref_info->loaded_status = PLUGIN_STATUS_TXT[plugin_info->status];
		pref_info->version = s_strdup_allow_null( plugin_info->pi.version );
		pref_info->date = s_strdup_allow_null( plugin_info->pi.date );
		pref_info->file_name = s_strdup_allow_null( plugin_info->name );
		pref_info->status_desc = s_strdup_allow_null( plugin_info->status_desc );
		pref_info->service_name = s_strdup_allow_null( plugin_info->service );
		pref_info->pref_list = s_copy_input_list( plugin_info->pi.prefs );
			
		module_prefs_list = l_list_append( module_prefs_list, pref_info);
	}
	
	return( l_list_nth( module_prefs_list, 0 ) );
}

static void	s_destroy_module_prefs_list( LList *io_module_prefs_list )
{
	for ( ; io_module_prefs_list != NULL; io_module_prefs_list = io_module_prefs_list->next )
	{
		module_pref		*pref_info = io_module_prefs_list->data;
		
		
		if ( pref_info == NULL )
			continue;
			
		free( (void *)pref_info->brief_desc );
		free( (void *)pref_info->version );
		free( (void *)pref_info->date );
		free( (void *)pref_info->file_name );
		free( (void *)pref_info->status_desc );
		free( (void *)pref_info->service_name );
		
		s_destroy_input_list( pref_info->pref_list );
	}
	
	l_list_free( io_module_prefs_list );
}

static void	s_free_pref_struct( struct prefs *ioPrefs )
{
	if ( ioPrefs == NULL )
		return;
		
	s_destroy_module_prefs_list( ioPrefs->module.module_info );
	free( ioPrefs );
}

static void	s_write_module_prefs( void *inListItem, void *inData )
{
	eb_PLUGIN_INFO	*plugin_info = inListItem;
	FILE			*fp = (FILE *)inData;
	LList			*master_prefs = NULL;
	LList			*current_prefs = NULL;


	fprintf( fp, "\t%s\n", plugin_info->name );

	master_prefs = GetPref( plugin_info->name );
	master_prefs = value_pair_remove( master_prefs, "load" );
	
	current_prefs = eb_input_to_value_pair( plugin_info->pi.prefs );

	if ( plugin_info->status == PLUGIN_LOADED )
		current_prefs = value_pair_add( current_prefs, "load", "1" );
	else
		current_prefs = value_pair_add( current_prefs, "load", "0" );

	master_prefs = value_pair_update( master_prefs, current_prefs );
	
	SetPref( plugin_info->name, master_prefs );
	
	value_pair_print_values( master_prefs, fp, 2 );
	
	fprintf( fp, "\tend\n" );
	
	value_pair_free( current_prefs );
}

static void	s_apply_or_cancel_module_prefs( void *inListItem, void *inData )
{
	module_pref	*the_prefs = inListItem;
	int			apply = (int)inData;
	
	
	if ( apply )
	{
		eb_PLUGIN_INFO	*plugin_info = NULL;
		
				
		/* now we have to apply to the things that have pointers in the 'real' list
			NOTE that this will hopefully be solved by changing the way prefs are
			applied to plugins
		*/
		plugin_info = FindPluginByName( the_prefs->file_name );
		
		if ( plugin_info != NULL )
		{
			input_list	*real_list = plugin_info->pi.prefs;
			input_list	*local_copy = the_prefs->pref_list;
			
			while ( real_list != NULL )
			{
				switch ( real_list->type )
				{
					case EB_INPUT_CHECKBOX:
						{
							if ( (real_list->widget.checkbox.value != NULL) &&
								(local_copy->widget.checkbox.value != NULL) )
							{
								*(real_list->widget.checkbox.value) = *(local_copy->widget.checkbox.value);
							}

						}
						break;

					case EB_INPUT_ENTRY:
						{
							strncpy( real_list->widget.entry.value, local_copy->widget.entry.value, MAX_PREF_LEN );
						}
						break;

					default:
						assert( FALSE );
						break;
				}
				
				real_list = real_list->next;
				
				assert( local_copy != NULL );
				
				local_copy = local_copy->next;
			}
		}
	}
}


void	ayttm_prefs_init( void )
{
#ifdef __MINGW32__
	const int	buff_size = 1024;
	char		buff[buff_size];
	const char	*base_dir = "\\Program Files\\ayttm\\";
#endif

	/* set default prefs */

	/* window positions, etc */
	iSetLocalPref( "length_contact_window", 256 );
	iSetLocalPref( "width_contact_window", 150 );
	iSetLocalPref( "status_show_level", 2 );

	/* general */
	iSetLocalPref( "do_ayttm_debug", 0 );
	iSetLocalPref( "do_ayttm_debug_html", 0 );
	iSetLocalPref( "do_plugin_debug", 0 );
	iSetLocalPref( "do_noautoresize", 0 );
	iSetLocalPref( "use_alternate_browser", 0 );
	cSetLocalPref( "alternate_browser", "" );
	cSetLocalPref( "print_cmd", "html2ps %s | lpr" );
	
	iSetLocalPref( "do_spell_checking", 0 );
	cSetLocalPref( "spell_dictionary", "" );
	
	/* logging */
	iSetLocalPref( "do_logging", 1 );
	iSetLocalPref( "do_strip_html", 0 );
	iSetLocalPref( "do_restore_last_conv", 0 );
	
	/* layout */
	iSetLocalPref( "do_tabbed_chat", 0 );
	iSetLocalPref( "do_tabbed_chat_orient", 0 );	/* Tab Orientation:  0 => bottom, 1 => top, 2=> left, 3 => right */
	
	/* chat */
	iSetLocalPref( "do_typing_notify", 1 );
	iSetLocalPref( "do_send_typing_notify", 1 );
	iSetLocalPref( "do_escape_close", 1 );
	iSetLocalPref( "do_convo_timestamp", 1 );
	iSetLocalPref( "do_enter_send", 1 );
	iSetLocalPref( "do_ignore_unknown", 0 );
	iSetLocalPref( "FontSize", 4 );
	iSetLocalPref( "do_multi_line", 1 );
	iSetLocalPref( "do_raise_window", 0 );
	iSetLocalPref( "do_send_idle_time", 0 );
	iSetLocalPref( "do_timestamp", 1 );
	iSetLocalPref( "do_ignore_fore", 1 );
	iSetLocalPref( "do_ignore_back", 1 );
	iSetLocalPref( "do_ignore_font", 1 );
	iSetLocalPref( "do_smiley", 1 );
	
	/* NOTE that the way the defaults for accelerators are stored is likely GTK-specific */
	cSetLocalPref( "accel_prev_tab", "<Control>Left" );
	cSetLocalPref( "accel_next_tab", "<Control>Right" );

	/* sound */
	iSetLocalPref( "do_no_sound_when_away", 0 );
	iSetLocalPref( "do_no_sound_for_ignore", 1 );
	iSetLocalPref( "do_online_sound", 1 );
	iSetLocalPref( "do_play_send", 1 );
	iSetLocalPref( "do_play_first", 1 );
	iSetLocalPref( "do_play_receive", 1 );
	
#ifdef __MINGW32__
	snprintf( buff, buff_size, "%s%s", base_dir, BuddyArriveDefault );
	cSetLocalPref( "BuddyArriveFilename", buff );
		
	snprintf( buff, buff_size, "%s%s", base_dir, BuddyLeaveDefault );
	cSetLocalPref( "BuddyAwayFilename", buff );
		
	snprintf( buff, buff_size, "%s%s", base_dir, BuddyLeaveDefault );
	cSetLocalPref( "BuddyLeaveFilename", buff );
		
	snprintf( buff, buff_size, "%s%s", base_dir, SendDefault );
	cSetLocalPref( "SendFilename", buff );
		
	snprintf( buff, buff_size, "%s%s", base_dir, ReceiveDefault );
	cSetLocalPref( "ReceiveFilename", buff );
		
	snprintf( buff, buff_size, "%s%s", base_dir, ReceiveDefault );
	cSetLocalPref( "FirstMsgFilename", buff );
#else
	cSetLocalPref( "BuddyArriveFilename", BuddyArriveDefault );
	cSetLocalPref( "BuddyAwayFilename", BuddyLeaveDefault );
	cSetLocalPref( "BuddyLeaveFilename", BuddyLeaveDefault );
	cSetLocalPref( "SendFilename", SendDefault );
	cSetLocalPref( "ReceiveFilename", ReceiveDefault );
	cSetLocalPref( "FirstMsgFilename", ReceiveDefault );
#endif

	fSetLocalPref( "SoundVolume", 0.0 );

	/* advanced */
	iSetLocalPref( "proxy_type", 0 );
	cSetLocalPref( "proxy_host", "" );
	iSetLocalPref( "proxy_port", 3128 );
	iSetLocalPref( "do_proxy_auth", 0 );
	cSetLocalPref( "proxy_user", "" );
	cSetLocalPref( "proxy_password", "" );
	
	iSetLocalPref( "use_recoding", 0 );
	cSetLocalPref( "local_encoding", "" );
	cSetLocalPref( "remote_encoding", "" );

	/* modules */
#ifdef __MINGW32__
	snprintf( buff, buff_size, "%s%s", base_dir, MODULE_DIR );
	cSetLocalPref( "modules_path", buff );
#else
	cSetLocalPref( "modules_path", MODULE_DIR );
#endif
}

void	ayttm_prefs_read( void )
{
	const int		buffer_size = 1024;
	char			buff[buffer_size];
	char * const	param = buff;		/* just another name for buff... */
	FILE			*fp = NULL;
	
	
	snprintf( buff, buffer_size, "%sprefs",config_dir );
	
	fp = fopen( buff, "r" );
	
	if ( fp == NULL )
	{
		printf( "Creating prefs file [%s]\n", buff );
		ayttm_prefs_write();
		return;
	}
	
	fgets( param, buffer_size, fp );
	
	while ( !feof( fp ) )
	{
		int		pref_type = CORE_PREF;
		char	*val = buff;
		

		s_strip_whitespace( param );
		
		if ( !strcasecmp( param, "plugins" ) )
			pref_type = PLUGIN_PREF;
		else if ( !strcasecmp( param, "connections" ) )
			pref_type = SERVICE_PREF;
			
		if ( pref_type != CORE_PREF )
		{
			for (;;)
			{
				int		id = -1;
				char	*plugin_name = NULL;
				LList	*session_prefs = NULL;
				
				
				fgets( param, buffer_size, fp );
				
				s_strip_whitespace( param );
				
				if ( !strcasecmp( param, "end" ) )
					break;

				switch( pref_type )
				{
					case PLUGIN_PREF:
						plugin_name = strdup( param );
						break;
						
					case SERVICE_PREF:
						id = get_service_id( param );
						break;
						
					default:
						assert( FALSE );
						break;
				}

				for (;;)
				{
					LList	*old_session_prefs = NULL;
				
					
					fgets( param, buffer_size, fp );
					
					s_strip_whitespace( param );
					
					if ( !strcasecmp( param, "end" ) )
					{
						switch( pref_type )
						{
							case PLUGIN_PREF:
								old_session_prefs = SetPref( plugin_name, session_prefs );
								
								free( plugin_name );
								break;
								
							case SERVICE_PREF:
								old_session_prefs = SetPref( get_service_name(id), session_prefs );
								break;
								
							default:
								assert( FALSE );
								break;
						}
						
						if ( old_session_prefs != NULL )
						{
							eb_debug(DBG_CORE, "Freeing old_session_prefs\n");
							value_pair_free( old_session_prefs );
						}
						break;
					}
					else
					{
						val = param;

						while ( *val != 0 && *val != '=' )
							val++;
							
						if ( *val == '=' )
						{
							*val = '\0';
							val++;
						}

						/* strip off quotes */
						if ( *val == '"' )
						{
							val++;
							val[strlen(val)-1] = '\0';
						}
						
						eb_debug( DBG_CORE, "Adding %s:%s to session_prefs\n", param, val );
						session_prefs = value_pair_add( session_prefs, param, val );
					}
				}
			}
			continue;
		} /* if(pref_type != CORE_PREF) */
		
		val = param;

		while ( *val != 0 && *val != '=' )
			val++;
			
		if ( *val == '=' )
		{
			*val = '\0';
			val++;
		}
		
		cSetLocalPref( param, val );
		
		fgets( param, buffer_size, fp );
	}
	
	fclose( fp );

	if ( iGetLocalPref("proxy_type") != 0 )
	{
		proxy_set_proxy( iGetLocalPref("proxy_type"), cGetLocalPref("proxy_host"), iGetLocalPref("proxy_port") );
		proxy_set_auth( iGetLocalPref("do_proxy_auth"), cGetLocalPref("proxy_user"), cGetLocalPref("proxy_password") );
	}
}

void	ayttm_prefs_write( void )
{
	const int	bufferLen = 1024;
	char		buff[bufferLen];
	char		file[bufferLen];
	FILE		*fp = NULL;


	snprintf( buff, bufferLen, "%sprefs.tmp", config_dir );
	snprintf( file, bufferLen, "%sprefs", config_dir );
	 
	fp = fopen( buff, "w" );

	/* window positions, etc. */
	fprintf( fp, "x_contact_window=%d\n", iGetLocalPref("x_contact_window") );
	fprintf( fp, "y_contact_window=%d\n", iGetLocalPref("y_contact_window") );
	fprintf( fp, "status_show_level=%d\n", iGetLocalPref("status_show_level") );
	
	/* general */
    fprintf( fp, "do_ayttm_debug=%d\n", iGetLocalPref("do_ayttm_debug") );
    fprintf( fp, "do_ayttm_debug_html=%d\n", iGetLocalPref("do_ayttm_debug_html") );
    fprintf( fp, "do_plugin_debug=%d\n", iGetLocalPref("do_ayttm_debug") );
    fprintf( fp, "length_contact_window=%d\n", iGetLocalPref("length_contact_window") );
    fprintf( fp, "width_contact_window=%d\n", iGetLocalPref("width_contact_window") );
#ifdef HAVE_ISPELL
    fprintf( fp, "do_spell_checking=%d\n", iGetLocalPref("do_spell_checking") );
	fprintf( fp, "spell_dictionary=%s\n", cGetLocalPref("spell_dictionary") );
#endif
    fprintf( fp, "do_noautoresize=%d\n", iGetLocalPref("do_noautoresize") ) ;
    fprintf( fp, "use_alternate_browser=%d\n", iGetLocalPref("use_alternate_browser") );
	fprintf( fp, "alternate_browser=%s\n", cGetLocalPref("alternate_browser") );
	fprintf( fp, "print_cmd=%s\n", cGetLocalPref("print_cmd") );

	/* logging */
	fprintf( fp, "do_logging=%d\n", iGetLocalPref("do_logging") );
	fprintf( fp, "do_strip_html=%d\n", iGetLocalPref("do_strip_html") );
	fprintf( fp, "do_restore_last_conv=%d\n", iGetLocalPref("do_restore_last_conv") );

	/* layout */
	fprintf( fp, "do_tabbed_chat=%d\n", iGetLocalPref("do_tabbed_chat") );
	fprintf( fp, "do_tabbed_chat_orient=%d\n", iGetLocalPref("do_tabbed_chat_orient") );

	/* chat */
	fprintf( fp, "do_typing_notify=%d\n", iGetLocalPref("do_typing_notify") );
	fprintf( fp, "do_send_typing_notify=%d\n", iGetLocalPref("do_send_typing_notify") );
	fprintf( fp, "do_escape_close=%d\n", iGetLocalPref("do_escape_close") );
	fprintf( fp, "do_convo_timestamp=%d\n", iGetLocalPref("do_convo_timestamp") );
	fprintf( fp, "do_enter_send=%d\n", iGetLocalPref("do_enter_send") );
	fprintf( fp, "do_ignore_unknown=%d\n", iGetLocalPref("do_ignore_unknown") );
	fprintf( fp, "FontSize=%d\n", iGetLocalPref("FontSize") );
	fprintf( fp, "do_multi_line=%d\n", iGetLocalPref("do_multi_line") );
	fprintf( fp, "do_raise_window=%d\n", iGetLocalPref("do_raise_window") );
	fprintf( fp, "do_send_idle_time=%d\n", iGetLocalPref("do_send_idle_time") );
	fprintf( fp, "do_timestamp=%d\n", iGetLocalPref("do_timestamp") );
	fprintf( fp, "do_ignore_fore=%d\n", iGetLocalPref("do_ignore_fore") );
	fprintf( fp, "do_ignore_back=%d\n", iGetLocalPref("do_ignore_back") );
	fprintf( fp, "do_ignore_font=%d\n", iGetLocalPref("do_ignore_font") );
	fprintf( fp, "do_smiley=%d\n", iGetLocalPref("do_smiley" ) );
	fprintf( fp, "accel_next_tab=%s\n", cGetLocalPref("accel_next_tab") );
	fprintf( fp, "accel_prev_tab=%s\n", cGetLocalPref("accel_prev_tab") );

	 /* sound */
	fprintf( fp, "do_no_sound_when_away=%d\n", iGetLocalPref("do_no_sound_when_away") );
	fprintf( fp, "do_no_sound_for_ignore=%d\n", iGetLocalPref("do_no_sound_for_ignore") );
	fprintf( fp, "do_online_sound=%d\n", iGetLocalPref("do_online_sound") );
	fprintf( fp, "do_play_send=%d\n", iGetLocalPref("do_play_send") );
	fprintf( fp, "do_play_first=%d\n", iGetLocalPref("do_play_first") );
	fprintf( fp, "do_play_receive=%d\n", iGetLocalPref("do_play_receive") );
	fprintf( fp, "BuddyArriveFilename=%s\n", cGetLocalPref("BuddyArriveFilename") );
	fprintf( fp, "BuddyAwayFilename=%s\n", cGetLocalPref("BuddyAwayFilename"));
	fprintf( fp, "BuddyLeaveFilename=%s\n", cGetLocalPref("BuddyLeaveFilename") );
	fprintf( fp, "SendFilename=%s\n", cGetLocalPref("SendFilename") );
	fprintf( fp, "ReceiveFilename=%s\n", cGetLocalPref("ReceiveFilename") );
	fprintf( fp, "FirstMsgFilename=%s\n", cGetLocalPref("FirstMsgFilename") );
	fprintf( fp, "SoundVolume=%f\n", fGetLocalPref("SoundVolume") );

	/* advanced */
	fprintf( fp, "proxy_type=%d\n", iGetLocalPref("proxy_type") );
	fprintf( fp, "proxy_host=%s\n", cGetLocalPref("proxy_host") );
	fprintf( fp, "proxy_port=%d\n", iGetLocalPref("proxy_port") );
	fprintf( fp, "do_proxy_auth=%d\n", iGetLocalPref("do_proxy_auth") );
	fprintf( fp, "proxy_user=%s\n", cGetLocalPref("proxy_user") );
	fprintf( fp, "proxy_password=%s\n", cGetLocalPref("proxy_password") );

	fprintf( fp, "use_recoding=%d\n", iGetLocalPref("use_recoding") );
	fprintf( fp, "local_encoding=%s\n", cGetLocalPref("local_encoding") );
	fprintf( fp, "remote_encoding=%s\n", cGetLocalPref("remote_encoding") );
	 
	/* modules */
	fprintf( fp, "modules_path=%s\n", cGetLocalPref("modules_path") );
	fprintf( fp, "plugins\n" );
	l_list_foreach( GetPref(EB_PLUGIN_LIST), s_write_module_prefs, fp );
	fprintf( fp, "end\n" );
	
	fclose( fp );

#ifdef __MINGW32__
	unlink( file );
#endif

	rename( buff, file );
}

void	ayttm_prefs_show_window( void )
{
	struct prefs	*prefs = calloc( 1, sizeof( struct prefs ) );

	
	/* general prefs */
	prefs->general.do_ayttm_debug        = iGetLocalPref("do_ayttm_debug");
	prefs->general.use_alternate_browser = iGetLocalPref("use_alternate_browser");
	strncpy( prefs->general.alternate_browser, cGetLocalPref("alternate_browser"), MAX_PREF_LEN );

	prefs->general.do_spell_checking     = iGetLocalPref("do_spell_checking");
	strncpy( prefs->general.spell_dictionary, cGetLocalPref("spell_dictionary"), MAX_PREF_LEN );

	/* logging prefs */
	prefs->logging.do_logging            = iGetLocalPref("do_logging");
	prefs->logging.do_restore_last_conv  = iGetLocalPref("do_restore_last_conv");

	/* layout prefs */
	prefs->layout.do_tabbed_chat        = iGetLocalPref("do_tabbed_chat");
	prefs->layout.do_tabbed_chat_orient = iGetLocalPref("do_tabbed_chat_orient");

	/* chat prefs */
	prefs->chat.do_ignore_unknown     = iGetLocalPref("do_ignore_unknown");
	prefs->chat.font_size	          = iGetLocalPref("FontSize");
	prefs->chat.do_raise_window       = iGetLocalPref("do_raise_window");	
	prefs->chat.do_send_idle_time     = iGetLocalPref("do_send_idle_time");
	prefs->chat.do_ignore_fore        = iGetLocalPref("do_ignore_fore");
	prefs->chat.do_ignore_back        = iGetLocalPref("do_ignore_back");
	prefs->chat.do_ignore_font        = iGetLocalPref("do_ignore_font");
	
	strncpy( prefs->chat.accel_next_tab, cGetLocalPref("accel_next_tab"), MAX_PREF_LEN );
	strncpy( prefs->chat.accel_prev_tab, cGetLocalPref("accel_prev_tab"), MAX_PREF_LEN );
	
	/* sound prefs */
	prefs->sound.do_no_sound_when_away = iGetLocalPref("do_no_sound_when_away");
	prefs->sound.do_no_sound_for_ignore= iGetLocalPref("do_no_sound_for_ignore");
	prefs->sound.do_online_sound       = iGetLocalPref("do_online_sound");
	prefs->sound.do_play_send          = iGetLocalPref("do_play_send");
	prefs->sound.do_play_first         = iGetLocalPref("do_play_first");
	prefs->sound.do_play_receive       = iGetLocalPref("do_play_receive");
	
	strncpy( prefs->sound.BuddyArriveFilename, cGetLocalPref("BuddyArriveFilename"), MAX_PREF_LEN );
	strncpy( prefs->sound.BuddyAwayFilename, cGetLocalPref("BuddyAwayFilename"), MAX_PREF_LEN );
	strncpy( prefs->sound.BuddyLeaveFilename, cGetLocalPref("BuddyLeaveFilename"), MAX_PREF_LEN );
	strncpy( prefs->sound.SendFilename, cGetLocalPref("SendFilename"), MAX_PREF_LEN );
	strncpy( prefs->sound.ReceiveFilename, cGetLocalPref("ReceiveFilename"), MAX_PREF_LEN );
	strncpy( prefs->sound.FirstMsgFilename, cGetLocalPref("FirstMsgFilename"), MAX_PREF_LEN );

	prefs->sound.SoundVolume        = fGetLocalPref("SoundVolume");
	
	/* advanced */
	prefs->advanced.proxy_type      = iGetLocalPref("proxy_type");
	strncpy( prefs->advanced.proxy_host, cGetLocalPref("proxy_host"), MAX_PREF_LEN );
	prefs->advanced.proxy_port      = iGetLocalPref("proxy_port");
	prefs->advanced.do_proxy_auth   = iGetLocalPref("do_proxy_auth");
	strncpy( prefs->advanced.proxy_user, cGetLocalPref("proxy_user"), MAX_PREF_LEN );
	strncpy( prefs->advanced.proxy_password, cGetLocalPref("proxy_password"), MAX_PREF_LEN );
	
	prefs->advanced.use_recoding    = iGetLocalPref("use_recoding");
	strncpy( prefs->advanced.local_encoding, cGetLocalPref("local_encoding"), MAX_PREF_LEN );
	strncpy( prefs->advanced.remote_encoding, cGetLocalPref("remote_encoding"), MAX_PREF_LEN );

	/* modules */
	prefs->module.module_info = s_create_module_prefs_list();
	
	ayttm_prefs_window_create( prefs );
}

module_pref	*ayttm_prefs_find_module_by_name( const struct prefs *inPrefs, const char *inName )
{
	LList	*module_list = NULL;
	LList 	*search_result =NULL;
	
	
	if ( (inPrefs == NULL) || (inName == NULL) )
		return( NULL );
	
	module_list = inPrefs->module.module_info;
	
	if ( module_list == NULL )
		return( NULL );

	search_result = l_list_find_custom( module_list, inName, s_compare_plugin_name );
	
	if ( search_result == NULL )
		return( NULL );
		
	return( search_result->data );
}

void	ayttm_prefs_apply( struct prefs *inPrefs )
{
	assert( inPrefs != NULL );
	
	/* general */
	iSetLocalPref( "do_ayttm_debug", inPrefs->general.do_ayttm_debug );
	iSetLocalPref( "do_plugin_debug", inPrefs->general.do_ayttm_debug );
	iSetLocalPref( "use_alternate_browser", inPrefs->general.use_alternate_browser );
	cSetLocalPref( "alternate_browser", inPrefs->general.alternate_browser );
	
	iSetLocalPref( "do_spell_checking", inPrefs->general.do_spell_checking );
	cSetLocalPref( "spell_dictionary", inPrefs->general.spell_dictionary );
	
	/* logging */
	iSetLocalPref( "do_logging", inPrefs->logging.do_logging );
	iSetLocalPref( "do_restore_last_conv", inPrefs->logging.do_restore_last_conv );
	
	/* layout */
	iSetLocalPref( "do_tabbed_chat", inPrefs->layout.do_tabbed_chat );
	iSetLocalPref( "do_tabbed_chat_orient", inPrefs->layout.do_tabbed_chat_orient );

	/* chat */
	iSetLocalPref( "do_ignore_unknown", inPrefs->chat.do_ignore_unknown );
	iSetLocalPref( "FontSize", inPrefs->chat.font_size );
	iSetLocalPref( "do_raise_window", inPrefs->chat.do_raise_window );
	iSetLocalPref( "do_send_idle_time", inPrefs->chat.do_send_idle_time );
	iSetLocalPref( "do_ignore_fore", inPrefs->chat.do_ignore_fore );
	iSetLocalPref( "do_ignore_back", inPrefs->chat.do_ignore_back );
	iSetLocalPref( "do_ignore_font", inPrefs->chat.do_ignore_font );
	
	cSetLocalPref( "accel_prev_tab", inPrefs->chat.accel_prev_tab );
	cSetLocalPref( "accel_next_tab", inPrefs->chat.accel_next_tab );
	
	/* sound */
	iSetLocalPref( "do_no_sound_when_away", inPrefs->sound.do_no_sound_when_away );
	iSetLocalPref( "do_no_sound_for_ignore", inPrefs->sound.do_no_sound_for_ignore );
	iSetLocalPref( "do_online_sound", inPrefs->sound.do_online_sound );
	iSetLocalPref( "do_play_send", inPrefs->sound.do_play_send );
	iSetLocalPref( "do_play_first", inPrefs->sound.do_play_first );
	iSetLocalPref( "do_play_receive", inPrefs->sound.do_play_receive );
	
	cSetLocalPref( "BuddyArriveFilename", inPrefs->sound.BuddyArriveFilename );
	cSetLocalPref( "BuddyAwayFilename", inPrefs->sound.BuddyAwayFilename );
	cSetLocalPref( "BuddyLeaveFilename", inPrefs->sound.BuddyLeaveFilename );
	cSetLocalPref( "SendFilename", inPrefs->sound.SendFilename );
	cSetLocalPref( "ReceiveFilename", inPrefs->sound.ReceiveFilename );
	cSetLocalPref( "FirstMsgFilename", inPrefs->sound.FirstMsgFilename );
	
	fSetLocalPref( "SoundVolume", inPrefs->sound.SoundVolume );

	/* advanced */
	iSetLocalPref( "proxy_type", inPrefs->advanced.proxy_type );
	cSetLocalPref( "proxy_host", inPrefs->advanced.proxy_host );
	iSetLocalPref( "proxy_port", inPrefs->advanced.proxy_port );
	iSetLocalPref( "do_proxy_auth", inPrefs->advanced.do_proxy_auth );
	cSetLocalPref( "proxy_user", inPrefs->advanced.proxy_user );
	cSetLocalPref( "proxy_password", inPrefs->advanced.proxy_password );
	
	iSetLocalPref( "use_recoding", inPrefs->advanced.use_recoding );
	cSetLocalPref( "local_encoding", inPrefs->advanced.local_encoding );
	cSetLocalPref( "remote_encoding", inPrefs->advanced.remote_encoding );

	/* modules */
	l_list_foreach( inPrefs->module.module_info, s_apply_or_cancel_module_prefs, (void *)1 );
	
	ayttm_prefs_write();
	
	if ( inPrefs->advanced.proxy_type != 0 )
	{
		proxy_set_proxy( iGetLocalPref("proxy_type"), cGetLocalPref("proxy_host"), iGetLocalPref("proxy_port") );
		proxy_set_auth( iGetLocalPref("do_proxy_auth"), cGetLocalPref("proxy_user"), cGetLocalPref("proxy_password") );
	}
	
	s_free_pref_struct( inPrefs );
}

void	ayttm_prefs_cancel( struct prefs *inPrefs )
{
	assert( inPrefs != NULL );
	
	l_list_foreach( inPrefs->module.module_info, s_apply_or_cancel_module_prefs, (void *)0 );
	
	s_free_pref_struct( inPrefs );
}

/* Find old pref data, and replace with new, returning old data */
void	*SetPref( const char *key, void *data )
{
	ptr_list	*pref_data = NULL;
	void		*old_data = NULL;
	LList		*list_data = l_list_find_custom( s_global_prefs, key, s_compare_ptr_key );


	if ( !list_data )
	{
		s_add_pref( key, data );
		
		return( NULL );
	}
	
	pref_data = (ptr_list *)list_data->data;
	old_data = pref_data->value;
	pref_data->value=data;
	
	return( old_data );
}

void	*GetPref( const char *key )
{
	ptr_list	*pref_data = NULL;
	LList		*list_data = l_list_find_custom( s_global_prefs, key, s_compare_ptr_key );


	if ( !list_data )
		return( NULL );
		
	pref_data = list_data->data;
	
	if ( !pref_data )
		return( NULL );
		
	return( pref_data->value );
}

void	cSetLocalPref( const char *key, char *data )
{
	char	newkey[MAX_PREF_NAME_LEN];
	char	*oldvalue = NULL;


	snprintf( newkey, MAX_PREF_NAME_LEN, "Local::%s", key );
	
	oldvalue = SetPref( newkey, strdup(data) );
	
	if ( oldvalue )
		free( oldvalue );
}

void	iSetLocalPref( const char *key, int data )
{
	char	value[MAX_PREF_LEN];


	snprintf( value, MAX_PREF_LEN, "%i", data );
	cSetLocalPref( key, value );
}

void	fSetLocalPref( const char *key, float data )
{
	char	value[MAX_PREF_LEN];


	snprintf( value, MAX_PREF_LEN, "%f", data );
	cSetLocalPref( key, value );
}

char	*cGetLocalPref(const char *key)
{
	char	newkey[MAX_PREF_NAME_LEN];
	char	*value = NULL;


	snprintf( newkey, MAX_PREF_NAME_LEN, "Local::%s", key );
	
	value = (char *)GetPref( newkey );
	
	if ( !value )
		value="";
		
	return( value );
}

float	fGetLocalPref( const char *key )
{
	float	value = 0.0;
	

	value = atof( cGetLocalPref( key ) );
	
	return( value );
}

int	iGetLocalPref( const char *key )
{
	int	value = 0;


	value = atoi( cGetLocalPref( key ) );
	
	return( value );
}

/* Used when loading service modules later, so passwords and user names are still available
 * as service:username */
void	save_account_info( const char *service, LList *pairs )
{
	const int	buffer_size = 256;
	char		buff[buffer_size];
	char		*val = value_pair_get_value( pairs, "SCREEN_NAME" );
	

	assert( val != NULL );
	
	snprintf( buff, buffer_size, "%s:%s", service, val );
	
	free( val );
	
	SetPref( buff, pairs );
}

