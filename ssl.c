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
#include <gnutls/gnutls.h>

#include "socket.h"
#include "utils.h"
#include "main.h"
#include "ssl.h"


/**
 * must be called once 
 */
int proxenet_ssl_init_global_context() 
{
	gnutls_global_init ();
	return 0;
}


/**
 *
 */
void proxenet_ssl_free_global_context()
{
	gnutls_global_deinit ();
}


/**
 *
 */
gnutls_session_t proxenet_ssl_init_server_context(ssl_ctx_t* ctx)
{
	gnutls_session_t session;
	gnutls_dh_params_t dh_params = ctx->dh_params;
	gnutls_certificate_credentials_t creds = ctx->srv_credz;
	int retcode;
	int bits;
	
	retcode = gnutls_certificate_allocate_credentials(&creds);
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_certificate_allocate_credentials: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	/* retcode = gnutls_certificate_set_x509_trust_file(creds, CAFILE, GNUTLS_X509_FMT_PEM); */
	/* if (retcode < 0){ */
	/* xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_certificate_set_x509_trust_file: %s\n", */
	/* gnutls_strerror(retcode)); */
	/* return NULL; */
	/* } */
	
	retcode=gnutls_certificate_set_x509_key_file(creds,cfg->certfile,cfg->keyfile,GNUTLS_X509_FMT_PEM);
	if (retcode < 0) {
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_certificate_set_x509_key_file: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LOW);
	
	retcode = gnutls_dh_params_init(&dh_params);
	if (retcode < 0) {
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_dh_params_init: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	retcode = gnutls_dh_params_generate2(dh_params, bits);
	if (retcode < 0) {
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_dh_params_generate2: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	gnutls_certificate_set_dh_params(creds, dh_params);
	
	retcode = gnutls_init(&session, GNUTLS_SERVER);
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_init: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	retcode=gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, creds);
	if (retcode != GNUTLS_E_SUCCESS) {
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_credentials_set: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	gnutls_certificate_server_set_request(session, GNUTLS_CERT_IGNORE);
	
	retcode = gnutls_priority_set_direct (session, "NORMAL", NULL);
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_server_context: gnutls_priority_set_direct: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	return session;
}


/**
 *
 */
gnutls_session_t proxenet_ssl_init_client_context(ssl_ctx_t* ctx)
{
	int retcode;
	gnutls_session_t session;
	gnutls_certificate_credentials_t creds = ctx->cli_credz;
	const char* err;
	
	gnutls_certificate_allocate_credentials(&creds);
	
	retcode = gnutls_init (&session, GNUTLS_CLIENT);     
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_client_context: gnutls_init: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	/* retcode = gnutls_certificate_set_x509_trust_file(creds, CAFILE, GNUTLS_X509_FMT_PEM); */
	/* if (retcode < 0){ */
	/* xlog(LOG_ERROR, "proxenet_ssl_init_client_context: gnutls_certificate_set_x509_trust_file: %s\n", */
	/* gnutls_strerror(retcode)); */
	/* return NULL; */
	/* } */
	
	retcode=gnutls_priority_set_direct (session, "NORMAL", &err);
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_client_context: gnutls_priority_set_direct: %s (err=%s)\n",
		     gnutls_strerror(retcode), err);
		return NULL;
	}
	
	retcode = gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, creds);
	if (retcode != GNUTLS_E_SUCCESS ){
		xlog(LOG_ERROR, "proxenet_ssl_init_client_context: gnutls_credentials_set: %s\n",
		     gnutls_strerror(retcode));
		return NULL;
	}
	
	return session;
} 


/**
 *
 */
void proxenet_ssl_wrap_socket(gnutls_session_t session, sock_t sock) 
{
	gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t)sock);
}


/**
 *
 */
int proxenet_ssl_handshake(gnutls_session_t session)
{
	int retcode = -1;
	
	retcode = gnutls_handshake(session);
	if (retcode < 0) {
		xlog(LOG_ERROR, "SSL handshake failed: %s\n", gnutls_strerror(retcode));
	} 
	
	return retcode;
}
