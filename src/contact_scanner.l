A [^\n\t"<>/= ]

%option noyywrap

%{
	#include <string.h>

	#include "value_pair.h"
	#include "account.h"
	#include "contact_parser.h"

	
    int Line_contact = 1;
%}
%%
[/]account  {return END_ACCOUNT;}
account		{return ACCOUNT;}
[<]GROUP[>]		{return GROUP;}
[<][/]GROUP[>]	{return END_GROUP;}
[<]CONTACT[>]	{return CONTACT;}
[<][/]CONTACT[>]	{return END_CONTACT;}
["=<>]		return contacttext[0];
#[^\n]*		{return COMMENT;}
{A}+		{contactlval.string = strdup(contacttext); return IDENTIFIER;}
["]([^"]|(\\\"))*["] { contactlval.string = strdup(contacttext+1); contactlval.string[strlen(contactlval.string)-1] = '\0'; return STRING; }
[ \t]       { }
[\n]	   { Line_contact++; }
.			printf("Lexical error on %c\n", contacttext[0]);

