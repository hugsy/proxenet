#ifndef _SSL_H
#define _SSL_H

#include "socket.h"
#include "main.h"
#include "utils.h"

#include <polarssl/ssl.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>

typedef ssl_context proxenet_ssl_context_t;
typedef x509_cert proxenet_ssl_cert_t;

typedef struct __ssl_atom_t {
		ssl_context context;
		x509_cert   cert;
		ctr_drbg_context ctr_drbg;
		entropy_context entropy;
		rsa_context rsa;
		bool is_valid;
} ssl_atom_t;

typedef struct __ssl_context_t {
		bool use_ssl;
		ssl_atom_t client;
		ssl_atom_t server;
} ssl_context_t;



void proxenet_ssl_free_certificate(proxenet_ssl_cert_t* ssl_cert);
	
int proxenet_ssl_init_server_context(ssl_atom_t* server);
int proxenet_ssl_init_client_context(ssl_atom_t* client);

void proxenet_ssl_wrap_socket(proxenet_ssl_context_t* s, sock_t* sock);
int proxenet_ssl_handshake(proxenet_ssl_context_t* s);

void proxenet_ssl_bye(proxenet_ssl_context_t *s);
int close_socket_ssl(sock_t sock, proxenet_ssl_context_t *s);

ssize_t proxenet_ssl_read(sock_t sock, void *n, size_t l, proxenet_ssl_context_t *s);
ssize_t proxenet_ssl_write(sock_t sock, void *n, size_t l, proxenet_ssl_context_t *s) ;
#endif /* _SSL_H */
