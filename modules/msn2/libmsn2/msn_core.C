/* msn_core.c - this contains all the functions used to do anything with MSN */
/* WARNING - Ayttm's libmsn2 differs from the original one and Meredydd,
   its original author, does not provide support for it :) */
/* libmsn2 now uses MSNP8. Many thanks to the guys at www.hypothetic.org for
   all their great reverse-engineering works! */
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __MINGW32__
#include <sys/poll.h>
#endif
#include <fcntl.h>
#include <time.h>

#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h> // for the accept() in filetrans code
#endif
#include <sys/stat.h>

#include "md5.h"

#include "msn_core.h"
#include "msn_bittybits.h"
#include "msn_interface.h"
#include "../../../src/ssl.h" /* should  clean */

#ifdef __MINGW32__
#include <glib.h>
#define snprintf _snprintf
#define sleep(x) Sleep(x)
#define bzero(x,n) memset(x,0,n)

struct timeval zerotime = {0,0};
#endif

extern int do_msn_debug;
#define DEBUG do_msn_debug

// Define all those extern'ed variables in msn_core.h:
llist * msnconnections=NULL;

static int next_trid=10;
static char buf[1250]; // used for anything temporary

static const char * errors[1000];
static const char default_error_msg[]="Unknown error code";

static void msn_finish_netmeeting_inv(msnconn *conn, invitation_voice *inv, char *ip);

void msn_init(msnconn * conn, char * username, char * password)
{
  srand(time(NULL));

  conn->auth=new authdata_NS;
  conn->type=CONN_NS;
  conn->ready=0;
  ((authdata_NS *)conn->auth)->username=msn_permstring(username);
  ((authdata_NS *)conn->auth)->password=msn_permstring(password);

  for(int a=0; a<1000; a++)
  {
    errors[a]=default_error_msg;
  }

  errors[200]=msn_permstring("Syntax error");
  errors[201]=msn_permstring("Invalid parameter");
  errors[205]=msn_permstring("Invalid user");
  errors[206]=msn_permstring("Domain name missing from username");
  errors[207]=msn_permstring("Already logged in");
  errors[208]=msn_permstring("Invalid username");
  errors[209]=msn_permstring("Invalid friendly name");
  errors[210]=msn_permstring("User list full");
  errors[215]=msn_permstring("This user is already on this list or in this session");
  errors[216]=msn_permstring("Not on list");
  errors[217]=msn_permstring("Contact is not online");
  errors[218]=msn_permstring("Already in this mode");
  errors[219]=msn_permstring("This user is already in the opposite list");
  errors[280]=msn_permstring("Switchboard server failed");
  errors[281]=msn_permstring("Transfer notification failed");
  
  errors[300]=msn_permstring("Required fields missing");
  errors[302]=msn_permstring("Not logged in");
  
  errors[500]=msn_permstring("Internal server error");
  errors[501]=msn_permstring("Database server error");
  errors[510]=msn_permstring("File operation failed at server");
  errors[520]=msn_permstring("Memory allocation failed on server");
  errors[540]=msn_permstring("Wrong CHL value sent to server");
  
  errors[600]=msn_permstring("The server is too busy");
  errors[601]=msn_permstring("The server is unavailable");
  errors[602]=msn_permstring("Peer Notification Server is down");
  errors[603]=msn_permstring("Database connection failed");
  errors[604]=msn_permstring("Server going down (mayday, time to reboot ;-)) for maintenance");
  
  errors[707]=msn_permstring("Server failed to create connection");
  errors[711]=msn_permstring("Blocking write failed on server");
  errors[712]=msn_permstring("Session overload on server");
  errors[713]=msn_permstring("You have been too active recently. Slow down!");
  errors[714]=msn_permstring("Too many sessions open");
  errors[715]=msn_permstring("Not expected (probably no permission to set friendlyname)");
  errors[717]=msn_permstring("Bad friend file on server");
  
  errors[911]=msn_permstring("Authentication failed. Check that you typed your username (which has to contain the @domain.tld part) and password correctly.");
  errors[913]=msn_permstring("This action is not allowed while you are offline");
  errors[920]=msn_permstring("This server is not accepting new users");

  msn_add_to_llist(msnconnections, conn);
}

void msn_show_verbose_error(msnconn * conn, int errcode)
{
  if(errcode != 215 && errcode != 216 && errcode != 219
	&& errcode != 224 && errcode != 225) {
    snprintf(buf, 1024, "An error has occurred while communicating with the MSN Messenger server: \n\n %s (code %d).", errors[errcode], errcode);
    ext_show_error(conn, buf);
  }
  if(errcode == 715)
    ext_disable_conncheck();
}

void msn_invite_user(msnconn * conn, char * rcpt)
{
  snprintf(buf, sizeof(buf), "CAL %d %s\r\n", next_trid++, rcpt);
  write(conn->sock, buf, strlen(buf));
}

void msn_send_IM(msnconn * conn, char * rcpt, char * s)
{
  static char header[]="MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n";
  message * msg=new message;
  msg->body=s;
  msg->header=msn_permstring(header);
  msg->font=NULL;
  msg->colour=NULL;
  msn_send_IM(conn, rcpt, msg);

  msg->body=NULL; // don't delete s
  delete msg;
}

void msn_send_IM(msnconn * conn, char * rcpt, message * msg)
{
  char header[1024];

  if(conn->type==CONN_NS)
  {
    llist * list;

    list=msnconnections;
    while(1)
    {
      msnconn * c;
      llist * users;

      if(list==NULL) { break ; }
      c=(msnconn *)list->data;
      if(c->type==CONN_NS) { list=list->next; continue; }
      users=c->users;
      // the below sends a message into this session if the only other participant is
      // the user we want to talk to
      if(users!=NULL && users->next==NULL && !strcmp(((char_data *)users->data)->c, rcpt))
      {
        msn_send_IM(c, rcpt, msg);
        return;
      }

      list=list->next;
    }
    // otherwise, just connect
    if (conn->status && !strcmp(conn->status, "HDN")) {
	    msn_set_state(conn, "NLN");
	    msn_request_SB(conn, rcpt, msg, NULL);
	    msn_set_state(conn, "HDN");
    } else 
	    msn_request_SB(conn, rcpt, msg, NULL);
    return;
  }

  if(msg->header==NULL)
  {
    if(msg->font==NULL)
    {
      snprintf(header, sizeof(header), "MIME-Version: 1.0\r\nContent-Type: %s\r\n\r\n", (msg->content==NULL)?("text/plain; charset=UTF-8"):(msg->content));
    } else {
      char * fontname=msn_encode_URL(msg->font);
      char ef[2] = {'\0', '\0'};
      if(msg->bold) { ef[0]='B'; }
      if(msg->underline) { ef[0]='U'; }

      snprintf(header, sizeof(header), "MIME-Version: 1.0\r\nContent-Type: %s\r\nX-MMS-IM-Format: FN=%s; EF=%s; CO=%s; CS=0; PF=%d\r\n\r\n",
        (msg->content==NULL)?("text/plain"):(msg->content), fontname, ef, msg->colour, msg->fontsize);

      delete fontname;
    }
  } else {
    strncpy(header, msg->header, sizeof(header));
  }


  snprintf(buf, sizeof(buf), "MSG %d N %d\r\n%s", next_trid, strlen(header)+strlen(msg->body), header);
  write(conn->sock, buf, strlen(buf));
  write(conn->sock, msg->body, strlen(msg->body));
  next_trid++;
}

void msn_send_typing(msnconn * conn)
{
  char header[]="MIME-Version: 1.0\r\nContent-Type: text/x-msmsgscontrol\r\nTypingUser: ";
  char * username=NULL;
  
  if (conn && ((authdata_SB *)conn->auth) && ((authdata_SB *)conn->auth)->username)
    username=((authdata_SB *)conn->auth)->username;
  else 
    return;
  
  snprintf(buf, sizeof(buf), "MSG %d U %d\r\n%s%s\r\n\r\n\r\n",
        next_trid++, strlen(header)+strlen(username)+6, header, username);

  write(conn->sock, buf, strlen(buf));
}

void msn_add_to_list(msnconn * conn, char * list, char * username)
{
  snprintf(buf, sizeof(buf), "ADD %d %s %s %s\r\n", next_trid++, list, username, username);
if(DEBUG)
  printf("%s\n",buf);

  write(conn->sock, buf, strlen(buf));
}

void msn_del_from_list(msnconn * conn, char * list, char * username)
{
  snprintf(buf, sizeof(buf), "REM %d %s %s\r\n", next_trid++, list, username);
if(DEBUG)
  printf("%s\n",buf);
  
  write(conn->sock, buf, strlen(buf));
}

void msn_set_GTC(msnconn * conn, char c)
{
  snprintf(buf, sizeof(buf), "GTC %d %c\r\n", next_trid++, c);
  write(conn->sock, buf, strlen(buf));
}

void msn_set_BLP(msnconn * conn, char c)
{
  snprintf(buf, sizeof(buf), "BLP %d %cL\r\n", next_trid++, c);
  write(conn->sock, buf, strlen(buf));
}

void msn_send_ping(msnconn * conn)
{
  snprintf(buf, sizeof(buf), "PNG\r\n");
  write(conn->sock, buf, strlen(buf));
}

int msn_set_friendlyname(msnconn * conn, char * friendlyname)
{
  char	*username = NULL;
  int	res = 0;
  char	*url = NULL;
  
  username=((authdata_NS *)conn->auth)->username;
  url = msn_encode_URL(friendlyname);
  snprintf(buf, sizeof(buf), "REA %d %s %s\r\n", next_trid++, username, url);
  delete [] url;
  
  res = write(conn->sock, buf, strlen(buf));
  
  return (res);
}

msnconn *find_nsconn_by_name(char *name)
{
    llist * list;

    list=msnconnections;
    for(; list; list = list->next)
    {
      msnconn * c;
      
      c=(msnconn *)list->data;
      if(c->type==CONN_NS &&
         !strcmp (name,((authdata_SB *)c->auth)->username)) {
	      /* printf("yeah: %s\n",((authdata_NS *)c->auth)->username); */
	      return c;
      }
   }
   return NULL;
}

void msn_sync_lists(msnconn * conn, int version)
{
  syncinfo *info=new syncinfo;
  ext_syncing_lists(conn, 1);
  info->serial=version;

  snprintf(buf, sizeof(buf), "SYN %d %d\r\n", next_trid, version);
  write(conn->sock, buf, strlen(buf));
  info->total_lst = -1; /* init */
  msn_add_callback(conn, msn_syncdata, next_trid, info);

  next_trid++;
}

int is_on_list(char *handle, llist *lst) 
{
	llist *olist;
	userdata *ocontact;
	for(olist=lst; olist != NULL && olist->data != NULL; olist=olist->next) {
		ocontact = (userdata *)olist->data;
		if(!strcasecmp(ocontact->username, handle))
			return 1;
	}
	return 0;
	
}
void msn_syncdata(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  syncinfo * info=(syncinfo *)data;

  if(!strcmp(args[0], "SYN"))
  {
    if(numargs >=3 && info && info->serial==atoi(args[2]))
    {
    /*  delete info;
      info=NULL;*/
      msn_del_callback(conn, trid);
      ext_syncing_lists(conn, 0);
      ext_got_info(conn, NULL);
      return;
    } else if (info) {
      info->serial=atoi(args[2]);
      ext_latest_serial(conn, info->serial);
    }
    info->total_lst = atoi(args[3]);
  }

  if(!strcmp(args[0], "LST"))
  {
    char *handle = args[1];
    char *fname  = args[2];	  
    int lists = atoi(args[3]);
    char *groups = args[4];
    
    info->total_lst--;
    
    if(numargs >=3 && lists&LIST_FL) 
    {
        userdata * newuser=new userdata();
        newuser->username=msn_permstring(handle);
        newuser->friendlyname=msn_decode_URL(msn_permstring(fname));
	newuser->groups=msn_permstring(groups);
	ext_got_friend(conn, newuser->username, newuser->groups);
        msn_add_to_llist(info->fl, newuser);
    }
    if(numargs >=3 && lists&LIST_RL) 
    {
        userdata * newuser=new userdata();
        newuser->username=msn_permstring(handle);
        newuser->friendlyname=msn_decode_URL(msn_permstring(fname));
        msn_add_to_llist(info->rl, newuser);
    }
    if(numargs >=3 && lists&LIST_AL) 
    {
        userdata * newuser=new userdata();
        newuser->username=msn_permstring(handle);
        newuser->friendlyname=msn_decode_URL(msn_permstring(fname));
        msn_add_to_llist(info->al, newuser);
    }
    if(numargs >=3 && lists&LIST_BL) 
    {
        userdata * newuser=new userdata();
        newuser->username=msn_permstring(handle);
        newuser->friendlyname=msn_decode_URL(msn_permstring(fname));
        msn_add_to_llist(info->bl, newuser);
    }
  }
  
  if(numargs >=3 && !strcmp(args[0], "LSG"))
  {
	  ext_got_group(conn, args[1], msn_decode_URL(args[2]));
	  return;
  }
  
  if(numargs >=1 && !strcmp(args[0], "GTC"))
  {
    info->gtc=args[3][0];
    info->complete|=COMPLETE_GTC;
    ext_got_GTC(conn, args[3][0]);
  }

  if(numargs >=1 && !strcmp(args[0], "BLP"))
  {
    info->blp=args[3][0];
    info->complete|=COMPLETE_BLP;
    ext_got_BLP(conn, args[3][0]);
  }

  if(info->total_lst == 0)
  {
    msn_del_callback(conn, trid);
    msn_check_rl(conn, info);
    ext_syncing_lists(conn, 0);
    ext_got_info(conn, info);
   /* delete info; */
  }
  
}

void msn_check_rl(msnconn * conn, syncinfo * info)
{
  llist * list; // FL
  llist * olist; // other list
  llist * flist;
  userdata * contact;
  userdata * ocontact;

  int is_on_list;
  int a=0;

  list = info->fl;
  while (list != NULL) {
    int is_on_al=0;

    contact = (userdata *)list->data;
if(DEBUG)    
    printf("checking if %s is on AL\n",contact->username);
    
    for(olist=info->al; olist != NULL && olist->data != NULL; olist=olist->next) {
      ocontact = (userdata *)olist->data;
      if(!strcasecmp(ocontact->username, contact->username))
          is_on_al=1;
      if (is_on_al)
	  break;
    }
    if (!is_on_al) {
if(DEBUG)    
       printf("  adding %s to AL\n",contact->username);

       msn_add_to_list(conn, "AL", contact->username);
    }
    list=list->next;
  }
  
  flist=info->rl;

  while(flist!=NULL)
  {
    userdata * fcontact=(userdata *)flist->data;
    is_on_list=0;

    a=0;
    for(olist=info->al; a<2; olist=info->bl, a++)
    {
      while(olist!=NULL)
      {
        ocontact=(userdata *)olist->data;
if(DEBUG)
        printf("Comparing %s to %s\n", ocontact->username, fcontact->username);

        if(!strcasecmp(ocontact->username, fcontact->username))
        {
          is_on_list=1;
          break;
        }
        olist=olist->next;
      }
      if(is_on_list) { break; } // avoid a second loop if unnecessary
    }

    if(!is_on_list)
    {
      ext_new_RL_entry(conn, fcontact->username, fcontact->friendlyname);
    }

    flist=flist->next;
  }
}

void msn_new_SB(msnconn * nsconn, void * tag)
{
  msn_request_SB(nsconn, NULL, NULL, tag);
}

void msn_request_SB(msnconn * nsconn, char * rcpt, message * msg, void * tag)
{
  conninfo_SB * info=new conninfo_SB;

  info->auth=new authdata_SB;
  info->auth->username=msn_permstring(((authdata_NS *)nsconn->auth)->username);
  info->auth->rcpt=msn_permstring(rcpt);
  if(msg==NULL)
  {
    info->auth->msg=NULL;
  } else {
    info->auth->msg=new message;
    info->auth->msg->header=msn_permstring(msg->header);
    info->auth->msg->body=msn_permstring(msg->body);
    info->auth->msg->font=msn_permstring(msg->font);
    info->auth->msg->colour=msn_permstring(msg->colour);
    info->auth->msg->content=msn_permstring(msg->content);
    info->auth->msg->bold=msg->bold;
    info->auth->msg->italic=msg->italic;
    info->auth->msg->underline=msg->underline;
  }

  info->auth->tag=tag;

  snprintf(buf, sizeof(buf), "XFR %d SB\r\n", next_trid);
  write(nsconn->sock, buf, strlen(buf));

  msn_add_callback(nsconn, msn_SBconn_2, next_trid, info);
  next_trid++;
}


static void msn_https_cb1(int fd, int error, void *data);
static void msn_https_cb2(int fd, int error, void *data);

void msn_SBconn_2(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  conninfo_SB * info=(conninfo_SB *)data;

  msn_del_callback(conn, trid);

  if(!strcmp(args[0], "USR") && !strcmp(args[2], "TWN")) {
	 char *url = strdup(args[4]);
	 https_data *hdata = (https_data *)malloc(sizeof(https_data));
	 
	 char *ru = NULL, *realurl = NULL, *endurl = NULL, *finalurl = NULL;
	 char *lc=NULL, *id=NULL, *tw=NULL;
	 
	 char *remote_host = NULL;

	 if (strstr(((authdata_NS *)conn->auth)->username,"@hotmail.com"))
	 	remote_host = "loginnet.passport.com";
	 else if (strstr(((authdata_NS *)conn->auth)->username,"@msn.com"))
	 	remote_host = "msnialogin.passport.com";
	 else
	 	remote_host = "login.passport.com";

	 while (strstr(url, ",")) {
		*(strstr(url,",")) = '&'; 
	 }
	 
	 lc = strdup(strstr(url, "lc=")+3);
	 id = strdup(strstr(url, "id=")+3);
	 tw = strdup(strstr(url, "tw=")+3);
	 ru = strstr(url, "ru=")+3;
	 
	 *(strstr(lc, "&")) = 0;
	 *(strstr(id, "&")) = 0;
	 *(strstr(tw, "&")) = 0;
	 endurl = strstr(ru, "&");
	 
	 realurl=strdup("http://messenger.msn.com"); /* shouldn't be hardcoded, better translate ru= param */
	 *ru = 0;
	 
	 finalurl = (char *)malloc(strlen(url)+strlen(realurl)+strlen(endurl)+1);
	 snprintf(finalurl, strlen(url)+strlen(realurl)+strlen(endurl),
			 "%s%s%s", url, realurl, endurl);
	 	 
	 snprintf(buf, sizeof(buf), "GET /login.srf?%s HTTP/1.0\r\n\r\n", finalurl);
	 
	 if (DEBUG) printf("---URL---\n%s\n---END---\n", buf);
	 
	 hdata->url = strdup(buf);
	 hdata->remote_host = strdup(remote_host);
	 hdata->lc = strdup(lc);
	 hdata->id = strdup(id);
	 hdata->tw = strdup(tw);
	 hdata->conn = conn;
	 hdata->info = info;
	 
	 free(lc);
	 free(id);
	 free(tw);
	 free(finalurl);
	 free(realurl);
	 free(url);
	 
	 if (ext_async_socket(remote_host, 443, (void *)msn_https_cb1, hdata) < 0) {
		 if(DEBUG) printf("immediate connect failure to %s\n", remote_host);    
		 ext_show_error(conn, "Could not connect to MSN HTTPS server.");
		 ext_closing_connection(conn);
	 }
	 return;
  }
  
  if(strcmp(args[0], "XFR"))
  {
    msn_show_verbose_error(conn, atoi(args[0]));
    delete info;
    return;
  }

  if (numargs < 6) return;
  info->auth->cookie=msn_permstring(args[5]);
  info->auth->sessionID=NULL;

  msnconn * newconn=new msnconn;

  newconn->auth=info->auth;
  newconn->type=CONN_SB;
  newconn->ready=0;
  newconn->ext_data = conn->ext_data;
  
  msn_add_to_llist(msnconnections, newconn);

  int port=1863;
  char * c;

  if((c=strstr(args[3], ":"))!=NULL)
  {
    *c='\0';
    c++;
    port=atoi(c);
  }

  delete info;

  msn_connect(newconn, args[3], port);
}

static void msn_https_cb1(int fd, int error, void *data) 
{
	 SockInfo *sock = (SockInfo*)malloc(sizeof(SockInfo));
	 char *urlread = NULL;
	 char *cookie0 = NULL, *cookie1 = NULL, *cookie2 = NULL;
	 char *tmp = NULL;
	 https_data *hdata = (https_data *)data;	
	 
	 sock->sock = fd;
	 if (DEBUG) printf("sock->sock = %d\n",sock->sock);
	 if (DEBUG) printf("entering msn_https_cb1\n");
	 if(fd == -1 || error)
	 {
		 ext_show_error(hdata->conn, "Could not connect to https server.");
		 return;
	 }

	 ssl_init();
	 if (!ssl_init_socket(sock, hdata->remote_host, 80)) {
		 ext_show_error(hdata->conn, "Could not connect to MSN HTTPS server (ssl error).");
		 return;
	 }
	 
	 ssl_write(sock->ssl, hdata->url, strlen(hdata->url));
	 
	 while (ssl_read(sock->ssl, buf, sizeof(buf))) {
		 size_t s = (size_t)strlen(buf) +1;
		 
		 if (urlread) {
			 s += strlen(urlread)+1;
			 tmp = strdup(urlread);
		 } 
		 
		 urlread = (char *)realloc(urlread, s);
		 
		 snprintf(urlread, s-1, "%s%s", tmp?tmp:"", buf);
		 free(tmp); tmp = NULL;
		 if(strstr(buf, "</HTML>"))
			 break;
		 bzero(buf, sizeof(buf));
	 }
	 
	 if (DEBUG) printf("---ANSWER---\n%s\n---END---\n",urlread);
	 
	 if (!urlread || !strstr(urlread, "BrowserTest") || !strstr(urlread, "MSPPost")) {
		 ext_show_error(hdata->conn, "Could not connect to MSN HTTPS server (bad cookies).");
		 ext_closing_connection(hdata->conn);
		 return;
	 }
	 
	 tmp = strdup(strstr(urlread, "BrowserTest"));
	 *(strstr(tmp+1, "\r\n")) = 0;
	 hdata->cookie0 = strdup(tmp);
	 free(tmp); tmp = NULL;
	 
	 tmp = strdup(strstr(urlread, "MSPPost"));
	 *(strstr(tmp+1, "\r\n")) = 0;
	 hdata->cookie1 = strdup(tmp);
	 free(tmp); tmp = NULL;
	 
	 if (DEBUG) printf("got cookies: Cookie1: %s\nCookie2: %s\n", hdata->cookie0, hdata->cookie1);
	 
	 
	 snprintf(buf, sizeof(buf),  
		"GET /ppsecure/post.srf?lc=%s&id=%s&tw=%s&cbid=%s&da=passport.com&login=%s&domain=%s&passwd=%s&sec=&mspp_shared=&padding= HTTP/1.0\r\n"
		"Cookie: %s\r\n"
		"Cookie: %s\r\n\r\n",
		hdata->lc, hdata->id, hdata->tw, hdata->id,
		((authdata_NS *)hdata->conn->auth)->username,
		"passport.com",
		"************",
		hdata->cookie0, hdata->cookie1);
	 
	 if (DEBUG) printf("---URL---\n%s\n---END---\n", buf);

	 bzero(buf, sizeof(buf));
	 snprintf(buf, sizeof(buf),  
		"GET /ppsecure/post.srf?lc=%s&id=%s&tw=%s&cbid=%s&da=passport.com&login=%s&domain=%s&passwd=%s&sec=&mspp_shared=&padding= HTTP/1.0\r\n"
		"Cookie: %s\r\n"
		"Cookie: %s\r\n\r\n",
		hdata->lc, hdata->id, hdata->tw, hdata->id,
		((authdata_NS *)hdata->conn->auth)->username,
		"passport.com",
		((authdata_NS *)hdata->conn->auth)->password,
		hdata->cookie0, hdata->cookie1);
	 
	 
	 ssl_done_socket(sock);
	 free(sock->hostname);
	 sock->ssl = NULL;
	 close(sock->sock);
	 
	 free (hdata->url); 
	 hdata->url = strdup(buf);
	 
	 if (ext_async_socket(hdata->remote_host, 443, (void *)msn_https_cb2, hdata) < 0) {
		 if(DEBUG) printf("immediate connect failure to %s\n", hdata->remote_host);    
		 ext_show_error(hdata->conn, "Could not connect to MSN HTTPS server.");
	 }
}
 	 
static void msn_https_cb2(int fd, int error, void *data) 
{
	 SockInfo *sock = (SockInfo*)malloc(sizeof(SockInfo));
	 char *urlread = NULL;
	 char *cookie0 = NULL, *cookie1 = NULL, *cookie2 = NULL;
	 char *tmp = NULL;	
	 https_data *hdata = (https_data *)data;	
	 sock->sock = fd;
	 
	 if (DEBUG) printf("entering msn_https_cb2\n");
	 if(fd == -1 || error)
	 {
		 ext_show_error(hdata->conn, "Could not connect to https server.");
		 return;
	 }

	 if (!ssl_init_socket(sock, hdata->remote_host, 80)) {
		 ext_show_error(hdata->conn, "Could not connect to MSN HTTPS server (ssl error).");
		 return;
	 }
	 
	 ssl_write(sock->ssl, hdata->url, strlen(hdata->url));
	 
	 free(urlread); urlread = NULL;
	 free(tmp); tmp = NULL;

	 bzero(buf, sizeof(buf));
	 while (ssl_read(sock->ssl, buf, sizeof(buf))) {
		 size_t s = (size_t)strlen(buf) +1;
		 
		 if (urlread) {
			 s += strlen(urlread)+1;
			 tmp = strdup(urlread);
		 } 
		 
		 urlread = (char *)realloc(urlread, s);
		 
		 snprintf(urlread, s-1, "%s%s", tmp?tmp:"", buf);
		 free(tmp); tmp = NULL;
		 if(strstr(buf, "</HTML>"))
			 break;
		 bzero(buf, sizeof(buf));
		 
	 }
	 ssl_done_socket(sock);
	 free(sock->hostname);
	 sock->ssl = NULL;
	 close(sock->sock);

	 if (DEBUG) printf("---ANSWER---\n%s\n---END---\n", urlread);
	 
	 if (urlread && (tmp = strstr(urlread, "passportdone.asp")) != NULL) {
		 char *t = NULL;
		 char *p = NULL;
		 
		 t = strdup(strstr(tmp,"&t=")+3);
		 p = strdup(strstr(tmp,"&p=")+3);
		 *strstr(t, "&") = 0;
		 *strstr(p, "\"") = 0;
	 
		 bzero(buf, sizeof(buf));
		 snprintf(buf, sizeof(buf),
			"USR %d TWN S t=%s&p=%s\r\n", next_trid, t, p);
	 
	 	 free (t);
		 free (p);
		 write(hdata->conn->sock, buf, strlen(buf));
		 msn_add_callback(hdata->conn, msn_connect_4, next_trid, hdata->info);
		 
		 next_trid++;
	 } else {
		msn_show_verbose_error(hdata->conn, 911);
   		msn_clean_up(hdata->conn);
	 }
	
	  free(hdata->url);
	  free(hdata->remote_host);
	  free(hdata->lc);
	  free(hdata->id);
	  free(hdata->tw);
	  free(hdata->cookie0);
	  free(hdata->cookie1);
	  free(hdata);
	  hdata=NULL;	 
}

 
void msn_SBconn_3(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  authdata_SB * auth=(authdata_SB *)conn->auth;

  msn_del_callback(conn, trid);

  if (numargs < 3) return;
  if(strcmp(args[2], "OK"))
  {
    msn_show_verbose_error(conn, atoi(args[0]));
    msn_clean_up(conn);
    return;
  }

  if(auth->rcpt==NULL) // they're requesting the SB session the proper way
  {
    ext_got_SB(conn, auth->tag);
  } else {
    snprintf(buf, sizeof(buf), "CAL %d %s\r\n", next_trid, auth->rcpt);
    write(conn->sock, buf, strlen(buf));

    delete [] auth->rcpt;
    auth->rcpt=NULL;

    next_trid++;
  }
  conn->ready=1;
  ext_new_connection(conn);
}

void msn_handle_incoming(msnconn *conn, int readable, int writable,
		char **args, int numargs)
{
  // First, we find which msnconn this socket belongs to

  callback * call = NULL;
  int trid=0;

  // first, siphon off any file transfer traffic to the special handler
  if(conn->type==CONN_FTP)
  { //msn_handle_filetrans_incoming(conn, readable, writable, args, numargs); 
	  printf("WHY THE FUCK IS CONN_FTP HANDLED HERE?\n");
    return; }

  // OK, it's for us. If it's readable, parse it, then deliver it to the appropriate handler

  if(!readable) { return; }

  if(args==NULL)
  { 
	  ext_show_error(conn,"MSN connection has been reset.");
	  msn_clean_up(conn); 
	  return; 
  }

  if(numargs >=3 && !strcmp(args[0], "XFR") && !strcmp(args[2], "NS"))
  {
    delete conn->callbacks; // delete the callback data
    conn->callbacks=NULL;

    ext_unregister_sock(conn, conn->sock);
    close(conn->sock);

    char * c = NULL;
    int port=1863;

    if(numargs >=4 && (c=strstr(args[3], ":"))!=NULL)
    {
      *c='\0';
      c++;
      port=atoi(c);
    }

    msn_connect(conn, args[3], port);
    return;
  }

  if(!strcmp(args[0], "RNG"))
  {
    msn_handle_RNG(conn, args, numargs);
    return;
  }
  
  if (!strcmp(args[0], "LSG")) {
	msn_syncdata(conn, 0, args, numargs, NULL);
	return;  
  }
  
  if(numargs >=2)
	  trid=atoi(args[1]);

  llist *list=conn->callbacks;

  if(list!=NULL && trid>0)
  {
    while(1)
    {
      call=(callback *)list->data;
      if(call->trid==trid)
      {
        (call->func)(conn, trid, args, numargs, call->data);
        return;
      }
      list=list->next;
      if(list==NULL) { break; } // defaults
    }
  } else if (list!=NULL && !strcmp(args[0],"LST")) {
	  /* hack because LST don't have trid anymore */
    while(1)
    {
      call=(callback *)list->data;
      if(call->func==msn_syncdata)
      {
        (call->func)(conn, trid, args, numargs, call->data);
        return;
      }
      list=list->next;
      if(list==NULL) { break; } // defaults
    }
  }

  msn_handle_default(conn, args, numargs);

}

void msn_handle_close(int sock)
{
  // First, we find which msnconn this socket belongs to

  llist * list;
  msnconn * conn;

  list=msnconnections;

  if(list==NULL) { return; }

  while(1)
  {
    conn=(msnconn *)list->data;
    if(conn->sock==sock)
    { break; }
    list=list->next;
    if(list==NULL)
    { if(DEBUG) printf("Socket close not for us\n"); 
    return; } // not for us
  }

  msn_clean_up(conn);
}

void msn_handle_default(msnconn * conn, char ** args, int numargs)
{

  //     Switchboard messages

  if(!strcmp(args[0], "MSG"))
  {
    msn_handle_MSG(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "NAK"))
  {
    msn_handle_NAK(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "JOI"))
  {
    msn_handle_JOI(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "BYE"))
  {
    msn_handle_BYE(conn, args, numargs);
    return;
  }

  //    Notification server messages

  if(!strcmp(args[0], "NLN") || !strcmp(args[0], "ILN") || !strcmp(args[0], "FLN"))
  {
    msn_handle_statechange(conn, args, numargs);
    return;
  }

  if(numargs >=3 && !strcmp(args[0], "CHG"))
  {
    ext_changed_state(conn, args[2]);
    return;
  }

  if(!strcmp(args[0], "ADD"))
  {
    msn_handle_ADD(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "REM"))
  {
    msn_handle_REM(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "BLP"))
  {
    msn_handle_BLP(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "GTC"))
  {
    msn_handle_GTC(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "REA"))
  {
    msn_handle_REA(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "QNG"))
  {
    ext_got_pong(conn);
    return;
  }

  if(!strcmp(args[0], "CHL"))
  {
    msn_handle_CHL(conn, args, numargs);
    return;
  }

  if(!strcmp(args[0], "OUT"))
  {
    msn_handle_OUT(conn, args, numargs);
    return;
  }
  
  if(numargs >=5 && !strcmp(args[0], "ADG"))
  {
	  ext_got_group(conn, args[4], msn_decode_URL(args[3]));
	  return;
  }
  
  if(isdigit(args[0][0]) && strlen(args[0])>2)
  {
    msn_show_verbose_error(conn, atoi(args[0]));
    if(conn->type==CONN_SB)
    {
      if(DEBUG)
      printf("As it is a Switchboard connection, terminating on error.\n");
      
      msn_clean_up(conn);
    }
    return;
  }

  if(DEBUG) {
    printf("Don't know what to do with this one, ignoring it:\n"); // DEBUG
    for(int a=0; a<numargs; a++)
    {
      printf("%s ", args[a]);
    }
    printf("\n");
  }  
}

void msn_handle_MSG(msnconn * conn, char ** args, int numargs)
{
  int msglen, remaining;
  char * msg = NULL;
  char * mime;
  char * body;
  char * tmp;
  int tries = 0;
  if (numargs < 4)
	  return;

  msglen=atoi(args[3]);

  msg=(char *)malloc(msglen+1);
  memset(msg,'\0',msglen);
  ext_unregister_sock(conn, conn->sock);

  remaining=msglen;
  do {
	  char tbuf[1250]="";
	  int i=read(conn->sock, tbuf, remaining);
	  if (errno == EAGAIN || i < remaining) {
		  sleep(1);
		  tries++;
	  }
	  if (i>=0) {
	  	remaining -= i;
  	  }
	  strncat(msg, tbuf, msglen-strlen(msg));
  } while (remaining > 0 && tries < 6);
  
  ext_register_sock(conn, conn->sock, 1, 0);
  
  msg[msglen]='\0';

  mime=msg;
  body=strstr(msg, "\r\n\r\n");
  if(body!=NULL) { body[2]='\0'; /* finish the MIME string */ body+=4; }

    // the below is a kludge until I remember the header name for TypingUser
  if((strstr(mime, "TypingUser")!=NULL) || (strstr(mime, "TypeingUser")!=NULL))
  { // the second of the above two is a workaround for a spelling bug in the Jabber MSN transport
    ext_typing_user(conn, args[1], msn_decode_URL(args[2]));
    free(msg);
    return;
  }

  /* Warning - the code below was written at what my body clock insisted was past midnight,
  jetlagged, on a foreign road, because I had nothing better to do. I would like to please
  call to your attention the no-warranty clause in the GPL.... :^) */

  char * content; // content-type

  content=msn_find_in_mime(mime, "Content-Type");
  if(content==NULL) { 
	  printf("mime:%s\n",mime);
	  printf("body:%s\n",body);
	  delete msg; 
	  return; 
  }
  if(DEBUG)
    printf("Content type: \"%s\"\n", content);
  
  if((tmp=strstr(content, "; charset"))!=NULL) { *tmp='\0'; }

  if(!strcmp(content, "text/plain"))
  {
    message * msg=new message;
    msg->header=msn_permstring(mime);
    msg->body=(body != NULL) ? strdup(body):strdup("");
    msg->font=NULL;
    msg->content=msn_find_in_mime(mime, "Content-Type"); // include any "charset=" I've chopped off
    ext_got_IM(conn, args[1], msn_decode_URL(args[2]), msg);
    delete msg;    
  } else if(!strcmp(content, "text/x-msmsgsinitialemailnotification")) {
    char * unread_ibc;
    char * unread_folc;
    int unread_ib=0, unread_fol=0;

    unread_ibc=msn_find_in_mime(body, "Inbox-Unread");
    unread_folc=msn_find_in_mime(body, "Folders-Unread");
    if(unread_ibc!=NULL) { unread_ib=atoi(unread_ibc); delete unread_ibc; }
    if(unread_folc!=NULL) { unread_fol=atoi(unread_folc); delete unread_folc; }

    ext_initial_email(conn, unread_ib, unread_fol);
  } else if(!strcmp(content, "text/x-msmsgsemailnotification")) {
    char * from=msn_find_in_mime(body, "From-Addr");
    char * subject=msn_find_in_mime(body, "Subject");

    ext_new_mail_arrived(conn, from, subject);

    delete from;
    delete subject;
  } else if(!strcmp(content, "text/x-msmsgsinvite")) {
    msn_handle_invite(conn, args[1], msn_decode_URL(args[2]), mime, body);
  } else {
    if(DEBUG)
      printf("Unknown content-type: \"%s\"\n", content);
    
  }
  delete [] content;
  free(msg);
}

char * msn_find_in_mime(char * mime, char * header)
{
  char * retval;
  int pos;

  if(!strncmp(mime, header, strlen(header)))
  {
    retval=mime;
  } else {
    retval=strstr(mime, header);
    if(retval==NULL) { return NULL; }
    retval+=2;
  }

  while(*retval!=':') { retval++; }
  retval++; // pass the colon
  while(isspace(*retval)) { retval++; } // position at start of value

  pos=0;
  while(retval[pos]!='\0')
  {
    if(retval[pos]=='\r')
    {
      char * tmp;
      retval[pos]='\0';
      tmp=msn_permstring(retval);
      retval[pos]='\r';
      return tmp;
    }
    pos++;
  }
  
  /* Should not be reached if header found */
  return NULL;
}

void msn_handle_invite(msnconn * conn, char * from, char * friendly, char * mime, char * body)
{
  char * command=msn_find_in_mime(body, "Invitation-Command");
  char * cookie=msn_find_in_mime(body, "Invitation-Cookie");
  int inv_is_out=0;
  invitation * inv=NULL;
  llist * l;

  l=conn->invitations_in;
  while(1)
  {
    if(l==NULL)
    { if(!inv_is_out) { l=conn->invitations_out; inv_is_out=1; continue; } 
      else { break; } }
    inv=(invitation *)l->data;
    if(inv) {
if(DEBUG)
    printf("invitation: checking %s against %s\n", inv->cookie, cookie);

    if(!strcmp(inv->cookie, cookie))
    { break ; }
    }
    inv=NULL;
    l=l->next;
  }
  delete cookie;

  if(!strcmp(command, "INVITE"))
  {
    msn_handle_new_invite(conn, from, friendly, mime, body);
  } else if(!strcmp(command, "ACCEPT")) {
    if(inv==NULL)
    { printf("Very odd - just got an ACCEPT out of mid-air...\n"); delete command; return; }

    if(!inv_is_out && inv->app==APP_FTP)
    {
if(DEBUG)
      printf("Downloading file from remote host..\n");

      msn_recv_file((invitation_ftp *)inv, body);
    } else if(inv_is_out && inv->app==APP_FTP) {
      msn_send_file((invitation_ftp *)inv, body);
    } else if(!inv_is_out && inv->app == APP_NETMEETING) {
	    char *ip = msn_find_in_mime(body, "IP-Address");
	    /* got a voice invitation and we agreed */
	    ext_start_netmeeting(ip);
	    free(ip);
    } else if(inv_is_out && inv->app == APP_NETMEETING) {
            msn_finish_netmeeting_inv(conn, (invitation_voice *)inv,
			    msn_find_in_mime(body,"\nIP-Address"));
    }
  } else if(!strcmp(command, "CANCEL") || !strcmp(command, "REJECT")) {
    if(inv==NULL)
    {
      printf("Very odd - just got a CANCEL/REJECT out of mid-air...\n");
      delete command;
      return;
    }
    if(inv->app==APP_FTP)
    {
      ext_filetrans_failed((invitation_ftp *)inv, 0, "Cancelled by remote user.");
      if(inv_is_out)
      {
        msn_del_from_llist(conn->invitations_out, inv);
      } else {
        msn_del_from_llist(conn->invitations_in, inv);
      }
      delete inv;
    } else {
      ext_show_error(conn,"Contact refused our invitation.\n");
      if(inv_is_out)
      {
        msn_del_from_llist(conn->invitations_out, inv);
      } else {
        msn_del_from_llist(conn->invitations_in, inv);
      }
      delete inv;
    }
  } else {
    printf("Argh, don't support %s yet!\n(%s)", command,body);
  }

  delete command;
}

void msn_handle_new_invite(msnconn * conn, char * from, char * friendlyname, char * mime, char * body)
{

  char * appname;

  char * tmp1=NULL;
  char * tmp2=NULL;
  int recognized = 0;
  
  appname=msn_find_in_mime(body, "Application-Name");
  invitation * invg=NULL;

  if((tmp1=msn_find_in_mime (body, "Application-File")) != NULL
  && (tmp2=msn_find_in_mime (body, "Application-FileSize")) != NULL)
  {
    invitation_ftp * inv=new invitation_ftp;
    invg=inv;
    invg->app=APP_FTP;
    invg->other_user=msn_permstring(from);
    invg->cookie=msn_find_in_mime(body, "Invitation-Cookie");
    invg->conn=conn;
    inv->filename=tmp1;
    tmp1=NULL;
    inv->filesize=atol(tmp2);

    ext_filetrans_invite(conn, from, friendlyname, inv);
    if(tmp1!=NULL) { delete tmp1; tmp1=NULL;}
    if(tmp2!=NULL) { delete tmp2; tmp1=NULL;}
  } else if ((tmp1=msn_find_in_mime(body, "Session-Protocol")) != NULL) {
    tmp2=msn_find_in_mime(body, "Context-Data");
    invitation_voice * inv=new invitation_voice;
    invg=inv;	  
    invg->app=tmp2 ? APP_VOICE:APP_NETMEETING;
    invg->other_user=msn_permstring(from);
    invg->cookie=msn_find_in_mime(body, "Invitation-Cookie");
    invg->conn=conn;
    inv->sessionid=msn_find_in_mime(body, "Session-ID");

    if(invg->app == APP_VOICE) {
	/* 
	   SIP doesn't seem really fun to implement
	
	   http://www.radvision.com/papers/C1_What_is_SIP.html 
	   http://www.cs.ucl.ac.uk/staff/J.Crowcroft/mmbook/book/node185.html
	   http://www.cs.columbia.edu/~kns10/software/siplib/ <<<<<==== this is for the one who'll implement it here ;-)
	   http://www.linphone.org/ <= probably simpler ;-)
	   RFC3428
	 */
	snprintf(buf, sizeof(buf), "%s (%s) would like to have a voice chat "
			"with you, but they use the SIP MSN Voice Protocol. "
			"Ayttm doesn't support SIP yet.\n"
			"You should ask your contact to use netmeeting instead.",
			friendlyname, from);
	ext_show_error(conn, buf);
	delete tmp2;
	delete tmp1;
	msn_netmeeting_reject(inv);
	msn_del_from_llist(conn->invitations_in, invg);
	delete invg;
	invg=NULL;
	recognized = 1;
    } else {
	ext_netmeeting_invite(conn, from, friendlyname, inv);

	if(tmp1!=NULL) { delete tmp1; tmp1=NULL;}
	if(tmp2!=NULL) { delete tmp2; tmp1=NULL;}
    }
  }
  

  delete appname;

  if(invg==NULL && !recognized)
  {
    ext_show_error(conn, "Unknown invitation type!");
  }
  else {
    msn_add_to_llist(conn->invitations_in, invg);
  }
}

void msn_recv_file(invitation_ftp * inv, char * msg_body)
{
  char * cookie=msn_find_in_mime(msg_body, "AuthCookie");
  char * remote=msn_find_in_mime(msg_body, "IP-Address");
  char * port_c=msn_find_in_mime(msg_body, "Port");
  int port;

  if(cookie==NULL || remote==NULL || port_c==NULL)
  {
    ext_filetrans_failed(inv, 0, "Missing parameters.");
    msn_del_from_llist(inv->conn->invitations_in, inv);
    if(cookie!=NULL) { delete cookie; }
    if(remote!=NULL) { delete remote; }
    if(port_c!=NULL) { delete port_c; }
    delete inv;
  }

  port=atoi(port_c);
  delete port_c;

  msnconn * conn=new msnconn;
  conn->type=CONN_FTP;
  conn->ext_data = inv->conn->ext_data;
  authdata_FTP * auth=new authdata_FTP;
  auth->cookie=msn_permstring(cookie);
  delete cookie;
  auth->inv=inv;
  auth->username=msn_permstring(((authdata_SB *)inv->conn->auth)->username);
  auth->direction=MSNFTP_RECV;

  conn->auth=auth;


  snprintf(buf, sizeof(buf), "Connecting to %s:%d\n", remote, port);
  ext_filetrans_progress(inv, buf, 0, 0);

  conn->sock=ext_connect_socket(remote, port);
  delete remote;

  if(conn->sock<0)
  {
    ext_filetrans_failed(inv, errno, strerror(errno));
    msn_del_from_llist(inv->conn->invitations_in, inv);
    delete cookie;
    delete inv;
    return;
  }

  ext_register_sock(conn, conn->sock, 1, 0);

  ext_filetrans_progress(inv, "Connected", 0, 0);

  msn_add_to_llist(msnconnections, conn);

  write(conn->sock, "VER MSNFTP\r\n", strlen("VER MSNFTP\r\n"));
}


void msn_filetrans_cancel(invitation_ftp * inv)
{
  if (!inv)
	  return;
  inv->cancelled = 1;
}

static void msn_filetrans_cancel_clean(invitation_ftp *inv) 
{
  llist * l;
  msnconn * conn;
  // one of the two below will fail, but it will do so safely and quietly
  msn_del_from_llist(inv->conn->invitations_in, inv);
  msn_del_from_llist(inv->conn->invitations_out, inv);

  l=msnconnections;

  while(1)
  {
    if(l==NULL) { delete inv; return; } // Hmm, couldn't find it...

    conn=(msnconn *)l->data;

    if(conn->type==CONN_FTP && ((authdata_FTP *)(conn->auth))->inv==inv)
    { break; }

    l=l->next;
  }

  msn_clean_up(conn);
  close(conn->ssock);
}

void msn_handle_filetrans_incoming(msnconn * conn, int readable, int writable)
{
  authdata_FTP * auth=(authdata_FTP *)conn->auth;

  if(auth->direction==MSNFTP_RECV)
  {
    if(!readable) { return; } // not interested...

    if(auth->fd==-1)
    {
      char ** args;
      int numargs;

      args=msn_read_line(conn, &numargs);

      if(args==NULL) { 
	      ext_show_error(conn,"MSN connection has been reset.");
	      msn_clean_up(conn); 
	      return; 
      }

      if(!strcmp(args[0], "VER"))
      {
        snprintf(buf, sizeof(buf), "USR %s %s\r\n", auth->username, auth->cookie);
        write(conn->sock, buf, strlen(buf));
        ext_filetrans_progress(auth->inv, "Negotiating", 0, 0);
      } else if (!strcmp(args[0], "FIL")) {
        auth->fd=open(auth->inv->filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
        if(auth->fd<0)
        {
          ext_filetrans_failed(auth->inv, errno, strerror(errno));
          msn_del_from_llist(conn->invitations_in, auth->inv);
          msn_clean_up(conn);
        } else {
          write(conn->sock, "TFR\r\n", strlen("TFR\r\n"));
	}
      } else if (!strcmp(args[0], "CCL")) {
        if(DEBUG)
          printf("got CCL\n");
	msn_clean_up(conn);      
      }
      delete [] args[0];
      delete [] args;
      auth->num_ignore=3;
    }

    
    if (auth->inv->cancelled) {
	    write(conn->sock,"CCL\r\n",5);
	    if(DEBUG) printf("Cancelling reception\n");
	    ext_filetrans_failed((invitation_ftp *)auth->inv, 0, "You cancelled the transfer.");
	    msn_del_from_llist(auth->inv->conn->invitations_in, auth->inv);
            msn_clean_up(conn);
	    return;
    }
    
#ifndef __MINGW32__
    struct pollfd pfd;
    unsigned char c;
#else
    char c;
#endif

#ifndef __MINGW32__
    pfd.fd=conn->sock;
    pfd.events=POLLIN;

    while(poll(&pfd, 1, 0)!=0)
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(conn->sock,&fds);
    ext_unregister_sock(conn, conn->sock);
    if (DEBUG) printf("unregistered FTP sock\n");
    
    while(select(conn->sock+1,&fds,NULL,NULL,&zerotime))
#endif
    {
      if(read(conn->sock, &c, 1)<1)
      {
	ext_filetrans_failed((invitation_ftp *)auth->inv, 0, "Connection dropped.");
	msn_del_from_llist(auth->inv->conn->invitations_in, auth->inv);
        msn_clean_up(conn);
        return;
      }
      if(auth->num_ignore>0)
      {
	auth->last_done = 0;          
	if(auth->num_ignore == 3 && c == 1) {
		ext_filetrans_failed((invitation_ftp *)auth->inv, 0, "Cancelled by remote user.");
		msn_del_from_llist(auth->inv->conn->invitations_in, auth->inv);
		msn_clean_up(conn);  
		return;
	} else if (auth->num_ignore == 2) {
		auth->must_read=c;
	} else if (auth->num_ignore == 1) {
		auth->must_read+=c*256;
		auth->last_done = auth->bytes_done;
	}
        auth->num_ignore--;
        continue;
      }
      auth->bytes_done++;
      write(auth->fd, &c, 1);
      if(auth->bytes_done==auth->inv->filesize)
      { 
        write(conn->sock, "BYE 16777989\r\n", strlen("BYE 16777989\r\n"));
        ext_filetrans_success(auth->inv);

        msn_del_from_llist(auth->inv->conn->invitations_in, auth->inv);
        msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
        msn_clean_up(conn);
        return;
      }
      if(auth->bytes_done - auth->last_done == auth->must_read) { 
	      auth->num_ignore=3; 
      }
      if(auth->bytes_done%1024==0) {
	char fstatus[1024];
	snprintf(fstatus, 1024, "Receiving %s...", auth->inv->filename);
        ext_filetrans_progress(auth->inv, fstatus, auth->bytes_done, auth->inv->filesize);
	return;
      }
    }
    ext_register_sock(conn, conn->sock, 1, 0);
    if (DEBUG) printf("registered again\n");
  } else {
    // We are sending

    if(!auth->connected) // we have not accept()ed yet, but the read/writability means there's one waiting
    {
      int s;

      if((s=accept(conn->sock, NULL, NULL))<0)
      {
        perror("Could not accept()\n");
        ext_filetrans_failed(auth->inv, errno, strerror(errno));
        msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
        msn_clean_up(conn); // that will nuke both auth and inv
        return;
      }

      ext_unregister_sock(conn, conn->sock);
      close(conn->sock);

      conn->sock=s;
      ext_register_sock(conn, conn->sock, 1, 1);

      ext_filetrans_progress(auth->inv, "Connected", 0, 0);

      auth->connected=1;
    } else {
      // we know it's connected already

      if(auth->fd==-1)
      {
        char ** args;
        int numargs;


        if(!readable) { return; } // not interested...

        if((args=msn_read_line(conn, &numargs))==NULL)
        {
          perror("read() failed");
          ext_filetrans_failed(auth->inv, errno, strerror(errno));
          msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
          msn_clean_up(conn);
          return;
        }

        if(!strcmp(args[0], "VER"))
        {
          snprintf(buf, sizeof(buf), "VER MSNFTP\r\n");
          write(conn->sock, buf, strlen(buf));
          ext_filetrans_progress(auth->inv, "Negotiating", 0, 0);
        }

        if(!strcmp(args[0], "USR"))
        {
          if(numargs >=3 && strcmp(args[2], auth->cookie))  // if they DIFFER
          {
            ext_filetrans_failed(auth->inv, errno, strerror(errno));
            msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
            msn_clean_up(conn);
          } else {
            snprintf(buf, sizeof(buf), "FIL %lu\r\n", auth->inv->filesize);
            write(conn->sock, buf, strlen(buf));
	  }
        }

        if(!strcmp(args[0], "TFR"))
        {
          // you asked for it, go to data-dump mode
          auth->fd=open(auth->inv->filename, O_RDONLY);
          if(auth->fd<0)
          {
            ext_filetrans_failed(auth->inv, 0, "Could not open file for reading.");
            msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
            msn_clean_up(conn);
          } else {

            // OK, now we lose control, but the next round of the polling loop
            // will say that the socket is writable, and then the fun starts...
            ext_filetrans_progress(auth->inv, "Sending data", 0, 0);
	  }
        }
	delete [] args[0];
	delete [] args;
      } else {
	      
        // just pumping data now

#ifndef __MINGW32__
        struct pollfd pfd;
	struct pollfd pin;
#endif
        char c;
#ifndef __MINGW32__
        pfd.fd=conn->sock;
        pfd.events=POLLOUT;
	pin.fd=conn->sock;
	pin.events=POLLIN;
        while(poll(&pfd, 1, 0)!=0)
#else
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(conn->sock,&fds);
        while(select(conn->sock+1,NULL,&fds,NULL,&zerotime))
#endif
        {
          if(auth->bytes_done%2045==0)
          {
#ifdef __MINGW32__
	    char check[3];
#else
            unsigned char check[3];
	    if (poll(&pin, 1, 0)!=0) {
		int numargs;
		char **args =msn_read_line(conn, &numargs);
		/* TODO Check if args is NULL */
		if (args && args[0] && !strcmp(args[0],"CCL")) {
			/* remote cancelled reception */
        		ext_filetrans_failed(auth->inv, 0, "Remote user cancelled transfer.");
        		msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
			msn_clean_up(conn);
		}
		delete [] args[0];
		delete [] args;
	    }
#endif
            int to_go=(auth->inv->filesize-auth->bytes_done>2045)?(2045):(auth->inv->filesize-auth->bytes_done);

            if (!auth->inv->cancelled) {
        	    check[0]='\0';
        	    check[1]=to_go%256;
        	    check[2]=to_go/256;
		    write(conn->sock, check, 3);
	    } else {
		    check[0]=1;
		    check[1]=0;
		    check[2]=0;
		    write(conn->sock, check, 3);
		    msn_filetrans_cancel_clean(auth->inv);
		    return;
	    }
            
          }

          if(read(auth->fd, &c, 1)<1)
          {
            ext_filetrans_failed(auth->inv, errno, strerror(errno));
            msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
            msn_clean_up(conn);
            return;
          }

          auth->bytes_done++;
          write(conn->sock, &c, 1);

          if(auth->bytes_done==auth->inv->filesize)
          {
            /* should send \1\0\0 ? */
	    if(auth->inv) {
	            ext_filetrans_success(auth->inv);
        	    msn_del_from_llist(auth->inv->conn->invitations_in, auth->inv);
        	    msn_del_from_llist(auth->inv->conn->invitations_out, auth->inv);
	    }
            msn_clean_up(conn);
            return;
          }
        }
	char fstatus[1024];
	snprintf(fstatus, 1024, "Sending %s...", auth->inv->filename);
        ext_filetrans_progress(auth->inv, fstatus, auth->bytes_done, auth->inv->filesize);
      }
    }
  }
}

void msn_send_file(invitation_ftp * inv, char * msg_body)
{
  int port=6891;
  msnconn * conn=new msnconn;

  ext_filetrans_progress(inv, "Sending IP address", 0, 0);

  conn->type=CONN_FTP;
  conn->ext_data = inv->conn->ext_data;
  while((conn->sock=ext_server_socket(port))<0)
  {
    port++;
    if(port>6911)
    {
      ext_filetrans_failed(inv, errno, strerror(errno));
      msn_del_from_llist(inv->conn->invitations_out, inv);
      delete inv;
      delete conn;
      return;
    }
  }

  conn->ssock=conn->sock;
  authdata_FTP * auth=new authdata_FTP;

  conn->auth=auth;
  
  auth->cookie=new char[64];
  sprintf(auth->cookie, "%d", rand());
  
  auth->username = msn_permstring(((authdata_SB *)(inv->conn->auth))->username);
  
  auth->inv=inv;
  auth->direction=MSNFTP_SEND;

  auth->connected=0;

  ext_register_sock(conn, conn->sock, 1, 0);

  msn_add_to_llist(msnconnections, conn);

  message * msg=new message;
  msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");


  snprintf(buf, sizeof(buf), "Invitation-Command: ACCEPT\r\n"
		  "Invitation-Cookie: %s\r\n"
		  "IP-Address: %s\r\n"
		  "Port: %d\r\n"
		  "AuthCookie: %s\r\n"
		  "Launch-Application: FALSE\r\n"
		  "Request-Data: IP-Address:\r\n\r\n",
    inv->cookie, ext_get_IP(), port, auth->cookie);

  msg->body=msn_permstring(buf);

  msn_send_IM(inv->conn, NULL, msg);

  delete msg;
}

void msn_handle_NAK(msnconn * conn, char ** args, int numargs)
{
  ext_IM_failed(conn);
}

void msn_handle_JOI(msnconn * conn, char ** args, int numargs)
{
  authdata_SB * auth;

  auth=(authdata_SB *)conn->auth;

  if (numargs < 3) return;
  if(!strcmp(args[1], auth->username)) { return; }

  msn_add_to_llist(conn->users, new char_data(msn_permstring(args[1])));
  ext_user_joined(conn, args[1], msn_decode_URL(args[2]), 0);

  if(auth->msg!=NULL)
  {
    msn_send_IM(conn, NULL, auth->msg);
    delete auth->msg;
    auth->msg=NULL;
  }
}

void msn_handle_RNG(msnconn * conn, char ** args, int numargs)
{
  msnconn * newSBconn=new msnconn;
  authdata_SB * auth=new authdata_SB;

  if (numargs < 5) return;
  newSBconn->type=CONN_SB;
  newSBconn->auth=auth;
  newSBconn->ext_data = conn->ext_data;
  auth->username=msn_permstring(((authdata_NS *)(conn->auth))->username);
  auth->sessionID=msn_permstring(args[1]);
  auth->cookie=msn_permstring(args[4]);
  auth->msg=NULL;

  msn_add_to_llist(msnconnections, newSBconn);

  char * c;
  int port=1863;
  if((c=strstr(args[2], ":"))!=NULL)
  {
    *c='\0';
    c++;
    port=atoi(c);
  }

  msn_connect(newSBconn, args[2], port);
}

void msn_handle_BYE(msnconn * conn, char ** args, int numargs)
{
  llist * list;
  char_data * c;

  list=conn->users;

  if (numargs < 2) return;
  ext_user_left(conn, args[1]);

  while(list!=NULL)
  {
    c=(char_data *)list->data;
    if(!strcmp(c->c, args[1])) // if the departing user matches this item on the list
    {
      if(list->next!=NULL)
      { list->next->prev=list->prev; }
      if(list->prev!=NULL)
      { list->prev->next=list->next; }
      if(list->prev==NULL) { conn->users=list->next; }
      list->next=NULL;  // otherwise the delete will go through the entire llist!
      list->prev=NULL;
      delete list; // will delete the char_data for us too
      break;
    }
    list=list->next;
  }

  if(conn->users==NULL)
  {
    msn_clean_up(conn);
  }
}

void msn_handle_statechange(msnconn * conn, char ** args, int numargs)
{
  char * buddy;
  char * state;
  char * friendlyname;
 
  if(!strcmp(args[0], "ILN"))
  {
    if (numargs < 5) return;
    friendlyname=args[4];
    buddy=args[3];
    state=args[2];
  } else if(!strcmp(args[0], "FLN")) {
    if (numargs < 2) return;
    buddy=args[1];
    ext_buddy_offline(conn, buddy);
    return;
  } else {
    if (numargs < 4) return;    
    friendlyname=args[3];
    buddy=args[2];
    state=args[1];
  }

  ext_buddy_set(conn, buddy, msn_decode_URL(friendlyname), state);
}

void msn_handle_ADD(msnconn * conn, char ** args, int numargs)
{
  if (numargs == 7) return; /* group add */
  if (numargs < 5) return;	
  if(!strcmp(args[2], "RL"))
  {
if(DEBUG)
    printf("Via ADD:\n");
    if (numargs < 6) return;
    ext_new_RL_entry(conn, args[4], msn_decode_URL(args[5]));
  }

  ext_new_list_entry(conn, args[2], args[4]);
  ext_latest_serial(conn, atoi(args[3]));
}

void msn_handle_REM(msnconn * conn, char ** args, int numargs)
{
  if (numargs == 6) return; /* group change */	
  if (numargs < 5) return;
  ext_del_list_entry(conn, args[2], args[4]);
  ext_latest_serial(conn, atoi(args[3]));
}


void msn_handle_BLP(msnconn * conn, char ** args, int numargs)
{
  if (numargs < 4) return;
  ext_got_BLP(conn, args[3][0]);
  ext_latest_serial(conn, atoi(args[3]));
}

void msn_handle_GTC(msnconn * conn, char ** args, int numargs)
{
  if (numargs < 4) return;
  ext_got_GTC(conn, args[3][0]);
  ext_latest_serial(conn, atoi(args[3]));
}

void msn_handle_REA(msnconn * conn, char ** args, int numargs)
{
  if (numargs < 5) return;
  ext_latest_serial(conn, atoi(args[2]));
  ext_got_friendlyname(conn, msn_decode_URL(args[4]));
}

void msn_handle_CHL(msnconn * conn, char ** args, int numargs)
{
  md5_state_t state;
  md5_byte_t digest[16];
  int a;

  if (numargs < 3) return;
  md5_init(&state);
  md5_append(&state, (md5_byte_t *)(args[2]), strlen(args[2]));
  md5_append(&state, (md5_byte_t *)"Q1P7W2E4J9R8U3S5", 16);
  md5_finish(&state, digest);

  snprintf(buf, sizeof(buf), "QRY %d msmsgs@msnmsgr.com 32\r\n", next_trid++);
  write(conn->sock, buf, strlen(buf));

  for(a=0; a<16; a++)
  {
    snprintf(buf, sizeof(buf), "%02x", digest[a]);
    write(conn->sock, buf, strlen(buf));
  }
}

void msn_handle_OUT(msnconn * conn, char ** args, int numargs)
{
  if(numargs>1)
  {
    if(!strcmp(args[1], "OTH"))
    {
      ext_show_error(conn, "You have logged onto MSN twice at once. Your MSN session will now terminate.");
    }
    if(!strcmp(args[1], "SSD"))
    {
      ext_show_error(conn, "This MSN server is going down for maintenance. Your MSN session will now terminate.");
    }
  }
  msn_clean_up(conn);
}

void msn_filetrans_reject(invitation_ftp * inv)
{
   message * msg=new message;

   snprintf(buf, sizeof(buf), "Invitation-Command: CANCEL\r\n"
		   "Invitation-Cookie: %s\r\n"
		   "Cancel-Code: REJECT\r\n",
     inv->cookie);
   msg->body=msn_permstring(buf);
   msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
   msn_send_IM(inv->conn, NULL, msg);
   delete msg;

if(DEBUG)
   printf("Rejecting file transfer\n");

   msn_del_from_llist(inv->conn->invitations_in, inv);
}

void msn_filetrans_accept(invitation_ftp * inv, const char * dest)
{
   message * msg=new message;

   delete inv->filename;
   inv->filename=msn_permstring(dest);
   snprintf(buf, sizeof(buf), "Invitation-Command: ACCEPT\r\n"
		   "Invitation-Cookie: %s\r\n"
		   "Launch-Application: FALSE\r\n"
		   "Request-Data: IP-Address\r\n\r\n",
     inv->cookie);
   msg->body=msn_permstring(buf);
   msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
   msn_send_IM(inv->conn, NULL, msg);
   delete msg;

if(DEBUG)
   printf("Accepting file transfer\n");
}

void msn_netmeeting_reject(invitation_voice * inv)
{
   message * msg=new message;

   snprintf(buf, sizeof(buf), "Invitation-Command: CANCEL\r\n"
		   "Invitation-Cookie: %s\r\n"
		   "Cancel-Code: REJECT\r\n",
     inv->cookie);
   msg->body=msn_permstring(buf);
   msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
   msn_send_IM(inv->conn, NULL, msg);
   delete msg;

if(DEBUG)
   printf("Rejecting netmeeting\n");

   msn_del_from_llist(inv->conn->invitations_in, inv);
}

void msn_netmeeting_accept(invitation_voice * inv)
{
   message * msg=new message;

   if (inv->app == APP_NETMEETING) {
     if(DEBUG) printf("ACCEPTING NETMEETING\n");
     snprintf(buf, sizeof(buf), "Invitation-Command: ACCEPT\r\n"
		   "Invitation-Cookie: %s\r\n"
		   "Launch-Application: TRUE\r\n"
		   "Session-ID: %s\r\n"
		   "Session-Protocol: SM1\r\n"
		   "Request-Data: IP-Address:\r\n"
		   "IP-Address: %s\r\n\r\n",
     inv->cookie,
     inv->sessionid,
     ext_get_IP());
   } else { /* SIP voice ... will be supported one day or another */
     if (DEBUG) printf("ACCEPTING VOICE\n");
     snprintf(buf, sizeof(buf), "Invitation-Command: ACCEPT\r\n"
		   "Invitation-Cookie: %s\r\n"
		   "Launch-Application: FALSE\r\n"
		   "Session-ID: %s\r\n"
		   "Context-Data: Requested:SIP_A,;Capabilities:SIP_A,;\r\n"
		   "Session-Protocol: SM1\r\n"
		   "Request-Data: IP-Address:\r\n"
		   "IP-Address: %s\r\n\r\n",
     inv->cookie,
     inv->sessionid,
     ext_get_IP());
	   
   }
   msg->body=msn_permstring(buf);
   msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
   msn_send_IM(inv->conn, NULL, msg);
   delete msg;

if(DEBUG)
   printf("Accepting netmeeting\n");

}

invitation_ftp * msn_filetrans_send(msnconn * conn, char * path)
{
  struct stat st_info;

  if(stat(path, &st_info)<0)
  { ext_show_error(conn, "Could not open file."); return NULL; }

  invitation_ftp * inv=new invitation_ftp;

  inv->app=APP_FTP;
  inv->cookie=new char[64];
  sprintf(inv->cookie, "%d", rand());
  inv->other_user=NULL;
  inv->conn=conn;

  inv->filename=msn_permstring(path);
  inv->filesize=st_info.st_size;

  message * msg=new message;
  char * basename;

  basename=inv->filename+strlen(inv->filename);

  while(basename>=inv->filename && *basename!='/' && *basename!='\\')
  {
    basename--;
  }

  basename++; // it will always be 1 char before the start of the string

  msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");

  snprintf(buf, sizeof(buf), "Application-Name: File transfer\r\n"
		  "Application-GUID: {5D3E02AB-6190-11d3-BBBB-00C04F795683}\r\n"
		  "Invitation-Command: INVITE\r\n"
		  "Invitation-Cookie: %s\r\n"
		  "Application-File: %s\r\n"
		  "Application-FileSize: %lu\r\n\r\n",
    inv->cookie, basename, inv->filesize);

  msg->body=msn_permstring(buf);

  msn_send_IM(conn, NULL, msg);

  msn_add_to_llist(conn->invitations_out, inv);

  delete msg;

  ext_filetrans_progress(inv, "Negotiating connection", 0, 0);

  return inv;
}

static void msn_connect_cb(int fd, int error, void *data)
{
    msnconn *conn = (msnconn *)data;	
    authdata_SB * auth=(authdata_SB *)conn->auth;
    if(fd == -1 || error)
    {
      ext_show_error(conn, "Could not connect to switchboard server.");
      ext_closing_connection(conn);
      return;
    }
    conn->sock=fd;
    ext_register_sock(conn, conn->sock, 1, 0);

    if(auth->sessionID==NULL)
    {
      snprintf(buf, sizeof(buf), "USR %d %s %s\r\n", next_trid, auth->username, auth->cookie);
      write(conn->sock, buf, strlen(buf));

      msn_add_callback(conn, msn_SBconn_3, next_trid, NULL);
    } else {
      snprintf(buf, sizeof(buf), "ANS %d %s %s %s\r\n", next_trid, auth->username, auth->cookie, auth->sessionID);
      write(conn->sock, buf, strlen(buf));

      ext_new_connection(conn);
      conn->ready=1;
      msn_add_callback(conn, msn_SB_ans, next_trid, NULL);
    }

    next_trid++;
}

static void msn_connect_cb2(int fd, int error, void *data)
{
  connectinfo * info;
  msnconn *conn = (msnconn *)data;	

  /* FIXME: This memory is leaked - Andy */
  info=new connectinfo;

  // The following is necessary as username and password may be temp variables
  // that will no longer exist when the next function is called
  info->username=msn_permstring( ((authdata_NS *)conn->auth)->username);
  info->password=msn_permstring( ((authdata_NS *)conn->auth)->password);

  if(fd==-1 || error)
  {
    ext_show_error(conn, "Could not connect to MSN server.");
    ext_closing_connection(conn);
    return;
  }
  conn->sock = fd;
  
  ext_register_sock(conn, conn->sock, 1, 0);

  if(DEBUG)
  printf("Connected\n"); // DEBUG
  

  snprintf(buf, sizeof(buf), "VER %d MSNP8 CVR0\r\n", next_trid);
  write(conn->sock, buf, strlen(buf));
  msn_add_callback(conn, msn_connect_2, next_trid, (callback_data *)info);
  next_trid++;
}

void msn_connect(msnconn * conn, char * server, int port)
{
  conn->ready=0;

  if(conn->type==CONN_SB)
  {
    if (ext_async_socket(server, port, (void *)msn_connect_cb, conn) < 0) {
	if(DEBUG) printf("immediate connect failure\n");    
	ext_show_error(conn, "Could not connect to MSN SB server.");
	ext_closing_connection(conn);
    }
    return;
  }
  conn->ready = 0;
  if (ext_async_socket(server, port, (void *)msn_connect_cb2, conn) < 0) {
	  if(DEBUG) printf("immediate connect2 failure\n"); 
	  ext_show_error(conn, "Could not connect to MSN server.");   
	  ext_closing_connection(conn);
  }
  
}

// Further connection functions:

void msn_connect_2(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  connectinfo * info;

  info=(connectinfo *)data;
  msn_del_callback(conn, trid);

  if (numargs < 3) return;
  if(strcmp(args[0], "VER") || strcmp(args[2], "MSNP8")) // if either *differs*...
  {
    ext_show_error(NULL, "MSN Protocol negotiation failed.");
    delete info;
    ext_unregister_sock(conn, conn->sock);
    close(conn->sock);
    conn->sock=-1;
    return;
  }

  snprintf(buf, sizeof(buf), "CVR %d 0x0409 winnt 5.2 i386 MSNMSGR 6.0.0250 MSMSGS %s\r\n", next_trid, info->username);
  write(conn->sock, buf, strlen(buf));

  msn_add_callback(conn, msn_connect_3, next_trid, data);
  next_trid++;
}

void msn_connect_3(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  connectinfo * info;

  md5_state_t state;
  md5_byte_t digest[16];
  int a;

  info=(connectinfo *)data;
  msn_del_callback(conn, trid);

  if (numargs < 5) return;
  if(isdigit(args[0][0]))
  {
    msn_show_verbose_error(conn, atoi(args[0]));
    msn_clean_up(conn);
    delete info;
    return;
  }

  snprintf(buf, sizeof(buf), "USR %d TWN I %s\r\n", next_trid, info->username);
  
  write(conn->sock, buf, strlen(buf));
  msn_add_callback(conn, msn_SBconn_2, next_trid, data);

  next_trid++;
}

void msn_connect_4(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  connectinfo * info;

  info=(connectinfo *)data;
  msn_del_callback(conn, trid);
  
  if(isdigit(args[0][0]))
  {
    msn_show_verbose_error(conn, atoi(args[0]));
    delete info;
    msn_clean_up(conn);
    return;
  }

  if (numargs < 5) return;
  ext_got_friendlyname(conn, msn_decode_URL(args[4]));
  
  delete info;

  next_trid++;

  conn->ready=1;
  ext_new_connection(conn);
}

void msn_SB_ans(msnconn * conn, int trid, char ** args, int numargs, callback_data * data)
{
  if (numargs < 3) return;
  if(!strcmp(args[0], "ANS") && !strcmp(args[2], "OK"))
  { return; }

  if(isdigit(args[0][0]))
  {
    msn_del_callback(conn, trid);
    msn_show_verbose_error(conn, atoi(args[0]));
    msn_clean_up(conn);
    return;
  }

  if(!strcmp(args[0], "IRO"))
  {
    if (numargs < 6) return;
    if(!strcmp(args[4], ((authdata_SB *)conn->auth)->username)) { return; }
    msn_add_to_llist(conn->users, new char_data(msn_permstring(args[4])));
    ext_user_joined(conn, args[4], msn_decode_URL(args[5]), 1);
    if(!strcmp(args[2], args[3]))
    {
      msn_del_callback(conn, trid);
    }
  }
}

void msn_set_state(msnconn * conn, char * state)
{
  snprintf(buf, sizeof(buf), "CHG %d %s\r\n", next_trid, state);
  write(conn->sock, buf, strlen(buf));
  next_trid++;
  
  if (conn->status != NULL) delete conn->status;
  conn->status = msn_permstring(state);
}

void msn_add_group(msnconn *conn, char *newgroup) {
	if (newgroup == NULL) {
		if(DEBUG)
		printf("Groupname is null !\n");

		return;
	}

	snprintf(buf, sizeof(buf), "ADG %d %s 0\r\n", next_trid,  msn_encode_URL(newgroup));
	write(conn->sock, buf, strlen(buf));
	next_trid++;
}

void msn_change_group(msnconn *conn, char *handle, char *oldgroup, char *newgroup) {
	if (newgroup == NULL) {
if(DEBUG)
		printf("Group doesn't exist !\n");

		return;
	}
	snprintf(buf, sizeof(buf), "ADD %d FL %s %s %s\r\n", next_trid, handle, handle, newgroup);
	write(conn->sock, buf, strlen(buf));
	next_trid++;
	if (oldgroup != NULL) {
		snprintf(buf, sizeof(buf), "REM %d FL %s %s\r\n", next_trid, handle, oldgroup);
		write(conn->sock, buf, strlen(buf));
		next_trid++;
	}
}

void msn_del_group(msnconn *conn, char *group_id) {
	if (group_id == NULL) {
		if(DEBUG)
			printf("Group_id is null !\n");
		return;
	}

	snprintf(buf, sizeof(buf), "RMG %d %s\r\n", next_trid,  group_id);
	write(conn->sock, buf, strlen(buf));
	next_trid++;
	if (DEBUG)
		printf("deleted group id %s\n",group_id);
}

void msn_rename_group(msnconn *conn, char *id, char *newname) {
	if (newname == NULL || id == NULL) {
if(DEBUG)
		printf("Groupname or ID is null !\n");

		return;
	}

	snprintf(buf, sizeof(buf), "REG %d %s %s 0\r\n", next_trid, id, msn_encode_URL(newname));
	write(conn->sock, buf, strlen(buf));
	next_trid++;
}

invitation_voice * msn_invite_netmeeting(msnconn *conn)
{
  invitation_voice * inv=new invitation_voice;
  inv->app=APP_NETMEETING;
  inv->cookie=new char[64];
  inv->sessionid = new char[42];
  sprintf(inv->cookie, "%d", rand());
  inv->conn=conn;
  snprintf(inv->sessionid,42,"{%08X-%04X-%04X-%04X-%012X}",
			rand(),(unsigned short)(rand()),
		  	(unsigned short)(rand()),
		  	(unsigned short)(rand()),
		        rand());

  message * msg = new message;
  msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
  
  snprintf(buf, sizeof(buf), "Application-Name: NetMeeting\r\n"
		  "Application-GUID: {44BBA842-CC51-11CF-AAFA-00AA00B6015C}\r\n"
		  "Session-Protocol: SM1\r\n"
		  "Invitation-Command: INVITE\r\n"
		  "Invitation-Cookie: %s\r\n"
		  "Session-ID: %s\r\n\r\n",
		  inv->cookie,
		  inv->sessionid);

  msg->body=msn_permstring(buf);

  msn_send_IM(conn, NULL, msg);

  msn_add_to_llist(conn->invitations_out, inv);

  if(DEBUG) printf("sent invitation!\n");
  delete msg;
  return inv;

}

static void msn_finish_netmeeting_inv(msnconn *conn, invitation_voice *inv, char *ip) 
{
  message * msg = new message;
  msg->content=msn_permstring("text/x-msmsgsinvite; charset=UTF-8");
  
  snprintf(buf, sizeof(buf), "Invitation-Command: ACCEPT\r\n"
		  "Invitation-Cookie: %s\r\n"
		  "Session-ID: %s\r\n"
		  "Launch-Application: TRUE\r\n"
		  "IP-Address: %s\r\n\r\n",
		  inv->cookie,
		  inv->sessionid,
		  ext_get_IP());
  
  msg->body=msn_permstring(buf);

  msn_send_IM(conn, NULL, msg);
  ext_start_netmeeting(NULL);
  delete msg;
}
