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

#ifndef __EB_INPUT_LIST_H__
#define __EB_INPUT_LIST_H__

#include "llist.h"

enum
{
	EB_INPUT_CHECKBOX,
	EB_INPUT_ENTRY
};


typedef struct _chekbox_input
{
	char * name;
	char * label;
	int * value;
	int saved_value;
} checkbox_input;

typedef struct _entry_input
{
	char * name;
	char * label;
	char * value;
	void * entry; /* GtkWidget */
} entry_input;

typedef struct _input_list
{
	int type;
	union
	{
		checkbox_input checkbox;
		entry_input entry;
	} widget;
	struct _input_list * next;
} input_list;
		

void eb_input_render(input_list * il, void * box); /* GtkWidget * box */
void eb_input_cancel(input_list * il);
void eb_input_accept(input_list * il);
LList *eb_input_to_value_pair(input_list * il);
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) void eb_update_from_value_pair(input_list *il, LList *vp);
#else
extern void eb_update_from_value_pair(input_list *il, LList *vp);
#endif

#endif
