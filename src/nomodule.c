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
 * nomodule.c
 * Empty protocol implementation
 */

#include "intl.h"

#include <stdlib.h>
#include <string.h>

#include "value_pair.h"
#include "service.h"
#include "util.h"
#include "globals.h"

/* Can never be online */
/* #include "pixmaps/nomodule_online.xpm" */
#include "pixmaps/nomodule_away.xpm"


#define SERVICE_INFO nomodule_SERVICE_INFO
/* This will end up being an array, one for each protocol we know about */
struct service SERVICE_INFO = { NULL, -1, SERVICE_CAN_NOTHING, NULL };

struct eb_nomodule_account_data {
	int status;
	int logged_in_time;
	int evil;
};

struct eb_nomodule_local_account_data {
	char password[255];
	int input;
	int keep_alive;
	int status;
};

enum
{
	NOMODULE_OFFLINE=0
};



/*   callbacks used by Ayttm    */

static int eb_nomodule_query_connected(eb_account * account)
{		
	return FALSE;
}

static void eb_nomodule_login( eb_local_account * account )
{
	return;							  
}

static void eb_nomodule_logout( eb_local_account * account )
{
	return;
}

static void eb_nomodule_send_im( eb_local_account * account_from,
				  eb_account * account_to,
				  char * message )
{
	return;
}

static eb_local_account * eb_nomodule_read_local_config(LList * pairs)
{
	eb_local_account * ela = calloc( 1, sizeof( eb_local_account ) );
	struct eb_nomodule_local_account_data * ala = calloc( 1, sizeof( struct eb_nomodule_local_account_data ) );
	char *ptr=NULL;
	
	eb_debug(DBG_CORE, "eb_nomodule_read_local_config: entering\n");	
	/*you know, eventually error handling should be put in here*/
	ptr = value_pair_get_value(pairs, "SCREEN_NAME");
	strncpy(ela->handle, ptr, sizeof(ela->handle));
	free(ptr);
	if(!ela->handle[0]) {
		fprintf(stderr, "Error!  Invalid account config no SCREEN_NAME defined!\n");
		return 0;
	}

	strncpy(ela->alias, ela->handle, MAX_PREF_LEN);
	ptr = value_pair_get_value(pairs, "PASSWORD");
	if(!ptr) {
		fprintf(stderr, "Warning!  No password specified for handle %s\n", ela->handle);
	}
	else {
		strncpy(ala->password, ptr, 255);
		free( ptr );
	}
	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = ala;
	ala->status = 0;
	eb_debug(DBG_CORE, "eb_nomodule_read_local_config: leaving\n");

	return ela;
}

static LList * eb_nomodule_write_local_config( eb_local_account * account )
{
	LList * list = NULL;
	struct eb_nomodule_local_account_data * alad = account->protocol_local_account_data;

	list = value_pair_add( list, "SCREEN_NAME", account->handle );
	list = value_pair_add( list, "PASSWORD", alad->password );

	return list;
}
			
static eb_account * eb_nomodule_read_config( eb_account *ea, LList * config )
{
	struct eb_nomodule_account_data * aad =  calloc( 1, sizeof( struct eb_nomodule_account_data ) );
	
	aad->status = 0;

	ea->protocol_account_data = aad;
	
	return ea;
}

static LList * eb_nomodule_get_states()
{
	return NULL;
}

/* return an error string in case this login is syntaxically 
 * bad, NULL if everything is OK.
 */
static char * eb_nomodule_check_login(char * login, char * pass)
{
	return NULL;
}

static int eb_nomodule_get_current_state(eb_local_account * account )
{
	return 0;
}

static void eb_nomodule_set_current_state( eb_local_account * account, int state )
{
	return;
}

static void eb_nomodule_add_user( eb_account * account )
{
	return;
}

static void eb_nomodule_del_user( eb_account * account )
{
	return;
}

static eb_account * eb_nomodule_new_account(eb_local_account *ela, const char * account )
{
	return NULL;
}

static char * eb_nomodule_get_status_string( eb_account * account )
{
	static char string[255];

	snprintf(string, 255, _("Offline"));		

	return string;
}

static char **eb_nomodule_get_status_pixmap( eb_account * account)
{
	return nomodule_away_xpm;
}

static void eb_nomodule_set_idle( eb_local_account * ela, int idle )
{
	return;
}

static void eb_nomodule_set_away(eb_local_account * account, char * message)
{
	return;
}


static void eb_nomodule_get_info( eb_local_account * from, eb_account * account_to )
{
	return;
}

static input_list * eb_nomodule_get_prefs()
{
	return NULL;
}

static void eb_nomodule_read_prefs_config(LList * values)
{
	return;
}

static LList * eb_nomodule_write_prefs_config()
{
	return NULL;
}

static void	eb_nomodule_free_account_data( eb_account *account )
{
	if ( account == NULL )
		return;
		
	free( account->protocol_account_data );
}

struct service_callbacks * eb_nomodule_query_callbacks()
{
	struct service_callbacks * sc;
	
	sc = calloc( 1, sizeof( struct service_callbacks ) );
	sc->query_connected = eb_nomodule_query_connected;
	sc->login = eb_nomodule_login;
	sc->logout = eb_nomodule_logout;
	sc->send_im = eb_nomodule_send_im;
	sc->read_local_account_config = eb_nomodule_read_local_config;
	sc->write_local_config = eb_nomodule_write_local_config;
	sc->read_account_config = eb_nomodule_read_config;
	sc->get_states = eb_nomodule_get_states;
	sc->get_current_state = eb_nomodule_get_current_state;
	sc->set_current_state = eb_nomodule_set_current_state;
	sc->check_login = eb_nomodule_check_login;
	sc->add_user = eb_nomodule_add_user;
	sc->del_user = eb_nomodule_del_user;
	sc->new_account = eb_nomodule_new_account;
	sc->get_status_string = eb_nomodule_get_status_string;
	sc->get_status_pixmap = eb_nomodule_get_status_pixmap;
	sc->set_idle = eb_nomodule_set_idle;
	sc->set_away = eb_nomodule_set_away;
	sc->send_chat_room_message = NULL;
	sc->join_chat_room = NULL;
	sc->leave_chat_room = NULL;
	sc->make_chat_room = NULL;
	sc->send_invite = NULL;
	sc->accept_invite = NULL;
	sc->decline_invite = NULL;
	sc->get_info = eb_nomodule_get_info;

	sc->get_prefs = eb_nomodule_get_prefs;
	sc->read_prefs_config = eb_nomodule_read_prefs_config;
	sc->write_prefs_config = eb_nomodule_write_prefs_config;

	sc->free_account_data = eb_nomodule_free_account_data;
	
	return sc;
}
