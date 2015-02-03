#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

#define MAX_THREADS 	20

/* some configuration variables */
#ifndef CFG_DEFAULT_BIND_ADDR
#define CFG_DEFAULT_BIND_ADDR             "localhost"
#endif
#ifndef CFG_DEFAULT_PORT
#define CFG_DEFAULT_PORT                  "8008"
#endif
#ifndef CFG_DEFAULT_PROXY_PORT
#define CFG_DEFAULT_PROXY_PORT	          "8080"
#endif
#ifndef CFG_DEFAULT_NB_THREAD
#define CFG_DEFAULT_NB_THREAD             10
#endif
#ifndef CFG_DEFAULT_TRY_EXIT_MAX
#define CFG_DEFAULT_TRY_EXIT_MAX          3
#endif
#ifndef CFG_DEFAULT_OUTPUT
#define CFG_DEFAULT_OUTPUT                stdout
#endif
#ifndef CFG_DEFAULT_PLUGINS_PATH
#define CFG_DEFAULT_PLUGINS_PATH          "./proxenet-plugins"
#define CFG_DEFAULT_PLUGINS_AUTOLOAD_PATH CFG_DEFAULT_PLUGINS_PATH"/autoload"
#endif
#ifndef CFG_DEFAULT_SSL_KEYFILE
#define CFG_DEFAULT_SSL_KEYFILE           "./keys/proxenet.key"
#endif
#ifndef CFG_DEFAULT_SSL_CERTFILE
#define CFG_DEFAULT_SSL_CERTFILE          "./keys/proxenet.crt"
#endif
#ifndef CFG_DEFAULT_IP_VERSION
#define CFG_DEFAULT_IP_VERSION             AF_INET
#endif
#ifndef CFG_REQUEST_PLUGIN_FUNCTION
#define CFG_REQUEST_PLUGIN_FUNCTION       "proxenet_request_hook"
#endif
#ifndef CFG_RESPONSE_PLUGIN_FUNCTION
#define CFG_RESPONSE_PLUGIN_FUNCTION      "proxenet_response_hook"
#endif

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
