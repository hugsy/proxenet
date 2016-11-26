#define _GNU_SOURCE     1

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
#include "socks.h"
#include "main.h"
#include "minica.h"


/**
 *
 */
void generic_http_error_page(sock_t sock, char* msg)
{
        const char* html_header = "<html>"
                "<head><title>"PROGNAME": ERROR!</title></head>"
                "<body><h1>Error</h1>"
                PROGNAME" encountered an error when loading your page<br><br>"
                "The following message was returned:<br><div style=\"border: solid black 1px;font-family: monospace;\"><br>";
        const char* html_footer = "</div></body></html>";

        if (write(sock, html_header, strlen(html_header)) < 0) {
                xlog(LOG_ERROR, "Failed to write error HTML header: %s\n", strerror(errno));
                return;
        }

        if(write(sock, msg, strlen(msg)) < 0){
                xlog(LOG_ERROR, "Failed to write error HTML page: %s\n", strerror(errno));
                return;
        }

        if(write(sock, html_footer, strlen(html_footer)) < 0){
                xlog(LOG_ERROR, "Failed to write error HTML footer: %s\n", strerror(errno));
                return;
        }

        return;
}


/**
 *
 */
static char* get_request_full_uri(request_t* req)
{
        char* uri;
        http_infos_t* hi = &req->http_infos;
        size_t len;

        len = sizeof(HTTPS_PROTO_STRING) + strlen(hi->hostname) + sizeof(":") + sizeof("65535");
        len+= strlen(hi->path);
        uri = (char*)proxenet_xmalloc(len+1);

        proxenet_xsnprintf(uri, len, "%s://%s:%d%s",
                           hi->proto,
                           hi->hostname,
                           hi->port,
                           hi->path);

        return uri;
}


/**
 *
 */
static bool update_http_infos_uri(request_t* req)
{

        if (!req )
                return -1;

        if (req->http_infos.uri)
                proxenet_xfree(req->http_infos.uri);

        req->http_infos.uri = get_request_full_uri(req);

        return req->http_infos.uri != NULL ? true : false;
}

/**
 * Look up for a header in a request. If found, this function will allocate a buffer containing
 * the header value (after the ': ') until a line RET (CR/LF) is found.
 * If not found, it returns NULL.
 *
 * @note if found, the allocated buffer *MUST* be free-ed by caller.
 * @return a pointer to a malloc-ed buffer with the header value, or NULL if not found.
 */
static char* get_header_by_name(char* request, const char* header_name)
{
        char *ptr, *ptr2, c, *header_value;

        /* get header */
        ptr = strstr(request, header_name);
        if(!ptr){
                return NULL;
        }

        /* move to start of hostname  */
        ptr += strlen(header_name);
        ptr2 = ptr;

        /* get the end of header line */
        for(; *ptr2 && *ptr2!='\r' && *ptr2!='\n'; ptr2++);
        c = *ptr2;
        *ptr2 = '\0';

        /* copy the value  */
        header_value = proxenet_xstrdup2(ptr);

        *ptr2 = c;
        proxenet_strip(header_value);
        return header_value;
}


/**
 *
 */
static int get_hostname_from_header(request_t *req)
{
        char *ptr, *header;

        header = get_header_by_name(req->data, "Host:");
        if (!header){
                return -1;
        }

        /* if port number, copy it */
        ptr = strchr(header, ':');
        if (ptr){
                *ptr = '\0';
                req->http_infos.hostname = proxenet_xstrdup2(header);
                req->http_infos.port = (unsigned short)atoi(ptr+1);
        } else {
                req->http_infos.hostname = proxenet_xstrdup2(header);
                req->http_infos.port = (req->is_ssl) ? HTTPS_DEFAULT_PORT : HTTP_DEFAULT_PORT;
        }

        req->http_infos.proto_type = (req->is_ssl) ? HTTPS : HTTP;
        req->http_infos.proto = (req->is_ssl) ? HTTPS_STRING : HTTP_STRING;

        proxenet_xfree(header);

        return 0;
}


/**
 * Identify the protocol to use from the request header
 * @return a proto_t value if defined/found, -1 otherwise
 */
static int get_http_protocol(request_t *req)
{
        char *ptr, *ptr2;

        ptr2 = strstr(req->data, CRLF);
        if(!ptr2)
                goto invalid_http_protocol;

        /* Move to beginning of URL */
        for(ptr=req->data; ptr && *ptr!=' ' && *ptr!='\x00' && ptr < ptr2; ptr++);

        if(!ptr || *ptr!=' ')
                goto invalid_http_protocol;

        ++ptr;
        if(*ptr=='/'){
                /* this indicates that the request is well formed already */
                /* this case would happen only when using transparent mode */
                return TRANSPARENT;
        }

        if( strncmp(ptr, HTTP_PROTO_STRING, sizeof(HTTP_PROTO_STRING)-1)==0 ){
                /* Check if the requested URL starts with 'http://' */
                req->http_infos.proto_type = HTTP;
                return HTTP;

        } else if( strncmp(ptr, HTTPS_PROTO_STRING, sizeof(HTTPS_PROTO_STRING)-1)==0 ){
                /* Check if the requested URL starts with 'https://' */
                req->http_infos.proto_type = HTTPS;
                return HTTPS;

        } else if( strncmp(ptr, WS_PROTO_STRING, sizeof(WS_PROTO_STRING)-1)==0 ){
                /* Check if the requested URL starts with 'ws://' */
                req->http_infos.proto_type = WS;
                return WS;
        } else if( strncmp(ptr, WSS_PROTO_STRING, sizeof(WSS_PROTO_STRING)-1)==0 ){
                /* Check if the requested URL starts with 'ws://' */
                req->http_infos.proto_type = WSS;
                return WSS;
        }


invalid_http_protocol:
        xlog(LOG_ERROR, "%s\n", "Request is not a valid HTTP(S)/WS(S) request");
        if (cfg->verbose)
                xlog(LOG_ERROR, "The invalid request is:\n%s\n", req->data);

        return -1;
}


/**
 * Modifies the HTTP request to strip out the protocol and hostname
 * If successful, the request will be modified on output of this function as a
 * valid HTTP request :
 *
 * METHOD /PATH HTTP/1.1[...]
 *
 * @return 0 if no error was encountered, -1 otherwise.
 */
int format_http_request(request_t* req)
{
        size_t request_len = req->size;
        size_t new_request_len = 0;
        char **request = &req->data;
        char *old_ptr, *new_ptr, *ptr;
        unsigned int i;
        int offlen;

        switch(get_http_protocol(req)){
                case HTTP:
                        /* check that !NULL is done by get_http_protocol() */
                        ptr = strstr(*request, HTTP_PROTO_STRING);

                        /* -1 because of \x00 added by sizeof */
                        offlen = sizeof(HTTP_PROTO_STRING)-1;
                        break;

                case HTTPS:
                        ptr = strstr(*request, HTTPS_PROTO_STRING);
                        offlen = sizeof(HTTPS_PROTO_STRING)-1;
                        break;

                case WS:
                        ptr = strstr(*request, WS_PROTO_STRING);
                        offlen = sizeof(WS_PROTO_STRING)-1;
                        break;

                case WSS:
                        ptr = strstr(*request, WSS_PROTO_STRING);
                        offlen = sizeof(WSS_PROTO_STRING)-1;
                        break;

                case TRANSPARENT:
                        return 0;

                default:
                        return -1;
        }

        old_ptr = ptr;
        new_ptr = strchr(old_ptr + offlen, '/');
        if (!new_ptr) {
                xlog(LOG_ERROR, "%s\n", "Cannot find path (must not be implicit)");
                return -1;
        }

        new_request_len = request_len - (new_ptr-old_ptr);

#ifdef DEBUG
        xlog(LOG_DEBUG, "Formatting HTTP request (%dB->%dB)\n", request_len, new_request_len);
#endif

        for (i=0; i<new_request_len - (old_ptr-*request);i++) {
                *(old_ptr+i) = *(new_ptr+i);
        }

        req->data = proxenet_xrealloc(*request, new_request_len);
        req->size = new_request_len;

        return 0;
}


/**
 *
 */
int format_ws_request(request_t* req)
{
        req->http_infos.proto_type = (req->is_ssl) ? WSS : WS;
        req->http_infos.proto = (req->is_ssl) ? WSS_STRING : WS_STRING;
        update_http_infos_uri(req);
        return 0;
}


/**
 * This function updates all the fields of the current request_t with the new values found in the
 * request. Since those values will be useful many times, they are strdup-ed in a specific structure
 * (http_infos_t). Those values *must* be free-ed later on.
 *
 * @return 0 if successful, -1 if any error occurs.
 */
int parse_http_request(request_t *req)
{
        char *ptr, *buf, c;

        if (req->http_infos.proto_type == WS || req->http_infos.proto_type == WSS){
                /*
                 * if the connection is in a WebSocket, the request properties are already populated
                 * meaning that there is no need for (re)parsing.
                 */
                return 0;
        }

        buf = req->data;

        /* method */
        ptr = strchr(buf, ' ');
        if (!ptr){
                xlog(LOG_ERROR, "%s\n", "Cannot find HTTP method in request");
                if (cfg->verbose)
                        xlog(LOG_ERROR, "Buffer sent:\n%s\n", buf);
                return -1;
        }

        c = *ptr;
        *ptr = '\0';
        req->http_infos.method = proxenet_xstrdup2(buf);
        if (!req->http_infos.method){
                xlog(LOG_ERROR, "%s\n", "strdup(method) failed, cannot pursue...");
                return -1;
        }

        *ptr = c;

        /* by default, we assume that the connection is plain HTTP */
        if (req->http_infos.proto_type == 0){
                req->http_infos.proto_type  = HTTP;
                req->http_infos.port        = HTTP_DEFAULT_PORT;
                req->http_infos.proto       = HTTP_STRING;
        }

        /* populate hostname and port fields of `request` */
        if( get_hostname_from_header(req) < 0 ){
                xlog(LOG_ERROR, "Failed to get hostname: reason '%s'\n", "Invalid HTTP/1.1 request (missing Host header)");
                goto failed_hostname;
        }


        /* path */
        buf = ptr+1;

        if (!strncmp(buf, HTTP_PROTO_STRING, strlen(HTTP_PROTO_STRING))){
                buf = strchr(buf + 8, '/');
        }

        ptr = strchr(buf, ' ');
        if (!ptr){
                xlog(LOG_ERROR, "%s\n", "Cannot find HTTP path in request");
                goto failed_path;
        }

        c = *ptr;
        *ptr = '\0';

        if (strcmp(req->http_infos.method, "CONNECT")==0){
                req->http_infos.path = proxenet_xstrdup2("/");
        } else {
                req->http_infos.path = proxenet_xstrdup2(buf);
        }
        if (!req->http_infos.path){
                xlog(LOG_ERROR, "%s\n", "strdup(path) failed, cannot pursue...");
                goto failed_path;
        }
        *ptr = c;

        buf = ptr+1;


        /* version */
        ptr = strchr(req->data, '\r');
        if (!ptr){
                xlog(LOG_ERROR, "%s\n", "Cannot find HTTP version");
                goto failed_version;
        }

        c = *ptr;
        *ptr = '\0';
        req->http_infos.version = proxenet_xstrdup2(buf);
        if (!req->http_infos.version){
                xlog(LOG_ERROR, "%s\n", "strdup(version) failed, cannot pursue...");
                goto failed_version;
        }
        *ptr = c;


        /* refresh uri */
        req->http_infos.uri = get_request_full_uri(req);
        if( !update_http_infos_uri(req) ){
                xlog(LOG_ERROR, "%s\n", "get_request_full_uri() failed");
                goto failed_uri;
        }


        if (cfg->verbose) {
                xlog(LOG_INFO, "New request %d to '%s'\n",
                     req->id, req->http_infos.uri);

                if (cfg->verbose > 1) {
                        xlog(LOG_INFO,
                             "Request HTTP information:\n"
                             "method=%s\n"
                             "proto=%s\n"
                             "hostname=%s\n"
                             "port=%d\n"
                             "path=%s\n"
                             "version=%s\n"
                             ,
                             req->http_infos.method,
                             req->http_infos.proto,
                             req->http_infos.hostname,
                             req->http_infos.port,
                             req->http_infos.path,
                             req->http_infos.version);
                }
        }

        return 0;


failed_uri:
        proxenet_xfree(req->http_infos.version);

failed_version:
        proxenet_xfree(req->http_infos.path);

failed_path:
        proxenet_xfree(req->http_infos.hostname);

failed_hostname:
        proxenet_xfree(req->http_infos.method);

        return -1;
}


/**
 * Free all the heap allocated blocks on http_infos (allocated by parse_http_request()).
 */
void free_http_infos(http_infos_t *hi)
{
        proxenet_xclean(hi->method,   strlen(hi->method));
        proxenet_xclean(hi->hostname, strlen(hi->hostname));
        proxenet_xclean(hi->path,     strlen(hi->path));
        proxenet_xclean(hi->version,  strlen(hi->version));
        proxenet_xclean(hi->uri,      strlen(hi->uri));

        hi->proto = NULL;
        hi->port = 0;
        return;
}


/**
 * This function does the same as `create_http_socket()`, except that it performs SSL/TLS
 * interception (if asked by configuration).
 */
int create_ssl_socket_with_interception(request_t *req, sock_t *cli_sock, sock_t *srv_sock, ssl_context_t* ctx)
{
        char *connect_buf = NULL;
        http_infos_t* http_infos = &req->http_infos;
        int retcode = -1;
        bool use_proxy = (cfg->proxy.host != NULL);
        bool use_http_proxy = use_proxy && (cfg->is_socks_proxy==false);

        /* disable all interception if ssl intercept was explicitely disabled by config */
        if (cfg->ssl_intercept == false)
                req->do_intercept = false;

        /* if an HTTP proxy is used, expect CONNECT request */
        if (use_http_proxy) {

                /* 0. set up proxy->proxy ssl session (i.e. forward CONNECT request) */
                retcode = proxenet_write(*cli_sock, req->data, req->size);
                if (retcode < 0) {
                        xlog(LOG_ERROR, "%s failed to CONNECT to proxy\n", PROGNAME);
                        return -1;
                }

                /* read response */
                retcode = proxenet_read_all(*cli_sock, &connect_buf, NULL);
                if (retcode < 0) {
                        xlog(LOG_ERROR, "%s failed to read from proxy (retcode=%#x)\n", PROGNAME);

                        if (retcode==-ENODATA && cfg->verbose){
                                xlog(LOG_ERROR, "%s\n", "Data expected but none received");
                        }

                        return -1;
                }

                /* expect HTTP 200 */
                if (   (strncmp(connect_buf, "HTTP/1.0 200", 12) != 0)
                       && (strncmp(connect_buf, "HTTP/1.1 200", 12) != 0)) {
                        xlog(LOG_ERROR, "%s->proxy: bad HTTP version\n", PROGNAME);
                        proxenet_xfree(connect_buf);

                        if (cfg->verbose)
                                xlog(LOG_ERROR, "Received %s\n", connect_buf);

                        return -1;
                }

                proxenet_xfree(connect_buf);
        }

        if (!req->do_intercept)
                return 0;


        /* 1. set up proxy->server ssl session with hostname */
        if(proxenet_ssl_init_client_context(&(ctx->client), http_infos->hostname) < 0) {
                return -1;
        }

        proxenet_ssl_wrap_socket(&(ctx->client.context), cli_sock);

        retcode = proxenet_ssl_handshake(&(ctx->client.context));
        if (retcode < 0) {
                char handshake_error_desc[256] = {0, };
                char *res;

                if (retcode & 0x00006000) // if mbedtls error
                        proxenet_ssl_strerror(retcode, handshake_error_desc, sizeof(handshake_error_desc));
                else
                        res = strerror_r(retcode, handshake_error_desc, sizeof(handshake_error_desc));

                xlog(LOG_ERROR, "handshake %s->server failed '%s:%d' [code: %#x, reason: %s]\n",
                     PROGNAME, http_infos->hostname, http_infos->port, retcode);
                return -1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "SSL handshake with %s done, cli_sock=%d\n", use_http_proxy?"proxy":"server", *cli_sock);
#endif

        /* 2. set up proxy->browser ssl session with hostname */
        if(proxenet_ssl_init_server_context(&(ctx->server), http_infos->hostname) < 0) {
                return -1;
        }

        proxenet_ssl_wrap_socket(&(ctx->server.context), srv_sock);

        retcode = proxenet_ssl_handshake(&(ctx->server.context));
        if (retcode < 0) {
                char handshake_error_desc[256] = {0, };
                char *res;

                if (retcode & 0x00006000) // if mbedtls error
                        proxenet_ssl_strerror(retcode, handshake_error_desc, sizeof(handshake_error_desc));
                else
                        res = strerror_r(retcode, handshake_error_desc, sizeof(handshake_error_desc));

                xlog(LOG_ERROR, "handshake %s->client failed for '%s:%d' [code: %#x, reason: %s]\n",
                     PROGNAME, http_infos->hostname, http_infos->port, retcode, handshake_error_desc);
                return -1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "SSL handshake with client done, srv_sock=%d\n", *srv_sock);
#endif

        return 0;
}


/**
 * This function aimes to establish whether a request should be intercepted or not.
 * A request should be intercepted if
 * - Interception is set to INTERCEPT_ONLY and the hostname regex matches the expression
 *   from configuration
 * - Interception is set to INTERCEPT_EXCEPT and the hostname regex does not match the
 *   expression from configuration.
 *
 * @return true if the request must be intercepted, false otherwise
 */
static bool apply_intercept_rules(http_infos_t* hi)
{
        if (cfg->intercept_mode==INTERCEPT_ONLY &&
            fnmatch(cfg->intercept_pattern, hi->hostname, FNM_CASEFOLD)==0)
                /* INTERCEPT_ONLY + match */
                return true;

        if (cfg->intercept_mode==INTERCEPT_EXCEPT &&
            fnmatch(cfg->intercept_pattern, hi->hostname, FNM_CASEFOLD)==FNM_NOMATCH)
                /* INTERCEPT_EXCEPT + nomatch */
                return true;

        /* All other cases */
        return false;
}


/**
 * Establish a connection from proxenet -> server.
 * If proxy forwarding configured, then this function performs the negociation with the other proxy.
 * If the host applies for SSL intercept rules, this function also handles the SSL handshake.
 *
 * @return 0 if successful, -1 otherwise
 */
int create_http_socket(request_t* req, sock_t* server_sock, sock_t* client_sock, ssl_context_t* ssl_ctx)
{
        int retcode;
        char *host, *port;
        char sport[7] = {0, };
        http_infos_t* http_infos = &req->http_infos;
        bool use_proxy = (cfg->proxy.host != NULL);
        bool use_socks_proxy = use_proxy && (cfg->is_socks_proxy==true);
        bool use_http_proxy = use_proxy && (cfg->is_socks_proxy==false);
        char errmsg[512]={0,};

        if (parse_http_request(req) < 0){
                xlog(LOG_ERROR, "%s\n", "Failed to extract valid parameters from URL.");
                return -1;
        }

        /* ssl_ctx->use_ssl = req->is_ssl; */
        proxenet_xsnprintf(sport, sizeof(sport), "%hu", http_infos->port);

        /* do we forward to another proxy ? */
        if (use_proxy) {
                host = cfg->proxy.host;
                port = cfg->proxy.port;
        } else {
                host = http_infos->hostname;
                port = sport;
        }


#ifdef DEBUG
        xlog(LOG_DEBUG, "Relay request %s to '%s:%s (type=%d)'\n",
             use_http_proxy ? "via HTTP proxy" : "direct",
             host, port, http_infos->proto_type);
#endif

        retcode = proxenet_open_socket(host, port);
        if (retcode < 0) {
                proxenet_xsnprintf(errmsg, sizeof(errmsg), "Cannot connect to %s:%s<br><br>Reason: %s",
                                   host, port, errno?strerror(errno):"<i>proxenet_open_socket()</i> failed");

                generic_http_error_page(*server_sock, errmsg);
                return -1;
        }

        if (cfg->verbose > 2)
                xlog(LOG_INFO, "Socket to %s '%s:%s': fd=%d\n",
                     use_http_proxy?"HTTP proxy":(use_socks_proxy?"SOCKS4 proxy":"server"),
                     host, port, retcode);

        *client_sock = retcode;

        req->do_intercept = apply_intercept_rules(http_infos);

        if (cfg->verbose > 1) {
                xlog(LOG_INFO, "Server '%s' %s match filter '%s' with pattern '%s'\n",
                     http_infos->hostname,
                     req->do_intercept ? "do" : "do not",
                     cfg->intercept_mode==INTERCEPT_ONLY?"INTERCEPT_ONLY":"INTERCEPT_EXCEPT",
                     cfg->intercept_pattern);
        }


        if(use_socks_proxy){
                char*rhost = http_infos->hostname;
                int  rport = http_infos->port;
                retcode = proxenet_socks_connect(*client_sock, rhost, rport, true);
                if( retcode<0 ){
                        proxenet_xsnprintf(errmsg, sizeof(errmsg), "Failed to open SOCKS4 tunnel to %s:%s.\n",
                                           host, port);
                        generic_http_error_page(*server_sock, errmsg);
                        xlog(LOG_ERROR, "%s", errmsg);
                        return -1;
                }
        }

        /* set up specific sockets */
        if (strcmp(http_infos->method, "CONNECT")==0){
                /*
                 * We can receive a CONNECT if the connection uses
                 * - HTTPS
                 * - WebSocket/Secure WebSocket (https://tools.ietf.org/html/rfc6455#section-4.1)
                 *
                 * In both case, we need to continue the handshake to see if the traffic is encrypted
                 */
                if (proxenet_write(*server_sock, "HTTP/1.0 200 Connection established\r\n\r\n", 39) < 0){
                        return -1;
                }

                /*
                 * And then peek into the socket to determine if the data type if plaintext.
                 */
                char peek_read[4] = {0,};
                retcode = recv(*server_sock, peek_read, 3, MSG_PEEK);
                if(retcode<0){
                        xlog(LOG_ERROR, "recv() failed with ret=%d, reason: %s\n", retcode, strerror(errno));
                        return -1;
                }

                /*
                 * From WebSocket RFC (6455):
                 * "The method of the request MUST be GET, and the HTTP version MUST
                 * be at least 1.1."
                 *
                 * This means that if the next message from Web Browser does not start with GET,
                 * we can assume the connection is SSL/TLS (i.e. HTTPS or WSS).
                 */
                if(strcmp(peek_read, "GET")!=0){
                        ssl_ctx->use_ssl = req->is_ssl = true;
                        req->http_infos.proto_type = HTTPS;
                        return create_ssl_socket_with_interception(req, client_sock, server_sock, ssl_ctx);
                }
        }
        return 0;
}


/**
* Add old IE support (Compatibility View) for POST requests by forcing a 2nd read on the
* server socket, to make IE send the POST body.
* Tests revealed that this mode does not change the behaviour for recent browser. Therefore,
* it can be used all the time. Option -i allows to disable it.
*
* @return 0 if successful, -1 otherwise
*/
int get_http_request_body(sock_t sock, request_t* req, proxenet_ssl_context_t* sslctx)
{
        int nb;
        size_t old_len, body_len;
        char *body, *clen;

        /* to speed-up, disregard requests without body */
        if (strcmp(req->http_infos.method, "POST")!=0 && \
            strcmp(req->http_infos.method, "PUT")!=0)
                return 0;

        /* read Content-Length header */
        clen = get_header_by_name(req->data, "Content-Length: ");
        if (!clen){
                xlog(LOG_ERROR, "%s\n", "Extending IE POST: No Content-Length");
                return -1;
        }

        /* if Content-Length is zero */
        body_len = (size_t)atoi(clen);
        proxenet_xfree(clen);

        if (body_len==0){
                return 0;
        }

        /* if everything has already been sent (i.e. end of already received buffer is not CRLF*2) */
        if (req->data[ req->size-4 ] != '\r')
                return 0;
        if (req->data[ req->size-3 ] != '\n')
                return 0;
        if (req->data[ req->size-2 ] != '\r')
                return 0;
        if (req->data[ req->size-1 ] != '\n')
                return 0;

        /* read data (if any) */
        nb = proxenet_read_all(sock, &body, sslctx);
        if (nb<0){
                xlog(LOG_ERROR, "%s\n", "Extending IE POST: failed to read");
                return -1;
        }

        /* and extend the request buffer */
        if (nb>0){
                old_len = req->size;
                req->size = old_len + (size_t)nb;
                req->data = proxenet_xrealloc(req->data, req->size);
                memcpy(req->data + old_len, body, (size_t)nb);
        }

        proxenet_xfree(body);

#ifdef DEBUG
        xlog(LOG_DEBUG, "Extending request for IE: new_size=%d (content-length=%d)\n",
             req->size, body_len);
#endif

        return 0;
}


/**
 * If a GET request is received, check if it is part of a WebSocket handshake (as defined in
 * https://tools.ietf.org/html/rfc6455#section-4.2.1)
 *
 * @param req is a pointer to the request structure
 * @return 0 if no error occured, but the request is not a WS upgrade message
 * @return 1 if no error occured, and the request is a WS upgrade message
 * @return -1 if an error occured
 */
int prepare_websocket(request_t* req)
{
        int res = 0;
        char *upgrade_header, *connection_header;

        xlog(LOG_DEBUG,  "testing websocket for req #%d\n", req->id);

        connection_header = get_header_by_name(req->data, "Connection: ");
        upgrade_header = get_header_by_name(req->data, "Upgrade: ");

        if(connection_header && strcasestr(connection_header, "Upgrade") && \
           upgrade_header && strcasecmp(upgrade_header, "WebSocket")==0){

                xlog(LOG_INFO, "Upgrading request %d to WebSocket\n", req->id);

                /* And header the request status accordingly */
                if (req->is_ssl){
                        req->http_infos.proto_type = WSS;
                        req->http_infos.proto = WSS_STRING;
                } else {
                        req->http_infos.proto_type = WS;
                        req->http_infos.proto = WS_STRING;
                }

                update_http_infos_uri(req);

                res = 1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG,  "Processing WebSocket handshake for request #%d ('%s')\n",
             req->id, req->http_infos.uri);
#endif

        if(connection_header)
                proxenet_xfree(connection_header);
        if(upgrade_header)
                proxenet_xfree(upgrade_header);

        return res;
}
