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

#ifndef __EB_WORKWIZU_H__
#define __EB_WORKWIZU_H__
 
#include "service.h"

struct service_callbacks * workwizu_query_callbacks();

typedef struct {
	int fd;
	int tag_r;
        int tag_w;
}tag_info;

typedef struct _wwz_account_data wwz_account_data;

struct _wwz_account_data
{
	char *password;
	int sock;
	eb_chat_room *chat_room; /* only one */	
};

typedef struct _wwz_user wwz_user;

struct _wwz_user {
	char username[255];
	char password[255];
	char challenge[255];
	char owner[255];
	int uid;
	int is_driver;
	int channel_id;
	int has_speak;
	int is_typing;
	int statssender;
	int typing_handler;
};

typedef enum 
{
	CONN_CLOSED,
	CONN_WAITING_CHALLENGE,
	CONN_WAITING_USERINFO,
	CONN_ESTABLISHED
} ConnState;
	
static void clean_up (int sock);
eb_account * eb_workwizu_new_account (const char *account);
void eb_workwizu_add_user (eb_account *account);
void eb_workwizu_del_user (eb_account *account);
void eb_workwizu_logout (eb_local_account *account);



/* destinations */
#define CHANNEL 0
#define NODEST -1
#define SERVER -2
#define EXCEPTME -3
#define NOTDEFINED -4
#define VNCWRAPPERREFLECTOR -5

/* tags */
#define INVALIDPACKET 1
#define CONNECTIONESTABLISHED 2
#define CONNECTIONCLOSED 3
#define CONNECT 4
#define DISCONNECT 5
#define NUKESTUDENT 6
#define NEWUSER 7
#define REMOVEUSER 8
#define REMOVECHANNEL 9
#define IAMHERE 11
#define CHAT 12
#define ASKPRIVATE 13
#define CLOSEPRIVATE 14
#define PRIVATECHAT 15
#define PHRASEFORCHATFIELD 16
#define APPEND 17
#define ASKCLOSE 18
#define USERLISTBLINK 19
#define USERLISTSELECT 20
#define REMOVEUSERLISTBLINK 21
#define CHANNELLIST 22
#define VISIBLEUSER 23
#define URL 100
#define DOCUMENTCOMPLETE 101
#define SHOW 102
#define WRITE 103
#define COMMENT 104
#define QUESTION 105
#define BEEPER 106
#define ALLOWSPEAK 107
#define STOPSPEAK 108
#define PRIVATEMESSAGEALERT 109
#define PRIVATEMESSAGEALERTREMOVE 110
#define QUIZZ 111
#define PROXYREQUEST 112
#define PROXYMESSAGE 113
#define SHOWCURRENT 114
#define PUBLISHCURRENT 115
#define ALLOWBROWSING 116
#define MARKER 117
#define MARKERURL 118
#define SCROLLURL 119
#define SHOWACTIVATE 120
#define REFRESHDIAPO 121
#define OPENBOARD 200
#define CLOSEBOARD 201
#define BOARDORDER 202
#define BOARDRESIZED 203
#define DRAWINGMODE 204
#define GIVEPENCIL 205
#define REMOVEPENCIL 206
#define OPENPRIVATEBOARD 207
#define BOARDTARGET 208
#define PENSIZE 209
#define PENCOLOR 210
#define OPENTRANSPARENT 211
#define BACKGROUNDNODE 212
#define RAWSHAPEORDER 213
#define SHAPEORDER 214
#define CHATFIELDTARGET 300
#define OPENMATHEDITOR 400
#define WAITFORADRIVER 500
#define DRIVERNOTFOUND 501
#define DRIVERENTEREDINCHATROOM 502
#define VNCWRAPPERTAG 600
#define NEWSENDPAGETOUSER 700
#define UPSIZE 800
#define DOWNSIZE 801
#define CHANGEPENSTYLE 802
#define GOTOBOARD 803
#define STARTSPEAKER 900
#define STOPSPEAKER 901
#define STARTVOICEIPLISTENING 902
#define SPEAKERSTARTED 903
#define STARTVIDEOSPEAKER 904
#define STOPVIDEOSPEAKER 905
#define SPEAKERVIDEOSTARTED 906
#define STARTVIDEOLISTENING 907
#define STARTVIDEOLISTENINGDONE 908
#define FRONTWINDOW 1000
#define VALIDATECHATFIELD 1100
#define STARTRECORD 1200
#define STOPRECORD 1201
#define TABACTIVATED 1300
#define TABSHOWN 1301
#define TABHIDDEN 1302
#define TABACTIVATE 1303
#define TABCLOSED 1304
#define DISPLAYUP 1400
#define DISPLAYDOWN 1401
#define SPECIFIC 1500
#define USERSTATE 1501
#define HISTORYPREV 1600
#define HISTORYNEXT 1601
#define POLL 1700
#define POLLRESULT 1701
#define OPENPOLL 1702
#define SWITCHCONFIG 1800
#define APPSHARESTART 1900
#define APPSHAREINVITE 1901
#define APPSHARERESPOND 1902
#define APPSHAREDATA 1903
#define APPSHARESTOP 1904
#define MEDIALOCATOR 2000
#endif /*__EB_WORKWIZU_H__*/
