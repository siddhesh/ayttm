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
 * auto_complete.h
 */

#ifndef __AUTO_COMPLETE_H__
#define __AUTO_COMPLETE_H__

#include <gtk/gtk.h>
#include "llist.h"

extern LList *auto_complete_session_words ;

void chat_auto_complete_validate	( GtkWidget * ) ;
int  chat_auto_complete			( GtkWidget *, LList *, GdkEventKey * ) ;
void chat_auto_complete_insert		( GtkWidget *, GdkEventKey * ) ;


#endif
