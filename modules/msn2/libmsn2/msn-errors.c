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

#include "msn-errors.h"

static const MsnError errors[] = {
	{MSN_ERROR_INVALID_SYNTAX, "Invalid Command Syntax", 1, 0},
	{MSN_ERROR_INVALID_PARAM, "Invalid command parameters", 0, 0},
	{MSN_ERROR_INVALID_USER, "User does not exist", 0, 0},
	{MSN_ERROR_DOMAIN_MISSING, "Domain name missing in your Passport Id", 0, 0},
	{MSN_ERROR_USER_LOGGED_IN, "You are already logged in", 0, 0},
	{MSN_ERROR_INVALID_BUDDY_PASSPORT, "Invalid buddy Passport Id", 0, 0},
	{MSN_ERROR_INVALID_NICK, "Cannot use this nickname", 0, 0},
	{MSN_ERROR_BUDDY_LIST_FULL, "No more space to add buddies", 0, 0},
	{MSN_ERROR_INVALID_PROFILE, "Invalid profile data", 0, 0},
	{MSN_ERROR_BUDDY_IN_ROOM, "Buddy already in chat room", 0, 0},
	{MSN_ERROR_USER_NOT_ONLINE, "User not online", 0, 0},
	{MSN_ERROR_TOO_MANY_GROUPS, "Too many groups", 0, 0},
	{MSN_ERROR_INVALID_GROUP, "Invalid group", 0, 0},
	{MSN_ERROR_USER_NOT_IN_GROUP, "User not in the specified group", 0, 0},
	{MSN_ERROR_GROUP_NOT_EMPTY, "Group not empty", 0, 0},
	{MSN_ERROR_GROUP_EXISTS, "Group with same name already exists", 0, 0},
	{MSN_ERROR_GROUP_NAME_TOO_LONG, "Group name too long", 0, 0},
	{MSN_ERROR_GROUP_REMOVE_FAIL, "Cannot remove this group", 0, 0},
	{MSN_ERROR_INVALID_GROUP2, "Invalid group", 0, 0},
	{MSN_ERROR_ADDRESS_SYNC_FAIL, "Error in address list synchronization", 1, 1},
	{MSN_ERROR_SWITCHBOADR_ERROR, "Switchboard error", 1, 0},
	{MSN_ERROR_P2P_ERROR, "P2P error", 1, 0},

	{MSN_ERROR_NOT_LOGGED_IN, "Not logged in", 1, 1},

	{MSN_ERROR_CONTACT_LIST_ERROR1, "Error accessing contact list", 0, 0},
	{MSN_ERROR_CONTACT_LIST_ERROR2, "Error accessing contact list", 0, 0},
	{MSN_ERROR_INVALID_ACCOUNT_PERMISSION, "Invalid account permissions", 1, 1},

	{MSN_ERROR_SERVICE_UNAVAILABLE, "Service temporarily unavailable", 1, 0},
	{MSN_ERROR_FEATURE_UNAVAILABLE, "This feature has been disabled", 0, 0},
	{MSN_ERROR_ACCOUNT_BANNED, "Your account has been banned", 1, 1},
	{MSN_ERROR_CHL_FAIL_BUG, "Challenge response failed. File a bug report", 1, 1},

	{MSN_ERROR_SERVER_GOING_DOWN, "Server is going down", 1, 1},

	{MSN_ERROR_CHAT_REQUEST_REJECTED, "Server rejected your chat request", 1, 0},
	{MSN_ERROR_TOO_MANY_SESSIONS, "Too many sessions", 1, 0},
	{MSN_ERROR_UNEXPECTED_REQUEST, "Unexpected Request", 0, 0},

	{MSN_ERROR_TOO_MANY_REQUESTS, "Too many requests. Please slow down", 0, 0},

	{MSN_ERROR_SERVER_TOO_BUSY, "Server too busy", 1, 0},
	{MSN_ERROR_AUTH_FAILED, "Authentication failed", 1, 0},
	{MSN_ERROR_YOU_HIDDEN, "You cannot do this while staying hidden", 0, 0},
	{MSN_ERROR_AUTH_FAILED2, "Authentication failed", 1, 1},
	{MSN_ERROR_BUDDY_ADD_CHAT_FAIL, "Cannot add more buddies to this chat", 0, 0},
	{MSN_ERROR_PASSPORT_UNVERIFIED, "Passport not yet verified", 1, 1},

	{MSN_ERROR_CONNECTION_RESET, "Server closed connection", 1, 0},
	{MSN_LOGIN_FAIL_PASSPORT, "Invalid username", 1, 1},
	{MSN_LOGIN_FAIL_SSO, "Invalid Username or password", 1, 1},
	{MSN_LOGIN_FAIL_VER, "Error during login", 1, 1},
	{MSN_LOGIN_FAIL_OTHER, "Login failed due to unknown reasons", 1, 1},
	{MSN_MESSAGE_DELIVERY_FAILED, "Failed to deliver message", 0, 0},

	{0, "Unknown error", 1, 1}
};

const MsnError *msn_strerror(int error_num)
{
	const MsnError *error = errors;

	while (error->error_num) {
		if (error->error_num == error_num)
			return error;
		error++;
	}

	return error;
}
