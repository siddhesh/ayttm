/*
 * Ayttm
 *
 * Copyright (C) 2003,2009 the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
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

#ifndef __LIBPROXY_H__
#define __LIBPROXY_H__

#include <glib.h>

#define AY_DEFAULT_PROXY_PORT 3128

/* Proxy Types */
typedef enum {
	PROXY_NONE = 0,		/* No Proxy */
	PROXY_HTTP,		/* HTTP */
	PROXY_SOCKS4,		/* Socks 4 */
	PROXY_SOCKS5,		/* Socks 5 */

	PROXY_MAX		/* Number of proxies supported */
} eProxyType;

/* 
 * Various status codes returned by the connection routines to
 * indicate the reason for failure. AY_NONE is success.
 */
typedef enum {
	AY_NONE = 0,
	AY_CONNECTION_REFUSED = -1,
	AY_HOSTNAME_LOOKUP_FAIL = -2,
	AY_PROXY_PERMISSION_DENIED = -3,
	AY_PROXY_AUTH_REQUIRED = -4,
	AY_SOCKS4_UNKNOWN = -5,
	AY_SOCKS4_IDENTD_FAIL = -6,
	AY_SOCKS4_IDENT_USER_DIFF = -7,
	AY_SOCKS4_INCOMPATIBLE_ERROR = -8,
	AY_SOCKS5_CONNECT_FAIL = -9,
	AY_INVALID_CONNECT_DATA = -10,
	AY_CANCEL_CONNECT = -11
} eConnectionStatus;

/* Proxy Structure */
typedef struct {
	char *host;		/* Proxy Host */
	int port;		/* Port */
	eProxyType type;	/* Proxy Type, see eProxyType for types supported */
	char *username;		/* Proxy username. NULL if the proxy requires no authentication */
	char *password;		/* Proxy Password */
} ay_proxy_data;

/* Update connection status in requestor */
typedef void (*ay_status_callback) (const char *msg, void *callback_data);

/* Callback function to be called once connection completes */
typedef void (*ay_connect_callback) (int fd, int error, void *callback_data);

/* The connection function for any proxy */
typedef int (*ay_connect_handler) (int sockfd, gpointer con);

/* This is what we throw all around the place within this 
 * module to get the connection done.
 */
typedef struct {
	char *host;		/* Hostname */
	int port;		/* Port */
	int sockfd;		/* The resultant socket file descriptor */
	ay_proxy_data *proxy;	/* Proxy. NULL if the connection is direct */

	ay_connect_handler connect;	/* Points to the appropriate connection function in case of proxy */

	ay_connect_callback callback;	/* What we should call when connection is completed */
	ay_status_callback status_callback;	/* Update status in the requestor */
	void *cb_data;		/* What the requestor wants us to give back once we're done */

	GAsyncQueue *leash;	/* What the main thread uses to keep track of its thread */
	guint source;		/* Tag for the polling idle function -- useful as a unique ID */
	eConnectionStatus status;	/* Status of connection. See the eConnectionStatus enum */
} ay_connection;

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

/* connect function */
	int ay_connect_host(const char *host, int port, void *cb, void *data,
		void *scb);

/* proxy setting */
	int ay_set_default_proxy(int proxy_type, const char *proxy_host,
		int proxy_port, char *username, char *password);

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif

#ifdef __cplusplus
}
#endif
#endif
