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
 * log_window.h
 */
#ifndef __LOG_WINDOW_H
#define __LOG_WINDOW_H

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>

struct contact;

typedef struct _log_window
{
  GtkWidget* window;
  GtkWidget* date_list;
  GtkWidget* date_scroller;
  GtkWidget* html_display;
  GtkWidget* html_scroller;
  GtkWidget* date_html_hbox;

  GtkWidget* close_button;
  struct contact* remote;
  char *filename;
  GSList* entries;   /* list of gslists */
} log_window;

typedef struct _log_info
{
  FILE *fp;
  char *filename;
  int log_started;
} log_info;

//log_window* eb_log_window_new(struct _chat_window* cw);
log_window* eb_log_window_new(struct contact* rc);
void log_parse_and_add(char *buff, void *text);
void eb_log_load_information(log_window* lw);
void eb_log_message(log_info *loginfo, gchar buff[], gchar *message);
void eb_log_close(log_info *loginfo);
#endif
