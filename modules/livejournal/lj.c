/*
 * lj.c
 */

/*
 * LiveJournal Extension for ayttm 
 *
 * Copyright (C) 2003, Philip Tellis <bluesmoon@users.sourceforge.net>
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
#include "activity_bar.h"

#include "status.h"

#include "input_list.h"
#include "value_pair.h"
#include "util.h"

#include "message_parse.h"	/* eb_parse_incoming_message */
#include "messages.h"

#include "lj_httplib.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info lj_LTX_plugin_info
#define SERVICE_INFO lj_LTX_SERVICE_INFO
#define module_version lj_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();
struct service_callbacks * query_callbacks();

static int is_setting_state = 0;

static int ref_count = 0;

static char lj_url[MAX_PREF_LEN]="http://www.livejournal.com/interface/flat";
static char lj_host[MAX_PREF_LEN]="";
static int lj_port;
static char lj_path[MAX_PREF_LEN];
static int do_lj_debug = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info =
{
	PLUGIN_SERVICE,
	"LiveJournal",
	"Ayttm client for LiveJournal (http://www.livejournal.com/)",
	"$Revision: 1.2 $",
	"$Date: 2003/08/04 04:47:14 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	NULL
};
struct service SERVICE_INFO = {
	"LiveJournal",
	-1,
	SERVICE_CAN_OFFLINEMSG |	/* all messages are offline */
	SERVICE_CAN_FILETRANSFER, 	/* true so i can prevent file 
					   transfer altogether */
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int plugin_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	ref_count = 0;

	plugin_info.prefs = il;
	il->widget.entry.value = lj_url;
	il->widget.entry.name = "lj_url";
	il->widget.entry.label = _("LiveJournal _URL:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.checkbox.value = &do_lj_debug;
	il->widget.checkbox.name = "do_lj_debug";
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
int LJ_DEBUGLOG(char *fmt,...)
#else
int LJ_DEBUGLOG(fmt, va_alist)
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


#define LOG(x) if(do_lj_debug) { LJ_DEBUGLOG("%s:%d: ", __FILE__, __LINE__); \
	LJ_DEBUGLOG x; \
	LJ_DEBUGLOG("\n"); }

#define WARNING(x) if(do_lj_debug) { LJ_DEBUGLOG("%s:%d: warning: ", __FILE__, __LINE__); \
	LJ_DEBUGLOG x; \
	LJ_DEBUGLOG("\n"); }


#define LJ_MSG_COLOUR	"#20b2aa"
static char *ay_lj_get_color(void) { return LJ_MSG_COLOUR; }

enum lj_status_code { LJ_STATUS_ONLINE, LJ_STATUS_OFFLINE };
	
typedef struct lj_account_data {
	int status;		/* always online */
} lj_account_data;

typedef struct lj_local_account {
	char password[MAX_PREF_LEN];
	int status;			/* always online */
	int connect_progress_tag;
	LList *buddies;

	char last_update[255];
	int poll_timeout_tag;
	int poll_interval;		/* 0 means don't poll */
} lj_local_account;

static void lj_account_prefs_init(eb_local_account *ela)
{
	lj_local_account *lla = ela->protocol_local_account_data;

	input_list *il = calloc(1, sizeof(input_list));

	ela->prefs = il;
	il->widget.entry.value = ela->handle;
	il->widget.entry.name = "SCREEN_NAME";
	il->widget.entry.label = _("_Username:");
	il->type = EB_INPUT_ENTRY;

	il->next = calloc(1, sizeof(input_list));
	il = il->next;
	il->widget.entry.value = lla->password;
	il->widget.entry.name = "PASSWORD";
	il->widget.entry.label = _("_Password:");
	il->type = EB_INPUT_PASSWORD;

}

static eb_local_account *ay_lj_read_local_account_config(LList * pairs)
{
	eb_local_account *ela;
	lj_local_account *lla;

	if(!pairs) {
		WARNING(("ay_lj_read_local_account_config: pairs == NULL"));
		return NULL;
	}

	ela = calloc(1, sizeof(eb_local_account));
	lla = calloc(1, sizeof(lj_local_account));

	lla->status = LJ_STATUS_OFFLINE;
	strcpy(lla->last_update, "0");
	lla->poll_interval = 1800;

	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = lla;

	lj_account_prefs_init(ela);
	eb_update_from_value_pair(ela->prefs, pairs);

	return ela;
}

static LList *ay_lj_write_local_config(eb_local_account * account)
{
	return eb_input_to_value_pair(account->prefs);
}

static LList *ay_lj_get_states()
{
	LList *states = NULL;

	states = l_list_append(states, "Online");
	states = l_list_append(states, "Offline");

	return states;
}

static int lj_find_and_handle_errors(int success, eb_local_account *ela, LList *pairs)
{
	char buff[1024]="", *str;
	switch(success) {
		case LJ_HTTP_NETWORK:
			snprintf(buff, sizeof(buff), _("Could not connect %s to LiveJournal service.\n\nThere was a network error."), ela->handle);
			break;
		case LJ_HTTP_NOK:
			snprintf(buff, sizeof(buff), _("Could not connect %s to LiveJournal service.\n\nThe server returned an unknown HTTP error."), ela->handle);
			break;
		case LJ_HTTP_OK:
			str = value_pair_get_value(pairs, "success");
			if(!str) {
				snprintf(buff, sizeof(buff), 
						_("Incomplete read from LiveJournal service.\n\nData lost."));
			} else if(strcmp(str, "OK") != 0) {
				free(str);
				str = value_pair_get_value(pairs, "errmsg");
				snprintf(buff, sizeof(buff), 
						_("%s!\n\n%s"), ela->handle, str);
				free(str);
			}
	
			break;
	}

	if(buff[0]) {
		ay_do_warning( _("LiveJournal Error"), buff);
		return 1;
	} else {
		return 0;
	}
}

static int ay_lj_get_current_state(eb_local_account *account)
{
	lj_local_account *lla = account->protocol_local_account_data;

	return lla->status;
}

static void ay_lj_set_idle(eb_local_account * account, int idle)
{
}

static void ay_lj_set_away(eb_local_account * account, char * message)
{
}

static eb_account *ay_lj_new_account(eb_local_account *ela, const char *handle)
{
	eb_account *ea = calloc(1, sizeof(eb_account));
	lj_account_data *lad = calloc(1, sizeof(lj_account_data));

	LOG(("ay_lj_new_account"));

	ea->protocol_account_data = lad;
	strncpy(ea->handle, handle, sizeof(ea->handle));
	ea->service_id = SERVICE_INFO.protocol_id;
	lad->status = LJ_STATUS_OFFLINE;
	ea->ela = ela;
	return ea;
}

static void _ay_lj_user_added(int success, eb_local_account *ela, LList *pairs)
{
	int nadded=0, i;
	char *str, buff[1024];
	eb_account *ea;

	lj_find_and_handle_errors(success, ela, pairs);

	str = value_pair_get_value(pairs, "friends_added");
	if(str) {
		nadded = atoi(str);
		free(str);
	}

	for(i=1; i<=nadded; i++) {
		snprintf(buff, sizeof(buff), "friend_%d_user", i);
		str = value_pair_get_value(pairs, buff);
		if(!str)
			continue;

		ea = find_account_with_ela(str, ela);
		free(str);

		snprintf(buff, sizeof(buff), "friend_%d_name", i);
		str = value_pair_get_value(pairs, buff);

		if(!ea || !str)
			continue;

		if(strcmp(ea->account_contact->nick, str))
			rename_contact(ea->account_contact, str);
		free(str);
	}
	
}

static void ay_lj_add_user(eb_account *ea)
{
	eb_local_account *ela = ea->ela;
	lj_local_account *lla = ela->protocol_local_account_data;
	char buff[1024];

	snprintf(buff, sizeof(buff), "mode=editfriends&user=%s&password=%s&editfriend_add_1_user=%s", 
			ela->handle, lla->password, ea->handle);

	send_http_request(buff, _ay_lj_user_added, ela);
}

static void ay_lj_del_user(eb_account *ea)
{
	eb_local_account *ela = ea->ela;
	lj_local_account *lla = ela->protocol_local_account_data;
	char buff[1024];

	snprintf(buff, sizeof(buff), "mode=editfriends&user=%s&password=%s&editfriend_delete_%s", 
			ela->handle, lla->password, ea->handle);

	send_http_request(buff, NULL, NULL);
}

static eb_account *ay_lj_read_account_config(eb_account *ea, LList * config)
{
	lj_account_data *lad = calloc(1, sizeof(lj_account_data));
	lad->status = LJ_STATUS_OFFLINE;
	ea->protocol_account_data = lad;

	return ea;
}

static int ay_lj_query_connected(eb_account * account)
{
	lj_account_data *lad = account->protocol_account_data;

	return (lad->status == LJ_STATUS_ONLINE);
}

static char *status_strings[] = {
	"",
	"Offline"
};

static char *ay_lj_get_status_string(eb_account * account)
{
	lj_account_data *lad = account->protocol_account_data;

	return status_strings[lad->status];
}


static char ** ay_lj_get_status_pixmap(eb_account * account)
{
	lj_account_data *lad;

	lad = account->protocol_account_data;

	if(lad->status == LJ_STATUS_ONLINE)
		return NULL;
	else
		return NULL;

}

static void ay_lj_send_file(eb_local_account *from, eb_account *to, char *file)
{
	ay_do_info( _("LiveJournal Warning"), _("You cannot send files through LiveJournal...") );
}

static void ay_lj_send_im(eb_local_account *account_from, eb_account *account_to, char *message)
{
}

static int ay_lj_ping_timeout_callback(gpointer data);

static void _ay_lj_got_update(int success, eb_local_account *ela, LList *pairs)
{
	lj_local_account *lla = ela->protocol_local_account_data;
	char *str, buff[1024];

	lj_find_and_handle_errors(success, ela, pairs);

	str = value_pair_get_value(pairs, "lastupdate");
	if(str && str[0])
		strncpy(lla->last_update, str, sizeof(lla->last_update));
	if(str)
		free(str);

	str = value_pair_get_value(pairs, "new");
	if(str) {
		if(!strcmp(str, "1")) {
			eb_account *ea = find_account_with_ela(ela->handle, ela);
			if(!ea) {
				ea = ay_lj_new_account(ela, ela->handle);
				add_dummy_contact(ela->handle, ea);
			}
			eb_timeout_remove(lla->poll_timeout_tag);
			lla->poll_timeout_tag = 0;
			
			snprintf(buff, sizeof(buff), _("%s, your LiveJournal friends view has been updated.<br>"
						"<a href=\"http://www.livejournal.com/users/%s/friends\">Go there now</a>"),
					ela->alias, ela->handle);
			eb_parse_incoming_message(ela, ea, buff);
		}
		free(str);
	}
	str = value_pair_get_value(pairs, "interval");
	if(str) {
		if(lla->poll_interval != atoi(str)) {
			lla->poll_interval = atoi(str);
			if(lla->poll_timeout_tag) {
				eb_timeout_remove(lla->poll_timeout_tag);
				lla->poll_timeout_tag = eb_timeout_add(lla->poll_interval * 1000, ay_lj_ping_timeout_callback, ela);
			}
		}
		free(str);
	}
}

static int ay_lj_ping_timeout_callback(gpointer data)
{
	eb_local_account *ela = data;
	lj_local_account *lla = ela->protocol_local_account_data;
	char buff[1024];

	snprintf(buff, sizeof(buff), "mode=checkfriends&user=%s&password=%s&lastupdate=%s", 
			ela->handle, lla->password, lla->last_update);

	send_http_request(buff, _ay_lj_got_update, ela);
	return 1;
}

static void _ay_lj_got_buddies(int success, eb_local_account *ela, LList *pairs)
{
	lj_local_account *lla = ela->protocol_local_account_data;
	eb_account *ea = NULL;
	int changed = 0;
	int i, nfriends=0;
	char *str, buff[1024];

	if(lj_find_and_handle_errors(success, ela, pairs))
		goto end_buddy_list;

	str = value_pair_get_value(pairs, "friend_count");
	if(!str)
		goto end_buddy_list;

	nfriends = atoi(str);
	free(str);

	for(i=1; i<=nfriends; i++) {
		char *uname=NULL, *real_name=NULL;
		struct contact *con;
		grouplist *g;

		snprintf(buff, sizeof(buff), "friend_%d_user", i);
		uname = value_pair_get_value(pairs, buff);

		ea = find_account_with_ela(str, ela);

		if(ea) {
			free(uname);
			continue;
		}

		snprintf(buff, sizeof(buff), "friend_%d_name", i);
		real_name = value_pair_get_value(pairs, buff);

		g = find_grouplist_by_name("LiveJournal");
		con = find_contact_in_group_by_nick(real_name, g);
		if(!con)
			con = find_contact_in_group_by_nick(uname, g);
		if(!con)
			con = find_contact_by_nick(real_name);
		if(!con)
			con = find_contact_by_nick(uname);
		if(!con) {
			changed = 1;
			con=add_new_contact("LiveJournal", real_name, SERVICE_INFO.protocol_id);
		}
		ea = ay_lj_new_account(ela, uname);
		add_account(con->nick, ea);
/*		lla->buddies = l_list_prepend(lla->buddies, uname);*/

		free(uname);
		free(real_name);
	}

	if(changed) {
		update_contact_list();
	}

end_buddy_list:
	if(lla->connect_progress_tag) {
		ay_activity_bar_remove(lla->connect_progress_tag);
		lla->connect_progress_tag=0;
	}
}

static void lj_get_buddies(eb_local_account *ela)
{
	lj_local_account *lla = ela->protocol_local_account_data;
	char buff[1024];

	snprintf(buff, sizeof(buff), _("Fetching buddies for %s..."), ela->handle);
	ay_activity_bar_update_label(lla->connect_progress_tag, buff);

	snprintf(buff, sizeof(buff), "mode=getfriends&user=%s&password=%s", 
			ela->handle, lla->password);

	send_http_request(buff, _ay_lj_got_buddies, ela);
}

static void _ay_lj_got_login(int success, eb_local_account *ela, LList *pairs)
{
	lj_local_account *lla = ela->protocol_local_account_data;

	if(lj_find_and_handle_errors(success, ela, pairs)) {
		ref_count--;
		if(lla->connect_progress_tag) {
			ay_activity_bar_remove(lla->connect_progress_tag);
			lla->connect_progress_tag=0;
		}
	} else {

		char *str = value_pair_get_value(pairs, "name");
		strncpy(ela->alias, str, sizeof(ela->alias));
		free(str);
	
		str = value_pair_get_value(pairs, "message");
		if(str) {
			ay_do_info(_("LiveJournal System Message"), str);
			free(str);
		}

		ela->connected = 1;
		ela->connecting = 0;
		lla->status = LJ_STATUS_ONLINE;

		is_setting_state = 1;
		if(ela->status_menu)
			eb_set_active_menu_status(ela->status_menu, lla->status);
		is_setting_state = 0;

		lj_get_buddies(ela);
		lla->poll_timeout_tag = eb_timeout_add(lla->poll_interval * 1000, ay_lj_ping_timeout_callback, ela);
	}
}

static void ay_lj_login(eb_local_account *ela)
{
	lj_local_account *lla = ela->protocol_local_account_data;
	char buff[1024];
	char buff2[1024];

	if(ela->connecting || ela->connected)
		return;

	url_to_host_port_path(lj_url, lj_host, &lj_port, lj_path);

	snprintf(buff, sizeof(buff), "clientversion=%s-%s/%s&mode=login&user=%s&password=%s", 
			HOST, PACKAGE, VERSION,
			ela->handle, lla->password);
	snprintf(buff2, sizeof(buff2), _("Logging in to LiveJournal account: %s"), ela->handle);

	ela->connecting = 1;
	lla->connect_progress_tag = ay_activity_bar_add(buff2, NULL, NULL);
	send_http_request(buff, _ay_lj_got_login, ela);

	ref_count++;
}

static void ay_lj_logout(eb_local_account *ela)
{
/*	lj_local_account *lla = ela->protocol_local_account_data;
	while(lla->buddies) {
		LList *l = lla->buddies;
		char *uname = l->data;
		eb_account *ea = find_account_with_ela(uname, ela);

		lla->buddies = l_list_remove_link(lla->buddies, l);
		remove_account(ea);
		free(uname);
		l_list_free_1(l);
	}
*/	ela->connected=0;
	ref_count--;
}

static void ay_lj_set_current_state(eb_local_account *account, int state)
{
	lj_local_account *lla = account->protocol_local_account_data;

	if(is_setting_state)
		return;

	if(lla->status == LJ_STATUS_OFFLINE && state == LJ_STATUS_ONLINE)
		ay_lj_login(account);
	else if(lla->status == LJ_STATUS_ONLINE && state == LJ_STATUS_OFFLINE)
		ay_lj_logout(account);

	lla->status = state;
}

static char * ay_lj_check_login(char * user, char * pass)
{
	return NULL;
}

const char * ext_lj_host() { return (const char *)lj_host; }
int ext_lj_port() { return lj_port; }
const char * ext_lj_path() { return (const char *)lj_path; }

struct service_callbacks *query_callbacks()
{
	struct service_callbacks *sc;

	sc = calloc(1, sizeof(struct service_callbacks));

	sc->query_connected 		= ay_lj_query_connected;
	sc->login			= ay_lj_login;
	sc->logout 			= ay_lj_logout;
	sc->check_login			= ay_lj_check_login;

	sc->send_im 			= ay_lj_send_im;

	sc->read_local_account_config 	= ay_lj_read_local_account_config;
	sc->write_local_config 		= ay_lj_write_local_config;
	sc->read_account_config 	= ay_lj_read_account_config;

	sc->get_states 			= ay_lj_get_states;
	sc->get_current_state 		= ay_lj_get_current_state;
	sc->set_current_state 		= ay_lj_set_current_state;
	
	sc->new_account 		= ay_lj_new_account;
	sc->add_user 			= ay_lj_add_user;
	sc->del_user 			= ay_lj_del_user;

	sc->get_status_string 		= ay_lj_get_status_string;
	sc->get_status_pixmap 		= ay_lj_get_status_pixmap;

	sc->set_idle 			= ay_lj_set_idle;
	sc->set_away 			= ay_lj_set_away;

	sc->send_file			= ay_lj_send_file;

        sc->get_color			= ay_lj_get_color;

	return sc;
}

