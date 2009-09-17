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
/* FIXME - because there is a local file named config.h, we include
	the relative path to get the one we want
*/
#include "../../../config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib.h>

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include <windows.h>
#include <io.h>
#else				/* !_WIN32 */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifndef __MINGW32__
#include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include "icq.h"
#include "libicq.h"
#include "receive.h"
#include "send.h"
#include "util.h"
#include "tcp.h"

extern gint Verbose;
extern gint sok;
extern gboolean serv_mess[1024];
extern guint16 last_cmd[1024];
extern guint32 our_ip;
extern guint32 our_port;
extern guint32 last_recv_uin;
extern GList *Search_Results;
extern GList *open_sockets;
extern gboolean Done_Login;
extern guint32 Current_Status;
extern guint32 UIN;
extern gchar nickname[];

/******************************************
Dumps out the packet data for debugging
******************************************/

void Dump_Packet(srv_net_icq_pak pak)
{
	int i, len;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Dump_Packet");

	fprintf(stdout, "\nPacket contents: \n");
	fprintf(stdout, "The version was 0x%X\t", Chars_2_Word(pak.head.ver));
	fprintf(stdout, "\nThe SEQ was 0x%04X\t", Chars_2_Word(pak.head.seq));
	len = (sizeof(pak) - 2);
	fprintf(stdout, "The size was %d\n", len);
	for (i = 0; i < len; i++) {
		if (i % 24 == 0)
			printf("\n");
		if (i % 8 == 0)
			printf(" ");
		fprintf(stdout, "%02X ", (unsigned char)pak.data[i]);
	}
}

/******************************************
Handles packets that the server sends to us.
*******************************************/
void Rec_Packet()
{
	srv_net_icq_pak pak;
	int s, cmd;
	gchar errmsg[255];
		     /*** MIZHI 04162001 */

	s = SOCKREAD(sok, &pak.head.ver, sizeof(pak) - 2);
	if (s <= 0) {
		return;
	}

	cmd = Chars_2_Word(pak.head.cmd);

/*** MIZHI 04162001 */
	sprintf(errmsg, "LIBICQ> Rec_Packet - ver: %04X, seq: %04X, cmd: %04X",
		Chars_2_Word(pak.head.ver), Chars_2_Word(pak.head.seq), cmd);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	if ((serv_mess[Chars_2_Word(pak.head.seq)]) &&
		(cmd != SRV_NEW_UIN) &&
		(cmd != SRV_MULTI_PACKET) &&
		(cmd != SRV_SYS_DELIVERED_MESS) &&
		(Chars_2_Word(pak.head.seq) != 0)) {
		if (cmd != SRV_ACK) {	/* ACKs don't matter */
			if (Verbose & ICQ_VERB_ERR)
				sprintf(errmsg, " - Ignored message cmd 0x%04x",
					cmd);
			ICQ_Debug(ICQ_VERB_WARN, errmsg);
			if (Verbose & ICQ_VERB_INFO)
				Dump_Packet(pak);

			Send_Ack(Chars_2_Word(pak.head.seq));
			return;
		}
	}

	if (Chars_2_Word(pak.head.seq) == 0) {
		if (Verbose & ICQ_VERB_INFO) {
			ICQ_Debug(ICQ_VERB_ERR,
				"LIBICQ> Packet sequence zero - winging it.");
			/* MTB: 1999/05/24 */
//      return;
		}
	}

	if (cmd != SRV_ACK) {
		serv_mess[Chars_2_Word(pak.head.seq)] = TRUE;
		Send_Ack(Chars_2_Word(pak.head.seq));
	}

	Process_Packet(pak, s - (sizeof(pak.head) - 2),
		cmd, Chars_2_Word(pak.head.ver),
		Chars_2_Word(pak.head.seq), Chars_2_DW(pak.head.UIN));
}

void Process_Packet(srv_net_icq_pak pak, guint32 len, guint16 cmd, guint16 ver,
	guint16 seq, guint32 uin)
{
	ICQ_Debug(ICQ_VERB_INFO, " - Process_Packet");

	switch (Chars_2_Word(pak.head.cmd)) {
	case SRV_ACK:
		if (Verbose & ICQ_VERB_INFO)
			printf(" - The server ack'd seq #%04X (cmd:%04X)",
				Chars_2_Word(pak.head.seq),
				last_cmd[Chars_2_Word(pak.head.seq)]);
		break;
	case SRV_MULTI_PACKET:
		Rec_Multi_Packet(pak.data);
		break;
	case SRV_NEW_UIN:
		printf(" - The new UIN is %d!", Chars_2_DW(&pak.data[2]));
		break;
	case SRV_LOGIN_REPLY:
		Rec_Login(pak);
		break;
	case SRV_RECV_MESSAGE:
		Rec_Message(pak);
		break;
	case SRV_X1:
		Rec_X1(pak);
		break;
	case SRV_X2:
		Rec_X2(pak);
		break;
	case SRV_INFO_REPLY:
		Rec_Info(pak);
		break;
	case SRV_EXT_INFO_REPLY:
		Rec_ExtInfo(pak);
		break;
	case SRV_USER_OFFLINE:
		Rec_UserOffline(pak);
		break;
	case SRV_USER_ONLINE:
		Rec_UserOnline(pak);
		break;
	case SRV_STATUS_UPDATE:
		Rec_StatusUpdate(pak);
		break;
	case SRV_TRY_AGAIN:
		Rec_TryAgain(pak);
		break;
	case SRV_GO_AWAY:
		Rec_GoAway(pak);
		break;
	case SRV_USER_FOUND:
		Rec_UserFound(pak);
		break;
	case SRV_END_OF_SEARCH:
		Rec_EndOfSearch(pak);
		break;
	case SRV_SYS_DELIVERED_MESS:
		Rec_SysDeliveredMess(pak);
		break;

		/* Added by Michael on 1999/02/12 */
	case SRV_FORCE_DISCONNECT:
		Rec_GoAway(pak);
		break;
	case SRV_BAD_PASS:
		Rec_GoAway(pak);
		break;
	case 0x7108:
		Rec_GoAway(pak);
		break;
	case 0x0064:
		Rec_GoAway(pak);
		break;

	default:		/* commands we dont handle yet */
		fprintf(stderr, " - Unknown command: 0x%04X",
			Chars_2_Word(pak.head.cmd));
		if (Verbose & ICQ_VERB_ERR)
			Dump_Packet(pak);

		/* fake like we know what we're doing */
		if (Chars_2_Word(pak.head.seq) != 0)
			Send_Ack(Chars_2_Word(pak.head.seq));
		else
			Send_Ack(1);	// so we do something???
		break;
	}
}

void Rec_Multi_Packet(guint8 *pdata)
{
	int num_pack, i;
	int len;
	guint8 *j;
	srv_net_icq_pak pak;
	num_pack = (unsigned char)pdata[0];
	j = pdata;
	j++;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_Multi_Packet(pdata)");

	for (i = 0; i < num_pack; i++) {
		len = Chars_2_Word(j);
		pak = *(srv_net_icq_pak *)j;
		j += 2;
		Process_Packet(pak, len - sizeof(pak.head),
			Chars_2_Word(pak.head.cmd), Chars_2_Word(pak.head.ver),
			Chars_2_Word(pak.head.seq), Chars_2_Word(pak.head.UIN));
		j += len;
	}
}

void Rec_Login(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_Login");

	Send_Ack(Chars_2_Word(pak.head.seq));

	our_ip = Chars_2_DW(&pak.data[0]);

	if (Verbose & ICQ_VERB_INFO)
		printf(" - successful!");

	Send_FinishLogin();
	Send_ContactList();
	Send_ChangeStatus(Current_Status);
	Send_InfoRequest(UIN);

	if (event[EVENT_LOGIN] != NULL)
		(*event[EVENT_LOGIN]) (NULL);
}

void Rec_X1(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_X1");

	if (Verbose & ICQ_VERB_INFO)
		printf(" - Acknowleged SRV_X1 0x021C");
	Send_Ack(Chars_2_Word(pak.head.seq));
}

void Rec_X2(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_X2");

	Send_Ack(Chars_2_Word(pak.head.seq));
	if (Verbose & ICQ_VERB_INFO)
		printf(" - Acknowleged SRV_X2 0x00E6");
	Done_Login = TRUE;
	Send_GotMessages();
}

void Rec_Message(srv_net_icq_pak pak)
{
	RECV_MESSAGE_PTR r_mesg;
	CLIENT_MESSAGE c_mesg;
	char *data, *tmp;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_Message");

	Send_Ack(Chars_2_Word(pak.head.seq));

	r_mesg = (RECV_MESSAGE_PTR) pak.data;
	c_mesg.uin = Chars_2_DW(r_mesg->uin);
	c_mesg.month = (int)(r_mesg->month);
	c_mesg.day = (int)(r_mesg->day);
	c_mesg.year = Chars_2_Word(r_mesg->year);
	c_mesg.hour = (int)(r_mesg->hour);
	c_mesg.minute = (int)(r_mesg->minute);
	c_mesg.type = Chars_2_Word(r_mesg->type);
	c_mesg.len = Chars_2_Word(r_mesg->len);

	data = (char *)(r_mesg->len + 2);

	if (c_mesg.type == URL_MESS) {
		tmp = strchr(data, '\xFE');
		if (!tmp)
			return;
		*tmp = 0;
		c_mesg.msg = data;

		data = ++tmp;
		c_mesg.url = data;
	} else {
		c_mesg.msg = data;
	}

	last_recv_uin = Chars_2_DW(r_mesg->uin);

	if (event[EVENT_MESSAGE] != NULL)
		(*event[EVENT_MESSAGE]) (&c_mesg);
}

void Rec_AwayMessage(guint32 uin, char *msg)
{
	CLIENT_MESSAGE c_mesg;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_AwayMessage");

	c_mesg.uin = (guint32) uin;
	c_mesg.month = 0;
	c_mesg.day = 0;
	c_mesg.year = 0;
	c_mesg.hour = 0;
	c_mesg.minute = 0;
	c_mesg.type = (guint16) AWAY_MESS;
	c_mesg.len = (guint16) (sizeof(msg) + 1);
	c_mesg.msg = msg;

	if (event[EVENT_MESSAGE] != NULL)
		(*event[EVENT_MESSAGE]) (&c_mesg);
}

void Rec_SysDeliveredMess(srv_net_icq_pak pak)
{
	SIMPLE_MESSAGE_PTR s_mesg;
	CLIENT_MESSAGE c_mesg;
	char *data, *tmp;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_SysDeliveredMess");

	Send_Ack(Chars_2_Word(pak.head.seq));

	s_mesg = (SIMPLE_MESSAGE_PTR) pak.data;
	last_recv_uin = Chars_2_DW(s_mesg->uin);

	s_mesg = (SIMPLE_MESSAGE_PTR) pak.data;
	c_mesg.uin = Chars_2_DW(s_mesg->uin);
	c_mesg.month = 0;
	c_mesg.day = 0;
	c_mesg.year = 0;	/*  will fill these in later :) */
	c_mesg.hour = 0;
	c_mesg.minute = 0;
	c_mesg.type = Chars_2_Word(s_mesg->type);
	c_mesg.len = Chars_2_Word(s_mesg->len);

	data = (char *)(s_mesg->len + 2);

	if (c_mesg.type == URL_MESS) {
		tmp = strchr(data, '\xFE');
		if (!tmp)
			return;
		*tmp = 0;
		c_mesg.msg = data;

		data = ++tmp;
		c_mesg.url = data;
	} else {
		c_mesg.msg = data;
	}

	if (event[EVENT_MESSAGE] != NULL)
		(*event[EVENT_MESSAGE]) (&c_mesg);
}

void Rec_Info(srv_net_icq_pak pak)
{
	USER_INFO info;
	char *tmp;
	int len;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_Info");

	Send_Ack(Chars_2_Word(pak.head.seq));

	info.uin = Chars_2_DW(&pak.data[0]);

	len = Chars_2_Word(&pak.data[4]);
	strcpy(info.nick, &pak.data[6]);

	tmp = &pak.data[len + 6];
	len = Chars_2_Word(tmp);
	strcpy(info.first, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(info.last, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(info.email, tmp + 2);
	tmp += len + 2;

	info.auth_required = *tmp;

	if (info.uin == UIN)
		strcpy(nickname, info.nick);

	if (event[EVENT_INFO] != NULL)
		(*event[EVENT_INFO]) (&info);
}

void Rec_ExtInfo(srv_net_icq_pak pak)
{
	USER_EXT_INFO extinfo;
	unsigned char *tmp;
	int len;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_ExtInfo");

	Send_Ack(Chars_2_Word(pak.head.seq));

	extinfo.uin = Chars_2_DW(&pak.data[0]);

	len = Chars_2_Word(&pak.data[4]);
	strcpy(extinfo.city, &pak.data[6]);
	tmp = &pak.data[6 + len];

	tmp += 2;		/* Bypass COUNTRY_CODE */
	tmp += 1;		/* Bypass COUNTRY_STATUS */

	len = Chars_2_Word(tmp);
	strcpy(extinfo.state, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	if (Chars_2_Word(tmp) != 0xffff)
		sprintf(extinfo.age, "%d", Chars_2_Word(tmp));
	else
		strcpy(extinfo.age, "Not Entered");
	tmp += 2;

	if (tmp[0] == 2)
		strcpy(extinfo.sex, "Male");
	else if (tmp[0] == 1)
		strcpy(extinfo.sex, "Female");
	else
		strcpy(extinfo.sex, "Not Specified");
	tmp += 1;

	len = Chars_2_Word(tmp);
	strcpy(extinfo.phone, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(extinfo.url, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(extinfo.about, tmp + 2);

/*win32--#warning "FIXME: country has to be copied as well!"*/

	if (event[EVENT_EXT_INFO] != NULL)
		(*event[EVENT_EXT_INFO]) (&extinfo);
}

void Rec_UserOffline(srv_net_icq_pak pak)
{
	USER_UPDATE user_update;
	int cindex;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_UserOffline");

	Send_Ack(Chars_2_Word(pak.head.seq));

	user_update.uin = pak.data[3];
	user_update.uin <<= 8;
	user_update.uin += pak.data[2];
	user_update.uin <<= 8;
	user_update.uin += pak.data[1];
	user_update.uin <<= 8;
	user_update.uin += pak.data[0];

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == user_update.uin)
			break;

	if (cindex <= Num_Contacts) {
		if (Contacts[cindex].tcp_sok > 0) {
			open_sockets = g_list_remove(open_sockets,
				(gpointer) Contacts[cindex].tcp_sok);
			close(Contacts[cindex].tcp_sok);
		}

		if (Contacts[cindex].chat_sok > 0) {
			open_sockets = g_list_remove(open_sockets,
				(gpointer) Contacts[cindex].chat_sok);
			close(Contacts[cindex].chat_sok);
		}

		Contacts[cindex].status = STATUS_OFFLINE;
		Contacts[cindex].last_time = time(NULL);
		Contacts[cindex].tcp_sok = 0;
		Contacts[cindex].tcp_port = 0;
		Contacts[cindex].tcp_status = TCP_NOT_CONNECTED;
		Contacts[cindex].chat_sok = 0;
		Contacts[cindex].chat_port = 0;
		Contacts[cindex].chat_status = TCP_NOT_CONNECTED;
		strcpy(user_update.nick, Contacts[cindex].nick);

	}

	user_update.status = STATUS_OFFLINE;

	if (event[EVENT_OFFLINE] != NULL)
		(*event[EVENT_OFFLINE]) (&user_update);
}

void Rec_UserOnline(srv_net_icq_pak pak)
{
	gchar errmsg[255];
		     /*** MIZHI 04162001 */
	USER_UPDATE user_update;
	int cindex;

/*win32--  #warning FIXME: Rec_UserOnline should send notification back to gicq*/

	Send_Ack(Chars_2_Word(pak.head.seq));

	user_update.uin = Chars_2_DW(&pak.data[0]);
	user_update.status = Chars_2_DW(&pak.data[17]);

/*** MIZHI 04162001 */
	sprintf(errmsg, "LIBICQ> Rec_UserOnline(%d)", user_update.uin);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == user_update.uin)
			break;

	if (cindex <= Num_Contacts) {
		long ll;
		ll = 0;
		memcpy(&ll, &pak.data[4], 4);
		Contacts[cindex].status = user_update.status;
		Contacts[cindex].current_ip = ntohl(ll);
		Contacts[cindex].tcp_port = Chars_2_DW(&pak.data[8]);
		Contacts[cindex].last_time = time(NULL);
		strcpy(user_update.nick, Contacts[cindex].nick);
	}

	if (event[EVENT_ONLINE] != NULL)
		(*event[EVENT_ONLINE]) (&user_update);
}

void Rec_StatusUpdate(srv_net_icq_pak pak)
{
	USER_UPDATE user_update;
	int index;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_StatusUpdate");

	Send_Ack(Chars_2_Word(pak.head.seq));

	user_update.uin = pak.data[3];
	user_update.uin <<= 8;
	user_update.uin += pak.data[2];
	user_update.uin <<= 8;
	user_update.uin += pak.data[1];
	user_update.uin <<= 8;
	user_update.uin += pak.data[0];

	user_update.status = pak.data[7];
	user_update.status <<= 8;
	user_update.status += pak.data[6];
	user_update.status <<= 8;
	user_update.status += pak.data[5];
	user_update.status <<= 8;
	user_update.status += pak.data[4];

	for (index = 0; index < Num_Contacts; index++)
		if (Contacts[index].uin == user_update.uin)
			break;

	if (index <= Num_Contacts) {
		Contacts[index].status = user_update.status;
		strcpy(user_update.nick, Contacts[index].nick);
	}

	if (event[EVENT_STATUS_UPDATE] != NULL)
		(*event[EVENT_STATUS_UPDATE]) (&user_update);

}

void ClearMessages()
{
	int i;

	for (i = 0; i < 1024; i++)
		serv_mess[i] = FALSE;
}

void Rec_TryAgain(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_TryAgain");

	if (Verbose & ICQ_VERB_ERR)
		fprintf(stderr, " - Server is busy, trying again...");

	Send_BeginLogin(UIN, &passwd[0], our_ip, our_port);

	ClearMessages();
}

void Rec_GoAway(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_GoAway ... ");

	/* MTB: 1999/02/12 Respond differently depending on the signal */

	switch (Chars_2_Word(pak.head.cmd)) {
	case SRV_GO_AWAY:
		fprintf(stderr, "Server told us to go away.");
		ICQ_Disconnect();
		ICQ_Connect();
		/* MTB: 1999/02/12 Send "restart logging in" message back to client */
		if (event[EVENT_OFFLINE] != NULL)
			(*event[EVENT_OFFLINE]) ((void *)SRV_GO_AWAY);
		break;
	case SRV_FORCE_DISCONNECT:
		fprintf(stderr, "Server doesn't think we're connected.");
		if (event[EVENT_OFFLINE] != NULL)
			(*event[EVENT_OFFLINE]) ((void *)SRV_FORCE_DISCONNECT);
		break;
	case 0x7108:
		fprintf(stderr, "SRV_GO_TO_HELL (0x7108) -Mike");
		if (event[EVENT_OFFLINE] != NULL)
			(*event[EVENT_OFFLINE]) ((void *)0x7108);
		break;
	case 0x0064:
		fprintf(stderr, "SRV_WHAT_THE_HELL? (0x0064) -Mike");
		if (event[EVENT_OFFLINE] != NULL)
			(*event[EVENT_OFFLINE]) ((void *)0x0064);
		break;
	default:
		fprintf(stderr, "This may be because of a bad password.");
		/* if(event[EVENT_OFFLINE] != NULL) (*event[EVENT_OFFLINE])(NULL); */
		fprintf(stderr, " - (cmd 0x%04X)", Chars_2_Word(pak.head.cmd));
		break;
	}

	/* Mike 1999/02/12: We need to clear the stored message history */

	ClearMessages();
}

void Rec_UserFound(srv_net_icq_pak pak)
{
	SEARCH_RESULT_PTR current_search_result;
	char *tmp;
	int len;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_UserFound");

	current_search_result = g_malloc(sizeof(SEARCH_RESULT));
	current_search_result->uin = Chars_2_DW(&pak.data[0]);

	len = Chars_2_Word(&pak.data[4]);
	strcpy(current_search_result->nick, &pak.data[6]);

	tmp = &pak.data[len + 6];
	len = Chars_2_Word(tmp);
	strcpy(current_search_result->first, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(current_search_result->last, tmp + 2);
	tmp += len + 2;

	len = Chars_2_Word(tmp);
	strcpy(current_search_result->email, tmp + 2);
	tmp += len + 2;

	current_search_result->auth_required = *tmp;

	Search_Results =
		g_list_append(Search_Results, (gpointer) current_search_result);
}

void Rec_EndOfSearch(srv_net_icq_pak pak)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Rec_EndOfSearch");

	Send_Ack(Chars_2_Word(pak.head.seq));

	if (event[EVENT_SEARCH_RESULTS] != NULL)
		(*event[EVENT_SEARCH_RESULTS]) ((void *)Search_Results);
}
