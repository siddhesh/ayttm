/*
 * Ayttm
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * Source Code taked from GAIM, by Mark Spencer
 *
 * some code: (most in this file)
 * Copyright (C) 1996 Netscape Communications Corporation, all rights reserved.
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Dec-94.
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
 * This code is mainly taken from Netscape's sample implementation of
 * their protocol.  Nifty.
 *
 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>

#ifdef __MINGW32__
# include <gdk/gdkwin32.h>
#else
# include <stdlib.h>
#endif

#ifdef _WIN32
# include <shellapi.h>
# include <process.h>
#endif

#include "prefs.h"
#include "globals.h"


#include "logs.h"

#ifndef _WIN32
	
	
void open_url(void *w, char *url) {
	char *browser = NULL;
	char command[1281];
	char *url_pos = NULL;
	char esc_url[1024];
	int i=0,j=1;
	
	if (!strncmp("log://", url, 6))
	{
		/*internal handling*/
		ay_log_window_file_create( url+6 );
		return;
	}

	esc_url[0]='"'; j=1;
	if (url[0]=='\'' || url[0]=='"')
		i++;
	for (; i< strlen(url) && j < 1023; i++,j++) {
		switch(url[i]) {
			case '$':
			case '\'':
			case '"':
			case ' ':
			case '\\':
				if (i==strlen(url)-1)
					break;
				esc_url[j]='\\';
				j++;
				esc_url[j]=url[i];
				break;
			default:
				esc_url[j]=url[i];
		}
	}
	esc_url[j++]='"';
	esc_url[j++]=0;
	
	if (iGetLocalPref("use_alternate_browser"))
		browser = cGetLocalPref("alternate_browser");
	
	if (!browser || (strlen(browser) == 0)) {
		browser = "mozilla %s";
	}
	url_pos = strstr(browser, "%s");
	/*
	 * if alternate_browser contains a %s, then we put
	 * the url in place of the %s, else, put the url
	 * at the end.
	 */
	if(url_pos) {
		int pre_len = url_pos-browser;
		strncpy(command, browser, pre_len);
		command[pre_len] = 0;
		strncat(command, esc_url, 1280 - pre_len);
		strncat(command, url_pos+2, 1280 - strlen(command));
		strncat(command, " &", 1280 - strlen(command));
	} else {
		strncpy(command, browser, 1280);
		strncat(command," ",1280-strlen(command));
		strncat(command,esc_url,1280-strlen(command));
		strncat(command, " &", 1280 - strlen(command));
	}
	eb_debug(DBG_CORE, "launching %s\n", command);
	system(command);
}

#else

void open_url(GdkWindow *w, char *url) {

	if (!w)
		return;

	if (!strncmp("log://", url, 6)) {
		/*internal handling*/
		ay_log_window_file_create( url+6 );
		return;
	}

	ShellExecute(GDK_DRAWABLE_XID(w),"open",url,NULL,"C:\\",0);
}

#endif
