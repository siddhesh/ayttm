/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 2003, Colin Leroy <colin@colino.net>
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
 
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "globals.h"
#include "action.h"
#include "dialog.h"
#include "prefs.h"
#include "mem_util.h"
#include "util.h"

#ifdef __MINGW32__
#define snprintf _snprintf
#endif

static void create_action_menu(char *html_file, char *plain_file, int sens);
static void save_action(char *action);

static void action_do_action(char * value, void * data)
{
#ifndef __MINGW32__
	char *filename_html;
	char *filename_plain;
	char **files = data;
	char *begin = NULL, *end = NULL, *cmd = NULL, *tvalue = NULL;
	int found = FALSE;
	pid_t child;
	
	filename_html = files[0];
	filename_plain = files[1];
	
	tvalue = strdup(value);
	if (strstr(tvalue, "%s") != NULL) {
		begin = g_strndup(tvalue, (int)(strstr(tvalue, "%s") - tvalue));
		end   = strstr(tvalue, "%s")+2; /* don't free */
		cmd = g_strdup_printf("%s%s%s", begin, filename_html, end);
		eb_debug(DBG_CORE, "action command: %s\n",cmd);	
		
		/* replace tvalue */
		free(tvalue);
		tvalue = strdup(cmd);
		
		found = TRUE;
	} 
	if (strstr(tvalue, "%p") != NULL) {
		begin = g_strndup(tvalue, (int)(strstr(tvalue, "%p") - tvalue));
		end   = strstr(tvalue, "%p")+2; /* don't free */
		if (found) /* free last cmd */
			g_free(cmd);
		cmd = g_strdup_printf("%s%s%s", begin, filename_plain, end);
		eb_debug(DBG_CORE, "action command: %s\n",cmd);	
		found = TRUE;
	} 
	
	free(filename_html);
	free(filename_plain);
	
	if (!found) {
		do_error_dialog(_("Command line is invalid (missing %s and %p)."), _("Action error"));
		return;
	}

	child = fork();
	if (child == 0) {
		/* in child */
		system(cmd);
		g_free(cmd);
		_exit(0);
	} else if (child > 0) {
		save_action(value);
		g_free(begin);
	} else {
		do_error_dialog(_("Cannot run command : fork() failed."), _("Action error"));
		perror("fork");
	}		
#endif
}

static void action_prepare(GtkWidget *w, void *data)
{
	char *val = gtk_widget_get_name(w);

	action_do_action(val, data);
}

void conversation_action(log_info *li, int to_end)
{
#ifndef __MINGW32__
	char buf[4096], *output_html, *output_plain;
	int firstline = 1;
	char *tempdir = NULL;
	log_info *loginfo = g_new0(log_info, 1);
	FILE *output_fhtml;
	FILE *output_fplain;

	if (!li || !li->filename)
		return;
	loginfo = g_new0(log_info, 1);
	loginfo->filename = strdup(li->filename);
	loginfo->log_started = li->log_started;
	loginfo->filepos = li->filepos;
	
	eb_debug(DBG_CORE,"printing %s (starting from %lu)\n", 
			loginfo->filename, 
			loginfo->filepos); 
	
	loginfo->fp = fopen(loginfo->filename, "r");
	
	free (loginfo->filename);
	
	if (!loginfo->fp) {
		do_error_dialog(_("Cannot use log: no logfile available."),_("Action error"));
		return;
	}
	if (fseek(loginfo->fp, loginfo->filepos, SEEK_SET) < 0) {
		do_error_dialog(_("Cannot use log: logfile too short (!?)."),_("Action error"));
		return;
	}
	
	tempdir = getenv("TEMP");
	if (!tempdir)
		tempdir = getenv("TMP");
	if (!tempdir)
		tempdir = "/tmp";
	
	output_html = g_strdup_printf("%s/tmp_htmlXXXXXX", tempdir);
	output_plain = g_strdup_printf("%s/tmp_plainXXXXXX", tempdir);
	close(mkstemp(output_html));
	close(mkstemp(output_plain));
	
	snprintf(buf, 4096, "%s.html", output_html);
	rename(output_html, buf);
	g_free(output_html);
	output_html = g_strdup(buf);
	
	snprintf(buf, 4096, "%s.txt", output_plain);
	rename(output_plain, buf);
	g_free(output_plain);
	output_plain = g_strdup(buf);
	
	output_fhtml = fopen(output_html, "w");
	output_fplain = fopen(output_plain, "w");
	
	if (output_fhtml == NULL || output_fplain == NULL) {
		perror("fopen");
		printf(" %s %s\n", output_html, output_plain);
		do_error_dialog(_("Cannot use log: Impossible to create temporary file."),_("Action error"));
		fclose(loginfo->fp);
		return;
	}
	
	fprintf(output_fhtml, "<html><head><title>Ayttm conversation</title></head><body>\n");
	
	while (fgets(buf, sizeof(buf), loginfo->fp)) {
		char *smil = NULL;
		/* smiley tags won't be so long */
		char *bhide = "<!--", *ehide = "-->";
		char *txt = NULL;
		int addbr = FALSE;
		
		if (!to_end && strstr(buf, _("Conversation ended on ")) != NULL)
			break;
		if (strstr(buf, _("Conversation started on ")) != NULL) {
			if (firstline) {
				firstline = 0;
			} else if (!to_end && !firstline) {
				break;
			}
		}
		while ( (smil = strstr(buf, "<smiley name=")) != NULL) {
			char *alt = strstr(smil, "\" alt=\"");
			
			if (alt) {
				alt+=4;
			} else {
				break; /* not normal ... */
			}
			strncpy(smil, bhide, strlen(bhide));
			strncpy(alt, ehide, strlen(ehide));

			smil = alt + strlen(ehide);
			alt = strstr(smil, "\">");
			if (alt)
				strncpy(alt, "  ", 2);
		}
		if (!strncmp(buf,"<P>",3)) {
			char *alt = strstr(buf, "</P>");
			if ((alt+5-buf) == strlen(buf)) {
				/* replace <P>...</P> by ...<BR> */
				strncpy(buf, "   ", 3);
				strncpy(alt, "<BR>", 4);
			}
		} else if (!strncmp(buf,"<HR WIDTH", 9)) {
			/* add <BR> */
			addbr = TRUE;
		}
		
		eb_debug(DBG_CORE,"=>%s\n",buf);
		fprintf(output_fhtml, "%s",buf);
		if (addbr)
			fprintf(output_fhtml, "<br>");
		
		strip_html(buf);
		
		txt = buf;
		while (txt && *txt==' ')
			txt++;
		fprintf(output_fplain, "%s",txt);
		
		firstline = 0;
	}
	
	if (errno) {
		do_error_dialog(strerror(errno), _("Action error"));
		fclose(loginfo->fp);
		fclose(output_fhtml);
		fclose(output_fplain);
		return;
	}
	
	fprintf(output_fhtml, "</body></html>\n");
	fclose(output_fhtml);
	fclose(output_fplain);
	
	if (ftell(loginfo->fp) == loginfo->filepos) {
		create_action_menu(output_html, output_plain, FALSE);
	} else	{
		create_action_menu(output_html, output_plain, TRUE);
	}
	g_free(output_html);
	g_free(output_plain);
	fclose(loginfo->fp);
#endif
}

static void add_command_cb(GtkWidget * w, void * a)
{
		do_text_input_window_multiline(
			_("Enter command:\n"
			  " %s = displayed conversation's file (as HTML)\n"
			  " %p = displayed conversation's file (as text)"), 
			"", FALSE, action_do_action, a); 
}


static LList *load_actions(void)
{
	FILE *in;
	char fname[255], buf[1024];
	LList *list = NULL;
	
	snprintf(fname, 255, "%sactions", config_dir);
	in = fopen(fname, "r");
	if (!in)
		return NULL;
	
	while (fgets(buf, sizeof(buf), in)) {
		char *val = strdup(buf);
		val[strlen(val)-1] = '\0';
		
		list = l_list_append(list, val);
	}
	
	fclose(in);
	return list;
}

static void save_action(char *action)
{
	FILE *in;
	char fname[255], buf[1024];
	
	snprintf(fname, 255, "%sactions", config_dir);
	in = fopen(fname, "r");
	if (in) {
		while (fgets(buf, sizeof(buf), in)) {
			if (!strncmp(action, buf, strlen(buf)-1)) {
				/* already exists */
				fclose(in);
				return;
			}
		}
		fclose(in);
	}
	
	in = fopen(fname, "a");
	fprintf(in, "%s\n",action);
	fclose(in);	
}

static void create_action_menu(char *html_file, char *plain_file, int sens)
{
	GtkWidget *menu = NULL;
	GtkWidget *item = NULL;
	LList *commands = NULL, *walk = NULL;
	char **files = ay_new(char *, 2);
	
	files[0] = strdup(html_file);	/* free after callback */
	files[1] = strdup(plain_file);
	
	menu = gtk_menu_new();
	item = eb_menu_button (GTK_MENU(menu), _("New command..."),
			GTK_SIGNAL_FUNC(add_command_cb), files);
	gtk_widget_set_sensitive(item, sens); /* don't set the whole menu unsensitive */

	commands = load_actions();
	
	if (l_list_empty(commands)) {
		/* create the print command so people will understand... */
		
		save_action("html2ps -U %s | lpr");
		commands = load_actions();
	}
	
	if (!l_list_empty(commands)) {
		eb_menu_button (GTK_MENU(menu), NULL, NULL, NULL); /* sep */
		walk = commands;
		while (walk) {
			item = eb_menu_button(GTK_MENU(menu), walk->data, 
					GTK_SIGNAL_FUNC(action_prepare), files);
			gtk_widget_set_name(item, walk->data);
			gtk_widget_set_sensitive(item, sens);
			walk = walk->next;
		}
	}	
	
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 1, 0 );
	l_list_free(commands);
}
