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

#ifndef __INPUT_LIST_H__
#define __INPUT_LIST_H__

#include "llist.h"

enum {
	EB_INPUT_CHECKBOX,
	EB_INPUT_ENTRY,
	EB_INPUT_PASSWORD,
	EB_INPUT_LIST,
	EB_INPUT_HIDDEN
};

typedef struct _checkbox_input {
	int *value;
	int saved_value;
} checkbox_input;

typedef struct _entry_input {
	char *value;
	void *entry;		/* GtkWidget */
} entry_input;

typedef struct _list_input {
	int *value;
	LList *list;
	void *widget;		/* GtkWidget */
} list_input;

typedef struct _input_list {
	int type;
	char *name;
	char *label;
	char *tooltip;
	union {
		checkbox_input checkbox;
		entry_input entry;
		list_input listbox;
	} widget;

	struct _input_list *next;
} input_list;

#ifdef __cplusplus
extern "C" {
#endif

	LList *eb_input_to_value_pair(input_list *il);

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
	 __declspec(dllimport) void eb_update_from_value_pair(input_list *il,
		LList *vp);
#else
	extern void eb_update_from_value_pair(input_list *il, LList *vp);
#endif

#ifdef __cplusplus
}
#endif
#endif
