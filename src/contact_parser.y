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
        #include "charconv.h"

	extern int Line_contact;
	#define contacterror(error) printf("Parse error on line %d: %s\n", Line_contact, error );
	static struct contact * cur_contact = NULL;
	static grouplist * cur_group = NULL;
	extern int contactlex();

%}

%union {
	LList * vals;
	value_pair * val;
	grouplist * grp;
	char * string;
	eb_account * acnt;
	struct contact * cntct;
}

%token <string> COMMENT
%token <string> IDENTIFIER
%token <string> STRING
%type <vals> group_list
%type <grp> group
%type <vals> contact_list
%type <cntct> contact
%type <vals> account_list
%type <acnt> account
%type <vals> value_list
%%

start:
	group_list { groups = $1; }
;

group_list:
	COMMENT group_list { $$=$2; }
|	group group_list {$$ = l_list_prepend( $2, $1 ); }
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
	contact_list END_GROUP
	{
		cur_group->members = $4;
		$$ = cur_group;
	}
;

contact_list:
	COMMENT contact_list { $$ = $2; }
|	contact contact_list { 
		if($1)
			$$ = l_list_insert_sorted( $2, $1, contact_cmp ); 
		else
			$$ = $2;
	}
|	EPSILON { $$ = 0; }
;

contact:
	CONTACT value_list
	{
		char * c;
		cur_contact = calloc(1, sizeof(struct contact));
		c = StrToUtf8(value_pair_get_value( $2, "NAME" ));
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

		c = value_pair_get_value( $2, "GPG_KEY" );
		if(c) {
			strncpy( cur_contact->gpg_key ,c, sizeof(cur_contact->gpg_key) );
			free(c);
		} else {
			cur_contact->gpg_key[0] = '\0';
		}

		c = value_pair_get_value( $2, "GPG_CRYPT" );
		cur_contact->gpg_do_encryption = (c && !strcmp(c,"1"));
		free(c);
		c = value_pair_get_value( $2, "GPG_SIGN" );
		cur_contact->gpg_do_signature = (c && !strcmp(c,"1"));
		free(c);
		
		c = value_pair_get_value( $2, "DEFAULT_PROTOCOL" );
		if(c) {
			cur_contact->default_filetransb = cur_contact->default_chatb = get_service_id(c);
			free(c);
		} else {
			cur_contact->default_filetransb = cur_contact->default_chatb = -1;
		}
		cur_contact->group = cur_group;
		cur_contact->icon_handler = -1;
		value_pair_free($2);
	}
	account_list END_CONTACT 
	{
		cur_contact->accounts = $4;
		if(cur_contact->accounts)
			$$=cur_contact;
		else {
			$$=0;
			free(cur_contact);
		}
	}
;	

account_list:
	COMMENT account_list { $$=$2; }
| 	account account_list { 
		if($1 != NULL && !l_list_find_custom($2, $1, account_cmp)) {
			$$ = l_list_insert_sorted( $2, $1, account_cmp); 
			if(cur_contact->default_chatb == -1)
				cur_contact->default_filetransb = cur_contact->default_chatb = $1->service_id;
		} else {
			$$ = $2;
		}
	}
|	EPSILON { $$ = 0; }

;

account:
	'<' ACCOUNT IDENTIFIER '>' value_list '<' END_ACCOUNT '>' 
	{
		{
			int id = get_service_id($3);
			char *handle, *local_handle;
			eb_local_account *ela=0;
			handle = value_pair_get_value( $5, "NAME" );
			local_handle = value_pair_get_value( $5, "LOCAL_ACCOUNT");
			if(local_handle) {
				ela = find_local_account_by_handle(local_handle, id);
				free(local_handle);
			}

			$$=0;
			if(handle && !find_account_with_ela(handle, ela)) {
				$$ = calloc(1, sizeof(eb_account));
				if($$) {
					char *c;
					$$->service_id = id;
					$$->account_contact = cur_contact;
					strncpy($$->handle, handle, sizeof($$->handle)-1);
					if(ela)
						$$->ela = ela;
					else
						$$->ela = find_local_account_for_remote($$, 0);

					c = value_pair_get_value( $5, "PRIORITY" );
					if(c) {
						$$->priority=atoi(c);
						free(c);
					}
					$$->icon_handler = $$->status_handler = -1;
					$$ = eb_services[id].sc->read_account_config($$, $5);
				}
				free(handle);
			}

			value_pair_free($5);
			free($3);
		}
	}
					
;

value_list:
		COMMENT value_list { $$ = $2; }
	|	IDENTIFIER '=' STRING value_list { 
			char *tmp = unescape_string($3);
			free($3);
			$$ = value_pair_add($4, $1, tmp);
			free($1);
			free(tmp);
		}
	|	EPSILON { $$ = 0; }

;

EPSILON : ;
