/*
 * EveryBuddy 
 *
 * Copyright (C) 2003, the Ayttm team
 * Modified by Nicolas Peninguy <ayttm@free-anatole.net>
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
#include <signal.h>
#if defined( _WIN32 )
/* FIXME is this still used ? */
typedef unsigned long u_long;
typedef unsigned long ulong;
#endif
#include "info_window.h"
#include "activity_bar.h"
/* #include "eb_aim.h" */
#include "gtk/gtk_eb_html.h"
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

unsigned int module_version() {return 16;}

/* Function Prototypes */
int plugin_init();
int plugin_finish();

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"AIM Oscar",
	"Provides AOL Instant Messenger support via the Oscar protocol",
	"$Revision: 1.12 $",
	"$Date: 2003/10/02 12:19:31 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "AIM-oscar", -1, 0, NULL };
/* End Module Exports */

static char *ay_aim_get_color(void) { static char color[]="#000088"; return color; }

int plugin_init()
{
	eb_debug(DBG_MOD, "plugin_init() : aim-oscar\n");
	ref_count=0;
	return(0);
}

int plugin_finish()
{
	eb_debug(DBG_MOD, "Returning the ref_count: %i\n", ref_count);
	return(ref_count);
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
};

struct eb_aim_local_account_data
{
	char password[255];
	gint status;

	LList *buddies;
	
	int fd;
	aim_conn_t *conn;
	aim_session_t aimsess;
	
	int input;
	int progress_bar;
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

static fu8_t features_aim[] = {0x01, 0x01, 0x01, 0x02};

static char * profile = "Visit the Ayttm website at <A HREF=\"http://ayttm.sf.net\">http://http://ayttm.sf.net/</A>.";

/* misc */
static char *aim_normalize (char *s);
static void connect_error (struct eb_aim_local_account_data *alad, char *msg); /* FIXME find a better name */

/* ayttm callbacks forward declaration */
static void ay_aim_login  (eb_local_account *account);
static void ay_aim_logout (eb_local_account *account);

static void ay_aim_add_user (eb_account *account);

static void ay_aim_callback (void *data, int source, eb_input_condition condition);

/* faim callbacks forward declaration */
static int faim_cb_parse_login        (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_authresp     (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_conninitdone_bos   (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_motd         (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_selfinfo           (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_ssi_parserights    (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_ssi_parselist      (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_locaterights (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_buddyrights  (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_serverready        (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_rateresp_bos       (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_icbmparaminfo      (aim_session_t *sess, aim_frame_t *fr, ...); /* fixme */

static int faim_cb_memrequest         (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_oncoming     (aim_session_t *sess, aim_frame_t *fr, ...);
static int faim_cb_parse_offgoing     (aim_session_t *sess, aim_frame_t *fr, ...);

static int faim_cb_parse_incoming_im  (aim_session_t *sess, aim_frame_t *fr, ...);

/*the callback to call all callbacks :P */

static void
ay_aim_callback(void *data, int source, eb_input_condition condition )
{
  eb_local_account *ela = (eb_local_account *)data;
  struct eb_aim_local_account_data * alad =
    (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
  aim_conn_t *conn = NULL;

#ifdef DEBUG
  g_message("ay_aim_callback, source=%d", source);
#endif
  
  g_assert (!(source < 0));

  if (alad->conn->fd == source)
    conn = alad->conn;

  if (conn == NULL)
    {
      // connection not found
      g_warning("connection not found");      
    }
  /*
  else if (condition & EB_INPUT_WRITE)
    {
      g_message ("It seems we can write !");
      aim_tx_flushqueue(&(alad->aimsess));
    }
  */
  else if (aim_get_command(&(alad->aimsess), conn) < 0)
    {
      if (conn->type == AIM_CONN_TYPE_BOS)
	{
	  g_warning("CONNECTION ERROR!!!!!! attempting to reconnect");
	  g_assert(ela);
	  ay_aim_logout(ela);
	  ay_aim_login(ela);
	}
#if 0
      else if (conn->type == AIM_CONN_TYPE_CHATNAV)
	{
	  g_warning("CONNECTION ERROR! (ChatNav)");
	  eb_input_remove(alad->chatnav_input);
	  aim_conn_kill(&(alad->aimsess), &conn);
	  alad->chatnav_conn = NULL;
	}
      else if (conn->type == AIM_CONN_TYPE_CHAT)
	{
	  g_warning("CONNECTION ERROR! (Chat)");
	  eb_input_remove(alad->chatnav_input);
	  aim_conn_kill(&(alad->aimsess), &conn);
	  alad->chat_conn = NULL;
	  alad->chat_room = NULL;
	}
#endif
      g_warning ("CONNECTION ERROR !!");
    }
  else
    {
      g_message ("Calling  aim_rxdispatch ()\n");
      aim_rxdispatch(&(alad->aimsess));
    }
}

int
faim_cb_parse_login (aim_session_t *sess,
		     aim_frame_t   *fr, ...)
{
	/* FIXME: What about telling AIM that we are Ayttm ? */
	struct client_info_s info= CLIENTINFO_AIM_KNOWNGOOD;
	eb_local_account *account = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data *alad;
	char *key;
	va_list ap;
	
	g_message("faim_cb_parse_login ()\n");
	
	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);
	
	alad = (struct eb_aim_local_account_data *) account->protocol_local_account_data;
	
	
	ay_progress_bar_update_progress (alad->progress_bar, 1);
	
	g_message ("Login=%s | Password=%s\n",
		   account->handle,
		   alad->password);

	aim_send_login (sess, fr->conn, account->handle,
			alad->password, &info, key);
	return 1;
}

int
faim_cb_parse_authresp (aim_session_t *sess,
			aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	va_list ap;
	struct aim_authresp_info *info;
	
	va_start (ap, fr);
	info = va_arg (ap, struct aim_authresp_info *);
	va_end (ap);
	
	g_message("faim_cb_parse_authresp () : Screen name : %s\n", info->sn);
	
	/* Check for errors */
	if (info->errorcode || !info->bosip || !info->cookie)
	{
		/* FIXME */
		connect_error (alad, "FIXME");
		g_warning("Login Error Code 0x%04x", info->errorcode);
		g_warning("Error URL: %s", info->errorurl);
		ay_aim_logout(ela); /* FIXME */
		return 1;
	}
	
	g_message("Closing auth connection...\n");
	
	g_message("REMOVING TAG = %d\n", alad->input);
	eb_input_remove(alad->input);
	aim_conn_kill(sess, &(alad->conn));
	
	ay_progress_bar_update_progress (alad->progress_bar, 2);
	
	alad->conn = aim_newconn(sess, AIM_CONN_TYPE_BOS, info->bosip);
	if (!alad->conn)
	{
		connect_error (alad, "connection to BOS failed: internal error\n");
		g_warning ("Connection to BOS failed: internal error\n");
		return 1;
	}
	if (alad->conn->status & AIM_CONN_STATUS_CONNERR)
	{
		connect_error (alad, "connection to BOS failed\n");
		g_warning ("Connection to BOS failed\n");
		return 1;
	}
	
	
	g_message("new connection fd=%d", alad->conn->fd );

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0 );
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, faim_cb_parse_motd, 0);

	// aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, faim_cb_connerr, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, faim_cb_conninitdone_bos, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_SELFINFO, faim_cb_selfinfo, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_RIGHTSINFO, faim_cb_ssi_parserights, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_LIST, faim_cb_ssi_parselist, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_SSI, AIM_CB_SSI_NOLIST, faim_cb_ssi_parselist, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_RIGHTSINFO, faim_cb_parse_locaterights, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, faim_cb_parse_buddyrights, 0);

	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, faim_cb_serverready, 0 );
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, faim_cb_rateresp_bos, 0 );
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_PARAMINFO, faim_cb_icbmparaminfo, 0 );
	
	/* FIXME : fix libfaim and use meaningful names:) */
	aim_conn_addhandler(sess, alad->conn, 0x0001, 0x001f, faim_cb_memrequest, 0);
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, faim_cb_parse_oncoming, 0);
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, faim_cb_parse_offgoing, 0); 
	
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, faim_cb_parse_incoming_im, 0 );
#if 0
	aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, eb_aim_handleredirect, 0 );
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
faim_cb_conninitdone_bos (aim_session_t *sess, aim_frame_t *fr, ...)
{
	g_message ("faim_cb_conninitdone_bos ()\n");

	aim_reqpersonalinfo (sess, fr->conn); /* -> faim_cb_selfinfo () */

	aim_ssi_reqrights (sess); /* -> faim_cb_ssi_parserights () */
	aim_ssi_reqdata (sess); /* -> faim_cb_ssi_parselist () */

	aim_locate_reqrights (sess); /* -> faim_cb_parse_locaterights () */
	aim_bos_reqbuddyrights(sess, fr->conn); /* -> faim_cb_parse_buddyrights () */
	aim_im_reqparams(sess); /* -> faim_cb_icbmparaminfo () */

	// aim_bos_reqrights(sess, fr->conn);
	
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
	
	g_message ("MOTD: %s (%hu)\n", msg ? msg : "Unknown", id);
	if (id < 4)
		g_message ("Your AIM connection may be lost.");
	
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

	g_message ("warn level=%d\n", info->warnlevel);

	return 1;
}

static int
faim_cb_ssi_parserights (aim_session_t *sess, aim_frame_t *fr, ...)
{
	int numtypes, i;
	fu16_t *maxitems;
	va_list ap;

	va_start(ap, fr);
	numtypes = va_arg(ap, int);
	maxitems = va_arg(ap, fu16_t *);
	va_end(ap);

	if (numtypes >= 0)
		g_message ("maxbuddies=%d\n", maxitems[0]);
	if (numtypes >= 1)
		g_message ("maxgroups=%d\n", maxitems[1]);
	if (numtypes >= 2)
		g_message ("maxpermits=%d\n", maxitems[2]);
	if (numtypes >= 3)
		g_message ("maxdenies=%d\n", maxitems[3]);

	return 1;
}

static int
faim_cb_ssi_parselist (aim_session_t *sess, aim_frame_t *fr, ...)
{
	/* TODO */
	aim_ssi_enable(sess);
}

static int
faim_cb_parse_locaterights (aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t maxsiglen;
	
	va_start(ap, fr);
	maxsiglen = (fu16_t) va_arg(ap, int);
	va_end(ap);
	
	g_message ("max away msg / signature len=%d\n", maxsiglen);

	aim_locate_setprofile(sess,
			      "us-ascii", profile, strlen(profile), /* Profile */
			      NULL, NULL, 0, /* Away message */
			      0); /* What we can do */

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
	
	g_message ("max buddies = %d, max watchers = %d\n", maxbuddies, maxwatchers);
	
	return 1;
}

static int
faim_cb_serverready (aim_session_t *sess, aim_frame_t *fr, ...)
{
	int famcount;
	fu16_t *families;
	va_list ap;

	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	va_start(ap, fr);
	famcount = va_arg(ap, int);
	families = va_arg(ap, fu16_t *);
	va_end(ap);
	
	switch (fr->conn->type)
	{
	case AIM_CONN_TYPE_BOS:
		ay_progress_bar_update_progress (alad->progress_bar, 3);

		aim_auth_setversions(sess, fr->conn);
		aim_bos_reqrate(sess, fr->conn);
		g_message ("done with auth server ready\n");
		break;
		
		
		/* FIXME add group chat :) */
	default:
		g_warning ("unknown connection type on Server Ready");
	}
	
	return 1;
}

/*
 * This helper function builds the buddy list to send to the
 * server. Buddies are separated with "$".
 * Don't forget to g_free the returned value after use !
 */
static gchar *
build_buddy_list (LList *h)
{
	gchar *result = "PJMirror$";
	LList *n;
	
	for (n = h; n; n = n->next) {
		gchar *tmp;
		tmp = result;
		result = g_strconcat (tmp, (gchar *)n->data, "$", NULL);
		if (n != h)
			g_free (tmp);
	}
	
	return result;
}



static int
faim_cb_rateresp_bos (aim_session_t *sess, aim_frame_t *fr, ...)
{
	gchar *buddy_list = NULL;
	
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	
	ay_progress_bar_update_progress (alad->progress_bar, 4);
	
	aim_bos_ackrateresp(sess, fr->conn);
	aim_bos_setprofile(sess, fr->conn,
			   profile,
			   NULL, /* Away message */
			   AIM_CAPS_SENDBUDDYLIST); /* What we can do */
	
	/* Send the buddy list */
	buddy_list = build_buddy_list (alad->buddies);
	g_message ("=====> Sending buddy list : %s\n", buddy_list);
	aim_bos_setbuddylist (sess, fr->conn, buddy_list);
	//g_free (buddy_list);
	
	/* This will result in a call of faim_cb_icbmparaminfo() */
	aim_reqicbmparams(sess, fr->conn);
	
	aim_bos_setprivacyflags(sess, fr->conn, AIM_PRIVFLAGS_ALLOWIDLE);
	
	return 1;
}

static int
faim_cb_icbmparaminfo (aim_session_t *sess, aim_frame_t *fr, ...)
{
	/* FIXME : these are faimtest values */
	
	struct aim_icbmparameters *params;
	va_list ap;
	
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);
	
	params->flags = 0x0000000b;
	params->maxmsglen = 8000;
	params->minmsginterval = 0; /* in milliseconds */
	
	aim_im_setparams (sess, params);

	aim_clientready(sess, fr->conn);
	aim_srv_setavailmsg(sess, NULL);
	aim_bos_setidle(sess, fr->conn, 0);
	
	g_message("You are now officially online.\n");
	
  
	{
		if(ela->status_menu) {
			/* Make sure set_current_state doesn't call us back */
			ela->connected=-1;
			eb_set_active_menu_status(ela->status_menu, AIM_ONLINE);
		}
		ela->connected = 1;
		ela->connecting = 0;
	}

	ay_activity_bar_remove (alad->progress_bar);
	alad->progress_bar = -1;

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

	g_message ("offset: %u, len: %u, file: %s\n",
		   offset, len, (modname ? modname : "aim.exe"));
	if (len == 0) {
		aim_sendmemblock(sess, fr->conn, offset, len, NULL, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		return 1;
	}

	g_warning ("You may be disconnected soon !");
	
	return 1;
}

static int
faim_cb_parse_oncoming (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_account * user = NULL;
	aim_userinfo_t *userinfo;
	va_list ap;

	va_start (ap, fr);
	userinfo = va_arg (ap, aim_userinfo_t *);
	va_end (ap);
	
	user = find_account_by_handle(userinfo->sn, SERVICE_INFO.protocol_id);
	if(!user)
	{
 		char tempname[512];
		// FIXME : uh ?
 		strcpy(tempname,aim_normalize(userinfo->sn));
	        user = find_account_by_handle(tempname, SERVICE_INFO.protocol_id);
	}
	if(user)
	{
		
		struct eb_aim_account_data * aad = user->protocol_account_data;
		
		if (strcmp (user->handle, userinfo->sn)) {
			g_warning("Updating contact list from %s to %s", user->handle, userinfo->sn);
			strcpy (user->handle, userinfo->sn);
			write_contact_list();
		}
		
		if(userinfo->flags & AIM_FLAG_AWAY )
		{
			aad->status = AIM_AWAY;
		}
		else
		{
		 	aad->status = AIM_ONLINE;
		}
		buddy_login(user);
/* wtf ????*/
#if 0
		for(i = 0; i < 45; i++ )
#endif
		{
		 	aad->idle_time = userinfo->idletime;
			aad->evil = userinfo->warnlevel;
	 		buddy_update_status(user);
		}
		
	}
	else
	{
		g_warning("Unable to find user %s", userinfo->sn);
	}
	
	return 1;
}

static int
faim_cb_parse_offgoing (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_account * user = NULL;	
	aim_userinfo_t *userinfo;
        va_list ap;
	
        va_start(ap, fr);
        userinfo = va_arg(ap, aim_userinfo_t *);
        va_end(ap);
	
	user = find_account_by_handle (userinfo->sn, SERVICE_INFO.protocol_id);
	if (user)
	{
		struct eb_aim_account_data *aad = user->protocol_account_data;
		aad->status = AIM_OFFLINE;
	}
	if(!user)
	{
		user = find_account_by_handle(aim_normalize(userinfo->sn), SERVICE_INFO.protocol_id);
	}
	else
	{
		g_warning("Unable to find user %s", userinfo->sn);
	}
#ifdef DEBUG
	g_message("eb_aim_parse_offgoing");
#endif
	buddy_logoff(user);
	return 1;
}

static int
faim_cb_parse_incoming_im (aim_session_t *sess, aim_frame_t *fr, ...)
{
	eb_local_account *ela = (eb_local_account *)sess->aux_data;
	struct eb_aim_local_account_data * alad =
		(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

	va_list ap;
	fu16_t channel;
	aim_userinfo_t *userinfo;
	char *msg = NULL;
	
	eb_account *sender = NULL;
	eb_local_account *reciever = NULL;	

#ifdef DEBUG
	g_message ("faim_cb_parse_incoming_im");
#endif
	
	va_start (ap, fr);
	channel = (fu16_t)va_arg (ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	
	if (channel == 1) {
		struct aim_incomingim_ch1_args *args;
                args = va_arg(ap, struct aim_incomingim_ch1_args *);
		va_end(ap);

		g_message ("Message from = %s\n", userinfo->sn);
		g_message ("Message = %s\n", args->msg);

		sender = find_account_by_handle(userinfo->sn, SERVICE_INFO.protocol_id);
		if(sender==NULL)
		{
			eb_account * ea = g_new0(eb_account, 1);
			struct eb_aim_account_data * aad = g_new0(struct eb_aim_account_data, 1);
			strcpy(ea->handle, userinfo->sn );
			ea->service_id = SERVICE_INFO.protocol_id;
			aad->status = AIM_OFFLINE;
			ea->protocol_account_data = aad;
			
			add_unknown(ea);
			//aim_add_buddy(command->conn,screenname);
			sender = ea;
			eb_aim_add_user(ea);
			
#ifdef DEBUG
			g_warning("Sender == NULL");
#endif
		}
		reciever = find_suitable_local_account( ela, SERVICE_INFO.protocol_id );
		//strip_html(msg);
		
		eb_parse_incoming_message(reciever, sender, args->msg);
		if(reciever == NULL)
		{
			g_warning("Reviever == NULL");
		}
		
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
			
			strcpy(ecr->id, roominfo->name);
			strcpy(ecr->room_name, roominfo->name);
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
	
	return 1;
}

#if 0

int eb_aim_handleredirect(struct aim_session_t *sess, struct command_rx_struct *command, ...)
{
  va_list ap;
  int serviceid;
  char *ip;
  char *cookie;
  /* this is the new buddy list */
  char * buddies = NULL;
  /* this is the new profile */
  char buff[1024];
  FILE *fp;  
  int i = 0;
  LList * node;
  eb_local_account * account = aim_find_local_account_by_conn(command->conn ); 
  struct eb_aim_local_account_data *alad =
    account->protocol_local_account_data;


#ifdef DEBUG
  g_message("eb_aim_handleredirect");
#endif


  for( node = aim_buddies; node && i < 50; node=node->next, i++ )
  {
    char *handle = node->data;
    if (buddies)
    {
      char * buddies_old = buddies;
      buddies = g_strconcat(buddies, aim_normalize(handle), "&", NULL);
      free(buddies_old);
    }
    else
    {
      buddies = g_strconcat(aim_normalize(handle), "&", NULL);
    }
  }
  node=aim_buddies;

#ifdef DEBUG
  g_message("%s\n", buddies);
#endif
  

  va_start(ap, command);
  serviceid = va_arg(ap, int);
  ip = va_arg(ap, char *);
  cookie = va_arg(ap, char *);
  va_end(ap);
  switch(serviceid)
  {
      case 0x0005: /* Advertisements */
        /* send the buddy list and profile (required, even if empty) */
		buddies = g_strconcat(aim_normalize(sess->logininfo.screen_name), "&", NULL ); 
        //aim_bos_setbuddylist(sess, command->conn, "");
        aim_bos_setprofile(sess, command->conn, profile, NULL, AIM_CAPS_CHAT);
		aim_seticbmparam(sess, command->conn);
		aim_conn_setlatency(command->conn, 1);

        /* send final login command (required) */
        aim_bos_clientready(sess, command->conn); /* tell BOS we're ready to go live */
		

		eb_input_remove(alad->input);
		while( node )
		{
			int j =0;
  			for(i=0 ; node && i < 50 ; node=node->next, i++ )
			{
				signal(SIGPIPE, SIG_IGN);
				aim_add_buddy(sess, command->conn, aim_normalize((char*)(node->data)));
		 		while (gtk_events_pending())
					gtk_main_iteration();
				usleep(10000);
				//fprintf(stderr, "Ignoring %s\n", node->data);	

			}
			if(node)
			{
			
				for( j = 0; j < 50; j++ )
				{
		 			while (gtk_events_pending())
						gtk_main_iteration();
					usleep(10000);
				}
			}

			//sleep(5);
		}
  		alad->input = eb_input_add(alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION , eb_aim_callback, account);


        /* you should now be ready to go */
#ifdef DEBUG
                                                                                                                        		g_message("You are now officially online. (%s)", ip);
#endif

        break;
      case 0x0007: /* Authorizer */
      {
         struct aim_conn_t *tstconn;
         /* Open a connection to the Auth */

         tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, ip);
         if ( (tstconn==NULL) || (tstconn->status >= AIM_CONN_STATUS_RESOLVERR) )
             g_warning("faimtest: unable to reconnect with authorizer");
         else
	 {
	   /* TODO */
	   eb_input_add(tstconn->fd,
			 EB_INPUT_READ | EB_INPUT_EXCEPTION,
			 eb_aim_callback, tstconn);

	   /* Send the cookie to the Auth */
	   aim_auth_sendcookie(sess, tstconn, cookie);
	 }
      }
      break;
      case 0x000d: /* ChatNav */
      {
          struct aim_conn_t *tstconn = NULL;
#ifdef DEBUG
	  g_message("eb_aim_handleredirect (ChatNav)");
#endif

          tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, ip);
          if ( (tstconn==NULL) || (tstconn->status >= AIM_CONN_STATUS_RESOLVERR))
	    g_warning("faimtest: unable to connect to chatnav server");
          else
	  {
#ifdef DEBUG
	    g_message("eb_aim_handleredirect (ChatNav)");
#endif

	    aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_GEN,
				AIM_CB_GEN_SERVERREADY,
				eb_aim_serverready, 0);
	    aim_auth_sendcookie(sess, tstconn, cookie);

	    alad->chatnav_conn = tstconn;
	    alad->chatnav_input =
	      eb_input_add(tstconn->fd,
			    EB_INPUT_READ|EB_INPUT_EXCEPTION,
			    eb_aim_callback, account);
	  }
      }
      break;
      case 0x000e: /* Chat */
      {
	char *roomname = NULL;
	struct aim_conn_t *tstconn = NULL;

#ifdef DEBUG
	g_message("eb_aim_handleredirect (Chat)");
#endif

	roomname = va_arg(ap, char *);

	if (alad->chat_conn == NULL)
	{
	  tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, ip);
	  if ((tstconn==NULL) ||
	      (tstconn->status >= AIM_CONN_STATUS_RESOLVERR))
          {
            g_warning("faimtest: unable to connect to chat server");
            if (tstconn) aim_conn_kill(sess, &tstconn);
	  }
	  else
	  {
	    aim_chat_attachname(tstconn, roomname);

	    aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_GEN,
				AIM_CB_GEN_SERVERREADY,
				eb_aim_serverready, 0);
	    aim_auth_sendcookie(sess, tstconn, cookie);

	    alad->chat_conn = tstconn;
	    strcpy(alad->chat_room->id, roomname);
	    alad->chat_input =
	      eb_input_add(tstconn->fd,
			    EB_INPUT_READ|EB_INPUT_EXCEPTION,
			    eb_aim_callback, account);

#ifdef DEBUG
	    g_message("new CHAT connection: %d", tstconn->fd);
#endif
	  }
	}
	else
	{
	  g_warning("Sorry, only one chat connection.");
	}
      }
      break;

   default:
    g_warning("uh oh... got redirect for unknown service 0x%04x!!", serviceid);
    /* dunno */
  }
  free(buddies);
  return 1;
}

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
	if (alad->progress_bar)
		ay_activity_bar_remove (alad->progress_bar);	
	
	ay_do_error ("AIM Error", msg);
}

static void
ay_aim_login (eb_local_account *account)
{
	struct eb_aim_local_account_data *alad;
	char buf[1024];

	if (account->connected || account->connecting) return;
	account->connecting = 1;

	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	ref_count++;
	fprintf (stderr, "ay_aim_login: Incrementing ref_count to %i\n", ref_count);
	
	snprintf (buf, sizeof(buf), "Logging in to AIM account: %s", account->handle);
	alad->progress_bar = ay_progress_bar_add (buf, 4, ay_aim_cancel_connect, account);

	aim_session_init(&alad->aimsess, 0, 1);
	alad->aimsess.aux_data = account;

	/* This is needed so we don't have to call aim_tx_flushqueue each  *
	 * time we want to send something !                                */
	aim_tx_setenqueue(&(alad->aimsess), AIM_TX_IMMEDIATE, NULL);
	
	alad->conn = aim_newconn(&(alad->aimsess), AIM_CONN_TYPE_AUTH, FAIM_LOGIN_SERVER);
	if(!alad->conn)
	{
		connect_error (alad, "Failed to connect to AIM server.");
		ref_count--;
		fprintf (stderr, "ay_aim_login: Decrementing ref_count to %i\n", ref_count);
		return;
	}
	if(alad->conn->fd == -1 )
	{
		ref_count--;
		fprintf (stderr, "ay_aim_login: Decrementing ref_count to %i\n", ref_count);
		if (alad->conn->status & AIM_CONN_STATUS_RESOLVERR)
		{
			connect_error (alad, "Could not resolve authorizer name.");
		}
		else if (alad->conn->status & AIM_CONN_STATUS_CONNERR)
		{
			connect_error (alad, "Could not connect to authorizer.");
		}
		else
		{
			connect_error (alad, "Unknown connection problem.");
		}
		return;
	}
		

	aim_conn_addhandler (&(alad->aimsess), alad->conn,
			     AIM_CB_FAM_ATH, AIM_CB_ATH_AUTHRESPONSE,
			     faim_cb_parse_login, 0);
	aim_conn_addhandler (&(alad->aimsess), alad->conn,
			     AIM_CB_FAM_ATH, AIM_CB_ATH_LOGINRESPONSE,
			     faim_cb_parse_authresp, 0);

	g_message ("Requesting connection with screename %s\n", account->handle);
	aim_request_login (&(alad->aimsess), alad->conn, account->handle);

	alad->input = eb_input_add (alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION ,
				    ay_aim_callback, account);
	g_message("ADDING TAG = %d\n", alad->input);
	
	//aim_tx_flushqueue(&(alad->aimsess));
	//alad->timer = eb_timeout_add(100, aim_poll_server, &(alad->aimsess));	
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
	char *buddy = (char *)b;
	
	user = find_account_by_handle(buddy, SERVICE_INFO.protocol_id);
	if (!user) {		
		g_warning ("Bug in make_buddy_offline for buddy %s\n", buddy);
		return;
	}

	aad = (struct eb_aim_account_data *)user->protocol_account_data;
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
	fprintf(stderr, "ay_aim_logout: Decrementing ref_count to %i\n", ref_count);
	
	/* mark buddies as Offline */
	l_list_foreach (alad->buddies, make_buddy_offline, NULL);
	
	account->connected = 0;
}

static LList *
ay_aim_write_local_config (eb_local_account *account)
{
	struct eb_aim_local_account_data * ala =
	  (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	LList * list = NULL;
	value_pair * vp;
	
	vp = g_new0( value_pair, 1 );
	strcpy( vp->key, "SCREEN_NAME" );
	strcpy( vp->value, account->handle );
	
	list = l_list_append( list, vp );

	vp = g_new0( value_pair, 1 );
	strcpy( vp->key, "PASSWORD" );
	strcpy( vp->value, ala->password );

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

static eb_local_account *
ay_aim_read_local_config (LList *pairs)
{
	
	eb_local_account * ela = g_new0(eb_local_account, 1);
	struct eb_aim_local_account_data * ala = g_new0(struct eb_aim_local_account_data, 1);
	char	*str = NULL;
	
	/*you know, eventually error handling should be put in here*/
	str = value_pair_get_value (pairs, "SCREEN_NAME");
	strncpy (ela->handle, str, 255);
	strncpy(ela->alias, ela->handle, 255);
	str = value_pair_get_value(pairs, "PASSWORD");
	strncpy(ala->password, str, 255);
	free( str );
	
	ela->service_id = SERVICE_INFO.protocol_id;
	ela->protocol_local_account_data = ala;
	ala->status = AIM_OFFLINE;
	
	return ela;
}

static eb_account *
ay_aim_read_config (eb_account *ea, LList *config)
{
	struct eb_aim_account_data *aad =  g_new0 (struct eb_aim_account_data, 1);
	
	aad->status = AIM_OFFLINE;
	ea->protocol_account_data = aad;
	
	ay_aim_add_user(ea);

	return ea;
}

static void
ay_aim_add_user (eb_account *account)
{
	eb_local_account *ela = NULL;
	struct eb_aim_local_account_data *alad = NULL;

	g_assert (eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id);

	ela = account->ela;
	g_assert (ela);

	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	g_assert (alad);

	if (!l_list_find (alad->buddies, account->handle))
		alad->buddies = l_list_append (alad->buddies, account->handle);

	if (alad->status != AIM_OFFLINE) {
		aim_add_buddy (&(alad->aimsess), alad->conn, account->handle);
	}
}

static eb_account *
ay_aim_new_account (eb_local_account * ela, const char *account)
{
	eb_account *a = g_new0 (eb_account, 1);
	struct eb_aim_account_data *aad = g_new0 (struct eb_aim_account_data, 1);
	
	g_message ("new account = %s\n", account);
	
	a->protocol_account_data = aad;
	strcpy (a->handle, account);
	a->service_id = SERVICE_INFO.protocol_id;
	aad->status = AIM_OFFLINE;
	a->ela = ela;
	return a;
}

static void
ay_aim_del_user (eb_account *account)
{
	eb_local_account *ela = NULL;
	struct eb_aim_local_account_data *alad = NULL;
	LList *node;
	
	ela = account->ela;
	if (!ela) {
		g_warning ("FIXME");
		return;
	}
	
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	
	if (!l_list_find (alad->buddies, account->handle)) {
		g_warning ("FIXME");
		return;
	}
	
	alad->buddies = l_list_remove (alad->buddies, account->handle);

	if (alad->status != AIM_OFFLINE) {
		aim_remove_buddy (&(alad->aimsess), alad->conn, account->handle);
	}

	g_free (account->protocol_account_data);
}

#if 0

void eb_aim_set_away(eb_local_account * account, gchar * message)
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	if (message) {
		if(account->status_menu)
		{
			eb_set_active_menu_status(account->status_menu, AIM_AWAY);

		}
        aim_bos_setprofile(&(alad->aimsess), alad->conn, profile, message, AIM_CAPS_CHAT);
	} else {
		if(account->status_menu)
		{
			eb_set_active_menu_status(account->status_menu, AIM_ONLINE);

		}
        aim_bos_setprofile(&(alad->aimsess), alad->conn, profile, NULL, AIM_CAPS_CHAT);
	}
}

#endif

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
	g_print ("ay_aim_get_states ()\n");
	
	LList * states = NULL;
	states = l_list_append(states, "Online");
	states = l_list_append(states, "Away");
	states = l_list_append(states, "Offline");
	
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
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );

	if(account == NULL || account->protocol_local_account_data == NULL )
	{
		g_warning("ACCOUNT state == NULL!!!!!!!!!!!!!!!!!!!!!");
	}

	if(alad->status == AIM_OFFLINE && state != AIM_OFFLINE )
	{
		ay_aim_login(account);
	}
	else if(alad->status != AIM_OFFLINE && state == AIM_OFFLINE )
	{
		ay_aim_logout(account);
	}
}

static char **
ay_aim_get_status_pixmap (eb_account * account)
{
	struct eb_aim_account_data *aad;
	
	aad = (struct eb_aim_account_data *)account->protocol_account_data;
	
	if (aad->status == AIM_AWAY)
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


#if 0

char * eb_aim_check_login(char * user, char * pass)
{
	return NULL;
}

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
	
	g_print("query_callbacks ()\n");

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

	sc->check_login = NULL; /* eb_aim_check_login; */

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
	sc->set_away = NULL; /* eb_aim_set_away; */

	/* Chat room */ /* Not yet implemented */

	sc->send_chat_room_message = NULL; /* eb_aim_send_chat_room_message; */
	sc->join_chat_room = NULL; /* eb_aim_join_chat_room; */
	sc->leave_chat_room = NULL; /* eb_aim_leave_chat_room; */
	sc->make_chat_room = NULL; /* eb_aim_make_chat_room; */

	sc->send_invite = NULL; /* eb_aim_send_invite; */
	sc->accept_invite = NULL; /* eb_aim_accept_invite; */
	sc->decline_invite = NULL; /* eb_aim_decline_invite; */

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


static char *
aim_normalize (char *s)
{
	static char buf[255];
        char *t, *u;
        int x=0;
	
        u = t = g_malloc(strlen(s) + 1);
	
        strcpy(t, s);
        g_strdown(t);
	
	while(*t) {
		if (*t != ' ') {
			buf[x] = *t;
			x++;
		}
		t++;
	}
        buf[x]='\0';
        g_free(u);
	return buf;
}

