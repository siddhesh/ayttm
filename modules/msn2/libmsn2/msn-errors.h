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

#ifndef _MSN_ERRORS_H_
#define _MSN_ERRORS_H_


#include "msn.h"

struct _MsnError {
	int error_num;
	const char *message;
	int fatal;
	int nsfatal;
};


enum {
	MSN_ERROR_CONNECTION_RESET=0x01,

	MSN_LOGIN_OK=0x1000,
	MSN_LOGIN_FAIL_PASSPORT,
	MSN_LOGIN_FAIL_SSO,
	MSN_LOGIN_FAIL_VER,
	MSN_LOGIN_FAIL_OTHER,
	MSN_MESSAGE_DELIVERY_FAILED=0x2000
};

const MsnError *msn_strerror(int error);

#endif

