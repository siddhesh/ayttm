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


#ifndef _MSN_SOAP_H_
#define _MSN_SOAP_H_

extern const char *MSN_SOAP_LOGIN_REQUEST;
extern const char *MSN_MEMBERSHIP_LIST_REQUEST;
extern const char *MSN_MEMBERSHIP_LIST_MODIFY;
extern const char *MSN_CONTACT_LIST_REQUEST;
extern const char *MSN_CONTACT_ADD_REQUEST;
extern const char *MSN_CONTACT_UPDATE_REQUEST;
extern const char *MSN_CONTACT_DELETE_REQUEST;
extern const char *MSN_GROUP_ADD_REQUEST;
extern const char *MSN_GROUP_MOD_REQUEST;
extern const char *MSN_GROUP_DEL_REQUEST;
extern const char *MSN_GROUP_CONTACT_REQUEST;


/* 
 * Provide values for the format specifiers in the skeleton
 * Returns a newly allocated string. Free it.
 */
char *msn_soap_build_request(const char *skel, ...);

#endif

