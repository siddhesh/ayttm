/*
 * lj_httplib.c
 *
 * Copyright (C) 2003, Philip S Tellis <philip . tellis AT gmx . net>
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

/*
 * This source file taken from yahoo_httplib.c from the libyahoo2 project
 * http://libyahoo2.sourceforge.net/
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <ctype.h>

#include "plugin_api.h"
#include "tcp_util.h"

#include "value_pair.h"
#include "lj_httplib.h"
#include "lj.h"

int url_to_host_port_path(const char *url, char *host, int *port, char *path)
{
	char *urlcopy=NULL;
	char *slash=NULL;
	char *colon=NULL;

	/*
	 * http://hostname
	 * http://hostname/
	 * http://hostname/path
	 * http://hostname/path:foo
	 * http://hostname:port
	 * http://hostname:port/
	 * http://hostname:port/path
	 * http://hostname:port/path:foo
	 */

	if(strstr(url, "http://") == url) {
		urlcopy = strdup(url+7);
	} else {
		/*WARNING(("Weird url - unknown protocol: %s", url));*/
		return 0;
	}

	slash = strchr(urlcopy, '/');
	colon = strchr(urlcopy, ':');

	if(!colon || (slash && slash < colon)) {
		*port = 80;
	} else {
		*colon = 0;
		*port = atoi(colon+1);
	}

	if(!slash) {
		strcpy(path, "/");
	} else {
		strcpy(path, slash);
		*slash = 0;
	}

	strcpy(host, urlcopy);

	free(urlcopy);

	return 1;
}

static int isurlchar(unsigned char c)
{
	return (isalnum(c) || '-' == c || '_' == c);
}

char *urlencode(const char *instr)
{
	int ipos=0, bpos=0;
	char *str = NULL;
	int len = strlen(instr);

	if(!(str = malloc(sizeof(char) * (3*len + 1)) ))
		return "";

	while(instr[ipos]) {
		while(isurlchar(instr[ipos]))
			str[bpos++] = instr[ipos++];
		if(!instr[ipos])
			break;

		snprintf(&str[bpos], 4, "%%%.2x", instr[ipos]);
		bpos+=3;
		ipos++;
	}
	str[bpos]='\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = realloc(str, sizeof(char) * (len+1));

	return (str);
}

char *urldecode(const char *instr)
{
	int ipos=0, bpos=0;
	char *str = NULL;
	char entity[3]={0,0,0};
	unsigned dec;
	int len = strlen(instr);

	if(!(str = malloc(sizeof(char) * (len+1)) ))
		return "";

	while(instr[ipos]) {
		while(instr[ipos] && instr[ipos]!='%')
			if(instr[ipos]=='+') {
				str[bpos++]=' ';
				ipos++;
			} else
				str[bpos++] = instr[ipos++];
		if(!instr[ipos])
			break;
		ipos++;

		entity[0]=instr[ipos++];
		entity[1]=instr[ipos++];
		sscanf(entity, "%2x", &dec);
		str[bpos++] = (char)dec;
	}
	str[bpos]='\0';

	/* free extra alloc'ed mem. */
	len = strlen(str);
	str = realloc(str, sizeof(char) * (len+1));

	return (str);
}

struct callback_data {
	int tag;
	lj_callback  callback;
	eb_local_account *ela;
	char *request;
};

static void read_http_response(void *data, int fd, eb_input_condition cond)
{
	struct callback_data *ccd = data;

	char buff[MAX_PREF_LEN];
	int n=0;

	LList *pairs=NULL;
	char key[MAX_PREF_NAME_LEN]="";

	int success=LJ_HTTP_NETWORK;

	while(success != LJ_HTTP_OK && (n=ay_tcp_readline(buff, sizeof(buff), fd)) > 0) {
		/* read up to blank line */
		if(!strcmp(buff, ""))
			success=LJ_HTTP_OK;
		else if(!strncmp(buff, "HTTP/", strlen("HTTP/")))
			if(!strstr(buff, " 200 "))
				success=LJ_HTTP_NOK;
	}

	if(n==0)
		goto end_of_read;

	while((n=ay_tcp_readline(buff, sizeof(buff), fd)) > 0) {
		if(!strcmp(key, "")) {
			strncpy(key, buff, sizeof(key));
		} else {
			pairs = value_pair_add(pairs, key, buff);
			strcpy(key, "");
		}
	}

end_of_read:
	if(ccd->callback)
		ccd->callback(success, ccd->ela, pairs);

	value_pair_free(pairs);

	eb_input_remove(ccd->tag);
	close(fd);

	free(ccd);
}

static void lj_got_connected(int fd, int error, void *data)
{
	struct callback_data *ccd = data;
	if(error == 0 && fd > 0) {
		write(fd, ccd->request, strlen(ccd->request));
		fsync(fd);
	}
	free(ccd->request);
	ccd->tag = eb_input_add(fd, EB_INPUT_READ, read_http_response, ccd);
}

void send_http_request(const char *request, lj_callback callback, eb_local_account *ela)
{
	char buff[1024];
	struct callback_data *ccd=calloc(1, sizeof(struct callback_data));
	snprintf(buff, sizeof(buff),
			"POST %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: %u\r\n"
			"\r\n"
			"%s",
			ext_lj_path(), ext_lj_host(), strlen(request), request);

	ccd->callback = callback;
	ccd->request = strdup(buff);
	ccd->ela = ela;

	ccd->tag = ay_socket_new_async(ext_lj_host(), ext_lj_port(), lj_got_connected, ccd, NULL);
}

