/* msn_core.h  - prototypes for msn_core.C */
#ifndef MSN_CORE_H
#define MSN_CORE_H
#ifdef __MINGW32__
#include <winsock2.h>
#define write(a,b,c) send(a,b,c,0)
#define read(a,b,c)  recv(a,b,c,0)
#endif

class llist_data // inherit it
{};

class llist
{
  public:
  llist_data * data;
  llist * next;
  llist * prev;

  llist() { data=NULL; next=NULL; prev=NULL; }
  ~llist() { if(data!=NULL) { delete data; } if(next!=NULL) { delete next; } }
};

class char_data : public llist_data
{
  public:
  char * c;
  char_data(char * tc) { c=tc; }
  ~char_data() { if(c!=NULL) { delete [] c; } }
};

class message : public llist_data // This class encapsulates all that you need to know (tm) about a MSG
{
  public:

  // Raw stuff
  char * header; // MIME
  char * body;

  // Parsed stuff
  char * font;
  char * colour;
  int bold;
  int italic;
  int underline;
  int fontsize;

  char * content; // Content-type

  message() { 
	  header=NULL; 
	  font=NULL; 
	  content=NULL;
	  body=NULL; }
	  
  ~message() { 
	  if (header) delete [] header;
	  if (font) delete [] font;
	  if (content) delete [] content;
	  if (body) free(body);
	  header=NULL; 
	  font=NULL; 
	  content=NULL;
	  body=NULL; }
};

class userdata : public llist_data
{
  public:
  char * username;
  char * friendlyname;
  char * groups;
  userdata() { username=friendlyname=NULL; }
  ~userdata() { if(username!=NULL) { delete [] username; } if(friendlyname!=NULL) { delete [] friendlyname; } if(groups!=NULL) { delete [] groups; } }
};

class authdata
{};

typedef struct {
	int fd;
	int tag_r;
        int tag_w;
}tag_info;

class msnconn : public llist_data
{
  public:
  int sock; // Socket (durr...)
  int ssock;// Server socket for filetransfer
  int type; // one of the #defines below
  int ready;
  llist * users; // Users in this session - only for SB connections
  llist * invitations_out; // invitations extended but not responded to
  llist * invitations_in; // invitations received but not responded to
  llist * callbacks;
  authdata * auth;

  tag_info tags[21];

  int pos, numspaces;
  char readbuf[1250];
  void *ext_data;
  char *status;
  
  msnconn() { 
	users=NULL; 
  	callbacks=NULL; 
  	invitations_out=NULL; 
  	invitations_in=NULL; 
	pos=numspaces=0;
	ext_data = NULL;
	status = NULL;
	memset(readbuf,0,sizeof(readbuf));
	}
  ~msnconn()
  {
    if(users!=NULL) { delete users; }
    if(invitations_in!=NULL) { delete invitations_in; }
    if(invitations_out!=NULL) { delete invitations_out; }
    if(callbacks!=NULL) { delete callbacks; }
  }
};

#define CONN_NS  1 // Notification Server (also Dispatch, as it does exactly the same thing)
#define CONN_SB  2 // Switchboard Server
#define CONN_FTP 3 // MSN file transfer

class authdata_NS : public authdata
{
  public:
  char * username;
  char * password;

  authdata_NS() { username=password=NULL; }
  ~authdata_NS() { if(username!=NULL) { delete [] username; delete [] password; } }
};

class authdata_SB : public authdata
{
  public:
  char * username;
  char * sessionID;
  char * cookie;
  char * rcpt;  // recipient of the below
  message * msg; // to be sent as soon as connection is complete and someone joins
  void * tag; // for SB connections without an initial message

  authdata_SB() { username=sessionID=cookie=NULL; }
  ~authdata_SB()
  { if(username!=NULL) { delete [] username; delete [] sessionID; delete [] cookie; } }
};

class invitation : public llist_data
{
  public:
  int app;
  char * cookie;
  char * other_user;
  msnconn * conn;
  int cancelled;
  
  invitation() { cookie=other_user=NULL; cancelled=0;}
  ~invitation() { if(cookie!=NULL) { delete [] cookie; } if(other_user!=NULL) { delete [] other_user; } }
};

#define APP_FTP 1       // NOTE: this is MSN file transfer, which is NOTHING to do with ordinary FTP!
#define APP_VOICE 2
#define APP_NETMEETING 3

class invitation_ftp : public invitation
{
  public:
  char * filename;
  long unsigned filesize;

  invitation_ftp() { filename=NULL;}
  ~invitation_ftp()
    { if(filename!=NULL) { delete [] filename; } }
};

class invitation_voice : public invitation
{
  public:
  char * sessionid;

  invitation_voice() { sessionid=NULL; }
  ~invitation_voice()
    { if(sessionid!=NULL) { delete [] sessionid; } }
};


class authdata_FTP : public authdata
{
  public:
  char * cookie;
  char * username;
  invitation_ftp * inv;
  int fd;
  unsigned long bytes_done;
  unsigned long last_done;
  int num_ignore;
  unsigned int must_read;
  int direction;
  int connected;

  authdata_FTP() { cookie=username=NULL; inv=NULL; fd=-1; must_read=last_done=bytes_done=num_ignore=connected=0; }
  ~authdata_FTP()
  {
    if(cookie!=NULL) { delete [] cookie; }
    if(username!=NULL) { delete [] username; }
    if(inv!=NULL) { delete inv; }
  }
};

#define MSNFTP_SEND 1
#define MSNFTP_RECV 2

class callback_data
{};

class callback : public llist_data
{
  public:
  int trid;
  void (*func)(struct msnconn * conn, int trid, char ** args, int numargs, callback_data * data);
  callback_data * data; // just gets passed
};


extern llist * msnconnections;

void msn_init(msnconn * conn, char * username, char * password);

void msn_show_verbose_error(msnconn * conn, int errcode);

void msn_connect(msnconn * conn, char * server, int port);

void msn_invite_user(msnconn * conn, char * rcpt);

void msn_send_IM(msnconn * conn, char * rcpt, char * msg);

void msn_send_IM(msnconn * conn, char * rcpt, message * msg);

void msn_send_typing(msnconn * conn);

void msn_filetrans_accept(invitation_ftp * inv, const char * dest);

void msn_filetrans_reject(invitation_ftp * inv);

void msn_filetrans_cancel(invitation_ftp * inv);

void msn_netmeeting_accept(invitation_voice * inv);

void msn_netmeeting_reject(invitation_voice * inv);

invitation_voice * msn_invite_netmeeting(msnconn *conn);

invitation_ftp * msn_filetrans_send(msnconn * conn, char * path);

msnconn *find_nsconn_by_name(char *name);

void msn_sync_lists(msnconn * conn, int version);

int is_on_list(char *handle, llist *lst) ;

#define LST_FL  1
#define LST_RL  2
#define LST_AL  4
#define LST_BL  8

#define COMPLETE_BLP 16
#define COMPLETE_GTC 32

#define LIST_FL 1
#define LIST_AL 2
#define LIST_BL 4
#define LIST_RL 8

// Intermediate steps in synchronisation
class syncinfo : public callback_data
{
  public:

  llist * fl;
  llist * rl;
  llist * al;
  llist * bl;

  unsigned int complete;
  int total_lst;
  int serial;

  char blp;
  char gtc;

  syncinfo() { blp='A'; gtc='A'; fl=rl=al=bl=NULL; total_lst = 0; complete=0; serial=0; }
  ~syncinfo()
  {
    if(fl!=NULL) { delete fl; }
    if(rl!=NULL) { delete rl; }
    if(al!=NULL) { delete al; }
    if(bl!=NULL) { delete bl; }
  }
};

int msn_set_friendlyname(msnconn * conn, char * friendlyname);
void msn_send_ping(msnconn * conn);

void msn_sync_lists(msnconn * conn, int version);
void msn_add_to_list(msnconn * conn, const char * list, char * user);
void msn_del_from_list(msnconn * conn, char * list, char * user);
void msn_set_GTC(msnconn * conn, char c);
void msn_set_BLP(msnconn * conn, char c);
void msn_syncdata(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);
void msn_check_rl(msnconn * conn, syncinfo * info);

void msn_connect_and_send(msnconn * nsconn, char * dest, char * msg);

void msn_set_state(msnconn * conn, const char * state);

// Intermediate steps in switchboard connection:
class conninfo_SB : public callback_data
{
  public:
  authdata_SB * auth;
};

void msn_new_SB(msnconn * nsconn, void * tag);
void msn_request_SB(msnconn * nsconn, char * rcpt, message * msg, void * tag);

void msn_SBconn_2(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);
void msn_SBconn_3(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);

void msn_handle_incoming(msnconn *conn, int readable, int writable, char **args, int nargs);

void msn_handle_filetrans_incoming(msnconn * conn, int readable, int writable);

void msn_handle_close(int sock);

void msn_handle_default(msnconn * conn, char ** args, int numargs);

void msn_handle_MSG(msnconn * conn, char ** args, int numargs);

void msn_handle_invite(msnconn * conn, char * from, char * friendly, char * mime, char * body);

void msn_handle_new_invite(msnconn * conn, char * from, char * friendly, char * mime, char * body);

void msn_recv_file(invitation_ftp * inv, char * dest);

char * msn_find_in_mime(char * mime, const char * header);

void msn_send_file(invitation_ftp * inv, char * msg_body);

void msn_handle_NAK(msnconn * conn, char ** args, int numargs);

void msn_handle_JOI(msnconn * conn, char ** args, int numargs);

void msn_handle_BYE(msnconn * conn, char ** args, int numargs);

void msn_handle_RNG(msnconn * conn, char ** args, int numargs);

void msn_handle_statechange(msnconn * conn, char ** args, int numargs);

void msn_handle_ADD(msnconn * conn, char ** args, int numargs);

void msn_handle_REM(msnconn * conn, char ** args, int numargs);

void msn_handle_BLP(msnconn * conn, char ** args, int numargs);

void msn_handle_GTC(msnconn * conn, char ** args, int numargs);

void msn_handle_REA(msnconn * conn, char ** args, int numargs);

void msn_handle_CHL(msnconn * conn, char ** args, int numargs);

void msn_handle_OUT(msnconn * conn, char ** args, int numargs);



// Intermediate steps in connection:
class connectinfo : public callback_data
{
  public:
  char * username;
  char * password;
  connectinfo() { username=password=NULL; }
  ~connectinfo()
  { if(username!=NULL) { delete [] username; } if(password!=NULL) { delete [] password; } }
};

typedef struct _https_data {
	char *url;
	char *remote_host;
	char *lc;
	char *id;
	char *tw;
	char *cookie0, *cookie1;
	conninfo_SB * info;
	msnconn *conn;
} https_data;

void msn_connect_2(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);
void msn_connect_3(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);
void msn_connect_4(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);

// Connecting to switchboards:
void msn_SB_ans(msnconn * conn, int trid, char ** args, int numargs, callback_data * data);

void msn_add_group(msnconn *conn, char *newgroup);
void msn_del_group(msnconn *conn, char *group);
void msn_change_group(msnconn *conn, char *handle, char *oldgroup, char *newgroup);
void msn_rename_group(msnconn *conn, char *id, char *newname);
#endif // MSN_CORE_H
