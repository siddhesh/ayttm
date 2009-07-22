/*
 * IRC protocol Implementation
 *
 * Copyright (C) 2008, Siddhesh Poyarekar and the Ayttm Team
 *
 * Some code Copyright (C) 2001, Erik Inge Bolso <knan@mo.himolde.no>
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


#ifndef _LIB_IRC_H_
#define _LIB_IRC_H_

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "intl.h"

#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif


/* RFC1459 and RFC2812 defines max message size
   including \n == 512 bytes, but being tolerant of
   violations is nice nevertheless. */
#define BUF_LEN 512*2


/* from util.c :
 *
 * The last state in the list of states will be the OFFLINE state
 * The first state in the list will be the ONLINE states
 * The rest of the states are the various AWAY states
 */

enum {
	IRC_ONLINE = 0,
	IRC_AWAY,
	IRC_INVISIBLE,
	IRC_OFFLINE,
	IRC_STATE_COUNT
};

enum {
	IRC_SIGNON_ONLINE = 0,
	IRC_SIGNON_INVISIBLE = 8
};

static char *irc_states[] =
{
	"Online",
	"Away",
	"Invisible",
	"Offline"
};

/* User modes */
enum {
	IRC_USER_MODE_AWAY = 1,
	IRC_USER_MODE_INVISIBLE = 1<<1,
	IRC_USER_MODE_RECEIVE_WALLOPS = 1<<2,
	IRC_USER_MODE_RESTRICT_USER_CONNECTION = 1<<3,
	IRC_USER_MODE_OPER = 1<<4,
	IRC_USER_MODE_LOCAL_OPER = 1<<5,
	IRC_USER_MODE_RECEIVE_SERVER_NOTICES = 1<<6
};

/* Message Prefix */
typedef struct {
	char *nick;
	char *servername;
	char *user;
	char *hostname;
} irc_message_prefix ;

/* Parameter List element */
typedef struct _irc_param_list {
	char *param;
	struct _irc_param_list *next;
} irc_param_list ;

static char irc_modes[] =
{
	'a',
	'i',
	'w',
	'r',
	'o',
	'O',
	's'
};


struct _irc_callbacks;

typedef struct {
	char  connect_address [255];
	char  port [16];
	char *nick;
	char *servername;
	char *hostname;
	char *user;
	int   fd;

	int   status;
	char *usermodes;
	char *channelmodes;

	struct _irc_callbacks *callbacks;

	/* Store partial messages in this */
	char buf[BUF_LEN];
	int len;

	void *data ;
} irc_account ;


typedef struct _namelist {
	char *name ;
	char attribute ;
	struct _namelist *next ;
} irc_name_list ;


typedef struct _irc_callbacks {

	void (*incoming_notice)		(const char *to, 
					const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
                                
	void (*buddy_quit)		(const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*buddy_join)		(const char *channel, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*buddy_part)		(const char *channel, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*got_invite)		(const char *to, 
					const char *channel, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*got_kick)		(const char *to, 
					const char *channel, 
					const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*nick_change)		(const char *newnick, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*got_privmsg)		(const char *recipient, 
					const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
                                
	void (*got_ping)		(const char *server, 
					irc_account *ia);
	                        	
	void (*got_welcome)		(const char *nick, 
					const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);

	void (*got_yourhost)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                
	void (*got_created)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
	                        	
	void (*got_myinfo)		(const char *servername, 
					const char *version,
					irc_message_prefix *prefix,
					irc_account *ia);

	void (*got_channel_list)	(const char *me, 
					const char *channel, 
					int users, 
					const char *topic, 
					irc_message_prefix *prefix,
					irc_account *ia);
	
	void (*got_channel_listend)	(const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);

	void (*got_away)		(const char *from, 
					const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        
	void (*got_unaway)		(const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
	                        	
	void (*got_nowaway)		(const char *message, 
					irc_message_prefix *prefix, 
					irc_account *ia);
                                	
	void (*got_ison)		(const char *userlist,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_whoisidle)		(const char *from,
					int since,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_whoisuser)		(const char *nick,
					const char *user,
					const char *host,
					const char *real_name,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_whoisserver)		(const char *nick,
					const char *server,
					const char *info,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_whoisop)	        (const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_whowasuser)		(const char *nick,
					const char *user,
					const char *host,
					const char *real_name,
					irc_message_prefix *prefix,
					irc_account *ia);
                                
	void (*got_endofwhois)		(const char *nicks,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_endofwhowas)		(const char *nicks,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_notopic)		(const char *channel,
					const char *message,
				        irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_topic)		(const char *channel,
					const char *topic,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*inviting)		(const char *channel,
					const char *nick,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*summoning)		(const char *user,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_version)		(const char *version,
					const char *debuglevel,
					const char *server,
					const char *comments,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_info)		(const char *info,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_endinfo)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                
	void (*motd_start)		(const char *server,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_motd)		(const char *motd,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*endofmotd)		(const char *message,
				        irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_endofwho)		(const char *name,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_endofnames)		(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*youreoper)		(const char *message,
				        irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*got_namelist)		(irc_name_list *list,
					const char *channel,
					irc_message_prefix *prefix,
					irc_account *ia);

	void (*got_time)		(const char *time,
					irc_message_prefix *prefix,
					irc_account *ia);
                                	
	void (*irc_error)		(const char *message,
				        void *data);
                                	
	void (*irc_warning)		(const char *message,
					void *data);
                                	
	void (*client_quit)		(irc_account *ia);
        
	/* ERROR HANDLERS */

	void (*irc_no_such_nick)	(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_such_server)	(const char *server,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_such_channel)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_cant_sendto_chnl)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_was_no_such_nick)	(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_too_many_chnl)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_too_many_targets)	(const char *target,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_origin)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_recipient)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_text_to_send)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_toplevel)		(const char *mask,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_wild_toplevel)	(const char *mask,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_unknown_cmd)		(const char *command,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_motd)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_admininfo)	(const char *server,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_file_error)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_nick_given)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_erroneous_nick)	(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_nick_in_use)		(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_nick_collision)	(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_user_not_in_chnl)	(const char *nick,
					const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_not_on_chnl)		(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_on_chnl)		(const char *user,
					const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_nologin)		(const char *nick,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_summon_disabled)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_user_disabled)	(const char *user_disabled,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_not_registered)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_need_more_params)	(const char *cmd,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_already_registered)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_perm_for_host)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_password_mismatch)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_banned)		(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_chnl_key_set)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_channel_full)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_unknown_mode)	(const char *mode,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_invite_only)		(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_banned_from_chnl)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_bad_chnl_key)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_privileges)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_chnlop_privs_needed)	(const char *channel,
					const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_cant_kill_server)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_no_o_per_host)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_mode_unknown)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

	void (*irc_user_dont_match)	(const char *message,
					irc_message_prefix *prefix,
					irc_account *ia );

} irc_callbacks ;


void irc_message_parse (char *incoming, irc_account *ia) ;

/* Local prototypes */

void irc_login(const char *password, int mode, irc_account *ia);
void irc_logout( irc_account *ia );

void irc_send_privmsg( const char *recipient, char *message, irc_account *ia) ;
void irc_send_notice( const char *recipient, char *message, irc_account *ia) ;

void irc_send_whois( const char *target, const char *mask, irc_account *ia );

void irc_get_names_list( const char *channel, irc_account *ia );

void irc_process_privmsg(const char *to, const char *message, irc_message_prefix *prefix, irc_account *ia);
void irc_process_notice(const char *to, const char *message, irc_message_prefix *prefix, irc_account *ia);

void irc_set_away( char * message, irc_account *ia );
void irc_set_mode(int irc_mode, irc_account *ia);
void irc_unset_mode(int irc_mode, irc_account *ia);
void irc_join (const char *room, irc_account *ia);
void irc_leave_chat_room(const char *room, irc_account *ia);
void irc_send_invite( const char *user, const char *room,
				const char * message, irc_account *ia );
void irc_send_pong(const char *servername, irc_account *ia);

/* Add and return the new head of list */
irc_param_list *irc_param_list_add(irc_param_list *param_list, const char *param);
char *irc_param_list_get_at(irc_param_list *param_list, int position);

/* Get the parameter at */
char *irc_param_list_get_at(irc_param_list *list, int offset);

void irc_param_list_free(irc_param_list *param_list ) ;

irc_name_list *irc_gen_name_list(char *message);

void irc_recv (irc_account *ia, int source, int condition) ;

void irc_send_data(void *buf, int len, irc_account *ia) ;

void irc_get_command_string(char *out, const char *recipient, char *command, char *params, irc_account *ia);

void irc_request_list ( const char *channel, const char *target, irc_account *ia );

#endif

