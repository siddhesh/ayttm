%token ACCOUNT END_ACCOUNT GROUP END_GROUP CONTACT END_CONTACT
%{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	#include "util.h"
	#include "globals.h"
	#include "account.h"
	#include "value_pair.h"
	#include "service.h"
	#include "trigger.h"

	extern int Line_contact;
	#define contacterror(error) printf("Parse error on line %d: %s\n", Line_contact, error );
	static struct contact * cur_contact = NULL;
	static grouplist * cur_group = NULL;
	extern int contactlex();

	/*
	static int group_cmp(const void * a, const void * b)
	{
		const grouplist *ga=(grouplist *)a, *gb=(grouplist *)b;
		return strcasecmp(ga->name, gb->name);
	}
	*/
	
	static int contact_cmp(const void * a, const void * b)
	{
		const struct contact *ca=(struct contact *)a, *cb=(struct contact *)b;
		return strcasecmp(ca->nick, cb->nick);
	}

	static int account_cmp(const void * a, const void * b)
	{
		eb_account *ca=(eb_account *)a, *cb=(eb_account *)b;
		return strcasecmp(ca->handle, cb->handle);
	}
	
%}

%union {
	LList * vals;
	value_pair * val;
	grouplist * grp;
	char * string;
	eb_account * acnt;
	struct contact * cntct;
}

%token <string> IDENTIFIER
%token <string> STRING
%type <vals> group_list
%type <grp> group
%type <vals> contact_list
%type <cntct> contact
%type <vals> account_list
%type <acnt> account
%type <vals> value_list
%type <val> key_pair
%%

start:
	group_list { temp_groups = $1; }
;

group_list:
	group group_list {$$ = l_list_prepend( $2, $1 ); }
|	EPSILON { $$ = 0; }
;

group:
	 GROUP value_list
	{
		char * c;
		cur_group = calloc(1, sizeof(grouplist));
		c = value_pair_get_value( $2, "NAME" );
		strncpy( cur_group->name, c , sizeof(cur_group->name));
		free(c);
		cur_group->tree = NULL;
		value_pair_free($2);
	}
				contact_list END_GROUP	{ cur_group->members = $4;
		$$ = cur_group; }
;

contact_list:
	contact contact_list { $$ = l_list_insert_sorted( $2, $1, contact_cmp ); }
|	EPSILON { $$ = 0; }
;

contact:
	CONTACT value_list
	{
		char * c;
		cur_contact = calloc(1, sizeof(struct contact));
		c = value_pair_get_value( $2, "NAME" );
		strncpy( cur_contact->nick, c , sizeof(cur_contact->nick));
		free(c);
		c = value_pair_get_value( $2, "TRIGGER_TYPE" );
		cur_contact->trigger.type = get_trigger_type_num(c);
		free(c);
		c = value_pair_get_value( $2, "TRIGGER_ACTION" );
		cur_contact->trigger.action = get_trigger_action_num(c);
		free(c);
		c = value_pair_get_value( $2, "TRIGGER_PARAM" );
		if(c) {
			strncpy( cur_contact->trigger.param ,c, sizeof(cur_contact->trigger.param) );
			free(c);
		} else {
			cur_contact->trigger.param[0] = '\0';
		}

		c = value_pair_get_value( $2, "LANGUAGE" );
		if(c!=NULL) {
			strncpy(cur_contact->language, c, sizeof(cur_contact->language));
			free(c);
		} else {
			cur_contact->language[0] = '\0';
		}

		c = value_pair_get_value( $2, "DEFAULT_PROTOCOL" );
		cur_contact->default_filetransb = cur_contact->default_chatb = get_service_id(c);
		free(c);
		cur_contact->group = cur_group;
		cur_contact->icon_handler = -1;
		value_pair_free($2);
	}
	account_list END_CONTACT 
	{
		cur_contact->accounts = $4;
		$$=cur_contact;
	}
;	

account_list:
 	account account_list{ $$ = l_list_insert_sorted( $2, $1, account_cmp); }
|	EPSILON { $$ = 0; }

;

account:
	'<' ACCOUNT IDENTIFIER '>' value_list '<' END_ACCOUNT '>' 
	{
		{
			int id = get_service_id($3);

			$$ = calloc(1, sizeof(eb_account));
			if($$) {
				char *c;
				$$->service_id = id;
				$$->account_contact = cur_contact;
				c = value_pair_get_value( $5, "NAME" );
				strncpy($$->handle, c, sizeof($$->handle)-1);
				free(c);
				c = value_pair_get_value( $5, "LOCAL_ACCOUNT");
				if(c && strcmp(c, "")) {
					$$->ela = find_local_account_by_handle(c, id);
					free(c);
				} else {
					$$->ela = find_local_account_for_remote($$, 0);
				}
				$$->icon_handler = $$->status_handler = -1;
				$$ = eb_services[id].sc->read_account_config($$, $5);
			}

			value_pair_free($5);
			free($3);
		}
	}
					
;

value_list:
		key_pair value_list { $$ = l_list_append( $2, $1 ); }
	|	key_pair { $$ = l_list_append(NULL, $1); }

;

key_pair:
		IDENTIFIER '=' STRING
		{
			{
				char * tmp = escape_string ($3);
				value_pair * vp = calloc(1, sizeof(value_pair));
				strncpy( vp->key, $1 , sizeof(vp->key));
				strncpy( vp->value, tmp, sizeof(vp->value) );
				free(tmp);
				free($1);
				free($3);
				$$ = vp;
			}
		}
;

EPSILON : ;
