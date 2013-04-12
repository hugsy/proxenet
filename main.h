#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

#define MAX_THREADS sizeof(long long)*4

/* some configuration variables */
#define CFG_DEFAULT_BIND_ADDR             "localhost"
#define CFG_DEFAULT_PORT                  "8008"
#define CFG_DEFAULT_NB_THREAD             10
#define CFG_DEFAULT_OUTPUT                stdout
#define CFG_DEFAULT_PLUGINS_PATH          "./plugins"
#define CFG_DEFAULT_SSL_KEYFILE           "./keys/proxenet.key"
#define CFG_DEFAULT_SSL_CERTFILE          "./keys/proxenet.crt"
#define CFG_DEFAULT_IP_VERSION             AF_UNSPEC
#define CFG_REQUEST_PLUGIN_FUNCTION       "pre_request_hook"
#define CFG_RESPONSE_PLUGIN_FUNCTION      "post_request_hook"


typedef struct __proxenet_config {
	  unsigned char verbose;
	  boolean use_color;
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

conf_t* cfg;

#endif /* _MAIN_H */
