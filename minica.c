#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <polarssl/x509_crt.h>
#include <polarssl/x509_csr.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/error.h>

#include "main.h"
#include "utils.h"

#define PROXENET_KEYS_DIR              "./keys"
#define PROXENET_CA_ROOT_CRT           PROXENET_KEYS_DIR"/proxenet.crt"
#define PROXENET_CA_ROOT_KEY           PROXENET_KEYS_DIR"/proxenet.key"
#define PROXENET_CA_ROOT_KEY_PWD       ""
#define PROXENET_CERTS_DIR             PROXENET_KEYS_DIR"/certs"
#define PROXENET_GENERIC_KEY           PROXENET_CERTS_DIR"/generic.key"

#define PROXENET_CERT_SERIAL         "1337"
#define PROXENET_CERT_NOT_BEFORE     "19700101000000"
#define PROXENET_CERT_NOT_AFTER      "20300000000000"
#define PROXENET_CERT_MAX_PATHLEN    2


/*
 *
 * Internal CA for proxenet, to generate on-the-fly valid certificates
 *
 */


static sem_t semaphore_create_crt;


/**
 *
 */
static int ca_init_prng(entropy_context *entropy, ctr_drbg_context *ctr_drbg)
{
        int retcode;

        entropy_init( entropy );
        retcode = ctr_drbg_init( ctr_drbg, entropy_func, entropy, (unsigned char*)PROGNAME, strlen(PROGNAME) );
        if( retcode < 0 ){
                xlog(LOG_ERROR, "ctr_drbg_init() returned %d\n", retcode);
                return -1;
        }

        return 0;
}


/**
 *
 */
static int ca_release_prng(entropy_context *entropy, ctr_drbg_context *ctr_drbg)
{
        ctr_drbg_free(ctr_drbg);
        entropy_free(entropy);
        return 0;
}


/**
 * Generate a temporary certificate signature request
 *
 * @return 0 if successful, -1 otherwise
 */
static int ca_generate_csr(ctr_drbg_context *ctr_drbg, char* hostname, unsigned char* csrbuf, size_t csrbuf_len)
{
        int retcode;
        char subj_name[256] = {0, };
        x509write_csr csr;
        pk_context key;
        unsigned short usage, cert_type;

        usage = KU_DIGITAL_SIGNATURE | KU_NON_REPUDIATION | KU_KEY_ENCIPHERMENT;
        usage|= KU_DATA_ENCIPHERMENT | KU_KEY_AGREEMENT | KU_KEY_CERT_SIGN;
        usage|= KU_CRL_SIGN;

        cert_type = NS_CERT_TYPE_SSL_SERVER | NS_CERT_TYPE_OBJECT_SIGNING | NS_CERT_TYPE_SSL_CA;

        /* init structures */
        x509write_csr_init( &csr );
        x509write_csr_set_md_alg( &csr, POLARSSL_MD_SHA1 );

        /* set usage */
        x509write_csr_set_key_usage( &csr, usage );
        x509write_csr_set_ns_cert_type( &csr, cert_type );

        /* set the key */
        pk_init( &key );
        retcode = pk_parse_keyfile(&key, PROXENET_GENERIC_KEY, NULL );
        if(retcode < 0) {
                xlog(LOG_ERROR, "pk_parse_keyfile() returned %d\n", retcode);
                retcode = -1;
                goto free_all;
        }
        x509write_csr_set_key( &csr, &key );

        /* set the subject name */
        snprintf(subj_name, sizeof(subj_name), "CN=%s,O=World Domination Corp.,C=US", hostname);
        retcode = x509write_csr_set_subject_name( &csr, subj_name);
        if( retcode < 0 ) {
                xlog(LOG_ERROR, "x509write_csr_set_subject_name() returned %d\n", retcode);
                retcode = -1;
                goto free_all;
        }

        /* write the csr */
        retcode = x509write_csr_pem(&csr, csrbuf, csrbuf_len, ctr_drbg_random, ctr_drbg);
        if (retcode < 0 ){
                xlog(LOG_ERROR, "x509write_csr_pem() returned %d\n", retcode);
                retcode = -1;
                goto free_all;
        }

        if (cfg->verbose)
                xlog(LOG_INFO, "Generated CSR for '%s'\n", hostname);

free_all:
        pk_free( &key );
        x509write_csr_free( &csr );
        return retcode;
}


/**
 *
 * @return 0 if successful, -1 otherwise
 */
int ca_generate_crt(ctr_drbg_context *ctr_drbg, unsigned char* csrbuf, size_t csrbuf_len, unsigned char* crtbuf, size_t crtbuf_len)
{
        int retcode = -1;
        char buf[1024] = {0, };
        char subject_name[128] = {0, };
        char issuer_name[128] = {0, };

        x509_csr csr;
        x509_crt issuer_crt;
        pk_context loaded_issuer_key, loaded_subject_key;
        x509write_cert crt;
        mpi serial;
        unsigned short usage, cert_type;


        usage = KU_DIGITAL_SIGNATURE | KU_NON_REPUDIATION | KU_KEY_ENCIPHERMENT;
        usage|= KU_DATA_ENCIPHERMENT | KU_KEY_AGREEMENT | KU_KEY_CERT_SIGN;
        usage|= KU_CRL_SIGN;

        cert_type = NS_CERT_TYPE_SSL_SERVER | NS_CERT_TYPE_OBJECT_SIGNING | NS_CERT_TYPE_SSL_CA;


        /* init structs */
        x509write_crt_init( &crt );
        x509write_crt_set_md_alg( &crt, POLARSSL_MD_SHA1 );
        pk_init( &loaded_issuer_key );
        pk_init( &loaded_subject_key );
        mpi_init( &serial );
        x509_crt_init( &issuer_crt );

        /* set serial */
        retcode = mpi_read_string( &serial, 16, PROXENET_CERT_SERIAL);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "mpi_read_string() returned -0x%02x - %s\n", -retcode, buf);
                goto exit;
        }

        /* load proxenet CA certificate */
        retcode = x509_crt_parse_file(&issuer_crt, PROXENET_CA_ROOT_CRT);
        if(retcode < 0) {
            polarssl_strerror(retcode, buf, sizeof(buf));
            xlog(LOG_ERROR, "x509_crt_parse_file() returned -0x%02x - %s\n", -retcode, buf);
            goto exit;
        }

       /* load proxenet CA key */
        retcode = pk_parse_keyfile(&loaded_issuer_key, PROXENET_CA_ROOT_KEY, PROXENET_CA_ROOT_KEY_PWD);
        if(retcode < 0){
            polarssl_strerror(retcode, buf, sizeof(buf));
            xlog(LOG_ERROR, "pk_parse_keyfile() returned -0x%02x - %s\n", -retcode, buf);
            goto exit;
        }

        /* get proxenet CA CN field */
        retcode = x509_dn_gets(issuer_name, sizeof(issuer_name), &issuer_crt.subject);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509_dn_gets() returned -0x%02x - %s\n", -retcode, buf);
                goto exit;
        }

        /* parse CSR  */
        retcode = x509_csr_parse(&csr, csrbuf, csrbuf_len);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509_csr_parse() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        /* load CSR subject name */
        retcode = x509_dn_gets(subject_name, sizeof(subject_name), &csr.subject);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509_csr_parse() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }


        /* apply settings // TODO: add checks retval */
        x509write_crt_set_subject_key(&crt, &csr.pk);
        x509write_crt_set_issuer_key(&crt, &issuer_crt.pk );

        retcode = x509write_crt_set_subject_name(&crt, subject_name);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_subject_name() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_issuer_name(&crt, issuer_name);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_issuer_name() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_serial(&crt, &serial);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_serial() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }
        retcode = x509write_crt_set_validity( &crt, PROXENET_CERT_NOT_BEFORE, PROXENET_CERT_NOT_AFTER);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_validity() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_basic_constraints(&crt, false, PROXENET_CERT_MAX_PATHLEN);
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_basic_constraints() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_subject_key_identifier( &crt );
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_subject_key_identifier() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_authority_key_identifier( &crt );
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_authority_key_identifier() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_key_usage( &crt, usage );
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_key_usage() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        retcode = x509write_crt_set_ns_cert_type( &crt, cert_type );
        if(retcode < 0) {
                polarssl_strerror(retcode, buf, sizeof(buf));
                xlog(LOG_ERROR, "x509write_crt_set_ns_cert_type() returned -0x%02x - %s\n", -retcode, buf );
                goto exit;
        }

        /* write CRT in buffer */
        retcode = x509write_crt_pem(&crt, crtbuf, crtbuf_len, ctr_drbg_random, ctr_drbg);
        if( retcode < 0 ){
                xlog(LOG_ERROR, "x509write_crt_pem() failed: %d\n", retcode);
                return retcode;
        }


        /* free structs */
exit:
        x509write_crt_free( &crt );
        pk_free( &loaded_subject_key );
        pk_free( &loaded_issuer_key );
        mpi_free( &serial );

        return retcode;
}


/**
 * Generate the CRT file for the hostname. Stores it in keys/certs.
 *
 * This whole function (and sub-functions) is semaphore-protected (thread-safe).
 *
 * @return 0 if successful, -1 otherwise
 */
static int create_crt(char* hostname, char* crtpath)
{
        int retcode, fd;
        unsigned char csrbuf[4096]={0, };
        unsigned char crtbuf[4096]={0, };
        ssize_t n;

        entropy_context entropy;
        ctr_drbg_context ctr_drbg;


        /* init prng */
        retcode = ca_init_prng(&entropy, &ctr_drbg);
        if(retcode<0)
                goto exit;

        /* generate csr w/ privkey static */
        retcode = ca_generate_csr(&ctr_drbg, hostname, csrbuf, sizeof(csrbuf));
        if(retcode<0)
                goto exit;

        /* sign csr w/ proxenet root crt (in `keys/`) */
        retcode = ca_generate_crt(&ctr_drbg, csrbuf, sizeof(csrbuf), crtbuf, sizeof(crtbuf));
        if(retcode<0)
                goto exit;

        /* write CRT on FS */
        fd = open(crtpath, O_WRONLY|O_CREAT|O_SYNC);
        if(fd<0){
                xlog(LOG_ERROR, "open() failed: %s\n", strerror(errno));
                goto exit;
        }

        n = write(fd, crtbuf, sizeof(csrbuf));
        if(n<0)
                goto exit;

        close(fd);

        if(cfg->verbose)
                xlog(LOG_INFO, "New CRT '%s'\n", crtpath);

        /* supprime le csr & free les rsrc */
exit:
        retcode = ca_release_prng(&entropy, &ctr_drbg);
        return retcode;
}


/**
 * Lookup for a valid CRT file in cert dir.
 *
 * If found, the absolute path is stored in *crtpath. Its content *must* be free-ed by caller
 * If not, *crtpath is NULL.
 *
 * @return 0 if successful, -1 otherwise
 */
int proxenet_lookup_crt(char* hostname, char** crtpath)
{
        int retcode, n;
        char buf[PATH_MAX];
        char *crt_realpath;

        n = snprintf(buf, PATH_MAX, "%s/%s.crt", PROXENET_CERTS_DIR, hostname);
        if (n<0){
                *crtpath = NULL;
                return -1;
        }

        crt_realpath = realpath(buf, NULL);
        if (!realpath) {
                *crtpath = NULL;
                return -1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "Looking for valid CRT at '%s'\n", crt_realpath);
#endif
        if (is_readable_file(crt_realpath)){
#ifdef DEBUG
                xlog(LOG_DEBUG, "'%s' exists\n", crt_realpath);
#endif
                *crtpath = crt_realpath;
                return 0;
        }


        sem_wait(&semaphore_create_crt);


        /**
         * To avoid TOC-TOU race, we check another time if CRT can be found
         */
        if (is_readable_file(crt_realpath)){
#ifdef DEBUG
                xlog(LOG_DEBUG, "'%s' exists - second check\n", crt_realpath);
#endif
                sem_post(&semaphore_create_crt);
                *crtpath = crt_realpath;
                return 0;
        }

        /* if exists, release lock && returns */

        /* if not, create && release lock && returns  */
        retcode = create_crt(hostname, crt_realpath);
        if (retcode < 0){
                sem_post(&semaphore_create_crt);
                *crtpath = NULL;
                return -1;
        }

        sem_post(&semaphore_create_crt);
        *crtpath = crt_realpath ;

#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s' created\n", crt_realpath);
#endif

        return 0;
}
