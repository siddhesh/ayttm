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

#ifndef __DIALOG__
#define __DIALOG__

#include "llist.h"
#ifdef __cplusplus
extern "C" {
#endif

/*dialog.c*/

void eb_do_dialog( const char *message, const char *title, 
		void (*action)(void *data, int result), 
		void *data );

void do_list_dialog( const char *message, const char *title, const char **list, 
		void (*action)(const char *text, void *data), 
		void *data );

void do_llist_dialog( const char *message, const char *title, const LList *list, 
		void (*action)(const char *text, void *data), 
		void *data );

void do_text_input_window( const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data );

void do_password_input_window( const char *title, const char *value, 
		void (*action)(const char *text, void *data), 
		void *data );

void do_text_input_window_multiline( const char *title, const char *value, 
		int ismulti, int ispassword, 
		void (*action)(const char *text, void *data), 
		void *data );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
