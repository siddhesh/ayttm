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


#ifndef _MSN_EXT_H_
#define _MSN_EXT_H_

#include "msn-connection.h"
#include "msn.h"

void ext_msn_free(MsnConnection *mc);
void ext_msn_error(MsnConnection *mc, const MsnError *error);
void ext_show_error(MsnConnection *mc, const char *msg);
void ext_msn_send_data(MsnConnection *mc, char *buf, int len);
void ext_msn_account_destroyed(MsnAccount *ma);
void ext_msn_connection_destroyed(MsnConnection *mc);
void ext_msn_connect(MsnConnection *mc, MsnConnectionCallback callback);
void ext_msn_login_response(MsnAccount *ma, int response);

int ext_msn_read(void *fd, char *buf, int len);
int ext_msn_write(void *fd, char *buf, int len);

void ext_register_read(MsnConnection *mc);
void ext_register_write(MsnConnection *mc);
void ext_unregister_read(MsnConnection *mc);
void ext_unregister_write(MsnConnection *mc);

void ext_msn_contacts_synced(MsnAccount *ma);
void ext_buddy_added(MsnAccount *ma, MsnBuddy *bud);
void ext_got_buddy_status(MsnConnection *mc, MsnBuddy *bud);
void ext_buddy_joined_chat(MsnConnection *mc, char *passport, char *friendlyname);
void ext_buddy_left(MsnConnection *mc, const char *passport);
void ext_buddy_removed(MsnAccount *ma, const char *bud);

void ext_buddy_unblock_response(MsnAccount *ma, int error, MsnBuddy *buddy);
void ext_buddy_block_response(MsnAccount *ma, int error, MsnBuddy *buddy);
void ext_buddy_group_remove_failed(MsnAccount *ma, MsnBuddy *bud, MsnGroup *group);
void ext_buddy_group_add_failed(MsnAccount *ma, MsnBuddy *bud, MsnGroup *group);
void ext_group_add_failed(MsnAccount *ma, const char *group, char *error);
void ext_buddy_add_failed(MsnAccount *ma, const char *passport, char *friendlyname);

void ext_got_status_change(MsnAccount *ma);
void ext_update_friendlyname(MsnConnection *mc);

void ext_got_IM_sb(MsnConnection *mc, MsnBuddy *bud);
void ext_got_IM(MsnConnection *mc, MsnIM *im, MsnBuddy *bud);
void ext_got_unknown_IM(MsnConnection *mc, MsnIM *im, const char *bud);
void ext_got_typing(MsnConnection *mc, MsnBuddy *bud);

void ext_got_ans(MsnConnection *mc);
void ext_new_sb(MsnConnection *mc);

int ext_confirm_invitation(MsnConnection *mc, const char *buddy);

#endif

