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

#include "intl.h"

#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "globals.h"
#include "service.h"
#include "mem_util.h"
#include "offline_queue_mgmt.h"  /* so the compiler tells us about type mismatches */


/* FIXME: temporary UGLY !!! */
eb_account * find_account_by_handle( const char * handle, int type );
/* end FIXME */

void contact_mgmt_queue_add(const eb_account *ea, int action, const char *new_group)
{
	FILE *fp;
	char buff [NAME_MAX];
	snprintf(buff, NAME_MAX, "%s%ccontact_actions_queue", 
				config_dir, 
				G_DIR_SEPARATOR);
	fp = fopen(buff,"a");
	if (!fp) {
		perror(buff);
		return;
	}
	
	fprintf(fp, "%s\t%d\t%s\t%s\t%s\n",
		ea->ela? ea->ela->handle:"____NULL____",
		action,
		ea->handle,
		(action!=MGMT_ADD ? ea->account_contact->group->name:"NULL"),
		(new_group!=NULL ? new_group:"NULL"));
	
	fclose(fp);
}


void group_mgmt_queue_add(const eb_local_account *ela, const char *old_group, int action, const char *new_group)
{
	FILE *fp;
	char buff [NAME_MAX];
	snprintf(buff, NAME_MAX, "%s%cgroup_actions_queue", 
				config_dir, 
				G_DIR_SEPARATOR);
	fp = fopen(buff,"a");
	if (!fp) {
		perror(buff);
		return;
	}
	
	fprintf(fp, "%s\t%d\t%s\t%s\n",
		ela->handle,
		action,
		(old_group!=NULL ? old_group:"NULL"),
		(new_group!=NULL ? new_group:"NULL"));
	
	fclose(fp);
}

int contact_mgmt_flush(eb_local_account *ela)
{
	FILE *queue, *temp;
	char buff[NAME_MAX], queue_name[NAME_MAX], temp_name[NAME_MAX];
	char **tokens = NULL;

	if (ela->connecting) {
		eb_debug(DBG_CORE,"connecting on %s (trying again later)\n",ela->handle);
		return 1;
	}
	
	ela->mgmt_flush_tag = 0; /* we can now consider it won't be called a second time */
	
	if (!ela->connected) {
		eb_debug(DBG_CORE,"disconnected on %s (cancelling flush)\n",ela->handle);
		return 0;
	}
			
	eb_debug(DBG_CORE,"connected on %s (flushing)\n",ela->handle);

	/* flush groups, too */
	group_mgmt_flush(ela);
	
	snprintf(queue_name, NAME_MAX, "%s%ccontact_actions_queue", 
				config_dir, 
				G_DIR_SEPARATOR);
	
	queue = fopen(queue_name, "r");
	if (!queue)
		return 0;
	snprintf(temp_name, NAME_MAX, "%s%ccontact_actions_queue.new", 
				config_dir, 
				G_DIR_SEPARATOR);
	
	temp = fopen(temp_name, "w");
	
	if (!temp) {
		perror(buff);
		return 0;
	}
	
	while( fgets(buff, sizeof(buff), queue)  != NULL )
	{		
		char buff_backup[NAME_MAX];
		strncpy(buff_backup, buff, NAME_MAX);
		tokens = ay_strsplit( buff, "\t", -1 );
		if(!strcmp(tokens[0], ela->handle)) {
			int action     = atoi(tokens[1]);
			char *handle   = tokens[2];
			char *oldgroup = tokens[3];
			char *newgroup = tokens[4];
			eb_account *ea = NULL;
			int ea_recreated = TRUE;
			
			if (strstr(newgroup,"\n")) {
				char *t = strstr(newgroup,"\n");
				*t = '\0';
			}
			
			ea = find_account_by_handle(handle, 
						    ela->service_id);

			if (ea && action == MGMT_ADD && CAN(ea, add_user)) {
				RUN_SERVICE(ea)->add_user(ea);
			}
			if (ea && action == MGMT_MOV) {
				char realgrp[NAME_MAX];
				/* this is "a bit" ugly :-( */
				strncpy(realgrp,ea->account_contact->group->name, sizeof(realgrp));
				strncpy(ea->account_contact->group->name,oldgroup, 
					sizeof(ea->account_contact->group->name));
				if (!strcmp(newgroup, _("Ignore")) && CAN(ea, ignore_user))
					RUN_SERVICE(ea)->ignore_user(ea);
				else if (!strcmp(oldgroup, _("Ignore")) && CAN(ea, unignore_user))
					RUN_SERVICE(ea)->unignore_user(ea, newgroup);
				else if (CAN(ea, change_group))
					RUN_SERVICE(ea)->change_group(ea, newgroup);
				strncpy(ea->account_contact->group->name, realgrp,
					sizeof(ea->account_contact->group->name));
			}
			
			if (ea && action == MGMT_REN && CAN(ea, change_user_name)) {
				RUN_SERVICE(ea)->change_user_name(ea, newgroup);
			}

			if (!ea) {
				ea_recreated = FALSE;
				ea = dummy_account(handle, oldgroup, ela);
			}

			if (action == MGMT_DEL && CAN(ea, del_user)) {
				/* some services syncing lists have
				   automatically re-added our deleted 
				   account */
				if (ea_recreated) {
					if (l_list_singleton(ea->account_contact->accounts))
						remove_contact(ea->account_contact);
					else
						remove_account(ea);
				}
				else
					RUN_SERVICE(ea)->del_user(ea);
			}
		} else {
			/* not for me */
			fprintf(temp, "%s", buff_backup);
		}

		ay_strfreev(tokens);
	}
	
	fclose (temp);
	fclose (queue);
	rename (temp_name, queue_name);
	return 0;
}

int group_mgmt_check_moved(const char *groupname)
{
	FILE *queue;
	char buff[NAME_MAX], queue_name[NAME_MAX];
	snprintf(queue_name, NAME_MAX, "%s%cgroup_actions_queue", 
				config_dir, 
				G_DIR_SEPARATOR);
	
	queue = fopen(queue_name, "r");
	if (!queue)
		return 0;

	while( fgets(buff, sizeof(buff), queue)  != NULL )
	{		
		char **tokens  = ay_strsplit( buff, "\t", -1 );
	/*	int action     = atoi(tokens[1]); */
		char *oldgroup = tokens[2];
	/*	char *newgroup = tokens[3]; */
				
		if (oldgroup && !strcmp(oldgroup, groupname)) {
			fclose(queue);
			return 1;
		}
		ay_strfreev(tokens);
	}
	
	fclose (queue);
	return 0;
}

int group_mgmt_flush(eb_local_account *ela)
{
	FILE *queue, *temp;
	char buff[NAME_MAX], queue_name[NAME_MAX], temp_name[NAME_MAX];
	char **tokens = NULL;
	snprintf(queue_name, NAME_MAX, "%s%cgroup_actions_queue", 
				config_dir, 
				G_DIR_SEPARATOR);
	
	queue = fopen(queue_name, "r");
	if (!queue)
		return 0;
	snprintf(temp_name, NAME_MAX, "%s%cgroup_actions_queue.new", 
				config_dir, 
				G_DIR_SEPARATOR);
	
	temp = fopen(temp_name, "w");
	
	if (!temp) {
		perror(buff);
		return 0;
	}
	
	while( fgets(buff, sizeof(buff), queue)  != NULL )
	{		
		char buff_backup[NAME_MAX];
		strncpy(buff_backup, buff, NAME_MAX);
		tokens = ay_strsplit( buff, "\t", -1 );
		if(!strcmp(tokens[0], ela->handle)) {
			int action     = atoi(tokens[1]);
			char *oldgroup = tokens[2];
			char *newgroup = tokens[3];
			
			if (strstr(newgroup,"\n")) {
				char *t = strstr(newgroup,"\n");
				*t = '\0';
			}
			
			if (action == MGMT_GRP_ADD && CAN(ela, add_group)) {
				RUN_SERVICE(ela)->add_group(ela, newgroup);
			}
			if (action == MGMT_GRP_REN && CAN(ela, rename_group)) {
				RUN_SERVICE(ela)->rename_group(ela, oldgroup,newgroup);					
			}
			if (action == MGMT_GRP_DEL && CAN(ela, del_group)) {
				RUN_SERVICE(ela)->del_group(ela, oldgroup);
			}
		} else {
			/* not for me */
			fprintf(temp, "%s", buff_backup);
		}
		ay_strfreev(tokens);
	}
	
	fclose (temp);
	fclose (queue);
	rename (temp_name, queue_name);
	return 0;
}



