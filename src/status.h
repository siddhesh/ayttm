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

/*
 * status.h
 */

#ifndef __STATUS_H__
#define __STATUS_H__

#include "account.h"

enum list_type
{
	LIST_ONLINE,
	LIST_EDIT
};

struct callback_item
{
	char name[255];
	void (*callback_function)();
};

#ifdef __cplusplus
extern "C" {
#endif

void add_contact_and_accounts(struct contact * c);
void add_contact_line(struct contact * ec);
void add_account_line(eb_account * ea);
void add_group_line(grouplist * eg);
void remove_account_line(eb_account * ea);
void remove_contact_line(struct contact * ec);
void remove_group_line(grouplist * eg);
void buddy_update_status_and_log(eb_account * ea);
void buddy_update_status(eb_account * ea);
void contact_update_status(struct contact * ec);
void buddy_login(eb_account * ea);
void buddy_logoff(eb_account * ea);
void eb_sign_on_all(void);
void eb_sign_on_startup(void);
void eb_sign_off_all(void);

void eb_smiley_window(void *smiley_submenuitem);
void eb_import_window(void *import_submenuitem);
void eb_profile_window(void *profile_submenuitem);
void eb_set_status_window(void *set_status_submenuitem);
void eb_status_window();
void ay_set_submenus(void);
/*GtkWidget* MakeStatusButton(eb_local_account * ela);*/
void update_contact_list ();
void update_user(eb_account * ea );
void update_contact_window_length ();
void focus_statuswindow (void);
int add_menu_items(void *menu, int cur_service, int should_sep,
			struct contact *conn, eb_account *acc, eb_local_account *ela);
void set_menu_sensitivity(void);
void reset_list (void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

