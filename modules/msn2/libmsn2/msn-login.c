/*
 * Credits for the Auth and mashup:
 * http://msnpiki.msnfanatic.com/index.php/MSNP15:SSO
 * http://www.openrce.org/blog/view/449
 */

#include <stdio.h>
#include <string.h>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/des.h>
#include <openssl/rand.h>

#include "msn.h"
#include "msn-connection.h"
#include "msn-util.h"
#include "msn-account.h"
#include "msn-login.h"
#include "msn-errors.h"
#include "msn-message.h"
#include "msn-soap.h"
#include "msn-http.h"


static void msn_login_connected(MsnConnection *mc);

char msn_port[8] = {'\0'};
char msn_host[512] = {'\0'};

/* MSN Protocol Connection Explained inline. Start from the bottom */

/* 8) Got the final USR response. Either we're in or we're out */
static void msn_got_final_usr_response(MsnConnection *mc)
{
	MsnAccount *ma = mc->account;

	if ( strcmp(mc->current_message->argv[2], "OK") ) {
		mc->account = NULL;
		ma->ns_connection = NULL;

		ext_msn_login_response(ma, MSN_LOGIN_FAIL_PASSPORT);

		return;
	}

	ext_msn_login_response(ma, MSN_LOGIN_OK);
}


/* 7) Mashup done. Send the final USR */
static void msn_send_final_usr(MsnAccount *ma, const char *mash)
{
	msn_message_send(ma->ns_connection, 
				NULL, 
				MSN_COMMAND_USR, 
				4,
				MSN_AUTH_SSO, 
				MSN_AUTH_SUBSEQUENT, 
				ma->ticket, 
				mash);

	msn_connection_push_callback(ma->ns_connection, msn_got_final_usr_response);
}


static void derive_key(unsigned char *key, int key_len, unsigned char *magic, int magic_len, unsigned char out[24])
{
	HMAC_CTX ctx;
	unsigned char intermediate1[64];
	unsigned char intermediate2[64];
	unsigned char hash1[20], hash2[20];
	unsigned int len = 0;

	memset(intermediate1, 0, sizeof(intermediate1));
	memset(intermediate2, 0, sizeof(intermediate2));

	/* Build the intermediates */
	HMAC_CTX_init(&ctx);
	HMAC(EVP_sha1(), key, key_len, (unsigned char *)magic, magic_len, intermediate1, &len);
	HMAC_CTX_cleanup(&ctx);

	HMAC_CTX_init(&ctx);
	HMAC(EVP_sha1(), key, key_len, intermediate1, len, intermediate2, &len);
	HMAC_CTX_cleanup(&ctx);

	memcpy(intermediate1+len, magic, magic_len);
	memcpy(intermediate2+len, magic, magic_len);

	HMAC_CTX_init(&ctx);
	HMAC(EVP_sha1(), key, key_len, intermediate1, magic_len+20, hash1, &len);
	HMAC_CTX_cleanup(&ctx);

	HMAC_CTX_init(&ctx);
	HMAC(EVP_sha1(), key, key_len, intermediate2, magic_len+20, hash2, &len);
	HMAC_CTX_cleanup(&ctx);

	memcpy(out, hash1, 20);
	memcpy(out+20, hash2, 4);
}


/* 6) Mashup all the stuff we got */
static void msn_mashup_tokens_and_login(MsnAccount *ma)
{
	unsigned char hash_key[24], enc_key[24];

	unsigned char k1[8], k2[8], k3[8];

	unsigned char iv[8];

	unsigned char *buf;

	char *encoded = NULL;

	HMAC_CTX ctx;
	DES_key_schedule ks1, ks2, ks3;

	buf = (unsigned char *)calloc(ma->nonce_len+8, 1);

	struct _send_data
	{
		unsigned int header_size;
		unsigned int crypt_mode;
		unsigned int cipher_type;
		unsigned int hash_type;
		unsigned int iv_length;
		unsigned int hash_length;
		unsigned int cipher_length;

		unsigned char iv[8];
		unsigned char hash[20];
		unsigned char cipher[72];
	} send_data;

	send_data.header_size = 28;
	send_data.crypt_mode = 1;		/* CRYPT_MODE_CBC */
	send_data.cipher_type = 0x6603;	/* TripleDES */
	send_data.hash_type = 0x8004;		/* SHA1 */
	send_data.iv_length = sizeof(send_data.iv);
	send_data.hash_length = sizeof(send_data.hash);
	send_data.cipher_length = sizeof(send_data.cipher);

	/* Go about doing the mashup now */

	/* derive 24 byte keys for hash and encryption */
	derive_key(ma->secret, ma->secret_len, (unsigned char *)"WS-SecureConversationSESSION KEY HASH", 
			strlen("WS-SecureConversationSESSION KEY HASH"), hash_key);
	derive_key(ma->secret, ma->secret_len, (unsigned char *)"WS-SecureConversationSESSION KEY ENCRYPTION", 
			strlen("WS-SecureConversationSESSION KEY ENCRYPTION"), enc_key);

	/* Generate hash of the nonce */
	HMAC_CTX_init(&ctx);

	HMAC(EVP_sha1(), 
		hash_key, 
		sizeof(hash_key), 
		(unsigned char *)ma->nonce, 
		ma->nonce_len, 
		send_data.hash, 
		&send_data.hash_length);

	HMAC_CTX_cleanup(&ctx);

	/* Generate a random iv */
	RAND_seed(iv, sizeof(iv));
	DES_random_key(&iv);

	memcpy(send_data.iv, iv, 8);

	/* Encrypt the nonce */
	memcpy(buf, ma->nonce, ma->nonce_len);
	memset(buf+ma->nonce_len, 8, 8);

	/* make three 8-byte keys out of the 24 byte key */
	memcpy(k1, enc_key, 8);
	memcpy(k2, enc_key+8, 8);
	memcpy(k3, enc_key+16, 8);

	DES_set_key_unchecked(&k1, &ks1);
	DES_set_key_unchecked(&k2, &ks2);
	DES_set_key_unchecked(&k3, &ks3);

	DES_ede3_cbc_encrypt(buf, send_data.cipher, ma->nonce_len+8, &ks1, &ks2, &ks3, &iv, DES_ENCRYPT);

	encoded = ext_base64_encode((unsigned char *)&send_data, sizeof(send_data));

	msn_send_final_usr(ma, encoded);

	free(encoded);
}


/* 5) SSO */

/* 5.2) Process Response */
static void msn_sso_response(MsnAccount *ma, char *data, int len)
{
	/*
	 * TODO: Check for redirects?
	 * TODO: Do SOAP stuff here? I can't see any reason why I should -- 
	 * I am discarding most of the data anyway.
	 */
	char *d = NULL;

	if ( (d = strstr(data, "<wsse:BinarySecurityToken Id=\"Compact1\">")) ) {
		char *ticket = strstr(d, "t=");

		if(ticket) {
			char *end;

			end = strstr(ticket, "&amp;p=");

			if(end) {
				end++;
				*end = 'p';
				end++;
				*end = '=';
				end++;
				*end = '\0';
				ma->ticket = strdup(ticket);
				d = end+1;	/* move d ahead so that we can work further */
			}
		}
	}

	if(!ma->ticket) {
		/* Flag Error */
		msn_connection_free(ma->ns_connection);
		ma->ns_connection = NULL;

		fprintf(stderr, "No ticket!!\n");

		ext_msn_login_response(ma, MSN_LOGIN_FAIL_SSO);

		return;
	}


	if ( (d = strstr(d, "<wst:BinarySecret>")) ) {
		char *end;
		char *secret = d+18;

		end = strstr(secret, "</wst:BinarySecret>");

		if(end) {
			*end = '\0';
			ma->secret = ext_base64_decode(secret, &ma->secret_len);
			d = end+1;	/* move d ahead so that we can work further */
		}
	}

	if(!ma->secret) {
		/* Flag Error */
		msn_connection_free(ma->ns_connection);
		ma->ns_connection = NULL;

		fprintf(stderr, "No secret!!\n");

		ext_msn_login_response(ma, MSN_LOGIN_FAIL_SSO);

		return;
	}

	if ( (d = strstr(d, "<wsse:BinarySecurityToken Id=\"Compact2\">")) ) {
		char *ticket = strstr(d, "t=");

		if(ticket) {
			char *end;

			end = strstr(ticket, "&amp;p=");

			if(end) {
				*end = '\0';
				ma->contact_ticket = strdup(ticket);
				d = end+1;	/* move d ahead so that we can work further */
			}
		}
	}

	if(!ma->contact_ticket) {
		/* Flag Error */
		msn_connection_free(ma->ns_connection);
		ma->ns_connection = NULL;

		fprintf(stderr, "No contact ticket!!\n");

		ext_msn_login_response(ma, MSN_LOGIN_FAIL_SSO);

		return;
	}

	msn_mashup_tokens_and_login(ma);
}


/* 5.1) Send Login Request to SSO */
static void msn_sso_start (MsnAccount *ma)
{
	char *url;
	char *sso_request = msn_soap_build_request(MSN_SOAP_LOGIN_REQUEST, 
						ma->passport,
						ma->password,
						ma->policy);
	
	char *params = "Connection: Keep-Alive\r\n"
			"Cache-Control: no-cache\r\n";

	if(strstr(ma->passport, "@msn.com")) {
		url = strdup("https://msnia.login.live.com/pp550/RST.srf");
	}
	else {
		url = strdup("https://login.live.com/RST.srf");
	}

	msn_http_request(ma, MSN_HTTP_POST, NULL, url, sso_request, msn_sso_response, params);

	free(sso_request);
	free(url);
}


/* 4) If the NS is not busy it will ask us to go ahead for the Tweener
 * else it will XFR us to another server where we will restart from 1) */
static void msn_got_usr_response(MsnConnection *mc)
{
	if ( mc->current_message->command == MSN_COMMAND_XFR ) {
		/* Update the IP address and port and connect to the other place */
		char *port_offset = NULL;
		MsnAccount *ma = mc->account;

		ma->ns_connection = msn_connection_new();

		ma->ns_connection->host = strdup(mc->current_message->argv[3]);
		port_offset = strchr(ma->ns_connection->host, ':');

		if(!port_offset) {
			msn_connection_free(ma->ns_connection);
			ma->ns_connection = NULL;

			ext_msn_login_response(mc->account, MSN_LOGIN_FAIL_OTHER);
			mc->account = NULL;

			return;
		}

		*port_offset = '\0';

		ma->ns_connection->port = atoi(++port_offset);

		ma->ns_connection->type = MSN_CONNECTION_NS;
		ma->ns_connection->account = ma;

		/* Dissociate from account so that it can be freed */
		mc->account = NULL;

		msn_connection_connect(ma->ns_connection, msn_login_connected);
	}
	else {
		/* Start SSO */
		mc->account->policy = strdup(mc->current_message->argv[4]);
		mc->account->nonce = (unsigned char *)strdup(mc->current_message->argv[5]);
		mc->account->nonce_len = strlen((char *)mc->account->nonce);

		msn_sso_start(mc->account);
	}
}

/* 3) Send a USR with our passport */
static void msn_got_client_info_response ( MsnConnection *mc )
{
	/* We don't care what msn recommends... Ayttm is teh rul3z!!!!111oneone */

	msn_message_send ( mc, NULL, MSN_COMMAND_USR, 3, MSN_AUTH_SSO, MSN_AUTH_INITIAL, mc->account->passport );

	msn_connection_push_callback(mc, msn_got_usr_response);
}

/* 2) Send CVR with our MSN client information */
static void msn_got_version_response(MsnConnection *mc)
{
	if( mc->current_message->argc < 3 || strcmp(mc->current_message->argv[2], MSN_PROTOCOL_VERSION) ) {
		ext_msn_login_response(mc->account, MSN_LOGIN_FAIL_VER);
		mc->account->ns_connection = NULL;
		mc->account = NULL;

		return;
	}

	/* TODO Get locale ID based on current locale and the list in:
	 http://msdn.microsoft.com/en-us/goglobal/bb964664.aspx */
	
	msn_message_send( mc, 
			NULL,
			MSN_COMMAND_CVR,
			MSN_LOCALE, 
			MSN_OS_NAME, 
			MSN_OS_VERSION, 
			MSN_ARCH, 
			MSN_CLIENT, 
			MSN_CLIENT_VER, 
			MSN_OFFICIAL_CLIENT,
			mc->account->passport );

	msn_connection_push_callback(mc, msn_got_client_info_response);
}

/* 1) Send VER with our MSN client version */
static void msn_login_connected(MsnConnection *mc)
{
	msn_message_send(mc, NULL, MSN_COMMAND_VER, MSN_PROTOCOL_VERSION, MSN_PROTO_CVR0);

	msn_connection_push_callback(mc, msn_got_version_response);
}

/* 0) We obviously begin with a connection to the server */
void msn_login(MsnAccount *ma)
{
	MsnConnection *mc = msn_connection_new();

	if(msn_host[0])
		mc->host = strdup(msn_host);
	else
		mc->host = strdup(MSN_DEFAULT_HOST);

	if(msn_port[0])
		mc->port = atoi(msn_port);
	else
		mc->port = MSN_DEFAULT_PORT;

	mc->type = MSN_CONNECTION_NS;
	mc->account = ma;

	ma->ns_connection = mc;

	msn_connection_connect(mc, msn_login_connected);
}


/* ^^ The login procedure above ^^  */


void msn_logout(MsnAccount *ma)
{
	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_OUT);

	msn_connection_free(ma->ns_connection);
	ma->ns_connection = NULL;

	ma->status = MSN_STATE_OFFLINE;

	l_list_foreach(ma->connections, msn_connection_free, NULL);
}


