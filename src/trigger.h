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

/*
 * trigger.h
 */

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

typedef enum { NO_TYPE, USER_ONLINE, USER_OFFLINE,
		USER_ON_OFF_LINE } trigger_type;
typedef enum { NO_ACTION, PLAY_SOUND, EXECUTE, DIALOG, POUNCE } trigger_action;

typedef struct _trigger {
	trigger_type type;
	trigger_action action;
	char param[1024];
} trigger_struct;

/* forward declaration */
struct contact;

void destroy_window();

trigger_type get_trigger_type_num(const char *text);
trigger_action get_trigger_action_num(const char *text);

char *get_trigger_type_text(trigger_type type);
char *get_trigger_action_text(trigger_action action);

void show_trigger_window();
void do_trigger_online(struct contact *con);
void do_trigger_offline(struct contact *con);
void do_trigger_action(struct contact *con, int trigger_type);

#endif
