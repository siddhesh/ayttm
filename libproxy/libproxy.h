/*
 * everybuddy
 *
 * Copyright (C) 1998-1999, Torrey Searle
 * proxy feature by Seb C.
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


#ifndef _INC_LIBPROXY_H
#define _INC_LIBPROXY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

enum eProxyType
{
	PROXY_NONE = 0,
	PROXY_HTTP,
	PROXY_SOCKS4,
	PROXY_SOCKS5,
	
	PROXY_MAX
};

/* gethostbyname function */
extern struct hostent * proxy_gethostbyname(char *host) ;
extern struct hostent * proxy_gethostbyname2(char *host,int type) ;

int proxy_send(int  s,  const  void *msg, int len, unsigned int
		               flags);

int proxy_recv( int s, void * buff, int len, unsigned int flags );


/* connect function */
extern int proxy_connect(int  sockfd, struct sockaddr *serv_addr, int
			 addrlen, void *cb, void *data, void *scb) ;

extern int proxy_connect_host
			(char *host, int port, void *cb, void *data, void *scb) ;

/* proxy setting */
extern int proxy_set_proxy(int proxy_type,char *proxy_host,int proxy_port);
/* proxy user and password setting */
extern int proxy_set_auth(const int required, const char *user, const char *passwd );
/* proxy getting */
extern char *proxy_get_auth();

/* variable for external testing .. Must use proxy_set_proxy to change
   value */ 
extern int proxy_type;
extern char * proxy_host;
extern int proxy_port;

extern int proxy_auth_required;
extern char * proxy_user;
extern char * proxy_password;
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif

#ifdef __cplusplus
}
#endif

#endif
