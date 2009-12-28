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
#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif

	GtkWidget *gtkut_button(const char *inText, int *inValue,
		GtkWidget *inPage, GtkAccelGroup *inAccelGroup);

/** Create a widget with an xpm.
	
	@param	inXPM		the pixmap
	@param	inParent	the parent whose window we'll use to create the pixmap
	
	@returns the new widget
*/
	GtkWidget *gtkut_create_icon_widget(const char **inXPM,
		GtkWidget *inParent);

/** Create a button with an xpm and optional label.

	@param	inLabel		optional label for the button
	@param	inXPM		the pixmap
	@param	inParent	the parent whose window we'll use to create the pixmap
	
	@returns the new button
*/
	GtkWidget *gtkut_create_icon_button(const char *inLabel,
		const char **inXPM, GtkWidget *inParent);

/** Create a button with a stock icon and custom label.

	@param	inLabel		optional label for the button
	@param	inXPM		the pixmap
	@param	inParent	the parent whose window we'll use to create the pixmap
	
	@returns the new button
*/
	GtkWidget *gtkut_stock_button_new_with_label(const char *label,
		const char *stock);

	void gtkut_set_pixbuf_from_xpm(const char **inXPM,
		GdkPixbuf **outPixbuf);

	void gtkut_set_pixbuf(eb_local_account *ela, const char **inXPM,
		GdkPixbuf **outPixbuf);

/** Get a widget's position.

	@param	inWidget	which widget?
	@param	outXpos		if not NULL returns the x position
	@param	outYpos		if not NULL returns the y position
*/
	void gtkut_widget_get_uposition(GtkWidget *inWidget, int *outXpos,
		int *outYpos);

/** Create a button with a label in it.

	@param	inButtonText	the text of the nutton
	@param	inSignalFunc	the callback function to call when it is clicked
	@param	inCallbackData	the callback data
	
	@returns	the new button
*/
	GtkWidget *gtkut_create_label_button(const char *inButtonText,
		GCallback inSignalFunc, void *inCallbackData);

/** Create a radio button and add it to a group.

	@param	ioGroup			the group to which we want to add this radio button
	@param	inParentBox		the v-or-h-box in which the radio button is to be inserted
	@param	inButtonText	the text of the button
	@param	inIsSelected	is this radio button currently selected?
	@param	inSignalFunc	the callback function to call when it is clicked
	@param	inCallbackData	the callback data
	
	@returns	the list 'ioGroup' with the new radio button added to it
*/
	GSList *gtkut_add_radio_button_to_group(GSList *ioGroup,
		GtkWidget *inParentBox, const char *inButtonText,
		int inIsSelected, GCallback inSignalFunc, void *inCallbackData);

/** Create a check button.

	@param	inParentBox		the v-or-h-box in which the button is to be inserted
	@param	inButtonText	the text of the button
	@param	inIsSelected	is this button currently selected?
	@param	inSignalFunc	the callback function to call when it is clicked
	@param	inCallbackData	the callback data
	
	@returns	the new button
*/
	GtkWidget *gtkut_check_button(GtkWidget *inParentBox,
		const char *inButtonText, int inIsSelected,
		GCallback inSignalFunc, void *inCallbackData);

/** Create a menu button and append it to a menu.

	@param	inMenu			the menu into which the button is to be appended
	@param	inLabel			the text of the button
	@param	inSignalFunc	the callback function to call when it is activated
	@param	inCallbackData	the callback data
	
	@returns	the new button
*/
	GtkWidget *gtkut_create_menu_button(GtkMenu *inMenu,
		const char *inLabel, GCallback inSignalFunc,
		void *inCallbackData);

/** Create a menu button, append it to a menu, and attach a submenu to it.

	@param	inMenu			the menu into which the button & submenu is to be appended
	@param	inLabel			the text of the button
	@param	inSubmenu		the submenu to attach
	@param	inActive		is the new button active?
	
	@returns	the new button
*/
	GtkWidget *gtkut_attach_submenu(GtkMenu *inMenu, const char *inLabel,
		GtkWidget *inSubmenu, int inActive);

/** Set a window's icon.

	@param	inWindow		the window whose icon we are setting
	@param	inXPM			the xpm we are setting it to [if NULL then use standard ayttm icon]
*/
	void gtkut_set_window_icon(GtkWindow *inWindow, const gchar **inXPM);

#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif
