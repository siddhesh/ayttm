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

#ifndef __LOGS_H__
#define __LOGS_H__

#include <stdio.h>

struct contact;

typedef void *t_log_window_id;

typedef struct _log_file {
	const char *filename;
	int log_started;
	FILE *fp;
	long filepos;
} log_file;

#ifdef __cplusplus
extern "C" {
#endif

/** Create a log window and display logs for a given remote user.

	@param	inRemoteContact		the remote contact
	
	@returns	the log window id
*/
	t_log_window_id ay_log_window_contact_create(struct contact
		*inRemoteContact);

/** Create a log window and display a file.

	@param	inFileName		the name of the log file
	
	@returns	the log window id
*/
	t_log_window_id ay_log_window_file_create(const char *inFileName);

/** Create a struct used to operate on log files.

	@param	inFileName		the name of the log file
	
	@returns	the new log file
*/
	log_file *ay_log_file_create(const char *inFileName);

/** Open a log file.

	This will copy the filename, log_started, and filepos fields to the
	new log_file.
	
	@param	ioLogFile		the log file to open
	@param	inMode			the mode to fopen with
	
	@returns	errno or 0 if no error
*/
	int ay_log_file_open(log_file *ioLogFile, const char *inMode);

/** Write a message to the log file.
	
	@param	ioLogFile		the log file to which we are writing
	@param	inName			the text of the 'name' portion of the message
	@param	inMessage		the text of the message itself
*/
	void ay_log_file_message(log_file *ioLogFile, const char *inMessage);

/** Close a log file.
	
	@param	ioLogFile		the log file which we are closing
*/
	void ay_log_file_close(log_file *ioLogFile);

/** Free all memory allocated for a log file and set it to NULL.
	
	@note This will call ay_log_file_close to close the file if need be
	
	@param	ioLogFile		the log file which we are destroying
*/
	void ay_log_file_destroy(log_file **ioLogFile);

/** Free all memory allocated for a log file and set it to NULL.
	
	@note This will not close the file
	
	@param	ioLogFile		the log file which we are destroying
*/
	void ay_log_file_destroy_no_close(log_file **ioLogFile);

#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif
