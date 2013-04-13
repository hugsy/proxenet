#ifndef _SSL_H
#define _SSL_H

#include "socket.h"
#include "main.h"

#ifdef _USE_POLARSSL
#include <polarssl/ssl.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>

typedef ssl_context proxenet_ssl_session_t;
typedef x509_cert proxenet_ssl_cert_t;

typedef struct __ssl_atom_t {
		ssl_context context;
		x509_cert   cert;
		ctr_drbg_context ctr_drbg;
		entropy_context entropy;
		boolean is_valid;
} ssl_atom_t;

typedef struct __ssl_context_t {
		ssl_atom_t client;
		ssl_atom_t server;
} ssl_context_t;

#else
/* GnuTLS */
#include <gnutls/gnutls.h>

typedef gnutls_session_t proxenet_ssl_session_t;
typedef gnutls_certificate_credentials_t proxenet_ssl_cert_t ;
typedef struct __ssl_context_t {
		gnutls_session_t cli;
		gnutls_session_t srv;
		gnutls_certificate_credentials_t srv_creds;
		gnutls_certificate_credentials_t cli_creds;
		gnutls_dh_params_t dh_params;		
} ssl_context_t;
#endif


int proxenet_ssl_init_global_context();
int proxenet_ssl_free_global_context();

int proxenet_ssl_init_server_context(ssl_atom_t* server);
int proxenet_ssl_init_client_context(ssl_atom_t* client);

void proxenet_ssl_wrap_socket(proxenet_ssl_session_t s, sock_t sock);
int proxenet_ssl_handshake(proxenet_ssl_session_t s);

void proxenet_ssl_bye(proxenet_ssl_session_t *s);
int close_socket_ssl(sock_t sock, proxenet_ssl_session_t *s);

ssize_t proxenet_ssl_read(sock_t sock, void *n, size_t l, proxenet_ssl_session_t *s);
ssize_t proxenet_ssl_write(sock_t sock, void *n, size_t l, proxenet_ssl_session_t *s) ;
#endif /* _SSL_H */
