/* msn_interface.c - functions that talk to the outside world */

#include <stdio.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "msn_core.h"
#include "msn_interface.h"
#include "msn_bittybits.h"
#include "ssl.h"
struct pollfd socks[21];

int countsocks(void);
int do_msn_debug = 1;

msnconn * mainconn;

void ext_syncing_lists(msnconn *, int){}
void ext_got_friend(msnconn *, char *, char *){}
void ext_got_pong(msnconn *){}
void ext_got_group(msnconn *, char *, char *){}
void ext_netmeeting_invite(msnconn *, char *, char *, invitation_voice *){}

main()
{
  for(int a=0; a<20; a++)
  { socks[a].fd=-1; socks[a].events=POLLIN; socks[a].revents=0; }
  socks[0].fd=0;
  socks[0].events=POLLIN;
  socks[0].revents=0;

  fprintf(stderr, "Enter your login name: ");
  fflush(stdout);
  char uname[512];
  char pass[512];
  fgets(uname, 512, stdin);
  uname[strlen(uname)-1]='\0';

  fprintf(stderr, "Enter your password: ");
  fgets(pass, 512, stdin);
  pass[strlen(pass)-1]='\0';

  fprintf(stderr, "Now connecting to the MSN Messenger service, hold onto your hats...\n");

  mainconn=new msnconn;
  msn_init(mainconn, uname, pass);
  msn_connect(mainconn, "messenger.hotmail.com", 1863);// 64.4.13.56

  fprintf(stderr, "> ");
  fflush(stderr);
  while(1)
  {
    poll(socks, 20, -1);
    for(int a=1; a<20; a++)
    {
      if(socks[a].fd==-1) { break; }
      if(socks[a].revents & POLLHUP) { socks[a].revents=0; continue; }
      if(socks[a].revents & (POLLERR|POLLNVAL))
      {
        printf("Dud socket (%d)! Code %x (ERR=%x, INVAL=%x)\n", socks[a].fd, socks[a].revents, POLLERR, POLLNVAL);
        msn_handle_close(socks[a].fd);
        socks[a].fd=-1;
        socks[a].revents=0;
        continue;
      }
      if(socks[a].revents & (POLLIN|POLLOUT|POLLPRI))
      {
        //printf("Incoming on socket number %d (%d)\n", a, socks[a].fd);
	char **args;
	int nargs;
	args = msn_read_line(mainconn, &nargs);
        msn_handle_incoming(mainconn, socks[a].revents&POLLIN, socks[a].revents&POLLOUT,
		args, nargs);
      }
      socks[a].revents=0;
    }
    if(socks[0].revents & POLLIN)
    {
      handle_command();
      socks[0].revents=0;
    }
  }
}

int ext_is_sock_registered(msnconn *conn, int s)
{
	return 1;
}

void handle_command(void)
{
  char command[40];

  scanf(" %s", command);

  if(!strcmp(command, "quit"))
  {
    exit(0);
  } else if(!strcmp(command, "msg")) {
    char rcpt[80];
    char msg[1024];

    scanf(" %s", rcpt);

    fgets(msg, 1024, stdin);

    msg[strlen(msg)-1]='\0';

    msn_send_IM(mainconn, rcpt, msg);
  } else if(!strcmp(command, "status")) {
    char state[10];

    scanf(" %s", state);

    msn_set_state(mainconn, state);
  } else if(!strcmp(command, "friendlyname")) {
    char fn[256];

    fgets(fn, 256, stdin);
    fn[strlen(fn)-1]='\0';

    msn_set_friendlyname(mainconn, fn);
  } else if(!strcmp(command, "add")) {
    char list[10];
    char user[128];

    scanf(" %s %s", list, user);

    msn_add_to_list(mainconn, list, user);
  } else if(!strcmp(command, "del")) {
    char list[10];
    char user[128];

    scanf(" %s %s", list, user);

    msn_del_from_list(mainconn, list, user);
  } else {
    fprintf(stderr, "\nBad command \"%s\"", command);
  }

  fprintf(stderr, "\n> ");
  fflush(stderr);
}

int countsocks(void) // at last!  parental nagging has a result --My father
{
  int retval=0;

  for(int a=0; a<20; a++)
  {
    if(socks[a].fd==-1) { break; }
    retval++;
  }
  return retval;
}

void ext_register_sock(msnconn *conn, int s, int reading, int writing)
{
  for(int a=0; a<20; a++)
  {
    if(socks[a].fd==-1)
    {
      socks[a].events=0;
      if(reading)
      { socks[a].events|=POLLIN;  }
      if(writing)
      { socks[a].events|=POLLOUT;  }
      socks[a].fd=s;
      printf("Added socket %d\n", a);
      return;
    }
  }
}

void ext_unregister_sock(msnconn *conn, int s)
{
  for(int a=0; a<20; a++)
  {
    if(socks[a].fd==s)
    {
      for(int b=a; b<19; b++)
      {
        socks[b].fd=socks[b+1].fd;
      }
      socks[19].fd=-1;
    }
  }
}

void ext_got_friendlyname(msnconn * conn, char * friendlyname)
{
  printf("Your friendlyname is now: %s\n", friendlyname);
}

void ext_got_info(msnconn * conn, syncinfo * info)
{
  int a, b;
  llist * list;

  printf("Got the sync info!\n");

  for(a=0; a<4; a++)
  {
    switch(a)
    {
      case(0) : { list=info->fl; printf("Forward list:\n"); break; }
      case(1) : { list=info->rl; printf("Reverse list:\n"); break; }
      case(2) : { list=info->al; printf("Allow list:\n"); break; }
      case(3) : { list=info->bl; printf("Block list:\n"); break; }
    }

    while(list!=NULL)
    {
      userdata * ud;
      ud=(userdata *)list->data;
      printf("-  %s (%s)\n", ud->friendlyname, ud->username);
      list=list->next;
    }

  }
}

void ext_latest_serial(msnconn * conn, int serial)
{
  printf("The latest serial number is: %d\n", serial);
}

void ext_got_GTC(msnconn * conn, char c)
{
  printf("Your GTC value is now %c\n", c);
}

void ext_got_BLP(msnconn * conn, char c)
{
  printf("Your BLP value is now %cL\n", c);
}

void ext_new_RL_entry(msnconn * conn, char * username, char * friendlyname)
{
  printf("%s (%s) has added you to their contact list.\nYou might want to add them to your Allow or Block list\n", username, friendlyname);
}

void ext_new_list_entry(msnconn * conn, char * list, char * username)
{
  printf("%s is now on your %s\n", username, list);
}

void ext_del_list_entry(msnconn * conn, char * list, char * username)
{
  printf("%s has been removed from your %s\n", username, list);
}

void ext_show_error(msnconn * conn, char * msg)
{
  printf("MSN: Error: %s\n", msg);
}

void ext_buddy_set(msnconn * conn, char * buddy, char * friendlyname, char * status)
{
  printf("%s (%s) is now %s\n", friendlyname, buddy, status);
}

void ext_buddy_offline(msnconn * conn, char * buddy)
{
  printf("%s is now offline\n", buddy);
}

void ext_got_SB(msnconn * conn, void * tag)
{
  printf("Got switchboard connection\n");
}

void ext_user_joined(msnconn * conn, char * username, char * friendlyname, int is_initial)
{
  printf("%s (%s) is now in the session\n", friendlyname, username);
}

void ext_user_left(msnconn * conn, char * username)
{
  printf("%s has now left the session\n", username);
}

void ext_got_IM(msnconn * conn, char * username, char * friendlyname, message * msg)
{
  printf("--- Message from %s (%s):\n%s\n", friendlyname, username, msg->body);
}

void ext_IM_failed(msnconn * conn)
{
  printf("**************************************************\n");
  printf("ERROR:  Your last message failed to send correctly\n");
  printf("**************************************************\n");
}

void ext_typing_user(msnconn * conn, char * username, char * friendlyname)
{
  printf("\t%s (%s) is typing...\n", friendlyname, username);
}

void ext_initial_email(msnconn * conn, int unread_inbox, int unread_folders)
{
  if(unread_inbox>0) { printf("You have %d new messages in your Inbox\n", unread_inbox); }
  if(unread_folders>0) { printf("You have %d new messages in other folders.\n", unread_folders); }
}

void ext_new_mail_arrived(msnconn * conn, char * from, char * subject)
{
  printf("New e-mail has arrived from %s.\nSubject: %s\n", from, subject);
}

void ext_filetrans_invite(msnconn * conn, char * username, char * friendlyname, invitation_ftp * inv)
{
  printf("Got file transfer invitation from %s (%s)\n", friendlyname, username);
  msn_filetrans_accept(inv, "tmp.out");
}

void ext_neetmeeting_invite(msnconn * conn, char * username, char * friendlyname, invitation_voice * inv)
{
  printf("Got netmeeting invitation from %s (%s)\n", friendlyname, username);
  msn_netmeeting_reject(inv);
}

void ext_start_netmeeting(char *ip)
{
	printf("run `gnomemeeting -c callto://%s`\n",ip);
}

void ext_filetrans_progress(invitation_ftp * inv, char * status, unsigned long sent, unsigned long total)
{
  printf("File transfer: %s\t(%lu/%lu bytes sent)\n", status, sent, total);
}

void ext_filetrans_failed(invitation_ftp * inv, int error, char * message)
{
  printf("File transfer failed: %s\n", message);
}

void ext_filetrans_success(invitation_ftp * inv)
{
  printf("File transfer successfully completed\n");
}

void ext_new_connection(msnconn * conn)
{
  if(conn->type==CONN_NS)
  {
    msn_sync_lists(conn, 0);
  }
}

void ext_closing_connection(msnconn * conn)
{
  printf("Closed connection with socket %d\n", conn->sock);
}

void ext_changed_state(msnconn * conn, char * state)
{
  printf("Your state is now: %s\n", state);
}

int ext_connect_socket(char * hostname, int port)
{
  struct sockaddr_in sa;
  struct hostent     *hp;
  int a, s;

  if ((hp= gethostbyname(hostname)) == NULL) { /* do we know the host's */
    errno= ECONNREFUSED;                       /* address? */
    return(-1);                                /* no */
  }

  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr,hp->h_addr,hp->h_length);     /* set address */
  sa.sin_family= hp->h_addrtype;
  sa.sin_port= htons((u_short)port);

  if ((s= socket(hp->h_addrtype,SOCK_STREAM,0)) < 0)     /* get socket */
    return(-1);
  if (connect(s,(struct sockaddr *)&sa,sizeof sa) < 0) { /* connect */
    close(s);
    return(-1);
  }
  sleep(2);
  return(s);
}

int ext_async_socket(char *host, int port, void *cb, void *conn)
{
	/* not implemented */
}

int ext_server_socket(int port)
{
  int s;
  struct sockaddr_in addr;

  if((s=socket(AF_INET, SOCK_STREAM, 0))<0)
  {
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_port=htons(port);

  if(bind(s, (sockaddr *)(&addr), sizeof(addr))<0 || listen(s, 1)<0)
  {
    close(s);
    return -1;
  }

  return s;
}

void ext_disable_conncheck(void){}

char * ext_get_IP(void)
{
  struct hostent * hn;
  char buf2[1024];

  gethostname(buf2,1024);
  hn = gethostbyname(buf2);

  return inet_ntoa( *((struct in_addr*)hn->h_addr));
}

void ext_got_group(char *id, char *name) 
{
	printf("got group id %s, %s\n",id,name);
}
void ext_got_friend(char *name, char *groups) 
{
	printf("got friend %s, %s\n",name,groups);
}
void ext_syncing_lists(int state)
{
}

