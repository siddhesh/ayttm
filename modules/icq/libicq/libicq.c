/* Copyright (C) 1998 Sean Gabriel <gabriel@korsoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <glib.h>

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include <windows.h>
#include <io.h>
#include <time.h>
#define ERRNO	WSAGetLastError()
#else /* !_WIN32 */
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef __MINGW32__
#include <sys/socket.h>
#endif
#include <sys/time.h>
#define ERRNO	errno
#endif /* _WIN32 */

#include "icq.h"
#include "libicq.h"
#include "send.h"
#include "receive.h"
#include "tcp.h"
#include "util.h"
#include "icqconfig.h"


void TCP_SendMessages(Contact_Member* c);
void UDP_SendMessages(Contact_Member* c);


int SRV_Addresses = 0;  /* The number of possible ICQ server IP addresses */
int SRV_AddressToUse = 0;  /* The index of the next address to try */
int Verbose = ICQ_VERB_NONE;
gboolean serv_mess[1024];
guint16 last_cmd[1024]; /* command issued for the first 1024 SEQ #'s */
guint16 seq_num = 1;  /* current sequence number */
guint32 our_ip = 0x0100007f; /* localhost for some reason */
guint32 our_port; /* the port to make tcp connections on */
Contact_Member Contacts[ ICQ_MAX_CONTACTS]; /* no more than 100 contacts max */
int Num_Contacts;
guint32 Current_Status=STATUS_ONLINE;
GList* Search_Results;
guint32 last_recv_uin=0;
guint32 UIN;
gchar nickname[64];
gchar passwd[100];
gchar server[100];
guint32 remote_port;
guint32 set_status;
gboolean Done_Login=FALSE;
gint sok;
gint tcp_sok;
GList* open_sockets;
/*win32--#warning "FIXME: add logfilename to config file"*/
gchar logfilename[]="debug.log";
gchar logpathfilename[255];

gint icq_logging = ICQ_LOG_NONE;

extern gchar contacts_rc[100];

gint ICQ_Read_Config()
{
/*
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> ICQ_Read_Config");
*/

  return Get_Config_Info();
}


gint ICQ_Connect()
{
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> ICQ_Connect");
  printf("\nLIBICQ> Connecting");

  Search_Results = NULL;
  if(Get_Config_Info() == 0) return 0;
  ICQ_Change_Status(Current_Status);

  printf("\nLIBICQ> Connected");
  return 1;
}


void ICQ_Disconnect()
{
  ICQ_Debug(ICQ_VERB_INFO,  "LIBICQ> ICQ_Disconnect");
  printf("\nLIBICQ> Disconnecting");

  Send_Disconnect();
  printf("\nLIBICQ> Disconnected");
}


void ICQ_Change_Status(guint32 new_status)
{
#if defined( _WIN32 ) && !defined(__MINGW32__)
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	static int startup = 0;
#endif

/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Change_Status(%d)", new_status);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  if(new_status == STATUS_OFFLINE && sok != 0)
  {
    ICQ_Disconnect();
    Current_Status = new_status;
    return;
  }

  if(new_status != STATUS_OFFLINE && sok == 0)
  {
#if defined( _WIN32 ) && !defined(__MINGW32__)
	if( startup == 0 ) {
		wVersionRequested = MAKEWORD( 2, 0 );

		err = WSAStartup( wVersionRequested, &wsaData );

		if( err != 0 ) {
			exit( 1 );
		}

		if( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 0 ) {
			WSACleanup();
			exit( 1 );
		}
		startup = 1;
	}
#endif

    Current_Status = new_status;
    if(Connect_Remote(server, remote_port) == 0)
    {
      ICQ_Debug(ICQ_VERB_ERR, " - Connect_Remote failed.");
    }
    else
    {
      Send_BeginLogin(UIN, &passwd[0], our_ip, our_port);
    }
    return;
  }

  if(sok != 0) Send_ChangeStatus(new_status);
}


void ICQ_Keep_Alive()
{
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> ICQ_Keep_Alive");

  Send_KeepAlive();
}

/* Called with timeout = 1000 from SelectServer
  ie, called 100x per second as long we're connecting */
/* timeout is in milliseconds */
/* !@# Check time since began login attempt !!! */
void ICQ_Check_Response(guint32 timeout)
{
  struct timeval tv;
  fd_set readfds, writefds;
  int numfds;
  int optval;
  int optlen;
  int sock;
  Contact_Member* c;
  GList* curr;

  optlen = sizeof(int);
  numfds = ((sok > tcp_sok) ? sok : tcp_sok);
  tv.tv_sec = 0;
  tv.tv_usec = timeout;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(sok, &readfds);
  FD_SET(tcp_sok, &readfds);
  
  curr = g_list_first(open_sockets);
  while(curr)
  {
    sock = (int)(curr->data);
    curr = g_list_next(curr);

    FD_SET(sock, &readfds);
    FD_SET(sock, &writefds);
    if(sock > numfds) numfds = sock;
  }

  /* don't care about exceptfds: */
  select(numfds + 1, &readfds, &writefds, NULL, &tv);

  if(FD_ISSET(sok, &readfds))
    Rec_Packet();

  if(FD_ISSET(tcp_sok, &readfds))
	{
			fprintf(stderr, "ABOUT TO CALL TCP_ReadPacket\n");
    TCP_ReadPacket(tcp_sok);
	}
  
  
  curr = g_list_first(open_sockets);
  while(curr)
  {
    sock = (int)(curr->data);
    c = contact_from_socket(sock);
    curr = g_list_next(curr);

    if(c)
    {
      if(sock == c->tcp_sok && FD_ISSET(sock, &writefds) &&
        c->tcp_status == TCP_NOT_CONNECTED)
      {
        if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) < 0)
			optval = ERRNO;

        if(optval)
        {
          ICQ_Debug (ICQ_VERB_INFO, "TCP connection failed\n");

          open_sockets = g_list_remove(open_sockets, (gpointer)sock);
          c->tcp_status |= TCP_FAILED;  /* Connection failed */
          c->tcp_sok = 0;
          UDP_SendMessages(c);
          break;
        }

          ICQ_Debug (ICQ_VERB_INFO, "TCP connection established\n");

        c->tcp_status |= TCP_CONNECTED;
        TCP_SendHelloPacket(sock);
        TCP_SendMessages(c);
        continue;
      }


      if(sock == c->chat_sok && FD_ISSET(sock, &writefds) &&
        c->chat_status == TCP_NOT_CONNECTED)
      {
        if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) < 0)
          optval = ERRNO;

        if(optval)
        {
            ICQ_Debug(ICQ_VERB_ERR, "TCP chat connection failed\n");

          open_sockets = g_list_remove(open_sockets, (gpointer)sock);
          c->chat_status |= TCP_FAILED;  /* Connection failed */
          c->chat_sok = 0;
          break;
        }

        ICQ_Debug(ICQ_VERB_INFO, "TCP chat connection established\n");

        c->chat_status |= TCP_SERVER;
        TCP_ChatServerHandshake(sock);
        continue;
      }


      if(sock == c->chat_sok && FD_ISSET(sock, &readfds))
      {
        if(c->chat_status & TCP_CLIENT)
          if(c->chat_status & TCP_HANDSHAKE_WAIT)
            TCP_ChatClientHandshake(sock);
          else
            TCP_ChatReadClient(sock);
        else
          TCP_ChatReadServer(sock);

        continue;
      }
    }

    if(FD_ISSET(sock, &readfds)) TCP_ReadPacket(sock);
  }
}


void ICQ_Register_Callback(gint type, FUNC_CALLBACK func)
{
#if 0
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> ICQ_Register_Callback");
#endif
  event[type] = func;
}


void ICQ_Send_Message(guint32 uin, gchar* text)
{
  Contact_Member* c;
  MESSAGE_DATA_PTR msg;

  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> ICQ_Send_Message");

  if(!(c = contact(uin))) return;

  if(c->tcp_status & TCP_CONNECTED)
  {
    if(!TCP_SendMessage(uin, text))
      Send_Message(uin, text);  /* fall back on UDP send */
  }
  else if(c->tcp_status & TCP_FAILED || c->status == STATUS_OFFLINE)
  {
    Send_Message(uin, text);
  }
  else
  {
    msg = g_malloc(sizeof(MESSAGE_DATA));

    msg->type = MSG_MESS;
    msg->text = g_strdup(text);
    msg->url = NULL;

    c->messages = g_list_append(c->messages, (gpointer)msg);
    c->tcp_sok = TCP_Connect(c->current_ip, c->tcp_port);
    if (c->tcp_sok == -1)
    {
      c->tcp_status |= TCP_FAILED;
      Send_Message(uin, text);
    }

  }
}

void ICQ_Send_URL(guint32 uin, gchar* url, gchar* text)
{
  Contact_Member* c;
  MESSAGE_DATA_PTR msg;

  ICQ_Debug (ICQ_VERB_INFO, "LIBICQ> ICQ_Send_URL");

  if(!(c = contact(uin))) return;

  if(c->tcp_status & TCP_CONNECTED)
  {
    if(!TCP_SendURL(uin, url, text))
      Send_URL(uin, url, text);
  }
  else if(c->tcp_status & TCP_FAILED || c->status == STATUS_OFFLINE)
  {
    Send_URL(uin, url, text);
  }
  else
  {
    msg = g_malloc(sizeof(MESSAGE_DATA));

    msg->type = URL_MESS;
    msg->text = g_strdup(text);
    msg->url = g_strdup(url);

    c->messages = g_list_append(c->messages, (gpointer)msg);
    c->tcp_sok = TCP_Connect(c->current_ip, c->tcp_port);
  }
}

void ICQ_Request_Chat(guint32 uin, gchar* text)
{
  Contact_Member* c;
  MESSAGE_DATA_PTR msg;

  if(!(c = contact(uin))) return;

  if(c->tcp_status & TCP_CONNECTED)
  {
    TCP_SendChatRequest(uin, text);
  }
  else if(c->tcp_status & TCP_FAILED || c->status == STATUS_OFFLINE)
  {
    printf("ICQ_Request_Chat(): Connection timed out\n");
  }
  else
  {
    msg = g_malloc(sizeof(MESSAGE_DATA));

    msg->type = CHAT_MESS;
    msg->text = g_strdup(text);
    msg->url = NULL;

    c->messages = g_list_append(c->messages, (gpointer)msg);
    c->tcp_sok = TCP_Connect(c->current_ip, c->tcp_port);
  }
}

void ICQ_Accept_Chat(guint32 uin)
{
/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Accept_Chat(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */
  TCP_AcceptChat(uin);
}

void ICQ_Refuse_Chat(guint32 uin)
{
/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Refuse_Chat(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  TCP_RefuseChat(uin);
}

void ICQ_Send_Chat(guint32 uin, gchar* text)
{
/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Send_Chat(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  TCP_ChatSend(uin, text);
}

void ICQ_Get_Away_Message(guint32 uin)
{
  Contact_Member* c;
  MESSAGE_DATA_PTR msg;

/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Get_Away_Message(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  if(!(c = contact(uin))) return;

  if(c->tcp_status & TCP_CONNECTED)
  {
    TCP_GetAwayMessage(uin);
  }
  else if(c->tcp_status & TCP_FAILED)
  {
    printf("ICQ_Get_Away_Message(): Connection timed out\n");
  }
  else
  {
    msg = g_malloc(sizeof(MESSAGE_DATA));

    msg->type = AWAY_MESS;
    msg->text = NULL;
    msg->url = NULL;

    c->messages = g_list_append(c->messages, (gpointer)msg);
    c->tcp_sok = TCP_Connect(c->current_ip, c->tcp_port);
  }
}


void ICQ_Search(gchar *email, gchar *nick, gchar* first, gchar* last)
{
/*** MIZHI 04162001 */
  gchar* errmsg;
  errmsg = g_new0(gchar, 128 + strlen(email) + strlen(nick) + strlen(first) + strlen(last));
  sprintf(errmsg, "LIBICQ> ICQ_Search(%s, %s, %s, %s)", email, nick, first, last);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  g_free(errmsg);
/*** */

  Send_SearchRequest(email, nick, first, last);
}


void ICQ_Get_Info(guint32 uin)
{
/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Get_Info(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  Send_InfoRequest(uin);
}


void ICQ_Get_Ext_Info(guint32 uin)
{
/*** MIZHI 04162001 */
  gchar errmsg[255];
  sprintf(errmsg, "LIBICQ> ICQ_Get_Info(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

  Send_ExtInfoRequest(uin);
}


void ICQ_Add_User(guint32 uin, gchar* name)
{
  Contact_Member* c;
  
/*** MIZHI 04162001 */
  gchar* errmsg;
  errmsg = g_new0(gchar, 128 + strlen(name));
  sprintf(errmsg, "LIBICQ> ICQ_Add_User(%d, %s)", uin, name);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  g_free(errmsg);
/*** */

  c = contact(uin);

  if(!c)
  {
    Contacts[Num_Contacts].uin = uin;
    Contacts[Num_Contacts].status = STATUS_OFFLINE;
    Contacts[Num_Contacts].last_time = -1L;
    Contacts[Num_Contacts].current_ip = -1L;
    Contacts[Num_Contacts].tcp_sok = -1L;
    Contacts[Num_Contacts].tcp_port = 0;
    Contacts[Num_Contacts].tcp_status = TCP_NOT_CONNECTED;
    Contacts[Num_Contacts].chat_sok = -1L;
    Contacts[Num_Contacts].chat_port = 0;
    Contacts[Num_Contacts].chat_status = TCP_NOT_CONNECTED;
    memcpy(Contacts[Num_Contacts].nick, name, sizeof(Contacts->nick));
    Num_Contacts++;
  }
  else
  {
    if(c->tcp_sok > 0)
    {
      open_sockets = g_list_remove(open_sockets, (gpointer)c->tcp_sok);
      close(c->tcp_sok);
    }

    if(c->chat_sok > 0)
    {
      open_sockets = g_list_remove(open_sockets, (gpointer)c->chat_sok);
      close(c->chat_sok);
    }

    c->status = STATUS_OFFLINE;
    c->current_ip = -1L;
    c->tcp_sok = 0;
    c->tcp_port = 0;
    c->tcp_status = TCP_NOT_CONNECTED;
    c->chat_sok = 0;
    c->chat_port = 0;
    c->chat_status = TCP_NOT_CONNECTED;
    c->chat_active = FALSE;
    c->chat_active2 = FALSE;
  }

  Send_ContactList();
//  Write_Contacts_RC(contacts_rc);
}


void ICQ_Rename_User(guint32 uin, gchar* name)
{
  Contact_Member* c;


/*** MIZHI 04162001 */
  gchar* errmsg;  
  errmsg = g_new0(gchar, 128 + strlen(name));
  sprintf(errmsg, "LIBICQ> ICQ_Rename_User(%d, %s)", uin, name);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  g_free(errmsg);
/*** */

  if(!(c = contact(uin))) return;

  memcpy(c->nick, name, sizeof(Contacts->nick));
//  Write_Contacts_RC(contacts_rc);
}


void ICQ_Delete_User(guint32 uin)
{
  Contact_Member* c;

/*** MIZHI 04162001 */
  gchar errmsg[255];  
  sprintf(errmsg, "LIBICQ> ICQ_Delete_User(%d)", uin);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */  

  if(!(c = contact(uin))) return;
 
  if(c->tcp_sok > 0)
  {
    open_sockets = g_list_remove(open_sockets, (gpointer)c->tcp_sok);
    close(c->tcp_sok);
  }

  if(c->chat_sok > 0)
  {
    open_sockets = g_list_remove(open_sockets, (gpointer)c->chat_sok);
    close(c->chat_sok);
  }
 
  c->uin = 0;
  c->tcp_sok = 0;
  c->tcp_port = 0;
  c->tcp_status = TCP_NOT_CONNECTED;
  c->chat_sok = 0;
  c->chat_port = 0;
  c->chat_status = TCP_NOT_CONNECTED;
  c->chat_active = FALSE;
  c->chat_active2 = FALSE;

//  Write_Contacts_RC(contacts_rc);
}

/* ICQ_Debug - added by Michael on 1999/04/11
   Handles all debugging messages and tosses them to stderr/out or a log file
*/

void ICQ_Debug(gint level, const gchar *message)
{
  FILE *logfile;
  time_t time_now;
  gchar time_now_string[100];
#if 0
  static gint been_here=0;
#endif

  if (icq_logging & ICQ_LOG_DEBUG)
  {
    /* Make date string */
    time(&time_now);
    strcpy(time_now_string, ctime(&time_now));
    time_now_string[strlen(time_now_string)-1] = '\0';
    /* Make file path+name string */
    strcpy(logpathfilename, (char *)getenv("HOME"));
    strcat(logpathfilename, "/.icq/");
    strcat(logpathfilename, logfilename);
#if 0
    if(been_here == 0)
    {
      fprintf(stderr, "\nLIBICQ> Logging debug info to: %s",
        logpathfilename);
      been_here++;
    }
#endif 
    if (Verbose & level)
    {
      fprintf(stderr, "\nDebug level %0X - %s",
        level, message);
    }
    logfile = fopen(logpathfilename, "a");
    if (logfile == NULL)
    {
      fprintf(stderr, "\nOpening logfile %s failed.\n",
        logpathfilename);
    }
    else
    {
      fprintf(logfile, "%s (%0X): %s\n",
        time_now_string, level, message);
    }
    fclose(logfile);
  }
}

void ICQ_SetDebug(gint debug)
{
  if((Verbose > ICQ_VERB_NONE) || (debug > ICQ_VERB_NONE))
    fprintf(stderr, "\nLIBICQ> ICQ_SetDebug(%d)", debug);
  
  if((debug >= ICQ_VERB_NONE) && (debug <= ICQ_VERB_MAX))
    Verbose = debug;
  else
    fprintf(stderr, " \\ Bad debug value: %d", debug);
}

/* ICQ_SetLogging (added by Michael on 1999/04/11)
   Handles logging features; both debugging messages and incoming/outgoing
   messages with contacts.

   logging == 0 == no logging
   logging && ICQ_LOG_DEBUG == log all debug messages
   logging && ICQ_LOG_MESS == log all contact messages
*/

void ICQ_SetLogging(gint logging)
{
  if(Verbose > ICQ_VERB_NONE || logging > ICQ_VERB_NONE)
    fprintf(stderr, "\nLIBICQ> ICQ_SetLogging(%d)", logging);

  icq_logging = logging;

  if(icq_logging > ICQ_LOG_ALL)
    icq_logging = ICQ_LOG_ALL;
  if(icq_logging < ICQ_LOG_NONE)
    icq_logging = ICQ_LOG_NONE;
}


void TCP_SendMessages(Contact_Member* c)
{
  GList* msg;
  MESSAGE_DATA_PTR curr_msg;

  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> TCP_SendMessages");

  while( ( msg = g_list_first(c->messages) ) )
  {
    curr_msg = ((MESSAGE_DATA_PTR)(msg->data));

    if(curr_msg->type == MSG_MESS)
      TCP_SendMessage(c->uin, curr_msg->text);
    else if(curr_msg->type == URL_MESS)
      TCP_SendURL(c->uin, curr_msg->url, curr_msg->text);
    else if(curr_msg->type == AWAY_MESS)
      TCP_GetAwayMessage(c->uin);
    else if(curr_msg->type == CHAT_MESS)
      TCP_SendChatRequest(c->uin, curr_msg->text);

    g_free(curr_msg->text);
    g_free(curr_msg->url);
    g_free(curr_msg);

    c->messages = g_list_remove_link(c->messages, g_list_first(c->messages));
  }
}

void UDP_SendMessages(Contact_Member* c)
{
  GList* msg;
  MESSAGE_DATA_PTR curr_msg;

  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> UDP_SendMessages");

  while( ( msg = g_list_first(c->messages) ) )
  {
    curr_msg = ((MESSAGE_DATA_PTR)(msg->data));

    if(curr_msg->type == MSG_MESS)
      Send_Message(c->uin, curr_msg->text);
    else if(curr_msg->type == URL_MESS)
      Send_URL(c->uin, curr_msg->url, curr_msg->text);

    g_free(curr_msg->text);
    g_free(curr_msg->url);
    g_free(curr_msg);

    c->messages = g_list_remove_link(c->messages, g_list_first(c->messages));
  }
}

