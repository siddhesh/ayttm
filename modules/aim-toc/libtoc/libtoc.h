/*
 * libtoc 
 *
 * Copyright (C) 1999, Torrey Searle <tsearle@uci.edu>
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
#ifndef __LIB_TOC__
#define __LIB_TOC__

#include <stdio.h>
#include "llist.h"

typedef struct _toc_conn {
	int fd;
	int seq_num;
	void *account;
	char server[255];
	short port;
	char *username;
	char *password;
	int input;
} toc_conn;

typedef struct _toc_file_conn {
	char header1[7];
	char header2[2048];
	int fd;
	unsigned long amount;
	FILE *file;
	int handle;
	int progress;
} toc_file_conn;

void (*toc_new_user) (toc_conn *conn, char *group, char *handle);
void (*toc_new_group) (char *group);
int (*toc_begin_file_receive) (const char *filename, unsigned long size);
void (*toc_update_file_status) (int tag, unsigned long progress);
void (*toc_complete_file_receive) (int tag);
void (*toc_im_in) (toc_conn *conn, char *user, char *message);
void (*toc_chat_im_in) (toc_conn *conn, char *id, char *user, char *message);
void (*update_user_status) (toc_conn *conn, char *user, int online, time_t idle,
	int evil, int unavailable);
void (*toc_error_message) (char *message);
void (*toc_disconnect) (toc_conn *conn);
void (*toc_chat_invite) (toc_conn *conn, char *id, char *name,
	char *sender, char *message);
void (*toc_join_ack) (toc_conn *conn, char *id, char *name);
void (*toc_join_error) (toc_conn *conn, char *name);
void (*toc_chat_update_buddy) (toc_conn *conn, char *id,
	char *user, int online);
void (*toc_file_offer) (toc_conn *conn, char *nick, char *ip, short port,
	char *cookie, char *filename);

void (*toc_user_info) (toc_conn *conn, char *user, char *message);
void (*toc_logged_in) (toc_conn *conn);
int (*toc_async_socket) (const char *host, int port, void *cb, void *data);

void toc_callback(toc_conn *conn);
int toc_signon(const char *username, const char *password,
	const char *server, short port, const char *info);
void toc_send_keep_alive(toc_conn *conn);
void toc_signoff(toc_conn *conn);
void toc_send_im(toc_conn *conn, const char *username, const char *message);
void toc_get_info(toc_conn *conn, const char *user);
void toc_add_buddies(toc_conn *conn, const char *group, LList *list);
void toc_add_buddy(toc_conn *conn, char *user, const char *group);
void toc_set_idle(toc_conn *conn, int idle);
void toc_set_away(toc_conn *conn, const char *message);
void toc_invite(toc_conn *conn, const char *id, const char *buddy,
	const char *message);
void toc_chat_join(toc_conn *conn, const char *chat_room_name);
void toc_chat_send(toc_conn *conn, const char *id, const char *message);
void toc_chat_leave(toc_conn *conn, const char *id);
void toc_chat_accept(toc_conn *conn, const char *id);
void toc_file_accept(toc_conn *conn, const char *nick, const char *ip,
	short port, const char *cookie, const char *filename);
void toc_file_cancel(toc_conn *conn, const char *nick, const char *cookie);
void toc_talk_accept(toc_conn *conn, const char *nick, const char *ip,
	short port, const char *cookie);
void toc_remove_buddy(toc_conn *conn, const char *user, const char *group);
void toc_add_group(toc_conn *conn, const char *group);
void toc_remove_group(toc_conn *conn, const char *group);

#endif
