/* msn_bittybits.h - all the little string-bashing functions */

/*
        char ** msn_read_line(int sock, int * numargs);

        Purpose:        Read a line from the MSN server, then break it up into
                        space-separated strings
        Arguments:
          sock          The socket to use
          numargs       Gets set to the number of arguments
        Returns:        A NULL-terminated array of strings, or NULL on error

IMPORTANT: Only free the array, and its zeroth element.
*/
char ** msn_read_line(msnconn *conn, int * numargs);

void msn_clean_up(msnconn * conn);

void msn_add_callback(msnconn * conn, void (*func)(msnconn * conn, int trid, char ** args, int numargs, callback_data * data), int trid, callback_data * data);

void msn_del_callback(msnconn * conn, int trid);

void msn_add_to_llist(llist *& listp, llist_data * data);

void msn_del_from_llist(llist *& listp, llist_data * data);

int msn_count_llist(llist * list);

char * msn_permstring(const char * s);

char * msn_decode_URL(char * s);
char * msn_encode_URL(char * s);

