/*
 * Ayttm 
 *
 * Copyright (C) Colin Leroy <colin@colino.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "intl.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

#ifdef __MINGW32__
#include <winsock2.h>
#include <sys/poll.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <gmp.h>

#if defined( _WIN32 )
typedef unsigned long u_long;
typedef unsigned long ulong;
#endif
#include "workwizu.h"
#include "contact.h"
#include "service.h"
#include "chat_window.h"
#include "chat_room.h"
#include "util.h"
#include "status.h"
#include "message_parse.h"
#include "value_pair.h"
#include "plugin_api.h"
#include "smileys.h"
#include "globals.h"
#include "tcp_util.h"
#include "activity_bar.h"
#include "messages.h"

#include "libproxy/libproxy.h"

#include "pixmaps/workwizu_online.xpm"
#include "pixmaps/workwizu_away.xpm"

#define WWZ_ONLINE 0
#define WWZ_OFFLINE 1

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/

#ifndef USE_POSIX_DLOPEN
	#define plugin_info workwizu_LTX_plugin_info
	#define SERVICE_INFO workwizu_LTX_SERVICE_INFO
	#define plugin_init workwizu_LTX_plugin_init
	#define plugin_finish workwizu_LTX_plugin_finish
	#define module_version workwizu_LTX_module_version
#endif

unsigned int module_version() {return CORE_VERSION;}


int plugin_init();
int plugin_finish();

int do_wwz_debug = 0;
#define DBG_WWZ do_wwz_debug

static int ref_count = 0;
static tag_info tags[21];

static char server[MAX_PREF_LEN] = "212.37.193.123";
static char port[MAX_PREF_LEN] = "32002";
static char owner[MAX_PREF_LEN] = "savoirweb";

static wwz_user *my_user;

LList *wwz_contacts = NULL;

int connstate;

static int reset_typing (gpointer sockp);

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"Workwizu",
	"Provides Workwizu Chat support",
	"$Revision: 1.17 $",
	"$Date: 2009/07/27 16:42:04 $",
	&ref_count,
	plugin_init,
	plugin_finish
};

struct service SERVICE_INFO = { "Workwizu", -1, FALSE, TRUE, FALSE, TRUE, NULL };
/* End Module Exports */

int plugin_init()
{
	int count;
	input_list *il = g_new0(input_list, 1);
	
	eb_debug(DBG_WWZ, "Workwizu init\n");
	
	ref_count=0;
	for(count=0;count<20;count++)
		tags[count].fd=-1;

	plugin_info.prefs = il;

	il->widget.entry.value = server;
	il->name = "server";
	il->label = _("Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = port;
	il->name = "port";
	il->label = _("Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.entry.value = owner;
	il->name = "owner";
	il->label = _("Distbase:");
	il->type = EB_INPUT_ENTRY;
	
	il->next = g_new0(input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_wwz_debug;
	il->name = "do_wwz_debug";
	il->label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

	return(0);
}

int plugin_finish()
{
	eb_debug(DBG_WWZ, "Returning the ref_count: %i\n", ref_count);
	return(ref_count);
}

char *sock_read (int sock)
{
  char c;
  char buf[4096];
  char *line = NULL;
  int pos = 0;
  while(pos < 4095)
  {
    if(read(sock, &c, 1)<1)
    {
      eb_debug(DBG_WWZ,"What the..?!\n"); //DEBUG
      return NULL;
    }
    if(c=='\r') { continue; }
    if(c=='\n') { buf[pos]='\0'; break; }
    buf[pos]=c;
    pos++;
  }
  line = strdup(buf);
  eb_debug(DBG_WWZ,"READ :%s\n",line);
  return line;
}

void sock_write (int sock, char *buf)
{
	eb_local_account *account;
	eb_debug(DBG_WWZ,"WRITE:%s\n",buf);
	if (write(sock, buf, strlen(buf)) == -1) {
		perror("write");
		account = find_local_account_by_handle(my_user->username, SERVICE_INFO.protocol_id);
		ay_do_error( _("Workwizu Error"), _("Connection closed (can't write)") );
		eb_workwizu_logout(account);
	}	
}
int eb_workwizu_query_connected (eb_account *account)
{
	return (ref_count > 0 && account->online);
}

unsigned long long getHash (char *s) 
{
	long long hash = 0;
	unsigned long long res = 0;
	int i = 0;
	for (i=0; i < strlen(s); i++) {
		int c = (int)s[i];
		int x = strlen(s);
		int l = x-(i+1);
		long long p = pow(31,l);
		hash += c*p;  /* 31^(strlen(s)-(1+1)) */
	}
	if (hash < 0) res = -hash;
	else res = hash;
	eb_debug(DBG_WWZ,"getHash %llu \n", res);
	return res;
}

char *compute_challenge() 
{
	char *result_c = NULL;
	unsigned long long hash;
	mpz_t beta;
	mpz_t result;
	mpz_t challenge;
	
	mpz_init(beta);
	mpz_init(result);
	mpz_init(challenge);
	
	mpz_set_str(beta, "1201147206560481999739023090913", 10);
	mpz_set_str(challenge, my_user->challenge, 10);
	hash = getHash(my_user->password);

	hash %= 10000;
	
	mpz_pow_ui(result, challenge, hash);

	mpz_tdiv_r (result, result, beta);
	result_c = mpz_get_str(NULL, 10, result);
	return result_c;	
}

void workwizu_answer_challenge (int sock)
{
	char *buf;
	buf = g_strdup_printf("%s%%%s%%0%%0%%0%%%s\n",
			      my_user->username,
			      compute_challenge(),
			      owner);
	sock_write(sock, buf);		
	g_free(buf);
	connstate = CONN_WAITING_USERINFO;
}

void parse_user_info (char *info)
{
	char **tokens;
	/* 0 user % 1 challenge_answer % 2 u_id % 3 is_driver % 4 channel_id % 5 owner */
	tokens = g_strsplit(info, "%", 6);
	my_user->uid = atoi(tokens[2]);
	my_user->is_driver = my_user->has_speak = atoi(tokens[3]);
	my_user->channel_id = atoi(tokens[4]);
	strncpy(my_user->owner,tokens[5], 255);
	g_strfreev(tokens);
	connstate = CONN_ESTABLISHED;
}

void send_packet (int sock, int tag, int from, int to, char *str)
{
	char *buf = g_strdup_printf("%d:%d:%d:%s\n",
				     tag, from, to,
				     str);
	sock_write(sock, buf);
	g_free(buf);
}	

void send_my_packet (int sock, int tag, int to, char *str)
{
	send_packet (sock, tag, my_user->uid, to, str);
}	

static int send_stats(gpointer sockp) 
{
	char *buf;
	int sock = GPOINTER_TO_INT(sockp);
/*	true	|true	|false	|true	|false	|0	|false 
	driver	|speak	|micro	|pencil	|w_act	|lastmv	|istyping	*/ 
	
	buf = g_strdup_printf("%s|%s|false|false|true|0|%s",
			      my_user->is_driver?"true":"false",
			      my_user->has_speak?"true":"false",
			      my_user->is_typing?"true":"false"); 
	send_my_packet(sock, USERSTATE, EXCEPTME, buf);
	return 1;
}

static void parse_stats (int from, char *stats)
{
	char **tokens = g_strsplit(stats,"|",7);
	char *sfrom = g_strdup_printf("%d",from);
	eb_account *ea = find_account_by_handle(sfrom, SERVICE_INFO.protocol_id);
	wwz_user *user;
	
	if(!ea)
		return;
	
	user = (wwz_user *)ea->protocol_account_data;
	user->is_driver = !strcmp(tokens[0],"true");
	user->is_typing = (tokens[6] != NULL && !strcmp(tokens[6],"true"));
	user->has_speak = !strcmp(tokens[1],"true");
	buddy_update_status(ea);
	if (user->is_typing && iGetLocalPref("do_typing_notify"))
		eb_update_status(ea, _("typing..."));
	g_free(sfrom);
		
}

void get_message (int from, int to, char *data)
{
	eb_account *ea;
	eb_local_account *ela;
	
	char *sfrom = g_strdup_printf("%d", from);

	
	ela = find_local_account_by_handle(my_user->username, SERVICE_INFO.protocol_id);
	ea = find_account_by_handle(sfrom, SERVICE_INFO.protocol_id);
	if (!ea) {
		eb_debug(DBG_WWZ, "no account found for %s\n",sfrom);
		g_free(sfrom);
		return;
	}
	if (!ela) {
		eb_debug(DBG_WWZ, "no local account found for %s\n",my_user->username);
		g_free(sfrom);
		return;
	}
	if (to > CHANNEL || to < NOTDEFINED)
		eb_parse_incoming_message(ela, ea, data);
	else {
		wwz_account_data *wad = (wwz_account_data *)ela->protocol_local_account_data;
		if (wad->chat_room == NULL) {
			char *name = next_chatroom_name();
			eb_start_chat_room(SERVICE_INFO.protocol_id, name);
			free(name);
		}
		eb_chat_room_show_message(wad->chat_room, ea->account_contact->nick, data);
	}
	g_free(sfrom);
}	

void new_user (int from, char *data)
{
	eb_account *ea;
	char *sfrom = g_strdup_printf("%d", from);
	
	ea = find_account_by_handle(sfrom, SERVICE_INFO.protocol_id);
	if (!ea) {
	  ea = eb_workwizu_new_account(sfrom);
	  if (!find_grouplist_by_name("Workwizu")) {
		add_group("Workwizu");  
	  }
	  add_unknown_with_name(ea, strdup(data));
	  g_free(sfrom);
	  move_contact("Workwizu", ea->account_contact);
	  update_contact_list();
	  write_contact_list();
        }
	eb_workwizu_add_user(ea);
	buddy_login(ea);
}

void rem_user (int from)
{
	char *sfrom = g_strdup_printf("%d", from);
	eb_account *ea = find_account_by_handle(sfrom, SERVICE_INFO.protocol_id);
	if (ea) {
		buddy_logoff(ea);
		eb_workwizu_del_user(ea);
	}
	g_free(sfrom);
}

void parse_packet(char *packet)
{
	char **tokens;
	int tag, from, to;
	char *data;

  	/* 0 tag : 1 from : 2 to : 3 data */
	tokens = g_strsplit(packet, ":", 3);
	tag = atoi(tokens[0]);
	from = atoi(tokens[1]);
	to = atoi(tokens[2]);
	data = g_strdup(tokens[3]);
	g_strfreev(tokens);
	eb_debug(DBG_WWZ,"tag:%d, to:%d,from:%d,data:%s\n",
			  tag,to,from,data);
	switch(tag) {
		case NEWUSER:
			new_user(from, data);
			free(data);
			break;
		case REMOVEUSER:
			rem_user(from);
			break;
		case CHAT:
			get_message(from, to, data);
			break;
		case ALLOWSPEAK:
			eb_debug(DBG_WWZ,"yeah, right to speak\n");
			my_user->has_speak = 1;
			break;
		case STOPSPEAK:
			eb_debug(DBG_WWZ,"server told me to shut up\n");
			my_user->has_speak = 0;
			break;
		case USERSTATE:
			parse_stats(from, data);
			break;
		default:
			break;
	}
}

void workwizu_handle_incoming(int sock, int readable, int writable)
{
	char *line;
	eb_local_account *account = find_local_account_by_handle(my_user->username, SERVICE_INFO.protocol_id);
	if(!readable)
		return;
	switch(connstate) {
		case CONN_CLOSED: 
			eb_debug(DBG_WWZ,"uh? conn is closed\n");
			break;
		case CONN_WAITING_CHALLENGE:
			line = sock_read(sock);
			if (line == NULL) {
				ay_do_error( _("Workwizu Error"), _("Server doesn't answer.") );
				eb_workwizu_logout(account);
				return;
			}
			strncpy(my_user->challenge,line, sizeof(my_user->challenge));
			free(line);
			eb_debug(DBG_WWZ,"read challenge %s\n",my_user->challenge);
			workwizu_answer_challenge(sock);
			break;
		case CONN_WAITING_USERINFO:
			line = sock_read(sock);
			if (line == NULL) {
				ay_do_error( _("Workwizu Error"), _("Bad authentication.") );
				eb_workwizu_logout(account);
				return;
			}
			if (strncmp(line, my_user->username, strlen(my_user->username))) {
				eb_debug(DBG_WWZ, "got normal packet before auth result :(\n");
				parse_packet(line);
				free(line);
				return;
			}
			parse_user_info(line);
			if (my_user->is_driver) 
				send_my_packet (sock, 
						DRIVERENTEREDINCHATROOM,
						EXCEPTME,
						my_user->username);
					    
			free(line);
			my_user->statssender = 
					eb_timeout_add	(10*60*1000, /* 10 minutes */
					(eb_timeout_function)send_stats, GINT_TO_POINTER(sock));
			break;
		case CONN_ESTABLISHED:
			line = sock_read(sock);
			if (line == NULL) {
				char *buf = g_strdup_printf(_("Connection to %s lost."),server); 
				ay_do_error( _("Workwizu Error"), buf );
				g_free(buf);
				eb_workwizu_logout(account);
				return;
			}
			parse_packet(line);
			free(line);
			break;
		default:
			eb_debug(DBG_WWZ,"unknown connection state ! :(\n");
			break;
	}
}

void eb_workwizu_incoming(void *data, int source, eb_input_condition condition)
{
        if (!(condition&EB_INPUT_EXCEPTION)) {
		workwizu_handle_incoming(source, 
					 condition & EB_INPUT_READ, 
					 condition & EB_INPUT_WRITE);
        }
}

void register_sock(int s, int reading, int writing)
{
  int a;
  eb_debug(DBG_WWZ, "Registering sock %i\n", s);
  for(a=0; a<20; a++)
  {
    if(tags[a].fd==-1) {
	tags[a].fd=s;

        tags[a].tag_r=tags[a].tag_w=-1;

        if(reading)
        {
	  tags[a].tag_r=eb_input_add(s, EB_INPUT_READ, eb_workwizu_incoming, NULL);
        }

        if(writing)
        {
	  tags[a].tag_w=eb_input_add(s, EB_INPUT_WRITE, eb_workwizu_incoming, NULL);
        }

        eb_debug(DBG_WWZ, "Successful %i\n", s);
	return;
    }
  }
}

void unregister_sock(int s)
{
  int a;
  eb_debug(DBG_WWZ, "Unregistering sock %i\n", s);
  for(a=0; a<20; a++)
  {
    if(tags[a].fd==s) {
        if(tags[a].tag_r!=-1) { eb_input_remove(tags[a].tag_r); }
        if(tags[a].tag_w!=-1) { eb_input_remove(tags[a].tag_w); }
	tags[a].fd=-1;
	tags[a].tag_r=tags[a].tag_w=0;
        eb_debug(DBG_WWZ, "Successful %i\n", s);
	return;
    }
  }
}

#if 0 /* now async */
static int connect_socket(char * hostname, int port)
{
  struct sockaddr_in sa;
  struct hostent     *hp;
  int s;
  eb_debug(DBG_WWZ,"Connecting to %s...\n", hostname);
  if ((hp= gethostbyname(hostname)) == NULL) { /* do we know the host's */
#ifndef __MINGW32__
    errno= ECONNREFUSED;                       /* address? */
#endif
    return(-1);                                /* no */
  }

  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr,hp->h_addr,hp->h_length);     /* set address */
  sa.sin_family= hp->h_addrtype;
  sa.sin_port= htons((u_short)port);

  if ((s= socket(hp->h_addrtype,SOCK_STREAM,0)) < 0)     /* get socket */
    return(-1);

#ifdef O_NONBLOCK
  fcntl(s, F_SETFL, O_NONBLOCK);
#endif

  if (connect(s,(struct sockaddr *)&sa,sizeof sa) < 0) /* connect */
  {
#ifndef __MINGW32__
    if(errno==EINPROGRESS || errno==EWOULDBLOCK)
    {
      struct pollfd pfd;
      pfd.fd=s;
      pfd.revents=0;
      pfd.events=POLLOUT;

      fcntl(s, F_SETFL, 0);
      if(poll(&pfd, 1, 7500)==1)
      {
        if(pfd.revents&POLLERR||pfd.revents&POLLHUP||pfd.revents&POLLNVAL)
        {
          eb_debug(DBG_WWZ, "Error!\n");
          close(s);
          return -1;
        } else {
          eb_debug(DBG_WWZ, "Connect went fine\n");
          sleep(2);
          return s;
        }
      }
    } else {
      fcntl(s, F_SETFL, 0);
#endif
      close(s);
      return -1;
#ifndef __MINGW32__
    }
#endif
  }
  sleep(1); // necessary?
  return(s);
}
#endif 

void eb_workwizu_connected (int fd, int error, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	wwz_account_data *wad = (wwz_account_data *) account->protocol_local_account_data;

	wad->sock = fd;
	
	ay_activity_bar_remove(wad->activity_tag);
	wad->activity_tag = 0;
	
	if (wad->sock == -1) {
		eb_debug(DBG_WWZ, "wad->sock=-1 !\n");
		if (error) {
		ay_do_error( _("Workwizu Error"), _("Server doesn't answer.") );
		}
		account->connected=0;
		eb_workwizu_logout(account);
		return;
	}
	ref_count++;
	if(account->status_menu)
	{
		/* Make sure set_current_state doesn't call us back */
		account->connected=-1;
		eb_set_active_menu_status(account->status_menu, WWZ_ONLINE);
	}
	account->connected=1;
	account->connecting=0;
	connstate = CONN_WAITING_CHALLENGE;
	register_sock(wad->sock, 1, 0);
}

static void ay_wwz_cancel_connect(void *data)
{
	eb_local_account *ela = (eb_local_account *)data;
	wwz_account_data * wad;
	wad = (wwz_account_data *)ela->protocol_local_account_data;
	eb_debug(DBG_WWZ, "cancelling conn %d\n",wad->connect_tag);
	ay_socket_cancel_async(wad->connect_tag);
	wad->activity_tag=0;
	eb_workwizu_logout(ela);
}

void eb_workwizu_login (eb_local_account *account)
{
	wwz_account_data *wad = (wwz_account_data *) account->protocol_local_account_data;
	char buff[1024];
	int res;
	if (account->connected || account->connecting) 
		return;
	
	snprintf(buff, sizeof(buff), _("Logging in to Workwizu account: %s"), 
			account->handle);
	wad->activity_tag = ay_activity_bar_add(buff, ay_wwz_cancel_connect, account);
	
	account->connecting = 1;
	my_user = g_new0(wwz_user, 1);
	strncpy(my_user->username, account->handle, sizeof(my_user->username));
	strncpy(my_user->password, wad->password, sizeof(my_user->password));

	my_user->typing_handler = -1;
	
	eb_debug(DBG_WWZ, "Logging in\n");
	if ((res = proxy_connect_host(server, atoi(port), 
			(ay_socket_callback)eb_workwizu_connected, account, NULL)) < 0) {
		eb_debug(DBG_WWZ, "cant connect socket");
		ay_do_error( _("Workwizu Error"), _("Server doesn't answer.") );
		ay_activity_bar_remove(wad->activity_tag);
		wad->activity_tag = 0;
		eb_workwizu_logout(account);
		return;
	}
	wad->connect_tag = res;
	
}

static void clean_up (int sock) 
{
	if (sock != -1) {
		if (my_user->statssender != -1) 
			eb_timeout_remove(my_user->statssender);
		my_user->statssender = -1;
			
		close(sock);
		unregister_sock(sock);
		connstate = CONN_CLOSED;
	}
}

void eb_workwizu_logout (eb_local_account *account)
{  
	LList *l = NULL;
	wwz_account_data *wad = (wwz_account_data *) account->protocol_local_account_data;
	if (!account->connected && !account->connecting) 
		return;
	eb_debug(DBG_WWZ, "Logging out\n");
	l = wwz_contacts;
	while (l && l->data) {
		LList *next = l->next;
		eb_account * ea = NULL;
		
		if (l)
			ea = (eb_account *)find_account_by_handle((char *)l->data, SERVICE_INFO.protocol_id);
		if(ea) {
			buddy_logoff(ea);
			buddy_update_status(ea);
			eb_workwizu_del_user(ea);
		}
		l = next;
	}
	account->connected = account->connecting = 0;
	eb_set_active_menu_status(account->status_menu, WWZ_OFFLINE);
	clean_up(wad->sock);
	if(ref_count >0)
		ref_count--;
	
}

char *translate_to_br(char *in)
{
	char **tokens = g_strsplit(in, "\r\n", 0);
	char *out = NULL;
	int i = 0;
	while (tokens[i]) {
		char *tok = NULL;
		
		tok = g_strdup(tokens[i]);
		
		if (out == NULL)
			out = g_strdup(tok);
		else {
			char *old = g_strdup(out);
			out = g_strconcat(old, "<br>", tok, NULL);
			g_free(old);
		}
		g_free(tok);
		i++;
	}
	g_strfreev(tokens);
	return out;
}

void eb_workwizu_send_im (eb_local_account *account_from, 
			  eb_account *account_to,
			  char *message)
{
	wwz_account_data *wad = (wwz_account_data *)account_from->protocol_local_account_data;
	wwz_user *user = (wwz_user *)account_to->protocol_account_data;
	char *send = translate_to_br(message);
	
	reset_typing(GINT_TO_POINTER(wad->sock));
	
	send_my_packet(wad->sock, CHAT, user->uid, send);
	if (!my_user->has_speak)
		ay_do_error( _("Workwizu Error"), _("You aren't allowed to speak.\nThis message has probably not arrived.") );
	g_free(send);
}

int eb_workwizu_send_typing (eb_local_account *account_from,
			     eb_account *account_to)
{
	wwz_account_data *wad = (wwz_account_data *)account_from->protocol_local_account_data;
	my_user->is_typing = 1;
	send_stats(GINT_TO_POINTER(wad->sock));
	if (my_user->typing_handler != -1)
		eb_timeout_remove(my_user->typing_handler);
	my_user->typing_handler = eb_timeout_add(5000, (eb_timeout_function)reset_typing, GINT_TO_POINTER(wad->sock));
	return 4;
}

int eb_workwizu_send_cr_typing (eb_chat_room *room)
{
	eb_local_account *account_from = (eb_local_account *)room->local_user;
	return eb_workwizu_send_typing(account_from, NULL);
}

static int reset_typing (gpointer sockp) {
	my_user->is_typing = 0;
	send_stats(sockp);
	my_user->typing_handler = -1;
	return 0;
}

eb_local_account *eb_workwizu_read_local_config (LList *values)
{
	eb_local_account *ela;
	char *str;
	wwz_account_data *wad = g_new0(wwz_account_data,1);
	if (values == NULL)
		return NULL;
	
	ela = g_new0 (eb_local_account, 1);
	
	ela->handle   = value_pair_get_value(values, "SCREEN_NAME");
	wad->password = value_pair_get_value(values, "PASSWORD");
	
	str = value_pair_get_value(values,"CONNECT");
	ela->connect_at_startup=(str && !strcmp(str,"1"));
	free(str);

	ela->protocol_local_account_data = wad;
	
	strncpy(ela->alias, ela->handle, 255);
	
	ela->service_id = SERVICE_INFO.protocol_id;
	ela->connected = FALSE;
	return ela;	
}

LList *eb_workwizu_write_local_config (eb_local_account *account)
{
	value_pair *pair = NULL;
	LList *vals = NULL;
	wwz_account_data *wad = (wwz_account_data *)account->protocol_local_account_data;
	
	pair = g_new0(value_pair, 1);
	strcpy(pair->key, "SCREEN_NAME");
	strncpy(pair->value, account->handle, 1024);	
	vals = l_list_append(vals, pair);
	
	pair = g_new0(value_pair, 1);
	strcpy(pair->key, "PASSWORD");
	strncpy(pair->value, wad->password, 1024);	
	vals = l_list_append(vals, pair);
	
	pair = g_new0( value_pair, 1 );
	strcpy( pair->key, "CONNECT" );
	if (account->connect_at_startup)
		strcpy( pair->value, "1");
	else 
		strcpy( pair->value, "0");
	
	vals = l_list_append( vals, pair );

	return vals;	
}

eb_account *eb_workwizu_read_config (eb_account *ea, LList *config)
{
	wwz_user *user = g_new0(wwz_user,1);
	
	user->uid = atoi(ea->handle);
	user->typing_handler = -1;
	
	ea->protocol_account_data = user;
	eb_debug(DBG_WWZ,"eb_workwizu_read_config for %s\n",ea->handle);
	return ea;
}

LList *eb_workwizu_get_states (void) 
{
	LList *status_list = NULL;
	status_list = l_list_append(status_list, "Online");
	status_list = l_list_append(status_list, "Offline");
	return status_list;
}

gint eb_workwizu_get_current_state (eb_local_account *account)
{
	if (account->connected)
		return WWZ_ONLINE;
	else 
		return WWZ_OFFLINE;
}

void eb_workwizu_set_current_state (eb_local_account *account, gint state)
{
	if (!account)
		return;
	
	if (state == WWZ_OFFLINE && account->connected)
		eb_workwizu_logout(account);
	else if (state == WWZ_ONLINE && !account->connected)
		eb_workwizu_login(account);
	
}

char *eb_workwizu_check_login (char *loginname, char *pass)
{
	if (strlen(loginname) == 0)
		return g_strdup(_("The login is empty."));
	return NULL;
}

void eb_workwizu_add_user (eb_account *account) 
{
	eb_local_account *account_from = find_local_account_by_handle(my_user->username, SERVICE_INFO.protocol_id);
	wwz_account_data *wad = (wwz_account_data *)account_from->protocol_local_account_data;
	if (account == NULL)
		return;
	wwz_contacts = l_list_append(wwz_contacts, account->handle);
	if (wad->chat_room)
		eb_chat_room_buddy_arrive(wad->chat_room, account->account_contact->nick, account->handle);
	/* nothing to do server-side */
}

void eb_workwizu_del_user (eb_account *account)
{
	eb_local_account *account_from = find_local_account_by_handle(my_user->username, SERVICE_INFO.protocol_id);
	wwz_account_data *wad = NULL; 
	if (!account_from)
		return;
	wad = (wwz_account_data *)account_from->protocol_local_account_data;
	if (account == NULL) 
		return;
	wwz_contacts = l_list_remove(wwz_contacts, account->handle);
	if (wad->chat_room)
		eb_chat_room_buddy_leave(wad->chat_room, account->handle);
	/* nothing to do server-side */
}

eb_account * eb_workwizu_new_account (const char *account)
{
	eb_account *ea = g_new0(eb_account, 1);
	wwz_user *user = g_new0(wwz_user, 1);
	
	strncpy(ea->handle, account, 255);
	ea->service_id = SERVICE_INFO.protocol_id;
	ea->online = FALSE;
	user->uid = atoi(ea->handle);
	
	user->typing_handler = -1;
	
	ea->protocol_account_data = user;
	eb_debug(DBG_WWZ,"eb_workwizu_new_account for %s\n",ea->handle);
	return ea;
}

char *eb_workwizu_get_status_string (eb_account *account) 
{
	wwz_user *user = (wwz_user *)account->protocol_account_data;
	if (account->online && user->has_speak)
		return "";
	else if (!account->online)
		return _("Offline");
	else if (account->online && !user->has_speak)
		return _("No speak");
	else
		return "?";
}

char **eb_workwizu_get_status_pixmap(eb_account * account)
{
	wwz_user *user = (wwz_user *)account->protocol_account_data;

	if (account->online && user->has_speak) {
		return workwizu_online_xpm;
	} else {
		return workwizu_away_xpm;
	}		
}

void eb_workwizu_send_chat_room_message(eb_chat_room *room, char *message)
{
	eb_local_account *account_from = (eb_local_account *)room->local_user;
	wwz_account_data *wad = (wwz_account_data *)account_from->protocol_local_account_data;
	char *send = translate_to_br(message);
	
	if (!my_user->has_speak)
		ay_do_error( _("Workwizu Error"), _("You aren't allowed to speak.\nThis message has probably not arrived.") );
	else {
		reset_typing(GINT_TO_POINTER(wad->sock));
		send_my_packet(wad->sock, CHAT, EXCEPTME, send); /*CHANNEL or EXCEPTME*/
		eb_chat_room_show_message(room, my_user->username, send);
	}
	
	g_free(send);
}

void eb_workwizu_join_chat_room (eb_chat_room *room)
{
	LList *all = NULL;
	room->connected = TRUE;
	room->fellows = NULL;
	for (all = wwz_contacts; all != NULL && all->data != NULL; all = all->next) {
		eb_account * ea = (eb_account *)find_account_by_handle((char *)all->data, SERVICE_INFO.protocol_id);
		if (!ea) 
			return;
		eb_chat_room_buddy_arrive(room, ea->account_contact->nick, ea->handle);
	}
}

void eb_workwizu_leave_chat_room (eb_chat_room *room)
{
	LList *all = NULL;
	eb_local_account *account = (eb_local_account *)room->local_user;
	wwz_account_data *wad = (wwz_account_data *)account->protocol_local_account_data;
	for (all = wwz_contacts; all != NULL && all->data != NULL; all = all->next) {
		eb_chat_room_buddy_leave(room, (char*)all->data);
	}
	room->connected = FALSE;
	room->fellows = NULL;
	wad->chat_room = NULL;
}

eb_chat_room *eb_workwizu_make_chat_room(char *name, eb_local_account *account, int is_public)
{
	eb_chat_room * ecr = NULL;
	wwz_account_data *wad = (wwz_account_data *)account->protocol_local_account_data;
	
	if (wad->chat_room != NULL) {
		eb_debug(DBG_WWZ,"return existing CR %p\n",wad->chat_room);
		return wad->chat_room;
	}
	ecr = g_new0(eb_chat_room, 1);
	strncpy(ecr->room_name, name, 1024);
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;
	eb_debug(DBG_WWZ,"ecr->local_user %p\n",ecr->local_user);
	wad->chat_room = ecr;
	
	eb_join_chat_room(ecr, TRUE);
	eb_chat_room_buddy_arrive(ecr, account->handle, account->alias);
	return ecr;
}

void eb_workwizu_send_invite (eb_local_account *account, eb_chat_room *room, char *user, char *message) 
{
	/* nothing */
}

static char *eb_workwizu_get_color(void)
{
	static char color[]="#8a448b";
	return color;
}

void eb_workwizu_set_away (eb_local_account * account, char * message ) {
	
}

void eb_workwizu_set_idle (eb_local_account * account, gint idle ) {
	
}

struct service_callbacks * query_callbacks()
{
  struct service_callbacks * sc;
	
  sc = g_new0( struct service_callbacks, 1 );
  sc->query_connected = eb_workwizu_query_connected;
  sc->login = eb_workwizu_login;
  sc->logout = eb_workwizu_logout;
  sc->send_im = eb_workwizu_send_im;
  sc->send_typing = eb_workwizu_send_typing;
  sc->send_cr_typing = eb_workwizu_send_cr_typing;
  sc->write_local_config = eb_workwizu_write_local_config;
  sc->read_local_account_config = eb_workwizu_read_local_config;
  sc->read_account_config = eb_workwizu_read_config;
  sc->get_states = eb_workwizu_get_states;
  sc->get_current_state = eb_workwizu_get_current_state;
  sc->set_current_state = eb_workwizu_set_current_state;
  sc->check_login = eb_workwizu_check_login;
  sc->add_user = eb_workwizu_add_user;
  sc->del_user = eb_workwizu_del_user;
  sc->new_account = eb_workwizu_new_account;
  sc->get_status_string = eb_workwizu_get_status_string;
  sc->get_status_pixmap = eb_workwizu_get_status_pixmap;
  sc->send_chat_room_message = eb_workwizu_send_chat_room_message;
  sc->join_chat_room = eb_workwizu_join_chat_room;
  sc->leave_chat_room = eb_workwizu_leave_chat_room;
  sc->make_chat_room = eb_workwizu_make_chat_room;
  sc->send_invite = eb_workwizu_send_invite;
  sc->get_color=eb_workwizu_get_color;
  sc->get_smileys=eb_default_smileys;
  sc->set_away=eb_workwizu_set_away;
  sc->set_idle=eb_workwizu_set_idle;

  return sc;
}
