
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


#ifndef __ICQ_H__
#define __ICQ_H__

#ifdef __MINGW32__
#include <winsock2.h>
#define write(a,b,c) send(a,b,c,0)
#define read(a,b,c)  recv(a,b,c,0)
#endif

#define ICQ_VER 0x0004

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define PACKET_TYPE_TCP          1
#define PACKET_TYPE_UDP          2

#define PACKET_DIRECTION_SEND    4
#define PACKET_DIRECTION_RECEIVE 8


typedef struct
{
  guint8 dummy[2]; /* to fix alignment problem */
  guint8 ver[2];
  guint8 rand[2];
  guint8 zero[2];
  guint8 cmd[2];
  guint8 seq[2];
  guint8 seq2[2];
  guint8  UIN[4];
  guint8 checkcode[4];
} ICQ_pak, *ICQ_PAK_PTR;

typedef struct
{
  guint16 dummy; /* to fix alignment problem */
  guint8 ver[2];
  guint8 cmd[2];
  guint8 seq[2];
  guint8 seq2[2];
  guint8 UIN[4];
  guint8 check[4];
} SRV_ICQ_pak, *SRV_ICQ_PAK_PTR;


typedef struct
{
   ICQ_pak  head;
   unsigned char  data[1024];
} net_icq_pak, *NET_ICQ_PTR;

typedef struct
{
   SRV_ICQ_pak  head;
   unsigned char  data[1024];
} srv_net_icq_pak, *SRV_NET_ICQ_PTR;

#define CMD_ACK            0x000A 	/* Just an acknowledgement */
#define CMD_SENDM          0x010E
#define CMD_LOGIN          0x03E8	/* Send password and port to server */
#define CMD_REG_NEW_USER   0x03FC	/* Send password (after 04EC) for new */
#define CMD_NEW_USER_REG   0x03FC
#define CMD_CONT_LIST      0x0406	/* Send list of users for status check*/
#define CMD_SEARCH_UIN     0x041a
#define CMD_SEARCH_USER    0x0424
#define CMD_KEEP_ALIVE     0x042e	/* Keep alive */
#define CMD_SEND_TEXT_CODE 0x0438	/* Disconnection code? */
#define CMD_LOGIN_1        0x044c	/* Sent after 0528 */
#define CMD_MSG_TO_NEW_USER 0x0456	/* dupe of below! */
#define CMD_REQ_ADD_LIST   0x0456
#define CMD_INFO_REQ       0x0460	/* Request user information */
#define CMD_EXT_INFO_REQ   0x046a	/* Request extended information */
#define CMD_CHANGE_PW      0x049c
#define CMD_ACK_MESSAGES   0x0442
#define CMD_NEW_USER_INFO  0x04A6	/* Request info on a new user */
#define CMD_UPDATE_EXT_INFO 0x04B0	/* Rest of info (new user) after 00B4*/
#define CMD_QUERY_SERVERS  0x04BA	/* Send after 04C4 when creating new */
#define CMD_QUERY_ADDONS   0x04C4	/* Send after 044C when creating new */
#define CMD_STATUS_CHANGE  0x04D8
#define CMD_NEW_USER_1     0x04EC	/* Request for new user ID */
#define CMD_UPDATE_INFO    0x050A
#define CMD_KEEP_ALIVE2    0x051E	/* Sent after ACK of 8200 */
#define CMD_LOGIN_2        0x0528	/* Sent to accept login after 005A */
#define CMD_ADD_TO_LIST    0x053C
#define CMD_RANDOM_USER    0x056E	/* Request random user */
#define CMD_VIS_LIST       0x06AE
#define CMD_INVIS_LIST     0x06AE

#define AUTH_MESSAGE       0x0008
#define SRV_ACK            0x000A
#define SRV_FORCE_DISCONNECT 0x0028
#define SRV_NEW_UIN        0x0046	/* Your new UIN from the server */
#define SRV_LOGIN_REPLY    0x005A	/* Accepted login request */
#define SRV_USER_ONLINE    0x006E
#define SRV_USER_OFFLINE   0x0078
#define SRV_QUERY          0x0082	/* Receive info on an external app */
#define SRV_USER_FOUND     0x008C
#define SRV_NEW_USER       0x00B4	/* Received after ACK of 04A6 */
#define SRV_UPDATE_EXT     0x00C8	/* Received after ACK of 04B0 */
#define SRV_RECV_MESSAGE   0x00DC
#define SRV_END_OF_SEARCH  0x00A0
#define SRV_X2             0x00E6	/* Received after ACK of 021C */
#define SRV_GO_AWAY        0x00F0
#define SRV_TRY_AGAIN      0x00FA	/* Login failure returned after 03E8 */
#define SRV_SYS_DELIVERED_MESS 0x0104
#define SRV_INFO_REPLY     0x0118
#define SRV_EXT_INFO_REPLY 0x0122
#define SRV_STATUS_UPDATE  0x01A4
#define SRV_SYSTEM_MESSAGE 0x01C2
#define SRV_UPDATE         0x01E0
#define SRV_X1             0x021C	/* Received after 0212 when create ID*/
#define SRV_MULTI_PACKET   0x0212	/* Full / user / app info receive */
#define SRV_RND_USER_INFO  0x024E	/* Receive info on random user */
#define SRV_BAD_PASS       0x6400

typedef struct
{
  guint8 time[4];
  guint8 port[4];
  guint8 len[2];
} login_1, *LOGIN_1_PTR;

typedef struct
{
  guint8 X1[4];
  guint8 ip[4];
  guint8  X2[1];
  guint8  status[4];
  guint8 X3[4];
  guint8  X4[4];
  guint8 X5[4];
} login_2, *LOGIN_2_PTR;

#define LOGIN_X1_DEF 0x00000098
#define LOGIN_X2_DEF 0x04
#define LOGIN_X3_DEF 0x00000003
#define LOGIN_X4_DEF 0x00000000
//#define LOGIN_X5_DEF 0x00980000
#define LOGIN_X5_DEF 0x00980010


typedef struct
{
  guint8 uin[4];
  guint8 year[2];
  guint8 month;
  guint8 day;
  guint8 hour;
  guint8 minute;
  guint8 type[2];
  guint8 len[2];
} RECV_MESSAGE, *RECV_MESSAGE_PTR;

typedef struct
{
  guint8 uin[4];
  guint8 type[2]; 
  guint8 len[2];
} SIMPLE_MESSAGE, *SIMPLE_MESSAGE_PTR;

#endif /* __ICQ_H__ */
