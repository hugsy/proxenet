#ifndef _HTTP_H
#define _HTTP_H

#include <sys/types.h>
#include "ssl.h"

#define MAX_HEADER_SIZE 128
#define HTTP_REQUEST_INIT_SIZE 1024
#define HTTP_RESPONSE_INIT_SIZE 1024

typedef struct _http_request_fields 
{
	  char* method;
	  char* proto;
	  boolean is_ssl;
	  char* hostname;
	  unsigned short port;
	  char* request_uri;
} http_request_t ;


boolean get_url_information(char*, http_request_t*); 
boolean is_http_header(char*, int);
void generic_http_error_page(sock_t, char*);
int create_http_socket(char*, sock_t*, sock_t*, ssl_context_t*); 
int format_http_request(char*);

#endif /* _HTTP_H */
