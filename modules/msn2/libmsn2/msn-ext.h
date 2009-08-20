
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

void ext_got_status_change(MsnAccount *ma);
void ext_update_friendlyname(MsnConnection *mc);

void ext_got_IM_sb(MsnConnection *mc, MsnBuddy *bud);
void ext_got_IM(MsnConnection *mc, MsnIM *im, MsnBuddy *bud);
void ext_got_typing(MsnConnection *mc, MsnBuddy *bud);

void ext_got_ans(MsnConnection *mc);

#endif

