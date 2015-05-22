#ifndef _HTTP_H
#define _HTTP_H

#include <sys/types.h>

#include "plugin.h"
#include "ssl.h"

#define MAX_HEADER_SIZE 128
#define HTTP_REQUEST_INIT_SIZE 1024
#define HTTP_RESPONSE_INIT_SIZE 1024
#define HTTP_DEFAULT_PORT 80
#define HTTPS_DEFAULT_PORT 443


void     generic_http_error_page(sock_t, char*);
int      create_http_socket(request_t*, sock_t*, sock_t*, ssl_context_t*);
int      format_http_request(char**, size_t*);
int      update_http_infos(request_t*);
void     free_http_infos(http_request_t *);
int      ie_compat_read_post_body(sock_t, request_t*, proxenet_ssl_context_t*);

#endif /* _HTTP_H */
