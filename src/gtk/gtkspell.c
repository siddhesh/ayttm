/* gtkspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
 * Modified to use pspell by Philip S Tellis 2003/04/28
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
#include "platform_defs.h"


/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024
/* number of suggestions to display on each menu. */
#define MENUCOUNT 20
#define BUGEMAIL "gtkspell-devel@lists.sourceforge.net"

/* because we keep only one copy of the spell program running, 
 * all ispell-related variables can be static.  
 */

/* FIXME? */
static GdkColor highlight = { 0, 255*256, 0, 0 };

static void entry_insert_cb(GtkText *gtktext, gchar *newtext, guint len, guint *ppos, gpointer d);

static GList* misspelled_suggest(char *word) 
{
	GList *l = NULL;

	/*
	 * TODO: Populate l with a list of suggestions
	 */

	return l;
}

static gboolean iswordsep(char c) 
{
	return !isalpha(c) && c != '\'';
}

static gboolean get_word_from_pos(GtkText* gtktext, int pos, char* buf, int *pstart, int *pend) 
{
	int start, end;

	if (iswordsep(GTK_TEXT_INDEX(gtktext, pos)))
		return FALSE;

	for (start = pos; start >= 0; --start) {
		if (iswordsep(GTK_TEXT_INDEX(gtktext, start)))
			break;
	}
	start++;

	for (end = pos; end <= gtk_text_get_length(gtktext); end++) {
		if (iswordsep(GTK_TEXT_INDEX(gtktext, end)))
			break;
	}

	if (buf) {
		for (pos = start; pos < end; pos++) 
			buf[pos-start] = GTK_TEXT_INDEX(gtktext, pos);
		buf[pos-start] = 0;
	}

	if (pstart)
		*pstart = start;
	if (pend)
		*pend = end;

	return TRUE;
}

static gboolean get_curword(GtkText* gtktext, char* buf, int *pstart, int *pend) 
{
	int pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	return get_word_from_pos(gtktext, pos, buf, pstart, pend);
}

static void change_color(GtkText *gtktext, int start, int end, GdkColor *color) 
{
	char *newtext = gtk_editable_get_chars(GTK_EDITABLE(gtktext), start, end);
	gtk_text_freeze(gtktext);
	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext), 
			GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
	
	gtk_text_set_point(gtktext, start);

	if(newtext && end-start > 0)
	{
		gtk_text_forward_delete(gtktext, end-start);
		gtk_text_insert(gtktext, NULL, color, NULL, newtext, end-start);
	}

	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext), 
			GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
	gtk_text_thaw(gtktext);
	g_free(newtext);
}

static gboolean check_at(GtkText *gtktext, int from_pos) 
{
	int start, end;
	char buf[BUFSIZE];

	if (!get_word_from_pos(gtktext, from_pos, buf, &start, &end)) {
		return FALSE;
	}

	if (ay_spell_check(buf)) {
		change_color(gtktext, start, end, &(GTK_WIDGET(gtktext)->style->fg[0]));
		return FALSE;
	} else { 
		if (highlight.pixel == 0) {
			/* add an entry for the highlight in the color map. */
			GdkColormap *gc = gtk_widget_get_colormap(GTK_WIDGET(gtktext));
			gdk_colormap_alloc_color(gc, &highlight, FALSE, TRUE);;
		}
		change_color(gtktext, start, end, &highlight);
		return TRUE;
	}
}

void gtkspell_check_all(GtkText *gtktext) 
{
	guint origpos;
	guint pos = 0;
	guint len;
	float adj_value;
	
	len = gtk_text_get_length(gtktext);

	adj_value = gtktext->vadj->value;
	gtk_text_freeze(gtktext);
	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	while (pos < len) {
		while (pos < len && iswordsep(GTK_TEXT_INDEX(gtktext, pos)))
			pos++;
		while (pos < len && !iswordsep(GTK_TEXT_INDEX(gtktext, pos)))
			pos++;
		if (pos > 0)
			check_at(gtktext, pos-1);
	}
	gtk_text_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
}

static void entry_insert_cb(GtkText *gtktext, gchar *newtext, guint len, guint *ppos, gpointer d) 
{
	int origpos;

	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(entry_insert_cb),
					 NULL);
	gtk_text_insert(GTK_TEXT(gtktext), NULL,
			&(GTK_WIDGET(gtktext)->style->fg[0]), NULL, newtext, len);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(entry_insert_cb),
					   NULL);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "insert-text");
	*ppos += len;

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (*ppos >= 2)
			check_at(gtktext, *ppos-2);

		/* did we just split a word? */
		if (*ppos < gtk_text_get_length(gtktext))
			check_at(gtktext, *ppos+1);
	} else {
		/* check as they type, *except* if they're typing at the end (the most
		 * common case.
		 */
		if (*ppos < gtk_text_get_length(gtktext) && 
				!iswordsep(GTK_TEXT_INDEX(gtktext, *ppos)))
			check_at(gtktext, *ppos-1);
	}

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
}

static void entry_delete_cb(GtkText *gtktext, gint start, gint end, gpointer d) 
{
	int origpos;

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	check_at(gtktext, start-1);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
	/* this is to *UNDO* the selection, in case they were holding shift
	 * while hitting backspace. */
}

static void replace_word(GtkWidget *w, gpointer d) 
{
	int start, end, newword_len;
	char *newword;
	char buf[BUFSIZE];

	/* we don't save their position, 
	 * because the cursor is moved by the click. */

	gtk_text_freeze(GTK_TEXT(d));

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), &newword);
	get_curword(GTK_TEXT(d), buf, &start, &end);

	newword_len = strlen(newword);

	gtk_text_set_point(GTK_TEXT(d), end);
	gtk_text_backward_delete(GTK_TEXT(d), end-start);
	gtk_text_insert(GTK_TEXT(d), NULL, NULL, NULL, newword, newword_len);

	gtk_text_thaw(GTK_TEXT(d));

	gtk_editable_set_position(GTK_EDITABLE(d), start+newword_len);
}

static GtkMenu *make_menu(GList *l, GtkText *gtktext) 
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
		gtk_menu_append(GTK_MENU(menu), item);

		item = gtk_menu_item_new();
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);

		l = l->next;
		if (l == NULL) {
			item = gtk_menu_item_new_with_label(_("(no suggestions)"));
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(menu), item);
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
					gtk_menu_append(GTK_MENU(curmenu), item);
					l = l->next;
				} else if (count > MENUCOUNT) {
					count -= MENUCOUNT;
					item = gtk_menu_item_new_with_label("More...");
					gtk_widget_show(item);
					gtk_menu_append(GTK_MENU(curmenu), item);
					curmenu = gtk_menu_new();
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
				}
				item = gtk_menu_item_new_with_label((char*)l->data);
				gtk_signal_connect(GTK_OBJECT(item), "activate",
						GTK_SIGNAL_FUNC(replace_word), gtktext);
				gtk_widget_show(item);
				gtk_menu_append(GTK_MENU(curmenu), item);
				count++;
			} while ((l = l->next) != NULL);
		}
	}
	return GTK_MENU(menu);
}

static void popup_menu(GtkText *gtktext, GdkEventButton *eb) 
{
	char buf[BUFSIZE];
	GList *list;

	get_curword(gtktext, buf, NULL, NULL);

	list = misspelled_suggest(buf);
	if (list != NULL) {
		gtk_menu_popup(make_menu(list, gtktext), NULL, NULL, NULL, NULL, eb->button, eb->time);
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
static gint button_press_intercept_cb(GtkText *gtktext, GdkEvent *e, gpointer d) {
	GdkEventButton *eb;
	gboolean retval;

	if (e->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) e;

	if (eb->button != 3) return FALSE;

	/* forge the leftclick */
	eb->button = 1;

	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext), 
			GTK_SIGNAL_FUNC(button_press_intercept_cb), d);
	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "button-press-event",
			e, &retval);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext), 
			GTK_SIGNAL_FUNC(button_press_intercept_cb), d);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "button-press-event");

	/* now do the menu wackiness */
	popup_menu(gtktext, eb);
	return TRUE;
}

void gtkspell_uncheck_all(GtkText *gtktext) 
{
	int origpos;
	char *text;
	float adj_value;

	adj_value = gtktext->vadj->value;
	gtk_text_freeze(gtktext);
	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);
	gtk_text_set_point(gtktext, 0);
	gtk_text_forward_delete(gtktext, gtk_text_get_length(gtktext));
	gtk_text_insert(gtktext, NULL, NULL, NULL, text, strlen(text));
	gtk_text_thaw(gtktext);

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);
}

void gtkspell_attach(GtkText *gtktext) 
{
	gtk_signal_connect(GTK_OBJECT(gtktext), "insert-text",
		GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
	gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
		GTK_SIGNAL_FUNC(entry_delete_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
			GTK_SIGNAL_FUNC(button_press_intercept_cb), NULL);
}

void gtkspell_detach(GtkText *gtktext) 
{
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
		GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
		GTK_SIGNAL_FUNC(entry_delete_cb), NULL);
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext), 
			GTK_SIGNAL_FUNC(button_press_intercept_cb), NULL);

	gtkspell_uncheck_all(gtktext);
}

