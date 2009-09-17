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

#ifndef _MSN_H_
#define _MSN_H_

typedef struct _MsnAccount MsnAccount;
typedef struct _MsnMessage MsnMessage;
typedef struct _MsnConnection MsnConnection;
typedef struct _MsnIM MsnIM;
typedef struct _MsnBuddy MsnBuddy;
typedef struct _MsnGroup MsnGroup;
typedef struct _MsnError MsnError;

typedef struct _SBPayload SBPayload;

void msn_login(MsnAccount *ma);
void msn_logout(MsnAccount *ma);

void msn_invite_buddy(MsnAccount *ma, MsnBuddy *buddy, const char *message);

void msn_get_contacts(MsnAccount *ma);
int msn_get_status_num(const char *state);
void msn_send_chl_response(MsnAccount *ma, const char *seed);
void msn_set_initial_presence(MsnAccount *ma, int state);

void msn_set_state(MsnAccount *ma, int state);

typedef enum {
	MSN_STATE_ONLINE,
	MSN_STATE_HIDDEN,
	MSN_STATE_BUSY,
	MSN_STATE_IDLE,
	MSN_STATE_BRB,
	MSN_STATE_AWAY,
	MSN_STATE_ON_PHONE,
	MSN_STATE_LUNCH,
	MSN_STATE_OFFLINE,
	MSN_STATES_COUNT
} MsnState;

extern const char *msn_state_strings[];

#include "msn-login.h"
#include "msn-contacts.h"
#include "msn-errors.h"
#include "msn-util.h"

#endif
