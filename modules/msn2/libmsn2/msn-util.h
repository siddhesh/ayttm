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

#ifndef _MSN_UTIL_H_
#define _MSN_UTIL_H_

#include <stdlib.h>
#include "llist.h"

#define m_new0(type,num) ((type *)calloc(num,sizeof(type)))
#define m_renew(type,ptr,newsize) ((type *)realloc(ptr,sizeof(type)*newsize))

extern char *ext_base64_encode(unsigned char *data, int len);
extern unsigned char *ext_base64_decode(char *data, int *len);

char *msn_urlencode(const char *in);
char *msn_urldecode(const char *in);

#endif
