/*
 * mem_util.h: memory handling utility functions
 *
 * taken from libyahoo2
 *
 * Copyright (C) 2002, Philip S Tellis <philip . tellis AT gmx . net>
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

#ifndef __MEM_UTIL_H__
#define __MEM_UTIL_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_GLIB
# include <glib.h>

# define ay_free(x)	if(x) {g_free(x); x=NULL;}

# define ay_new		g_new
# define ay_new0	g_new0
# define ay_renew	g_renew

# define ay_memdup	g_memdup
# define ay_strsplit	g_strsplit
# define ay_strfreev	g_strfreev

#else
/* No glib - we use our own */

# include <stdlib.h>
# include <stdarg.h>

# define ay_free(x)		if(x) {free(x); x=NULL;}

# define ay_new(type, n)	(type *)malloc(sizeof(type) * (n))
# define ay_new0(type, n)	(type *)calloc((n), sizeof(type))
# define ay_renew(type, mem, n)	(type *)realloc(mem, n)

void * ay_memdup(const void * addr, int n);
char ** ay_strsplit(const char * str, const char * sep, int nelem);
void ay_strfreev(char ** vector);

int strncasecmp(const char * s1, const char * s2, size_t n);
int strcasecmp(const char * s1, const char * s2);

char * strdup(const char *s);

#endif

#include "llist.h"

#if !defined(TRUE) && !defined(FALSE)
enum {FALSE, TRUE};
#endif

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

/* 
 * The following three functions return newly allocated memory.
 * You must free it yourself
 */
char * ay_string_append(char * str, char * append);
char * ay_str_to_utf8(const char * in);
char * ay_utf8_to_str(const char * in);
#endif

