/*
 * EveryBuddy 
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
 * gtk_globals.h
 *
 */


#ifndef __GTK_GLOBALS_H__
#define __GTK_GLOBALS_H__

#include <gtk/gtk.h>

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

/* defined in status.c */
extern GtkWidget *statuswindow;
extern GtkWidget *away_menu;

/* current parent (for error dialogs) */
/*    - defined in main.c */
extern GtkWidget *current_parent_widget;

/* tabbed chat tab switching hotkeys */
extern GdkDeviceKey accel_next_tab;
extern GdkDeviceKey accel_prev_tab;

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif

#include "globals.h"
#endif
