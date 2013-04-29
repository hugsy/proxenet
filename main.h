#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

#define PROGNAME 	"proxenet"
#define AUTHOR 		"hugsy < @__hugsy__>"
#define LICENSE 	"GPLv2"


#define MAX_THREADS 	20

/* some configuration variables */
#define CFG_DEFAULT_BIND_ADDR             "localhost"
#define CFG_DEFAULT_PORT                  "8008"
#define CFG_DEFAULT_NB_THREAD             10
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
} conf_t;

conf_t current_config;
conf_t* cfg;

int	proxenet_init_config(int, char**);
void	proxenet_free_config();
	
#endif /* _MAIN_H */
