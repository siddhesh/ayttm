/* select-keys.c - GTK+ based key selection
 *      Copyright (C) 2001 Werner Koch (dd9jn)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "intl.h"
#include "select-keys.h"
#include "debug.h"
#include "dialog.h"
#include "gtk/gtkutils.h"

#define DIM(v) (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)

enum col_titles { 
    COL_ALGO,
    COL_KEYID,
    COL_NAME,
    COL_EMAIL,
    COL_VALIDITY,

    N_COL_TITLES
};

static void set_row (GtkCList *clist, GpgmeKey key);
static void fill_clist (struct select_keys_s *sk, const char *pattern);
static void create_dialog (struct select_keys_s *sk);
static void open_dialog (struct select_keys_s *sk);
static void close_dialog (struct select_keys_s *sk);
static gint delete_event_cb (GtkWidget *widget,
                             GdkEventAny *event, gpointer data);
static void key_pressed_cb (GtkWidget *widget,
                            GdkEventKey *event, gpointer data);
static void select_btn_cb (GtkWidget *widget, gpointer data);
static void cancel_btn_cb (GtkWidget *widget, gpointer data);
static void other_btn_cb (GtkWidget *widget, gpointer data);
static void sort_keys (struct select_keys_s *sk, enum col_titles column);
static void sort_keys_name (GtkWidget *widget, gpointer data);
static void sort_keys_email (GtkWidget *widget, gpointer data);

char *aycrypt_last_pass = NULL;
int aycrypt_pass_ack = 0;

static void
update_progress (struct select_keys_s *sk, int running, const char *pattern)
{
    static int windmill[] = { '-', '\\', '|', '/' };
    char *buf;

    if (!running)
        buf = g_strdup_printf (_("Please select key for `%s'"), 
                               pattern);
    else 
        buf = g_strdup_printf (_("Collecting info for `%s' ... %c"), 
                               pattern,
                               windmill[running%DIM(windmill)]);
    gtk_label_set_text (sk->toplabel, buf);
    g_free (buf);
}


/**
 * select_keys_get_recipients:
 * @recp_names: A list of email addresses
 * 
 * Select a list of recipients from a given list of email addresses.
 * This may pop up a window to present the user a choice, it will also
 * check that the recipients key are all valid.
 * 
 * Return value: NULL on error or a list of list of recipients.
 **/
struct select_keys_s
gpgmegtk_recipient_selection (GSList *recp_names, int crypt, int sign)
{
    struct select_keys_s sk;
    GpgmeError err;
    GSList *recp_walk = recp_names;
    memset (&sk, 0, sizeof sk);

    err = gpgme_recipients_new (&sk.rset);
    if (err) {
        g_warning ("failed to allocate recipients set: %s",
                   gpgme_strerror (err));
	sk.rset= NULL; sk.key = NULL;	   
        return sk;
    }

    sk.do_crypt = crypt;
    sk.do_sign = sign;
    open_dialog (&sk);

    gtk_clist_clear (sk.clist);
    do {
        sk.pattern = recp_walk? recp_walk->data:NULL;
	printf("sk.pattern = %s\n",sk.pattern);
        fill_clist (&sk, sk.pattern);
        update_progress (&sk, 0, recp_names?recp_names->data:NULL);
        
        if (recp_walk)
            recp_walk = recp_walk->next;
    } while (recp_walk);
    gtk_main ();

    close_dialog (&sk);

    if (!sk.okay) {
        gpgme_recipients_release (sk.rset);
        sk.rset = NULL; sk.key = NULL;
    }
    return sk;
} 

static void
destroy_key (gpointer data)
{
    GpgmeKey key = data;
    gpgme_key_release (key);
}

static void
set_row (GtkCList *clist, GpgmeKey key)
{
    const char *s;
    const char *text[N_COL_TITLES];
    char *algo_buf;
    int row = 0;
    GpgmeKey temp;
    
    while ((temp = gtk_clist_get_row_data(clist, row)) != NULL) {
	if (!strcmp(gpgme_key_get_string_attr (key,  GPGME_ATTR_KEYID, NULL, 0),
		    gpgme_key_get_string_attr (temp, GPGME_ATTR_KEYID, NULL, 0)))
		return; /* already found */
	row++;    
    }
    /* first check whether the key is capable of encryption which is not
     * the case for revoked, expired or sign-only keys */
    if ( !gpgme_key_get_ulong_attr (key, GPGME_ATTR_CAN_ENCRYPT, NULL, 0 ) )
        return;
    
    algo_buf = g_strdup_printf ("%lu/%s", 
         gpgme_key_get_ulong_attr (key, GPGME_ATTR_LEN, NULL, 0 ),
         gpgme_key_get_string_attr (key, GPGME_ATTR_ALGO, NULL, 0 ) );
    text[COL_ALGO] = algo_buf;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_KEYID, NULL, 0);
    if (strlen (s) == 16)
        s += 8; /* show only the short keyID */
    text[COL_KEYID] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_NAME, NULL, 0);
    text[COL_NAME] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0);
    text[COL_EMAIL] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_VALIDITY, NULL, 0);
    text[COL_VALIDITY] = s;

    row = gtk_clist_append (clist, (gchar**)text);
    g_free (algo_buf);

    gtk_clist_set_row_data_full (clist, row, key, destroy_key);
}


static void 
fill_clist (struct select_keys_s *sk, const char *pattern)
{
    GtkCList *clist;
    GpgmeCtx ctx;
    GpgmeError err;
    GpgmeKey key;
    int running=0;

    g_return_if_fail (sk);
    clist = sk->clist;
    g_return_if_fail (clist);


    /*gtk_clist_freeze (select_keys.clist);*/
    err = gpgme_new (&ctx);
    g_assert (!err);

    sk->select_ctx = ctx;

    update_progress (sk, ++running, pattern);
    while (gtk_events_pending ())
        gtk_main_iteration ();

    err = gpgme_op_keylist_start (ctx, pattern, 0);
    if (err) {
        sk->select_ctx = NULL;
        return;
    }
    update_progress (sk, ++running, pattern);
    while ( !(err = gpgme_op_keylist_next ( ctx, &key )) ) {
        set_row (clist, key ); key = NULL;
        update_progress (sk, ++running, pattern);
        while (gtk_events_pending ())
            gtk_main_iteration ();
    }
    sk->select_ctx = NULL;
    gpgme_release (ctx);
    /*gtk_clist_thaw (select_keys.clist);*/
}

static void crypt_changed_cb(GtkWidget *w, void *data)
{
	struct select_keys_s *sk = (struct select_keys_s *)data;
	sk->do_crypt = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static void sign_changed_cb(GtkWidget *w, void *data)
{
	struct select_keys_s *sk = (struct select_keys_s *)data;
	sk->do_sign = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static void 
create_dialog (struct select_keys_s *sk)
{
    GtkWidget *window;
    GtkWidget *vbox, *vbox2, *hbox;
    GtkWidget *bbox;
    GtkWidget *scrolledwin;
    GtkWidget *clist;
    GtkWidget *label;
    GtkWidget *vbox3;
    GtkWidget *select_btn, *cancel_btn, *other_btn, *do_crypt_btn, *do_sign_btn;
    const char *titles[N_COL_TITLES];

    g_assert (!sk->window);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize (window, 520, 280);
    gtk_container_set_border_width (GTK_CONTAINER (window), 8);
    gtk_window_set_title (GTK_WINDOW (window), _("Select Keys"));
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event_cb), sk);
    gtk_signal_connect (GTK_OBJECT (window), "key_press_event",
                        GTK_SIGNAL_FUNC (key_pressed_cb), sk);

    vbox = gtk_vbox_new (FALSE, 8);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox  = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new ( "" );
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

    scrolledwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    titles[COL_ALGO]     = _("Size");
    titles[COL_KEYID]    = _("Key ID");
    titles[COL_NAME]     = _("Name");
    titles[COL_EMAIL]    = _("Address");
    titles[COL_VALIDITY] = _("Val");

    clist = gtk_clist_new_with_titles (N_COL_TITLES, (char**)titles);
    gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_ALGO,      72);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_KEYID,     76);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_NAME,     130);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_EMAIL,    130);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_VALIDITY,  20);
    gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    gtk_signal_connect (GTK_OBJECT(GTK_CLIST(clist)->column[COL_NAME].button),
		        "clicked",
                        GTK_SIGNAL_FUNC(sort_keys_name), sk);
    gtk_signal_connect (GTK_OBJECT(GTK_CLIST(clist)->column[COL_EMAIL].button),
		        "clicked",
                        GTK_SIGNAL_FUNC(sort_keys_email), sk);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    bbox = gtk_hbox_new (FALSE, 2);
    select_btn = gtkut_create_label_button (_("Select"), GTK_SIGNAL_FUNC (select_btn_cb), sk);
    cancel_btn = gtkut_create_label_button (_("Cancel"), GTK_SIGNAL_FUNC (cancel_btn_cb), sk);
    other_btn = gtkut_create_label_button (_("Other..."), GTK_SIGNAL_FUNC (other_btn_cb), sk);
    vbox3 = gtk_vbox_new(FALSE,2);
    do_crypt_btn = gtkut_check_button( vbox3, _("Enable encryption"), sk->do_crypt,
        crypt_changed_cb, sk );
    do_sign_btn = gtkut_check_button( vbox3, _("Enable signing"), sk->do_sign,
        sign_changed_cb, sk );

    gtk_box_pack_end (GTK_BOX (hbox), select_btn, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox), other_btn, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox), cancel_btn, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox), vbox3, FALSE, FALSE, 0);

    gtk_box_pack_end (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
    
    vbox2 = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

    gtk_widget_show_all (window);

    sk->window = window;
    sk->toplabel = GTK_LABEL (label);
    sk->clist  = GTK_CLIST (clist);
}


static void
open_dialog (struct select_keys_s *sk)
{
    if (!sk->window)
        create_dialog (sk);
    sk->okay = 0;
    sk->sort_column = N_COL_TITLES; /* use an invalid value */
    sk->sort_type = GTK_SORT_ASCENDING;
    gtk_widget_show (sk->window);
}


static void
close_dialog (struct select_keys_s *sk)
{
    g_return_if_fail (sk);
    gtk_widget_destroy (sk->window);
    sk->window = NULL;
}


static gint
delete_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    struct select_keys_s *sk = data;

    sk->okay = 0;
    gtk_main_quit ();

    return TRUE;
}


static void 
key_pressed_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    struct select_keys_s *sk = data;

    g_return_if_fail (sk);
    if (event && event->keyval == GDK_Escape) {
        sk->okay = 0;
        gtk_main_quit ();
    }
}


static void 
select_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = (struct select_keys_s *)data;
    int row;
    GpgmeKey key;

    g_return_if_fail (sk);
    if (!GTK_CLIST(sk->clist)->selection) {
        return;
    }
    row = GPOINTER_TO_INT(GTK_CLIST(sk->clist)->selection->data);
    key = gtk_clist_get_row_data(sk->clist, row);
    if (key) {
        const char *s = gpgme_key_get_string_attr (key,
                                                   GPGME_ATTR_FPR,
                                                   NULL, 0 );
        if ( gpgme_key_get_ulong_attr (key, GPGME_ATTR_VALIDITY, NULL, 0 )
             < GPGME_VALIDITY_FULL ) {
        }
        if (!gpgme_recipients_add_name_with_validity (sk->rset, s,
                                                      GPGME_VALIDITY_FULL) ) {
            sk->okay = 1;
	    sk->key = strdup(s);
            gtk_main_quit ();
        }
    }
}


static void 
cancel_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;

    g_return_if_fail (sk);
    sk->okay = 0;
    if (sk->select_ctx)
        gpgme_cancel (sk->select_ctx);
    gtk_main_quit ();
}

static void
other_selected_cb(char *text, void *data)
{
    struct select_keys_s *sk = data;
    if (!text)
        return;
    fill_clist (sk, text);
    update_progress (sk, 0, sk->pattern);

}

static void
other_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;

    g_return_if_fail (sk);
    do_text_input_window_multiline( _("Enter another user or key ID:"),
                         _(""), 0, 0, other_selected_cb, sk);
}

static gint 
cmp_attr (gconstpointer pa, gconstpointer pb, GpgmeAttr attr)
{
    GpgmeKey a = ((GtkCListRow *)pa)->data;
    GpgmeKey b = ((GtkCListRow *)pb)->data;
    const char *sa, *sb;
    
    sa = a? gpgme_key_get_string_attr (a, attr, NULL, 0 ) : NULL;
    sb = b? gpgme_key_get_string_attr (b, attr, NULL, 0 ) : NULL;
    if (!sa)
        return !!sb;
    if (!sb)
        return -1;
    return strcasecmp(sa, sb);
}

static gint 
cmp_name (GtkCList *clist, gconstpointer pa, gconstpointer pb)
{
    return cmp_attr (pa, pb, GPGME_ATTR_NAME);
}

static gint 
cmp_email (GtkCList *clist, gconstpointer pa, gconstpointer pb)
{
    return cmp_attr (pa, pb, GPGME_ATTR_EMAIL);
}

static void
sort_keys ( struct select_keys_s *sk, enum col_titles column)
{
    GtkCList *clist = sk->clist;

    switch (column) {
      case COL_NAME:
        gtk_clist_set_compare_func (clist, cmp_name);
        break;
      case COL_EMAIL:
        gtk_clist_set_compare_func (clist, cmp_email);
        break;
      default:
        return;
    }

    /* column clicked again: toggle as-/decending */
    if ( sk->sort_column == column) {
        sk->sort_type = sk->sort_type == GTK_SORT_ASCENDING ?
                        GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
    }
    else
        sk->sort_type = GTK_SORT_ASCENDING;

    sk->sort_column = column;
    gtk_clist_set_sort_type (clist, sk->sort_type);
    gtk_clist_sort (clist);
}

static void
sort_keys_name (GtkWidget *widget, gpointer data)
{
    sort_keys ((struct select_keys_s*)data, COL_NAME);
}

static void
sort_keys_email (GtkWidget *widget, gpointer data)
{
    sort_keys ((struct select_keys_s*)data, COL_EMAIL);
}

static void 
passphrase_ok_cb(GtkWidget *widget, gpointer data)
{
    aycrypt_pass_ack = TRUE;
    gtk_main_quit();
}

static void 
passphrase_cancel_cb(GtkWidget *widget, gpointer data)
{
    aycrypt_pass_ack = FALSE;
    gtk_main_quit();
}

static gint
passphrase_deleted(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    passphrase_cancel_cb(NULL, NULL);
    return TRUE;
}


static void 
passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event && event->keyval == GDK_Escape)
        passphrase_cancel_cb(NULL, NULL);
}

static GtkWidget *
create_description (const gchar *desc)
{
    const gchar *cmd = NULL, *uid = NULL, *info = NULL;
    gchar *buf;
    GtkWidget *label;

    cmd = desc;
    uid = strchr (cmd, '\n');
    if (uid) {
        info = strchr (++uid, '\n');
        if (info )
            info++;
    }

    if (!uid)
        uid = _("[no user id]");
    if (!info)
        info = "";

    buf = g_strdup_printf (_("%sPlease enter the passphrase for:\n\n"
                           "  %.*s  \n"
                           "(%.*s)\n"),
                           !strncmp (cmd, "TRY_AGAIN", 9 ) ?
                           _("Bad passphrase! Try again...\n\n") : "",
                           strlen (uid), uid, strlen (info), info);

    label = gtk_label_new (buf);
    g_free (buf);

    return label;
}

const char*
passphrase_mbox (const char *desc)
{
    char *the_passphrase = NULL;
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *pass_label;
    GtkWidget *confirm_box;
    GtkWidget *window;
    GtkWidget *pass_entry;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;

    window = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_title(GTK_WINDOW(window), _("Passphrase"));
    gtk_widget_set_usize(window, 450, -1);
    gtk_container_set_border_width(GTK_CONTAINER(window), 4);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
    gtk_signal_connect(GTK_OBJECT(window), "delete_event",
                       GTK_SIGNAL_FUNC(passphrase_deleted), NULL);
    gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
                       GTK_SIGNAL_FUNC(passphrase_key_pressed), NULL);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    if (desc) {
        GtkWidget *label;
        label = create_description (desc);
        gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, TRUE, 0);
    }

    table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 8);
    gtk_table_set_row_spacings(GTK_TABLE(table), 12);
    gtk_table_set_col_spacings(GTK_TABLE(table), 8);


    pass_label = gtk_label_new("");
    gtk_table_attach (GTK_TABLE(table), pass_label, 0, 1, 0, 1,
                      GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
    gtk_misc_set_alignment (GTK_MISC (pass_label), 1, 0.5);

    pass_entry = gtk_entry_new();
    gtk_table_attach (GTK_TABLE(table), pass_entry, 1, 2, 0, 1,
                      GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);
    gtk_entry_set_visibility (GTK_ENTRY(pass_entry), FALSE);
    gtk_widget_grab_focus (pass_entry);


    confirm_box = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX(confirm_box), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing (GTK_BUTTON_BOX(confirm_box), 5);

    ok_button = gtk_button_new_with_label (_("OK"));
    gtk_box_pack_start (GTK_BOX(confirm_box), ok_button, TRUE, TRUE, 0);

    cancel_button = gtk_button_new_with_label (_("Cancel"));
    gtk_box_pack_start(GTK_BOX(confirm_box), cancel_button, TRUE, TRUE, 0);

    gtk_box_pack_end(GTK_BOX(vbox), confirm_box, FALSE, FALSE, 0);

    gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
                       GTK_SIGNAL_FUNC(passphrase_ok_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(pass_entry), "activate",
                       GTK_SIGNAL_FUNC(passphrase_ok_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
                       GTK_SIGNAL_FUNC(passphrase_cancel_cb), NULL);

    gtk_object_set (GTK_OBJECT(window), "type", GTK_WINDOW_DIALOG, NULL);
    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_policy (GTK_WINDOW(window), FALSE, FALSE, TRUE);
    
    gtk_widget_show_all(window);

    gtk_main();

    if (aycrypt_pass_ack) {
        the_passphrase = gtk_editable_get_chars(GTK_EDITABLE(pass_entry), 0, -1);
    }
    gtk_widget_destroy (window);

    return the_passphrase;
}

