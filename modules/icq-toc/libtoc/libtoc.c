/*
 * libtoc 
 *
 * Copyright (C) 1999, Torrey Searle <tsearle@uci.edu>
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

#include <sys/types.h>
#ifdef __MINGW32__
#define __IN_PLUGIN__
#include <winsock2.h>
#define O_NONBLOCK 0x4000
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#ifndef __MINGW32__
#include <sys/socket.h>
#endif
#include <fcntl.h>
#include <gtk/gtk.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "libproxy/libproxy.h"
#include "libtoc.h"
#include "plugin_api.h"

#ifdef __MINGW32__
#define write(a,b,c) send(a,b,c,0)
#define read(a,b,c)  recv(a,b,c,0)
#define snprintf _snprintf
#endif


#define TOC_HOST "toc.oscar.aol.com"
#define TOC_PORT 80
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define REVISION "TIC:TOC2:Ayttm"
#define ROAST "Tic/Toc"

#define TALK_UUID "09461341-4C7F-11D1-8222-444553540000"
#define IM_IMAGE_UUID "09461345-4C7F-11D1-8222-444553540000"
#define SEND_FILE_UUID "09461343-4C7F-11D1-8222-444553540000"
#define GET_FILE_UUID "09461348-4C7F-11D1-8222-444553540000"
#define SEND_BUDDYLIST "0946134B-4C7F-11D1-8222-444553540000"
#define BUDDY_ICON_UUID "09461346-4C7F-11D1-8222-444553540000"

extern int do_icq_debug;
#define DEBUG do_icq_debug

static char user_info_id[1024];


// HACK ALERT - user info hack begin
char * info;
// end hack (user info)


enum Type
{
	SIGNON = 1,
	DATA = 2,
#ifdef __MINGW32__
	MYERROR = 3,
#else
	ERROR = 3, //not used by TOC
#endif
	SIGNOFF = 4, //not used by TOC
	KEEP_ALIVE = 5
};
unsigned long flap_version = 1;


typedef struct _flap_header
{
	char ast;
	char type;
	short seq;
	short len;
} flap_header;

void (*icqtoc_im_in)(toc_conn  * conn, char * user, char * message );
void (*icqtoc_chat_im_in)(toc_conn  * conn, char * id, char * user, char * message );
void (*icqtoc_chat_invite)(toc_conn * conn, char * id, char * name,
	   char * sender, char * message );

void (*icqtoc_new_user)(char * group, char * handle);
void (*icqtoc_new_group)(char * group);
void (*icqtoc_join_ack)(toc_conn * conn, char * id, char * name);
void (*icqupdate_user_status)(char * user, int online, time_t idle, int evil, int unavailable );
void (*icqtoc_error_message)(char * message);
void (*icqtoc_disconnect)(toc_conn * conn);
void (*icqtoc_chat_update_buddy)(toc_conn * conn, char * id, 
		char * user, int online );

int  (*icqtoc_begin_file_recieve)( char * filename, unsigned long size );
void (*icqtoc_update_file_status)( int tag, unsigned long progress );
void (*icqtoc_complete_file_recieve)( int tag );
void (*icqtoc_file_offer)( toc_conn * conn, char * nick, char * ip, short port,
		char * cookie, char * filename );
void (*icqtoc_logged_in)(toc_conn *conn);


void (*icqtoc_user_info)(toc_conn  * conn, char * user, char * message );
static void icqtoc_signon_cb(int fd, int error, void *data);


unsigned int get_address(char *hostname)
{
	struct hostent *hp;
	if ((hp = proxy_gethostbyname(hostname))) 
	{
		return ((struct in_addr *)(hp->h_addr))->s_addr;
	}
	printf("unknown host %s\n", hostname);
	return 0;
}

static int connect_address(unsigned int addy, unsigned short port, void *cb, void *data)
{
	int fd;
	struct sockaddr_in sin;

	sin.sin_addr.s_addr = addy;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if (!cb) {
		fd = socket(AF_INET, SOCK_STREAM, 0);

		if (fd > -1) 
		{
			//quad_addr=strdup(inet_ntoa(sin.sin_addr));
			if (proxy_connect(fd, (struct sockaddr *)&sin, sizeof(sin),NULL,NULL) > -1) 
			{
				return fd;
			}
		}
		return -1;
	} else {
		proxy_connect(-1, (struct sockaddr *)&sin, sizeof(sin), cb, data);
		return 1;
	}
}

static char char_decode( char c )
{
	if( c >= 'A' && c <= 'Z' )
	{
		return c - 'A';
	}
	if( c >= 'a' && c <= 'z' )
	{
		return c - 'a' + 26;
	}
	if( c >= '0' && c <= '9' )
	{
		return c - '0' + 52;
	}
	if( c == '+' )
	{
		return 62;
	}
	if( c == '/' )
	{
		return 63;
	}
	return 0;
}

static char * base64_decode( char * input )
{
	char * output = g_new0( char, strlen(input) );
	int i = 0;
	int j = 0;

if(DEBUG)
	printf("Decoding %s\n", input );


	for( i = 0, j = 0; input[i]; i+=4, j += 3 )
	{
		char value[4];
		value[0] = char_decode( input[i] );
		value[1] = char_decode( input[i+1] );
		value[2] = char_decode( input[i+2] );
		value[3] = char_decode( input[i+3] );

		value[0] = value[0] << 2;
		value[0] = value[0] | ( (value[1] & 0x30) >> 4 );
		value[1] = ( value[1] & 0x0F ) << 4;
		value[1] = value[1] | ( (value[2] & 0x3C) >> 2 );
		value[2] = value[2] << 6;
		value[2] = value[2] | value[3] ;

		output[j] = value[0];
		output[j+1] = value[1];
		output[j+2] = value[2];
		output[j+3] = 0;

	}
	output[j] = 0;

if(DEBUG) {
	for(i = 0; i < j; i += 2 )
	{
		printf("%c%c", output[i], output[i+1] );
	}
	printf("\n");
}
	return output;
}

//ERROR:<Error Code>:Var args
char * parse_error(char * data)
{
	int code;
	static char message[1024];
	data[3] = 0;  //terminate the string at the :
	code = atoi(data);

	switch(code)
	{
		case 901:
			g_snprintf(message,1024,"%s not currently available", data+4);
			break;
		case 902:
			g_snprintf(message, 1024, "Warning of %s not currently available", data+4);
			break;
		case 903:
			g_snprintf(message, 1024, "A message has been dropped, you are exceeding the server speed limit");
			break;
		case 950:
			g_snprintf(message, 1024, "Chat in %s is unavailable.", data+4);
			break;
		case 960:
			g_snprintf(message, 1024, "You are sending message too fast to %s", data+4);
			break;
		case 961:
			g_snprintf(message, 1024, "You missed an im from %s because it was too big.", data+4);
			break;
		case 962:
			g_snprintf(message, 1024, "You missed an im from %s because it was sent too fast.", data+4);
			break;
		case 970:
			g_snprintf(message, 1024, "Failure");
			break;
		case 971:
			g_snprintf(message, 1024, "Too many matches");
			break;
		case 972:
			g_snprintf(message, 1024, "Need more qualifiers");
			break;
		case 973:
			g_snprintf(message, 1024, "Dir service temporarily unavailable");
			break;
		case 974:
			g_snprintf(message, 1024, "Email lookup restricted");
			break;
		case 975:
			g_snprintf(message, 1024, "Keyword Ignored");
			break;
		case 976:
			g_snprintf(message, 1024, "No Keywords");
			break;
		case 977:
			g_snprintf(message, 1024, "Language not supported");
			break;
		case 978:
			g_snprintf(message, 1024, "Country not supported");
			break;
		case 979:
			g_snprintf(message, 1024, "Failure unknown %s", data+4);
			break;
		case 980:
			g_snprintf(message, 1024, "Incorrect nickname or password");
			break;
		case 981:
			g_snprintf(message, 1024, "The service is temporarily unavailable.");
			break;
		case 982:
			g_snprintf(message, 1024, "Your warning level is currently too high to sign on.");
			break;
		case 983:
			g_snprintf(message, 1024, "You have been connecting and disconnecting too frequently.  Wait 10 minutes and try again.  If you continue to try, you will need to wait even longer");
			break;
		case 989:
			g_snprintf(message, 1024, "An unknown signon error has occured %s", data+4);
			break;
		default:
			g_snprintf(message, 1024, "Unknown error code");
			break;
	}
	return message;
}


char *icq_encode(char * s)
{
	int len = strlen(s);
	int i = 0;
	int j = 0;
	static char buff[2048];

	for( i = 0; i < len+1 && j < 2048; i++ )
	{
		switch(s[i])
		{
			case '$':
			case '{':
			case '}':
			case '[':
			case ']':
			case '(':
			case ')':
				case '\"':
			case '\\':
					buff[j++] = '\\';
					buff[j++] = s[i];
					break;

			default:
					buff[j++] = s[i];
					break;
		}
	}
	return buff;
}



char *icq_normalize(char *s)
{
	static char buf[255];
	char *t, *u;
	int x=0;

	u = t = g_malloc(strlen(s) + 1);

	strncpy(t, s, strlen(s)+1);
	g_strdown(t);

	while(*t) {
		if (*t != ' ') {
			buf[x] = *t;
			x++;
		}
		t++;
	}
	buf[x]='\0';
	g_free(u);
	return buf;
}

unsigned char *roast_password(char *pass)
{
	/* Trivial "encryption" */
	static char rp[256];
	static char *roast = ROAST;
	int pos=2;
	int x;
	strcpy(rp, "0x");
	for (x=0;(x<150) && pass[x]; x++)
		pos+=snprintf(&rp[pos], 256, "%02x", pass[x] ^ roast[x % strlen(roast)]);
	rp[pos]='\0';
	return rp;
}



void send_flap( toc_conn * conn, int type, char * data )
{
	char buff[2048];
	int i;
	/*
	 * FLAP Header (6 bytes)
	 * -----------
	 * Offset   Size  Type
	 * 0        1     ASTERISK (literal ASCII '*')
	 * 1        1     Frame Type
	 * 2        2     Sequence Number
	 * 4        2     Data Length
	 */ 


	flap_header fh;
	int len = strlen(data);

	if(len+sizeof(flap_header)>2047)
	{
		len=2047-sizeof(flap_header);
		//do_error("truncated a messaae");
	}

	if(!conn)
		return;
if(DEBUG)
	printf( "send_flap BEFORE %d %d\n", conn->fd, conn->seq_num );


	fh.ast = '*';
	fh.type = type;
	fh.seq = htons(conn->seq_num++);
	fh.len = htons((short)(len+1));

	memcpy(buff, &fh, sizeof(flap_header));
	memcpy(buff+sizeof(flap_header), data, len+1);
	i = 0;
	while(i < sizeof(flap_header)+len+1 )
	{
		int ret;
		ret = send(conn->fd,buff+i,sizeof(flap_header)+len+1-i, MSG_NOSIGNAL);
		if(ret < 0)
		{
			fprintf(stderr, "Error sending in send_flap!");
			break;
		}
		i += ret;	

	}

	if(DEBUG) {
		printf("%s\n", data);
		printf( "send_flap AFTER %d %d\n", conn->fd, conn->seq_num );
	}	
}

/*
void icqtoc_set_config( toc_conn * conn, char * config )
{
	char buffer[2048];

	g_snprintf(buffer, 2048, "toc_set_config \"%s\"", config);
if(DEBUG)
	printf("%s\n",buffer);	
	send_flap(conn, DATA, buffer);
}	*/

static void icqtoc_get_file_data( gpointer data, int source, eb_input_condition condition )
{

	char val[1025];
	toc_file_conn * conn = data;
	long total_len = ntohl(*((long*)(conn->header2+22)));
	short header_size =ntohs(*((short*)(conn->header1+4)));
	int read_size;
	if( total_len - conn->amount > 1024 )
	{
		read_size = 1024;
	}
	else
	{
		read_size = total_len - conn->amount;
	}

	if( conn->amount < total_len  
			&&  (read_size = recv( conn->fd, val, read_size, O_NONBLOCK )) > 0 )
	{
		int i;
		conn->amount += read_size;
		for( i = 0; i < read_size; i++ )
		{
			fprintf(conn->file, "%c", val[i] );
		}
		icqtoc_update_file_status(conn->progress, conn->amount);
			
	}
	if( conn->amount >= total_len )
	{
		fclose(conn->file);
		*((short*)(conn->header2 + 18)) = 0;
		*((short*)(conn->header2 + 20)) = 0;
		conn->header2[94] = 0;
		conn->header2[1] = 0x04;
		memcpy(conn->header2+58, conn->header2+34, 4);
		memcpy(conn->header2+54, conn->header2+22, 4);
		fprintf(stderr, "sending final packet\n");
		send( conn->fd, conn->header1, 6, 0 );
		send( conn->fd, conn->header2, header_size - 6, 0 );
		eb_input_remove(conn->handle);
		close(conn->fd);
		g_free(conn);
		icqtoc_complete_file_recieve(conn->progress);
	}
}

void icqtoc_get_file( char * ip, short port, char * cookie, char * filename )
{
		int fd;
		char buff[2048];
		char buff2[7];

		FILE * file;

		toc_file_conn * conn = g_new0( toc_file_conn, 1 );
	

		typedef struct _file_header
		{
			short magic;
			char cookie[8];
			short encryption;
			short compression;
			short total_num_files;
			short total_num_files_left;
			short total_num_parts;
			short total_num_parts_left;
			long total_file_size;
			long file_size;
			long modified_time;
			long checksum;
			long res_fork_checksum;
			long res_fork_size;
			long creation_time;
			long res_fork_checksum2;
			long num_received;
			long received_checksum;
			char id_string[32];
			char flags;
			char list_name_offset;
			char list_size_offset;
			char dummy[69];
			char mac_file_info[16];
			short name_encoding;
			short name_language;
		} file_header;

		file_header * fh = (file_header *)buff;
		short header_size;
		char * cookie2 = base64_decode(cookie);
		int i;
		
		for( i = 0; (fd = connect_address( inet_addr(ip), port ,NULL,NULL)) <= 0 && i < 10; i++ );

		
if(DEBUG)
		fprintf(stderr, "connected to %s\n", ip );


		recv( fd, buff2, 6, 0 );
		buff2[6] = '\0';
		header_size =ntohs(*((short*)(buff2+4)));

if(DEBUG)
		fprintf(stderr, "header_size = %d\n", header_size );

		
		recv( fd, buff, header_size - 6, 0 );

		if( fh->magic != 0x0101 )
		{
			fprintf(stderr, "bad magic number %x\n", fh->magic );
			close(fd);
			return;
		}

if(DEBUG)
		fprintf( stderr, "magic = %04x\n", fh->magic );

		
		fh->magic = htons(0x0202);
		memcpy(fh->cookie, cookie2, 8);
		g_free(cookie2);
if(DEBUG) {
		fprintf(stderr, "id_string = %s\n", buff + 62 );
		fprintf(stderr, "file_name = %s\n", buff + 186 );
}
		memset(buff + 62, 0, 32);
		strncpy(buff + 62, "TIK", sizeof(buff)-62);
		fh->encryption = 0;
		fh->compression = 0;
		fh->total_num_parts = htons(1);
		fh->total_num_parts_left = htons(1);

if(DEBUG) {
		fprintf(stderr, "total_num_parts = %04x total_num_parts_left = %04x file_size = %u\n",
		fh->total_num_parts, fh->total_num_parts_left, (unsigned int)ntohl(*((long*)(buff+22))));

}

		send( fd, buff2, 6, 0 );
		send( fd, buff, header_size - 6, 0 );

		file = fopen( filename, "w" );

		memcpy(conn->header1, buff2, 7 );
		memcpy(conn->header2, buff, 2048 );
		conn->fd = fd;
		conn->amount = 0;
		conn->file = file;

		conn->progress = icqtoc_begin_file_recieve( filename,  ntohl(*((long*)(buff+22))) );

		conn->handle = eb_input_add(fd, EB_INPUT_READ, icqtoc_get_file_data, conn);

}

void icqtoc_get_talk( char * ip, short port, char * cookie )
{
	fprintf( stderr, "Trying to connect to %s:%d\n", ip, port );
}

void icqtoc_send_keep_alive( toc_conn * conn )
{
	char buff[2048];
	int i;
	/*
	 * FLAP Header (6 bytes)
	 * -----------
	 * Offset   Size  Type
	 * 0        1     ASTERISK (literal ASCII '*')
	 * 1        1     Frame Type
	 * 2        2     Sequence Number
	 * 4        2     Data Length
	 */ 

	 
	flap_header fh;
	
if(DEBUG)
	printf( "toc_send_keep_alive BEFORE %d %d\n", conn->fd, conn->seq_num );

	
	fh.ast = '*';
	fh.type = KEEP_ALIVE;
	fh.seq = htons(conn->seq_num++);
	fh.len = 0;

	memcpy(buff, &fh, sizeof(flap_header));
	i = 0;
	while(i < sizeof(flap_header) )
	{
		i += write(conn->fd,buff+i,sizeof(flap_header)-i);
	}
if(DEBUG)
	printf( "toc_send_keep_alive AFTER %d %d\n", conn->fd, conn->seq_num );

	
}

void icqtoc_accept_user(toc_conn *conn, char *user)
{
	char buff2[2048];
	if (user) { /* Only is user is not NULL */
		g_snprintf(buff2, 2048, "toc2_add_permit %s", 
			icq_normalize(user));
		send_flap(conn, DATA, buff2);
		g_snprintf(buff2, 2048, "toc2_remove_deny %s", 
			icq_normalize(user));
		send_flap(conn, DATA, buff2);
	}
}

char * get_flap(toc_conn * conn )
{
	static char buff[8192];
	flap_header fh;
	int len = 0;
	int stat = 0;
	fd_set fs;
	int sb,ind;

if(DEBUG)
	printf( "get_flap BEFORE %d %d\n", conn->fd, conn->seq_num );

	FD_ZERO(&fs);
	FD_SET(conn->fd, &fs);

	select(conn->fd+1, &fs, NULL, NULL, NULL );
	stat = read(conn->fd, &fh, sizeof(flap_header));
	
	if(stat <= 0 )
	{
		fprintf(stderr, "Server disconnect, stat failed: %s\n", strerror(errno));
		icqtoc_disconnect(conn);
		return NULL;
	}
	while(len < ntohs(fh.len) && len < 8192 )
	{
		int val;
		FD_ZERO(&fs);
		FD_SET(conn->fd, &fs);

		select(conn->fd+1, &fs, NULL, NULL, NULL );
		val = read( conn->fd, buff+len, ntohs(fh.len)-len);
		if(val > 0)
			len += val;
		else
		{
			fprintf(stderr, "Server Disconnect, no read on connection: %s", strerror(errno));
			icqtoc_disconnect(conn);
			return NULL;
		}
	}
	buff[len] = 0;
	
	for (sb=0;sb<len;sb++)
        {
            if (buff[sb] == 0)
            {
                for(ind=sb;ind<len;ind++)
                {
                    buff[ind] = buff[ind+1];
                }
                sb--;
                len--;
            }
/*            printf("%d:",buff[sb]); */
        }
/*        printf("\r\n"); */

if(DEBUG) {
	fprintf(stderr, "Flap length = %d\n", len);
	printf( "get_flap AFTER %d %d\n", conn->fd, conn->seq_num );
}
	return buff;
}

void icqtoc_callback( toc_conn * conn )
{
	char buff[8192];
	char c[8192];
	int i = 0;
	int j = 0;
	char * temp;
//	GList * node;
//	char buddies[2048];

if(DEBUG) {
	printf( "icqtoc_callback BEFORE %d %d\n", conn->fd, conn->seq_num );
}

	temp = get_flap(conn);
	if( temp )
	{
		strncpy(buff, temp,8192);
	}
	else
		return;

if(DEBUG)
	fprintf(stderr,"Received flap: %s\n", buff);



	for( j = 0; buff[i] != ':' && buff[i]; j++, i++ )
	{
		c[j] = buff[i];
	}
	c[j] = 0;
	i++;

	if(!strcmp(c, "SIGN_ON"))
	{
                // HACK ALERT begin user info hack
#ifdef __MINGW32__
                g_snprintf(buff, 2000, "toc_set_info \"%s\"", icq_encode(info));
#else
                snprintf(buff, 2000, "toc_set_info \"%s\"", icq_encode(info));
#endif
                // end hack (user info)

		send_flap(conn, DATA, "toc_init_done");

                // HACK ALERT begin user info hack
		send_flap(conn, DATA, buff);
                // end hack (user info)

		send_flap(conn, DATA, "toc_set_caps 09461343-4C7F-11D1-8222-444553540000 09461344-4C7F-11D1-8222-444553540000 09461341-4C7F-11D1-8222-444553540000 09461347-4C7F-11D1-8222-444553540000 09461348-4C7F-11D1-8222-444553540000 09461345-4C7F-11D1-8222-444553540000 09461346-4C7F-11D1-8222-444553540000");
	}
	else if(!strcmp(c, "CONFIG2"))
	{
		char * d = strtok(&(buff[i]), "\n");
		char group[255] = "Unknown";

		while(d)
		{
			if(*d == 'g')
			{
				strncpy(group, d+2, sizeof(group));
				icqtoc_new_group(group);
			}
			else if(*d == 'b')
			{
				icqtoc_new_user(group, d+2);
			}
			d = strtok(NULL, "\n");
		}
	}
	else if(!strcmp(c, "IM_IN2"))
	{
		/*
		 * IM_IN:<Source User>:<Auto Response T/F?>:<Message>
		 * IM_IN2:<Source User>:<Auto Response T/F?>:??:<Message>
		 */
		 
	
		char user[255];
		char message[2048];

		for( j = 0; buff[i] != ':' ; j++, i++ )
		{
			user[j] = buff[i];
		}
		user[j] = 0;
		i++;

		for(; buff[i] != ':'; i++ );
		i++;
		
		for(; buff[i] != ':'; i++ );
		i++;
		strncpy(message, buff+i,2048);

		icqtoc_im_in(conn, user, message);
	}
	else if(!strcmp(c, "UPDATE_BUDDY2"))
	{
		/*
		 * UPDATE_BUDDY:<Buddy User>:<Online? T/F>:<Evil Amount>:
		 * <Signon Time>:<IdleTime>:<UC>
		 * UPDATE_BUDDY2:<Buddy User>:<Online? T/F>:<Evil Amount>:
		 * <Signon Time>:<IdleTime>:<UC>:??
		 */
		char user[255];
		char idle[255], evil[256];
		time_t idle_time = 0;
//		time_t signon;
		int unavailable = 0;
		int online;

		/* get username */
		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			user[j] = buff[i];
		}
		user[j] = 0;
		i++;

		/* are we online? */
		online = (buff[i]=='T')?1:0;
		i+=2;
		

		/*skip evil for now*/

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			evil[j] = buff[i];
		}
		evil[j] = 0;
		i++;

		/*skip online time for now*/

		for(; buff[i] != ':'; i++ );
		i++;

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			idle[j] = buff[i];
		}
		idle[j] = 0;
		i++;

		for ( j = 0; j < 3; j++, i++)
		{
			if ((j==2) && (buff[i]=='U'))
				unavailable = 1;
		}

		if (atoi(idle)) {
			time(&idle_time);
			idle_time -= atoi(idle)*60;
		}

		icqupdate_user_status(user, online, idle_time, atoi(evil), unavailable);
		
	}
	else if(!strcmp(c, "CHAT_UPDATE_BUDDY"))
	{
		/*
		 * CHAT_UPDATE_BUDDY:<Chat Room Id>:<Inside? T/F>:<User 1>:<User 2>...
		 */

		char id[255];
		int inside;

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			id[j] = buff[i];
		}
		id[j] = 0;
		i++;

		inside = (buff[i]=='T'?1:0);
		i+=2;

		while( i < strlen(buff) )
		{
			char user[255];
			for( j = 0; buff[i] != ':' && buff[i] != '\0'; j++, i++ )
			{
				user[j] = buff[i];
			}
			user[j] = 0;
			i++;

if(DEBUG)
			fprintf(stderr, "toc_chat_update_buddy %s, %s, %d\n", id, user, inside);

			icqtoc_chat_update_buddy( conn, id, user, inside);
		}
	}
	else if(!strcmp(c, "CHAT_IN"))
	{
		/*
		 * CHAT_IN:<Chat Room Id>:<Source User>:<Whisper? T/F>:<Message>
		 */

		char user[255];
		char id[255];
		char message[2048];
		
		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			id[j] = buff[i];
		}
		id[j] = 0;
		i++;

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			user[j] = buff[i];
		}
		user[j] = 0;
		i++;
		
		for(; buff[i] != ':'; i++ );
		i++;
		strncpy(message, buff+i,2048);

		icqtoc_chat_im_in(conn, id, user, message );
	}
	else if(!strcmp(c, "CHAT_INVITE"))
	{
		/*
		 * CHAT_INVITE:<Chat Room Name>:<Chat Room Id>:<Invite Sender>:<Message>
		 */

		char name[255];
		char id[255];
		char user[255];
		char message[2048];

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			name[j] = buff[i];
		}
		name[j] = 0;
		i++;
		
		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			id[j] = buff[i];
		}
		id[j] = 0;
		i++;
		
		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			user[j] = buff[i];
		}
		user[j] = 0;
		i++;
		
		strncpy(message, buff+i,2048);

		icqtoc_chat_invite(conn, id, name, user, message );


	}

	else if(!strcmp(c, "CHAT_JOIN"))
	{

		/*
		 * CHAT_JOIN:<Chat Room Id>:<Chat Room Name>
		 */

		char name[255];
		char id[255];

		for( j = 0; buff[i] != ':'; j++, i++ )
		{
			id[j] = buff[i];
		}
		id[j] = 0;
		i++;
		
		strncpy(name, buff+i,255);

		icqtoc_join_ack(conn, id, name);

	}
	else if(!strcmp(c, "RVOUS_PROPOSE") )
	{
		char * file_tlv;
		char tlv[2048];
		char nick[200];
		char cookie[20];
		char uuid[100];
		char port[5];
		char ip[20];
		char filename[255];

		
		for(j=0; buff[i] != ':'; i++, j++ )
		{
			nick[j] = buff[i];
		}
		nick[j] = '\0';
		i++;

		for(j=0; buff[i] != ':'; i++, j++ )
		{
			uuid[j] = buff[i];
		}
		uuid[j] = '\0';
		i++;
		for(j=0; buff[i] != ':'; i++, j++ )
		{
			cookie[j] = buff[i];
		}
		cookie[j] = '\0';
		i++;
		for(; buff[i] != ':'; i++ );
		i++;
		for(; buff[i] != ':'; i++ );
		i++;
		for(j=0; buff[i] != ':'; i++, j++ )
		{
			ip[j] = buff[i];
		}
		ip[j] = 0;
		i++;
		for(; buff[i] != ':'; i++ );
		i++;
		for(j=0; buff[i] != ':'; i++,j++ )
		{
			port[j] = buff[i];
		}
		port[j] = '\0';

		while( buff[i] )
		{
			char type[10];
			i++;
			for(j=0; buff[i] != ':'; i++, j++ )
			{
				type[j] = buff[i];
			}
			i++;
			type[j] = '\0';
			
			for( j = 0; buff[i] && buff[i] != ':'; j++, i++ ) 
			{
				tlv[j] = buff[i];
			}
			tlv[j] = '\0';


			file_tlv = base64_decode(tlv);

			for( j = strlen(8+file_tlv); j > 0 && file_tlv[8+j] != '\\'; j-- );

			g_snprintf(filename, 255, "%s/%s", getenv("HOME"),
					   file_tlv +j +9 );


if(DEBUG)
			printf( "TLV value = %s\n", file_tlv + 8 );

			g_free( file_tlv );
		}

		if(!strcmp(uuid, "09461343-4C7F-11D1-8222-444553540000"))
		{
			icqtoc_file_offer( conn, nick, ip, atoi(port), cookie, filename );
		}
		else if(!strcmp(uuid, "09461341-4C7F-11D1-8222-444553540000"))
		{
			icqtoc_talk_accept( conn, nick, ip, atoi(port), cookie );
		}

	}
	else if(!strcmp(c, "GOTO_URL"))
	{
		char url[1024];
		char http_req[1024];
		char format[1024];
		GString * string = g_string_sized_new(1024);
		int start_saving = 0;
		char byte;
		int fd;

		for(j=0; buff[i] != ':' && buff[i] != '\0'; i++, j++ )
		{
			format[j] = buff[i];
		}
		i++;
		if(!strncmp(format, "HTTP", 4)) {
			for(j=0; buff[i] != ':' && buff[i] != '\0'; i++, j++ )
			{
				url[j] = buff[i];
			}
		}
		else
		{
			strncpy(url, &buff[i], 1024);
		}
		fd = connect_address( get_address(conn->server), conn->port,NULL,NULL);
		g_snprintf(http_req, 1024, "GET /%s HTTP/1.0\n\n", url);
		write(fd, http_req, strlen(http_req));

		while(read(fd, &byte, 1))
		{
			if(byte == '<')
				start_saving = 1;

			if(start_saving)
				g_string_append_c(string, byte);
		}

		close(fd);
		icqtoc_user_info(conn, user_info_id, string->str);
		g_string_free(string, TRUE);

	}
	else if (!strcmp(c, "NEW_BUDDY_REPLY2"))
	{
		char nick[200];
		char result[20];
		char message[1024];
		
		for(j=0; buff[i] != ':' && j < 200; i++, j++ )
		{
			nick[j] = buff[i];
		}
		nick[j] = '\0';
		i++;

		for(j=0; buff[i] != ':' && j < 20; i++, j++ )
		{
			result[j] = buff[i];
		}
		result[j] = '\0';
		i++;
		if (!strcmp("auth", result)) {
			printf("contact %s need to accept your add.\n", nick);
		} else if (!strcmp("added", result)) {
			printf("contact %s is added\n", nick);
		} else {
			g_snprintf(message, 1024, "Unknown add result for %s: %s\n",nick,result);
			icqtoc_error_message(message);
		}
	}
	else if (!strcmp(c,"YOU_WERE_ADDED2"))
	{
		char nick[200];
		for(j=0; buff[i] != ':' && j < 200; i++, j++ )
		{
			nick[j] = buff[i];
		}
		nick[j] = '\0';
		i++;
		icqtoc_accept_user(conn, nick);
	}
	else if(!strcmp(c, "ERROR"))
	{
		icqtoc_error_message(parse_error(buff+6));
		if (atoi(buff+6) > 979) {
			icqtoc_disconnect(conn);
		}
	}

if(DEBUG)
	printf( "icqtoc_callback AFTER %d %d\n", conn->fd, conn->seq_num );




}

unsigned int generate_code(char * username, char * password)
{
	int sn = *username - 96;
	int pw = *password - 96;
	int a = sn * 7696 + 738816;
	int b = sn * 746512;
	int c = pw * a;

	return c - a + b + 71665152;
}



void icqtoc_signon( char * username, char * password,
		    char * server, short port, char * tinfo )
{
	toc_conn * conn = g_new0(toc_conn, 1);

	conn->username = strdup(username);
	conn->password = strdup(password);
	
        // HACK ALERT begin user info hack
        info=strdup(tinfo);
        // end hack (user info)

	strncpy(conn->server, server, sizeof(conn->server));
	conn->port = port;


	connect_address(get_address(server), port,icqtoc_signon_cb, conn);
	
}

static void icqtoc_signon_cb(int fd, int error, void *data)
{
	fd_set fs;
	toc_conn *conn = (toc_conn *)data;
	char *flap_result=NULL;
	char *norm_username = icq_normalize(conn->username);
	unsigned short username_length = htons(strlen(norm_username));
	char sflap_header[] = {0,0,0,1,0,1};
	char buff[2048];
	flap_header fh;

	conn->fd=fd;
	
	if ( conn->fd < 0 || error)
	{
		conn->fd=-1;
		icqtoc_logged_in(conn);
		return;
	}

	/* Client sends "FLAPON\r\n\r\n" */

	write(conn->fd, "FLAPON\r\n\n\0", 10);

	/* TOC sends Client FLAP SIGNON */

	FD_ZERO(&fs);
	FD_SET(conn->fd, &fs);

	//select(conn->fd+1, &fs, NULL, NULL, NULL );

	flap_result=get_flap(conn);
	if(flap_result)
		memcpy( buff, flap_result,10);	
	else
	{
		fprintf(stderr, "Error!  get_flap failed\n");
		conn->fd=-1;
		icqtoc_logged_in(conn);
		return;
	}

	buff[10] = 0;

	/* Client sends TOC FLAP SIGNON */


	fh.ast = '*';
	fh.type = 1;
	fh.seq = htons(conn->seq_num++);
	fh.len = htons((short)(strlen(norm_username)+8));

	memcpy(buff, &fh, sizeof(flap_header));
	memcpy(buff+6, sflap_header, 6);
	memcpy(buff+12,&username_length,2);
	memcpy(buff+14,norm_username, strlen(norm_username));

	write(conn->fd, buff, strlen(norm_username)+14);

	/* Client sends TOC "toc_signon" message */

	/*toc_signon <authorizer host> <authorizer port> <User Name> <Password>
	 *            <language> <version>
	 *toc2_signon <address> <port> <screenname> <roasted pw> 
	 *	      <language> <version*> 160 <code***>
	 */

	g_snprintf(buff, 2048, "toc2_signon %s %d %s %s %s \"%s\" 160 %d", 
			"login.oscar.aol.com", 29999, norm_username, roast_password(conn->password),
			"english-US", REVISION, 
			generate_code(norm_username, conn->password) );

	send_flap(conn, DATA, buff );

if(DEBUG)
	printf( "toc_signon AFTER %d %d\n", conn->fd, conn->seq_num );
	
	icqtoc_logged_in(conn);
}


void icqtoc_signoff( toc_conn * conn )
{
if(DEBUG)
	printf( "toc_signoff BEFORE %d %d\n", conn->fd, conn->seq_num );

	close(conn->fd);
if(DEBUG)
	printf( "toc_signoff AFTER %d %d\n", conn->fd, conn->seq_num );

}

void icqtoc_chat_join( toc_conn * conn, char * chat_room_name )
{
	char buff[2048];

	g_snprintf(buff,2048, "toc_chat_join 4 \"%s\"", icq_encode(chat_room_name));

	send_flap(conn, DATA, buff);
}

void icqtoc_chat_accept( toc_conn * conn, char * id)
{
	char buff[2048];

	g_snprintf(buff,2048, "toc_chat_accept %s", id);

	send_flap(conn, DATA, buff);


}

void icqtoc_chat_send( toc_conn * conn, char * id, char * message)
{
	char buff[2048];
	g_snprintf(buff,2048, "toc_chat_send %s \"%s\"", id, icq_encode(message));
	send_flap(conn, DATA, buff);
}

void icqtoc_chat_leave( toc_conn * conn, char * id )
{
	char buff[2048];
	g_snprintf(buff,2048, "toc_chat_leave %s", id);
	send_flap(conn, DATA, buff);
}

void icqtoc_set_away( toc_conn * conn, const char * message)
{
	char buff[2048];
    if (message)
		g_snprintf(buff, sizeof(buff), "toc_set_away \"%s\"", message);
	else
		g_snprintf(buff, sizeof(buff), "toc_set_away");
	send_flap(conn, DATA, buff);
}

void icqtoc_send_im( toc_conn * conn, char * username, char * message )
{
	/* toc_send_im <Destination User> <Message> [auto] */

	char buff[2048];
if(DEBUG)
	printf( "toc2_send_im BEFORE %d %d\n", conn->fd, conn->seq_num );
 
	g_snprintf(buff, 2048, "toc2_send_im %s \"%s\"", icq_normalize(username), icq_encode(message));
	send_flap(conn, DATA, buff);
if(DEBUG)
	printf( "toc_send_im AFTER %d %d\n", conn->fd, conn->seq_num );

}


void icqtoc_add_buddies( toc_conn * conn, char * group, LList * list )
{
	char buff[2001];
	char buff2[2048];
	LList * node;

	buff[0] = '\0';
	strcat(buff, "g:");
	strncat(buff, group, sizeof(buff)-strlen(buff));
	strncat(buff, "\n", sizeof(buff)-strlen(buff));

	for( node = list; node; node=node->next )
	{
		char * handle = node->data;	

		strncat( buff, "b:", sizeof(buff)-strlen(buff));
		strncat( buff, icq_normalize(handle), sizeof(buff)-strlen(buff));
		strncat( buff, "\n", sizeof(buff)-strlen(buff));

		if(strlen(buff) > 100 )
		{
			g_snprintf(buff2, 2048, "toc2_new_buddies {%s}", buff); 
			send_flap(conn, DATA, buff2);
			buff[0] = '\0';
			strncat(buff, "g:", sizeof(buff)-strlen(buff));
			strncat(buff, group, sizeof(buff)-strlen(buff));
			strncat(buff, "\n", sizeof(buff)-strlen(buff));
		}

	}
	if(strlen(buff) > strlen(group)+3 )
	{
		g_snprintf(buff2, 2048, "toc2_new_buddies {%s}", buff); 
		send_flap(conn, DATA, buff2);
	}

	for( node = list; node; node=node->next )
	{
		char * handle = node->data;
		icqtoc_accept_user(conn, handle);
	}
}

void icqtoc_add_buddy( toc_conn * conn, char * user, char * group )
{
	/* toc_add_buddy <Buddy User 1> [<Buddy User2> [<Buddy User 3> [...]]] */
	LList * buddies = NULL;

	buddies = l_list_append(buddies, user);
	icqtoc_add_buddies(conn, group, buddies);
	l_list_free(buddies);

}

void icqtoc_get_info( toc_conn * conn, char * user )
{
	char buff[2048];

	g_snprintf(buff, 2048, "toc_get_info %s", icq_normalize(user));
	strncpy(user_info_id, user, sizeof(user_info_id));
	send_flap(conn, DATA, buff);
}

void icqtoc_remove_buddy( toc_conn * conn, char * user, char * group )
{
	char buff[2048];
	char buff2[2048];

	strncpy(buff2, icq_normalize(user), sizeof(buff2));

	g_snprintf(buff, 2048, "toc2_remove_buddy %s \"%s\"", buff2, group);
	strncpy(user_info_id, user, sizeof(user_info_id));
	send_flap(conn, DATA, buff);
}

void icqtoc_add_group(toc_conn *conn, const char *group)
{
	char buff[2048];

	g_snprintf(buff, 2048, "toc2_add_group %s", group);
	send_flap(conn, DATA, buff);	
}

void icqtoc_remove_group(toc_conn *conn, const char *group)
{
	char buff[2048];

	g_snprintf(buff, 2048, "toc2_remove_group %s", group);
	send_flap(conn, DATA, buff);	
}

void icqtoc_set_idle( toc_conn * conn, int idle )
{
	char buff[2048];

if(DEBUG)
	printf( "toc_set_idle BEFORE %d %d\n", conn->fd, conn->seq_num );

	g_snprintf(buff, 2048, "toc_set_idle %d", idle);
	send_flap(conn, DATA, buff);
if(DEBUG)
	printf( "toc_set_idle AFTER %d %d\n", conn->fd, conn->seq_num );


}

void icqtoc_invite( toc_conn * conn, char * id, char * buddy, char * message )
{
	/*
	 * toc_chat_invite <Chat Room ID> <Invite Msg> <buddy1> [<buddy2> [<buddy3> [...]]]
	 */

	char buff[2048];
	g_snprintf( buff, 2048, "toc_chat_invite %s \"%s\" %s", id, 
				icq_encode(message), icq_normalize(buddy) );
	send_flap(conn, DATA, buff );
}

void icqtoc_file_accept( toc_conn * conn, char * nick, char * ip, short port, 
					  char * cookie, char * filename )
{
	char message[2048];
	char uuid[] = "09461343-4C7F-11D1-8222-444553540000";

	g_snprintf( message, 2048, "toc_rvous_accept %s %s %s",
				icq_normalize(nick), cookie, uuid );

	send_flap(conn, DATA, message );

	icqtoc_get_file( ip, port, cookie, filename );

}

void icqtoc_talk_accept( toc_conn * conn, char * nick, char * ip, short port, 
					  char * cookie )
{
	char message[2048];
	char uuid[] = "09461341-4C7F-11D1-8222-444553540000";

	g_snprintf( message, 2048, "toc_rvous_accept %s %s %s 3 GADJ4Q==",
				icq_normalize(nick), cookie, uuid );

	send_flap(conn, DATA, message );

	icqtoc_get_talk( ip, port, cookie );

}

void icqtoc_file_cancel( toc_conn * conn, char * nick, char * cookie )
{
	char message[2048];
	char uuid[] = "09461343-4C7F-11D1-8222-444553540000";

	g_snprintf( message, 2048, "toc_rvous_cancel %s %s %s",
				icq_normalize(nick), cookie, uuid );

	send_flap(conn, DATA, message );

}
	


#if 0

void icqtoc_test_im_in(toc_conn * conn, char * user, char * message )
{
	char buff[2048];
	g_snprintf(buff,2048, "Hello %s", user );
	toc_send_im(conn, user, buff );
}

void icqtoc_callback_handler(gpointer data, int source, eb_input_condition condition )
{
	icqtoc_callback(data);
}
	

int main()
{
	toc_conn * conn;
	char buff[] = "toc_send_im tsearle256 \"if you see this message send a message to tsearle256\"";
	proxy_set_proxy( PROXY_NONE, "", 0 );
	conn = toc_signon("", "");
	icqtoc_im_in = toc_test_im_in;
	eb_input_add(conn->fd, EB_INPUT_READ, icqtoc_callback_handler, conn);

	gtk_main();
}
#endif

