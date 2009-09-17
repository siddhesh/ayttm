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

#ifndef __SEND_H__
#define __SEND_H__

int Connect_Remote(char *hostname, guint32 port);

void Send_Ack(gint seq);
void Send_BeginLogin(guint32 UIN, char *pass, guint32 ip, guint32 port);
void Send_FinishLogin();
void Send_KeepAlive();
void Send_GotMessages();
void Send_ContactList();
void Send_Message(guint32 uin, char *text);
void Send_AuthMessage(guint32 uin);
void Send_ChangeStatus(guint32 status);
void Send_InfoRequest(guint32 uin);
void Send_ExtInfoRequest(guint32 uin);
void Send_SearchRequest(char *email, char *nick, char *first, char *last);
void Send_RegisterNewUser(char *pass);
void Send_Disconnect();
void Send_URL(guint32 uin, char *url, char *text);
#endif				/* __SEND_H__ */
