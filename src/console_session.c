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

#include "console_session.h"

#include <stdlib.h>
#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include "chat_window.h"
#include "util.h"
#include "status.h"


static void console_session_close(int * session)
{
	eb_input_remove(*((int*)session));
	g_free(session);
}

void console_session_get_command(void *data, int source, eb_input_condition condition )
{
	char * contact_name;
	char * message;
	struct contact * remote_contact;
	short len;
	int pos = 0;
	int ret;

	if(read(source, &len, sizeof(short))<=0)
	{
		console_session_close((int*)data);
		return;
	}
	contact_name = alloca(len);
	if(read(source, contact_name, len)<=0)
	{
		console_session_close((int*)data);
		return;
	}
	if(strcmp(contact_name, "focus-ayttm")) {
		if(read(source, &len, sizeof(short))<=0)
		{
			console_session_close((int*)data);
			return;
		}
		message = alloca(len);
		if(read(source, message, len)<=0)
		{
			console_session_close((int*)data);
			return;
		}

		remote_contact = find_contact_by_nick(contact_name);
		if(!remote_contact)
		{
			ret = -1;
			write(source, &ret, sizeof(int));
			return;
		}
		eb_chat_window_display_contact(remote_contact);
		remote_contact->chatwindow->preferred= 
				find_suitable_remote_account(remote_contact->chatwindow->preferred, 
											remote_contact->chatwindow->contact);
		if(!remote_contact->chatwindow->preferred)
		{
			ret = -2;
			write(source, &ret, sizeof(int));
			return;
		}

		gtk_editable_insert_text(GTK_EDITABLE(remote_contact->chatwindow->entry),
								 message, strlen(message), &pos);
		send_message(NULL, remote_contact->chatwindow);
	} else {
		focus_statuswindow();
	}
	
	ret = 0;
	write(source, &ret, sizeof(int));

}

void console_session_init(void *data, int source, eb_input_condition condition)
{
#ifndef __MINGW32__
	struct sockaddr_un remote;
	int len;
	int sock;
	int * listener = g_new0(int, 1);
	
	sock = accept(source, (struct sockaddr *)&remote, &len);
	*listener = eb_input_add(sock, EB_INPUT_READ,
					console_session_get_command, 
					(gpointer)listener);
#endif
}
