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

#include "intl.h"

#include <gdk/gdkprivate.h>
#ifndef __MINGW32__
# include <gdk/gdkx.h>
# include <X11/Xlib.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "service.h"
#include "globals.h"
#include "chat_window.h"
#include "dialog.h"
#include "util.h"
#include "nomodule.h"
#include "plugin_api.h"
#include "value_pair.h"

#ifdef HAVE_MIT_SAVER_EXTENSION
#include <X11/extensions/scrnsaver.h>
#endif /* HAVE_MIT_SAVER_EXTENSION */

#ifdef __MINGW32__
#define snprintf _snprintf
#endif

static guint idle_timer;
static time_t lastsent = 0;
static int is_idle = 0;

#ifdef HAVE_MIT_SAVER_EXTENSION
static int scrnsaver_ext = 0;
#endif

int NUM_SERVICES=0;		/* Used in prefs.c */


/* Following chunk will be moved to a 'real' code file */
/* FIXME: Should be a dynamic array */
struct service eb_services[255];

/* Called when a service module changes */
static void refresh_service_contacts(int type)
{
	LList *l1, *l2, *l3;
	LList * config=NULL;
	struct contact *con=NULL;

	eb_debug(DBG_CORE, ">Refreshing contacts for %i\n", type);
	for(l1 = groups; l1; l1=l1->next ) {
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			con=l2->data;
			if(con->chatwindow && con->chatwindow->preferred && con->chatwindow->preferred->service_id==type) {
				eb_debug(DBG_MOD, "Setting the preferred service to NULL for %s\n", con->nick);
				con->chatwindow->preferred=NULL;
			}
			for(l3 = con->accounts; l3; l3=l3->next) {
				eb_account * account = l3->data;
				if(account->service_id == type) {
					eb_debug(DBG_CORE, "Refreshing %s - %i\n", account->handle, type);
					config = value_pair_add(NULL, "NAME", account->handle);
					if(RUN_SERVICE(account)->free_account_data)
						RUN_SERVICE(account)->free_account_data(account);
					g_free(account);
					account = eb_services[type].sc->read_account_config(config, con);
					/* Is this a nomodule account?  Make it the right service number */
					if(account->service_id==-1)
					       account->service_id=type;
					value_pair_free(config);
					config=NULL;
					l3->data=account;
				}
			}
		}
	}
	eb_debug(DBG_CORE, "<Leaving\n");
	return;
}

static void reload_service_accounts(int service_id)
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
		snprintf(buff, buffer_size, "%s:%s", get_service_name(oela->service_id), oela->handle);
		account_pairs = GetPref(buff);
		nela = eb_services[oela->service_id].sc->read_local_account_config(account_pairs);
		if(!nela) {
			snprintf(buff2, buffer_size, _("Unable to create account for %s.\nCheck your config file."), buff);
			do_error_dialog(buff2, _("Invalid account"));
			oela->service_id=get_service_id("NO SERVICE");
		}
		else {
			nela->service_id = oela->service_id;
			node->data=nela;
			//FIXME: This should probably be left to the service to clean up, though at this point, it may not exist
			free(oela->handle);
			free(oela->protocol_local_account_data);
			free(oela);
		}
		node = node->next;
	}
}


/* Add a new service, or replace an existing one */
int add_service(struct service *Service_Info)
{
	int i;
	LList *session_prefs=NULL;

	assert(Service_Info);

	eb_debug(DBG_CORE, ">Entering\n");
	if (Service_Info->sc->read_prefs_config) {
		session_prefs=GetPref(Service_Info->name);
		Service_Info->sc->read_prefs_config(session_prefs);
	}

	for (i=0; i < NUM_SERVICES; i++ ) {
		/* Check to see if another service exists for the same protocol, if so, replace it */
		if (!strcasecmp(eb_services[i].name,Service_Info->name))
		{
				eb_debug(DBG_CORE, "Replacing %s service ", Service_Info->name);
				free(eb_services[i].sc);
				Service_Info->protocol_id=i;
				eb_debug(DBG_CORE, "(service_id %d)\n",Service_Info->protocol_id);
				memcpy(&eb_services[i], Service_Info, sizeof(struct service));
				refresh_service_contacts(i);
				reload_service_accounts(i);
				eb_debug(DBG_CORE, "<Replaced existing service\n");
				return(i);
		}
		
	}
	Service_Info->protocol_id=NUM_SERVICES++;
	eb_debug(DBG_CORE, "(%s: service_id %d)\n",Service_Info->name, Service_Info->protocol_id);
	memcpy(&eb_services[Service_Info->protocol_id], Service_Info, sizeof(struct service));
	refresh_service_contacts(i);
	reload_service_accounts(Service_Info->protocol_id);
	eb_debug(DBG_CORE, "<Added new service \n");
	return(Service_Info->protocol_id);
}

/* This now creates a service if the name is not recognized */
int get_service_id( char * servicename )
{
	int i;
	char buf[1024];

	for (i=0; i < NUM_SERVICES; i++ ) {
		if (strcasecmp(eb_services[i].name,servicename)==0)
		{
			return i;	
		}
	}
	eb_debug(DBG_CORE, "Creating empty service for %s\n", servicename);
	memcpy(&eb_services[NUM_SERVICES], &nomodule_SERVICE_INFO, sizeof(struct service));
	eb_services[NUM_SERVICES].sc=eb_nomodule_query_callbacks();
	eb_services[NUM_SERVICES].name = strdup(servicename);
	eb_services[NUM_SERVICES].protocol_id=NUM_SERVICES;
	NUM_SERVICES++;
	snprintf(buf, sizeof(buf), "%s::path", servicename);
	cSetLocalPref(buf, "Empty Module");
	return(NUM_SERVICES-1);
}

char *get_service_name( int service_id )
{
	if ((service_id >= 0) && (service_id < NUM_SERVICES))
		return (eb_services[service_id].name);

	fprintf(stderr, "warning: unknown service id: %d\n", service_id);
	return "unknown";
}

LList * get_service_list()
{
	LList * newlist = NULL;
	int i;
	for ( i = 0; i < NUM_SERVICES; i++ )
		newlist = l_list_append( newlist, eb_services[i].name );

	return newlist;
}

void serv_touch_idle()
{
	/* Are we idle?  If so, not anymore */
	if (is_idle > 0) {
		LList * node;
		is_idle = 0;
		for (node = accounts; node; node = node->next ) {
			if (((eb_local_account *)(node->data))->connected
			&& RUN_SERVICE(((eb_local_account*)(node->data)))->set_idle) {
				RUN_SERVICE(((eb_local_account*)node->data))->set_idle(
					    (eb_local_account*)node->data, 0);
			}
		}
    	}
	time(&lastsent);
}

static guint idle_time = 0;

static int report_idle(void * data)
{
	LList * node;

	if (!is_idle)
		return FALSE;

	for (node = accounts; node; node = node->next ) {
		if (((eb_local_account *)node->data)->connected
		&& RUN_SERVICE(((eb_local_account*)node->data))->set_idle) {
			RUN_SERVICE(((eb_local_account*)node->data))->set_idle(
				    (eb_local_account*)node->data, idle_time);
		}
	}
	return TRUE;
}

static int check_idle()
{
	int		idle_reporter = -1;
	const int	sendIdleTime = iGetLocalPref("do_send_idle_time");

#ifdef HAVE_MIT_SAVER_EXTENSION
	if (scrnsaver_ext) {
		static XScreenSaverInfo * mit_info = NULL;
		mit_info = XScreenSaverAllocInfo();
		XScreenSaverQueryInfo(gdk_display, DefaultRootWindow(gdk_display), mit_info);
		idle_time = mit_info->idle/1000;
		free(mit_info);
	} else
#endif
	{
    		time_t t;

    		if (is_idle)
        		return TRUE;
    		time(&t);
		idle_time = t - lastsent;
	}

	if ((idle_time >= 600) && sendIdleTime) {
		if (is_idle == 0) {
			LList * node;
			idle_reporter = eb_timeout_add(60000, report_idle, NULL);
			for (node = accounts; node; node = node->next ) {
				if (((eb_local_account *)node->data)->connected
				&& RUN_SERVICE(((eb_local_account*)node->data))->set_idle) {
					RUN_SERVICE(((eb_local_account*)node->data))->set_idle(
						    (eb_local_account*)node->data, idle_time);
				}
			}
		}
		is_idle = 1;
	} else if ((idle_time < 600) && sendIdleTime && (is_idle == 1)) {
		if (idle_reporter != -1)
			gtk_idle_remove(idle_reporter);
		serv_touch_idle();
	}
	return TRUE;
}

void add_idle_check()
{
	idle_timer = eb_timeout_add(5000, (GtkFunction)check_idle, NULL);
	serv_touch_idle();
#ifdef HAVE_MIT_SAVER_EXTENSION
	{
		int	eventnum;
		int	errornum;
		
		if (XScreenSaverQueryExtension(gdk_display, &eventnum, &errornum))
			scrnsaver_ext = 1;
	}
#endif
}
