%token ACCOUNT END_ACCOUNT

%{
        #include <stdlib.h>
        #include <string.h>

	#include "input_list.h"
        #include "util.h"
        #include "globals.h"
        #include "value_pair.h"
        #include "service.h"


	extern int Line;
	#define accounterror(error) printf("Parse error on line %d: %s\n", Line, error );
	extern int accountlex();

	const char *decode_password(const char *pass_in, int enc_type);

	static int laccount_cmp(const void * a, const void * b)
	{
		const eb_local_account *ca=a, *cb=b;
		int cmp = 0;
		if(cmp == 0)
			cmp = strcasecmp(ca->handle, cb->handle);
		if(cmp == 0)
			cmp = strcasecmp(get_service_name(ca->service_id), get_service_name(cb->service_id));

		return cmp;
	}

%}

%union {
	LList * vals;
	value_pair * val;
	char * string;
	eb_local_account * acnt;
}

%token <string> COMMENT
%token <string> IDENTIFIER
%token <string> STRING
%type <vals> account_list
%type <acnt> account
%type <vals> value_list
%%

start:
	account_list { accounts = $1; }
;

account_list:
		COMMENT account_list { $$=$2; }
	| 	account account_list
		{
			if($1) {
				$$ = l_list_insert_sorted( $2, $1, laccount_cmp );
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

			char *s = value_pair_get_value($5, "enc_type");
			if(s) {
				int enc_type = atoi(s);
				char *password = value_pair_get_value($5, "password_encoded");
				free(s);
				$5 = value_pair_add($5, "PASSWORD", decode_password(password, enc_type));
				free(password);
			}
			

			eb_debug(DBG_CORE, "calling read_local_account_config for %s[%i]\n", $3, id);
			$$ = eb_services[id].sc->read_local_account_config($5);
			eb_debug(DBG_CORE, "read_local_account_config returned: %p\n", $$);
			if($$) {
/*				eb_update_from_value_pair( $$->prefs, $5 );*/
				$$->service_id = id;
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
		COMMENT value_list { $$ = $2; }
	|	IDENTIFIER '=' STRING value_list { 
			$$ = value_pair_add($4, $1, $3);
			free($1);
			free($3);
		}
	|	EPSILON { $$ = 0; }

;

EPSILON : ;
	


