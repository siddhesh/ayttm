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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>	/* for strcmp */
#include <stdlib.h>

#include "globals.h"
#include "plugin.h"
#include "dialog.h"
#include "prefs.h"

extern void set_menu_sensitivity(void);

/* End local structures and functions */

/* GUI Functions */
ebmImportData *ebmImportData_new()
{
	ebmImportData *eid=g_new0(ebmImportData, 1);
	((ebmCallbackData *)eid)->CDType=ebmIMPORTDATA;
	return(eid);
}

ebmCallbackData *ebmProfileData_new(eb_local_account * ela)
{
	ebmCallbackData *eid=g_new0(ebmCallbackData, 1);
	((ebmCallbackData *)eid)->CDType=ebmPROFILEDATA;
	eid->user_data = ela;
	return(eid);
}


ebmContactData *ebmContactData_new()
{
	ebmContactData *ecd=g_new0(ebmContactData, 1);
	((ebmCallbackData *)ecd)->CDType=ebmCONTACTDATA;
	return(ecd);
}

void eb_menu_item_set_protocol(void *item, char * protocol)
{
	menu_item_data *mid = (menu_item_data *)item;
	eb_debug(DBG_CORE, "set prtocol:%s\n",protocol);
	mid->protocol = protocol;
}

void *eb_add_menu_item(char *label, char *menu_name, eb_menu_callback callback, ebmType type, void *data)
{
	menu_item_data *mid=NULL;
	menu_data *md=NULL;
	LList *list;
	
	eb_debug(DBG_CORE, ">Adding %s to menu %s\n", label, menu_name);
	md = GetPref(menu_name);
	if(!md)  {
		eb_debug(DBG_CORE, "Unknown menu requested: %s\n", menu_name);
		return(NULL);
	}
	/* Make sure they're expecting the right kind of structure to be passed back */
	if(type!=md->type) {
		eb_debug(DBG_CORE, "Incorrect ebmType passed, not adding menu item: %s\n", label);
		return(NULL);
	}
	for(list = md->menu_items; list && list->data; list = list->next ){
		menu_item_data *tmid = (menu_item_data *)list->data;
		if( tmid->callback == callback && !strcmp(tmid->label,label)) {
			/* already in list */
			return ((void *)tmid);
		}
	}
	mid=calloc(1, sizeof(menu_item_data));
	mid->user_data=data;
	mid->label=label;
	mid->callback=callback;
	mid->protocol=NULL;
	md->menu_items = l_list_append(md->menu_items, mid);
	if(md->redraw_menu) {
		eb_debug(DBG_CORE, "Calling redraw_menu for %s\n", menu_name);
		md->redraw_menu();
	}
	eb_debug(DBG_CORE, "<Successfully added menu item\n");
	/* Return the menu_item_data pointer, so that l_list_remove can be used */
	return((void *)mid);
}

int eb_activate_menu_item(char *menu_name, void *tag)
{
	menu_data *md=NULL;
	menu_item_data *mid=tag;

	if(mid) {
		eb_debug(DBG_CORE, ">Request to activate %s from menu %s\n", mid->label, menu_name);
	}
	if(!tag) {
		eb_debug(DBG_CORE, "<Received request to activate from menu %s, with empty tag\n", menu_name);
		return(-1);
	}
	md=GetPref(menu_name);
	if(!md) {
		eb_debug(DBG_CORE, "<Requested menu %s does not exist, returning failure\n", menu_name);
		return(-1);
	}
	md->active = mid;
	if(md->redraw_menu) {
		eb_debug(DBG_CORE, "Calling redraw_menu\n");
		md->redraw_menu();
	}
	eb_debug(DBG_CORE, "<Returning success\n");
	return(0);
}

int eb_remove_menu_item(char *menu_name, void *tag)
{
	menu_data *md=NULL;
	menu_item_data *mid=tag;

	if(mid) {
		eb_debug(DBG_CORE, ">Request to remove %s from menu %s\n", mid->label, menu_name);
	}
	if(!tag) {
		eb_debug(DBG_CORE, "<Received request to delete from menu %s, with empty tag\n", menu_name);
		return(-1);
	}
	md=GetPref(menu_name);
	if(!md) {
		eb_debug(DBG_CORE, "<Requested menu %s does not exist, returning failure\n", menu_name);
		return(-1);
	}
	md->menu_items=l_list_remove(md->menu_items, tag);
	if(md->redraw_menu) {
		eb_debug(DBG_CORE, "Calling redraw_menu\n");
		md->redraw_menu();
	}
	eb_debug(DBG_CORE, "<Returning success\n");
	return(0);
}

void eb_set_active_menu_status(LList *status_menu, int status)
{
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(
			 l_list_nth(status_menu, status)->data), TRUE);
	
	set_menu_sensitivity();
}

/* File */
int eb_input_add(int fd, eb_input_condition condition, eb_input_function function,
		 void *callback_data)
{
	return(gdk_input_add(fd, condition, function, callback_data));
}

void eb_input_remove(int tag)
{
	gdk_input_remove(tag);
}

int eb_timeout_add(int ms, eb_timeout_function function, void *callback_data)
{
	return(gtk_timeout_add(ms, function, callback_data));
}

void eb_timeout_remove(int tag)
{
	gtk_timeout_remove(tag);
}



/* Debugging */
#ifdef __STDC__
int EB_DEBUG(const char *func, char *file, int line, char *fmt, ...)
#else
int EB_DEBUG(const char *func, char *file, int line, char *fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	static int indent=0;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if(fmt && fmt[0]=='>')
		indent++;

	fprintf(stderr, "%*.*s%s[%i]:%s - ", indent, indent, " ", file, line, func);
	vfprintf(stderr, fmt, ap);
	fflush(stderr);
	va_end(ap);

	if(fmt && fmt[0]=='<')
		indent--;
	if(indent<0)
		indent=0;
	return(0);
}

const char *eb_config_dir()
{
	return(config_dir);
}
