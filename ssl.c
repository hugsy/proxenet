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

#include <mbedtls/config.h>
#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>

#include "socket.h"
#include "utils.h"
#include "main.h"
#include "ssl.h"
#include "minica.h"


/*
 * SSL context setup
 */

/*
 * Compatible with mbedTLS 2.x API
 */




#ifdef DEBUG_SSL

static char errbuf[4096] = {0, };

#include <mbedtls/debug.h>

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
static int ssl_init_context(ssl_atom_t* ssl_atom, int type, char* hostname)
{
        int retcode = -1;
        char ssl_error_buffer[128] = {0, };
        proxenet_ssl_context_t *context = &(ssl_atom->context);
        mbedtls_ssl_config *conf = &(ssl_atom->conf);
        char *certfile, *keyfile, *keyfile_pwd, *domain;
        const char* type_str = (type==MBEDTLS_SSL_IS_CLIENT)?"CLIENT":"SERVER";
        bool use_ssl_client_auth = (type==MBEDTLS_SSL_IS_CLIENT && cfg->sslcli_certfile && cfg->sslcli_keyfile)?true:false;

        certfile = keyfile = keyfile_pwd = domain = NULL;


        /* We only define a certificate if we're a server, or the user requested SSL cert auth */
        if (type==MBEDTLS_SSL_IS_SERVER) {
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


        /* init rng */
        mbedtls_entropy_init( &(ssl_atom->entropy) );
        mbedtls_ctr_drbg_init( &(ssl_atom->ctr_drbg) );

        /* seed pool from rng */
        retcode = mbedtls_ctr_drbg_seed( &(ssl_atom->ctr_drbg), mbedtls_entropy_func,
                                         &(ssl_atom->entropy), NULL, 0);
        if(retcode){
                xlog(LOG_ERROR, "failed to seed the rng, retcode=%#x\n", retcode);
                goto end_init;
        }

        mbedtls_x509_crt_init( &(ssl_atom->cert) );
        if (type==MBEDTLS_SSL_IS_SERVER || use_ssl_client_auth){
                /* checking ssl_atom certificate */
                retcode = mbedtls_x509_crt_parse_file(&(ssl_atom->cert), certfile);
                if(retcode) {
                        mbedtls_strerror(retcode, ssl_error_buffer, 127);
                        xlog(LOG_ERROR, "Failed to parse %s certificate '%s': %s\n",
                             type_str, certfile, ssl_error_buffer);
                        retcode = -1;
                        goto end_init;
                }

                retcode = mbedtls_x509_crt_parse_file(&(ssl_atom->ca), cfg->cafile);
                if(retcode) {
                        mbedtls_strerror(retcode, ssl_error_buffer, 127);
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
                mbedtls_rsa_init(&(ssl_atom->rsa), MBEDTLS_RSA_PKCS_V15, 0);
                mbedtls_pk_init( &(ssl_atom->pkey) );
                retcode = mbedtls_pk_parse_keyfile(&(ssl_atom->pkey), keyfile, keyfile_pwd);
                if(retcode) {
                        mbedtls_strerror(retcode, ssl_error_buffer, 127);
                        mbedtls_rsa_free(&(ssl_atom->rsa));
                        xlog(LOG_ERROR, "Failed to parse key '%s' (pwd='%s'): %s\n", keyfile, keyfile_pwd, ssl_error_buffer);
                        retcode = -1;
                        goto end_init;
                }
#ifdef DEBUG
                xlog(LOG_DEBUG, "Loaded %s private key '%s'\n", type_str, keyfile);
#endif
        }

        /* init ssl context */
        mbedtls_ssl_init(context);
        mbedtls_ssl_config_init(conf);
        mbedtls_ssl_config_defaults(conf, type, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &(ssl_atom->ctr_drbg) );

        switch(type) {
                case MBEDTLS_SSL_IS_SERVER:
                        mbedtls_ssl_conf_ca_chain(conf, &(ssl_atom->ca), NULL);
                        mbedtls_ssl_conf_own_cert(conf, &(ssl_atom->cert), &(ssl_atom->pkey));
                        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
                        mbedtls_ssl_conf_min_version(conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_1); // TLSv1.0+
                        break;

                case MBEDTLS_SSL_IS_CLIENT:
                        mbedtls_ssl_conf_ca_chain(conf, &(ssl_atom->cert), NULL);
                        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
                        if(use_ssl_client_auth){
                                mbedtls_ssl_set_hostname(context, domain);
                                mbedtls_ssl_conf_own_cert(conf, &(ssl_atom->cert), &(ssl_atom->pkey));
                        } else {
                                mbedtls_ssl_set_hostname(context, hostname);
                        }
                        break;

                default:
                        retcode = -1;
                        goto end_init;
        }

        retcode = mbedtls_ssl_setup(context, conf);
        if(retcode < 0){
                xlog(LOG_ERROR, "failed to setup mbedtls session: retcode=%#x\n", retcode);
                goto end_init;
        }


#ifdef DEBUG_SSL
        mbedtls_ssl_conf_dbg(context, proxenet_ssl_debug, stderr);
#endif

        ssl_atom->is_valid = true;
        retcode = 0;

end_init:
        if (type==MBEDTLS_SSL_IS_SERVER)
                proxenet_xfree(certfile);

        return retcode;
}


/**
 *
 */
int proxenet_ssl_init_server_context(ssl_atom_t *server, char* hostname)
{
        return ssl_init_context(server, MBEDTLS_SSL_IS_SERVER, hostname);
}


/**
 *
 */
int proxenet_ssl_init_client_context(ssl_atom_t* client, char* hostname)
{
        return ssl_init_context(client, MBEDTLS_SSL_IS_CLIENT, hostname);
}


/**
 *
 */
void proxenet_ssl_wrap_socket(proxenet_ssl_context_t* ctx, sock_t* sock)
{
        mbedtls_ssl_set_bio(ctx, sock, mbedtls_net_send, mbedtls_net_recv, NULL);
}


/**
 *
 */
int proxenet_ssl_handshake(proxenet_ssl_context_t* ctx)
{
        int retcode = -1;
        char ssl_strerror[4096];

        do {
                retcode = mbedtls_ssl_handshake( ctx );
                if(retcode == 0)
                        break;

                if(retcode!=MBEDTLS_ERR_SSL_WANT_READ && \
                   retcode!=MBEDTLS_ERR_SSL_WANT_WRITE) {

                        if (retcode == MBEDTLS_ERR_NET_CONN_RESET)
                                xlog(LOG_WARNING,
                                     "Peer has reset the SSL connection during handshake (error "
                                     "%#x).To remove this warning, make sure to add proxenet "
                                     "CA certificate as a Trusted CA for websites.\n",
                                     -retcode);
                        else
                                xlog(LOG_ERROR, "SSL handshake failed (returns %#x)\n", -retcode);
                        break;
                }

        } while( true );


        if (cfg->verbose){
                proxenet_xzero(ssl_strerror, sizeof(ssl_strerror));

                if (retcode){
                        proxenet_xsnprintf(ssl_strerror, sizeof(ssl_strerror)-1,
                                 "SSL Handshake: "RED"fail"NOCOLOR" [%d]",
                                 retcode);
                        xlog(LOG_ERROR, "%s\n", ssl_strerror);

                        if (cfg->verbose > 1){
                                proxenet_xzero(ssl_strerror, sizeof(ssl_strerror));
                                mbedtls_strerror(retcode, ssl_strerror, sizeof(ssl_strerror));
                                xlog(LOG_ERROR, "Details: %s\n", ssl_strerror);
                        }
                }
                else
                {
                        proxenet_xsnprintf(ssl_strerror, sizeof(ssl_strerror)-1,
                                 "SSL Handshake: "GREEN"success"NOCOLOR" [proto='%s',cipher='%s']",
                                 mbedtls_ssl_get_version( ctx ),
                                 mbedtls_ssl_get_ciphersuite( ctx ) );
                        xlog(LOG_INFO, "%s\n", ssl_strerror);

                        if (cfg->verbose>1){
                                if(mbedtls_ssl_get_peer_cert( ctx ) != NULL){
                                        proxenet_xzero(ssl_strerror, sizeof(ssl_strerror));
                                        mbedtls_x509_crt_info( ssl_strerror, sizeof(ssl_strerror)-1,
                                                               "      ", mbedtls_ssl_get_peer_cert( ctx ) );
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
                        proxenet_xsnprintf(errbuf+strlen(errbuf), sizeof(errbuf)-strlen(errbuf)-1, RED"failed"NOCOLOR" [%d]\n", ret);
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
        mbedtls_x509_crt_free( &ssl->cert );
        mbedtls_x509_crt_free( &ssl->ca );
        mbedtls_ctr_drbg_free( &ssl->ctr_drbg );
        mbedtls_entropy_free( &ssl->entropy );
	mbedtls_rsa_free(&ssl->rsa);
        mbedtls_pk_free( &ssl->pkey );
        mbedtls_ssl_free( &ssl->context );

        ssl->is_valid = false;

        return;
}


/**
 *
 */
void proxenet_ssl_finish(ssl_atom_t* ssl)
{
        mbedtls_ssl_close_notify( &ssl->context );
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

                if (retcode == MBEDTLS_ERR_SSL_WANT_READ ||\
                    retcode == MBEDTLS_ERR_SSL_WANT_WRITE )
                        continue;

                if (retcode>0)
                        break;

                if (retcode <= 0) {
                        char ssl_strerror[256] = {0, };

                        switch(retcode) {
                                case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY :
#ifdef DEBUG
                                        /* acceptable case */
                                        xlog(LOG_DEBUG, "%s\n", "SSL close notify received");
#endif
                                        return 0;

                                case MBEDTLS_ERR_NET_CONN_RESET:
                                        xlog(LOG_ERROR, "%s\n", "SSL connection reset by peer");
                                        return -1;

                                case 0:
                                        return 0;

                                default:
                                        mbedtls_strerror(retcode, ssl_strerror, 127);
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
        int (*func)() = &mbedtls_ssl_read;
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
        int (*func)() = &mbedtls_ssl_write;
        int ret = -1;

        ret = proxenet_ssl_ioctl(func, buf, count, ssl);
        if (ret < 0)
                xlog(LOG_ERROR, "%s\n", "Error while writing SSL stream");

        return ret;
}
