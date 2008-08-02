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
 * irc.h
 */


/* RFC1459 and RFC2812 defines max message size
   including \n == 512 bytes, but being tolerant of
   violations is nice nevertheless. */
#define BUF_LEN 512*2

#define AY_IRC_GET_HANDLE(handle,nick,addr,size) {\
	strncpy(handle, nick, size);\
	strncat(handle, "@", size-strlen(handle));\
	strncat(handle, addr, size-strlen(handle));\
}

#define DBG_IRC do_irc_debug

typedef struct irc_local_account_type
{
	char		password[MAX_PREF_LEN];
	int		fd_tag;			
	int		keepalive_tag;
	int		connect_tag;
	int		activity_tag;
	char  *		buff;
	int		buff_user;
	LList *		friends;
	LList *		channel_list;
	LList *	current_rooms;
	irc_account *ia;
	void (*got_public_chatrooms) (LList *list, void *data);
	void *public_chatroom_callback_data;
} irc_local_account;

typedef struct irc_account_type
{
	char		server[255];
	char		realserver[255];
	int		status;
	int		idle;
	gboolean	dummy;
} ay_irc_account;

typedef struct _irc_info
{
	char 		*whois_info;
	eb_account 	*me;
	char		*fullmessage;
} irc_info;

static int is_setting_state = 0;

/* Denotes the end of list in irc_command_action array (irc_ca) ... */
#define IRC_END_COMMAND         ""

/* Max command and action strings length in an irc_command_action struct */
#define MAX_IRC_COMMAND_LEN 255
#define MAX_IRC_ACTION_LEN 255

/* IRC_COMMAND_TYPE: type of command in an irc_command_action struct. */
enum IRC_COMMAND_TYPE
{
    IRC_COMMAND_BUILTIN = 1,
    IRC_COMMAND_USER_DEF = 2
};

typedef struct irc_command_action_type
{
    char command[MAX_IRC_COMMAND_LEN];
    char action[MAX_IRC_ACTION_LEN];
    enum IRC_COMMAND_TYPE command_type;
} irc_command_action;


/* Local prototypes */
static unsigned char *strip_color (unsigned char *text);
static int ay_irc_query_connected (eb_account * account);
static void ay_irc_login( eb_local_account * account);
static void ay_irc_logout( eb_local_account * ela );
static void ay_irc_send_im( eb_local_account * account_from, eb_account * account_to,char *message );
static eb_local_account * ay_irc_read_local_config(LList * pairs);
static LList * ay_irc_write_local_config( eb_local_account * account );
static eb_account * ay_irc_read_config(eb_account *ea, LList *config );
static LList * ay_irc_get_states();
static int ay_irc_get_current_state(eb_local_account * account );
static void ay_irc_set_current_state(eb_local_account * account, int state );
static char * ay_irc_check_login(const char * user, const char * pass);
static void ay_irc_add_user( eb_account * account );
static void ay_irc_del_user( eb_account * account );
static int ay_irc_is_suitable (eb_local_account *local, eb_account *remote);
static eb_account * ay_irc_new_account(eb_local_account *ela, const char * account );
static char * ay_irc_get_status_string( eb_account * account );
static const char ** ay_irc_get_status_pixmap( eb_account * account);
static void ay_irc_set_idle(eb_local_account * account, int idle );
static void ay_irc_set_away( eb_local_account * account, char * message, int away);
static void ay_irc_send_file( eb_local_account * from, eb_account * to, char * file );
static void irc_info_update(info_window * iw);
static void irc_info_data_cleanup(info_window * iw);
static void ay_irc_get_info( eb_local_account * account_from, eb_account * account_to);
static void ay_irc_join_chat_room(eb_chat_room *room);
static void ay_irc_leave_chat_room(eb_chat_room *room);
static void ay_irc_send_chat_room_message(eb_chat_room *room, char *message);
static void ay_irc_send_invite( eb_local_account * account, eb_chat_room * room, char * user, const char * message);
static eb_chat_room * ay_irc_make_chat_room(char * name, eb_local_account * account, int is_public);
static eb_chat_room *ay_irc_make_chat_room_window(char *name, eb_local_account *account, int is_public, int send_join);
static void ay_irc_accept_invite( eb_local_account * account, void * invitation );
static void ay_irc_decline_invite( eb_local_account * account, void * invitation );
static void eb_irc_read_prefs_config(LList * values);
static LList * eb_irc_write_prefs_config();
static void irc_connect_cb(int fd, int error, void *data);

void irc_finish_login(eb_local_account *ela);
static irc_callbacks *ay_irc_map_callbacks( void ) ;

