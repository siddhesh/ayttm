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

#include "intl.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "util.h"
#include "prefs.h"
#include "globals.h"

#include "ui_log_window.h"

t_log_window_id ay_log_window_contact_create(struct contact *inRemoteContact)
{
	return (ay_ui_log_window_contact_create(inRemoteContact));
}

t_log_window_id ay_log_window_file_create(const char *inFileName)
{
	return (ay_ui_log_window_file_create(inFileName));
}

log_file *ay_log_file_create(const char *inFileName)
{
	log_file *new_file = NULL;

	assert(inFileName != NULL);

	new_file = calloc(1, sizeof(log_file));

	new_file->filename = strdup(inFileName);
	new_file->log_started = 0;
	new_file->fp = NULL;
	new_file->filepos = 0;

	return (new_file);
}

int ay_log_file_open(log_file *ioLogFile, const char *inMode)
{
	assert(ioLogFile != NULL);
	assert(ioLogFile->filename != NULL);

	ioLogFile->fp = fopen(ioLogFile->filename, inMode);

	if (ioLogFile->fp == NULL) {
		perror(ioLogFile->filename);
		ioLogFile->filepos = 0;

		return (errno);
	}

	ioLogFile->filepos = ftell(ioLogFile->fp);

	eb_debug(DBG_CORE, "ay_log_file_open set filepos of [%s] to [%lu]\n",
		ioLogFile->filename, ioLogFile->filepos);

	return (0);
}

void ay_log_file_message(log_file *ioLogFile, const char *inMessage)
{
	char *my_message = NULL;
	const int stripHTML = iGetLocalPref("do_strip_html");

	if (!ioLogFile || !ioLogFile->fp)
		return;

	my_message = strdup(inMessage);

	if (stripHTML)
		strip_html(my_message);

	if (!ioLogFile->log_started) {
		time_t my_time = time(NULL);

		fprintf(ioLogFile->fp, _("%sConversation started on %s %s\n"),
			(stripHTML ? "" : "<HR WIDTH=\"100%\"><B>"),
			g_strchomp(asctime(localtime(&my_time))),
			(stripHTML ? "" : "</B>"));

		fflush(ioLogFile->fp);

		ioLogFile->log_started = 1;
	}

	fprintf(ioLogFile->fp, "%s\n", my_message);

	fflush(ioLogFile->fp);

	free(my_message);
}

void ay_log_file_close(log_file *ioLogFile)
{
	if (ioLogFile == NULL)
		return;

	if ((ioLogFile->fp != NULL) && ioLogFile->log_started) {
		if (iGetLocalPref("do_logging")) {
			time_t my_time = time(NULL);
			const int stripHTML = iGetLocalPref("do_strip_html");

			fprintf(ioLogFile->fp,
				_("%sConversation ended on %s %s\n"),
				(stripHTML ? "" : "<B>"),
				g_strchomp(asctime(localtime(&my_time))),
				(stripHTML ? "" : "</B>"));
		}
	}

	if (ioLogFile->fp != NULL) {
		fclose(ioLogFile->fp);
		ioLogFile->fp = NULL;
	}
}

static void ay_log_file_destroy_ext(log_file **ioLogFile, int close)
{
	log_file *the_log_file = NULL;

	assert(ioLogFile != NULL);
	assert(*ioLogFile != NULL);

	the_log_file = *ioLogFile;

	if (close)
		ay_log_file_close(the_log_file);

	if (the_log_file->filename != NULL) {
		free((char *)the_log_file->filename);
		the_log_file->filename = NULL;
	}

	memset(the_log_file, 0, sizeof(log_file));

	*ioLogFile = NULL;
}

void ay_log_file_destroy(log_file **ioLogFile)
{
	ay_log_file_destroy_ext(ioLogFile, 1);
}

void ay_log_file_destroy_no_close(log_file **ioLogFile)
{
	ay_log_file_destroy_ext(ioLogFile, 0);
}
