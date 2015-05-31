#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

typedef enum {
        INTERCEPT_ONLY,
        INTERCEPT_EXCEPT
} intercept_t;

typedef struct __proxenet_config {
		unsigned char verbose;
		bool use_color;
		unsigned short nb_threads;
                bool daemon;
		char* logfile;
		FILE* logfile_fd;
		char* port;
		char* iface;
		int ip_version;

                /* interception parameters */
                intercept_t intercept_mode;
                char* intercept_pattern;

                /* plugin paths */
		char* plugins_path;
                char* autoload_path;

                /* proxenet SSL certificate for SSL interception */
		char* cafile;                        // realpath to proxenet CA certificate
		char* keyfile;                       // realpath to proxenet private key
                char* keyfile_pwd;                   // password to unlock the private key
                char* certsdir;                      // realpath to stored certificates
                char* certskey;                      // realpath to stored certificates private key
                char* certskey_pwd;                  // password to unlock the stored certificates private key

                /* client-side SSL certificate */
                char* sslcli_certfile;               // realpath to SSL client certificate
		char* sslcli_keyfile;                // realpath to SSL client private key
                char* sslcli_keyfile_pwd;            // password to unlock the client private key
                char* sslcli_domain;                 // domain to use the client the certificate

		struct _proxy_t {
				char* host;
				char* port;
		} proxy;
                bool is_socks_proxy;

		unsigned short try_exit;
		unsigned short try_exit_max;

                bool ie_compat;
                bool ssl_intercept;
} conf_t;

conf_t current_config;
conf_t* cfg;

int	proxenet_init_config(int, char**);
void	proxenet_free_config();

#endif /* _MAIN_H */
