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
 * log_window.c
 */

#include "intl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>

#include "globals.h"
#include "gtk/gtk_eb_html.h"
#include "log_window.h"
#include "util.h"
#include "prefs.h"
#include "action.h"

#include "pixmaps/cancel.xpm"
#include "pixmaps/action.xpm"

#ifdef __MINGW32__
#define snprintf _snprintf
#endif

static void eb_log_show(log_window *lw);

int current_row=-1, last_pos=-1;
/* module private functions */
static void free_string_list(gpointer data, gpointer user_data)
{
  g_free(data);
}

static void free_gs_list(gpointer data, gpointer user_data)
{
  g_slist_foreach((GSList*)data, (GFunc)free_string_list, NULL);
  g_slist_free((GSList*)data);
}

static void create_html_widget(log_window* lw)
{  
  /* add in the html display */
  lw->html_display = ext_gtk_text_new(NULL, NULL);
  gtk_eb_html_init(EXT_GTK_TEXT(lw->html_display));

  /* need to add scrollbar to the html text box*/
  lw->html_scroller = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_usize(lw->html_scroller, 375, 150);
  gtk_container_add(GTK_CONTAINER(lw->html_scroller), lw->html_display);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lw->html_scroller), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  gtk_box_pack_end(GTK_BOX(lw->date_html_hbox), lw->html_scroller, TRUE, TRUE, 5);  

  gtk_widget_show(GTK_WIDGET(lw->html_scroller));
  gtk_widget_show(GTK_WIDGET(lw->html_display));  
}

/* sets the html widget to display the log text */
static void set_html_text(GSList* gl, log_window* lw)
{
  GSList *h;

  /* need to destroy the html widget, and add it again */
  if (lw->html_display != NULL)
     gtk_widget_destroy(GTK_WIDGET(lw->html_display));
  if (lw->html_scroller != NULL)
	 gtk_widget_destroy(GTK_WIDGET(lw->html_scroller));


  /* create the html stuff */
  create_html_widget(lw);
  
  ext_gtk_text_freeze(EXT_GTK_TEXT(lw->html_display)); /* cold snap */

  /* walk down the list and add the test */
  h = gl;  
  while (h != NULL) {
    if (h->data != NULL) 
	    log_parse_and_add(h->data, lw->html_display);
    h = g_slist_next(h);
  }

  ext_gtk_text_thaw(EXT_GTK_TEXT(lw->html_display)); /* and now, a warm front */

}

void eb_log_load_information(log_window* lw)
{
  gchar name_buffer[NAME_MAX];
  gchar read_buffer[4096];
  FILE* fp;
  gchar* p1, *p2;
  gchar date_buffer[128];
  gchar* p3[1];
  GSList* gl = NULL, *gl1 = NULL;
  char posbuf[128];
  gint idx;
  gboolean empty = TRUE;
  
  gtk_clist_freeze(GTK_CLIST(lw->date_list)); /* freeze, thaw?  So corny. */

  if (lw->remote) {
	  make_safe_filename(name_buffer, lw->remote->nick, lw->remote->group->name);
	  lw->filename = strdup(name_buffer);
  } else if (lw->filename)
	  strncpy(name_buffer, lw->filename, sizeof(name_buffer));
  else
	  return;
			  	
  if ( (fp = fopen(name_buffer, "r")) != NULL) {
    gl = g_slist_alloc();

    lw->fp = fp;
    
    while (fgets(read_buffer, 4096, fp) != NULL) {
      empty=FALSE;	    
      if ((p1 = strstr(read_buffer, _("Conversation started on "))) != NULL) 
	  {

	/* extract the date, I am not sure if this is portable (but it
	   works on my machine, so if someone wants to hack it, go ahead */
	p1 += strlen(_("Conversation started on "));

	/* if html tags are in the buffer, gotta strip the last two tags */
	if ((p2 = strstr(read_buffer, "</B>")) != NULL && p2 > p1) {
	  strncpy(date_buffer, p1, (p2 - p1) > 127 ? 127:(p2-p1));
	  date_buffer[(p2 - p1) > 127 ? 127:(p2-p1)] = '\0';
	} else {
          int len = (read_buffer + strlen(read_buffer)) - p1;
	  if (len > 127) len = 127;
	  if (len > 0) {
		  /* eww, c's string handling sucks */
		  strncpy(date_buffer, p1, len);
		  date_buffer[len - 1] = '\0';
	  }
	}
	
	p3[0] = date_buffer;
	idx = gtk_clist_append(GTK_CLIST(lw->date_list), p3);

	/* add new gslist entry */
	gl1 = g_slist_alloc();
	snprintf(posbuf, sizeof(posbuf), "%lu", ftell(fp));
	gl = g_slist_append(gl, gl1);	
	gl1 = g_slist_append(gl1, strdup(posbuf));
	eb_debug(DBG_CORE,"%s at %s\n", p3[0],posbuf);
	
	/* set the datapointer for the clist */
	gtk_clist_set_row_data(GTK_CLIST(lw->date_list), idx, gl1);
      } else if ((p1 = strstr(read_buffer, _("Conversation ended on "))) == NULL) {
	if (gl1 != NULL) {
		gl1 = g_slist_append(gl1, strdup(read_buffer));
	}
	else
	{
		char buff1[4096];
		p3[0] = _("Undated Log");

		strncpy(buff1, read_buffer, sizeof(buff1));
		idx = gtk_clist_append(GTK_CLIST(lw->date_list), p3);
		/* add new gslist entry */
		snprintf(posbuf, sizeof(posbuf), "%lu", ftell(fp));
		gl = g_slist_append(gl, gl1);	
		gl1 = g_slist_append(gl1, strdup(posbuf));
		eb_debug(DBG_CORE,"%s at %s\n", p3[0],posbuf);

		/* set the datapointer for the clist */
		gtk_clist_set_row_data(GTK_CLIST(lw->date_list), idx, gl1);
		gl1 = g_slist_append(gl1, strdup(buff1));
	}
      }
    }
    fclose(fp);
  } else {
    perror(name_buffer);
  }

  if (empty) {
    p3[0]=_("No log available");
    gtk_clist_append(GTK_CLIST(lw->date_list), p3);
  }	  
  lw->entries = gl;

  gtk_clist_thaw(GTK_CLIST(lw->date_list));

  eb_log_show(lw);  
}


/* Event handlers */
static void destroy_event(GtkWidget* widget, gpointer data)
{
  log_window* lw = (log_window*)data;
  
  g_slist_foreach(lw->entries, (GFunc)free_gs_list, NULL);

  if (lw->remote)
	  lw->remote->logwindow = NULL;
  
  if (lw->filename)
	  free(lw->filename);
  
  lw->window = NULL;
  g_free(lw);
}

static void close_log_window_callback(GtkWidget* button, gpointer data)
{
  log_window* lw = (log_window*)data;
  gtk_widget_destroy(lw->window);
}

static void action_callback(GtkWidget *widget, gpointer d)
{
	log_window* lw = (log_window*)d;
	log_info *li = g_new0(log_info, 1);
	
	li->filename = strdup(lw->filename);
	li->log_started = 0;
	li->fp = lw->fp;
	li->filepos = lw->filepos;
	conversation_action(li, FALSE);
}


static gboolean search_callback(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  int cur_pos=0,cur_log=current_row;
  log_window * lw = (log_window *)data;
  gboolean looped=FALSE;
  /* if user wants to search */
  if (event->keyval == GDK_Return) {
    gchar * text = gtk_editable_get_chars(GTK_EDITABLE(widget),0,-1);
    strip_html(text);
    if(!text || strlen(text) == 0)
       return TRUE;
       
    eb_debug(DBG_CORE,"search: begin at cur_log:%d\n",cur_log);
    
    while(!looped) {
      int total_length=0;
      GSList *c_slist, *s_list;
      /* get the selected conversation's log */
      s_list = (GSList*)gtk_clist_get_row_data(GTK_CLIST(lw->date_list), cur_log);
      if (!s_list) {
          /* end of the conversation's list, back to the beginning */
          s_list = (GSList*)gtk_clist_get_row_data(GTK_CLIST(lw->date_list), 0);
	  /*reset */
	  cur_log=cur_pos=current_row=last_pos=total_length=0;
	  eb_debug(DBG_CORE,"search:looped\n");
	  looped=TRUE;
      }
      if(!s_list) {
          eb_debug(DBG_CORE, "search: breaking\n");
          /* empty !*/
	  break;
      }
      
      /* keep a pointer to the beginning of the conversation 
       * as we'll move s_list pointer */
      c_slist = s_list;
      while(s_list && cur_pos <= last_pos) {
	  s_list=s_list->next;
	  if(s_list && s_list->data && cur_pos<last_pos) {
        	gchar *list_text = (gchar *)s_list->data;
		gchar *no_html_text = NULL;
		if (list_text) 
			no_html_text = strdup(list_text);
		if (no_html_text) {
			strip_html(no_html_text);
			total_length+=strlen((char*)no_html_text);
			free(no_html_text);
		}
	  	eb_debug(DBG_CORE,"search:adding to %d\n",total_length);
	  }
          if(s_list) cur_pos++;
      }
      while(s_list && s_list->data) {
             gchar *list_text = (gchar *)s_list->data;
	     gchar *no_html_text = NULL;
	     if (list_text) 
		     no_html_text = strdup(list_text);
	     if (no_html_text) 
		     strip_html(no_html_text);
	     /* found string ? */
	     if(no_html_text && strstr(no_html_text,text) != NULL) {
		   GtkEditable *editable;
		   /* go to the the found conversation */
                   gtk_clist_select_row(GTK_CLIST(lw->date_list),cur_log,0);
                   gtk_clist_moveto (GTK_CLIST(lw->date_list),
		                     cur_log,0,0,0);
                   /* update the conversation log */			     
	           set_html_text(c_slist,lw);
                   editable = GTK_EDITABLE (lw->html_display);

                   last_pos=cur_pos+1;
                   current_row=cur_log;
		   /* select the matching substring */
	  	   eb_debug(DBG_CORE, "search: total2=%d (%d,%s)\n",total_length, cur_log,strstr(no_html_text,text) );
		   gtk_editable_set_position ( editable,
		   			       total_length+(int)(strstr(no_html_text,text)-no_html_text));
		   gtk_editable_select_region( editable,
		   			       total_length+(int)(strstr(no_html_text,text)-no_html_text),
					       total_length+(int)(strstr(no_html_text,text)-no_html_text)+strlen(text));
		   total_length+=strlen(no_html_text);
		   if (text)
			   free(text);
	  	   return TRUE;
	     } else if (no_html_text)
		     total_length+=strlen(no_html_text);
	     if (no_html_text)
		     free(no_html_text);
	     /* next line */
	     s_list=s_list->next;
	     cur_pos++; 
	     last_pos = cur_pos;
      }
      /* next conversation */
      eb_debug(DBG_CORE,"search: next one\n");
      cur_log++;
      cur_pos=0;last_pos=0;
      
      current_row=cur_log;
    }
  if (text)
	  free(text);
    
  }
  return TRUE;
}

/* clist callbacks... about the only callback I'll be getting */
static void select_date_callback (GtkCList *clist, gint row, gint column, 
				  GdkEventButton *event, gpointer user_data)
{
  GSList* string_list;
  log_window* lw = (log_window*)user_data;

  string_list = (GSList*)gtk_clist_get_row_data(GTK_CLIST(clist), row);
  while (string_list && !string_list->data)
	  string_list = g_slist_next(string_list);
  if (string_list && string_list->data) {
	  lw->filepos = atol((char *)string_list->data);
	  eb_debug(DBG_CORE,"filepos now %lu\n",lw->filepos);
	  string_list = g_slist_next(string_list);
  } else 
	 eb_debug(DBG_CORE,"string_list->data null\n");

  
  set_html_text(string_list, lw);
  current_row = row; last_pos=0;
}

/* Window creation (let there be light!) */
log_window* eb_log_window_new(struct contact *rc)
{
  GtkWidget *iconwid0;
  GdkPixmap *icon0;
  GdkBitmap *mask;

  log_window* lw = g_new0(log_window, 1);
  char buffer[512];
  GtkWidget* vbox;
  GtkWidget* hbox;
  GtkWidget* button;
  GtkWidget* toolbar;
  GtkWidget* search_label;
  GtkWidget* search_entry;
  GtkWidget* separator;
  lw->remote = rc;
  lw->filename = NULL;
  lw->fp = NULL;
  lw->filepos=0;
  
  if (rc)
	  rc->logwindow = lw;
  lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(lw->window), GTK_WIN_POS_MOUSE);
  /* show who the conversation is with */
  if (rc)
	  snprintf(buffer, sizeof(buffer), _("Past conversations with %s"), rc->nick);
  else 
	  snprintf(buffer, sizeof(buffer), _("Past conversation"));
  
  gtk_window_set_title(GTK_WINDOW(lw->window), buffer);
  gtk_container_set_border_width(GTK_CONTAINER(lw->window), 5);
  

  /* for the html and list box */
  lw->date_html_hbox = gtk_hbox_new(FALSE, 0);

  lw->date_list = gtk_clist_new(1);
  gtk_clist_set_column_width(GTK_CLIST(lw->date_list), 0, 100);
  gtk_clist_set_selection_mode(GTK_CLIST(lw->date_list), GTK_SELECTION_BROWSE);

  /* scroll window for the date list */
  lw->date_scroller = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_usize(lw->date_scroller, 180, 150);
  gtk_container_add(GTK_CONTAINER(lw->date_scroller), lw->date_list);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lw->date_scroller), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  
  /* let the packing begin (god that sounds wrong) */
  gtk_box_pack_start(GTK_BOX(lw->date_html_hbox), lw->date_scroller, TRUE, TRUE, 5);
  vbox = gtk_vbox_new(FALSE, 0);  
  gtk_box_pack_start(GTK_BOX(vbox), lw->date_html_hbox, TRUE, TRUE, 5);
  gtk_container_add(GTK_CONTAINER(lw->window), vbox);

  gtk_signal_connect(GTK_OBJECT(lw->window), "destroy",
		     GTK_SIGNAL_FUNC(destroy_event), lw);

  /* buttons toolbar */
  hbox = gtk_hbox_new(FALSE, 0);
  toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);

  /* need to realize the window for the pixmaps */
  gtk_widget_realize(GTK_WIDGET(lw->window));


  /* search field */
  search_label = gtk_label_new(_("Search: "));
  search_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), search_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), search_entry, TRUE, TRUE, 0);
  gtk_widget_show (search_label);
  gtk_widget_show (search_entry);
  gtk_signal_connect(GTK_OBJECT(search_entry), "key_press_event",
		     GTK_SIGNAL_FUNC(search_callback), lw);

  
  /* close button*/
#define TOOLBAR_APPEND(txt,icn,cbk,cwx) \
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),txt,txt,txt,icn,GTK_SIGNAL_FUNC(cbk),cwx); 
#define TOOLBAR_SEPARATOR() {\
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));\
	separator = gtk_vseparator_new();\
	gtk_widget_set_usize(GTK_WIDGET(separator), 0, 20);\
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar), separator, NULL, NULL);\
	gtk_widget_show(separator);\
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar)); }
#define ICON_CREATE(icon,iconwid,xpm) {\
	icon = gdk_pixmap_create_from_xpm_d(lw->window->window, &mask, NULL, xpm); \
	iconwid = gtk_pixmap_new(icon, mask); \
	gtk_widget_show(iconwid); }
	
  gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
  
  ICON_CREATE(icon0, iconwid0, action_xpm);
  button = TOOLBAR_APPEND(_("Actions..."), iconwid0, action_callback, lw);

  TOOLBAR_SEPARATOR();
  ICON_CREATE(icon0, iconwid0, cancel_xpm);
  button = TOOLBAR_APPEND(_("Close"), iconwid0, close_log_window_callback, lw);
  
  gtk_widget_show(GTK_WIDGET(button));
  gtk_widget_show(GTK_WIDGET(toolbar));

  gtk_box_pack_end(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  /* connect signals to the date list */
  gtk_signal_connect(GTK_OBJECT(lw->date_list), "select-row", 
		     GTK_SIGNAL_FUNC(select_date_callback), lw);

  gtk_widget_show(GTK_WIDGET(hbox));  
  gtk_widget_show(GTK_WIDGET(vbox));

  if (rc)
	  eb_log_load_information(lw);  

  return lw;
}

static void eb_log_show(log_window *lw) {
	
  /* show everything... can you get arrested for baring it all? */
  gtk_widget_show(GTK_WIDGET(lw->date_list));
  gtk_widget_show(GTK_WIDGET(lw->html_display));
  gtk_widget_show(GTK_WIDGET(lw->date_scroller));
  gtk_widget_show(GTK_WIDGET(lw->date_html_hbox));

  gtk_widget_show(GTK_WIDGET(lw->window));

  gtk_clist_select_row(GTK_CLIST(lw->date_list), 0, 0);
}

void log_parse_and_add(char *buff, void *text) 
{
	char *tmp = NULL;
	char *buff2 = NULL;
	/* ME */
	tmp = strdup(buff);
	if (!strncmp(tmp,"<P> <FONT COLOR=\"#", strlen("<P> <FONT COLOR=\"#")) 
	&& isdigit(tmp[strlen("<P> <FONT COLOR=\"#??????\"><B>1")])
	&& strstr(tmp, "/FONT> ")) {
		/* seems to be a beginning of line... */
		buff2 = strstr(tmp, "/FONT> ")+strlen("/FONT>");
		buff2[0] = '\0';
		buff2++;
		gtk_eb_html_add(EXT_GTK_TEXT(text), tmp,0,0,0);
		gtk_eb_html_add(EXT_GTK_TEXT(text), buff2,
				iGetLocalPref("do_ignore_back"),
				iGetLocalPref("do_ignore_fore"),
				iGetLocalPref("do_ignore_font"));
	} /* OTHER */
	else if (
		(!strncmp(tmp,"<P> <B><FONT COLOR=\"#", strlen("<P> <B><FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P> <B><FONT COLOR=\"#??????\">1")])
		&& strstr(tmp, "/FONT> </B> "))
	|| 	(!strncmp(tmp,"<P><B><FONT COLOR=\"#", strlen("<P><B><FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P><B><FONT COLOR=\"#??????\">1")])
		&& strstr(tmp, "/FONT> </B> "))
		) {
		/* seems to be a beginning of line... */
		buff2 = strstr(tmp, "/FONT> </B> ")+strlen("/FONT> </B>");
		buff2[0] = '\0';
		buff2++;
		gtk_eb_html_add(EXT_GTK_TEXT(text), tmp,0,0,0);
		gtk_eb_html_add(EXT_GTK_TEXT(text), buff2,
				iGetLocalPref("do_ignore_back"),
				iGetLocalPref("do_ignore_fore"),
				iGetLocalPref("do_ignore_font"));
	} else {
		gtk_eb_html_add(EXT_GTK_TEXT(text), tmp,0,0,0);
	}
	free(tmp);
}

void eb_log_message(log_info *loginfo, gchar buff[], gchar *message)
{
	gchar * my_name = strdup(buff);
	gchar * my_message = strdup(message);
	const int stripHTML = iGetLocalPref("do_strip_html");

	if (!loginfo || !loginfo->fp) {
		free(my_message);
		free(my_name);
		return;
	}
	
	if (stripHTML) {
		strip_html(my_name);
		strip_html(my_message);
	}

	if(!loginfo->log_started) {
		time_t my_time = time(NULL);
		fprintf(loginfo->fp, _("%sConversation started on %s %s\n"),
		      (stripHTML ? "" : "<HR WIDTH=\"100%\"><B>"),
		      g_strchomp(asctime(localtime(&my_time))), (stripHTML?"":"</B>"));
		fflush(loginfo->fp);
		loginfo->log_started = 1;
	}
	fprintf(loginfo->fp, "%s%s %s%s\n",
	  (stripHTML ? "" : "<P>"), my_name, my_message, (stripHTML ? "" : "</P>"));
	fflush(loginfo->fp);

	free(my_message);
	free(my_name);
}

void eb_log_close(log_info *loginfo)
{
	if(loginfo && loginfo->fp && loginfo->log_started) {
		if ( iGetLocalPref("do_logging") ) {
			time_t		my_time = time(NULL);
			const int	stripHTML = iGetLocalPref("do_strip_html");
			fprintf(loginfo->fp, _("%sConversation ended on %s %s\n"),
			(stripHTML ? "" : "<B>"),
			g_strchomp(asctime(localtime(&my_time))),
			(stripHTML ? "" : "</B>"));
		}
	}
	/*
	* close the log file
	*/

	if (loginfo && loginfo->fp != NULL ) {
		fclose(loginfo->fp);
		loginfo->fp = NULL;
	}
	
	if (loginfo && loginfo->filename != NULL ) {
		free(loginfo->filename);
		loginfo->filename = NULL;
	}
}
