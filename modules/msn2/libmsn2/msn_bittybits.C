/* msn_bittybits.c - all the little string- and list-bashing functions */
/* WARNING - Ayttm's libmsn2 differs from the original one and Meredydd,
   its original author, does not provide support for it :) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifndef __MINGW32__
#include <sys/poll.h>
#endif

#include "msn_core.h"
#include "msn_interface.h"
#include "msn_bittybits.h"
#include <errno.h>
#include <sys/time.h>

char ** msn_read_line(msnconn *conn, int * numargs)
{
  // Right, this is quite a task. Step One is to read the thing in.
  char ** retval;
  char c;
  int sock = conn->sock;
  int finished = 0;
  fd_set inp;
  struct timeval tv;
  
  FD_ZERO(&inp);
  FD_SET(sock, &inp);

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  while(select(sock+1, &inp, NULL, NULL, &tv)>0)
  {
    	int result=0;
	if(FD_ISSET(sock, &inp)) {
		if((result=read(sock, &c, 1))<1) {
			if (result == 0) {
				*numargs=-1;
				return NULL;
			}
			else if (errno == EAGAIN) {
				goto continued;
			} else if (errno) {
				/* may have been unregistered */
				if (!ext_is_sock_registered(conn, sock)) {
					*numargs=0;
					return NULL;
				}
				
				printf("error %d %s\n",errno, strerror(errno));
			        printf("What the.. (%d) (%s)?!\n", sock, conn->readbuf); //DEBUG
				*numargs=-1;
				return NULL;      
			} else if(conn->type==CONN_FTP) {
				conn->numspaces++; conn->readbuf[conn->pos]='\0'; finished=1; break;
			}
		}
		if(conn->pos == 1249) {conn->readbuf[conn->pos]='\0'; goto continued; }
		if(c=='\r' || conn->pos > 1249) { goto continued; }
		if(c=='\n') { conn->numspaces++; conn->readbuf[conn->pos]='\0'; finished=1; break; }
		if(c==' ') { conn->numspaces++; }
		conn->readbuf[conn->pos]=c;
		conn->pos++;
	} else {
		break;
	}
continued:
  	FD_ZERO(&inp);
	FD_SET(sock, &inp);
  }
  if (!finished) {
    *numargs=0;
    return NULL;
  }
  
  if(conn->numspaces==0) { printf("What the..?\n"); *numargs=-1; return NULL; }

  int len = strlen(conn->readbuf);

  retval=new char * [conn->numspaces];
  retval[0]=new char[len+1];
  strcpy(retval[0], conn->readbuf); /* long enough */
  *numargs=conn->numspaces;

  // OK, take it as read (boom, boom!)
  // Now we cruise through the string, changing all spaces to null 0's and setting
  // a pointer at the beginning of each substring

  conn->pos=0;
  conn->numspaces=1; // pointer #0 is already set at the beginning
  while(conn->pos <= len)
  {
	if(retval[0][conn->pos]==' ')
	{
		retval[0][conn->pos]='\0';
		retval[conn->numspaces]=retval[0]+conn->pos+1;
		conn->numspaces++;
	} else if(retval[0][conn->pos]=='\0') {
		break;
	}

	conn->pos++;
  }

  conn->pos = conn->numspaces = 0;
  memset(conn->readbuf,0, sizeof(conn->readbuf));

  return retval;
}

void msn_clean_up(msnconn * conn)
{
  llist * connlist;
  connlist=msnconnections;

  if(conn->type!=CONN_FTP)
  { ext_closing_connection(conn); }

  while(1)
  {
    if(connlist==NULL) { return; }
    if(connlist->data==conn) { break; }
    connlist=connlist->next;
  }
  if (conn->callbacks != NULL) {
	  delete conn->callbacks; // delete the callback data
	  conn->callbacks=NULL;
  }

  close(conn->sock);
  ext_unregister_sock(conn, conn->sock);
  delete conn;

  if(connlist->next!=NULL)
  { connlist->next->prev=connlist->prev; }
  if(connlist->prev!=NULL)
  { connlist->prev->next=connlist->next; } else { msnconnections=connlist->next; }
  connlist->prev=NULL; // no recursive destructors, please...
  connlist->next=NULL;
  connlist->data=NULL; // already deleted the conn object
  delete connlist;
}

void msn_add_callback(msnconn * conn, void (*func)(msnconn * conn, int trid, char ** args, int numargs, callback_data * data), int trid, callback_data * data)
{
  callback * call;

  call=new callback;
  call->trid=trid;
  call->data=data;
  call->func=func;

  msn_add_to_llist(conn->callbacks, call);
}

void msn_del_callback(msnconn * conn, int trid)
{
  llist * list;
  callback * call;

  list=conn->callbacks;

  if(list==NULL) { return; }

  do
  {
    call=(callback *)list->data;
    if(call->trid==trid)
    {
      if(list->next!=NULL)
	      { list->next->prev=list->prev; }
      if(list->prev!=NULL)
	      { list->prev->next=list->next; } 
      else 
	      { conn->callbacks=NULL; }
      list->prev=NULL; // no recursive destructors
      list->next=NULL;
      delete list;
      break;
    }
    list=list->next;
  } while(list!=NULL);
}

void msn_add_to_llist(llist *& listp, llist_data * data)
{
  llist * tlist;
  llist * newlist;

  if(listp==NULL) { listp=new llist; listp->data=data; return; }

  tlist=listp;

  while(tlist->next!=NULL) { tlist=tlist->next; }

  newlist=new llist;
  newlist->prev=tlist;
  newlist->next=NULL;
  newlist->data=data;
  tlist->next=newlist;
}

void msn_del_from_llist(llist *& listp, llist_data * data)
{
  // Note: this function does NOT delete the data, only the list object
  llist * tlist;

  tlist=listp;

  while(tlist!=NULL)
  {
    if(tlist->data==data)
    {
      if(tlist->next!=NULL)
      { tlist->next->prev=tlist->prev; }
      if(tlist->prev!=NULL)
      { tlist->prev->next=tlist->next; } else { listp=tlist->next; }
      tlist->next=NULL; // otherwise the whole list is clobbered by the destructor
      tlist->prev=NULL;
      tlist->data=NULL;
      delete tlist;
      return;
    }
    tlist=tlist->next;
  }
}

int msn_count_llist(llist * list)
{
  llist * l=list;
  int retval=0;

  while(l!=NULL) { l=l->next; retval++; }

  return retval;
}

char * msn_permstring(char * s)
{
  char * retval = NULL;

  if(s==NULL) { return NULL; }

  retval=new char[strlen(s)+1];
  strcpy(retval, s);
  return retval;
}

char * msn_decode_URL(char * s)
{
  char * rpos; // read
  char * wpos; // write

  wpos=rpos=s;

  while(1)
  {
    if(*rpos=='\0') { *wpos='\0'; break; }

    if(*rpos=='%')
    {
      char buf[3];
      int c;
      rpos++;
      buf[0]=*rpos;
      rpos++;
      buf[1]=*rpos;
      rpos++;
      buf[2]='\0';
      sscanf(buf, "%x", &c);
      *wpos=c;
      wpos++;
      continue;
    }

    *wpos=*rpos;
    rpos++;
    wpos++;
  }
  return s;
}

char * msn_encode_URL(char * s)
{
  char * rptr;
  char * wptr;
  char * retval;

  wptr=retval=new char[strlen(s)*3];
  rptr=s;

  while(1)
  {
    if(*rptr=='\0')
    { *wptr='\0'; break; }
    if(!(isalpha(*rptr) || isdigit(*rptr)))
    {
      sprintf(wptr, "%%%2x", (int)(*rptr)); /* this one is ok */

      rptr++;
      wptr+=3;
      continue;
    }

    *wptr=*rptr;
    wptr++;
    rptr++;
  }

  return retval;
}

