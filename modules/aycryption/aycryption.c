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
#include "tcp_util.h"
#include "messages.h"
#include "debug.h"
#include "platform_defs.h"
#include "select-keys.h"
#include <gtk/gtk.h>
#include "gtk/extgtktext.h"
#include "gtk/gtk_eb_html.h"

/*******************************************************************************
 *                             Begin Module Code
 ******************************************************************************/
/*  Module defines */
#define plugin_info aycryption_LTX_plugin_info
#define module_version aycryption_LTX_module_version


/* Function Prototypes */
static char *aycryption_out(const eb_local_account * local, const eb_account * remote,
			    const struct contact *contact, const char * s);
static char *aycryption_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *contact, const char * s);
static void set_gpg_key(ebmCallbackData *data);
static void show_gpg_log(ebmCallbackData *data);
static GpgmeData pgp_encrypt ( GpgmeData plain, GpgmeRecipients rset, int sign );

const char *gpgmegtk_passphrase_cb (void *opaque, const char *desc, void **r_hd);
static int aycryption_init();
static int aycryption_finish();

struct passphrase_cb_info_s {
    GpgmeCtx c;
    int did_it;
};

static int store_passphrase = 0;
static char mykey[MAX_PREF_LEN] = "";

static int ref_count = 0;
static int gpgme_inited = 0;
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
	"$Revision: 1.16 $",
	"$Date: 2003/06/04 10:27:25 $",
	&ref_count,
	aycryption_init,
	aycryption_finish,
	NULL
};
/* End Module Exports */

unsigned int module_version() {return CORE_VERSION;}

static int aycryption_init()
{
	input_list *il = calloc(1, sizeof(input_list));
	plugin_info.prefs = il;

	il->widget.checkbox.value = &store_passphrase;
	il->widget.checkbox.name = "store_passphrase";
	il->widget.checkbox.label = strdup(_("Store passphrase in memory"));
	il->type = EB_INPUT_CHECKBOX;

	il->next = g_new0(input_list, 1);
       	il = il->next;
	il->widget.entry.value = mykey;
	il->widget.entry.name = "mykey";
	il->widget.entry.label = strdup(_("Private key for signing:"));
	il->type = EB_INPUT_ENTRY;

	il->next = g_new0(input_list, 1);
       	il = il->next;
	il->widget.checkbox.value = &do_aycryption_debug;
	il->widget.checkbox.name = "do_aycryption_debug";
	il->widget.checkbox.label = strdup(_("Enable debugging"));
	il->type = EB_INPUT_CHECKBOX;

	outgoing_message_filters = l_list_append(outgoing_message_filters, &aycryption_out);
	incoming_message_filters = l_list_append(incoming_message_filters, &aycryption_in);
	
	gpg_log_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gpg_log_text = ext_gtk_text_new(NULL,NULL);
	gpg_log_swindow = gtk_scrolled_window_new(NULL,NULL);
	
	gtk_window_set_title(GTK_WINDOW(gpg_log_window), _("Aycryption - status"));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gpg_log_swindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	gtk_eb_html_init(EXT_GTK_TEXT(gpg_log_text));
	gtk_widget_set_usize(gpg_log_text, 450, 150);
	
	gtk_container_add(GTK_CONTAINER(gpg_log_swindow), gpg_log_text);
	gtk_container_add(GTK_CONTAINER(gpg_log_window), gpg_log_swindow);
	gtk_widget_show(gpg_log_text);
	gtk_widget_show(gpg_log_swindow);
	
	gtk_signal_connect(GTK_OBJECT(gpg_log_window), "delete-event", 
			GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	
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


	if(!gpgme_inited) {
		const char *version = gpgme_check_version(NULL);
		if(strncmp("0.3",version , 3)) {
			ay_do_error(_("aycryption error"), _("You need gpgme version 0.3 for aycryption to work."));
			return -1;
		}
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
			if (!strncasecmp(text+i+1, "br/", 3))
			{
				visible = 0;
				text[j++] = '\n';
			}
		}
		else if (text[i] == '>')
		{
			if(!visible)
				visible = 1;
			continue;
		}
		if (visible)
			text[j++] = text[i];
	}
	text[j] = '\0';
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
	if (keys.rset && keys.key) {
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
	gtk_eb_html_add(EXT_GTK_TEXT(gpg_log_text), buf, 0, 0, 0);
	if (loglevel == LOG_ERR) {
		show_gpg_log(NULL);
	}
}

static char *aycryption_out(const eb_local_account * local, const eb_account * remote,
			    const struct contact *ct, const char * s)
{
	char *p = NULL;
	char buf[4096];
	int nread;

	GpgmeRecipients rset;
	if ((!ct->gpg_do_encryption || !ct->gpg_key || ct->gpg_key[0]=='\0')
	&& !ct->gpg_do_signature) {
		if (ct->gpg_do_encryption)
			log_action(ct, LOG_ERR, "Could not encrypt message.");
		return strdup(s);
	}
        gpgme_recipients_new (&rset);
	if ((ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0])
	&& gpgme_recipients_add_name_with_validity( rset, ct->gpg_key, 
		GPGME_VALIDITY_FULL) ) {
		eb_debug(DBG_CRYPT,"can't init outgoing crypt: %d %p %c\n",
				ct->gpg_do_encryption, ct->gpg_key, ct->gpg_key[0]);
		log_action(ct, LOG_ERR, "Could not encrypt message - you may have to set your contact's key.");
		return strdup(s);
	} else {
		GpgmeData plain = NULL;
		GpgmeData cipher = NULL;
		int err;
		gpgme_data_new(&plain);
		gpgme_data_write(plain, s, strlen(s));
		
		/* encrypt only */
		if (ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0]
		&& !ct->gpg_do_signature) {
			cipher = pgp_encrypt (plain, rset, FALSE);
			gpgme_data_release (plain); plain = NULL;
  			gpgme_recipients_release (rset); rset = NULL;
			log_action(ct, LOG_NORM, "Sent encrypted, unsigned message.");
		/* sign only */
		} else if (!(ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0])
		&& ct->gpg_do_signature) {
			cipher = pgp_encrypt(plain, NULL, TRUE);
			gpgme_data_release (plain); plain = NULL;
			log_action(ct, LOG_NORM, "Sent uncrypted, signed message.");		
		/* encrypt and sign */
		} else if (ct->gpg_do_encryption && ct->gpg_key && ct->gpg_key[0]
		&& ct->gpg_do_signature) {
			cipher = pgp_encrypt (plain, rset, TRUE);
			gpgme_data_release (plain); plain = NULL;
  			gpgme_recipients_release (rset); rset = NULL;
			log_action(ct, LOG_NORM, "Sent encrypted, signed message.");
		}
		err = gpgme_data_rewind (cipher);
		if (err)
			eb_debug(DBG_CRYPT,"error: %s\n",
				gpgme_strerror(err));

		memset(buf, 0, sizeof(buf));
		while (!gpgme_data_read (cipher, buf, 4096, &nread)) {
			char tmp[4096];
			if (nread) {
				snprintf(tmp, sizeof(tmp), "%s%s",(p!=NULL)?p:"", buf);
				if (p)
					free(p);
				p = strdup(tmp);
				memset(buf, 0, sizeof(buf));
			}
		}
		
	}
	return p;
}

static char *aycryption_in(const eb_local_account * local, const eb_account * remote,
			   const struct contact *ct, const char * s)
{
	char *p = NULL, *res = NULL, *s_nohtml = NULL;
	GpgmeData plain = NULL, cipher = NULL;
	GpgmeKey key = NULL;
        struct passphrase_cb_info_s info;
	int err;
	char buf[4096];
	int nread;
	GpgmeCtx ctx = NULL;
	GpgmeSigStat sigstat = 0;
	char s_sigstat[1024];
	int was_crypted = 1;
	int curloglevel = 0;
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
		log_action(ct, LOG_ERR, "Memory eror while stripping html.");
		return strdup(s);
	}
	br_to_nl(s_nohtml);
	eb_debug(DBG_CRYPT,"html stripped: %s\n",s_nohtml);

	gpgme_data_write(cipher, s_nohtml, strlen(s_nohtml));

	free(s_nohtml);
	
	if (!getenv("GPG_AGENT_INFO")) {
            info.c = ctx;
            gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	} 
	err = gpgme_op_decrypt_verify (ctx, cipher, plain, &sigstat);

	if (err && err != GPGME_No_Data) {
		log_action(ct, LOG_ERR, "Cannot decrypt message - maybe your contact uses an incorrect key.");
		return strdup(s);
	} else if (err == GPGME_No_Data) { /*plaintext signed*/
		was_crypted = 0;
		gpgme_data_rewind(cipher);
		gpgme_data_new(&plain);
		err = gpgme_op_verify(ctx, cipher, plain, &sigstat);
		if (err)
			eb_debug(DBG_CRYPT, "plaintext err: %d\n", err);
	}
	
	err = gpgme_get_sig_key(ctx, 0, &key);
	if (err) {
		eb_debug(DBG_CRYPT, "getkey err %d\n", err);
		key = NULL;
	}
	err = gpgme_data_rewind(plain);
	if (err)
		eb_debug(DBG_CRYPT, "rewind err %d\n", err);
	
	memset(buf, 0, sizeof(buf));
	while (!(err = gpgme_data_read (plain, buf, sizeof(buf), &nread))) {
		char tmp[4096];
		memset(tmp, 0, 4096);
		if (nread) {
			snprintf(tmp, sizeof(tmp), "%s%s",(p!=NULL)?p:"", buf);
			if (p)
				free(p);
			p = strdup(tmp);
			memset(buf, 0, sizeof(buf));
		}            
	}
		
	if (p) {
		while (p[strlen(p)-1] == '\n' || p[strlen(p)-1] == '\r')
			p[strlen(p)-1] = '\0';	
	}
	
	gpgme_release(ctx);
	
	switch(sigstat) {
		case GPGME_SIG_STAT_NONE:
			curloglevel = LOG_NORM;
			break;
		case GPGME_SIG_STAT_GOOD:
			curloglevel = LOG_OK;
			break;
		default:
			curloglevel = LOG_ERR;
	}
	strcpy(s_sigstat, _("Got an "));
	strcat(s_sigstat, was_crypted?_("encrypted"):_("uncrypted"));
	switch(sigstat) {
		case GPGME_SIG_STAT_NONE:
			strcat(s_sigstat, _(", unsigned message."));
			break;
		case GPGME_SIG_STAT_GOOD:
			strcat(s_sigstat, _(", correctly signed (by "));
			strcat(s_sigstat, gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0));
			strcat(s_sigstat, ") message.");
			break;
		case GPGME_SIG_STAT_BAD:
			strcat(s_sigstat, _(", badly signed (by "));
			strcat(s_sigstat, gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0));
			strcat(s_sigstat, ") message.");
			break;
		case GPGME_SIG_STAT_NOKEY:
			strcat(s_sigstat, _(" message with no key."));
			break;
		case GPGME_SIG_STAT_NOSIG:
			strcat(s_sigstat, _(" message with no signature."));
			break;
		case GPGME_SIG_STAT_ERROR:
			strcat(s_sigstat, _(" message ; error happened while checking signature."));
			break;
		case GPGME_SIG_STAT_DIFF:
			strcat(s_sigstat, _(" message with more than one signature (their status differ)."));
			break;
		case GPGME_SIG_STAT_GOOD_EXP:
			strcat(s_sigstat, _(" correctly signed (by "));
			strcat(s_sigstat, gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0));
			strcat(s_sigstat, _(") message, but signature has expired."));
			break;
		case GPGME_SIG_STAT_GOOD_EXPKEY:
			strcat(s_sigstat, _(" correctly signed (by "));
			strcat(s_sigstat, gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0));
			strcat(s_sigstat, _(") message, but key has expired."));
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
	return p;
}

static GSList *create_signers_list (const char *keyid)
{
	GSList *key_list = NULL;
	GpgmeCtx list_ctx = NULL;
	GSList *p;
	GpgmeError err;
	GpgmeKey key;

	err = gpgme_new (&list_ctx);
	if (err)
		goto leave;
	err = gpgme_op_keylist_start (list_ctx, keyid, 1);
	if (err)
		goto leave;
	while ( !(err = gpgme_op_keylist_next (list_ctx, &key)) ) {
		key_list = g_slist_append (key_list, key);
	}
	if (err != GPGME_EOF)
		goto leave;
	err = 0;
	if (key_list == NULL) {
		eb_debug (DBG_CRYPT,"no keys found for keyid \"%s\"\n", keyid);
	}

leave:
	if (err) {
		eb_debug (DBG_CRYPT,"create_signers_list failed: %s\n", gpgme_strerror (err));
		for (p = key_list; p != NULL; p = p->next)
			gpgme_key_unref ((GpgmeKey) p->data);
		g_slist_free (key_list);
	}
	if (list_ctx)
		gpgme_release (list_ctx);
	return err ? NULL : key_list;
}

/*
 * plain contains an entire mime object.
 * Encrypt it and return an GpgmeData object with the encrypted version of
 * the file or NULL in case of error.
 */
static GpgmeData
pgp_encrypt ( GpgmeData plain, GpgmeRecipients rset, int sign )
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL;
    GSList *p;
    struct passphrase_cb_info_s info;
    GSList *key_list = NULL;
    memset (&info, 0, sizeof info);

    if(sign && mykey && mykey[0]) {
     key_list = create_signers_list(mykey);
    }
    
    err = gpgme_new (&ctx);
    if (!err)
	err = gpgme_data_new (&cipher);
    if (!err && sign) {

	    if (!getenv("GPG_AGENT_INFO")) {
        	info.c = ctx;
        	gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	    }
	    if (rset != NULL) {
		    gpgme_set_textmode (ctx, 1);
		    gpgme_set_armor (ctx, 1);
	    }
	    gpgme_signers_clear (ctx);
	    for (p = key_list; p != NULL; p = p->next) {
		err = gpgme_signers_add (ctx, (GpgmeKey) p->data);
	    }
	    for (p = key_list; p != NULL; p = p->next)
		gpgme_key_unref ((GpgmeKey) p->data);
	    g_slist_free (key_list);
	    if (rset != NULL) {
	        err = gpgme_op_encrypt_sign(ctx, rset, plain, cipher);
	    } else {
	    	err = gpgme_op_sign (ctx, plain, cipher, GPGME_SIG_MODE_CLEAR);
	    }
    }
    else if (!err) {
	gpgme_set_armor (ctx, 1);
	err = gpgme_op_encrypt (ctx, rset, plain, cipher);
    }

    if (err) {
        gpgme_data_release (cipher);
        cipher = NULL;
    }

    gpgme_release (ctx);
    return cipher;
}

const char*
gpgmegtk_passphrase_cb (void *opaque, const char *desc, void **r_hd)
{
    struct passphrase_cb_info_s *info = opaque;
    GpgmeCtx ctx = info ? info->c : NULL;
    const char *pass;
    
    if (!desc) {
        /* FIXME: cleanup by looking at *r_hd */
        return NULL;
    }
    
    if (store_passphrase && aycrypt_last_pass != NULL &&
        strncmp(desc, "TRY_AGAIN", 9) != 0)
        return g_strdup(aycrypt_last_pass);

    pass = passphrase_mbox (desc);
    if (!pass) {
        gpgme_cancel (ctx);
    }
    else {
        if (store_passphrase) {
 		if (aycrypt_last_pass)
			g_free(aycrypt_last_pass);
		aycrypt_last_pass = g_strdup(pass);
        }
    }
    return pass;
}

