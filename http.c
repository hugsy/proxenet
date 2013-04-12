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


/**
 *
 * request MUST be like
 * METHOD proto://hostname[:port][/location][?param=value....]\r\n
 * cf. RFC2616
 */
boolean get_url_information(char* request, http_request_t* http)
{ 
	char *start_pos, *cur_pos, *end_pos;
	unsigned int str_len;
	
	str_len = -1;
	cur_pos = NULL;
	
	
	/* find method */
	start_pos = index(request, ' ');
	end_pos = index(start_pos+1, ' ');
	
	if (start_pos==NULL || end_pos==NULL) {
		xlog(LOG_ERROR, "Malformed HTTP Header\n");
		return FALSE;
	}
	
	
	str_len = start_pos-request ; 
	http->method = (char*)xmalloc(str_len +1);
	memcpy(http->method, request, str_len);
	
	++start_pos;
	
	/* get proto */
	if (!strncmp(start_pos,"http://",7)) {
		http->proto = "http";
		http->port = 80;
		start_pos += 7;
		
	} else if (!strncmp(start_pos,"https://",8)) {
		http->proto = "https";
		http->port = 443;
		http->is_ssl = TRUE;
		start_pos += 8;
	} else if (!strcmp(http->method,"CONNECT")) {
		http->proto = "https";
		http->port = 443;
		http->is_ssl = TRUE;
	} else {
		xlog(LOG_ERROR, "Malformed HTTP/HTTPS URL, unknown proto\n");
		xlog(LOG_DEBUG, "%s\n", request);
		xfree(http->method);
		return FALSE;
	}
	
	cur_pos = start_pos;
	
	/* get hostname */
	for(; *cur_pos && *cur_pos!=':' && *cur_pos!='/' && cur_pos<end_pos; cur_pos++);
	str_len = cur_pos - start_pos;
	http->hostname = (char*)xmalloc(str_len+1);
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
		http->request_uri = (char*) xmalloc(str_len+1);
		memcpy(http->request_uri, cur_pos, str_len);
	} else {
		http->request_uri = (char*) xmalloc(2);
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
	
	return TRUE;
}


/**
 *
 */
int format_http_request(char* request) 
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
			xlog(LOG_ERROR, "Cannot find protocol\n");	       
			return -1;
		}
	}
	
	new_ptr = index(new_ptr, '/');
	old_reqlen = strlen(request) - (old_ptr-request);
	new_reqlen = old_reqlen  - (new_ptr-old_ptr);
	
	for (i=0; i<new_reqlen; i++) *(old_ptr+i) = *(new_ptr+i);
	for (i=new_reqlen; i<old_reqlen; i++) *(old_ptr+i) = '\0';
	
	return 0;
}


/**
 *
 */
sock_t create_http_socket(char* http_request, sock_t srv_sock, ssl_ctx_t* ssl_ctx) 
{
	sock_t http_socket;
	http_request_t http_infos;
	int retcode;
	char* err;
	char sport[6];
	
	xzero(sport, 6);
	xzero(&http_infos, sizeof(http_request_t));
	http_infos.method = NULL;
	http_infos.hostname = NULL;
	http_infos.request_uri = NULL;
	
	/* get target from string and establish client socket to dest */
	if (get_url_information(http_request, &http_infos) == FALSE) {
		xlog(LOG_ERROR, "Failed to extract valid parameters from URL.\n");
		return -1;
	}
	
	snprintf(sport, 5, "%d", http_infos.port);
	
	http_socket = create_connect_socket(http_infos.hostname, sport, &err);
	if (http_socket < 0) {
		if (err)
			generic_http_error_page(srv_sock, err);
		else
			generic_http_error_page(srv_sock, "Unknown error in <i>create_connect_socket</i>");
		
		retcode = -1;
		
	} else {
		retcode = http_socket;
		
		/* if ssl, set up ssl interception */
		if (http_infos.is_ssl) {
			
			/* 1. set up proxy->server ssl session */ 
			ssl_ctx->cli = proxenet_ssl_init_client_context(ssl_ctx);
			if(ssl_ctx->cli==NULL) {
				retcode = -1;
				goto create_http_socket_end;
			}
			
			proxenet_ssl_wrap_socket(ssl_ctx->cli, http_socket);
			if (proxenet_ssl_handshake(ssl_ctx->cli) < 0) {
				xlog(LOG_ERROR, "proxy->server: handshake\n");
				retcode = -1;
				goto create_http_socket_end;
			}
#ifdef DEBUG
			xlog(LOG_DEBUG, "SSL Handshake with server done\n");
#endif
			proxenet_write(srv_sock, "HTTP/1.1 200 OK\r\n\r\n", 19, NULL);
			
			/* 2. respond to client with our own ssl materials */
			ssl_ctx->srv = proxenet_ssl_init_server_context(ssl_ctx);
			if(ssl_ctx->srv==NULL) {
				retcode = -1;
				goto create_http_socket_end;		 
			}
			
			proxenet_ssl_wrap_socket(ssl_ctx->srv, srv_sock);
			if (proxenet_ssl_handshake(ssl_ctx->srv) < 0) {
				xlog(LOG_ERROR, "proxy->client: handshake\n");
				retcode = -1;
				goto create_http_socket_end;
			}
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "SSL Handshake with client done\n");
#endif
		}
	}
	
create_http_socket_end:
	xfree(http_infos.method);
	xfree(http_infos.hostname);
	xfree(http_infos.request_uri);
	
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
		xlog(LOG_ERROR, "Failed to write error HTML header\n");
	}
	
	if(write(sock, msg, strlen(msg)) < 0){
		xlog(LOG_ERROR, "Failed to write error HTML page\n");
	}
	
	if(write(sock, html_footer, strlen(html_footer)) < 0){
		xlog(LOG_ERROR, "Failed to write error HTML footer\n");
	}
}
