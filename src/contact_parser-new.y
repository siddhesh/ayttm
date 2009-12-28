%token ACCOUNT END_ACCOUNT GROUP END_GROUP CONTACT END_CONTACT
%{
        #include <gtk/gtk.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>

        #include "globals.h"
        #include "account.h"
        #include "value_pair.h"
        #include "service.h"
        #include "trigger.h"
        #include "contact_util.h"
	#include "util.h"

	extern int Line_contact;
	#define contacterror(error) printf("Parse error on line %d: %s\n", Line_contact, error );
	static struct contact *cur_contact = NULL;
	static grouplist *cur_group = NULL;
	extern int contactlex();
%}

%union {
	LList *vals;
	value_pair *val;
	grouplist *grp;
	char *string;
	eb_account *ea;
	struct contact *con;
}

%token <string> IDENTIFIER
%token <string> STRING
%type <vals> group_list
%type <grp> group
%type <vals> contact_list
%type <con> contact
%type <vals> account_list
%type <ea> account
%type <vals> value_list
%type <val> key_pair
%%

start:
	group_list 
	{
		groups = $1;
	}
;

group_list:
		group_list group { $$ = l_list_prepend( $1, $2 ); }
	|	EPSILON { $$ = 0; }
;

group:
	GROUP value_list
	{
		char *c;
		c = value_pair_get_value( $2, "NAME" );
		cur_group = add_group(c);
		g_free(c);
		value_pair_free($2);
	}
	contact_list END_GROUP	
	{
		$$ = cur_group;
	}
;

contact_list:
		contact_list contact { $$ = l_list_insert_sorted( $1, $2, contact_cmp ); }
	|	EPSILON { $$ = 0; }
;

contact:
	CONTACT value_list
	{
		char * c;
		int i;
		c = value_pair_get_value( $2, "DEFAULT_PROTOCOL" );
		i = get_service_id(c);
		g_free(c);
		c = value_pair_get_value( $2, "NAME" );
		cur_contact = add_contact_with_group(c, cur_group, i);
		g_free(c);

		c = value_pair_get_value( $2, "TRIGGER_TYPE" );
		cur_contact->trigger.type = get_trigger_type_num(c);
		g_free(c);
		c = value_pair_get_value( $2, "TRIGGER_ACTION" );
		cur_contact->trigger.action = get_trigger_action_num(c);
		g_free(c);
		c = value_pair_get_value( $2, "TRIGGER_PARAM" );
		if(c) {
			strncpy( cur_contact->trigger.param ,c, sizeof(cur_contact->trigger.param) );
			g_free(c);
		} else {
			cur_contact->trigger.param[0] = '\0';
		}

                c = value_pair_get_value( $2, "LANGUAGE" );
		if(c!=NULL) {
			strncpy(cur_contact->language, c, sizeof(cur_contact->language));
			g_free(c);
		} else {
			cur_contact->language[0] = '\0';
		}

		value_pair_free($2);
	}
	account_list END_CONTACT 
	{
		cur_contact->accounts = $4;
		$$=cur_contact;
	}
;	

account_list:
	 	account_list account { $$ = l_list_insert_sorted( $1, $2, account_cmp ); }
	 |	EPSILON { $$ = 0; }

;

account:
	'<' ACCOUNT IDENTIFIER '>' value_list '<' END_ACCOUNT '>' 
	{
		{
			int id = get_service_id($3);

			$$ = eb_services[id].sc->read_account_config($5, cur_contact);
			if($$) {
				$$->service_id = id;
				$$->account_contact = cur_contact;
				c = value_pair_get_value( $2, "NAME" );
				strncpy($$->handle, c, sizeof($$->handle)-1);
				g_free(c);
				c = value_pair_get_value( $2, "LOCAL_ACCOUNT");
				if(c) {
					$$->ela = find_local_account_by_handle(c, id);
					g_free(c);
				} else {
					$$->ela = find_local_account_for_remote($$, 0);
				}
				$$->icon_handler = $$->status_handler = -1;
			}
				
			value_pair_free($5);
			g_free($3);
		}
	}
					
;

value_list:
		value_list key_pair { $$ = l_list_prepend( $1, $2 ); }
	|	EPSILON { $$ = 0; }

;

key_pair:
		IDENTIFIER '=' STRING
		{
			{
				char * tmp = escape_string ($3);
				value_pair * vp = g_new0( value_pair, 1 );
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
