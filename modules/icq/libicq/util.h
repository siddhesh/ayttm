/* Copyright (C) 1998 Sean Gabriel <gabriel@korsoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __UTIL_H__
#define __UTIL_H__

Contact_Member *contact(guint32 uin);
Contact_Member *contact_from_socket(gint sock);
guint32 Chars_2_DW(guchar *buf);
guint16 Chars_2_Word(guchar *buf);
void DW_2_Chars(guchar *buf, guint32 num);
void Word_2_Chars(guchar *buf, guint16 num);
size_t SOCKWRITE(gint sok, void *ptr, size_t len);
size_t SOCKREAD(gint sok, void *ptr, size_t len);

#endif				/* __UTIL_H__ */
