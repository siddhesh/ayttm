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

#include "gtkutils.h"
#include "gtk_globals.h"

void gtkut_set_pixmap_from_xpm(char **xpm, GtkPixmap *pixmap)
{
	if (xpm && pixmap) {
		GdkPixmap *tpx= NULL;
		GdkBitmap *tbx= NULL;
		tpx = gdk_pixmap_create_from_xpm_d(statuswindow->window, &tbx, 
						NULL, xpm);
		gtk_pixmap_set(pixmap, tpx, tbx);
	}
}

void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py)
{
	gint x, y;
	gint sx, sy;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	/* gdk_window_get_root_origin ever return *rootwindow*'s position */
	gdk_window_get_root_origin(widget->window, &x, &y);

	x %= sx; if (x < 0) x = 0;
	y %= sy; if (y < 0) y = 0;
	*px = x;
	*py = y;
}
