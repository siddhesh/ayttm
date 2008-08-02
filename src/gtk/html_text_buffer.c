
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

#define _GNU_SOURCE

#include "html_text_buffer.h"
#include <stdlib.h>
#include <ctype.h>
#include "prefs.h"
#include <string.h>
#include "smileys.h"
#include "browser.h"

#include "pixmaps/no_such_smiley.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/free_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/admin_icon.xpm"

/* This will help to give unique tag names */
int messageid=0;

/*
 * This is our typical tag. It has...
 */
typedef struct {
	char id[8];		/* A unique identifier... */
	char *name;		/* A Name along with its attributes... */
	GtkTextMark start;	/* A marker indicating its start in the buffer... */
	GtkTextMark end;	/* And a marker indicating its end in the buffer */
} tag;


/* 
 * Our own strcasestr since we don't have a standard strcasestr. The strcasestr is a
 * non-standard extension of strstr. Somebody tell me if a standard extension comes up.
 */
static char *ay_strcasestr(const char *haystack, const char *needle)
{
	int start=0, iter=0;
	
	int needle_len = strlen(needle);
	int haystack_len = strlen(haystack) - needle_len;

	for( start=0; start<haystack_len; start++ ) {
		for(iter=0; iter<needle_len; iter++) {
			if( g_ascii_tolower(haystack[start+iter]) != g_ascii_tolower(needle[iter]) )
				break;
		}

		if ( iter == needle_len )
			return ((char *)haystack+start);
	}

	return NULL;
}

/*
 * Crude eh...
 */
gboolean tag_is_valid(char *tag_string)
{
	if( !g_ascii_strncasecmp(tag_string, "head", strlen("head")) 
	    || !g_ascii_strncasecmp(tag_string, "hr", strlen("hr")) 
	    || !g_ascii_strncasecmp(tag_string, "br", strlen("br")) 
	    || !g_ascii_strncasecmp(tag_string, "body", strlen("body")) 
	    || !g_ascii_strcasecmp(tag_string, "p") 
	    || !g_ascii_strcasecmp(tag_string, "html") 
	    || !g_ascii_strcasecmp(tag_string, "title") 
	    || !g_ascii_strcasecmp(tag_string, "pre") 
	    || !g_ascii_strcasecmp(tag_string, "b") 
	    || !g_ascii_strcasecmp(tag_string, "u") 
	    || !g_ascii_strcasecmp(tag_string, "i") 
	    || !g_ascii_strncasecmp(tag_string, "font", strlen("font")) 
	    || !g_ascii_strncasecmp(tag_string, "a ", strlen("a ")) 
	    || !g_ascii_strcasecmp(tag_string, "a")
	    || !g_ascii_strncasecmp(tag_string, "smiley", strlen("smiley")) 
	    || !g_ascii_strncasecmp(tag_string, "img", strlen("img")) )
	{
		return TRUE;
	}

	return FALSE;
}


/*
 * From the gtk_eb_html code
 */
static void _extract_parameter (char *param, char *buffer, int buffer_size)
{
	/*
	 * if we start with a quote then we must end with a quote
	 * otherwise we end with a space
	 */

	if(*param == '\"') {
		int cnt = 0;
		param += 1;
		
		for(cnt = 0; *param && *param != '\"' && cnt< buffer_size-1;
				cnt++, param++ )
		{
			buffer[cnt] = *param;
		}
		buffer[cnt] = '\0';
	}
	else {
		int cnt = 0;
		
		for(cnt = 0; *param && !isspace(*param) && *param != '>' && cnt< buffer_size-1;
				cnt++, param++ )
		{
			buffer[cnt] = *param;
		}
		buffer[cnt] = '\0';
	}
}

/*
 * Inserts an xpm image into a textbuffer at the given iter
 */
void insert_xpm_at_iter(GtkTextView *text_view, GtkTextIter *start, char **xpm)
{
	GtkTextBuffer		*buffer = gtk_text_view_get_buffer(text_view);
	GdkPixbuf		*icon = gdk_pixbuf_new_from_xpm_data( (const char **) xpm);
	GtkWidget		*pixmap = gtk_image_new_from_pixbuf(icon);
	GtkTextChildAnchor	*anchor = gtk_text_buffer_create_child_anchor(buffer, start);

	gtk_widget_show(pixmap);

	gtk_text_view_add_child_at_anchor(text_view, pixmap, anchor);
}

/*
 * Apply styles to the string based on the tags
 */
gboolean apply_tag (GtkTextView *text_view, tag in_tag, int ignore)
{
	/*
	 * TODO: Create GTK_TAGs for the following tags:
	 * 2) body: All done except for the width=100% portion
	 */
	
	GtkTextTag *style;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

	/* HEAD */
	if( !g_ascii_strncasecmp(in_tag.name, "head", strlen("head")) ) {

		GtkTextIter start, end;
		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));
		
		gtk_text_buffer_delete(buffer, &start, &end);
		
		return TRUE;
	}
	
	/* HORIZONTAL LINE */
	if( !g_ascii_strncasecmp(in_tag.name, "hr", strlen("hr")) ) {
		GtkTextIter start;
		GtkWidget *line;
		GtkTextChildAnchor *anchor;
		GtkRequisition r;

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_insert(buffer, &start, "\n", strlen("\n"));

		/* Reinitialized since the insert invalidates the iter */
		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_iter_forward_line(&start);
		
		line = gtk_hseparator_new();

		anchor = gtk_text_buffer_create_child_anchor(buffer, &start);
		gtk_text_view_add_child_at_anchor(text_view, line, anchor);

		/* FIXME: Need a way to stretch the HR on resize */
		gtk_widget_size_request(GTK_WIDGET(text_view), &r);
		gtk_widget_set_size_request(line, r.width, 2);

		gtk_widget_show(line);
		
		return TRUE;
	}
	
	/* BR */
	if( !g_ascii_strncasecmp(in_tag.name, "br", strlen("br")) ) {

		GtkTextIter start;
		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_insert(buffer, &start, "\n", strlen("\n"));
		
		return TRUE;
	}

	/* BODY */
	if( !g_ascii_strncasecmp(in_tag.name, "body", strlen("body")) ) {
		char *param = NULL;
		char attr_val[255];
		GtkTextIter start, end;

		bzero(attr_val, 255);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));
		
		if( (param = ay_strcasestr(in_tag.name, "bgcolor="))!=NULL )
		{
			param+=strlen("bgcolor=");
			_extract_parameter(param, attr_val, 255);
			style = gtk_text_buffer_create_tag(buffer, in_tag.id, 
					"background", attr_val,
					NULL);

			gtk_text_buffer_apply_tag(buffer, style, &start,&end);
		}
		
		return TRUE;
	}

	/* BOLD */
	if( !g_ascii_strcasecmp(in_tag.name, "b") ) {
		GtkTextIter start, end;

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));

		style = gtk_text_buffer_create_tag(buffer, in_tag.id, 
				"weight", PANGO_WEIGHT_BOLD,
				NULL);
		gtk_text_buffer_apply_tag(buffer, style, &start,&end);
		return TRUE;
	}

	/* UNDERLINE */
	if( !g_ascii_strcasecmp(in_tag.name, "u") ) {
		GtkTextIter start, end;

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));

		style = gtk_text_buffer_create_tag(buffer, in_tag.id, 
				"underline", PANGO_UNDERLINE_SINGLE,
				NULL);
		gtk_text_buffer_apply_tag(buffer, style, &start,&end);
		return TRUE;
	}

	/* ITALICS */
	if( !g_ascii_strcasecmp(in_tag.name, "i") ) {
		GtkTextIter start, end;

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));

		style = gtk_text_buffer_create_tag(buffer, in_tag.id, 
				"style", PANGO_STYLE_ITALIC,
				NULL);
		gtk_text_buffer_apply_tag(buffer, style, &start,&end);
		return TRUE;
	}

	/* FONT */
	if( !g_ascii_strncasecmp(in_tag.name, "font", strlen("font")) ) {
		GtkTextIter start, end;
		char *param = NULL;
		char attr_val[255];

		bzero(attr_val, 255);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));
			
		style = gtk_text_buffer_create_tag(buffer, in_tag.id, NULL);

		/* Font Face */
		if( !(ignore & HTML_IGNORE_FONT) &&
				(param = ay_strcasestr(in_tag.name, "face="))!=NULL )
		{
			param+=strlen("face=");
			_extract_parameter(param, attr_val, 255);

			g_object_set(style, "family", attr_val, NULL);
		}

		/* Font color */
		if( !(ignore & HTML_IGNORE_FOREGROUND) &&
				(param = ay_strcasestr(in_tag.name, "color="))!=NULL )
		{
			param+=strlen("color=");
			_extract_parameter(param, attr_val, 255);
			
			g_object_set(style, "foreground", attr_val, NULL);
		}

		/* Font Size */
		if( (param = ay_strcasestr(in_tag.name, "ptsize="))!=NULL )
		{
			param+=strlen("ptsize=");
			_extract_parameter(param, attr_val, 255);

			g_object_set(style, "size-points", strtod(attr_val, NULL), NULL);
		}
		else if( (param = ay_strcasestr(in_tag.name, "absz="))!=NULL )
		{
			param+=strlen("absz=");
			_extract_parameter(param, attr_val, 255);

			g_object_set(style, "size-points", strtod(attr_val, NULL), NULL);
		}
		else if( (param = ay_strcasestr(in_tag.name, "size="))!=NULL )
		{
			/* Get the current font size */
			gint cur_size=0;
			PangoContext *context = gtk_widget_get_pango_context(
					GTK_WIDGET(text_view));
			PangoFontDescription *desc = pango_context_get_font_description(
					context);

			param+=strlen("size=");
			
			cur_size = pango_font_description_get_size(desc)/PANGO_SCALE;

			_extract_parameter(param, attr_val, 255);

			if(attr_val == NULL) {
				/* Do nothing */
			}
			else if(*attr_val=='+') {
				int font_size = atoi(attr_val+1)*4;
				g_object_set(style, "size-points",
						(double)cur_size+font_size, NULL);
			}
			else if(*attr_val=='-') {
				int font_size = atoi(attr_val+1)*4;
				g_object_set(style, "size-points",
						(double)cur_size-font_size, NULL);
			}
			else {
				int font_size = atoi(attr_val)*4;
				g_object_set(style, "size-points",
						(double)font_size, NULL);
			}

		}

		gtk_text_buffer_apply_tag(buffer, style, &start,&end);

		return TRUE;
	}

	/* ANCHOR */
	if( !g_ascii_strncasecmp(in_tag.name, "a ", strlen("a ")) ) {
		GtkTextIter start, end;
		char *param = NULL;
		gchar *attr_val = (gchar *)malloc(1024);

		bzero(attr_val, 1024);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));
		gtk_text_buffer_get_iter_at_mark(buffer, &end, &(in_tag.end));

		/* Font Face */
		if( (param = ay_strcasestr(in_tag.name, "href="))!=NULL )
		{
			param+=strlen("href=");
			_extract_parameter(param, attr_val, 1024);

			style = gtk_text_buffer_create_tag(buffer, in_tag.id, 
					"underline", PANGO_UNDERLINE_SINGLE,
					"foreground", "blue",
					NULL);
			g_object_set_data(G_OBJECT(style), "href", attr_val);
	
			gtk_text_buffer_apply_tag(buffer, style, &start,&end);
		}
		return TRUE;
	}

	
	/* SMILEY */
	if( !g_ascii_strncasecmp(in_tag.name, "smiley", strlen("smiley")) ) {
		GtkTextIter start;
		
		char *param = NULL;
		char smiley_name[64];
		char smiley_protocol[64];
		smiley *smile = NULL;
		bzero(smiley_name, 64);
		bzero(smiley_protocol, 64);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));

		if( (param = ay_strcasestr(in_tag.name, "name="))!=NULL ) {

			param+=strlen("name=");
			_extract_parameter(param, smiley_name, 64);

			if( (param = ay_strcasestr(in_tag.name, "protocol="))!=NULL ) {
				param+=strlen("protocol=");
				_extract_parameter(param, smiley_protocol, 64);
				smile = get_smiley_by_name_and_service(smiley_name,
						smiley_protocol);
			}
			else {
				smile = get_smiley_by_name(smiley_name);
			}

			if(smile) {
				insert_xpm_at_iter(text_view, &start, smile->pixmap);
			}
			else if( (param = ay_strcasestr(in_tag.name, "alt=")) != NULL ) {
				param+=strlen("alt=");
				_extract_parameter(param, smiley_name, 64);
				gtk_text_buffer_insert(buffer, &start,
						smiley_name, strlen(smiley_name));
			}
			else {
				insert_xpm_at_iter(text_view, &start, no_such_smiley_xpm);
			}
		}
		
		return TRUE;
	}
	
	/* IMAGE */
	if( !g_ascii_strncasecmp(in_tag.name, "img", strlen("img")) ) {
		GtkTextIter start;
		char *param = NULL;
		char img_loc[1024];
		
		bzero(img_loc, 1024);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, &(in_tag.start));

		if( (param = ay_strcasestr(in_tag.name, "src="))!=NULL ) {
			
			param+=strlen("src=");
			_extract_parameter(param, img_loc, 1024);
		
			if(!strcmp(img_loc, "aol_icon.gif")) 
				insert_xpm_at_iter(text_view, &start, aol_icon_xpm);
			else if(!strcmp(img_loc, "free_icon.gif")) 
				insert_xpm_at_iter(text_view, &start, free_icon_xpm);
			else if(!strcmp(img_loc, "dt_icon.gif")) 
				insert_xpm_at_iter(text_view, &start, dt_icon_xpm);
			else if(!strcmp(img_loc, "admin_icon.gif")) 
				insert_xpm_at_iter(text_view, &start, admin_icon_xpm);
		}
		
		return TRUE;
	}

	return FALSE;
}

/*
 * Callback for search_char
 */
gboolean find_tag_callback(gunichar ch, gpointer data)
{
	gchar *gch;
	gch=g_ucs4_to_utf8(&ch, 1, NULL, NULL, NULL);
	
	if(gch && *gch==*((gchar *)data) ) {
		return TRUE;
	}
	else
		return FALSE;
}

/*
 * Search for a character in the buffer using an iterator
 */
gboolean search_char(GtkTextIter *result_iter, gchar needle)
{
	if(find_tag_callback(gtk_text_iter_get_char(result_iter), &needle))
		return TRUE;
	
	return gtk_text_iter_forward_find_char(result_iter, find_tag_callback,
			&needle, NULL);
}

/*
 * Unescapes escape sequences for some characters like &, >, <
 */
void unescape_html(GtkTextBuffer *buffer, GtkTextMark html_start)
{
	GtkTextIter start, end;
	GtkTextMark *start_mark = NULL, *end_mark = NULL;
	int html_found = 0;

	gtk_text_buffer_get_iter_at_mark(buffer, &start, &html_start);
	gtk_text_buffer_get_iter_at_mark(buffer, &end, &html_start);

	while( search_char(&start, '&') &&
	       search_char(&end, ';') &&
	       gtk_text_iter_compare(&start, &end) < 0 )
	{
		gchar *code;

		gtk_text_iter_forward_char(&end);

		code = gtk_text_buffer_get_slice(buffer, &start, &end, TRUE );
		start_mark = gtk_text_buffer_create_mark(buffer, NULL, &start, TRUE);
		end_mark = gtk_text_buffer_create_mark(buffer, NULL, &end, TRUE);

		if(!g_ascii_strncasecmp(code, "&gt;", 4)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, ">", strlen(">"));
			html_found = 1;
		}
		if(!g_ascii_strncasecmp(code, "&lt;", 4)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, "<", strlen("<"));
			html_found = 1;
		}
		if(!g_ascii_strncasecmp(code, "&amp;", 5)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, "&", strlen("&"));
			html_found = 1;
		}
		if(!g_ascii_strncasecmp(code, "&#8212;", 7)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, "--", strlen("--"));
			html_found = 1;
		}
		if(!g_ascii_strncasecmp(code, "&nbsp;", 6)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, " ", strlen(" "));
			html_found = 1;
		}
		if(!g_ascii_strncasecmp(code, "&quot;", 6)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_insert(buffer, &start, "\"", strlen("\""));
			html_found = 1;
		}

		if(html_found) {
			gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
			gtk_text_buffer_get_iter_at_mark(buffer, &end, start_mark);
			gtk_text_iter_forward_char(&end);
		}
		
		gtk_text_iter_forward_char(&start);
	}
}


/*
 * The almighty html parser method
 */
void parse_html( GtkTextView *text_view, GtkTextMark html_start, int ignore )
{
	GtkTextBuffer *html_buffer = gtk_text_view_get_buffer(text_view);

	tag *last_tag;
	GList *tag_list = NULL;

	int tagid=0;
	GtkTextIter start_iter, end_iter;
	GtkTextMark *end_mark;
	GtkTextIter tag_start_iter, tag_end_iter;

	gtk_text_buffer_get_iter_at_mark(html_buffer, &start_iter, &html_start);
	gtk_text_buffer_get_end_iter(html_buffer, &end_iter);

	end_mark = gtk_text_buffer_create_mark(html_buffer, NULL, &end_iter, TRUE);
	
	gtk_text_buffer_get_iter_at_mark(html_buffer, &tag_start_iter, &html_start);
	gtk_text_buffer_get_iter_at_mark(html_buffer, &tag_end_iter, &html_start);

	/* Check if < and > exist in that order */
	while(  search_char(&tag_start_iter, '<') &&
		search_char(&tag_end_iter, '>') )
	{
		gchar *tag_string;
		GtkTextMark *tag_start_mark = NULL, *next_start_mark = NULL;

		if(gtk_text_iter_compare(&tag_start_iter, &tag_end_iter) > 0) {
			gtk_text_iter_forward_char(&tag_end_iter);
			tag_start_iter = tag_end_iter;
			continue;
		}

		gtk_text_iter_forward_char(&tag_end_iter);
		
		tag_start_mark = gtk_text_buffer_create_mark(html_buffer, NULL,
							     &tag_start_iter, TRUE);

		next_start_mark = gtk_text_buffer_create_mark(html_buffer, NULL,
							      &tag_end_iter, TRUE);
	
		tag_string=gtk_text_buffer_get_slice(html_buffer, &tag_start_iter,
				&tag_end_iter, TRUE );

		/* Get rid of the < and > and clean up the tag string */
		tag_string = strstr(tag_string, "<")+1;


		if( tag_string && strstr(tag_string, ">") )
			*(strstr(tag_string, ">")) = '\0';

		g_strstrip(tag_string);

		if( *tag_string == '/' && tag_is_valid(++tag_string) ) {
			int found_match=0;
			last_tag = NULL;
			
                        /* Now get rid of the tag from the text*/
			gtk_text_buffer_delete(html_buffer, &tag_start_iter, &tag_end_iter);

			/* 
			 * This is an end tag. So now we must apply the tag to
			 * the enclosed text
			 */
			do{
				last_tag = g_list_nth_data(g_list_last(tag_list), 0);
				if(last_tag == NULL)
					break;

				if(!g_ascii_strncasecmp(tag_string, last_tag->name, strlen(tag_string))) {
					last_tag->end = *tag_start_mark;
					found_match=1;
				} else {
					last_tag->end = *end_mark;
				}

				apply_tag(text_view, *last_tag, ignore );
				
				tag_list = g_list_remove(tag_list, last_tag);
			}
			while( !found_match );
		}
		else if (tag_is_valid(tag_string)) {
			tag *cur;
	
			/* Now get rid of the tag from the text*/
			gtk_text_buffer_delete(html_buffer, &tag_start_iter, &tag_end_iter);

			/* This is a start tag. So put this into the list */
			cur = (tag *)malloc(sizeof(tag));
			bzero(cur->id, 8);

			sprintf(cur->id, "%d%d", messageid, tagid++);
			cur->name = strdup(tag_string);
			cur->start = *tag_start_mark;
		
			/* 
			 * Insert into the tag list only if it's a
			 * closing type tag
			 */
			if( !(ay_strcasestr(tag_string, "smiley") == tag_string ||
					ay_strcasestr(tag_string, "br") == tag_string ||
					ay_strcasestr(tag_string, "img") == tag_string ||
					ay_strcasestr(tag_string, "hr") == tag_string) )
			{
				tag_list = g_list_append(tag_list, cur);
			}
			else {
				apply_tag(text_view, *cur, ignore );
				free(cur);
			}

		}
	
		/* Re-initialize the string to get new positions */
		gtk_text_buffer_get_end_iter(html_buffer, &end_iter);

		end_mark = gtk_text_buffer_create_mark(html_buffer, NULL, &end_iter, TRUE);

		gtk_text_buffer_get_iter_at_mark(html_buffer, &tag_start_iter, next_start_mark);
		gtk_text_buffer_get_iter_at_mark(html_buffer, &tag_end_iter, next_start_mark);
	}
	
	while (	(last_tag = g_list_nth_data(g_list_last(tag_list), 0)) ) {
		last_tag->end = *end_mark;
		apply_tag(text_view, *last_tag, ignore );
		tag_list = g_list_remove(tag_list, last_tag);
	}

	g_list_free(tag_list);

	unescape_html( html_buffer, html_start );
	messageid++;
}


/*
 * Append a line of text to the buffer
 */
void html_text_buffer_append ( GtkTextView *text_view, char *txt, int ignore )
{
	char *text = strdup(txt);
	GtkTextIter iter;
	GtkTextMark *insert_mark;
	GtkTextIter end;

	GdkRectangle iter_loc;
	GdkRectangle visible_rect;

	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

	if( strcasestr(text, "<br>") ) {
		char *c = text;
		while((c=strchr(text, '\n')) != 0 )
			*c = ' ';
		while((c=strchr(text, '\r')) != 0 )
			*c = ' ';
	}
	else if( strchr(text, '\r') ) {
		char *c = text;
		if( strchr(text, '\n') ) {
			while( (c=strchr(c, '\r')) != 0 )
				*c = ' ';
		}
		else {
			while( (c=strchr(c, '\r')) != 0 )
				*c = '\n';
		}
	}

	gtk_text_buffer_get_end_iter(buffer, &iter);

	/* Decide first if we want to scroll the text to the end or not */
	gtk_text_view_get_iter_location(text_view, &iter, &iter_loc);
	gtk_text_view_get_visible_rect(text_view, &visible_rect);

	insert_mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, TRUE);

	gtk_text_buffer_insert(buffer, &iter, text, strlen(text));
	parse_html(text_view, *insert_mark, ignore);

	if(iter_loc.y <= visible_rect.y+visible_rect.height) {
		gtk_text_buffer_get_end_iter(buffer, &end);

		gtk_text_view_scroll_mark_onscreen( text_view, 
				gtk_text_buffer_create_mark(buffer, NULL, &end, TRUE) );
	}
}


gboolean HOVERING_OVER_LINK = FALSE;
GdkCursor *hand_cursor = NULL;
GdkCursor *regular_cursor = NULL;

/*
 * Gets the url string we had added while parsing the html
 */
void get_url_at_location(gchar **return_val, GtkTextView *text_view, gint x, gint y)
{
	GSList *tags = NULL, *curr_tag = NULL;
	GtkTextIter iter;
	gchar *url = NULL;
	
	gtk_text_view_get_iter_at_location(text_view, &iter, x, y);

	tags = gtk_text_iter_get_tags(&iter);

	for( curr_tag = tags; curr_tag != NULL; curr_tag = curr_tag->next ) {
		GtkTextTag *tag = curr_tag->data;
		url = g_object_get_data(G_OBJECT(tag), "href");

		if(url != NULL) 
			break;
	}

	*return_val = url;


	if(tags)
		g_slist_free(tags);
}

/*
 * Sets the mouse cursor as hand if hovering over a link
 * or normal otherwise
 */
void set_mouse_cursor(GtkTextView *text_view, gint x, gint y)
{
	gchar *url = NULL;
	gboolean hovering = FALSE;

	get_url_at_location(&url, GTK_TEXT_VIEW(text_view), x, y);

	if(url)
		hovering = TRUE;
	
	if(hovering != HOVERING_OVER_LINK) {
		HOVERING_OVER_LINK = hovering;

		if(HOVERING_OVER_LINK)
			gdk_window_set_cursor(gtk_text_view_get_window(text_view,
						GTK_TEXT_WINDOW_TEXT),
					hand_cursor);
		else
			gdk_window_set_cursor(gtk_text_view_get_window(text_view,
						GTK_TEXT_WINDOW_TEXT),
					regular_cursor);

	}
}

static gboolean mouse_move_callback(GtkWidget *text_view, GdkEventMotion *event)
{
	gint x, y;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view),
			GTK_TEXT_WINDOW_WIDGET, event->x, event->y, &x, &y);
	set_mouse_cursor(GTK_TEXT_VIEW(text_view), x, y);

	gdk_window_get_pointer(text_view->window, NULL, NULL, NULL);

	return FALSE;
}

static gboolean mouse_click_callback(GtkWidget *text_view, GdkEventButton *event)
{
	gint x, y;
	gchar *url = NULL;

	if(event->type != GDK_2BUTTON_PRESS)
		return FALSE;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view),
			GTK_TEXT_WINDOW_WIDGET, event->x, event->y, &x, &y);

	get_url_at_location(&url, GTK_TEXT_VIEW(text_view), x, y);

	if(url != NULL) {
		open_url(text_view, url);
	}
	
	return TRUE;
}

static gboolean toggle_visibility_callback(GtkWidget *text_view, GdkEventVisibility *event)
{
	gint wx, wy, x, y;

	gdk_window_get_pointer(text_view->window, &wx, &wy, NULL);
	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view),
			GTK_TEXT_WINDOW_WIDGET, wx, wy, &x, &y);
	
	set_mouse_cursor(GTK_TEXT_VIEW(text_view), x, y);

	return FALSE;
}

/*
 * Initialize the text view as per our template
 */
void html_text_view_init ( GtkTextView *text_view, int ignore_font )
{
	char *str;
	PangoFontDescription *font_desc;

	hand_cursor = gdk_cursor_new(GDK_HAND2);
	regular_cursor = gdk_cursor_new(GDK_XTERM);

	str = cGetLocalPref("FontFace");

	gtk_text_view_set_editable( text_view, FALSE );
	gtk_text_view_set_wrap_mode( text_view, GTK_WRAP_WORD );
	gtk_text_view_set_cursor_visible(text_view, FALSE);

	g_signal_connect(text_view, "motion-notify-event",
			G_CALLBACK(mouse_move_callback), NULL);
	g_signal_connect(text_view, "visibility-notify-event",
			G_CALLBACK(toggle_visibility_callback), NULL);
	g_signal_connect(text_view, "button-press-event",
			G_CALLBACK(mouse_click_callback), NULL);

	if( str && ignore_font == 0 ) {
		font_desc = pango_font_description_from_string(str);
		gtk_widget_modify_font(GTK_WIDGET(text_view), font_desc);
		pango_font_description_free(font_desc);
	}
}

