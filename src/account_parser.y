%token ACCOUNT END_ACCOUNT

%{
        #include <stdlib.h>
        #include <string.h>

        #include "util.h"
        #include "globals.h"
        #include "value_pair.h"
        #include "service.h"


	extern int Line;
	#define accounterror(error) printf("Parse error on line %d: %s\n", Line, error );
	extern int accountlex();

%}

%union {
	LList * vals;
	value_pair * val;
	char * string;
	eb_local_account * acnt;
}

%token <string> IDENTIFIER
%token <string> STRING
%type <vals> account_list
%type <acnt> account
%type <vals> value_list
%type <val> key_pair
%%

start:
	account_list { accounts = $1; }
;

account_list:
	 	account account_list
		{
			if($1) {
				$$ = l_list_append( $2, $1 );
				eb_debug(DBG_CORE, "Adding account %s\n", $1->handle);
			} else {
				$$=$2;
				eb_debug(DBG_CORE, "Not adding NULL account\n");
			}
		}
	 |	EPSILON { $$ = 0; }

;

account:
	'<' ACCOUNT IDENTIFIER '>' value_list '<' END_ACCOUNT '>' 
	{
		{
			int id = get_service_id($3);

			eb_debug(DBG_CORE, "calling read_local_account_config for %s[%i]\n", $3, id);
			$$ = eb_services[id].sc->read_local_account_config($5);
			eb_debug(DBG_CORE, "read_local_account_config returned: %p\n", $$);
			if($$) {
				$$->service_id = id;
				/*eb_services[id].sc->login($$);*/
			}
			else {
				g_warning("Failed to create %s account", $3);
			}
			save_account_info($3, $5);
			/* value_pair_free($5); */
			g_free($3);
		}
	}
					
;

value_list:
		key_pair value_list { $$ = l_list_append( $2, $1 ); }
	|	EPSILON { $$ = 0; }

;

key_pair:
		IDENTIFIER '=' STRING
		{
			{
				value_pair * vp = g_new0( value_pair, 1 );
				char * value = unescape_string($3);
				strncpy( vp->key, $1 , sizeof(vp->key));
				strncpy( vp->value, value, sizeof(vp->value));

				free($1);
				free($3);
				free(value);
				$$ = vp;
			}
		}
;

EPSILON : ;
	


