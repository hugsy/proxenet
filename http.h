#ifndef _HTTP_H
#define _HTTP_H

#include <sys/types.h>

#include "plugin.h"
#include "ssl.h"

#define MAX_HEADER_SIZE 128
#define HTTP_REQUEST_INIT_SIZE 1024
#define HTTP_RESPONSE_INIT_SIZE 1024

int create_http_socket(char*, sock_t*, sock_t*, ssl_context_t*, request_t*); 
bool is_valid_http_request(char*);
char* get_request_full_uri(request_t*);

#endif /* _HTTP_H */
