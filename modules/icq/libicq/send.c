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



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined( _WIN32 ) && !defined(__MINGW32__)
#include <windows.h>
#include <winbase.h>
#define sleep( x ) Sleep( x )
#include <io.h>
#else /* !_WIN32 */
#ifndef __MINGW32__
#include <netdb.h>
#endif
#include <unistd.h>
#ifndef __MINGW32__
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#endif /* _WIN32 */
#ifdef __MINGW32__
#define sleep(x) Sleep(x)
#endif

#include <sys/types.h>


#include <glib.h>

#include "icq.h"
#include "libicq.h"
#include "send.h"
#include "tcp.h"
#include "util.h"
#include "libproxy.h"

extern gint SRV_Addresses;
extern gint SRV_AddressToUse;
extern gint Verbose;
extern gint sok;
extern gint tcp_sok;
extern guint16 last_cmd[1024];
extern guint32 UIN;
extern guint32 our_ip;
extern guint32 our_port;
extern guint16 seq_num;
extern Contact_Member Contacts[];
extern int Num_Contacts;
extern guint32 Current_Status;
extern GList* Search_Results;
extern GList* open_sockets;


/*****************************************************
Connects to hostname on port port
hostname can be DNS or nnn.nnn.nnn.nnn
write out messages to the FD aux
******************************************************/
int Connect_Remote(char* hostname, guint32 port)
{
  int length, ip_index, retval;
  struct sockaddr_in sin;           /* used to store inet addr stuff */
  struct hostent *host_struct=NULL; /* used in DNS lookup */
  char *host = NULL;
  

/*** MIZHI 04162001 */
  char* errmsg;
  errmsg = g_new0(gchar, 128 + strlen(hostname));
  sprintf(errmsg, "LIBICQ> Connect_Remote(%s, %d)", hostname, port);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  g_free(errmsg);
/*** */  

  ip_index = 0;

  sin.sin_addr.s_addr = inet_addr(hostname);  /* checks for n.n.n.n notation */

  if(sin.sin_addr.s_addr  == -1)  /* name isn't n.n.n.n so must be DNS */
  {
    do
    {
      if((host_struct = proxy_gethostbyname(hostname)) == NULL)
      {
        switch(h_errno)
        {
          case HOST_NOT_FOUND:
	    errmsg = g_new0(gchar, 128 + strlen(hostname));	
            sprintf(errmsg, "%s: host not found!\n", hostname);	    
            ICQ_Debug(ICQ_VERB_ERR, errmsg);
	    g_free(errmsg);
            return 0;
          case NO_ADDRESS:
	    errmsg = g_new0(gchar, 128 + strlen(hostname));	
            sprintf(errmsg, "%s does not have an IP address.\n", hostname);
            ICQ_Debug(ICQ_VERB_ERR, errmsg);
	    g_free(errmsg);
            return 0;
          case NO_RECOVERY:
	    errmsg = g_new0(gchar, 128 + strlen(hostname));	
            sprintf(errmsg, "Unrecoverable DNS error while looking up %s.\n",
                    hostname);
            ICQ_Debug(ICQ_VERB_ERR, errmsg);
	    g_free(errmsg);
            return 0;
          case TRY_AGAIN:
	    errmsg = g_new0(gchar, 128 + strlen(hostname));	
            sprintf(errmsg, "Couldn't look up %s.  Trying again.\n", hostname);
            ICQ_Debug(ICQ_VERB_ERR, errmsg);
            sleep(1);  /* don't want to do this too fast! */
	    g_free(errmsg);
            break;
        }
      }
    } while (h_errno == TRY_AGAIN);

    errmsg = g_new0(gchar, 128 + strlen(host_struct->h_name));
    sprintf(errmsg, "Server name: %s", host_struct->h_name);
    ICQ_Debug(ICQ_VERB_INFO, errmsg);
    ICQ_Debug(ICQ_VERB_INFO, "Addresses follow ... ");
    g_free(errmsg);

    while(host_struct->h_addr_list[ip_index])
    {
      sin.sin_addr = *((struct in_addr *)host_struct->h_addr_list[ip_index]);
      errmsg = g_new0(gchar, 128 + strlen(inet_ntoa(sin.sin_addr)));
      sprintf(errmsg, "Address #%d: %s", ip_index, inet_ntoa(sin.sin_addr));
      ICQ_Debug(ICQ_VERB_INFO, errmsg);
      g_free(errmsg);
      ip_index++;
    }

    /* Record the number of addresses: */
    SRV_Addresses = ip_index;

    /* Actually use the next IP and increment */
    sin.sin_addr = *((struct in_addr *)host_struct->h_addr_list[SRV_AddressToUse++]);
    host = (char*)strdup(inet_ntoa(sin.sin_addr));

    /* Make sure we're not getting into trouble next time */
    if(SRV_AddressToUse >= SRV_Addresses) SRV_AddressToUse = 0;
  } /* ENDIF DNS */
  else
  {
    sin.sin_addr = *((struct in_addr *)host_struct->h_addr);
  }

  sin.sin_family = host_struct->h_addrtype;
  sin.sin_port = htons(port);
  /* bzero is old and outdated, use memset instead (POSIX + Win32)*/
  /* win32--bzero(&(sin.sin_zero), 8);*/
  memset( &(sin.sin_zero), 0, 8 );
   
  sok = socket(AF_INET, SOCK_DGRAM, 0);  /* create the unconnected socket*/

  if(sok == -1)
  {
    ICQ_Debug(ICQ_VERB_ERR, "Socket creation failed.");
    return 0;
  }
   
    ICQ_Debug(ICQ_VERB_INFO, "Socket created.  Attempting to connect." );
 
  retval = proxy_connect(sok, (struct sockaddr*)&sin, sizeof(sin));

  if(retval == -1)  /* did we connect ?*/
  {
    errmsg = g_new0(gchar, 128 + strlen(host));    
    sprintf(errmsg, "Conection Refused on port %d at %s", port, host);
    ICQ_Debug(ICQ_VERB_ERR, errmsg);
    g_free(errmsg);
    g_free(host);
    return 0;
  }

  length = sizeof(sin) ;
  getsockname(sok, (struct sockaddr*)&sin, &length);
  our_ip = sin.sin_addr.s_addr;
  our_port = ntohs(sin.sin_port + 2);

  errmsg = g_new0(gchar, 128 + strlen(host));
  sprintf(errmsg, "Our IP address is %08X\n", our_ip);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  sprintf(errmsg, "The port that will be used for tcp is %d\n", our_port);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  sprintf(errmsg, "Connected to %s, waiting for response\n", host);
  ICQ_Debug(ICQ_VERB_INFO, errmsg);
  g_free(errmsg);

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(our_port);
  /* bzero is old and outdated, use memset instead (POSIX + Win32)*/
  /* win32--bzero(&(sin.sin_zero), 8);*/
  memset( &(sin.sin_zero), 0, 8 );

  tcp_sok = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(tcp_sok, SOL_SOCKET, SO_REUSEADDR, (const char *)&retval, 4);
  set_nonblock(tcp_sok);
  retval = bind(tcp_sok, (struct sockaddr*)&sin, sizeof(sin));
  
  if(retval == -1)
  {
    if(Verbose & ICQ_VERB_WARN)
    {
      errmsg = g_new0(gchar, 128);
      sprintf(errmsg, "Could not bind to tcp socket %d, port %d\n", 
        tcp_sok, our_port);
      ICQ_Debug(ICQ_VERB_WARN, errmsg);
      g_free(errmsg);
    }
  }

  retval = listen(tcp_sok, 10);

  if(retval == -1)
  {
      errmsg = g_new0(gchar, 128);
      sprintf(errmsg, "Could not listen to tcp socket %d, port %d\n", 
        tcp_sok, our_port);
      ICQ_Debug(ICQ_VERB_WARN, errmsg);
      g_free(errmsg);
  }

  free(host);  
  return sok;
}

/***********************************************************
This routine sends the acknowledgement command to
the server.  It appears that this must be done after
everything the server sends us
************************************************************/
void Send_Ack(int seq)
{
   net_icq_pak pak;
/*** MIZHI 04162001 */
   gchar errmsg[255];
   sprintf(errmsg, "LIBICQ> Send_Ack(%d)", seq);
   ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_ACK );
   Word_2_Chars( pak.head.seq, seq );
   DW_2_Chars( pak.head.UIN, UIN);
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );

   /* Kinda cheap way of hopefully making connection more stable
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   */
}


/************************************************************
This procedure logins into the server with UIN and pass on the socket
sok and gives our ip and port.  It does NOT wait for any kind of a response.
*************************************************************/
void Send_BeginLogin(guint32 UIN, char* pass, guint32 ip, guint32 port)
{
   net_icq_pak pak;
   int size;
   login_1 s1;
   login_2 s2;
   struct sockaddr_in sin;  /* used to store inet addr stuff  */
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_BeginLogin");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_LOGIN );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   DW_2_Chars(s1.port, port);
   Word_2_Chars( s1.len, strlen( pass ) + 1 );
   
   DW_2_Chars( s2.ip, ip );
   sin.sin_addr.s_addr = Chars_2_DW( s2.ip );
   DW_2_Chars( s2.status, 0 );
   
   DW_2_Chars( s2.X1, LOGIN_X1_DEF );
   s2.X2[0] = LOGIN_X2_DEF;
   DW_2_Chars( s2.X3, LOGIN_X3_DEF );
   DW_2_Chars( s2.X4, LOGIN_X4_DEF );
   DW_2_Chars( s2.X5, LOGIN_X5_DEF );
   
   memcpy( pak.data, &s1, sizeof( s1 ) );
   size = sizeof(s1);
   memcpy( &pak.data[size], pass, Chars_2_Word( s1.len ) );
   size += Chars_2_Word( s1.len );
   memcpy( &pak.data[size], &s2.X1, sizeof( s2.X1 ) );
   size += sizeof( s2.X1 );
   memcpy( &pak.data[size], &s2.ip, sizeof( s2.ip ) );
   size += sizeof( s2.ip );
   memcpy( &pak.data[size], &s2.X2, sizeof( s2.X2 ) );
   size += sizeof( s2.X2 );
   memcpy( &pak.data[size], &s2.status, sizeof( s2.status ) );
   size += sizeof( s2.status );
   memcpy( &pak.data[size], &s2.X3, sizeof( s2.X3 ) );
   size += sizeof( s2.X3 );
   memcpy( &pak.data[size], &s2.X4, sizeof( s2.X4 ) );
   size += sizeof( s2.X4 );
   memcpy( &pak.data[size], &s2.X5, sizeof( s2.X5 ) );
   size += sizeof( s2.X5 );
   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head )- 2 );
   last_cmd[ seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
} 


/**************************************
This sends the second login command
this is necessary to finish logging in.
***************************************/
void Send_FinishLogin()
{
   net_icq_pak pak;
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_FinishLogin");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_LOGIN_1 );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/*********************************************
This must be called every 2 minutes so the server knows
we're still alive.  JAVA client sends two different commands
so we do also :)
**********************************************/
void Send_KeepAlive()
{
   net_icq_pak pak;
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_KeepAlive");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_KEEP_ALIVE );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   last_cmd[(seq_num - 1) & 0x3ff ] = Chars_2_Word( pak.head.cmd );

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_KEEP_ALIVE2 );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   last_cmd[(seq_num - 1) & 0x3ff ] = Chars_2_Word( pak.head.cmd );

   if(Verbose & ICQ_VERB_INFO)
      printf( "\nSent keep alive packet to the server\n" );
}


/********************************************************
This must be called to remove messages from the server
*********************************************************/
void Send_GotMessages()
{
   net_icq_pak pak;
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_GotMessages");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_ACK_MESSAGES );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 );
   last_cmd[ (seq_num - 1) & 0x3ff ] = Chars_2_Word( pak.head.cmd );
}


/*************************************
this sends over the contact list
**************************************/
void Send_ContactList()
{
   net_icq_pak pak;
   int num_used;
   int i, size;
   char *tmp;

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_ContactList");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_CONT_LIST );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   tmp = pak.data;
   tmp++;
   for(i = 0, num_used = 0; i < Num_Contacts; i++)
   { 
      if(Contacts[i].uin > 0 && Contacts[i].status != STATUS_NOT_IN_LIST)
      {
         DW_2_Chars(tmp, Contacts[i].uin);
         tmp += 4;
         num_used++;
      }
   }

   pak.data[0] = num_used;
   size = ((size_t)tmp - (size_t)pak.data);
   size += sizeof(pak.head) - 2;

   SOCKWRITE( sok, &(pak.head.ver), size );

   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/***************************************************
Sends a message thru the server to uin.  Text is the
message to send.
****************************************************/
void Send_Message(guint32 uin, char *text)
{
   SIMPLE_MESSAGE msg;
   net_icq_pak pak;
   int size, len; 

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_Message");

   len = strlen(text);
   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_SENDM );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   DW_2_Chars( msg.uin, uin );
   DW_2_Chars(msg.type, 0x0001 );      /* A type 1 msg*/
   Word_2_Chars( msg.len, len + 1 );      /* length + the NULL */

   memcpy(&pak.data, &msg, sizeof( msg ) );
   memcpy(&pak.data[8], text, len + 1);

   size = sizeof( msg ) + len + 1;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/***************************************************
Sends a URL message through the server to uin.
****************************************************/
void Send_URL(guint32 uin, char* url, char *text)
{
   SIMPLE_MESSAGE msg;
   net_icq_pak pak;
   char temp[2048];
   int size, len; 
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_URL");

   if(!url) url = "";
   if(!text) text = "";
   
   strcpy(temp, text);
   strcat(temp, "\xFE");
   strcat(temp, url);

   len = strlen(temp);
   Word_2_Chars(pak.head.ver, ICQ_VER);
   Word_2_Chars(pak.head.cmd, CMD_SENDM);
   Word_2_Chars(pak.head.seq, seq_num++);
   DW_2_Chars(pak.head.UIN, UIN);

   DW_2_Chars(msg.uin, uin);
   DW_2_Chars(msg.type, URL_MESS);
   Word_2_Chars(msg.len, len + 1 );      /* length + the NULL */

   memcpy(&pak.data, &msg, sizeof(msg));
   memcpy(&pak.data[8], temp, len + 1);

   size = sizeof(msg) + len + 1;

   SOCKWRITE(sok, &(pak.head.ver), size + sizeof(pak.head) - 2);
   last_cmd[seq_num - 1] = Chars_2_Word(pak.head.cmd);
}


/**************************************************
Sends an authorization reply to the server so the Mirabilis
client can add the user.
***************************************************/
void Send_AuthMessage(guint32 uin)
{
   SIMPLE_MESSAGE msg;
   net_icq_pak pak;
   int size; 

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_AuthMessage");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_SENDM );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   DW_2_Chars( msg.uin, uin );
   DW_2_Chars( msg.type, AUTH_MESSAGE );      /* A type authorization msg*/
   Word_2_Chars( msg.len, 2 );      

   memcpy(&pak.data, &msg, sizeof( msg ) );

   pak.data[ sizeof(msg) ]=0x03;
   pak.data[ sizeof(msg) + 1]=0x00;

   size = sizeof( msg ) + 2;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}

/***************************************************
Changes the users status on the server
****************************************************/
void Send_ChangeStatus(guint32 status )
{
   net_icq_pak pak;
   int size;

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_ChangeStatus");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_STATUS_CHANGE );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   DW_2_Chars( pak.data, status | 0x10000 );
   Current_Status = status;

   size = 4;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/*********************************************************
Sends a request to the server for info on a specific user
**********************************************************/
void Send_InfoRequest(guint32 uin )
{
   net_icq_pak pak;
   int size;

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_InfoRequest");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_INFO_REQ );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   DW_2_Chars( pak.data, uin );
   size = 4;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/*********************************************************
Sends a request to the server for info on a specific user
**********************************************************/
void Send_ExtInfoRequest(guint32 uin)
{
   net_icq_pak pak;
   int size;

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_ExtInfoRequest");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_EXT_INFO_REQ );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

   DW_2_Chars( pak.data, uin );
   size = 4;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 2] = Chars_2_Word(pak.head.cmd);
}


/***************************************************************
Initializes a server search for the information specified
****************************************************************/
void Send_SearchRequest(char *email, char *nick, char* first, char* last)
{
   gpointer data;
   net_icq_pak pak;
   int size;
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_SearchRequest");

   while(g_list_last(Search_Results))
   {
     data = (g_list_last(Search_Results))->data;
     g_free(data);
     Search_Results = g_list_remove(Search_Results, data);
   }

   Search_Results = NULL;

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_SEARCH_USER );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );

/*
   Word_2_Chars( pak.data , seq_num++ );
   size = 2;
*/

   size = 0;
   Word_2_Chars( pak.data + size, strlen( nick ) + 1 );
   size += 2;
   strcpy( pak.data + size , nick );
   size += strlen( nick ) + 1;
   Word_2_Chars( pak.data + size, strlen( first ) + 1 );
   size += 2;
   strcpy( pak.data + size , first );
   size += strlen( first ) + 1;
   Word_2_Chars( pak.data + size, strlen( last ) + 1);
   size += 2;
   strcpy( pak.data + size , last );
   size += strlen( last ) +1 ;
   Word_2_Chars( pak.data + size, strlen( email ) + 1  );
   size += 2;
   strcpy( pak.data + size , email );
   size += strlen( email ) + 1;
   last_cmd[seq_num - 2 ] = Chars_2_Word( pak.head.cmd );
   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
}


/***************************************************
Registers a new UIN in the ICQ network
****************************************************/
void Send_RegisterNewUser(char *pass)
{
   net_icq_pak pak;
   char len_buf[2];
   int size, len; 

   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_RegisterNewUser");

   len = strlen(pass);
   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_REG_NEW_USER );
   Word_2_Chars( pak.head.seq, seq_num++ );
   Word_2_Chars( len_buf, len );

   memcpy(&pak.data[0], len_buf, 2 );
   memcpy(&pak.data[2], pass, len + 1);
   DW_2_Chars( &pak.data[2+len], 0xA0 );
   DW_2_Chars( &pak.data[6+len], 0x2461 );
   DW_2_Chars( &pak.data[10+len], 0xa00000 );
   DW_2_Chars( &pak.data[14+len], 0x00 );
   size = len + 18;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
}


/******************************************************************
Logs off ICQ should handle other cleanup as well
********************************************************************/
void Send_Disconnect()
{
   net_icq_pak pak;
   int size, len, cindex;
   
   ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Send_Disconnect");

   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_SEND_TEXT_CODE );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   len = strlen( "B_USER_DISCONNECTED" ) + 1;
   *(short * ) pak.data = len;
   size = len + 4;
   
   memcpy( &pak.data[2], "B_USER_DISCONNECTED", len );
   pak.data[ 2 + len ] = 05;
   pak.data[ 3 + len ] = 00;

   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head ) - 2);

   close(sok);
   close(tcp_sok);
   sok = 0;
   tcp_sok = 0;
   last_cmd[seq_num - 1 ] = Chars_2_Word( pak.head.cmd );

   for(cindex = 0; cindex < Num_Contacts; cindex++)
   {
     if(Contacts[cindex].tcp_sok > 0)
     {
       open_sockets = g_list_remove(open_sockets,
         (gpointer)Contacts[cindex].tcp_sok);
       close(Contacts[cindex].tcp_sok);
     }

     if(Contacts[cindex].chat_sok > 0)
     {
       open_sockets = g_list_remove(open_sockets,
         (gpointer)Contacts[cindex].chat_sok);
       close(Contacts[cindex].chat_sok);
     }

     Contacts[cindex].status = STATUS_OFFLINE;
     Contacts[cindex].current_ip = -1L;
     Contacts[cindex].tcp_sok = 0;
     Contacts[cindex].tcp_port = -1L;
     Contacts[cindex].tcp_status = TCP_NOT_CONNECTED;
     Contacts[cindex].chat_sok = 0;
     Contacts[cindex].chat_port = -1L;
     Contacts[cindex].chat_status = TCP_NOT_CONNECTED;
   }
}


