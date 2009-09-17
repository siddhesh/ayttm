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

#ifndef __LIBICQ_H__
#define __LIBICQ_H__

#define STATUS_NOT_IN_LIST (guint32)(-3L)	/* This is a made-up number */
#define STATUS_OFFLINE     (guint32)(-1L)
#define STATUS_ONLINE     0x0000
#define STATUS_INVISIBLE  0x0100
#define STATUS_NA         0x0005
#define STATUS_FREE_CHAT  0x0020
#define STATUS_OCCUPIED   0x0011
#define STATUS_AWAY       0x0001
#define STATUS_DND        0x0013

#define MSG_MESS          0x0001
#define MSG_MULTI_MESS	  0x0801
#define URL_MESS          0x0004
#define AUTH_REQ_MESS     0x0006
#define USER_ADDED_MESS   0x000C
#define CONTACT_MESS      0x0013

#define AWAY_MESS         0x1001	/* These are made-up numbers */
#define CHAT_MESS         0x1002
#define FILE_MESS         0x1003

/* to go with the new int version of Verbose */
#define ICQ_VERB_NONE	0	// show no messages
#define ICQ_VERB_ERR	(1<<0)	// show only errors
#define ICQ_VERB_WARN	(1<<1)	// show kludge messages
#define ICQ_VERB_INFO	(1<<2)	// show informative messages
#define ICQ_VERB_MEM	(1<<3)	// show memory usage
/* fill in extra range values here */
#define ICQ_VERB_MAX	( ICQ_VERB_ERR \
		|	ICQ_VERB_INFO \
		|	ICQ_VERB_WARN \
		|	ICQ_VERB_MEM )
#define ICQ_VERB_ALL	ICQ_VERB_MAX

#define ICQ_LOG_NONE	0	// log nothing to file
#define ICQ_LOG_DEBUG	(1<<0)	// log debugging messages to file
#define ICQ_LOG_MESS	(1<<1)	// log messages to file
#define ICQ_LOG_ALL	( ICQ_LOG_DEBUG \
		|	ICQ_LOG_MESS )

#define ICQ_MAX_CONTACTS 255	// maximum contacts

typedef struct {
	guint32 uin;
	gint year;
	gint month;
	gint day;
	gint hour;
	gint minute;
	gint type;
	gint32 len;
	gchar *msg;
	gchar *url;
	gchar *filename;
	guint32 file_size;
	guint32 seq;
} CLIENT_MESSAGE, *CLIENT_MESSAGE_PTR;

typedef struct {
	guint32 uin;
	gchar nick[20];
	guint32 status;
} USER_UPDATE, *USER_UPDATE_PTR;

typedef struct {
	guint32 uin;
	gchar nick[20];
	gchar first[50];
	gchar last[50];
	gchar email[50];
	gint auth_required;
} USER_INFO, *USER_INFO_PTR;

typedef struct {
	guint32 uin;
	gchar city[50];
	gchar country[50];
	gint country_code;
	gint country_status;
	gchar state[50];
	gchar age[15];
	gchar sex[15];
	gchar phone[15];
	gchar url[150];
	gchar about[1000];
} USER_EXT_INFO, *USER_EXT_INFO_PTR;

typedef struct {
	guint32 uin;
	guint32 status;
	guint32 last_time;	/* last time online or when came online */
	guint32 current_ip;
	gint tcp_sok;
	guint32 tcp_port;
	gint tcp_status;	/* can we get some stats on this one */
	gint chat_sok;
	guint32 chat_port;
	gint chat_status;
	gint chat_active;
	gint chat_active2;
	guint32 chat_seq;
	guint32 file_gdk_input;
	gchar nick[20];
	GList *messages;
} Contact_Member, *CONTACT_PTR;

typedef struct {
	guint32 uin;
	gchar nick[20];
	gchar first[50];
	gchar last[50];
	gchar email[50];
	gint auth_required;
} SEARCH_RESULT, *SEARCH_RESULT_PTR;

typedef struct {
	gint type;
	gchar *text;
	gchar *url;
} MESSAGE_DATA, *MESSAGE_DATA_PTR;

typedef struct {
	guint32 uin;
	gchar c;
} CHAT_DATA, *CHAT_DATA_PTR;

extern Contact_Member Contacts[ICQ_MAX_CONTACTS];
extern gint Num_Contacts;

extern guint32 UIN;
extern gchar passwd[];
extern gchar server[];
extern guint32 remote_port;
extern guint32 set_status;

/* To make dealing with function pointers a LOT easier... */
/* CALLBACK is a reserved word in Win32 code for FAR PASCAL functions
 * instead lets use FUNC_CALLBACK */
typedef void (*FUNC_CALLBACK) (void *);

/* Types of server messages */
enum { EVENT_LOGIN, EVENT_MESSAGE, EVENT_INFO, EVENT_EXT_INFO,
	EVENT_OFFLINE, EVENT_ONLINE, EVENT_STATUS_UPDATE,
	EVENT_SEARCH_RESULTS, EVENT_DISCONNECT, EVENT_CHAT_CONNECT,
	EVENT_CHAT_DISCONNECT, EVENT_CHAT_READ, EVENT_RTNSTATUS, NUM_EVENTS
};

/* An array of function pointers to store the callbacks */
void (*event[NUM_EVENTS]) (void *data);

/* Assigns "func" to the appropriate "event" pointer */
void ICQ_Register_Callback(gint event_type, FUNC_CALLBACK func);

/* This is the single polling function */
void ICQ_Check_Response(guint32 timeout);

/* Client is responsible for calling this every 2 min or whatever */
void ICQ_Keep_Alive();

/* Will be moved to gicq soon */
gint ICQ_Read_Config();

gint ICQ_Connect();
void ICQ_Disconnect();
void ICQ_Change_Status(guint32 new_status);
void ICQ_Send_Message(guint32 uin, gchar *text);
void ICQ_Send_URL(guint32 uin, gchar *url, gchar *text);
void ICQ_Request_Chat(guint32 uin, gchar *text);
void ICQ_Accept_Chat(guint32 uin);
void ICQ_Refuse_Chat(guint32 uin);
void ICQ_Send_Chat(guint32 uin, gchar *text);
void ICQ_Search(gchar *email, gchar *nick, gchar *first, gchar *last);
void ICQ_Get_Info(guint32 uin);
void ICQ_Get_Ext_Info(guint32 uin);
void ICQ_Get_Away_Message(guint32 uin);
void ICQ_Add_User(guint32 uin, gchar *name);
void ICQ_Rename_User(guint32 uin, gchar *name);
void ICQ_Delete_User(guint32 uin);
int ICQSendFile(char *ip, char *port, char *uin, char *fileName, char *comment);

void ICQ_SetLogging(gint logging);
void ICQ_Debug(gint level, const gchar *debug_message);
void ICQ_SetDebug(gint debug);

void DW_2_Chars(unsigned char *buf, guint32 num);
void Word_2_Chars(unsigned char *buf, guint16 num);

#endif				/* __LIBICQ_H__ */
