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


#ifndef __TCP_H__
#define __TCP_H__

#define ICQ_CMDxTCP_START             0x07EE
#define ICQ_CMDxTCP_CANCEL            0x07D0
#define ICQ_CMDxTCP_ACK               0x07DA
#define ICQ_CMDxTCP_MSG               0x0001
#define ICQ_CMDxTCP_MULTI_MSG		  0x8001
#define ICQ_CMDxTCP_FILE              0x0003
#define ICQ_CMDxTCP_CHAT              0x0002
#define ICQ_CMDxTCP_URL               0x0004
#define ICQ_CMDxTCP_READxAWAYxMSG     0x03E8
#define ICQ_CMDxTCP_READxOCCxMSG      0x03E9
#define ICQ_CMDxTCP_READxNAxMSG       0x03EA
#define ICQ_CMDxTCP_READxDNDxMSG      0x03EB
#define ICQ_CMDxTCP_HANDSHAKE         0x03FF
#define ICQ_CMDxTCP_HANDSHAKE2        0x04FF
#define ICQ_CMDxTCP_HANDSHAKE3        0x02FF

#define ICQ_ACKxTCP_ONLINE            0x0000
#define ICQ_ACKxTCP_AWAY              0x0004
#define ICQ_ACKxTCP_NA                0x000E
#define ICQ_ACKxTCP_DND               0x000A
#define ICQ_ACKxTCP_OCC               0x0009
#define ICQ_ACKxTCP_REFUSE            0x0001

/*
#define TCP_FAILED -1
#define TCP_NOT_CONNECTED 0
#define TCP_CONNECTED 1
#define TCP_CONNECTED_CLIENT 2
#define TCP_CONNECTED_SERVER 3
*/

#define TCP_NOT_CONNECTED	0
#define TCP_CONNECTED		(1 << 0)
#define TCP_FAILED		(1 << 1)
#define TCP_CLIENT		(1 << 2)
#define TCP_SERVER		(1 << 3)
#define TCP_HANDSHAKE_WAIT	(1 << 4)

#define FONT_PLAIN                    0x00000000

int set_nonblock(int sock);

gint TCP_Ack(gint sock, guint16 cmd, gint seq);
void TCP_SendHelloPacket(gint sock);
void TCP_ProcessPacket(guint8* packet, gint packet_length, gint sock);
gint TCP_ReadPacket(gint sock);
gint TCP_Connect(guint32 ip, guint32 port);
gint TCP_SendMessage(guint32 uin, char* msg);
gint TCP_SendURL(guint32 uin, char* url, char *text);
gint TCP_GetAwayMessage(guint32 uin);

gint TCP_ChatServerHandshake(gint sock);
gint TCP_ChatClientHandshake(gint sock);
gint TCP_ChatReadClient(gint sock);
gint TCP_ChatReadServer(gint sock);
gint TCP_SendChatRequest(guint32 uin, gchar* text);
gint TCP_AcceptChat(guint32 uin);
gint TCP_RefuseChat(guint32 uin);
gint TCP_ChatSend(guint32 uin, gchar* text);
gint TCP_ConnectChat(guint32 port, guint32 uin);
gint TCP_TerminateChat(guint32 uin);
gint TCP_AcceptFile( int sock, int uin, guint32 seq);


#endif /* __TCP_H__ */

