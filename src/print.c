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

#include <errno.h>
#include <unistd.h>
#include "print.h"
#include "dialog.h"
#include "prefs_window.h"

void print_do_print(char * value, void * data)
{
	char filename[255];
	char *begin = NULL, *end = NULL, *cmd = NULL;
	snprintf(filename, 255, "%stmp_print", config_dir);

	if (strstr(value, "%s") == NULL) {
		do_error_dialog(_("Print command line is invalid (missing %s)."), _("Print error"));
		unlink(filename);
		return;
	}

	begin = g_strndup(value, (int)(strstr(value, "%s") - value));
	end   = strstr(value, "%s")+2; /* don't free */
	cmd = g_strdup_printf("%s%s%s", begin, filename, end);
	eb_debug(DBG_CORE, "print command: %s\n",cmd);
	
	system(cmd);
	
	cSetLocalPref("print_cmd", value);
	write_prefs();
	
	g_free(begin);
	g_free(cmd);
	
	unlink(filename);
}

void print_conversation(log_info *li)
{
	char buf[4096], output_fname[255];
	int firstline = 1;
	print_window *pw = g_new0(print_window, 1);
	FILE *output_file;

	if (!li || !li->filename)
		return;
	pw->loginfo = g_new0(log_info, 1);
	pw->loginfo->filename = strdup(li->filename);
	pw->loginfo->log_started = li->log_started;
	pw->loginfo->filepos = li->filepos;
	
	eb_debug(DBG_CORE,"printing %s (starting from %lu)\n", 
			pw->loginfo->filename, 
			pw->loginfo->filepos); 
	
	pw->loginfo->fp = fopen(pw->loginfo->filename, "r");
	if (!pw->loginfo->fp) {
		do_error_dialog(_("Cannot print log: no data available."),_("Print error"));
		return;
	}
	if (fseek(pw->loginfo->fp, pw->loginfo->filepos, SEEK_SET) < 0) {
		do_error_dialog(_("Cannot print log: no data available."),_("Print error"));
		return;
	}
	
	snprintf(output_fname, 255, "%stmp_print", config_dir);
	output_file = fopen(output_fname, "w");
	if (output_file == NULL) {
		perror("fopen");
		do_error_dialog(_("Cannot print log: Impossible to create temporary file."),_("Print error"));
		fclose(pw->loginfo->fp);
		return;
	}
	
	fprintf(output_file, "<html><head><title>Ayttm conversation</title></head><body>\n");
	while (fgets(buf, sizeof(buf), pw->loginfo->fp)) {
		if (strstr(buf, _("Conversation ended on ")) != NULL)
			break;
		if (strstr(buf, _("Conversation started on ")) != NULL) {
			if (firstline) {
				firstline = 0;
				continue;
			} else {
				break;
			}
		}
		eb_debug(DBG_CORE,"=>%s\n",buf);
		fprintf(output_file, "%s",buf);
		firstline = 0;
	}
	
	if (errno) {
		do_error_dialog(strerror(errno), _("Print error"));
		fclose(pw->loginfo->fp);
		fclose(output_file);
		unlink(output_fname);
		return;
	}
	
	fprintf(output_file, "</body></html>\n");
	fclose(output_file);
	if (ftell(pw->loginfo->fp) == pw->loginfo->filepos) {
		do_error_dialog(_("Cannot print log: no data available."),_("Print error"));
		fclose(output_file);
		unlink(output_fname);
	} else	
		do_text_input_window_multiline(_("Enter print command:\n(%s will be the current conversation's temporary file)"), 
			     cGetLocalPref("print_cmd"), FALSE, print_do_print, NULL); 
	fclose(pw->loginfo->fp);
}
