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

#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#define eb_debug(type, format, args...) {if(type) {EB_DEBUG(__FUNCTION__, __FILE__, __LINE__, format, ##args);}}
#ifdef __STDC__
	int EB_DEBUG(const char *func, char *file, int line, const char *fmt,
		...);
#else
	int EB_DEBUG(const char *func, char *file, int line, const char *fmt,
		va_alist);
#endif

#ifdef __cplusplus
}				/* end extern "C" */
#endif
#endif
