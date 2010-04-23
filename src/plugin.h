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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include "plugin_api.h"

#ifdef USE_POSIX_DLOPEN
#include <dlfcn.h>
#else
#include <ltdl.h>
#endif

#ifdef USE_POSIX_DLOPEN

#define PLUGIN_SO_EXT ".so"
typedef void * AyttmPlugin;
#define ayttm_dlerror dlerror
#define ayttm_dlopen(path) dlopen(path, RTLD_LAZY)
#define ayttm_dlsym dlsym
#define ayttm_dlerror dlerror
#define ayttm_dlclose dlclose

#define ayttm_dlinit(path) { \
}

#else

#define PLUGIN_SO_EXT ".la"
typedef lt_dlhandle AyttmPlugin;
#define ayttm_dlopen(path) lt_dlopen(path)
#define ayttm_dlsym lt_dlsym
#define ayttm_dlerror lt_dlerror
#define ayttm_dlclose lt_dlclose

#define ayttm_dlinit(path) { \
	lt_dlinit(); \
	lt_dlsetsearchpath(path); \
}

#endif

#define EB_PLUGIN_LIST "PLUGIN::LIST"

extern const char *PLUGIN_TYPE_TXT[];
extern const char *PLUGIN_STATUS_TXT[];

typedef enum {
	PLUGIN_NOT_LOADED,
	PLUGIN_LOADED,
	PLUGIN_CANNOT_LOAD,
	PLUGIN_NO_STATUS
} PLUGIN_STATUS;

typedef struct {
	PLUGIN_INFO pi;		/* Information provided by the plugin */
	AyttmPlugin Module;	/* Reference to the shared object itself */
	char *path;		/* Full Path */
	char *name;		/* File Name */
	char *service;		/* Non NULL if this is a service plugin */
	PLUGIN_STATUS status;	/* Is the plugin loaded? */
	const char *status_desc;	/* Error dsecription */
} eb_PLUGIN_INFO;

typedef struct {
	char *label;
	eb_menu_callback callback;
	/*FIXME: Should have some sort of conditional callback to see if menu item should be displayed */
	ebmCallbackData *data;
	void *user_data;
	char *protocol;
} menu_item_data;

typedef void (*menu_func) ();
typedef struct {
	LList *menu_items;	/* A LList of menu_item_data elements */
	menu_item_data *active;	/* Currently active menu item - for radio menus */
	menu_func redraw_menu;	/* The function to call when the menu is changed */
	ebmType type;		/* What kind of data structure do we send back? */
} menu_data;

eb_PLUGIN_INFO *FindPluginByName(const char *name);

int load_module_full_path(const char *inFullPath);
int load_module(const char *path, const char *name);
void load_modules(void);

int unload_module_full_path(const char *inFullPath);
int unload_module(eb_PLUGIN_INFO *epi);
void unload_modules(void);

/* Make sure that all the plugin_api accessible menus, as defined in plugin_api.h, are initialized */
int init_menus();

#endif				/* _PLUGIN_H */
