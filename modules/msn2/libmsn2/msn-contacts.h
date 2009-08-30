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

#ifndef _MSN_CONTACTS_H_
#define _MSN_CONTACTS_H_

#include "msn.h"

void msn_download_address_book(MsnAccount *ma);
void msn_sync_contacts(MsnAccount *ma);

void msn_buddy_add(MsnAccount *ma, const char *passport, const char *friendlyname, const char *grp);
void msn_buddy_allow(MsnAccount *ma, MsnBuddy *bud);
void msn_buddy_remove(MsnAccount *ma, MsnBuddy *bud);
void msn_buddy_block(MsnAccount *ma, MsnBuddy *bud);
void msn_buddy_unblock(MsnAccount *ma, MsnBuddy *bud);

void msn_buddy_invite(MsnConnection *mc, const char *passport);

void msn_buddy_remove_pending(MsnAccount *ma, MsnBuddy *bud);

void msn_group_add(MsnAccount *ma, const char *groupname);
void msn_change_buddy_group(MsnAccount *ma, MsnBuddy *bud, MsnGroup *newgrp);
void msn_add_buddy_to_group(MsnAccount *ma, MsnBuddy *bud, MsnGroup *newgrp);
void msn_remove_buddy_from_group(MsnAccount *ma, MsnBuddy *bud);
void msn_group_mod(MsnAccount *ma, MsnGroup *group, const char *groupname);
void msn_group_del(MsnAccount *ma, MsnGroup *group);
void msn_group_add(MsnAccount *ma, const char *groupname);


#endif
