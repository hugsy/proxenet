#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "utils.h"
#include "socket.h"
#include "http.h"
#include "ssl.h"
#include "main.h"


/**
 *
 * request MUST be like
 * METHOD proto://hostname[:port][/location][?param=value....] HTTP/X.Y\r\n
 * cf. RFC2616
 */
bool get_url_information(char* request, http_request_t* http)
{ 
	char *start_pos, *cur_pos, *end_pos;
	unsigned int str_len;
	
	str_len = -1;
	cur_pos = NULL;
	
	
	/* find method */
	start_pos = index(request, ' ');
	if (start_pos == NULL) {
		xlog(LOG_ERROR, "%s\n", "Malformed HTTP Header");
		return false;
	}
	
	end_pos = index(start_pos+1, ' ');
	if (end_pos==NULL) {
		xlog(LOG_ERROR, "%s\n", "Malformed HTTP Header");
		return false;
	}
	
	
	str_len = start_pos-request ; 
	http->method = (char*)proxenet_xmalloc(str_len +1);
	memcpy(http->method, request, str_len);
	
	++start_pos;
	
	/* get proto */
	if (!strncmp(start_pos,"http://", 7)) {
		http->proto = "http";
		http->port = 80;
		start_pos += 7;
		
	} else if (!strncmp(start_pos,"https://", 8)) {
		http->proto = "https";
		http->port = 443;
		http->is_ssl = true;
		start_pos += 8;
		
	} else if (!strcmp(http->method, "CONNECT")) {
		http->proto = "https";
		http->port = 443;
		http->is_ssl = true;
		
	} else {
		xlog(LOG_ERROR, "%s\n", "Malformed HTTP/HTTPS URL, unknown proto");
		xlog(LOG_DEBUG, "%s\n", request);
		proxenet_xfree(http->method);
		return false;
	}
	
	cur_pos = start_pos;
	
	/* get hostname */
	for(; *cur_pos && *cur_pos!=':' && *cur_pos!='/' && cur_pos<end_pos; cur_pos++);
	str_len = cur_pos - start_pos;
	http->hostname = (char*)proxenet_xmalloc(str_len+1);
	memcpy(http->hostname, start_pos, str_len);
	
	/* get port if set explicitly (i.e ':<port_num>'), otherwise default */
	if(*cur_pos == ':') {
		cur_pos++;
		http->port = (unsigned short)atoi(cur_pos);
		for(;*cur_pos!='/' && cur_pos<end_pos;cur_pos++);
	}
	
	/* get request_uri (no need to parse) */
	str_len = end_pos - cur_pos;
	if (str_len > 0) {
		http->request_uri = (char*) proxenet_xmalloc(str_len+1);
		memcpy(http->request_uri, cur_pos, str_len);
	} else {
		http->request_uri = (char*) proxenet_xmalloc(2);
		*(http->request_uri) = '/';
	}
	
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "URL: %s %s://%s:%d%s\n",
	     http->method,
	     http->proto,
	     http->hostname,
	     http->port,
	     http->request_uri);
#endif 
	
	return true;
}


/**
 *
 */
bool is_valid_http_request(char* request) 
{
	size_t old_reqlen = -1;
	size_t new_reqlen = -1;
	char* old_ptr = NULL;
	char* new_ptr = NULL;
	int i = -1;

	old_ptr = strstr(request, "http://");
	if (old_ptr) {
		new_ptr = old_ptr + 7;
	} else {
		old_ptr = strstr(request, "https://");
		if (old_ptr) {
			new_ptr = old_ptr + 8;
		} else {
			xlog(LOG_ERROR, "Cannot find protocol (http|https) in request:\n%s\n", request);
			return false;
		}
	}
	
	new_ptr = index(new_ptr, '/');
	old_reqlen = strlen(request) - (old_ptr-request);
	new_reqlen = old_reqlen  - (new_ptr-old_ptr);
	
	for (i=0; i<new_reqlen; i++) *(old_ptr+i) = *(new_ptr+i);
	for (i=new_reqlen; i<old_reqlen; i++) *(old_ptr+i) = '\0';
	
	return true;
}


/**
 * Establish a connection from proxenet -> server. If proxy forwarding configured, then process
 * request to other proxy.
 * 
 */
int create_http_socket(char* http_request, sock_t* server_sock, sock_t* client_sock, ssl_context_t* ssl_ctx) 
{
	int retcode;
	char *host, *port;
	char sport[6] = {0, };
	http_request_t http_infos = { .method = NULL,
				      .proto = NULL,
				      .is_ssl = false,
				      .hostname = NULL,
				      .port = 0,
				      .request_uri = NULL };
	bool use_proxy = (cfg->proxy.host != NULL) ;

	/* get target from string and establish client socket to dest */
	if (get_url_information(http_request, &http_infos) == false) {
		xlog(LOG_ERROR, "%s\n", "Failed to extract valid parameters from URL.");
		return -1;
	}

	ssl_ctx->use_ssl = http_infos.is_ssl;
	snprintf(sport, 5, "%u", http_infos.port);

	/* do we forward to another proxy ? */
	if (use_proxy) {
		host = cfg->proxy.host;
		port = cfg->proxy.port;
		
	} else {
		host = http_infos.hostname;
		port = sport;
	}
	
	retcode = create_connect_socket(host, port);
	if (retcode < 0) {
		if (errno)
			generic_http_error_page(*server_sock, strerror(errno));
		else
			generic_http_error_page(*server_sock, "Unknown error in <i>create_connect_socket</i>");
		
		retcode = -1;
		
	} else {
		*client_sock = retcode;
		
		/* if ssl, set up ssl interception */
		if (http_infos.is_ssl) {

			if (use_proxy) {
				char *connect_buf = NULL;
				
				/* 0. set up proxy->proxy ssl session (i.e. forward CONNECT request) */ 
				retcode = proxenet_write(*client_sock, http_request, strlen(http_request) );
				if (retcode < 0) {
					xlog(LOG_ERROR, "%s failed to CONNECT to proxy\n", PROGNAME);
					retcode = -1;
					goto http_sock_end;
				}

				/* read response */
				retcode = proxenet_read_all(*client_sock, &connect_buf, NULL);
				if (retcode < 0) {
					xlog(LOG_ERROR, "%s Failed to read from proxy\n", PROGNAME);
					retcode = -1;
					goto http_sock_end;
				}

				/* expect HTTP 200 */
				if (   (strncmp(connect_buf, "HTTP/1.0 200", 12) != 0) 
				    && (strncmp(connect_buf, "HTTP/1.1 200", 12) != 0)) {
					xlog(LOG_ERROR, "%s->proxy: bad response %s\n", PROGNAME, connect_buf);
					retcode = -1;
					goto http_sock_end;
				}
			}

			/* 1. set up proxy->server ssl session */ 
			if(proxenet_ssl_init_client_context(&(ssl_ctx->client)) < 0) {
				retcode = -1;
				goto http_sock_end;
			}
			
			proxenet_ssl_wrap_socket(&(ssl_ctx->client.context), client_sock);
			if (proxenet_ssl_handshake(&(ssl_ctx->client.context)) < 0) {
				xlog(LOG_ERROR, "%s->server: handshake\n", PROGNAME);
				retcode = -1;
				goto http_sock_end;
			}

#ifdef DEBUG
			xlog(LOG_DEBUG, "%s %d %d\n", "SSL handshake with server done", *client_sock, *server_sock);
#endif
			if (proxenet_write(*server_sock,
					   "HTTP/1.0 200 Connection established\r\n\r\n",
					   39) < 0){
				retcode = -1;
				goto http_sock_end;
			}

			/* 2. set up proxy->browser ssl session  */
			if(proxenet_ssl_init_server_context(&(ssl_ctx->server)) < 0) {
				retcode = -1;
				goto http_sock_end;
			}

			proxenet_ssl_wrap_socket(&(ssl_ctx->server.context), server_sock);
			if (proxenet_ssl_handshake(&(ssl_ctx->server.context)) < 0) {
				xlog(LOG_ERROR, "handshake %s->client '%s:%d' failed\n",
				     PROGNAME, http_infos.hostname, http_infos.port);
				retcode = -1;
				goto http_sock_end;
			}

#ifdef DEBUG
			xlog(LOG_DEBUG, "%s\n", "SSL Handshake with client done");
#endif
		}
	}
	
http_sock_end:
	proxenet_xfree(http_infos.method);
	proxenet_xfree(http_infos.hostname);
	proxenet_xfree(http_infos.request_uri);
	
	return retcode;
}


/**
 *
 */
void generic_http_error_page(sock_t sock, char* msg)
{
	char* html_header = "<html><body><h1>proxenet error page</h1><br/>";
	char* html_footer = "</body></html>";
	
	if (write(sock, html_header, strlen(html_header)) < 0) {
		xlog(LOG_ERROR, "%s\n", "Failed to write error HTML header");
	}
	
	if(write(sock, msg, strlen(msg)) < 0){
		xlog(LOG_ERROR, "%s\n", "Failed to write error HTML page");
	}
	
	if(write(sock, html_footer, strlen(html_footer)) < 0){
		xlog(LOG_ERROR, "%s\n", "Failed to write error HTML footer");
	}
}
