/*
 * IRC protocol support for Ayttm
 *
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
#include "gtk/gtk_eb_html.h"
#include "plugin_api.h"
#include "smileys.h"
#include "messages.h"

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

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"IRC",
	"Provides Internet Relay Chat (IRC) support",
	"$Revision: 1.24 $",
	"$Date: 2003/06/27 12:06:18 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "IRC", -1, SERVICE_CAN_GROUPCHAT | SERVICE_CAN_ICONVERT, NULL };
/* End Module Exports */

static char *eb_irc_get_color(void) { static char color[]="#880088"; return color; }

unsigned int module_version() {return CORE_VERSION;}
static int plugin_init()
{
	eb_debug(DBG_MOD, "IRC\n");
	ref_count=0;
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


/* RFC1459 and RFC2812 defines max message size
   including \n == 512 bytes, but being tolerant of
   violations is nice nevertheless. */
#define BUF_LEN 512*2

static int set_nonblock (int fd)
{
#ifndef __MINGW32__
  int flags = fcntl (fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  return fcntl (fd, F_SETFL, flags | O_NONBLOCK);
#endif
}



typedef struct irc_local_account_type
{
	char		server[255];
	char		password[255];
	char 		port[10];
	int 		fd;
	int		fd_tag;
	int		keepalive_tag;
	GString *	buff;
	int		status;
	LList *		friends;
	LList *		channel_list;
} irc_local_account;

typedef struct irc_account_type
{
	char		server[255];
	char		realserver[255];
	int		status;
	int		idle;
} irc_account;

typedef struct _irc_info
{
	char 		*whois_info;
	eb_account 	*me;
	char		*fullmessage;
} irc_info;

/* from util.c :
 *
 * The last state in the list of states will be the OFFLINE state
 * The first state in the list will be the ONLINE states
 * The rest of the states are the various AWAY states
 */

enum {
	IRC_ONLINE = 0,
	IRC_AWAY,
	IRC_OFFLINE
};

static char *irc_states[] =
{
	"",
	"Away",
	"Offline"
};

static int is_setting_state = 0;

/* Local prototypes */
static unsigned char *strip_color (unsigned char *text);
           static int sendall(int s, char *buf, int len);
      static int irc_query_connected (eb_account * account);
          static void irc_parse_incoming_message (eb_local_account * ela, char *buff);
          static void irc_parse (eb_local_account * ela, char *buff);
          static void irc_callback (void *data, int source, eb_input_condition condition);
          static void irc_ask_after_users ( eb_local_account * account );
          static int irc_keep_alive( gpointer data );
          static void irc_login( eb_local_account * account);
          static void irc_logout( eb_local_account * ela );
          static void irc_send_im( eb_local_account * account_from, eb_account * account_to,char *message );
static eb_local_account * irc_read_local_config(LList * pairs);
       static LList * irc_write_local_config( eb_local_account * account );
  static eb_account * irc_read_config(eb_account *ea, LList *config );
       static LList * irc_get_states();
          static int irc_get_current_state(eb_local_account * account );
          static void irc_set_current_state(eb_local_account * account, int state );
        static char * irc_check_login(char * user, char * pass);
          static void irc_add_user( eb_account * account );
          static void irc_del_user( eb_account * account );
      static int irc_is_suitable (eb_local_account *local, eb_account *remote);
	static eb_account * irc_new_account(eb_local_account *ela, const char * account );
       static char * irc_get_status_string( eb_account * account );
          static char ** irc_get_status_pixmap( eb_account * account);
          static void irc_set_idle(eb_local_account * account, int idle );
          static void irc_set_away( eb_local_account * account, char * message);
          static void irc_send_file( eb_local_account * from, eb_account * to, char * file );
          static void irc_info_update(info_window * iw);
          static void irc_info_data_cleanup(info_window * iw);
          static void irc_get_info( eb_local_account * account_from, eb_account * account_to);
          static void irc_join_chat_room(eb_chat_room *room);
          static void irc_leave_chat_room(eb_chat_room *room);
          static void irc_send_chat_room_message(eb_chat_room *room, char *message);
          static void irc_send_invite( eb_local_account * account, eb_chat_room * room, char * user, char * message);
static eb_chat_room * irc_make_chat_room(char * name, eb_local_account * account, int is_public);
          static void irc_accept_invite( eb_local_account * account, void * invitation );
          static void irc_decline_invite( eb_local_account * account, void * invitation );
          static void eb_irc_read_prefs_config(LList * values);
       static LList * eb_irc_write_prefs_config();

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
		text = "";

	len = strlen (text);
	new_str = malloc (len + 2);

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

/* Taken from Beej's Guide to Network Programming, and somewhat modified */
static int sendall(int s, char *buf, int len)
{
    int total = 0;	  // how many bytes we've sent
    int bytesleft = len; // how many we have left to send
    int n = 0;
    int errors = 0;

    while(total < len) {
	n = send(s, buf+total, bytesleft, 0);
	if (n == -1)
	{
		errors++;
		
		/* sleep a little bit and try again, up to 10 times */
		if ((errno == EAGAIN) && (errors < 10))
		{
			n = 0;
			usleep(1); 
		}
		else
		{
			break;
		}
	}
	total += n;
	bytesleft -= n;
    }

    return n==-1?-1:total; // return -1 on failure, bytes sent on success
}

static int irc_query_connected (eb_account * account)
{
	irc_account * ia = NULL;

	if (account == NULL) return FALSE;

	ia = (irc_account *) account->protocol_account_data;
	
	return (ia->status != IRC_OFFLINE);
}

/* Handle a message-to-be-read here ... CTCP ACTION, PRIVMSG and NOTICE */

static void irc_parse_incoming_message (eb_local_account * ela, char *buff)
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	char **buff2;
	int is_nickserv = 0;
	eb_account *ea = NULL;
	irc_account *ia = NULL;

	char orig_nick[256];
	char nick[256];
	char *alpha;
	

	unsigned char tempstring[BUF_LEN];
	unsigned char *tempstring2 = NULL;

	/* remove the crlf */
	g_strchomp(buff);

	buff2 = g_strsplit(buff, " ", 3);
	strncpy(nick, buff2[0]+1, 100);
	/* according to RFC2812, channels can be marked by #, &, + or !, not only the conventional #. */
	if ((*(buff2[2]) == '#') || (*(buff2[2]) == '&') || (*(buff2[2]) == '+') || (*(buff2[2]) == '!')) {
		eb_chat_room * ecr = NULL;
		strncpy(tempstring, buff2[2], BUF_LEN/2);
		strncat (tempstring, "@", sizeof(tempstring)-strlen(tempstring));
		strncat (tempstring, ila->server, sizeof(tempstring)-strlen(tempstring));
		g_strdown(tempstring);
		ecr = find_chat_room_by_id(tempstring);
		if (ecr) {
			alpha = strchr(nick, '!');
			if (alpha != NULL)
				*alpha = '\0';
			tempstring2 = strip_color(buff2[3] + 1);
			eb_chat_room_show_message( ecr, nick, tempstring2);
			free(tempstring2);
		}
		g_strfreev (buff2);
		return;
	}
	g_strfreev (buff2);
		
	/* remove the hostmask part */
	alpha = strchr(nick, '!');

	/* If server-to-user notice, not user-to-user - ignore */
	if (alpha == NULL) return; 

	*alpha = '\0';
	if(!strcasecmp(nick, "NickServ"))
	{
		is_nickserv = 1;
	}

	strncpy(orig_nick, nick, sizeof(orig_nick));

	/* Add our internal @ircserver part */
	strncat(nick, "@", 255-strlen(nick));
	strncat(nick, ila->server, 255 - strlen(nick));

	/* ... and search for the sender */
	ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		
	if (!ea)
	{
		/* Some unknown person is msging us - add him */
		irc_account * ia = g_new0(irc_account, 1);
		ea = g_new0(eb_account, 1);
		strncpy(ea->handle, nick, 255);
		ea->service_id = ela->service_id;
		ia->status = IRC_OFFLINE;
		strncpy(ia->server, ila->server, 255);
		ea->protocol_account_data = ia;
		add_unknown_with_name(ea, orig_nick);
	}

	ia = (irc_account *)ea->protocol_account_data;

	if (ia->status == IRC_OFFLINE)
	{
		/* Okay, if someone msgs us, they are per definition online... */
		buddy_login(ea);
		ia->status = IRC_ONLINE;
		buddy_update_status(ea);
	}

	buff2 = g_strsplit(buff, ":", 2);
	if (buff2[2] != NULL) /* Is there any actual message out there? */
	{
		tempstring2 = strip_color(buff2[2]);
	}
	else
	{
		tempstring2 = strdup("");
	}
	g_strfreev (buff2);
	if(!is_nickserv)
	{
		eb_parse_incoming_message(ela, ea, tempstring2);
	}
	else if(strstr(tempstring2,"This nickname") || 
		(strstr(tempstring2, "NickServ") && 
		 (strstr(tempstring2, "identify") ||
		  strstr(tempstring2, "IDENTIFY"))))
	{
	        int ret;
		char ps[255];
		if (strstr(tempstring2,"/NickServ")) {
		  g_snprintf(ps, 255, "NICKSERV :identify %s\n", ila->password);
		  fprintf(stderr, "IRC: NICKSERV sending password to NickServ\n");
		  ret = sendall(ila->fd, ps, strlen(ps));
	  if (ret == -1) irc_logout(ela);
		} else {
		  g_snprintf(ps, 255, "identify %s", ila->password);
		  fprintf(stderr, "IRC: PRIVMSG sending password to NickServ\n");
		  irc_send_im( ela, ea,ps);
		}
	}
	free(tempstring2);
	return;
}

/* The main IRC protocol parser */

static void irc_parse (eb_local_account * ela, char *buff)
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	char **buff2;
	char **split_buff;
	eb_account *ea = NULL;
	irc_account *ia = NULL;
	int ret = 0;

	split_buff = g_strsplit(buff, " ", 2);

	if (!split_buff[1]) { g_strfreev (split_buff); return; }

	if (strncmp(buff, "PING :", 6) == 0)
	{
		/* Keepalive ping from server - reply to avoid being disconnected */
		buff2 = g_strsplit(buff, ":", 1);
		g_strchomp(buff2[1]);

		g_snprintf(buff, BUF_LEN, "PONG :%s\n", buff2[1]);
		g_strfreev (buff2);

		ret = sendall(ila->fd, buff, strlen(buff)); 
	}
	else if ((!strncmp(split_buff[1], "376", 3)) || (!strncmp(split_buff[1], "422", 3)))
	{
		/* RPL_ENDOFMOTD(376) or ERR_NOMOTD(422) - okay, we're in and can start sending commands */
		irc_ask_after_users(ela);
	}
	else if ((!strncmp(split_buff[1], "463", 3)) || (!strncmp(split_buff[1], "465", 3)))
	{
		/* ERR_NOPERMFORHOST(463) or ERR_YOUREBANNEDCREEP(465)
		   - we're not allowed to connect, give up and logout */
		irc_logout(ela);
	}
	else if (!strncmp(split_buff[1], "401", 3))
	{
		/* ERR_NOSUCHNICK(401) - set user to offline */
		char nick[256];
		
		/* Get the nick */
		buff2 = g_strsplit(buff, " ", 4);
		strncpy(nick, buff2[3], 100);
		g_strfreev (buff2);

		/* Add our internal @ircserver part */
		strncat(nick, "@", 255-strlen(nick));
		strncat(nick, ila->server, 255 - strlen(nick));
		
		/* Search and set offline */
		ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		if (ea)
		{
			ia = (irc_account *)ea->protocol_account_data;

			if(ia->status != IRC_OFFLINE)
			{
				buddy_logoff(ea);
				ia->status = IRC_OFFLINE;
				buddy_update_status(ea);
				ia->realserver[0] = '\0';
			}
		}
	}
	else if (!strncmp(split_buff[1], "311", 3))
	{
		/* RPL_WHOISUSER(311) - okay, user is around - set user to online */
		char nick[256];

		/* Get the nick */
		buff2 = g_strsplit(buff, " ", 4);
		strncpy(nick, buff2[3], 100);
		g_strfreev (buff2);

		/* Add our internal @ircserver part */
		strncat(nick, "@", 255-strlen(nick));
		strncat(nick, ila->server, 255 - strlen(nick));
		
		/* Search and set online */
		ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		if (ea)
		{
			ia = (irc_account *)ea->protocol_account_data;
			if (ia->status == IRC_OFFLINE)
			{
				buddy_login(ea);
				ia->status = IRC_ONLINE;
				buddy_update_status(ea);
			}
			if (ea->infowindow != NULL )
			{
				char **priv_split_buff = g_strsplit(buff, " ", 3);
				
				irc_info *ii = (irc_info *)ea->infowindow->info_data;
				if (ii->whois_info != NULL) { free(ii->whois_info); }
				ii->whois_info = malloc(BUF_LEN);
			
				strncpy(ii->whois_info, priv_split_buff[3], sizeof(ii->whois_info));
				g_strfreev (priv_split_buff);
				
				irc_info_update(ea->infowindow);
			}
		}
	}
	else if (!strncmp(split_buff[1], "317", 3))
	{
		/* RPL_WHOISIDLE(317) - user is around but idle - record idle time */
		char nick[256];

		/* Get the nick */
		buff2 = g_strsplit(buff, " ", 5);
		strncpy(nick, buff2[3], 100);

		/* Add our internal @ircserver part */
		strncat(nick, "@", 255-strlen(nick));
		strncat(nick, ila->server, 255 - strlen(nick));
		
		/* Search and set idle time */
		ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		if (ea)
		{
			ia = (irc_account *)ea->protocol_account_data;
			ia->idle = atoi(buff2[4]);
			buddy_update_status(ea);
			if (ea->infowindow)
			{
				irc_info_update(ea->infowindow);
			}
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "312", 3))
	{
		/* RPL_WHOISSERVER(312) - user is around, on some server
		   - record server for later WHOISes */
		char nick[256];

		/* Get the nick */
		buff2 = g_strsplit(buff, " ", 5);
		strncpy(nick, buff2[3], 100);
		/* Add our internal @ircserver part */
		strncat(nick, "@", 255-strlen(nick));
		strncat(nick, ila->server, 255 - strlen(nick));
		
		/* Search and set realserver */
		ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		if (ea)
		{
			ia = (irc_account *)ea->protocol_account_data;
			strncpy(ia->realserver, buff2[4], 255);
			if (ea->infowindow)
			{
				irc_info_update(ea->infowindow);
			}
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "301", 3))
	{
		/* RPL_AWAY(301) - user is away */
		char nick[256];

		/* Get the nick */
		buff2 = g_strsplit(buff, " ", 4);
		strncpy(nick, buff2[3], 100);
		g_strfreev (buff2);

		/* Add our internal @ircserver part */
		strncat(nick, "@", 255-strlen(nick));
		strncat(nick, ila->server, 255 - strlen(nick));
		
		/* Search and set online */
		ea = find_account_by_handle(nick, SERVICE_INFO.protocol_id);
		if (ea)
		{
			ia = (irc_account *)ea->protocol_account_data;
			if (ia->status == IRC_OFFLINE)
			{
				/* Can this happen? */
				buddy_login(ea);
				ia->status = IRC_AWAY;
				buddy_update_status(ea);
			}
			else if (ia->status == IRC_ONLINE)
			{
				ia->status = IRC_AWAY;
				buddy_update_status(ea);
			}
			if (ea->infowindow)
			{
				irc_info_update(ea->infowindow);
			}
		}
	}
	else if (!strncmp(split_buff[1], "353", 3))
	{
		/* RPL_NAMEREPLY(353) - lists users in a channel */
		eb_chat_room * ecr;
		char *buddy;
		char tempstring[BUF_LEN];
		int i = 6;
		g_strchomp(buff);
		buff2 = g_strsplit(buff, " ", -1);
		strncpy(tempstring, buff2[4], BUF_LEN);
		strncat(tempstring, "@", BUF_LEN-strlen(tempstring));
		strncat(tempstring, ila->server, BUF_LEN-strlen(tempstring));
		g_strdown(tempstring);
		ecr = find_chat_room_by_id(tempstring);
		if (ecr) {
			if ((*(buff2[5] + 1) == '@') || (*(buff2[5] + 1) == '+'))
				buddy = buff2[5] + 2;
			else 
				buddy = buff2[5] + 1;
			if (!eb_chat_room_buddy_connected( ecr, buddy ))
				eb_chat_room_buddy_arrive( ecr, buddy, buddy);
			while (buff2[i] != NULL) {
					if ((*(buff2[i]) == '@') || (*(buff2[i]) == '+'))
						buddy = buff2[i] + 1;
					else
						buddy = buff2[i];
					if (!eb_chat_room_buddy_connected( ecr, buddy ))
						eb_chat_room_buddy_arrive( ecr, buddy, buddy);
					i++;
			}
		} else {
			printf("IRC: RPL_NAMEREPLY without joining channel %s on %s!\n", tempstring, ila->server);
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "321", 3)) /* RPL_LISTSTART */
	{
		if (ila->channel_list)
			l_list_free(ila->channel_list);
		ila->channel_list = NULL;
	}
	else if (!strncmp(split_buff[1], "322", 3)) /* RPL_LIST */
	{
		buff2 = g_strsplit(buff, " ", -1);
		ila->channel_list = l_list_insert_sorted(ila->channel_list, strdup(buff2[3]),
					strcasecmp);
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "322", 3)) /* RPL_LISTEND */
	{
	}
	else if (!strncmp(split_buff[1], "JOIN", 4))
	{
		/* Someone has joined the channel */
		eb_chat_room * ecr;
		char nick[256];
		char tempstring[BUF_LEN];
		char *alpha;

		buff2 = g_strsplit(buff, " ", 3);
		g_strchomp(buff2[2]);
		strncpy(tempstring, buff2[2]+1, BUF_LEN);
		strncat(tempstring, "@", BUF_LEN-strlen(tempstring));
		strncat(tempstring, ila->server, BUF_LEN-strlen(tempstring));
		g_strdown(tempstring);
		ecr = find_chat_room_by_id(tempstring);

		if (ecr) {
			/* Get the nick */
			strncpy(nick, buff2[0]+1, 100);
			alpha = strchr(nick, '!');
			if (alpha != NULL)
				*alpha = '\0';

			eb_chat_room_buddy_arrive( ecr, nick, nick );
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "QUIT", 4))
	{
		/* Someone has left all the channels */
		LList * node = chat_rooms;
		eb_chat_room * ecr;
		char nick[256];
		char *alpha;

		/* Get the nick */
		strncpy(nick, split_buff[0]+1, 100);
		alpha = strchr(nick, '!');
		*alpha = '\0';

		for(node = chat_rooms; node; node = node->next)
		{
			ecr = (eb_chat_room*)node->data;
			if (ecr && ecr->local_user->service_id == SERVICE_INFO.protocol_id ) 
			{
				char ** buff3 = g_strsplit(ecr->id, "@", 2);
				if(!strcmp(buff3[1], ila->server)
					&& eb_chat_room_buddy_connected(ecr, nick))
				{
					eb_chat_room_buddy_leave( ecr, nick );
				}
				g_strfreev (buff3);
			}
		}
	}
	else if (!strncmp(split_buff[1], "PART", 4))
	{
		/* Someone has left the channel */
		eb_chat_room * ecr;
		char nick[256];
		char tempstring[BUF_LEN];
		char *alpha;

		buff2 = g_strsplit(buff, " ", 3);
		g_strchomp(buff2[2]);
		strncpy(tempstring, buff2[2], BUF_LEN);
		strncat(tempstring, "@", BUF_LEN-strlen(tempstring));
		strncat(tempstring, ila->server, BUF_LEN-strlen(tempstring));
		g_strdown(tempstring);
		ecr = find_chat_room_by_id(tempstring);

		if (ecr) {
			/* Get the nick */
			strncpy(nick, buff2[0]+1, 100);
			alpha = strchr(nick, '!');
			*alpha = '\0';

			eb_chat_room_buddy_leave( ecr, nick );
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "NICK", 4))
	{
		/* Someone has left the channel */
		LList * node;
		eb_chat_room * ecr;
		char nick[256];
		char tempstring[BUF_LEN];
		char *alpha;

		buff2 = g_strsplit(buff, " ", 3);
		g_strchomp(buff2[2]);
		strncpy(tempstring, buff2[2]+1, BUF_LEN);

		/* Get the nick */
		strncpy(nick, buff2[0]+1, 100);
		alpha = strchr(nick, '!');
		*alpha = '\0';

		for(node = chat_rooms; node; node = node->next)
		{
			ecr = (eb_chat_room*)node->data;
			if (ecr && ecr->local_user->service_id == SERVICE_INFO.protocol_id ) 
			{
				char ** buff3 = g_strsplit(ecr->id, "@", 2);
				if(!strcmp(buff3[1], ila->server) 
						&& eb_chat_room_buddy_connected(ecr, nick))
				{
					eb_chat_room_buddy_leave( ecr, nick );
					eb_chat_room_buddy_arrive( ecr, tempstring, tempstring);
				}
				g_strfreev (buff3);
			}
		}
		g_strfreev (buff2);
	}
	else if (!strncmp(split_buff[1], "INVITE", 6))
	{
		/* Someone wants you to come by their channel */
		eb_chat_room * ecr;
		char nick[256];
		char tempstring[BUF_LEN];
		char *alpha;

		g_strchomp(buff);
		buff2 = g_strsplit(buff, " ", 4);
		strncpy(tempstring, buff2[3]+1, BUF_LEN);
		strncat(tempstring, "@", BUF_LEN-strlen(tempstring));
		strncat(tempstring, ila->server, BUF_LEN-strlen(tempstring));
		g_strdown(tempstring);
		ecr = find_chat_room_by_id(tempstring);

		if (!ecr) {
			/* Get the nick */
			strncpy(nick, buff2[0]+1, 100);
			alpha = strchr(nick, '!');
			*alpha = '\0';

			invite_dialog( ela, nick, buff2[3] + 1, strdup(buff2[3] + 1) );
		}
		g_strfreev (buff2);
	}
	else if (((!strncmp(split_buff[1], "PRIVMSG", 7)) || (!strncmp(split_buff[1], "NOTICE", 6))) && (strstr(buff, "\001")))
	{
		/* CTCP - Client To Client Protocol */
		char nick[256];
		char message[BUF_LEN];
		char *alpha;

		buff2 = g_strsplit(buff, " ", 3);
		strncpy(nick, buff2[0]+1, 100);
		g_strfreev (buff2);
		
		/* remove the hostmask part */
		alpha = strchr(nick, '!');
		*alpha = '\0';

		message[0] = '\0'; /* so that strlen(message) will work */

		/* TODO:
		   DCC SENDing files and receiving DCC SEND'ed files. */

		/* Reply as per the CTCP pseudo-RFC with a NOTICE, not a PRIVMSG */
		if (strstr(buff, "\001VERSION\001"))
		{
			g_snprintf(message, BUF_LEN, "NOTICE %s :\001VERSION Ayttm:%s:http://ayttm.sf.net/\001\n", nick, VERSION);
		}
		else if (strstr(buff, "\001PING"))
		{
			buff2 = g_strsplit(buff, ":", 2);
			g_snprintf(message, BUF_LEN, "NOTICE %s :%s\n", nick, buff2[2]);
			g_strfreev (buff2);
		}
		else if (strstr(buff, "\001ACTION"))
		{
			eb_chat_room * ecr;
			char message[BUF_LEN];
			char tempstring[BUF_LEN];
			char *pointer = NULL;
			int i = 0;
			int nickcopied = 0;
			message[0]='\0';
			
			pointer = buff;
			while (*pointer != '\0')
			{
				if (*pointer == '\001' && nickcopied == 0)
				{
					pointer += 7;
					
					strncpy(message+ i, nick, BUF_LEN-i);
					i += strlen(nick);
					
					nickcopied = 1;
				}
				else if (*pointer == '\001' && nickcopied == 1)
				{
					break;
				}
				else
				{
					message[i] = *pointer;
					pointer++, i++;
				}
			}
			message[i] = '\0';
			
			buff2 = g_strsplit(message, " ", 3);
			g_strchomp(buff2[2]);
			strncpy(tempstring, buff2[2], BUF_LEN);
			strncat(tempstring, "@", BUF_LEN-strlen(tempstring));
			strncat(tempstring, ila->server, BUF_LEN-strlen(tempstring));
			g_strdown(tempstring);
			ecr = find_chat_room_by_id(tempstring);
			if(ecr)
			{
				char colorized_message[BUF_LEN];
				char * tempstring2 = strip_color(buff2[3]+1);
				g_snprintf(colorized_message, BUF_LEN, 
						"<font color=\"#00AA00\">*%s</font>",
						tempstring2);
				eb_chat_room_show_3rdperson(ecr, colorized_message);
			}
		}
		else if (strstr(buff, "\001CLIENTINFO"))
		{
			if (strstr(buff, "\001CLIENTINFO\001"))
			{
				g_snprintf(message, BUF_LEN, _("NOTICE %s :\001CLIENTINFO ACTION CLIENTINFO PING VERSION :Use CLIENTINFO <COMMAND> to get more specific information\001\n"), nick);
			}
			else if (strstr(buff, " ACTION\001"))
			{
				g_snprintf(message, BUF_LEN, _("NOTICE %s :\001CLIENTINFO :ACTION contains action descriptions for atmosphere\001\n"), nick);
			}
			else if (strstr(buff, " CLIENTINFO\001"))
			{
				g_snprintf(message, BUF_LEN, _("NOTICE %s :\001CLIENTINFO :CLIENTINFO gives information about available CTCP commands\001\n"), nick);
			}
			else if (strstr(buff, "PING\001"))
			{
				g_snprintf(message, BUF_LEN, _("NOTICE %s :\001CLIENTINFO :PING echoes back whatever it receives\001\n"), nick);
			}
			else if (strstr(buff, " VERSION\001"))
			{
				g_snprintf(message, BUF_LEN, _("NOTICE %s :\001CLIENTINFO :VERSION shows client type, version and environment\001\n"), nick);
			}
		}

		/* Actually send the reply chosen above
		   w/sanity check - never reply to NOTICEs */
		if (strlen(message) && strstr(buff, " PRIVMSG "))
		{
			ret = sendall(ila->fd, message, strlen(message));
		}
#ifdef DEBUG
 		else { fprintf(stderr, "Unreplied CTCP command: %s", buff); }
#endif
	}
	else if ((!strncmp(split_buff[1], "PRIVMSG", 7)) || (!strncmp(split_buff[1], "NOTICE", 6)))
	{
		irc_parse_incoming_message(ela, buff);
	}
#ifdef DEBUG
	/* for debugging only - print unhandled messages */
 	else { fprintf(stderr, "%s", buff); }
#endif
	g_strfreev (split_buff);

	/* If we couldn't send replies... something's wrong. Logout. */
	if (ret == -1) irc_logout(ela);

	return;
}

static void irc_callback (void *data, int source, eb_input_condition condition)
{
	eb_local_account * ela = (eb_local_account *) data;
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;

	eb_account * ea = NULL;
	irc_account *ia = NULL;
	LList * node;
	static int i = 0;
	int firstread = 1;
	char c;
	static char buff[BUF_LEN];
	
	if ((source == ila->fd) && (buff != NULL))
	{
		int status;

		/* Read and null-terminate an IRC message string */
		do
		{
			status = read(ila->fd, &c, 1);
			if ((status < 0) && (errno == EAGAIN))
			{
				/* no data there */
				status = 0;
			}

			if( status == -1 || (status == 0 && firstread == 1))
			{
				/* Connection closed by other side - log off */
				char buff[1024]; 
				snprintf(buff, sizeof(buff), _("Connection closed by %s."), ila->server);
				ay_do_error( _("IRC Error"), buff );
				fprintf(stderr, buff);
				
				ela->connected = 0;
				if (ila->fd_tag) 	eb_input_remove(ila->fd_tag);
				if (ila->keepalive_tag) eb_timeout_remove(ila->keepalive_tag);
				ila->fd = 0;
				ila->fd_tag = 0;
				ila->keepalive_tag = 0;
				ila->status = IRC_OFFLINE;
				
				is_setting_state = 1;
	
				if(ela->status_menu)
					eb_set_active_menu_status(ela->status_menu, IRC_OFFLINE);
				
				is_setting_state = 0;
				
				/* Make sure relevant accounts get logged off */
				for( node = ila->friends; node; node = node->next )
				{
					ea = (eb_account *)(node->data);
					ia = (irc_account *)ea->protocol_account_data;
			
					if(ia->status != IRC_OFFLINE)
					{
						buddy_logoff(ea);
						ia->status = IRC_OFFLINE;
						buddy_update_status(ea);
						ia->realserver[0] = '\0';
					}
				}
				return;
			}
			firstread = 0;

			if (status > 0)
			{
				buff[i] = c;
				i++;

				if ((c == '\n') || (i >= (BUF_LEN - 1)))
				{
					/* Got a complete message */
					buff[i] = '\0';

					/* parse and react to the IRC message */
					irc_parse(ela, buff);

					/* reset to start of buffer */
					i = 0;
				}
			}
#ifdef DEBUG
			else
			{
				fprintf(stderr, "IRC: read returned %d\n", status);
			}
#endif
		} while(status > 0);
	}
	return;
}

static void irc_ask_after_users ( eb_local_account * account )
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;
	eb_account * ea = NULL;
	irc_account * ia = NULL;
	LList * node;
	char buff[BUF_LEN];
	char *nick = NULL;
	char *alpha = NULL;
	int ret = 0;

	for( node = ila->friends; node; node = node->next )
	{
		ea = (eb_account *)(node->data);
		ia = (irc_account *)ea->protocol_account_data;
		
		nick = strdup(ea->handle);
		if (nick != NULL)
		{
			alpha = strchr(nick, '@');
			if(alpha == NULL) return;
			*alpha = '\0';
		
			if(strlen(ia->realserver) > 0)
			{
				g_snprintf(buff, BUF_LEN, "WHOIS %s %s\n", ia->realserver, nick);
			}
			else
			{
				g_snprintf(buff, BUF_LEN, "WHOIS %s\n", nick);
			}
			ret = sendall(ila->fd, buff, strlen(buff));
			if (ret == -1) irc_logout(account);
		
			free(nick);
		}
	}
	return;
}

/* Called once a minute, checks whether buddies still are online */

static int irc_keep_alive( gpointer data )
{
	eb_local_account * ela = (eb_local_account *) data;

	irc_ask_after_users(ela);

	return TRUE;
}

static void irc_login( eb_local_account * account)
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;

	char buff[BUF_LEN];
	struct hostent *host;
	struct sockaddr_in site;
	int i;
	int ret = 0;
	char *nick = NULL;
	char *alpha = NULL;

	/* Setup and connect */

	host = proxy_gethostbyname(ila->server);
	if (!host) { 
		char buff[1024]; 
		snprintf(buff, sizeof(buff), _("%s: Unknown host."), ila->server);
		ay_do_error( _("IRC Error"), buff );
		fprintf(stderr, buff);

		return; 
	}

	site.sin_family = AF_INET;
	site.sin_addr.s_addr = ((struct in_addr *)(host->h_addr))->s_addr;
	site.sin_port = htons(atoi(ila->port));
	/* default to port 6667 if nothing specified */
	if (ila->port[0] == '\0') site.sin_port = htons(6667);
	
	i = socket(AF_INET, SOCK_STREAM, 0);
	if (i < 0) { fprintf(stderr, "IRC: socket() failed for %s\n", ila->server); return; }
	
	if (proxy_connect(i, (struct sockaddr *)&site, sizeof(site),NULL,NULL,NULL) < 0)
	{
		char buff[1024]; 
		snprintf(buff, sizeof(buff), _("Cannot connect to %s."), ila->server);
		ay_do_error( _("IRC Error"), buff );
		fprintf(stderr, buff);

		return;
	}

	/* Puzzle out the nick we're going to ask the server for */
	nick = strdup(account->handle);
	if (nick == NULL) return;
	alpha = strchr(nick, '@');
	if(alpha == NULL) { free (nick); return; }
	*alpha = '\0';

	set_nonblock(i);

	ila->fd = i;

	/* Set up callbacks and timeouts */

	ila->fd_tag = eb_input_add(ila->fd, EB_INPUT_READ, irc_callback, account);

	/* Log in */
	g_snprintf(buff, BUF_LEN, "NICK %s\nUSER %s 0 * :Ayttm user\n", nick, nick);
	free (nick);

	/* Try twice, then give up */
	ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) { irc_logout(account); return; }
	
	/* No use adding this one before we're in anyway */
	ila->keepalive_tag = eb_timeout_add((guint32)60000, irc_keep_alive, (gpointer)account );

	/* get list of channels */
	ret = sendall(ila->fd, "LIST\n", strlen("LIST\n"));

	/* Claim us to be online */
	account->connected = TRUE;
	ila->status = IRC_ONLINE;
	ref_count++;

	is_setting_state = 1;
	if(account->status_menu)
		eb_set_active_menu_status(account->status_menu, IRC_ONLINE);
	is_setting_state = 0;

	return;
}

static void irc_logout( eb_local_account * ela )
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;

	LList * node;
	char buff[BUF_LEN];
	eb_account * ea = NULL;
	irc_account *ia = NULL;

	ela->connected = 0;
	eb_input_remove(ila->fd_tag);
	eb_timeout_remove(ila->keepalive_tag);

	/* Log off in a nice way */
	g_snprintf(buff, BUF_LEN, "QUIT :Ayttm logging off\n");
	sendall(ila->fd, buff, strlen(buff));
	close(ila->fd);

	ila->fd = 0;
	ila->fd_tag = 0;
	ila->keepalive_tag = 0;
	ila->status = IRC_OFFLINE;
				
	is_setting_state = 1;
	if(ela->status_menu)
		eb_set_active_menu_status(ela->status_menu, IRC_OFFLINE);
	is_setting_state = 0;

	/* Make sure relevant accounts get logged off */
	for( node = ila->friends; node; node = node->next )
	{
		ea = (eb_account *)(node->data);
		ia = (irc_account *)ea->protocol_account_data;

		if(ia->status != IRC_OFFLINE)
		{
			buddy_logoff(ea);
			ia->status = IRC_OFFLINE;
			buddy_update_status(ea);
			ia->realserver[0] = '\0';
		}
	}

	ref_count--;
	return;
}

static void irc_send_im( eb_local_account * account_from,
                        eb_account * account_to,
                              char *message )
{
	irc_local_account * ila = (irc_local_account *) account_from->protocol_local_account_data;
	char buff[BUF_LEN];
	char *nick = NULL;
	char *alpha = NULL;
	int ret = 0;

	nick = strdup(account_to->handle);
	if (nick != NULL)
	{
		alpha = strchr(nick, '@');
		if (alpha == NULL) return;
		*alpha = '\0';

		g_snprintf(buff, BUF_LEN, "PRIVMSG %s :%s\n", nick, message);
		ret = sendall(ila->fd, buff, strlen(buff));
		if (ret == -1) irc_logout(account_from);
		
		free(nick);
	}

	return;
}

static eb_local_account * irc_read_local_config(LList * pairs)
{
	eb_local_account * ela = g_new0(eb_local_account, 1);
	irc_local_account * ila = g_new0(irc_local_account, 1);

	char *temp = NULL;
	char *temp2 = NULL;
	char *str = NULL;
	ela->protocol_local_account_data = ila;
	ila->status = IRC_OFFLINE;

	temp = value_pair_get_value(pairs, "SCREEN_NAME");
	if (temp)
	{
		ela->handle = temp;
			
		strncpy(ela->alias, ela->handle, 254);
		ela->service_id = SERVICE_INFO.protocol_id;

		/* string magic - point to the first char after '@' */
		if (strrchr(temp, '@') != NULL)
		{
			temp = strrchr(temp, '@') + 1;
			strncpy(ila->server, temp, 254);

			/* Remove the port from ila->server */
			temp2 = strrchr(ila->server, ':');
			if (temp2) temp2[0] = '\0';

			/* string magic - point to the first char after ':' */
			if (strrchr(temp, ':') != NULL)
			{
				temp = strrchr(temp, ':') + 1;
				strncpy(ila->port, temp, 9);
			}

			temp = value_pair_get_value(pairs, "PASSWORD");
			if (temp)
			{
				strncpy(ila->password, temp, sizeof(ila->password));
				free( temp );
				temp = NULL;
			}
			str = value_pair_get_value(pairs,"CONNECT");
			ela->connect_at_startup=(str && !strcmp(str,"1"));
			free(str);

			return (ela);
		}
		eb_debug(DBG_MOD, "no @ found in login name\n");
	}
	else
		eb_debug(DBG_MOD, "SCREEN_NAME not defined\n");
		

	/* Uh-oh ... something has gone wrong. */
	return NULL;
}

static LList * irc_write_local_config( eb_local_account * account )
{
	LList * list = NULL;
	value_pair * vp;
	irc_local_account * ila = (irc_local_account *)account->protocol_local_account_data;

	vp = g_new0( value_pair, 1 );

	strcpy( vp->key, "SCREEN_NAME");
	strncpy( vp->value, account->handle, 254 );

	list = l_list_append( list, vp );

	vp = g_new0( value_pair, 1 );

	strcpy( vp->key, "PASSWORD");
	strncpy( vp->value, ila->password, 254 );

	list = l_list_append( list, vp );
	
	vp = g_new0( value_pair, 1 );
	strcpy( vp->key, "CONNECT" );
	if (account->connect_at_startup)
		strcpy( vp->value, "1");
	else 
		strcpy( vp->value, "0");
	
	list = l_list_append( list, vp );
	return list;
}

static eb_account * irc_read_config(eb_account *ea, LList *config)
{
	irc_account * ia = g_new0(irc_account, 1);
	char * temp;

		
	ea->protocol_account_data = ia;
	
	/* This func expects account names of the form Nick@server,
	   for example Knan@irc.midgardsormen.net */

	temp = strrchr(ea->handle, '@');
	if(temp)
		strncpy(ia->server, temp+1, sizeof(ia->server));
	
	ia->idle = 0;
	ia->status = IRC_OFFLINE;

	if(ea->ela) {
		irc_local_account * ila = ea->ela->protocol_local_account_data;

		if (!strcmp(ila->server, ia->server))
			ila->friends = l_list_append( ila->friends, ea );
	}
	return ea;
}

static LList * irc_get_states()
{
	LList  * states = NULL;
	states = l_list_append(states, "Online");
	states = l_list_append(states, "Away");
	states = l_list_append(states, "Offline");
	return states;
}

static char * irc_check_login(char * user, char * pass)
{
	if (strrchr(user, '@') == NULL) {
		return strdup(_("No hostname found in your login (which should be in user@host form)."));
	}
	return NULL;
}


static int irc_get_current_state(eb_local_account * account )
{
	irc_local_account * ila = (irc_local_account *) account->protocol_local_account_data;

	return ila->status;
}

static void irc_set_current_state(eb_local_account * account, int state )
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

	if ((ila->status != IRC_OFFLINE) && state == IRC_OFFLINE) irc_logout(account);
	else if ((ila->status == IRC_OFFLINE) && state != IRC_OFFLINE) irc_login(account);

	ila->status = state;
	return;
}

static void irc_add_user( eb_account * account )
{
	/* find the proper local account */
	irc_account *ia = (irc_account *) account->protocol_account_data;
	LList * node;

	for( node = accounts; node; node = node->next )
	{
		eb_local_account * ela = (eb_local_account *)(node->data);
		
		if (ela->service_id == SERVICE_INFO.protocol_id)
		{
			irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

			if (!strcmp(ila->server, ia->server))
			{
				ila->friends = l_list_append( ila->friends, account );
			}
		}
	}

	return;
}

static void irc_del_user( eb_account * account )
{
	/* find the proper local account */
	irc_account *ia = (irc_account *) account->protocol_account_data;
	LList * node;

	for( node = accounts; node; node = node->next )
	{
		eb_local_account * ela = (eb_local_account *)(node->data);
		
		if (ela->service_id == SERVICE_INFO.protocol_id)
		{
			irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;
			if (ia && ia->server && !strcmp(ila->server, ia->server))
			{
				ila->friends = l_list_remove( ila->friends, account );
			}
		}
	}

	return;
}

static int irc_is_suitable (eb_local_account *local, eb_account *remote)
{
	irc_account *ia = NULL;
	irc_local_account *ila = NULL;
	
	if (!local || !remote)
		return FALSE;
	
	if (remote->ela == local)
		return TRUE;
		
	ia = (irc_account *)remote->protocol_account_data; 
	ila = (irc_local_account *)local->protocol_local_account_data;
	
	if (!ia || !ila)
		return FALSE;
		
	if (!strcmp(ia->server, ila->server)) 
		return TRUE;
	
	return FALSE;
}

static eb_local_account * irc_search_for_local_account (char *server)
{
	LList *node;
	
	for( node = accounts; node; node = node->next )
	{
		eb_local_account * ela = (eb_local_account *)(node->data);
		
		if (ela->service_id == SERVICE_INFO.protocol_id)
		{
			irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

			if (!strcmp(ila->server, server))
				return ela;
		}
	}
	return NULL;
}

/* This func expects account names of the form Nick@server,
   for example Knan@irc.midgardsormen.net, and will return
   NULL otherwise, very probably causing a crash. */
static eb_account * irc_new_account(eb_local_account *ela, const char * account )
{
	eb_account * ea = g_new0(eb_account, 1);
	irc_account * ia = g_new0(irc_account, 1);
	LList * node;
	
	strncpy(ea->handle, account, 254);
	ea->ela = ela;
	ea->protocol_account_data = ia;
	ea->service_id = SERVICE_INFO.protocol_id;
	ea->list_item = NULL;
	ea->online = 0;
	ea->status = NULL;
	ea->pix = NULL;
	ea->icon_handler = -1;
	ea->status_handler = -1;
	ea->infowindow = NULL;

	ia->idle = 0;
	ia->status = IRC_OFFLINE;

	/* string magic - point to the first char after '@' */
	if (strrchr(account, '@') != NULL)
	{
		account = strrchr(account, '@') + 1;
		strncpy(ia->server, account, 254);
	}
	else
	{
		fprintf(stderr, "Warning - IRC account name without @server part,\n"
				"picking first local account's server\n");

		for( node = accounts; node; node = node->next )
		{
			eb_local_account * ela = (eb_local_account *)(node->data);
		
			if (ela->service_id == SERVICE_INFO.protocol_id)
			{
				irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

				strncpy(ia->server, ila->server, 254);
				strncat(ea->handle, "@", 254-strlen(ea->handle));
				strncat(ea->handle, ia->server, 254-strlen(ea->handle));
				break;
			}
		}
	}

	return ea;
}

static char * irc_get_status_string( eb_account * account )
{
	irc_account * ia = (irc_account *) account->protocol_account_data;
	static char string[255];
	static char buf[255];
	
	strcpy(string, "");
	strcpy(buf, "");
	
	if(ia->idle >= 60)
	{
		int days, hours, minutes;
		
		minutes = ia->idle / 60;
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
	strncat(string, irc_states[ia->status], 255 - strlen(string));

	return string;
}

static char ** irc_get_status_pixmap( eb_account * account)
{
	irc_account * ia;
	
	/*if (!pixmaps)
		irc_init_pixmaps();*/
	
	ia = account->protocol_account_data;
	
	if (ia->status == IRC_ONLINE)
		return irc_online_xpm;
	else
		return irc_away_xpm;
}

/* Not needed with IRC, the server detects our idleness */
static void irc_set_idle(eb_local_account * account, int idle )
{
	return;
}

static void irc_set_away( eb_local_account * account, char * message)
{
	irc_local_account *ila = (irc_local_account *)account->protocol_local_account_data;
	char buf[BUF_LEN];
	int ret = 0;

	if (!account->connected)
		return;

	if (message) {
		is_setting_state = 1;
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, IRC_AWAY);
		is_setting_state = 0;
		/* Actually set away */
		snprintf(buf, BUF_LEN, "AWAY :%s\n", message);
		ret = sendall(ila->fd, buf, strlen(buf));
		if (ret == -1) irc_logout(account);

	} else {
		is_setting_state = 1;
		if(account->status_menu)
			eb_set_active_menu_status(account->status_menu, IRC_ONLINE);
		is_setting_state = 0;
		/* Unset away */
		snprintf(buf, BUF_LEN, "AWAY\n");
		ret = sendall(ila->fd, buf, strlen(buf));
		if (ret == -1) irc_logout(account);
	}
	return;
}

static void irc_send_file( eb_local_account * from, eb_account * to, char * file )
{
	return;
}

static void irc_info_update(info_window * iw)
{
	char message[BUF_LEN*4];
	char temp[BUF_LEN];
	char *alpha = NULL;
	char *freeme = NULL;
	irc_info *ii = (irc_info *)iw->info_data;

	eb_account * ea = ii->me;
	irc_account * ia = (irc_account *)ea->protocol_account_data;

	strncpy(temp, ea->handle, BUF_LEN);
	alpha = strchr(temp, '@');
	if (alpha != NULL) *alpha = '\0';

	snprintf(message, sizeof(message), _("<b>User info for</b> %s<br>"), temp);
	snprintf(temp, sizeof(temp), _("<b>Server:</b> %s<br>"), strlen(ia->realserver)>0 ? ia->realserver : ia->server);
	strncat(message, temp, sizeof(message)-strlen(message));
	snprintf(temp, sizeof(temp), _("<b>Idle time and online status:</b> %s<br>"), irc_get_status_string(ea));
	strncat(message, temp, sizeof(message)-strlen(message));
	if (ii->whois_info != NULL)
	{
		freeme = strip_color(ii->whois_info);
		snprintf(temp, sizeof(temp), _("<b>Whois info:</b> %s<br>"), freeme);
		free(freeme);
		strncat(message, temp, sizeof(message)-strlen(message));
	}

	eb_info_window_clear(iw);

	if (ii->fullmessage)
	{
		free(ii->fullmessage);
		ii->fullmessage = NULL;
	}
	ii->fullmessage = strdup(message);

	gtk_eb_html_add(EXT_GTK_TEXT(iw->info), ii->fullmessage,1,1,0);
	gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(iw->scrollwindow)),0);
	return;
}

static void irc_info_data_cleanup(info_window * iw)
{
	if (((irc_info *)(iw->info_data))->whois_info != NULL)
	{
		free (((irc_info *)(iw->info_data))->whois_info);
	}
	free (((irc_info *)(iw->info_data))->fullmessage);
	free (iw->info_data);
	iw->info_data = NULL;
	return;
}

static void irc_get_info( eb_local_account * account_from, eb_account * account_to)
{
	char	*nick;
	char	*alpha;
	char buff[BUF_LEN];

	irc_local_account * ila =
		(irc_local_account *) account_from->protocol_local_account_data;

	irc_account * ia = (irc_account *)account_to->protocol_account_data;
		
	nick = strdup(account_to->handle);
	alpha = strchr(nick, '@');
	if (alpha != NULL) *alpha = '\0';

	/* Send a WHOIS request */
	if(strlen(ia->realserver) > 0)
	{
		g_snprintf(buff, BUF_LEN, "WHOIS %s %s\n", ia->realserver, nick);
	}
	else
	{
		g_snprintf(buff, BUF_LEN, "WHOIS %s\n", nick);
	}
	sendall(ila->fd, buff, strlen(buff));
		
	/* Find the pointer to the info window */
	if(account_to->infowindow == NULL )
	{
		/* Create one */
		account_to->infowindow = eb_info_window_new(account_from, account_to);
		gtk_widget_show(account_to->infowindow->window);
	}

	account_to->infowindow->info_data = malloc(sizeof (irc_info));
	memset(account_to->infowindow->info_data, 0, sizeof(irc_info));
	((irc_info *)(account_to->infowindow->info_data))->me = account_to;
	((irc_info *)(account_to->infowindow->info_data))->fullmessage = malloc(1);
	*((irc_info *)(account_to->infowindow->info_data))->fullmessage = '\0';
	account_to->infowindow->cleanup = irc_info_data_cleanup;
	irc_info_update(account_to->infowindow);
 
	return;
}

static void irc_join_chat_room(eb_chat_room *room)
{
	char buff[BUF_LEN];
	signed int ret;
	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	g_snprintf(buff, BUF_LEN, "JOIN :%s\n", room->room_name);
			
	ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) irc_logout(room->local_user);
	return;
}

static void irc_leave_chat_room(eb_chat_room *room)
{
	char buff[BUF_LEN];
	signed int ret;
	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	g_snprintf(buff, BUF_LEN, "PART :%s\n", room->room_name);
			
	ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) irc_logout(room->local_user);
	return;
}

static void irc_send_chat_room_message(eb_chat_room *room, char *message)
{
	char buff[BUF_LEN];
	char nick[255];
	char *alpha;
	signed int ret;

	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	g_snprintf(buff, BUF_LEN, "PRIVMSG %s :%s\n", room->room_name, message);
			
	ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) irc_logout(room->local_user);

	strncpy(nick, room->local_user->alias, 255);
	alpha = strchr(nick, '@');
	if (alpha != NULL) *alpha = '\0';
		
	eb_chat_room_show_message( room, nick, message );
	return;
}

static void irc_send_invite( eb_local_account * account, eb_chat_room * room,
				char * user, char * message)
{
	char buff[BUF_LEN];
	signed int ret;
	irc_local_account * ila = (irc_local_account *) room->local_user->protocol_local_account_data;

	if (*message) {
		g_snprintf(buff, BUF_LEN, "PRIVMSG %s :%s\n", user, message);
			
		ret = sendall(ila->fd, buff, strlen(buff));
		if (ret == -1) irc_logout(room->local_user);
	}

	g_snprintf(buff, BUF_LEN, "INVITE %s :%s\n", user, room->room_name);
			
	ret = sendall(ila->fd, buff, strlen(buff));
	if (ret == -1) irc_logout(room->local_user);
		
	if (*message) {
		g_snprintf(buff, BUF_LEN, _(">>> Inviting %s [%s] <<<"), user, message);
	} else {
		g_snprintf(buff, BUF_LEN, _(">>> Inviting %s <<<"), user);
	}
		
	eb_chat_room_show_message( room, room->local_user->alias, buff);
	return;
}

static eb_chat_room * irc_make_chat_room(char * name, eb_local_account * account, int is_public)
{
	LList * node;
	eb_chat_room * ecr;
	char *chatroom_server = NULL;
	char *alpha = NULL;
	char *channelname = g_new0(char, strlen(name)+100);

	/* according to RFC2812, channels can be marked by #, &, + or !, not only the conventional #. */
	if ((*name != '#') && (*name != '&') && (*name != '+') && (*name != '!')) {
		strcpy(channelname, "#");
	}

	strncat(channelname, name, strlen(name)+100);

	if (strrchr(channelname, '@') != NULL)
	{
		chatroom_server = strrchr(channelname, '@') + 1;
	}
	else
	{
		fprintf(stderr, "Warning - chat_room name without @server part,\n"
				"picking first local account's server\n");

		for( node = accounts; node; node = node->next )
		{
			eb_local_account * ela = (eb_local_account *)(node->data);
		
			if (ela->service_id == SERVICE_INFO.protocol_id)
			{
				irc_local_account * ila = (irc_local_account *)ela->protocol_local_account_data;

				chatroom_server = strdup(ila->server);
				strncat(channelname, "@", strlen(name)+100-strlen(channelname));
				strncat(channelname, chatroom_server, strlen(name)+100-strlen(channelname));
				break;
			}
		}
	}

	g_strdown(channelname);
	ecr = find_chat_room_by_id(channelname);

	if( ecr )
	{
		g_free(channelname);
		return NULL;
	}

	ecr = g_new0(eb_chat_room, 1);

	strncpy(ecr->id, channelname, sizeof(ecr->id));
		
	alpha = strchr(channelname, '@');
	if (alpha != NULL) *alpha = '\0';

	strncpy(ecr->room_name, channelname, sizeof(ecr->room_name));

	ecr->connected = 0;
	ecr->local_user = irc_search_for_local_account (chatroom_server);

	eb_join_chat_room(ecr);

	g_free(channelname);
	return ecr;
}

static void irc_accept_invite( eb_local_account * account, void * invitation )
{
	eb_chat_room * ecr = irc_make_chat_room((char *) invitation, account, FALSE);
	free(invitation);
	if ( ecr )
	{
		//eb_join_chat_room(ecr);
	}
	return;
}

static void irc_decline_invite( eb_local_account * account, void * invitation )
{
	free(invitation);
	return;
}

static void eb_irc_read_prefs_config(LList * values)
{
	return;
}

static LList * eb_irc_write_prefs_config()
{
	return (NULL);
}

static LList * eb_irc_get_public_chatrooms(eb_local_account *ela)
{
	irc_local_account * ila = (irc_local_account *) ela->protocol_local_account_data;
	
	return l_list_copy(ila->channel_list);
}

struct service_callbacks * query_callbacks()
{
	struct service_callbacks * sc;

	sc = g_new0( struct service_callbacks, 1 );
	/* Done */ sc->query_connected = irc_query_connected;
	/* Done */ sc->login = irc_login;
	/* Done */ sc->logout = irc_logout;
	/* Done */ sc->send_im = irc_send_im;
	/* Done */ sc->read_local_account_config = irc_read_local_config;
	/* Done */ sc->write_local_config = irc_write_local_config;
	/* Done */ sc->read_account_config = irc_read_config;
	/* Done */ sc->get_states = irc_get_states;
	/* Done */ sc->get_current_state = irc_get_current_state;
	/* Done */ sc->set_current_state = irc_set_current_state;
	/* Done */ sc->check_login = irc_check_login;
	/* Done */ sc->add_user = irc_add_user;
	/* Done */ sc->del_user = irc_del_user;
	/* Done */ sc->is_suitable = irc_is_suitable;
	/* Done */ sc->new_account = irc_new_account;
	/* Done */ sc->get_status_string = irc_get_status_string;
	/* Done */ sc->get_status_pixmap = irc_get_status_pixmap;
	/* Done */ sc->set_idle = irc_set_idle;
	/* Done */ sc->set_away = irc_set_away;
	sc->send_file = irc_send_file;
	sc->get_info = irc_get_info;
	
	/* Done */ sc->make_chat_room = irc_make_chat_room;
	/* Done */ sc->send_chat_room_message = irc_send_chat_room_message;
	/* Done */ sc->join_chat_room = irc_join_chat_room;
	/* Done */ sc->leave_chat_room = irc_leave_chat_room;
	/* Done */ sc->send_invite = irc_send_invite;
	/* Done */ sc->accept_invite = irc_accept_invite;
	/* Done */ sc->decline_invite = irc_decline_invite;
	
	/* Done */ sc->read_prefs_config = eb_irc_read_prefs_config;
	/* Done */ sc->write_prefs_config = eb_irc_write_prefs_config;
	sc->add_importers = NULL;

	sc->get_color=eb_irc_get_color;
	sc->get_smileys=eb_default_smileys;
	sc->get_public_chatrooms = eb_irc_get_public_chatrooms;
	
	return sc;
}

