/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
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

#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "service.h"
#include "globals.h"
#include "value_pair.h"
#include "util.h"
#include "add_contact_window.h"

#ifndef NAME_MAX
#define NAME_MAX 4096
#endif


void write_account_list()
{
	FILE * fp;
	char buff2[1024];
	LList * l1;

	/*
	 * The contact list is a 3 dimensional linked list, at the top
	 * level you have a list of groups, each group can have several
	 * contacts, each contact can have several accounts.  Please see
	 * the Nomenclature file in the docs directory for details
	 */

	snprintf(buff2, 1024, "%saccounts", config_dir);
	if(! (fp = fdopen( creat(buff2, S_IRWXU), "w") ) )
		return;


	for(l1 = accounts; l1; l1=l1->next )
	{
		eb_local_account * ela = (eb_local_account*)l1->data; 

		/*
		* This is the deal, what a protocol stores as account data is 
		* protocol specific, so you just query for the values you need to write
		* cool stuff :-)
		*/

		LList * config = RUN_SERVICE(ela)->write_local_config(ela);
		fprintf(fp, "<ACCOUNT %s>\n", eb_services[ela->service_id].name);
		value_pair_print_values(config, fp, 1);
		fprintf( fp, "</ACCOUNT>\n" );
	}

	fclose(fp);
}
/*
 * Saves the contact list to (the list of people you see go on and off
 * line)
 */

void write_contact_list()
{
	FILE * fp;
	char buff2[1024];
	LList * l1;
	LList * l2;
	LList * l3;

	/*
	 * The contact list is a 3 dimensional linked list, at the top
	 * level you have a list of groups, each group can have several
	 * contacts, each contact can have several accounts.  Please see
	 * the Nomenclature file in the docs directory for details
	 */

	snprintf(buff2, 1024, "%scontacts", config_dir);
	if(!(fp = fdopen(creat(buff2,0700),"w")))
		return;


	for(l1 = groups; l1; l1=l1->next ) {
		fprintf(fp,"<GROUP>\n\tNAME=\"%s\"\n", ((grouplist*)l1->data)->name);
		for(l2 = ((grouplist*)l1->data)->members; l2; l2=l2->next ) {
			struct contact * c = (struct contact*)l2->data;
			char *strbuf = NULL;
			fprintf(fp, "\t<CONTACT>\n\t\tNAME=\"%s\"\n\t\tDEFAULT_PROTOCOL=\"%s\"\n\t\tLANGUAGE=\"%s\"\n",
					c->nick, eb_services[c->default_chatb].name, c->language);
			strbuf = escape_string(c->trigger.param);
			fprintf(fp, "\t\tTRIGGER_TYPE=\"%s\"\n\t\tTRIGGER_ACTION=\"%s\"\n\t\tTRIGGER_PARAM=\"%s\"\n",
					get_trigger_type_text(c->trigger.type), 
					get_trigger_action_text(c->trigger.action),
					strbuf);
			g_free (strbuf);
			
			for(l3 = ((struct contact*)l2->data)->accounts; l3; l3=l3->next) {
				eb_account * account = (eb_account*)l3->data;
				fprintf( fp, "\t\t<ACCOUNT %s>\n"
						"\t\t\tNAME=\"%s\"\n"
						"\t\t\tLOCAL_ACCOUNT=\"%s\"\n"
					     "\t\t</ACCOUNT>\n",
					 eb_services[account->service_id].name,
					 account->handle,
					 account->ela ? account->ela->handle:"");	 

			}
			fprintf( fp, "\t</CONTACT>\n" );
		}
		fprintf( fp, "</GROUP>\n" );
	}

	fclose(fp);
}

/*
 * To load the accounts file we currently use flex and bison for
 * speed and robustness.... so all that we have to do is just
 * call account parse after opening the file and we are home free
 */

int load_accounts()
{
	FILE * fp;
	char buff2[1024];
	extern int accountparse();
	extern FILE * accountin;

	snprintf(buff2, 1024, "%saccounts",config_dir);

	if(!(fp = fopen(buff2,"r")))
		return 0;
	accounts = NULL;

	accountin = fp;
	accountparse();
	eb_debug(DBG_CORE, "closing fp\n");
	fclose(fp);
	return accounts != NULL;
}

/*
 * to load the contact list we also use flex and bison, so we just
 * call the parser,  (isn't life so simple now :) )
 */

int load_contacts()
{
	FILE * fp;
	char buff2[1024];
	extern int contactparse();
	extern FILE * contactin;
	LList *cts = NULL;
	snprintf(buff2, 1024, "%scontacts",config_dir);

	if(!(fp = fopen(buff2,"r")))
		return 0;
	contactin = fp;
	groups = NULL;

	contactparse();

	fclose(fp);
	
	/* rename logs from old format (contact->nick) to new 
	   (contact->nick "-" contact->group->name) */
	cts = get_all_contacts();
	for (; cts && cts->data; cts=cts->next) {
		struct contact * c = (struct contact *)cts->data;
		FILE *test = NULL;
		char buff[NAME_MAX];
		eb_debug(DBG_CORE,"contact:%s\n",c->nick);
		make_safe_filename(buff, c->nick, c->group->name);
		if ( (test = fopen(buff,"r")) != NULL)
			fclose(test);
		else 
			rename_nick_log(NULL, c->nick, c->group->name, c->nick);
	}
	return 1;
}

eb_account *dummy_account(char *handle, char *group, int serviceid)
{
	eb_account *dum = g_new0(eb_account, 1);
	dum->account_contact = g_new0(struct contact, 1);
	dum->account_contact->group = g_new0(grouplist, 1);
	
	strncpy(dum->handle, handle, sizeof(dum->handle));
	strncpy(dum->account_contact->group->name, group,
		sizeof(dum->account_contact->group->name));
	dum->service_id = serviceid;
	
	return dum;
}
