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

#include "util.h"
#include "globals.h"
#include "service.h"
#include "intl.h"
#include "message_parse.h"
#include "file_select.h"


static void send_file_callback( const char *selected_filename, void *data )
{
	eb_account	*ea = (eb_account *)data;
	eb_account	*x_fer_account = NULL;
    
	if ( (selected_filename == NULL) || (ea == NULL) )
		return;
        
	x_fer_account = find_suitable_file_transfer_account(ea, ea->account_contact);

	if ( x_fer_account != NULL ) {
		eb_local_account *ela = find_suitable_local_account_for_remote( x_fer_account, NULL );
		
		RUN_SERVICE(ela)->send_file( ela, x_fer_account, (char *)selected_filename );
	} else {
		eb_local_account *ela = find_suitable_local_account_for_remote( ea, NULL );
		
		strncpy( filename, selected_filename, sizeof(filename) );
		RUN_SERVICE(ela)->send_im( ela, ea, "EB_COMMAND SEND_FILE" );
	}
}

void eb_do_send_file( eb_account *ea )
{
	ay_do_file_selection( NULL, _("Select file to send"), send_file_callback, ea );
}

void eb_view_log( struct contact *contact )
{
	if ( !contact ) {
		eb_debug(DBG_CORE, "Contact is null!\n");
		return;
	}

	if ( contact->logwindow == NULL )
		contact->logwindow = ay_log_window_contact_create( contact );
}

