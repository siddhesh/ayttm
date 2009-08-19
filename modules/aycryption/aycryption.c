/*
 * Aycryption (GPG support) module for Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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
#ifdef __MINGW32__
#define __IN_PLUGIN__
#endif

#include "intl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <gpgme.h>

#include "externs.h"
#include "plugin_api.h"
#include "prefs.h"
#include "util.h"
#include "messages.h"
#include "debug.h"
#include "platform_defs.h"
#include "select-keys.h"
#include <gtk/gtk.h>
#include "gtk/html_text_buffer.h"
#include <errno.h>

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#ifndef USE_POSIX_DLOPEN
	#define plugin_info aycryption_LTX_plugin_info
	#define module_version aycryption_LTX_module_version
#endif


/* Function Prototypes */
static char *aycryption_out(const eb_local_account * local, const eb_account * remote,
			    struct contact *contact, const char * s);
static char *aycryption_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *contact, const char * s);
static void set_gpg_key(ebmCallbackData *data);
static void show_gpg_log(ebmCallbackData *data);
void pgp_encrypt ( gpgme_data_t plain, gpgme_data_t *cipher, gpgme_key_t *kset, int sign );

gpgme_error_t gpgmegtk_passphrase_cb (void *opaque, 
				      const char *desc, 
				      const char *passphrase_info, 
				      int prev_was_bad, 
				      int fd);

static int aycryption_init();
static int aycryption_finish();

struct passphrase_cb_info_s {
    gpgme_ctx_t c;
    int did_it;
};

static int store_passphrase = 0;
static char mykey[MAX_PREF_LEN] = "";

static int ref_count = 0;
static void *tag1=NULL;
static void *tag2=NULL;
static void *tag3=NULL;
static void *tag4=NULL;

static GtkWidget *gpg_log_window = NULL;
static GtkWidget *gpg_log_text = NULL;
static GtkWidget *gpg_log_swindow = NULL;
static int do_aycryption_debug = 0;
#define DBG_CRYPT do_aycryption_debug

/*  Module Exports */
PLUGIN_INFO plugin_info = {
	PLUGIN_FILTER,
	"Aycryption",
	"Encrypts messages with GPG.\n"
	"WARNING: Apparently MSN servers randomly truncates GPG signed/encrypted messages.",
	"$Revision: 1.25 $",
	"$Date: 2009/08/19 04:07:04 $",
	&ref_count,
	aycryption_init,
	aycryption_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int aycryption_init()
{
	input_list *il = g_new0(input_list, 1);
	plugin_info.prefs = il;

	il->widget.checkbox.value = &store_passphrase;
	il->name = "store_passphrase";
	il->label = strdup(_("Store passphrase in memory"));
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
       	il = il->next;
	il->widget.entry.value = mykey;
	il->name = "mykey";
	il->label = strdup(_("Private key for signing:"));
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
       	il = il->next;
	il->widget.checkbox.value = &do_aycryption_debug;
	il->name = "do_aycryption_debug";
	il->label = strdup(_("Enable debugging"));
	il->type = EB_INPUT_CHECKBOX;

	outgoing_message_filters = l_list_append(outgoing_message_filters, &aycryption_out);
	incoming_message_filters = l_list_append(incoming_message_filters, &aycryption_in);
	
	gpg_log_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gpg_log_text = gtk_text_view_new();
	gpg_log_swindow = gtk_scrolled_window_new(NULL,NULL);
	
	gtk_window_set_title(GTK_WINDOW(gpg_log_window), _("Aycryption - status"));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gpg_log_swindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	html_text_view_init(GTK_TEXT_VIEW(gpg_log_text), HTML_IGNORE_FONT);
	gtk_widget_set_size_request(gpg_log_text, 450, 150);
	
	gtk_container_add(GTK_CONTAINER(gpg_log_swindow), gpg_log_text);
	gtk_container_add(GTK_CONTAINER(gpg_log_window), gpg_log_swindow);
	gtk_widget_show(gpg_log_text);
	gtk_widget_show(gpg_log_swindow);
	
	g_signal_connect(gpg_log_window, "delete-event",
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	
	gtk_widget_realize(gpg_log_window);
	gtk_widget_realize(gpg_log_swindow);
	gtk_widget_realize(gpg_log_text);

	tag1=eb_add_menu_item(_("GPG settings..."), EB_CHAT_WINDOW_MENU, set_gpg_key, ebmCONTACTDATA, NULL);
	if(!tag1) {
		eb_debug(DBG_MOD, "Error!  Unable to add aycryption menu to chat window menu\n");
		return(-1);
	}
	tag2=eb_add_menu_item(_("GPG settings..."), EB_CONTACT_MENU, set_gpg_key, ebmCONTACTDATA, NULL);
	if(!tag2) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
		eb_debug(DBG_MOD, "Error!  Unable to add aycryption menu to contact menu\n");
		return(-1);
	}
	tag3=eb_add_menu_item(_("GPG status..."), EB_CHAT_WINDOW_MENU, show_gpg_log, ebmCONTACTDATA, NULL);
	if(!tag3) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag2);
		eb_debug(DBG_MOD, "Error!  Unable to add aycryption menu to chat window menu\n");
		return(-1);
	}
	tag4=eb_add_menu_item(_("GPG status..."), EB_CONTACT_MENU, show_gpg_log, ebmCONTACTDATA, NULL);
	if(!tag4) {
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag2);
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag3);
		eb_debug(DBG_MOD, "Error!  Unable to add aycryption menu to contact menu\n");
		return(-1);
	}

	return 0;
}

static int aycryption_finish()
{
	outgoing_message_filters = l_list_remove(outgoing_message_filters, &aycryption_out);
	incoming_message_filters = l_list_remove(incoming_message_filters, &aycryption_in);
	
	while(plugin_info.prefs) {
		input_list *il = plugin_info.prefs->next;
		free(plugin_info.prefs);
		plugin_info.prefs = il;
	}

	if (tag1)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag1);
	if (tag2)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag2);
	if (tag3)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag3);
	if (tag4)
		eb_remove_menu_item(EB_CHAT_WINDOW_MENU, tag4);
	
	gtk_widget_destroy(gpg_log_window);
	return 0;
}

/*******************************************************************************
 *                             End Module Code
 ******************************************************************************/

/* removes the <br/> crap that kopete adds */
static void br_to_nl(char * text)
{
	int i, j;
	int visible = 1;
	for (i=0, j=0; text[i]; i++)
	{
		if(text[i] == '<')
		{
			if (!strncasecmp(text+i+1, "br/", 3)) {
				/* Kopete compat */
				visible = 0;
				text[j++] = '\n';
			} else if (!strncasecmp(text+i+1, "br", 2)) {
				/* Fire compat */
				visible = 0;
				text[j++] = '\n';
			}
		}
		else if (text[i] == '>')
		{
			if(!visible) {
				visible = 1;
				continue;
			}
		}
		if (visible)
			text[j++] = text[i];
	}
	text[j] = '\0';
}

static gpgme_error_t mygpgme_data_rewind(gpgme_data_t dh)
{
	return (gpgme_data_seek (dh, 0, SEEK_SET) == -1)
		? gpgme_error_from_errno (errno) : 0;
}

static void show_gpg_log(ebmCallbackData *data)
{
	gtk_widget_show(gpg_log_window);
	gtk_widget_show(gpg_log_swindow);
	gtk_widget_show(gpg_log_text);
	gdk_window_raise(gpg_log_window->window);
}

static void set_gpg_key(ebmCallbackData *data)
{
	ebmContactData *ecd=NULL;
	struct contact *ct = NULL;
	struct select_keys_s keys;
	GSList *recp_names = NULL;
	if(IS_ebmContactData(data))
		ecd=(ebmContactData *)data;
	
	if (ecd)
		ct = find_contact_by_nick(ecd->contact);
	
	if (!ct) {
		eb_debug(DBG_CRYPT, "contact is null !\n");
		return;
	}
	recp_names = g_slist_append(recp_names, strdup(ct->nick));
	if (ct->gpg_key && ct->gpg_key[0]);
		recp_names = g_slist_append(recp_names, strdup(ct->gpg_key));
	keys = gpgmegtk_recipient_selection(recp_names, ct->gpg_do_encryption, ct->gpg_do_signature);
	if (keys.kset && keys.key) {
		eb_debug(DBG_CRYPT,"got key %s\n", keys.key);
		strncpy(ct->gpg_key, keys.key, 48);

		ct->gpg_do_encryption = keys.do_crypt;
		ct->gpg_do_signature = keys.do_sign;
	} else {
		eb_debug(DBG_CRYPT, "no key\n");
		strncpy(ct->gpg_key, "", 48);

		ct->gpg_do_encryption = 0;
		ct->gpg_do_signature = keys.do_sign;
	}
	write_contact_list();
	
}
static char *logcolor[3] = {"#ffa8a8", "#a8a8a8", "#a8ffa8"};
typedef enum {
	LOG_ERR=0,
	LOG_NORM,
	LOG_OK
} LogLevel;
	
static void log_action(const struct contact *ct, int loglevel, const char *string)
{
	
	char buf[1024];
	snprintf(buf, 1024, _("<font color=%s><b>%s</b>: %s</font><br>"),logcolor[loglevel], ct->nick, string);
	html_text_buffer_append(GTK_TEXT_VIEW(gpg_log_text), buf, HTML_IGNORE_NONE);
	if (loglevel == LOG_ERR) {
		show_gpg_log(NULL);
	}
}

void gpg_get_kset(struct contact *ct, gpgme_key_t **kset)
{
	int num_keys = 0;
	gpgme_ctx_t ctx;
	gpgme_error_t err;

	err = gpgme_new(&ctx);
	g_assert(!err);

	err = gpgme_op_keylist_start (ctx, ct->gpg_key, 0);
	if (err) {
		eb_debug(DBG_CRYPT, "err: %s\n", gpgme_strerror(err));
		*kset = NULL;
		return;
	}

	*kset = g_malloc(sizeof(gpgme_key_t));

	while ( !(err = gpgme_op_keylist_next ( ctx, &(*kset)[num_keys] )) ) {
		eb_debug(DBG_CRYPT,"found a key for %s with name %s\n", 
			 ct->gpg_key, 
			 (*kset)[num_keys]->uids->name);

		*kset = g_realloc( *kset, sizeof(gpgme_key_t) * (num_keys + 1) );
		num_keys++;
	}

	gpgme_release(ctx);
}


static char *aycryption_out(const eb_local_account * local, const eb_account * remote,
			    struct contact *ct, const char * s)
{
	char *p = NULL;
	char buf[4096];

	gpgme_data_t plain = NULL;
	gpgme_data_t cipher = NULL;
	gpgme_error_t error;
	gpgme_key_t *kset = NULL;
	int err;

	if ((!ct->gpg_do_encryption || !ct->gpg_key || ct->gpg_key == '\0')
	    && !ct->gpg_do_signature) {
		if (ct->gpg_do_encryption)
			log_action(ct, LOG_ERR, "Could not encrypt message.");
		return strdup(s);
	}

	if( ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0] )
		gpg_get_kset(ct, &kset);


	if ( ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0] && !kset ) {
		eb_debug(DBG_CRYPT,"can't init outgoing crypt: %d %p %c\n",
				ct->gpg_do_encryption, ct->gpg_key, ct->gpg_key[0]);
		log_action(ct, LOG_ERR, "Could not encrypt message - you may have to set your contact's key.");
		return strdup(s);
	}
		
	error = gpgme_data_new(&plain);
	err = gpgme_data_write(plain, s, strlen(s));
	
	/* encrypt only */
	if (ct->gpg_do_encryption && kset && !ct->gpg_do_signature) {
		pgp_encrypt (plain, &cipher, kset, FALSE);
		gpgme_data_release (plain); plain = NULL;
		log_action(ct, LOG_NORM, "Sent encrypted, unsigned message.");
	/* sign only */
	} else if (!(ct->gpg_do_encryption && kset) && ct->gpg_do_signature) {
		pgp_encrypt(plain, &cipher, NULL, TRUE);
		gpgme_data_release (plain); plain = NULL;
		log_action(ct, LOG_NORM, "Sent uncrypted, signed message.");		
	/* encrypt and sign */
	} else if (ct->gpg_do_encryption && kset && ct->gpg_do_signature) {
		pgp_encrypt (plain, &cipher, kset, TRUE);
		gpgme_data_release (plain); plain = NULL;
		log_action(ct, LOG_NORM, "Sent encrypted, signed message.");
	}
	err = mygpgme_data_rewind (cipher);
	if (err)
		eb_debug(DBG_CRYPT,"error: %s\n",
			 gpgme_strerror(err));
	
	memset(buf, 0, sizeof(buf));

	while ( gpgme_data_read (cipher, buf, 4096) >0 ) {
		char tmp[4096];
		
		snprintf(tmp, sizeof(tmp), "%s%s",(p!=NULL)?p:"", buf);
		if (p)
			free(p);
		p = strdup(tmp);
		memset(buf, 0, sizeof(buf));
	}

	if(cipher)
		gpgme_data_release(cipher);	

	return p;
}

static char *aycryption_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *ct, const char * s)
{
	char *p = NULL, *res = NULL, *s_nohtml = NULL;
	gpgme_data_t plain = NULL, cipher = NULL;
	gpgme_key_t key = NULL;
	int err;
	char buf[4096];

	gpgme_ctx_t ctx = NULL;
	gpgme_data_t sigstat = NULL;
	char s_sigstat[1024];
	int was_crypted = 1;
	int curloglevel = 0;
	gpgme_verify_result_t verify_result;

	int sig_code = 0;

	memset(buf, 0, 4096);
	if (strncmp (s, "-----BEGIN PGP ", strlen("-----BEGIN PGP "))) {
		eb_debug(DBG_CRYPT, "Incoming message isn't PGP formatted\n");
		return strdup(s);
	}

	err = gpgme_new (&ctx);

	if (err) {
            eb_debug(DBG_CRYPT,"gpgme_new failed: %s\n", gpgme_strerror (err));
	    log_action(ct, LOG_ERR, "Memory error.");
            return strdup(s);
	}
	gpgme_data_new(&plain);
	gpgme_data_new(&cipher);
       
	/* Clean out kopete HTML crap
	 * < vdanen> so like KDE to just bloat stuff for the hell of it
	 */
	s_nohtml = strdup(s);
	if (!s_nohtml)
	{
		eb_debug(DBG_CRYPT,"Couldn't copy message to strip html");
		log_action(ct, LOG_ERR, "Memory error while stripping html.");
		return strdup(s);
	}
	br_to_nl(s_nohtml);
	eb_debug(DBG_CRYPT,"html stripped: %s\n",s_nohtml);

	err = gpgme_data_write(cipher, s_nohtml, strlen(s_nohtml));

	if(err == -1)
		perror("cipher write error");

	free(s_nohtml);

	mygpgme_data_rewind(cipher);
	mygpgme_data_rewind(plain);
	
	if (!getenv("GPG_AGENT_INFO")) {
            gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, NULL);
	} 
	err = gpgme_op_decrypt_verify (ctx, cipher, plain);

	if (err && gpg_err_code(err) != GPG_ERR_NO_DATA) {
		log_action(ct, LOG_ERR, "Cannot decrypt message - maybe your contact uses an incorrect key.");
		return strdup(s);
	} else if (gpg_err_code(err) == GPG_ERR_NO_DATA) { /*plaintext signed*/
		was_crypted = 0;
		mygpgme_data_rewind(cipher);
		mygpgme_data_rewind(plain);
		err = gpgme_op_verify(ctx, cipher, sigstat, plain);
		if (err)
			eb_debug(DBG_CRYPT, "plaintext err: %s\n", gpgme_strerror(err));
	}
	
	verify_result = gpgme_op_verify_result(ctx);
	
	if(verify_result && verify_result->signatures) {
		err = gpgme_get_key(ctx, verify_result->signatures->fpr, &key, 0);
		if (err) {
			eb_debug(DBG_CRYPT, "getkey err %s\n", gpgme_strerror(err));
			key = NULL;
		}
	} 
	else
		key = NULL;

	err = mygpgme_data_rewind(plain);
	if (err)
		eb_debug(DBG_CRYPT, "rewind err %d\n", err);

	
	memset(buf, 0, sizeof(buf));
	while ( gpgme_data_read (plain, buf, 4096) > 0 ) {
		char tmp[4096];
		memset(tmp, 0, 4096);

		snprintf(tmp, sizeof(tmp), "%s%s",(p!=NULL)?p:"", buf);
		if (p)
			free(p);
		p = strdup(tmp);
		memset(buf, 0, sizeof(buf));
	}
		
	if (p) {
		while (p[strlen(p)-1] == '\n' || p[strlen(p)-1] == '\r')
			p[strlen(p)-1] = '\0';	
	}

	gpgme_release(ctx);

	if( verify_result && verify_result->signatures ) {

		sig_code = gpg_err_code(verify_result->signatures->status);
		
		switch( sig_code ) {
		case GPG_ERR_NO_DATA:
			curloglevel = LOG_NORM;
			break;
		case GPG_ERR_NO_ERROR:
			curloglevel = LOG_OK;
			break;
		default:
			curloglevel = LOG_ERR;
			break;
		}
		
		strcpy(s_sigstat, _("Got an "));
		strcat(s_sigstat, was_crypted?_("encrypted"):_("unencrypted"));
		
		switch( sig_code ) {
		case GPG_ERR_NO_DATA:
			strcat(s_sigstat, _(", unsigned message."));
			break;
		case GPG_ERR_NO_ERROR:
			strcat(s_sigstat, _(", correctly signed (by "));
			strcat(s_sigstat, key->uids->email);
			strcat(s_sigstat, ") message.");
			break;
		case GPG_ERR_SIG_EXPIRED:
			strcat(s_sigstat, _(", badly signed (by "));
			strcat(s_sigstat, key->uids->email);
			strcat(s_sigstat, ") message.");
			break;
		case GPG_ERR_NO_PUBKEY:
			strcat(s_sigstat, _(" message with no key."));
			break;
		case GPG_ERR_CERT_REVOKED:
			strcat(s_sigstat, _(" message ; signature is valid but  the signing key has been revoked"));
			break;
		case GPG_ERR_BAD_SIGNATURE:
			strcat(s_sigstat, _(" correctly signed (by "));
			strcat(s_sigstat, key->uids->email);
			strcat(s_sigstat, _(") message, but signature has expired."));
			break;
		case GPG_ERR_KEY_EXPIRED:
			strcat(s_sigstat, _(" correctly signed (by "));
			strcat(s_sigstat, key->uids->email);
			strcat(s_sigstat, _(") message, but key has expired."));
			break;
		case GPG_ERR_GENERAL:
			strcat(s_sigstat, _(") message, but there was an error verifying the signature"));
			break;
		default:
			strcat(s_sigstat, _(" message - Unknown signature status (file a bugreport)!"));
			break;
		}

		if (curloglevel == LOG_ERR) {
			res = strdup(s);
			free(p);
			p = res;
		}
		log_action(ct, curloglevel, s_sigstat);
	}

	return p;
}

static GSList *create_signers_list (const char *keyid)
{
	GSList *key_list = NULL;
	gpgme_ctx_t list_ctx = NULL;
	GSList *p;
	gpgme_error_t err;
	gpgme_key_t key;

	err = gpgme_new (&list_ctx);
	if ( gpg_err_code(err) != GPG_ERR_NO_ERROR ) {
		goto leave;
	}
	err = gpgme_op_keylist_start (list_ctx, keyid, 1);
	if ( gpg_err_code(err) != GPG_ERR_NO_ERROR ) {
		goto leave;
	}

	err = gpgme_op_keylist_next(list_ctx, &key);
	while ( gpg_err_code(err) != GPG_ERR_NO_ERROR ) {
		key_list = g_slist_append (key_list, key);
		err = gpgme_op_keylist_next(list_ctx, &key);
	}
	if (gpg_err_code(err) != GPG_ERR_EOF) {
		goto leave;
	}
	err = 0;
	if (key_list == NULL) {
		eb_debug (DBG_CRYPT,"no keys found for keyid \"%s\"\n", keyid);
	}

leave:
	if (gpg_err_code(err) != GPG_ERR_NO_ERROR) {
		eb_debug (DBG_CRYPT,"create_signers_list failed: %s\n", gpgme_strerror (err));
		for (p = key_list; p != NULL; p = p->next)
			gpgme_key_unref ((gpgme_key_t) p->data);
		g_slist_free (key_list);
	}
	if (list_ctx)
		gpgme_release (list_ctx);
	return err ? NULL : key_list;
}

/*
 * plain contains an entire mime object.
 * Encrypt it and return an gpgme_data_t object with the encrypted version of
 * the file or NULL in case of error.
 */
void
pgp_encrypt ( gpgme_data_t plain, gpgme_data_t *cipher, gpgme_key_t *kset, int sign )
{
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err ;
	GSList *p;
	GSList *key_list = NULL;


	if(sign && mykey[0]) {
		key_list = create_signers_list(mykey);
	}
	
	err = gpgme_new (&ctx);
	if (gpg_err_code(err) == GPG_ERR_NO_ERROR)
		err = gpgme_data_new (cipher);

	if (gpg_err_code(err) == GPG_ERR_NO_ERROR && sign) {
		
		if (!getenv("GPG_AGENT_INFO")) {
			gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, NULL);
		}
		if (kset != NULL) {
			gpgme_set_textmode (ctx, 1);
			gpgme_set_armor (ctx, 1);
		}
		gpgme_signers_clear (ctx);
		for (p = key_list; p != NULL; p = p->next) {
			err = gpgme_signers_add (ctx, (gpgme_key_t) p->data);
		}
		if (kset != NULL) {
			mygpgme_data_rewind(plain);
			err = gpgme_op_encrypt_sign(ctx, kset, 0, plain, *cipher);
		} else {
			mygpgme_data_rewind(plain);
			err = gpgme_op_sign (ctx, plain, *cipher, GPGME_SIG_MODE_CLEAR);
		}
		for (p = key_list; p != NULL; p = p->next)
			gpgme_key_unref ((gpgme_key_t) p->data);
		g_slist_free (key_list);
	}
	else if (gpg_err_code(err) == GPG_ERR_NO_ERROR) {
		gpgme_set_armor (ctx, 1);
		mygpgme_data_rewind(plain);
		err = gpgme_op_encrypt (ctx, kset, 0, plain, *cipher);
	}
	
	if (gpg_err_code(err) != GPG_ERR_NO_ERROR) {
		eb_debug(DBG_CRYPT, "pgp_encrypt failed: %s\n", gpgme_strerror(err));
		gpgme_data_release (*cipher);
		*cipher = NULL;
	}

	gpgme_release (ctx);
}

gpgme_error_t
gpgmegtk_passphrase_cb (void *opaque, const char *desc, const char *passphrase_info, int prev_was_bad, int fd)
{
	const char *pass;
	
	if (store_passphrase && aycrypt_last_pass != NULL && !prev_was_bad) {
		write(fd, aycrypt_last_pass, strlen(aycrypt_last_pass));
		write(fd, "\n", 1);
		return GPG_ERR_NO_ERROR;
	}

	pass = passphrase_mbox (desc, prev_was_bad);
	if (!pass) {
		eb_debug(DBG_CRYPT, "Cancelled passphrase entry\n");
		write(fd, "\n", 1);
		return GPG_ERR_CANCELED;
	}
	else {
		if (store_passphrase) {
			if (aycrypt_last_pass)
				g_free(aycrypt_last_pass);
			aycrypt_last_pass = g_strdup(pass);
		}
	}
	write (fd, pass, strlen(pass));
	write (fd, "\n", 1);
	return GPG_ERR_NO_ERROR;
}

