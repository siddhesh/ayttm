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
#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>


#ifdef __cplusplus
extern "C" {
#endif

void gtkut_set_pixmap_from_xpm(char **xpm, GtkPixmap *pixmap);
void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py);

/** Create a radio button and add it to a group.

	@param	ioGroup			the group to which we want to add this radio button
	@param	inParentBox		the v-or-h-box in which the radio button is to be inserted
	@param	inButtonText	the text of the button
	@param	inIsSelected	is this radio button currently selected?
	@param	inSignalFunc	the callback function to call when it is clicked
	@param	inCallbackData	the callback data
	
	@returns	the list 'ioGroup' with the new radio button added to it
*/
GSList	*gtkut_add_radio_button_to_group( GSList *ioGroup, GtkWidget *inParentBox,
			const char *inButtonText, int inIsSelected,
			GtkSignalFunc inSignalFunc, void *inCallbackData );

/** Create a check button.

	@param	inParentBox		the v-or-h-box in which the button is to be inserted
	@param	inButtonText	the text of the button
	@param	inIsSelected	is this button currently selected?
	@param	inSignalFunc	the callback function to call when it is clicked
	@param	inCallbackData	the callback data
	
	@returns	the new button
*/
GtkWidget	*gtkut_check_button( GtkWidget *inParentBox, const char *inButtonText, int inIsSelected,
	GtkSignalFunc inSignalFunc, void *inCallbackData );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
