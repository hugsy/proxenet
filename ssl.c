#ifdef HAVE_CONFIG_H
#include "config.h"
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

/*
 * Compatible with PolarSSL 1.3+ API
 */


static char errbuf[2048] = {0, };


#ifdef DEBUG_SSL
/**
 *
 */
static void proxenet_ssl_debug(void *who, int level, const char *str )
{
	size_t l = strlen(str);
	size_t k = strlen(errbuf);

	strncpy(errbuf+k, str, l);

	if (str[l-1] == '\n') {
		xlog(LOG_DEBUG, "%s[%d] - %s", (char*)who, level, errbuf);
		proxenet_xzero(errbuf, 2048);
	}
}

#endif


/**
 *
 */
static inline int _proxenet_ssl_init_context(ssl_atom_t* ssl_atom, int type)
{
	int retcode = -1;
        char ssl_error_buffer[128] = {0, };
	proxenet_ssl_context_t *context = &(ssl_atom->context);
        char *certfile, *keyfile, *keyfile_pwd, *domain;
        const char* type_str = (type==SSL_IS_CLIENT)?"CLIENT":"SERVER";
	bool use_ssl_client_auth = (type==SSL_IS_CLIENT && cfg->sslcli_certfile && cfg->sslcli_keyfile)?true:false;

        certfile = keyfile = keyfile_pwd = domain = NULL;


        /* We only define a certificate if we're a server, or the user requested SSL cert auth */
        if (type==SSL_IS_SERVER) {
                certfile = cfg->certfile;
                keyfile = cfg->keyfile;
                keyfile_pwd = cfg->keyfile_pwd;
        } else if(use_ssl_client_auth){
                certfile = cfg->sslcli_certfile;
                keyfile = cfg->sslcli_keyfile;
                keyfile_pwd = cfg->sslcli_keyfile_pwd;
                domain = cfg->sslcli_domain;
#ifdef DEBUG
                xlog(LOG_DEBUG, "Configuring SSL client cert='%s' key='%s' domain='%s'\n",
                     certfile,
                     keyfile,
                     domain);
#endif
        }


	entropy_init( &(ssl_atom->entropy) );

	/* init rng */
	retcode = ctr_drbg_init( &(ssl_atom->ctr_drbg), entropy_func, &(ssl_atom->entropy), NULL, 0);
	if( retcode != 0 ) {
		xlog(LOG_ERROR, "ctr_drbg_init returned %d\n", retcode);
		return -1;
	}


        if (type==SSL_IS_SERVER || use_ssl_client_auth){
                /* checking ssl_atom certificate */
                x509_crt_init( &(ssl_atom->cert) );
                retcode = x509_crt_parse_file(&(ssl_atom->cert), certfile);
                if(retcode) {
                        error_strerror(retcode, ssl_error_buffer, 127);
                        xlog(LOG_CRITICAL, "Failed to parse %s certificate: %s\n", type_str, ssl_error_buffer);
                        return -1;
                }

#ifdef DEBUG
                xlog(LOG_DEBUG, "Parsed %s certificate '%s'\n", type_str, certfile);
#endif
#ifdef DEBUG_SSL
                proxenet_xzero(errbuf, sizeof(errbuf));
                retcode = x509_crt_info( errbuf, sizeof(errbuf)-1, "\t", &(ssl_atom->cert) );
                if(retcode < 0){
                        xlog(LOG_DEBUG, "Failed to get %s certificate information : %d\n", type_str, retcode);
                } else {
                        xlog(LOG_DEBUG, "Certificate '%s' information:\n%s\n", certfile, errbuf);
                }
#endif

                /* checking private key */
                rsa_init(&(ssl_atom->rsa), RSA_PKCS_V15, 0);
                pk_init( &(ssl_atom->pkey) );
                retcode = pk_parse_keyfile(&(ssl_atom->pkey), keyfile, keyfile_pwd);
                if(retcode) {
                        error_strerror(retcode, ssl_error_buffer, 127);
                        rsa_free(&(ssl_atom->rsa));
                        xlog(LOG_CRITICAL, "Failed to parse key: %s\n", ssl_error_buffer);
                        return -1;
                }
#ifdef DEBUG
                xlog(LOG_DEBUG, "Loaded %s private key '%s'\n", type_str, keyfile);
#endif
        }

	/* init ssl context */
	if (ssl_init(context) != 0)
		return -1;

	ssl_set_endpoint(context, type );
	ssl_set_authmode(context, SSL_VERIFY_OPTIONAL );
	ssl_set_rng(context, ctr_drbg_random, &(ssl_atom->ctr_drbg) );

        if (type==SSL_IS_SERVER || use_ssl_client_auth){
                ssl_set_ca_chain(context, &(ssl_atom->cert), NULL, NULL);
                ssl_set_own_cert(context, &(ssl_atom->cert), &(ssl_atom->pkey));
        }

#ifdef DEBUG_SSL
        ssl_set_dbg(context, proxenet_ssl_debug, stderr);
#endif

	ssl_atom->is_valid = true;

	return 0;
}


/**
 *
 */
int proxenet_ssl_init_server_context(ssl_atom_t *server)
{
	return _proxenet_ssl_init_context(server, SSL_IS_SERVER);
}


/**
 *
 */
int proxenet_ssl_init_client_context(ssl_atom_t* client)
{
	return _proxenet_ssl_init_context(client, SSL_IS_CLIENT);
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
			xlog(LOG_ERROR, "SSL handshake failed (returns %#x): %s\n",
			     -retcode, ssl_strerror);
			break;
		}

	} while( retcode != 0 );


#ifdef DEBUG
        proxenet_xzero(errbuf, sizeof(errbuf));
        strncat(errbuf, "SSL Handshake: ", sizeof(errbuf)-strlen(errbuf)-1);
        if (retcode) {
                snprintf(errbuf+strlen(errbuf), sizeof(errbuf)-strlen(errbuf)-1,
                         RED"fail"NOCOLOR" [%d]",
                         retcode);
        } else {
                snprintf(errbuf+strlen(errbuf), sizeof(errbuf)-strlen(errbuf)-1,
                         GREEN"success"NOCOLOR" [proto='%s',cipher='%s']",
                         ssl_get_version( ctx ),
                         ssl_get_ciphersuite( ctx ) );
        }
        xlog(LOG_DEBUG, "%s\n", errbuf);
#endif

#ifdef DEBUG_SSL
        proxenet_xzero(errbuf, sizeof(errbuf));

        /* check certificate */
        proxenet_xzero(errbuf, sizeof(errbuf));
        strncat(errbuf, "Verify X509 cert: ", sizeof(errbuf)-strlen(errbuf)-1);
        retcode = ssl_get_verify_result( ctx );
        if( retcode != 0 ) {
                snprintf(errbuf+strlen(errbuf), sizeof(errbuf)-strlen(errbuf)-1, RED"failed"NOCOLOR" [%d]\n", retcode);
                if( retcode & BADCERT_EXPIRED )
                        strncat(errbuf, "\t[-] certificate expired\n", sizeof(errbuf)-strlen(errbuf)-1);
                if( retcode & BADCERT_REVOKED )
                        strncat(errbuf, "\t[-] certificate revoked\n", sizeof(errbuf)-strlen(errbuf)-1);
                if( retcode & BADCERT_CN_MISMATCH )
                        strncat(errbuf, "\t[-] CN mismatch\n", sizeof(errbuf)-strlen(errbuf)-1);
                if( retcode & BADCERT_NOT_TRUSTED )
                        strncat(errbuf, "\t[-] self-signed or not signed by a trusted CA\n", sizeof(errbuf)-strlen(errbuf)-1);
        } else {
                strncat(errbuf, GREEN"ok\n"NOCOLOR, sizeof(errbuf)-strlen(errbuf)-1);
        }
        xlog(LOG_DEBUG, "%s", errbuf);

#endif
	return retcode;
}


/**
 *
 */
static void proxenet_ssl_free_certificate(proxenet_ssl_cert_t* ssl_cert)
{
	x509_crt_free( ssl_cert );
}


/**
 *
 */
static void proxenet_ssl_bye(proxenet_ssl_context_t* ssl)
{
	ssl_close_notify( ssl );
}


/**
 *
 */
void proxenet_ssl_finish(ssl_atom_t* ssl, bool is_server)
{
	rsa_free(&ssl->rsa);
	proxenet_ssl_bye(&ssl->context);
	proxenet_ssl_free_certificate(&ssl->cert);
	if (is_server)
		pk_free( &ssl->pkey );
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
static ssize_t proxenet_ssl_ioctl(int (*func)(), void *buf, size_t count, proxenet_ssl_context_t* ssl) {
	int retcode = -1;

	do {
		retcode = (*func)(ssl, buf, count);

		if (retcode >= 0)
			break;

		if(retcode == POLARSSL_ERR_NET_WANT_READ ||\
		   retcode == POLARSSL_ERR_NET_WANT_WRITE )
			continue;

		if (retcode < 0) {
			char ssl_strerror[128] = {0, };

			switch(retcode) {
				case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY :
					/* acceptable case */
					return 0;

				default:
					error_strerror(retcode, ssl_strerror, 127);
					xlog(LOG_ERROR, "SSL I/O got %#x: %s\n", -retcode, ssl_strerror);
					return -1;
			}
		}

	} while (true);

	return retcode;
}


/**
 *
 */
ssize_t proxenet_ssl_read(proxenet_ssl_context_t* ssl, void *buf, size_t count)
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
ssize_t proxenet_ssl_write(proxenet_ssl_context_t* ssl, void *buf, size_t count)
{
	int (*func)() = &ssl_write;
	int ret = -1;

	ret = proxenet_ssl_ioctl(func, buf, count, ssl);
	if (ret < 0)
		xlog(LOG_ERROR, "%s\n", "Error while writing SSL stream");

	return ret;
}
