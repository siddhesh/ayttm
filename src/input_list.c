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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "globals.h"
#include "input_list.h"
#include "value_pair.h"


static void	s_convert_space_to_underscore( char *inStr )
{
	char	*ptr = inStr;
	
	if ( ptr == NULL )
		return;
		
	for ( ptr = strchr( inStr, ' ' ); ptr && *ptr; ptr = strchr( ptr, ' ' ) )
		*ptr = '_';
}

LList	*eb_input_to_value_pair( input_list *il )
{
	LList	*vp = NULL;
	char	key[MAX_PREF_NAME_LEN];
	char	value[MAX_PREF_LEN];

	
	while ( il != NULL )
	{
		switch ( il->type )
		{
			case EB_INPUT_CHECKBOX:
				{
					snprintf( key, sizeof(key), "%s", il->widget.checkbox.name );
					s_convert_space_to_underscore( key );
						
					snprintf( value, sizeof(value), "%i", *il->widget.checkbox.value );
					
					vp = value_pair_add( vp, key, value );
				}
				break;
				
			case EB_INPUT_ENTRY:
				{
					snprintf( key, sizeof(key), "%s", il->widget.entry.name );
					s_convert_space_to_underscore( key );
						
					vp = value_pair_add( vp, key, il->widget.entry.value );
				}
				break;
		}
		
		il = il->next;
	}
	
	return( vp );
}

void	eb_update_from_value_pair( input_list *il, LList *vp )
{
	char	key[MAX_PREF_NAME_LEN];
	char	*value = NULL;

	
	if ( vp == NULL )
		return;
		
	while ( il != NULL )
	{
		switch ( il->type )
		{
			case EB_INPUT_CHECKBOX:
				{
					if ( il->widget.checkbox.value == NULL )
					{
						eb_debug(DBG_CORE, "checkbox.value is NULL\n");
						break;
					}
					
					snprintf( key, sizeof(key), "%s", il->widget.checkbox.name );
					s_convert_space_to_underscore( key );
						
					value = value_pair_get_value( vp, key );

					if ( value != NULL )
					{
						*il->widget.checkbox.value = atoi(value);
						free( value );
					}
				}
				break;
				
			case EB_INPUT_ENTRY:
				{
					if ( il->widget.entry.value == NULL )
					{
						eb_debug(DBG_CORE, "entry.value is NULL\n");
						break;
					}
					
					snprintf( key, sizeof(key), "%s", il->widget.entry.name );
					s_convert_space_to_underscore( key );
					
					value = value_pair_get_value( vp, key );

					if ( value != NULL )
					{
						strncpy( il->widget.entry.value, value, MAX_PREF_LEN );
						free( value );
					}
				}
				break;
		}
		
		il = il->next;
	}
}
