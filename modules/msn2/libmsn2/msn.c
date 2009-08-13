#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>

#include "msn-account.h"
#include "msn-message.h"
#include "msn-connection.h"
#include "msn-ext.h"
#include "msn.h"

typedef struct {
	int trid;
	MsnCommandHandler handler;
} MsnCallback;

const char *msn_state_strings[] = { "NLN",  "HDN", "BSY", "IDL", "BRB", "AWY", "PHN", "LUN", "FLN" };
#define MSN_PRP_FRIENDLYNAME "MFN"
#define MSN_STATUS_COUNT 9

#define MSN_PRODUCT_KEY "ILTXC!4IXB5FB*PX"
#define MSN_PRODUCT_ID "PROD0119GSJUC$18"


void msn_account_free(MsnAccount *ma)
{
	free(ma->passport);
	free(ma->friendlyname);

	ext_msn_account_destroyed(ma);

	free(ma->policy);
	free(ma->nonce);
	free(ma->ticket);
	free(ma->secret);
	free(ma->contact_ticket);

	if(ma->ns_connection)
		msn_connection_free(ma->ns_connection);
	
	l_list_foreach(ma->connections, (LListFunc)msn_connection_free, NULL);

	l_list_free(ma->connections);
}


char *msn_strerror(int error)
{
	char buf[1024];

	sprintf(buf, "Error code %p\n", error);

	return strdup(buf);
}


void msn_account_cancel_connect(MsnAccount *ma)
{
	msn_connection_free(ma->ns_connection);
	ma->ns_connection = NULL;

	l_list_foreach(ma->connections, msn_connection_free, NULL);
}


void msn_set_initial_presence(MsnAccount *ma, int state)
{
	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_CHG, msn_state_strings[state]);
}


/* Reference: http://imfreedom.org/wiki/MSN:NS/Challenges */
void msn_send_chl_response(MsnAccount *ma, const char *seed)
{
	MD5_CTX ctx;

	int i=0;

	unsigned char md5[16];
	unsigned char key[16];

	long long high = 0, low = 0, temp = 0;
	unsigned int hash[4];

	char prod_id[256];
	int offset=0, size = 0;

	unsigned int *prod_id_array = NULL;

	char out[33];

	MD5_Init(&ctx);
	MD5_Update(&ctx, seed, strlen(seed));
	MD5_Update(&ctx, MSN_PRODUCT_KEY, strlen(MSN_PRODUCT_KEY));
	MD5_Final(md5, &ctx);

	/* Copy into an unsigned int array */
	memcpy(hash, md5, 16);

	for(i=0; i<4; i++)
		hash[i] &= 0x7FFFFFFF;

	memset(prod_id, 0, sizeof(prod_id));
	snprintf(prod_id, sizeof(prod_id), "%s%s", seed, MSN_PRODUCT_ID);
	size = strlen(prod_id);
	offset = 8-(size%8);
	memset(prod_id+size, '0', offset);
	size += offset;

	prod_id_array = (unsigned int *)calloc(size/4, sizeof(unsigned int));

	memcpy(prod_id_array, prod_id, size);

	for(i=0; i < strlen(prod_id)/4; i += 2) {
		temp = ( (long long)prod_id_array[i] * 0x0E79A9C1 ) % 0x7FFFFFFF + low;
		temp = (temp * hash[0] + hash[1]) % 0x7FFFFFFF;

		low = ( (long long)prod_id_array[i+1] + temp ) % 0x7FFFFFFF;
		low = (low * hash[2] + hash[3]) % 0x7FFFFFFF;

		high += low + temp;
	}

	low  = (low  + hash[1]) % 0x7FFFFFFF;
	high = (high + hash[3]) % 0x7FFFFFFF;

	/* Generate key */
	memcpy(key, &low, 4);
	memcpy(key+4, &high, 4);
	memcpy(key+8, &low, 4);
	memcpy(key+12, &high, 4);

	/* Print it out in hex */
	for(i=0; i<16; i++)
		sprintf(out+i*2, "%02x", md5[i] ^ key[i]);
	
	msn_message_send(ma->ns_connection, out, MSN_COMMAND_QRY, MSN_PRODUCT_ID, "32");

	free(prod_id_array);
}


int msn_get_status_num(const char *state)
{
	int i=0;

	for(; i<MSN_STATUS_COUNT; i++)
		if(!strcmp(msn_state_strings[i], state))
			return i;

	return -1;
}


void msn_set_state(MsnAccount *ma, int state)
{
	msn_message_send(ma->ns_connection, NULL, MSN_COMMAND_CHG, msn_state_strings[state]);
}


