/*
 * Ayttm module
 *
 * Copyright (C) 2000, Leigh L. Klotz, Jr. <klotz@graflex.org>
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
 *  Message Speak
 *
 */

#ifndef __SPEAK_MESSAGE__
#define __SPEAK_MESSAGE__

#include"account.h"

#ifdef __cplusplus
extern "C" {
#endif

	void speak_message(eb_account *remote, gchar *voice, gchar *message);

	void say_strings(gchar *s1, gchar *s2, gchar *s3);
#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif
