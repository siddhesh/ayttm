/*
 * llist.h: linked list routines
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
 */

/*
 * This is a replacement for the GList.  It only provides functions that 
 * we use in Ayttm.  Thanks to Meredyyd from everybuddy dev for doing 
 * most of it.
 */

#ifndef __LLIST_H__
#define __LLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _LList {
	struct _LList *next;
	struct _LList *prev;
	void *data;
} LList;

typedef int (*LListCompFunc) (const void *, const void *);
typedef void (*LListFunc) (void *, void *);

LList *l_list_append(LList * list, void *data);
LList *l_list_prepend(LList * list, void *data);
LList *l_list_remove_link(LList * list, const LList * link);
LList *l_list_remove(LList * list, void *data);

LList *l_list_insert_sorted(LList * list, void * data, LListCompFunc comp);

LList *l_list_copy(LList * list);

LList *l_list_concat(LList * list, LList * add);

LList *l_list_find(LList * list, const void *data);
LList *l_list_find_custom(LList * list, const void *data, LListCompFunc comp);

LList *l_list_nth(LList * list, int n);

void l_list_foreach(LList * list, LListFunc fn, void *user_data);

void l_list_free_1(LList * list);
void l_list_free(LList * list);
int  l_list_length(const LList * list);
int  l_list_empty(const LList * list);
int  l_list_singleton(const LList * list);

#define l_list_next(list)	list->next

#ifdef __cplusplus
}
#endif
#endif
