/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
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
 
#include "intl.h"

#include "messages.h"

#include "ui_message_windows.h"


void	ay_do_info( const char *inTitle, const char *inMessage )
{
	ay_ui_info_window_create( inTitle, inMessage );
}

void	ay_do_warning( const char *inTitle, const char *inMessage )
{
	ay_ui_warning_window_create( inTitle, inMessage );
}

void	ay_do_error( const char *inTitle, const char *inMessage )
{
	ay_ui_error_window_create( inTitle, inMessage );
}
