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

/****************************
  Message Parser
  ***************************/

#include "intl.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#endif

#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>

#include "chat_window.h"
#include "message_parse.h"
#include "util.h"
#include "dialog.h"
#include "globals.h"
#include "service.h"
#include "plugin_api.h"
#include "activity_bar.h"

#ifdef __MINGW32__
#define snprintf _snprintf
#endif


char filename[1024];
// static unsigned long filename_len = 0; // unused
static int xfer_in_progress = 0;
static FILE * fp;
static unsigned long amount_received;
static int fd;
#ifndef __MINGW32__
static pthread_mutex_t mutex;
#endif

typedef struct _send_file_struct
{
	char filename[1024];
	int s;
} send_file_struct;

typedef struct {
	int timer;
	int input;
	int tag;
} progress_callback_data;


static void send_file2(void * ptr )
{
#ifndef __MINGW32__
	send_file_struct * sfs = ptr;
	unsigned long i = 0;
	char buff[1025];


	pthread_mutex_lock(&mutex);
	xfer_in_progress = 1;
	pthread_mutex_unlock(&mutex);
	signal(SIGPIPE,SIG_IGN);



	if(!fp)
	{
		close(sfs->s);
		pthread_mutex_lock(&mutex);
		xfer_in_progress = -2;
		pthread_mutex_unlock(&mutex);
		signal(SIGPIPE,SIG_DFL);
		pthread_mutex_destroy(&mutex);
		pthread_exit(0);
	}
	i = 0;
	while(!feof(fp))
	{
		buff[i%1024] = fgetc(fp);
		buff[i%1024+1] = 0;

		if(++i % 1024 == 0 )
		{
				int j = write(sfs->s, buff,1024);
				if( j < 0 )
				{
					signal(SIGPIPE,SIG_DFL);
					fclose(fp);
					close(sfs->s);
					pthread_mutex_lock(&mutex);
					xfer_in_progress = -1;
					pthread_mutex_unlock(&mutex);
					pthread_mutex_destroy(&mutex);
					pthread_exit(0);
				}
				while(j < 1024 )
				{
					int k = send(sfs->s, j+buff, 1024-j,0);
					if(k < 0 )
					{
						signal(SIGPIPE,SIG_DFL);
						pthread_mutex_lock(&mutex);
						xfer_in_progress = -1;
						pthread_mutex_unlock(&mutex);
						fclose(fp);
						close(sfs->s);
						pthread_mutex_destroy(&mutex);
						pthread_exit(0);
					}

					j += k;
				}
				pthread_mutex_lock(&mutex);
				amount_received = i;
				pthread_mutex_unlock(&mutex);
		}
	}

	if( (i-1)%1024 != 0 )
	{
		write(sfs->s, buff, (i-1)%1024);
	}

	signal(SIGPIPE,SIG_DFL);

	pthread_mutex_lock(&mutex);
	xfer_in_progress = 0;
	pthread_mutex_unlock(&mutex);
	fclose(fp);
	close(sfs->s);
	pthread_mutex_destroy(&mutex);
	pthread_exit(0);
#endif
}
		
static int update_send_progress(void * data )
{
	progress_callback_data * pcd = data;
#ifndef __MINGW32__
	pthread_mutex_lock(&mutex);
#endif
	if( xfer_in_progress > 0 )
	{
		ay_progress_bar_update_progress(pcd->tag, amount_received);
	}
	else if( xfer_in_progress == -1 )
	{
		do_error_dialog(_("Remote Side Disconnected"), _("Ayttm file x-fer"));
		ay_activity_bar_remove(pcd->tag);
		eb_timeout_remove(pcd->timer);
		free(pcd);
	}
	else if( xfer_in_progress == -2 )
	{
		do_error_dialog(_("Unable to open file!"), _("Ayttm file x-fer"));
		ay_activity_bar_remove(pcd->tag);
		eb_timeout_remove(pcd->timer);
		free(pcd);
	}
	else
	{
		do_error_dialog(_("File Sent Successfully"), _("Ayttm file x-fer"));
		ay_activity_bar_remove(pcd->tag);
		eb_timeout_remove(pcd->timer);
		free(pcd);
	}
#ifndef __MINGW32__
	pthread_mutex_unlock(&mutex);
#endif
	return TRUE;
}

static void send_file( char * filename, int s )
{
	static send_file_struct sfs;
	struct stat fileinfo;
#ifndef __MINGW32__
	static pthread_t thread;
#endif
	int i;
	char buff[6];
	unsigned long filelen;
//	struct timeval tv;
	char accept[10] = "";
//	fd_set set;

	if(xfer_in_progress)
		return;


	strncpy(sfs.filename, filename,1024);
	sfs.s = s;
	stat( sfs.filename, &fileinfo);

	for( i = strlen(filename); i >=0; i-- )
	{
		if(filename[i]=='/')
		{
			break;
		}
	}
	snprintf(buff, 1025, "%05d", strlen(filename+i+1));
	write(s,buff,5);
	write(s,filename+i+1,strlen(filename+i+1));
	filelen = htonl(fileinfo.st_size);
	write(s,&filelen,4);

	/*
	FD_ZERO(&set);
	FD_SET(s, &set);

	tv.tv_sec = 0;
	tv.tv_usec = 20;

	while(!select( s+1, &set, NULL, NULL, &tv ) )
	{
		 while (gtk_events_pending())
				gtk_main_iteration();
	}
*/
	read( s, accept, 10);

	if(!strcmp(accept,"ACCEPT") )
	{
		progress_callback_data * pcd = calloc(1, sizeof(progress_callback_data));
		char label[1024];
		xfer_in_progress = 1;
		fp = fopen(filename,"rb");
		printf("%s %s %d %5d %p\n", filename, filename+i+1, strlen(filename), htons(strlen(filename+i+1)), fp);
		snprintf(label,1024,"Transferring %s...", filename);
		pcd->tag = ay_progress_bar_add(label,fileinfo.st_size,NULL,NULL);
#ifndef __MINGW32__
		pthread_mutex_init(&mutex, NULL);
		if(pthread_create(&thread, NULL, 
					(void*)&send_file2, (void*)&sfs ))
			exit(1);
#else
		send_file2(&sfs);
#endif
		pcd->timer = eb_timeout_add(500, update_send_progress, pcd);
	}
	else
	{
		do_error_dialog(_("Remote Side has aborted the file transfer"),
						_("Ayttm file x-fer"));
	}

}



static void get_file2( void *data, int source, eb_input_condition condition )
{
	char buffer[1025];
	int len2;
	progress_callback_data *pcd = data;

	if(!(len2 = recv(source, buffer, 1024, 0)))
	{
		fclose(fp);
		close(source);
		do_error_dialog(_("File Receive Complete"), _("Ayttm File x-fer"));
		ay_activity_bar_remove(pcd->tag);

		xfer_in_progress = 0;
		eb_input_remove(pcd->input);
		free(pcd);
	}
	else
	{
		int i;
		for(i=0; i <len2; i++)
		{
			fputc(buffer[i], fp);
		}
		amount_received += len2;
		ay_progress_bar_update_progress(pcd->tag, amount_received);
	}
}

	
static void accept_file( GtkWidget * widget, gpointer data )
{
	int result = (int)gtk_object_get_user_data( GTK_OBJECT(widget));
	progress_callback_data * pcd = data;
	if(result)
	{
		char val[10] = "ACCEPT";
		printf("write: %d\n", write(fd, val, 10));
#ifndef __MINGW32__
		fsync(fd);
#endif
		pcd->input = eb_input_add(fd, EB_INPUT_READ, get_file2, pcd); 
	}
	else
	{
		char val[10] = "DENY";
		write(fd, val, 10);
		close(fd);
		fclose(fp);
		xfer_in_progress = 0;
		ay_activity_bar_remove(pcd->tag);
		free(pcd);
	}
}

static void get_file( int s )
{
	int len;
	unsigned long filelen;
	struct timeval tv;
	char buffer2[1024];
	char buffer[1024];
	char buffer3[1024];
	progress_callback_data *pcd = calloc(1, sizeof(progress_callback_data));
	fd_set set;

	fd = accept(s, NULL, NULL );
	close(s);
	if(xfer_in_progress)
		return;
	xfer_in_progress = 1;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	tv.tv_sec = 0;
	tv.tv_usec = 20;

	while(!select( fd+1, &set, NULL, NULL, &tv ) )
	{
		 while (gtk_events_pending())
				gtk_main_iteration();
	}

	recv(fd, buffer, 5, 0);
	buffer[5] = 0;
	len = atoi(buffer);
	recv(fd, buffer2, len, 0);
	buffer2[len]=0;
	recv(fd, &filelen, 4, 0);
	filelen = ntohl(filelen);

	snprintf( buffer, 1024, "Transferring %s...",buffer2);
	pcd->tag = ay_progress_bar_add(buffer, filelen, NULL, NULL);

	snprintf( buffer, 1024, "%s/%s", getenv("HOME"),buffer2);
	printf("receiving file %s\n", buffer);
	amount_received = 0;
	fp = fopen(buffer, "wb");

	snprintf( buffer3, 1024, _("Would you like to accept\n the file %s?\nSize=%lu"), buffer2,(unsigned long)filelen);
	do_dialog( buffer3, _("Download File"), accept_file, pcd );
}


void eb_parse_incoming_message( eb_local_account * account,
				 eb_account * remote,
				 char * message )
{
	char * buff;
	char * ptr;
	buff = strdup(message);

	ptr = strtok(buff," ");

	if(ptr && !strcmp(ptr, "EB_COMMAND") && !xfer_in_progress)
	{
		eb_debug(DBG_CORE, "EB_COMMAND received\n");
		ptr = strtok(NULL, " ");
		if(ptr && !strcmp(ptr, "SEND_FILE"))
		{
			char buff2[1024];
 			char   myname[1024];
  			int    s;
  			struct sockaddr_in sa;
  			struct hostent *hp;

  			memset(&sa, 0, sizeof(struct sockaddr_in)); /* clear our address */
  			gethostname(myname, 1023);           /* who are we? */
  			hp= gethostbyname(myname);                  /* get our address info */
  			if (hp == NULL) {                            /* we don't exist !? */
				eb_debug(DBG_CORE, "gethostbyname failed: %s\n", strerror(errno));
				free(buff);
    				return;
			}
  			sa.sin_family= hp->h_addrtype;              /* this is our host address */
  			sa.sin_port= htons(45678);                  /* this is our port number */
  			if ((s= socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* create socket */
				eb_debug(DBG_CORE, "socket failed: %s\n", strerror(errno));
				free(buff);
    				return;
			}
  			if (bind(s,(struct sockaddr *)&sa,sizeof(struct sockaddr_in)) < 0) {
				eb_debug(DBG_CORE, "bind failed: %s\n", strerror(errno));
    				close(s);
				free(buff);
   			 	return;                               /* bind address to socket */
  			}
  			listen(s, 1);                               /* max # of queued connects */
			snprintf(buff2,1024,"EB_COMMAND ACCEPT %s", get_local_addresses());
			RUN_SERVICE(remote)->send_im(account,remote, buff2);
			get_file(s);
		}
		if(ptr && !strcmp(ptr, "ACCEPT"))
		{
			int sockfd;
			struct sockaddr_in dest_addr;

			ptr = strtok(NULL, " ");
			if(!ptr)
			{
				free(buff);
				return;
			}

			sockfd = socket(AF_INET, SOCK_STREAM, 0);

			dest_addr.sin_family = AF_INET;
			dest_addr.sin_port = htons(45678);
			dest_addr.sin_addr.s_addr = inet_addr(ptr);
			memset(&(dest_addr.sin_zero), 0, 8);

			connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
			send_file(filename, sockfd);
		}
	}
	else
	{
		char * message2 = linkify(message);
		eb_chat_window_display_remote_message( account, remote, message2 );
		free(message2);
	}
	free(buff);
}

void eb_update_status( eb_account * remote,
                       char * message )
{
        eb_chat_window_display_status( remote, message );
	eb_chat_room_display_status(remote, message);
}
