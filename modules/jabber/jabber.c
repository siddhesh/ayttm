/*
 * Jabber Extension for yattm
 *
 * Copyright (C) 2000, Alex Wheeler <awheeler@speakeasy.net>
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
 * jabber.c
 */

#define DEBUG

#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif

#include "intl.h"

#undef MIN
#undef MAX
#if defined( _WIN32 ) && !defined(__MINGW32__)
#include "../libEBjabber.h"
typedef unsigned long u_long;
typedef unsigned long ulong;
#else
#include "libEBjabber.h"
#endif

#include "service.h"
#include "info_window.h"
#include "util.h"
#include "status.h"
#include "message_parse.h"
#include "value_pair.h"
#include "plugin_api.h"
#include "smileys.h"
#include "gtk_globals.h"
#include "activity_bar.h"
#include "tcp_util.h"
#include "messages.h"
#include "dialog.h"

#include "pixmaps/jabber_online.xpm"
#include "pixmaps/jabber_away.xpm"

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info jabber_LTX_plugin_info
#define SERVICE_INFO jabber_LTX_SERVICE_INFO
#define plugin_init jabber_LTX_plugin_init
#define plugin_finish jabber_LTX_plugin_finish
#define module_version jabber_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();

static int ref_count = 0;
static char jabber_server[MAX_PREF_LEN] = "jabber.com";
static char jabber_port[MAX_PREF_LEN] = "5222";
int do_jabber_debug=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE, 
	"Jabber Service", 
	"Jabber Messenger support", 
	"$Revision: 1.26 $",
	"$Date: 2003/04/30 09:43:53 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
struct service SERVICE_INFO = { "Jabber", -1, 
	SERVICE_CAN_OFFLINEMSG | SERVICE_CAN_GROUPCHAT | SERVICE_CAN_ICONVERT, NULL };
/* End Module Exports */

static char *eb_jabber_get_color(void) { static char color[]="#88aa00"; return color; }

unsigned int module_version() {return CORE_VERSION;}

static int plugin_init()
{
	input_list * il = g_new0(input_list, 1);
	eb_debug(DBG_MOD, "Jabber\n");
	ref_count=0;
		plugin_info.prefs = il;
		il->widget.entry.value = jabber_server;
		il->widget.entry.name = "jabber_server";
		il->widget.entry.label = _("Default Server:");
		il->type = EB_INPUT_ENTRY;

		il->next = g_new0(input_list, 1);
		il = il->next;
		il->widget.entry.value = jabber_port;
		il->widget.entry.name = "jabber_port";
		il->widget.entry.label = _("Port:");
		il->type = EB_INPUT_ENTRY;

		il->next = g_new0(input_list, 1);
                il = il->next;
                il->widget.checkbox.value = &do_jabber_debug;
                il->widget.checkbox.name = "do_jabber_debug";
                il->widget.checkbox.label = _("Enable debugging");
                il->type = EB_INPUT_CHECKBOX;
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

static LList *jabber_contacts = NULL;

static char * jabber_status_strings[] =
{ "", "Away", "Do Not Disturb", "Extended Away", "Chat", "Offline"};


/*	There are no prefs for Jabber at the moment.
static input_list * jabber_prefs = NULL;
*/

/* Use this struct to hold any service specific information you need
 * about people on your contact list
 */

typedef struct _eb_jabber_account_data
{
	int status;		//the status of the user
	JABBER_Conn *JConn;	//The connection we know about them from
} eb_jabber_account_data;

typedef struct _jabber_info_data
{
    char *profile;
} jabber_info_data;


/* Use this struct to hold any service specific information you need about
 * local accounts
 * below are just some suggested values
 */

typedef struct _eb_jabber_local_account_data
{
	char password[255]; // account password
	int fd;				// the file descriptor
	int status;			// the current status of the user
	JABBER_Conn	*JConn;
	int activity_tag;
	int connect_tag;
} eb_jabber_local_account_data;

static eb_local_account *jabber_local_account;
static void eb_jabber_terminate_chat( eb_account * account );
static void eb_jabber_add_user( eb_account * account );
static void eb_jabber_del_user( eb_account * account );
static void eb_jabber_login( eb_local_account * account );
static void eb_jabber_logout( eb_local_account * account );
static void jabber_info_update(eb_account *account); 
static void jabber_info_data_cleanup(info_window *iw);
static int is_setting_state = 0;

static void jabber_dialog_callback( gpointer data, int result );
static void jabber_list_dialog_callback( char * text, gpointer data );
void JABBERInstantMessage(void *data);
void JABBERStatusChange(void *data);
void JABBERDialog(void *data);
void JABBERListDialog(char **list, void *data);
void JABBERLogout(void *data);
void JABBERError( char *message, char *title );

static void jabber_dialog_callback( gpointer data, int response )
{
    JABBER_Dialog_PTR jd;

    jd = (JABBER_Dialog_PTR)data;
    eb_debug(DBG_JBR, "**** response: %i\n", response);
    if(response)
    {
      jd->callback(data);
    }
    if(jd->requestor)
      free(jd->requestor);
    free(jd->message);
    free(jd);
}

static void jabber_list_dialog_callback( char * text, gpointer data )
{
    JABBER_Dialog_PTR jd;

    eb_debug(DBG_JBR, ">\n");
    jd = (JABBER_Dialog_PTR)data;
    eb_debug(DBG_JBR, "**** response: %s\n", text);
    jd->response=text;
    jd->callback(data);
    free(jd->message);
    free(jd->requestor);
    free(jd->response);
    free(jd);
    eb_debug(DBG_JBR, "<");
}

static int eb_jabber_query_connected( eb_account * account )
{
    eb_jabber_account_data * jad = account->protocol_account_data;

    eb_debug(DBG_JBR, ">\n");
    if(ref_count <= 0 ) {
        jad->status = JABBER_OFFLINE;
	ref_count = 0;
    }
    eb_debug(DBG_JBR, "<, returning: %i\n", jad->status != JABBER_OFFLINE);
    return jad->status != JABBER_OFFLINE;

}

static void ay_jabber_cancel_connect (void *data) 
{
	eb_local_account *ela = (eb_local_account *)data;
	eb_jabber_local_account_data * jlad;
	jlad = (eb_jabber_local_account_data *)ela->protocol_local_account_data;
	
	ay_socket_cancel_async(jlad->connect_tag);
	jlad->activity_tag=0;
	jlad->connect_tag = 0;
	eb_jabber_logout(ela);
}

static void eb_jabber_login( eb_local_account * account )
{
    eb_jabber_local_account_data * jlad;
    char buff[1024];
    
    eb_debug(DBG_JBR, ">\n");

    jlad = (eb_jabber_local_account_data *)account->protocol_local_account_data;
    jabber_local_account = account;
    
    if (account->connected || account->connecting) 
	    return;
    
    account->connected = 0;
    account->connecting = 1;
    snprintf(buff, sizeof(buff), _("Logging in to Jabber account: %s"), account->handle);
    jlad->activity_tag = ay_activity_bar_add(buff, ay_jabber_cancel_connect, account);
    jlad->connect_tag = JABBER_Login(account->handle, jlad->password, 
			    jabber_server,  atoi(jabber_port));
}

void JABBERNotConnected(void *data)
{
    eb_jabber_local_account_data * jlad;
    jlad = (eb_jabber_local_account_data *)jabber_local_account->protocol_local_account_data;
    jabber_local_account->connected=0;
    jabber_local_account->connecting=0;

    ay_activity_bar_remove(jlad->activity_tag);
    jlad->activity_tag = 0;	
}

void JABBERConnected(void *data)
{
    eb_jabber_local_account_data * jlad;
    jlad = (eb_jabber_local_account_data *)jabber_local_account->protocol_local_account_data;

    ay_activity_bar_remove(jlad->activity_tag);
    jlad->activity_tag = 0;
    
    jlad->JConn=data;
    is_setting_state = 1;
    if(!jlad->JConn)
    {
        jabber_local_account->connected = 0;
        jabber_local_account->connecting = 0;
    	jlad->status=JABBER_OFFLINE;
    }
    else
    {
        jlad->status=JABBER_ONLINE;
        ref_count++;
        is_setting_state = 1;
        jabber_local_account->connected = 1;
        jabber_local_account->connecting = 0;

    	if(jabber_local_account->status_menu)
    	{
        	eb_debug(DBG_JBR, "eb_jabber_login: status - %i\n", jlad->status);
		eb_set_active_menu_status(jabber_local_account->status_menu, jlad->status);
    	}
    }
    is_setting_state = 0;
}

static void eb_jabber_logout( eb_local_account * account )
{
	eb_jabber_local_account_data * jlad;
	eb_jabber_account_data *jad;
	LList *l;

	eb_debug(DBG_JBR, ">\n");
	jlad = (eb_jabber_local_account_data *)account->protocol_local_account_data;
	for (l = jabber_contacts; l; l = l->next) {
		eb_account * ea = find_account_by_handle(l->data, SERVICE_INFO.protocol_id);
		if(!ea)
			fprintf(stderr, "Unable to find account for user: %s\n", (char *)l->data);
		else {
			eb_debug(DBG_JBR, "Checking to logoff buddy %s\n", (char *)l->data);
			jad = ea->protocol_account_data;
			if(jad->status!=JABBER_OFFLINE && jlad->JConn==jad->JConn) {
				buddy_logoff(ea);
				jad->status=JABBER_OFFLINE;
				buddy_update_status(ea);
			}
		}
	}
	eb_debug(DBG_JBR, "Calling JABBER_Logout\n");
	account->connected = 0;
	account->connecting = 0;
	JABBER_Logout(jlad->JConn);
	jlad->JConn=NULL;
	jlad->status=JABBER_OFFLINE;
	eb_debug(DBG_JBR, "<\n");
}

static void eb_jabber_send_im( eb_local_account * from, eb_account * account_to, 
					 char * message)
{
	eb_jabber_account_data * jad = account_to->protocol_account_data;

    JABBER_SendMessage(jad->JConn, account_to->handle, message);
}

static eb_local_account * eb_jabber_read_local_account_config( LList * values )
{
	char buff[255];
	char * name, *pass,*str;

	eb_local_account * ela = NULL;
	eb_jabber_local_account_data *jlad = NULL;

	name=value_pair_get_value( values, "SCREEN_NAME" );
	if(!name) {
		fprintf(stderr, "Error!  SCREEN_NAME not defined for jabber account!\n");
	}
	else {
		pass=value_pair_get_value(values, "PASSWORD");
		if(!pass) {
			fprintf(stderr, "Error!  PASSWORD not defined for jabber account %s!\n", name);
		}
		else {
			jlad = g_new0(eb_jabber_local_account_data,1);
			jlad->status = JABBER_OFFLINE;
			strcpy( jlad->password, pass);
			free( pass );

			ela = g_new0( eb_local_account, 1);
			ela->handle = name;

			/*the alias will be the persons login minus the @servername */
			strcpy( buff, ela->handle );
			strtok( buff, "@" );
			strcpy(ela->alias, buff );
			ela->service_id = SERVICE_INFO.protocol_id;
			ela->protocol_local_account_data = jlad;
		}
	}
	str = value_pair_get_value(values,"CONNECT");
	ela->connect_at_startup=(str && !strcmp(str,"1"));
	free(str);

	return ela;
}

static LList * eb_jabber_write_local_config( eb_local_account * account )
{
	value_pair * val;
	LList * vals = NULL;
	eb_jabber_local_account_data * jlad = account->protocol_local_account_data;

	val = g_new0( value_pair, 1 );
	strcpy(val->key, "SCREEN_NAME" );
	strcpy(val->value, account->handle );
	vals = l_list_append( vals, val );

	val = g_new0( value_pair, 1 );
	strcpy(val->key, "PASSWORD");
	strcpy(val->value, jlad->password );
	vals = l_list_append( vals, val );

	val = g_new0( value_pair, 1 );
	strcpy( val->key, "CONNECT" );
	if (account->connect_at_startup)
		strcpy( val->value, "1");
	else 
		strcpy( val->value, "0");
	
	vals = l_list_append( vals, val );
	return vals;
}

static eb_account * eb_jabber_read_account_config( LList * config, struct contact * contact)
{
	eb_account * ea = g_new0(eb_account, 1);
	eb_jabber_account_data * jad = g_new0( eb_jabber_account_data, 1 );
	char	*str = NULL;

	jad->status = JABBER_OFFLINE;
	jad->JConn = NULL;
	str = value_pair_get_value( config, "NAME");
	strncpy(ea->handle, str, 255 );
	free( str );
	str = value_pair_get_value(config, "LOCAL_ACCOUNT");
	if (str) {
		ea->ela = find_local_account_by_handle(str, SERVICE_INFO.protocol_id);
		g_free(str);
	} else 
		ea->ela = find_local_account_for_remote(ea, 0);
	
	ea->service_id = SERVICE_INFO.protocol_id;
	ea->protocol_account_data = jad;
	ea->account_contact = contact;
	ea->list_item = NULL;
	ea->online = 0;
	ea->status = NULL;
	ea->pix = NULL;
	ea->icon_handler = -1;
	ea->status_handler = -1;

	eb_jabber_add_user(ea);

	return ea;
}

static LList * eb_jabber_get_states()
{
	LList * list = NULL;

	eb_debug(DBG_JBR, ">\n");
	list = l_list_append( list, "Online" );
	list = l_list_append( list, "Away" );
	list = l_list_append( list, "Do Not Disturb" );
	list = l_list_append( list, "Extended Away" );
	list = l_list_append( list, "Chat" );
	list = l_list_append( list, "Offline" );

	eb_debug(DBG_JBR, "<\n");
	return list;
}

static char * eb_jabber_check_login(char * user, char * pass)
{
	return NULL;
}

static int eb_jabber_get_current_state( eb_local_account * account )
{
	eb_jabber_local_account_data * jlad = account->protocol_local_account_data;
	eb_debug(DBG_JBR, "returning: %i\n", jlad->status);
	return jlad->status;
}

static void eb_jabber_set_current_state( eb_local_account * account, int state )
{
	eb_jabber_local_account_data * jlad = account->protocol_local_account_data;

	if (is_setting_state) {
		jlad->status = state;
		return;
	}
	
	eb_debug(DBG_JBR, ">: state %i jlad->status: %i\n", state, jlad->status);
	if(state == JABBER_OFFLINE && jlad->status != JABBER_OFFLINE) {
		eb_debug(DBG_JBR, "Calling eb_jabber_logout\n");
		eb_jabber_logout(account);
	}
	else if(jlad->status == JABBER_OFFLINE && state != JABBER_OFFLINE) {
		eb_jabber_login(account);
		if(!account->connected)
		{
			eb_debug(DBG_JBR, "<, account not connected\n");
			return;
		}
		eb_debug(DBG_JBR, "Calling JABBER_ChangeState\n");
		JABBER_ChangeState(jlad->JConn, state);
	}
	else {
		eb_debug(DBG_JBR, "Calling JABBER_ChangeState\n");
		JABBER_ChangeState(jlad->JConn, state);
	}
	jlad->status=state;
	eb_debug(DBG_JBR, "<, new state is: %d\n", jlad->status);
}

static void eb_jabber_terminate_chat(eb_account * account )
{
	eb_jabber_account_data *jad=account->protocol_account_data;

    JABBER_EndChat(jad->JConn, account->handle);
}

static void eb_jabber_add_user(eb_account * account )
{
	eb_jabber_account_data *jad=account->protocol_account_data;

    jabber_contacts = l_list_append(jabber_contacts, account->handle);
    if (jad) JABBER_AddContact(jad->JConn, account->handle);
}

static void eb_jabber_del_user(eb_account * account )
{
    eb_jabber_account_data *jad=account->protocol_account_data;
     
    if(jad && JABBER_RemoveContact(jad->JConn, account->handle)==0)
    	jabber_contacts = l_list_remove(jabber_contacts, account->handle);
    g_free(jad);
    account->protocol_account_data=NULL;
}

static eb_account * eb_jabber_new_account(eb_local_account *ela, const char * account )
{
	eb_account * ea = g_new0(eb_account, 1);
	eb_jabber_account_data * jad = g_new0( eb_jabber_account_data, 1 );
	ea->ela = ela;
	ea->protocol_account_data = jad;
	strncpy(ea->handle, account, 255 );
	ea->service_id = SERVICE_INFO.protocol_id;
	jad->status = JABBER_OFFLINE;

	return ea;
}

static char ** eb_jabber_get_status_pixmap( eb_account * account)
{
	eb_jabber_account_data * jad;

	/*if (!pixmaps)
		eb_jabber_init_pixmaps();*/
	
	jad = account->protocol_account_data;
	
	if(jad->status == JABBER_ONLINE)
		return jabber_online_xpm;
	else
		return jabber_away_xpm;
}

static char * eb_jabber_get_status_string( eb_account * account )
{
	eb_jabber_account_data * jad = account->protocol_account_data;
	return jabber_status_strings[jad->status];
}

static void eb_jabber_set_idle( eb_local_account * account, int idle )
{
    if ((idle == 0) && eb_jabber_get_current_state(account) == JABBER_AWAY)
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, JABBER_ONLINE);

        }

    }
    if( idle >= 600 && eb_jabber_get_current_state(account) == JABBER_ONLINE )
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, JABBER_AWAY);

        }
    }

}

static void eb_jabber_set_away( eb_local_account * account, char * message )
{
    if(message)
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, JABBER_AWAY);

        }
    }
    else
    {
        if(account->status_menu)
        {
	    eb_set_active_menu_status(account->status_menu, JABBER_ONLINE);

        }

    }

}

static void eb_jabber_send_chat_room_message( eb_chat_room * room, char * message )
{
	eb_jabber_local_account_data *jlad = room->local_user->protocol_local_account_data;
	JABBER_SendChatRoomMessage(jlad->JConn, room->room_name, message, room->local_user->alias);
}

static void eb_jabber_join_chat_room( eb_chat_room * room )
{
	eb_jabber_local_account_data *jlad = room->local_user->protocol_local_account_data;

	eb_debug(DBG_JBR, ">\n");
	JABBER_JoinChatRoom(jlad->JConn, room->room_name, room->local_user->alias);
	eb_debug(DBG_JBR, "<\n");
}

static void eb_jabber_leave_chat_room( eb_chat_room * room )
{
	eb_jabber_local_account_data *jlad = room->local_user->protocol_local_account_data;
	JABBER_LeaveChatRoom(jlad->JConn, room->room_name, room->local_user->alias);
}

static eb_chat_room * eb_jabber_make_chat_room( char * name, eb_local_account * account )
{
	eb_chat_room *ecr = g_new0(eb_chat_room, 1);
	char *ptr=NULL;

	eb_debug(DBG_JBR, ">\n");
	j_list_agents();
	strcpy( ecr->room_name, name );
	strcpy( ecr->id, name);
	ptr=ecr->id;
	/* Convert the id to lowercase */
	while(*ptr) {
		*ptr=tolower(*ptr);
		ptr++;
	}
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;
	eb_join_chat_room(ecr);
	eb_debug(DBG_JBR, "<\n");
	return ecr;
}

static void eb_jabber_send_invite( eb_local_account * account, eb_chat_room * room,
						  char * user, char * message )
{
	eb_debug(DBG_JBR, "Empty function\n");
}

static void eb_jabber_get_info( eb_local_account * reciever, eb_account * sender)
{
   char buff[1024];

   eb_debug(DBG_JBR, "Not implemented yet\n");
   if(sender->infowindow == NULL){
     sender->infowindow = eb_info_window_new(reciever, sender);
   }

   if(sender->infowindow->info_type == -1 || sender->infowindow->info_data == NULL){
      if(sender->infowindow->info_data == NULL) {
        sender->infowindow->info_data = malloc(sizeof(jabber_info_data));
        ((jabber_info_data *)sender->infowindow->info_data)->profile = NULL;
        sender->infowindow->cleanup = jabber_info_data_cleanup;
      }
      sender->infowindow->info_type = SERVICE_INFO.protocol_id;
    }
    if(sender->infowindow->info_type != SERVICE_INFO.protocol_id) {
       /*hmm, I wonder what should really be done here*/
       return; 
    }
    sprintf(buff,"THIS_IS_NOT_IMPLEMENTED YET(%s)",sender->handle);
    if( ((jabber_info_data *)sender->infowindow->info_data)->profile != NULL)
      free(((jabber_info_data *)sender->infowindow->info_data)->profile);
    ((jabber_info_data *)sender->infowindow->info_data)->profile = malloc(strlen(buff)+1);
    strcpy(((jabber_info_data *)sender->infowindow->info_data)->profile,buff);

    jabber_info_update(sender);
}


static void jabber_info_update(eb_account *account) {
  char buff[1024];
  info_window *iw=account->infowindow;
  jabber_info_data * mid = (jabber_info_data *)iw->info_data;

  eb_debug(DBG_JBR, "Not implemented yet\n");
  eb_info_window_clear(iw);
  sprintf(buff,"Profile for <B>%s</B><BR><HR>",iw->remote_account->handle);
  eb_info_window_add_info(account, buff, 0, 0, 0);
  sprintf(buff,"<a href=\"%s\">%s</a>",mid->profile,mid->profile);
  eb_info_window_add_info(account, buff, 0, 0, 0);
}

static void jabber_info_data_cleanup(info_window *iw){
  jabber_info_data * mid = (jabber_info_data *)iw->info_data;
  eb_debug(DBG_JBR, "Entering and leaving\n");
  if(mid->profile != NULL) free(mid->profile);
}

/*	There are no prefs for Jabber at the moment.

static input_list * eb_jabber_get_prefs()
{
	return jabber_prefs;
}
*/

static void eb_jabber_read_prefs_config(LList * values)
{
	char * c;
	c = value_pair_get_value(values, "server");

	if(c)
	{
		strcpy(jabber_server, c);
		free( c );
	}
	c = value_pair_get_value(values, "port");
	if(c)
	{
		strcpy(jabber_port, c);
		free( c );
	}
	c = value_pair_get_value(values, "do_jabber_debug");
	if(c)
	{
		do_jabber_debug=atoi(c);
		free( c );
	}
}

static LList * eb_jabber_write_prefs_config()
{
	LList * config  = NULL;
	char buffer[5];

	config = value_pair_add(config, "server", jabber_server);
	config = value_pair_add(config, "port", jabber_port);
	sprintf(buffer, "%d", do_jabber_debug);
	config = value_pair_add(config, "do_jabber_debug", buffer);

	return config;
}

struct service_callbacks * query_callbacks()
{
	struct service_callbacks * sc;

	sc = g_new0( struct service_callbacks, 1 );
	
	sc->query_connected = eb_jabber_query_connected;
	sc->login = eb_jabber_login;
	sc->logout = eb_jabber_logout;
	sc->send_im = eb_jabber_send_im;
	sc->read_local_account_config = eb_jabber_read_local_account_config;
	sc->write_local_config = eb_jabber_write_local_config;
	sc->read_account_config = eb_jabber_read_account_config;
	sc->get_states = eb_jabber_get_states;
	sc->get_current_state = eb_jabber_get_current_state;
	sc->set_current_state = eb_jabber_set_current_state;
	sc->check_login = eb_jabber_check_login;
	sc->add_user = eb_jabber_add_user;
	sc->del_user = eb_jabber_del_user;
	sc->new_account = eb_jabber_new_account;
	sc->get_status_string = eb_jabber_get_status_string;
	sc->get_status_pixmap = eb_jabber_get_status_pixmap;
	sc->set_idle = eb_jabber_set_idle;
	sc->set_away = eb_jabber_set_away;
	sc->send_chat_room_message = eb_jabber_send_chat_room_message;
	sc->join_chat_room = eb_jabber_join_chat_room;
	sc->leave_chat_room = eb_jabber_leave_chat_room;
	sc->make_chat_room = eb_jabber_make_chat_room;
	sc->send_invite = eb_jabber_send_invite;
	sc->terminate_chat = eb_jabber_terminate_chat;
	sc->get_info = eb_jabber_get_info;
	sc->get_prefs = NULL;
	sc->read_prefs_config = eb_jabber_read_prefs_config;
	sc->write_prefs_config = eb_jabber_write_prefs_config;
	sc->add_importers = NULL;

	sc->get_color = eb_jabber_get_color;
	sc->get_smileys = eb_default_smileys;

	return sc;
}

void JABBERChatRoomBuddyStatus(char *id, char *user, int offline)
{
	eb_chat_room *ecr=find_chat_room_by_id(id);

	if(!ecr)
	{
		g_warning("Chat room does not exist: %s", id);
		return;
	}
	if(!offline)
	{
		eb_account *ea=find_account_by_handle(user, SERVICE_INFO.protocol_id);
		if(ea)
		{
			eb_chat_room_buddy_arrive(ecr, ea->account_contact->nick, user);
		}
		else
		{
			eb_chat_room_buddy_arrive(ecr, user, user);
		}

	}
	else
		eb_chat_room_buddy_leave(ecr, user);
}

void JABBERChatRoomMessage(char *id, char *user, char *message)
{
	eb_chat_room *ecr=find_chat_room_by_id(id);
	eb_account *ea=find_account_by_handle(user, SERVICE_INFO.protocol_id);
	char *message2 = linkify(message);

	if(!ecr)
	{
		g_warning("Chat room does not exist: %s", id);
		g_free(message2);
		return;
	}
	if(ea)
	{
		eb_chat_room_show_message(ecr, ea->account_contact->nick, message2);
	}
	else
	{
		eb_chat_room_show_message(ecr, user, message2);
	}
	g_free(message2);
}


void JABBERDelBuddy(void *data)
{
	eb_account *ea;
	char *jid=data;

	if(!data) {
		eb_debug(DBG_JBR, "called null argument\n");
		return;
	}

	ea = find_account_by_handle(jid, SERVICE_INFO.protocol_id);
	if(!ea) {
		eb_debug(DBG_JBR, "Unable to find %s to remove\n", jid);
		return;
	}
	eb_jabber_del_user(ea);
}

/*
** Name:    JABBERAddBuddy
** Purpose: This function is a callback that is called to ensure that
**          everybuddy knows about our new buddies.
** Input:   data - data passed to contact
** Output:  none
*/

void JABBERAddBuddy(void *data)
{
    struct jabber_buddy *jb;
    eb_account *ea;
	eb_jabber_account_data *jad;

    if(!data)
        return;
    jb = (struct jabber_buddy *)data;
    eb_debug(DBG_JBR, "> - %s\n", jb->jid);

    ea = find_account_by_handle(jb->jid, SERVICE_INFO.protocol_id);
    if (!ea) {	/* Add an unknown account */
	ea = eb_jabber_new_account(NULL, jb->jid);
        if ( !find_grouplist_by_name("Unknown" )) {
          add_group("Unknown");
        }
        add_unknown(ea);
    }
	jad=ea->protocol_account_data;
	/* Add the connection so we know which account a buddy is tied to */
	jad->JConn=jb->JConn;
    eb_debug(DBG_JBR, "<\n");
}

/*
** Name:     JABBERInstantMessage
** Purporse: This function acts as the gateway between the libjabber and the 
**           gtk interface
** Input:    data    - data needed for instant message
** Output:   none
*/

void JABBERInstantMessage(void *data)
{
    JABBER_InstantMessage_PTR im;
    eb_account *sender = NULL;
    eb_account *ea;

    eb_debug(DBG_JBR, ">\n");
    im = (JABBER_InstantMessage_PTR)data;
    sender = find_account_by_handle(im->sender, SERVICE_INFO.protocol_id);
    if (sender == NULL) {
	ea = eb_jabber_new_account(NULL, im->sender);

        add_unknown(ea);
        sender = ea;
    }

    eb_parse_incoming_message(jabber_local_account, sender, im->msg);
    eb_debug(DBG_JBR, "<\n");
}

/*
** Name:    JABBERStatusChange
** Purpose: This function is a callback that is called to update the
**          status of a contact in the forward list
** Input:   data - data passed to contact
** Output:  none
*/

void JABBERStatusChange(void *data)
{
    struct jabber_buddy *jb;
    eb_account *ea;
    eb_jabber_account_data *jad;

    if(!data)
        return;
    eb_debug(DBG_JBR, ">\n");
    jb = (struct jabber_buddy *)data;

    ea = find_account_by_handle(jb->jid, SERVICE_INFO.protocol_id);
    if (!ea) {	/* Add an unknown account */
	ea = eb_jabber_new_account(NULL, jb->jid);
        if ( !find_grouplist_by_name("Unknown" )) {
          add_group("Unknown");
        }
        add_unknown(ea);
    }
    jad = ea->protocol_account_data;

    eb_debug(DBG_JBR,  "New status for %s is %i\n", jb->jid, jb->status);
    if (jb->status != JABBER_OFFLINE && jad->status==JABBER_OFFLINE) {
       jad->status = jb->status;
       buddy_login(ea);
    } else if (jb->status == JABBER_OFFLINE && jad->status!=JABBER_OFFLINE) {
       jad->status = jb->status;
       buddy_logoff(ea);
    }
    jad->status = jb->status;
	jad->JConn=jb->JConn;
    buddy_update_status(ea); 
    eb_debug(DBG_JBR, "<\n");
}

void JABBERListDialog(char **list, void *data)
{
    JABBER_Dialog_PTR jd;

    if(!data || !list)
        return;

    jd = (JABBER_Dialog_PTR)data;
    eb_debug(DBG_JBR, "Calling do_list_dialog\n");
    do_list_dialog(jd->message, jd->heading, list, jabber_list_dialog_callback, (gpointer)jd);
    eb_debug(DBG_JBR, "Returned from do_list_dialog\n");
}

/*
** Name:    JABBERDialog
** Purpose: This function is a callback that is called to ask for authorization
** Input:   data - data passed to contact
** Output:  none
*/

void JABBERDialog(void *data)
{
    JABBER_Dialog_PTR jd;

    if(!data)
        return;
    eb_debug(DBG_JBR, ">\n");
    jd = (JABBER_Dialog_PTR)data;
    eb_do_dialog(jd->message, jd->heading, jabber_dialog_callback, (gpointer)jd);
    eb_debug(DBG_JBR, "<\n");
              
    return;
}

/*
** Name:    JABBERLogout
** Purpose: This function is called when a user is logged out automatically
**          by the server
** Input:   data - date to be passed
** Output:  none
*/

void JABBERLogout(void *data)
{
    if (ref_count >0)
	    ref_count--;
    is_setting_state = 1;

    eb_debug(DBG_JBR, ">\n");
    jabber_local_account->connected = 0;
    jabber_local_account->connecting = 0;
    if(jabber_local_account->status_menu) {
        eb_debug(DBG_JBR, "Setting menu to JABBER_OFFLINE\n");
	eb_set_active_menu_status(jabber_local_account->status_menu, JABBER_OFFLINE);
    }
    is_setting_state = 0;
    JABBERNotConnected(NULL);
    eb_debug(DBG_JBR, "<\n");
}

void	JABBERError( char *message, char *title )
{
	ay_do_error( title, message );
}
