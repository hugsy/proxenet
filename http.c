#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "core.h"
#include "utils.h"
#include "socket.h"
#include "http.h"
#include "ssl.h"
#include "main.h"


/**
 *
 */
static void generic_http_error_page(sock_t sock, char* msg)
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


/**
 *
 * request MUST be like
 * METHOD proto://hostname[:port][/location][?param=value....] HTTP/X.Y\r\n
 * cf. RFC2616
 */
static bool get_url_information(char* request, http_request_t* http)
{
	char *start_pos, *cur_pos, *end_pos;
	unsigned int str_len;

	cur_pos = NULL;


	/* find method */
	start_pos = strchr(request, ' ');
	if (start_pos == NULL) {
		xlog(LOG_ERROR, "%s\n", "Malformed HTTP Header");
		return false;
	}

	end_pos = strchr(start_pos+1, ' ');
	if (end_pos==NULL) {
		xlog(LOG_ERROR, "%s\n", "Malformed HTTP Header");
		return false;
	}


	str_len = start_pos - request ;
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

	/* get uri (no need to parse) */
	str_len = end_pos - cur_pos;
	if (str_len > 0) {
		http->uri = (char*) proxenet_xmalloc(str_len+1);
		memcpy(http->uri, cur_pos, str_len);
	} else {
		http->uri = (char*) proxenet_xmalloc(2);
		*(http->uri) = '/';
	}

	/* get version */
	cur_pos+= str_len + 1;
	end_pos = strchr(cur_pos, '\r');
	if (!end_pos)
		return false;
	str_len = end_pos - cur_pos;
	http->version = (char*) proxenet_xmalloc(str_len+1);
	memcpy(http->version, cur_pos, str_len);

	return true;
}


/**
 *
 */
bool is_valid_http_request(char** request, size_t* request_len)
{
	size_t new_request_len = -1;
	char *old_ptr, *new_ptr;
	unsigned int i;
	int offlen;

        offlen = -1;
	old_ptr = new_ptr = NULL;
	old_ptr = strstr(*request, "http://");
	if (old_ptr)
		offlen = 7;
	else {
		old_ptr = strstr(*request, "https://");
		if (old_ptr)
			offlen = 8;
	}

	if (offlen < 0) {
		xlog(LOG_ERROR, "Cannot find protocol (http|https) in request:\n%s\n", *request);
		return false;
	}

	new_ptr = strchr(old_ptr + offlen, '/');
	if (!new_ptr) {
		xlog(LOG_ERROR, "%s\n", "Cannot find path (must not be implicit)");
		return false;
	}

	new_request_len = *request_len - (new_ptr-old_ptr);

#ifdef DEBUG
	xlog(LOG_DEBUG, "Adjusting buffer to %d->%d bytes\n", *request_len, new_request_len);
#endif

	for (i=0; i<new_request_len - (old_ptr-*request);i++) {
		*(old_ptr+i) = *(new_ptr+i);
        }

	*request = proxenet_xrealloc(*request, new_request_len);
	*request_len = new_request_len;

	return true;
}


/**
 * read first HTTP header from request and parse it to fill the request struct
 *
 * @return 0 if successful, -1 if any error occurs
 */
int set_https_infos(request_t *req) {
	char *ptr, *buf;
	char c;

	buf = req->data;

	/* method  */
	ptr = strchr(buf, ' ');
	if (!ptr)
                return -1;

	c = *ptr;
	*ptr = '\0';
	proxenet_xfree(req->http_infos.method);
	req->http_infos.method = strdup(buf);
	*ptr = c;

	buf = ptr+1;

	/* path */
	ptr = strchr(buf, ' ');
	if (!ptr)
                return -1;
	c = *ptr;
	*ptr = '\0';
	proxenet_xfree(req->http_infos.uri);
	req->http_infos.uri = strdup(buf);
	*ptr = c;

	buf = ptr+1;

	/* version */
	ptr = strchr(req->data, '\r');
	if (!ptr)
                return -1;

	c = *ptr;
	*ptr = '\0';
	req->http_infos.version = strdup(buf);
	*ptr = c;

#ifdef DEBUG
        xlog(LOG_DEBUG, "request %d method='%s' path='%s' version='%s'\n",
             req->id, req->http_infos.method, req->http_infos.uri, req->http_infos.version);
#endif

        return 0;
}


/**
 *
 */
static char* get_request_full_uri(request_t* req)
{
	char* uri;
	http_request_t* http_infos = &req->http_infos;
	size_t len;

	if (!req || !http_infos)
			return NULL;

	http_infos = &req->http_infos;
	len = strlen("https://") + strlen(http_infos->hostname) + strlen(":") + strlen("65535");
	len+= strlen(http_infos->uri);
	uri = (char*)proxenet_xmalloc(len+1);

	snprintf(uri, len, "%s://%s:%d%s",
		 http_infos->is_ssl?"https":"http",
		 http_infos->hostname,
		 http_infos->port,
		 http_infos->uri);

	return uri;
}


/**
 * Establish a connection from proxenet -> server. If proxy forwarding configured, then process
 * request to other proxy.
 *
 * @return 0 if successful, -1 otherwise
 */
int create_http_socket(request_t* req, sock_t* server_sock, sock_t* client_sock, ssl_context_t* ssl_ctx)
{
	int retcode;
	char *host, *port;
	char sport[6] = {0, };
	http_request_t* http_infos = &req->http_infos;
	bool use_proxy = (cfg->proxy.host != NULL) ;


	/* get target from string and establish client socket to dest */
	if (get_url_information(req->data, http_infos) == false) {
		xlog(LOG_ERROR, "%s\n", "Failed to extract valid parameters from URL.");
		return -1;
	}

        req->uri = get_request_full_uri(req);

#ifdef DEBUG
	xlog(LOG_DEBUG, "URL: %s\n", req->uri);
#endif

	ssl_ctx->use_ssl = http_infos->is_ssl;
	snprintf(sport, 5, "%u", http_infos->port);

	/* do we forward to another proxy ? */
	if (use_proxy) {
		host = cfg->proxy.host;
		port = cfg->proxy.port;
	} else {
		host = http_infos->hostname;
		port = sport;
	}

#ifdef DEBUG
        xlog(LOG_DEBUG, "Relay request %d to %s '%s:%s'\n",
             req->id,
             use_proxy?"PROXY":"",
             host, port);
#endif


	retcode = create_connect_socket(host, port);
	if (retcode < 0) {
		if (errno)
			generic_http_error_page(*server_sock, strerror(errno));
		else
			generic_http_error_page(*server_sock, "Unknown error in <i>create_connect_socket</i>");

		return -1;

	}

#ifdef DEBUG
        xlog(LOG_DEBUG, "Socket to %s '%s:%s' : %d\n",
             use_proxy?"proxy":"server",
             host, port,
             retcode);
#endif

        *client_sock = retcode;

        http_infos->do_intercept = ( (cfg->intercept_mode==INTERCEPT_ONLY && fnmatch(cfg->intercept_pattern, http_infos->hostname, 0)==0) || \
                                     (cfg->intercept_mode==INTERCEPT_EXCEPT && fnmatch(cfg->intercept_pattern, http_infos->hostname, 0)==FNM_NOMATCH) );

#ifdef DEBUG
        xlog(LOG_DEBUG, "Server '%s' %s match interception '%s' with pattern '%s'\n",
             http_infos->hostname,
             http_infos->do_intercept?"do":"do not",
             cfg->intercept_mode==INTERCEPT_ONLY?"INTERCEPT_ONLY":"INTERCEPT_EXCEPT",
             cfg->intercept_pattern);
#endif

        /* set up ssl layer */
        if (http_infos->is_ssl) {

                if (use_proxy) {
                        char *connect_buf = NULL;

                        /* 0. set up proxy->proxy ssl session (i.e. forward CONNECT request) */
                        retcode = proxenet_write(*client_sock, req->data, req->size);
                        if (retcode < 0) {
                                xlog(LOG_ERROR, "%s failed to CONNECT to proxy\n", PROGNAME);
                                return -1;
                        }

                        /* read response */
                        retcode = proxenet_read_all(*client_sock, &connect_buf, NULL);
                        if (retcode < 0) {
                                xlog(LOG_ERROR, "%s Failed to read from proxy\n", PROGNAME);
                                return -1;
                        }

                        /* expect HTTP 200 */
                        if (   (strncmp(connect_buf, "HTTP/1.0 200", 12) != 0)
                               && (strncmp(connect_buf, "HTTP/1.1 200", 12) != 0)) {
                                xlog(LOG_ERROR, "%s->proxy: bad HTTP version\n", PROGNAME);
                                if (cfg->verbose)
                                        xlog(LOG_ERROR, "Received %s\n", connect_buf);

                                return -1;
                        }
#ifdef DEBUG
                        xlog(LOG_DEBUG, "HTTP Connect Ok with '%s:%s', cli_sock=%d\n", host, port, *client_sock);
#endif
                }

                if (http_infos->do_intercept){

                        /* 1. set up proxy->server ssl session with hostname */
                        if(proxenet_ssl_init_client_context(&(ssl_ctx->client), http_infos->hostname) < 0) {
                                return -1;
                        }

                        proxenet_ssl_wrap_socket(&(ssl_ctx->client.context), client_sock);
                        if (proxenet_ssl_handshake(&(ssl_ctx->client.context)) < 0) {
                                xlog(LOG_ERROR, "%s->server: handshake\n", PROGNAME);
                                return -1;
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "SSL handshake with %s done, cli_sock=%d\n",
                             use_proxy?"proxy":"server", *client_sock);
#endif
                }

                if (proxenet_write(*server_sock, "HTTP/1.0 200 Connection established\r\n\r\n", 39) < 0){
                        return -1;
                }

                if (http_infos->do_intercept) {

                        /* 2. set up proxy->browser ssl session with hostname */
                        if(proxenet_ssl_init_server_context(&(ssl_ctx->server), http_infos->hostname) < 0) {
                                return -1;
                        }

                        proxenet_ssl_wrap_socket(&(ssl_ctx->server.context), server_sock);
                        if (proxenet_ssl_handshake(&(ssl_ctx->server.context)) < 0) {
                                xlog(LOG_ERROR, "handshake %s->client failed\n", PROGNAME);
                                return -1;
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "SSL handshake with client done, srv_sock=%d\n", *server_sock);
#endif
                }
        }


	return retcode;
}
