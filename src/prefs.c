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

#include "intl.h"

#include <stdlib.h>
#include <string.h>

#include "service.h"
#include "value_pair.h"
#include "globals.h"
#include "dialog.h"


typedef struct _ptr_list
{
	char key[MAX_PREF_NAME_LEN+1];
	void *value;
} ptr_list;


static GList *s_global_prefs = NULL;


static int compare_ptr_key(gconstpointer a, gconstpointer b)
{
		if(!strcmp( ((ptr_list *)a)->key, (char *)b))
			return(0);
		return(1);
}

static void AddPref(char *key, void *data)
{
	ptr_list *PrefData = g_new0(ptr_list, 1);
	
	strncpy(PrefData->key, key, sizeof(PrefData->key));
	PrefData->value=(void *)data;
	s_global_prefs = g_list_append(s_global_prefs, PrefData);
	return;
}

/* Find old pref data, and replace with new, returning old data */
void *SetPref(char *key, void *data)
{
	ptr_list	*PrefData = NULL;
	void		*old_data = NULL;
	GList		*ListData = g_list_find_custom(s_global_prefs, key, compare_ptr_key);

	if (!ListData)
	{
		AddPref(key, data);
		return(NULL);
	}
	PrefData=(ptr_list *)ListData->data;
	old_data=PrefData->value;
	PrefData->value=data;
	return(old_data);
}

void *GetPref(char *key)
{
	ptr_list	*PrefData = NULL;
	GList		*ListData = g_list_find_custom(s_global_prefs, key, compare_ptr_key);

	if(!ListData)
		return(NULL);
	PrefData = ListData->data;
	if(!PrefData)
		return(NULL);
	return(PrefData->value);
}

void cSetLocalPref(char *key, char *data)
{
	char	newkey[MAX_PREF_NAME_LEN];
	char	*oldvalue = NULL;

	g_snprintf(newkey, MAX_PREF_NAME_LEN, "Local::%s", key);
	oldvalue = SetPref(newkey, strdup(data));
	if(oldvalue)
		g_free(oldvalue);
	return;
}

void iSetLocalPref(char *key, int data)
{
	char	value[MAX_PREF_LEN];

	g_snprintf(value, MAX_PREF_LEN, "%i", data);
	cSetLocalPref(key, value);
	return;
}

void fSetLocalPref(char *key, float data)
{
	char	value[MAX_PREF_LEN];

	g_snprintf(value, MAX_PREF_LEN, "%f", data);
	cSetLocalPref(key, value);
	return;
}

char *cGetLocalPref(char *key)
{
	char	newkey[MAX_PREF_NAME_LEN];
	char	*value = NULL;

	g_snprintf( newkey, MAX_PREF_NAME_LEN, "Local::%s", key );
	value = (char *)GetPref(newkey);
	if(!value)
		value="";
	return(value);
}

float fGetLocalPref(char *key)
{
	float	value = 0.0;

	value=atof(cGetLocalPref(key));
	return(value);
}

int iGetLocalPref(char *key)
{
	int	value = 0;

	value=atoi(cGetLocalPref(key));
	return(value);
}

/* Used when loading service modules later, so passwords and user names are still available
 * as service:username */
void save_account_info(char *service, LList *pairs)
{
	const int	buffer_size = 256;
	char		buff[buffer_size];
	char		*val = value_pair_get_value(pairs, "SCREEN_NAME");
	
	g_snprintf(buff, buffer_size, "%s:%s", service, val);
	g_free(val);
	SetPref(buff, pairs);
}

void reload_service_accounts(int service_id)
{
	LList				*node = accounts;
	LList				*account_pairs = NULL;
	eb_local_account	*oela = NULL;
	eb_local_account	*nela = NULL;
	const int			buffer_size = 256;
	char				buff[buffer_size];
	char				buff2[buffer_size];

	while(node)
	{
		oela=node->data;
		if(oela->service_id != service_id || oela->connected) {
			node = node->next;
			continue;
		}
		eb_debug(DBG_CORE, "Account: handle:%s service: %s\n", oela->handle, get_service_name(oela->service_id));
		g_snprintf(buff, buffer_size, "%s:%s", get_service_name(oela->service_id), oela->handle);
		account_pairs = GetPref(buff);
		nela = eb_services[oela->service_id].sc->read_local_account_config(account_pairs);
		if(!nela) {
			g_snprintf(buff2, buffer_size, _("Unable to create account for %s.\nCheck your config file."), buff);
			do_error_dialog(buff2, _("Invalid account"));
			oela->service_id=get_service_id("NO SERVICE");
		}
		else {
			nela->service_id = oela->service_id;
			node->data=nela;
			//FIXME: This should probably be left to the service to clean up, though at this point, it may not exist
			g_free(oela->handle);
			g_free(oela->protocol_local_account_data);
			g_free(oela);
		}
		node = node->next;
	}
}

