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
int do_jabber_debug=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE, 
	"Jabber", 
	"Provides Jabber Messenger support", 
	"$Revision: 1.45 $",
	"$Date: 2005/02/13 13:32:27 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
struct service SERVICE_INFO = { "Jabber", -1, 
	SERVICE_CAN_MULTIACCOUNT | SERVICE_CAN_OFFLINEMSG | SERVICE_CAN_GROUPCHAT | SERVICE_CAN_ICONVERT, NULL };
/* End Module Exports */

static char *eb_jabber_get_color(void) { static char color[]="#88aa00"; return color; }
static int eb_jabber_send_typing( eb_local_account * from, eb_account * account_to );
static int eb_jabber_send_cr_typing( eb_chat_room *chatroom );
unsigned int module_version() {return CORE_VERSION;}

static int plugin_init()
{
	input_list * il = g_new0(input_list, 1);
	eb_debug(DBG_MOD, "Jabber\n");
	ref_count=0;
	plugin_info.prefs = il;
	il->widget.entry.value = jabber_server;
	il->name = "jabber_server";
	il->label = _("Default Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_jabber_debug;
	il->name = "do_jabber_debug";
	il->label = _("Enable debugging");
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
	char password[MAX_PREF_LEN];	// account password
	int fd;				// the file descriptor
	int status;			// the current status of the user
	int prompt_password;
	JABBER_Conn	*JConn;
	int activity_tag;
	int connect_tag;
	int typing_tag;
	int use_ssl;
	char server_port[MAX_PREF_LEN];
	char ssl_server_port[MAX_PREF_LEN];
	LList *jabber_contacts;
} eb_jabber_local_account_data;

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
void JABBERListDialog(const char **list, void *data);
void JABBERLogout(void *data);
void JABBERError( char *message, char *title );

static eb_local_account *find_local_account_by_conn(JABBER_Conn *JConn)
{
	LList *acc;
	for (acc = accounts; acc && acc->data; acc=acc->next) {
		if ( ((eb_local_account *)(acc->data))->service_id == SERVICE_INFO.protocol_id) {
			eb_jabber_local_account_data *jlad = ((eb_local_account *)(acc->data))->protocol_local_account_data;
			if (jlad->JConn && jlad->JConn == JConn) {
				eb_debug(DBG_JBR, "found (%s) !\n", ((eb_local_account *)(acc->data))->handle);
				return (eb_local_account *)(acc->data);
			} else 
				eb_debug(DBG_JBR, "JConns: %p %p didn't match\n",
						JConn, jlad->JConn);
		}
	}
	for (acc = accounts; acc && acc->data; acc=acc->next) {
		if ( ((eb_local_account *)(acc->data))->service_id == SERVICE_INFO.protocol_id) {
			eb_local_account *ela = ((eb_local_account *)(acc->data));
			eb_jabber_local_account_data *jlad = ela->protocol_local_account_data;
			char *user = strdup(JConn->jid);
			if (*strstr(user, "/"))
				*strstr(user, "/") = 0;
			
			if (!jlad->JConn && !strcmp(ela->handle, user)) {
				eb_debug(DBG_JBR, "found (%s) via handle!\n", ((eb_local_account *)(acc->data))->handle);
				free(user);
				return (eb_local_account *)(acc->data);
			} else 
				eb_debug(DBG_JBR, "JConns: %p %p didn't match\n",
						JConn, jlad->JConn);
			free(user);
		}
	}
	
	return NULL;
}

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

static void eb_jabber_finish_login( const char *password, void *data)
{
	eb_local_account *account = data;
	eb_jabber_local_account_data * jlad;
	char buff[1024];
	int port = 5222;
	
	eb_debug(DBG_JBR, ">\n");

	jlad = (eb_jabber_local_account_data *)account->protocol_local_account_data;
	
	account->connected = 0;
	account->connecting = 1;
	snprintf(buff, sizeof(buff), _("Logging in to Jabber account: %s"), account->handle);
	jlad->activity_tag = ay_activity_bar_add(buff, ay_jabber_cancel_connect, account);
	if (!jlad->server_port || !strlen(jlad->server_port)) {
#ifdef HAVE_OPENSSL
	strcpy(jlad->ssl_server_port,"5223");
#endif
	strcpy(jlad->server_port,"5222");
	}
	
#ifdef HAVE_OPENSSL
	if (jlad->use_ssl)
		port = atoi(jlad->ssl_server_port);
	else
#endif
		port = atoi(jlad->server_port);
	
	jlad->connect_tag = JABBER_Login(account->handle, (char *)password, 
			jabber_server, jlad->use_ssl, port);
}

static void eb_jabber_login( eb_local_account * account )
{
	eb_jabber_local_account_data * jlad;
	char buff[1024];

	if (account->connected || account->connecting) 
		return;
	
	jlad = (eb_jabber_local_account_data *)account->protocol_local_account_data;
	if(jlad->prompt_password || !jlad->password || !strlen(jlad->password)) {
		snprintf(buff, sizeof(buff), _("Jabber password for: %s"), account->handle);
		do_password_input_window(buff, "", eb_jabber_finish_login, account);
	} else {
		eb_jabber_finish_login(jlad->password, account);
	}

}

void JABBERNotConnected(void *data)
{
	JABBER_Conn *JConn = (JABBER_Conn *)data;
	eb_local_account *ela = NULL;
	eb_jabber_local_account_data * jlad = NULL;
	if (!JConn) {
	eb_debug(DBG_JBR, "No JConn!\n");
	return;
	}
	ela = find_local_account_by_conn(JConn);
	if (!ela) {
		eb_debug(DBG_JBR, "No ela!\n");
		return;
	}
	jlad = ela->protocol_local_account_data;
	
	ela->connected = ela->connecting = 0;

	ay_activity_bar_remove(jlad->activity_tag);
	jlad->activity_tag = 0;	
}

void JABBERConnected(void *data)
{
	JABBER_Conn *JConn = (JABBER_Conn *)data;
	eb_local_account *ela = NULL;
	eb_jabber_local_account_data * jlad = NULL;
	if (!JConn) {
	eb_debug(DBG_JBR, "No JConn!\n");
	return;
	}
	ela = find_local_account_by_conn(JConn);
	if (!ela) {
		eb_debug(DBG_JBR, "No ela!\n");
		return;
	}
	jlad = ela->protocol_local_account_data;

	ay_activity_bar_remove(jlad->activity_tag);
	jlad->activity_tag = 0;
	
	jlad->JConn=data;
	is_setting_state = 1;
	if(!jlad->JConn)
	{
		ela->connected = 0;
		ela->connecting = 0;
		jlad->status=JABBER_OFFLINE;
	}
	else
	{
		jlad->status=JABBER_ONLINE;
		ref_count++;
		is_setting_state = 1;
		ela->connected = 1;
		ela->connecting = 0;

		if(ela->status_menu)
		{
			eb_debug(DBG_JBR, "eb_jabber_login: status - %i\n", jlad->status);
		eb_set_active_menu_status(ela->status_menu, jlad->status);
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
	for (l = jlad->jabber_contacts; l; l = l->next) {
		eb_account * ea = find_account_with_ela(l->data, account);
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

static void jabber_account_prefs_init(eb_local_account *ela)
{
	eb_jabber_local_account_data *jlad = ela->protocol_local_account_data;
	input_list *il = g_new0(input_list, 1);

	ela->prefs = il;

	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label= _("_Username:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = jlad->password;
	il->name = "PASSWORD";
	il->label= _("_Password:");
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &jlad->prompt_password;
	il->name = "prompt_password";
	il->label= _("_Ask for password at Login time");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ela->connect_at_startup;
	il->name = "CONNECT";
	il->label= _("_Connect at startup");
	il->type = EB_INPUT_CHECKBOX;

#ifdef HAVE_OPENSSL
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &jlad->use_ssl;
	il->name = "USE_SSL";
	il->label= _("Use _SSL");
	il->type = EB_INPUT_CHECKBOX;
#endif	
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = jlad->server_port;
	il->name = "PORT";
	il->label= _("P_ort:");
	il->type = EB_INPUT_ENTRY;
	
#ifdef HAVE_OPENSSL
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = jlad->ssl_server_port;
	il->name = "SSL_PORT";
	il->label= _("SSL Po_rt:");
	il->type = EB_INPUT_ENTRY;
#endif	
}

static eb_local_account * eb_jabber_read_local_account_config( LList * values )
{
	char buff[255], *tmp;

	eb_local_account * ela = NULL;
	eb_jabber_local_account_data *jlad = NULL;

	jlad = g_new0(eb_jabber_local_account_data,1);
	jlad->status = JABBER_OFFLINE;

	ela = g_new0( eb_local_account, 1);
	ela->protocol_local_account_data = jlad;

	jabber_account_prefs_init(ela);
	eb_update_from_value_pair(ela->prefs, values);

	/*the alias will be the persons login minus the @servername */
	strcpy( buff, ela->handle );
	tmp = strchr(buff, '@');
	if(tmp)
		*tmp='\0';
	strcpy(ela->alias, buff );
	ela->service_id = SERVICE_INFO.protocol_id;

	return ela;
}

static LList * eb_jabber_write_local_config( eb_local_account * account )
{
	return eb_input_to_value_pair(account->prefs);
}

static eb_account * eb_jabber_read_account_config( eb_account *ea, LList * config)
{
	eb_jabber_account_data * jad = g_new0( eb_jabber_account_data, 1 );

	jad->status = JABBER_OFFLINE;
	jad->JConn = NULL;
	
	ea->protocol_account_data = jad;

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
	eb_jabber_local_account_data *jlad = NULL;
	JABBER_Conn *conn = NULL;
	if (account->ela) {
	jlad = (eb_jabber_local_account_data *)(account->ela->protocol_local_account_data);
	conn = jlad->JConn;
	} else {
	conn = jad->JConn;
	}
	if (jlad) jlad->jabber_contacts = l_list_append(jlad->jabber_contacts, account->handle);
	if (jad) JABBER_AddContact(conn, account->handle);
}

static void eb_jabber_del_user(eb_account * account )
{
	JABBER_Conn *conn = NULL;
	eb_jabber_local_account_data *jlad = NULL;
	if (account->ela) {
		jlad = (eb_jabber_local_account_data *)(account->ela->protocol_local_account_data);
		conn = jlad->JConn;
	} else {
	eb_jabber_account_data *jad = (eb_jabber_account_data *)(account->protocol_account_data);
	if (jad) 
		conn = jad->JConn;
	}
	if(JABBER_RemoveContact(conn, account->handle)==0)
		jlad->jabber_contacts = l_list_remove(jlad->jabber_contacts, account->handle);
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

static void eb_jabber_set_away( eb_local_account * account, char * message, int away )
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

static eb_chat_room * eb_jabber_make_chat_room( char * name, eb_local_account * account, int is_public )
{
	eb_chat_room *ecr = g_new0(eb_chat_room, 1);
	char *ptr=NULL;

	eb_debug(DBG_JBR, ">\n");
	j_list_agents();
	while (strstr(name, " "))
		*(strstr(name, " ")) = '_';
	
	ptr=name;
	/* Convert the name to lowercase */
	while(*ptr) {
		*ptr=tolower(*ptr);
		ptr++;
	}
	
	strcpy( ecr->room_name, name );

	strcpy( ecr->id, name);
	
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


static void jabber_info_update(eb_account *account)
{
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

static void jabber_info_data_cleanup(info_window *iw)
{
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
	c = value_pair_get_value(values, "do_jabber_debug");
	if(c)
	{
		do_jabber_debug=atoi(c);
		free( c );
	}
}

static LList * eb_jabber_write_prefs_config()
{
	LList * config = NULL;
	char buffer[5];

	config = value_pair_add(config, "server", jabber_server);
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
	sc->send_typing = eb_jabber_send_typing;
	sc->send_cr_typing = eb_jabber_send_cr_typing;
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
	char *id2 = strdup(id);
	
	if (!ecr) {
		if (strstr(id2,"@"))
			*strstr(id2,"@") = 0;
		ecr = find_chat_room_by_id(id2);
		free(id2);
	}
	if(!ecr)
	{
		g_warning("Chat room does not exist: %s", id);
		return;
	}
	if(!offline)
	{
		eb_account *ea=find_account_with_ela(user, ecr->local_user);
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
	eb_account *ea=NULL;
	char *id2 = strdup(id);
	char *message2 = linkify(message);
	
	if (!ecr) {
		if (strstr(id2,"@"))
			*strstr(id2,"@") = 0;
		ecr = find_chat_room_by_id(id2);
		free(id2);
	}
	if(!ecr)
	{
		g_warning("Chat room does not exist: %s", id);
		g_free(message2);
		return;
	}
	
	ea = find_account_with_ela(user, ecr->local_user);
	
	if (!strcmp(id, user)) {
		//system message
		char *muser = strdup(message);
		if (strchr(muser,' '))
			*strchr(muser,' ') = 0;
		if (strstr(message," has joined")) {
			eb_chat_room_buddy_arrive(ecr, muser, muser);			
		} else if (strstr(message, " has left")) {
			eb_chat_room_buddy_leave(ecr, muser);
		}
		free(muser);
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


void JABBERDelBuddy(JABBER_Conn *JConn, void *data)
{
	eb_account *ea;
	char *jid=data;
	eb_local_account *ela = NULL;
	eb_jabber_local_account_data * jlad = NULL;
	if (!JConn) {
	eb_debug(DBG_JBR, "No JConn!\n");
	return;
	}
	ela = find_local_account_by_conn(JConn);
	if (!ela) {
		eb_debug(DBG_JBR, "No ela!\n");
		return;
	}
	jlad = ela->protocol_local_account_data;

	if(!data) {
		eb_debug(DBG_JBR, "called null argument\n");
		return;
	}

	ea = find_account_with_ela(jid, ela);
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
	struct jabber_buddy *jb = (struct jabber_buddy *)data;
	eb_account *ea;
	eb_jabber_account_data *jad;
	eb_local_account *ela = find_local_account_by_conn(jb->JConn);
	
	if(!data)
		return;
	
	if (!ela) {
		eb_debug(DBG_JBR, "can't find ela\n");
		return;
	}
	
	eb_debug(DBG_JBR, "> - %s\n", jb->jid);

	ea = find_account_with_ela(jb->jid, ela);
	if (!ea) {	/* Add an unknown account */
	ea = eb_jabber_new_account(ela, jb->jid);
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
	JABBER_InstantMessage_PTR im = (JABBER_InstantMessage_PTR)data;
	eb_account *sender = NULL;
	eb_account *ea;
	eb_local_account *ela = find_local_account_by_conn(im->JConn);
	
	if (!ela) {
		eb_debug(DBG_JBR, "no ela\n");
		sender = find_account_by_handle(im->sender, SERVICE_INFO.protocol_id);
		if (sender)
			ela = sender->ela;
		if (!ela) {
			eb_debug(DBG_JBR, "still no ela !\n");
				return;
		}
	}
	
	eb_debug(DBG_JBR, ">\n");
	sender = find_account_with_ela(im->sender, ela);

	if (sender == NULL) {
	ea = eb_jabber_new_account(ela, im->sender);

		add_unknown(ea);
		sender = ea;
	}

	eb_parse_incoming_message(ela, sender, im->msg);
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
	eb_local_account *ela = NULL;
	
	if(!data)
		return;
	eb_debug(DBG_JBR, ">\n");
	jb = (struct jabber_buddy *)data;

	ela = find_local_account_by_conn(jb->JConn);
	if (!ela) {
		eb_debug(DBG_JBR, "no ela!\n");
		return;
	}
	ea = find_account_with_ela(jb->jid, ela);
	if (!ea) {	/* Add an unknown account */
	ea = eb_jabber_new_account(ela, jb->jid);
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

void JABBERListDialog(const char **list, void *data)
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
	JABBER_Conn *JConn = (JABBER_Conn *)data;
	eb_local_account *ela = NULL;
	eb_jabber_local_account_data * jlad = NULL;
	if (!JConn) {
	eb_debug(DBG_JBR, "No JConn!\n");
	return;
	}
	ela = find_local_account_by_conn(JConn);
	if (!ela) {
		eb_debug(DBG_JBR, "No ela!\n");
		return;
	}
	jlad = ela->protocol_local_account_data;
	
	if (ref_count >0)
		ref_count--;
	is_setting_state = 1;

	eb_debug(DBG_JBR, ">\n");
	ela->connected = 0;
	ela->connecting = 0;
	if(ela->status_menu) {
		eb_debug(DBG_JBR, "Setting menu to JABBER_OFFLINE\n");
	eb_set_active_menu_status(ela->status_menu, JABBER_OFFLINE);
	}
	is_setting_state = 0;
	JABBERNotConnected(JConn);
	eb_debug(DBG_JBR, "<\n");
}

void	JABBERError( char *message, char *title )
{
	ay_do_error( title, message );
}

void JABBERBuddy_typing(JABBER_Conn *JConn, char *from, int typing) {
	eb_local_account *ela = NULL;
	eb_account *ea = NULL;
	
	ela = find_local_account_by_conn(JConn);
	printf("JABBERBuddy_Typing %s\n", from);
	if (!ela)
		return;
	printf("ela %s\n",ela->handle);
	ea = find_account_with_ela(from, ela);

	if (!ea) 
		return;
	printf("ea %s\n",ea->handle);
	if (iGetLocalPref("do_typing_notify"))
		eb_update_status(ea, typing?_("typing..."):"");
	
	
}

typedef struct {
	eb_local_account *from;
	eb_account *to;
} jabber_typing_callback_data;

static int eb_jabber_send_typing_stop(void *data)
{
	jabber_typing_callback_data *tcd = (jabber_typing_callback_data *)data;
	eb_local_account *from = tcd->from;
	eb_account *to = tcd->to;
	eb_jabber_local_account_data *jlad = (eb_jabber_local_account_data *)from->protocol_local_account_data;
	/* Stop typing notify is always sent */
	/*
	if (!iGetLocalPref("do_typing_notify"))
		return 0;
	*/
	JABBER_Send_typing(jlad->JConn, from->handle, to->handle, 0);
	free(tcd);

	/* return 0 to remove the timeout */
	return 0;
}

static int eb_jabber_send_typing( eb_local_account * from, eb_account * to )
{
	eb_jabber_local_account_data *jlad = (eb_jabber_local_account_data *)from->protocol_local_account_data;
	jabber_typing_callback_data *tcd = (jabber_typing_callback_data *)malloc(sizeof(jabber_typing_callback_data));
	
	if (!iGetLocalPref("do_typing_notify"))
		return 20;
	
	if (jlad->typing_tag) {
		eb_timeout_remove(jlad->typing_tag);
	}
	tcd->from = from;
	tcd->to = to;
	jlad->typing_tag = eb_timeout_add(5000, (void *)eb_jabber_send_typing_stop, tcd);
	JABBER_Send_typing(jlad->JConn, from->handle, to->handle, 1);
	return 4;
}

static int eb_jabber_send_cr_typing( eb_chat_room *chatroom )
{
	if (!iGetLocalPref("do_typing_notify"))
		return 20;
	
	return 4;
}
