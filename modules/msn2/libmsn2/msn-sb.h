/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
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

#ifndef _MSN_SB_H_
#define _MSN_SB_H_

#include "msn.h"

typedef void (*SBCallback) (MsnConnection *mc, int error, void *data);

struct _SBPayload {
	char *challenge;
	char *room;

	char *session_id;
	void *data;
	SBCallback callback;

	int num_members;
	int incoming;
} ;

void msn_connect_sb(MsnAccount *ma, const char *host, int port);

void msn_connect_sb_with_info(MsnConnection *mc, char *room_name, void *data);
void msn_get_sb(MsnAccount *ma, char *room_name, void *data, SBCallback callback);

void msn_got_sb_join(MsnConnection *mc);

void msn_sb_disconnect(MsnConnection *sb);
void msn_sb_disconnected(MsnConnection *sb);

#endif

