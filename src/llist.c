/*
 * llist.c: linked list routines
 *
 * Some code copyright (C) 2002-2003, Philip S Tellis <philip . tellis AT gmx . net>
 * Other code copyright Meredydd Luff <meredydd AT everybuddy.com>
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
 * Some of this code was borrowed from elist.c in the eb-lite sources
 *
 */

#include <stdlib.h>

#include "llist.h"

LList *l_list_append(LList * list, void *data)
{
	LList *n;
	LList *new_list = malloc(sizeof(LList));
	LList *attach_to = NULL;

	new_list->next = NULL;
	new_list->data = data;

	for (n = list; n != NULL; n = n->next) {
		attach_to = n;
	}

	if (attach_to == NULL) {
		new_list->prev = NULL;
		return new_list;
	} else {
		new_list->prev = attach_to;
		attach_to->next = new_list;
		return list;
	}
}

LList *l_list_prepend(LList * list, void *data)
{
	LList *n = malloc(sizeof(LList));

	n->next = list;
	n->prev = NULL;
	n->data = data;
	if (list)
		list->prev = n;

	return n;
}

LList *l_list_concat(LList * list, LList * add)
{
	LList *l;

	if(!list)
		return add;

	if(!add)
		return list;

	for (l = list; l->next; l = l->next)
		;

	l->next = add;
	add->prev = l;

	return list;
}

LList *l_list_remove(LList * list, void *data)
{
	LList *n;

	for (n = list; n != NULL; n = n->next) {
		if (n->data == data) {
			LList *new=l_list_remove_link(list, n);
			free(n);
			return new;
		}
	}

	return list;
}

/* Warning */
/* link MUST be part of list */
LList *l_list_remove_link(LList * list, const LList * link)
{
	if (!link)
		return list;

	if (link->next)
		link->next->prev = link->prev;
	if (link->prev)
		link->prev->next = link->next;

	if (link == list)
		list = link->next;
	
	return list;
}

int l_list_length(const LList * list)
{
	int retval = 0;
	const LList *n = list;

	for (n = list; n != NULL; n = n->next) {
		retval++;
	}

	return retval;
}

/* well, you could just check for list == NULL, but that would be
 * implementation dependent
 */
int l_list_empty(const LList * list)
{
	if(!list)
		return 1;
	else
		return 0;
}

int l_list_singleton(const LList * list)
{
	if(!list || list->next)
		return 0;
	return 1;
}

LList *l_list_copy(LList * list)
{
	LList *n;
	LList *copy = NULL;

	for (n = list; n != NULL; n = n->next) {
		copy = l_list_append(copy, n->data);
	}

	return copy;
}

void l_list_free_1(LList * list)
{
	free(list);
}

void l_list_free(LList * list)
{
	LList *n = list;

	while (n != NULL) {
		LList *next = n->next;
		free(n);
		n = next;
	}
}

LList *l_list_find(LList * list, const void *data)
{
	LList *l;
	for (l = list; l && l->data != data; l = l->next)
		;

	return l;
}

void l_list_foreach(LList * list, LListFunc fn, void * user_data)
{
	for (; list; list = list->next)
		fn(list->data, user_data);
}

LList *l_list_find_custom(LList * list, const void *data, LListCompFunc comp)
{
	LList *l;
	for (l = list; l; l = l->next)
		if (comp(l->data, data) == 0)
			return l;

	return NULL;
}

LList *l_list_nth(LList * list, int n)
{
	int i=n;
	for ( ; list && i; list = list->next, i--)
		;

	return list;
}

LList *l_list_insert_sorted(LList * list, void *data, LListCompFunc comp)
{
	LList *l, *n, *prev = NULL;
	if (!list)
		return l_list_append(list, data);

       	n = malloc(sizeof(LList));
	n->data = data;
	for (l = list; l && comp(l->data, n->data) <= 0; l = l->next)
		prev = l;

	if (l) {
		n->prev = l->prev;
		l->prev = n;
	} else
		n->prev = prev;

	n->next = l;

	if(n->prev) {
		n->prev->next = n;
		return list;
	} else {
		return n;
	}
		
}
