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
 * info_window.h
 * header file for the info window
 *
 */

#ifndef __INFO_WINDOW_H__
#define __INFO_WINDOW_H__

#include "account.h"


typedef struct _info_window
{
	GtkWidget * window;
	GtkWidget * info;
        GtkWidget * scrollwindow;
        void (*cleanup)(struct _info_window *);
        void *info_data;
        int info_type;
	struct account * remote_account;
	eb_local_account * local_user;

} info_window;


#ifdef __cplusplus
extern "C" {
#endif

info_window * eb_info_window_new( eb_local_account * local, struct account *);
void eb_info_window_clear(info_window *iw); 
void eb_info_window_add_info( eb_account * remote_account, gchar* text, gint ignore_bg, gint ignore_fg, gint ignore_font );

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif
