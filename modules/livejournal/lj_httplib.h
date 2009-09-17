#include "account.h"
#include "llist.h"

#ifndef LJ_HTTPLIB_H
#define LJ_HTTPLIB_H

enum lj_http_success {
	LJ_HTTP_OK,
	LJ_HTTP_NOK,
	LJ_HTTP_NETWORK
};

#ifdef __cplusplus
extern "C" {
#endif

	typedef void (*lj_callback) (int success, eb_local_account *ela,
		LList *pairs);

	void send_http_request(const char *request, lj_callback callback,
		eb_local_account *ela);
	int url_to_host_port_path(const char *url, char *host, int *port,
		char *path);

#ifdef __cplusplus
}
#endif
#endif
