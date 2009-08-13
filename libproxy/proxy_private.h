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

/*
 * Proxy stuff used only inside this library.
 */

#ifndef __PROXY_PRIVATE_H__
#define __PROXY_PRIVATE_H__

AyProxyData *default_proxy;

#ifdef __cplusplus
extern "C" {
#endif

int socks4_connect	( int sock, const char *host, int port, AyProxyData *proxy );
int socks5_connect	( int sock, const char *host, int port, AyProxyData *proxy );
int http_connect	( int sock, const char *host, int port, AyProxyData *proxy );

#ifdef __cplusplus
}
#endif

#endif
