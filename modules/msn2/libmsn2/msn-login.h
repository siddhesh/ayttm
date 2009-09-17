/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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

#ifndef _MSN_LOGIN_H_
#define _MSN_LOGIN_H_

#define MSN_PROTOCOL_VERSION	"MSNP15"

#define MSN_LOCALE		"0x0409"
#define MSN_OS_NAME		"winnt"
#define MSN_OS_VERSION		"5.1"
#define MSN_ARCH		"i386"
#define MSN_CLIENT		"MSNMSGR"
#define MSN_CLIENT_VER		"8.5.1302"
#define MSN_OFFICIAL_CLIENT	"BC01"

#define MSN_DEFAULT_HOST	"messenger.hotmail.com"
#define MSN_DEFAULT_PORT	"1863"

#define MSN_PROTO_CVR0		"CVR0"
#define MSN_AUTH_SSO		"SSO"
#define MSN_AUTH_INITIAL	"I"
#define MSN_AUTH_SUBSEQUENT	"S"

extern char msn_host[512];
extern char msn_port[8];

#endif
