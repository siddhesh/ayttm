/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
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

#include "intl.h"

#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>

#include "prefs_window.h"
#include "dialog.h"
#include "service.h"
#include "util.h"
#include "libproxy/libproxy.h"
#include "sound.h"
#include "defaults.h"
#include "value_pair.h"
#include "gtk_globals.h"
#include "status.h"
#include "plugin.h"
#include "file_select.h"

#ifdef HAVE_ISPELL
#include "gtk/gtkspell.h"
#endif

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"


static GtkWidget	*s_prefs_window = NULL;
static struct prefs	*s_prefs = NULL;


/*
 * Preferences Dialog
 */

static void build_general_prefs(GtkWidget *); /* Sat Apr 28 2001 S. K. Mandal*/
static void build_sound_tab(GtkWidget *);
static void build_sound_prefs(GtkWidget *);
static void build_logs_prefs(GtkWidget *);
static void build_proxy_prefs(GtkWidget *);
static void build_layout_prefs(GtkWidget *);
static void build_chat_prefs(GtkWidget *);
#ifdef HAVE_ICONV_H
static void build_encoding_prefs(GtkWidget *);
#endif
static void build_modules_prefs(GtkWidget *);
static void ok_callback(GtkWidget * widget, gpointer data);
static void cancel_callback(GtkWidget * widget, gpointer data);
static void destroy(GtkWidget * widget, gpointer data);


void	ayttm_prefs_window_create( struct prefs *inPrefs )
{
     if ( s_prefs_window == NULL )
     {
	  GtkWidget	*hbox = NULL;
	  GtkWidget	*button = NULL;
	  GtkWidget	*hbox2 = NULL;
	  GtkWidget	*prefs_vbox = NULL;

	  
	  s_prefs = inPrefs;
	  
	  prefs_vbox = gtk_vbox_new(FALSE, 5);
	  s_prefs_window = gtk_window_new(GTK_WINDOW_DIALOG);
	  gtk_window_set_position(GTK_WINDOW(s_prefs_window), GTK_WIN_POS_MOUSE);
	  gtk_widget_realize(s_prefs_window);
	  gtk_window_set_title(GTK_WINDOW(s_prefs_window), _("Ayttm Preferences"));
	  eb_icon(s_prefs_window->window);
	  gtk_signal_connect(GTK_OBJECT(s_prefs_window), "destroy",
			      GTK_SIGNAL_FUNC(destroy), NULL);

	  {
	       GtkWidget *prefs_note = gtk_notebook_new();
  
		   build_general_prefs(prefs_note);
	       build_logs_prefs(prefs_note);
	       build_sound_prefs(prefs_note);
	       build_layout_prefs(prefs_note);
	       build_chat_prefs(prefs_note);

	       gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_note, FALSE, FALSE, 0);
	       build_proxy_prefs(prefs_note);
#ifdef HAVE_ICONV_H
	       build_encoding_prefs(prefs_note);
#endif
	       build_modules_prefs(prefs_note);
	  }

	  /* OK Button */
	  hbox2 = gtk_hbox_new(TRUE, 5);
	  gtk_widget_set_usize(hbox2, 200,25);

	  button = do_icon_button(_("OK"), ok_xpm, s_prefs_window);

	  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			     ok_callback, s_prefs_window);

	  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				     GTK_SIGNAL_FUNC (gtk_widget_destroy),
				     GTK_OBJECT (s_prefs_window));
	  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 5);
	  gtk_widget_show(button);

	  /* Cancel Button */
	  button = do_icon_button(_("Cancel"), cancel_xpm, s_prefs_window);

	  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			     cancel_callback, s_prefs_window);

	  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				     GTK_SIGNAL_FUNC (gtk_widget_destroy),
				     GTK_OBJECT (s_prefs_window));

	  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 5);
	  gtk_widget_show(button);

	  /* End Buttons */
	  hbox = gtk_hbox_new(FALSE, 5);

	  gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 5);
	  gtk_widget_show(hbox2);

	  gtk_box_pack_start(GTK_BOX(prefs_vbox), hbox, FALSE, FALSE, 5);
	  gtk_widget_show(hbox);
	  gtk_widget_show(prefs_vbox);
	  gtk_container_add(GTK_CONTAINER(s_prefs_window), prefs_vbox);

	  gtk_widget_show(s_prefs_window);
     }
}

/*
 * General Preferences Tab
 * Sat Apr 28 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
 *
 */

static GtkWidget	*font_size_entry = NULL;
static GtkWidget	*alternate_browser_entry = NULL;
static GtkWidget	*g_browser_browse_button = NULL;
#ifdef HAVE_ISPELL
static GtkWidget	*dictionary_entry = NULL;
#endif

#ifdef HAVE_ISPELL
static void	set_use_spell(GtkWidget * w, int * data);
#endif

static void set_browser_path(char * selected_filename, gpointer data)
{
	if(!selected_filename)
		return;
	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), selected_filename);
}

static void get_alt_browser_path (GtkWidget * t_browser_browse_button, gpointer userdata)
{
     eb_debug(DBG_CORE, "Just entered get_alt_browser_path\n");
     eb_do_file_selector(gtk_entry_get_text(GTK_ENTRY(alternate_browser_entry)), 
		     _("Select your browser"), set_browser_path, NULL);
}

static void set_use_alternate_browser(GtkWidget * w, int * data)
{
     if ( s_prefs->general.use_alternate_browser == 0 )
     {
	  gtk_widget_set_sensitive(alternate_browser_entry, FALSE);
	  gtk_widget_set_sensitive(g_browser_browse_button, FALSE);
     }
     else
     {
	  gtk_widget_set_sensitive(alternate_browser_entry, TRUE);
	  gtk_widget_set_sensitive(g_browser_browse_button, TRUE);
     }
}

static void build_general_prefs(GtkWidget *prefs_note) {
    GtkWidget	*vbox = NULL;
    GtkWidget	*brbutton = NULL;
    GtkWidget	*label = NULL;
    GtkWidget	*hbox = NULL;


    vbox = gtk_vbox_new(FALSE, 0);

#ifdef HAVE_ISPELL
    hbox = gtk_hbox_new(FALSE, 0);
    dictionary_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(dictionary_entry), s_prefs->general.spell_dictionary);

    brbutton = eb_button(_("Use spell checking - dictionary (empty for default):"), &s_prefs->general.do_spell_checking, hbox);
    gtk_box_pack_start(GTK_BOX(hbox), dictionary_entry, TRUE, TRUE, 10);
    gtk_widget_show(dictionary_entry);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    if(s_prefs->general.do_spell_checking == 0)
    {
	gtk_widget_set_sensitive(dictionary_entry, FALSE);
    }
    gtk_signal_connect(GTK_OBJECT(brbutton), "clicked",
	    GTK_SIGNAL_FUNC(set_use_spell), (gpointer)brbutton);
#endif

    eb_button(_("Enable debug messages"), &s_prefs->general.do_ayttm_debug, vbox);

    alternate_browser_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), s_prefs->general.alternate_browser);

    brbutton = eb_button(_("Use alternate browser"), &s_prefs->general.use_alternate_browser, vbox);
    gtk_signal_connect(GTK_OBJECT(brbutton), "clicked",
	    GTK_SIGNAL_FUNC(set_use_alternate_browser), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(_("Alternate browser command\n(%s will be replaced by the URL):"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    gtk_box_pack_start(GTK_BOX(hbox), alternate_browser_entry, TRUE, TRUE, 10);
    gtk_widget_show(alternate_browser_entry);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    g_browser_browse_button = gtk_button_new_with_label (_("Browse"));
    gtk_signal_connect(GTK_OBJECT(g_browser_browse_button), "clicked", 
		    GTK_SIGNAL_FUNC(get_alt_browser_path), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), g_browser_browse_button, FALSE, FALSE, 5);
    gtk_widget_show (g_browser_browse_button);

    gtk_widget_show(vbox);

    if(s_prefs->general.use_alternate_browser == 0)
    {
	gtk_widget_set_sensitive(alternate_browser_entry, FALSE);
	gtk_widget_set_sensitive(g_browser_browse_button, FALSE);
    }

    gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox,
	    gtk_label_new(_("General")));
}


static void	set_general_prefs( void )
{     
#ifdef HAVE_ISPELL
	if ( s_prefs->general.do_spell_checking )
	{
		char	*newspell = NULL;

	    newspell = (dictionary_entry != NULL && strlen(gtk_entry_get_text(GTK_ENTRY(dictionary_entry))))
			    ?gtk_entry_get_text(GTK_ENTRY(dictionary_entry)):"";

	    if ( strcmp( newspell, s_prefs->general.spell_dictionary ) )
		{
			gtkspell_stop();
	    }
    }
    if ( dictionary_entry != NULL )
		strncpy( s_prefs->general.spell_dictionary, gtk_entry_get_text(GTK_ENTRY(dictionary_entry)), MAX_PREF_LEN );
#endif
    
	if( alternate_browser_entry != NULL )
 		strncpy( s_prefs->general.alternate_browser, gtk_entry_get_text(GTK_ENTRY(alternate_browser_entry)), MAX_PREF_LEN );
}

#ifdef HAVE_ISPELL
static void set_use_spell(GtkWidget * w, int * data)
{
     if(s_prefs->general.do_spell_checking == 0)
     {
	  gtk_widget_set_sensitive(dictionary_entry, FALSE);
     }
     else
     {
	  gtk_widget_set_sensitive(dictionary_entry, TRUE);
     }
}
#endif

static void destroy_general_prefs()
{
	if(alternate_browser_entry)
	{
		gtk_widget_destroy(alternate_browser_entry);
		gtk_widget_destroy(g_browser_browse_button);
		alternate_browser_entry = NULL;
		g_browser_browse_button = NULL;
	}
#ifdef HAVE_ISPELL
	if(dictionary_entry)
	{
		gtk_widget_destroy(dictionary_entry);
		dictionary_entry = NULL;
	}
#endif
}


/*
 * Logs Preferences Tab
 */

static void build_logs_prefs(GtkWidget *prefs_note)
{
     GtkWidget *vbox = gtk_vbox_new(FALSE, 5);

     eb_button(_("Save all conversations to logfiles"), &s_prefs->logging.do_logging, vbox);
     eb_button(_("Restore last conversation when opening a chat window"), &s_prefs->logging.do_restore_last_conv, vbox);
     
	 gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Logs")));
}

static void	set_logs_prefs( void )
{
	/* nothing to do here... */
}

/*
 * Layout Preferences Tab
 */

/*
  Callback function for setting element of Tabbed Chat Orientation radio
  button group.  Ignores w and sets global to variable
*/
static void set_tco_element (GtkWidget * w, int data)
{
	s_prefs->layout.do_tabbed_chat_orient = data;
}

static void build_layout_prefs(GtkWidget *prefs_note) 
{
	GtkWidget	*vbox = NULL;
	GtkWidget	*hbox = NULL;
	GSList		*group = NULL;
	GtkWidget	*label = NULL;
	int			save_orientation = 0;

     vbox = gtk_vbox_new(FALSE, 5);

     /* Values */
     eb_button(_("Use tabs in chat windows"), &s_prefs->layout.do_tabbed_chat, vbox);

     /*
       Radio button group for tabbed chat orientation
     */

     /* Setup box to hold it */
     hbox = gtk_hbox_new(FALSE, 5);

     /* setup intro label */
     label = gtk_label_new (_("Tab position:"));
     gtk_widget_show (label);
     gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 5);
	 
	 /* Because it seems that the 'clicked' function is called when we create the second radio button [!],
	 	we must save our current value and restore it after the creation of the radio buttons
	*/
	
	save_orientation = s_prefs->layout.do_tabbed_chat_orient;
	
     /* Create buttons */
     group = eb_radio (group, _("Bottom"), s_prefs->layout.do_tabbed_chat_orient, 0, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Top"),    s_prefs->layout.do_tabbed_chat_orient, 1, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Left"),   s_prefs->layout.do_tabbed_chat_orient, 2, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Right"),  s_prefs->layout.do_tabbed_chat_orient, 3, hbox,
		       set_tco_element);

	s_prefs->layout.do_tabbed_chat_orient = save_orientation;
	
     /* Put it in the vbox and make it visible */
     gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
     gtk_widget_show (hbox);

     gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Layout")));
}

static void	set_layout_prefs( void )
{
	/* nothing to do here... */
}

/*
 * Chat Preferences Tab
 */

static GdkDeviceKey 	local_accel_prev_tab;
static GdkDeviceKey 	local_accel_next_tab;

static guint accel_change_handler_id = 0;


/* apparently all signal handlers are supposed to be boolean... */
static gboolean newkey_callback (GtkWidget *keybutton, GdkEventKey *event, int *userdata) {
	GtkWidget *label = GTK_BIN(keybutton)->child;
	/* remove stupid things like.. numlock scrolllock and capslock
	 * mod1 = alt, mod2 = numlock, mod3 = modeshift/altgr, mod4 = meta, mod5 = scrolllock */

	int state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
	if (state != 0) {
			/* this unfortunately was the only way I could do this without using
			 * a key release event
			 */
		switch(event->keyval) {
				case GDK_Shift_L:
				case GDK_Shift_R:
				case GDK_Control_L:
				case GDK_Control_R:
				case GDK_Caps_Lock:
				case GDK_Shift_Lock:
				case GDK_Meta_L:
				case GDK_Meta_R:
				case GDK_Alt_L:
				case GDK_Alt_R:
				case GDK_Super_L:
				case GDK_Super_R:
				case GDK_Hyper_L:
				case GDK_Hyper_R:
						/* don't let the user set a modifier as a hotkey */
						break;
				default:
					userdata[0] = event->keyval;
					userdata[1] = state;
					gtk_label_set_text(GTK_LABEL(label), gtk_accelerator_name(userdata[0], userdata[1]));
					gtk_signal_disconnect(GTK_OBJECT(keybutton), accel_change_handler_id);
					gtk_grab_remove(keybutton);
					accel_change_handler_id = 0;
		}
	}
	/* eat the event and make focus keys (arrows) not change the focus */
	return gtk_true();
}

static void getnewkey (GtkWidget * keybutton, gpointer userdata) {
	GtkWidget *label = GTK_BIN(keybutton)->child;

	if(accel_change_handler_id == 0) {
		gtk_label_set_text(GTK_LABEL(label), _("Please press new key and modifier now."));

		gtk_object_set_data(GTK_OBJECT(keybutton), "accel", userdata);

		/* it's sad how this works: It grabs the events in the event mask
		 * of the widget the mouse is over, NOT the grabbed widget.
		 * Oh, and persistantly clicking makes the grab go away...
		 */
		gtk_grab_add(keybutton);

		accel_change_handler_id = gtk_signal_connect_after (GTK_OBJECT(keybutton),
					"key_press_event",
					GTK_SIGNAL_FUNC (newkey_callback),
					userdata);
	}
}

static void add_key_set(char *labelString,
					GdkDeviceKey *keydata,
					GtkWidget *vbox)
{
     GtkWidget *hbox;
     GtkWidget *label;
     GtkWidget *button;
     char *clabel=NULL;
     hbox = gtk_hbox_new(FALSE, 2);
     gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

     label = gtk_label_new (labelString);
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
     gtk_widget_set_usize(label, 275, 10);
	 gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
     gtk_widget_show (label);

     clabel=gtk_accelerator_name(keydata->keyval, keydata->modifiers);
     button = gtk_button_new_with_label(clabel);
     g_free(clabel);

     gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			GTK_SIGNAL_FUNC(getnewkey), 
			(gpointer)keydata);
     gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
     gtk_widget_show (button);

     gtk_widget_show(hbox);
}

static void build_chat_prefs(GtkWidget *prefs_note)
{
     GtkWidget *vbox;
     GtkWidget *hbox;
     GtkWidget *label;
     char buff[10];   
     vbox = gtk_vbox_new(FALSE, 5);

     /* Values */
	 accel_change_handler_id = 0;

     eb_button(_("Send idle/away status to servers"), &s_prefs->chat.do_send_idle_time, vbox);
     eb_button(_("Raise chat-window when receiving a message"), &s_prefs->chat.do_raise_window, vbox);
     eb_button(_("Ignore unknown people"), &s_prefs->chat.do_ignore_unknown, vbox);
     eb_button(_("Ignore foreground Colors"), &s_prefs->chat.do_ignore_fore, vbox);
     eb_button(_("Ignore background Colors"), &s_prefs->chat.do_ignore_back, vbox);
     eb_button(_("Ignore fonts"), &s_prefs->chat.do_ignore_font, vbox);

    hbox = gtk_hbox_new(FALSE, 0);

    label = gtk_label_new(_("Font size: "));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    font_size_entry = gtk_entry_new();
    gtk_widget_set_usize(font_size_entry, 60, -1);
    g_snprintf(buff, 10, "%d", s_prefs->chat.font_size);

    gtk_entry_set_text(GTK_ENTRY(font_size_entry), buff);
    gtk_box_pack_start(GTK_BOX(hbox), font_size_entry,
		       FALSE, FALSE, 0);
    gtk_widget_show(font_size_entry);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
	gtk_accelerator_parse( s_prefs->chat.accel_next_tab, &(local_accel_next_tab.keyval), &(local_accel_next_tab.modifiers) );
	gtk_accelerator_parse( s_prefs->chat.accel_prev_tab, &(local_accel_prev_tab.keyval), &(local_accel_prev_tab.modifiers) );

	add_key_set(_("Hotkey to go to previous tab (requires a modifier)"), &local_accel_prev_tab, vbox);
	add_key_set(_("Hotkey to go to next tab (requires a modifier)"), &local_accel_next_tab, vbox);

     gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Chat")));
}

static void	set_chat_prefs( void )
{
	char *ptr = NULL;

	if ( font_size_entry != NULL )
	{
		ptr = gtk_entry_get_text(GTK_ENTRY(font_size_entry));
		s_prefs->chat.font_size = atoi(ptr);
	}

	ptr = gtk_accelerator_name( local_accel_next_tab.keyval, local_accel_next_tab.modifiers );
	strncpy( s_prefs->chat.accel_next_tab, ptr, MAX_PREF_LEN );
	g_free(ptr);
	 
	ptr = gtk_accelerator_name( local_accel_prev_tab.keyval, local_accel_prev_tab.modifiers );
	strncpy( s_prefs->chat.accel_prev_tab, ptr, MAX_PREF_LEN );
	g_free(ptr);
}

static void destroy_chat_prefs (void)
{
	gtk_widget_destroy(font_size_entry);
	font_size_entry=NULL;
}

/*
 * Proxies Preferences Tab
 */

static GtkWidget * proxy_server_entry = NULL;
static GtkWidget * proxy_port_entry = NULL;
static GtkWidget * proxy_user_entry = NULL;
static GtkWidget * proxy_password_entry = NULL;

static void set_proxy_type(GtkWidget *w, int *data);

static void build_proxy_prefs(GtkWidget *prefs_note)
{
     GtkWidget *label;
     GtkWidget *button;
     GtkWidget *hbox;
     GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
     char buff[10];   

     proxy_server_entry = gtk_entry_new();
     proxy_port_entry = gtk_entry_new();
     proxy_user_entry = gtk_entry_new();
     proxy_password_entry = gtk_entry_new();

     label = gtk_label_new(_("Warning: Not all services are available through\nproxy, please see the README for details."));
     gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);
     button = gtk_radio_button_new_with_label(NULL, _("No proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_NONE);

     if(s_prefs->advanced.proxy_type == PROXY_NONE)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use HTTP Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_HTTP);

     if(s_prefs->advanced.proxy_type == PROXY_HTTP)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }


     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use SOCKS4 Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_SOCKS4);

     if(s_prefs->advanced.proxy_type == PROXY_SOCKS4)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use SOCKS5 Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_SOCKS5);

     if(s_prefs->advanced.proxy_type == PROXY_SOCKS5)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Proxy Server:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

	 gtk_entry_set_text(GTK_ENTRY(proxy_server_entry), s_prefs->advanced.proxy_host);

     gtk_box_pack_start(GTK_BOX(hbox), proxy_server_entry, TRUE, TRUE, 10);
     gtk_widget_show(proxy_server_entry);

     label = gtk_label_new(_("Proxy Port:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     g_snprintf(buff, 10, "%d", s_prefs->advanced.proxy_port);
     gtk_entry_set_text(GTK_ENTRY(proxy_port_entry), buff);
     gtk_box_pack_start(GTK_BOX(hbox), proxy_port_entry, FALSE, FALSE, 0);
     gtk_widget_show(proxy_port_entry);

     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
     gtk_widget_show(hbox);

	 eb_button(_("Proxy requires authentication"), &s_prefs->advanced.do_proxy_auth, vbox);

     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Proxy User ID:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

	  gtk_entry_set_text(GTK_ENTRY(proxy_user_entry), s_prefs->advanced.proxy_user);

     gtk_box_pack_start(GTK_BOX(hbox), proxy_user_entry, TRUE, TRUE, 10);
     gtk_widget_show(proxy_user_entry);

     label = gtk_label_new(_("Proxy Password:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

	  gtk_entry_set_text(GTK_ENTRY(proxy_password_entry), s_prefs->advanced.proxy_password);

	 gtk_entry_set_visibility(GTK_ENTRY(proxy_password_entry), FALSE);

     gtk_box_pack_start(GTK_BOX(hbox), proxy_password_entry, FALSE, FALSE, 0);
     gtk_widget_show(proxy_password_entry);

     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
     gtk_widget_show(hbox);

     if(s_prefs->advanced.proxy_type == PROXY_NONE)
     {
	  gtk_widget_set_sensitive(proxy_server_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_port_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_user_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_password_entry, FALSE);
     }

     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Proxy")));
     gtk_widget_show(vbox);
}

static void destroy_proxy()
{
	if(proxy_server_entry == NULL) return;
	
	gtk_widget_destroy(proxy_server_entry);
	gtk_widget_destroy(proxy_port_entry);
	gtk_widget_destroy(proxy_user_entry);
	gtk_widget_destroy(proxy_password_entry);

	proxy_server_entry=NULL;
	proxy_port_entry=NULL;
	proxy_user_entry=NULL;
	proxy_password_entry=NULL;
}

static void	set_proxy_prefs( void )
{
	strncpy( s_prefs->advanced.proxy_host, gtk_entry_get_text(GTK_ENTRY(proxy_server_entry)), MAX_PREF_LEN );
	s_prefs->advanced.proxy_port = atol( gtk_entry_get_text(GTK_ENTRY(proxy_port_entry)) );
	
	strncpy( s_prefs->advanced.proxy_user, gtk_entry_get_text(GTK_ENTRY(proxy_user_entry)), MAX_PREF_LEN );
	strncpy( s_prefs->advanced.proxy_password, gtk_entry_get_text(GTK_ENTRY(proxy_password_entry)), MAX_PREF_LEN );
}


static void set_proxy_type(GtkWidget * w, int * data)
{
     s_prefs->advanced.proxy_type = (int)data;
     if(s_prefs->advanced.proxy_type == PROXY_NONE)
     {
	  gtk_widget_set_sensitive(proxy_server_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_port_entry, FALSE);
     }
     else
     {
	  gtk_widget_set_sensitive(proxy_server_entry, TRUE);
	  gtk_widget_set_sensitive(proxy_port_entry, TRUE);
     }
}


/*
 * Encoding Preferences Tab
 */


#ifdef HAVE_ICONV_H
#define ENCODE_LEN 64

static GtkWidget * local_encoding_entry = NULL;
static GtkWidget * remote_encoding_entry = NULL;

static void set_use_of_recoding(GtkWidget * w, int * data);

/* Encoding conversion Configuration tabs */
static void build_encoding_prefs(GtkWidget * prefs_note)
{
     GtkWidget * hbox;
     GtkWidget * label;
     GtkWidget * button;
     GtkWidget *vbox = gtk_vbox_new(FALSE, 5);

     local_encoding_entry = gtk_entry_new();
     remote_encoding_entry = gtk_entry_new();

     label = gtk_label_new(_("Warning: conversion is made using iconv() wich is UNIX98 standard and we hope you have it :-)\nFor list of possible encodings run 'iconv --list'."));
     gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     button = eb_button(_("Use recoding in conversations"), &s_prefs->advanced.use_recoding, vbox);

     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_use_of_recoding), (gpointer)button);

     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Local encoding:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     gtk_entry_set_text(GTK_ENTRY(local_encoding_entry), cGetLocalPref("local_encoding"));
     gtk_box_pack_start(GTK_BOX(hbox), local_encoding_entry, TRUE, TRUE, 10);
     gtk_widget_show(local_encoding_entry);
     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);
     gtk_widget_show(hbox);


     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Remote encoding:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     gtk_entry_set_text(GTK_ENTRY(remote_encoding_entry), cGetLocalPref("remote_encoding"));
     gtk_box_pack_start(GTK_BOX(hbox), remote_encoding_entry, TRUE, TRUE, 10);
     gtk_widget_show(remote_encoding_entry);
     gtk_widget_show(hbox);

     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Recoding")));
     gtk_widget_show(vbox);

     if(s_prefs->advanced.use_recoding == 0)
     {
	  gtk_widget_set_sensitive(local_encoding_entry, FALSE);
	  gtk_widget_set_sensitive(remote_encoding_entry, FALSE);
     }

}

static void destroy_encodings()
{
     if(local_encoding_entry==NULL) return;
     gtk_widget_destroy(local_encoding_entry);
     gtk_widget_destroy(remote_encoding_entry);
     local_encoding_entry=NULL;
     remote_encoding_entry=NULL;
}

static void set_use_of_recoding(GtkWidget * w, int * data)
{
     if(s_prefs->advanced.use_recoding == 0)
     {
	  gtk_widget_set_sensitive(local_encoding_entry, FALSE);
	  gtk_widget_set_sensitive(remote_encoding_entry, FALSE);
     }
     else
     {
	  gtk_widget_set_sensitive(local_encoding_entry, TRUE);
	  gtk_widget_set_sensitive(remote_encoding_entry, TRUE);
     }
}

static void	set_encoding_prefs( void )
{
	strncpy( s_prefs->advanced.local_encoding, gtk_entry_get_text(GTK_ENTRY(local_encoding_entry)), MAX_PREF_LEN );
	strncpy( s_prefs->advanced.remote_encoding, gtk_entry_get_text(GTK_ENTRY(remote_encoding_entry)), MAX_PREF_LEN );
}

#endif

/*
 * Plugin Preferences Tab
 */

enum {
	PLUGIN_TYPE_COL = 0,
	PLUGIN_BRIEF_COL,
	PLUGIN_STATUS_COL,
	PLUGIN_VERSION_COL,
	PLUGIN_DATE_COL,
	PLUGIN_PATH_COL,
	PLUGIN_ERROR_COL
};

const int	s_plugin_info_num_columns = 7;

static GtkWidget	*plugin_prefs_win = NULL;

static void	build_modules_list( GtkWidget * module_list )
{
	const char	*row_data[s_plugin_info_num_columns];	
	LList		*module_info = s_prefs->module.module_info;
	
	
	for ( ; module_info != NULL; module_info = module_info->next )
	{
		module_pref		*pref_info = module_info->data;
	
		if ( pref_info == NULL )
			continue;
					
		eb_debug( DBG_CORE, "Adding plugin %s\n", pref_info->file_name );
				
		row_data[PLUGIN_TYPE_COL] = (pref_info->module_type == NULL) ? "" : pref_info->module_type;
		row_data[PLUGIN_BRIEF_COL] = (pref_info->brief_desc == NULL) ? "" : pref_info->brief_desc;
		row_data[PLUGIN_STATUS_COL] = (pref_info->loaded_status == NULL) ? "" : pref_info->loaded_status;
		row_data[PLUGIN_VERSION_COL] = (pref_info->version == NULL) ? "" : pref_info->version;
		row_data[PLUGIN_DATE_COL] = (pref_info->date == NULL) ? "" : pref_info->date;
		row_data[PLUGIN_PATH_COL] = (pref_info->file_name == NULL) ? "" : pref_info->file_name;
		row_data[PLUGIN_ERROR_COL] = (pref_info->status_desc == NULL) ? "" : pref_info->status_desc;
			
		gtk_clist_append( GTK_CLIST(module_list), (char **)row_data );
	}
}

static void update_plugin_prefs(GtkWidget * w, input_list *prefs)
{
	if(prefs) {
		eb_debug(DBG_MOD, "updating prefs\n");
		eb_input_accept(prefs);
	}

	gtk_widget_destroy(plugin_prefs_win);
	plugin_prefs_win = NULL;
}

static void cancel_plugin_prefs( GtkWidget *w, input_list *prefs )
{
	if(prefs) {
		eb_debug(DBG_MOD, "cancelling prefs\n");
		eb_input_cancel( prefs );
	}

	gtk_widget_destroy( plugin_prefs_win );
	plugin_prefs_win = NULL;
}

static void destroy_plugin_prefs(GtkWidget *w, void * data)
{
	gtk_widget_destroy( plugin_prefs_win );
	plugin_prefs_win = NULL;
}

/* Sort the clist based upon the column passed */
static void plugin_column_sort(GtkCList *clist, int column, gpointer userdata)
{
	eb_debug(DBG_CORE, "Sorting on column: %i\n", column);
	gtk_clist_set_sort_column(GTK_CLIST(clist), column);
	gtk_clist_sort(GTK_CLIST(clist));
}

static void plugin_selected(GtkCList *clist, int row, int column, GdkEventButton *event, gpointer userdata)
{
	char		*plugin_path=NULL;
	module_pref	*pref_info;

	if ( !event )
		return;
		
	eb_debug(DBG_CORE, "row: %i column: %i button: %i\n", row, column, event->button);

	gtk_clist_get_text(GTK_CLIST(clist), row, PLUGIN_PATH_COL, &plugin_path);
	eb_debug(DBG_CORE, "plugin: %s\n", plugin_path);

	pref_info = ayttm_prefs_find_module_by_name( s_prefs, plugin_path );

	if ( !pref_info )
	{
		fprintf( stderr, "Unable to find plugin %s in plugin list!\n", plugin_path );
		return;
	}
	
	if ( event->button == 1 )
	{
		GString		*windowName = g_string_new( NULL );
		const char	*pluginDescr = _("Plugin");

		if ( pref_info->service_name != NULL )
			pluginDescr = pref_info->service_name;

		windowName = g_string_append( windowName, pluginDescr );
		windowName = g_string_append( windowName, _(" Preferences") );

		eb_debug(DBG_MOD, "Setting prefs for %s\n", plugin_path);

		/* Create the window if it doesn't exist. */
		if ( plugin_prefs_win == NULL )
		{
			plugin_prefs_win = gtk_window_new( GTK_WINDOW_DIALOG );
			gtk_widget_realize( plugin_prefs_win );

			gtk_window_set_position( GTK_WINDOW(plugin_prefs_win), GTK_WIN_POS_MOUSE) ;
			gtk_window_set_policy( GTK_WINDOW(plugin_prefs_win), FALSE, FALSE, TRUE );

			gtk_container_border_width( GTK_CONTAINER(plugin_prefs_win), 5 );

			gtk_signal_connect(GTK_OBJECT(plugin_prefs_win), "destroy",
					   GTK_SIGNAL_FUNC(destroy_plugin_prefs), NULL );
		}
		else
		{
			/* otherwise just destroy it's current contents */
			gtk_widget_destroy( GTK_BIN(plugin_prefs_win)->child );
		}

		gtk_window_set_title( GTK_WINDOW(plugin_prefs_win), windowName->str );

		{
		GtkWidget	*button = NULL;
		GtkWidget	*hbox = NULL;
		GtkWidget	*label = NULL;
		GtkWidget	*iconwid = NULL;
		GdkPixmap	*icon = NULL;
		GdkBitmap	*mask = NULL;
		GtkWidget	*buttonHBox = gtk_hbox_new( TRUE, 5 );
		GtkWidget	*vbox = gtk_vbox_new( FALSE, 5 );

		/* Show the plugin prefs */
		if ( pref_info->pref_list != NULL )
		{
			eb_input_render( pref_info->pref_list, vbox );
		}
		else
		{
			GtkLabel	*label = GTK_LABEL(gtk_label_new( NULL ));
			GString		*labelText = g_string_new( _("No preferences for ") );
			int		newWidth = 0;
			int		newHeight = 0;

			if ( pref_info->service_name != NULL )
			{
				labelText = g_string_append( labelText, pref_info->service_name );
			}
			else
			{
				labelText = g_string_append( labelText, "'" );
				labelText = g_string_append( labelText, pref_info->brief_desc );
				labelText = g_string_append( labelText, "'" );
			}

			gtk_label_set_text( label, labelText->str );
			gtk_widget_show( GTK_WIDGET(label) );

			/* adjust to a reasonable height if necessary */
			newHeight = GTK_WIDGET(label)->allocation.height;
			if ( newHeight < 50 )
				newHeight = 50;

			gtk_widget_set_usize( GTK_WIDGET(label), newWidth, newHeight );

			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(label), FALSE, FALSE, 2);

			eb_debug(DBG_MOD, "No prefs defined for %s\n", plugin_path);

			g_string_free( labelText, TRUE );
		}


		gtk_widget_set_usize( buttonHBox, 200, 25 );
		gtk_widget_show( buttonHBox );

		/* OK Button*/
		hbox = gtk_hbox_new( FALSE, 5 );

		icon = gdk_pixmap_create_from_xpm_d( plugin_prefs_win->window, &mask, NULL, ok_xpm );
		iconwid = gtk_pixmap_new( icon, mask );
		label = gtk_label_new( _("Ok") );
		button = gtk_button_new();

		gtk_box_pack_start( GTK_BOX(hbox), iconwid, FALSE, FALSE, 2 );
		gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 2 );

		gtk_widget_show( iconwid );
		gtk_widget_show( label );
		gtk_widget_show( hbox );
		gtk_widget_show( button );

		gtk_signal_connect( GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(update_plugin_prefs), (gpointer)pref_info->pref_list );

		gtk_container_add( GTK_CONTAINER(button), hbox );

		gtk_box_pack_start( GTK_BOX(buttonHBox), button, TRUE, TRUE, 5 );

		/*Cancel Button*/
		hbox = gtk_hbox_new( FALSE, 5 );

		icon = gdk_pixmap_create_from_xpm_d(plugin_prefs_win->window, &mask, NULL, cancel_xpm);
		iconwid = gtk_pixmap_new(icon, mask);
		label = gtk_label_new(_("Cancel"));
		button = gtk_button_new();

		gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 2);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

		gtk_widget_show( iconwid );
		gtk_widget_show( label  );
		gtk_widget_show( hbox );
		gtk_widget_show( button );

		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(cancel_plugin_prefs), (gpointer)pref_info->pref_list );

		gtk_container_add( GTK_CONTAINER (button), hbox );

		gtk_box_pack_start( GTK_BOX(buttonHBox), button, TRUE, TRUE, 5 );

		/* ICK!  use another hbox to put the buttons over on the right */
		hbox = gtk_hbox_new( FALSE, 5 );
		gtk_widget_show( hbox );	
		gtk_box_pack_end( GTK_BOX(hbox), buttonHBox, FALSE, FALSE, 5 );

		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

		gtk_widget_show( vbox );

		gtk_container_add( GTK_CONTAINER(plugin_prefs_win), GTK_WIDGET(vbox) );
		}

		gtk_widget_show( plugin_prefs_win );
		gdk_window_raise( plugin_prefs_win->window );

		g_string_free( windowName, TRUE );
	}
}

static void build_modules_prefs(GtkWidget * prefs_note)
{
	GtkWidget	*hbox = NULL;
	GtkWidget	*label = NULL;
	GtkWidget	*vbox = gtk_vbox_new(FALSE, 5);
	GtkWidget	*vbox2 = NULL;
	GtkWidget	*module_list = NULL;
	GtkWidget	*w = NULL;
	int			col = 0;
	char		*titles[7] = {_("Type"), 
			   _("Brief Description"), 
			   _("Status"), 
			   _("Version"), 
			   _("Date Last Modified") ,
			   _("Path to module"), 
			   _("Error Text")};
	char		*ptr = NULL;
	int			mod_path_pos = 0;
	char		modules_path_text[MAX_PREF_LEN];
	GtkWidget	*modules_path_widget = NULL;

	
	modules_path_widget = gtk_text_new( NULL, NULL );
	gtk_editable_set_editable( GTK_EDITABLE(modules_path_widget), FALSE );

	vbox2 = gtk_vbox_new(TRUE, 5);
	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Module Paths"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	strncpy( modules_path_text, cGetLocalPref("modules_path"), MAX_PREF_LEN );
	ptr = modules_path_text;
	while( (ptr=strchr(ptr, ':')) )
		ptr[0]='\n';
	gtk_widget_set_usize(modules_path_widget, 200, 75);
	gtk_editable_insert_text(GTK_EDITABLE(modules_path_widget),
		modules_path_text, strlen(modules_path_text), &mod_path_pos);
	gtk_box_pack_start(GTK_BOX(hbox), modules_path_widget, TRUE, TRUE, 10);
	gtk_widget_show(modules_path_widget);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 10);
	gtk_widget_show(hbox);

	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(w, 400, 200);
	
	/* Need to create a new one each time window is created as the old one was destroyed */
	module_list = gtk_clist_new_with_titles(s_plugin_info_num_columns, titles);
	gtk_container_add(GTK_CONTAINER(w), module_list);
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 10);
	
	build_modules_list( module_list );
	
	gtk_clist_set_reorderable(GTK_CLIST(module_list), TRUE);
	gtk_clist_set_auto_sort(GTK_CLIST(module_list), TRUE);
	gtk_clist_column_titles_active(GTK_CLIST(module_list));
	gtk_signal_connect(GTK_OBJECT(module_list), "click-column",
			GTK_SIGNAL_FUNC(plugin_column_sort), NULL);
	gtk_clist_set_selection_mode(GTK_CLIST(module_list), GTK_SELECTION_SINGLE);
	for(col=0;col<s_plugin_info_num_columns;col++)
		gtk_clist_set_column_auto_resize(GTK_CLIST(module_list), col, TRUE);
	/* Make sure right mouse button works */
	gtk_clist_set_button_actions(GTK_CLIST(module_list), 1, GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(module_list), 2, GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(module_list), 3, GTK_BUTTON_SELECTS);
	gtk_signal_connect(GTK_OBJECT(module_list), "select-row",
			GTK_SIGNAL_FUNC(plugin_selected), NULL);

	gtk_widget_show(label);

	gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Modules")));
	gtk_widget_show(vbox);
	gtk_widget_show(GTK_WIDGET(module_list));
}

static void destroy_modules()
{
}

static void	set_module_prefs( void )
{
}

/*
 * Sound Preferences Tab
 */
static GtkWidget *arrivesoundentry=NULL;
static GtkWidget *awaysoundentry=NULL;
static GtkWidget *leavesoundentry=NULL;
static GtkWidget *sendsoundentry=NULL;
static GtkWidget *receivesoundentry=NULL;
static GtkWidget *firstmsgsoundentry=NULL;
static GtkWidget *volumesoundentry=NULL;

static GtkWidget *add_sound_file_selection_box(char *, GtkWidget *, char *, int);
static GtkWidget *add_sound_volume_selection_box(char *, GtkWidget *, GtkAdjustment *);
/* Main Sound Preferences Funtion, hooks in the rest */

static void build_sound_prefs(GtkWidget *prefs_note)
{
     GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
     build_sound_tab(vbox);
     gtk_widget_show (vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Sound")));

     gtk_widget_show(prefs_note);
}

     /* This is the sound sub-tabs code */

static void build_sound_tab(GtkWidget * parent_window)
{

     GtkWidget * sound_vbox;
     GtkWidget * sound_note;

     sound_note = gtk_notebook_new();

     sound_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(sound_vbox);

     eb_button(_("Disable sounds when I am away"), &s_prefs->sound.do_no_sound_when_away, sound_vbox);
     eb_button(_("Disable sounds for Ignored people"), &s_prefs->sound.do_no_sound_for_ignore, sound_vbox);
     eb_button(_("Play sounds when people sign on or off"), &s_prefs->sound.do_online_sound, sound_vbox);
     eb_button(_("Play a sound when sending a message"), &s_prefs->sound.do_play_send, sound_vbox);
     eb_button(_("Play a sound when receiving a message"), &s_prefs->sound.do_play_receive, sound_vbox);
     eb_button(_("Play a special sound when receiving first message"), &s_prefs->sound.do_play_first, sound_vbox);

    gtk_notebook_append_page(GTK_NOTEBOOK(sound_note), sound_vbox, gtk_label_new(_("General")));

     /* Files Tab */

     sound_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(sound_vbox);

     arrivesoundentry = 
	  add_sound_file_selection_box(_("Contact signs on: "),
				       sound_vbox,
				       s_prefs->sound.BuddyArriveFilename,
				       BUDDY_ARRIVE);
     awaysoundentry = 
	  add_sound_file_selection_box(_("Contact goes away: "),
				       sound_vbox,
				       s_prefs->sound.BuddyAwayFilename,
				       BUDDY_AWAY);
     leavesoundentry = 
	  add_sound_file_selection_box(_("Contact signs off: "),
				       sound_vbox,
				       s_prefs->sound.BuddyLeaveFilename,
				       BUDDY_LEAVE);

     sendsoundentry = 
	  add_sound_file_selection_box(_("Message sent: "),
				       sound_vbox,
				       s_prefs->sound.SendFilename,
				       SEND);

     receivesoundentry = 
	  add_sound_file_selection_box(_("Message received: "),
				       sound_vbox,
				       s_prefs->sound.ReceiveFilename,
				       RECEIVE);

     firstmsgsoundentry = 
	  add_sound_file_selection_box(_("First message received: "),
				       sound_vbox,
				       s_prefs->sound.FirstMsgFilename,
				       FIRSTMSG);

     volumesoundentry =
       add_sound_volume_selection_box(_("Relative volume (dB)"),
				      sound_vbox,
				      GTK_ADJUSTMENT(gtk_adjustment_new(s_prefs->sound.SoundVolume,
							 -40,0,1,5,0)));

     gtk_widget_show(sound_vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(sound_note), sound_vbox, 
			      gtk_label_new(_("Files")));

     gtk_widget_show(sound_note);
     gtk_container_add(GTK_CONTAINER(parent_window), sound_note);
}

static void getsoundfile (GtkWidget * sendf_button, gpointer userdata);
static void testsoundfile (GtkWidget * sendf_button, int userdata);
static void soundvolume_changed(GtkAdjustment * adjust, int userdata);

static GtkWidget *add_sound_file_selection_box(char *labelString,
					GtkWidget *vbox,
					char *initialFilename,
					int userdata) 
{
     GtkWidget *hbox;
     GtkWidget *widget;
     GtkWidget *label;
     GtkWidget *button;
     hbox = gtk_hbox_new(FALSE, 3);
     gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

     label = gtk_label_new (labelString);
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
     gtk_widget_set_usize(label, 125, 10);
     gtk_widget_show (label);

     widget = gtk_entry_new ();
     gtk_entry_set_text (GTK_ENTRY (widget), initialFilename);
     gtk_widget_ref (widget);
     gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
     gtk_widget_show (widget);

     button = gtk_button_new_with_label (_("Browse"));
     gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			GTK_SIGNAL_FUNC(getsoundfile), 
			(gpointer)userdata);
     gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
     gtk_widget_show (button);

     button = gtk_button_new_with_label (_("Preview"));
     gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			GTK_SIGNAL_FUNC(testsoundfile), 
			(gpointer)userdata);
     gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
     gtk_widget_show (button);
     gtk_widget_show(hbox);
     return widget;
}

static GtkWidget *add_sound_volume_selection_box(char *labelString,
					  GtkWidget *vbox,
					  GtkAdjustment *adjust)
{
     GtkWidget *hbox;
     GtkWidget *widget;
     GtkWidget *label;
     int dummy=0;
     hbox = gtk_hbox_new(FALSE, 3);
     gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

     label = gtk_label_new (labelString);
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
     gtk_widget_set_usize(label, 125, 10);
     gtk_widget_show (label);

     widget=gtk_hscale_new(adjust);
     gtk_widget_ref (widget);
     gtk_signal_connect(GTK_OBJECT(adjust), "value_changed", 
			GTK_SIGNAL_FUNC(soundvolume_changed), 
			(gpointer) dummy);
     gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
     gtk_widget_show (widget);

     gtk_widget_show(hbox);
     return widget;
}

static void setsoundfilename(char * selected_filename, gpointer data)
{
	int userdata = (int)data;

	if(!selected_filename)
		return;

     eb_debug(DBG_CORE, "Just entered setsoundfilename and the selected file is %s\n", selected_filename);
     switch(userdata) 
     {
     case BUDDY_ARRIVE: 	
	  strncpy( s_prefs->sound.BuddyArriveFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (arrivesoundentry), 
			      selected_filename);
	  break;
     case BUDDY_LEAVE:	
	  strncpy( s_prefs->sound.BuddyLeaveFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (leavesoundentry), 
			      selected_filename);
	  break;
     case SEND:		
	  strncpy( s_prefs->sound.SendFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (sendsoundentry), selected_filename);
	  break;
     case RECEIVE:		
	  strncpy( s_prefs->sound.ReceiveFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (receivesoundentry), 
			      selected_filename);
	  break;
     case BUDDY_AWAY: 	
	  strncpy( s_prefs->sound.BuddyAwayFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (awaysoundentry), selected_filename);
	  break;
     case FIRSTMSG:		
	  strncpy( s_prefs->sound.FirstMsgFilename, selected_filename, MAX_PREF_LEN);
	  gtk_entry_set_text (GTK_ENTRY (firstmsgsoundentry), 
			      selected_filename);
	  break;
     }
}

static void getsoundfile (GtkWidget * sendf_button, gpointer userdata)
{
     eb_debug(DBG_CORE, "Just entered getsoundfile\n");
     eb_do_file_selector(NULL, _("Select a file to use"), setsoundfilename, 
		     userdata);
}

static void testsoundfile (GtkWidget * sendf_button, int userdata)
{
     switch(userdata) 
     {
     case BUDDY_ARRIVE:
	  playsoundfile(s_prefs->sound.BuddyArriveFilename);
	  break;

     case BUDDY_LEAVE:
	  playsoundfile(s_prefs->sound.BuddyLeaveFilename);
	  break;

     case SEND:
	  playsoundfile(s_prefs->sound.SendFilename);
	  break;

     case RECEIVE:
	  playsoundfile(s_prefs->sound.ReceiveFilename);
	  break;

     case BUDDY_AWAY:
	  playsoundfile(s_prefs->sound.BuddyAwayFilename);
	  break;

     case FIRSTMSG:
	  playsoundfile(s_prefs->sound.FirstMsgFilename);
	  break;

     }
}

static void soundvolume_changed(GtkAdjustment * adjust, int userdata)
{
  s_prefs->sound.SoundVolume = adjust->value;
}

static void	set_sound_prefs( void )
{
	if (arrivesoundentry == NULL)
		return;
	
	strncpy( s_prefs->sound.BuddyArriveFilename, gtk_entry_get_text(GTK_ENTRY (arrivesoundentry)), MAX_PREF_LEN );
	strncpy( s_prefs->sound.BuddyLeaveFilename, gtk_entry_get_text(GTK_ENTRY (leavesoundentry)), MAX_PREF_LEN );
	strncpy( s_prefs->sound.SendFilename, gtk_entry_get_text(GTK_ENTRY (sendsoundentry)), MAX_PREF_LEN );
	strncpy( s_prefs->sound.ReceiveFilename, gtk_entry_get_text(GTK_ENTRY (receivesoundentry)), MAX_PREF_LEN );
	strncpy( s_prefs->sound.BuddyAwayFilename, gtk_entry_get_text(GTK_ENTRY (awaysoundentry)), MAX_PREF_LEN );
	strncpy( s_prefs->sound.FirstMsgFilename, gtk_entry_get_text(GTK_ENTRY (firstmsgsoundentry)), MAX_PREF_LEN );
}

static void destroy_sound_prefs(void)
{
	gtk_widget_destroy(arrivesoundentry);
	gtk_widget_destroy(awaysoundentry);
	gtk_widget_destroy(leavesoundentry);
	gtk_widget_destroy(sendsoundentry);
	gtk_widget_destroy(receivesoundentry);
	gtk_widget_destroy(firstmsgsoundentry);
	gtk_widget_destroy(volumesoundentry);
	arrivesoundentry=NULL;
	awaysoundentry=NULL;
	leavesoundentry=NULL;
	sendsoundentry=NULL;
	receivesoundentry=NULL;
	firstmsgsoundentry=NULL;
	volumesoundentry=NULL;
}

/*
 * OK & Cancel callbacks
 */

static void	ok_callback(GtkWidget *widget, gpointer data)
{
	set_general_prefs();
	set_logs_prefs();
	set_sound_prefs();
	set_chat_prefs();
	set_layout_prefs();
#ifdef HAVE_ICONV_H
	set_encoding_prefs();
#endif
	set_proxy_prefs();
	set_module_prefs();

	ayttm_prefs_apply( s_prefs );
	s_prefs = NULL;
}

static void	cancel_callback( GtkWidget *widget, gpointer data )
{
	ayttm_prefs_cancel( s_prefs );
	s_prefs = NULL;
}

/*
 * Destroy prefs window
 */
static void destroy(GtkWidget * widget, gpointer data)
{
     destroy_proxy();
     destroy_general_prefs();
     destroy_chat_prefs();
     destroy_sound_prefs();

#ifdef HAVE_ICONV_H
     destroy_encodings();
#endif
	 destroy_modules();

#ifdef HAVE_ISPELL
     if ((!iGetLocalPref("do_spell_checking")) && (gtkspell_running()))
	     gtkspell_stop();
#endif

     gtk_widget_destroy(s_prefs_window);
     s_prefs_window = NULL;
}

