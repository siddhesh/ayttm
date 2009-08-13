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


#ifndef __PROXY_H__
#define __PROXY_H__

#define AY_DEFAULT_PROXY_PORT 3128

/* Proxy Types */
typedef enum
{
	PROXY_NONE = 0,	/* No Proxy */
	PROXY_HTTP,	/* HTTP */
	PROXY_SOCKS4,	/* Socks 4 */
	PROXY_SOCKS5,	/* Socks 5 */
	
	PROXY_MAX	/* Number of proxies supported */
} AyProxyType ;


/* Proxy Structure */
typedef struct
{
	char *host;		/* Proxy Host */
	int port;		/* Port */
	AyProxyType type;	/* Proxy Type, see eProxyType for types supported */
	char *username;		/* Proxy username. NULL if the proxy requires no authentication */
	char *password;		/* Proxy Password */
} AyProxyData ;

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

int ay_proxy_set_default ( AyProxyType type, const char *host, int port, char *username, char *password) ;

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif


#ifdef __cplusplus
}
#endif

#endif
