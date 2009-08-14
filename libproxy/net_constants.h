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


#ifndef __NET_CONSTANTS_H__
#define __NET_CONSTANTS_H__

/* 
 * Various status codes returned by the connection routines to
 * indicate the reason for failure. AY_NONE is success.
 */
typedef enum
{
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
	AY_CANCEL_CONNECT = -11,
	AY_SSL_CERT_REJECT = -12,
	AY_UI_REQUEST = -13,
	AY_CONNECT_FAILED = -14,
	AY_NUM_ERRORS = -15
} AyConnectionStatus;


typedef enum
{
	AY_CONNECTION_TYPE_PLAIN = 1,
	AY_CONNECTION_TYPE_SSL = 2,
	AY_CONNECTION_TYPE_SERVER = 3
} AyConnectionType ;


#endif
