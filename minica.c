#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include <polarssl/x509_crt.h>
#include <polarssl/x509_csr.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/error.h>

#include "minica.h"
#include "main.h"
#include "core.h"
#include "utils.h"

#define PROXENET_CERT_SERIAL_SIZE      32
#define PROXENET_CERT_NOT_BEFORE       "20010101000000"
#define PROXENET_CERT_NOT_AFTER        "20301231235959"
#define PROXENET_CERT_MAX_PATHLEN      -1
#define PROXENET_CERT_SUBJECT          "CN=%s,OU=BlackHats,O=World Domination Corp.,C=US"


static pthread_mutex_t	certificate_mutex;


/*
 *
 * Internal CA for proxenet, to generate on-the-fly valid certificates
 *
 */


/**
 * Get the next serial
 *
 * @return serial
 */
static int ca_get_serial(char** buf, int buflen)
{
        unsigned long s;
        s = ++seriali;
        return snprintf(*buf, buflen, "%lu", s);;
}


/**
 *
 */
static int ca_init_prng(entropy_context *entropy, ctr_drbg_context *ctr_drbg)
{
        int retcode;

        entropy_init( entropy );
        retcode = ctr_drbg_init(ctr_drbg, entropy_func, entropy, (unsigned char*)PROGNAME, strlen(PROGNAME));
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

        /* init structures */
        x509write_csr_init( &csr );
        x509write_csr_set_md_alg( &csr, POLARSSL_MD_SHA1 );

        /* set the key */
        pk_init( &key );
        retcode = pk_parse_keyfile(&key, cfg->certskey, cfg->certskey_pwd);
        if(retcode < 0) {
                xlog(LOG_ERROR, "pk_parse_keyfile() returned %d\n", retcode);
                retcode = -1;
                goto free_all;
        }
        x509write_csr_set_key( &csr, &key );

        /* set the subject name */
        snprintf(subj_name, sizeof(subj_name), PROXENET_CERT_SUBJECT, hostname);
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
 * Generate a CRT for CSR given in input.
 *
 * @return 0 if successful, -1 otherwise
 */
int ca_generate_crt(ctr_drbg_context *ctr_drbg, unsigned char* csrbuf, size_t csrbuf_len, unsigned char* crtbuf, size_t crtbuf_len)
{
        int retcode = -1;
        char errbuf[128] = {0, };
        char subject_name[128] = {0, };
        char issuer_name[128] = {0, };
        char *serial_str;

        x509_csr csr;
        x509_crt issuer_crt;
        pk_context issuer_key;
        x509write_cert crt;
        mpi serial;

#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "%s\n", "CRT init structs");
#endif

        /* init structs */
        x509write_crt_init( &crt );
        x509write_crt_set_md_alg( &crt, POLARSSL_MD_SHA1 );
        x509_csr_init( &csr );
        x509_crt_init( &issuer_crt );
        pk_init( &issuer_key );
        mpi_init( &serial );

#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "%s\n", "CRT load cert & key");
#endif

        /* set serial */
        serial_str = (char*)alloca(PROXENET_CERT_SERIAL_SIZE);
        retcode = ca_get_serial(&serial_str, PROXENET_CERT_SERIAL_SIZE);
        if(retcode < 0) {
                goto exit;
        }

        retcode = mpi_read_string(&serial, 10, serial_str);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "mpi_read_string() returned -0x%02x - %s\n", -retcode, errbuf);
                goto exit;
        }


        /* load proxenet CA certificate */
        retcode = x509_crt_parse_file(&issuer_crt, cfg->cafile);
        if(retcode < 0) {
            polarssl_strerror(retcode, errbuf, sizeof(errbuf));
            xlog(LOG_ERROR, "x509_crt_parse_file() returned -0x%02x - %s\n", -retcode, errbuf);
            goto exit;
        }

       /* load proxenet CA key */
        retcode = pk_parse_keyfile(&issuer_key, cfg->keyfile, cfg->keyfile_pwd);
        if(retcode < 0){
            polarssl_strerror(retcode, errbuf, sizeof(errbuf));
            xlog(LOG_ERROR, "pk_parse_keyfile() returned -0x%02x - %s\n", -retcode, errbuf);
            goto exit;
        }

        /* get proxenet CA CN field */
        retcode = x509_dn_gets(issuer_name, sizeof(issuer_name), &issuer_crt.subject);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509_dn_gets() returned -0x%02x - %s\n", -retcode, errbuf);
                goto exit;
        }

        /* parse CSR  */
        retcode = x509_csr_parse(&csr, csrbuf, csrbuf_len);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509_csr_parse() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        /* load CSR subject name */
        retcode = x509_dn_gets(subject_name, sizeof(subject_name), &csr.subject);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509_csr_parse() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "%s\n", "CRT fill new crt fields");
#endif

        /* apply settings */
        x509write_crt_set_subject_key(&crt, &csr.pk);
        x509write_crt_set_issuer_key(&crt, &issuer_key);

        retcode = x509write_crt_set_subject_name(&crt, subject_name);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_subject_name() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_issuer_name(&crt, issuer_name);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_issuer_name() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_serial(&crt, &serial);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_serial() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_validity(&crt, PROXENET_CERT_NOT_BEFORE, PROXENET_CERT_NOT_AFTER);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_validity() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_basic_constraints(&crt, false, 0);
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_basic_constraints() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_subject_key_identifier( &crt );
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_subject_key_identifier() returned -0x%02x - %s\n", -retcode, errbuf );
                goto exit;
        }

        retcode = x509write_crt_set_authority_key_identifier( &crt );
        if(retcode < 0) {
                polarssl_strerror(retcode, errbuf, sizeof(errbuf));
                xlog(LOG_ERROR, "x509write_crt_set_authority_key_identifier() returned -0x%02x - %s\n", -retcode, errbuf );
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
        pk_free( &issuer_key );
        mpi_free( &serial );

        return retcode;
}


/**
 * Generate the CRT file for the hostname. Stores it in keys/certs.
 *
 * This whole function (and sub-functions) is thread-safe (mutex).
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
#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "CSR for '%s':\n%s\n", hostname, csrbuf);
#endif

        /* sign csr w/ proxenet root crt (in `keys/`) */
#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "Signing CSR for '%s' with '%s'(key='%s')\n", hostname, cfg->cafile, cfg->keyfile);
#endif
        retcode = ca_generate_crt(&ctr_drbg, csrbuf, sizeof(csrbuf), crtbuf, sizeof(crtbuf));
        if(retcode<0)
                goto exit;
#ifdef DEBUG_SSL
        xlog(LOG_DEBUG, "CRT signed for '%s':\n%s\n", hostname, crtbuf);
#endif

        /* write CRT on FS */
#ifdef DEBUG
        xlog(LOG_DEBUG, "Writing CRT signed in '%s'\n", crtpath);
#endif
        fd = open(crtpath, O_WRONLY|O_CREAT|O_SYNC, S_IRUSR|S_IWUSR);
        if(fd<0){
                xlog(LOG_ERROR, "CRT open() failed: %s\n", strerror(errno));
                retcode = -1;
                goto exit;
        }

        n = write(fd, crtbuf, sizeof(csrbuf));
        if(n<0){
                xlog(LOG_ERROR, "CRT write() failed: %s\n", strerror(errno));
                retcode = -1;
                goto exit;
        }

        close(fd);

        if(cfg->verbose)
                xlog(LOG_INFO, "New CRT '%s'\n", crtpath);

        /* supprime le csr & free les rsrc */
        retcode = 0;

exit:
        ca_release_prng(&entropy, &ctr_drbg);
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

        n = snprintf(buf, PATH_MAX, "%s/%s.crt", cfg->certsdir, hostname);
        if (n<0){
                *crtpath = NULL;
                return -1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "Solving '%s'\n", buf);
#endif
        crt_realpath = realpath(buf, NULL);
        if (crt_realpath) {
#ifdef DEBUG
                xlog(LOG_DEBUG, "realpath() found CRT at '%s'\n", crt_realpath);
#endif
                if (!is_readable_file(crt_realpath)){
                        xlog(LOG_ERROR, "'%s' exists but is not readable\n", crt_realpath);
                        *crtpath = NULL;
                        return -1;
                }
#ifdef DEBUG
                xlog(LOG_DEBUG, "Hit cache for CRT '%s'\n", crt_realpath);
#endif
                *crtpath = crt_realpath;
                return 0;
        }


        /* if here, realpath() failed, check the errno */
        switch(errno){
                case ENOENT:
                        if (cfg->verbose)
                                xlog(LOG_INFO, "Could not find CRT for '%s'. Generating...\n", hostname);
                        crt_realpath = proxenet_xstrdup2(buf);
                        break;

                default:
                        xlog(LOG_ERROR, "realpath() failed for '%s': %s\n", buf, strerror(errno));
                        *crtpath = NULL;
                        return -1;
        }

        /* critical section to avoid race conditions on crt creation */
#ifdef DEBUG
        xlog(LOG_DEBUG, "Getting crt create lock at %p\n", &certificate_mutex);
#endif
        pthread_mutex_lock(&certificate_mutex);

        /**
         * To avoid TOC-TOU race, we check another time if CRT can be found
         */
        if (is_readable_file(crt_realpath)){
#ifdef DEBUG
                xlog(LOG_DEBUG, "'%s' cache hit (second check)\n", crt_realpath);
#endif
                *crtpath = crt_realpath;
                pthread_mutex_unlock(&certificate_mutex);
                return 0;
        }

        /* if not, create && release lock && returns  */
        retcode = create_crt(hostname, crt_realpath);
        if (retcode < 0){
                xlog(LOG_ERROR, "create_crt(hostname='%s', crt='%s') has failed\n",
                     hostname, crt_realpath);
                *crtpath = NULL;
                pthread_mutex_unlock(&certificate_mutex);
                return -1;
        }

        *crtpath = crt_realpath;
        pthread_mutex_unlock(&certificate_mutex);

#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s' created\n", *crtpath);
#endif
        /* end of critical section */

        return 0;
}
