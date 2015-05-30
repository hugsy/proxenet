#ifndef _MINICA_H
#define _MINICA_H

#define PROXENET_CERT_SERIAL_SIZE      X509_RFC5280_MAX_SERIAL_LEN
#define PROXENET_CERT_NOT_BEFORE       "20010101000000"
#define PROXENET_CERT_NOT_AFTER        "20301231235959"
#define PROXENET_CERT_MAX_PATHLEN      -1
#define PROXENET_CERT_SUBJECT          "CN=%s,OU=BlackHats,O=World Domination Corp.,C=US"

int     serial_base;

int     proxenet_get_cert_serial(ssl_atom_t* ssl, proxenet_ssl_buf_t* dst);
int     proxenet_lookup_crt(char* hostname, char** crtpath);

#endif /* _MINICA_H */
