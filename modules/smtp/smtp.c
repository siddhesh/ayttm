/*
 * smtp.c
 */

/*
 * SMTP Extension for everybuddy 
 *
 * Copyright (C) 2002, Philip Tellis <philip.tellis@iname.com>
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

#include "intl.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "service.h"
#include "plugin_api.h"
#include "tcp_util.h"

#include "input_list.h"
#include "value_pair.h"
#include "util.h"

#include "message_parse.h"	/* eb_parse_incoming_message */
#include "status.h"		/* buddy_login,  */
				/* buddy_logoff,  */
				/* buddy_update_status */

void do_message_dialog(char *message, char *title, int modal);

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info smtp_LTX_plugin_info
#define SERVICE_INFO smtp_LTX_SERVICE_INFO

/* Function Prototypes */
static int plugin_init();
static int plugin_finish();
struct service_callbacks * query_callbacks();

static int is_setting_state = 0;

static int ref_count = 0;

static char smtp_host[MAX_PREF_LEN] = "127.0.0.1";
static char smtp_port[MAX_PREF_LEN] = "25";
static int do_smtp_debug = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info =
{
	PLUGIN_SERVICE,
	"SMTP Service",
	"SMTP Service Module",
	"$Revision: 1.4 $",
	"$Date: 2003/04/04 09:15:45 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
struct service SERVICE_INFO = {
	"SMTP",
	-1,
	TRUE,	/* all messages are offline */
	FALSE, 
	TRUE, 	/* true so i can prevent file transfer altogether */
	FALSE, 
	NULL
};
/* End Module Exports */

static int plugin_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	ref_count = 0;

	plugin_info.prefs = il;
	il->widget.entry.value = smtp_host;
	il->widget.entry.name = "smtp_host";
	il->widget.entry.label = _("SMTP Server:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = smtp_port;
	il->widget.entry.name = "smtp_port";
	il->widget.entry.label = _("Port:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.checkbox.value = &do_smtp_debug;
	il->widget.checkbox.name = "do_smtp_debug";
	il->widget.checkbox.label = _("Enable debugging");
	il->type = EB_INPUT_CHECKBOX;

	return (0);
}

static int plugin_finish()
{
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return (ref_count);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/


#ifdef __STDC__
int SMTP_DEBUGLOG(char *fmt,...)
#else
int SMTP_DEBUGLOG(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	vfprintf(stderr, fmt, ap);
	fflush(stderr);
	va_end(ap);
	return 0;
}


#define LOG(x) if(do_smtp_debug) { SMTP_DEBUGLOG("%s:%d: ", __FILE__, __LINE__); \
	SMTP_DEBUGLOG x; \
	SMTP_DEBUGLOG("\n"); }

#define WARNING(x) if(do_smtp_debug) { SMTP_DEBUGLOG("%s:%d: warning: ", __FILE__, __LINE__); \
	SMTP_DEBUGLOG x; \
	SMTP_DEBUGLOG("\n"); }


#define SMTP_MSG_COLOR	"#2080d0"
static char *eb_smtp_get_color(void) { return SMTP_MSG_COLOR; }

enum smtp_status_code { SMTP_STATUS_ONLINE, SMTP_STATUS_OFFLINE };
	
typedef struct eb_smtp_account_data {
	int status;		/* always online */
} eb_smtp_account_data;

typedef struct eb_smtp_local_account_data {
	char password[255];	/* in case of SMTP Auth? */
	int status;		/* always online */
} eb_smtp_local_account_data;

static LList * eb_smtp_buddies = NULL;

static int smtp_tcp_writeline(char *buff, int fd)
{
	int len = strlen(buff);
	int i;
	for (i=1; i<=2; i++)
		if(buff[len-i] == '\r' || buff[len-i] == '\n')
			buff[len-i]='\0';
	return ay_tcp_writeline(buff, strlen(buff), fd);
}

static eb_local_account *eb_smtp_read_local_account_config(LList * pairs)
{
	eb_local_account *ela;
	eb_smtp_local_account_data *sla;
	char	*str = NULL;

	if(!pairs) {
		WARNING(("eb_smtp_read_local_account_config: pairs == NULL"));
		return NULL;
	}

	ela = calloc(1, sizeof(eb_local_account));
	sla = calloc(1, sizeof(eb_smtp_local_account_data));

	ela->handle = value_pair_get_value(pairs, "SCREEN_NAME");
	strncpy(ela->alias, ela->handle, sizeof(ela->alias));
	str = value_pair_get_value(pairs, "PASSWORD");
	strncpy(sla->password, str, sizeof(sla->password));
	free( str );
	str = value_pair_get_value(pairs,"CONNECT");
	ela->connect_at_startup=(str && !strcmp(str,"1"));
	free(str);

	sla->status = SMTP_STATUS_OFFLINE;

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = sla;

	return ela;
}

static LList *eb_smtp_write_local_config(eb_local_account * account)
{
	eb_smtp_local_account_data *sla = account->protocol_local_account_data;
	LList *list = NULL;
	value_pair *vp;

	vp = calloc(1, sizeof(value_pair));
	strcpy(vp->key, "SCREEN_NAME");
	strcpy(vp->value, account->handle);

	list = l_list_append(list, vp);

	vp = calloc(1, sizeof(value_pair));
	strcpy(vp->key, "PASSWORD");
	strcpy(vp->value, sla->password);

	list = l_list_append(list, vp);

	vp = g_new0( value_pair, 1 );
	strcpy( vp->key, "CONNECT" );
	if (account->connect_at_startup)
		strcpy( vp->value, "1");
	else 
		strcpy( vp->value, "0");
	
	list = l_list_append( list, vp );

	return list;
}

static void _buddy_change_state(void * data, void * user_data)
{
	eb_account *ea = find_account_by_handle(data, SERVICE_INFO.protocol_id);
	eb_smtp_account_data *sad;
	int status = (int)user_data;

	if(!ea)
		return;

	sad = ea->protocol_account_data;

	sad->status = status;

	if(status == SMTP_STATUS_ONLINE)
		buddy_login(ea);
	else
		buddy_logoff(ea);

	buddy_update_status(ea);
}

static LList * pending_connects = NULL;

static void eb_smtp_login(eb_local_account *account)
{
	/* we should always be logged in */
	eb_smtp_local_account_data *sla = account->protocol_local_account_data;

	if(account->status_menu) {
		sla->status = SMTP_STATUS_ONLINE;
		is_setting_state = 1;
		eb_set_active_menu_status(account->status_menu, SMTP_STATUS_ONLINE);
		is_setting_state = 0;
	} 
	account->connected = 1;
	ref_count++;

	l_list_foreach(eb_smtp_buddies, _buddy_change_state, 
			(void *)SMTP_STATUS_ONLINE);
}

static void eb_smtp_logout(eb_local_account *account)
{
	/* cannot logout */
	eb_smtp_local_account_data *sla = account->protocol_local_account_data;
	LList * l;

	for(l = pending_connects; l; l=l->next)
		ay_socket_cancel_async((int)l->data);

	account->connected = 0;
	ref_count--;
	if(account->status_menu) {
		sla->status = SMTP_STATUS_OFFLINE;
		is_setting_state = 1;
		eb_set_active_menu_status(account->status_menu, SMTP_STATUS_OFFLINE);
		is_setting_state = 0;
	} 

	l_list_foreach(eb_smtp_buddies, _buddy_change_state, 
			(void *)SMTP_STATUS_OFFLINE);
}

static LList *eb_smtp_get_states()
{
	LList *states = NULL;

	states = l_list_append(states, "Online");
	states = l_list_append(states, "Offline");

	return states;
}

static int eb_smtp_get_current_state(eb_local_account *account)
{
	eb_smtp_local_account_data *sla = account->protocol_local_account_data;

	return sla->status;
}

static void eb_smtp_set_current_state(eb_local_account *account, int state)
{
	eb_smtp_local_account_data *sla = account->protocol_local_account_data;

	if(is_setting_state)
		return;

	if(sla->status == SMTP_STATUS_OFFLINE && state == SMTP_STATUS_ONLINE)
		eb_smtp_login(account);
	else if(sla->status == SMTP_STATUS_ONLINE && state == SMTP_STATUS_OFFLINE)
		eb_smtp_logout(account);

	sla->status = state;
}

static void eb_smtp_set_idle(eb_local_account * account, int idle)
{
}

static void eb_smtp_set_away(eb_local_account * account, char * message)
{
}

static eb_account *eb_smtp_new_account(const char * account)
{
	eb_account *ea = calloc(1, sizeof(eb_account));
	eb_smtp_account_data *sad = calloc(1, sizeof(eb_smtp_account_data));

	ea->protocol_account_data = sad;

	strncpy(ea->handle, account, 255);
	ea->service_id = SERVICE_INFO.protocol_id;
	sad->status = SMTP_STATUS_OFFLINE;

	return ea;
}

static void eb_smtp_add_user(eb_account * account)
{
	eb_smtp_account_data * sad = account->protocol_account_data;
	eb_local_account * ela = find_local_account_for_remote(account, 0);
	eb_smtp_local_account_data *sla;

	if(!ela) {
		WARNING(("eb_smtp_add_user: ela == NULL"));
		return;
	}
	
	sla = ela->protocol_local_account_data;

	eb_smtp_buddies = l_list_append(eb_smtp_buddies, account->handle);

	if( (sad->status = sla->status) == SMTP_STATUS_ONLINE)
		buddy_login(account);
}

static void eb_smtp_del_user(eb_account * account)
{
	eb_smtp_buddies = l_list_remove(eb_smtp_buddies, account->handle);
}

static eb_account *eb_smtp_read_account_config(LList * config, struct contact * contact)
{
	eb_account *ea = calloc(1, sizeof(eb_account));
	eb_smtp_account_data *sad = calloc(1, sizeof(eb_smtp_account_data));
	char	*str = NULL;

	sad->status = SMTP_STATUS_OFFLINE;

	str = value_pair_get_value(config, "NAME");
	strncpy(ea->handle, str, sizeof(ea->handle));
	free( str );

	ea->service_id = SERVICE_INFO.protocol_id;
	ea->protocol_account_data = sad;
	ea->account_contact = contact;
	ea->icon_handler = -1;
	ea->status_handler = -1;

	eb_smtp_add_user(ea);

	return ea;
}

static int eb_smtp_query_connected(eb_account * account)
{
	eb_smtp_account_data *sad = account->protocol_account_data;

	return (sad->status == SMTP_STATUS_ONLINE);
}

static char *status_strings[] = {
	"",
	"Offline"
};

static char *eb_smtp_get_status_string(eb_account * account)
{
	eb_smtp_account_data *sad = account->protocol_account_data;

	return status_strings[sad->status];
}


#include "smtp_online.xpm"
#include "smtp_away.xpm"

static char ** eb_smtp_get_status_pixmap(eb_account * account)
{
	eb_smtp_account_data *sad;

	sad = account->protocol_account_data;

	if(sad->status == SMTP_STATUS_ONLINE)
		return smtp_online_xpm;
	else
		return smtp_away_xpm;

}

static void eb_smtp_send_file(eb_local_account *from, eb_account *to, char *file)
{
	do_message_dialog(_("You cannot send files through SMTP... yet"), 
			_("SMTP send file"), 0);
}

static int validate_or_die_gracefully(const char *buff, const char *valid, int fd)
{
	if(strstr(buff, valid) == buff) {
		return 1;
	} 

	LOG(("Server responded: %s", buff));
	smtp_tcp_writeline("QUIT", fd);
	close(fd);
	return 0;
}

enum smtp_states { 
	SMTP_CONNECT, SMTP_HELO, 
	SMTP_FROM, SMTP_TO, 
	SMTP_DATA, SMTP_DATA_END,
	SMTP_QUIT
};

struct smtp_callback_data {
	int tag;
	char localhost[255];
	eb_local_account * from;
	eb_account * to;
	char * msg;
	enum smtp_states state;
};

static const char *expected[7]={
	"220", "250", "250", "250", "354", "250", "221"
};
static void destroy_callback_data(struct smtp_callback_data * d)
{
	if(d->tag)
		eb_input_remove(d->tag);
	free(d->msg);
	free(d);
}

static void smtp_message_sent(struct smtp_callback_data *d, int success)
{
	char reply[1024] = "<FONT COLOR=\"#a0a0a0\"><I>";
	if(success)
		strcat(reply, _("Message Sent"));
	else
		strcat(reply, _("Error Sending Message"));
	strcat(reply, "</I></FONT>");

	eb_parse_incoming_message(d->from, d->to, reply);
}

static void send_message_async(void * data, int fd, eb_input_condition cond)
{
	struct smtp_callback_data * d = data;
	char buff[1024];

	if(ay_tcp_readline(buff, sizeof(buff)-1, fd) <= 0) {
		WARNING(("smtp server closed connection"));
		close(fd);
		destroy_callback_data(d);
	};

	if(!validate_or_die_gracefully(buff, expected[d->state], fd)) {
		smtp_message_sent(d, 0);
		destroy_callback_data(d);
	}

	switch(d->state) {
	case SMTP_CONNECT:
		snprintf(buff, sizeof(buff)-1, "HELO %s", d->localhost);
		d->state = SMTP_HELO;
		break;
	case SMTP_HELO:
		snprintf(buff, sizeof(buff)-1, "MAIL FROM: %s", d->from->handle);
		d->state = SMTP_FROM;
		break;
	case SMTP_FROM:
		snprintf(buff, sizeof(buff)-1, "RCPT TO: %s", d->to->handle);
		d->state = SMTP_TO;
		break;
	case SMTP_TO:
		strcpy(buff, "DATA");
		d->state = SMTP_DATA;
		break;
	case SMTP_DATA:
		{
		int n, len = strlen(d->msg);
		for(n=1; d->msg[len-n] == '\r' || d->msg[len-n] == '\n'; n++)
			d->msg[len-n] = '\0';
		if(strncasecmp(d->msg, "Subject:", strlen("Subject:")))
			smtp_tcp_writeline("", fd);
		smtp_tcp_writeline(d->msg, fd);
		strcpy(buff, ".");
		}
		d->state = SMTP_DATA_END;
		break;
	case SMTP_DATA_END:
		strcpy(buff, "QUIT");
		d->state = SMTP_QUIT;
		break;
	case SMTP_QUIT:
		smtp_message_sent(d, 1);
		destroy_callback_data(d);
		return;
	}
	smtp_tcp_writeline(buff, fd);
}

static void eb_smtp_got_connected(int fd, int error, void *data)
{
	struct smtp_callback_data * d = data;

	if(error) {
		WARNING(("Could not connect to smtp server: %d: %s", 
					error, strerror(error)));
		destroy_callback_data(d);
		return;
	}

	pending_connects = l_list_remove(pending_connects, (void *)d->tag);

	d->tag = eb_input_add(fd, EB_INPUT_READ, send_message_async, d);
}

static void eb_smtp_send_im(eb_local_account * account_from, 
		eb_account * account_to, char * message)

{
	char localhost[255];
	struct smtp_callback_data * d;

	if(gethostname(localhost, sizeof(localhost)-1) == -1) {
		strcpy(localhost, "localhost");
		WARNING(("could not get localhost name: %d: %s", 
				errno, strerror(errno)));
		return;
	}

	d = calloc(1, sizeof(struct smtp_callback_data));
	strcpy(d->localhost, localhost);
	d->from = account_from;
	d->to = account_to;
	d->msg = strdup(message);
	d->tag = ay_socket_new_async(smtp_host, atoi(smtp_port), eb_smtp_got_connected, d, NULL);

	pending_connects = l_list_append(pending_connects, (void *)d->tag);
}

static char * eb_smtp_check_login(char * user, char * pass)
{
	return NULL;
}

struct service_callbacks *query_callbacks()
{
	struct service_callbacks *sc;

	sc = calloc(1, sizeof(struct service_callbacks));

	sc->query_connected 		= eb_smtp_query_connected;
	sc->login			= eb_smtp_login;
	sc->logout 			= eb_smtp_logout;
	sc->check_login			= eb_smtp_check_login;

	sc->send_im 			= eb_smtp_send_im;

	sc->read_local_account_config 	= eb_smtp_read_local_account_config;
	sc->write_local_config 		= eb_smtp_write_local_config;
	sc->read_account_config 	= eb_smtp_read_account_config;

	sc->get_states 			= eb_smtp_get_states;
	sc->get_current_state 		= eb_smtp_get_current_state;
	sc->set_current_state 		= eb_smtp_set_current_state;
	
	sc->new_account 		= eb_smtp_new_account;
	sc->add_user 			= eb_smtp_add_user;
	sc->del_user 			= eb_smtp_del_user;

	sc->get_status_string 		= eb_smtp_get_status_string;
	sc->get_status_pixmap 		= eb_smtp_get_status_pixmap;

	sc->set_idle 			= eb_smtp_set_idle;
	sc->set_away 			= eb_smtp_set_away;

	sc->send_file			= eb_smtp_send_file;

        sc->get_color			= eb_smtp_get_color;

	return sc;
}

