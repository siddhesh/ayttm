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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_LIBXFT
#include <gdk/gdkprivate.h>
#include <X11/Xft/Xft.h>
#endif

#include "extgtktext.h"
#include "globals.h"
#include "smileys.h"
#include "prefs.h"
#include "browser.h"
#include "mem_util.h"

#include "pixmaps/aol_icon.xpm"
#include "pixmaps/free_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/admin_icon.xpm"
#include "pixmaps/no_such_smiley.xpm"


static GData * font_cache;
static gboolean cache_init = FALSE;


/*
 * adjust font metric attempts to find a legal font size that best matches
 * the requested font
 */

static int _adjust_font_metrics(int size)
{
	if (size > 60)
	{
		size = 60;
	}

	return size;
}

static char * _unescape_string(char * input)
{
	int i = 0;
	int j = 0;
	for(i = 0; input[i]; )
	{
		if(input[i] == '&')
		{
			if(!g_strncasecmp(input+i+1, "gt;", 3))
			{
				input[j++] = '>';
				i+=4;
			}
			else if(!g_strncasecmp(input+i+1, "lt;", 3))
			{
				input[j++] = '<';
				i+=4;
			}
			else if(!g_strncasecmp(input+i+1, "amp;", 4))
			{
				input[j++] = '&';
				i+=5;
			}
			else if(!g_strncasecmp(input+i+1, "#8212;", 6))
			{
				input[j++] = '-';
				input[j++] = '-';
				i+=7;
			}
			else if(!g_strncasecmp(input+i+1, "nbsp;", 5))
			{
				input[j++] = ' ';
				i+=6;
			}
			else
			{
				input[j++] = '&';
				i++;
			}
		}
		else
		{
			input[j++] = input[i++];
		}
	}
	input[j] = '\0';
	return input;
}


#ifndef HAVE_LIBXFT
static int tries=0;
static GdkFont * _getfont(char * font, int bold, int italic, int size, int ptsize)
{
    gchar font_name[1024] = "-*-";
    char *tmp = NULL;
    GdkFont * my_font;

	if(!cache_init)
	{
		g_datalist_init(&font_cache);
		cache_init = TRUE;
	}


    if ((tmp = cGetLocalPref("FontFace")) != NULL) {
	if (strstr(tmp, "-*-")) {
		/* new pref format */
		char mfont[1024] = "";
		char **tokens = ay_strsplit(tmp, "-", -1);
		int i;
		/*-bitstream-charter-medium-r-normal-*-13-160-*-*-p-*-iso8859-1*/
		/*1         2       3      4 5      6 7  8   9 0 1 2 3      (4)*/
		/* set size */
		
		i = 0;
		/* tokens[0] is empty as the string start with the delimiter */
		strcpy(mfont, "-");
		if (strcmp(font, tmp)) /* specified */
			strcat(mfont, "*");
		else
			strcat(mfont, tokens[1]);
		strcat(mfont, "-");
		if (strcmp(font, tmp)) /* specified */
			strcat(mfont, font);
		else
			strcat(mfont, tokens[2]);
		strcat(mfont, "-");
		if (bold)
			strcat(mfont, "bold");
		else	
			strcat(mfont, tokens[3]);
		strcat(mfont, "-");
		if (italic == 1)
			strcat(mfont, "i");
		else if (italic == 2)
			strcat(mfont, "o");
		else
			strcat(mfont, tokens[4]);
		strcat(mfont, "-");
		if (strcmp(font, tmp)) /* specified */
			strcat(mfont, "*");
		else
			strcat(mfont, tokens[5]);
		strcat(mfont, "-");
		strcat(mfont, tokens[6]);
		strcat(mfont, "-");
		
		if (size != -1 && strcmp(tokens[7], "*")) { 
			/* if token 7 contains * we want to keep it */
			char buf[10];
			snprintf(buf, 10, "%d", size);
			strcat(mfont, buf);
		} else
			strcat(mfont, tokens[7]);
		strcat(mfont, "-");
		
		if (ptsize != -1 && strcmp(tokens[8], "*")) {
			/* if token 8 contains * we want to keep it */
			char buf[10];
			snprintf(buf, 10, "%d", ptsize);
			strcat(mfont, buf);
		} else
			strcat(mfont, tokens[8]);
		i = 9;
		while (tokens[i]) {
			strcat(mfont, "-");
			strcat(mfont, tokens[i]);
			i++;
		}
		strcpy(font_name, mfont);
		ay_strfreev(tokens);
	}
    }
	
    if (!strcmp(font_name, "-*-")) /* not new format or no pref */ {    
	    if(strlen(font))
	    {
        	strncat( font_name, font, 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    }
	    else
	    {
    		strncat( font_name, "*", 
			(size_t)(sizeof(font_name)-strlen(font_name)) );
	    }
	    strncat( font_name, "-", 
			(size_t)(sizeof(font_name)-strlen(font_name)) );

	    if(bold)
	    {
        	strncat( font_name, "bold", 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    }
	    else
	    {
        	strncat( font_name, "medium", 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    }
	    strncat( font_name, "-", 
			(size_t)(sizeof(font_name)-strlen(font_name)) );

	    /*
	     * here is the deal, some fonts have oblique but not italics
	     * other fonts have italics but not oblique
	     * so we are going to try both
	     */

	    if( italic == 1 )
	    {
        	strncat( font_name, "i", 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    }
	    else if( italic == 2 )
	    {
        	strncat( font_name, "o", 
			(size_t)(sizeof(font_name)-strlen(font_name)) );
	    }
	    else
	    {
        	strncat( font_name, "r", 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    }
	    strncat( font_name, "-*-*-", 
			(size_t)(sizeof(font_name)-strlen(font_name)));
	    {
        	char buff[256];
			if(size != -1)
			{
				int size2 = 0;
				size2 = _adjust_font_metrics(size);
        			snprintf(buff, sizeof(buff), "%d-*-*-*-*-*-*-*", size2);
			}
			else
			{
        			snprintf(buff, sizeof(buff), "*-%d-*-*-*-*-*-*", (ptsize+ 5 -6)*10);
			}

        	strncat( font_name, buff, 
			(size_t)(sizeof(font_name)-strlen(font_name)) );
	    }

		eb_debug(DBG_HTML, "Wanting font %s\n", font_name);

	    g_strdown(font_name);

    }

    if(( my_font =
            g_datalist_id_get_data(&font_cache, g_quark_from_string(font_name)) ))
    {
        tries = 0;
        return my_font;
    }
    my_font = gdk_font_load(font_name);
    if( !my_font )
    {   tries++;
        if(tries > 3) {
		my_font = _getfont("helvetica", 0, 0, size, ptsize);
	}
        else if( italic == 1 )
        {
            my_font = _getfont(font, bold, 2, size, ptsize );
        }
        else if(strcmp(font, "helvetica"))
        {
            my_font = _getfont("helvetica", bold, italic, size, ptsize );
        }
	else
	{
            my_font = _getfont("fixed", bold, italic, size, ptsize );
	}
    } else tries = 0;
    g_datalist_id_set_data( &font_cache,
                            g_quark_from_string(font_name),
                            my_font );
    return my_font;
}
#else
static XftFont * _getfont(char * font, int bold, int italic, int size, int ptsize)
{
	XftFont * xftfont;
	XftPattern * pattern;
	gchar font_name[1024] = "";
	char *tmp = NULL;
	
	if(!cache_init)
	{
		g_datalist_init(&font_cache);
		cache_init = TRUE;
	}

	if ((tmp = cGetLocalPref("FontFace")) != NULL) {
		if (strstr(tmp, "-")) {
			/* new pref format */
			char **tokens = ay_strsplit(tmp,"-",-1);
			char **etokens = ay_strsplit(tmp,":",-1);
			char mfont[1024];
			char *endsize = NULL;
			int attributespos = 1;
			int mysize = 0;
			int i = 0;
			
			if (strcmp(tmp, font)) {
				/* font specified */
				strcpy(mfont, font);
			} else
				strcpy(mfont, tokens[0]);
			
			if (tokens[1] && !strstr(tokens[1], ":")) {
				mysize = atoi(tokens[1]);
			} else if (tokens[1]) {
				char *buf = strdup(tokens[1]);
				*strstr(buf, ":") = '\0';
				mysize = atoi(buf);
				free(buf);
			}
			
			if (size != -1) {
				char buf[10];
				snprintf(buf, 10, "%d", size);
				strcat(mfont, "-");
				strcat(mfont, buf);
			} else if (ptsize != -1) {
				char buf[10];
				snprintf(buf, 10, "%d", ptsize/10);
				strcat(mfont, "-");
				strcat(mfont, buf);
			} else if (mysize) {
				char buf[10];
				snprintf(buf, 10, "%d", mysize);
				strcat(mfont, "-");
				strcat(mfont, buf);
			}
			
			if (bold) {
				strcat(mfont, ":");
				strcat(mfont, "bold");
			} else if(etokens[1]) {
				strcat(mfont, ":");
				strcat(mfont, etokens[1]);
				attributespos++;
			}
			if (italic)
				strcat(mfont, ":slant=italic,oblique");
			else if(etokens[1] && etokens[2]) {
				strcat(mfont, ":");
				strcat(mfont, etokens[2]);
				attributespos++;
			}
			
			ay_strfreev(tokens);
			ay_strfreev(etokens);
			strcpy(font_name, mfont);
		}
	}

	if (!strcmp(font_name, "")) /* not new format */ {    

		snprintf(font_name, sizeof(font_name), "%s,helvetica,arial", font);
		if(size != -1)
		{
			int size2 = 0;
			char buff[256] = "";

			eb_debug(DBG_HTML, "Size param = %d\n", size);
			eb_debug(DBG_HTML, "fontsize adjust = %d\n", 5);
			size2 = _adjust_font_metrics(size);
			snprintf(buff, sizeof(buff), "-%d", size2);
			strncat(font_name, buff, 
				(size_t)(sizeof(font_name)-strlen(font_name)));
		}
		else
		{
			int size2 = ptsize+(5-3);
			char buff[256] = "";
			snprintf(buff, sizeof(buff), "-%d", size2);
			strncat(font_name, buff, 
				(size_t)(sizeof(font_name)-strlen(font_name)));
		}
		if(bold)
		{
			strncat(font_name, ":bold", 
				(size_t)(sizeof(font_name)-strlen(font_name)));
		}
		if(italic)
		{
			strncat(font_name, ":slant=italic,oblique", 
				(size_t)(sizeof(font_name)-strlen(font_name)));
		}
    		if(( xftfont =
		    g_datalist_id_get_data(&font_cache, g_quark_from_string(font_name)) ))
    		{
			return xftfont;
    		}

		eb_debug(DBG_HTML, "Wanting font %s\n", font_name);
	}
	
	pattern = XftNameParse(font_name);
	xftfont = XftFontOpenName(gdk_display, DefaultScreen(gdk_display), font_name);

	if(xftfont == NULL)
	{
		eb_debug(DBG_HTML, "font %s Not Found!!!\n", font_name);
	}

    	g_datalist_id_set_data( &font_cache,
                            g_quark_from_string(font_name),
                            xftfont);
	return xftfont;
}
#endif

static GdkColor * _getcolor(GdkColormap * map, char * name)
{
	GdkColor * color;

	color = (GdkColor *)g_new0(GdkColor, 1);
	gdk_color_parse(name, color);
	gdk_color_alloc(map, color);

	return color;
}

static void _extract_parameter(char * parm, char * buffer, int buffer_size)
{
	/*
	 *if we start with a quote, we should end with a quote
	 * other wise we end with a space
	 */

	if(*parm == '\"')
	{
		int cnt = 0;
		parm += 1;

		for(cnt = 0; *parm && *parm != '\"' && cnt < buffer_size-1;
				parm++, cnt++)
		{
			buffer[cnt] = *parm;
		}
		buffer[cnt] = '\0';
	}
	else
	{
		int cnt = 0;

		for(cnt = 0; *parm && !isspace(*parm) && *parm != '>' && cnt < buffer_size-1;
				parm++, cnt++)
		{
			buffer[cnt] = *parm;
		}
		buffer[cnt] = '\0';
	}
}

typedef struct _font_stack
{
	GdkColor * fore;
	GdkColor * back;
	char font_name[255];
	int font_size;
	int font_ptsize;
	struct _font_stack * next;
} font_stack;

static font_stack * _font_stack_init()
{
	font_stack * fs = g_new0(font_stack, 1);
	char *str = cGetLocalPref("FontFace");
	
#ifndef HAVE_LIBXFT	
	if (str && strstr(str,"-*-")) {
		char *iwalk = strdup(str);
		char *walk;
		char *size;
		char *ptsize;
		int i;
		
		walk = iwalk;
		for (i = 0; i < 6; i++)
			walk = strstr(walk+sizeof(char),"-");
		size = walk+sizeof(char);
		walk = strstr(walk+sizeof(char),"-");
		*walk = 0;
		ptsize = walk+sizeof(char);
		walk = strstr(walk+sizeof(char),"-");
		*walk = 0;
		if (strcmp(size,"*")) {
			fs->font_size = atoi(size);
			fs->font_ptsize = atoi(size)*10;
		}
		else if (strcmp(ptsize,"*")) {
			fs->font_ptsize = atoi(ptsize);
			fs->font_size = fs->font_ptsize/10;
		} else {
			fs->font_size = 12;
			fs->font_ptsize = 120;
		}
		free(iwalk);
#else
	if (str && strstr(str,"-")) {
		char *tmp = strdup(strstr(str,"-")+1);
		if (strstr(tmp,":"))
			*strstr(tmp,":") = '\0';
			
		fs->font_size = atoi(tmp);
		fs->font_ptsize = atoi(tmp)*10;
		
		free(tmp);
#endif
	} else {
		fs->font_size = 12;
		fs->font_ptsize = 120;
	}
	
	strcpy(fs->font_name, cGetLocalPref("FontFace"));
	return fs;
}

static font_stack * _font_stack_push(font_stack * fs)
{
	font_stack * fs2 = g_new0(font_stack, 1);
	memcpy(fs2, fs, sizeof(font_stack));
	fs2->next = fs;

	if(fs2->fore)
	{
		fs2->fore = g_new0(GdkColor, 1);
		memcpy(fs2->fore, fs->fore, sizeof(GdkColor));
	}

	if(fs2->back)
	{
		fs2->back = g_new0(GdkColor, 1);
		memcpy(fs2->back, fs->back, sizeof(GdkColor));
	}

	return fs2;
}

static font_stack * _font_stack_pop(font_stack * fs)
{

	font_stack * fs2 = fs->next;
	if(fs->fore)
		g_free(fs->fore);
	if(fs->back)
		g_free(fs->back);
	g_free(fs);

	return fs2;
}

static void handle_click(GdkWindow *widget, gpointer data)
{
   if(data && strlen(data) > 1 && strcmp(data, "\n"))
   {
#ifdef __MINGW32__
      open_url(widget, data);
#else
      open_url(NULL, data);
#endif
   }
}

void gtk_eb_html_init(ExtGtkText * widget)
{
    gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
            GTK_SIGNAL_FUNC(handle_click), NULL);
    ext_gtk_text_set_editable(EXT_GTK_TEXT(widget), FALSE);
    ext_gtk_text_set_line_wrap(EXT_GTK_TEXT(widget), TRUE);
    ext_gtk_text_set_word_wrap(EXT_GTK_TEXT(widget), TRUE);



}

static const int dark_threshold = 0xd0;
static const int lighten_delta = 0x38;
static const int darken_delta = 0x60;
static int _lighten(int colour)
{
	if(!colour)
		return 0;
	if(colour + lighten_delta < 0xff)
		return colour + lighten_delta;
	else
		return 0xff;
}

static int _darken(int colour)
{
	if(!colour)
		return 0;
	if(colour - darken_delta > 0)
		return colour - darken_delta;
	else
		return 0;
}

static int _is_dark(int r, int g, int b)
{
	return (r <= dark_threshold &&
			g <= dark_threshold &&
			b <= dark_threshold);
}

static void _adjust_colors(int*fg_r, int*fg_g, int*fg_b,
		int r, int g, int b)
{

	int bg_dark = _is_dark(r,g,b);
	int fg_dark = _is_dark(*fg_r, *fg_g, *fg_b);

	eb_debug(DBG_HTML, "adjusting color %x %x %x", *fg_r, *fg_g, *fg_b);

	if(bg_dark && fg_dark)
	{
		*fg_r = _lighten(*fg_r);
		*fg_g = _lighten(*fg_g);
		*fg_b = _lighten(*fg_b);
	}
	else if(!bg_dark && !fg_dark)
	{
		*fg_r = _darken(*fg_r);
		*fg_g = _darken(*fg_g);
		*fg_b = _darken(*fg_b);
	}
}



void gtk_eb_html_add(ExtGtkText* widget, char * text,
		int ignore_bgcolor, int ignore_fgcolor, int ignore_font)
{
	gchar ** tokens;
	int i = 0;
	int fill = 0;
#ifndef HAVE_LIBXFT
	GdkFont * font = NULL;
	GdkFont * previousfont = NULL;
#else
	XftFont * font = NULL;
#endif
	/*
	 * these atributes don't go into the font stact because we should not
	 * allow nesting of these attributes, but allow then to be changed
	 * independently of the font stact
	 */

	int font_bold = 0;
	int font_italic = 0;
	char * url = NULL;

	int first = 0;



	/*
	 * the ignore variable is used so we can ignore any header that may
	 * be on the html
	 */

	int ignore = 0;

	font_stack * fs = _font_stack_init();

	fs->fore =  g_new0(GdkColor, 1);
	memcpy(fs->fore, &GTK_WIDGET (widget)->style->fg[0],
			sizeof(GdkColor));

	font = _getfont(fs->font_name, font_bold, font_italic,
		fs->font_size, fs->font_ptsize);
#ifndef HAVE_LIBXFT
	previousfont = _getfont(fs->font_name, font_bold, font_italic,
		fs->font_size, fs->font_ptsize);
#endif

	/*
	 * Since the first thing in a split list may or may not be starting with a
	 * < we need to keep track of that manually
	 */

	if(*text != '<')
	{
		first = 1;
	}

	/*
	 * a nice heuristic to decide if we should be proper with
	 * our html and ignore the \n
	 * or deal with our not-so-fortunate human typers
	 * that don't do proper html
	 */

	if(strstr(text, "<br>") || strstr(text, "<BR>"))
	{
		char * c = text;
		while((c = strstr(c, "\n")) != 0)
		{
			*c = ' ';
		}
		c = text;
		while((c = strstr(c, "\r")) != 0)
		{
			*c = ' ';
		}
	}
	else if(strstr(text, "\r"))
	{	char * c = text;
		if(strstr(text, "\n"))
		{	/* drop the useless \r's */
			while((c = strstr(c,"\r")) != 0)
			{
				*c = ' ';
			}
		}
		else
		{	/* convert the \r to readable \n */
			while((c = strstr(c,"\r")) != 0)
			{
				*c = '\n';
			}
		}
	}

	tokens = g_strsplit(text, "<", 0);


	/*
	 * if we started with a < then the first thing in the list
	 * will be a 0 lenght string, so we want to skip that
	 */

	for( i = first?0:1; tokens[i]; i++ )
	{


		eb_debug(DBG_HTML, "Part %d: %s\n", i, tokens[i]);

		if(!first && strstr(tokens[i], ">"))
		{
			char * copy = strdup(tokens[i]);
#ifdef __MINGW32__
			gchar ** tags = g_strsplit(tokens[i], ">", 2);
#else
			gchar ** tags = g_strsplit(tokens[i], ">", 1);
#endif
			/*
			 * Okay, we now know that tags[0] is an html tag because
			 * there was a < before it and a > after it
			 */

			/*
			 * first let's clean it up
			 */

			eb_debug(DBG_HTML, "\t Part a: %s\n", tags[0]);
			eb_debug(DBG_HTML, "\t Part b: %s\n", tags[1]);
			g_strstrip(tags[0]);
			g_strdown(tags[0]);

			if(!strncmp(tags[0], "font", strlen("font")))
			{
				char * data = tags[0] + strlen("font");
				char * parm;

				fs = _font_stack_push(fs);

				if((parm = strstr(data, "face=")) != NULL)
				{
					parm += strlen("face=");
					_extract_parameter(parm, fs->font_name, 255);
				}

				if((parm = strstr(data, "ptsize=")) != NULL)
				{
					char size[25];
					parm += strlen("ptsize=");
					_extract_parameter(parm, size, 25);
					fs->font_size = atoi(size);
					fs->font_ptsize = atoi(size)*10; /* html ptsize is less */
					eb_debug(DBG_HTML, "got a ptsize of %d\n", fs->font_ptsize);
				}
				else if((parm = strstr(data, "absz=")) != NULL)
				{
					char size[25];
					parm += strlen("absz=");
					_extract_parameter(parm, size, 25);
					fs->font_size = atoi(size)/10;
					fs->font_ptsize = atoi(size);
					eb_debug(DBG_HTML, "got a ptsize of %d\n", fs->font_ptsize);
				}
				else if((parm = strstr(data, "size=")) != NULL)
				{
					char size[25];
					parm += strlen("size=");
					_extract_parameter(parm, size, 25);

					if(*size == '+')
					{
						fs->font_size += atoi(size+1);
						fs->font_ptsize += 10*atoi(size+1);
					}
					else if(*size == '-')
					{
						fs->font_size -= atoi(size+1);
						fs->font_ptsize -= 10*atoi(size+1);
					}
					else
					{
						fs->font_size += atoi(size);
						fs->font_ptsize += 10*(atoi(size));
					}
				}
				if((parm = strstr(data, "color=")) != NULL)
				{
					char color[255];
					parm += strlen("color=");
					_extract_parameter(parm, color, 255);
					if(fs->fore)
					{
						g_free(fs->fore);
						fs->fore = NULL;
					}
					if(!ignore_fgcolor)
					{
						//fprintf(stderr, "color: %s\n", color);


						if(!fs->back)
						{
							GdkColor * bg = &GTK_WIDGET(widget)->style->base[GTK_STATE_NORMAL];
							int r, g, b;
							char color2[20];
							int fg_r, fg_g, fg_b;
							if(!sscanf(color, "#%02x%02x%02x", &fg_r, &fg_g, &fg_b))
							{
								GdkColor *tmpColor = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), color);
								fg_r = (tmpColor->red >> 8)&0xFF;
								fg_g = (tmpColor->green >> 8)&0xFF;
								fg_b = (tmpColor->blue >> 8)&0xFF;
								g_free(tmpColor);

							}


							r = (bg->red >> 8)&0xFF;
							g = (bg->green >> 8)&0xFF;
							b = (bg->blue >> 8)&0xFF;

							_adjust_colors(&fg_r, &fg_g, &fg_b,
									r, g, b);

							snprintf(color2, sizeof(color2), "#%02x%02x%02x",fg_r, fg_g,fg_b);
							//fprintf(stderr, "Result: %s\n", color2);
							fs->fore = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), color2);
						}
						else
						{
							fs->fore = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), color);
						}
					}
					else
					{
						if(!fs->back && !ignore_bgcolor)
						{
							fs->fore =  g_new0(GdkColor, 1);
							memcpy(fs->fore, &GTK_WIDGET (widget)->style->fg[0],
									sizeof(GdkColor));
						}
						else
						{
							fs->fore = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), "black");
						}
					}

				}

				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic,
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strncmp(tags[0], "img ", strlen("img ")))
			{
				char * parm;
				if((parm = strstr(copy, "src=")) != NULL
					||(parm = strstr(copy, "SRC=")) != NULL)
				{
					char tmp_url[1024];
					GdkPixmap *icon;
					GdkBitmap *mask;
					parm += strlen("src=");

					_extract_parameter(parm,
							tmp_url, 1024);

					if(!strcmp(tmp_url, "aol_icon.gif"))
					{
						icon = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, aol_icon_xpm);
						ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
						icon, mask, "aol_icon", 0);

					}
					else if(!strcmp(tmp_url, "free_icon.gif"))
					{
						icon = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, free_icon_xpm);
						ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
						icon, mask, "free_icon", 0);

					}
					else if(!strcmp(tmp_url, "dt_icon.gif"))
					{
						icon = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, dt_icon_xpm);
						ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
						icon, mask, "dt_icon", 0);

					}
					else if(!strcmp(tmp_url, "admin_icon.gif"))
					{
						icon = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, admin_icon_xpm);
						ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
						icon, mask, "admin_icon", 0);

					}
				}
			}
			else if(!strncmp(tags[0], "smiley ", strlen("smiley ")))
			{
				char * parm;
				if((parm = strstr(copy, "name=")) != NULL
					||(parm = strstr(copy, "NAME=")) != NULL)
				{
					char tmp_sname[64];
					GdkPixmap *icon;
					GdkBitmap *mask;
                                        LList * slist=smileys;
                                        smiley * smile;

					parm += strlen("name=");
                                        // here
                                        _extract_parameter(parm, tmp_sname, 64);

                                        while(slist!=NULL)
                                        {
                                                smile=(smiley *)slist->data;
                                                if(!strcmp(smile->name, tmp_sname))
                                                {
						  icon = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, smile->pixmap);
						  ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
						  icon, mask, "smiley", strlen("smiley"));
                                                  break;
                                                }
                                                slist=slist->next;
                                        }
                                        if(slist==NULL)
                                        {
						if((parm = strstr(copy, "alt=")) != NULL
								|| (parm = strstr(copy, "ALT=")) != NULL) 
						{
							parm += strlen("alt=");
							_extract_parameter(parm, tmp_sname, 64);
							ext_gtk_text_insert(widget, font, fs->fore, fs->back,
									tmp_sname, strlen(tmp_sname), fill);
						}
						else
						{
							icon =gdk_pixmap_create_from_xpm_d(GTK_WIDGET(widget)->window, &mask, NULL, no_such_smiley_xpm);
							ext_gtk_text_insert_pixmap(widget, font, fs->fore, fs->back,
							icon, mask, "smiley", strlen("smiley"));
						}
                                        }
                                }
                        }
			else if(!strncmp(tags[0], "a ", strlen("a ")))
			{
				/* UNUSED char * data = tags[0] + strlen("a "); */
				char * parm;

				fs = _font_stack_push(fs);

				if((parm = strstr(copy, "href=")) != NULL
					||(parm = strstr(copy, "HREF=")) != NULL
)
				{
					char tmp_url[1024];
					int r=0, g=0, b=0xFF;
					GdkColor * bg = &GTK_WIDGET(widget)->style->base[GTK_STATE_NORMAL];
					char color[20];
					int bg_r, bg_g, bg_b;


					bg_r = (bg->red >> 8)&0xFF;
					bg_g = (bg->green >> 8)&0xFF;
					bg_b = (bg->blue >> 8)&0xFF;

					_adjust_colors(&r, &g, &b, bg_r, bg_g, bg_b);
					snprintf(color, sizeof(color), "#%02x%02x%02x", r, g, b);
					fs->fore = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), color);

					parm += strlen("href=");

					_extract_parameter(parm, 
							tmp_url, 1024);
					if(url)
					{
						g_free(url);
					}
					url = strdup(tmp_url);


				}
			}
			else if(!strcmp(tags[0], "/a"))
			{
				fs = _font_stack_pop(fs);
				if(url)
				{
					g_free(url);
				}
				url = NULL;

				if(!fs)
				{
					fs = _font_stack_init();
					fs->fore =  g_new0(GdkColor, 1);
					memcpy(fs->fore, &GTK_WIDGET (widget)->style->fg[0],
							sizeof(GdkColor));
				}
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strcmp(tags[0], "b"))
			{
				font_bold = 1;
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
				
			}
			else if(!strcmp(tags[0], "u"))
			{
				eb_debug(DBG_HTML, "Underline tag found\n");
				if(!url)
				{
					url = strdup("");
				}
				else
				{
					eb_debug(DBG_HTML, "Underline tag ignored\n");
				}
			}
			else if(!strcmp(tags[0], "/u"))
			{
				/*
				 * the widget stores a copy, so we must free
				 * the original
				 */
				if(url)
				{
					g_free(url);
					url = NULL;
				}
			}
			else if(!strcmp(tags[0], "/b"))
			{
				font_bold = 0;
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strcmp(tags[0], "i"))
			{
				font_italic = 1;
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strcmp(tags[0], "/i"))
			{
				font_italic = 0;
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strcmp(tags[0], "br"))
			{
				if(url)
				{
					ext_gtk_text_insert_data_underlined(widget, font, fs->fore, fs->back, 
						url, strlen(url)+1, handle_click, "\n", strlen("\n"), fill);
				}
				else
				{
					ext_gtk_text_insert(widget, font, fs->fore, fs->back, 
						"\n", strlen("\n"), fill);
				}
			}
			else if(!strcmp(tags[0], "html") 
					| !strcmp(tags[0], "/html")
					| !strcmp(tags[0], "/p")
					| !strcmp(tags[0], "p")
					| !strcmp(tags[0], "title")
					| !strcmp(tags[0], "/title")
					| !strcmp(tags[0], "pre")
					| !strcmp(tags[0], "/pre")
					| !strncmp(tags[0], "img", strlen("img")))
			{
			}
			else if(!strcmp(tags[0], "hr") 
					|| !strncmp(tags[0], "hr ", strlen("hr ")))
			{
				ext_gtk_text_insert(widget, font, fs->fore, fs->back, 
						"\n", strlen("\n"),fill);
				ext_gtk_text_insert_divider(widget, font, fs->fore, fs->back, 
						" \n", strlen(" \n"));


			}
			else if(!strcmp(tags[0], "/font")
					|| !strcmp(tags[0], "/body"))
			{
				fs = _font_stack_pop(fs);
				
			
	/*
	 * we don't want to pop off the very first element in the stack
	 * because that is the defaults, if we are in a positon of trying
	 * that means the user tried doing one too many </font>'s
	 * heh
	 */
				
				if(!fs)
				{
					fs = _font_stack_init();
					fs->fore =  g_new0(GdkColor, 1);
					memcpy(fs->fore, &GTK_WIDGET (widget)->style->fg[0],
							sizeof(GdkColor));
				}
				if(!ignore_font)
				{
					font = _getfont(fs->font_name, font_bold, font_italic, 
						fs->font_size, fs->font_ptsize);
				}
				else
				{
#ifndef HAVE_LIBXFT
					font = previousfont;
#endif
				}
			}
			else if(!strncmp(tags[0], "head", strlen("head")))
			{
				/*
				 * we want to ignore the header of an html doc
				 */

				ignore = 1;
			}
			else if(!strncmp(tags[0], "body", strlen("body")))
			{
				char * data = tags[0] + strlen("body");
				char * parm;

				ignore = 0;


				fs = _font_stack_push(fs);

				if((parm = strstr(data, "bgcolor=")) != NULL)
				{
					char color[255];
					parm += strlen("bgcolor=");
					_extract_parameter(parm, color, 255);
					if(fs->back)
					{
						g_free(fs->back);
					}
					if(!ignore_bgcolor)
					{
						fs->back = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), color);
						//fs->fore = _getcolor(gdk_window_get_colormap(GTK_WIDGET(widget)->window), "black");
					}
					else
					{
						fs->back = NULL;
					}
				}
				if((parm = strstr(data, "width=")) != NULL) 
				{
					char width[10];
					parm += strlen("width=");
					_extract_parameter(parm, width, 10);
					if (!strcmp(width, "*") || !strcmp(width, "100%")) {
						fill = 1;
					} else {
						eb_debug(DBG_HTML, "body width != 100% unimplemented yet\n");
					}
				}
			}
			else
			{
				_unescape_string(copy);
				if(url)
				{
					ext_gtk_text_insert_data_underlined(widget, font, fs->fore, fs->back,
								url, strlen(url)+1, handle_click, "<", 1, fill);
					ext_gtk_text_insert_data_underlined(widget, font, fs->fore, fs->back,
								url, strlen(url)+1, handle_click, copy, strlen(copy), fill);
				}
				else
				{
					ext_gtk_text_insert(widget, font, fs->fore, fs->back,
								"<", 1, fill);
					ext_gtk_text_insert(widget, font, fs->fore, fs->back,
								copy, strlen(copy), fill);
				}
				g_strfreev(tags);
				free(copy);
				first = 0;
				continue;

			}

			if(tags[1] && !ignore)
			{
				_unescape_string(tags[1]);

				if(url)
				{
					ext_gtk_text_insert_data_underlined(widget, font, fs->fore, fs->back,
								url, strlen(url)+1, handle_click, tags[1], strlen(tags[1]), fill);
					eb_debug(DBG_HTML, "Underlined text inserted\n");
				}
				else
				{
					ext_gtk_text_insert(widget, font, fs->fore, fs->back,
								tags[1], strlen(tags[1]), fill);
				}
			}
			g_strfreev(tags);
			free(copy);
		}
		else
		{
			/*
			 * Otherwise then it is all just text
			 */

			/*
			 * first we were not supposed to have gotten rid of that < ;P
			 */

			if(!ignore)
			{
				if(!first)
				{
					if(url)
					{
						ext_gtk_text_insert_data_underlined( widget, font, fs->fore, 
								fs->back, url, strlen(url)+1, handle_click, "<", strlen("<"),fill );
					}
					else
					{
						ext_gtk_text_insert( widget, font, fs->fore, 
								fs->back, "<", strlen("<"),fill );
					}
				}

				_unescape_string(tokens[i]);
				if(url)
				{
					ext_gtk_text_insert_data_underlined(widget, font, fs->fore, fs->back,
									url, strlen(url)+1, handle_click, tokens[i], strlen(tokens[i]), fill);
				}
				else
				{
					ext_gtk_text_insert(widget, font, fs->fore, fs->back,
									tokens[i], strlen(tokens[i]), fill);
				}
			}

		}
		first = 0;
	}
	/*
	 * we got this quirk of loosing the < if it ends
	 * with the <
	 * this is the fix
	 */

	if(text[strlen(text)-1] == '<')
	{
		if(url)
		{
			ext_gtk_text_insert_data_underlined( widget, font, fs->fore, 
				fs->back, url, strlen(url)+1, handle_click, "<", strlen("<") ,fill);
		}
		else
		{
			ext_gtk_text_insert( widget, font, fs->fore, 
				fs->back, "<", strlen("<") ,fill);
		}
	}
		
	g_strfreev(tokens);

	while(fs)
	{
		fs = _font_stack_pop(fs);
	}
}

void	gtk_eb_html_log_parse_and_add( ExtGtkText *widget, const char *log )
{
	char *tmp = NULL;
	char *buff2 = NULL;
	
	/* ME */
	tmp = strdup( log );

	if ( ((!strncmp(tmp,"<P> <FONT COLOR=\"#", strlen("<P> <FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P> <FONT COLOR=\"#??????\"><B>")]))
	|| ( !strncmp(tmp,"<P><FONT COLOR=\"#", strlen("<P><FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P><FONT COLOR=\"#??????\"><B>")])))
	&& strstr(tmp, "/FONT> ")) {
		/* seems to be a beginning of line... */
		buff2 = strstr(tmp, "/FONT> ")+strlen("/FONT>");
		buff2[0] = '\0';
		buff2++;
		gtk_eb_html_add(widget, tmp,0,0,0);
		
		gtk_eb_html_add(widget, buff2,
				iGetLocalPref("do_ignore_back"),
				iGetLocalPref("do_ignore_fore"),
				iGetLocalPref("do_ignore_font"));
	} /* OTHER */
	else if (
		(!strncmp(tmp,"<P> <B><FONT COLOR=\"#", strlen("<P> <B><FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P> <B><FONT COLOR=\"#??????\">")])
		&& strstr(tmp, "/FONT> </B> "))
	|| 	(!strncmp(tmp,"<P><B><FONT COLOR=\"#", strlen("<P><B><FONT COLOR=\"#")) 
		&& isdigit(tmp[strlen("<P><B><FONT COLOR=\"#??????\">")])
		&& strstr(tmp, "/FONT> </B> "))
		)
	{
		/* seems to be a beginning of line... */
		buff2 = strstr(tmp, "/FONT> </B> ")+strlen("/FONT> </B>");
		buff2[0] = '\0';
		buff2++;
		gtk_eb_html_add(widget, tmp,0,0,0);
		
		gtk_eb_html_add(widget, buff2,
				iGetLocalPref("do_ignore_back"),
				iGetLocalPref("do_ignore_fore"),
				iGetLocalPref("do_ignore_font"));
	}
	else
	{
		gtk_eb_html_add(widget, tmp,0,0,0);
	}
	
	free( tmp );
}
