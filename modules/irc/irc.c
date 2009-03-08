/*
 * IRC protocol support for Ayttm
 *
 * Copyright (C) 2008 Ayttm Team
 *
 * Derived from code which was:
 * Copyright (C) 2001, Erik Inge Bolso <knan@mo.himolde.no>
 *                     and others
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
 * irc.c
 */

#include "intl.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include "gtk_globals.h"
#include "service.h"
#include "util.h"
#include "value_pair.h"
#include "status.h"
#include "message_parse.h"
#include "info_window.h"
#include "gtk/html_text_buffer.h"
#include "plugin_api.h"
#include "smileys.h"
#include "messages.h"
#include "tcp_util.h"
#include "activity_bar.h"
#include "config.h"

#include "libirc.h"
#include "ctcp.h"
#include "irc.h"

#include "libproxy/libproxy.h"

#include "pixmaps/irc_online.xpm"
#include "pixmaps/irc_away.xpm"


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info irc_LTX_plugin_info
#define SERVICE_INFO irc_LTX_SERVICE_INFO
#define plugin_init irc_LTX_plugin_init
#define plugin_finish irc_LTX_plugin_finish
#define module_version irc_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();

static int ref_count = 0;
static int do_irc_debug=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"IRC",
	"Provides Internet Relay Chat (IRC) support",
	"$Revision: 1.49 $",
	"$Date: 2009/03/08 18:42:31 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "IRC", -1, 
				SERVICE_CAN_MULTIACCOUNT 
					| SERVICE_CAN_GROUPCHAT 
					| SERVICE_CAN_ICONVERT
					| SERVICE_CAN_OFFLINEMSG, 
				NULL };
/* End Module Exports */

static char *eb_irc_get_color(void) { static char color[]="#880088"; return color; }

unsigned int module_version() {return CORE_VERSION;}
static int plugin_init()
{
	input_list * il = g_new0(input_list, 1);

	eb_debug(DBG_MOD, "IRC\n");
	ref_count=0;

	plugin_info.prefs = il;
	il->widget.checkbox.value = &do_irc_debug;
	il->name = "do_irc_debug";
	il->label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

	return(0);
}

static int plugin_finish()
{
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return(ref_count);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/


/* taken from X-Chat 1.6.4: src/common/util.c */
/* Added: stripping of CTCP/2 color/formatting attributes, which is
   everything between two '\006' = ctrl-F characters.
   See http://www.lag.net/~robey/ctcp/ctcp2.2.txt for details. */
static unsigned char *strip_color (unsigned char *text)
{
	int nc = 0;
	int i = 0;
	int col = 0;
	int ctcp2 = 0;
	int len;
	unsigned char *new_str;

	if (text == NULL)
		text[0] = '\0';

	len = strlen ((char *)text);
	new_str = calloc (len + 2, sizeof(char));

	while (len > 0)
	{
		if ((col && isdigit (*text) && nc < 2) ||
			 (col && *text == ',' && nc < 3))
		{
			nc++;
			if (*text == ',')
				nc = 0;
		} else if (ctcp2)
		{
		} else
		{
			if (col)
				col = 0;
			switch (*text)
			{
			case '\003':			  /*ATTR_COLOR: */
				col = 1;
				nc = 0;
				break;
			case '\006':			  /* CTCP/2 attribute */
				ctcp2 = !ctcp2;
				break;
			case '\007':			  /*ATTR_BEEP: */
			case '\017':			  /*ATTR_RESET: */
			case '\026':			  /*ATTR_REVERSE: */
			case '\002':			  /*ATTR_BOLD: */
			case '\037':			  /*ATTR_UNDERLINE: */
				break;
			default:
				new_str[i] = *text;
				i++;
			}
		}
		text++;
		len--;
	}

	new_str[i] = 0;

	return new_str;
}


static int ay_irc_query_connected (eb_account * account)
{
	ay_irc_account * eia = NULL;

	if (!account) 
		return FALSE;

	eia = (ay_irc_account *) account->protocol_account_data;
	
	return ( eia->status != IRC_OFFLINE );
}


static void ay_irc_client_quit(irc_account *ia)
{
	ay_irc_logout((eb_local_account *)ia->data);
}


static void ay_irc_error(const char *msg, void *data)
{
	ay_do_error( _("IRC Error"), msg );
	ay_irc_logout((eb_local_account *)data);
}


static void ay_irc_warning(const char *msg, void *data)
{
	ay_do_warning( _("IRC Warning"), msg );
}


static void ay_irc_cancel_connect(void *data)
{

	eb_local_account *ela = (eb_local_account *)data;
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	
	ay_socket_cancel_async(ila->connect_tag);
	ila->activity_tag = 0 ;
	ay_irc_logout(ela);
}

static void ay_irc_connect_status(const char *msg, void *data)
{
	eb_local_account *ela = (eb_local_account *)data;


	irc_local_account *ila =
		(irc_local_account *)ela->protocol_local_account_data;

	if (!ila) 
		return;

	ay_activity_bar_update_label(ila->activity_tag, msg);
}


static void ay_irc_login( eb_local_account * account)
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;

	char buff[BUF_LEN];
	int tag;

	if ( account->connecting || account->connected )
		return ;

	account->connecting = 1 ;
	account->connected  = 0 ;

	/* Setup and connect */
	snprintf(buff, sizeof(buff), _("Logging in to IRC account: %s"), account->handle);
	ila->activity_tag = ay_activity_bar_add(buff, ay_irc_cancel_connect, account);

	/* default to port 6667 if nothing specified */
	if (ila->ia->port[0] == '\0' )
		strcpy(ila->ia->port, "6667") ;

	if ((tag = ay_connect_host(ila->ia->connect_address, atoi(ila->ia->port), irc_connect_cb, 
					account, (void *)ay_irc_connect_status)) < 0)
	{
		char buff[1024]; 
		snprintf(buff, sizeof(buff), _("Cannot connect to %s."), ila->ia->connect_address);
		ay_do_error( _("IRC Error"), buff );
		eb_debug(DBG_IRC, "%s\n", buff);
		ay_activity_bar_remove(ila->activity_tag);
		ila->activity_tag = 0;
		ay_irc_cancel_connect(account);
		return;
	}

	ila->connect_tag = tag;
}

static void irc_connect_cb(int fd, int error, void *data)
{

	eb_local_account *ela = (eb_local_account *)data;	
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;

	char buff[BUF_LEN];

	if(fd == -1 || error) {
		snprintf(buff, sizeof(buff), _("Cannot connect to %s."), ila->ia->connect_address);
		ay_do_error( _("IRC Error"), buff );
		eb_debug(DBG_IRC, "%s\n", buff);
		ay_activity_bar_remove(ila->activity_tag);
		ila->activity_tag = 0;
		ay_irc_cancel_connect(ela);
		return;
	}

	eb_debug(DBG_IRC, "Connected to IRC\n");

	/* Puzzle out the nick we're going to ask the server for */
	ila->ia->nick = strdup(ela->handle);
	ila->ia->user = strdup(ela->handle);

	if (ila->ia->nick == NULL)
		return;

	ila->ia->fd = fd;

	/* Set up callbacks and timeouts */

	ila->fd_tag = eb_input_add(ila->ia->fd, EB_INPUT_READ, irc_recv, ila->ia);

	// Login
	irc_login(ila->password, IRC_SIGNON_ONLINE, ila->ia);
	
	/* Claim us to be online */
	ela->connected = TRUE;
	ila->ia->status = IRC_ONLINE;
	ref_count++;

	is_setting_state = 1;
	if(ela->status_menu)
		eb_set_active_menu_status(ela->status_menu, IRC_ONLINE);

	is_setting_state = 0;
}


static void ay_irc_got_welcome ( const char *nick, const char *message,
				 irc_message_prefix *prefix, irc_account *ia)
{
	if (!strcmp(nick, ia->nick))
		return ;

	// Change my nickname if the server has truncated it.
	if ( ia->nick )
		free(ia->nick);

	ia->nick = strdup(nick);
}


static void ay_irc_got_myinfo ( const char *servername, const char *version, 
			irc_message_prefix *prefix, irc_account *ia )
{
	// TODO Log this message in the log window
	
	irc_finish_login(ia->data);
}


void irc_finish_login(eb_local_account *ela)
{
	irc_local_account *ila = (irc_local_account *)ela->protocol_local_account_data ;

	ay_activity_bar_remove(ila->activity_tag);
	ila->connect_tag = 0;
	ila->activity_tag = 0;

	ela->connecting = 0 ;
	ela->connected  = 1 ;
}

static void ay_irc_logout( eb_local_account * ela )
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;

	LList * node;
	eb_account * ea = NULL;
	ay_irc_account *eia = NULL;

	if ( !(ela->connected || ela->connecting) ) {
		return;
	}

	ela->connected = 0;
	ela->connecting= 0;

	/* Log off in a nice way */
	if(ila->fd_tag>0) {
		irc_logout(ila->ia);
		eb_input_remove(ila->fd_tag);
	}

	ay_activity_bar_remove(ila->activity_tag);
	ila->activity_tag = ila->connect_tag = 0;

	close(ila->ia->fd);

	ila->ia->fd = 0;
	ila->fd_tag = 0;
	ila->ia->status = IRC_OFFLINE;
	
	is_setting_state = 1;
	if(ela->status_menu)
		eb_set_active_menu_status(ela->status_menu, IRC_OFFLINE);
	is_setting_state = 0;

	/* Make sure relevant accounts get logged off */
	for( node = ila->friends; node; )
	{
		ea = (eb_account *)(node->data);
		eia = (ay_irc_account *)ea->protocol_account_data;

		eb_debug(DBG_IRC, "Logging off :: %s\n", ea->handle);

		if( eia->status != IRC_OFFLINE )
		{
			if( eia->dummy ) {
				ea->account_contact->online--;
				ea->online = FALSE;
				if (ea->account_contact->online == 0)
					ea->account_contact->group->contacts_online--;

				eb_debug(DBG_IRC, "Dummy logoff :: %s\n", ea->handle);
			}
			else {
				eb_debug(DBG_IRC, "Buddy logoff :: %s\n", ea->handle);
				buddy_logoff(ea);
			}

			eia->status = IRC_OFFLINE;
			buddy_update_status(ea);
			eia->realserver[0] = '\0';
		}

		if ( eia->dummy ) {
			struct contact * c = ea->account_contact;

			/* Stand back.... */
			node = node->prev;
			/* This one's gonna bloww!!! */
			ay_irc_del_user(ea);

			eb_debug(DBG_IRC, "Removed User :: %s\n", ea->handle);

			c->accounts = l_list_remove(c->accounts, ea);
			g_free(eia);
			eia = NULL;
			g_free(ea);
			ea = NULL;
		}

		// If node ended up as NULL then we must have deleted from the start of the list
		if(node)
			node = node->next ;
		else
			node = ila->friends ;
	}

	ref_count--;
}

static void ay_irc_send_im( eb_local_account * account_from,
                            eb_account * account_to,
                            char *message )
{
	irc_local_account * ila = (irc_local_account *) account_from->protocol_local_account_data;
	char *nick = NULL;
	char *alpha = NULL;

	if ( account_to->handle )
	{
		nick = strdup(account_to->handle);
		alpha = strchr(nick, '@');
		if (alpha == NULL) return;
		*alpha = '\0';

		irc_send_privmsg(nick, message, ila->ia);

		if(nick)
			free(nick);
	}
}

static void irc_init_account_prefs(eb_local_account *ela)
{

	irc_local_account *ila = ela->protocol_local_account_data;
	input_list *il = g_new0(input_list, 1);

	ela->prefs = il;

	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label= _("_Nick:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = ila->password;
	il->name = "PASSWORD";
	il->label= _("_Password:");
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &ela->connect_at_startup;
	il->name = "CONNECT";
	il->label= _("_Connect at startup");
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = ila->ia->connect_address;
	il->name = "irc_host";
	il->label= _("IRC _Host:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = ila->ia->port;
	il->name = "irc_port";
	il->label= _("IRC P_ort:");
	il->type = EB_INPUT_ENTRY;

}

static eb_local_account * ay_irc_read_local_config(LList * pairs)
{
	eb_local_account * ela = g_new0(eb_local_account, 1);
	irc_local_account * ila = g_new0(irc_local_account, 1);
	ila->ia = g_new0(irc_account, 1);

	ila->ia->callbacks = ay_irc_map_callbacks();

	char *temp = NULL;
	char *temp2 = NULL;
	ela->protocol_local_account_data = ila;
	ila->ia->status = IRC_OFFLINE;
	ila->ia->data = ela;

	temp = ela->handle;

	ela->service_id = SERVICE_INFO.protocol_id;

	irc_init_account_prefs(ela);
	eb_update_from_value_pair(ela->prefs, pairs);

	/* string magic - point to the first char after '@' */
	if ((temp = strrchr(ela->handle, '@')) != NULL)
	{
		*temp='\0';
		temp++;
		strncpy(ila->ia->connect_address, temp, strlen(temp));

		/* Remove the port from ila->ia->connect_address */
		temp2 = strrchr(ila->ia->connect_address, ':');
		if (temp2)
			*temp2 = '\0';

		/* string magic - point to the first char after ':' */
		if ((temp = strrchr(temp, ':')) != NULL)
		{
			temp++;
			strncpy(ila->ia->port, temp, sizeof(ila->ia->port)-1);
		}
	}

	strncpy(ela->alias, ela->handle, MAX_PREF_LEN);

	if(ela->handle[0] && ila->ia->connect_address && ila->ia->connect_address[0])
		return ela;
	else
		return NULL;
}

static LList * ay_irc_write_local_config( eb_local_account * account )
{
	return eb_input_to_value_pair(account->prefs);
}

static eb_account * ay_irc_read_config(eb_account *ea, LList *config)
{
	ay_irc_account * eia = g_new0(ay_irc_account, 1);
	char * temp;

		
	ea->protocol_account_data = eia;
	
	/* This func expects account names of the form Nick@server,
	   for example Knan@irc.midgardsormen.net */

	temp = strrchr(ea->handle, '@');
	if(temp)
		strncpy(eia->server, temp+1, sizeof(eia->server));
	
	eia->idle = 0;
	eia->status = IRC_OFFLINE;

	if(ea->ela) {
		irc_local_account * ila = ea->ela->protocol_local_account_data;

		if (!strcmp(ila->ia->connect_address, eia->server))
			ila->friends = l_list_append( ila->friends, ea );
	}
	return ea;
}

static LList * ay_irc_get_states()
{
	int i=0 ;
	LList  * states = NULL;

	for (;i<IRC_STATE_COUNT;i++) {
		states = l_list_append(states, irc_states[i]);
	}

	return states;
}

static char * ay_irc_check_login(const char * user, const char * pass)
{
	if (strrchr(user, '@') == NULL) {
		return strdup(_("No hostname found in your login (which should be in user@host form)."));
	}
	return NULL;
}


static int ay_irc_get_current_state(eb_local_account * account )
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;

	return ila->ia->status;
}

static void ay_irc_set_current_state(eb_local_account * account, int state )
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;

	/*
 	* If we are changing the selection in some_routine
	* don't let the corresponding set_current_state get called
	* again.
 	*
 	* ... thanks to the author of aim-toc.c for the trick :-)
 	*/
	if( is_setting_state )
		return;

	if ((ila->ia->status != IRC_OFFLINE) && state == IRC_OFFLINE)
		ay_irc_logout(account);
	else if ((ila->ia->status == IRC_OFFLINE) && state != IRC_OFFLINE)
		ay_irc_login(account);

	ila->ia->status = state;
}

static void ay_irc_add_user( eb_account * account )
{
	/* find the proper local account */
	ay_irc_account *eia = (ay_irc_account *) account->protocol_account_data;
	eb_local_account * ela = account->ela;

	if (ela == NULL) {
		eb_debug(DBG_MOD, "ela == NULL!\n");
		return;
	}
	
	if (ela->service_id == SERVICE_INFO.protocol_id)
	{
		irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

		if (!strcmp(ila->ia->connect_address, eia->server))
		{
			ila->friends = l_list_append( ila->friends, account );
		}
	}
}

static void ay_irc_del_user( eb_account * account )
{
	/* find the proper local account */
	ay_irc_account *eia = (ay_irc_account *) account->protocol_account_data;
	eb_local_account * ela = account->ela;

	if (ela == NULL) {
		eb_debug(DBG_MOD, "ela == NULL!\n");
		return;
	}
	
	if (ela->service_id == SERVICE_INFO.protocol_id)
	{
		irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;
		if (eia && eia->server && !strcmp(ila->ia->connect_address, eia->server))
		{
			ila->friends = l_list_remove( ila->friends, account );
		}
	}
}

static int ay_irc_is_suitable (eb_local_account *local, eb_account *remote)
{
	ay_irc_account *eia = NULL;
	irc_local_account *ila = NULL;
	
	if (!local || !remote)
		return FALSE;
	
	if (remote->ela == local)
		return TRUE;
		
	eia = (ay_irc_account *)remote->protocol_account_data; 
	ila = (irc_local_account *)local->protocol_local_account_data;
	
	if (!eia || !ila)
		return FALSE;
		
	if (!strcmp(eia->server, ila->ia->connect_address)) 
		return TRUE;
	
	return FALSE;
}

/* Create a new account */
static eb_account * ay_irc_new_account(eb_local_account *ela, const char * account )
{
	eb_account * ea = g_new0(eb_account, 1);
	ay_irc_account * eia = g_new0(ay_irc_account, 1);
	
	strncpy(ea->handle, account, 254);
	ea->ela = ela;
	ea->protocol_account_data = eia;
	ea->service_id = SERVICE_INFO.protocol_id;
	ea->list_item = NULL;
	ea->online = 0;
	ea->status = NULL;
	ea->pix = NULL;
	ea->icon_handler = -1;
	ea->status_handler = -1;
	ea->infowindow = NULL;

	eia->idle = 0;
	eia->status = IRC_OFFLINE;

	/* string magic - point to the first char after '@' */
	if (strrchr(account, '@') != NULL)
	{
		account = strrchr(account, '@') + 1;
		strncpy(eia->server, account, 254);
	}
	else
	{
		if (ela->service_id == SERVICE_INFO.protocol_id)
		{
			irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

			strncpy(eia->server, ila->ia->connect_address, 254);
			strncat(ea->handle, "@", 254-strlen(ea->handle));
			strncat(ea->handle, eia->server, 254-strlen(ea->handle));
		}
	}

	return ea;
}

static char * ay_irc_get_status_string( eb_account * account )
{
	ay_irc_account * eia = (ay_irc_account *) account->protocol_account_data;
	static char string[255];
	static char buf[255];
	
	strcpy(string, "");
	strcpy(buf, "");
	
	if(eia->idle >= 60)
	{
		int days, hours, minutes;
		
		minutes = eia->idle / 60;
		hours 	= minutes / 60;
		minutes = minutes % 60;
		days	= hours / 24;
		hours	= hours % 24;

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
	
	strncat(string, buf, 255);
	strncat(string, irc_states[eia->status], 255 - strlen(string));

	return string;
}

static const char ** ay_irc_get_status_pixmap( eb_account * account)
{
	ay_irc_account * eia;
	
	/*if (!pixmaps)
		irc_init_pixmaps();*/
	
	eia = account->protocol_account_data;
	
	if (eia->status == IRC_ONLINE)
		return irc_online_xpm;
	else
		return irc_away_xpm;
}

/* Not needed with IRC, the server detects our idleness */
static void ay_irc_set_idle(eb_local_account * account, int idle )
{
	return;
}

static void ay_irc_set_away( eb_local_account * account, char * message, int away)
{
	irc_local_account *ila = (irc_local_account *)account->protocol_local_account_data;
	char *out_msg = NULL ;

	if (!account->connected)
		return;

	if (message) {
		is_setting_state = 1;
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, IRC_AWAY);
		is_setting_state = 0;
		/* Actually set away */
		if(away)
			irc_set_away(message, ila->ia);
		else {
			LList *l;
			for(l=ila->current_rooms; l; l=l->next) {
				out_msg = ctcp_gen_extended_data_request(CTCP_ACTION, message);
				irc_send_privmsg(((eb_chat_room*)l->data)->room_name, out_msg, ila->ia);
			}
		}
	} else {
		is_setting_state = 1;
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, IRC_ONLINE);

		is_setting_state = 0;
		/* Unset away */
		irc_set_away(message, ila->ia);
	}
}


/* TODO */
static void ay_irc_send_file( eb_local_account * from, eb_account * to, char * file )
{
	return;
}

static void irc_info_update(info_window * iw)
{
	char message[BUF_LEN*4];
	char temp[BUF_LEN];
	char *alpha = NULL;
	unsigned char *freeme = NULL;
	irc_info *ii = (irc_info *)iw->info_data;

	eb_account * ea = ii->me;
	ay_irc_account * eia = (ay_irc_account *)ea->protocol_account_data;

	strncpy(temp, ea->handle, BUF_LEN);
	alpha = strchr(temp, '@');
	if (alpha != NULL) *alpha = '\0';

	if (!ii->fullmessage)
	{
		snprintf(message, sizeof(message), _("<b>User info for</b> %s<br>"), temp);
		snprintf(temp, sizeof(temp), _("<b>Server:</b> %s<br>"), 
				strlen(eia->realserver)>0 ? eia->realserver : eia->server);
		strncat(message, temp, sizeof(message)-strlen(message));
		snprintf(temp, sizeof(temp), _("<b>Idle time and online status:</b> %s<br>"), 
				ay_irc_get_status_string(ea));
		strncat(message, temp, sizeof(message)-strlen(message)-1);
		strncat(message, _("<b>Whois info:</b><br><br>"), sizeof(message)-strlen(message)-1);
	}
	else
	{
		strncpy(message, ii->fullmessage, sizeof(message)-1) ;
	}

	if (ii->whois_info != NULL)
	{
		freeme = strip_color((unsigned char *)ii->whois_info);
		strncat(message, (char *)freeme, sizeof(message)-strlen(message));

		if(freeme) {
			free(freeme);
			freeme= NULL;
		}
	}

	eb_info_window_clear(iw);

	if (ii->fullmessage)
	{
		if(ii->fullmessage)
			free(ii->fullmessage);

		ii->fullmessage = NULL;
	}
	ii->fullmessage = strdup(message);

	html_text_buffer_append(GTK_TEXT_VIEW(iw->info), ii->fullmessage,
			HTML_IGNORE_BACKGROUND|HTML_IGNORE_FOREGROUND);
	gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(iw->scrollwindow)),0);
}

static void irc_info_data_cleanup(info_window * iw)
{
	if (((irc_info *)iw->info_data)->whois_info)
		free (((irc_info *)(iw->info_data))->whois_info);

	if (((irc_info *)(iw->info_data))->fullmessage)
		free (((irc_info *)(iw->info_data))->fullmessage);

	if (iw->info_data)
		free (iw->info_data);

	iw->info_data = NULL;
}

static void ay_irc_get_info( eb_local_account * account_from, eb_account * account_to)
{
	char	*nick;
	char	*alpha;

	irc_local_account * ila =
		(irc_local_account *) account_from->protocol_local_account_data;

	ay_irc_account * eia = (ay_irc_account *)account_to->protocol_account_data;
		
	nick = strdup(account_to->handle);
	alpha = strchr(nick, '@');
	if (alpha != NULL) *alpha = '\0';

	irc_send_whois(eia->realserver, nick, ila->ia);

	/* Find the pointer to the info window */
	if(account_to->infowindow == NULL )
	{
		/* Create one */
		account_to->infowindow = eb_info_window_new(account_from, account_to);
		gtk_widget_show(account_to->infowindow->window);
	}

	account_to->infowindow->info_data = calloc(1, sizeof (irc_info));

	((irc_info *)(account_to->infowindow->info_data))->me = account_to;
	account_to->infowindow->cleanup = irc_info_data_cleanup;
}


// IRC Handler
static void ay_got_whoisuser (const char *nick, const char *user, const char *host, 
				const char *real_name, irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data ;
	char handle[MAX_PREF_LEN] ;
	char whois_info[BUF_LEN] ;

	memset(whois_info, 0, sizeof(whois_info));

	eb_account *ea = NULL;
	ay_irc_account *eia = NULL;

	AY_IRC_GET_HANDLE(handle, nick, ia->connect_address, MAX_PREF_LEN-1);

	snprintf(whois_info, sizeof(whois_info), 
			_("<i><b>User Name: </b></i> %s<br><i><b>Real Name: </b></i> %s<br>"), 
			user, real_name);

	ea = find_account_by_handle(handle, ela->service_id);

	if (ea)
		eia = (ay_irc_account *)ea->protocol_account_data ;
	else
	{
		eia = g_new0(ay_irc_account, 1);
		ea = g_new0(eb_account, 1);

		strncpy(ea->handle, handle, 255);
		ea->service_id = ela->service_id;
		eia->status = IRC_OFFLINE;
		strncpy(eia->server, ia->connect_address, 255);
		ea->protocol_account_data = eia;
		ea->ela = ela ;
		eia->dummy = TRUE ;
		add_dummy_contact(prefix->nick, ea);

		eb_debug(DBG_IRC, "Created Dummy user :: %s\n", handle);

		// set status of the dummy contact to online so that we can message them...
		ea->account_contact->online++;
		ea->online = TRUE;
		if (ea->account_contact->online == 1)
			ea->account_contact->group->contacts_online++;

		buddy_update_status(ea);

		eia->status = IRC_ONLINE;
	}

	strncpy(eia->realserver, host, sizeof(eia->realserver)-1) ;

	/* Find the pointer to the info window */
	if(ea->infowindow == NULL )
	{
		/* Create one */
		ea->infowindow = eb_info_window_new(ela, ea);
		gtk_widget_show(ea->infowindow->window);
	}

	if ( !ea->infowindow->info_data )
		ea->infowindow->info_data = calloc(1, sizeof (irc_info));

	((irc_info *)ea->infowindow->info_data)->whois_info = strdup(whois_info);;
	((irc_info *)ea->infowindow->info_data)->me = ea;
	ea->infowindow->cleanup = irc_info_data_cleanup;

	irc_info_update(ea->infowindow);
}


// IRC Handler
static void ay_got_whoisidle (const char *from, int since, const char *message, 
				irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data ;
	char handle[MAX_PREF_LEN] ;

	eb_account *ea ;
	ay_irc_account *eia ;

	AY_IRC_GET_HANDLE(handle, from, ia->connect_address, MAX_PREF_LEN-1);

	ea = find_account_by_handle(handle, ela->service_id);

	if (ea)
		eia = (ay_irc_account *)ea->protocol_account_data ;
	else
	{
		eia = g_new0(ay_irc_account, 1);
		ea = g_new0(eb_account, 1);

		strncpy(ea->handle, handle, 255);
		ea->service_id = ela->service_id;
		eia->status = IRC_OFFLINE;
		strncpy(eia->server, ia->connect_address, 255);
		ea->protocol_account_data = eia;
		ea->ela = ela ;
		eia->dummy = TRUE ;
		add_dummy_contact(prefix->nick, ea);

		eb_debug(DBG_IRC, "Created Dummy user :: %s\n", handle);

		// set status of the dummy contact to online so that we can message them...
		ea->account_contact->online++;
		ea->online = TRUE;
		if (ea->account_contact->online == 1)
			ea->account_contact->group->contacts_online++;

		eia->status = IRC_ONLINE;
	}

	/* Find the pointer to the info window */
	if(ea->infowindow == NULL )
	{
		/* Create one */
		ea->infowindow = eb_info_window_new(ela, ea);
		gtk_widget_show(ea->infowindow->window);
	}

	if ( !ea->infowindow->info_data )
		ea->infowindow->info_data = calloc(1, sizeof (irc_info));

	eia->idle = since ;

	buddy_update_status(ea);

	irc_info_update(ea->infowindow);
}


// IRC Handler
static void ay_got_whoisserver (const char *nick, const char *server, const char *info, 
				irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data ;
	char handle[MAX_PREF_LEN] ;
	char whois_info[BUF_LEN] ;

	memset(whois_info, 0, sizeof(whois_info));

	eb_account *ea ;
	ay_irc_account *eia ;

	AY_IRC_GET_HANDLE(handle, nick, ia->connect_address, MAX_PREF_LEN-1);

	ea = find_account_by_handle(handle, ela->service_id);

	if (ea)
		eia = (ay_irc_account *)ea->protocol_account_data ;
	else
	{
		eia = g_new0(ay_irc_account, 1);
		ea = g_new0(eb_account, 1);

		strncpy(ea->handle, handle, 255);
		ea->service_id = ela->service_id;
		eia->status = IRC_OFFLINE;
		strncpy(eia->server, ia->connect_address, 255);
		ea->protocol_account_data = eia;
		ea->ela = ela ;
		eia->dummy = TRUE ;
		add_dummy_contact(prefix->nick, ea);
		
		eb_debug(DBG_IRC, "Created Dummy user :: %s\n", handle);

		// set status of the dummy contact to online so that we can message them...
		ea->account_contact->online++;
		ea->online = TRUE;
		if (ea->account_contact->online == 1)
			ea->account_contact->group->contacts_online++;

		buddy_update_status(ea);

		eia->status = IRC_ONLINE;
	}

	/* Find the pointer to the info window */
	if(ea->infowindow == NULL )
	{
		/* Create one */
		ea->infowindow = eb_info_window_new(ela, ea);
		gtk_widget_show(ea->infowindow->window);
	}

	if ( !ea->infowindow->info_data )
		ea->infowindow->info_data = calloc(1, sizeof (irc_info));

	snprintf(whois_info, sizeof(whois_info), _("<i><b>Server Info: </b></i> %s<br>"), info);

	strncpy(eia->realserver, server, sizeof(eia->realserver)-1) ;

	((irc_info *)ea->infowindow->info_data)->whois_info = strdup(whois_info);;

	irc_info_update(ea->infowindow);
}




// IRC Handler
static void ay_got_away (const char *from, const char *message,
				irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data ;
	char handle[MAX_PREF_LEN] ;
	char whois_info[BUF_LEN] ;

	memset(whois_info, 0, sizeof(whois_info));

	eb_account *ea ;
	ay_irc_account *eia ;

	AY_IRC_GET_HANDLE(handle, from, ia->connect_address, MAX_PREF_LEN-1);

	ea = find_account_by_handle(handle, ela->service_id);

	eia = (ay_irc_account *)ea->protocol_account_data ;

	if (eia->status == IRC_OFFLINE) {
		buddy_login(ea);
		eia->status = IRC_AWAY;
		buddy_update_status(ea);
	}
	else if (eia->status == IRC_ONLINE) {
		eia->status = IRC_AWAY;
		buddy_update_status(ea);
	}

	if (ea->infowindow)
		irc_info_update(ea->infowindow);
}


// IRC Handler
static void ay_buddy_join (const char *channel, irc_message_prefix *prefix, irc_account *ia)
{
	char room_name[BUF_LEN] ;
	char buddy_name[BUF_LEN] ;
	eb_chat_room *ecr ;
	eb_account *ea ;
	ay_irc_account *eia;
	eb_local_account *ela = (eb_local_account *)ia->data;

	snprintf(room_name, sizeof(room_name), "%s@%s", channel, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	/* If the JOIN notification was generated by me, then ask for NAMES */
	if ( prefix->nick && !strcmp(prefix->nick, ia->nick) ) {

		if(!ecr)
			ecr = ay_irc_make_chat_room_window(room_name, ela, FALSE, FALSE);

		irc_get_names_list (channel, ia) ;
	}

	snprintf(buddy_name, sizeof(buddy_name), "%s@%s", prefix->nick, ia->connect_address);

	/* See if there's anyone we recognize */
	ea = find_account_with_ela(buddy_name, ela);

	if (ea) {
		eb_debug(DBG_IRC, "Logged in JOINed user :: %s\n", buddy_name);

		eia = (ay_irc_account *)ea->protocol_account_data;

		if ( eia->status == IRC_OFFLINE && !eia->dummy ) {

			/* Okay, if someone msgs us, they are per definition online... */
			buddy_login(ea);
		}

		buddy_update_status(ea);
		eia->status = IRC_ONLINE;
	}

	if(ecr) {
		eb_chat_room_buddy_arrive(ecr, prefix->nick, prefix->nick);
	}
}


// IRC handler
static void ay_got_namereply (irc_name_list *list, const char *channel,
				irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	eb_account *ea ;
	eb_local_account *ela = (eb_local_account *)ia->data;
	ay_irc_account *eia;

	char room_name[BUF_LEN] ;
	char buddy_name[BUF_LEN] ;

	snprintf(room_name, sizeof(room_name), "%s@%s", channel, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	if(ecr) {
		eb_chat_room_buddy * ecrb = NULL;
		
		while(list) {
			if(!eb_chat_room_buddy_connected(ecr, list->name)) {
				ecrb = g_new0(eb_chat_room_buddy, 1 );
				strncpy( ecrb->alias, list->name, sizeof(ecrb->alias));
				strncpy( ecrb->handle, list->name, sizeof(ecrb->handle));
				ecr->fellows = l_list_append(ecr->fellows, ecrb);

				eb_chat_room_refresh_list(ecr, list->name, CHAT_ROOM_JOIN);

				snprintf(buddy_name, sizeof(buddy_name), "%s@%s", list->name, ia->connect_address);

				/* See if there's anyone we recognize */
				ea = find_account_with_ela(buddy_name, ela);

				if (ea) {
					eb_debug(DBG_IRC, "Logged in NAMEREPLY user :: %s\n", buddy_name);

					eia = (ay_irc_account *)ea->protocol_account_data;
                        
					if ( eia->status == IRC_OFFLINE ) {
                        
						/* Okay, if someone msgs us, they are per definition online... */
						buddy_login(ea);
					}
                        
					buddy_update_status(ea);
					eia->status = IRC_ONLINE;
				}
			}

			list = list -> next ;
		}
	}
}


// IRC Handler
static void ay_got_channel_list (const char *me, const char *channel, 
				int users, const char *topic,
				irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia -> data ;
	irc_local_account *ila = (irc_local_account *)ela-> protocol_local_account_data ;

	char *chnl = strdup(channel);

	ila->channel_list = l_list_insert_sorted(ila->channel_list, chnl,
						(LListCompFunc) strcasecmp);
}


static void ay_got_channel_listend (const char *message, irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data;
        irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	
	ila->got_public_chatrooms (l_list_copy(ila->channel_list), ila->public_chatroom_callback_data);

	l_list_free(ila->channel_list);

	ila->channel_list = NULL;
}


// IRC Handler
static void ay_buddy_quit (const char *message, irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	LList *node = chat_rooms ;
	char buddy_name[BUF_LEN];

	eb_account *ea ;
	eb_local_account *ela = (eb_local_account *)ia->data;
	ay_irc_account *eia;


	for(node = chat_rooms; node; node = node->next) {
		ecr = (eb_chat_room*)node->data;
		if (ecr && ecr->local_user->service_id == SERVICE_INFO.protocol_id ) {
			char ** buff3 = g_strsplit(ecr->id, "@", 2);

			if (!strcmp(buff3[1], ia->connect_address) 
					&& eb_chat_room_buddy_connected(ecr, prefix->nick))
			{
				snprintf(buddy_name, sizeof(buddy_name), "%s@%s", prefix->nick, ia->connect_address);
                        
				/* See if someone we recognize is going */
				ea = find_account_with_ela(buddy_name, ela);
                        
				if (ea) {
					eb_debug(DBG_IRC, "Logged off QUITed user :: %s\n", buddy_name);
                        
					eia = (ay_irc_account *)ea->protocol_account_data;
                        
					if ( eia->status == !IRC_OFFLINE ) {
						if( eia->dummy ) {
							ea->account_contact->online--;
							ea->online = FALSE;
							if (ea->account_contact->online == 0)
								ea->account_contact->group->contacts_online--;
                        
							eb_debug(DBG_IRC, "Dummy logoff :: %s\n", ea->handle);
						}
						else {
							eb_debug(DBG_IRC, "Buddy logoff :: %s\n", ea->handle);
							buddy_logoff(ea);
						}
					}
                        
					buddy_update_status(ea);
					eia->status = IRC_OFFLINE;
				}

				eb_chat_room_buddy_leave(ecr, prefix->nick);
			}


			if(buff3) {
				g_strfreev(buff3);
			}
		}
	}
}


// IRC Handler
static void ay_buddy_part (const char *channel, irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	char room_name[BUF_LEN] ;

	snprintf(room_name, sizeof(room_name), "%s@%s", channel, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	if(ecr) {
		eb_chat_room_buddy_leave(ecr, prefix->nick);

		// This means we sent a /PART or /LEAVE command
		if ( !strcmp(prefix->nick, ia->nick) ) {
			eb_destroy_chat_room(ecr);
		}
	}
}


// IRC Handler
static void ay_buddy_nick_change (const char *newnick, irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	char room_name[BUF_LEN];

	LList *node = chat_rooms ;

	snprintf(room_name, sizeof(room_name), "#notices-%s-%s@%s", 
			ia->nick, ia->connect_address, ia->connect_address);

	for(node = chat_rooms; node; node = node->next) {
		ecr = (eb_chat_room*)node->data;

		if (ecr && ecr->local_user->service_id == SERVICE_INFO.protocol_id ) {
			char ** buff3 = g_strsplit(ecr->id, "@", 2);

			// If it's me I've got to change my name too eh...
			if(!strcmp(ia->nick, prefix->nick)) {
				char colorized_msg[BUF_LEN];

				snprintf(colorized_msg,BUF_LEN, 
						"<font color=#008888><b>Your nickname is now %s</b></font>",
						newnick);

				eb_chat_room_show_3rdperson(ecr, colorized_msg);
				strcpy(ia->nick, newnick);

				// Rename my notices channel
				if(!strcmp(ecr->id, room_name)) {
					char *tmp=NULL;

					snprintf(room_name, sizeof(room_name), "#notices-%s-%s@%s", 
						ia->nick, ia->connect_address, ia->connect_address);

					strncpy(ecr->id, room_name, sizeof(ecr->id));
					tmp=strchr(room_name, '@');
					if(tmp)
						*tmp='\0';

					strncpy(ecr->room_name, room_name, sizeof(ecr->room_name));
				}
			}

			if (!strcmp(buff3[1], ia->connect_address) 
					&& eb_chat_room_buddy_connected(ecr, prefix->nick))
			{
				eb_chat_room_buddy_leave(ecr, prefix->nick);
				eb_chat_room_buddy_arrive(ecr, newnick, newnick);
			}

			if(buff3) {
				g_strfreev(buff3);
			}
		}
	}
}


// IRC Handler
static void ay_got_invite (const char *to, const char *channel, 
		irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	char room_name[BUF_LEN] ;

	eb_local_account *ela = (eb_local_account *)ia->data ;

	snprintf(room_name, sizeof(room_name), "%s@%s", channel, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	if(!ecr) {
		invite_dialog(ela, prefix->nick, strdup(room_name), strdup(room_name));
	}
}


void ay_irc_process_incoming_message (const char *recipient, const char *message, 
					irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data;
	irc_local_account *ila = (irc_local_account *)ela->protocol_local_account_data;
	ay_irc_account *eia = NULL ;
	char *msg = NULL;

	/* This was a personal message... for my eyes only */
	if(!strcmp(recipient, ia->nick)) {
		char sender_handle[BUF_LEN];
		eb_account *ea;

		if (prefix->nick) {
			AY_IRC_GET_HANDLE(sender_handle, prefix->nick, ia->connect_address,MAX_PREF_LEN-1);
		}
		else {
			AY_IRC_GET_HANDLE(sender_handle, prefix->servername, ia->connect_address,MAX_PREF_LEN-1);
		}

		ea = find_account_with_ela(sender_handle, ela);

		if (!ea) {
			/* Some unknown person is msging us - add him */
			ay_irc_account * eia = g_new0(ay_irc_account, 1);

			ea = g_new0(eb_account, 1);
			strncpy(ea->handle, sender_handle, 255);
			ea->service_id = ela->service_id;
			eia->status = IRC_OFFLINE;
			strncpy(eia->server, ia->connect_address, 255);
			ea->protocol_account_data = eia;
			ea->ela = ela ;
			eia->dummy = TRUE ;
			add_dummy_contact(prefix->nick, ea);

			eb_debug(DBG_IRC, "Created Dummy user :: %s\n", ea->handle);

			// add to friends list so that we can sign them off on logoff
			ila->friends = l_list_append( ila->friends, ea );
		}
		else if (!ea->ela)
			ea->ela = ela ;

		eia = (ay_irc_account *)ea->protocol_account_data;

		if ( eia->status == IRC_OFFLINE && !eia->dummy ) {
			eb_debug(DBG_IRC, "Logging in user :: %s\n", ea->handle);

			/* Okay, if someone msgs us, they are per definition online... */
			buddy_login(ea);
		}
		else if ( eia->status == IRC_OFFLINE ) {
			eb_debug(DBG_IRC, "Logging in dummy user :: %s\n", ea->handle);
			// set status of the dummy contact to online so that we can message them...
			ea->account_contact->online++;
			ea->online = TRUE;
			if (ea->account_contact->online == 1)
				ea->account_contact->group->contacts_online++;
		}

		buddy_update_status(ea);
		eia->status = IRC_ONLINE;

		if (message) {
			msg = (char *)strip_color((unsigned char *)message);
		}
		else {
			msg = strdup("");
		}

		eb_parse_incoming_message(ela, ea, msg);
	}
	else {
		char *out_msg = NULL ;
		char room_name[BUF_LEN] ;
		eb_chat_room *ecr = NULL ;

		snprintf(room_name, sizeof(room_name), "%s@%s", recipient, ia->connect_address);

		ecr = find_chat_room_by_id(room_name);

		if (ecr) {
			msg = (char *)strip_color((unsigned char *)message);

			/* Highlight my nickname if someone has mentioned it */
			if (!strncmp(msg, ia->nick, strlen(ia->nick))) {
				out_msg = g_strdup_printf("<font color=\"#ff0000\">%s</font> %s",
						ia->nick, msg+strlen(ia->nick));

				eb_chat_room_show_message(ecr, prefix->nick, out_msg);

				if(out_msg) {
					free(out_msg);
					out_msg = NULL;
				}
			}
			else {
				eb_chat_room_show_message(ecr, prefix->nick, msg);
			}
		}
	}

	if(msg) {
		free(msg);
		msg = NULL;
	}
}


// IRC Handler
static void ay_irc_got_privmsg (const char *recipient, const char *message, 
			irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	char room_name[BUF_LEN] ;
	ctcp_extended_data_list *data_list = NULL ;
	ctcp_extended_data *element = NULL ;

	snprintf(room_name, sizeof(room_name), "%s@%s", recipient, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	/* There's either a chatroom by the name or it was meant personally for me */
	if(ecr || !strcmp(recipient, ia->nick)) {
		data_list = ctcp_get_extended_data (message, strlen(message)) ;

		while (data_list) {
			element = (ctcp_extended_data *)data_list->ext_data;

			switch (element->type) {
				case CTCP_ACTION:
				{
					char colorized_message[BUF_LEN] ;
					unsigned char *msg = strip_color((unsigned char *)element->data);

					g_snprintf(colorized_message, BUF_LEN,  
							"<font color=\"#00AA00\">*%s %s</font>",
							prefix->nick, msg);

					if (ecr)
						eb_chat_room_show_3rdperson(ecr, colorized_message);
					else {
						ay_irc_process_incoming_message (recipient, 
								colorized_message, prefix, ia) ;
					}

					if(msg) {
						free(msg);
						msg = NULL;
					}

					break;
				}
				case CTCP_VERSION:
				{
					/* Send Ayttm version info */
					char *msg = ctcp_gen_version_response
							(PACKAGE_NAME,  PACKAGE_VERSION "-" RELEASE, HOST);

					irc_send_notice(prefix->nick, msg, ia);
					break;
				}
				case CTCP_SOURCE:
					break;
				case CTCP_USERINFO:
					break;
				case CTCP_CLIENTINFO:
					break;
				case CTCP_TIME:
				{
					/* Send Current Time */
					char *msg = ctcp_gen_time_response();
					irc_send_notice(prefix->nick, msg, ia);
					break;
				}
				case CTCP_PING:
				{
					/* Send CTCP PING response */
					char *msg = ctcp_gen_ping_response(element->data);
					irc_send_notice(prefix->nick, msg, ia);
					break;
				}
				case CTCP_SED:
					break;
				case CTCP_DCC:
					break;
				case CTCP_FINGER:
					break;
				case CTCP_ERRMSG:
					break;
				default:
				{
					ay_irc_process_incoming_message (recipient, 
							element->data, prefix, ia) ;
				}
			}

			data_list = data_list->next ;
		}
	}
}


/* Got the channel Topic */
static void ay_irc_got_topic (const char *channel, const char *topic,
			irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr ;
	char room_name[BUF_LEN] ;

	snprintf(room_name, sizeof(room_name), "%s@%s", channel, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	if(ecr) {
		char colorized_message[BUF_LEN] ;
		unsigned char *msg = strip_color((unsigned char *)topic);

		g_snprintf(colorized_message, BUF_LEN, "<font color=\"#775500\">%s</font>", msg);

		eb_chat_room_show_3rdperson(ecr, colorized_message);

		if(msg) {
			free(msg);
			msg = NULL;
		}
	}
}


// IRC Handler
static void ay_irc_got_notice (const char *recipient, const char *message, 
			irc_message_prefix *prefix, irc_account *ia)
{
	char room_name[BUF_LEN];
	ctcp_extended_data_list *data_list = NULL ;
	ctcp_extended_data *element = NULL ;
	eb_local_account *ela = (eb_local_account *)ia->data;
	eb_chat_room *cr = NULL;

	/* 
	 * This is one of those AUTH NOTICEs... 
	 * TODO: Put these in the message log 
	 */
	if (!prefix->nick && !prefix->servername)
		return ;

	snprintf(room_name, sizeof(room_name), "#notices-%s-%s@%s", 
			recipient, ia->connect_address, ia->connect_address);

	cr = find_chat_room_by_id(room_name);

	if(!cr) {
		cr = ay_irc_make_chat_room_window(room_name, ela, FALSE, FALSE);
	}

	data_list = ctcp_get_extended_data (message, strlen(message)) ;

	while (data_list) {
		element = (ctcp_extended_data *)data_list->ext_data;

		switch (element->type) {
			case CTCP_VERSION:
			{
				char notice[BUF_LEN] ;
				ctcp_version *version = ctcp_got_version(element->data);

				if(version && version->name) {
					snprintf(notice, sizeof(notice), _("<font color=\"#00BBBB\">%s is running %s"), 
							(prefix->nick?prefix->nick:prefix->servername), 
							version->name) ;
					if (version->version) {
						strncat(notice, "-", sizeof(notice)-strlen(notice)-1);
						strncat(notice, version->version, sizeof(notice)-strlen(notice)-1);
					}

					if (version->env) {
						strncat(notice, _(" on "), sizeof(notice)-strlen(notice)-1) ;
						strncat(notice, version->env, sizeof(notice)-strlen(notice)-1);
					}
					strncat(notice, "</font>", sizeof(notice)-strlen(notice)-1);
				}

				if(element->data) {
					free(element->data);
					element->data = NULL;
				}

				element->data = strdup(notice);

				break;
			}
			case CTCP_SOURCE:
				break;
			case CTCP_USERINFO:
				break;
			case CTCP_CLIENTINFO:
				break;
			case CTCP_TIME:
			{
				char notice[BUF_LEN] ;

				snprintf(notice, sizeof(notice), 
						_("<font color=\"#AABB44\">%s has sent Time as <B>%s</B></font>"), 
						(prefix->nick?prefix->nick:prefix->servername), element->data);

				if(element->data) {
					free(element->data);
					element->data = NULL;
				}

				element->data = strdup(notice);

				break;
			}
			case CTCP_PING:
				break;
			case CTCP_SED:
				break;
			case CTCP_DCC:
				break;
			case CTCP_FINGER:
				break;
			case CTCP_ERRMSG:
				break;
			default:
			{
				char notice[BUF_LEN] ;

				snprintf(notice, sizeof(notice), 
						_("<I><B><font color=\"#AA0000\">%s:</font></B> %s</I>"), 
						(prefix->nick?prefix->nick:prefix->servername), element->data);

				if(element->data) {
					free(element->data);
					element->data = NULL;
				}

				element->data = strdup(notice);
			}
		}


		if (element->data && element->data[0]) {
			if(prefix->nick) {
				free(prefix->nick);
				prefix->nick = strdup(room_name);
			}
			else if(prefix->servername) {
				free(prefix->servername);
				prefix->servername = strdup(room_name);
			}

			// We use chat room windows instead of normal chat windows since we want
			// to be able to type commands in them.
			eb_chat_room_show_3rdperson(cr, element->data);
//			ay_irc_process_incoming_message(recipient, element->data, prefix, ia);
		}

		data_list = data_list->next ;
	}

	ctcp_free_extended_data(data_list);
}


static void ay_irc_join_chat_room(eb_chat_room *room)
{
	char room_name[BUF_LEN];

	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	if (!room->local_user->connected)
		return;

	// UGLY HACK ALERT!! I'm explicitly checking for the notice window room name.
	snprintf(room_name, sizeof(room_name), "#notices-%s-%s", ila->ia->nick, ila->ia->connect_address);

	if(strcasecmp(room->room_name, room_name))
		irc_join(room->room_name, ila->ia) ;

	ila->current_rooms = l_list_prepend(ila->current_rooms, room);
}

static void ay_irc_leave_chat_room(eb_chat_room *room)
{
	char room_name[BUF_LEN];

	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	if (!room->local_user->connected)
		return;

	// UGLY HACK ALERT!! I'm explicitly checking for the notice window room name.
	snprintf(room_name, sizeof(room_name), "#notices-%s-%s", ila->ia->nick, ila->ia->connect_address);

	if(strcasecmp(room->room_name, room_name))
		irc_leave_chat_room(room->room_name, ila->ia) ;

	ila->current_rooms = l_list_remove(ila->current_rooms, room);
}

static void ay_irc_send_chat_room_message(eb_chat_room *room, char *message)
{
	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	if (message)
		irc_send_privmsg(room->room_name, message, ila->ia);

	eb_chat_room_show_message( room, ila->ia->nick, message );
}

static void ay_irc_send_invite( eb_local_account * account, eb_chat_room * room,
				char * user, const char * message)
{
	char buff[BUF_LEN];
	char *simple_user = strdup(user);
	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	if (!room->local_user->connected)
		return;

	if (strstr(simple_user, "@"))
		*(strstr(simple_user, "@")) = '\0';

	irc_send_invite(simple_user, room->room_name, message, ila->ia);

	free(simple_user);
	
	if (*message) {
		g_snprintf(buff, BUF_LEN, _(">>> Inviting %s [%s] <<<"), user, message);
	} else {
		g_snprintf(buff, BUF_LEN, _(">>> Inviting %s <<<"), user);
	}
		
	eb_chat_room_show_message( room, room->local_user->alias, buff);
}


static eb_chat_room *ay_irc_make_chat_room_window(char *name, eb_local_account *account, int is_public, int send_join)
{
	eb_chat_room * ecr;

	int name_len = 0 ;
	char *alpha = NULL;
	char *channelname = NULL;
	
	name_len = strlen(name)+100;

	channelname = g_new0(char, name_len);

	/* according to RFC2812, channels can be marked by #, &, + or !, not only the conventional #. */
	if ((*name != '#') && (*name != '&') && (*name != '+') && (*name != '!')) {
		strcpy(channelname, "#");
	}

	strncat(channelname, name, name_len);

	if (strrchr(channelname, '@') == NULL && account->service_id == SERVICE_INFO.protocol_id)
	{
		irc_local_account * ila = (irc_local_account *)account->protocol_local_account_data;

		strncat(channelname, "@", name_len-strlen(channelname));
		strncat(channelname, ila->ia->connect_address, name_len-strlen(channelname));
	}

	g_strdown(channelname);
	ecr = find_chat_room_by_id(channelname);

	if( ecr )
	{
		g_free(channelname);
		return ecr;
	}

	ecr = g_new0(eb_chat_room, 1);

	strncpy(ecr->id, channelname, sizeof(ecr->id));
		
	alpha = strchr(channelname, '@');
	if (alpha != NULL) *alpha = '\0';

	strncpy(ecr->room_name, channelname, sizeof(ecr->room_name));

	ecr->connected = 0;
	ecr->local_user = account;

	eb_join_chat_room(ecr, send_join);

	g_free(channelname);

	return ecr;
}


static eb_chat_room * ay_irc_make_chat_room(char * name, eb_local_account * account, int is_public)
{
	return ay_irc_make_chat_room_window(name, account, is_public, TRUE);
}


static void ay_irc_accept_invite( eb_local_account * account, void * invitation )
{
	eb_chat_room * ecr = ay_irc_make_chat_room((char *) invitation, account, FALSE);
	if(invitation) {
		free(invitation);
		invitation = NULL ;
	}

	if ( ecr )
	{
		//eb_join_chat_room(ecr);
	}
	return;
}

static void ay_irc_decline_invite( eb_local_account * account, void * invitation )
{
	if(invitation) {
		free(invitation);
		invitation = NULL;
	}

	return;
}

static void eb_irc_read_prefs_config(LList * values)
{
	char *c;

	c = value_pair_get_value(values, "do_irc_debug");

	if(c) {
		do_irc_debug=atoi(c);
		free(c);
	}

	return;
}

static LList * eb_irc_write_prefs_config()
{
	LList * config = NULL;
	char buffer[5];

	sprintf(buffer, "%d", do_irc_debug);
	config = value_pair_add(config, "do_irc_debug", buffer);

	return config;
}

static void eb_irc_get_public_chatrooms(eb_local_account *ela, 
		void (*public_chatroom_callback) (LList *list, void *data),
		void *data )
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	
	ila->got_public_chatrooms = public_chatroom_callback;
	ila->public_chatroom_callback_data = data;

	irc_request_list(NULL, NULL, ila->ia);
}


static void ay_got_motd(const char *motd, irc_message_prefix *prefix, irc_account *ia)
{
	eb_chat_room *ecr = NULL ;
	char room_name[BUF_LEN] ;
	char colorized_message[BUF_LEN] ;
	eb_local_account *ela = (eb_local_account *)ia->data;

	unsigned char *msg = strip_color((unsigned char *)motd);

	snprintf(room_name, sizeof(room_name), "#notices-%s-%s@%s", ia->nick, ia->connect_address, ia->connect_address);

	ecr = find_chat_room_by_id(room_name);

	if(!ecr) {
		ecr = ay_irc_make_chat_room_window(room_name, ela, FALSE, FALSE);
	}

	g_snprintf(colorized_message, BUF_LEN, "<font color=\"#AA77AA\">%s</font>", msg);

	eb_chat_room_show_3rdperson(ecr, colorized_message);

	if(msg) {
		free(msg);
		msg = NULL;
	}
}


/* *************** *
 * Error Responses *
 * *************** */


static void ay_irc_nick_in_use(const char *nick, const char *message, 
		irc_message_prefix *prefix, irc_account *ia)
{
	char msg [ BUF_LEN ];
	eb_local_account *ela = (eb_local_account *)ia->data;

	strncpy(msg, nick, BUF_LEN - 1);
	strncat(msg, ": ", BUF_LEN - 1 - strlen(msg));
	strncat(msg, message, BUF_LEN - 1 - strlen(msg));

	ay_irc_logout(ela);
	ay_do_error(_("Nickname in Use"), msg);
}


static void ay_irc_no_such_server(const char *server, const char *message, 
		irc_message_prefix *prefix, irc_account *ia)
{
	char msg [ BUF_LEN ];

	strncpy(msg, server, BUF_LEN - 1);
	strncat(msg, ": ", BUF_LEN - 1 - strlen(msg));
	strncat(msg, message, BUF_LEN - 1 - strlen(msg));

	ay_do_warning(_("No such Server"), msg);
}


static void ay_irc_no_such_channel(const char *channel, const char *message, 
		irc_message_prefix *prefix, irc_account *ia)
{
	char msg [ BUF_LEN ];

	strncpy(msg, channel, BUF_LEN - 1);
	strncat(msg, ": ", BUF_LEN - 1 - strlen(msg));
	strncat(msg, message, BUF_LEN - 1 - strlen(msg));

	ay_do_warning(_("No such Channel"), msg);
}


static void ay_irc_no_such_nick(const char *nick, const char *message, 
		irc_message_prefix *prefix, irc_account *ia)
{
	char msg [ BUF_LEN ];

	strncpy(msg, nick, BUF_LEN - 1);
	strncat(msg, ": ", BUF_LEN - 1 - strlen(msg));
	strncat(msg, message, BUF_LEN - 1 - strlen(msg));

	ay_do_warning(_("No such Nickname"), msg);
}


static void ay_irc_password_mismatch(const char *message, irc_message_prefix *prefix, irc_account *ia)
{
	eb_local_account *ela = (eb_local_account *)ia->data;

	ay_irc_logout(ela);
	ay_do_error(_("Nickname in Use"), message);
}


/* ********************************************** *
 * Attach Service Callbacks and IRC Callbacks     *
 * ********************************************** */

struct service_callbacks * query_callbacks(void)
{
	struct service_callbacks * sc;

	sc = g_new0( struct service_callbacks, 1 );
	/* Done */ sc->query_connected = ay_irc_query_connected;
	/* Done */ sc->login = ay_irc_login;
	/* Done */ sc->logout = ay_irc_logout;
	/* Done */ sc->send_im = ay_irc_send_im;
	/* Done */ sc->read_local_account_config = ay_irc_read_local_config;
	/* Done */ sc->write_local_config = ay_irc_write_local_config;
	/* Done */ sc->read_account_config = ay_irc_read_config;
	/* Done */ sc->get_states = ay_irc_get_states;
	/* Done */ sc->get_current_state = ay_irc_get_current_state;
	/* Done */ sc->set_current_state = ay_irc_set_current_state;
	/* Done */ sc->check_login = ay_irc_check_login;
	/* Done */ sc->add_user = ay_irc_add_user;
	/* Done */ sc->del_user = ay_irc_del_user;
	/* Done */ sc->is_suitable = ay_irc_is_suitable;
	/* Done */ sc->new_account = ay_irc_new_account;
	/* Done */ sc->get_status_string = ay_irc_get_status_string;
	/* Done */ sc->get_status_pixmap = ay_irc_get_status_pixmap;
	/* Done */ sc->set_idle = ay_irc_set_idle;
	/* Done */ sc->set_away = ay_irc_set_away;
	sc->send_file = ay_irc_send_file;
	sc->get_info = ay_irc_get_info;
	
	/* Done */ sc->make_chat_room = ay_irc_make_chat_room;
	/* Done */ sc->send_chat_room_message = ay_irc_send_chat_room_message;
	/* Done */ sc->join_chat_room = ay_irc_join_chat_room;
	/* Done */ sc->leave_chat_room = ay_irc_leave_chat_room;
	/* Done */ sc->send_invite = ay_irc_send_invite;
	/* Done */ sc->accept_invite = ay_irc_accept_invite;
	/* Done */ sc->decline_invite = ay_irc_decline_invite;
	
	/* Done */ sc->read_prefs_config = eb_irc_read_prefs_config;
	/* Done */ sc->write_prefs_config = eb_irc_write_prefs_config;
	sc->add_importers = NULL;

	sc->get_color=eb_irc_get_color;
	sc->get_smileys=eb_default_smileys;
	sc->get_public_chatrooms = eb_irc_get_public_chatrooms;
	
	return sc;
}


static irc_callbacks *ay_irc_map_callbacks( void )
{
	irc_callbacks *cb = g_new0(irc_callbacks, 1);

	/* Only these are implemented so far. More to come... */
	cb->got_welcome      		= ay_irc_got_welcome ;
	cb->incoming_notice		= ay_irc_got_notice ;
	cb->buddy_quit			= ay_buddy_quit ;
	cb->buddy_join			= ay_buddy_join ;
	cb->buddy_part			= ay_buddy_part ;
	cb->got_invite			= ay_got_invite ;
	cb->nick_change			= ay_buddy_nick_change ;
	cb->got_privmsg			= ay_irc_got_privmsg ;
	cb->got_topic			= ay_irc_got_topic ;
	cb->got_myinfo       		= ay_irc_got_myinfo ;
	cb->got_away         		= ay_got_away ;
	cb->got_whoisidle		= ay_got_whoisidle ;
	cb->got_whoisuser		= ay_got_whoisuser ;
	cb->got_whoisserver		= ay_got_whoisserver ;
	cb->got_channel_list		= ay_got_channel_list ;
	cb->got_channel_listend		= ay_got_channel_listend ;
	cb->got_namelist		= ay_got_namereply ;
	cb->irc_error			= ay_irc_error ;
	cb->irc_warning			= ay_irc_warning ;
	cb->client_quit			= ay_irc_client_quit ;
	cb->got_motd         		= ay_got_motd ;
                                
	/* Errors */
	cb->irc_no_such_nick		= ay_irc_no_such_nick ;
	cb->irc_no_such_server		= ay_irc_no_such_server ;
	cb->irc_no_such_channel		= ay_irc_no_such_channel ;
	cb->irc_nick_in_use		= ay_irc_nick_in_use ;
	cb->irc_password_mismatch	= ay_irc_password_mismatch ;
/*	cb->irc_cant_sendto_chnl	= ay_irc_cant_sendto_chnl ;
	cb->irc_was_no_such_nick	= ay_irc_was_no_such_nick ;
	cb->irc_too_many_chnl		= ay_irc_too_many_chnl ;
	cb->irc_too_many_targets	= ay_irc_too_many_targets ;
	cb->irc_no_origin		= ay_irc_no_origin ;
	cb->irc_no_recipient		= ay_irc_no_recipient ;
	cb->irc_no_text_to_send		= ay_irc_no_text_to_send ;
	cb->irc_no_toplevel		= ay_irc_no_toplevel ;
	cb->irc_wild_toplevel		= ay_irc_wild_toplevel ;
	cb->irc_unknown_cmd		= ay_irc_unknown_cmd ;
	cb->irc_no_motd			= ay_irc_no_motd ;
	cb->irc_no_admininfo		= ay_irc_no_admininfo ;
	cb->irc_file_error		= ay_irc_file_error ;
	cb->irc_no_nick_given		= ay_irc_no_nick_given ;
	cb->irc_erroneous_nick		= ay_irc_erroneous_nick ;
	cb->irc_nick_collision		= ay_irc_nick_collision ;
	cb->irc_user_not_in_chnl	= ay_irc_user_not_in_chnl ;
	cb->irc_not_on_chnl		= ay_irc_not_on_chnl ;
	cb->irc_on_chnl			= ay_irc_on_chnl ;
	cb->irc_nologin			= ay_irc_nologin ;
	cb->irc_summon_disabled		= ay_irc_summon_disabled ;
	cb->irc_user_disabled		= ay_irc_user_disabled ;
	cb->irc_not_registered		= ay_irc_not_registered ;
	cb->irc_need_more_params	= ay_irc_need_more_params ;
	cb->irc_already_registered	= ay_irc_already_registered ;
	cb->irc_no_perm_for_host	= ay_irc_no_perm_for_host ;
	cb->irc_banned			= ay_irc_banned ;
	cb->irc_chnl_key_set		= ay_irc_chnl_key_set ;
	cb->irc_channel_full		= ay_irc_channel_full ;
	cb->irc_unknown_mode		= ay_irc_unknown_mode ;
	cb->irc_invite_only		= ay_irc_invite_only ;
	cb->irc_banned_from_chnl	= ay_irc_banned_from_chnl ;
	cb->irc_bad_chnl_key		= ay_irc_bad_chnl_key ;
	cb->irc_no_privileges		= ay_irc_no_privileges ;
	cb->irc_chnlop_privs_needed	= ay_irc_chnlop_privs_needed ;
	cb->irc_cant_kill_server	= ay_irc_cant_kill_server ;
	cb->irc_no_o_per_host		= ay_irc_no_o_per_host ;
	cb->irc_mode_unknown		= ay_irc_mode_unknown ;
	cb->irc_user_dont_match		= ay_irc_user_dont_match ;*/

	return cb ;
}

