
/*
 * IRC protocol Implementation
 *
 * Copyright (C) 2008, Siddhesh Poyarekar and the Ayttm Team
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

/* TODO: DDC and SED implementation */

#ifndef __CTCP_H__
#define __CTCP_H__


/*
 * Special characters with special meanings in CTCP protocol
 */
#define CTCP_TAG	'\001'	/* Tag delimiter */
#define CTCP_XQUOTE	'\134' 	/* CTCP level quoting char for CTCP_TAG */

#define CTCP_MQUOTE	'\020' 	/* Low level quoting char for \n, \r and \0 */
#define CTCP_NUL	'\0'   	/* NULL */
#define CTCP_NL		'\n'    /* Newline */
#define CTCP_CR		'\r'    /* Carriage Return */

/*
 * CTCP Data element. This describes a CTCP command 
 * i.e. the command string, length of command string and a description returned to a CLIENTINFO request
 */
typedef struct {
	const char *data ;
	const int length ;
	const char *clientinfo_description ;
} CTCPExtData ;


/*
 * CTCP extended data types.
 */
enum {
	CTCP_NONE 	= -1  ,
	CTCP_ACTION 	=  0  ,
	CTCP_DCC        =  1  ,
	CTCP_SED        =  2  ,
	CTCP_FINGER     =  3  ,
	CTCP_VERSION    =  4  ,
	CTCP_SOURCE     =  5  ,
	CTCP_USERINFO   =  6  ,
	CTCP_CLIENTINFO =  7  ,
	CTCP_ERRMSG     =  8  ,
	CTCP_PING       =  9  ,
	CTCP_TIME       =  10 ,
	CTCP_DATA_COUNT =  11 
} CTCPCommandType ;


/*
 * An extended data element
 */
typedef struct {
	int type ;	/* Type of Extended data */
	char *data ;	/* The string argument to the extended data type */
} ctcp_extended_data ;


/*
 * A singly linked list of elements of type ctcp_extended_data
 */
typedef struct _extended_data_list {

	ctcp_extended_data *ext_data ;
	struct _extended_data_list *next ;

} ctcp_extended_data_list ;


/*
 * Response structure to \001VERSION\001
 */
typedef struct {
	char *name ;
	char *version ;
	char *env ;
} ctcp_version ;


/*
 * Response structure to \001SOURCE\001
 */
typedef struct {
	char *host ;
	char *path ;
	char *file ;
} ctcp_source ;



/* Quote a message string to send to server */
char * ctcp_encode 
	( const char *msg, 
	  int size ) ;


/* Unquote a message string received from server */
char *ctcp_decode 
	( const char *msg, 
	  int size ) ;


/* 
 * Decode extended data from the message string. 
 * Return value: Pointer to a ctcp_extended_data_list
 */
ctcp_extended_data_list *ctcp_get_extended_data 
				( const char *in_msg, 
				  int size ) ;

/*
 * Free the data list
 */
void ctcp_free_extended_data
	( ctcp_extended_data_list *data_list );


/*
 * Generate an Extended Data Request
 * Return value: The complete request string, with leading and trailing CTCP tags (\001)
 */
char * ctcp_gen_extended_data_request 
	( int type, 
	  const char *msg ) ;


/*
 * Generate an Extended Data Response. behaviour is the same as ctcp_gen_extended_data_request
 * Return value: The complete response string, with leading and trailing CTCP tags (\001)
 */
#define ctcp_gen_extended_data_response ctcp_gen_extended_data_request


/*
 * Parse the response string to get the various parameters set by VERSION
 * Return value: ctcp_version structure containing the name, version and environment of the client
 */
ctcp_version *ctcp_got_version 
		( const char *msg ) ;


/*
 * Generate a CTCP string response to \001VERSION\001
 * Return value: CTCP response to VERSION containing client name, version and environment
 */
char * ctcp_gen_version_response 
	( char *client_name, 
	  char *client_version, 
	  char *client_env ) ;


/*
 * Parse CTCP response to \001SOURCE\001 request
 * Return value: ctcp_source structure containing parameters of response to SOURCE query
 */
ctcp_source *ctcp_got_source
		( const char *msg ) ;


/*
 * Generate a CTCP response to \001SOURCE\001 query
 * Return value: CTCP Response containing the host, path and filename as requested by the SOURCE query
 */
char *ctcp_gen_source_response 	
	( char *source_host,
	  char *source_path,
	  char *source_file ) ;


/*
 * Generate response to \001TIME\001 Query
 * Return value: CTCP response containing the current time
 */
char *ctcp_gen_time_response
	( void ) ;


/*
 * Generate response to \001CLIENTINFO\001 query.
 * Return value: CTCP response containing description of the COMMAND provided as argument
 */
char *ctcp_gen_clientinfo_response
	( const char *command ) ;

/*
 * Generate response to a \001PING\001 query
 */
char *ctcp_gen_ping_response
	(char *timestamp) ;

#endif

