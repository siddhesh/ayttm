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

#ifdef __MINGW32__
extern int WinVer;
#define rindex(a,b) strrchr(a,b)
#endif

static int is_prefs_open = 0;


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


void build_prefs()
{
     if(!is_prefs_open)
     {
	  GtkWidget *hbox;
	  GtkWidget *button;
	  GtkWidget *hbox2;
	  GtkWidget *prefs_vbox;
	  GtkWidget *prefs_window;

	  prefs_vbox = gtk_vbox_new(FALSE, 5);
	  prefs_window = gtk_window_new(GTK_WINDOW_DIALOG);
	  gtk_window_set_position(GTK_WINDOW(prefs_window), GTK_WIN_POS_MOUSE);
	  /* set current parent to prefs so error dialogs know who real
	   * parent is */
	  current_parent_widget = prefs_window;
	  gtk_widget_realize(prefs_window);
	  gtk_window_set_title(GTK_WINDOW(prefs_window), _("Ayttm Preferences"));
	  eb_icon(prefs_window->window);
	  gtk_signal_connect(GTK_OBJECT(prefs_window), "destroy",
			      GTK_SIGNAL_FUNC(destroy), NULL);

	  /*
       ************************************************************
	   Below the different tabs are defined for the preferences
	   window.  In which the user can make ayttm work more
	   the way he/she wants it too.
	   ************************************************************
       */

	  {
	       GtkWidget *prefs_note = gtk_notebook_new();
               SetPref("widget::prefs_note", prefs_note);
	       build_general_prefs(prefs_note);
	       build_logs_prefs(prefs_note);
	       build_sound_prefs(prefs_note);
	       build_layout_prefs(prefs_note);
	       build_chat_prefs(prefs_note);
//	       build_connections_prefs(prefs_note);
	       gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_note, FALSE, FALSE, 0);
	       build_proxy_prefs(prefs_note);
#ifdef HAVE_ICONV_H
	       build_encoding_prefs(prefs_note);
#endif
	       build_modules_prefs(prefs_note);
	  }

	  /*Okay Button*/
	  hbox2 = gtk_hbox_new(TRUE, 5);
	  gtk_widget_set_usize(hbox2, 200,25);

	  button = do_icon_button(_("OK"), ok_xpm, prefs_window);

	  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			     ok_callback, prefs_window);

	  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				     GTK_SIGNAL_FUNC (gtk_widget_destroy),
				     GTK_OBJECT (prefs_window));
	  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 5);
	  gtk_widget_show(button);

	  /*Cancel Button*/

	  button = do_icon_button(_("Cancel"), cancel_xpm, prefs_window);

	  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			     cancel_callback, prefs_window);

	  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				     GTK_SIGNAL_FUNC (gtk_widget_destroy),
				     GTK_OBJECT (prefs_window));

	  gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 5);
	  gtk_widget_show(button);

	  /*End Buttons*/
	  hbox = gtk_hbox_new(FALSE, 5);

	  gtk_box_pack_end(GTK_BOX(hbox), hbox2, FALSE, FALSE, 5);
	  gtk_widget_show(hbox2);

	  gtk_box_pack_start(GTK_BOX(prefs_vbox), hbox, FALSE, FALSE, 5);
	  gtk_widget_show(hbox);
	  gtk_widget_show(prefs_vbox);
	  gtk_container_add(GTK_CONTAINER(prefs_window), prefs_vbox);

	  gtk_widget_show(prefs_window);
	  is_prefs_open = 1;
     }
}

/*
 * General Preferences Tab
 * Sat Apr 28 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
 *
 */

static GtkWidget	*length_contact_window_entry = NULL;
static GtkWidget	*font_size_entry = NULL;
static GtkWidget	*alternate_browser_entry = NULL;
static GtkWidget	*g_browser_browse_button = NULL;
#ifdef HAVE_ISPELL
static GtkWidget	*dictionary_entry = NULL;
#endif
static const int	length_contact_window_default = 256;
static const int	width_contact_window_default = 150;
static const int	status_show_level_default = 2;

static const int	font_size_def = 4;

static int	length_contact_window_old = 0;

static int	do_login_on_startup = 0;
static int	do_login_on_startup_old = 0 ;

int		do_ayttm_debug = 0;
static int	do_ayttm_debug_old = 0;

int		do_ayttm_debug_html = 0;
static int	do_ayttm_debug_html_old = 0;

int		do_plugin_debug = 0;
static int	do_plugin_debug_old = 0;

#ifdef HAVE_ISPELL
static int	do_spell_checking = 0;
static int	do_spell_checking_old = 0;
#endif

static int	do_noautoresize = 0;
static int	do_noautoresize_old = 0;

static int	use_alternate_browser = 0;
static int	use_alternate_browser_old = 0;

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
     if(use_alternate_browser == 0)
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
    GtkWidget *vbox;
    GtkWidget *brbutton;
    GtkWidget *label;
    GtkWidget *hbox;

    char buff [10];
    char buff2 [256];
    buff [0] = '\0';
    buff2[0] = '\0';

    length_contact_window_entry = gtk_entry_new ();
    length_contact_window_old = iGetLocalPref("length_contact_window");

    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);

    g_snprintf(buff2, 256, _("Length of Contact List (num. lines, 1 -> %d):"),
	       0);
    label = gtk_label_new(buff2);
    //gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    //gtk_widget_show(label);

    g_snprintf(buff, 10, "%d", iGetLocalPref("length_contact_window"));
    //gtk_entry_set_text(GTK_ENTRY(length_contact_window_entry), buff);
    //gtk_box_pack_start(GTK_BOX(hbox), length_contact_window_entry,
//		       FALSE, FALSE, 0);
//    gtk_widget_show(length_contact_window_entry);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    do_login_on_startup_old = do_login_on_startup;
    eb_button(_("Log in on startup"), &do_login_on_startup, vbox);

#ifdef HAVE_ISPELL
    hbox = gtk_hbox_new(FALSE, 0);
    dictionary_entry = gtk_entry_new();

    if (cGetLocalPref("spell_dictionary")) {
	gtk_entry_set_text(GTK_ENTRY(dictionary_entry), cGetLocalPref("spell_dictionary"));
    } else {
	gtk_entry_set_text(GTK_ENTRY(dictionary_entry), "");
    }
    do_spell_checking_old = do_spell_checking;
    brbutton = eb_button(_("Use spell checking - dictionary (empty for default):"), &do_spell_checking, hbox);
    gtk_box_pack_start(GTK_BOX(hbox), dictionary_entry, TRUE, TRUE, 10);
    gtk_widget_show(dictionary_entry);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    if(do_spell_checking == 0)
    {
	gtk_widget_set_sensitive(dictionary_entry, FALSE);
    }
    gtk_signal_connect(GTK_OBJECT(brbutton), "clicked",
	    GTK_SIGNAL_FUNC(set_use_spell), (gpointer)brbutton);
#endif

    do_ayttm_debug_old = do_ayttm_debug;
    eb_button(_("Enable debug messages"), &do_ayttm_debug, vbox);
    do_ayttm_debug_html_old = do_ayttm_debug_html;
 //   eb_button(_("Enable HTML Debug Messages"), &do_ayttm_debug_html, vbox);
    do_plugin_debug_old = do_plugin_debug;
 //   eb_button(_("Enable Plugin Debug Messages"), &do_plugin_debug, vbox);

    do_noautoresize_old = do_noautoresize;
 //   eb_button(_("Don\'t Automatically Resize Contact List"), &do_noautoresize, vbox);

    use_alternate_browser_old = use_alternate_browser;
    alternate_browser_entry = gtk_entry_new();

    if (cGetLocalPref("alternate_browser")) {
	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), cGetLocalPref("alternate_browser"));
    } else {
	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), "");
    }

    brbutton = eb_button(_("Use alternate browser"), &use_alternate_browser, vbox);
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

    if(use_alternate_browser == 0)
    {
	gtk_widget_set_sensitive(alternate_browser_entry, FALSE);
	gtk_widget_set_sensitive(g_browser_browse_button, FALSE);
    }

    gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox,
	    gtk_label_new(_("General")));
}

static void cancel_general_prefs()
{
  char buff [10];
  buff [0] = '\0';

    do_login_on_startup = do_login_on_startup_old;
    do_ayttm_debug = do_ayttm_debug_old;
    do_ayttm_debug_html = do_ayttm_debug_html_old;
    do_plugin_debug = do_plugin_debug_old;

#ifdef HAVE_ISPELL
    do_spell_checking = do_spell_checking_old ;
    if(cGetLocalPref("spell_dictionary"))
	    gtk_entry_set_text(GTK_ENTRY(dictionary_entry), cGetLocalPref("spell_dictionary"));
    else
	    gtk_entry_set_text(GTK_ENTRY(dictionary_entry), "");
#endif

    use_alternate_browser = use_alternate_browser_old;
    if (cGetLocalPref("alternate_browser")) {
	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), cGetLocalPref("alternate_browser"));
    } else {
	gtk_entry_set_text(GTK_ENTRY(alternate_browser_entry), "");
    }
}

static void write_general_prefs(FILE *fp)
{
    fprintf(fp,"do_login_on_startup=%d\n", do_login_on_startup) ;
    fprintf(fp,"do_ayttm_debug=%d\n", do_ayttm_debug) ;
    fprintf(fp,"do_ayttm_debug_html=%d\n", do_ayttm_debug_html) ;
    fprintf(fp,"do_plugin_debug=%d\n", do_ayttm_debug) ;
    do_plugin_debug=do_ayttm_debug;
    eb_debug (DBG_CORE, "length_contact_window=%d\n", iGetLocalPref("length_contact_window"));
    fprintf(fp,"length_contact_window=%d\n", iGetLocalPref("length_contact_window"));
    fprintf(fp,"width_contact_window=%d\n", iGetLocalPref("width_contact_window"));
    fprintf(fp,"x_contact_window=%d\n", iGetLocalPref("x_contact_window"));
    fprintf(fp,"y_contact_window=%d\n", iGetLocalPref("y_contact_window"));
    fprintf(fp,"status_show_level=%d\n", iGetLocalPref("status_show_level"));
#ifdef HAVE_ISPELL
    fprintf(fp,"do_spell_checking=%d\n", do_spell_checking) ;
    if (do_spell_checking) {
	    char *oldspell, *newspell;
	    newspell = (dictionary_entry != NULL && strlen(gtk_entry_get_text(GTK_ENTRY(dictionary_entry))))
			    ?gtk_entry_get_text(GTK_ENTRY(dictionary_entry)):"";
	    oldspell = (cGetLocalPref("spell_dictionary") && strlen(cGetLocalPref("spell_dictionary")))
			    ?cGetLocalPref("spell_dictionary"):"";
	    if(strcmp(newspell, oldspell)) {
		gtkspell_stop();
	    }
    }
    if (dictionary_entry!=NULL)
        cSetLocalPref("spell_dictionary", gtk_entry_get_text(GTK_ENTRY(dictionary_entry)));
    if (strlen(cGetLocalPref("spell_dictionary")) > 0) {
	fprintf(fp,"spell_dictionary=%s\n", cGetLocalPref("spell_dictionary"));
    }
#endif
    fprintf(fp,"do_noautoresize=%d\n", do_noautoresize) ;
    fprintf(fp,"use_alternate_browser=%d\n", use_alternate_browser);
    if(alternate_browser_entry!=NULL)
        cSetLocalPref("alternate_browser", gtk_entry_get_text(GTK_ENTRY(alternate_browser_entry)));
    if (strlen(cGetLocalPref("alternate_browser")) > 0) {
	fprintf(fp,"alternate_browser=%s\n", cGetLocalPref("alternate_browser"));
    }
}

#ifdef HAVE_ISPELL
static void set_use_spell(GtkWidget * w, int * data)
{
     if(do_spell_checking == 0)
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
	if(alternate_browser_entry) {
		cSetLocalPref("alternate_browser", gtk_entry_get_text(GTK_ENTRY(alternate_browser_entry)));
		gtk_widget_destroy(alternate_browser_entry);
		gtk_widget_destroy(g_browser_browse_button);
		alternate_browser_entry = NULL;
		g_browser_browse_button = NULL;
	}
#ifdef HAVE_ISPELL
	if(dictionary_entry) {
		cSetLocalPref("spell_dictionary", gtk_entry_get_text(GTK_ENTRY(dictionary_entry)));
		gtk_widget_destroy(dictionary_entry);
		dictionary_entry = NULL;
	}
#endif
	if(length_contact_window_entry) {
		gtk_widget_destroy(length_contact_window_entry);
		length_contact_window_entry = NULL;
	}
}


/*
 * Logs Preferences Tab
 */

static int	do_logging = 1;
static int	do_logging_old = 1;
static int	do_strip_html = 0;
static int	do_strip_html_old = 0;
static int	do_restore_last_conv = 0;
static int	do_restore_last_conv_old = 0;

static void build_logs_prefs(GtkWidget *prefs_note)
{
     GtkWidget *vbox = gtk_vbox_new(FALSE, 5);

     do_logging_old = do_logging;
     do_strip_html_old = do_strip_html;
     do_restore_last_conv_old = do_restore_last_conv;

     eb_button(_("Save all conversations to logfiles"), &do_logging, vbox);
     eb_button(_("Restore last conversation when opening a chat window"), &do_restore_last_conv, vbox);
/* advanced ;)     eb_button(_("Strip HTML tags"), &do_strip_html, vbox); */
     gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Logs")));
}

static void cancel_logs_prefs()
{
     /* Values */
     do_logging = do_logging_old;
     do_strip_html = do_strip_html_old;
     do_restore_last_conv = do_restore_last_conv_old;
}

static void write_logs_prefs(FILE *fp)
{
     fprintf(fp,"do_logging=%d\n", do_logging);
     fprintf(fp,"do_strip_html=%d\n", do_strip_html);
     fprintf(fp,"do_restore_last_conv=%d\n", do_restore_last_conv);
}

/*
 * Layout Preferences Tab
 */

static int do_tabbed_chat = 0;
static int do_tabbed_chat_old = 0;

/* Tab Orientation:  0 => bottom, 1 => top, 2=> left, 3 => right */
static int do_tabbed_chat_orient = 0;
static int do_tabbed_chat_orient_old = 0;

/*
  Callback function for setting element of Tabbed Chat Orientation radio
  button group.  Ignores w and sets global to variable
*/
static void set_tco_element (GtkWidget * w, int data)
{
	do_tabbed_chat_orient = data;
}

static void build_layout_prefs(GtkWidget *prefs_note) 
{
     GtkWidget	*vbox = NULL;
     GtkWidget	*hbox = NULL;
     GSList		*group = NULL;
     GtkWidget	*label = NULL;

     vbox = gtk_vbox_new(FALSE, 5);

     /* Values */
     do_tabbed_chat_old = do_tabbed_chat;
     do_tabbed_chat_orient_old = do_tabbed_chat_orient = iGetLocalPref("do_tabbed_chat_orient");
     eb_button(_("Use tabbed chat-windows"), &do_tabbed_chat, vbox);

     /*
       Radio button group for tabbed chat orientation
     */

     /* Setup box to hold it */
     hbox = gtk_hbox_new(FALSE, 5);

     /* setup intro label */
     label = gtk_label_new (_("Tabs position:"));
     gtk_widget_show (label);
     gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 5);

     /* Setup group -- set to NULL to create new group */
     group = NULL;

     /* Create buttons */
     group = eb_radio (group, _("Bottom"), do_tabbed_chat_orient, 0, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Top"),    do_tabbed_chat_orient, 1, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Left"),   do_tabbed_chat_orient, 2, hbox,
		       set_tco_element);
     group = eb_radio (group, _("Right"),  do_tabbed_chat_orient, 3, hbox,
		       set_tco_element);

     /* bug here , var gets messed up. why ? */
     do_tabbed_chat_orient = do_tabbed_chat_orient_old;
     /* Put it in the vbox and make it visible */
     gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
     gtk_widget_show (hbox);

     gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Layout")));
}

static void cancel_layout_prefs()
{
     do_tabbed_chat = do_tabbed_chat_old;
     do_tabbed_chat_orient = do_tabbed_chat_orient_old;
}

static void write_layout_prefs(FILE *fp)
{
     fprintf(fp,"do_tabbed_chat=%d\n", do_tabbed_chat);
     fprintf(fp,"do_tabbed_chat_orient=%d\n", do_tabbed_chat_orient);
     iSetLocalPref("do_tabbed_chat_orient",do_tabbed_chat_orient);
}

/* Not necessary */
/*
static void destroy_layout_prefs (void)
{
}
*/

/*
 * Chat Preferences Tab
 */

static int do_typing_notify = 1;
static int do_typing_notify_old = 1;

static int do_send_typing_notify = 1;
static int do_send_typing_notify_old = 1;

static int do_escape_close = 1;
static int do_escape_close_old = 1;

static int do_convo_timestamp = 1;
static int do_convo_timestamp_old = 1;

static int do_enter_send = 1;
static int do_enter_send_old = 1;

static int do_ignore_unknown = 0;
static int do_ignore_unknown_old = 0;

static int font_size_old = 4;
static int font_size = 4;

static int do_multi_line = 1;
static int do_multi_line_old = 1;

static int do_raise_window = 0;
static int do_raise_window_old = 0;

static int do_send_idle_time = 0;
static int do_send_idle_time_old = 0;

static int do_timestamp = 1;
static int do_timestamp_old = 1;

static int do_ignore_fore = 1;
static int do_ignore_fore_old = 1;

static int do_ignore_back = 1;
static int do_ignore_back_old = 1;

static int do_ignore_font = 1;
static int do_ignore_font_old = 1;

static int do_smiley = 1;
static int do_smiley_old = 1;

GdkDeviceKey accel_prev_tab = { GDK_Left, GDK_CONTROL_MASK };
GdkDeviceKey accel_next_tab = { GDK_Right, GDK_CONTROL_MASK };

static GdkDeviceKey accel_prev_tab_old = { GDK_Left, GDK_CONTROL_MASK };
static GdkDeviceKey accel_next_tab_old = { GDK_Right, GDK_CONTROL_MASK };

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
     gtk_widget_set_usize(label, 255, 10);
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
     do_ignore_unknown_old = do_ignore_unknown;
     do_send_idle_time_old = do_send_idle_time;
     do_raise_window_old = do_raise_window;
     do_timestamp_old = do_timestamp;
     do_multi_line_old = do_multi_line;
     do_enter_send_old = do_enter_send;
     do_smiley_old = do_smiley;
     do_convo_timestamp_old = do_convo_timestamp;
     do_escape_close_old = do_escape_close;
     do_typing_notify_old = do_typing_notify;
     do_send_typing_notify_old = do_send_typing_notify;
     font_size = iGetLocalPref("FontSize");
	 do_ignore_fore_old = do_ignore_fore;
	 do_ignore_back_old = do_ignore_back;
	 do_ignore_font_old = do_ignore_font;

	 accel_next_tab_old = accel_next_tab;
	 accel_prev_tab_old = accel_prev_tab;
	 accel_change_handler_id = 0;

     eb_button(_("Send idle/away status to servers"), &do_send_idle_time, vbox);
     eb_button(_("Raise chat-window when receiving a message"), &do_raise_window, vbox);
//     eb_button(_("Timestamp when a user logs on/off"), &do_timestamp, vbox);
     eb_button(_("Ignore unknown people"), &do_ignore_unknown, vbox);
//     eb_button(_("Enable multi-line chat"), &do_multi_line, vbox);
//     eb_button(_("Press enter to send"), &do_enter_send, vbox);
//     eb_button(_("Timestamps on Messages"), &do_convo_timestamp, vbox);
//     eb_button(_("Show typing... status"), &do_typing_notify, vbox);
//     eb_button(_("Send typing... status"), &do_send_typing_notify, vbox);
//     eb_button(_("Draw Smilies"), &do_smiley, vbox);
//     eb_button(_("Escape Closes Chat"), &do_escape_close, vbox);

     eb_button(_("Ignore foreground Colors"), &do_ignore_fore, vbox);
     eb_button(_("Ignore background Colors"), &do_ignore_back, vbox);
     eb_button(_("Ignore fonts"), &do_ignore_font, vbox);

    hbox = gtk_hbox_new(FALSE, 0);

    label = gtk_label_new(_("Font size: "));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    font_size_entry = gtk_entry_new();
    gtk_widget_set_usize(font_size_entry, 60, -1);
    g_snprintf(buff, 10, "%d", iGetLocalPref("FontSize")==0?4:iGetLocalPref("FontSize"));

    gtk_entry_set_text(GTK_ENTRY(font_size_entry), buff);
    gtk_box_pack_start(GTK_BOX(hbox), font_size_entry,
		       FALSE, FALSE, 0);
    gtk_widget_show(font_size_entry);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	 add_key_set(_("Hotkey to go to previous tab (requires a modifier)"), &accel_prev_tab, vbox);
	 add_key_set(_("Hotkey to go to next tab (requires a modifier)"), &accel_next_tab, vbox);

     gtk_widget_show(vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Chat")));
}

static void cancel_chat_prefs()
{
     do_ignore_unknown = do_ignore_unknown_old;
     do_send_idle_time = do_send_idle_time_old;
     do_raise_window = do_raise_window_old;
     do_timestamp = do_timestamp_old;
     do_multi_line = do_multi_line_old;
     do_enter_send = do_enter_send_old;
     do_convo_timestamp = do_convo_timestamp_old;
     do_escape_close = do_escape_close_old;
     do_typing_notify = do_typing_notify_old;
     do_send_typing_notify = do_send_typing_notify_old;
     do_smiley = do_smiley_old;
     font_size = font_size_old;
	 do_ignore_fore = do_ignore_fore_old;
	 do_ignore_back = do_ignore_back_old;
	 do_ignore_font = do_ignore_font_old;	 

	 accel_next_tab = accel_next_tab_old;
	 accel_prev_tab = accel_prev_tab_old;
}

static void write_chat_prefs(FILE *fp)
{
	char *ptr=NULL;
     fprintf(fp,"do_ignore_unknown=%d\n", do_ignore_unknown);
     fprintf(fp,"do_send_idle_time=%d\n", do_send_idle_time);
     fprintf(fp,"do_raise_window=%d\n", do_raise_window);
     fprintf(fp,"do_timestamp=%d\n", do_timestamp);
     fprintf(fp,"do_multi_line=%d\n", do_multi_line);
     fprintf(fp,"do_enter_send=%d\n", do_enter_send);
     fprintf(fp,"do_convo_timestamp=%d\n", do_convo_timestamp);
     fprintf(fp,"do_typing_notify=%d\n", do_typing_notify);
     fprintf(fp,"do_send_typing_notify=%d\n", do_send_typing_notify);
     fprintf(fp,"do_smiley=%d\n", do_smiley);
     fprintf(fp,"do_escape_close=%d\n", do_escape_close);
     if (font_size_entry != NULL) {
	     ptr = gtk_entry_get_text(GTK_ENTRY(font_size_entry));
	     font_size = atoi(ptr);
     }
     fprintf(fp,"FontSize=%d\n", font_size);
     iSetLocalPref("FontSize",font_size);
	 fprintf(fp,"do_ignore_fore=%d\n", do_ignore_fore);
	 fprintf(fp,"do_ignore_back=%d\n", do_ignore_back);
	 fprintf(fp,"do_ignore_font=%d\n", do_ignore_font);
	 ptr=gtk_accelerator_name(accel_next_tab.keyval, accel_next_tab.modifiers);
	 fprintf(fp,"accel_next_tab=%s\n", ptr);
	 g_free(ptr);
	 ptr=gtk_accelerator_name(accel_prev_tab.keyval, accel_prev_tab.modifiers);
	 fprintf(fp,"accel_prev_tab=%s\n", ptr);
	 g_free(ptr);
}

static void destroy_chat_prefs (void) {
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
static int new_proxy_type;
static int do_proxy_auth;
static int do_proxy_auth_old;

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

     if(proxy_type == PROXY_NONE)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use HTTP Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_HTTP);

     if(proxy_type == PROXY_HTTP)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }


     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use SOCKS4 Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_SOCKS4);

     if(proxy_type == PROXY_SOCKS4)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     button = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
					       _("Use SOCKS5 Proxy"));
     gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(set_proxy_type), (gpointer)PROXY_SOCKS5);

     if(proxy_type == PROXY_SOCKS5)
     {
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
     }

     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Proxy Server:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     if(proxy_host)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_server_entry), proxy_host);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_server_entry), "");
     }

     gtk_box_pack_start(GTK_BOX(hbox), proxy_server_entry, TRUE, TRUE, 10);
     gtk_widget_show(proxy_server_entry);

     label = gtk_label_new(_("Proxy Port:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     g_snprintf(buff, 10, "%d", proxy_port);
     gtk_entry_set_text(GTK_ENTRY(proxy_port_entry), buff);
     gtk_box_pack_start(GTK_BOX(hbox), proxy_port_entry, FALSE, FALSE, 0);
     gtk_widget_show(proxy_port_entry);

     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
     gtk_widget_show(hbox);

	 do_proxy_auth_old = do_proxy_auth ;
	 eb_button(_("Proxy requires authentication"), &do_proxy_auth, vbox);

     hbox = gtk_hbox_new(FALSE, 0);
     label = gtk_label_new(_("Proxy User ID:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     if(proxy_user)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_user_entry), proxy_user);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_user_entry), "");
     }

     gtk_box_pack_start(GTK_BOX(hbox), proxy_user_entry, TRUE, TRUE, 10);
     gtk_widget_show(proxy_user_entry);

     label = gtk_label_new(_("Proxy Password:"));
     gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
     gtk_widget_show(label);

     if(proxy_password)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_password_entry), proxy_password);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_password_entry), "");
     }

	 gtk_entry_set_visibility(GTK_ENTRY(proxy_password_entry), FALSE);

     gtk_box_pack_start(GTK_BOX(hbox), proxy_password_entry, FALSE, FALSE, 0);
     gtk_widget_show(proxy_password_entry);

     gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
     gtk_widget_show(hbox);

     if(proxy_type == PROXY_NONE)
     {
	  gtk_widget_set_sensitive(proxy_server_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_port_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_user_entry, FALSE);
	  gtk_widget_set_sensitive(proxy_password_entry, FALSE);
     }

     gtk_notebook_append_page(GTK_NOTEBOOK(prefs_note), vbox, gtk_label_new(_("Proxy")));
     gtk_widget_show(vbox);
}

static void cancel_proxy_prefs()
{
     char buff[10];

     new_proxy_type = proxy_type;

     if(proxy_host)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_server_entry), proxy_host);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_server_entry), "");
     }

     g_snprintf(buff, 10, "%d", proxy_port);
     gtk_entry_set_text(GTK_ENTRY(proxy_port_entry), buff);

	 do_proxy_auth = do_proxy_auth_old ;

     if(proxy_user)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_user_entry), proxy_user);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_user_entry), "");
     }

     if(proxy_password)
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_password_entry), proxy_password);
     }
     else
     {
	  gtk_entry_set_text(GTK_ENTRY(proxy_password_entry), "");
     }

}

static void destroy_proxy()
{
	if(proxy_server_entry == NULL) return;
     proxy_set_proxy(new_proxy_type,
		     gtk_entry_get_text(GTK_ENTRY(proxy_server_entry)),
		     atol(gtk_entry_get_text(GTK_ENTRY(proxy_port_entry))));
	 proxy_set_auth(do_proxy_auth,
		 gtk_entry_get_text(GTK_ENTRY(proxy_user_entry)),
		 gtk_entry_get_text(GTK_ENTRY(proxy_password_entry)));
	gtk_widget_destroy(proxy_server_entry);
	gtk_widget_destroy(proxy_port_entry);
	gtk_widget_destroy(proxy_user_entry);
	gtk_widget_destroy(proxy_password_entry);

	proxy_server_entry=NULL;
	proxy_port_entry=NULL;
	proxy_user_entry=NULL;
	proxy_password_entry=NULL;
}

static void write_proxy_prefs(FILE *fp)
{
     fprintf(fp,"proxy_type=%d\n", proxy_type);
     if(proxy_host)
     {
	  fprintf(fp,"proxy_host=%s\n", proxy_host);
     }
     fprintf(fp,"proxy_port=%d\n", proxy_port);
     fprintf(fp,"do_proxy_auth=%d\n", do_proxy_auth);
     if (proxy_user)
     {
      fprintf(fp,"proxy_user=%s\n", proxy_user);
     }
     if (proxy_password)
     {
      fprintf(fp,"proxy_password=%s\n", proxy_password);
     }
}


static void set_proxy_type(GtkWidget * w, int * data)
{
     new_proxy_type = (int)data;
     if(new_proxy_type == PROXY_NONE)
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
int use_recoding = 0;

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

     button = eb_button(_("Use recoding in conversations"), &use_recoding, vbox);

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

     if(use_recoding == 0)
     {
	  gtk_widget_set_sensitive(local_encoding_entry, FALSE);
	  gtk_widget_set_sensitive(remote_encoding_entry, FALSE);
     }

}

static void destroy_encodings()
{
     if(local_encoding_entry==NULL) return;
     cSetLocalPref("local_encoding", gtk_entry_get_text(GTK_ENTRY(local_encoding_entry)));
     cSetLocalPref("remote_encoding", gtk_entry_get_text(GTK_ENTRY(remote_encoding_entry)));
     gtk_widget_destroy(local_encoding_entry);
     gtk_widget_destroy(remote_encoding_entry);
     local_encoding_entry=NULL;
     remote_encoding_entry=NULL;

}

static void set_use_of_recoding(GtkWidget * w, int * data)
{
     //new_proxy_type = (int)data;
     if(use_recoding == 0)
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

static void write_encoding_prefs(FILE *fp)
{
	 char *enc=NULL;

     fprintf(fp,"use_recoding=%d\n", use_recoding);
	 enc = cGetLocalPref("local_encoding");
     if(strlen(enc) > 0)
	  fprintf(fp,"local_encoding=%s\n", enc);
	 enc = cGetLocalPref("remote_encoding");
     if(strlen(enc) > 0)
	  fprintf(fp,"remote_encoding=%s\n", enc);
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

static GtkWidget	*modules_path_text = NULL;
static GtkWidget	*plugin_prefs_win = NULL;

static void build_modules_list(GtkWidget * module_list)
{
	char *module_info[7];
	GList *plugins=NULL;
	eb_PLUGIN_INFO *epi=NULL;

	for(plugins=GetPref(EB_PLUGIN_LIST); plugins; plugins=plugins->next) {
		epi=plugins->data;
		eb_debug(DBG_CORE, "Adding plugin %s\n", epi->name);
		module_info[0]=PLUGIN_TYPE_TXT[epi->pi.type-1];
		module_info[1]=epi->pi.brief_desc;
		module_info[2]=PLUGIN_STATUS_TXT[epi->status];
		module_info[3]=epi->pi.version;
		module_info[4]=epi->pi.date;
		module_info[5]=epi->name;
		if(epi->status_desc)
			module_info[6]=(char *)epi->status_desc;
		else
			module_info[6]="";
		gtk_clist_append(GTK_CLIST(module_list), module_info);
	}
}

void rebuild_set_status_menu()
{
	GtkWidget *set_status_submenuitem;

	set_status_submenuitem = GetPref("widget::set_status_submenuitem");
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(set_status_submenuitem));
	eb_set_status_window(set_status_submenuitem);
	gtk_widget_draw(GTK_WIDGET(set_status_submenuitem), NULL);
}

void rebuild_import_menu()
{
	GtkWidget *import_submenuitem;

	import_submenuitem = GetPref("widget::import_submenuitem");
	if(!import_submenuitem) {
		eb_debug(DBG_CORE, "Not rebuilding import menu, it's never been built.\n");
		return;
	}
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(import_submenuitem));
	eb_import_window(import_submenuitem);
	gtk_widget_draw(GTK_WIDGET(import_submenuitem), NULL);
}
void rebuild_profile_menu()
{
	GtkWidget *profile_submenuitem;

	profile_submenuitem = GetPref("widget::profile_submenuitem");
	if(!profile_submenuitem) {
		eb_debug(DBG_CORE, "Not rebuilding profile menu, it's never been built.\n");
		return;
	}
	gtk_menu_item_remove_submenu(GTK_MENU_ITEM(profile_submenuitem));
	eb_profile_window(profile_submenuitem);
	gtk_widget_draw(GTK_WIDGET(profile_submenuitem), NULL);
}

static void reload_modules(GtkWidget * w, int * data)
{
	 GtkWidget *module_list;
	 char *ptr=NULL;
	 char *modules_path=NULL;

	 module_list = GetPref("widget::module_list");
	 gtk_clist_clear(GTK_CLIST(module_list));

	 eb_debug(DBG_CORE, "Reloading modules\n");
	 modules_path = gtk_editable_get_chars(GTK_EDITABLE(modules_path_text), 0, -1);
	 ptr=modules_path;
	 while( (ptr=strchr(ptr, '\n')) )
		ptr[0]=':';
	 cSetLocalPref("modules_path", modules_path);
	 load_modules();
	 build_modules_list(module_list);
	 rebuild_set_status_menu();
	 rebuild_import_menu();
	 rebuild_profile_menu();
	 g_free(modules_path);
}

typedef struct {
	GtkCList *clist;
	eb_PLUGIN_INFO *epi;
	int row;
}Plugin_Callback_Data;

static void reload_plugin_callback(GtkWidget * reload_button, gpointer userdata)
{
	Plugin_Callback_Data *PCD=(Plugin_Callback_Data *)userdata;
	char *name=NULL, *path=NULL, buffer[1024];
	char *module_info[7];

	strncpy(buffer, PCD->epi->name, 1023);
	name=rindex(buffer, '/');
	if(name) {
		name[0]='\0';
		name++;
		path=buffer;
	}
	else {
			name=buffer;
			path=NULL;
	}
	if(PCD->epi->status==PLUGIN_LOADED)
	{
		eb_debug(DBG_CORE, "Unloading plugin %s\n", PCD->epi->name);
		if(unload_module(PCD->epi)!=0) {
			eb_debug(DBG_CORE, "Could not unload plugin %s\n", PCD->epi->name);
			return;
		}
		load_module(path, name);
	}
	else {
			load_module(path, name);
	}
	module_info[0]=PLUGIN_TYPE_TXT[PCD->epi->pi.type-1];
	module_info[1]=PCD->epi->pi.brief_desc;
	module_info[2]=PLUGIN_STATUS_TXT[PCD->epi->status];
	module_info[3]=PCD->epi->pi.version;
	module_info[4]=PCD->epi->pi.date;
	module_info[5]=PCD->epi->name;
	if(PCD->epi->status_desc)
		module_info[6]=(char *)PCD->epi->status_desc;
	else
		module_info[6]="";
	gtk_clist_remove(PCD->clist, PCD->row);
	gtk_clist_insert(PCD->clist, PCD->row, module_info);
	if(PCD->epi->pi.type==PLUGIN_SERVICE)
	{
		rebuild_set_status_menu();
	}
}

static void unload_plugin_callback(GtkWidget * reload_button, gpointer userdata)
{
	Plugin_Callback_Data *PCD=(Plugin_Callback_Data *)userdata;

	/*FIXME: Should set a pref so this module is not automatically loaded next time */
	eb_debug(DBG_CORE, "Unloading plugin %s\n", PCD->epi->name);
	if(unload_module(PCD->epi)==0) {
		gtk_clist_set_text(PCD->clist, PCD->row, PLUGIN_STATUS_COL, PLUGIN_STATUS_TXT[PLUGIN_NOT_LOADED]);
		eb_debug(DBG_CORE, "Unloaded plugin %s\n", PCD->epi->name);
		if(PCD->epi->pi.type==PLUGIN_SERVICE)
		{
			rebuild_set_status_menu();
		}
	}
	else
		eb_debug(DBG_CORE, "Could not unload plugin %s\n", PCD->epi->name);
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
	char *plugin_path=NULL;
	eb_PLUGIN_INFO *epi=NULL;

	if(!event)
		return;
	eb_debug(DBG_CORE, "row: %i column: %i button: %i\n", row, column, event->button);

	gtk_clist_get_text(GTK_CLIST(clist), row, PLUGIN_PATH_COL, &plugin_path);
	eb_debug(DBG_CORE, "plugin: %s\n", plugin_path);
	epi=FindPluginByName(plugin_path);

	if(!epi) {
			fprintf(stderr, "Unable to find plugin %s in plugin list!\n", plugin_path);
			return;
	}
	if (event->button == 3)
	{
		GtkWidget *menu;
		Plugin_Callback_Data *PCD=g_new0(Plugin_Callback_Data, 1);
		GtkWidget *button;

		menu = gtk_menu_new();

		PCD->clist=clist;
		PCD->epi=epi;
		PCD->row=row;
		if(epi->status==PLUGIN_LOADED) { // Is this an already loaded plugin? 
			eb_debug(DBG_CORE, "Adding Reload button\n");
			button = gtk_menu_item_new_with_label(_("Reload"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
				GTK_SIGNAL_FUNC(reload_plugin_callback), PCD);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
			eb_debug(DBG_CORE, "Adding Unload button\n");
			button = gtk_menu_item_new_with_label(_("Unload"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
				GTK_SIGNAL_FUNC(unload_plugin_callback), PCD);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}
		else
		{
			eb_debug(DBG_CORE, "Adding Reload button\n");
			button = gtk_menu_item_new_with_label(_("Load"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
				GTK_SIGNAL_FUNC(reload_plugin_callback), PCD);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}
		gtk_widget_show(menu);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			event->button, event->time );
	}
	else if(event->button == 1)
	{
		GString		*windowName = g_string_new( NULL );
		const char	*pluginDescr = _("Plugin");

		if ( epi->service != NULL )
			pluginDescr = epi->service;

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
		if ( epi->pi.prefs != NULL )
			eb_input_render( epi->pi.prefs, vbox );
		else {
			GtkLabel	*label = GTK_LABEL(gtk_label_new( NULL ));
			GString		*labelText = g_string_new( _("No preferences for ") );
			int		newWidth = 0;
			int		newHeight = 0;

			if ( epi->service != NULL )
			{
				labelText = g_string_append( labelText, epi->service );
			}
			else
			{
				labelText = g_string_append( labelText, "'" );
				labelText = g_string_append( labelText, epi->pi.brief_desc );
				labelText = g_string_append( labelText, "'" );
			}

			gtk_label_set_text( label, labelText->str );
			gtk_widget_show( GTK_WIDGET(label) );

			/* adjust to a reasonable height if necessary */
			newHeight = GTK_WIDGET(label)->allocation.height;
			if ( newHeight < 50 )
				newHeight = 50;

			gtk_widget_set_usize( GTK_WIDGET(label), newWidth, newHeight );

			/*gtk_container_add(GTK_CONTAINER(plugin_prefs_win), GTK_WIDGET(label));*/
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
				GTK_SIGNAL_FUNC(update_plugin_prefs), (gpointer)epi->pi.prefs );

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
				   GTK_SIGNAL_FUNC(cancel_plugin_prefs), (gpointer)epi->pi.prefs );

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
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * button;
	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
	GtkWidget *vbox2 = NULL;
	GtkWidget * module_list=NULL;
	GtkWidget *w;
	int Columns=7, col=0;
	char *titles[7] = {_("Type"), 
			   _("Brief Description"), 
			   _("Status"), 
			   _("Version"), 
			   _("Date Last Modified") ,
			   _("Path to module"), 
			   _("Error Text")};
	char *modules_path, *ptr=NULL;
	int mod_path_pos=0;

	modules_path_text = gtk_text_new(NULL, NULL);
	gtk_editable_set_editable(GTK_EDITABLE(modules_path_text), TRUE);

	vbox2 = gtk_vbox_new(TRUE, 5);
	hbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Module Paths"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	button = eb_push_button(_("Rescan"), vbox2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(reload_modules), (gpointer)button);


	modules_path = cGetLocalPref("modules_path");
	ptr=modules_path;
	while( (ptr=strchr(ptr, ':')) )
		ptr[0]='\n';
	gtk_widget_set_usize(modules_path_text, 200, 75);
	gtk_editable_insert_text(GTK_EDITABLE(modules_path_text), modules_path, strlen(modules_path), &mod_path_pos);
	gtk_box_pack_start(GTK_BOX(hbox), modules_path_text, TRUE, TRUE, 10);
	gtk_widget_show(modules_path_text);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 10);
	gtk_widget_show(hbox);

	w=gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(w, 400, 200);
	/* Need to create a new one each time window is created as the old one was destroyed */
	module_list=gtk_clist_new_with_titles(Columns, titles);
	gtk_container_add(GTK_CONTAINER(w), module_list);
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 10);
	SetPref("widget::module_list", module_list);
	build_modules_list(module_list);
	gtk_clist_set_reorderable(GTK_CLIST(module_list), TRUE);
	gtk_clist_set_auto_sort(GTK_CLIST(module_list), TRUE);
	gtk_clist_column_titles_active(GTK_CLIST(module_list));
	gtk_signal_connect(GTK_OBJECT(module_list), "click-column",
			GTK_SIGNAL_FUNC(plugin_column_sort), NULL);
	gtk_clist_set_selection_mode(GTK_CLIST(module_list), GTK_SELECTION_SINGLE);
	for(col=0;col<Columns;col++)
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
	char *modules_path=NULL;
	char *ptr=NULL;
	if(modules_path_text==NULL) return;
	modules_path=gtk_editable_get_chars(GTK_EDITABLE(modules_path_text), 0, -1);
	ptr=modules_path;
	while( (ptr=strchr(ptr, '\n')) )
		ptr[0]=':';
	cSetLocalPref("modules_path", modules_path);
	g_free(modules_path);
     gtk_widget_destroy(modules_path_text);
     modules_path_text=NULL;
}

static void write_module_prefs(FILE *fp)
{
	GList *plugins=NULL;
	LList *master_prefs=NULL;
	LList *current_prefs=NULL;
	eb_PLUGIN_INFO *epi=NULL;

	fprintf(fp,"modules_path=%s\n", cGetLocalPref("modules_path"));

	fprintf(fp,"plugins\n");
	for(plugins=GetPref(EB_PLUGIN_LIST); plugins; plugins=plugins->next) {
		epi=plugins->data;
		fprintf(fp, "\t%s\n", epi->name);
		master_prefs=GetPref(epi->name);
		master_prefs=value_pair_remove(master_prefs, "load");
		current_prefs=eb_input_to_value_pair(epi->pi.prefs);
		if(epi->status==PLUGIN_LOADED)
			current_prefs = value_pair_add(current_prefs, "load", "1");
		else
			current_prefs = value_pair_add(current_prefs, "load", "0");
		master_prefs=value_pair_update(master_prefs, current_prefs);
		SetPref(epi->name, master_prefs);
		value_pair_print_values(master_prefs, fp, 2);
		fprintf(fp, "\tend\n");
		value_pair_free(current_prefs);
	}
	fprintf(fp,"end\n");
}

/*
 * Sound Preferences Tab
 */

static int	do_no_sound_when_away = 0;
static int	do_no_sound_when_away_old = 0;

static int	do_no_sound_for_ignore = 1;
static int	do_no_sound_for_ignore_old = 1;

static int	do_online_sound = 1;
static int	do_online_sound_old = 1;

static int	do_play_send = 1;
static int do_play_send_old = 1;

static int	do_play_first = 1;
static int	do_play_first_old = 1;

static int	do_play_receive = 1;
static int	do_play_receive_old = 1;

static char BuddyArriveFilename_old[1024] = BuddyArriveDefault;
static char BuddyAwayFilename_old[1024] = BuddyLeaveDefault;
static char BuddyLeaveFilename_old[1024] = BuddyLeaveDefault;
static char FirstMsgFilename_old[1024] = ReceiveDefault;
static char ReceiveFilename_old[1024] = ReceiveDefault;
static char SendFilename_old[1024] = SendDefault;

static gfloat volume_sound_old = 0;

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

     /* Values */
     do_no_sound_when_away_old = do_no_sound_when_away;
     do_no_sound_for_ignore_old = do_no_sound_for_ignore;
     do_online_sound_old = do_online_sound;
     do_play_send_old = do_play_send;
     do_play_first_old = do_play_first;
     do_play_receive_old = do_play_receive;
     strncpy(BuddyArriveFilename_old, cGetLocalPref("BuddyArriveFilename"), 1024);
     strncpy(BuddyAwayFilename_old, cGetLocalPref("BuddyAwayFilename"), 1024);
     strncpy(BuddyLeaveFilename_old, cGetLocalPref("BuddyLeaveFilename"), 1024);
     strncpy(SendFilename_old, cGetLocalPref("SendFilename"), 1024);
     strncpy(ReceiveFilename_old, cGetLocalPref("ReceiveFilename"), 1024);
     strncpy(FirstMsgFilename_old, cGetLocalPref("FirstMsgFilename"), 1024);
     volume_sound_old = iGetLocalPref("SoundVolume");

     /* General Tab */

    sound_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(sound_vbox);

     eb_button(_("Disable sounds when I am away"), &do_no_sound_when_away, sound_vbox);
     eb_button(_("Disable sounds for Ignored people"), &do_no_sound_for_ignore, sound_vbox);
     eb_button(_("Play sounds when people sign on or off"), &do_online_sound, sound_vbox);
     eb_button(_("Play a sound when sending a message"), &do_play_send, sound_vbox);
     eb_button(_("Play a sound when receiving a message"), &do_play_receive, sound_vbox);
     eb_button(_("Play a special sound when receiving first message"), &do_play_first, sound_vbox);

    gtk_notebook_append_page(GTK_NOTEBOOK(sound_note), sound_vbox, gtk_label_new(_("General")));

     /* Files Tab */

     sound_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(sound_vbox);

     //FIXME: These widgets are never properly freed, why not?
     arrivesoundentry = 
	  add_sound_file_selection_box(_("Contact signs on: "),
				       sound_vbox,
				       cGetLocalPref("BuddyArriveFilename"),
				       BUDDY_ARRIVE);
     awaysoundentry = 
	  add_sound_file_selection_box(_("Contact goes away: "),
				       sound_vbox,
				       cGetLocalPref("BuddyAwayFilename"),
				       BUDDY_AWAY);
     leavesoundentry = 
	  add_sound_file_selection_box(_("Contact signs off: "),
				       sound_vbox,
				       cGetLocalPref("BuddyLeaveFilename"),
				       BUDDY_LEAVE);

     sendsoundentry = 
	  add_sound_file_selection_box(_("Message sent: "),
				       sound_vbox,
				       cGetLocalPref("SendFilename"),
				       SEND);

     receivesoundentry = 
	  add_sound_file_selection_box(_("Message received: "),
				       sound_vbox,
				       cGetLocalPref("ReceiveFilename"),
				       RECEIVE);

     firstmsgsoundentry = 
	  add_sound_file_selection_box(_("First message received: "),
				       sound_vbox,
				       cGetLocalPref("FirstMsgFilename"),
				       FIRSTMSG);

     volumesoundentry =
       add_sound_volume_selection_box(_("Relative volume (dB)"),
				      sound_vbox,
				      GTK_ADJUSTMENT(gtk_adjustment_new(iGetLocalPref("SoundVolume"),
							 -40,0,1,5,0)));

     gtk_widget_show(sound_vbox);
     gtk_notebook_append_page(GTK_NOTEBOOK(sound_note), sound_vbox, 
			      gtk_label_new(_("Files")));

     gtk_widget_show(sound_note);
     gtk_container_add(GTK_CONTAINER(parent_window), sound_note);

}

static void cancel_sound_prefs()
{
     /* Values */
     do_play_send = do_play_send_old;
     do_play_first = do_play_first_old;
     do_play_receive = do_play_receive_old;
     do_online_sound = do_online_sound_old;
     do_no_sound_when_away = do_no_sound_when_away_old;
     do_no_sound_for_ignore = do_no_sound_for_ignore_old;
	 cSetLocalPref("BuddyArriveFilename", BuddyArriveFilename_old);
	 cSetLocalPref("BuddyAwayFilename", BuddyAwayFilename_old);
	 cSetLocalPref("BuddyLeaveFilename", BuddyLeaveFilename_old);
	 cSetLocalPref("SendFilename", SendFilename_old);
	 cSetLocalPref("ReceiveFilename", ReceiveFilename_old);
	 cSetLocalPref("FirstMsgFilename", FirstMsgFilename_old);
     fSetLocalPref("SoundVolume", volume_sound_old);

     /* Widgets */
     gtk_entry_set_text (GTK_ENTRY (arrivesoundentry), cGetLocalPref("BuddyArriveFilename"));
     gtk_entry_set_text (GTK_ENTRY (awaysoundentry), cGetLocalPref("BuddyAwayFilename"));
     gtk_entry_set_text (GTK_ENTRY (leavesoundentry), cGetLocalPref("BuddyLeaveFilename"));
     gtk_entry_set_text (GTK_ENTRY (sendsoundentry), cGetLocalPref("SendFilename"));
     gtk_entry_set_text (GTK_ENTRY (receivesoundentry), cGetLocalPref("ReceiveFilename"));
     gtk_entry_set_text (GTK_ENTRY (firstmsgsoundentry), cGetLocalPref("FirstMsgFilename"));
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
     /*     widget = gtk_entry_new ();
	    gtk_entry_set_text (GTK_ENTRY (widget), initialFilename); */
     gtk_widget_ref (widget);
     gtk_signal_connect(GTK_OBJECT(adjust), "value_changed", 
			GTK_SIGNAL_FUNC(soundvolume_changed), 
			(gpointer) dummy);
     gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
     gtk_widget_show (widget);

     gtk_widget_show(hbox);
     return widget;
}

static void update_soundfile_entries()
{
	if(arrivesoundentry == NULL) return;
	cSetLocalPref("BuddyArriveFilename", 
			gtk_entry_get_text(GTK_ENTRY (arrivesoundentry)));
	cSetLocalPref("BuddyLeaveFilename", 
			gtk_entry_get_text(GTK_ENTRY (leavesoundentry)));
	cSetLocalPref("SendFilename", 
			gtk_entry_get_text(GTK_ENTRY (sendsoundentry)));
	cSetLocalPref("ReceiveFilename", 
			gtk_entry_get_text(GTK_ENTRY (receivesoundentry)));
	cSetLocalPref("BuddyAwayFilename", 
			gtk_entry_get_text(GTK_ENTRY (awaysoundentry)));
	cSetLocalPref("FirstMsgFilename", 
			gtk_entry_get_text(GTK_ENTRY (firstmsgsoundentry)));
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
	  cSetLocalPref("BuddyArriveFilename", selected_filename);
	  gtk_entry_set_text (GTK_ENTRY (arrivesoundentry), 
			      selected_filename);
	  break;
     case BUDDY_LEAVE:	
	  cSetLocalPref("BuddyLeaveFilename", selected_filename);
	  gtk_entry_set_text (GTK_ENTRY (leavesoundentry), 
			      selected_filename);
	  break;
     case SEND:		
	  cSetLocalPref("SendFilename", selected_filename);
	  gtk_entry_set_text (GTK_ENTRY (sendsoundentry), selected_filename);
	  break;
     case RECEIVE:		
	  cSetLocalPref("ReceiveFilename", selected_filename);
	  gtk_entry_set_text (GTK_ENTRY (receivesoundentry), 
			      selected_filename);
	  break;
     case BUDDY_AWAY: 	
	  cSetLocalPref("BuddyAwayFilename", selected_filename);
	  gtk_entry_set_text (GTK_ENTRY (awaysoundentry), selected_filename);
	  break;
     case FIRSTMSG:		
	  cSetLocalPref("FirstMsgFilename", selected_filename);
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
	  playsoundfile(cGetLocalPref("BuddyArriveFilename"));
	  break;

     case BUDDY_LEAVE:
	  playsoundfile(cGetLocalPref("BuddyLeaveFilename"));
	  break;

     case SEND:
	  playsoundfile(cGetLocalPref("SendFilename"));
	  break;

     case RECEIVE:
	  playsoundfile(cGetLocalPref("ReceiveFilename"));
	  break;

     case BUDDY_AWAY:
	  playsoundfile(cGetLocalPref("BuddyAwayFilename"));
	  break;

     case FIRSTMSG:
	  playsoundfile(cGetLocalPref("FirstMsgFilename"));
	  break;

     }
}

static void soundvolume_changed(GtkAdjustment * adjust, int userdata)
{
  fSetLocalPref("SoundVolume", adjust->value);
}

static void write_sound_prefs(FILE *fp)
{
	update_soundfile_entries();

     fprintf(fp,"do_play_send=%d\n", do_play_send);
     fprintf(fp,"do_play_first=%d\n", do_play_first);
     fprintf(fp,"do_play_receive=%d\n", do_play_receive);
     fprintf(fp,"do_online_sound=%d\n", do_online_sound);
     fprintf(fp,"do_no_sound_when_away=%d\n", do_no_sound_when_away);
     fprintf(fp,"BuddyArriveFilename=%s\n", cGetLocalPref("BuddyArriveFilename"));
     fprintf(fp,"BuddyAwayFilename=%s\n", cGetLocalPref("BuddyAwayFilename"));
     fprintf(fp,"BuddyLeaveFilename=%s\n", cGetLocalPref("BuddyLeaveFilename"));
     fprintf(fp,"SendFilename=%s\n", cGetLocalPref("SendFilename"));
     fprintf(fp,"ReceiveFilename=%s\n", cGetLocalPref("ReceiveFilename"));
     fprintf(fp,"FirstMsgFilename=%s\n", cGetLocalPref("FirstMsgFilename"));
     fprintf(fp,"SoundVolume=%f\n", fGetLocalPref("SoundVolume"));
}

static void destroy_sound_prefs(void) {
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
 * Reading/Writing Prefs
 */

enum {
	CORE_PREF,
	PLUGIN_PREF,
	SERVICE_PREF
};

// todo: re-organize to separate pref reading into modules
void eb_read_prefs()
{
	char buff[1024];
	int pref_type=0;
	FILE * fp;
	char *param=buff;
	char *val=buff;
	/* Set some default values */
	iSetLocalPref("length_contact_window", length_contact_window_default);
	iSetLocalPref("width_contact_window", width_contact_window_default);
	iSetLocalPref("status_show_level", status_show_level_default);
#ifdef __MINGW32__
	{
		char buff[1024];
		// Base this in WinVer? Nah..
		char *base_dir = "\\Program Files\\ayttm\\";
		g_snprintf(buff, 1024, "%s%s",config_dir,MODULE_DIR);
		cSetLocalPref("modules_path", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, BuddyArriveDefault);
		cSetLocalPref("BuddyArriveFilename", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, BuddyLeaveDefault);
		cSetLocalPref("BuddyAwayFilename", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, BuddyLeaveDefault);
		cSetLocalPref("BuddyLeaveFilename", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, ReceiveDefault);
		cSetLocalPref("FirstMsgFilename", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, ReceiveDefault);
		cSetLocalPref("ReceiveFilename", buff);
		g_snprintf(buff, 1024, "%s%s",config_dir, SendDefault);
		cSetLocalPref("SendFilename", buff);
	}
#else
	cSetLocalPref("modules_path", MODULE_DIR);
	cSetLocalPref("BuddyArriveFilename", BuddyArriveDefault);
	cSetLocalPref("BuddyAwayFilename", BuddyLeaveDefault);
	cSetLocalPref("BuddyLeaveFilename", BuddyLeaveDefault);
	cSetLocalPref("FirstMsgFilename", ReceiveDefault);
	cSetLocalPref("ReceiveFilename", ReceiveDefault);
	cSetLocalPref("SendFilename", SendDefault);
#endif
	iSetLocalPref("FontSize", font_size_def);

	g_snprintf(buff, 1024, "%sprefs",config_dir);
	fp = fopen(buff, "r");
	if(!fp)
	{
		printf("Creating prefs\n");
		write_prefs();
		return;
	}
	while(!feof(fp))
	{
		fgets(buff, 1024, fp);
		param=buff;
		if(feof(fp))
			break;
		g_strstrip(param);
		pref_type=CORE_PREF;
		if(!strcasecmp(buff, "plugins"))
			pref_type=PLUGIN_PREF;
		else if(!strcasecmp(buff, "connections"))
			pref_type=SERVICE_PREF;
		if(pref_type!=CORE_PREF)
		{
			for(;;)
			{
				int id=-1;
				char *plugin_name=NULL;
				LList * session_prefs = NULL;
				LList *osession_prefs = NULL;
				fgets(buff, 1024, fp);
				param=buff;
				g_strstrip(param);
				if(!strcasecmp(param, "end"))
				{
					break;
				}
				switch(pref_type) {
				case PLUGIN_PREF:
					plugin_name=strdup(param);
					break;
				case SERVICE_PREF:
					id = get_service_id(param);
					break;
				default:
					break;
				}

				for(;;)
				{
					fgets(buff, 1024, fp);
					param=buff;
					g_strstrip(param);
					if(!strcasecmp(param, "end"))
					{
						switch(pref_type) {
						case PLUGIN_PREF:
							osession_prefs = SetPref(plugin_name, session_prefs);
							if(osession_prefs)
							{
								eb_debug(DBG_CORE, "Freeing osession_prefs\n");
								value_pair_free(osession_prefs);
							}
							free(plugin_name);
							break;
						case SERVICE_PREF:
							osession_prefs = SetPref(get_service_name(id), session_prefs);
							if(osession_prefs)
							{
								eb_debug(DBG_CORE, "Freeing osession_prefs\n");
								value_pair_free(osession_prefs);
							}
							break;
						default:
							eb_debug(DBG_CORE, "Error!  We're not supposed to ever get here!\n");
							break;
						}
						break;
					}
					else
					{
						val=param;

						while(*val != 0 && *val !='=')
							val++;
						if(*val=='=')
						{
							*val='\0';
							val++;
						}

						/*
						* if quoted strip off the quotes
						*/

						if(*val == '"')
						{
							val++;
							val[strlen(val)-1] = '\0';
						}
						eb_debug(DBG_CORE,"Adding %s:%s to session_prefs\n", param, val);
						session_prefs = value_pair_add(session_prefs, param, val);
					}
				}
			}
			continue;
		} /* if(pref_type != CORE_PREF) */
		val=param;

		while(*val != 0 && *val !='=')
			val++;
		if(*val=='=')
		{
			*val='\0';
			val++;
		}
		cSetLocalPref(param, val);
		/* enable debugging as soon as possible */
		if(!strcasecmp(param, "do_ayttm_debug"))
			do_ayttm_debug = iGetLocalPref("do_ayttm_debug");
	}

	/* general prefs */
	do_login_on_startup   = iGetLocalPref("do_login_on_startup");
	do_plugin_debug       = iGetLocalPref("do_plugin_debug");
#ifdef HAVE_ISPELL
	do_spell_checking     = iGetLocalPref("do_spell_checking");
#endif
	use_alternate_browser = iGetLocalPref("use_alternate_browser");
	do_noautoresize       = iGetLocalPref("do_noautoresize");
	do_ayttm_debug        = iGetLocalPref("do_ayttm_debug");
	do_ayttm_debug_html   = iGetLocalPref("do_ayttm_debug_html");

	/* logging prefs */
	do_logging            = iGetLocalPref("do_logging");
	do_strip_html         = iGetLocalPref("do_strip_html");
	do_restore_last_conv  = iGetLocalPref("do_restore_last_conv");

	/* layout prefs */
	do_tabbed_chat        = iGetLocalPref("do_tabbed_chat");
	do_tabbed_chat_orient = iGetLocalPref("do_tabbed_chat_orient");

	/* sound prefs */
	do_online_sound       = iGetLocalPref("do_online_sound");
	do_no_sound_when_away = iGetLocalPref("do_no_sound_when_away");
	do_no_sound_for_ignore= iGetLocalPref("do_no_sound_for_ignore");
	do_play_send          = iGetLocalPref("do_play_send");
	do_play_first         = iGetLocalPref("do_play_first");
	do_play_receive       = iGetLocalPref("do_play_receive");

	/* chat prefs */
	do_typing_notify      = iGetLocalPref("do_typing_notify");
	do_send_typing_notify = iGetLocalPref("do_send_typing_notify");
	do_escape_close       = iGetLocalPref("do_escape_close");
	do_convo_timestamp    = iGetLocalPref("do_convo_timestamp");
	do_enter_send         = iGetLocalPref("do_enter_send");
	do_ignore_unknown     = iGetLocalPref("do_ignore_unknown");
	font_size	          = iGetLocalPref("FontSize");
	do_multi_line         = iGetLocalPref("do_multi_line");
	do_raise_window       = iGetLocalPref("do_raise_window");	
	do_send_idle_time     = iGetLocalPref("do_send_idle_time");
	do_timestamp          = iGetLocalPref("do_timestamp");
	do_ignore_fore        = iGetLocalPref("do_ignore_fore");
	do_ignore_back        = iGetLocalPref("do_ignore_back");
	do_ignore_font        = iGetLocalPref("do_ignore_font");
	do_smiley             = iGetLocalPref("do_smiley");

	gtk_accelerator_parse(cGetLocalPref("accel_next_tab"), &(accel_next_tab.keyval), &(accel_next_tab.modifiers));
	gtk_accelerator_parse(cGetLocalPref("accel_prev_tab"), &(accel_prev_tab.keyval), &(accel_prev_tab.modifiers));

#ifdef HAVE_ICONV_H
	use_recoding          =iGetLocalPref("use_recoding");
#endif

     proxy_set_proxy(iGetLocalPref("proxy_type"), cGetLocalPref("proxy_host"), iGetLocalPref("proxy_port"));
     proxy_set_auth(iGetLocalPref("do_proxy_auth"), cGetLocalPref("proxy_user"), cGetLocalPref("proxy_password"));
     fclose(fp);
}

void write_prefs()
{
     char buff[1024], file[1024];
     FILE * fp;
     g_snprintf(buff, 1024, "%sprefs.tmp",config_dir);
     g_snprintf(file, 1024, "%sprefs",config_dir);
     fp = fopen(buff, "w");
     write_general_prefs(fp);
     write_logs_prefs(fp);
     write_sound_prefs(fp);
     write_chat_prefs(fp);
     write_layout_prefs(fp);
#ifdef HAVE_ICONV_H
     write_encoding_prefs(fp);
#endif
     write_proxy_prefs(fp);
     write_module_prefs(fp);
     fprintf(fp, "end\n");
     fclose(fp);
#ifdef __MINGW32__
     unlink(file);
#endif
     rename(buff,file);
     eb_read_prefs();
}

/*
 * OK && Cancel callbacks
 */

static void ok_callback(GtkWidget * widget, gpointer data)
{
     write_prefs();
}

static void cancel_callback(GtkWidget * widget, gpointer data)
{
     cancel_general_prefs();
     cancel_logs_prefs();
     cancel_layout_prefs();
     cancel_chat_prefs();
     cancel_sound_prefs();
     cancel_proxy_prefs();
}

/*
 * Destroy prefs window
 */
static void destroy(GtkWidget * widget, gpointer data)
{
     is_prefs_open = 0;
     destroy_proxy();
     destroy_general_prefs();
     destroy_chat_prefs();
     destroy_sound_prefs();
     /* reset current parent so error dialogs work correctly */
#ifdef HAVE_ICONV_H
     destroy_encodings();
#endif
	 destroy_modules();
     /* it'd be nice to check validity of recoding here 
	and cancel change if not supported */

#ifdef HAVE_ISPELL
     if ((!do_spell_checking) && (gtkspell_running()))
	     gtkspell_stop();
#endif
     gtk_widget_destroy(current_parent_widget);
     current_parent_widget = NULL;

}
