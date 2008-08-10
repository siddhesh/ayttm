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

/*
 * chat_window.c
 * implementation for the conversation window
 * This is the window where you will be doing most of your talking :)
 *
 */

#include "auto_complete.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#include "debug.h"
#include "globals.h"


LList *auto_complete_session_words = NULL;


char * complete_word( LList * l, const char *begin, int *choice)
{
	char * complete = NULL;
	LList *possible = NULL;
	LList *cur = NULL;
	int list_length = 0;
	*choice = TRUE;
	
	if (begin == NULL)
		return NULL;
	
	if (strlen(begin) == 0)
		return NULL;
	
	for (cur = l; cur && cur->data; cur = cur->next) {
		char * curnick = (char *) cur->data;
		if (!strncmp(curnick, begin, strlen(begin))) {
			possible = l_list_prepend(possible, curnick);
		}
		list_length++;
	}
	if (possible == NULL)
		return NULL;
	else if (l_list_length(possible) == 1) {
		complete = strdup((char *)possible->data);
		l_list_free(possible);
		possible=NULL;
		*choice = FALSE;
		return complete;
	} else {
		int i = 0;
		char *last_good = NULL;
		for (i = 0; i < 255; i ++) {
			int common = TRUE;
			char * sub = malloc(i+1);
			memset(sub,0,i+1);
			strncpy(sub,(char*)possible->data, i);
			cur = possible;
			while (cur && cur->data) {
				char * compareto = cur->data;
				if (strncmp(compareto, sub, strlen(sub))) {
					common = FALSE;
					break;
				} 
				cur = cur->next;
			}
			if (common == TRUE) {
				if (last_good) free(last_good);
				last_good = sub;
			} else {
				l_list_free(possible);
				free(sub);
				return last_good;
			}
		}
	}
	if (possible)
		l_list_free(possible);
	return complete;
}


int chat_auto_complete (GtkWidget *entry, LList *words, GdkEventKey *event)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
	GtkTextIter insert_iter;
	GtkTextIter start, sel_start_iter, sel_end_iter, b_iter;
	int x;
	gboolean selected;
	const GdkModifierType	modifiers = event->state & 
		(GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

	GtkTextMark *insert_mark = gtk_text_buffer_get_insert(buffer);

	gtk_text_buffer_get_iter_at_mark(buffer, &insert_iter, insert_mark);

	x = gtk_text_iter_get_offset(&insert_iter);

	if (modifiers && modifiers != GDK_SHIFT_MASK)
		return FALSE;		

	if ((event->keyval >= GDK_Shift_L && event->keyval <= GDK_Meta_R) ) {
		return FALSE;
	}
	
	selected = gtk_text_buffer_get_selection_bounds(buffer,
			&sel_start_iter, &sel_end_iter);
	gtk_text_buffer_get_start_iter(buffer, &start);

	if ( selected && gtk_text_iter_compare(&sel_start_iter, &start) > 0 &&
			gtk_text_iter_compare(&sel_start_iter, &insert_iter) < 0 )
	{
		insert_iter = sel_start_iter;
	}
	if ( x > 0 ) {
		char * word= gtk_text_buffer_get_text(buffer, &start, &insert_iter, FALSE);
		char * last_word = strrchr(word, ' ');
		char * comp_word = NULL;
		char * nick = NULL;
		int choice = TRUE;

		if (last_word == NULL)
			last_word = strrchr(word, '\n');

		if (last_word == NULL)
			last_word = word;

		if (last_word == NULL) {
			return FALSE;
		}
		if (last_word != word) 
			last_word++;

		comp_word = malloc(strlen(last_word)+2);
		sprintf(comp_word, "%s%c",last_word,event->keyval);
		eb_debug(DBG_CORE, "word caught: %s\n",comp_word);
		nick = complete_word(words, comp_word, &choice);

		if (nick != NULL) {
			int b = strlen(word) - strlen(last_word);
			gtk_text_buffer_get_iter_at_offset(buffer, &b_iter, b);

			/*int inserted=b;*/
			if (gtk_text_iter_compare(&start, &sel_start_iter) < 0 ) { 
				gtk_text_buffer_delete_selection(buffer, FALSE, TRUE);
				gtk_text_buffer_get_iter_at_offset(buffer, &b_iter, b);
				gtk_text_buffer_get_iter_at_offset(buffer, &insert_iter, x);
			}

			gtk_text_buffer_delete(buffer, &b_iter, &insert_iter);
			gtk_text_buffer_get_iter_at_offset(buffer, &b_iter, b);

			eb_debug(DBG_CORE, "insert %s at %d\n",nick, b);
			gtk_text_buffer_insert(buffer, &b_iter, nick, strlen(nick));
			b+=strlen(nick);
			gtk_text_buffer_get_iter_at_offset(buffer, &b_iter, b);
			gtk_text_buffer_get_iter_at_offset(buffer, &insert_iter, x);
			
			if (!choice) {
				gtk_text_buffer_insert(buffer, &b_iter, " ", strlen(" "));
				b++;
				gtk_text_buffer_get_iter_at_offset(buffer, &b_iter, b);
			}
		
			gtk_text_buffer_place_cursor(buffer, &b_iter);
			gtk_text_buffer_get_iter_at_offset(buffer, &insert_iter, x+1);

			gtk_text_buffer_select_range(buffer, &insert_iter, &b_iter);
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key-press-event");
			return TRUE;
		}
	}
	return FALSE;
}


void chat_auto_complete_insert(GtkWidget *entry, GdkEventKey *event)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
	GtkTextIter insert_iter;
	GtkTextIter start, end, sel_start_iter, sel_end_iter;
	int x;
	gboolean selected;

	GtkTextMark *insert_mark = gtk_text_buffer_get_insert(buffer);

	gtk_text_buffer_get_iter_at_mark(buffer, &insert_iter, insert_mark);

	x = gtk_text_iter_get_offset(&insert_iter);
	
	if ((event->keyval >= GDK_Shift_L && event->keyval <= GDK_Meta_R) ) {
		return;
	}
	
	selected = gtk_text_buffer_get_selection_bounds(buffer,
			&sel_start_iter, &sel_end_iter);
	gtk_text_buffer_get_bounds(buffer, &start, &end);

	if ( selected && gtk_text_iter_compare(&sel_start_iter, &start) > 0 &&
			gtk_text_iter_compare(&sel_start_iter, &insert_iter) < 0 )
	{
		insert_iter = sel_start_iter;
	}
	if ( x > 0 ) {
		char * word= gtk_text_buffer_get_text(buffer, &start, &insert_iter, FALSE);
		char * last_word = NULL;
		/*char * nick = NULL;*/
		/*int choice = TRUE;*/
		int found=0;
		LList *l = auto_complete_session_words ;

		/* trim last space */
		while (strlen (word) && (word[strlen(word)-1] == ' ' || ispunct(word[strlen(word)-1]))) {
			word[strlen(word)-1]='\0';
		}

		last_word = strrchr(word, ' ');
		if (last_word == NULL)
			last_word = strrchr(word, '\n');

		if (last_word == NULL)
			last_word = word;

		if (last_word == NULL)
			return;
			
		if (last_word != word) 
			last_word++;

		if(last_word == NULL)
			return;

		while(l && l->data) {
			if (!strcmp((char *)l->data, last_word)) {
				found=1;
				break;
			}
			l = l->next;
		}
		if (!found) {
			eb_debug(DBG_CORE, "word inserted: %s\n",last_word);
			auto_complete_session_words = l_list_prepend(auto_complete_session_words, last_word);
		}
	}
	
}


void chat_auto_complete_validate (GtkWidget *entry)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
	GtkTextMark *insert_mark = gtk_text_buffer_get_insert(buffer);
	GtkTextIter insert_iter;
	GtkTextIter word_end_iter;

	gtk_text_buffer_get_iter_at_mark(buffer, &insert_iter, insert_mark);
	word_end_iter = insert_iter;

	gtk_text_iter_forward_word_end(&word_end_iter);
        /* We want the space at the end of the autocomplete as well, so... */ 
	gtk_text_iter_forward_char(&word_end_iter);     

	gtk_text_buffer_select_range(buffer, &word_end_iter, &word_end_iter);
	g_signal_stop_emission_by_name(G_OBJECT(entry), "key-press-event");
}

