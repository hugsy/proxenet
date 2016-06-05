#ifndef _HTTP_H
#define _HTTP_H

#include <sys/types.h>

#include "ssl.h"

#define MAX_HEADER_SIZE 128
#define HTTP_REQUEST_INIT_SIZE 1024
#define HTTP_RESPONSE_INIT_SIZE 1024
#define HTTP_DEFAULT_PORT 80
#define HTTPS_DEFAULT_PORT 443

typedef enum _http_protocol_types {
        HTTP = 1,
        HTTPS,
        WS,
        WSS,
        TRANSPARENT
} proto_t;

typedef struct _http_request_fields
{
                proto_t proto_type;
                char* method;
                char* proto;
                char* hostname;
                unsigned short port;
                char* path;
                char* version;
                char* uri;
} http_infos_t ;

typedef enum _request_t {
	REQUEST = 0,
	RESPONSE,
        ONLOAD,
        ONLEAVE
} req_t;

typedef struct _request_type {
		long id;
		req_t type;
		char* data;
		size_t size;
                bool is_ssl;
		http_infos_t http_infos;
                bool do_intercept;
} request_t;

#define CRLF "\r\n"
#define HTTP_STRING  "http"
#define HTTPS_STRING "https"
#define WS_STRING "ws"
#define WSS_STRING "wss"

#define HTTP_PROTO_STRING  HTTP_STRING"://"
#define HTTPS_PROTO_STRING HTTPS_STRING"://"
#define WS_PROTO_STRING  WS_STRING"://"


void     generic_http_error_page(sock_t, char*);
int      create_http_socket(request_t*, sock_t*, sock_t*, ssl_context_t*);
int      format_http_request(request_t*);
int      parse_http_request(request_t*);
void     free_http_infos(http_infos_t *);
int      ie_compat_read_post_body(sock_t, request_t*, proxenet_ssl_context_t*);

#endif /* _HTTP_H */
