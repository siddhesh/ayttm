/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 * Modified by Christof Meerwald <cmeerw@web.de>
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
 * aim.c
 * AIM implementation
 */

#define DEBUG	1

#include "aim.h"

#include <gtk/gtk.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#if defined( _WIN32 )
/* FIXME is this still used ? */
typedef unsigned long u_long;
typedef unsigned long ulong;
#endif
#include "info_window.h"
#include "activity_bar.h"
/* #include "eb_aim.h" */
#include "away_window.h"
#include "service.h"
#include "llist.h"
#include "chat_window.h"
#include "chat_room.h"
#include "util.h"
#include "status.h"
#include "globals.h"
#include "message_parse.h"
#include "value_pair.h"
#include "plugin_api.h"
#include "smileys.h"
#include "messages.h"
#include "config.h"
#include "dialog.h"

#include "libproxy/libproxy.h"

#include "pixmaps/aim_online.xpm"
#include "pixmaps/aim_away.xpm"

/*************************************************************************************
 *                             Begin Module Code
 ************************************************************************************/
/*  Module defines */
#define plugin_info aim_oscar_LTX_plugin_info
#define SERVICE_INFO aim_oscar_LTX_SERVICE_INFO
#define plugin_init aim_oscar_LTX_plugin_init
#define plugin_finish aim_oscar_LTX_plugin_finish
#define module_version aim_oscar_LTX_module_version

unsigned int module_version () {return CORE_VERSION;}

/* Function Prototypes */
static int plugin_init   ();
static int plugin_finish ();
static int reload_prefs  ();

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info =
{
	PLUGIN_SERVICE,
	"AIM/ICQ Oscar",
	"Provides AOL Instant Messenger and ICQ support via the Oscar protocol",
	"$Revision: 1.25 $",
	"$Date: 2008/08/03 20:26:46 $",
	&ref_count,
	plugin_init,
	plugin_finish,
	reload_prefs,
	NULL
};

struct service SERVICE_INFO = { "AIM-ICQ", -1, SERVICE_CAN_MULTIACCOUNT | SERVICE_CAN_GROUPCHAT , NULL };
/* End Module Exports */

static char *ay_aim_get_color(void) { static char color[]="#000088"; return color; }

static int do_oscar_debug = 0;
static int do_libfaim_debug = 0;

static int
plugin_init ()
{
	input_list *il = g_new0 (input_list, 1);

	eb_debug (DBG_MOD, "plugin_init() : aim-oscar\n");
	ref_count = 0;

	plugin_info.prefs = il;
	il->widget.checkbox.value = &do_oscar_debug;
	il->name = "do_oscar_debug";
	il->label = "Enable debugging";
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0 (input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &do_libfaim_debug;
	il->name = "do_libfaim_debug";
	il->label = "Enable libfaim debugging";
	il->type = EB_INPUT_CHECKBOX;

	return 0;
}

static int
plugin_finish ()
{
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		if (il && il->type == EB_INPUT_LIST) {
			l_list_free(il->widget.listbox.list);
		}
		g_free(plugin_info.prefs);
		plugin_info.prefs = il;
	}
	eb_debug (DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return ref_count;
}

static int
reload_prefs ()
{
	return 0;
}

/*************************************************************************************
 *                             End Module Code
 ************************************************************************************/

struct eb_aim_account_data
{
	gint idle_time;
	gint logged_in_time;
	gint status;
	gint evil;
	gint on_server;
};

struct oscar_chat_room_data {
	char *name;
	gint exchange;
	eb_chat_room *ecr;
};

struct oscar_chat_room {
	char *name;
	char *show;
	fu16_t exchange;
	fu16_t instance;

	int input;
	aim_conn_t *conn;
};

struct eb_aim_local_account_data
{
	char password[255];
	char tmp_password[255];
	gint status;
	gint listsyncing;
	gint perm_deny;
	
	LList *buddies;
	LList *pending_rooms;
	LList *rooms;
	
	int fd;
	aim_conn_t *conn;
	aim_conn_t *conn_chatnav;
	aim_session_t aimsess;
	
	int input;
	int input_chatnav;
	int login_activity_bar;

	int prompt_password;
};

typedef struct _aim_info_data 
{
	gchar *away;
	gchar *profile;
} aim_info_data;

enum
{
	AIM_ONLINE=0,
	AIM_AWAY=1,
	AIM_OFFLINE=2
};

static int caps_aim = AIM_CAPS_CHAT; /*| AIM_CAPS_BUDDYICON | AIM_CAPS_DIRECTIM | AIM_CAPS_SENDFILE | AIM_CAPS_INTEROPERATE;*/
static fu8_t features_aim[] = {0x01, 0x01, 0x01, 0x02};

static char * profile = "Visit the Ayttm website at <A HREF=\"http://ayttm.sf.net\">http://ayttm.sf.net/</A>.";

static char *msgerrreason[] = {
	"Invalid error",
	"Invalid SNAC",
	"Rate to host",
	"Rate to client",
	"Not logged in",
	"Service unavailable",
	"Service not defined",
	"Obsolete SNAC",
	"Not supported by host",
	"Not supported by client",
	"Refused by client",
	"Reply too big",
	"Responses lost",
	"Request denied",
	"Busted SNAC payload",
	"Insufficient rights",
	"In local permit/deny",
	"Too evil (sender)",
	"Too evil (receiver)",
	"User temporarily unavailable",
	"No match",
	"List overflow",
	"Request ambiguous",
	"Queue full",
	"Not while on AOL"
};
static int msgerrreasonlen = 25;

/* misc */
static const char *aim_normalize (const char *s);
static char *extract_name (const char *name);
static void connect_error (struct eb_aim_local_account_data *alad, char *msg); /* FIXME find a better name */
static eb_account *oscar_find_account_with_ela (const char *handle, eb_local_account *ela, gint should_update);
static eb_chat_room *oscar_find_chat_room_by_conn (struct eb_aim_local_account_data *alad, aim_conn_t *conn);
static eb_chat_room *oscar_find_chat_room_by_name (struct eb_aim_local_account_data *alad, const char *name);
static aim_conn_t *oscar_find_chat_conn_by_source (struct eb_aim_local_account_data *alad, int fd);

/* ayttm callbacks forward declaration */
static void ay_aim_login  (eb_local_account *account);
static void ay_aim_logout (eb_local_account *account);
static eb_account * ay_aim_new_account (eb_local_account * ela, const char *account);
static void ay_aim_add_user (eb_account *account);
static void ay_aim_callback (void *data, int source, eb_input_condition condition);

/* faim callbacks forward declaration */
static int faim_cb_parse_login        (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_authresp     (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_connerr            (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_genericerr   (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_conninitdone_bos   (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_motd         (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_selfinfo           (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_ssi_parserights    (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_ssi_parselist      (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_ssi_parseack       (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_locaterights   (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_buddyrights    (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_icbmparaminfo        (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_memrequest           (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_oncoming       (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_offgoing       (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_incoming_im    (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_handle_redirect      (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_conninitdone_chatnav (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_chatnav_info         (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_conninitdone_chat    (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_chat_join            (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_chat_leave           (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_chat_info_update     (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_chat_incoming_msg    (aim_session_t *sess, aim_frame_t *fr, ...);

/* Connection callbacks */
static void oscar_login_connect (int fd, int error, void *data);
static void oscar_login_connect_status (const char *msg, void *data);
static void oscar_chatnav_connect (int fd, int error, void *data);
static void oscar_chatnav_connect_status (const char *msg, void *data);
static void oscar_chat_connect (int fd, int error, void *data);

/* Debug functions taken from yahoo module */

#define OSCAR_DEBUGLOG ext_oscar_log

static int
ext_oscar_log (char *fmt,...)
{
	va_list ap;
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	fflush (stderr);
	va_end (ap);
	return 0;
}

#define LOG(x) if (do_oscar_debug) { OSCAR_DEBUGLOG("%s:%d: ", __FILE__, __LINE__); \
	OSCAR_DEBUGLOG x; \
	OSCAR_DEBUGLOG("\n"); }

#define WARNING(x) if (do_oscar_debug) { OSCAR_DEBUGLOG("%s:%d: WARNING: ", __FILE__, __LINE__); \
	OSCAR_DEBUGLOG x; \
	OSCAR_DEBUGLOG("\n"); }


/*the callback to call all callbacks :P */

static void
ay_aim_callback (void *data, int source, eb_input_condition condition)
{
	eb_local_account *ela = (eb_local_account *)data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	aim_conn_t *conn = NULL;
	
	LOG (("ay_aim_callback, source=%d", source))
  
	g_assert (!(source < 0));
	
	if (alad->conn->fd == source)
		conn = alad->conn;
	else if (alad->conn_chatnav->fd == source)
		conn = alad->conn_chatnav;
	else if ((conn = oscar_find_chat_conn_by_source (alad, source)) != NULL)
		;

	if (conn == NULL) {
		// connection not found
		WARNING (("connection not found"))

	} else if (aim_get_command(&(alad->aimsess), conn) < 0)	{

		if (conn->type == AIM_CONN_TYPE_BOS) {
			WARNING (("CONNECTION ERROR!!!!!! attempting to reconnect"))
			g_assert(ela);
			ay_aim_logout(ela);
			ay_aim_login(ela);
		} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
			WARNING (("CONNECTION ERROR! (ChatNav)"))
			eb_input_remove (alad->input_chatnav);
			aim_conn_kill (&(alad->aimsess), &conn);
			alad->conn_chatnav = NULL;
		} else if (conn->type == AIM_CONN_TYPE_CHAT) {
			WARNING (("CONNECTION ERROR! (Chat)"))
#if 0
			eb_input_remove(alad->chatnav_input);
			aim_conn_kill(&(alad->aimsess), &conn);
			alad->chat_conn = NULL;
			alad->chat_room = NULL;
#endif
		}
	} else {
		aim_rxdispatch (&(alad->aimsess));
	}
}

static void
faim_cb_oscar_debug (aim_session_t *sess, int level, const char *format, va_list va)
{
	if (do_libfaim_debug) {
		vfprintf (stderr, format, va);
		fflush (stderr);
	}
}

static int
faim_cb_parse_login (aim_session_t *sess, aim_frame_t *fr, ...)
{
	/* FIXME: What about telling AIM that we are Ayttm ? */
	struct client_info_s info= CLIENTINFO_AIM_KNOWNGOOD;
	eb_local_account *account = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data *alad;
	char *key;
	va_list ap;
	
	LOG (("faim_cb_parse_login ()\n"))
	
	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);
	
	alad = (struct eb_aim_local_account_data *) account->protocol_local_account_data;
	
	
	ay_activity_bar_update_label (alad->login_activity_bar, "Sending password...");
	
	LOG (("Login=%s | Password=%s\n", account->handle, alad->tmp_password))

	aim_send_login (sess, fr->conn, account->handle,
			alad->tmp_password, &info, key);
	
	memset (alad->tmp_password, 0, sizeof (alad->tmp_password));

	return 1;
}

/*
 * Callback used to parse the authentification response.
 */
static int
faim_cb_parse_authresp (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	va_list ap;
	struct aim_authresp_info *info;
	
	va_start (ap, fr);
	info = va_arg (ap, struct aim_authresp_info *);
	va_end (ap);
	
	LOG (("faim_cb_parse_authresp () : Screen name : %s", info->sn))
	
	/* Check for errors */
	if (info->errorcode || !info->bosip || !info->cookielen || !info->cookie) {

		switch (info->errorcode) {
		case 0x05:
			/* Incorrect nick/password */
			connect_error (alad, "Incorrect nickname or password.");
			break;
		case 0x11:
			/* Suspended account */
			connect_error (alad, "Your account is currently suspended.");
			break;
		case 0x14:
			/* service temporarily unavailable */
			connect_error (alad, "The AOL Instant Messenger service is temporarily unavailable.");
			break;
		case 0x18:
			/* connecting too frequently */
			connect_error (alad, "You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer.");
			break;
		case 0x1c:
			/* client too old */
			connect_error (alad, "The client version you are using is too old. Please upgrade at http://ayttm.sf.net/");
			break;
		default:
			connect_error (alad, "Authentication failed for an unknown reason");
			break;
		}
		
		WARNING (("Login Error Code 0x%04x", info->errorcode))
		WARNING (("Error URL: %s", info->errorurl));

		ay_aim_logout (ela);
		return 1;
	}
	
	LOG (("Closing auth connection...\n"))
	LOG (("REMOVING TAG = %d\n", alad->input))

	eb_input_remove (alad->input);
	aim_conn_kill (sess, &(alad->conn));
	
	ay_activity_bar_update_label (alad->login_activity_bar, "Getting buddy list...");
	
	alad->conn = aim_newconn(sess, AIM_CONN_TYPE_BOS, info->bosip);
	if (!alad->conn)
	{
		connect_error (alad, "Connection to BOS failed: internal error");
		WARNING (("Connection to BOS failed: internal error"))
		return 1;
	}
	if (alad->conn->status & AIM_CONN_STATUS_CONNERR)
	{
		connect_error (alad, "Connection to BOS failed");
		WARNING (("Connection to BOS failed\n"))
		return 1;
	}
	
	
	LOG (("New connection fd=%d", alad->conn->fd ))

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0 );
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, faim_cb_parse_motd, 0);
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, faim_cb_connerr, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, faim_cb_conninitdone_bos, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_SELFINFO, faim_cb_selfinfo, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_RIGHTSINFO, faim_cb_ssi_parserights, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_LIST, faim_cb_ssi_parselist, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_NOLIST, faim_cb_ssi_parselist, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_SRVACK, faim_cb_ssi_parseack, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_RIGHTSINFO, faim_cb_parse_locaterights, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, faim_cb_parse_buddyrights, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_PARAMINFO, faim_cb_icbmparaminfo, 0 );

	/* FIXME : fix libfaim and use meaningful names:) */
	aim_conn_addhandler(sess, alad->conn, 0x0001, 0x001f, faim_cb_memrequest, 0);
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, faim_cb_parse_oncoming, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, faim_cb_parse_offgoing, 0); 
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, faim_cb_parse_incoming_im, 0 );

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, faim_cb_handle_redirect, 0 );
#if 0
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_STS, AIM_CB_STS_SETREPORTINTERVAL, NULL, 0 );
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, eb_aim_msg_missed, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, eb_aim_msg_error, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, (rxcallback_t) eb_aim_info_responce, 0);
#endif
	
	alad->input = eb_input_add (alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION , ay_aim_callback, ela);
	
	aim_sendcookie(sess, alad->conn, info->cookielen, info->cookie);
	alad->status=AIM_ONLINE;
	// serv_touch_idle();
	
	return 1;
}

static int
faim_cb_connerr (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_list ap;
	fu16_t code;
	char *msg;
	
	va_start (ap, fr);
	code = (fu16_t)va_arg (ap, int);
	msg = va_arg (ap, char *);
	va_end (ap);

	LOG (("Disconnected.  Code is 0x%04x and msg is %s\n", code, msg))

	if ((fr) && (fr->conn) && (fr->conn->type == AIM_CONN_TYPE_BOS)) {
		if (code == 0x0001) {
			connect_error (alad, "You have been disconnected because you have signed on with this screen name at another location.");
		} else {
			connect_error (alad, "You have been signed off for an unknown reason.");
		}
		if (ela->status_menu) {
			eb_set_active_menu_status (ela->status_menu, AIM_OFFLINE);
		}
		// ay_aim_logout (ela);
	}

	return 1;
}

static int
faim_cb_parse_genericerr (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t reason;

	va_start(ap, fr);
	reason = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	WARNING (("snac threw error (reason 0x%04hx: %s)\n", reason, (reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown"))

	return 1;
}

static int
faim_cb_conninitdone_bos (aim_session_t *sess, aim_frame_t *fr, ...)
{
	LOG (("faim_cb_conninitdone_bos ()"))

	aim_reqpersonalinfo (sess, fr->conn); /* -> faim_cb_selfinfo () */

	aim_ssi_reqrights (sess); /* -> faim_cb_ssi_parserights () */
	aim_ssi_reqdata (sess); /* -> faim_cb_ssi_parselist () */

	aim_locate_reqrights (sess); /* -> faim_cb_parse_locaterights () */
	aim_bos_reqbuddyrights(sess, fr->conn); /* -> faim_cb_parse_buddyrights () */
	aim_im_reqparams(sess); /* -> faim_cb_icbmparaminfo () */

	return 1;
}

static int
faim_cb_parse_motd (aim_session_t *sess, aim_frame_t *fr, ...)
{
	char *msg;
	fu16_t id;
	va_list ap;

	va_start(ap, fr);
	id  = (fu16_t) va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);
	
	LOG (("MOTD: %s (%hu)\n", msg ? msg : "Unknown", id))
	if (id < 4)
		WARNING (("Your AIM connection may be lost."))
	
	return 1;
}

static int
faim_cb_selfinfo (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	aim_userinfo_t *info;

	va_start (ap, fr);
	info = va_arg (ap, aim_userinfo_t *);
	va_end (ap);

	LOG (("warn level=%d\n", info->warnlevel))

	return 1;
}

static int
faim_cb_ssi_parserights (aim_session_t *sess, aim_frame_t *fr, ...)
{
	int numtypes;
	fu16_t *maxitems;
	va_list ap;

	va_start(ap, fr);
	numtypes = va_arg(ap, int);
	maxitems = va_arg(ap, fu16_t *);
	va_end(ap);

	if (numtypes >= 0)
		LOG (("maxbuddies=%d\n", maxitems[0]))
	if (numtypes >= 1)
		LOG (("maxgroups=%d\n", maxitems[1]))
	if (numtypes >= 2)
		LOG (("maxpermits=%d\n", maxitems[2]))
	if (numtypes >= 3)
		LOG (("maxdenies=%d\n", maxitems[3]))

	return 1;
}

static int
faim_cb_ssi_parselist (aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct aim_ssi_item *curitem = NULL;
	int changed = FALSE;
	eb_local_account * ela = NULL;
	struct eb_aim_local_account_data *alad = NULL;
	LList *node;
	eb_account *ea = NULL;
	struct eb_aim_account_data *aad = NULL;
	
	ela = (eb_local_account *)sess->aux_data;
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	alad->listsyncing = TRUE;

	/* Clean the buddy list */
	aim_ssi_cleanlist(sess);

	/* Add from server list to local list */
	for (curitem=sess->ssi.local; curitem; curitem=curitem->next) {

		switch (curitem->type) {
			
		case 0x0000: /* Buddy */
			if (curitem->name) {
				char *group = aim_ssi_itemlist_findparentname (sess->ssi.local, curitem->name);
				char *alias = aim_ssi_getalias (sess->ssi.local, group, curitem->name);
								
				LOG (("[SSI] \\ Buddy %s, Group %s, Nick %s.", curitem->name, group, alias))

				ea = oscar_find_account_with_ela (curitem->name, ela, TRUE);
				
				if (ea) {
					aad = (struct eb_aim_account_data *)ea->protocol_account_data;
					aad->on_server = TRUE;
					LOG (("       Found the corresponding account"))
						
						/* TODO: is the buddy in the right group ? */
				} else {
					LOG (("       Cannot find the corresponding account"))
					ea = ay_aim_new_account (ela, curitem->name);
					add_unknown (ea);
					
					if (group != NULL && group[0] && strcmp ("~", group)) {
						if (!find_grouplist_by_name (group))
							add_group (group);
						move_contact (group, ea->account_contact);
					} else {
						if (!find_grouplist_by_name ("Buddies"))
							add_group ("Buddies");
						move_contact ("Buddies", ea->account_contact);
					}

					changed = TRUE;
				}
			} else {
				LOG (("[SSI] A buddy with no name ! :)"))
			}
			break;
			
		case 0x0001: /* Group */
			if (curitem->name) {
				LOG (("[SSI] Group %s", curitem->name))
			} else {
				LOG (("[SSI] A group with no name ! :)"))
			}
			break;

		case 0x0002: /* Permit buddy */
			break;
			
		case 0x0003: /* Deny buddy */
			break;

		case 0x0004: /* Permit/deny setting */
			if (curitem->data) {
				fu8_t permdeny;
				permdeny = aim_ssi_getpermdeny (sess->ssi.local);
				LOG (("[SSI] permdeny = %hhu", permdeny))
				if (permdeny != alad->perm_deny) {
					LOG (("[SSI] Changing permdeny from %d to %hhu\n",
					      alad->perm_deny, permdeny))
					alad->perm_deny = permdeny;
				}
				changed = TRUE;
			}
			break;
		}
	}
	
	/* FIXME: save account prefs ? */
	if (changed) {
		update_contact_list();
		write_contact_list();
	}

	/* Add from local list to server */
	for (node = alad->buddies; node; node = l_list_next (node)) {
		ea = (eb_account *)node->data;
		aad = (struct eb_aim_account_data *)ea->protocol_account_data;
		if (!aad->on_server) {
			LOG (("[SSI] Need to add buddy %s on the server !", ea->handle))
			aim_ssi_addbuddy(sess, ea->handle, ea->account_contact->group->name,
					 ea->account_contact->nick, NULL, NULL, 0);
		}
	}
	
	aim_ssi_enable(sess);
	alad->listsyncing = FALSE;

	return 1;
}

static int
faim_cb_ssi_parseack (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	struct aim_ssi_tmp *retval;
	
	va_start(ap, fr);
	retval = va_arg(ap, struct aim_ssi_tmp *);
	va_end(ap);
	
	while (retval) {
		LOG (("[SSI] Status is 0x%04hx for a 0x%04hx action with name %s\n", retval->ack,  retval->action, retval->item ? (retval->item->name ? retval->item->name : "no name") : "no item"))
		
		switch (retval->ack) {
		case 0x0000:
			LOG (("[SSI] Added successfully"))
			break;
		case 0x000c:
			LOG (("[SSI] Error, too many buddies in your buddy list"))
			break;
		case 0x000e:
			LOG (("[SSI] buddy requires authorization"))
			// TODO
			break;
		case 0xffff:
			LOG (("[SSI] ack : 0xffff"))
			break;
		default:
			LOG (("[SSI] Failed to add the buddy, unknown error"))
			break;
		}

		retval = retval->next;
	}

	return 1;
}

static int
faim_cb_parse_locaterights (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t maxsiglen;
	
	va_start(ap, fr);
	maxsiglen = (fu16_t) va_arg(ap, int);
	va_end(ap);
	
	LOG (("max away msg / signature len=%d\n", maxsiglen))

	aim_locate_setprofile(sess,
			      "us-ascii", profile, strlen(profile), /* Profile */
			      NULL, NULL, 0, /* Away message */
			      caps_aim); /* What we can do */

	return 1;
}

static int
faim_cb_parse_buddyrights (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t maxbuddies, maxwatchers;
	
	va_start(ap, fr);
	maxbuddies = (fu16_t) va_arg(ap, unsigned int);
	maxwatchers = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);
	
	LOG (("max buddies = %d, max watchers = %d\n", maxbuddies, maxwatchers))
	
	return 1;
}

static int
faim_cb_icbmparaminfo (aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct aim_icbmparameters *params;
	va_list ap;
	
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_start (ap, fr);
	params = va_arg (ap, struct aim_icbmparameters *);
	va_end (ap);
	
	params->flags = 0x0000000b;
	params->maxmsglen = 8000;
	params->minmsginterval = 0; /* in milliseconds */
	
	aim_im_setparams (sess, params);

	aim_clientready (sess, fr->conn);
	aim_srv_setavailmsg (sess, NULL);
	aim_bos_setidle (sess, fr->conn, 0);
	
	LOG (("You are now officially online.\n"))
	
	if(ela->status_menu) {
		/* Make sure set_current_state doesn't call us back */
		ela->connected = -1;
		eb_set_active_menu_status(ela->status_menu, AIM_ONLINE);
	}
	ela->connected = 1;
	ela->connecting = 0;
	
	ay_activity_bar_remove (alad->login_activity_bar);
	alad->login_activity_bar = -1;

	return 1;
}

static int
faim_cb_memrequest (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
        fu32_t offset, len;
        char *modname;
	
        va_start(ap, fr);
        offset = va_arg(ap, fu32_t);
        len = va_arg(ap, fu32_t);
        modname = va_arg(ap, char *);
        va_end(ap);

	LOG (("offset: %u, len: %u, file: %s\n",
	      offset, len, (modname ? modname : "aim.exe")))
	if (len == 0) {
		aim_sendmemblock(sess, fr->conn, offset, len, NULL, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		return 1;
	}
	
	WARNING (("You may be disconnected soon !"))
		
	return 1;
}

static int
faim_cb_parse_oncoming (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_account * user = NULL;
	aim_userinfo_t *userinfo;
	va_list ap;
	eb_local_account *ela = (eb_local_account *)sess->aux_data;

	va_start (ap, fr);
	userinfo = va_arg (ap, aim_userinfo_t *);
	va_end (ap);
	
	user = oscar_find_account_with_ela (userinfo->sn, ela, TRUE);

	if (user) {

		struct eb_aim_account_data * aad = user->protocol_account_data;
		
		if (userinfo->flags & AIM_FLAG_AWAY) {
			aad->status = AIM_AWAY;
		} else {
		 	aad->status = AIM_ONLINE;
		}
		buddy_login(user);

		aad->idle_time = userinfo->idletime;
		aad->evil = userinfo->warnlevel;
		buddy_update_status(user);
	} else {
		WARNING (("Unable to find user %s", userinfo->sn))
	}
	
	return 1;
}

static int
faim_cb_parse_offgoing (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_account * user = NULL;	
	aim_userinfo_t *userinfo;
        va_list ap;
	eb_local_account *ela = (eb_local_account *)sess->aux_data;

        va_start(ap, fr);
        userinfo = va_arg(ap, aim_userinfo_t *);
        va_end(ap);

	user = oscar_find_account_with_ela (userinfo->sn, ela, TRUE);

	if (user) {
		struct eb_aim_account_data *aad = user->protocol_account_data;
		aad->status = AIM_OFFLINE;
		buddy_logoff(user);
	} else {
		WARNING (("Unable to find user %s", userinfo->sn))
	}

	return 1;
}

static int
oscar_incoming_im_chan1 (aim_session_t *sess,
			 aim_conn_t *conn, aim_userinfo_t *userinfo,
			 struct aim_incomingim_ch1_args *args)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	eb_account *sender = NULL;

	LOG (("Message from = %s\n", userinfo->sn))
	LOG (("Message = %s\n", args->msg))

	sender = oscar_find_account_with_ela (userinfo->sn, ela, TRUE);

	if (!sender) {
		WARNING (("Sender == NULL"))
		sender = ay_aim_new_account (ela, userinfo->sn);
		add_unknown (sender);
		ay_aim_add_user (sender);
	}

	/* TODO: the message might be in unicode UCS-2BE format */
	eb_parse_incoming_message(ela, sender, args->msg);

	return 1;
}


static int
oscar_incoming_im_chan2 (aim_session_t *sess,
			 aim_conn_t *conn, aim_userinfo_t *userinfo,
			 struct aim_incomingim_ch2_args *args)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;

	LOG (("Rendez vous with %s", userinfo->sn))

	if (args->reqclass & AIM_CAPS_CHAT) {
		char *name;
		struct oscar_chat_room_data *ocrd;

		if (!args->info.chat.roominfo.name || !args->info.chat.roominfo.exchange || !args->msg)
			return 1;
		
		name = extract_name (args->info.chat.roominfo.name);
		LOG (("Chat room name = %s", name))

		ocrd = g_new0 (struct oscar_chat_room_data, 1);
		ocrd->name = (name) ? g_strdup (name) : g_strdup (args->info.chat.roominfo.name);
		ocrd->exchange = args->info.chat.roominfo.exchange;

		invite_dialog (ela, g_strdup (userinfo->sn),
			       (name) ? g_strdup (name) : g_strdup (args->info.chat.roominfo.name),
			       ocrd);

		if (name) {
			g_free (name);
		}
	}

	return 1;
}

static int
faim_cb_parse_incoming_im (aim_session_t *sess, aim_frame_t *fr, ...)
{
	int ret;

	va_list ap;
	fu16_t channel;
	aim_userinfo_t *userinfo;
	
	LOG (("faim_cb_parse_incoming_im"))

	va_start (ap, fr);
	channel = (fu16_t)va_arg (ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	switch (channel) {
	case 1: { /* Standard message */
		struct aim_incomingim_ch1_args *args;
                args = va_arg (ap, struct aim_incomingim_ch1_args *);
		va_end (ap);
		ret = oscar_incoming_im_chan1 (sess, fr->conn, userinfo, args);
	} break;

	case 2: { /* Rendez vous */
		struct aim_incomingim_ch2_args *args;
		args = va_arg (ap, struct aim_incomingim_ch2_args *);
		va_end (ap);
		ret = oscar_incoming_im_chan2 (sess, fr->conn, userinfo, args);
	} break;

	default:
		WARNING (("ICBM received on unsupported channel (channel 0x%04hx).", channel))
		ret = 0;
	}
#if 0
	else if (channel == 2)
	{
		int rendtype = va_arg(ap, int);
		if (rendtype & AIM_CAPS_CHAT)
		{
			char *encoding, *lang;
			struct aim_chat_roominfo *roominfo;
			eb_chat_room *ecr = g_new0(eb_chat_room, 1);
			
			userinfo = va_arg(ap, struct aim_userinfo_s *);
			roominfo = va_arg(ap, struct aim_chat_roominfo *);
			msg      = va_arg(ap, char *);
			encoding = va_arg(ap, char *);
			lang     = va_arg(ap, char *);
			va_end(ap);
			
#ifdef DEBUG
			g_message("invite: %s, %d", roominfo->name, roominfo->exchange);
#endif
			
			strncpy(ecr->id, sizeof(ecr->id)-1, roominfo->name);
			strncpy(ecr->room_name, sizeof(ecr->room_name)-1, roominfo->name);
			ecr->connected = FALSE;
			ecr->fellows = NULL;
			
			ecr->protocol_local_chat_room_data = (void *) ((int)roominfo->exchange);
			
			ecr->local_user = ela;
			
			alad->chat_room = ecr;
			
			ecr->local_user = ela;
			
			invite_dialog(ela, g_strdup(userinfo->sn), g_strdup(roominfo->name),
				      g_strdup(roominfo->name));
		}
		else
		{
			g_warning("Unknown rendtype %d", rendtype);
		}
	}
#endif	

	return ret;
}


static int
faim_cb_handle_redirect (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	va_list ap;
	struct aim_redirect_data *redir;
	aim_conn_t *tstconn;

	char *host;
	int port, i;

	LOG (("faim_cb_handle_redirect ()"))
	
	va_start (ap, fr);
	redir = va_arg (ap, struct aim_redirect_data *);
	va_end (ap);

	port = FAIM_LOGIN_PORT;
	for (i = 0; i < (int)strlen (redir->ip); i++) {
		if (redir->ip[i] == ':') {
			port = atoi (&(redir->ip[i+1]));
			break;
		}
	}
	host = g_strndup (redir->ip, i);


	switch (redir->group) {

	case 0x7: /* Authorizer */
		LOG ((" -> Authorizer"))
		/* TODO */
		break;

	case 0xd: /* ChatNav */
		LOG ((" -> ChatNav"))
		tstconn = aim_newconn (&(alad->aimsess), AIM_CONN_TYPE_CHATNAV, NULL);
		alad->conn_chatnav = tstconn;

		if (!tstconn) {
			WARNING (("unable to connect to chatnav server\n"))
			g_free (host);
			return 1;
		}

		aim_conn_addhandler(&(alad->aimsess), tstconn, AIM_CB_FAM_SPECIAL,
				    AIM_CB_SPECIAL_CONNERR, faim_cb_connerr, 0);
		aim_conn_addhandler(&(alad->aimsess), tstconn, AIM_CB_FAM_SPECIAL,
				    AIM_CB_SPECIAL_CONNINITDONE, faim_cb_conninitdone_chatnav, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;

		if (proxy_connect_host (host, port,
					oscar_chatnav_connect, ela,
					oscar_chatnav_connect_status) < 0) {
			aim_conn_kill (sess, &tstconn);
			alad->conn_chatnav = NULL;
			WARNING (("unable to create socket to chatnav server\n"))
			g_free (host);
			return 1;
		}

		aim_sendcookie (sess, tstconn, redir->cookielen, redir->cookie);

		break;

	case 0xe: { /* Chat */
		struct oscar_chat_room *ocr;
		eb_chat_room *ecr;

		LOG ((" -> Chat"))

		tstconn = aim_newconn (&(alad->aimsess), AIM_CONN_TYPE_CHAT, NULL);

		if (!tstconn) {
			WARNING (("unable to connect to chatnav server\n"))
			g_free (host);
			return 1;
		}

		aim_conn_addhandler(&(alad->aimsess), tstconn, AIM_CB_FAM_SPECIAL,
				    AIM_CB_SPECIAL_CONNERR, faim_cb_connerr, 0);
		aim_conn_addhandler(&(alad->aimsess), tstconn, AIM_CB_FAM_SPECIAL,
				    AIM_CB_SPECIAL_CONNINITDONE, faim_cb_conninitdone_chat, 0);

		ocr = g_new0 (struct oscar_chat_room, 1);
		ocr->conn = tstconn;
		ocr->name = g_strdup(redir->chat.room);
		ocr->exchange = redir->chat.exchange;
		ocr->instance = redir->chat.instance;
		ocr->show = extract_name(redir->chat.room);

		ecr = oscar_find_chat_room_by_name (alad, ocr->show);
		ecr->protocol_local_chat_room_data = ocr;

		ocr->conn->status |= AIM_CONN_STATUS_INPROGRESS;

		if (proxy_connect_host (host, port,
					oscar_chat_connect, ecr,
					oscar_chatnav_connect_status) < 0) {
			aim_conn_kill (&(alad->aimsess), &tstconn);
			WARNING (("unable to create socket to chat server\n"))
			g_free (host);
			g_free (ocr->name);
			g_free (ocr->show);
			g_free (ocr);
			return 1;
		}

		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);

		} break;

	case 0x0010: /* Icon */
		LOG ((" -> Icon"))
		/* TODO */
		break;

	case 0x0018: /* EMail */
		LOG ((" -> EMail"))
		/* TODO */
		break;

	default:
		WARNING ((" -> got redirect for unknown service 0x%04hx\n", redir->group))
		break;
	}

	g_free (host);

	return 1;
}

static int
faim_cb_conninitdone_chatnav (aim_session_t *sess, aim_frame_t *fr, ...)
{
	LOG (("faim_cb_conninitdone_chatnav()"))

	aim_conn_addhandler (sess, fr->conn, 0x000d, 0x0001, faim_cb_parse_genericerr, 0);
	aim_conn_addhandler (sess, fr->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, faim_cb_chatnav_info, 0);

	aim_clientready (sess, fr->conn);
	aim_chatnav_reqrights (sess, fr->conn);

	return 1;
}

static int
faim_cb_chatnav_info (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t type;
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_start (ap, fr);
	type = (fu16_t)va_arg (ap, unsigned int);

	LOG (("faim_cb_chatnav_info() with type %04hx", type))

	switch(type) {
	case 0x0002: {
		fu8_t maxrooms;
		struct aim_chat_exchangeinfo *exchanges;
		int exchangecount, i;

		maxrooms = (fu8_t) va_arg(ap, unsigned int);
		exchangecount = va_arg(ap, int);
		exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);

		LOG (("chat info: Chat Rights:"))
		LOG (("chat info: \tMax Concurrent Rooms: %hhd", maxrooms))
		LOG (("chat info: \tExchange List: (%d total)", exchangecount))

		for (i = 0; i < exchangecount; i++) {
			LOG (("chat info: \t\t%hu    %s", exchanges[i].number, exchanges[i].name ? exchanges[i].name : ""))
		}

		while (alad->pending_rooms) {
			struct oscar_chat_room_data *ocrd =
				(struct oscar_chat_room_data *)alad->pending_rooms->data;
			LOG (("Creating room %s", ocrd->name))

			alad->rooms = l_list_append (alad->rooms, ocrd->ecr);
			aim_chatnav_createroom (sess, fr->conn, ocrd->name, ocrd->exchange);

			g_free (ocrd->name);
			alad->pending_rooms = l_list_remove (alad->pending_rooms, ocrd);
			g_free (ocrd);
		}
	}
		break;

	case 0x0008: {
		char *fqcn, *name, *ck;
		fu16_t instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
		fu8_t createperms;
		fu32_t createtime;

		fqcn = va_arg(ap, char *);
		instance = (fu16_t)va_arg(ap, unsigned int);
		exchange = (fu16_t)va_arg(ap, unsigned int);
		flags = (fu16_t)va_arg(ap, unsigned int);
		createtime = va_arg(ap, fu32_t);
		maxmsglen = (fu16_t)va_arg(ap, unsigned int);
		maxoccupancy = (fu16_t)va_arg(ap, unsigned int);
		createperms = (fu8_t)va_arg(ap, unsigned int);
		unknown = (fu16_t)va_arg(ap, unsigned int);
		name = va_arg(ap, char *);
		ck = va_arg(ap, char *);

		LOG (("created room: %s %hu %hu %hu %u %hu %hu %hhu %hu %s %s\n", fqcn, exchange, instance, flags, createtime, maxmsglen, maxoccupancy, createperms, unknown, name, ck))
		aim_chat_join (&(alad->aimsess), alad->conn, exchange, ck, instance);
	}
		break;
	default:
		LOG (("chatnav info: unknown type (%04hx)\n", type))
		break;
	}

	va_end(ap);

	return 1;
}

static int
faim_cb_conninitdone_chat (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_chat_room *ecr;
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	aim_conn_addhandler (&(alad->aimsess), fr->conn, AIM_CB_FAM_CHT,
			     0x0001, faim_cb_parse_genericerr, 0);
	aim_conn_addhandler (&(alad->aimsess), fr->conn, AIM_CB_FAM_CHT,
			     AIM_CB_CHT_USERJOIN, faim_cb_chat_join, 0);
	aim_conn_addhandler (&(alad->aimsess), fr->conn, AIM_CB_FAM_CHT,
			     AIM_CB_CHT_USERLEAVE, faim_cb_chat_leave, 0);
	aim_conn_addhandler (&(alad->aimsess), fr->conn, AIM_CB_FAM_CHT,
			     AIM_CB_CHT_ROOMINFOUPDATE, faim_cb_chat_info_update, 0);
	aim_conn_addhandler (&(alad->aimsess), fr->conn, AIM_CB_FAM_CHT,
			     AIM_CB_CHT_INCOMINGMSG, faim_cb_chat_incoming_msg, 0);

	aim_clientready(&(alad->aimsess), fr->conn);

	ecr = oscar_find_chat_room_by_conn (alad, fr->conn);
	eb_join_chat_room (ecr, TRUE);

	return 1;
}

static int
faim_cb_chat_join (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	eb_chat_room *ecr;

	LOG (("faim_cb_chat_join()"))

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	ecr = oscar_find_chat_room_by_conn (alad, fr->conn);
	if (!ecr) {
		WARNING (("Can't find chatroom !"))
		return 1;
	}

	for (i = 0; i < count; i++) {
		eb_account *ea = oscar_find_account_with_ela (info[i].sn, ela, TRUE);
		if (ea) {
			eb_chat_room_buddy_arrive (ecr, ea->account_contact->nick, info[i].sn);
		} else {
			eb_chat_room_buddy_arrive (ecr, info[i].sn, info[i].sn);
		}
	}

	return 1;
}

static int
faim_cb_chat_leave (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	eb_chat_room *ecr;

	LOG (("faim_cb_chat_leave()"))

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	ecr = oscar_find_chat_room_by_conn (alad, fr->conn);
	if (!ecr) {
		WARNING (("Can't find chatroom !"))
		return 1;
	}

	for (i = 0; i < count; i++) {
		eb_chat_room_buddy_leave (ecr, info[i].sn);
	}

	return 1;
}

static int
faim_cb_chat_info_update (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	aim_userinfo_t *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	fu16_t unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	fu32_t creationtime;

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount= va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (fu16_t)va_arg(ap, unsigned int);
	creationtime = va_arg(ap, fu32_t);
	maxmsglen = (fu16_t)va_arg(ap, unsigned int);
	unknown_d2 = (fu16_t)va_arg(ap, unsigned int);
	unknown_d5 = (fu16_t)va_arg(ap, unsigned int);
	maxvisiblemsglen = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	LOG (("inside chat_info_update (maxmsglen = %hu, maxvislen = %hu)\n", maxmsglen, maxvisiblemsglen))

	return 1;
}

static int
faim_cb_chat_incoming_msg (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_list ap;
	aim_userinfo_t *info;
	char *msg;

	eb_chat_room *ecr;
	eb_account *ea;

	va_start (ap, fr);
	info = va_arg (ap, aim_userinfo_t *);
	msg = va_arg (ap, char *);
	va_end (ap);

	LOG (("faim_cb_chat_incoming_msg(): %s => %s", info->sn, msg))

	ecr = oscar_find_chat_room_by_conn (alad, fr->conn);
	if (!ecr) {
		WARNING (("Can't find chatroom !"))
		return 1;
	}

	ea = oscar_find_account_with_ela (info->sn, ela, TRUE);
	if (ea) {
		eb_chat_room_show_message (ecr, ea->account_contact->nick, msg);
	} else {
		eb_chat_room_show_message (ecr, info->sn, msg);
	}

	return 1;
}

#if 0

int eb_aim_msg_missed(struct aim_session_t * sess,struct command_rx_struct *command, ...)
{
  eb_account *ea = find_account_by_handle(&command->data[13], SERVICE_INFO.protocol_id);
  eb_chat_window_display_error(ea, "Message was sent too fast.");

  return 1;
}

int eb_aim_msg_error(struct aim_session_t * sess,struct command_rx_struct *command, ...)
{
  ay_do_error( "AIM Error", "Last message could not be sent" );

  return 1;
}

#endif /* #if 0 */


/************************/
/* Connection callbacks */
/************************/

static void
oscar_login_connect (int fd, int error, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;

	LOG (("oscar_login_connect(): fd=%d, error=%d", fd, error))

	alad->conn->fd = fd;

	if (fd < 0) {
		connect_error (alad, "Could not connect to host");
		ref_count--;
		return;
	}

	aim_conn_completeconnect (&(alad->aimsess), alad->conn);

	alad->input = eb_input_add (alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION ,
				    ay_aim_callback, account);
}

static void
oscar_login_connect_status (const char *msg, void *data)
{
	LOG (("oscar_login_connect_status() : %s", msg))
	/* TODO */
}			    

static void
oscar_chatnav_connect (int fd, int error, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;

	LOG (("oscar_chatnav_connect(): fd=%d, error=%d", fd, error))

	alad->conn_chatnav->fd = fd;

	if (fd < 0) {
		WARNING (("unable to create socket to chatnav server\n"))
		return;
	}

	aim_conn_completeconnect (&(alad->aimsess), alad->conn_chatnav);

	alad->input_chatnav = eb_input_add (alad->conn_chatnav->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION ,
					    ay_aim_callback, account);
}

static void
oscar_chatnav_connect_status (const char *msg, void *data)
{
	LOG (("oscar_chatnav_connect_status() : %s", msg))
}			    

static void
oscar_chat_connect (int fd, int error, void *data)
{
	eb_chat_room *ecr = (eb_chat_room *)data;
	struct oscar_chat_room *ocr = (struct oscar_chat_room *)ecr->protocol_local_chat_room_data;
	eb_local_account *account = ecr->local_user;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;

	LOG (("oscar_chat_connect(): fd=%d, error=%d", fd, error))

	if (fd < 0) {
		aim_conn_kill (&(alad->aimsess), &(ocr->conn));
		g_free (ocr->name);
		g_free (ocr->show);
		g_free (ocr);
		WARNING (("unable to create socket to chat server\n"))
		return;
	}

	ocr->conn->fd = fd;
	aim_conn_completeconnect (&(alad->aimsess), ocr->conn);
	ocr->input = eb_input_add (ocr->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION ,
				   ay_aim_callback, account);
}

/*********************************/
/***  callbacks used by Ayttm  ***/
/*********************************/

static gboolean
ay_aim_query_connected (eb_account *account)
{
	struct eb_aim_account_data *aad =
		(struct eb_aim_account_data *)account->protocol_account_data;
	return (aad->status != AIM_OFFLINE);
}

static void
ay_aim_cancel_connect (void *data)
{
	eb_local_account *ela = (eb_local_account *)data;
	ay_aim_logout (ela);
}

/* Helper function */
static void
connect_error (struct eb_aim_local_account_data *alad, char *msg)
{
	if (alad->login_activity_bar)
		ay_activity_bar_remove (alad->login_activity_bar);	
	
	ay_do_error ("AIM Error", msg);
}

static void
ay_oscar_finish_login (const char *password, void *data)
{
	eb_local_account *account = (eb_local_account *)data;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;
	char buf[256];

	snprintf (buf, sizeof(buf), "Logging in to AIM account: %s", account->handle);
	alad->login_activity_bar = ay_activity_bar_add (buf, ay_aim_cancel_connect, account);

	strncpy (alad->tmp_password, password, sizeof (alad->tmp_password));

	aim_session_init(&(alad->aimsess), AIM_SESS_FLAGS_NONBLOCKCONNECT, 1);
	aim_setdebuggingcb (&(alad->aimsess), faim_cb_oscar_debug);
	alad->aimsess.aux_data = account;

	/* This is needed so we don't have to call aim_tx_flushqueue each  *
	 * time we want to send something !                                */
	aim_tx_setenqueue(&(alad->aimsess), AIM_TX_IMMEDIATE, NULL);
	
	alad->conn = aim_newconn(&(alad->aimsess), AIM_CONN_TYPE_AUTH, NULL);
	if(!alad->conn)
	{
		connect_error (alad, "Failed to connect to AIM server.");
		ref_count--;
		fprintf (stderr, "ay_aim_login: Decrementing ref_count to %i\n", ref_count);
		return;
	}

	aim_conn_addhandler (&(alad->aimsess), alad->conn,
			     AIM_CB_FAM_ATH, AIM_CB_ATH_AUTHRESPONSE,
			     faim_cb_parse_login, 0);
	aim_conn_addhandler (&(alad->aimsess), alad->conn,
			     AIM_CB_FAM_ATH, AIM_CB_ATH_LOGINRESPONSE,
			     faim_cb_parse_authresp, 0);
	aim_conn_addhandler (&(alad->aimsess), alad->conn,
			     AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR,
			     faim_cb_connerr, 0);

	alad->conn->status |= AIM_CONN_STATUS_INPROGRESS;

	if (proxy_connect_host (FAIM_LOGIN_SERVER, FAIM_LOGIN_PORT,
				oscar_login_connect, account,
				oscar_login_connect_status) < 0) {
		connect_error (alad, "Could not connect to host");
		ref_count--;
		return;
	}

	LOG (("Requesting connection with screename %s\n", account->handle))
	aim_request_login (&(alad->aimsess), alad->conn, account->handle);
}

static void
ay_aim_login (eb_local_account *account)
{
	struct eb_aim_local_account_data *alad;
	char buf[256];

	if (account->connected || account->connecting) return;
	account->connecting = 1;

	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	ref_count++;
	LOG (("ay_aim_login: Incrementing ref_count to %i\n", ref_count))

	if (alad->prompt_password || !alad->password || !strlen(alad->password)) {
		
		snprintf (buf, sizeof (buf), "AIM password for: %s", account->handle);
		do_password_input_window (buf, "", ay_oscar_finish_login, account);
	} else {
		ay_oscar_finish_login(alad->password, account);
	}
}


/*
 * This helper function is called for each buddy, and just make the given
 * buddy offline.
 */
static void
make_buddy_offline (void *b, void *n)
{
	eb_account *user;
	struct eb_aim_account_data *aad;
	
	user = (eb_account *)b;
	aad = (struct eb_aim_account_data *)user->protocol_account_data;
	
	aad->on_server = FALSE;

	if (aad->status != AIM_OFFLINE) {
		aad->status = AIM_OFFLINE;
		buddy_logoff(user);
		buddy_update_status(user);
	}
}

static void
ay_aim_logout (eb_local_account *account)
{
	struct eb_aim_local_account_data * alad;

	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	eb_input_remove(alad->input);
	aim_conn_kill(&(alad->aimsess), &(alad->conn));
	alad->status=AIM_OFFLINE;
	ref_count--;
	LOG (("ay_aim_logout: Decrementing ref_count to %i\n", ref_count))
	
	/* mark buddies as Offline */
	l_list_foreach (alad->buddies, make_buddy_offline, NULL);
	
	account->connected = 0;
	account->connecting = 0;
}

static LList *
ay_aim_write_local_config (eb_local_account *ela)
{
	return eb_input_to_value_pair (ela->prefs);
}

static void
oscar_init_account_prefs (eb_local_account *ela)
{
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	input_list *il = g_new0 (input_list, 1);

	ela->prefs = il;

	il->widget.entry.value = ela->handle;
	il->name = "SCREEN_NAME";
	il->label= "Screen name:";
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0 (input_list, 1);
	il = il->next;
	il->widget.entry.value = alad->password;
	il->name = "PASSWORD";
	il->label= "Password:";
	il->type = EB_INPUT_PASSWORD;

	il->next = g_new0 (input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &alad->prompt_password;
	il->name = "prompt_password";
	il->label= "_Ask for password at Login time";
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0 (input_list, 1);
	il = il->next;
	il->widget.checkbox.value = &(ela->connect_at_startup);
	il->name = "CONNECT";
	il->label= "Connect at startup";
	il->type = EB_INPUT_CHECKBOX;
}

static eb_local_account *
ay_aim_read_local_config (LList *pairs)
{
	
	eb_local_account * ela = g_new0(eb_local_account, 1);
	struct eb_aim_local_account_data * alad =
		g_new0(struct eb_aim_local_account_data, 1);
	
	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = alad;
	alad->status = AIM_OFFLINE;
	alad->perm_deny = 0;

	oscar_init_account_prefs (ela);
	eb_update_from_value_pair(ela->prefs, pairs);

	strncpy(ela->alias, ela->handle, sizeof(ela->alias));

	return ela;
}

static eb_account *
ay_aim_read_config (eb_account *ea, LList *config)
{
	struct eb_aim_account_data *aad =  g_new0 (struct eb_aim_account_data, 1);
	
	aad->status = AIM_OFFLINE;
	aad->on_server= FALSE;
	ea->protocol_account_data = aad;
	
	ay_aim_add_user(ea);

	return ea;
}

static void
ay_aim_add_user (eb_account *account)
{
	eb_local_account *ela = NULL;
	struct eb_aim_local_account_data *alad = NULL;

	char *name;
	char *group;
	char *nick;

	ela = account->ela;
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	name = account->handle;
	group = account->account_contact->group->name;
	nick = account->account_contact->nick;

	LOG (("Adding buddy %s in group %s (nick=%s)", name, group, nick))

	if (!l_list_find (alad->buddies, account))
		alad->buddies = l_list_append (alad->buddies, account);
	else
		return;

	if ((alad->status != AIM_OFFLINE) && (!alad->listsyncing)) {
		LOG (("Adding the buddy to the remote buddy list"))
		
		aim_ssi_addbuddy (&(alad->aimsess), name, group, nick, NULL, NULL, 0);
	}
}

static eb_account *
ay_aim_new_account (eb_local_account * ela, const char *account)
{
	eb_account *a = g_new0 (eb_account, 1);
	struct eb_aim_account_data *aad = g_new0 (struct eb_aim_account_data, 1);

	LOG (("new account = %s\n", account))

	a->protocol_account_data = aad;
	strncpy (a->handle, account, sizeof(a->handle)-1);
	a->service_id = SERVICE_INFO.protocol_id;
	aad->status = AIM_OFFLINE;
	aad->on_server = FALSE;
	a->ela = ela;
	return a;
}

static void
ay_aim_del_user (eb_account *account)
{
	eb_local_account *ela = NULL;
	struct eb_aim_local_account_data *alad = NULL;
	
	ela = account->ela;
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	if (!l_list_find (alad->buddies, account)) {
		WARNING (("FIXME"))
		return;
	}
	
	alad->buddies = l_list_remove (alad->buddies, account);

	if ((alad->status != AIM_OFFLINE) && (!alad->listsyncing)) {
		LOG (("suppression ### %s ..... %s", account->handle, account->account_contact->group->name))
		aim_ssi_delbuddy (&(alad->aimsess), account->handle,
				  account->account_contact->group->name);
	}
	
	g_free (account->protocol_account_data);
}

static void
ay_oscar_set_away (eb_local_account * account, gchar * message, int away)
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	if (message) {
		if (account->status_menu) {
			eb_set_active_menu_status (account->status_menu, AIM_AWAY);
		}
	} else {
		if (account->status_menu) {
			eb_set_active_menu_status (account->status_menu, AIM_ONLINE);
		}
	}
}

static void
ay_aim_send_im (eb_local_account *account_from,
		eb_account       *account_to,
		gchar            *message)
{
	struct aim_sendimext_args args;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account_from->protocol_local_account_data;
	
	args.flags = AIM_IMFLAGS_ACK | AIM_IMFLAGS_CUSTOMFEATURES;
	args.features = features_aim;
	args.featureslen = sizeof (features_aim);
	args.destsn = account_to->handle;
	
	/* FIXME: we only send ISO-8859-1 :-( */
	args.charset = 0x0003;
	args.charsubset = 0x0000;
	args.flags |= AIM_IMFLAGS_ISO_8859_1;
	args.msg = message;
	args.msglen = strlen (message);

	aim_im_sendch1_ext (&alad->aimsess, &args);
}


static LList *
ay_aim_get_states ()
{
	LList * states = NULL;
	LOG (("ay_aim_get_states ()"))
	
	states = l_list_append (states, "Online");
	states = l_list_append (states, "Away");
	states = l_list_append (states, "Offline");
	
	return states;
}

static gint
ay_aim_get_current_state (eb_local_account * account)
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	
	return alad->status;
}

static void
ay_aim_set_current_state (eb_local_account * account, gint state)
{
	char *awaymsg;
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	LOG (("ay_aim_set_current_state = %d", state))

	switch (state) {
	case AIM_ONLINE:
		if (account->connected == 0 && account->connecting == 0) {
			ay_aim_login (account);
		}

		/* This is how we suppress away message */
		aim_locate_setprofile(&(alad->aimsess),
				      NULL, NULL, 0, /* Profile */
				      NULL, "", 0, /* Away message */
				      caps_aim); /* What we can do */
		break;

	case AIM_AWAY:
		if (account->connected == 0 && account->connecting == 0) {
			ay_aim_login (account);
		}
		if (is_away) {
			awaymsg = get_away_message();
		} else {
			awaymsg = "User is currently away";
		}
		aim_locate_setprofile(&(alad->aimsess),
				      NULL, NULL, 0,
				      "iso-8859-1", awaymsg, strlen (awaymsg),
				      caps_aim);
		break;

	case AIM_OFFLINE:
		if (account->connected) {
			ay_aim_logout (account);
		}
		break;
	}

	alad->status = state;
}

static char **
ay_aim_get_status_pixmap (eb_account * account)
{
	struct eb_aim_account_data *aad;
	
	aad = (struct eb_aim_account_data *)account->protocol_account_data;
	
	if (aad->status == AIM_AWAY || aad->status == AIM_OFFLINE)
		return aim_away_xpm;
	else
		return aim_online_xpm;
}


static gchar *
ay_aim_get_status_string (eb_account *account)
{
	static gchar string[255], buf[255];
	struct eb_aim_account_data * aad =
		(struct eb_aim_account_data *)account->protocol_account_data;
	strcpy(buf, "");
	strcpy(string, "");
		
	if(aad->idle_time){
		int hours, minutes, days;
		//minutes = (time(NULL) - (aad->idle_time*60))/60;
		minutes = aad->idle_time;
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


static char *
ay_aim_check_login (const char *user, const char *pass)
{
	return NULL;
}

static void
oscar_create_room (eb_local_account *account, struct oscar_chat_room_data *ocrd)
{
	aim_conn_t *cur;
	struct eb_aim_local_account_data *alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	if ((cur = aim_getconn_type(&(alad->aimsess), AIM_CONN_TYPE_CHATNAV))) {
		LOG (("chatnav exists, creating room"))

		alad->rooms = l_list_append (alad->rooms, ocrd->ecr);
		aim_chatnav_createroom(&(alad->aimsess), cur, ocrd->name, ocrd->exchange);
		g_free (ocrd->name);
		g_free (ocrd);
	} else {
		LOG (("chatnav does not exist, opening chatnav"))
		alad->pending_rooms = l_list_append (alad->pending_rooms, ocrd);
		aim_reqservice(&(alad->aimsess), alad->conn, AIM_CONN_TYPE_CHATNAV);
	}
}

static void
ay_oscar_accept_invite (eb_local_account *account, void *data)
{
	struct oscar_chat_room_data *ocrd =
		(struct oscar_chat_room_data *)data;
	eb_chat_room * ecr = g_new0 (eb_chat_room, 1);

	LOG (("ay_oscar_accept_invite()"))

	strncpy(ecr->room_name, ocrd->name, sizeof(ecr->room_name));
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;

	ocrd->ecr = ecr;

	oscar_create_room (account, ocrd);
}

static void
ay_oscar_decline_invite (eb_local_account *account, void *data)
{
	struct oscar_chat_room_data *ocrd =
		(struct oscar_chat_room_data *)data;
	LOG (("ay_oscar_decline_invite()"))
	g_free (ocrd->name);
	g_free (ocrd);
}

static eb_chat_room *
ay_oscar_make_chat_room (char *name, eb_local_account *account, int is_public)
{
	struct oscar_chat_room_data *ocrd;
	eb_chat_room * ecr = g_new0 (eb_chat_room, 1);

	LOG (("ay_oscar_make_chat_room()"))

	strncpy(ecr->room_name, name, sizeof(ecr->room_name));
	ecr->fellows = NULL;
	ecr->connected = FALSE;
	ecr->local_user = account;

	ocrd = g_new0 (struct oscar_chat_room_data, 1);
	ocrd->name = g_strdup (name);
	ocrd->exchange = 4; /* FIXME */
	ocrd->ecr = ecr;

	oscar_create_room (account, ocrd);

	return ecr;
}

static void
ay_oscar_join_chat_room (eb_chat_room * room)
{
	LOG (("ay_oscar_join_chat_room()"))
}

static void
ay_oscar_leave_chat_room (eb_chat_room * room)
{
	struct oscar_chat_room *ocr = (struct oscar_chat_room *)room->protocol_local_chat_room_data;
	eb_local_account *account = room->local_user;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;

	LOG (("ay_oscar_leave_chat_room()"))

	alad->rooms = l_list_remove (alad->rooms, room);
	g_free (ocr->name);
	g_free (ocr->show);

	aim_conn_kill (&(alad->aimsess), &(ocr->conn));
	eb_input_remove (ocr->input);

	g_free (ocr);
}

static void
ay_oscar_send_chat_room_message (eb_chat_room *room, char * message)
{
	struct oscar_chat_room *ocr = (struct oscar_chat_room *)room->protocol_local_chat_room_data;
	eb_local_account *account = room->local_user;
	struct eb_aim_local_account_data *alad =
		(struct eb_aim_local_account_data *)account->protocol_local_account_data;

	aim_chat_send_im(&(alad->aimsess), ocr->conn, 0, message, strlen(message));
}

static void
ay_oscar_send_invite (eb_local_account *account, eb_chat_room *room,
		      char *user, const char *message)
{
	struct eb_aim_local_account_data * alad;
	struct oscar_chat_room *ocr;

	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	ocr = (struct oscar_chat_room *)room->protocol_local_chat_room_data;

	aim_chat_invite (&(alad->aimsess), alad->conn, user, message,
			 ocr->exchange, ocr->name, 0x0);
}

#if 0

void eb_aim_set_idle( eb_local_account * ela, gint idle )
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	aim_bos_setidle( &(alad->aimsess), alad->conn, idle );
}

#endif

struct service_callbacks *
query_callbacks ()
{
	struct service_callbacks * sc;
	
	LOG (("query_callbacks ()\n"))

	sc = g_new0( struct service_callbacks, 1 );

	sc->query_connected = ay_aim_query_connected;
	sc->login = ay_aim_login;
	sc->logout = ay_aim_logout;

	sc->send_im = ay_aim_send_im;

	/* Not yet implemented */
	sc->send_typing = NULL;
	sc->send_cr_typing = NULL;

	sc->write_local_config = ay_aim_write_local_config;
	sc->read_local_account_config = ay_aim_read_local_config;
	sc->read_account_config = ay_aim_read_config;

	sc->get_states = ay_aim_get_states;
	sc->get_current_state = ay_aim_get_current_state;
	sc->set_current_state = ay_aim_set_current_state;

	sc->check_login = ay_aim_check_login;

	sc->add_user = ay_aim_add_user;
	sc->del_user = ay_aim_del_user;

	/* Not yet implemented */
	sc->ignore_user = NULL;
	sc->unignore_user = NULL;

	/* Not yet implemented */
	sc->change_user_name = NULL;

	/* Not yet implemented */
	sc->change_group = NULL;
	sc->del_group = NULL;
	sc->add_group = NULL;
	sc->rename_group = NULL;

	/* Not yet implemented */
	sc->is_suitable = NULL;

	sc->new_account = ay_aim_new_account;

	sc->get_status_string = ay_aim_get_status_string;
	sc->get_status_pixmap = ay_aim_get_status_pixmap;

	sc->set_idle = NULL; /* eb_aim_set_idle; */
	sc->set_away = ay_oscar_set_away;

	/* Chat room */

	sc->send_chat_room_message = ay_oscar_send_chat_room_message;
	sc->join_chat_room =  ay_oscar_join_chat_room;
	sc->leave_chat_room = ay_oscar_leave_chat_room;
	sc->make_chat_room = ay_oscar_make_chat_room;

	sc->send_invite = ay_oscar_send_invite;
	sc->accept_invite = ay_oscar_accept_invite;
	sc->decline_invite = ay_oscar_decline_invite;

	/* Not yet implemented */
	sc->send_file = NULL;

	/* Not yet implemented */
	sc->terminate_chat = NULL;

	sc->get_info = NULL; /* eb_aim_get_info; */

	/* Not yet implemented */
	sc->get_prefs = NULL;
	sc->read_prefs_config = NULL;
	sc->write_prefs_config = NULL;

	/* Not yet implemented */
	sc->add_importers = NULL;

	sc->get_smileys = NULL; /* eb_default_smileys; */
	sc->get_color = ay_aim_get_color;

	/* Not yet implemented */
	sc->free_account_data = NULL;

	/* Not yet implemented */
	sc->handle_url = NULL;

	/* Not yet implemented */
	sc->get_public_chatrooms = NULL;

	return sc;
}

/* Remove spaces, and convert to lower case.
 * Note that we have two buffers to allow things like
 * strcmp(aim_normalize(s1), aim_normalize(s2)) */
static const char *
aim_normalize (const char *s)
{
	static char buf[512];
	static int current = 0;

	const char *r;
	char *w;

	current = (current == 0) ? 1 : 0;
	r = s;
	w = buf + (current * 256);

	while (*r) {
		if (*r != ' ') {
			*w = tolower (*r);
			w++;
		}
		r++;
	}
	*w = '\0';
	
	return buf + (current * 256);
}


/* Extract Chat room name */
static char *
extract_name (const char *name)
{
	char *tmp, *x;
	int i, j;
	
	if (!name)
		return NULL;
	
	x = strchr(name, '-');
	
	if (!x) return NULL;
	x = strchr(++x, '-');
	if (!x) return NULL;
	tmp = g_strdup(++x);

	for (i = 0, j = 0; x[i]; i++) {
		char hex[3];
		if (x[i] != '%') {
			tmp[j++] = x[i];
			continue;
		}
		strncpy(hex, x + ++i, 2); hex[2] = 0;
		i++;
		tmp[j++] = strtol(hex, NULL, 16);
	}

	tmp[j] = 0;
	return tmp;
}

static eb_account *
oscar_find_account_with_ela (const char *handle, eb_local_account *ela, gint should_update)
{
	eb_account *result = NULL;


	result = find_account_with_ela (aim_normalize(handle), ela);

	/* Really it doesn't mean it doesn't exist ! Try to find
	   it in our local buddy list */
	if (!result) {
		struct eb_aim_local_account_data *alad =
			(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
		LList *node = alad->buddies;
		eb_account *tmp;

		while (node) {
			tmp = (eb_account *)node->data;
			if (strcmp (aim_normalize (tmp->handle),
				    aim_normalize (handle)) == 0) {
				LOG (("Yeah this code is useful ! :)"))
				result = tmp;
				break;
			}
			node = node->next;
		}
	}

	if (result) {
		if (should_update && (strcmp (handle, result->handle) != 0)) {
			WARNING (("Updating contact list from %s to %s", result->handle, handle))
			strncpy (result->handle, handle, sizeof (result->handle)-1);
			write_contact_list ();
		}
	}

	if (result) {
		LOG (("oscar_find_account_with_ela(): %s => %s", handle, result->account_contact->nick))
	} else {
		LOG (("oscar_find_account_with_ela(): nothing found"))
	}

	return result;
}

static eb_chat_room *
oscar_find_chat_room_by_name (struct eb_aim_local_account_data *alad,
			      const char *name)
{
	LList *node;
	eb_chat_room *result = NULL;
	eb_chat_room *ecr = NULL;

	node = alad->rooms;
	while (node) {
		ecr = (eb_chat_room *)node->data;
		if (strcmp (ecr->room_name, name) == 0) {
			result = ecr;
			break;
		}

		node = l_list_next (node);
	}

	return result;
}

static aim_conn_t *
oscar_find_chat_conn_by_source (struct eb_aim_local_account_data *alad,
				int fd)
{
	LList *node;
	eb_chat_room *ecr = NULL;
	struct oscar_chat_room *ocr = NULL;
	aim_conn_t *result = NULL;

	node = alad->rooms;
	while (node) {
		ecr = (eb_chat_room *)node->data;
		ocr = (struct oscar_chat_room *)ecr->protocol_local_chat_room_data;
		if (ocr->conn->fd == fd) {
			result = ocr->conn;
			break;
		}

		node = l_list_next (node);
	}

	return result;
}

static eb_chat_room *
oscar_find_chat_room_by_conn (struct eb_aim_local_account_data *alad,
			      aim_conn_t *conn)
{
	LList *node;
	eb_chat_room *result = NULL;
	eb_chat_room *ecr = NULL;

	node = alad->rooms;
	while (node) {
		ecr = (eb_chat_room *)node->data;

		if (((struct oscar_chat_room *)ecr->protocol_local_chat_room_data)->conn == conn) {
			result = ecr;
			break;
		}

		node = l_list_next (node);
	}

	return result;
}
