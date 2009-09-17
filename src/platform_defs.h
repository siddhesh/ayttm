/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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

#ifndef __PLATFORM_DEFS_H__
#define __PLATFORM_DEFS_H__

#ifdef __MINGW32__
#define snprintf		_snprintf
#define vsnprintf		_vsnprintf

#define hstrerror		strerror

#define sleep(a)		Sleep(1000*a)
#define usleep(a)		Sleep(a)

#define bcopy(a,b,c)	memcpy(b,a,c)
#define bzero(a,b)		memset(a,0,b)

#define write(a,b,c)	send(a,b,c,0)
#define read(a,b,c)		recv(a,b,c,0)

#define ECONNREFUSED	WSAECONNREFUSED
#define EINPROGRESS		WSAEINPROGRESS
#endif

#ifdef _WIN32
#define mkdir( x, y )	_mkdir( x )
#endif

#ifdef _AIX
#include <glib.h>
#define snprintf		g_snprintf
#define strcasecmp( a, b )	g_strcasecmp( a, b )
#define strncasecmp( a, b, c )	g_strncasecmp( a, b, c )
#endif

#endif
