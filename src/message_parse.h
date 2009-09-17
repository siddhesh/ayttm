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
 *  Message Parse
 *
 */

#ifndef __MESSAGE_PARSE_H__
#define __MESSAGE_PARSE_H__

#include "contact.h"

/* forward declarations */
struct service;

#ifdef __cplusplus
extern "C" {
#endif

	void eb_parse_incoming_message(eb_local_account *account,
		eb_account *remote, char *message);

	void eb_update_status(eb_account *remote, const char *message);
#ifdef __cplusplus
}				/* extern "C" */
#endif
extern char filename[1024];
#endif
