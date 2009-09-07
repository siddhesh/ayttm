/* select-keys.c - GTK+ based key selection
 *  Copyright (C) 2001 Werner Koch (dd9jn)
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

// TODO Destroy key data from row

#ifdef HAVE_CONFIG_H
#include <config.h>
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
	COL_GPG_KEY,
	N_COL_TITLES
};

static void set_row (GtkListStore *clist_store, gpgme_key_t key);
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

	GSList *recp_walk = recp_names;
	memset (&sk, 0, sizeof sk);

	sk.do_crypt = crypt;
	sk.do_sign = sign;
	open_dialog (&sk);

	gtk_list_store_clear (GTK_LIST_STORE(gtk_tree_view_get_model(sk.clist)));
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
		g_free (sk.kset);
		sk.kset = NULL; sk.key = NULL;
	} else {
		/* Terminating the key list with a NULL */
		sk.kset = g_realloc( sk.kset, sizeof(gpgme_key_t) * (sk.num_keys + 1) );
		sk.kset[sk.num_keys] = NULL;
	}
	return sk;
} 

/* Release key data */
static gboolean destroy_key(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gpgme_key_t key;
	gtk_tree_model_get(model, iter, 
			COL_GPG_KEY, &key,
			-1);

	if(key)
		gpgme_key_release (key);

	return FALSE;
}

static void
destroy_keys (GtkWidget *widget, gpointer data)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	gtk_tree_model_foreach(model, destroy_key, NULL);
}

static void
set_row (GtkListStore *clist_store, gpgme_key_t key)
{
	const char *s;
	const char *text[N_COL_TITLES];
	char *algo_buf;
	GtkTreeIter row, newrow;
	gpgme_key_t temp;
	gboolean finished = FALSE;

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(clist_store), &row))
		gtk_tree_model_get(GTK_TREE_MODEL(clist_store), &row, 
				COL_GPG_KEY, &temp, -1);
	
	while ( !temp->subkeys && !finished ) {
		if ( !strcmp(key->subkeys->keyid, temp->subkeys->keyid) )
			return; /* already found */
		if( !gtk_tree_model_iter_next(GTK_TREE_MODEL(clist_store), &row)) 
			finished = TRUE;
		else
			gtk_tree_model_get(GTK_TREE_MODEL(clist_store), &row, 
					COL_GPG_KEY, &temp, -1);
	}

	if(key->subkeys)
		printf("Found key: %s\n", key->uids->email);

	/* first check whether the key is capable of encryption which is not
	 * the case for revoked, expired or sign-only keys */
	if ( !key->can_encrypt ) {
		printf("Cannot encrypt\n");
		return;
	}
	
	algo_buf = g_strdup_printf ("%u/%s", 
		 key->subkeys->length,
		 gpgme_pubkey_algo_name (key->subkeys->pubkey_algo ) );
	text[COL_ALGO] = algo_buf;

	s = key->subkeys->keyid;
	if (strlen (s) == 16)
		s += 8; /* show only the short keyID */
	text[COL_KEYID] = s;

	s = key->uids->name;
	text[COL_NAME] = s;

	s = key->uids->email;
	text[COL_EMAIL] = s;

	switch (key->uids->validity)
	{
	case GPGME_VALIDITY_UNDEFINED:
		s="q";
		break;
	case GPGME_VALIDITY_NEVER:
		s="n";
		break;
	case GPGME_VALIDITY_MARGINAL:
		s="m";
		break;
	case GPGME_VALIDITY_FULL:
		s="f";
		break;
	case GPGME_VALIDITY_ULTIMATE:
		s="u";
		break;
	case GPGME_VALIDITY_UNKNOWN:
	default:
		s="?";
		break;
	}

	text[COL_VALIDITY] = s;

	gtk_list_store_append(clist_store, &newrow);
	gtk_list_store_set(clist_store, &newrow,
			COL_ALGO, text[COL_ALGO],
			COL_KEYID, text[COL_KEYID],
			COL_NAME, text[COL_NAME],
			COL_EMAIL, text[COL_EMAIL],
			COL_VALIDITY, text[COL_VALIDITY],
			COL_GPG_KEY, key,
			-1);

	g_free (algo_buf);
}


static void 
fill_clist (struct select_keys_s *sk, const char *pattern)
{
	GtkListStore *clist_store;
	gpgme_ctx_t ctx;
	gpgme_error_t err;
	gpgme_key_t key;
	int running=0;

	g_return_if_fail (sk);
	clist_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(sk->clist)));
	g_return_if_fail (clist_store);

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
		set_row (clist_store, key );
		key = NULL;
		update_progress (sk, ++running, pattern);
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
	sk->select_ctx = NULL;
	gpgme_release (ctx);
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


/* Sorting function for Name column */
static gint cmp_name (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	gpgme_key_t pa,pb;
	const char *sa, *sb;

	gtk_tree_model_get(model, a,
			COL_GPG_KEY, &pa,
			-1);
	gtk_tree_model_get(model, b,
			COL_GPG_KEY, &pb,
			-1);

	sa = pa?pa->uids->name:NULL;
	sb = pb?pb->uids->name:NULL;

	if(!sa)
		return !!sb;
	if(!sb)
		return -1;
	return strcasecmp(sa, sb);

}

/* Sorting function for email column */
static gint cmp_email (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	gpgme_key_t pa,pb;
	const char *sa, *sb;

	gtk_tree_model_get(model, a,
			COL_GPG_KEY, &pa,
			-1);
	gtk_tree_model_get(model, b,
			COL_GPG_KEY, &pb,
			-1);

	sa = pa?pa->uids->email:NULL;
	sb = pb?pb->uids->email:NULL;

	if(!sa)
		return !!sb;
	if(!sb)
		return -1;
	return strcasecmp(sa, sb);
}

static void 
create_dialog (struct select_keys_s *sk)
{
	GtkWidget *window;
	GtkWidget *vbox, *vbox2, *hbox;
	GtkWidget *bbox;
	GtkWidget *scrolledwin;
	GtkWidget *clist;
	GtkListStore *clist_store;
	GtkWidget *label;
	GtkWidget *vbox3;
	GtkWidget *select_btn, *cancel_btn, *other_btn, *do_crypt_btn, *do_sign_btn;
	const char *titles[N_COL_TITLES];
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	g_assert (!sk->window);
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request (window, 520, 280);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Select Keys"));
	g_signal_connect (window, "delete-event", G_CALLBACK (delete_event_cb), sk);
	g_signal_connect (window, "key-press-event", G_CALLBACK (key_pressed_cb), sk);

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

	titles[COL_ALGO]	= _("Size");
	titles[COL_KEYID]	= _("Key ID");
	titles[COL_NAME]	= _("Name");
	titles[COL_EMAIL]	= _("Address");
	titles[COL_VALIDITY]    = _("Val");

	clist_store = gtk_list_store_new(N_COL_TITLES,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);

	clist = gtk_tree_view_new_with_model( GTK_TREE_MODEL(clist_store) );
	gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(clist)), GTK_SELECTION_BROWSE);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(titles[COL_ALGO], renderer,
			"text", COL_ALGO, 
			NULL);
	g_object_set(column, "min-width", 72, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(titles[COL_KEYID], renderer,
			"text", COL_KEYID,
			NULL);
	g_object_set(column, "min-width", 76, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(titles[COL_NAME], renderer,
			"text", COL_NAME,
			NULL);
	g_object_set(column, "min-width", 130, NULL);
	gtk_tree_view_column_set_sort_column_id(column, COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(titles[COL_EMAIL], renderer,
			"text", COL_EMAIL,
			NULL);
	g_object_set(column, "min-width", 130, NULL);
	gtk_tree_view_column_set_sort_column_id(column, COL_EMAIL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(titles[COL_VALIDITY], renderer,
			"text", COL_VALIDITY,
			NULL);
	g_object_set(column, "min-width", 20, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(clist), column);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(clist_store),
			COL_NAME, cmp_name, sk, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(clist_store),
			COL_EMAIL, cmp_email, sk, NULL);

	g_signal_connect(clist, "destroy", G_CALLBACK(destroy_keys), NULL);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	bbox = gtk_hbox_new (FALSE, 2);
	select_btn = gtkut_create_label_button (_("Select"), G_CALLBACK (select_btn_cb), sk);
	cancel_btn = gtkut_create_label_button (_("Cancel"), G_CALLBACK (cancel_btn_cb), sk);
	other_btn = gtkut_create_label_button (_("Other..."), G_CALLBACK (other_btn_cb), sk);
	vbox3 = gtk_vbox_new(FALSE,2);
	do_crypt_btn = gtkut_check_button( vbox3, _("Enable encryption"), sk->do_crypt,
		(GCallback) crypt_changed_cb, sk );
	do_sign_btn = gtkut_check_button( vbox3, _("Enable signing"), sk->do_sign,
		(GCallback) sign_changed_cb, sk );

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
	sk->clist  = GTK_TREE_VIEW (clist);
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

	gpgme_key_t key;
	GtkTreeIter selected;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	g_return_if_fail (sk);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sk->clist));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sk->clist));

	if(!gtk_tree_selection_get_selected(selection, NULL, &selected))
		return;

	gtk_tree_model_get(model, &selected,
			COL_GPG_KEY, &key,
			-1);

	if (key) {
		const char *s = key->subkeys->fpr;
		if ( key->uids->validity < GPGME_VALIDITY_FULL ) {

		}
		
		sk->kset = g_realloc( sk->kset, sizeof(gpgme_key_t) * (sk->num_keys + 1) );
		gpgme_key_ref(key);
		sk->kset[sk->num_keys] = key;
		sk->num_keys++;

		sk->okay = 1;
		sk->key = strdup(s);
		gtk_main_quit ();

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
other_selected_cb(const char *text, void *data)
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
	do_text_input_window( _("Enter another user or key ID:"), _(""), other_selected_cb, sk);
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


static gboolean 
passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		passphrase_cancel_cb(NULL, NULL);
		return TRUE;
	}
	return FALSE;
}

static GtkWidget *
create_description (const gchar *desc, int prev_was_bad)
{
	const gchar *uid = NULL;
	gchar *buf;
	GtkWidget *label;

	uid = desc;

	if (!uid)
		uid = _("[no user id]");

	buf = g_strdup_printf (_("%sPlease enter the passphrase for:\n\n"
				 "  %.*s  \n"),
			       prev_was_bad ? _("Bad passphrase! Try again...\n\n") : "",
			       (int)strlen (uid), uid);

	label = gtk_label_new (buf);
	g_free (buf);

	return label;
}

const char*
passphrase_mbox (const char *desc, int prev_was_bad)
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

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Passphrase"));
	gtk_widget_set_size_request(window, 450, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	g_signal_connect(window, "delete-event", G_CALLBACK(passphrase_deleted), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(passphrase_key_pressed), NULL);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	if (desc) {
		GtkWidget *label;
		label = create_description (desc, prev_was_bad);
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

	ok_button = gtk_button_new_with_label (_("OK"));
	gtk_box_pack_start (GTK_BOX(confirm_box), ok_button, TRUE, TRUE, 0);

	cancel_button = gtk_button_new_with_label (_("Cancel"));
	gtk_box_pack_start(GTK_BOX(confirm_box), cancel_button, TRUE, TRUE, 0);

	gtk_box_pack_end(GTK_BOX(vbox), confirm_box, FALSE, FALSE, 0);

	g_signal_connect(ok_button, "clicked", G_CALLBACK(passphrase_ok_cb), NULL);
	g_signal_connect(pass_entry, "activate", G_CALLBACK(passphrase_ok_cb), NULL);
	g_signal_connect(cancel_button, "clicked", G_CALLBACK(passphrase_cancel_cb), NULL);

	gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
	
	gtk_widget_show_all(window);

	gtk_main();

	if (aycrypt_pass_ack) {
		the_passphrase = gtk_editable_get_chars(GTK_EDITABLE(pass_entry), 0, -1);
	}
	gtk_widget_destroy (window);

	return the_passphrase;
}

