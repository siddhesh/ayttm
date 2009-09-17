/* This program is free software; you can redistribute it and/or modify
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <glib.h>
#include <string.h>

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include <winsock2.h>
#include <io.h>
#else				/* !_WIN32 */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifndef __MINGW32__
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#endif

#include "icq.h"
#include "libicq.h"
#include "receive.h"
#include "tcp.h"
#include "libproxy.h"
#include "plugin_api.h"

const guint32 LOCALHOST = 0x0100007F;

extern gint Verbose;
extern guint32 our_ip;
extern guint32 our_port;
extern gint tcp_sok;
extern guint16 seq_num;
extern Contact_Member Contacts[];
extern gint Num_Contacts;
extern guint32 Current_Status;
extern GList *open_sockets;
extern guint32 UIN;
extern gchar nickname[];

/* #define DEBUG */

void packet_print(guint8 *packet, gint size)
{
#ifdef DEBUG
	int cx;

	printf("\nPacket:\n");

	for (cx = 0; cx < size; cx++) {
		if (cx % 16 == 0 && cx)
			printf("  \n");
		printf("%02x ", packet[cx]);
	}
	printf("\n");
#endif
}

/* might need fcntl(F_SETFL), or ioctl(FIONBIO) */
/* Posix.1g says fcntl */

#ifdef O_NONBLOCK

int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_block(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
}

#elif defined( _WIN32 )

int set_nonblock(int fd)
{
#ifdef __MINGW32__
	long yes = 1;
#else
	int yes = 1;
#endif
	return ioctlsocket(fd, FIONBIO, &yes);
}

int set_block(int fd)
{
#ifdef __MINGW32__
	long yes = 0;
#else
	int yes = 0;
#endif
	return ioctlsocket(fd, FIONBIO, &yes);
}

#else

int set_nonblock(int fd)
{
	int yes = 1;
	return ioctl(fd, FIONBIO, &yes);
}

int set_block(int fd)
{
	int yes = 0;
	return ioctl(fd, FIONBIO, &yes);
}

#endif

gint TCP_Ack(gint sock, guint16 cmd, gint seq)
{
	char buffer[1024];
	guint16 intsize;
	char *sent_message;

	typedef struct {
		guint8 uin1[4];
		guint8 version[2];
		guint8 command[2];
		guint8 zero[2];
		guint8 uin2[4];
		guint8 cmd[2];
		guint8 message_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 ip_real[4];
		guint8 port[4];
		guint8 junk;
		guint8 status[4];
		guint8 seq[4];
	} tcp_tail;

	tcp_head pack_head;
	tcp_tail pack_tail;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_Ack(%d, %04X, %d)", sock, cmd, seq);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	sent_message = "";
/*
   if( Current_Status != STATUS_ONLINE && Current_Status != STATUS_FREE_CHAT )
      sent_message = Away_Message;
*/

	DW_2_Chars(pack_head.uin1, UIN);
	Word_2_Chars(pack_head.version, 0x0003);
	Word_2_Chars(pack_head.command, ICQ_CMDxTCP_ACK);
	Word_2_Chars(pack_head.zero, 0x0000);
	DW_2_Chars(pack_head.uin2, UIN);
	DW_2_Chars(pack_head.cmd, cmd);
	DW_2_Chars(pack_head.message_length, strlen(sent_message) + 1);

	DW_2_Chars(pack_tail.ip, our_ip);
	DW_2_Chars(pack_tail.ip_real, LOCALHOST);
	DW_2_Chars(pack_tail.port, our_port);
	pack_tail.junk = 0x04;
	DW_2_Chars(pack_tail.seq, seq);

	switch (Current_Status) {
	case STATUS_ONLINE:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_ONLINE);
		break;
	case STATUS_AWAY:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_AWAY);
		break;
	case STATUS_DND:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_DND);
		break;
	case STATUS_OCCUPIED:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_OCC);
		break;
	case STATUS_NA:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_NA);
		break;
	case STATUS_INVISIBLE:
		DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_REFUSE);
		break;
	}

	if (sock != -1) {
		char mybuffer[2];
		intsize =
			sizeof(tcp_head) + sizeof(tcp_tail) +
			strlen(sent_message) + 1;
		Word_2_Chars(mybuffer, intsize);
		memcpy(&buffer[0], mybuffer, 2);
		memcpy(&buffer[2], &pack_head, sizeof(pack_head));
		memcpy(&buffer[2 + sizeof(pack_head)], sent_message,
			strlen(sent_message) + 1);
		memcpy(&buffer[2 + sizeof(pack_head) + strlen(sent_message) +
				1], &pack_tail, sizeof(pack_tail));
		write(sock, buffer, intsize + 2);
		packet_print(buffer, intsize + 2);
	} else {
		return -1;
	}

	return 1;
}

void TCP_SendHelloPacket(gint sock)
{

	struct {
		guint8 size[2];
		guint8 command;
		guint8 version[4];
		guint8 szero[4];
		guint8 uin[4];
		guint8 ip[4];
		guint8 real_ip[4];
		guint8 four;
		guint8 port[4];
	} hello_packet;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_SendHelloPacket");

	Word_2_Chars(hello_packet.size, 0x001A);
	hello_packet.command = 0xFF;
	DW_2_Chars(hello_packet.version, 0x00000003);
	DW_2_Chars(hello_packet.szero, 0x00000000);
	DW_2_Chars(hello_packet.uin, UIN);
	DW_2_Chars(hello_packet.ip, LOCALHOST);
	DW_2_Chars(hello_packet.real_ip, LOCALHOST);
	hello_packet.four = 0x04;
	DW_2_Chars(hello_packet.port, (guint32) our_port);

	write(sock, &hello_packet, sizeof(hello_packet));
}

void TCP_ProcessPacket(guint8 *packet, gint packet_length, gint sock)
{
	CLIENT_MESSAGE c_mesg;
	int chat_sock;
	int cx;

	typedef struct {
		guint32 uin1;
		guint16 version;
		guint16 command;
		guint16 zero;
		guint32 uin2;
		guint16 cmd;
		guint16 message_length;
	} tcp_head;

	typedef struct {
		guint32 ip_sender;
		guint32 ip_local;
		guint32 port;
		guint8 junk;
		guint32 status;
		guint32 chat_port;
		guint32 seq;
	} tcp_tail;

	typedef struct {
		guint16 name_len;
		char *name;
		guint32 size;
		guint32 zero;
		guint32 seq;
	} tcp_file;

	char *message, *data, *tmp;
	// char *away_message; UNUSED right now
	tcp_head pack_head;
	tcp_tail pack_tail;

	gint cindex;
	gint size;

	guint32 i;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_ProcessPacket(%p, %d, %d)", packet,
		packet_length, sock);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	if (packet[0] == 0xFF)	/* 0xFF means it's just a "Hello" packet; ignore */
		return;

	memcpy(&pack_head.uin1, packet, 4);
	pack_head.uin1 = GUINT32_FROM_LE(pack_head.uin1);
	memcpy(&pack_head.version, (packet + 4), 2);
	pack_head.version = GUINT16_FROM_LE(pack_head.version);
	memcpy(&pack_head.command, (packet + 6), 2);
	pack_head.command = GUINT16_FROM_LE(pack_head.command);
	memcpy(&pack_head.zero, (packet + 8), 2);
	memcpy(&pack_head.uin2, (packet + 10), 4);
	pack_head.uin2 = GUINT32_FROM_LE(pack_head.uin2);
	memcpy(&pack_head.cmd, (packet + 14), 2);
	pack_head.cmd = GUINT16_FROM_LE(pack_head.cmd);
	memcpy(&pack_head.message_length, (packet + 16), 2);
	pack_head.message_length = GUINT16_FROM_LE(pack_head.message_length);

	message = (char *)g_malloc0(pack_head.message_length);
	memcpy(message, (packet + 18), pack_head.message_length);
	fprintf(stderr, "CMD = %x COMMAND = %x \n", pack_head.cmd,
		pack_head.command);
	write(1, message, pack_head.message_length);

	memcpy(&pack_tail.ip_sender, (packet + 18 + pack_head.message_length),
		4);
	memcpy(&pack_tail.ip_local,
		(packet + 18 + pack_head.message_length + 4), 4);
	memcpy(&pack_tail.port, (packet + 18 + pack_head.message_length + 8),
		4);
	memcpy(&pack_tail.status, (packet + 18 + pack_head.message_length + 13),
		4);
	memcpy(&pack_tail.seq, (packet + packet_length - 4), 4);
	memcpy(&pack_tail.chat_port, (packet + packet_length - 8), 4);

	size = pack_head.message_length + 18 + 21;

	i = pack_tail.ip_sender;
	pack_tail.ip_sender =
		((i << 24) | ((i & 0xff00) << 8) | ((i & 0xff0000) >> 8) | (i >>
			24));
	i = pack_tail.ip_local;
	pack_tail.ip_local =
		((i << 24) | ((i & 0xff00) << 8) | ((i & 0xff0000) >> 8) | (i >>
			24));

	if (pack_head.command == ICQ_CMDxTCP_START) {
		switch (pack_head.cmd) {
		case ICQ_CMDxTCP_MSG:
		case ICQ_CMDxTCP_MULTI_MSG:
			c_mesg.uin = pack_head.uin1;
			c_mesg.year = 0;
			c_mesg.month = 0;
			c_mesg.day = 0;
			c_mesg.hour = 0;
			c_mesg.minute = 0;
			c_mesg.type = MSG_MESS;
			c_mesg.len = strlen(message) + 1;
			c_mesg.msg = message;

			if (Verbose & ICQ_VERB_INFO)
				printf("\nTCP_ProcessPacket(): Received message through tcp");

			TCP_Ack(sock, pack_head.cmd, pack_tail.seq);

			if (event[EVENT_MESSAGE] != NULL)
				(*event[EVENT_MESSAGE]) (&c_mesg);

			break;

		case ICQ_CMDxTCP_URL:
			c_mesg.uin = pack_head.uin1;
			c_mesg.year = 0;
			c_mesg.month = 0;
			c_mesg.day = 0;
			c_mesg.hour = 0;
			c_mesg.minute = 0;
			c_mesg.type = URL_MESS;
			c_mesg.len = strlen(message) + 1;

			data = message;
			tmp = (char *)strchr(data, '\xfe');
			if (!tmp)
				return;
			*tmp = 0;
			c_mesg.msg = data;

			data = ++tmp;
			c_mesg.url = data;
			if (Verbose & ICQ_VERB_INFO)
				printf("\nTCP_ProcessPacket(): Received URL through tcp");

			TCP_Ack(sock, pack_head.cmd, pack_tail.seq);

			if (event[EVENT_MESSAGE] != NULL)
				(*event[EVENT_MESSAGE]) (&c_mesg);

		case ICQ_CMDxTCP_READxAWAYxMSG:
		case ICQ_CMDxTCP_READxOCCxMSG:
		case ICQ_CMDxTCP_READxDNDxMSG:
		case ICQ_CMDxTCP_READxNAxMSG:
			for (cindex = 0; cindex < Num_Contacts; cindex++)
				if (Contacts[cindex].uin == pack_head.uin2)
					break;

			if (Current_Status != STATUS_ONLINE
				&& Current_Status != STATUS_FREE_CHAT
				&& cindex != Num_Contacts)
				TCP_Ack(sock, ICQ_CMDxTCP_READxAWAYxMSG,
					pack_tail.seq);

			break;

		case ICQ_CMDxTCP_CHAT:
			if (Verbose & ICQ_VERB_INFO)
				printf("\nReceived chat request");

			c_mesg.uin = pack_head.uin1;
			c_mesg.year = 0;
			c_mesg.month = 0;
			c_mesg.day = 0;
			c_mesg.hour = 0;
			c_mesg.minute = 0;
			c_mesg.type = CHAT_MESS;
			c_mesg.len = strlen(message) + 1;
			c_mesg.msg = message;

			if (event[EVENT_MESSAGE] != NULL)
				(*event[EVENT_MESSAGE]) (&c_mesg);

			break;

		case ICQ_CMDxTCP_FILE:
			if (Verbose & ICQ_VERB_INFO)
				printf("\nReceived file transfer request");
			c_mesg.uin = pack_head.uin1;
			c_mesg.year = 0;
			c_mesg.month = 0;
			c_mesg.day = 0;
			c_mesg.hour = 0;
			c_mesg.minute = 0;
			c_mesg.type = FILE_MESS;
			c_mesg.len = strlen(message) + 1;
			c_mesg.msg = message;

			{
				size += 2;
				c_mesg.filename = packet + size;
				fprintf(stderr, "Got file name of %s\n",
					c_mesg.filename);
				size += strlen(c_mesg.filename);
				c_mesg.file_size = *((guint32 *)packet + size);
				c_mesg.file_size =
					GUINT32_FROM_LE(c_mesg.file_size);
				size += 4;

				c_mesg.seq =
					GUINT32_FROM_LE(*((guint32 *)packet +
						size));
			}

			if (event[EVENT_MESSAGE] != NULL)
				(*event[EVENT_MESSAGE]) (&c_mesg);
			break;
		}
	}

	if (pack_head.command == ICQ_CMDxTCP_ACK) {
		switch (pack_head.cmd) {
		case ICQ_CMDxTCP_MSG:
			if (Verbose & ICQ_VERB_INFO)
				printf("\nTCP_ProcessPacket(): Message sent successfully - seq = %d", pack_tail.seq);
			break;

		case ICQ_CMDxTCP_URL:
			if (Verbose & ICQ_VERB_INFO)
				printf("\nTCP_ProcessPacket(): URL sent successfully");
			break;

		case ICQ_CMDxTCP_READxAWAYxMSG:
		case ICQ_CMDxTCP_READxOCCxMSG:
		case ICQ_CMDxTCP_READxDNDxMSG:
		case ICQ_CMDxTCP_READxNAxMSG:
			for (cx = 0; cx < Num_Contacts; cx++)
				if (Contacts[cx].uin == pack_head.uin2)
					break;

			if (pack_tail.status == ICQ_ACKxTCP_AWAY ||
				pack_tail.status == ICQ_ACKxTCP_NA ||
				pack_tail.status == ICQ_ACKxTCP_DND ||
				pack_tail.status == ICQ_ACKxTCP_OCC) {
				Rec_AwayMessage(Contacts[cx].uin, message);
			}
			break;

		case ICQ_CMDxTCP_CHAT:
			if (pack_tail.chat_port > 0)
				chat_sock =
					TCP_ConnectChat(pack_tail.chat_port,
					pack_head.uin1);
			break;

		case ICQ_CMDxTCP_FILE:
			if (Verbose & ICQ_VERB_INFO)
				printf("Received file transfer ack\n");
			break;
		}
	}

	if (pack_head.command == ICQ_CMDxTCP_CANCEL) {
		switch (pack_head.cmd) {
		case ICQ_CMDxTCP_CHAT:
			if (Verbose & ICQ_VERB_INFO)
				printf("Chat request cancelled\n");
			break;

		case ICQ_CMDxTCP_FILE:
			if (Verbose & ICQ_VERB_INFO)
				printf("File transfer cancelled\n");
			break;
		}
	}

	g_free(message);
}

gint TCP_ReadPacket(gint sock)
{
	guint32 uin;
	gint cindex;
	guint16 packet_size;
	gint real_size, addr_size;
	guint8 *packet;
	struct sockaddr addr;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_ReadPacket");

	if (sock == tcp_sok) {
		addr_size = sizeof(struct sockaddr_in);
		sock = accept(sock, &addr, &addr_size);
		set_nonblock(sock);
		open_sockets = g_list_append(open_sockets, (gpointer) sock);
	}

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].tcp_sok == sock)
			break;

	if (recv(sock, &packet_size, 2, MSG_PEEK) <= 0) {
#if defined( _WIN32 )
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			return 1;
#else
		if (errno == EWOULDBLOCK)
			return 1;
#endif

		if (cindex < Num_Contacts) {
			Contacts[cindex].tcp_sok = 0;
			Contacts[cindex].tcp_status = TCP_NOT_CONNECTED;
		}

		open_sockets = g_list_remove(open_sockets, (gpointer) sock);
		close(sock);
		return TRUE;
	}
	packet_size = GUINT16_FROM_LE(packet_size);

	fprintf(stderr, "Packet Size = %d\n", packet_size);

	packet = (guint8 *)g_malloc(packet_size + 2);
	real_size = recv(sock, packet, packet_size + 2, MSG_PEEK);
	fprintf(stderr, "real_size = %d\n", real_size);

	if (real_size < packet_size) {

		if (real_size >= 0 || (real_size == -1 &&
#if defined( _WIN32 )
				WSAGetLastError() == WSAEWOULDBLOCK))
#else
				errno == EWOULDBLOCK))
#endif
		{
			return TRUE;
		}

		if (cindex != Num_Contacts) {
			Contacts[cindex].tcp_sok = 0;
			Contacts[cindex].tcp_status = TCP_NOT_CONNECTED;
		}

		open_sockets = g_list_remove(open_sockets, (gpointer) sock);
		close(sock);
		return TRUE;
	}

	recv(sock, packet, packet_size + 2, 0);

	memcpy(&uin, (packet + 11), 4);

	for (cindex = 0; cindex < Num_Contacts; cindex++) {
		if (Contacts[cindex].uin == uin) {
			Contacts[cindex].tcp_sok = sock;
			Contacts[cindex].tcp_status |= TCP_CONNECTED;
			break;
		}
	}

	if (cindex == Num_Contacts) {
		Contacts[Num_Contacts].uin = uin;
		Contacts[Num_Contacts].status = STATUS_NOT_IN_LIST;
		Contacts[Num_Contacts].last_time = -1L;
		Contacts[Num_Contacts].current_ip = -1L;
		Contacts[Num_Contacts].tcp_sok = sock;
		Contacts[Num_Contacts].tcp_port = 0;
		Contacts[Num_Contacts].tcp_status |= TCP_CONNECTED;
		Contacts[Num_Contacts].chat_sok = 0;
		Contacts[Num_Contacts].chat_port = 0;
		Contacts[Num_Contacts].chat_status = TCP_NOT_CONNECTED;
		Contacts[Num_Contacts].chat_active = FALSE;
		Contacts[Num_Contacts].chat_active2 = FALSE;
		sprintf(Contacts[Num_Contacts].nick, "%d", uin);
		Num_Contacts++;
	}

	if (packet_size < 1024) {
		fprintf(stderr, "TCP_ProcessPacket about to be called \n");
		TCP_ProcessPacket(packet + 2, packet_size, sock);
	}

	g_free(packet);
	return TRUE;
}

gint TCP_Connect(guint32 ip, guint32 port)
{
	struct sockaddr_in local, remote;
	int sizeofSockaddr = sizeof(struct sockaddr);
	int rc, sock;

	ICQ_Debug(ICQ_VERB_INFO, "\nTCP> TCP_Connect");

	if (ip == 0)
		return -1;

	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(0);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_port = htons(port);
	remote.sin_addr.s_addr = htonl(ip);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return -1;

	set_nonblock(sock);

	if ((bind(sock, (struct sockaddr *)&local,
				sizeof(struct sockaddr))) == -1)
		return -1;

	getsockname(sock, (struct sockaddr *)&local, &sizeofSockaddr);

	rc = proxy_connect(sock, (struct sockaddr *)&remote, sizeof(remote));
	open_sockets = g_list_append(open_sockets, (gpointer) sock);

	if (rc >= 0) {
		if (Verbose & ICQ_VERB_INFO)
			fprintf(stderr,
				"TCP_Connect(): connect() completed immediately\n");
	}
#if defined( _WIN32 )
	else if (WSAGetLastError() == WSAEINPROGRESS)
#else
	else if (errno == EINPROGRESS)
#endif
	{
		if (Verbose & ICQ_VERB_INFO)
			fprintf(stderr,
				"TCP_Connect(): connect() in progress...\n");
	} else {
/*
      perror("TCP_Connect():");
*/
		fprintf(stderr, "TCP_Connect(): Connection Refused.\n");
		return (-1);
	}

	return sock;
}

gint TCP_SendMessage(guint32 uin, char *msg)
{
	int cx;
	int sock;
	unsigned short intsize;
	char buffer[1024];

	typedef struct {
		guint8 uin_a[4];	// UIN
		guint8 version[2];	// VERSION
		guint8 cmd[2];	// TCP_START
		guint8 zero[2];	// 0x0000
		guint8 uin_b[4];	// UIN
		guint8 command[2];	// TCP_MSG
		guint8 msg_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 real_ip[4];
		guint8 port[4];
		guint8 four;
		guint8 zero[4];
		guint8 seq[4];
	} tcp_tail;

	struct {
		tcp_head head;
		char *body;
		tcp_tail tail;
	} packet;

/*** MITCH 04/17/2001 */
	gchar *errmsg;		/* dynamic allocation... a little slower, but a hell of alot safer */
	errmsg = g_new0(gchar, 128 + strlen(msg));	/* 128 is the length that the format string can */
	/* fit in, thestrlen ensures enough space once */
	/* the string is printed in */
	sprintf(errmsg, "TCP> TCP_SendMessage(%04X, %s)\n", uin, msg);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
	g_free(errmsg);
/*** */

	DW_2_Chars(packet.head.uin_a, UIN);
	Word_2_Chars(packet.head.version, 0x0003);	/* ICQ_VER ??? */
	Word_2_Chars(packet.head.cmd, ICQ_CMDxTCP_START);
	Word_2_Chars(packet.head.zero, 0x0000);
	DW_2_Chars(packet.head.uin_b, UIN);
	Word_2_Chars(packet.head.command, ICQ_CMDxTCP_MSG);
	Word_2_Chars(packet.head.msg_length, (strlen(msg) + 1));

	packet.body = msg;	/* Mike 1999/02/13: Memory issues ... hmmm ... */

	DW_2_Chars(packet.tail.ip, our_ip);
	DW_2_Chars(packet.tail.real_ip, our_ip);
	DW_2_Chars(packet.tail.port, our_port);
	packet.tail.four = 0x04;
	DW_2_Chars(packet.tail.zero, 0x00100000);
	DW_2_Chars(packet.tail.seq, seq_num++);

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].uin == uin)
			break;

	if (cx == Num_Contacts)
		return 0;

	sock = Contacts[cx].tcp_sok;

	if (sock != -1) {
		char my_buf[2];
		ICQ_Debug(ICQ_VERB_INFO, "TCP Connection established");

		intsize = sizeof(tcp_head) + sizeof(tcp_tail) + strlen(msg) + 1;

		Word_2_Chars(my_buf, intsize);
		//memcpy(&buffer[0], &intsize, 2);
		memcpy(&buffer[0], my_buf, 2);
		memcpy(&buffer[2], &packet.head, sizeof(packet.head));
		memcpy(&buffer[2 + sizeof(packet.head)], packet.body,
			strlen(packet.body) + 1);
		memcpy(&buffer[2 + sizeof(packet.head) + strlen(packet.body) +
				1], &packet.tail, sizeof(packet.tail));
		write(sock, buffer, intsize + 2);
		packet_print(buffer, intsize + 2);
		return 1;
	}

	ICQ_Debug(ICQ_VERB_ERR, "TCP Connection failed.");

	return 0;
}

gint TCP_SendURL(guint32 uin, char *url, char *text)
{
	int cx;
	int sock;
	unsigned short intsize;
	char buffer[1024], msg[1024];

	typedef struct {
		guint8 uin_a[4];
		guint8 version[2];
		guint8 cmd[2];
		guint8 zero[2];
		guint8 uin_b[4];
		guint8 command[2];
		guint8 msg_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 real_ip[4];
		guint8 port[4];
		guint8 four;
		guint8 zero[4];
		guint8 seq[4];
	} tcp_tail;

	struct {
		tcp_head head;
		char *body;
		tcp_tail tail;
	} packet;

/*** MITCH 04/17/2001 */
	gchar *errmsg;		/* dynamic allocation... a little slower, but a hell of alot safer */
	errmsg = g_new0(gchar, 128 + strlen(url) + strlen(text));
	sprintf(errmsg, "TCP> TCP_SendURL(%04X, %s, %s)", uin, url, text);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
	g_free(errmsg);
/*** */

	if (!url)
		url = "";
	if (!text)
		text = "";

	strcpy(msg, text);
	strcat(msg, "\xFE");
	strcat(msg, url);

	DW_2_Chars(packet.head.uin_a, UIN);
	Word_2_Chars(packet.head.version, 0x0003);
	Word_2_Chars(packet.head.cmd, ICQ_CMDxTCP_START);
	Word_2_Chars(packet.head.zero, 0x0000);
	DW_2_Chars(packet.head.uin_b, UIN);
	Word_2_Chars(packet.head.command, ICQ_CMDxTCP_URL);
	Word_2_Chars(packet.head.msg_length, (strlen(msg) + 1));

	packet.body = msg;

	DW_2_Chars(packet.tail.ip, our_ip);
	DW_2_Chars(packet.tail.real_ip, our_ip);
	DW_2_Chars(packet.tail.port, our_port);
	packet.tail.four = 0x04;
	DW_2_Chars(packet.tail.zero, 0x00100000);
	DW_2_Chars(packet.tail.seq, seq_num++);

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].uin == uin)
			break;

	if (cx == Num_Contacts)
		return 0;

	sock = Contacts[cx].tcp_sok;

	if (sock != -1) {
		char my_buf[2];
		intsize = sizeof(tcp_head) + sizeof(tcp_tail) + strlen(msg) + 1;
		Word_2_Chars(my_buf, intsize);

		//memcpy(&buffer[0], &intsize, 2);
		memcpy(&buffer[0], my_buf, 2);
		memcpy(&buffer[2], &packet.head, sizeof(packet.head));
		memcpy(&buffer[2 + sizeof(packet.head)], packet.body,
			strlen(packet.body) + 1);
		memcpy(&buffer[2 + sizeof(packet.head) + strlen(packet.body) +
				1], &packet.tail, sizeof(packet.tail));
		write(sock, buffer, intsize + 2);
		packet_print(buffer, intsize + 2);
		return 1;
	}

	return 0;
}

gint TCP_GetAwayMessage(guint32 uin)
{
	int cx;
	int sock;
	unsigned short intsize;
	char buffer[1024];
	int status_type;

	typedef struct {
		guint8 uin_a[4];
		guint8 version[2];
		guint8 cmd[2];
		guint8 zero[2];
		guint8 uin_b[4];
		guint8 command[2];
		guint8 msg_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 real_ip[4];
		guint8 port[4];
		guint8 four;
		guint8 zero[4];
		guint8 seq[4];
	} tcp_tail;

	struct {
		tcp_head head;
		char *body;
		tcp_tail tail;
	} packet;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_GetAwayMessage(%04X)", uin);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].uin == uin)
			break;

	if (cx == Num_Contacts)
		return 0;

	switch (Contacts[cx].status & 0xffff) {
	case STATUS_AWAY:
		status_type = ICQ_CMDxTCP_READxAWAYxMSG;
		break;
	case STATUS_NA:
		status_type = ICQ_CMDxTCP_READxNAxMSG;
		break;
	case STATUS_OCCUPIED:
		status_type = ICQ_CMDxTCP_READxOCCxMSG;
		break;
	case STATUS_DND:
		status_type = ICQ_CMDxTCP_READxDNDxMSG;
		break;
	default:
		status_type = ICQ_CMDxTCP_READxAWAYxMSG;
		break;
	}

	DW_2_Chars(packet.head.uin_a, UIN);
	Word_2_Chars(packet.head.version, 0x0003);
	Word_2_Chars(packet.head.cmd, ICQ_CMDxTCP_START);
	Word_2_Chars(packet.head.zero, 0x0000);
	DW_2_Chars(packet.head.uin_b, UIN);
	Word_2_Chars(packet.head.command, status_type);
	Word_2_Chars(packet.head.msg_length, 0x0001);

	packet.body = "";

	DW_2_Chars(packet.tail.ip, our_ip);
	DW_2_Chars(packet.tail.real_ip, LOCALHOST);
	DW_2_Chars(packet.tail.port, our_port);
	packet.tail.four = 0x04;
	DW_2_Chars(packet.tail.zero, 0x00001000);
	DW_2_Chars(packet.tail.seq, seq_num++);

	sock = Contacts[cx].tcp_sok;

	if (sock != -1) {
		char my_buf[2];
		intsize = sizeof(tcp_head) + sizeof(tcp_tail) + 1;

		Word_2_Chars(my_buf, intsize);
		memcpy(&buffer[0], my_buf, 2);
		//memcpy( &buffer[0], &intsize, 2 );
		memcpy(&buffer[2], &packet.head, sizeof(packet.head));
		memcpy(&buffer[2 + sizeof(packet.head)], packet.body,
			strlen(packet.body) + 1);
		memcpy(&buffer[2 + sizeof(packet.head) + strlen(packet.body) +
				1], &packet.tail, sizeof(packet.tail));
		write(sock, buffer, intsize + 2);
		packet_print(buffer, intsize + 2);
	} else {
		return 0;
	}

	return 1;
}

gint TCP_SendChatRequest(guint32 uin, char *msg)
{
	int cx;
	int sock;
	unsigned short intsize;
	char buffer[1024];
	char my_buf[2];

	typedef struct {
		guint8 uin_a[4];
		guint8 version[2];
		guint8 cmd[2];
		guint8 zero[2];
		guint8 uin_b[4];
		guint8 command[2];
		guint8 msg_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 real_ip[4];
		guint8 port[4];
		guint8 trail1[4];
		guint8 trail2[4];
		guint8 trail3[4];
		guint8 trail4[4];
		guint8 seq[4];
	} tcp_tail;

	struct {
		tcp_head head;
		char *body;
		tcp_tail tail;
	} packet;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_SendChatRequest(%04X)", uin);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].uin == uin)
			break;

	if (cx == Num_Contacts)
		return 0;

	DW_2_Chars(packet.head.uin_a, UIN);
	Word_2_Chars(packet.head.version, 0x0003);
	Word_2_Chars(packet.head.cmd, ICQ_CMDxTCP_START);
	Word_2_Chars(packet.head.zero, 0x0000);
	DW_2_Chars(packet.head.uin_b, UIN);
	Word_2_Chars(packet.head.command, ICQ_CMDxTCP_CHAT);
	Word_2_Chars(packet.head.msg_length, (strlen(msg) + 1));

	packet.body = msg;

	DW_2_Chars(packet.tail.ip, our_ip);
	DW_2_Chars(packet.tail.real_ip, LOCALHOST);
	DW_2_Chars(packet.tail.port, our_port);
	DW_2_Chars(packet.tail.trail1, 0x10000004);
	DW_2_Chars(packet.tail.trail2, 0x00000100);
	DW_2_Chars(packet.tail.trail3, 0x00000000);
	DW_2_Chars(packet.tail.trail4, 0x00000000);
	DW_2_Chars(packet.tail.seq, seq_num++);

	sock = Contacts[cx].tcp_sok;

	if (sock == -1)
		return -1;

	intsize = sizeof(tcp_head) + sizeof(tcp_tail) + strlen(msg) + 1;
	Word_2_Chars(my_buf, intsize);

	//memcpy(&buffer[0], &intsize, 2);
	memcpy(&buffer[0], my_buf, 2);
	memcpy(&buffer[2], &packet.head, sizeof(packet.head));
	memcpy(&buffer[2 + sizeof(packet.head)], packet.body,
		strlen(packet.body) + 1);
	memcpy(&buffer[2 + sizeof(packet.head) + strlen(packet.body) + 1],
		&packet.tail, sizeof(packet.tail));

	write(sock, buffer, intsize + 2);

	return 1;
}

gint TCP_ChatClientHandshake(gint sock)
{
	gint cindex;
	gint size = sizeof(struct sockaddr);
	struct sockaddr_in their_addr;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_ChatClientHandshake");

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].chat_sok == sock)
			break;

	Contacts[cindex].chat_sok =
		accept(sock, (struct sockaddr *)&their_addr, &size);
	Contacts[cindex].chat_port = ntohs(their_addr.sin_port);
	Contacts[cindex].chat_status ^= TCP_HANDSHAKE_WAIT;
	open_sockets = g_list_append(open_sockets,
		(gpointer) Contacts[cindex].chat_sok);

	return TRUE;
}

gint TCP_ChatServerHandshake(gint sock)
{
	gint cindex;
	guint32 localport;
	struct sockaddr_in local;
	int sizeofSockaddr = sizeof(struct sockaddr);
	guint16 size;
	char buffer[1024];
	char my_buf[2];

	typedef struct {
		guint8 code;
		guint8 version[4];
		guint8 chat_porta[4];
		guint8 uin[4];
		guint8 ip_local[4];
		guint8 ip_remote[4];
		guint8 four;
		guint8 chat_portb[4];
	} handshake_a;

	typedef struct {
		guint8 code[4];
		guint8 biga[4];
		guint8 uin[4];
		guint8 name_length[2];
	} handshake_b;

	typedef struct {
		guint8 revporta;
		guint8 revportb;
		guint8 foreground[4];
		guint8 background[4];
		guint8 zero;
	} handshake_c;

	handshake_a hsa;
	handshake_b hsb;
	handshake_c hsc;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_ChatServerHandshake()");

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].chat_sok == sock)
			break;

	getsockname(sock, (struct sockaddr *)&local, &sizeofSockaddr);
	localport = ntohs(local.sin_port);

	Contacts[cindex].chat_sok = sock;

	hsa.code = 0xFF;
	DW_2_Chars(hsa.version, 0x00000004);
	DW_2_Chars(hsa.uin, UIN);
	DW_2_Chars(hsa.ip_local, LOCALHOST);
	DW_2_Chars(hsa.ip_remote, LOCALHOST);
	hsa.four = 0x04;
	DW_2_Chars(hsa.chat_portb, (guint32) localport);
	DW_2_Chars(hsa.chat_porta, (guint32) localport);

	size = sizeof(handshake_a);
	Word_2_Chars(my_buf, size);

	//memcpy(&buffer[0], &size, 2);
	memcpy(&buffer[0], my_buf, 2);
	memcpy(&buffer[2], &hsa, size);
	write(sock, buffer, size + 2);

	DW_2_Chars(hsb.code, 0x00000064);
	DW_2_Chars(hsb.biga, 0xFFFFFFFD);
	DW_2_Chars(hsb.uin, UIN);
	Word_2_Chars(hsb.name_length, strlen(nickname) + 1);
	hsc.revporta = ((char *)(&localport))[1];
	hsc.revportb = ((char *)(&localport))[0];
	DW_2_Chars(hsc.foreground, 0x00FFFFFF);
	DW_2_Chars(hsc.background, 0x00000000);
	hsc.zero = 0x00;

	size = sizeof(handshake_b) + sizeof(handshake_c) + strlen(nickname);
	/* bzero is old and non-POSIX, use memset instead */
	/* win32--bzero((void*)buffer, 1024); */
	memset((void *)buffer, 0, 1024);

	//memcpy(&buffer[0], &size, 2);
	Word_2_Chars(my_buf, size);
	memcpy(&buffer[0], my_buf, 2);

	memcpy(&buffer[2], &hsb, sizeof(handshake_b));
	memcpy(&buffer[2 + sizeof(handshake_b)], nickname,
		strlen(nickname) + 1);
	memcpy(&buffer[3 + sizeof(handshake_b) + strlen(nickname)], &hsc,
		sizeof(handshake_c));
	write(sock, buffer, size + 2);

	return sock;
}

gint TCP_ConnectChat(guint32 port, guint32 uin)
{
	struct sockaddr_in local, remote;
	int sizeofSockaddr = sizeof(struct sockaddr);
	gint sock;
	gint cindex;
	guint32 ip;
	gint rc;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_ConnectChat(%04X)", uin);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return 0;

	if (Contacts[cindex].chat_sok > 0)
		return Contacts[cindex].chat_sok;

	if (!(ip = Contacts[cindex].current_ip))
		return -1;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock == -1)
		return -1;

	set_nonblock(sock);

	/* bzero is old and faded, use memset instead, god bless POSIX */
	/*win32--bzero(&local.sin_zero, 8); */
	memset(&local.sin_zero, 0, 8);
	/*win32--bzero(&remote.sin_zero, 8); */
	memset(&remote.sin_zero, 0, 8);

	local.sin_family = AF_INET;
	remote.sin_family = AF_INET;
	local.sin_port = htons(0);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	remote.sin_port = htons(port);
	remote.sin_addr.s_addr = htonl(ip);

	rc = proxy_connect(sock, (struct sockaddr *)&remote, sizeofSockaddr);
	open_sockets = g_list_append(open_sockets, (gpointer) sock);

	Contacts[cindex].chat_sok = sock;
	Contacts[cindex].chat_status = TCP_NOT_CONNECTED;

	if (rc >= 0) {
		if (Verbose & ICQ_VERB_INFO)
			fprintf(stderr,
				"TCP_ConnectChat(): connect() completed immediately\n");

		Contacts[cindex].chat_status |= TCP_CONNECTED;
		Contacts[cindex].chat_status |= TCP_SERVER;

		TCP_ChatServerHandshake(sock);
	}
#if defined( _WIN32 )
	else if (WSAGetLastError() == WSAEINPROGRESS)
#else
	else if (errno == EINPROGRESS)
#endif
	{
		if (Verbose & ICQ_VERB_INFO)
			fprintf(stderr,
				"TCP_ConnectChat(): connect() in progress...\n");
	} else {
		perror("TCP_ConnectChat():");
	}

	return sock;
}

int TCP_RefuseChat(guint32 uin)
{
	gint cindex;
	gint sock;
	guint32 seq;
	char buffer[1024];
	char my_buf[2];
	unsigned short intsize;

	typedef struct {
		guint8 uin1[4];
		guint8 version[2];
		guint8 command[2];
		guint8 zero[2];
		guint8 uin2[4];
		guint8 cmd[2];
		guint8 message_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 ip_real[4];
		guint8 porta[4];
		guint8 junk;
		guint8 status[4];
		guint8 zeroa[4];
		guint8 zerob[4];
		guint8 zeroc[2];
		guint8 zerod;
		guint8 seq[4];
	} tcp_tail;

	tcp_head pack_head;
	tcp_tail pack_tail;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_RefuseChat");

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return 0;

	sock = Contacts[cindex].tcp_sok;
	seq = Contacts[cindex].chat_seq;

	DW_2_Chars(pack_head.uin1, UIN);
	Word_2_Chars(pack_head.version, 0x0003);
	Word_2_Chars(pack_head.command, ICQ_CMDxTCP_ACK);
	Word_2_Chars(pack_head.zero, 0x0000);
	DW_2_Chars(pack_head.uin2, UIN);
	DW_2_Chars(pack_head.cmd, ICQ_CMDxTCP_CHAT);
	DW_2_Chars(pack_head.message_length, 1);

	DW_2_Chars(pack_tail.ip, our_ip);
	DW_2_Chars(pack_tail.ip_real, LOCALHOST);
	DW_2_Chars(pack_tail.porta, our_port);
	pack_tail.junk = 0x04;
	DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_REFUSE);

	DW_2_Chars(pack_tail.zeroa, 0x00000001);
	DW_2_Chars(pack_tail.zerob, 0x00000000);
	DW_2_Chars(pack_tail.zeroc, 0x00000000);
	pack_tail.zerod = 0x00;
	DW_2_Chars(pack_tail.seq, seq);

	if (sock == -1)
		return -1;

	intsize = sizeof(tcp_head) + sizeof(tcp_tail) + 1;
	//memcpy(&buffer[0], &intsize, 2);
	Word_2_Chars(my_buf, intsize);
	memcpy(&buffer[0], my_buf, 2);

	memcpy(&buffer[2], &pack_head, sizeof(pack_head));
	buffer[2 + sizeof(pack_head)] = 0x00;
	memcpy(&buffer[2 + sizeof(pack_head) + 1], &pack_tail,
		sizeof(pack_tail));
	write(sock, buffer, intsize + 2);

	return 1;
}

/* This is called when the user accepts a chat request */

gint TCP_AcceptChat(guint32 uin)
{
	gint cindex;
	guint32 seq;
	char buffer[1024];
	char my_buf[2];
	guint16 intsize;
	gint chat_sock;
	guint16 chat_port, backport, tempport;
	int length = sizeof(struct sockaddr);

	typedef struct {
		guint8 uin1[4];
		guint8 version[2];
		guint8 command[2];
		guint8 zero[2];
		guint8 uin2[4];
		guint8 cmd[2];
		guint8 message_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 ip_real[4];
		guint8 porta[4];
		guint8 junk;
		guint8 status[4];
		guint8 one[2];
		guint8 zero;
		guint8 back_port[4];
		guint8 portb[4];
		guint8 seq[4];
	} tcp_tail;

	tcp_head pack_head;
	tcp_tail pack_tail;

	struct sockaddr_in my_addr;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_AcceptChat");

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return 0;

	seq = Contacts[cindex].chat_seq;

	chat_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (chat_sock <= 0)
		return -1;

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = 0;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	/* you've heard my rant about bzero, just use MEMSET! */
	/*win32--bzero(&(my_addr.sin_zero), 8); */
	memset(&(my_addr.sin_zero), 0, 8);

	if (bind(chat_sock, (struct sockaddr *)&my_addr,
			sizeof(struct sockaddr)) == -1)
		return -1;

	listen(chat_sock, 1);

	getsockname(chat_sock, (struct sockaddr *)&my_addr, &length);

	set_nonblock(chat_sock);

	chat_port = ntohs(my_addr.sin_port);

	tempport = chat_port;
	backport = (tempport >> 8) + (tempport << 8);

	DW_2_Chars(pack_head.uin1, UIN);
	Word_2_Chars(pack_head.version, 0x0003);
	Word_2_Chars(pack_head.command, ICQ_CMDxTCP_ACK);
	Word_2_Chars(pack_head.zero, 0x0000);
	DW_2_Chars(pack_head.uin2, UIN);
	DW_2_Chars(pack_head.cmd, ICQ_CMDxTCP_CHAT);
	DW_2_Chars(pack_head.message_length, 1);

	DW_2_Chars(pack_tail.ip, 0x00000000);
	DW_2_Chars(pack_tail.ip_real, LOCALHOST);
	DW_2_Chars(pack_tail.porta, our_port);
	pack_tail.junk = 0x04;
	DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_ONLINE);

	DW_2_Chars(pack_tail.one, 0x0001);
	pack_tail.zero = 0x00;
	DW_2_Chars(pack_tail.back_port, backport);
	DW_2_Chars(pack_tail.portb, chat_port);
	DW_2_Chars(pack_tail.seq, seq);

	if (chat_sock == -1)
		return -1;

	intsize = sizeof(tcp_head) + sizeof(tcp_tail) + 1;
	//memcpy(&buffer[0], &intsize, 2);
	Word_2_Chars(my_buf, intsize);
	memcpy(&buffer[0], my_buf, 2);
	memcpy(&buffer[2], &pack_head, sizeof(pack_head));
	buffer[2 + sizeof(pack_head)] = 0x00;
	memcpy(&buffer[2 + sizeof(pack_head) + 1], &pack_tail,
		sizeof(pack_tail));
	write(Contacts[cindex].tcp_sok, buffer, intsize + 2);

	Contacts[cindex].chat_sok = chat_sock;
	Contacts[cindex].chat_status |= TCP_CONNECTED;
	Contacts[cindex].chat_status |= TCP_CLIENT;
	Contacts[cindex].chat_status |= TCP_HANDSHAKE_WAIT;

	open_sockets = g_list_append(open_sockets, (gpointer) chat_sock);

	return 1;
}

gint TCP_ChatReadServer(gint sock)
{
	gint cx;
	guint16 packet_size;
	char *packet;
	char buffer[1024];
	char my_buf[2];
	char c;
	CHAT_DATA cd;

	char *font_name = "Arial";
	guint16 font_size = 12;

	typedef struct {
		guint8 version[4];
		guint8 chat_port[4];
		guint8 ip_local[4];
		guint8 ip_remote[4];
		guint8 four;
		guint8 our_port[2];
		guint8 font_size[4];
		guint8 font_face[4];
		guint8 font_length[2];
	} begin_chat_a;

	typedef struct {
		guint8 one[2];
	} begin_chat_b;

	typedef struct {
		guint32 code;
		guint32 uin;
		guint16 name_length;
		guint8 foreground[4];
		guint8 background[4];
		guint8 version[4];
		guint8 chat_port[4];
		guint8 ip_local[4];
		guint8 ip_remote[4];
		guint8 four;
		guint8 our_port[2];
		guint8 font_size[4];
		guint8 font_face[4];
		guint8 font_length[2];
	} read_pak;

	static char *oneline;

	begin_chat_a paka;
	begin_chat_b pakb;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_ChatReadServer");

	if (oneline == NULL) {
		oneline = (char *)malloc(1024);
		strcpy(oneline, "");
	}

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].chat_sok == sock)
			break;

	if (Contacts[cx].chat_active == FALSE) {
		read(sock, (char *)(&packet_size), 1);
		read(sock, (char *)(&packet_size) + 1, 1);

		DW_2_Chars(paka.version, 0x00000004);
		DW_2_Chars(paka.chat_port, Contacts[cx].chat_port);
		DW_2_Chars(paka.ip_local, LOCALHOST);
		DW_2_Chars(paka.ip_remote, LOCALHOST);
		paka.four = 0x04;
		Word_2_Chars(paka.our_port, our_port);
		DW_2_Chars(paka.font_size, font_size);
		DW_2_Chars(paka.font_face, FONT_PLAIN);
		Word_2_Chars(paka.font_length, strlen(font_name) + 1);

		Word_2_Chars(pakb.one, 0x0001);

		packet_size =
			sizeof(begin_chat_a) + sizeof(begin_chat_b) +
			strlen(font_name) + 1;
		packet = (guint8 *)malloc(packet_size);

		//memcpy(&buffer[0], &packet_size, 2);
		Word_2_Chars(my_buf, packet_size);
		memcpy(&buffer[0], my_buf, 2);
		memcpy(&buffer[2], &paka, sizeof(begin_chat_a));
		memcpy(&buffer[2 + sizeof(begin_chat_a)], font_name,
			strlen(font_name) + 1);
		memcpy(&buffer[3 + sizeof(begin_chat_a) + strlen(font_name)],
			&pakb, sizeof(begin_chat_b));
		write(sock, buffer, packet_size + 2);

		free(packet);

		Contacts[cx].chat_active = TRUE;

		if (event[EVENT_CHAT_CONNECT] != NULL)
			(*event[EVENT_CHAT_CONNECT]) ((gpointer) Contacts[cx].
				uin);
	} else {
		if ((read(sock, &c, 1)) <= 0) {
#if defined( _WIN32 )
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return 1;
#else
			if (errno == EWOULDBLOCK)
				return 1;
#endif

			open_sockets =
				g_list_remove(open_sockets, (gpointer) sock);
			close(sock);
			Contacts[cx].chat_sok = 0;
			Contacts[cx].chat_port = 0;
			Contacts[cx].chat_status = 0;
			Contacts[cx].chat_active = Contacts[cx].chat_active2 =
				FALSE;
			if (event[EVENT_CHAT_DISCONNECT] != NULL)
				(*event[EVENT_CHAT_DISCONNECT]) ((gpointer)
					Contacts[cx].uin);
			return TRUE;
		}

		/* send 'c' to front-end */

		cd.uin = Contacts[cx].uin;
		cd.c = c;

		if (event[EVENT_CHAT_READ] != NULL)
			(*event[EVENT_CHAT_READ]) (&cd);

		if (recv(sock, &c, 1, MSG_PEEK) > 0) ;
		TCP_ChatReadServer(sock);
	}

	return 1;
}

gint TCP_ChatReadClient(gint sock)
{
	gint cx;
	guint16 packet_size;
	char *packet;
	char buffer[1024];
	char my_buf[2];
	char c;
	CHAT_DATA cd;

	char *font_name = "Arial";
	guint32 font_size = 12;

	typedef struct {
		guint8 code[4];
		guint8 uin[4];
		guint8 name_length[2];
	} begin_chat_a;

	typedef struct {
		guint8 foreground[4];
		guint8 background[4];
		guint8 version[4];
		guint8 chat_port[4];
		guint8 ip_local[4];
		guint8 ip_remote[4];
		guint8 four;
		guint8 our_port[2];
		guint8 font_size[4];
		guint8 font_face[4];
		guint8 font_length[2];
	} begin_chat_b;

	typedef struct {
		guint8 zeroa[2];
		guint8 zerob;
	} begin_chat_c;

	begin_chat_a paka;
	begin_chat_b pakb;
	begin_chat_c pakc;

	ICQ_Debug(ICQ_VERB_INFO, "TCP> TCP_ChatReadClient");

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].chat_sok == sock)
			break;

	set_nonblock(sock);

	if (Contacts[cx].chat_active == FALSE) {
		char c;
//    read(sock, (char*)(&packet_size), 1);
//    read(sock, (char*)(&packet_size) + 1, 1);

//      packet = g_malloc( packet_size );
//      read( sock, packet, packet_size );
//      g_free(packet);

		DW_2_Chars(paka.code, 0x00000064);
		DW_2_Chars(paka.uin, UIN);
		Word_2_Chars(paka.name_length, strlen(nickname) + 1);
		DW_2_Chars(pakb.foreground, 0x00FFFFFF);
		DW_2_Chars(pakb.background, 0x00000000);
		DW_2_Chars(pakb.version, 0x00000004);
		DW_2_Chars(pakb.chat_port, Contacts[cx].chat_port);
		DW_2_Chars(pakb.ip_local, LOCALHOST);
		DW_2_Chars(pakb.ip_remote, LOCALHOST);
		pakb.four = 0x04;
		Word_2_Chars(pakb.our_port, our_port);
		DW_2_Chars(pakb.font_size, font_size);
		DW_2_Chars(pakb.font_face, FONT_PLAIN);
		Word_2_Chars(pakb.font_length, strlen(font_name) + 1);
		Word_2_Chars(pakc.zeroa, 0x0000);
		pakc.zerob = 0x00;

		if (recv(sock, &c, 1, MSG_PEEK) > 0) {
			fprintf(stderr, "I got a 0x%02x\n", c);
			if (c == 0xfffffffd) {
				Contacts[cx].chat_active = TRUE;
			} else {
				read(sock, &c, 1);
			}
		}

		if (Contacts[cx].chat_active2 == FALSE) {
			//    read(sock, (char*)(&packet_size), 1);
			//     read(sock, (char*)(&packet_size) + 1, 1);

//        packet = g_malloc( packet_size );
//        read( sock, packet, packet_size );
//        g_free(packet);

			packet_size =
				sizeof(begin_chat_a) + sizeof(begin_chat_b) +
				sizeof(begin_chat_c) + strlen(nickname) + 1 +
				strlen(font_name) + 1;
			packet = (guint8 *)malloc(packet_size);
			//memcpy(&buffer[0], &packet_size, 2);
			Word_2_Chars(my_buf, packet_size);
			memcpy(&buffer[0], my_buf, 2);
			memcpy(&buffer[2], &paka, sizeof(begin_chat_a));
			memcpy(&buffer[2 + sizeof(begin_chat_a)], nickname,
				strlen(nickname) + 1);
			memcpy(&buffer[3 + sizeof(begin_chat_a) +
					strlen(nickname)], &pakb,
				sizeof(begin_chat_b));
			memcpy(&buffer[3 + sizeof(begin_chat_a) +
					strlen(nickname) +
					sizeof(begin_chat_b)], font_name,
				strlen(font_name) + 1);
			memcpy(&buffer[4 + sizeof(begin_chat_a) +
					strlen(nickname) +
					sizeof(begin_chat_b) +
					strlen(font_name)], &pakc,
				sizeof(begin_chat_c));
			write(sock, buffer, packet_size + 2);

			free(packet);
			Contacts[cx].chat_active2 = TRUE;

			if (event[EVENT_CHAT_CONNECT] != NULL)
				(*event[EVENT_CHAT_CONNECT]) ((gpointer)
					Contacts[cx].uin);
		}
		/*
		   else
		   {
		   Contacts[cx].chat_active = TRUE;
		   }
		 */
	}
	if (Contacts[cx].chat_active == TRUE) {
		if ((read(sock, &c, 1)) <= 0) {
#if defined( _WIN32 )
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return 1;
#else
			if (errno == EWOULDBLOCK)
				return 1;
#endif

			open_sockets =
				g_list_remove(open_sockets, (gpointer) sock);
			close(sock);
			Contacts[cx].chat_sok = 0;
			Contacts[cx].chat_port = 0;
			Contacts[cx].chat_status = 0;
			Contacts[cx].chat_active = Contacts[cx].chat_active2 =
				FALSE;
			if (event[EVENT_CHAT_DISCONNECT] != NULL)
				(*event[EVENT_CHAT_DISCONNECT]) ((gpointer)
					Contacts[cx].uin);
			return TRUE;
		}

		/* send 'c' to front-end */

		cd.uin = Contacts[cx].uin;
		cd.c = c;

		if (event[EVENT_CHAT_READ] != NULL)
			(*event[EVENT_CHAT_READ]) (&cd);

		if (recv(sock, &c, 1, MSG_PEEK) > 0)
			TCP_ChatReadClient(sock);
	}

	return TRUE;
}

int TCP_ChatSend(guint32 uin, gchar *text)
{
	gint cindex;

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return 0;
	if (Contacts[cindex].chat_sok <= 0)
		return 0;

	write(Contacts[cindex].chat_sok, text, strlen(text));

	return 1;
}

gint TCP_TerminateChat(guint32 uin)
{
	int cx;

/*** MITCH 04/17/2001 */
	/* dynamic allocation isn't used here because it's not needed */
	/* there aren't any strings lurking in the sprintf call to bite */
	/* us in the ass */
	gchar errmsg[255];
	sprintf(errmsg, "TCP> TCP_TerminateChat(%04X)", uin);
	ICQ_Debug(ICQ_VERB_INFO, errmsg);
/*** */

	for (cx = 0; cx < Num_Contacts; cx++)
		if (Contacts[cx].uin == uin)
			break;

	if (cx == Num_Contacts)
		return 0;

	open_sockets = g_list_remove(open_sockets,
		(gpointer) Contacts[cx].chat_sok);

	close(Contacts[cx].chat_sok);
	Contacts[cx].chat_sok = 0;
	Contacts[cx].chat_port = 0;
	Contacts[cx].chat_status = 0;
	Contacts[cx].chat_active = Contacts[cx].chat_active2 = FALSE;

	return 1;
}

int TCPFileHandshake(Contact_Member *contact, int sock, eb_input_condition cond)
{
	return TRUE;
}

int TCP_AcceptFile(int sock, int uin, guint32 seq)
{
	int cindex;
	guint8 *buffer;
	guint16 intsize;
	gint file_sock;
	guint16 file_port, backport, tempport;

	typedef struct {
		guint8 uin1[4];
		guint8 version[2];
		guint8 command[2];
		guint8 zero[2];
		guint8 uin2[4];
		guint8 cmd[2];
		guint8 message_length[2];
	} tcp_head;

	typedef struct {
		guint8 ip[4];
		guint8 ip_real[4];
		guint8 porta[4];
		guint8 junk;
		guint8 status[4];
		guint8 back_port[4];
		guint8 one[2];
		guint8 zero;
		guint8 bigzero[4];
		guint8 portb[4];
		guint8 seq[4];
	} tcp_tail;

	tcp_head pack_head;
	tcp_tail pack_tail;

	struct sockaddr_in my_addr;

	file_sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef O_NONBLOCK
	fcntl(file_sock, O_NONBLOCK);
#endif

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = 0;
	my_addr.sin_addr.s_addr = g_htonl(INADDR_ANY);

	if (bind(file_sock, (struct sockaddr *)&my_addr,
			sizeof(struct sockaddr)) == -1)
		return FALSE;

	listen(file_sock, 1);

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return 0;

	Contacts[cindex].file_gdk_input = eb_input_add(file_sock,
		EB_INPUT_READ,
		(eb_input_function) TCPFileHandshake, &Contacts[cindex]);

	file_port = g_htons(my_addr.sin_port);

	tempport = file_port;
	backport = (tempport >> 8) + (tempport << 8);

	DW_2_Chars(pack_head.uin1, UIN);
	Word_2_Chars(pack_head.version, 0x0003);
	Word_2_Chars(pack_head.command, ICQ_CMDxTCP_ACK);
	Word_2_Chars(pack_head.zero, 0x0000);
	DW_2_Chars(pack_head.uin2, UIN);
	DW_2_Chars(pack_head.cmd, ICQ_CMDxTCP_FILE);

	DW_2_Chars(pack_tail.ip, our_ip);
	DW_2_Chars(pack_tail.ip_real, LOCALHOST);

	DW_2_Chars(pack_tail.porta, our_port);
	pack_tail.junk = 0x04;
	DW_2_Chars(pack_tail.status, ICQ_ACKxTCP_ONLINE);
	DW_2_Chars(pack_tail.portb, tempport);
	DW_2_Chars(pack_tail.one, 0x0001);
	pack_tail.zero = 0x00;
	DW_2_Chars(pack_tail.bigzero, backport);
	DW_2_Chars(pack_tail.back_port, backport);
	DW_2_Chars(pack_tail.seq, seq);
	if (sock != -1) {
		intsize = sizeof(tcp_head) + sizeof(tcp_tail) + 1;
		buffer = (guint8 *)g_malloc(intsize + 2);

		Word_2_Chars(buffer, intsize);
		memcpy(buffer + 2, &pack_head, sizeof(pack_head));
		buffer[2 + sizeof(pack_head)] = 0x00;
		memcpy(buffer + 2 + sizeof(pack_head) + 1,
			&pack_tail, sizeof(pack_tail));
		write(sock, buffer, intsize + 2);
	} else
		return -1;

	return 1;

}
