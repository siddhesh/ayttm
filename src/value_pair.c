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
#include <string.h>
#include <stdlib.h>
#include "value_pair.h"
#include "util.h"

char * value_pair_get_value( LList * pairs, const char * key )
{
	LList * node;
	for( node = pairs; node; node=node->next) {
		value_pair * vp = node->data;
		if(!strcasecmp(key, vp->key))
			return unescape_string(vp->value);
	}
	return NULL;
}

void value_pair_free( LList * pairs )
{
	LList * node;
	for( node = pairs; node; node = node->next )
		free(node->data);
	l_list_free(pairs);
}

void value_pair_print_values( LList * pairs, FILE * file, int indent )
{
	LList * node;
	int i;
	
	for( node = pairs; node; node = node->next ) {
		value_pair * vp = node->data;

		for( i = 0; i < indent; i++ )
			fprintf( file, "\t" );

		/* XXX do we need to escape vp->value first? */
		fprintf(file, "%s=\"%s\"\n", vp->key, vp->value);
	}
}

LList * value_pair_add(LList * list, const char * key, const char * value)
{
	char * tmp = escape_string(value);
	value_pair * vp;
	
	if (value_pair_get_value(list, key) != NULL)
		list = value_pair_remove(list, key);

	vp = calloc(1, sizeof(value_pair));
	strncpy(vp->key, key, MAX_PREF_NAME_LEN);
	strncpy(vp->value,tmp, MAX_PREF_LEN);
	free(tmp);
	return l_list_append(list, vp);
}

LList * value_pair_remove( LList *pairs, const char *key )
{
	LList * node;
	for( node = pairs; node; node=node->next) {
		value_pair * vp = node->data;
		if(vp && !strcasecmp(key, vp->key)) {
			pairs=l_list_remove_link(pairs, node);
			free(node->data);
			free(node);
			break;
		}
	}
	return pairs;
}

LList * value_pair_update(LList * pairs, LList * new_list)
{

	LList * node;
	for( node = new_list; node; node=node->next) {
		value_pair * vp = node->data;
		pairs = value_pair_remove(pairs, vp->key);
		pairs = value_pair_add(pairs, vp->key, vp->value);
	}
	return pairs;
}

