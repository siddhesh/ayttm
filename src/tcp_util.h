/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
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

/*
 *  tcp_util.h
 *  tcp/ip utilities
 *
 */

#ifndef __TCP_UTIL_H__
#define __TCP_UTIL_H__

/**
 * @msg: a status message to be displayed
 * @callback_data: callback_data passed to the ay_socket_new_async function
 **/
typedef void ( *ay_socket_status_callback )(const char *msg, void *callback_data);

/**
 * @fd: file descriptor to connected socket; -1 if connect fails
 * @error: 0 on success, else one of the error values returned by connect
 * @callback_data: callback_data passed to the ay_socket_new_async function
 **/
typedef void ( *ay_socket_callback )(int fd, int error, void *callback_data);

#ifdef __cplusplus
extern "C" {
#endif

int  ay_socket_new(const char * host, int port);
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport)
#endif
int  ay_socket_new_async(const char * host, int port, ay_socket_callback callback, void * callback_data, ay_socket_status_callback status_callback);

void ay_socket_cancel_async(int tag);
int  ay_tcp_readline(char * buff, int maxlen, int fd);
int ay_tcp_writeline(const char *buff, int nbytes, int fd);

#ifdef __cplusplus
}
#endif

#endif
