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

typedef struct _toc_conn
{
    int fd;
    int seq_num;
    void * account;
    char server[255];
    short port;
    char *username;
    char *password;
    int input;
} toc_conn;

typedef struct _toc_file_conn
{
	char header1[7];
	char header2[2048];
	int  fd;
	unsigned long amount;
	FILE * file;
	int  handle;
	int  progress;
} toc_file_conn;

void (*icqtoc_new_user)(toc_conn *conn, char * group, char * handle);
void (*icqtoc_new_group)(char * group);
int  (*icqtoc_begin_file_recieve)( const char * filename, unsigned long size );
void (*icqtoc_update_file_status)( int tag, unsigned long progress );
void (*icqtoc_complete_file_recieve)( int tag );
void (*icqtoc_im_in)(toc_conn  * conn, char * user, char * message );
void (*icqtoc_chat_im_in)(toc_conn  * conn, char * id, char * user, char * message );
void (*icqupdate_user_status)(char * user, int online, time_t idle, int evil, int unavailable );
void (*icqtoc_error_message)(char * message);
void (*icqtoc_disconnect)(toc_conn * conn);
void (*icqtoc_chat_invite)(toc_conn * conn, char * id, char * name, 
		      char * sender, char * message );
void (*icqtoc_join_ack)(toc_conn * conn, char * id, char * name);
void (*icqtoc_join_error)(toc_conn * conn, char * name);
void (*icqtoc_chat_update_buddy)(toc_conn * conn, char * id, 
		                             char * user, int online );
void (*icqtoc_file_offer)( toc_conn * conn, char * nick, char * ip, short port,
		                      char * cookie, char * filename );

void (*icqtoc_user_info)(toc_conn  * conn, char * user, char * message );
void (*icqtoc_logged_in)(toc_conn *conn);
int  (*icqtoc_async_socket)(char *host, int port, void *cb, void *data);

void icqtoc_callback( toc_conn * conn );
int icqtoc_signon( const char * username, const char * password,
		const char * server, short port, const char * info );
void icqtoc_send_keep_alive( toc_conn * conn );
void icqtoc_signoff( toc_conn * conn );
void icqtoc_send_im( toc_conn * conn, const char * username, const char * message );
void icqtoc_get_info( toc_conn * conn, const char * user );
void icqtoc_add_buddies( toc_conn * conn, const char * group, LList * list );
void icqtoc_add_buddy( toc_conn * conn, char * user, const char * group );
void icqtoc_set_idle( toc_conn * conn, int idle );
void icqtoc_set_away( toc_conn * conn, const char * message);
void icqtoc_invite( toc_conn * conn, const char * id, const char * buddy, const char * message );
void icqtoc_chat_join( toc_conn * conn, const char * chat_room_name );
void icqtoc_chat_send( toc_conn * conn, const char * id, const char * message);
void icqtoc_chat_leave( toc_conn * conn, const char * id );
void icqtoc_chat_accept( toc_conn * conn, const char * id);
void icqtoc_file_accept( toc_conn * conn, const char * nick, const char * ip, short port,
		                      const char * cookie, const char * filename );
void icqtoc_file_cancel( toc_conn * conn, const char * nick, const char * cookie );
void icqtoc_talk_accept( toc_conn * conn, const char * nick, const char * ip, short port, 
					  const char * cookie );
void icqtoc_remove_buddy( toc_conn * conn, const char * user, const char * group );
void icqtoc_add_group(toc_conn *conn, const char *group);
void icqtoc_remove_group(toc_conn *conn, const char *group);



#endif
