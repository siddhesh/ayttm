/*
 * ICQ 99a contact list extractor
 *
 * Copyright (C) 2000, Thomas Lundin <thomas.lundin@miratek.se>
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <fcntl.h>				
#include <gtk/gtk.h>
#include <string.h>
#ifdef __MINGW32__
#define __IN_PLUGIN__ 1
#endif
#include "plugin_api.h"
#include "account.h"
#include "util.h"
#include "service.h"
#include "globals.h"
#include "status.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/

/*  Module defines */
#define plugin_info importicq_LTX_plugin_info
#define module_version importicq_LTX_module_version


/* Function Prototypes */
static int plugin_init();
static int plugin_finish();
unsigned int module_version() {return CORE_VERSION;}

static int ref_count=0;

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_UTILITY, 
	"Import ICQ99 Contact List", 
	"Import the ICQ99 Contact List", 
	"$Revision: 1.5 $",
	"$Date: 2003/04/30 06:03:57 $",
	&ref_count,
	plugin_init,
	plugin_finish
};

/* End Module Exports */

/* the .IDX linked list entry */

struct idxEntry {
	guint32 status;
	guint32 dat_number;
	guint32 next;
	guint32 prev;
	guint32 dat_offset;
};

struct groups {
	guint32 number;
	gchar name[30];
};

/*DAT entry numbers */

#define MY_DETAILS      1005
#define CHAT_EVENT      1006
#define MSG_EVENT1      1015
#define MSG_EVENT2      1025
#define PLUGINS         1101
#define OBJ_WORDS       1102
#define SERVER_LIST     1110
#define USERS_DATA      2000

#define LAST_FOLDER     998
#define IGNORE_FOLDER   999     /*own definition */
#define FIRST_IDX       0
#define NEXT_IDX        1

struct my_details {
	gchar user_name[20];
	gchar nick_name[20];
	gchar first_name[20];
	gchar last_name[20];
	gchar group[30];
	guint32 folder;
	guint32 uin;
};

static void import_icq99_contacts(ebmCallbackData * data);

static void *buddy_list_tag=NULL;

static int plugin_init()
{
	eb_debug(DBG_MOD,"ICQ99 Contact List init\n");
	buddy_list_tag=eb_add_menu_item("ICQ99 Contact List", EB_IMPORT_MENU, import_icq99_contacts, ebmIMPORTDATA, NULL);
	if(!buddy_list_tag)
		return(-1);
	return(0);
}

static int plugin_finish()
{
	int result;

	result=eb_remove_menu_item(EB_IMPORT_MENU, buddy_list_tag);
	if(result) {
		g_warning("Unable to remove ICQ99 Contact List menu item from import menu!");
		return(-1);
	}
	return(0);
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

static int wrong_type(struct idxEntry *entry, long type)
{
        if(type==USERS_DATA)
        {
                if(entry->dat_number > type)
                        return 0;
        }
        else
        {
                if(entry->dat_number == type)
                        return 0;
        }
        return 1;

}

gint find_idx_entry(int handle, struct idxEntry * entry,guint32 type, gint mode)
{
	if(mode==FIRST_IDX)
		lseek(handle,225,SEEK_SET);
	else
	{
		if(entry->next !=-1)
			lseek(handle,entry->next,SEEK_SET);
			entry->dat_number=0;	
	}
	while(wrong_type(entry, type) && (entry->next != -1))
	{
		read(handle, entry,  20);
		while(entry->status != -2 && (entry->next != -1))
		{
			read(handle, entry,  20);
			if(entry->next !=-1)
				lseek(handle,entry->next,SEEK_SET);
		}
		if(entry->next !=-1)
			lseek(handle,entry->next,SEEK_SET);
	}
	if(!wrong_type(entry,type) && (entry->next == -1))
		return -1;
	else
		return 1;
}
void pass_strings(int handle, guint32 number, gint loop_pre_inc, gint post_inc)
{
	guint16	length, i;
	for(i=0;i<number;i++)
	{
		lseek(handle,loop_pre_inc, SEEK_CUR);
		read(handle, &length, 2);		/*get the length of the string*/
		lseek(handle,length, SEEK_CUR);
	}
	lseek(handle,post_inc, SEEK_CUR);

}
void parse_my_details(int dat, struct my_details * my_details)
{
	guint8 property_value;
	guint32 length;

	lseek(dat,0x2a,SEEK_CUR);					/*step to number of vaw entries 47*/
	read(dat,&length,4);
	pass_strings(dat,length,10,40);
	read(dat,&length,4);
	for(;length>0;length--)
	{
		pass_strings(dat,1,0,0);
		read(dat,&property_value,1);	
		switch (property_value)
		{
			case 'e':
				lseek(dat,1,SEEK_CUR);		
				break;
			case 'f':
			case 'k':
				lseek(dat,2,SEEK_CUR);
				break;
			case 'h':
			case 'i':
				lseek(dat,4,SEEK_CUR);
				break;
			default:
				eb_debug(DBG_MOD,"oh-oh....we haven't seen this one before!\n");
				break;
		}
	}
	read(dat,&length,2);
	if(length==0)
		my_details->user_name[0]='\0';
	read(dat,&my_details->user_name,length);
	read(dat,&length,2);
	if(length==0)
		my_details->nick_name[0]='\0';
	read(dat,&my_details->nick_name,length);
	pass_strings(dat,3,0,0);		/*step over, first name, last name and primary e-mail*/
	read(dat,&my_details->uin,4);
	lseek(dat,15,SEEK_CUR);			/*gender, country and age*/
	pass_strings(dat,6,0,12);		/*step over city, state, add. text, homepage, home phone, notes, zip code*/
	read(dat,&length,4);			/*number of phonebook entries*/
	for(;length>0;length--)
	{
		pass_strings(dat,4,0,2);
		pass_strings(dat,1,0,0);
	}
	lseek(dat,14,SEEK_CUR);			/*unused and timestamp of my details*/
	pass_strings(dat,2,0,18);		/*secondary and old e-mail, birth data, languages spoken*/
	pass_strings(dat,3,0,4);		/*home address, fax and cellular*/
	pass_strings(dat,1,0,5);		/*company, occupation*/
	pass_strings(dat,5,0,8);		/*company position name and address, company zip and country*/
	pass_strings(dat,4,0,2);		/*work phone fax and URL and past bkg*/
	pass_strings(dat,1,0,2);		/*past bkg 2*/
	pass_strings(dat,1,0,2);		/*past bkg 3*/
	pass_strings(dat,1,0,2);		/*Affiliation*/
	pass_strings(dat,1,0,2);		/*Affiliation 2*/
	pass_strings(dat,1,0,22);		/*Affiliation 3*/
	pass_strings(dat,1,0,2);		/*interest keyword*/
	pass_strings(dat,1,0,2);		/*interest keyword 2*/
	pass_strings(dat,1,0,2);		/*interest keyword 3*/
	pass_strings(dat,1,0,42);		/*interest keyword 4*/

}
static gint icq_get_groups(int idx, int dat,struct groups group_array[], struct my_details * my_details)
{
	struct idxEntry entry={0,0,0,0,0};
	guint32	length=0;
	guint16	i, group_length;

	if(!find_idx_entry(idx,&entry, MY_DETAILS,FIRST_IDX))
	{
		eb_debug(DBG_MOD,"Can't find my details\n");
		return 0;
	}
	lseek(dat,entry.dat_offset,SEEK_SET);
	lseek(dat,12,SEEK_CUR);
	read(dat,&length,1);
	if(length!=0xe4)
		return 0;
	
	lseek(dat,29,SEEK_CUR);
	parse_my_details(dat,my_details);
	pass_strings(dat,1,0,18);		/*PASSWORD!*/
	pass_strings(dat,3,0,21);		/*???*/
	read(dat,&length,4);
	for(i=0;length>0;length--)
	{
		read(dat,&group_array[i].number,4);
		read(dat,&group_length,2);
		read(dat,group_array[i].name,group_length);
		lseek(dat,6,SEEK_CUR);			/*unused and open/closed flag*/
		i++;
	}
	group_array[i].number=IGNORE_FOLDER;
	memcpy(group_array[i++].name,"Ignore\0",sizeof("Ignore\0"));
	group_array[i].number=LAST_FOLDER;
	group_array[i++].name[0]=0;
	return 1;	
}

char * match_group(struct groups groupies[], struct my_details * my_details)
{
	gint	i=0;
	while((groupies[i].number!=LAST_FOLDER) && (groupies[i].number!=my_details->folder))
		i++;
	return groupies[i].name;
}

guint32 get_contact(int idx, int dat,struct groups groups[], struct my_details * my_details, struct idxEntry *entry)
{
	gchar*	group_name;
	guint8		type,i=0;
	guint32	stored, folder, magic;
	
	if(my_details->uin==0)	
		find_idx_entry(idx,entry, USERS_DATA, FIRST_IDX);
	else
		find_idx_entry(idx,entry, USERS_DATA, NEXT_IDX);
	while(entry->next!=-1)
	{
		lseek(dat,entry->dat_offset,SEEK_SET);
		lseek(dat,4,SEEK_CUR);
		read(dat,&stored,4);
		if((stored==1) || (stored==2))
		{
			lseek(dat,4,SEEK_CUR);
				read(dat,&type,1);
			if(type==0xe5)
			{
				lseek(dat,21,SEEK_CUR);
				read(dat,&magic,4);					/*status flag, 5==deleted???*/
				if((magic==2) || (magic==3) || (magic==12))
				{
					read(dat,&folder,4);
					my_details->folder=((stored==1)?folder:IGNORE_FOLDER);
					parse_my_details(dat,my_details);
					while((groups[i].number!=LAST_FOLDER) && (groups[i].number!=my_details->folder))
						i++;
	
					group_name=groups[i].name;
					i=0;
					while(group_name!=0 && i<30)
						my_details->group[i++]=*group_name++;
					my_details->group[i++]=0;
				return 1;
				}	
			}
		}
		find_idx_entry(idx,entry, USERS_DATA,NEXT_IDX);
	}
	return entry->next;
}	


void import_icq99_ok( GtkWidget  *w,GtkFileSelection *fs )
{
    char * selected, *fileext, uin[11];
    gint	dat,idx;
	struct groups *groups_ptr;
	struct	my_details contact;
	struct idxEntry entry={0,0,0,0,0};
	eb_account * ea;
	gint ICQ_ID=-1;

     ICQ_ID=get_service_id("ICQ");
    // Is there an ICQ service loaded?
    if(ICQ_ID < 0) {
        return;
    }

     selected=gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));

     fileext=strrchr(selected,'.');
     if(*(fileext+4)!=0)
          return;
     memcpy(fileext,".idx",4);
    if( !(idx = open(selected, O_RDONLY)) )
        return;
     memcpy(fileext,".dat",4);
	if( !(dat = open(selected, O_RDONLY)) )
        return;
	groups_ptr=g_malloc(sizeof(groups)*50);
	icq_get_groups(idx, dat, groups_ptr, &contact);
	contact.uin=0;
	while(get_contact(idx, dat, groups_ptr, &contact, &entry)!=-1)
	{
		g_snprintf(uin,11,"%i",contact.uin);
		if(!find_grouplist_by_name(contact.group))
			add_group(contact.group);
		if(find_account_by_handle(uin, ICQ_ID))
			continue;
        if((!find_contact_by_nick(contact.nick_name)) && (!find_contact_by_nick(contact.user_name)))
		{
			if(contact.nick_name[0]!=0)
				add_new_contact(contact.group, contact.nick_name, ICQ_ID );
			else
			{
				if(contact.user_name[0]==0)
				     memcpy(contact.user_name,"NoName",7);					
				add_new_contact(contact.group, contact.user_name, ICQ_ID );
			}
		}
		ea = eb_services[ICQ_ID].sc->new_account(NULL,uin);

		if(find_contact_by_nick(contact.user_name))
               add_account( contact.user_name, ea );
          else
               add_account( contact.nick_name, ea );

//          RUN_SERVICE(ea)->add_user(ea);
     }
	update_contact_list ();
	write_contact_list();
	g_free(groups_ptr);
	close(idx);
	close(dat);
	gtk_widget_destroy(GTK_WIDGET(fs));
}
void import_icq99_contacts(ebmCallbackData *data)
{
     GtkWidget *filew;

     filew = gtk_file_selection_new ("ICQ99 IDX file to import");

     gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                              "clicked", (GtkSignalFunc) import_icq99_ok, filew );

     gtk_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(filew)->cancel_button),
          "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));

     gtk_widget_show(filew);
}

