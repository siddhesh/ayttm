/*
 * EveryBuddy 
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
 *Modified By Ivor Cave on 29 Apr 2003 to make it more l337!!! :)
 *email ivor@happyfragger.com
 */

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif

#include "intl.h"

#include <stdlib.h>
#include <string.h>

#include "externs.h"
#include "plugin_api.h"
#include "prefs.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info middle_LTX_plugin_info
#define plugin_init middle_LTX_plugin_init
#define plugin_finish middle_LTX_plugin_finish
#define module_version middle_LTX_module_version


/* Function Prototypes */
static char *plstripHTML(const eb_local_account * local, const eb_account * remote,
			 const struct contact *contact, const char *s);
static int middle_init( void );
static int middle_finish( void );

static int	s_doLeet = 0;
static int	s_doExtremeLeet = 0;

static int	s_ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info =
{
	PLUGIN_FILTER,
	"L33t-o-matic",
	"Turns all incoming and outgoing messages into l33t-speak",
	"$Revision: 1.8 $",
	"$Date: 2003/05/06 17:04:50 $",
	&s_ref_count,
	middle_init,
	middle_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int middle_init( void )
{
	input_list *il = calloc( 1, sizeof(input_list) );
	plugin_info.prefs = il;

	il->widget.checkbox.value = &s_doLeet;
	il->widget.checkbox.name = "s_doLeet";
	il->widget.entry.label = strdup( _("Enable L33t-speak conversion") );
	il->type = EB_INPUT_CHECKBOX;
	
	il->next = calloc( 1, sizeof(input_list) );
	il = il->next;
	il->widget.checkbox.value = &s_doExtremeLeet;
	il->widget.checkbox.name = "s_doExtremeLeet";
	il->widget.entry.label = strdup( _("Enable 3x7r3m3 L33t-speak [implies previous]") );
	il->type = EB_INPUT_CHECKBOX;

	eb_debug(DBG_MOD, "L33tSp33k initialised\n");

	outgoing_message_filters = l_list_append( outgoing_message_filters, &plstripHTML );
	incoming_message_filters = l_list_append( incoming_message_filters, &plstripHTML );

	return( 0 );
}

static int middle_finish( void )
{
	eb_debug(DBG_MOD, "L33tSp33k shutting down\n");
	outgoing_message_filters = l_list_remove( outgoing_message_filters, &plstripHTML );
	incoming_message_filters = l_list_remove( incoming_message_filters, &plstripHTML );
	
	while( plugin_info.prefs )
	{
		input_list *il = plugin_info.prefs->next;
		free( plugin_info.prefs );
		plugin_info.prefs = il;
	}

	return( 0 );
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/
static char *plstripHTML(const eb_local_account * local, const eb_account * remote,
			 const struct contact *contact, const char *in)
{
	int		pos = 0;
	int		post = 0;
	char	*s = strdup(in);
	char	*t = NULL;
	
	
	if ( !s_doLeet && !s_doExtremeLeet )
		return( s );
	
	t = calloc( 3, strlen( s ) );

	while ( s[pos] != '\0' )
	{
		switch ( s[pos] )
		{
			case 'a': case 'A':
				t[post] = '4';
				break;
				
			case 'c': case 'C':
				t[post] = '(';
				break;
				
			case 'e': case 'E':
				t[post] = '3';
				break;
				
			case 'l': case 'L':
				t[post] = '1';
				break;
				
			case 's': case 'S':
				t[post] = '5';
				break;
				
			case 't': case 'T':
				t[post] = '7';
				break;
			
			default:
				t[post] = s[pos];
				break;
		}
		
		if ( s_doExtremeLeet )
		{
			switch ( s[pos] )
			{
				case 'd': case 'D':
					t[post++] = '|';
					t[post]   = ')';
					break;

				case 'h': case 'H':
					t[post++] = '|';
					t[post++] ='-';
					t[post]   ='|';
					break;
					
				case 'k': case 'K':
					t[post++] = '|';
					t[post]   ='<';
					break;
					
				case 'm': case 'M':
					t[post++] = '/';
					t[post++] = 'v';
					t[post]   = '\\';
					break;
					
				case 'n': case 'N':
					t[post++] = '|';
					t[post++] = '\\';
					t[post]   = '|';
					break;
					
				case 'o': case 'O':
					t[post++] = '(';
					t[post]   = ')';
					break;
					
				case 'v': case 'V':
					t[post++] = '\\';
					t[post]   = '/';
					break;
					
				case 'w': case 'W':
					t[post++] = '\\';
					t[post++] = '^';
					t[post]   = '/';
					break;

				default:
					break;
			}
		}
		
		pos++;
		post++;
	}
	
	t[post] = '\0';
	
	free( s );
	
	return( t );
}
