/*
 * Ayttm plug-in
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

/****************************
  Speak message
  ***************************/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
#include "speech.h"
#include "globals.h"

#define DEFAULTMESSAGEBODYVOICE "`v2"

void speak_message(eb_account *remote, gchar *voice, gchar *message)
{

	char xbuff[1024];
	char mbuff[256];

	if (do_no_sound_when_away && is_away)
		return;

	strncpy(xbuff, message, sizeof(xbuff));
	strip_html(xbuff);
	// todo: convert "LOL" to "ha ha ha"?
	snprintf(mbuff, sizeof(mbuff), "%s says: ", remote->handle);
	say_strings(mbuff,
		(voice == NULL ? DEFAULTMESSAGEBODYVOICE : voice), xbuff);
}

void say_strings(gchar *s1, gchar *s2, gchar *s3)
{
	pid_t p = fork();
	if (p == 0) {
		execl(SpeechProgramFilename, SpeechProgramFilename,
			s1, s2, s3, NULL);
	}
}
