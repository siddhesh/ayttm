/*
 * Ayttm
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

#ifndef __BROWSER_H__
#define __BROWSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MINGW32__

#include <gdk/gdk.h>

void open_url(GdkWindow *w, char *url);

#else

void open_url(void *w, char *url);

#endif	/* __MINGW32__ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
