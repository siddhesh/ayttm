/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
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

#ifndef __PREFS_H__
#define __PREFS_H__

#include "llist.h"


#define MAX_PREF_NAME_LEN 255
#define MAX_PREF_LEN 1024

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) void *GetPref(char *key);
#else
void	*GetPref(char *key);
#endif

void	*SetPref(char *key, void *data);

int		iGetLocalPref(char *key);
void	iSetLocalPref(char *key, int data);

float	fGetLocalPref(char *key);
void	fSetLocalPref(char *key, float data);

char	*cGetLocalPref(char *key);
void	cSetLocalPref(char *key, char *data);

void	save_account_info(char *service, LList *pairs);
void	reload_service_accounts(int service_id);

#ifdef __cplusplus
}
#endif

#endif
