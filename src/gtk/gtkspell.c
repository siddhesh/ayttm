/* gtkspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
 * Modified to use pspell by Philip S Tellis 2003/04/28
 * Modified to use aspell by pigiron 2009/07/31
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include "intl.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "spellcheck.h"
#include "util.h"
#include "platform_defs.h"


/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024
/* number of suggestions to display on each menu. */
#define MENUCOUNT 20
#define BUGEMAIL "gtkspell-devel@lists.sourceforge.net"

/* because we keep only one copy of the spell program running, 
 * all ispell-related variables can be static.  
 */

GtkTextTag *error_tag = NULL;


static gboolean iswordsep(char c) 
{
	return !isalpha(c) && c != '\'';
}

/*
 * Gets the text and boundaries of the word under a given position
 */
static gboolean get_word_from_pos(GtkTextBuffer* gtktext, GtkTextIter pos, char* buf,
		GtkTextIter *pstart,GtkTextIter *pend) 
{
	GtkTextIter word_start, word_end;
	gchar *tmp;

	if(  !gtk_text_iter_inside_word(&pos) && gtk_text_iter_get_char(&pos)!='\'' )
		return FALSE;

	/* Get the end iter */
	word_end = pos;
	if( !gtk_text_iter_forward_word_end(&word_end) )
		return FALSE;

	if( gtk_text_iter_get_char(&word_end) == '\'' ) {
		gtk_text_iter_forward_char(&word_end);

		if(g_unichar_isalpha(gtk_text_iter_get_char(&word_end))) {
			if(!gtk_text_iter_forward_word_end(&word_end))
				return FALSE;
		}
	}

	if(pend)
		*pend = word_end;

	/* Get the word start iter */
	word_start = pos;
	if( !gtk_text_iter_backward_word_start(&word_start) )
		return FALSE;

	if( gtk_text_iter_get_char(&word_start) == '\'' ) {
		gtk_text_iter_backward_char(&word_start);

		if(g_unichar_isalpha( gtk_text_iter_get_char(&word_start) )) {
			if(!gtk_text_iter_backward_word_start(&word_start))
				return FALSE;
		}
	}

	if(pstart)
		*pstart = word_start;

	gtk_text_iter_forward_char(&word_end);
	tmp = gtk_text_buffer_get_text(gtktext, &word_start, &word_end, FALSE);

	g_strlcpy(buf, tmp, strlen(tmp));

	return TRUE;
}

/*
 * Gets word under the cursor
 */
static gboolean get_curword(GtkTextBuffer* buffer, char* buf,
		GtkTextIter *pstart, GtkTextIter *pend) 
{
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));

	return get_word_from_pos(buffer, iter, buf, pstart, pend);
}

/*
 * Check word under the cursor and mark it if its spelling is incorrect
 */
static gboolean check_at(GtkTextBuffer *gtktext, GtkTextIter *from_pos) 
{
	GtkTextIter start, end;

	char buf[BUFSIZE]={0};

	if (!get_word_from_pos(gtktext, *from_pos, buf, &start, &end)) {
		return FALSE;
	}

	if (ay_spell_check(buf)) {
		gtk_text_buffer_remove_tag(gtktext, error_tag, &start, &end);
		return FALSE;
	} else { 
		gtk_text_buffer_apply_tag(gtktext, error_tag, &start, &end);
		return TRUE;
	}
}

/*
 * Check the entire buffer and mark spelling erors
 */
void gtkspell_check_all(GtkTextBuffer *gtktext) 
{
	GtkTextIter pos;
	GtkTextIter end;

	gtk_text_buffer_get_bounds(gtktext, &pos, &end);
	
	while ( gtk_text_iter_compare(&pos, &end) < 0 ) {
		while ( gtk_text_iter_compare(&pos, &end) <0 &&
				iswordsep(gtk_text_iter_get_char(&pos)))
		{
			gtk_text_iter_forward_char(&pos);
		}
		while ( gtk_text_iter_compare(&pos, &end) <0 &&
				!iswordsep(gtk_text_iter_get_char(&pos)))
		{
			gtk_text_iter_forward_char(&pos);
		}
		gtk_text_iter_backward_char(&pos);
		check_at(gtktext, &pos);
	}
}

/*
 * Text insert callback
 */
static void entry_insert_cb(GtkTextBuffer *gtktext, GtkTextIter *ppos,
			gchar *newtext, guint len, gpointer d) 
{
	GtkTextIter end;

	g_signal_handlers_block_by_func(gtktext, G_CALLBACK(entry_insert_cb), NULL);
	gtk_text_buffer_insert_at_cursor(gtktext, newtext, len);
	g_signal_handlers_unblock_by_func(gtktext, G_CALLBACK(entry_insert_cb), NULL);
	g_signal_stop_emission_by_name(gtktext, "insert-text");

	gtk_text_buffer_get_iter_at_mark(gtktext, ppos, gtk_text_buffer_get_insert(gtktext));
	gtk_text_buffer_get_end_iter(gtktext, &end);

	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (gtk_text_iter_get_offset(ppos) >= 2) {
			gtk_text_iter_backward_chars(ppos, 2);
			check_at(gtktext, ppos);
			gtk_text_iter_forward_chars(ppos, 2);
		}

		/* did we just split a word? */
		if (gtk_text_iter_get_offset(ppos) < gtk_text_iter_get_offset(&end)) {
			gtk_text_iter_forward_char(ppos);
			check_at(gtktext, ppos);
			gtk_text_iter_backward_char(ppos);
		}
	} else {
		/* check as they type, *except* if they're typing at the end (the most
		 * common case.
		 */
		if (gtk_text_iter_get_offset(ppos) < gtk_text_iter_get_offset(&end) &&
				!iswordsep(gtk_text_iter_get_char(ppos)))
		{
			gtk_text_iter_backward_char(ppos);
			check_at(gtktext, ppos);
			gtk_text_iter_forward_char(ppos);
		}
	}

}

/*
 * Text delete callback
 */
static void entry_delete_cb(GtkTextBuffer *gtktext,
		GtkTextIter *start, GtkTextIter *end, gpointer d) 
{
	check_at(gtktext, start);
}

/*
 * Replace word with selected spelling suggestion
 */
static void replace_word(GtkWidget *w, gpointer d) 
{
	GtkTextIter start, end;

	int newword_len;
	const gchar *newword;
	char buf[BUFSIZE]={0};
	int offset;

	/* we don't save their position, 
	 * because the cursor is moved by the click. */

	newword = gtk_label_get_text(GTK_LABEL(GTK_BIN(w)->child));
	get_curword(GTK_TEXT_BUFFER(d), buf, &start, &end);

	offset = gtk_text_iter_get_offset(&start);

	newword_len = strlen(newword);

	gtk_text_buffer_delete(GTK_TEXT_BUFFER(d), &start, &end);
	
	gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(d), &start, offset);
	gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(d), &start);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(d), &start, newword, newword_len);
}

/*
 * Add spelling suggestions into a menu.
 */
static GtkMenu *make_menu(GList *l, GtkTextBuffer *gtktext) 
{
	GtkWidget *menu, *item;
	char *caption;
	menu = gtk_menu_new(); 
	{
		caption = g_strdup_printf(_("Not in dictionary: %s"), (char*)l->data);
		item = gtk_menu_item_new_with_label(caption);
		/* I'd like to make it so this item is never selectable, like
		 * the menu titles in the GNOME panel... unfortunately, the GNOME
		 * panel creates their own custom widget to do this! */
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		item = gtk_menu_item_new();
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		l = l->next;
		if (l == NULL) {
			item = gtk_menu_item_new_with_label(_("(no suggestions)"));
			gtk_widget_show(item);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		} else {
			GtkWidget *curmenu = menu;
			int count = 0;
			do {
				if (l->data == NULL && l->next != NULL) {
					count = 0;
					curmenu = gtk_menu_new();
					item = gtk_menu_item_new_with_label(_("Other Possibilities..."));
					gtk_widget_show(item);
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
					gtk_menu_shell_append(GTK_MENU_SHELL(curmenu), item);
					l = l->next;
				} else if (count > MENUCOUNT) {
					count -= MENUCOUNT;
					item = gtk_menu_item_new_with_label("More...");
					gtk_widget_show(item);
					gtk_menu_shell_append(GTK_MENU_SHELL(curmenu), item);
					curmenu = gtk_menu_new();
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
				}
				item = gtk_menu_item_new_with_label((char*)l->data);
				g_signal_connect(item, "activate",
						G_CALLBACK(replace_word), gtktext);
				gtk_widget_show(item);
				gtk_menu_shell_append(GTK_MENU_SHELL(curmenu), item);
				count++;
			} while ((l = l->next) != NULL);
		}
	}
	return GTK_MENU(menu);
}

/*
 * Position the popup menu at the cursor
 */
void position_popup_menu(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	GdkEventButton *eb = data;

	if(eb) {
		*x = eb->x_root;
		*y = eb->y_root;
	}
	else {
		GdkRectangle location;
		GtkTextIter cur_pos;
		int wx, wy;

		GtkTextView *text_view = g_object_get_data(G_OBJECT(menu), "parent_textview");
		GtkTextBuffer *gtktext = gtk_text_view_get_buffer(text_view);
		GdkWindow *parent = gtk_widget_get_parent_window(GTK_WIDGET(text_view));

		gdk_window_get_position(parent, &wx, &wy);

		gtk_text_buffer_get_iter_at_mark(gtktext, &cur_pos,
				gtk_text_buffer_get_insert(gtktext));

		gtk_text_view_get_iter_location(text_view, &cur_pos, &location);

		gtk_text_view_buffer_to_window_coords(text_view, GTK_TEXT_WINDOW_WIDGET,
				location.x, location.y, x, y);

		*x += GTK_WIDGET(text_view)->allocation.x + wx + location.width/2;
		*y += GTK_WIDGET(text_view)->allocation.y + wy + location.height/2;
	}
}

/*
 * Popup a menu filled with spelling suggestions
 */
static void popup_menu(GtkTextView *text_view, GdkEventButton *eb) 
{
	char buf[BUFSIZE]={0};
	GtkTextBuffer *gtktext = gtk_text_view_get_buffer(text_view);
	GtkTextIter pos;
	int bx, by, trailing;
	int button, event_time;

	GList *list;
	
	if(eb) {
		button = eb->button;
		event_time = eb->time;

		gtk_text_view_window_to_buffer_coords(text_view, GTK_TEXT_WINDOW_TEXT,
				eb->x, eb->y, &bx, &by);
		gtk_text_view_get_iter_at_position(text_view, &pos, &trailing, bx, by);
		gtk_text_buffer_place_cursor(gtktext, &pos);

		get_word_from_pos(gtktext, pos, buf, NULL, NULL);
	}
	else {
		button = 0;
		event_time = gtk_get_current_event_time();
		get_curword(gtktext, buf, NULL, NULL);
	}


	if(ay_spell_check(buf))
		return;

	list = llist_to_glist( ay_spell_check_suggest(buf), 1 );
	list = g_list_prepend(list, strdup(buf));
	if (list != NULL) {
		GtkMenu *menu = make_menu(list, gtktext);
		g_object_set_data(G_OBJECT(menu), "parent_textview", text_view);
		gtk_menu_popup(menu, NULL, NULL, position_popup_menu, eb, button, event_time);
		while (list) {
			GList * l = g_list_next(list);
			g_free(list->data);
			g_list_free_1(list);
			list = l;
		}
	}
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor, 
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu. 
 * HACK HACK HACK. 
 */
/*
 * UPDATE: Nothing whacky about it anymore. Move along.
 */

static gboolean button_press_intercept_cb(GtkTextView *gtktext, GdkEventButton *event, gpointer d) {

	if(event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		popup_menu(gtktext, event);
		return TRUE;
	}

	return FALSE;
}

static gboolean popup_menu_callback(GtkTextView *text_view)
{
	popup_menu(text_view, NULL);
	return TRUE;
}

/*
 * Get rid of all the spelling error notifications
 */
void gtkspell_uncheck_all(GtkTextBuffer *gtktext) 
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(gtktext, &start, &end);
	gtk_text_buffer_remove_tag(gtktext, error_tag, &start, &end);
}

/*
 * The fun starts here.
 */
void gtkspell_attach(GtkTextView *text_view) 
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
	error_tag = gtk_text_buffer_create_tag(buffer, "error_tag",
			"underline", PANGO_UNDERLINE_ERROR, NULL);

	g_signal_connect(buffer, "insert-text", G_CALLBACK(entry_insert_cb), NULL);
	g_signal_connect_after(buffer, "delete-range", G_CALLBACK(entry_delete_cb), NULL);
	g_signal_connect(text_view, "button-press-event",
			G_CALLBACK(button_press_intercept_cb), NULL);
	g_signal_connect(text_view, "popup-menu",
			G_CALLBACK(popup_menu_callback), NULL);
}

/*
 * The fun ends here.
 */
void gtkspell_detach(GtkTextView *gtktext) 
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(gtktext);
	
	g_signal_handlers_disconnect_by_func(buffer, G_CALLBACK(entry_insert_cb), NULL);
	g_signal_handlers_disconnect_by_func(buffer, G_CALLBACK(entry_delete_cb), NULL);
	g_signal_handlers_disconnect_by_func(gtktext, G_CALLBACK(button_press_intercept_cb), NULL);

	gtkspell_uncheck_all(buffer);
}

