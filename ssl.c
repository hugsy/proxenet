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
#include "minica.h"


/*
 * SSL context setup
 */

/*
 * Compatible with PolarSSL 1.3+ API
 */




#ifdef DEBUG_SSL

static char errbuf[4096] = {0, };

#include <polarssl/debug.h>

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
static inline int _proxenet_ssl_init_context(ssl_atom_t* ssl_atom, int type, char* hostname)
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
                if (proxenet_lookup_crt(hostname, &certfile) < 0){
                        xlog(LOG_ERROR, "proxenet_lookup_crt() failed for '%s'\n", hostname);
                        return -1;
                }

                keyfile = cfg->certskey;
                keyfile_pwd = cfg->certskey_pwd;

                if(cfg->verbose > 1)
                        xlog(LOG_INFO, "Using Server CRT '%s' (key='%s', pwd='%s') \n",
                             certfile, keyfile, keyfile_pwd);
        }

        else if(use_ssl_client_auth){
                certfile = cfg->sslcli_certfile;
                keyfile = cfg->sslcli_keyfile;
                keyfile_pwd = cfg->sslcli_keyfile_pwd;
                domain = cfg->sslcli_domain;

                if(cfg->verbose > 1)
                        xlog(LOG_DEBUG,
                             "Configuring SSL client certificate:\n\tcert='%s'\n\tkey='%s'\n\tdomain='%s'\n",
                             certfile, keyfile, domain);
        }


        /* init entropy */
        entropy_init( &(ssl_atom->entropy) );

        /* init rng */
        retcode = ctr_drbg_init( &(ssl_atom->ctr_drbg), entropy_func, &(ssl_atom->entropy), NULL, 0);
        if( retcode != 0 ) {
                xlog(LOG_ERROR, "ctr_drbg_init returned %d\n", retcode);
                retcode = -1;
                goto end_init;
        }


        x509_crt_init( &(ssl_atom->cert) );
        if (type==SSL_IS_SERVER || use_ssl_client_auth){
                /* checking ssl_atom certificate */
                retcode = x509_crt_parse_file(&(ssl_atom->cert), certfile);
                if(retcode) {
                        error_strerror(retcode, ssl_error_buffer, 127);
                        xlog(LOG_ERROR, "Failed to parse %s certificate '%s': %s\n",
                             type_str, certfile, ssl_error_buffer);
                        retcode = -1;
                        goto end_init;
                }

                retcode = x509_crt_parse_file(&(ssl_atom->ca), cfg->cafile);
                if(retcode) {
                        error_strerror(retcode, ssl_error_buffer, 127);
                        xlog(LOG_ERROR, "Failed to parse %s CA certificate '%s': %s\n",
                             type_str, cfg->cafile, ssl_error_buffer);
                        retcode = -1;
                        goto end_init;
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
                        xlog(LOG_ERROR, "Failed to parse key '%s' (pwd='%s'): %s\n", keyfile, keyfile_pwd, ssl_error_buffer);
                        retcode = -1;
                        goto end_init;
                }
#ifdef DEBUG
                xlog(LOG_DEBUG, "Loaded %s private key '%s'\n", type_str, keyfile);
#endif
        }

        /* init ssl context */
        if (ssl_init(context) != 0) {
                retcode = -1;
                goto end_init;
        }

        ssl_set_endpoint(context, type);
        ssl_set_rng(context, ctr_drbg_random, &(ssl_atom->ctr_drbg) );

        switch(type) {
                case SSL_IS_SERVER:
                        ssl_set_ca_chain(context, &(ssl_atom->ca), NULL, hostname);
                        ssl_set_own_cert(context, &(ssl_atom->cert), &(ssl_atom->pkey));
                        ssl_set_authmode(context, SSL_VERIFY_NONE);
                        ssl_set_min_version(context, SSL_MAJOR_VERSION_3, SSL_MINOR_VERSION_1); // TLSv1.0+
                        break;

                case SSL_IS_CLIENT:
                        ssl_set_ca_chain(context, &(ssl_atom->cert), NULL, NULL);
                        ssl_set_authmode(context, SSL_VERIFY_NONE);
                        if(use_ssl_client_auth){
                                ssl_set_hostname( context, domain );
                                ssl_set_own_cert(context, &(ssl_atom->cert), &(ssl_atom->pkey));
                        } else {
                                ssl_set_hostname( context, hostname );
                        }

                        break;

                default:
                        retcode = -1;
                        goto end_init;
        }


#ifdef DEBUG_SSL
        ssl_set_dbg(context, proxenet_ssl_debug, stderr);
#endif

        ssl_atom->is_valid = true;
        retcode = 0;

end_init:
        if (type==SSL_IS_SERVER)
                proxenet_xfree(certfile);

        return retcode;
}


/**
 *
 */
int proxenet_ssl_init_server_context(ssl_atom_t *server, char* hostname)
{
        return _proxenet_ssl_init_context(server, SSL_IS_SERVER, hostname);
}


/**
 *
 */
int proxenet_ssl_init_client_context(ssl_atom_t* client, char* hostname)
{
        return _proxenet_ssl_init_context(client, SSL_IS_CLIENT, hostname);
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
        char ssl_strerror[4096];

        do {
                retcode = ssl_handshake( ctx );
                if(retcode == 0)
                        break;

                if(retcode!=POLARSSL_ERR_NET_WANT_READ && \
                   retcode!=POLARSSL_ERR_NET_WANT_WRITE) {
                        xlog(LOG_ERROR, "SSL handshake failed (returns %#x)\n", -retcode);
                        break;
                }

        } while( true );


        if (cfg->verbose){
                proxenet_xzero(ssl_strerror, sizeof(ssl_strerror));

                if (retcode){
                        snprintf(ssl_strerror, sizeof(ssl_strerror)-1,
                                 "SSL Handshake: "RED"fail"NOCOLOR" [%d]",
                                 retcode);
                        xlog(LOG_ERROR, "%s\n", ssl_strerror);

                        if (cfg->verbose > 1){
                                proxenet_xzero(ssl_strerror, sizeof(ssl_strerror)-1);
                                polarssl_strerror(retcode, ssl_strerror, sizeof(ssl_strerror));
                                xlog(LOG_ERROR, "Details: %s\n", ssl_strerror);
                        }
                }
                else
                {
                        snprintf(ssl_strerror, sizeof(ssl_strerror)-1,
                                 "SSL Handshake: "GREEN"success"NOCOLOR" [proto='%s',cipher='%s']",
                                 ssl_get_version( ctx ),
                                 ssl_get_ciphersuite( ctx ) );
                        xlog(LOG_INFO, "%s\n", ssl_strerror);

                        if (cfg->verbose>1){
                                if(ssl_get_peer_cert( ctx ) != NULL){
                                        proxenet_xzero(ssl_strerror, sizeof(ssl_strerror));
                                        x509_crt_info( ssl_strerror, sizeof(ssl_strerror)-1,
                                                       "      ", ssl_get_peer_cert( ctx ) );
                                        xlog(LOG_INFO, "Peer SSL certificate info:\n%s", ssl_strerror);
                                }
                        }
                }
        }


#ifdef DEBUG_SSL
        if(retcode == 0) {
                int ret;
                proxenet_xzero(errbuf, sizeof(errbuf));

                /* check certificate */
                proxenet_xzero(errbuf, sizeof(errbuf));
                strncat(errbuf, "Verify X509 cert: ", sizeof(errbuf)-strlen(errbuf)-1);
                ret = ssl_get_verify_result( ctx );
                if( ret != 0 ) {
                        snprintf(errbuf+strlen(errbuf), sizeof(errbuf)-strlen(errbuf)-1, RED"failed"NOCOLOR" [%d]\n", ret);
                        if( ret & BADCERT_EXPIRED )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" certificate expired\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_REVOKED )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" certificate revoked\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_CN_MISMATCH )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" CN mismatch\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_NOT_TRUSTED )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" self-signed or not signed by a trusted CA\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_MISSING )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" certificate missing\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_SKIP_VERIFY )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" certificate check is skipped\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_OTHER )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" other reason\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCERT_FUTURE )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" certificate validity is in the future\n", sizeof(errbuf)-strlen(errbuf)-1);

                        if( ret & BADCRL_EXPIRED )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" CRL expired\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCRL_NOT_TRUSTED )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" CRL is not correctly signed by trusted CA\n", sizeof(errbuf)-strlen(errbuf)-1);
                        if( ret & BADCRL_FUTURE )
                                strncat(errbuf, RED"\t[-]"NOCOLOR" CRL validity is in the future\n", sizeof(errbuf)-strlen(errbuf)-1);

                } else {
                        strncat(errbuf, GREEN"ok\n"NOCOLOR, sizeof(errbuf)-strlen(errbuf)-1);
                }
                xlog(LOG_DEBUG, "%s", errbuf);
        }
#endif

        return retcode;
}


/**
 *
 */
void proxenet_ssl_free_structs(ssl_atom_t* ssl)
{
        x509_crt_free( &ssl->cert );
        x509_crt_free( &ssl->ca );
        ctr_drbg_free( &ssl->ctr_drbg );
        entropy_free( &ssl->entropy );
	rsa_free(&ssl->rsa);
        pk_free( &ssl->pkey );
        ssl_free( &ssl->context );

        ssl->is_valid = false;

        return;
}


/**
 *
 */
void proxenet_ssl_finish(ssl_atom_t* ssl)
{
        ssl_close_notify( &ssl->context );
	return;
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

                if (retcode == POLARSSL_ERR_NET_WANT_READ ||\
                    retcode == POLARSSL_ERR_NET_WANT_WRITE )
                        continue;

                if (retcode>0)
                        break;

                if (retcode <= 0) {
                        char ssl_strerror[256] = {0, };

                        switch(retcode) {
                                case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY :
#ifdef DEBUG
                                        /* acceptable case */
                                        xlog(LOG_DEBUG, "%s\n", "SSL close notify received");
#endif
                                        return 0;

                                case POLARSSL_ERR_NET_CONN_RESET:
                                        xlog(LOG_ERROR, "%s\n", "SSL connection reset by peer");
                                        return -1;

                                case 0:
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
