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
#include <stdlib.h>

#include "globals.h"
#include "input_list.h"
#include "value_pair.h"
#include "dialog.h"
#ifdef __MINGW32__
#define snprintf _snprintf
#endif


void eb_input_render(input_list * il, void * box)
{
	/* XXX: for backwards compatibility remove later */
	char *item_label=NULL;

	if(!il)
	{
		return;
	}

	/* XXX: for backwards compatibility change later */
	switch(il->type)
	{
		case EB_INPUT_CHECKBOX:
			if(il->widget.checkbox.label)
				item_label = il->widget.checkbox.label;
			else
				item_label = il->widget.checkbox.name;

			break;
		case EB_INPUT_ENTRY:
			if(il->widget.entry.label)
				item_label = il->widget.entry.label;
			else
				item_label = il->widget.entry.name;

			break;
	}
	/* End b/w comp code */

	switch(il->type)
	{
		case EB_INPUT_CHECKBOX:
			{
				eb_button(item_label, /* XXX */
						il->widget.checkbox.value,
						box);
				il->widget.checkbox.saved_value =
					*(il->widget.checkbox.value);
			}
			break;
		case EB_INPUT_ENTRY:
			{
				GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
				GtkWidget * widget = gtk_label_new(item_label); /* XXX */
				gtk_widget_set_usize(widget, 100, 15);
				gtk_box_pack_start(GTK_BOX(hbox),
						widget, FALSE, FALSE, 0);
				gtk_widget_show(widget);

				widget = gtk_entry_new();
				il->widget.entry.entry = widget;
				gtk_entry_set_text(GTK_ENTRY(widget),
						il->widget.entry.value);
				gtk_box_pack_start(GTK_BOX(hbox),
						widget, FALSE, FALSE, 0);
				gtk_widget_show(widget);

				gtk_box_pack_start(GTK_BOX(box), hbox,
						FALSE, FALSE, 0);
				gtk_widget_show(hbox);


			}
			break;
	}

	eb_input_render(il->next, box);
}

void eb_input_cancel(input_list * il)
{
	if(!il)
	{
		return;
	}

	switch(il->type)
	{
		case EB_INPUT_CHECKBOX:
			{
				*(il->widget.checkbox.value)
					= il->widget.checkbox.saved_value;
			}
			break;
		case EB_INPUT_ENTRY:
			{
				GtkWidget * w = il->widget.entry.entry;
				char * text = il->widget.entry.value;
				gtk_entry_set_text(GTK_ENTRY(w), text);
			}
			break;
	}

	eb_input_cancel(il->next);
}

void eb_input_accept(input_list * il)
{
	if(!il)
	{
		return;
	}

	switch(il->type)
	{
		case EB_INPUT_CHECKBOX:
			{
			}
			break;
		case EB_INPUT_ENTRY:
			{
				GtkWidget * w = il->widget.entry.entry;
				char * text = gtk_entry_get_text(GTK_ENTRY(w));
				strncpy(il->widget.entry.value, text, MAX_PREF_LEN);
				gtk_entry_set_text(GTK_ENTRY(w), il->widget.entry.value);

			}
			break;
	}

	eb_input_accept(il->next);
}

LList *eb_input_to_value_pair(input_list * il)
{
	LList *vp=NULL;
	char key[MAX_PREF_NAME_LEN];
	char value[MAX_PREF_LEN];
	char *ptr=NULL;

	for(; il; il=il->next)
	{
		switch(il->type)
		{
			case EB_INPUT_CHECKBOX:
				{
					snprintf(key, sizeof(key), "%s", il->widget.checkbox.name);
					for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
						*ptr='_';
					snprintf(value, sizeof(value), "%i", *il->widget.checkbox.value);
					vp=value_pair_add(vp, key, value);
				}
				break;
			case EB_INPUT_ENTRY:
				{
					snprintf(key, sizeof(key), "%s", il->widget.entry.name);
					for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
						*ptr='_';
					vp=value_pair_add(vp, key, il->widget.entry.value);
				}
				break;
		}
	}
	return(vp);
}

void eb_update_from_value_pair(input_list *il, LList *vp)
{
	char key[MAX_PREF_NAME_LEN];
	char *value;
	char *ptr=NULL;

	if(!il || ! vp)
		return;
	for(; il; il=il->next)
	{
		switch(il->type)
		{
			case EB_INPUT_CHECKBOX:
				{
					if(!il->widget.checkbox.value) {
						eb_debug(DBG_CORE, "checkbox.value is NULL\n");
						break;
					}
					snprintf(key, sizeof(key), "%s", il->widget.checkbox.name);
					for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
						*ptr='_';
					value=value_pair_get_value(vp, key);

					/* XXX
					 * this block is to convert old prefs files to new format.
					 * this should be removed after about two or three versions
					 * have been released.  We're currently in 0.4.2
					 */
					if(!value && il->widget.checkbox.label) {
						snprintf(key, sizeof(key), "%s", il->widget.checkbox.label);
						for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
							*ptr='_';
						value=value_pair_get_value(vp, key);
					}

					/* Was there a matching entry? */
					if(value) {
						*il->widget.checkbox.value=atoi(value);
						g_free(value);
					}
				}
				break;
			case EB_INPUT_ENTRY:
				{
					if(!il->widget.entry.value) {
						eb_debug(DBG_CORE, "entry.value is NULL\n");
						break;
					}
					snprintf(key, sizeof(key), "%s", il->widget.entry.name);
					for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
						*ptr='_';
					value=value_pair_get_value(vp, key);
					
					/* XXX
					 * this block is to convert old prefs files to new format.
					 * this should be removed after about two or three versions
					 * have been released.  We're currently in 0.4.2
					 */
					if(!value && il->widget.entry.label) {
						snprintf(key, sizeof(key), "%s", il->widget.entry.label);
						for( ptr=strchr(key, ' '); ptr && *ptr; ptr=strchr(ptr, ' '))
							*ptr='_';
						value=value_pair_get_value(vp, key);
					}

					/* Was there a matching entry? */
					if(value) {
						strncpy(il->widget.entry.value, value, MAX_PREF_LEN);
						g_free(value);
					}
				}
				break;
		}
	}
}
