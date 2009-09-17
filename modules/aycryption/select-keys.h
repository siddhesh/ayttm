/* select-keys.h - GTK+ based key selection
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

#ifndef GPGMEGTK_SELECT_KEYS_H
#define GPGMEGTK_SELECT_KEYS_H

#include <glib.h>
#include <gpgme.h>

struct select_keys_s {
	int okay;
	void *window;
	void *toplabel;
	void *clist;
	const char *pattern;
	gpgme_key_t *kset;
	unsigned int num_keys;
	gpgme_ctx_t select_ctx;
	char *key;
	int sort_type;
	int sort_column;
	int do_crypt;
	int do_sign;
};

extern char *aycrypt_last_pass;
extern int aycrypt_pass_ack;

struct select_keys_s gpgmegtk_recipient_selection(GSList *recp_names, int crypt,
	int sign);
const char *passphrase_mbox(const char *desc, int prev_was_bad);

#endif				/* GPGMEGTK_SELECT_KEYS_H */
