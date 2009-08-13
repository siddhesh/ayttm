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


#ifndef __COMMON_NET_H__
#define __COMMON_NET_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Lookup address */
struct addrinfo *lookup_address(const char *host, int port_num, int family);

/* Connect to an address on a port */
int connect_address( const char *host, int port_num );

/* Receive a line of data from socket */
int ay_recv_line( int sock, char **resultp );

#ifdef __cplusplus
}
#endif

#endif
