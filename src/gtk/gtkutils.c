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

#include <assert.h>
 
#include <gdk/gdkprivate.h>

#include "gtkutils.h"
#include "gtk_globals.h"

#include "pixmaps/ayttm.xpm"


/* saves ayttm icon info so we don't have to redo it every time */
static GdkPixmap *s_ayttm_icon_pm = NULL;
static GdkBitmap *s_ayttm_icon_bm = NULL;


static void	s_set_option( GtkWidget *inWidget, int *ioData )
{
	*ioData = !(*ioData);	
}

GtkWidget	*gtkut_button( const char *inText, int *inValue, GtkWidget *inPage )
{
	GtkWidget	*button = gtk_check_button_new_with_label( inText );
	
	assert( inValue != NULL );
	
	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(button), *inValue );
	gtk_box_pack_start( GTK_BOX(inPage), button, FALSE, FALSE, 0 );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(s_set_option), inValue );
	gtk_widget_show( button );
	
	return( button );
}

GtkWidget *gtkut_create_icon_button( const char *inLabel, char **inXPM, GtkWidget *inParent )
{
	GtkWidget	*button = NULL;
	GtkWidget	*label = NULL;
	GtkWidget	*hbox = NULL;
	GtkWidget	*iconwid = NULL;
	GdkPixmap	*icon = NULL;
	GdkBitmap	*mask = NULL;
	GtkStyle	*style = NULL;
	const int	min_width = 80;
	int			width = min_width;

	
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( hbox );

	style = gtk_widget_get_style( inParent );
	
	icon = gdk_pixmap_create_from_xpm_d( inParent->window, &mask, &style->bg[GTK_STATE_NORMAL], inXPM );
	iconwid = gtk_pixmap_new( icon, mask );
	gtk_widget_show( iconwid );
	
	label = gtk_label_new( inLabel );
	gtk_widget_show( label );

	gtk_box_pack_start( GTK_BOX(hbox), iconwid, FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX(hbox), label, TRUE, TRUE, 0 );
	
	button = gtk_button_new();
	gtk_container_add( GTK_CONTAINER(button), hbox );
	if ( button->requisition.width > width )
		width = button->requisition.width;
	gtk_widget_set_usize( button, width, -1 );
	gtk_widget_show( button );
	
	return( button );
}

void	gtkut_set_pixmap_from_xpm( char **inXPM, GtkPixmap *outPixmap )
{
	GdkPixmap	*tpx = NULL;
	GdkBitmap	*tbx = NULL;
	
	
	if ( (inXPM == NULL) || (outPixmap == NULL) )
		return;
		
	tpx = gdk_pixmap_create_from_xpm_d( statuswindow->window, &tbx, NULL, inXPM );
	gtk_pixmap_set( outPixmap, tpx, tbx );
}

void	gtkut_widget_get_uposition( GtkWidget *inWidget, int *outXpos, int *outYpos )
{
	int	x = 0;
	int	y = 0;
	int	sx = 0;
	int	sy = 0;

	
	if ( outXpos != NULL )
		*outXpos = 0;
		
	if ( outYpos != NULL )
		*outYpos = 0;
	
	if ( (inWidget == NULL) || (inWidget->window == NULL) )
		return;	

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_root_origin( inWidget->window, &x, &y );

	x %= sx;
	if ( x < 0 )
		x = 0;
		
	y %= sy;
	if ( y < 0 )
		y = 0;
		
	if ( outXpos != NULL )
		*outXpos = x;
		
	if ( outYpos != NULL )
		*outYpos = y;
}

GtkWidget	*gtkut_create_label_button( const char *inButtonText, GtkSignalFunc inSignalFunc, void *inCallbackData )
{
	GtkWidget	*label_hbox = NULL;
	GtkWidget	*label = NULL;
	GtkWidget	*button = NULL;
	int			button_width = 100;	// minimum width for the button
	
	
	label_hbox = gtk_hbox_new( FALSE, 0 );
	gtk_widget_show( label_hbox );
	
	label = gtk_label_new( inButtonText );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX(label_hbox), label, TRUE, TRUE, 0 );
		
	button = gtk_button_new();
	gtk_container_add( GTK_CONTAINER(button), label_hbox );
	gtk_widget_show( button );
		
	if ( button->requisition.width > button_width )
		button_width = button->requisition.width;
		
	gtk_widget_set_usize( button, button_width, -1 );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", inSignalFunc, inCallbackData );
	
	return( button );
}

GSList	*gtkut_add_radio_button_to_group( GSList *ioGroup, GtkWidget *inParentBox,
			const char *inButtonText, int inIsSelected,
			GtkSignalFunc inSignalFunc, void *inCallbackData )
{
	GtkWidget	*radio = NULL;

	
	if ( inButtonText == NULL )
		radio = gtk_radio_button_new( ioGroup );
	else
		radio = gtk_radio_button_new_with_label( ioGroup, inButtonText );
	
	gtk_widget_show( radio );

	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio), inIsSelected );

	gtk_box_pack_start( GTK_BOX(inParentBox), radio, FALSE, FALSE, 0 );

	gtk_signal_connect( GTK_OBJECT(radio), "clicked", inSignalFunc, inCallbackData );

	return( gtk_radio_button_group( GTK_RADIO_BUTTON(radio) ) );
}


GtkWidget	*gtkut_check_button( GtkWidget *inParentBox, const char *inButtonText, int inIsSelected,
	GtkSignalFunc inSignalFunc, void *inCallbackData )
{
	GtkWidget *button = NULL;

		
	if ( inButtonText == NULL )
		button = gtk_check_button_new();
	else
		button = gtk_check_button_new_with_label( inButtonText );
	
	gtk_widget_show( button );
	
	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(button), inIsSelected );
	
	gtk_box_pack_start( GTK_BOX(inParentBox), button, FALSE, FALSE, 0 );
	
	gtk_signal_connect( GTK_OBJECT(button), "clicked", inSignalFunc, inCallbackData );
	
	return( button );
}


GtkWidget	*gtkut_create_menu_button( GtkMenu *inMenu, const char *inLabel,
	GtkSignalFunc inSignalFunc, void *inCallbackData )
{
	GtkWidget *button = NULL;

	
	assert( inMenu != NULL );
	
	if ( inLabel == NULL )
		button = gtk_menu_item_new();
	else
		button = gtk_menu_item_new_with_label( inLabel ) ;
		
	if ( inSignalFunc != NULL )
		gtk_signal_connect( GTK_OBJECT(button), "activate", inSignalFunc, inCallbackData );
		
	gtk_menu_append( inMenu, button );
	gtk_widget_show( button );

	return( button );
}

GtkWidget	*gtkut_attach_submenu( GtkMenu *inMenu, const char *inLabel,
			     GtkWidget *inSubmenu, int inActive )
{
	GtkWidget *button = NULL;

	
	if ( inLabel == NULL )
		button = gtk_menu_item_new();
	else
		button = gtk_menu_item_new_with_label( inLabel ) ;
	
	gtk_widget_set_sensitive( button, inActive );

	gtk_menu_append( GTK_MENU(inMenu), button );

	gtk_menu_item_set_submenu( GTK_MENU_ITEM(button), inSubmenu );
	gtk_widget_show( button );

	return( button );
}

void	gtkut_set_window_icon( GdkWindow *inWindow, gchar **inXPM )
{
	GdkAtom		icon_atom;
	glong		data[2];
	GdkPixmap	*the_pixmap = s_ayttm_icon_pm;
	GdkBitmap	*the_bitmap = s_ayttm_icon_bm;

		
	if ( inXPM != NULL )
	{
		the_pixmap = gdk_pixmap_create_from_xpm_d( inWindow, &the_bitmap,
					NULL, inXPM );
	}
	else
	{
		if ( s_ayttm_icon_pm == NULL )
		{
			s_ayttm_icon_pm = gdk_pixmap_create_from_xpm_d( inWindow, &s_ayttm_icon_bm,
						NULL, (gchar **)ayttm_xpm );
			
			the_pixmap = s_ayttm_icon_pm;
			the_bitmap = s_ayttm_icon_bm;
		}
	}

#ifndef __MINGW32__
	data[0] = ((GdkPixmapPrivate *)the_pixmap)->xwindow;
	data[1] = ((GdkPixmapPrivate *)the_bitmap)->xwindow;

	icon_atom = gdk_atom_intern( "KWM_WIN_ICON", FALSE);
	gdk_property_change( inWindow, icon_atom, icon_atom,
			32, GDK_PROP_MODE_REPLACE,
			(guchar *)data, 2);
#endif
	
	gdk_window_set_icon( inWindow, NULL, the_pixmap, the_bitmap );
}
