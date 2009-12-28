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

#ifndef _MSN_HTTP_H_
#define _MSN_HTTP_H_

#include "msn.h"

typedef enum {
	MSN_HTTP_POST = 1,
	MSN_HTTP_GET
} MsnRequestType;

typedef void (*MsnHttpCallback) (MsnAccount *ma, char *data, int len,
	void *cbdata);

void msn_http_request(MsnAccount *ma, MsnRequestType type, char *soap_action,
	char *url, char *request, MsnHttpCallback callback, char *params,
	void *data);

int msn_http_got_response(MsnConnection *mc, int len);

#endif
