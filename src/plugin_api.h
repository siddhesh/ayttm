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

#ifndef __PLUGIN_API_H__
#define __PLUGIN_API_H__

#include "input_list.h"
#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Plugin */

typedef enum {
	PLUGIN_SERVICE = 1,
	PLUGIN_FILTER,
	PLUGIN_IMPORTER,
	PLUGIN_SMILEY,
	PLUGIN_UTILITY,
	PLUGIN_UNKNOWN
} PLUGIN_TYPE;

typedef int (*eb_plugin_func)();

typedef struct {
	PLUGIN_TYPE type;
	char *module_name;
	char *description;
	char *version;
	char *date;
	int *ref_count;
	eb_plugin_func init;
	eb_plugin_func finish;
	eb_plugin_func reload_prefs;
	input_list *prefs;
} PLUGIN_INFO;


#define IS_ebmCallbackData(x) (x->CDType>=ebmCALLBACKDATA)
/* Names of menus and the data structure passed to callbacks for them */
#define EB_IMPORT_MENU "IMPORT MENU"
#define IS_ebmImportData(x) (x->CDType==ebmIMPORTDATA)
#define EB_SMILEY_MENU "SMILEY MENU"
#define IS_ebmSmileyData(x) (x->CDType==ebmSMILEYDATA)
#define EB_PROFILE_MENU "PROFILE MENU"
#define IS_ebmProfileData(x) (x->CDType==ebmPROFILEDATA)
#define EB_CHAT_WINDOW_MENU "CHAT MENU"
#define EB_CONTACT_MENU "CONTACT MENU"
#define IS_ebmContactData(x) (x->CDType==ebmCONTACTDATA)

typedef enum {
	ebmCALLBACKDATA=10,
	ebmIMPORTDATA,
	ebmCONTACTDATA,
	ebmPROFILEDATA,
	ebmSMILEYDATA
} ebmType;

typedef struct {
	ebmType CDType;
	void *user_data;
} ebmCallbackData;


typedef void ebmSmileyData;

typedef struct {
	ebmCallbackData cd;
	char *MenuName;
} ebmImportData;

typedef struct {
	ebmCallbackData cd;
	char *group;		/* Can have two contacts with same name in diff groups */
	char *contact;		/* Name of the contact we're chatting with */
	char *remote_account;	/* The actual account name the contact is using */
	char *local_account;    /* The actual account we're using */
} ebmContactData;

typedef void (*eb_menu_callback) (ebmCallbackData *data);

ebmCallbackData *ebmProfileData_new(eb_local_account * ela);
ebmImportData *ebmImportData_new();
ebmSmileyData *ebmSmileyData_new();
ebmContactData *ebmContactData_new();
void eb_set_active_menu_status(LList *status_menu, int status);

/* eb_add_menu_item returns a tag, which can be used by eb_remove_menu_item 
 * label:	The name of the menu item to add to the menu
 * menu_name:	The name of the menu to add the item to, as in EB_IMPORT_MENU
 * eb_menu_callback: The function to call when the menu item is selected
 * type:	The expected type of data passed to the callback for this menu
 * data:	Any user data to be passed along with the menu data
 */
void *eb_add_menu_item(char *label, char *menu_name, eb_menu_callback callback, ebmType type, void *data);
/* FIXME: Want an eb_add_menu_item_condition function */
/* tag comes from a call to eb_add_menu_item, returns 0 on success */
int eb_activate_menu_item(char *menu_name, void *tag);
int eb_remove_menu_item(char *menu_name, void *tag);
void eb_menu_item_set_protocol(void *item, char * protocol);

/* File */
typedef enum {
	EB_INPUT_READ = 1 << 0,
	EB_INPUT_WRITE = 1 << 1,
	EB_INPUT_EXCEPTION = 1 << 2
} eb_input_condition;

typedef void (*eb_input_function) (void *data, int source, eb_input_condition condition);
typedef int (*eb_timeout_function) (void *data);

/* Returns a tag to be used by eb_input_remove */
int eb_input_add(int fd, eb_input_condition condition, eb_input_function function,
		 void *callback_data);
void eb_input_remove(int tag);

/* Returns a tag to be used by eb_timeout_remove */
int eb_timeout_add(int ms, eb_timeout_function function, void *callback_data);
void eb_timeout_remove(int tag);

const char *eb_config_dir();

extern void ay_set_submenus(void);
extern void set_menu_sensitivity(void);

/* Service */

/* Debugging */
#include "debug.h"

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) int do_plugin_debug;
#else
extern int do_plugin_debug;
#endif
#define DBG_MOD iGetLocalPref( "do_plugin_debug" )

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PLUGIN_API_H */
