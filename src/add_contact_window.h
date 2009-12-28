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

#ifndef __ADD_CONTACT_WINDOW__
#define __ADD_CONTACT_WINDOW__

#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif
	void show_add_contact_window();
	void show_add_contact_to_group_window(grouplist *g);
	void show_add_group_window();
	void show_add_account_to_contact_window(struct contact *cont);
	void edit_contact_window_new(struct contact *c);
	void edit_account_window_new(eb_account *ea);
	void add_unknown_account_window_new(eb_account *ea);
	void edit_group_window_new(grouplist *g);
	LList *get_all_accounts(int service);
	LList *get_all_contacts();
#ifdef __cplusplus
}
#endif
#endif
