/*
 * Ayttm 
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

#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "service.h"
#include "smileys.h"
#include "prefs.h"
#include "value_pair.h"

#include "pixmaps/smile.xpm"
#include "pixmaps/sad.xpm"
#include "pixmaps/wink.xpm"
#include "pixmaps/biglaugh.xpm"
#include "pixmaps/laugh.xpm"
#include "pixmaps/cry.xpm"
#include "pixmaps/tongue.xpm"
#include "pixmaps/cooldude.xpm"
#include "pixmaps/note.xpm"
#include "pixmaps/dude.xpm"
#include "pixmaps/worried.xpm"
#include "pixmaps/confused.xpm"
#include "pixmaps/grin.xpm"
#include "pixmaps/blankface.xpm"
#include "pixmaps/blush.xpm"
#include "pixmaps/oh.xpm"
#include "pixmaps/heyyy.xpm"
#include "pixmaps/lovey.xpm"
#include "pixmaps/angry.xpm"
#include "pixmaps/wine.xpm"
#include "pixmaps/beer.xpm"

#include "pixmaps/msn/boy.xpm"
#include "pixmaps/msn/bulb.xpm"
#include "pixmaps/msn/cake.xpm"
#include "pixmaps/msn/cat.xpm"
#include "pixmaps/msn/coffee.xpm"
#include "pixmaps/msn/dog.xpm"
#include "pixmaps/msn/email.xpm"
#include "pixmaps/msn/gift.xpm"
#include "pixmaps/msn/girl.xpm"
#include "pixmaps/msn/kiss.xpm"
#include "pixmaps/msn/phone.xpm"
#include "pixmaps/msn/question.xpm"
#include "pixmaps/msn/run.xpm"
#include "pixmaps/msn/runback.xpm"
#include "pixmaps/msn/star.xpm"
#include "pixmaps/msn/sun.xpm"
#include "pixmaps/msn/thumbdown.xpm"
#include "pixmaps/msn/thumbup.xpm"
#include "pixmaps/msn/angel.xpm"
#include "pixmaps/msn/bat.xpm"
#include "pixmaps/msn/neutral.xpm"
#include "pixmaps/msn/brheart.xpm"
#include "pixmaps/msn/clock.xpm"
#include "pixmaps/msn/deadflower.xpm"
#include "pixmaps/msn/film.xpm"
#include "pixmaps/msn/flower.xpm"
#include "pixmaps/msn/handcuffs.xpm"
#include "pixmaps/msn/heart.xpm"
#include "pixmaps/msn/photo.xpm"
#include "pixmaps/msn/rainbow.xpm"
#include "pixmaps/msn/moon.xpm"
#include "pixmaps/msn/devil.xpm"

static char *no_smileys[] = {
	"http://",
	"https://",
	"ftp://",
	"mailto:/",
	"log://",
	NULL
};

LList *smileys=NULL;
static LList *default_smileys = NULL;

static LList *_eb_smileys=NULL;

static LList	*s_smiley_sets = NULL;



void	ay_add_smiley_set( const char *inName, LList *inSet )
{
	ay_remove_smiley_set( inName );
	
	s_smiley_sets = value_pair_add( s_smiley_sets, inName, (const char *)inSet );
}

LList *ay_lookup_smiley_set( const char *inName )
{
	void	*current = value_pair_get_value( s_smiley_sets, inName );
	
	return( (LList *)current );
}

void	ay_remove_smiley_set( const char *inName )
{
	if ( ay_lookup_smiley_set( inName ) )
		s_smiley_sets = value_pair_remove( s_smiley_sets, inName );
}

/* someone figure out how to do this with LList * const */
LList * eb_smileys(void)
{
  return _eb_smileys;
}

void init_smileys(void)
{
  smileys=add_smiley(smileys, "smile", smile_xpm);
  smileys=add_smiley(smileys, "sad", sad_xpm);
  smileys=add_smiley(smileys, "wink", wink_xpm);
  smileys=add_smiley(smileys, "biglaugh", biglaugh_xpm);
  smileys=add_smiley(smileys, "laugh", laugh_xpm);
  smileys=add_smiley(smileys, "cry", cry_xpm);
  smileys=add_smiley(smileys, "tongue", tongue_xpm);
  smileys=add_smiley(smileys, "cooldude", cooldude_xpm);
  smileys=add_smiley(smileys, "note", note_xpm);
  smileys=add_smiley(smileys, "dude", dude_xpm);
  smileys=add_smiley(smileys, "worried", worried_xpm);
  smileys=add_smiley(smileys, "grin", grin_xpm);
  smileys=add_smiley(smileys, "confused", confused_xpm);
  smileys=add_smiley(smileys, "blank", blankface_xpm);
  smileys=add_smiley(smileys, "blush", blush_xpm);
  smileys=add_smiley(smileys, "oh", oh_xpm);
  smileys=add_smiley(smileys, "heyyy", heyyy_xpm);
  smileys=add_smiley(smileys, "lovey", lovey_xpm);
  smileys=add_smiley(smileys, "angry", angry_xpm);
  smileys=add_smiley(smileys, "wine", wine_xpm);
  smileys=add_smiley(smileys, "beer", beer_xpm);

  smileys=add_smiley(smileys, "boy", boy_xpm);
  smileys=add_smiley(smileys, "lightbulb", bulb_xpm);
  smileys=add_smiley(smileys, "cake", cake_xpm);
  smileys=add_smiley(smileys, "cat", cat_xpm);
  smileys=add_smiley(smileys, "coffee", coffee_xpm);
  smileys=add_smiley(smileys, "dog", dog_xpm);
  smileys=add_smiley(smileys, "letter", email_xpm);
  smileys=add_smiley(smileys, "gift", gift_xpm);
  smileys=add_smiley(smileys, "girl", girl_xpm);
  smileys=add_smiley(smileys, "kiss", kiss_xpm);
  smileys=add_smiley(smileys, "phone", phone_xpm);
  smileys=add_smiley(smileys, "a/s/l", question_xpm);
  smileys=add_smiley(smileys, "boy_right", run_xpm);
  smileys=add_smiley(smileys, "girl_left", runback_xpm);
  smileys=add_smiley(smileys, "star", star_xpm);
  smileys=add_smiley(smileys, "sun", sun_xpm);
  smileys=add_smiley(smileys, "no", thumbdown_xpm);
  smileys=add_smiley(smileys, "yes", thumbup_xpm);
  smileys=add_smiley(smileys, "angel", angel_xpm);
  smileys=add_smiley(smileys, "bat", bat_xpm);
  smileys=add_smiley(smileys, "neutral", neutral_xpm);
  smileys=add_smiley(smileys, "broken_heart", brheart_xpm);
  smileys=add_smiley(smileys, "clock", clock_xpm);
  smileys=add_smiley(smileys, "dead_flower", deadflower_xpm);
  smileys=add_smiley(smileys, "film", film_xpm);
  smileys=add_smiley(smileys, "flower", flower_xpm);
  smileys=add_smiley(smileys, "handcuffs", handcuffs_xpm);
  smileys=add_smiley(smileys, "heart", heart_xpm);
  
  smileys=add_smiley(smileys, "moon", moon_xpm);
  smileys=add_smiley(smileys, "devil", devil_xpm);
  smileys=add_smiley(smileys, "camera", photo_xpm);
  smileys=add_smiley(smileys, "rainbow", rainbow_xpm);
  
  _eb_smileys = smileys;

  default_smileys=add_protocol_smiley(default_smileys, ":-)", "smile");
  default_smileys=add_protocol_smiley(default_smileys, ":)", "smile");
  default_smileys=add_protocol_smiley(default_smileys, "=)", "smile");

  default_smileys=add_protocol_smiley(default_smileys, ":-(", "sad");
  default_smileys=add_protocol_smiley(default_smileys, ":(", "sad");

  default_smileys=add_protocol_smiley(default_smileys, ";-)", "wink");
  default_smileys=add_protocol_smiley(default_smileys, ";)", "wink");

  default_smileys=add_protocol_smiley(default_smileys, ":-D", "biglaugh");
  default_smileys=add_protocol_smiley(default_smileys, ":D", "biglaugh");

  default_smileys=add_protocol_smiley(default_smileys, ":-P", "tongue");
  default_smileys=add_protocol_smiley(default_smileys, ":P", "tongue");

  default_smileys=add_protocol_smiley(default_smileys, ":-p", "tongue");
  default_smileys=add_protocol_smiley(default_smileys, ":p", "tongue");

  default_smileys=add_protocol_smiley(default_smileys, ":-S", "worried");
  default_smileys=add_protocol_smiley(default_smileys, ":S", "worried");
  default_smileys=add_protocol_smiley(default_smileys, ":-s", "worried");
  default_smileys=add_protocol_smiley(default_smileys, ":s", "worried");

  default_smileys=add_protocol_smiley(default_smileys, ":-/", "worried");
  default_smileys=add_protocol_smiley(default_smileys, ":/", "worried");

  default_smileys=add_protocol_smiley(default_smileys, ">:-O", "angry");
  default_smileys=add_protocol_smiley(default_smileys, ">:O", "angry");
  default_smileys=add_protocol_smiley(default_smileys, ">:-o", "angry");
  default_smileys=add_protocol_smiley(default_smileys, ">:o", "angry");

  default_smileys=add_protocol_smiley(default_smileys, ":'(", "cry");
}

gchar * eb_smilify( const char *text, LList *protocol_smileys )
{
  int ipos=0;
  int found;
  int i;
  LList * l=protocol_smileys;
  GString * newstr;
  char * result;

  if ( !iGetLocalPref("do_smiley") )
  {
	  return g_strdup(text);
  }

  newstr=g_string_sized_new(4096);
  
  if (!text) return g_strdup("");
  
  while(text[ipos]!='\0')
  {
    /* ignore anything in < > */
    while(text[ipos] == '<') {
      while(text[ipos] && text[ipos] != '>') {
	if (ipos < strlen(text))
        	g_string_append_c(newstr, text[ipos++]);
      }
      if(text[ipos] == '>') {
	if (ipos < strlen(text))
        	g_string_append_c(newstr, text[ipos++]);
      } else	/* text[ipos] is null */
        break;
    }

    /* ignore anything in the ignore list (http://, ftp://, etc.) */
    for(i=0; no_smileys[i]; i++) {
      int len = strlen(no_smileys[i]);
      if( !strncmp( text + ipos, no_smileys[i], len ) ) {
        g_string_append(newstr, no_smileys[i]);
	ipos += len;
      }
    }

    
    l=protocol_smileys;

    found=0;

    while(l!=NULL)
    {
      protocol_smiley * ps=(protocol_smiley *)l->data;
      if(!strncmp(text+ipos, ps->text, strlen(ps->text)))
      {
	char html_text[128];
	int i = 0, j = 0;
        g_string_append(newstr, "<smiley name=\"");
        g_string_append(newstr, ps->name);
	g_string_append(newstr, "\" alt=\"");
	while (ps->text[i] && j<123) {
		if(ps->text[i]=='>') {
			html_text[j++]='&';
			html_text[j++]='g';
			html_text[j++]='t';
			html_text[j++]=';';
		} else if (ps->text[i]=='<') {
			html_text[j++]='&';
			html_text[j++]='l';
			html_text[j++]='t';
			html_text[j++]=';';
		} else 
			html_text[j++]=ps->text[i];
		i++;
	}
	html_text[j]='\0';
	g_string_append(newstr, html_text);
        g_string_append(newstr, "\">");
        ipos += strlen(ps->text);
        found=1;
        break;
      }
      l=l->next;
    }
    if(!found) { 
	if (ipos < strlen(text))
		g_string_append_c(newstr, text[ipos++]);
    }
  }

  result = newstr->str;
  g_string_free(newstr, FALSE);

  return result;
}

LList * eb_default_smileys(void)
{
  return default_smileys;
}

LList * add_protocol_smiley(LList * list, const char * text, const char * name)
{
  protocol_smiley * psmile;

  psmile=g_new0(protocol_smiley, 1);
  strncpy(psmile->text, text, sizeof(psmile->text));
  strncpy(psmile->name, name, sizeof(psmile->name));
  return l_list_append(list, psmile);
}

LList * add_smiley( LList * list, const char *name, gchar **data )
{
  smiley * psmile;

  psmile=g_new0(smiley, 1);
  strncpy(psmile->name, name, sizeof(psmile->name));
  psmile->pixmap=data;
  return l_list_append(list, psmile);
}

smiley * get_smiley_by_name( const char *name )
{
  smiley * psmile;
  LList * l = smileys;
  while(l != NULL) {
    psmile = (smiley *)(l->data);
    if(!strcmp(psmile->name,name)) 
       return psmile;
    l=l->next;
  }
  return NULL;
}

static void insert_smiley_cb (GtkWidget * widget, smiley_callback_data *data) 
{
	GtkEditable *entry = NULL;
	int pos;
	if (data && data->c_room)
		entry = GTK_EDITABLE(data->c_room->entry);
	else if (data && data->c_window)
		entry = GTK_EDITABLE(data->c_window->entry);
	else {
		eb_debug(DBG_CORE, "smiley_callback_data has no chat* !\n");
		return;
	}
	
	if(entry) {
		char *content = NULL;
		char *smiley = gtk_widget_get_name(widget);
		if (smiley) {
			pos=entry->current_pos;
			gtk_editable_insert_text(entry, 
					smiley, strlen(smiley),
					&pos);
			content = gtk_editable_get_chars(entry, 0, -1);
			gtk_editable_set_position(entry, pos<strlen(content)?pos:strlen(content));
			g_free(content);
		}
	}
	
	if (data->c_room && data->c_room->smiley_window) {
		gtk_widget_destroy(data->c_room->smiley_window);
		data->c_room->smiley_window = NULL;
	} else if (data->c_window && data->c_window->smiley_window) {
		gtk_widget_destroy(data->c_window->smiley_window);
		data->c_window->smiley_window = NULL;		
	}
	else 
		eb_debug(DBG_CORE, "smiley_window is null * !\n");
}

void show_smileys_cb (smiley_callback_data *data) {
	eb_local_account *account;
	LList *smileys = NULL;
	protocol_smiley * msmiley = NULL;
	GtkWidget * smileys_table = NULL;
	GtkWidget * button = NULL;
	GtkWidget *iconwid;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkWidget *smiley_window;
	LList     *done = NULL;
	int len = 0, real_len = 0, x=-1, y=0;
	int win_w=0, win_h=0, w, h;
	int win_x, win_y;
	
	/* close popup if open */
	if (data->c_room && data->c_room->smiley_window) {
		gtk_widget_destroy(data->c_room->smiley_window);
		data->c_room->smiley_window = NULL;
		return;
	} else if (data->c_window && data->c_window->smiley_window) {
		gtk_widget_destroy(data->c_window->smiley_window);
		data->c_window->smiley_window = NULL;		
		return;
	}

	if (data && data->c_room)
		account = data->c_room->local_user;
	else if (data && data->c_window)
		account = data->c_window->local_user;
	else {
		eb_debug(DBG_CORE, "no chat* in data !\n");
		return;
	}
	
	if (account && RUN_SERVICE(account)->get_smileys)
		smileys = RUN_SERVICE(account)->get_smileys();
	else 
		return;
	len = l_list_length(smileys);
	smileys_table = gtk_table_new(5,len/5 +((len%5==0)?1:2),TRUE);
	for(;smileys;smileys=smileys->next) {
		LList * l;
		gboolean already_done = FALSE;
		smiley * dsmile = NULL;
		msmiley = (smileys->data);
		for(l=done; l; l=l->next) {
			if(!strcmp((char*)l->data, msmiley->name)) {
				already_done = TRUE;
				break;
			}
		}

		if(already_done)
			continue;

		done = l_list_append(done, msmiley->name);

		dsmile = get_smiley_by_name(msmiley->name);
		if(dsmile != NULL) {
			GtkWidget *parent = NULL;
			if (data && data->c_room)
				parent = data->c_room->window;
			else if (data && data->c_window)
				parent = data->c_window->window;
			icon = gdk_pixmap_create_from_xpm_d(parent->window, &mask, NULL, dsmile->pixmap);
			iconwid = gtk_pixmap_new(icon, mask);
			sscanf (dsmile->pixmap [0], "%d %d", &w, &h);
			if(x<5) {
				x++;
				if(y==0) 
					win_h+=h+2;
			}
			if(x==5) {
				y++;
				x=0;
				if(x==0) 
					win_w+=w+2;
			}
			gtk_widget_show(iconwid);
			button = gtk_button_new();
			gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
			gtk_container_add(GTK_CONTAINER(button), iconwid);
			gtk_widget_show(button);
			gtk_widget_set_name(button,msmiley->text);
			gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (insert_smiley_cb), data);
			gtk_table_attach(GTK_TABLE(smileys_table),
			button,
			y,y+1,
			x,x+1,
			GTK_FILL, GTK_FILL, 0, 0);
			real_len++;
		}
	}

	l_list_free(done);
	done = NULL;
	gtk_table_resize(GTK_TABLE(smileys_table), 5,real_len/5 +((real_len%5==0)?0:1));

	smiley_window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_modal(GTK_WINDOW(smiley_window), FALSE);
	gtk_window_set_wmclass(GTK_WINDOW(smiley_window), "ayttm-chat", "Ayttm");
	gtk_window_set_title(GTK_WINDOW(smiley_window), "Smileys");
	gtk_window_set_policy(GTK_WINDOW(smiley_window), FALSE, FALSE, FALSE);
	gtk_widget_realize(smiley_window);

	gtk_widget_show(smiley_window);

	gtk_container_add(GTK_CONTAINER(smiley_window), smileys_table);
	gtk_widget_show(smileys_table);

	/* move the window a bit after the cursor and in the screen */
	gdk_window_get_pointer (NULL, &win_x, &win_y, NULL);
	win_x += 5; win_y += 5;
	while ((win_x)+win_w > gdk_screen_width() - 30)
		win_x -= 20;
	while ((win_y)+win_h > gdk_screen_height() - 30)
		win_y -= 20;
	gdk_window_move_resize(smiley_window->window, win_x, win_y, win_w, win_h);
	
	if (data && data->c_room)
		data->c_room->smiley_window = smiley_window;
	else if (data && data->c_window)
		data->c_window->smiley_window = smiley_window;

}
