#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

typedef struct __proxenet_config {
		unsigned char verbose;
		bool use_color;
		unsigned short nb_threads;
                bool daemon;
		char* logfile;
		FILE* logfile_fd;
		char* port;
		char* iface;

                /* plugin paths */
		char* plugins_path;
                char* autoload_path;

                /* proxenet SSL certificate for SSL interception */
		char* certfile;                      // realpath to proxenet certificate
		char* keyfile;                       // realpath to proxenet private key
                char* keyfile_pwd;                   // password to unlock the private key

                /* client-side SSL certificate */
                char* sslcli_crtfile;                // realpath to SSL client certificate
		char* sslcli_keyfile;                // realpath to SSL client private key
                char* sslcli_keyfile_pwd;            // password to unlock the client private key
                char* domain;                        // domain to use the client the certificate

		int ip_version;
		struct _proxy_t {
				char* host;
				char* port;
		} proxy;

		unsigned short try_exit;
		unsigned short try_exit_max;
} conf_t;

conf_t current_config;
conf_t* cfg;

int	proxenet_init_config(int, char**);
void	proxenet_free_config();

#endif /* _MAIN_H */
