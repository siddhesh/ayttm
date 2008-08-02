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
 */

/*
 * icq.c
 * icq implementation
 */
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif

#include "intl.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include "../libtoc/libtoc.h"
typedef unsigned long u_long;
typedef unsigned long ulong;
#else
#include "libtoc/libtoc.h"
#endif
#include "icq.h"
#include "away_window.h"
#include "util.h"
#include "status.h"
#include "activity_bar.h"
#include "message_parse.h"
#include "value_pair.h"
#include "tcp_util.h"
#include "info_window.h"
#include "plugin_api.h"
#include "smileys.h"
#include "globals.h"
#include "messages.h"
#include "dialog.h"
#include "offline_queue_mgmt.h"
#include "libproxy/libproxy.h"

#include "pixmaps/icq_online.xpm"
#include "pixmaps/icq_away.xpm"


#define DBG_TOC do_icq_debug
int do_icq_debug = 0;

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info icq_toc_LTX_plugin_info
#define SERVICE_INFO icq_toc_LTX_SERVICE_INFO
#define plugin_init icq_toc_LTX_plugin_init
#define plugin_finish icq_toc_LTX_plugin_finish
#define module_version icq_toc_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();

static int ref_count = 0;
static char icq_server[MAX_PREF_LEN] = "toc.oscar.aol.com";
static char icq_port[MAX_PREF_LEN] = "80";
static int icq_fallback_ports[]={9898, 80, 0};
static int icq_last_fallback=0;
static int should_fallback=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"ICQ TOC",
	"Provides ICQ support via the TOC protocol",
	"$Revision: 1.49 $",
	"$Date: 2008/08/02 06:13:09 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "ICQ", -1, SERVICE_CAN_GROUPCHAT | SERVICE_CAN_ICONVERT, NULL };
/* End Module Exports */

static char *eb_toc_get_color(void) { static char color[]="#000088"; return color; }
static eb_chat_room * eb_icq_make_chat_room(char * name, eb_local_account * account, int is_public );

unsigned int module_version() {return CORE_VERSION;}
static int plugin_init()
{
	input_list * il = g_new0(input_list, 1);
	eb_debug(DBG_MOD, "icq-toc\n");
	ref_count=0;
	plugin_info.prefs = il;
	il->widget.entry.value = icq_server;
	il->name = "icq_server";
	il->label = _("Server:");
	il->type = EB_INPUT_ENTRY;

        il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = icq_port;
	il->name = "icq_port";
	il->label = _("Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
       	il = il->next;
       	il->widget.checkbox.value = &do_icq_debug;
       	il->name = "do_icq_debug";
       	il->label = _("Enable debugging");
       	il->type = EB_INPUT_CHECKBOX;

/*
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = icq_info;
	il->name = "icq_info";
	il->label = _("Info:");
	il->type = EB_INPUT_ENTRY;
*/
        return(0);
}

static int plugin_finish()
{
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		g_free(plugin_info.prefs);
		plugin_info.prefs = il;
	}
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return(ref_count);
}

/*************************************************************************************
 *                             End Module Code
 ************************************************************************************/

struct eb_icq_account_data {
		int status;
        time_t idle_time;
        int logged_in_time;
	int evil;
};

struct eb_icq_local_account_data {
	char icq_info[MAX_PREF_LEN]; 
        char password[MAX_PREF_LEN];
        int fd;
	toc_conn * conn;
	int input;
	int keep_alive;
	int status;
	int activity_tag;
	int connect_tag;
	LList * icq_buddies;
	int is_setting_state;
	int prompt_password;	
};

enum
{
	ICQ_ONLINE=0,
	ICQ_AWAY=1,
	ICQ_OFFLINE=2
};


typedef struct _eb_icq_file_request
{
	toc_conn * conn;
	char nick[255];
	char ip[255];
	short port;
	char cookie[255];
	char filename[255];
} eb_icq_file_request;

/* here is the list of locally stored buddies */

/*	There are no prefs for icq-TOC at the moment.
static input_list * icq_prefs = NULL;
*/

static eb_account * eb_icq_new_account(eb_local_account *ela, const char * account );
static void eb_icq_add_user( eb_account * account );
static void eb_icq_login( eb_local_account * account );
static void eb_icq_logout( eb_local_account * account );
static void icq_info_data_cleanup(info_window * iw);
static void icq_info_update(eb_account *sender);


/*********
 * the following variable is a hack, it if we are changing the selection
 * don't let the corresponding set_current_state get called again
 */

static char * eb_icq_check_login(const char * user, const char * pass)
{
	return NULL;
}

static eb_local_account * icq_find_local_account_by_conn(toc_conn * conn)
{
	LList * node;
	for( node = accounts; node; node = node->next )
	{
		eb_local_account * ela = (eb_local_account *)node->data;
		if(ela->service_id == SERVICE_INFO.protocol_id) 
		{
			struct eb_icq_local_account_data * alad = (struct eb_icq_local_account_data *)ela->protocol_local_account_data;
		    if( alad->conn == conn )
			{
				return ela;
			}
		}
	}
	return NULL;
}

/* Unused
static void icq_set_profile_callback(const char * value, void * data)
{
	eb_local_account * account = (eb_local_account*)data;
	struct eb_icq_local_account_data * alad 
		= (struct eb_icq_local_account_data *)account->protocol_local_account_data;

	strncpy(alad->icq_info, value, MAX_PREF_LEN);
	write_account_list();
}


static void icq_set_profile_window(ebmCallbackData *data)
{
	eb_local_account * account = NULL;
	struct eb_icq_local_account_data * alad;

	char buff[256];
	if(IS_ebmProfileData(data))
		account = (eb_local_account*)data->user_data;
	else 
	{*/	/* This should never happen, unless something is horribly wrong */
/*		fprintf(stderr, "data->CDType %d\n", data->CDType);
		fprintf(stderr, "Error! not of profile type!\n");
		return;
	}
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;

	g_snprintf(buff, 256, _("Profile for account %s"), account->handle); 
	do_text_input_window(buff, alad->icq_info, icq_set_profile_callback, account); 
}
*/

static void eb_icq_disconnect( toc_conn * conn )
{
	eb_local_account * ela = conn->account;
	eb_debug(DBG_TOC, "eb_icq_disconnect %d %d\n", conn->fd, conn->seq_num);
	if(ela) {
		eb_icq_logout(ela);
	}
	else
		g_warning("NULL account associated with icq connection");
}

static void eb_icq_join_ack(toc_conn * conn, char * id, char * name)
{
	eb_chat_room * ecr = find_chat_room_by_name(name, SERVICE_INFO.protocol_id);
	eb_local_account * ela = conn->account;
	
	eb_debug(DBG_TOC, "eb_icq_join_ack %s %s\n", id, name );

	if(!ecr) {
		ecr = eb_icq_make_chat_room(name, ela, 0);
	}
	eb_debug(DBG_TOC, "Match found, copying id!!");

	strncpy( ecr->id, id , sizeof(ecr->id));

	eb_join_chat_room(ecr, TRUE);
}

static void eb_icq_join_error(toc_conn * conn, char * name)
{
	eb_chat_room * ecr = find_chat_room_by_name(name , SERVICE_INFO.protocol_id);

	eb_debug(DBG_TOC, "eb_icq_join_err %s\n", name );

	if(!ecr)
		return;
	
	eb_destroy_chat_room(ecr);
}

static void eb_icq_chat_update_buddy(toc_conn * conn, char * id, 
		                      char * user, int online )
{
	eb_chat_room * ecr = find_chat_room_by_id(id);
	if(!ecr)
	{
			fprintf(stderr, "Error: unable to fine the chat room!!!\n" );
	}
	if(online)
	{
		eb_account * ea = find_account_with_ela(user, icq_find_local_account_by_conn(conn));
		if( ea)
		{
			eb_chat_room_buddy_arrive(ecr, ea->account_contact->nick, user );
		}
		else
		{
			eb_chat_room_buddy_arrive(ecr, user, user);
		}
	}
	else
	{
		eb_chat_room_buddy_leave(ecr, user);
	}
}


/*the callback to call all callbacks :P */

static void eb_icq_callback(void *data, int source, eb_input_condition condition )
{
	struct eb_icq_local_account_data * alad = data;
	toc_conn * conn = alad->conn;
	eb_debug(DBG_TOC, "eb_icq_callback %d %d\n", conn->fd, conn->seq_num);
	if(source < 0 )
	{
		//eb_input_remove(*((int*)data));
		g_assert(0);
	}
	icqtoc_callback(((struct eb_icq_local_account_data *)data)->conn);

}

static int eb_icq_keep_alive(gpointer data )
{
	struct eb_icq_local_account_data * alad = data;
	toc_conn * conn = alad->conn;
	eb_debug(DBG_TOC, "eb_icq_keep_alive %d %d\n", conn->fd, conn->seq_num);
	icqtoc_send_keep_alive(alad->conn);
	return TRUE;
}

static void eb_icq_process_file_request( gpointer data, int result )
{
	int accepted = result;
	eb_icq_file_request * eafr = data;

	if(accepted)
	{
		icqtoc_file_accept( eafr->conn, eafr->nick, eafr->ip, eafr->port,
						 eafr->cookie, eafr->filename );
		g_free(eafr);
	}
	else
	{
		icqtoc_file_cancel( eafr->conn, eafr->nick, eafr->cookie );
		g_free(eafr);
	}
}

static void eb_icq_file_offer(toc_conn * conn, char * nick, char * ip, short port,
							  char * cookie, char * filename )
{
	eb_icq_file_request * eafr = g_new0( eb_icq_file_request, 1);
	char message[1024];

	eafr->conn = conn;
	strncpy( eafr->nick, nick, 255);
	strncpy( eafr->ip,  ip, 255);
	eafr->port = port;
	strncpy( eafr->filename, filename, 255);
	strncpy( eafr->cookie, cookie, 255 );

	g_snprintf( message, 1024, _("icq user %s would like to\nsend you the file\n%s\ndo you want to accept?"), nick, filename );

	eb_do_dialog( message, _("Incoming icq File Request"), eb_icq_process_file_request, eafr );

}


static void eb_icq_chat_invite(toc_conn * conn, char * id, char * name,
		                char * sender, char * message )
{
	eb_chat_room * chat_room = g_new0(eb_chat_room, 1);
  	eb_local_account * ela = icq_find_local_account_by_conn(conn);
	strncpy(chat_room->id, id, sizeof(chat_room->id));
	strncpy(chat_room->room_name, name, sizeof(chat_room->room_name));
	chat_room->connected = FALSE;
	chat_room->fellows = NULL;
	chat_room->protocol_local_chat_room_data = NULL; /* not needed for icq */
	
	chat_room->local_user =  icq_find_local_account_by_conn(conn);

	invite_dialog( ela, sender, name, strdup(id) );

}


static void eb_icq_error_message( char * message )
{
	ay_do_error( _("ICQ Error"), message );
}
	
static void eb_icq_oncoming_buddy(toc_conn *conn, char * user, int online, time_t idle, int evil, int unavailable )
{
	eb_account * ea = find_account_with_ela( user, icq_find_local_account_by_conn(conn));
	struct eb_icq_account_data * aad ;
	struct eb_icq_local_account_data *alad;
	
	if(!ea)
		return;

       	alad = ea->ela?(struct eb_icq_local_account_data *)ea->ela->protocol_local_account_data:NULL;
	
	aad = ea->protocol_account_data;
	if (alad && !l_list_find(alad->icq_buddies, ea->handle))
		alad->icq_buddies = l_list_append(alad->icq_buddies, ea->handle);

	if (online && (aad->status == ICQ_OFFLINE))
	{
		aad->status = ICQ_ONLINE;
		buddy_login(ea);
	}
	else if(!online && (aad->status != ICQ_OFFLINE))
	{
		aad->status = ICQ_OFFLINE;
		buddy_logoff(ea);
	}

	if (online && unavailable)
		aad->status = ICQ_AWAY;
	else if (online)
		aad->status = ICQ_ONLINE;

	aad->evil = evil;
	aad->idle_time = idle;
	buddy_update_status(ea);
}

/* This is how we deal with incomming chat room messages */

static void eb_icq_toc_chat_im_in( toc_conn * conn, char * id, char * user, char * message )
{
	eb_chat_room * ecr = find_chat_room_by_id( id );
	eb_account * ea = find_account_with_ela(user, icq_find_local_account_by_conn(conn));
	char * message2 = linkify(message);

	if(!ecr)
	{
		g_warning("Chat room does not Exist!!!");
		g_free(message2);
		return;
	}

	if( ea)
	{
		eb_chat_room_show_message( ecr, ea->account_contact->nick, message2 );
	}
	else
	{
		eb_chat_room_show_message( ecr, user, message2 );
	}
	g_free(message2);
}

static void eb_icq_user_info(toc_conn * conn, char * user, char * message )
{
	eb_local_account * ela =  icq_find_local_account_by_conn(conn);
	eb_account * sender = NULL;
	eb_local_account * reciever = NULL;


	sender = find_account_with_ela(user, ela);
	if(sender==NULL)
	{
		eb_account * ea = g_new0(eb_account, 1);
		struct eb_icq_account_data * aad = g_new0(struct eb_icq_account_data, 1);
		strncpy(ea->handle, user, 255);
		ea->service_id = ela->service_id;
		aad->status = ICQ_OFFLINE;
		ea->protocol_account_data = aad;
		ea->ela = ela;
		add_unknown(ea);
		sender = ea;

	}
	reciever = find_suitable_local_account( ela, ela->service_id );

	if(sender->infowindow == NULL )
	{
		sender->infowindow = eb_info_window_new(reciever, sender);
	}

	sender->infowindow->info_data = strdup(message);
	sender->infowindow->cleanup = icq_info_data_cleanup;
	icq_info_update(sender);
}

static void eb_icq_new_group(char * group)
{
	eb_debug(DBG_TOC, "===> %p %x\n", find_grouplist_by_name(group),
			group_mgmt_check_moved(group));
	if(!find_grouplist_by_name(group) && !group_mgmt_check_moved(group)) {
		add_group(group);
	}
}

static void eb_icq_new_user(toc_conn *conn, char * group, char * f_handle)
{
	eb_account * ea = NULL;
  	eb_local_account * ela = NULL;
	struct eb_icq_local_account_data * alad = NULL;
	char *handle = strdup(f_handle);
	char *fname = NULL;
	
	if (conn)
		ela = icq_find_local_account_by_conn(conn);
	if (ela)
		alad = ela->protocol_local_account_data;
	
	if (strchr(handle,':')) {
		fname = strchr(handle,':') + 1;
		*(strchr(handle,':')) = '\0';
	}
	else
		fname = handle;
		
	ea = find_account_with_ela( handle, ela );
	
	if(!ea)
	{
		grouplist * gl = find_grouplist_by_name(group);
		struct contact * c = find_contact_by_nick(fname);
		ea = eb_icq_new_account(ela, handle);
	

		if(!gl && !c)
		{
			add_group(group);
		}
		if(!c)
		{
			c = add_new_contact(group, fname, SERVICE_INFO.protocol_id);
		}

		ea->list_item = NULL;
		ea->online = 0;
		ea->status = NULL;
		ea->pix = NULL;
		ea->icon_handler = -1;
		ea->status_handler = -1;
	
		if (alad)
			alad->icq_buddies = l_list_append(alad->icq_buddies, handle);
		c->accounts = l_list_append(c->accounts, ea);
		ea->account_contact = c;

		update_contact_list ();
		write_contact_list();
	}
	
	free(handle);
}
		


	

static void eb_icq_parse_incoming_im(toc_conn * conn, char * user, char * message )
{
	    //time_t  t = 0;
  		eb_local_account * ela = icq_find_local_account_by_conn(conn);
		struct eb_icq_local_account_data * alad = ela->protocol_local_account_data;
		
		eb_account * sender = NULL;
		eb_local_account * reciever = NULL;

		eb_debug(DBG_TOC, "eb_icq_parse_incomming_im %d %d, %d %d\n", conn->fd, conn->seq_num, alad->conn->fd, alad->conn->seq_num );

		sender = find_account_with_ela(user, ela);
		if(sender==NULL)
		{
			eb_account * ea = g_new0(eb_account, 1);
			struct eb_icq_account_data * aad = g_new0(struct eb_icq_account_data, 1);
			strncpy(ea->handle, user, 255);
			ea->service_id = ela->service_id;
			aad->status = ICQ_OFFLINE;
			ea->protocol_account_data = aad;
			ea->ela = ela;
			add_unknown(ea);
			sender = ea;

			eb_debug(DBG_TOC, "Sender == NULL");
		}
		
		if (sender && !sender->online) {
			/* that's impossible if he talks to us */
			icqtoc_add_buddy(conn,sender->handle,
					sender->account_contact->group->name);
		}
		
		reciever = find_suitable_local_account( ela, ela->service_id);
		//strip_html(msg);

		eb_parse_incoming_message(reciever, sender, message);
		if(reciever == NULL)
		{
			g_warning("Reciever == NULL");
		}

        eb_debug(DBG_TOC, "%s %s\n", user, message);
		return;
}



/*   callbacks used by EveryBuddy    */
static int eb_icq_query_connected(eb_account * account)
{		
	struct eb_icq_account_data * aad = account->protocol_account_data;

	if(ref_count <= 0 )
		aad->status = ICQ_OFFLINE;
	return aad->status != ICQ_OFFLINE;
}

static void eb_icq_accept_invite( eb_local_account * account, void * invitation )
{
	char * id = invitation;

	struct eb_icq_local_account_data * alad = 
						account->protocol_local_account_data;
	
	toc_conn * conn = alad->conn;

	icqtoc_chat_accept(conn, id);
	free(id);
}

static void eb_icq_decline_invite( eb_local_account * account, void * invitation )
{
	char * id = invitation;
	free( id );
}


static void eb_icq_send_chat_room_message( eb_chat_room * room, char * message )
{
	struct eb_icq_local_account_data * alad = room->local_user->protocol_local_account_data;
	toc_conn * conn = alad->conn;
	char * message2 = strdup(message);

	icqtoc_chat_send(conn, room->id, message2 );
	g_free(message2);
}

static void eb_icq_join_chat_room( eb_chat_room * room )
{
	struct eb_icq_local_account_data * alad = room->local_user->protocol_local_account_data;
	toc_conn * conn = alad->conn;
	icqtoc_chat_join(conn, room->room_name);
}

static void eb_icq_leave_chat_room( eb_chat_room * room )
{
	struct eb_icq_local_account_data * alad = room->local_user->protocol_local_account_data;
	toc_conn * conn = alad->conn;
	icqtoc_chat_leave(conn, room->id);
}

static eb_chat_room * eb_icq_make_chat_room(char * name, eb_local_account * account, int is_public )
{
	eb_chat_room * ecr = g_new0(eb_chat_room, 1);

	strncpy( ecr->room_name, name , sizeof(ecr->room_name));
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;
	eb_join_chat_room(ecr, TRUE);
	

	return ecr;
}

static eb_account * eb_icq_new_account(eb_local_account *ela, const char * account )
{
	eb_account * a = g_new0(eb_account, 1);
	struct eb_icq_account_data * aad = g_new0(struct eb_icq_account_data, 1);

	a->protocol_account_data = aad;
	strncpy(a->handle, account, 255);
	a->service_id = SERVICE_INFO.protocol_id;
	a->ela = ela;
	aad->status = ICQ_OFFLINE;

	return a;
}

static void eb_icq_del_user( eb_account * account )
{
	LList * node;
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
	for( node = accounts; node; node=node->next )
	{
		eb_local_account * ela = node->data;
		if( ela->connected && ela->service_id == account->service_id)
		{
			struct eb_icq_local_account_data * alad = ela->protocol_local_account_data;
			icqtoc_remove_buddy(alad->conn,account->handle,
					account->account_contact->group->name);
		}
	}
}

static void eb_icq_add_user( eb_account * account )
{
	LList * node;

	struct eb_icq_local_account_data *alad = account->ela?(struct eb_icq_local_account_data *)account->ela->protocol_local_account_data:NULL;
	if (!alad) return;
	
 	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
	alad->icq_buddies = l_list_append(alad->icq_buddies, account->handle);

	for( node = accounts; node; node=node->next )
	{
		eb_local_account * ela = node->data;
		if( ela && ela->connected && ela->service_id == account->service_id)
		{
			struct eb_icq_local_account_data * alad = ela->protocol_local_account_data;
			icqtoc_add_buddy(alad->conn,account->handle,
					account->account_contact->group->name);
		}
		
	}
}

static void ay_icq_cancel_connect(void *data)
{
	eb_local_account *ela = (eb_local_account *)data;
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)ela->protocol_local_account_data;
	
	ay_socket_cancel_async(alad->connect_tag);
	alad->activity_tag=0;
	eb_icq_logout(ela);
}

static void eb_icq_finish_login(const char *password, void *data)
{
	struct eb_icq_local_account_data * alad;
	char buff[1024];
	int port = atoi(icq_port);
	eb_local_account *account = (eb_local_account *)data;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;
	
	snprintf(buff, sizeof(buff), _("Logging in to ICQ account: %s"), account->handle);
 	alad->activity_tag = ay_activity_bar_add(buff, ay_icq_cancel_connect, account);

	if (should_fallback) {
		port = icq_fallback_ports[icq_last_fallback];
		icq_last_fallback++;
		should_fallback = 0;
	}

	alad->connect_tag = icqtoc_signon( account->handle, password,
			      icq_server, atoi(icq_port), alad->icq_info);
}

static void eb_icq_login( eb_local_account * account )
{
	struct eb_icq_local_account_data * alad;
	char buff[1024];
	
	if (account->connecting || account->connected)
		return;
	
	account->connecting = 1;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;

	if (alad->prompt_password || !alad->password || !strlen(alad->password)) {
		snprintf(buff, sizeof(buff), _("ICQ password for: %s"), account->handle);
		do_password_input_window(buff, "", 
				eb_icq_finish_login, account);
	} else {
		eb_icq_finish_login(alad->password, account);
	}

}

static void eb_icq_logged_in (toc_conn *conn) 
{
 	struct eb_icq_local_account_data * alad;
 	eb_local_account *ela = NULL;
	
	if (!conn)
		return;
	
	ela = find_local_account_by_handle(conn->username, SERVICE_INFO.protocol_id);
 	alad = (struct eb_icq_local_account_data *)ela->protocol_local_account_data;
 	alad->conn = conn;
	
 	ay_activity_bar_remove(alad->activity_tag);
	alad->activity_tag = 0;
	 	
	if(alad->conn->fd == -1 )
	{
		g_warning("eb_icq UNKNOWN CONNECTION PROBLEM");
		if(icq_fallback_ports[icq_last_fallback] != 0) {
			should_fallback=1;
			eb_icq_login(ela);
		} else {
			ay_do_error( _("ICQ Error"), _("Cannot connect to ICQ due to network problem.") );
			should_fallback=0;
			icq_last_fallback=0;
		}
		return;
	}
	eb_debug(DBG_TOC, "eb_icq_login %d %d\n", alad->conn->fd, alad->conn->seq_num );
	alad->conn->account = ela;
	alad->status = ICQ_ONLINE;
	ref_count++;
	alad->input = eb_input_add(alad->conn->fd, EB_INPUT_READ, eb_icq_callback, alad);
		
	alad->keep_alive = eb_timeout_add((guint32)60000, eb_icq_keep_alive, (gpointer)alad );

	alad->is_setting_state = 1;
	
	if(ela->status_menu)
		eb_set_active_menu_status(ela->status_menu, ICQ_ONLINE);

	alad->is_setting_state = 0;
	ela->connecting = 0;
	ela->connected = 1;
	
	icqtoc_add_buddy(alad->conn,ela->handle,
			"Unknown");
	alad->icq_buddies = l_list_append(alad->icq_buddies, ela->handle);
								  
}

static void eb_icq_send_invite( eb_local_account * account, eb_chat_room * room,
						 char * user, const char * message)
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;
	icqtoc_invite( alad->conn, room->id, user, message );
}
	

static void eb_icq_logout( eb_local_account * account )
{
	LList *l;
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;
	eb_input_remove(alad->input);
	eb_timeout_remove(alad->keep_alive);
	alad->connect_tag = 0;
	if(alad->conn)
	{
		eb_debug(DBG_TOC, "eb_icq_logout %d %d\n", alad->conn->fd, alad->conn->seq_num );
		icqtoc_signoff(alad->conn);
		if (ref_count >0)
			ref_count--;
	}
#if 0
	if(account->status_menu)
		eb_set_active_menu_status(account->status_menu, ICQ_ONLINE);
#endif
	alad->status=ICQ_OFFLINE;
	
	account->connected = account->connecting = 0;

	alad->is_setting_state = 1;

	if(account->status_menu)
		eb_set_active_menu_status(account->status_menu, ICQ_OFFLINE);

	alad->is_setting_state = 0;
	
	/* Make sure each icq buddy gets logged off from the status window */
	for(l = alad->icq_buddies; l && alad->conn; l=l->next)
	{
		eb_icq_oncoming_buddy(alad->conn, l->data, FALSE, 0, 0, FALSE);
	}
	
	if (alad->conn) {
		g_free(alad->conn);
		alad->conn = NULL;
	}		
}

static void ay_toc_connect_status(const char *msg, void *data)
{
  toc_conn *conn = (toc_conn *)data;
  char *handle = conn->username;
  eb_local_account *ela = NULL;
  
  if (!handle) return;
  
  ela = find_local_account_by_handle(handle, SERVICE_INFO.protocol_id);
  if (!ela) { 
	  return;
  } else {
	  struct eb_icq_local_account_data *alad =
			(struct eb_icq_local_account_data *)ela->protocol_local_account_data;
	  if (!alad) return;
	  ay_activity_bar_update_label(alad->activity_tag, msg);
  }
}


static int eb_icq_async_socket(const char *host, int port, void *cb, void *data)
{
  int tag = proxy_connect_host(host, port, cb, data, (void *)ay_toc_connect_status);
  
  return tag;
}

static void eb_icq_send_im( eb_local_account * account_from,
				  eb_account * account_to,
				  char * message )
{
	struct eb_icq_local_account_data * plad = (struct eb_icq_local_account_data*)account_from->protocol_local_account_data;
	char * message2 = strdup(message);
	if(strlen(message2) > 2000)
	{
		ay_do_warning( _("ICQ Warning"), _("Message Truncated") );
		message2[2000] = '\0';
	}
	icqtoc_send_im(plad->conn,account_to->handle, message2);
	eb_debug(DBG_TOC, "eb_icq_send_im %d %d\n", plad->conn->fd, plad->conn->seq_num);
	eb_debug(DBG_TOC, "eb_icq_send_im %s", message);

	g_free(message2);
}
		
static void icq_init_account_prefs(eb_local_account * ela)
{
	struct eb_icq_local_account_data *alad = ela->protocol_local_account_data;
	input_list *il = g_new0(input_list, 1);

	ela->prefs = il;

	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label= _("ICQ _UIN:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = alad->password;
	il->name = "PASSWORD";
	il->label= _("_Password:");
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &alad->prompt_password;
	il->name = "prompt_password";
	il->label= _("_Ask for password at Login time");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ela->connect_at_startup;
	il->name = "CONNECT";
	il->label= _("_Connect at startup");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = alad->icq_info;
	il->name = "PROFILE";
	il->label= _("P_rofile:");
	il->type = EB_INPUT_ENTRY;

}

static eb_local_account * eb_icq_read_local_config(LList * pairs)
{

	eb_local_account * ela = g_new0(eb_local_account, 1);
	struct eb_icq_local_account_data * ala = g_new0(struct eb_icq_local_account_data, 1);
	
	eb_debug(DBG_TOC, "eb_icq_read_local_config: entering\n");	

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = ala;
	ala->status = ICQ_OFFLINE;
	icq_init_account_prefs(ela);

	eb_update_from_value_pair(ela->prefs, pairs);

	eb_debug(DBG_TOC, "eb_icq_read_local_config: returning %p\n", ela);

	return ela;
}

static LList * eb_icq_write_local_config( eb_local_account * account )
{
	return eb_input_to_value_pair( account->prefs );
}


static eb_account * eb_icq_read_config( eb_account *ea, LList * config )
{
	struct eb_icq_account_data * aad =  g_new0(struct eb_icq_account_data,1);
	
	aad->status = ICQ_OFFLINE;
	ea->protocol_account_data = aad;
	eb_icq_add_user(ea);

	return ea;
}

static LList * eb_icq_get_states()
{
	LList * states = NULL;
	states = l_list_append(states, "Online");
	states = l_list_append(states, "Away");
	states = l_list_append(states, "Offline");
	
	return states;
}

static int eb_icq_get_current_state(eb_local_account * account )
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;
	eb_debug(DBG_TOC, "eb_icq_get_current_state: %i %i\n", eb_services[account->service_id].protocol_id, SERVICE_INFO.protocol_id);	
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );

	return alad->status;
}

static void eb_icq_set_current_state( eb_local_account * account, int state )
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;

	/* stop the recursion */
	if( alad->is_setting_state )
		return;

	eb_debug(DBG_TOC, "eb_set_current_state %d\n", state );
//	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
	if(account == NULL || account->protocol_local_account_data == NULL )
	{
		g_warning("ACCOUNT state == NULL!!!!!!!!!!!!!!!!!!!!!");
	}

	switch(state) {
	case ICQ_ONLINE:
		if (account->connected == 0 && account->connecting == 0) {
			eb_icq_login(account);
		}
		icqtoc_set_away(alad->conn, NULL);
		break;
	case ICQ_AWAY:
		if (account->connected == 0 && account->connecting == 0) {
			eb_icq_login(account);
		}
		if (is_away) {
			char *awaymsg = get_away_message();
			icqtoc_set_away(alad->conn, awaymsg);
			g_free(awaymsg);
		} else
			icqtoc_set_away(alad->conn, _("User is currently away"));
		break;
	case ICQ_OFFLINE:
		if (account->connected == 1) {
			eb_icq_logout(account);
		}
		break;
	}
	alad->status = state;

}

static void eb_icq_set_away(eb_local_account * account, char * message, int away)
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)account->protocol_local_account_data;

	if (message) {
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, ICQ_AWAY);
		icqtoc_set_away(alad->conn, message);
	} else {
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, ICQ_ONLINE);
	}
}

static char **eb_icq_get_status_pixmap( eb_account * account)
{
	struct eb_icq_account_data * aad;
	
	aad = account->protocol_account_data;

	if (aad->status == ICQ_ONLINE)
		return icq_online_xpm;
	else
		return icq_away_xpm;
	
}

static char * eb_icq_get_status_string( eb_account * account )
{
	static char string[255], buf[255];
	struct eb_icq_account_data * aad = account->protocol_account_data;
	strcpy(buf, "");
	strcpy(string, "");
	if(aad->idle_time)
	{
		int hours, minutes, days;
		minutes = (time(NULL) - aad->idle_time)/60;
		hours = minutes/60;
		minutes = minutes%60;
		days = hours/24;
		hours = hours%24;
		if( days )
		{
			g_snprintf( buf, 255, " %d:%02d:%02d", days, hours, minutes );
		}
		else if(hours)
		{
			g_snprintf( buf, 255, " %d:%02d", hours, minutes);
		}
		else
		{
			g_snprintf( buf, 255, " %d", minutes); 
		}
	}

	if (aad->evil)
		g_snprintf(string, 255, "[%d%%]%s", aad->evil, buf);
	else
		g_snprintf(string, 255, "%s", buf);

	if (!account->online)
		g_snprintf(string, 255, "Offline");		

	return string;
}

static void eb_icq_set_idle( eb_local_account * ela, int idle )
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)ela->protocol_local_account_data;
	eb_debug(DBG_TOC, "eb_icq_set_idle %d %d\n", alad->conn->fd, alad->conn->seq_num );
	icqtoc_set_idle( alad->conn, idle );
}

static void eb_icq_real_change_group(eb_account * ea, const char *old_group, const char *new_group)
{
	char str[2048];
	struct eb_icq_local_account_data * alad = NULL;

	
	g_snprintf(str, 2048, "g %s\nb %s", new_group, ea->handle);

	if( eb_services[ea->service_id].protocol_id != SERVICE_INFO.protocol_id )
		return;

	if( ea->ela && ea->ela->connected && ea->ela->service_id == ea->service_id)
	{
		alad = ea->ela->protocol_local_account_data;
		icqtoc_remove_buddy(alad->conn, ea->handle, (char *)old_group);
		icqtoc_add_buddy(alad->conn, ea->handle, (char *)new_group);
	}
}

static void eb_icq_change_group(eb_account * ea, const char *new_group)
{
	eb_icq_real_change_group(ea, ea->account_contact->group->name, new_group);
}

static void eb_icq_del_group(eb_local_account *ela, const char *group)
{
	struct eb_icq_local_account_data * alad;

	if( ela && ela->connected && ela->service_id == SERVICE_INFO.protocol_id)
	{
		alad = ela->protocol_local_account_data;
		icqtoc_remove_group(alad->conn, group);
	}
}
	
static void eb_icq_add_group(eb_local_account *ela, const char *group)
{
	struct eb_icq_local_account_data * alad;
	
	if( ela && ela->connected && ela->service_id == SERVICE_INFO.protocol_id)
	{
		alad = ela->protocol_local_account_data;
		icqtoc_add_group(alad->conn, group);
	}
}

static void eb_icq_rename_group(eb_local_account *ela, const char *old_group, const char *new_group)
{
	LList *l;
	
	struct eb_icq_local_account_data *alad = (struct eb_icq_local_account_data *)ela->protocol_local_account_data;	
	for(l = alad->icq_buddies; l; l=l->next)
	{
		eb_account *ea = find_account_with_ela(l->data, ela);
		if (ea) 
			eb_debug(DBG_TOC, "checking if we should move %s from %s\n",ea->handle, ea->account_contact->group->name);
		if (ea && !strcmp(ea->account_contact->group->name, new_group)) {
			eb_debug(DBG_TOC, "Moving %s from %s to %s\n",ea->handle, old_group, new_group);
			eb_icq_real_change_group(ea, old_group, new_group);
		}
	}
}

static void eb_icq_get_info( eb_local_account * from, eb_account * account_to )
{
	struct eb_icq_local_account_data * alad;
	alad = (struct eb_icq_local_account_data *)from->protocol_local_account_data;

	icqtoc_get_info( alad->conn, account_to->handle );
}

static void icq_info_update(eb_account *sender)
{
	info_window *iw = sender->infowindow;
	char * data = (char *)iw->info_data;
	eb_info_window_clear(iw);
	eb_info_window_add_info(sender, data,1,1,0);
}

static void icq_info_data_cleanup(info_window * iw)
{
}

/*	There are no prefs for icq-TOC at the moment.

static input_list * eb_icq_get_prefs()
{
	return icq_prefs;
}
*/

static void eb_icq_read_prefs_config(LList * values)
{
	char * c;
	c = value_pair_get_value(values, "server");
	if(c)
	{
		strncpy(icq_server, c, sizeof(icq_server));
		free( c );
	}
	c = value_pair_get_value(values, "port");
	if(c)
	{
		strncpy(icq_port, c, sizeof(icq_port));
		free( c );
	}
	c = value_pair_get_value(values, "do_icq_debug");
	if(c)
	{
		do_icq_debug = atoi(c);
		free( c );
	}
}

static LList * eb_icq_write_prefs_config()
{
	LList * config = NULL;
	char buffer[5];

	config = value_pair_add(config, "server", icq_server);
	config = value_pair_add(config, "port", icq_port);
	snprintf(buffer, 5, "%d", do_icq_debug);
	config = value_pair_add(config, "do_icq_debug", buffer);

	return config;
}

static int eb_icq_progress_window_new (const char *fname, unsigned long size)
{
	char label[1024];
	snprintf(label,1024,"Transferring %s...", fname);
	return ay_progress_bar_add(label, size, NULL, NULL);	
}

static void eb_icq_update_progress(int tag, unsigned long progress)
{
	ay_progress_bar_update_progress(tag, progress);
}

static void eb_icq_progress_window_close(int tag)
{
	ay_activity_bar_remove(tag);
}

struct service_callbacks * query_callbacks()
{
	struct service_callbacks * sc;
	
	icqtoc_im_in = eb_icq_parse_incoming_im;
	icqupdate_user_status = eb_icq_oncoming_buddy; 
	icqtoc_error_message = eb_icq_error_message;
	icqtoc_disconnect = eb_icq_disconnect;
	icqtoc_chat_im_in = eb_icq_toc_chat_im_in;
	icqtoc_chat_invite = eb_icq_chat_invite;
	icqtoc_join_ack = eb_icq_join_ack;
	icqtoc_join_error = eb_icq_join_error;
	icqtoc_chat_update_buddy = eb_icq_chat_update_buddy;
	icqtoc_begin_file_recieve = eb_icq_progress_window_new;
	icqtoc_update_file_status = eb_icq_update_progress;
	icqtoc_complete_file_recieve = eb_icq_progress_window_close;
	icqtoc_file_offer = eb_icq_file_offer;
	icqtoc_user_info = eb_icq_user_info;
	icqtoc_new_user = eb_icq_new_user;
	icqtoc_new_group = eb_icq_new_group;
	icqtoc_logged_in = eb_icq_logged_in;
	icqtoc_async_socket = eb_icq_async_socket;
	
	sc = g_new0( struct service_callbacks, 1 );
	sc->query_connected = eb_icq_query_connected;
	sc->login = eb_icq_login;
	sc->logout = eb_icq_logout;
	sc->send_im = eb_icq_send_im;
	sc->read_local_account_config = eb_icq_read_local_config;
	sc->write_local_config = eb_icq_write_local_config;
	sc->read_account_config = eb_icq_read_config;
	sc->get_states = eb_icq_get_states;
	sc->get_current_state = eb_icq_get_current_state;
	sc->set_current_state = eb_icq_set_current_state;
	sc->check_login = eb_icq_check_login;
	sc->add_user = eb_icq_add_user;
	sc->del_user = eb_icq_del_user;
	sc->new_account = eb_icq_new_account;
	sc->get_status_string = eb_icq_get_status_string;
	sc->get_status_pixmap = eb_icq_get_status_pixmap;
	sc->set_idle = eb_icq_set_idle;
	sc->set_away = eb_icq_set_away;
	sc->send_chat_room_message = eb_icq_send_chat_room_message;
	sc->join_chat_room = eb_icq_join_chat_room;
	sc->leave_chat_room = eb_icq_leave_chat_room;
	sc->make_chat_room = eb_icq_make_chat_room;
	sc->send_invite = eb_icq_send_invite;
	sc->accept_invite = eb_icq_accept_invite;
	sc->decline_invite = eb_icq_decline_invite;
	sc->get_info = eb_icq_get_info;

	sc->get_prefs = NULL;
	sc->read_prefs_config = eb_icq_read_prefs_config;
	sc->write_prefs_config = eb_icq_write_prefs_config;

	sc->get_color = eb_toc_get_color;
	sc->get_smileys = eb_default_smileys;
	
	sc->change_group = eb_icq_change_group;
	sc->add_group = eb_icq_add_group;
	sc->del_group = eb_icq_del_group;
	sc->rename_group = eb_icq_rename_group;
	return sc;
}
