/* gtkspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
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

#ifndef __gtkspell_h__
#define __gtkspell_h__

/* PLEASE NOTE that this API is unstable and subject to change. */
#define GTKSPELL_VERSION "0.3.1"

#ifdef __cplusplus
extern "C" {
#endif

	extern void gtkspell_attach(GtkTextView *text);
/* Attach GtkSpell to a GtkText Widget.
 * This enables checking as you type and the popup menu.
 *
 * Arguments:
 *  - "text" is the widget to which GtkSpell should attach.
 *
 * Example:
 *  GtkWidget *text;
 *  text = gtk_text_new(NULL, NULL); 
 *  gtk_text_set_editable(GTK_TEXT(text), TRUE);
 *  gtkspell_attach(GTK_TEXT(text));
 */

	void gtkspell_detach(GtkTextView *gtktext);
/* Detach GtkSpell from a GtkText widget.
 * 
 * Arguments:
 *  - "text" is the widget from which GtkSpell should detach.
 * 
 */

	void gtkspell_check_all(GtkTextBuffer *gtktext);
/* Check and highlight the misspelled words.
 * Note that the popup menu will not work unless you gtkspell_attach().
 *
 * Arguments:
 *  - "text" is the widget to check.
 */

	void gtkspell_uncheck_all(GtkTextBuffer *gtktext);
/* Remove all of the highlighting from the widget.
 *
 * Arguments:
 *  - "text" is the widget to check.
 */

#ifdef __cplusplus
}
#endif
#endif				/* __gtkspell_h__ */
