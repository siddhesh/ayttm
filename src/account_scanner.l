A [^\n\t"<>/= ]

%{
	#include <string.h>

	#include "value_pair.h"
	#include "account.h"
	#include "account_parser.h"

	#define accountwrap() 1
	int Line = 1;
%}
%%
[/]account  {return END_ACCOUNT;}
account		{return ACCOUNT;}
["=<>]		return accounttext[0];
#[^\n]*		{return COMMENT;}
{A}+		{accountlval.string = strdup(accounttext); return IDENTIFIER;}
["]([^"]|(\\\"))*["] { accountlval.string = strdup(accounttext+1); accountlval.string[strlen(accountlval.string)-1] = '\0'; return STRING; }
[\n]	   Line++;
[ \t]	   { }
.			printf("Lexical error on %c\n", accounttext[0]);

