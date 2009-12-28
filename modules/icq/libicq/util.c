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
#include <stdlib.h>
#include <glib.h>
#include <sys/types.h>

#if defined( _WIN32 ) && !defined(__MINGW32__)
#include <io.h>
#else
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#ifndef __MINGW32__
#include <sys/socket.h>
#endif

#include "icq.h"
#include "libicq.h"

extern Contact_Member Contacts[];
extern int Num_Contacts;

/* #define DEBUG */

/********************************************************
The following data constitutes fair use for compatibility.
*********************************************************/
unsigned char icq_check_data[257] = {
	0x0a, 0x5b, 0x31, 0x5d, 0x20, 0x59, 0x6f, 0x75,
	0x20, 0x63, 0x61, 0x6e, 0x20, 0x6d, 0x6f, 0x64,
	0x69, 0x66, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20,
	0x73, 0x6f, 0x75, 0x6e, 0x64, 0x73, 0x20, 0x49,
	0x43, 0x51, 0x20, 0x6d, 0x61, 0x6b, 0x65, 0x73,
	0x2e, 0x20, 0x4a, 0x75, 0x73, 0x74, 0x20, 0x73,
	0x65, 0x6c, 0x65, 0x63, 0x74, 0x20, 0x22, 0x53,
	0x6f, 0x75, 0x6e, 0x64, 0x73, 0x22, 0x20, 0x66,
	0x72, 0x6f, 0x6d, 0x20, 0x74, 0x68, 0x65, 0x20,
	0x22, 0x70, 0x72, 0x65, 0x66, 0x65, 0x72, 0x65,
	0x6e, 0x63, 0x65, 0x73, 0x2f, 0x6d, 0x69, 0x73,
	0x63, 0x22, 0x20, 0x69, 0x6e, 0x20, 0x49, 0x43,
	0x51, 0x20, 0x6f, 0x72, 0x20, 0x66, 0x72, 0x6f,
	0x6d, 0x20, 0x74, 0x68, 0x65, 0x20, 0x22, 0x53,
	0x6f, 0x75, 0x6e, 0x64, 0x73, 0x22, 0x20, 0x69,
	0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6f,
	0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x20, 0x70, 0x61,
	0x6e, 0x65, 0x6c, 0x2e, 0x20, 0x43, 0x72, 0x65,
	0x64, 0x69, 0x74, 0x3a, 0x20, 0x45, 0x72, 0x61,
	0x6e, 0x0a, 0x5b, 0x32, 0x5d, 0x20, 0x43, 0x61,
	0x6e, 0x27, 0x74, 0x20, 0x72, 0x65, 0x6d, 0x65,
	0x6d, 0x62, 0x65, 0x72, 0x20, 0x77, 0x68, 0x61,
	0x74, 0x20, 0x77, 0x61, 0x73, 0x20, 0x73, 0x61,
	0x69, 0x64, 0x3f, 0x20, 0x20, 0x44, 0x6f, 0x75,
	0x62, 0x6c, 0x65, 0x2d, 0x63, 0x6c, 0x69, 0x63,
	0x6b, 0x20, 0x6f, 0x6e, 0x20, 0x61, 0x20, 0x75,
	0x73, 0x65, 0x72, 0x20, 0x74, 0x6f, 0x20, 0x67,
	0x65, 0x74, 0x20, 0x61, 0x20, 0x64, 0x69, 0x61,
	0x6c, 0x6f, 0x67, 0x20, 0x6f, 0x66, 0x20, 0x61,
	0x6c, 0x6c, 0x20, 0x6d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x73, 0x20, 0x73, 0x65, 0x6e, 0x74,
	0x20, 0x69, 0x6e, 0x63, 0x6f, 0x6d, 0x69, 0x6e, 0
};

Contact_Member *contact(guint32 uin)
{
	int cindex;

	for (cindex = 0; cindex < Num_Contacts; cindex++)
		if (Contacts[cindex].uin == uin)
			break;

	if (cindex == Num_Contacts)
		return NULL;

	return &Contacts[cindex];
}

Contact_Member *contact_from_socket(gint sock)
{
	int cindex;

	for (cindex = 0; cindex < Num_Contacts; cindex++) {
		if (Contacts[cindex].tcp_sok == sock)
			break;
		if (Contacts[cindex].chat_sok == sock)
			break;
	}

	if (cindex == Num_Contacts)
		return NULL;

	return &Contacts[cindex];
}

guint32 Chars_2_DW(unsigned char *buf)
{
	return (guint32) ((buf[3] << 24) + (buf[2] << 16) + (buf[1] << 8) +
		buf[0]);
}

guint16 Chars_2_Word(unsigned char *buf)
{
	return (guint16) ((buf[1] << 8) + buf[0]);
}

void DW_2_Chars(unsigned char *buf, guint32 num)
{
	buf[3] = (guchar) ((num >> 24) & 0x000000FF);
	buf[2] = (guchar) ((num >> 16) & 0x000000FF);
	buf[1] = (guchar) ((num >> 8) & 0x000000FF);
	buf[0] = (guchar) (num & 0x000000FF);
}

void Word_2_Chars(unsigned char *buf, guint16 num)
{
	buf[1] = (guchar) ((num >> 8) & 0x00FF);
	buf[0] = (guchar) (num & 0x00FF);
}

void wrinkle_packet(void *ptr, size_t len)
{
	guint8 *buf;
	guint32 chksum;
	guint32 key;
	guint32 tkey, temp;
	guint8 r1, r2;
	gint n, pos;
	guint32 chk1, chk2;

	buf = ptr;

	buf[2] = rand() & 0xff;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;

	r1 = rand() % (len - 4);
	r2 = rand() & 0xff;

	chk1 = (guint8) buf[8];
	chk1 <<= 8;
	chk1 += (guint8) buf[4];
	chk1 <<= 8;
	chk1 += (guint8) buf[2];
	chk1 <<= 8;
	chk1 += (guint8) buf[6];
	chk2 = r1;
	chk2 <<= 8;
	chk2 += (guint8) (buf[r1]);
	chk2 <<= 8;
	chk2 += r2;
	chk2 <<= 8;
	chk2 += (guint8) (icq_check_data[r2]);
	chk2 ^= 0x00ff00ff;

	chksum = chk2 ^ chk1;

	DW_2_Chars(&buf[0x10], chksum);
	key = len;
	key *= 0x66756B65;
	key += chksum;
	n = (len + 3) / 4;
	pos = 0;
	while (pos < n) {
		if (pos != 0x10) {
			tkey = key + icq_check_data[pos & 0xff];
			temp = Chars_2_DW(&buf[pos]);
			temp ^= tkey;
			DW_2_Chars(&buf[pos], temp);
		}
		pos += 4;
	}
	Word_2_Chars(&buf[0], ICQ_VER);
}

size_t SOCKREAD(gint sok, void *ptr, size_t len)
{
	size_t sz;

	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> SOCKREAD ...");

#ifndef __MINGW32__
	sz = recv(sok, ptr, len, MSG_DONTWAIT);
#else
	sz = recv(sok, ptr, len, 0);
#endif
	return sz;
}

size_t SOCKWRITE(gint sok, void *ptr, size_t len)
{
	ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> SOCKWRITE ...");
#if 0
	packet_print(ptr, len);
#endif

	Word_2_Chars(&((guint8 *)ptr)[4], 0);
	((guint8 *)ptr)[0x0A] = ((guint8 *)ptr)[8];
	((guint8 *)ptr)[0x0B] = ((guint8 *)ptr)[9];

	wrinkle_packet(ptr, len);

	return write(sok, ptr, len);
}
