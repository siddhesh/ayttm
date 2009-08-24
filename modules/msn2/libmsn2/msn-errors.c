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
			{200, "Invalid Command Syntax", 1, 0},
			{201, "Invalid command parameters", 0, 0},
			{205, "User does not exist", 0, 0},
			{206, "Domain name missing in your Passport Id", 0, 0},
			{207, "You are already logged in", 0, 0},
			{208, "Invalid buddy Passport Id", 0, 0},
			{209, "Cannot use this nickname", 0, 0},
			{210, "No more space to add buddies", 0, 0},
			{213, "Invalid profile data", 0, 0},
			{215, "Buddy already in chat room", 0, 0},
			{217, "User not online", 0, 0},
			{223, "Too many groups", 0, 0},
			{224, "Invalid group", 0, 0},
			{225, "User not in the specified group", 0, 0},
			{227, "Group not empty", 0, 0},
			{228, "Group with same name already exists", 0, 0},
			{229, "Group name too long", 0, 0},
			{230, "Cannot remove this group", 0, 0},
			{231, "Invalid group", 0, 0},
			{240, "Error in address list synchronization", 1, 1},
			{280, "Switchboard error", 1, 0},
			{282, "P2P error", 1, 0},

			{302, "Not logged in", 1, 1},

			{402, "Error accessing contact list", 0, 0},
			{403, "Error accessing contact list", 0, 0},
			{420, "Invalid account permissions", 1, 1},

			{500, "Service temporarily unavailable", 1, 0},
			{502, "This feature has been disabled", 0, 0},
			{511, "Your account has been banned", 1, 1},
			{540, "Challenge response failed. File a bug report", 1, 1},

			{604, "Server is going down", 1, 1},

			{713, "Server rejected your chat request", 1, 0},
			{714, "Too many sessions", 1, 0},
			{715, "Unexpected Request", 0, 0},

			{800, "Too many requests. Please slow down", 0, 0},

			{910, "Server too busy", 1, 0},
			{911, "Authentication failed", 1, 0},
			{913, "You cannot do this while staying hidden", 0, 0},
			{917, "Authentication failed", 1, 1},
			{920, "Cannot add more buddies to this chat", 0, 0},
			{924, "Passport not yet verified", 1, 1},

			{MSN_ERROR_CONNECTION_RESET, "Server closed connection", 1, 0},
			{MSN_LOGIN_FAIL_PASSPORT, "Invalid username", 1, 1},
			{MSN_LOGIN_FAIL_SSO, "Authentication failed", 1, 1},
			{MSN_LOGIN_FAIL_VER, "Error during login", 1, 1},
			{MSN_LOGIN_FAIL_OTHER, "Login failed due to unknown reasons", 1, 1},
			{MSN_MESSAGE_DELIVERY_FAILED, "Failed to deliver message", 0, 0},

			{0, "Unknown error", 1, 1}
		};

const MsnError *msn_strerror(int error_num)
{
	const MsnError *error = errors;

	while(error->error_num) {
		if(error->error_num == error_num)
			return error;
		error++;
	}

	return error;
}


