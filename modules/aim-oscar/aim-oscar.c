/*
 * EveryBuddy 
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

#include <gtk/gtk.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#if defined( _WIN32 )
#include "../libfaim/aim.h"
typedef unsigned long u_long;
typedef unsigned long ulong;
#else
#include "libfaim/faim/aim.h"
#endif
#include "info_window.h"
#include "aim.h"
#include "gtk/gtk_eb_html.h"
#include "service.h"
#include "llist.h"
#include "chat_window.h"
#include "chat_room.h"
#include "util.h"
#include "status.h"
#include "globals.h"
#include "dialog.h"
#include "message_parse.h"
#include "value_pair.h"
#include "plugin_api.h"
#include "smileys.h"

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

/* Function Prototypes */
int plugin_init();
int plugin_finish();

static int ref_count = 0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_SERVICE,
	"AIM Oscar Service",
	"Aol Instant Messenger support via the Oscar protocol",
	"$Revision: 1.4 $",
	"$Date: 2003/04/04 09:15:40 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
struct service SERVICE_INFO = { "AIM", -1, FALSE, TRUE, FALSE, TRUE, NULL };
/* End Module Exports */

static char *eb_aim_get_color(void) { static char color[]="#000088"; return color; }

int plugin_init()
{
	eb_debug(DBG_MOD, "aim-oscar\n");
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

static LList * aim_buddies = NULL;
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
  int fd;
  struct aim_conn_t *conn;
  struct aim_conn_t *chatnav_conn;

  // we are currently limited to one chat room/chat connection
  struct aim_conn_t *chat_conn;
  eb_chat_room *chat_room;

  gchar *create_chat_room_id;
  gint create_chat_room_exchange;

  struct aim_session_t aimsess;

  int input;
  int chatnav_input;
  int chat_input;

  int timer;
  gint status;
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

static char * profile = "Visit the Everybuddy website at <A HREF=\"http://www.everybuddy.com\">http://www.everybuddy.com</A>.";


void eb_aim_add_user( eb_account * account );
void eb_aim_login( eb_local_account * account );
void eb_aim_logout( eb_local_account * account );
void aim_info_update(info_window *iw); 
void aim_info_data_cleanup(info_window *iw);


int eb_aim_serverready(struct aim_session_t *sess,
		       struct command_rx_struct *command, ...);

int eb_aim_chatnav_info(struct aim_session_t *sess,
			struct command_rx_struct *command, ...);

int eb_aim_chat_join(struct aim_session_t *sess,
		     struct command_rx_struct *command, ...);

int eb_aim_chat_leave(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...);

int eb_aim_chat_infoupdate(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...);

int eb_aim_chat_incomingmsg(struct aim_session_t *sess,
			    struct command_rx_struct *command, ...);


eb_local_account *aim_find_local_account_by_conn(struct aim_conn_t * conn)
{
  LList * node;

  for (node = accounts; node; node = node->next)
  {
    eb_local_account * ela = (eb_local_account *)node->data;
    if (ela->service_id == SERVICE_INFO.protocol_id)
    {
      struct eb_aim_local_account_data * alad =
	(struct eb_aim_local_account_data *)ela->protocol_local_account_data;

      if ((conn->type == AIM_CONN_TYPE_CHATNAV) &&
	  (alad->chatnav_conn == conn))
      {
	return ela;
      }
      else if ((conn->type == AIM_CONN_TYPE_CHAT) &&
	  (alad->chat_conn == conn))
      {
	return ela;
      }
      else if (alad->conn == conn)
      {
	return ela;
      }
    }
  }
  return NULL;
}
			


char *aim_normalize(char *s)
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


/*the callback to call all callbacks :P */


void eb_aim_callback(void *data, int source, eb_input_condition condition )
{
  eb_local_account * ela = data;
  struct eb_aim_local_account_data * alad =
    (struct eb_aim_local_account_data *)ela->protocol_local_account_data;

#ifdef DEBUG
  g_message("eb_aim_callback, source=%d", source);
#endif

  if(source < 0 )
  {
    g_assert(0);
  }
  else
  {
    struct aim_conn_t *conn = NULL;

    if (alad->conn->fd == source)
      conn = alad->conn;
    else if (alad->chatnav_conn && (alad->chatnav_conn->fd == source))
      conn = alad->chatnav_conn;
    else if (alad->chat_conn && (alad->chat_conn->fd == source))
      conn = alad->chat_conn;

    if (conn == NULL)
    {
      // connection not found
      g_warning("connection not found");
    }
    else if(aim_get_command(&(alad->aimsess), conn) < 0 )
    {
      if (conn->type == AIM_CONN_TYPE_BOS)
      {
	g_warning("CONNECTION ERROR!!!!!! attempting to reconnect");
	if(!ela)
	  g_assert(0);
	eb_aim_logout(ela);
	eb_aim_login(ela);
      }
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
    }
    else
    {
      aim_rxdispatch(&(alad->aimsess));
    }
  }
}
/* callbacks needed by libfaim ******************************************/
int eb_aim_parse_oncoming(struct aim_session_t * sess,struct command_rx_struct *command, ...)
{
	 eb_account * user = NULL;
	 struct aim_userinfo_s *info;
	 va_list ap;
	 va_start(ap, command);
	 info = va_arg(ap, struct aim_userinfo_s *);
	 va_end(ap);
	 
	 user = find_account_by_handle(info->sn, SERVICE_INFO.protocol_id);
	 if(!user)
	 {
 		char tempname[512];
 		strcpy(tempname,aim_normalize(info->sn));
	        user = find_account_by_handle(tempname, SERVICE_INFO.protocol_id);
	 }
	 if(user)
	 {

		 struct eb_aim_account_data * aad = user->protocol_account_data;
		 int i = 0;
                 if(strcmp(user->handle,info->sn)) {
#ifdef DEBUG
                      g_warning("Updating contact list from %s to %s",user->handle,info->sn);
#endif
                      strcpy(user->handle,info->sn);
                      write_contact_list();
		 }
 
		 if(info->flags & AIM_FLAG_AWAY )
		 {
			 aad->status = AIM_AWAY;
		 }
		 else
		 {
		 	aad->status = AIM_ONLINE;
		 }
	 	 buddy_login(user);
		 for(i = 0; i < 45; i++ )
		 {
			//aad->idle_time = *((int*)(&command->data[43]));
		 	aad->idle_time = info->idletime;
			aad->evil = info->warnlevel;
	 		buddy_update_status(user);
		 }

	 }
	 else
	 {
		 g_warning("Unable to find user %s", &command->data[11]);
	 }
#ifdef DEBUG
	 g_message("eb_aim_parse_oncoming %s", &command->data[11]);
#endif
	 return 1;
}

int eb_aim_parse_offgoing(struct aim_session_t * sess,struct command_rx_struct *command, ...)
{
	 eb_account * user = NULL;
	 user = find_account_by_handle(&command->data[11], SERVICE_INFO.protocol_id);
	 if(user)
	 {
		 struct eb_aim_account_data * aad = user->protocol_account_data;
		 aad->status = AIM_OFFLINE;
	 }
	 if(!user)
	 {
		  user = find_account_by_handle(aim_normalize(&command->data[11]), SERVICE_INFO.protocol_id);
	 }
	 else
	 {
		 g_warning("Unable to find user %s", &command->data[11]);
	 }
#ifdef DEBUG
	 g_message("eb_aim_parse_offgoing");
#endif
	 buddy_logoff(user);
	 return 1;
}

int eb_aim_auth_error(struct aim_session_t * sess,struct command_rx_struct * command, ...)
{
  va_list ap;
  struct login_phase1_struct *logininfo;
  char *errorurl;
  short errorcode;
  eb_local_account *account = aim_find_local_account_by_conn(command->conn ); 
  
  g_message("eb_aim_auth_error");

  va_start(ap, command);
  logininfo = va_arg(ap, struct login_phase1_struct *);
#ifdef DEBUG
  g_message("Screen name: %s", sess->logininfo.screen_name);
#endif
  errorurl = va_arg(ap, char *);
  g_warning("Error URL: %s", errorurl);
  errorcode = va_arg(ap, int);
  g_warning("Error code: 0x%02x", errorcode);
  va_end(ap);

  eb_aim_logout(account);
 // exit(0);
 
  return 0;
}

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

int eb_aim_serverready(struct aim_session_t *sess, struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;

#ifdef DEBUG
  g_message("eb_aim_serverready");
#endif


  switch (command->conn->type)
    {
    case AIM_CONN_TYPE_BOS:
      aim_bos_reqrate(sess, command->conn); /* request rate info */
      aim_bos_ackrateresp(sess, command->conn);  /* ack rate info response -- can we say timing? */
      aim_bos_setprivacyflags(sess, command->conn, 0x00000003);
    
#if 0
      aim_bos_reqpersonalinfo(command->conn);
#endif

	  aim_setversions(sess, command->conn);
      aim_bos_reqservice(sess, command->conn, AIM_CONN_TYPE_ADS); /* 0x05 == Advertisments */

#if 0
      aim_bos_reqrights(NULL);
      aim_bos_reqbuddyrights(NULL);
      aim_bos_reqlocaterights(NULL);
      aim_bos_reqicbmparaminfo(NULL);
#endif
    
      /* set group permissions */
      aim_bos_setgroupperm(sess, command->conn, 0x1f);
#ifdef DEBUG
      g_message("faimtest: done with BOS ServerReady");
#endif
      break;

    case AIM_CONN_TYPE_CHATNAV:
#ifdef DEBUG
      g_message("faimtest: chatnav: got server ready");
#endif
      aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CTN,
			  AIM_CB_CTN_INFO, eb_aim_chatnav_info, 0);
      aim_bos_reqrate(sess, command->conn); /* request rate info */
      aim_bos_ackrateresp(sess, command->conn);  /* ack rate info response -- can we say timing? */
      aim_chatnav_clientready(sess, command->conn);
      aim_chatnav_reqrights(sess, command->conn);

      break;

    case AIM_CONN_TYPE_CHAT:
#ifdef DEBUG
     g_message("faimtest: chat: got server ready");
#endif
      aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT,
			  AIM_CB_CHT_USERJOIN, eb_aim_chat_join, 0);
      aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT,
			  AIM_CB_CHT_USERLEAVE, eb_aim_chat_leave, 0);
      aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT,
			  AIM_CB_CHT_ROOMINFOUPDATE,
			  eb_aim_chat_infoupdate, 0);
      aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT,
			  AIM_CB_CHT_INCOMINGMSG,
			  eb_aim_chat_incomingmsg, 0);
      aim_bos_reqrate(sess, command->conn); /* request rate info */
      aim_bos_ackrateresp(sess, command->conn);  /* ack rate info response -- can we say timing? */
      aim_chat_clientready(sess, command->conn);
      eb_join_chat_room(alad->chat_room);

      break;

    default:
     g_warning("faimtest: unknown connection type on Server Ready");
    }
	aim_tx_flushqueue(sess);
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
  do_error_dialog("Last message could not be sent", "AIM: Error");

  return 1;
}

int eb_aim_info_responce(struct aim_session_t * sess,struct command_rx_struct *command, struct aim_userinfo_s * userinfo, char *text_encoding, char *text, char type)
{
  eb_account * sender = NULL;
  eb_local_account * reciever = NULL;
  eb_local_account * ela = aim_find_local_account_by_conn(command->conn);

#ifdef DEBUG
  g_message("eb_aim_info_response");
#endif

  sender = find_account_by_handle(userinfo->sn, SERVICE_INFO.protocol_id);
  if(sender==NULL)
  {
#ifdef DEBUG
    g_warning("Sender == NULL");
#endif
    return 1;
  }

    reciever = find_suitable_local_account( ela, SERVICE_INFO.protocol_id );

    if(sender->infowindow == NULL){
     sender->infowindow = eb_info_window_new(reciever, sender);
     gtk_widget_show(sender->infowindow->window);
    }

    if(sender->infowindow->info_type == -1 || sender->infowindow->info_data == NULL){
      if(sender->infowindow->info_data == NULL) {
        sender->infowindow->info_data = malloc(sizeof(aim_info_data));
        ((aim_info_data *)sender->infowindow->info_data)->away = NULL;
        ((aim_info_data *)sender->infowindow->info_data)->profile = NULL;
        sender->infowindow->cleanup = aim_info_data_cleanup;
      }
      sender->infowindow->info_type = SERVICE_INFO.protocol_id;
    }
    if(sender->infowindow->info_type != SERVICE_INFO.protocol_id) {
       /*hmm, I wonder what should really be done here*/
       return 1;
    }
    if(type == AIM_GETINFO_GENERALINFO) {
      if(((aim_info_data *)sender->infowindow->info_data)->profile != NULL) {
         free(((aim_info_data *)sender->infowindow->info_data)->profile);
         ((aim_info_data *)sender->infowindow->info_data)->profile = NULL;
      }
      if(text != NULL) {
         ((aim_info_data *)sender->infowindow->info_data)->profile = malloc(strlen(text)+1);
         strcpy( ((aim_info_data *)sender->infowindow->info_data)->profile, text);
      }
   } else if (type == AIM_GETINFO_AWAYMESSAGE) {
      if(((aim_info_data *)sender->infowindow->info_data)->away != NULL) {
         free(((aim_info_data *)sender->infowindow->info_data)->away);
         ((aim_info_data *)sender->infowindow->info_data)->away = NULL;
      }
      if(text != NULL) {
         ((aim_info_data *)sender->infowindow->info_data)->away = malloc(strlen(text)+1);
         strcpy( ((aim_info_data *)sender->infowindow->info_data)->away, text);
      }
    } else {
      g_warning("This is an invalid type %d",type);
    }
    aim_info_update(sender->infowindow);
    
   // eb_info_window_add_info(sender,text);

#ifdef DEBUG
     if(text != NULL)
        g_warning("Recived info respnse: %s",text);
     else g_warning("NULL info response recived");
#endif
   return 1;
}

int eb_aim_parse_incoming_im(struct aim_session_t *sess, struct command_rx_struct *command, ...)
{
  //time_t  t = 0;
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;
  va_list ap;

  eb_account * sender = NULL;
  eb_local_account * reciever = NULL;
  int channel;
  struct aim_userinfo_s *userinfo;
  char *msg = NULL;


#ifdef DEBUG
  g_message("eb_aim_parse_incoming_im");
#endif

  va_start(ap, command);
  channel = va_arg(ap, int);
  if (channel == 1)
  {
    u_int icbmflags = 0;
    char * tmpstr = NULL;
    u_short flag1, flag2;

    userinfo = va_arg(ap, struct aim_userinfo_s *);
    msg = va_arg(ap, char*);
    icbmflags = va_arg(ap, u_int);

    flag1 = va_arg(ap, u_int);
    flag2 = va_arg(ap, u_int);

    command->handled = 1;


    va_end(ap);

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

    eb_parse_incoming_message(reciever, sender, msg);
    if(reciever == NULL)
    {
      g_warning("Reviever == NULL");
    }

#ifdef DEBUG
    g_message("%s %s", userinfo->sn, msg);
#endif
  }
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


  return 1;
}


int eb_aim_parse_login(struct aim_session_t * sess,
		       struct command_rx_struct * command, ...)
{
  /*would anyboyd like to figure out the meanings of these fields?*/
  struct client_info_s info= {"EveryBuddy, rocking the world away!",
			      4, 30,3141, "us", "en"};
  eb_local_account *account;
  struct eb_aim_local_account_data *alad;
  char *key;
  va_list ap;

#ifdef DEBUG
  g_message("eb_aim_parse_login");
#endif

  va_start(ap, command);
  key = va_arg(ap, char *);
  va_end(ap);


  account = aim_find_local_account_by_conn(command->conn);
  alad = (struct eb_aim_local_account_data *) account->protocol_local_account_data;

  aim_send_login(sess, command->conn,
		 account->handle, alad->password, &info, key);
  return 1;
}


int eb_aim_auth_success(struct aim_session_t * sess, struct command_rx_struct * command, ...)
{
  va_list ap;
  struct login_phase1_struct *logininfo;
  eb_local_account * ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data * alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;

#ifdef DEBUG
  g_message("eb_aim_auth_success");
#endif

  

  if(sess->logininfo.errorcode)
  {
    g_warning("Login Error Code 0x%04x", sess->logininfo.errorcode);
    g_warning("Error URL: %s", sess->logininfo.errorurl);
    eb_aim_logout(ela);
    return 1;
  }

#ifdef DEBUG
  g_message("Closing auth connection...\n");
#endif

  eb_input_remove(alad->input);
 // eb_timeout_remove(alad->timer);
  aim_conn_kill(sess, &(alad->conn));
  do
  {
#ifdef DEBUG
    g_message("x");
#endif
    alad->conn = aim_newconn(sess, AIM_CONN_TYPE_BOS, sess->logininfo.BOSIP);
  }while(alad->conn->fd == -1 );

#ifdef DEBUG
  g_message("%d", alad->conn->fd );
#endif

  aim_auth_sendcookie(sess, alad->conn, sess->logininfo.cookie);
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, eb_aim_serverready, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, NULL, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, eb_aim_handleredirect, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_STS, AIM_CB_STS_SETREPORTINTERVAL, NULL, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, eb_aim_parse_incoming_im, 0 );
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, eb_aim_parse_oncoming, 0);
  aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, eb_aim_parse_offgoing, 0); 
   aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, eb_aim_msg_missed, 0);
   aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, eb_aim_msg_error, 0);
   aim_conn_addhandler(sess, alad->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, (rxcallback_t) eb_aim_info_responce, 0);
  alad->input = eb_input_add(alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION , eb_aim_callback, ela);
  //alad->timer = eb_timeout_add(100, aim_poll_server, sess);


	alad->status=AIM_ONLINE;
/*

	if(ela->status_button)
	{
		gtk_widget_destroy(ela->status_button);
		ela->status_button = MakeStatusButton(ela);
    	gtk_widget_show(ela->status_button);
    	gtk_container_add(GTK_CONTAINER(ela->status_frame),ela->status_button);
	}
*/
	serv_touch_idle();
  return 1;
}



//int eb_aim_parse_userinfo(struct command_rx_struct *command, ...);
//int eb_aim_pwdchngdone(struct command_rx_struct *command, ...);

/*   callbacks used by EveryBuddy    */
gboolean eb_aim_query_connected(eb_account * account)
{
	struct eb_aim_account_data * aad = account->protocol_account_data;
	return aad->status != AIM_OFFLINE;
}

eb_account * eb_aim_new_account( const char * account )
{
	eb_account * a = g_new0(eb_account, 1);
	struct eb_aim_account_data * aad = g_new0(struct eb_aim_account_data, 1);
	
	a->protocol_account_data = aad;
	strcpy(a->handle, account);
	a->service_id = SERVICE_INFO.protocol_id;
	aad->status = AIM_OFFLINE;
	
	return a;
}

void eb_aim_del_user( eb_account * account )
{
	LList * node;
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
	aim_buddies = l_list_remove(aim_buddies, account->handle );
	for( node = accounts; node; node=node->next )
	{
		eb_local_account * ela = node->data;
		if( ela->connected && ela->service_id == SERVICE_INFO.protocol_id )
		{
			struct eb_aim_local_account_data * alad = ela->protocol_local_account_data;
			aim_remove_buddy(&(alad->aimsess), alad->conn,account->handle);
		}
	}
}

void eb_aim_add_user( eb_account * account )
{
	LList * node;
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );

	if(!l_list_find(aim_buddies, account->handle))
		aim_buddies = l_list_append(aim_buddies, account->handle);
	for( node = accounts; node; node=node->next )
	{
		eb_local_account * ela = node->data;
		if( ela->connected && ela->service_id == SERVICE_INFO.protocol_id )
		{
			struct eb_aim_local_account_data * alad = ela->protocol_local_account_data;
			aim_add_buddy(&(alad->aimsess), alad->conn,account->handle);
		}
	}
}

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

void eb_aim_login( eb_local_account * account )
{
	struct eb_aim_local_account_data * alad;

	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;

	if (account->connected==-1) return;
	ref_count++;
	fprintf(stderr, "eb_aim_login: Incrementing ref_count to %i\n", ref_count);
	aim_session_init(&alad->aimsess, 0); // AIM_SESS_FLAGS_NONBLOCKCONNECT
	alad->aimsess.tx_enqueue = &aim_tx_enqueue__immediate;

	alad->conn = aim_newconn(&(alad->aimsess), AIM_CONN_TYPE_AUTH, FAIM_LOGIN_SERVER);
	if(!alad->conn)
	{
		g_warning("FAILED TO CONNECT TO AIM SERVER!!!!!!!!!!!!");
		ref_count--;
		fprintf(stderr, "eb_aim_login: Decrementing ref_count to %i\n", ref_count);
		return;
	}
	if(alad->conn->fd == -1 )
	{
		ref_count--;
		fprintf(stderr, "eb_aim_login: Decrementing ref_count to %i\n", ref_count);
		if(alad->conn->status & AIM_CONN_STATUS_RESOLVERR )
		{
			g_warning("COULD NOT RESOLVE AUTHORIZER NAMER");
		}
		else if(alad->conn->status & AIM_CONN_STATUS_CONNERR )
		{
			g_warning("COULD NOT CONNECT TO AUTHORIZOR!!!!");
		}
		else
		{
			g_warning("eb_aim UNKNOWN CONNECTION PROBLEM");
		}
		return;
	}
		

	aim_conn_addhandler(&(alad->aimsess), alad->conn,
			    AIM_CB_FAM_ATH, AIM_CB_ATH_AUTHRESPONSE,
			    eb_aim_parse_login, 0);
	aim_conn_addhandler(&(alad->aimsess), alad->conn,
			    AIM_CB_FAM_ATH, AIM_CB_ATH_LOGINRESPONSE,
			    eb_aim_auth_success, 0);

	//aim_sendconnack(&(alad->aimsess), alad->conn);
	aim_request_login(&(alad->aimsess), alad->conn, account->handle);

#if 0
	aim_send_login(&(alad->aimsess), alad->conn, /*aim_normalize*/(account->handle), alad->password, &info );
	aim_conn_addhandler(&(alad->aimsess), alad->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_AUTHSUCCESS, eb_aim_auth_success, 0 );
	aim_conn_addhandler(&(alad->aimsess), alad->conn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, eb_aim_serverready, 0 );
#endif

	account->connected = 1;
	printf("Connected %p!!!\n",account->status_menu);
	if(account->status_menu)
	{
		/* Make sure set_current_state doesn't call us back */
		account->connected=-1;
		eb_set_active_menu_status(account->status_menu, AIM_ONLINE);
	}
	alad->input = eb_input_add(alad->conn->fd, EB_INPUT_READ|EB_INPUT_EXCEPTION , eb_aim_callback, account);
//  	alad->timer = eb_timeout_add(100, aim_poll_server, &(alad->aimsess));
	

								  
}

void eb_aim_logout( eb_local_account * account )
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	eb_input_remove(alad->input);
	aim_conn_kill(&(alad->aimsess), &(alad->conn));
	alad->status=AIM_OFFLINE;
	ref_count--;
	fprintf(stderr, "eb_aim_logout: Decrementing ref_count to %i\n", ref_count);

	account->connected = 0;
}

void eb_aim_send_im( eb_local_account * account_from,
				  eb_account * account_to,
				  gchar * message )
{
  struct eb_aim_local_account_data *alad =
    account_from->protocol_local_account_data;


#ifdef DEBUG
  g_message("eb_aim_send_im %s", message);
#endif

  aim_send_im(&alad->aimsess, alad->conn, account_to->handle,
	      0, message);
  aim_tx_flushqueue(&alad->aimsess);
}

// request user profile and awaymessage
void eb_aim_get_info( eb_local_account * account_from, eb_account * account_to)
{
  struct eb_aim_local_account_data *alad =
    account_from->protocol_local_account_data;


#ifdef DEBUG
  g_message("eb_aim_get_info");
#endif

  aim_getinfo(&alad->aimsess, alad->conn, account_to->handle,
	      AIM_GETINFO_AWAYMESSAGE);
  aim_getinfo(&alad->aimsess, alad->conn, account_to->handle,
	      AIM_GETINFO_GENERALINFO);

  aim_tx_flushqueue(&alad->aimsess);
}


LList * eb_aim_write_local_config ( eb_local_account * account )
{
	struct eb_aim_local_account_data * ala = account->protocol_local_account_data;
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

eb_local_account * eb_aim_read_local_config(LList * pairs)
{

	eb_local_account * ela = g_new0(eb_local_account, 1);
	struct eb_aim_local_account_data * ala = g_new0(struct eb_aim_local_account_data, 1);
	char	*str = NULL;
	
    /*you know, eventually error handling should be put in here*/
    ela->handle=value_pair_get_value(pairs, "SCREEN_NAME");
	strncpy(ela->alias, ela->handle, 255);
	str = value_pair_get_value(pairs, "PASSWORD");
    strncpy(ala->password, str, 255);
	free( str );

    ela->service_id = SERVICE_INFO.protocol_id;
    ela->protocol_local_account_data = ala;
	ala->status = AIM_OFFLINE;

    return ela;
}


eb_account * eb_aim_read_config( LList * config, struct contact *contact )
{
    eb_account * ea = g_new0(eb_account, 1 );
    struct eb_aim_account_data * aad =  g_new0(struct eb_aim_account_data,1);
	char	*str = NULL;
	
	aad->status = AIM_OFFLINE;

    /*you know, eventually error handling should be put in here*/
	str = value_pair_get_value( config, "NAME");
    strncpy(ea->handle, str, 255);

    ea->service_id = SERVICE_INFO.protocol_id;
    ea->protocol_account_data = aad;
    ea->account_contact = contact;
	ea->list_item = NULL;
	ea->online = 0;
	ea->status = NULL;
	ea->pix = NULL;
	ea->icon_handler = -1;
	ea->status_handler = -1;
	
	eb_aim_add_user(ea);

    return ea;
}


LList * eb_aim_get_states()
{
	LList * states = NULL;
	states = l_list_append(states, "Online");
	states = l_list_append(states, "Away");
	states = l_list_append(states, "Offline");
	
	return states;
}

gint eb_aim_get_current_state(eb_local_account * account )
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
#ifdef DEBUG
	g_warning("FIXME: eb_aim_get_current_state STUB!\n");
#endif
	return alad->status;
}

void eb_aim_set_current_state( eb_local_account * account, gint state )
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)account->protocol_local_account_data;
	assert( eb_services[account->service_id].protocol_id == SERVICE_INFO.protocol_id );
#ifdef DEBUG
	g_warning("FIXME: eb_aim_set_current_state STUB!\n");
#endif
	if(account == NULL || account->protocol_local_account_data == NULL )
	{
		g_warning("ACCOUNT state == NULL!!!!!!!!!!!!!!!!!!!!!");
	}

	if(alad->status == AIM_OFFLINE && state != AIM_OFFLINE )
	{
		eb_aim_login(account);
	}
	else if(alad->status != AIM_OFFLINE && state == AIM_OFFLINE )
	{
		eb_aim_logout(account);
	}
}

char * eb_aim_check_login(char * user, char * pass)
{
	return NULL;
}

char **eb_aim_get_status_pixmap( eb_account * account)
{
	struct eb_aim_account_data * aad;
	
	aad = account->protocol_account_data;
	
	if (aad->status == AIM_AWAY)
		return aim_away_xpm;
	else
		return aim_online_xpm;
}
gchar * eb_aim_get_status_string( eb_account * account )
{
	static gchar string[255], buf[255];
	struct eb_aim_account_data * aad = account->protocol_account_data;
	strcpy(buf, "");
	strcpy(string, "");
	
	
	if(aad->idle_time)
	{
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

void eb_aim_set_idle( eb_local_account * ela, gint idle )
{
	struct eb_aim_local_account_data * alad;
	alad = (struct eb_aim_local_account_data *)ela->protocol_local_account_data;
	aim_bos_setidle( &(alad->aimsess), alad->conn, idle );
}

void aim_info_update(info_window *iw) {
     aim_info_data * aid = (aim_info_data *)iw->info_data;
     clear_info_window(iw);
     gtk_eb_html_add(EXT_GTK_TEXT(iw->info),"AIM Info:<BR>",1,1,0);
     if(aid->away != NULL) {
         gtk_eb_html_add(EXT_GTK_TEXT(iw->info),"<FONT color=red>",1,1,0);
         gtk_eb_html_add(EXT_GTK_TEXT(iw->info),aid->away,1,1,0);
         gtk_eb_html_add(EXT_GTK_TEXT(iw->info),"</FONT><hr>",1,1,0);
     }
     if(aid->profile != NULL) {
         gtk_eb_html_add(EXT_GTK_TEXT(iw->info),aid->profile,1,1,0);
     }
     gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(iw->scrollwindow)),0);
}

void aim_info_data_cleanup(info_window *iw){
     aim_info_data * aid = (aim_info_data *)iw->info_data;
     if(aid->away != NULL) free (aid->away);
     if(aid->profile != NULL) free(aid->profile);
}


int eb_aim_chatnav_info(struct aim_session_t *sess,
			struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;

  unsigned short type;
  va_list ap;


#ifdef DEBUG
  g_message("eb_aim_chatnav_info");
#endif

  va_start(ap, command);
  type = va_arg(ap, int);

  switch(type)
  {
   case 0x0002:
   {
     int maxrooms;
     struct aim_chat_exchangeinfo *exchanges;
     int exchangecount,i = 0;
    
     maxrooms = va_arg(ap, int);
     exchangecount = va_arg(ap, int);
     exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);
     va_end(ap);

#ifdef DEBUG
     g_message("faimtest: chat info: Chat Rights:");
     g_message("faimtest: chat info: \tMax Concurrent Rooms: %d", maxrooms);

     g_message("faimtest: chat info: \tExchange List: (%d total)",
	       exchangecount);

     while (i < exchangecount)
     {
       g_message("faimtest: chat info: \t\t%x: %s (%s/%s)", 
		 exchanges[i].number,
		 exchanges[i].name,
		 exchanges[i].charset1,
		 exchanges[i].lang1);
       i++;
     }
#endif

     if (alad->create_chat_room_id)
     {
       // process create_chat_room
       aim_chatnav_createroom(&alad->aimsess, alad->chatnav_conn,
			      alad->create_chat_room_id,
			      alad->create_chat_room_exchange);
       g_free(alad->create_chat_room_id);
       alad->create_chat_room_id = NULL;
     }
   }
   break;

   case 0x0008:
   {
    char *fqcn, *name, *ck;
    unsigned short instance, flags, maxmsglen, maxoccupancy, unknown;
    unsigned char createperms;
    unsigned long createtime;

    fqcn = va_arg(ap, char *);
    instance = va_arg(ap, int);
    flags = va_arg(ap, int);
    createtime = va_arg(ap, unsigned long);
    maxmsglen = va_arg(ap, int);
    maxoccupancy = va_arg(ap, int);
    createperms = va_arg(ap, int);
    unknown = va_arg(ap, int);
    name = va_arg(ap, char *);
    ck = va_arg(ap, char *);
    va_end(ap);

#ifdef DEBUG
    g_message("faimtest: received room create reply for %s", fqcn);
    g_message("ck: %s", ck);
#endif

    aim_chat_join(sess, alad->conn, alad->create_chat_room_exchange, ck);
   }
   break;

   default:
    va_end(ap);
    g_warning("faimtest: chatnav info: unknown type (%04x)", type);
  }

  return 1;
}

int eb_aim_chat_join(struct aim_session_t *sess,
		     struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;
  eb_chat_room *ecr = alad->chat_room;

  va_list ap;
  struct aim_userinfo_s *userinfo;
  int count = 0, i = 0;


#ifdef DEBUG
  g_message("eb_aim_chat_join");
#endif
  
  va_start(ap, command);
  count = va_arg(ap, int);
  userinfo = va_arg(ap, struct aim_userinfo_s *);
  va_end(ap);

#ifdef DEBUG
  g_message("faimtest: chat: %s:  New occupants have joined:", ecr->id);
#endif
  while (i < count)
  {
    eb_account *ea = find_account_by_handle(userinfo[i].sn, SERVICE_INFO.protocol_id);
#ifdef DEBUG
    g_message("faimtest: chat: %s: \t%s", ecr->id, userinfo[i].sn);
#endif

    if( ea)
    {
      eb_chat_room_buddy_arrive(ecr,
				ea->account_contact->nick, userinfo[i].sn);
    }
    else
    {
      eb_chat_room_buddy_arrive(ecr, userinfo[i].sn, userinfo[i].sn);
    }

    i++;
  }


  return 1;
}

int eb_aim_chat_leave(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;
  eb_chat_room *ecr = alad->chat_room;

  va_list ap;
  struct aim_userinfo_s *userinfo;
  int count = 0, i = 0;

  
#ifdef DEBUG
  g_message("eb_aim_chat_leave");
#endif

  va_start(ap, command);
  count = va_arg(ap, int);
  userinfo = va_arg(ap, struct aim_userinfo_s *);
  va_end(ap);

#ifdef DEBUG
  g_message("faimtest: chat: %s:  Some occupants have left:", ecr->id);
#endif

  while (i < count)
  {
#ifdef DEBUG
    g_message("faimtest: chat: %s: \t%s", ecr->id, userinfo[i].sn);
#endif

    eb_chat_room_buddy_leave(ecr, userinfo[i].sn);
    i++;
  }

  return 1;
}

int eb_aim_chat_infoupdate(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;
  eb_chat_room *ecr = alad->chat_room;

  va_list ap;
  struct aim_userinfo_s *userinfo;
  struct aim_chat_roominfo *roominfo;
  char *roomname;
  int usercount,i;
  char *roomdesc;
  unsigned short unknown_c9, unknown_d2, unknown_d5, maxmsglen;
  unsigned long creationtime;


#ifdef DEBUG
  g_message("eb_aim_chat_infoupdate");
#endif

  va_start(ap, command);
  roominfo = va_arg(ap, struct aim_chat_roominfo *);
  roomname = va_arg(ap, char *);
  usercount= va_arg(ap, int);
  userinfo = va_arg(ap, struct aim_userinfo_s *);
  roomdesc = va_arg(ap, char *);
  unknown_c9 = va_arg(ap, int);
  creationtime = va_arg(ap, unsigned long);
  maxmsglen = va_arg(ap, int);
  unknown_d2 = va_arg(ap, int);
  unknown_d5 = va_arg(ap, int);
  va_end(ap);

#ifdef DEBUG
  g_message("faimtest: chat: %s:  info update:", ecr->id);
  g_message("faimtest: chat: %s:  \tRoominfo: {%04x, %s, %04x}",
         ecr->id,
         roominfo->exchange,
         roominfo->name,
         roominfo->instance);
  g_message("faimtest: chat: %s:  \tRoomname: %s",
	 ecr->id, roomname);
  g_message("faimtest: chat: %s:  \tRoomdesc: %s",
	 ecr->id, roomdesc);
  g_message("faimtest: chat: %s:  \tOccupants: (%d)",
	 ecr->id, usercount);
  
  i = 0;
  while (i < usercount)
  {
    g_message("faimtest: chat: %s:  \t\t%s",
	      ecr->id, userinfo[i++].sn);
  }

  g_message("faimtest: chat: %s:  \tUnknown_c9: 0x%04x",
	    ecr->id, unknown_c9);
  g_message("faimtest: chat: %s:  \tCreation time: %lu (time_t)",
	    ecr->id, creationtime);
  g_message("faimtest: chat: %s:  \tMax message length: %d bytes",
	    ecr->id, maxmsglen);
  g_message("faimtest: chat: %s:  \tUnknown_d2: 0x%04x",
	    ecr->id, unknown_d2);
  g_message("faimtest: chat: %s:  \tUnknown_d5: 0x%02x",
	    ecr->id, unknown_d5);
#endif


  ecr->protocol_local_chat_room_data = (void *) ((int)roominfo->exchange);
  if (roomname)
  {
    strcpy(ecr->room_name, roomname);
  }
  else
  {
    strcpy(ecr->room_name, roomdesc);
  }


  return 1;

}

int eb_aim_chat_incomingmsg(struct aim_session_t *sess,
			    struct command_rx_struct *command, ...)
{
  eb_local_account *ela = aim_find_local_account_by_conn(command->conn);
  struct eb_aim_local_account_data *alad =
    (struct eb_aim_local_account_data *) ela->protocol_local_account_data;
  eb_chat_room *ecr = alad->chat_room;

  va_list ap;
  struct aim_userinfo_s *userinfo;
  char *msg;
  char tmpbuf[1152];
 

#ifdef DEBUG
  g_message("eb_aim_chat_incomingmsg");
#endif

  va_start(ap, command);
  userinfo = va_arg(ap, struct aim_userinfo_s *);       
  msg = va_arg(ap, char *);
  va_end(ap);

#ifdef DEBUG
  g_message("faimtest: chat: %s: incoming msg from %s: %s",
	    ecr->id, userinfo->sn, msg);
#endif


  {
    eb_account * ea = find_account_by_handle(userinfo->sn, SERVICE_INFO.protocol_id);
    gchar * message2 = linkify(msg);

    if( ea)
    {
      eb_chat_room_show_message(ecr, ea->account_contact->nick, message2);
    }
    else
    {
      eb_chat_room_show_message(ecr, userinfo->sn, message2);
    }
    g_free(message2);
  }


  return 1;
}


void eb_aim_send_invite(eb_local_account *account, eb_chat_room *room,
			char *user, char *message)
{
  struct eb_aim_local_account_data * alad =
    (struct eb_aim_local_account_data *) account->protocol_local_account_data;


#ifdef DEBUG
  g_message("send invite: \"%s\"", room->id);
#endif

  aim_chat_invite(&alad->aimsess, alad->conn, user, message,
		  (int) room->protocol_local_chat_room_data,
		  room->id, 0);
}

void eb_aim_accept_invite(eb_local_account *account, void *invitation)
{
  gchar *id = invitation;
  eb_chat_room *chat_room = find_chat_room_by_id(id);
  struct eb_aim_local_account_data *alad = 
    account->protocol_local_account_data;


#ifdef DEBUG
  g_message("accept_invite: %s", id);
#endif

  alad->create_chat_room_exchange =
    (gint) chat_room->protocol_local_chat_room_data;


  if (alad->chatnav_conn != NULL)
  {
    /* TODO */
    aim_chatnav_createroom(&alad->aimsess, alad->chatnav_conn, id,
			   (gint) chat_room->protocol_local_chat_room_data);
  }
  else
  {
    alad->create_chat_room_id = g_strdup(id);

    aim_bos_reqservice(&alad->aimsess, alad->conn, AIM_CONN_TYPE_CHATNAV);
  }

  free(id);
}

void eb_aim_decline_invite(eb_local_account *account, void *invitation)
{
  char *id = invitation;
  free(id);
}

void eb_aim_send_chat_room_message( eb_chat_room * room, gchar * message )
{
  struct eb_aim_local_account_data* alad =
    room->local_user->protocol_local_account_data;

  gchar *message2 = linkify(message);

  aim_chat_send_im(&alad->aimsess, alad->chat_conn, message2);
  g_free(message2);
}

void eb_aim_join_chat_room(eb_chat_room *room)
{
  struct eb_aim_local_account_data *alad =
    room->local_user->protocol_local_account_data;

#ifdef DEBUG
  g_message("eb_aim_join_chat_room: %s", room->id);
#endif

  aim_chat_join(&alad->aimsess, alad->conn, 0x0004, room->id);
}

void eb_aim_leave_chat_room( eb_chat_room * room )
{
  struct eb_aim_local_account_data *alad =
    room->local_user->protocol_local_account_data;

#ifdef DEBUG
  g_message("eb_aim_leave_chat_room: %s", room->id);
#endif

#ifdef DEBUG
  g_message("closing CHAT connection: %d", alad->chat_conn->fd);
#endif

  eb_input_remove(alad->chat_input);
  aim_conn_kill(&alad->aimsess, &alad->chat_conn);
  alad->chat_conn = NULL;
  alad->chat_room = NULL;
}

eb_chat_room *eb_aim_make_chat_room(gchar *name, eb_local_account *ela)
{
  struct eb_aim_local_account_data *alad = ela->protocol_local_account_data;
  eb_chat_room *ecr = g_new0(eb_chat_room, 1);

  if (name[0] == '+')
  {
    alad->create_chat_room_exchange = 0x5;
    strcpy(ecr->room_name, name + 1);
  }
  else
  {
    alad->create_chat_room_exchange = 0x4;
    strcpy(ecr->room_name, name);
  }

  ecr->fellows = NULL;
  ecr->connected = FALSE;
  ecr->local_user = ela;

  // TODO: HACK
  alad->chat_room = ecr;

  if (alad->chatnav_conn != NULL)
  {
    /* TODO */
    aim_chatnav_createroom(&alad->aimsess, alad->chatnav_conn, ecr->room_name,
			   alad->create_chat_room_exchange);
  }
  else
  {
    alad->create_chat_room_id = g_strdup(ecr->room_name);

    aim_bos_reqservice(&alad->aimsess, alad->conn, AIM_CONN_TYPE_CHATNAV);
  }


  return ecr;
}


struct service_callbacks * query_callbacks()
{
  struct service_callbacks * sc;
	
  /*this is an ugly hack, but we would like this command to be executed
    only once on initialization, and that would happen here :P */
  //aim_connrst(&);
	
  sc = g_new0( struct service_callbacks, 1 );
  sc->query_connected = eb_aim_query_connected;
  sc->login = eb_aim_login;
  sc->logout = eb_aim_logout;
  sc->send_im = eb_aim_send_im;
  sc->write_local_config = eb_aim_write_local_config;
  sc->read_local_account_config = eb_aim_read_local_config;
  sc->read_account_config = eb_aim_read_config;
  sc->get_states = eb_aim_get_states;
  sc->get_current_state = eb_aim_get_current_state;
  sc->set_current_state = eb_aim_set_current_state;
  sc->check_login = eb_aim_check_login;
  sc->add_user = eb_aim_add_user;
  sc->del_user = eb_aim_del_user;
  sc->new_account = eb_aim_new_account;
  sc->get_status_string = eb_aim_get_status_string;
  sc->set_idle = eb_aim_set_idle;
  sc->set_away = eb_aim_set_away;
  sc->get_status_pixmap = eb_aim_get_status_pixmap;
  sc->send_chat_room_message = eb_aim_send_chat_room_message;
  sc->join_chat_room = eb_aim_join_chat_room;
  sc->leave_chat_room = eb_aim_leave_chat_room;
  sc->make_chat_room = eb_aim_make_chat_room;
  sc->send_invite = eb_aim_send_invite;
  sc->accept_invite = eb_aim_accept_invite;
  sc->decline_invite = eb_aim_decline_invite;
  sc->get_info = eb_aim_get_info;
  sc->add_importers = NULL;
  sc->get_color=eb_aim_get_color;
  sc->get_smileys=eb_default_smileys;

  return sc;
}
