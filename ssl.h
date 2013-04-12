#ifndef _SSL_H
#define _SSL_H

#include <gnutls/gnutls.h>

typedef struct __ssl_context_t 
{
	  gnutls_session_t cli;
	  gnutls_session_t srv;
	  gnutls_certificate_credentials_t srv_credz;
	  gnutls_certificate_credentials_t cli_credz;
	  gnutls_dh_params_t dh_params;
} ssl_ctx_t;

int proxenet_ssl_init_global_context();
void proxenet_ssl_free_global_context();

gnutls_session_t proxenet_ssl_init_server_context(ssl_ctx_t* ctx);
gnutls_session_t proxenet_ssl_init_client_context(ssl_ctx_t* ctx);
void proxenet_ssl_wrap_socket(gnutls_session_t, sock_t);
int proxenet_ssl_handshake(gnutls_session_t);

#endif /* _SSL_H */
