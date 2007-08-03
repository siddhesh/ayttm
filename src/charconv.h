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

#ifndef _CHAR_CONV_C_
#define _CHAR_CONV_C_


/*
** Name:    Str2Utf8
** Purpose: convert a string in UTF-8 format
** Input:   in     - the string to convert
** Output:  a new string in UTF-8 format
*/
char *StrToUtf8(const char *in);

/*
** Name:    Utf8ToStr
** Purpose: revert UTF-8 string conversion
** Input:   in     - the string to decode
** Output:  a new decoded string
*/
char *Utf8ToStr(const char *in);


#endif
