/*
 * Licq contact list extractor
 * Supports both newer and older licq contact list formats
 *
 * Copyright (C) 2000, John Starks <jstarks@penguinpowered.com>
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <time.h>
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "account.h"
#include "contact.h"
#include "util.h"
#include "prefs.h"
#include "plugin_api.h"
#include "service.h"
#include "messages.h"


/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/

/*  Module defines */
#ifndef USE_POSIX_DLOPEN
	#define plugin_info importlicq_LTX_plugin_info
	#define module_version importlicq_LTX_module_version
#endif


/* Function Prototypes */
void import_licq_accounts(ebmCallbackData *data);
static int plugin_init();
static int plugin_finish();
unsigned int module_version() {return CORE_VERSION;}


static int ref_count=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_IMPORTER, 
	"Licq Contact List", 
	"Imports your Licq contact list into Ayttm", 
	"$Revision: 1.8 $",
	"$Date: 2009/07/27 16:42:03 $",
	&ref_count,
	plugin_init,
	plugin_finish
};
/* End Module Exports */

static void *buddy_list_tag=NULL;

static int plugin_init()
{
	eb_debug(DBG_MOD,"Licq Contact List init\n");
	buddy_list_tag=eb_add_menu_item("Licq Contact List", EB_IMPORT_MENU, import_licq_accounts, ebmIMPORTDATA, NULL);
	if(!buddy_list_tag)
		return(-1);
	return(0);
}

static int plugin_finish()
{
	int result;

	result=eb_remove_menu_item(EB_IMPORT_MENU, buddy_list_tag);
	if(result) {
		g_warning("Unable to remove Licq Contact List menu item from import menu!");
		return(-1);
	}
	return(0);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

/* Removes the spaces surrounding a token */
char* remove_spaces(char* token)
{
  g_strchomp(token); /* Remove spaces at the end */
  while (*token == ' ') token++;  /* Remove spaces at the beginning */
  return token;
}

/* Gets your nickname for an licq buddy */
char* get_licq_nick(const char* uin, int licq_version)
{
    gchar buff[1024]; 
    static gchar c[1024];
    FILE *fp;
    char *token, *nick;

    g_snprintf(buff, 1024, "%s/.licq/%s/%s.uin", getenv("HOME"), 
               (licq_version < 7 ? "conf" : "users"), uin);
    if (!(fp = fopen(buff, "r")))
        return NULL;

    while (!feof(fp))
    {
        fgets(c, 1024, fp);
        token = remove_spaces(strtok(c, "="));
        if (g_strcasecmp(token, "Alias"))
            continue;

        nick = remove_spaces(strtok(NULL, "="));
        fclose(fp);
        return nick;
    }
    fclose(fp);
    return NULL;
}


/* Imports licq accounts
 * Supports newer and older contact list formats (0.7x and 0.6x)
 */
void import_licq_accounts(ebmCallbackData *data)
{
    gchar group_name[] = "Licq Users";
    int num_users;
    int unused_num;
    int licq_version;
    int ICQ_ID=-1;

    gchar c[1024], msg[1024];
    char *nick, *uin, *token;

    FILE *fp;

    ICQ_ID=get_service_id("ICQ");
    
    g_snprintf(c, 1024, "%s/.licq/users.conf", getenv("HOME"));
    if ((fp = fopen(c, "r")))
        licq_version = 7;
    else
    {
        g_snprintf(c, 1024, "%s/.licq/conf/users.conf", getenv("HOME"));
        if (!(fp = fopen(c, "r"))) {
			g_snprintf(msg, 1024, "Unable to import licq accounts from neither %s/.licq/users.conf, nor %s\n", getenv("HOME"), c);
			ay_do_error( "Import Error", msg );
            return;
		}
        licq_version = 6;
    }

    while(!feof(fp))
    {
        fgets(c, 1024, fp);
        token = remove_spaces(c);
        if(!g_strcasecmp(token, "[users]"))
            break;
    }
    if(feof(fp))
    {
        fclose(fp);
		ay_do_warning( "Import Warning", "No users found in licq file to import" );
        return;
    }

    while (!feof(fp))
    {
        fgets(c, 1024, fp);
        token = remove_spaces(strtok(c, "="));
        if (!g_strncasecmp(token, "NumOfUsers", strlen("NumOfUsers") + 1))
            break;
    }
    if (feof(fp)) 
    {
        fclose(fp);
		ay_do_warning( "Import Warning", "No users found in licq file to import" );
        return;
    }

    token = strtok(NULL, "=");
    num_users = atoi(token);
    if (num_users < 1)
    {
        fclose(fp);
		ay_do_warning( "Import Warning", "No users found in licq file to import" );
        return;
    }
    
    if(!find_grouplist_by_name(group_name))
        add_group(group_name);

    while(!feof(fp))
    {
        fgets(c, 1024, fp);
        if(feof(fp))
            break;
        token = strtok(c, "=");
        if (sscanf(token, "User%d", &unused_num) <= 0) 
            continue;

        uin = remove_spaces(strtok(NULL, "="));
        nick = get_licq_nick(uin, licq_version);
        if (!nick)
            nick = uin;

        if(find_account_by_handle(uin, ICQ_ID))
        {
            continue;
        }
        if(!find_contact_by_nick(nick))
        {
            add_new_contact(group_name, nick, ICQ_ID );
        }
        if(!find_account_by_handle(uin, ICQ_ID))
        {
            eb_account * ea = eb_services[ICQ_ID].sc->new_account(NULL,uin);
            add_account( nick, ea );
//            RUN_SERVICE(ea)->add_user(ea);
        }
    }
    fclose(fp);
	
	ay_do_info( "Import", "Successfully imported licq contact list" );
}
