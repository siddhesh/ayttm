/* msn_interface.h - functions that talk to the outside world */

/*
        void ext_show_error(char * msg);

        Purpose:        Displays an error
        Arguments:
          msg           The error message
        Return:         Nothing
*/

void handle_command(void);
void ext_update_contact(char *username);
void ext_register_sock(int s, int read, int write);
void ext_unregister_sock(int s);

void ext_show_error(msnconn * conn, char * msg);

void ext_buddy_set(msnconn * conn, char * buddy, char * friendlyname, char * state);

void ext_buddy_offline(msnconn * conn, char * buddy);

void ext_got_friendlyname(msnconn * conn, char * friendlyname);
void ext_got_pong(msnconn *conn);
void ext_got_info(msnconn * conn, syncinfo * data);

void ext_latest_serial(msnconn * conn, int serial);

void ext_got_GTC(msnconn * conn, char c);

void ext_got_BLP(msnconn * conn, char c);

void ext_new_RL_entry(msnconn * conn, char * username, char * friendlyname);

void ext_new_list_entry(msnconn * conn, char * list, char * username);

void ext_del_list_entry(msnconn * conn, char * list, char * username);

void ext_got_SB(msnconn * conn, void * tag);

void ext_user_joined(msnconn * conn, char * username, char * friendlyname, int is_initial);

void ext_user_left(msnconn * conn, char * username);

void ext_got_IM(msnconn * conn, char * username, char * friendlyname, message * msg);

void ext_IM_failed(msnconn * conn);

void ext_typing_user(msnconn * conn, char * username, char * friendlyname);

void ext_initial_email(msnconn * conn, int unread_inbox, int unread_folders);

void ext_new_mail_arrived(msnconn * conn, char * from, char * subject);

void ext_filetrans_invite(msnconn * conn, char * username, char * friendlyname, invitation_ftp * inv);
void ext_netmeeting_invite(msnconn * conn, char * username, char * friendlyname, invitation_voice * inv);
void ext_start_netmeeting(char *ip);

void ext_filetrans_progress(invitation_ftp * inv, char * status, unsigned long recv, unsigned long total);

void ext_filetrans_failed(invitation_ftp * inv, int error, char * message);

void ext_filetrans_success(invitation_ftp * inv);

void ext_new_connection(msnconn * conn);

void ext_closing_connection(msnconn * conn);

void ext_changed_state(msnconn * conn, char * state);

/*
        int connect_socket(char * server, int port);

        Purpose:        Makes a TCP socket, connects it to the specified server and port,
                        and then registers it for
        Arguments:
          server        The server name
          port          The TCP port to connect to
        Return:         Nothing
*/
int ext_connect_socket(char * server, int port);
int ext_async_socket(char *host, int port, void *cb, void *conn);

int ext_server_socket(int port);

char * ext_get_IP(void);

void ext_update_local_contact(char *c);

void ext_disable_conncheck(void);

void ext_got_group(char *id, char *name);
void ext_got_friend(char *name, char *groups);
void ext_syncing_lists(int state);
