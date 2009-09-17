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

#ifndef __RECEIVE_H__
#define __RECEIVE_H__

void Rec_Packet();
void Process_Packet(srv_net_icq_pak pak, guint32 len, guint16 cmd, guint16 ver,
	guint16 seq, guint32 uin);

void Rec_Multi_Packet(guint8 *pdata);
void Rec_Login(srv_net_icq_pak pak);
void Rec_X1(srv_net_icq_pak pak);
void Rec_X2(srv_net_icq_pak pak);
void Rec_Message(srv_net_icq_pak pak);
void Rec_SysDeliveredMess(srv_net_icq_pak pak);
void Rec_Info(srv_net_icq_pak pak);
void Rec_ExtInfo(srv_net_icq_pak pak);
void Rec_UserOffline(srv_net_icq_pak pak);
void Rec_UserOnline(srv_net_icq_pak pak);
void Rec_StatusUpdate(srv_net_icq_pak pak);
void Rec_TryAgain(srv_net_icq_pak pak);
void Rec_GoAway(srv_net_icq_pak pak);
void Rec_UserFound(srv_net_icq_pak pak);
void Rec_EndOfSearch(srv_net_icq_pak pak);
void Rec_AwayMessage(guint32 uin, char *msg);

#endif				/* __RECEIVE_H__ */
