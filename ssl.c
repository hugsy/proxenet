#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include <polarssl/config.h>
#include <polarssl/net.h>
#include <polarssl/ssl.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/error.h>
#include <polarssl/certs.h>

#include "socket.h"
#include "utils.h"
#include "main.h"
#include "ssl.h"


/*
 * SSL context setup
 */


#ifdef DEBUG_SSL

static char buf[2048] = {0, };


/**
 *
 */
void proxenet_ssl_debug(void *who, int level, const char *str )
{
	size_t l = strlen(str);
	size_t k = strlen(buf);

	strncpy(buf+k, str, l);
	
	if (str[l-1] == '\n') {
		xlog(LOG_DEBUG, "%s - %s", (char*)who, buf);
		xzero(buf, 2048);
	}
}

#endif


/**
 *
 */
int proxenet_ssl_init_server_context(ssl_atom_t *server)
{
	int retcode = -1;
	char ssl_error_buffer[128] = {0, };
	proxenet_ssl_context_t *context = &(server->context);
	
	entropy_init( &(server->entropy) );

	/* init rng */
	retcode = ctr_drbg_init( &(server->ctr_drbg), entropy_func, &(server->entropy),
				 (const unsigned char*)PROGNAME, strlen(PROGNAME));
	if( retcode != 0 ) {
		error_strerror(retcode, ssl_error_buffer, 127);
		xlog(LOG_ERROR, "ctr_drbg_init returned %d\n", retcode);
		return -1;
	}
	
	/* checking certificate */
	retcode = x509parse_crtfile(&(server->cert), cfg->certfile);
	if(retcode) {
		error_strerror(retcode, ssl_error_buffer, 127);
		xlog(LOG_CRITICAL, "Failed to parse certificate: %s\n", ssl_error_buffer);
		return -1;
	}
	
	/* checking private key */
	rsa_init(&(server->rsa), RSA_PKCS_V15, 0);
	retcode = x509parse_keyfile(&(server->rsa), cfg->keyfile, NULL);
	if(retcode) {
		error_strerror(retcode, ssl_error_buffer, 127);
		xlog(LOG_CRITICAL, "Failed to parse key: %s\n", ssl_error_buffer);
		return -1;
	}

	/* init server context */
	if (ssl_init(context) != 0)
		return -1;

	ssl_set_endpoint(context, SSL_IS_SERVER );
	ssl_set_authmode(context, SSL_VERIFY_NONE );
	ssl_set_rng(context, ctr_drbg_random, &(server->ctr_drbg) );
	ssl_set_ca_chain(context, server->cert.next, NULL, NULL );
	ssl_set_own_cert(context, &(server->cert), &(server->rsa));

#ifdef DEBUG_SSL
	ssl_set_dbg(context, proxenet_ssl_debug, "SERVER");
#endif
	server->is_valid = TRUE;
	
	return 0;
}


/**
 *
 */
int proxenet_ssl_init_client_context(ssl_atom_t* client)
{
	int retcode = -1;
	proxenet_ssl_context_t *context = &(client->context);
	
	entropy_init( &(client->entropy) );
	
	/* init rng */
	retcode = ctr_drbg_init( &(client->ctr_drbg), entropy_func, &(client->entropy),
				 NULL, 0);
	if( retcode != 0 ) {
		xlog(LOG_ERROR, "ctr_drbg_init returned %d\n", retcode);
		return -1;
	}

	/* init ssl context */
	if (ssl_init(context) != 0)
		return -1;
	
	ssl_set_endpoint(context, SSL_IS_CLIENT );
	ssl_set_authmode(context, SSL_VERIFY_OPTIONAL );
	ssl_set_rng(context, ctr_drbg_random, &(client->ctr_drbg) );
	ssl_set_ca_chain(context, &(client->cert), NULL, NULL);

#ifdef DEBUG_SSL
	ssl_set_dbg(context, proxenet_ssl_debug, "CLIENT");
#endif
	
	client->is_valid = TRUE;
	
	return 0;
} 



/**
 *
 */
void proxenet_ssl_wrap_socket(proxenet_ssl_context_t* ctx, sock_t* sock) 
{
	ssl_set_bio(ctx, net_recv, sock, net_send, sock );
}


/**
 *
 */
int proxenet_ssl_handshake(proxenet_ssl_context_t* ctx)
{
	int retcode = -1;

	do {
		retcode = ssl_handshake( ctx );
		if (retcode == 0)
			break;
		
		if(retcode!=POLARSSL_ERR_NET_WANT_READ && retcode!=POLARSSL_ERR_NET_WANT_WRITE) {
			char ssl_strerror[128] = {0, };
			error_strerror(retcode, ssl_strerror, 127);
			xlog(LOG_ERROR, "SSL handshake failed, %#x: %s\n",
			     -retcode, ssl_strerror);
			break;
		}
		
	} while( retcode != 0 );

	return retcode;	
}


/**
 *
 */
void proxenet_ssl_free_certificate(proxenet_ssl_cert_t* ssl_cert)
{
	x509_free( ssl_cert );
}


/**
 *
 */
void proxenet_ssl_bye(proxenet_ssl_context_t* ssl)
{
	ssl_close_notify( ssl );
}
			     
			     
/**
 *
 */
int close_socket_ssl(sock_t sock, proxenet_ssl_context_t* ssl)
{
	int ret;

	ret = close_socket(sock);
	ssl_free( ssl );
	
	return ret;
}  


/*
 * SSL I/O
 */

/**
 *
 */
ssize_t proxenet_ssl_ioctl(int (*func)(), void *buf, size_t count, proxenet_ssl_context_t* ssl) {
	int retcode = -1;

	do {
		retcode = (*func)(ssl, buf, count);

		if (retcode > 0)
			break;

		if(retcode == POLARSSL_ERR_NET_WANT_READ ||\
		   retcode == POLARSSL_ERR_NET_WANT_WRITE )
			continue;

		if (retcode <= 0) {
			char ssl_strerror[128] = {0, };
			error_strerror(retcode, ssl_strerror, 127);
			xlog(LOG_ERROR, "SSL I/O got %#x: %s\n", -retcode, ssl_strerror);
			return -1;
		}
		
	} while (TRUE);
	
	return retcode;	
}


/**
 *
 */
ssize_t proxenet_ssl_read(sock_t sock, void *buf, size_t count, proxenet_ssl_context_t* ssl) 
{
	int (*func)() = &ssl_read;
	int ret = -1;
	
	ret = proxenet_ssl_ioctl(func, buf, count, ssl);
	if (ret<0) {
		xlog(LOG_ERROR, "%s\n", "Error while reading SSL stream");
		return ret;
	}

	/* fixme : 1st read = 1 byte recvd (bug from FF ?) */
	if (ret==1)
		return proxenet_ssl_ioctl(func, buf+1, count-1, ssl) + 1;
	else
		return ret;
}


/**
 *
 */
ssize_t proxenet_ssl_write(sock_t sock, void *buf, size_t count, proxenet_ssl_context_t* ssl_sess) 
{
	int (*func)() = &ssl_write;
	int ret = -1;

	ret = proxenet_ssl_ioctl(func, buf, count, ssl_sess);
	if (ret < 0)
		xlog(LOG_ERROR, "%s\n", "Error while writing SSL stream");
	
	return ret;
}

