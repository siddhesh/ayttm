/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 *
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
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
 * Moved out of msn module since msn doesn't need it anymore.
 * UTF good, everything else bad ;)
 */

#include <stdio.h>
#include "charconv.h"
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "globals.h"

/*
** Name:    Str2Utf8
** Purpose: convert a string in UTF-8 format
** Input:   in     - the string to convert
** Output:  a new string in UTF-8 format
*/
char *StrToUtf8(const char *in)
{
	unsigned int n, i = 0;
	char *result = NULL;

	/* Is it already in the right format? */
	if(g_utf8_validate(in, -1, NULL))
		return (char *)in;

	eb_debug(DBG_CORE, "Converting %s\n", in);

	result = (char *) malloc(strlen(in) * 2 + 1);

	/* convert a string to UTF-8 Format */
	for (n = 0; n < strlen(in); n++) {
		unsigned char c = (unsigned char)in[n];

		if (c < 128) {
			result[i++] = (char) c;
		}
		else {
			result[i++] = (char) ((c >> 6) | 192);
			result[i++] = (char) ((c & 63) | 128);
		}
	}
	result[i] = '\0';
	return result;
}

/*
** Name:    Utf8ToStr
** Purpose: revert UTF-8 string conversion
** Input:   in     - the string to decode
** Output:  a new decoded string
*/
char *Utf8ToStr(const char *in)
{
	int i = 0;
	unsigned int n;
	char *result = NULL;

	/* Is it in UTF? */
	if( !g_utf8_validate(in, -1, NULL) )
		return (char *)in;

	if(in == NULL)
		return "";

	result = (char *) malloc(strlen(in) + 1);

	/* convert a string from UTF-8 Format */
	for (n = 0; n < strlen(in); n++) {
		unsigned char c = in[n];

		if (c < 128) {
                        result[i++] = (char) c;
		}
		else {
			result[i++] = (c << 6) | (in[++n] & 63);
		}
	}
	result[i] = '\0';
	return result;
}
