#ifndef _MAIN_H
#define _MAIN_H

#include "config.h"

#include <stdio.h>
#include "utils.h"

#define MAX_THREADS 	20

/* some configuration variables */
#define CFG_DEFAULT_BIND_ADDR             "localhost"
#define CFG_DEFAULT_PORT                  "8008"
#define CFG_DEFAULT_PROXY_PORT	          "8080"
#define CFG_DEFAULT_NB_THREAD             10
#define CFG_DEFAULT_TRY_EXIT_MAX          3
#define CFG_DEFAULT_OUTPUT                stdout
#define CFG_DEFAULT_PLUGINS_PATH          "./plugins"
#define CFG_DEFAULT_SSL_KEYFILE           "./keys/proxenet.key"
#define CFG_DEFAULT_SSL_CERTFILE          "./keys/proxenet.crt"
#define CFG_DEFAULT_IP_VERSION             AF_INET
#define CFG_REQUEST_PLUGIN_FUNCTION       "proxenet_request_hook"
#define CFG_RESPONSE_PLUGIN_FUNCTION      "proxenet_response_hook"


typedef struct __proxenet_config {
		unsigned char verbose;
		bool use_color;
		unsigned short nb_threads;
		char* logfile;
		FILE* logfile_fd;
		char* port;	
		char* iface;
		char* plugins_path;
		char* certfile;
		char* keyfile;
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
